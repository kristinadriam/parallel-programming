#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>
#include <thread>
#include <vector>
#include "ordered_set.hpp"

using Key = int;

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

static bool apply_lf(lockfree::set<Key> &s, const Operation &o) {
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

static const char *op_name(Op o) {
    switch (o) {
        case Op::Add:
            return "add";
        case Op::Remove:
            return "remove";
        case Op::Contains:
            return "contains";
        case Op::IsEmpty:
            return "isEmpty";
    }
    return "?";
}

static std::set<std::vector<char>> legal_outcomes(
    const std::vector<Key> &init,
    const std::vector<Operation> &ops
) {
    const int n = static_cast<int>(ops.size());
    std::vector<int> perm(n);
    for (int i = 0; i < n; ++i) {
        perm[i] = i;
    }

    std::set<std::vector<char>> legal;
    do {
        std::set<Key> ref(init.begin(), init.end());
        std::vector<char> res(n, 0);
        for (int p : perm) {
            res[p] = apply_ref(ref, ops[p]) ? 1 : 0;
        }
        legal.insert(res);
    } while (std::next_permutation(perm.begin(), perm.end()));

    return legal;
}

static std::vector<char> run_concurrent(
    const std::vector<Key> &init,
    const std::vector<Operation> &ops
) {
    const int n = static_cast<int>(ops.size());
    lockfree::set<Key> set;
    for (Key k : init)
        set.add(k);

    std::vector<char> result(n, 0);
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    std::vector<std::thread> ths;
    ths.reserve(n);
    for (int i = 0; i < n; ++i) {
        ths.emplace_back([&, i] {
            ready.fetch_add(1, std::memory_order_acq_rel);
            while (!go.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            result[i] = apply_lf(set, ops[i]) ? 1 : 0;
        });
    }

    while (ready.load(std::memory_order_acquire) != n) {
        std::this_thread::yield();
    }

    go.store(true, std::memory_order_release);

    for (auto &t : ths) {
        t.join();
    }

    return result;
}

int main(int argc, char **argv) {
    std::mt19937_64 rng(0xC0FFEE);

    const int N = 6;
    const int domain = 4;
    int CONFIGS = (argc > 1) ? std::atoi(argv[1]) : 600;
    int REPEATS = (argc > 2) ? std::atoi(argv[2]) : 25;

    std::uniform_int_distribution<int> dkey(0, domain - 1);
    std::uniform_int_distribution<int> dop(0, 3);

    long long total_runs = 0, violations = 0;

    for (int c = 0; c < CONFIGS; ++c) {
        std::set<Key> init_set;
        std::uniform_int_distribution<int> dinit(0, domain);
        int isz = dinit(rng);
        for (int i = 0; i < isz; ++i)
            init_set.insert(dkey(rng));
        std::vector<Key> init(init_set.begin(), init_set.end());

        std::vector<Operation> ops(N);
        for (int i = 0; i < N; ++i) {
            Op o = static_cast<Op>(dop(rng));
            ops[i] = Operation{o, dkey(rng)};
        }

        auto legal = legal_outcomes(init, ops);

        for (int r = 0; r < REPEATS; ++r) {
            auto observed = run_concurrent(init, ops);
            ++total_runs;
            if (legal.find(observed) == legal.end()) {
                ++violations;
                std::cout << "нарушение линеаризуемости (config " << c
                          << ", repeat " << r << ")\n  init = {";
                for (Key k : init)
                    std::cout << k << " ";
                std::cout << "}\n  ops:\n";
                for (int i = 0; i < N; ++i) {
                    std::cout << "    op" << i << ": " << op_name(ops[i].op);
                    if (ops[i].op != Op::IsEmpty)
                        std::cout << "(" << ops[i].arg << ")";
                    std::cout << " => observed "
                              << (observed[i] ? "true" : "false") << "\n";
                }
                std::cout << "  observed-кортеж нет ни в одном из "
                          << legal.size() << " легальных порядков\n";
            }
        }
    }

    std::cout << "\nпрогонов: " << total_runs << "  конфигураций: " << CONFIGS
              << "  нарушений: " << violations << "\n";
    if (violations == 0) {
        std::cout << "OK: все конкурентные исходы линеаризуемы.\n";
        return 0;
    }
    std::cout << "FAIL: обнаружены нелинеаризуемые исходы.\n";
    return 1;
}
