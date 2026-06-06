#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <random>
#include <set>
#include <thread>
#include <vector>

using Key = int;

class BuggySet {
    std::set<Key> s_;
    std::mutex m_;

public:
    bool add(const Key &v) {
        bool absent;
        {
            std::lock_guard<std::mutex> g(m_);
            absent = s_.count(v) == 0;
        }
        std::this_thread::yield();
        {
            std::lock_guard<std::mutex> g(m_);
            s_.insert(v);
        }
        return absent;
    }

    bool remove(const Key &v) {
        std::lock_guard<std::mutex> g(m_);
        return s_.erase(v) > 0;
    }

    bool contains(const Key &v) {
        std::lock_guard<std::mutex> g(m_);
        return s_.count(v) > 0;
    }

    bool isEmpty() {
        std::lock_guard<std::mutex> g(m_);
        return s_.empty();
    }
};

enum class Op { Add, Remove, Contains, IsEmpty };

struct Operation {
    Op op;
    Key arg;
};

static bool apply_ref(std::set<Key> &s, const Operation &o) {
    switch (o.op) {
        case Op::Add:
            return s.insert(o.arg).second;
        case Op::Remove:
            return s.erase(o.arg) > 0;
        case Op::Contains:
            return s.count(o.arg) > 0;
        case Op::IsEmpty:
            return s.empty();
    }
    return false;
}

static bool apply_buggy(BuggySet &s, const Operation &o) {
    switch (o.op) {
        case Op::Add:
            return s.add(o.arg);
        case Op::Remove:
            return s.remove(o.arg);
        case Op::Contains:
            return s.contains(o.arg);
        case Op::IsEmpty:
            return s.isEmpty();
    }
    return false;
}

static std::set<std::vector<char>> legal_outcomes(
    const std::vector<Key> &init,
    const std::vector<Operation> &ops
) {
    const int n = (int)ops.size();
    std::vector<int> perm(n);
    for (int i = 0; i < n; ++i) {
        perm[i] = i;
    }

    std::set<std::vector<char>> legal;
    do {
        std::set<Key> ref(init.begin(), init.end());
        std::vector<char> res(n, 0);
        for (int p : perm)
            res[p] = apply_ref(ref, ops[p]) ? 1 : 0;
        legal.insert(res);
    } while (std::next_permutation(perm.begin(), perm.end()));

    return legal;
}

static std::vector<char> run_concurrent(
    const std::vector<Key> &init,
    const std::vector<Operation> &ops
) {
    const int n = (int)ops.size();
    BuggySet set;
    for (Key k : init) {
        set.add(k);
    }

    std::vector<char> result(n, 0);
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> ths;
    for (int i = 0; i < n; ++i) {
        ths.emplace_back([&, i] {
            ready.fetch_add(1);
            while (!go.load()) {
                std::this_thread::yield();
            }
            result[i] = apply_buggy(set, ops[i]) ? 1 : 0;
        });
    }

    while (ready.load() != n) {
        std::this_thread::yield();
    }

    go.store(true);

    for (auto &t : ths) {
        t.join();
    }

    return result;
}

int main(int argc, char **argv) {
    std::mt19937_64 rng(0xC0FFEE);

    const int N = 6;
    const int domain = 4;

    int CONFIGS = argc > 1 ? std::atoi(argv[1]) : 400;
    int REPEATS = argc > 2 ? std::atoi(argv[2]) : 25;

    std::uniform_int_distribution<int> dkey(0, domain - 1), dop(0, 3);

    long long runs = 0, violations = 0;
    for (int c = 0; c < CONFIGS && violations < 3; ++c) {
        std::set<Key> init_set;
        std::uniform_int_distribution<int> dinit(0, domain);
        int isz = dinit(rng);
        for (int i = 0; i < isz; ++i) {
            init_set.insert(dkey(rng));
        }

        std::vector<Key> init(init_set.begin(), init_set.end());
        std::vector<Operation> ops(N);
        for (int i = 0; i < N; ++i) {
            ops[i] = {static_cast<Op>(dop(rng)), dkey(rng)};
        }

        auto legal = legal_outcomes(init, ops);
        for (int r = 0; r < REPEATS; ++r) {
            auto obs = run_concurrent(init, ops);
            ++runs;
            if (legal.find(obs) == legal.end()) {
                ++violations;
                std::cout << "поймано нарушение (config " << c
                          << "): observed-кортеж нелегален\n";
            }
        }
    }

    std::cout << "прогонов: " << runs << "  нарушений: " << violations << "\n";
    std::cout << (violations ? "Харнесс поймал баг.\n" : "баг не пойман =(\n");

    return 0;
}
