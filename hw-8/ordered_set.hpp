#pragma once

#include <atomic>
#include <cstdint>
#include <new>
#include <random>
#include <vector>

namespace lockfree {

template <typename T>
class set {
public:
    static constexpr int MAX_HEIGHT = 20;

private:
    struct Node;

    struct TaggedPtr {
        uintptr_t v;

        Node *ptr() const {
            return reinterpret_cast<Node *>(v & ~uintptr_t(1));
        }

        bool marked() const {
            return (v & 1u) != 0;
        }

        TaggedPtr() : v(0) {
        }

        TaggedPtr(Node *p, bool m)
            : v(reinterpret_cast<uintptr_t>(p) | (m ? 1u : 0u)) {
        }

        bool operator==(const TaggedPtr &o) const {
            return v == o.v;
        }
    };

    struct Node {
        T key;
        int height;
        std::atomic<TaggedPtr> next[];

        static Node *make(const T &k, int h) {
            void *mem = ::operator new(
                sizeof(Node) + h * sizeof(std::atomic<TaggedPtr>)
            );
            return new (mem) Node(k, h);
        }

        static void destroy(Node *n) {
            if (!n) {
                return;
            }
            n->~Node();
            ::operator delete(static_cast<void *>(n));
        }

        Node(const T &k, int h) : key(k), height(h) {
            for (int i = 0; i < h; ++i) {
                next[i].store(
                    TaggedPtr(nullptr, false), std::memory_order_relaxed
                );
            }
        }
    };

    struct Participant {
        std::atomic<bool> active{false};
        std::atomic<uint32_t> local_epoch{0};
        std::atomic<bool> owned{false};
        std::atomic<Participant *> list_next{nullptr};

        struct Retired {
            Node *node;
            uint32_t epoch;
        };

        std::vector<Retired> retired;
    };

    Node *head_;
    mutable std::atomic<uint32_t> global_epoch_{0};
    mutable std::atomic<Participant *> participants_{nullptr};

    Participant *acquire_participant() const {
        for (Participant *p = participants_.load(std::memory_order_acquire); p;
             p = p->list_next.load(std::memory_order_acquire)) {
            bool expected = false;
            if (!p->owned.load(std::memory_order_acquire) &&
                p->owned.compare_exchange_strong(
                    expected, true, std::memory_order_acq_rel,
                    std::memory_order_relaxed
                )) {
                return p;
            }
        }

        Participant *np = new Participant();
        np->owned.store(true, std::memory_order_relaxed);

        Participant *head = participants_.load(std::memory_order_relaxed);
        do {
            np->list_next.store(head, std::memory_order_relaxed);
        } while (!participants_.compare_exchange_weak(
            head, np, std::memory_order_release, std::memory_order_relaxed
        ));

        return np;
    }

    void pin(Participant *p) const {
        uint32_t ge = global_epoch_.load(std::memory_order_acquire);

        p->local_epoch.store(ge, std::memory_order_relaxed);
        p->active.store(true, std::memory_order_relaxed);

        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void unpin(Participant *p) const {
        p->active.store(false, std::memory_order_release);
    }

    void release_participant(Participant *p) const {
        p->owned.store(false, std::memory_order_release);
    }

    void retire(Participant *p, Node *n) {
        uint32_t e = global_epoch_.load(std::memory_order_acquire);

        p->retired.push_back({n, e});

        try_collect(p);
    }

    void try_collect(Participant *p) {
        uint32_t ge = global_epoch_.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        bool can_advance = true;

        for (Participant *q = participants_.load(std::memory_order_acquire); q;
             q = q->list_next.load(std::memory_order_acquire)) {
            if (q->active.load(std::memory_order_acquire) &&
                q->local_epoch.load(std::memory_order_acquire) != ge) {
                can_advance = false;
                break;
            }
        }

        if (can_advance) {
            global_epoch_.compare_exchange_strong(
                ge, ge + 1, std::memory_order_acq_rel, std::memory_order_relaxed
            );
        }

        uint32_t cur = global_epoch_.load(std::memory_order_acquire);
        auto &R = p->retired;
        size_t w = 0;

        for (size_t i = 0; i < R.size(); ++i) {
            if (R[i].epoch + 2 <= cur) {
                Node::destroy(R[i].node);
            } else {
                R[w++] = R[i];
            }
        }
        R.resize(w);
    }

    static int random_height() {
        thread_local std::mt19937 rng(
            std::random_device{}() ^
            static_cast<unsigned>(reinterpret_cast<uintptr_t>(&rng))
        );

        thread_local std::geometric_distribution<int> dist(0.5);
        int h = dist(rng) + 1;

        return h > MAX_HEIGHT ? MAX_HEIGHT : h;
    }

    bool find(const T &key, Node **preds, Node **succs) {
    retry:
        Node *pred = head_;
        for (int lvl = MAX_HEIGHT - 1; lvl >= 0; --lvl) {
            Node *curr = pred->next[lvl].load(std::memory_order_acquire).ptr();

            while (curr) {
                TaggedPtr succt =
                    curr->next[lvl].load(std::memory_order_acquire);

                Node *succ = succt.ptr();
                if (succt.marked()) {
                    TaggedPtr expected(curr, false);
                    if (!pred->next[lvl].compare_exchange_strong(
                            expected, TaggedPtr(succ, false),
                            std::memory_order_acq_rel, std::memory_order_acquire
                        )) {
                        goto retry;
                    }
                    curr = succ;
                    continue;
                }

                if (curr->key < key) {
                    pred = curr;
                    curr = succ;
                } else
                    break;
            }
            preds[lvl] = pred;
            succs[lvl] = curr;
        }
        return succs[0] != nullptr && succs[0]->key == key;
    }

public:
    set() {
        head_ = Node::make(T(), MAX_HEIGHT);
    }

    ~set() {
        Node *curr = head_->next[0].load(std::memory_order_relaxed).ptr();
        while (curr) {
            Node *nx = curr->next[0].load(std::memory_order_relaxed).ptr();
            Node::destroy(curr);
            curr = nx;
        }

        Node::destroy(head_);

        Participant *p = participants_.load(std::memory_order_relaxed);
        while (p) {
            Participant *nx = p->list_next.load(std::memory_order_relaxed);
            for (auto &r : p->retired)
                Node::destroy(r.node);
            delete p;
            p = nx;
        }
    }

    set(const set &) = delete;
    set &operator=(const set &) = delete;

    bool add(T value) {
        Participant *p = acquire_participant();
        pin(p);

        Node *preds[MAX_HEIGHT];
        Node *succs[MAX_HEIGHT];
        bool result;

        while (true) {
            if (find(value, preds, succs)) {
                result = false;
                break;
            }

            int height = random_height();
            Node *nn = Node::make(value, height);
            for (int lvl = 0; lvl < height; ++lvl)
                nn->next[lvl].store(
                    TaggedPtr(succs[lvl], false), std::memory_order_relaxed
                );

            TaggedPtr e0(succs[0], false);
            if (!preds[0]->next[0].compare_exchange_strong(
                    e0, TaggedPtr(nn, false), std::memory_order_acq_rel,
                    std::memory_order_acquire
                )) {
                Node::destroy(nn);
                continue;
            }

            for (int lvl = 1; lvl < height; ++lvl) {
                while (true) {
                    if (nn->next[lvl]
                            .load(std::memory_order_acquire)
                            .marked()) {
                        result = true;
                        goto done;
                    }

                    Node *pred = preds[lvl];
                    Node *succ = succs[lvl];
                    TaggedPtr nnx =
                        nn->next[lvl].load(std::memory_order_acquire);

                    if (!nnx.marked() && nnx.ptr() != succ) {
                        if (!nn->next[lvl].compare_exchange_strong(
                                nnx, TaggedPtr(succ, false),
                                std::memory_order_acq_rel,
                                std::memory_order_acquire
                            )) {
                            if (nn->next[lvl]
                                    .load(std::memory_order_acquire)
                                    .marked()) {
                                result = true;
                                goto done;
                            }
                            continue;
                        }
                    }

                    TaggedPtr ep(succ, false);
                    if (pred->next[lvl].compare_exchange_strong(
                            ep, TaggedPtr(nn, false), std::memory_order_acq_rel,
                            std::memory_order_acquire
                        )) {
                        break;
                    }

                    if (!find(value, preds, succs)) {
                        result = true;
                        goto done;
                    }
                }
            }
            result = true;
            break;
        }
    done:
        unpin(p);
        release_participant(p);
        return result;
    }

    bool remove(T value) {
        Participant *p = acquire_participant();
        pin(p);
        Node *preds[MAX_HEIGHT];
        Node *succs[MAX_HEIGHT];
        bool result;
        while (true) {
            if (!find(value, preds, succs)) {
                result = false;
                break;
            }
            Node *target = succs[0];

            for (int lvl = target->height - 1; lvl >= 1; --lvl) {
                TaggedPtr nx =
                    target->next[lvl].load(std::memory_order_acquire);
                while (!nx.marked()) {
                    if (target->next[lvl].compare_exchange_strong(
                            nx, TaggedPtr(nx.ptr(), true),
                            std::memory_order_acq_rel, std::memory_order_acquire
                        )) {
                        break;
                    }
                }
            }

            TaggedPtr nx0 = target->next[0].load(std::memory_order_acquire);
            bool committed = false, lost = false;
            while (true) {
                if (nx0.marked()) {
                    lost = true;
                    break;
                }
                if (target->next[0].compare_exchange_strong(
                        nx0, TaggedPtr(nx0.ptr(), true),
                        std::memory_order_acq_rel, std::memory_order_acquire
                    )) {
                    committed = true;
                    break;
                }
            }

            if (lost) {
                result = false;
                break;
            }

            if (committed) {
                find(value, preds, succs);
                retire(p, target);
                result = true;
                break;
            }
        }

        unpin(p);
        release_participant(p);
        return result;
    }

    bool contains(T value) const {
        Participant *p = acquire_participant();
        pin(p);
        bool result = false;
        Node *pred = head_;
        for (int lvl = MAX_HEIGHT - 1; lvl >= 0; --lvl) {
            Node *curr = pred->next[lvl].load(std::memory_order_acquire).ptr();
            while (curr) {
                if (curr->key > value)
                    break;
                if (curr->key == value) {
                    result =
                        !curr->next[0].load(std::memory_order_acquire).marked();
                    goto done;
                }
                pred = curr;
                curr = curr->next[lvl].load(std::memory_order_acquire).ptr();
            }
        }
    done:
        unpin(p);
        release_participant(p);
        return result;
    }

    bool isEmpty() const {
        Participant *p = acquire_participant();
        pin(p);

        bool result = true;

        Node *curr = head_->next[0].load(std::memory_order_acquire).ptr();
        while (curr) {
            TaggedPtr nx = curr->next[0].load(std::memory_order_acquire);
            if (!nx.marked()) {
                result = false;
                break;
            }
            curr = nx.ptr();
        }

        unpin(p);
        release_participant(p);
        return result;
    }
};

}  // namespace lockfree
