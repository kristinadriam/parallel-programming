#pragma once

#include <atomic>
#include <mutex>

class IntSetBase {
public:
    virtual bool Insert(int value) = 0;
    virtual bool Remove(int value) = 0;
    virtual bool Contains(int value) const = 0;
    virtual ~IntSetBase() = default;
};

class RawSet final: public IntSetBase {
public:
    ~RawSet() override {
        Node* curr = head_;

        while (curr) {
            Node* next = curr->next;

            delete curr;

            curr = next;
        }
    }

    bool Insert(int value) override {
        std::lock_guard lock(mtx_);

        Node* prev = nullptr;
        Node* curr = head_;

        while (curr && curr->value < value) {
            prev = curr;
            curr = curr->next;
        }

        if (curr && curr->value == value) {
            return false;
        }

        Node* node = new Node(value, curr);

        (prev ? prev->next : head_) = node;

        return true;
    }

    bool Remove(int value) override {
        std::lock_guard lock(mtx_);

        Node* prev = nullptr;
        Node* curr = head_;

        while (curr && curr->value < value) {
            prev = curr;
            curr = curr->next;
        }

        if (!curr || curr->value != value) {
            return false;
        }

        (prev ? prev->next : head_) = curr->next;

        delete curr;

        return true;
    }

    bool Contains(int value) const override {
        std::lock_guard lock(mtx_);

        Node* curr = head_;

        while (curr && curr->value < value) {
            curr = curr->next;
        }

        return curr && curr->value == value;
    }

private:
    struct Node {
        Node(int v, Node* n = nullptr)
            : value(v)
            , next(n)
        {
        }

        int value;
        Node* next;
    };

    Node* head_ = nullptr;
    mutable std::mutex mtx_;
};

class FineSet final: public IntSetBase {
public:
    ~FineSet() override {
        Node* curr = head_;

        while (curr) {
            Node* next = curr->next;

            delete curr;

            curr = next;
        }
    }

    bool Insert(int value) override {
        head_mtx_.lock();

        if (!head_ || head_->value > value) {
            head_ = new Node(value, head_);

            head_mtx_.unlock();

            return true;
        }

        if (head_->value == value) {
            head_mtx_.unlock();

            return false;
        }

        Node* pred = head_;

        pred->mtx.lock();
        head_mtx_.unlock();

        Node* curr = pred->next;

        if (curr) {
            curr->mtx.lock();
        }

        while (curr && curr->value < value) {
            pred->mtx.unlock();

            pred = curr;
            curr = curr->next;

            if (curr) {
                curr->mtx.lock();
            }
        }

        bool added = !(curr && curr->value == value);

        if (added) {
            pred->next = new Node(value, curr);
        }

        if (curr) {
            curr->mtx.unlock();
        }

        pred->mtx.unlock();

        return added;
    }

    bool Remove(int value) override {
        head_mtx_.lock();

        if (!head_ || head_->value > value) {
            head_mtx_.unlock();

            return false;
        }

        if (head_->value == value) {
            head_->mtx.lock();

            Node* del = head_;

            head_ = head_->next;

            head_mtx_.unlock();
            del->mtx.unlock();

            delete del;

            return true;
        }

        Node* pred = head_;

        pred->mtx.lock();
        head_mtx_.unlock();

        Node* curr = pred->next;

        if (curr) {
            curr->mtx.lock();
        }

        while (curr && curr->value < value) {
            pred->mtx.unlock();

            pred = curr;
            curr = curr->next;

            if (curr) {
                curr->mtx.lock();
            }
        }

        bool removed = curr && curr->value == value;

        if (removed) {
            pred->next = curr->next;

            curr->mtx.unlock();

            delete curr;
        } else if (curr) {
            curr->mtx.unlock();
        }

        pred->mtx.unlock();

        return removed;
    }

    bool Contains(int value) const override {
        head_mtx_.lock();

        if (!head_ || head_->value > value) {
            head_mtx_.unlock();

            return false;
        }

        if (head_->value == value) {
            head_mtx_.unlock();

            return true;
        }

        Node* pred = head_;

        pred->mtx.lock();
        head_mtx_.unlock();

        Node* curr = pred->next;

        if (curr) {
            curr->mtx.lock();
        }

        while (curr && curr->value < value) {
            pred->mtx.unlock();

            pred = curr;
            curr = curr->next;

            if (curr) {
                curr->mtx.lock();
            }
        }

        bool found = curr && curr->value == value;

        if (curr) {
            curr->mtx.unlock();
        }

        pred->mtx.unlock();

        return found;
    }

private:
    struct Node {
        Node(int v, Node* n = nullptr)
            : value(v)
            , next(n)
        {
        }

        int value;
        Node* next;
        mutable std::mutex mtx;
    };

    Node* head_ = nullptr;
    mutable std::mutex head_mtx_;
};

class OptimisticSet final: public IntSetBase {
public:
    ~OptimisticSet() override {
        Node* curr = head_.load();

        while (curr) {
            Node* next = curr->next.load();

            delete curr;

            curr = next;
        }

        curr = retired_.load();

        while (curr) {
            Node* next = curr->next.load();

            delete curr;

            curr = next;
        }
    }

    bool Insert(int value) override {
        while (true) {
            auto [pred, curr] = Find(value);

            LockPred(pred);

            if (curr) {
                curr->mtx.lock();
            }

            if (!Validate(pred, curr)) {
                Unlock(pred, curr);

                continue;
            }

            if (curr && curr->value == value) {
                Unlock(pred, curr);

                return false;
            }

            Node* node = new Node(value, curr);

            SetNext(pred, node);
            Unlock(pred, curr);

            return true;
        }
    }

    bool Remove(int value) override {
        while (true) {
            auto [pred, curr] = Find(value);

            LockPred(pred);

            if (curr) {
                curr->mtx.lock();
            }

            if (!Validate(pred, curr)) {
                Unlock(pred, curr);

                continue;
            }

            if (!curr || curr->value != value) {
                Unlock(pred, curr);

                return false;
            }

            SetNext(pred, curr->next.load());

            curr->mtx.unlock();

            UnlockPred(pred);

            Retire(curr);

            return true;
        }
    }

    bool Contains(int value) const override {
        while (true) {
            auto [pred, curr] = Find(value);

            LockPred(pred);

            if (curr) {
                curr->mtx.lock();
            }

            if (!Validate(pred, curr)) {
                Unlock(pred, curr);

                continue;
            }

            bool found = curr && curr->value == value;

            Unlock(pred, curr);

            return found;
        }
    }

private:
    struct Node {
        Node(int v, Node* n = nullptr)
            : value(v)
        {
            next.store(n);
        }

        int value;
        std::atomic<Node*> next{nullptr};
        mutable std::mutex mtx;
    };

    std::atomic<Node*> head_{nullptr};
    std::atomic<Node*> retired_{nullptr};
    mutable std::mutex head_mtx_;

    std::pair<Node*, Node*> Find(int value) const {
        Node* pred = nullptr;
        Node* curr = head_.load(std::memory_order_acquire);

        while (curr && curr->value < value) {
            pred = curr;
            curr = curr->next.load(std::memory_order_acquire);
        }

        return {pred, curr};
    }

    bool Validate(Node* pred, Node* curr) const {
        if (pred == nullptr) {
            return head_.load() == curr;
        }

        Node* node = head_.load();

        while (node && node->value < pred->value) {
            node = node->next.load();
        }

        return node == pred && pred->next.load() == curr;
    }

    void LockPred(Node* pred) const {
        if (pred == nullptr) {
            head_mtx_.lock();
        } else {
            pred->mtx.lock();
        }
    }

    void UnlockPred(Node* pred) const {
        if (pred == nullptr) {
            head_mtx_.unlock();
        } else {
            pred->mtx.unlock();
        }
    }

    void Unlock(Node* pred, Node* curr) const {
        if (curr) {
            curr->mtx.unlock();
        }

        UnlockPred(pred);
    }

    void SetNext(Node* pred, Node* next) {
        if (pred == nullptr) {
            head_.store(next, std::memory_order_release);
        } else {
            pred->next.store(next, std::memory_order_release);
        }
    }

    void Retire(Node* node) {
        Node* head = retired_.load(std::memory_order_relaxed);
        do {
            node->next.store(head, std::memory_order_relaxed);
        } while (!retired_.compare_exchange_weak(
            head, node, std::memory_order_release, std::memory_order_relaxed));
    }
};

class LazySet final: public IntSetBase {
public:
    ~LazySet() override {
        Node* curr = head_.load();

        while (curr) {
            Node* next = curr->next.load();

            delete curr;

            curr = next;
        }

        curr = retired_.load();

        while (curr) {
            Node* next = curr->next.load();

            delete curr;

            curr = next;
        }
    }

    bool Insert(int value) override {
        while (true) {
            auto [pred, curr] = Find(value);

            LockPred(pred);

            if (curr) {
                curr->mtx.lock();
            }

            if (!Validate(pred, curr)) {
                Unlock(pred, curr);
                continue;
            }

            if (curr && curr->value == value) {
                Unlock(pred, curr);
                return false;
            }

            Node* node = new Node(value, curr);

            SetNext(pred, node);
            Unlock(pred, curr);

            return true;
        }
    }

    bool Remove(int value) override {
        while (true) {
            auto [pred, curr] = Find(value);

            LockPred(pred);

            if (curr) {
                curr->mtx.lock();
            }

            if (!Validate(pred, curr)) {
                Unlock(pred, curr);

                continue;
            }

            if (!curr || curr->value != value) {
                Unlock(pred, curr);

                return false;
            }

            curr->marked.store(true, std::memory_order_relaxed);

            SetNext(pred, curr->next.load());

            curr->mtx.unlock();

            UnlockPred(pred);
            Retire(curr);

            return true;
        }
    }

    bool Contains(int value) const override {
        Node* curr = head_.load(std::memory_order_acquire);

        while (curr && curr->value < value) {
            curr = curr->next.load(std::memory_order_acquire);
        }

        return curr && curr->value == value && !curr->marked.load(std::memory_order_relaxed);
    }

private:
    struct Node {
        Node(int v, Node* n = nullptr)
            : value(v)
        {
            next.store(n);
        }

        int value;
        std::atomic<Node*> next{nullptr};
        mutable std::mutex mtx;
        std::atomic<bool> marked{false};
    };

    std::atomic<Node*> head_{nullptr};
    mutable std::mutex head_mtx_;
    mutable std::atomic<Node*> retired_{nullptr};

    std::pair<Node*, Node*> Find(int value) const {
        Node* pred = nullptr;
        Node* curr = head_.load(std::memory_order_acquire);

        while (curr && curr->value < value) {
            pred = curr;
            curr = curr->next.load(std::memory_order_acquire);
        }

        return {pred, curr};
    }

    bool Validate(Node* pred, Node* curr) const {
        if (pred == nullptr) {
            return head_.load() == curr;
        }

        return !pred->marked.load(std::memory_order_relaxed) &&
               pred->next.load(std::memory_order_relaxed) == curr &&
               (curr == nullptr || !curr->marked.load(std::memory_order_relaxed));
    }

    void LockPred(Node* pred) const {
        if (pred == nullptr) {
            head_mtx_.lock();
        } else {
            pred->mtx.lock();
        }
    }

    void UnlockPred(Node* pred) const {
        if (pred == nullptr) {
            head_mtx_.unlock();
        } else {
            pred->mtx.unlock();
        }
    }

    void Unlock(Node* pred, Node* curr) const {
        if (curr) {
            curr->mtx.unlock();
        }

        UnlockPred(pred);
    }

    void SetNext(Node* pred, Node* next) {
        if (pred == nullptr) {
            head_.store(next, std::memory_order_release);
        } else {
            pred->next.store(next, std::memory_order_release);
        }
    }

    void Retire(Node* node) const {
        Node* head = retired_.load(std::memory_order_relaxed);

        do {
            node->next.store(head, std::memory_order_relaxed);
        } while (!retired_.compare_exchange_weak(
            head, node, std::memory_order_release, std::memory_order_relaxed));
    }
};

