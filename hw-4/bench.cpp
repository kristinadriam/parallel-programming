#include "set.h"

#include <benchmark/benchmark.h>
#include <random>

// 10% insert / 10% remove / 80% contains
template <typename Set>
void BM_Mixed(benchmark::State& state) {
    struct SharedState {
        Set s;

        SharedState() {
            std::mt19937 rng(42);
            for (int i = 0; i < 500; ++i) {
                s.Insert(rng() % 1000);
            }
        }
    };

    static SharedState shared;

    std::mt19937 rng(state.thread_index() * 31337u + 13u);

    for (auto _ : state) {
        int v = rng() % 1000;
        int op = rng() % 100;
        if (op < 10) {
            shared.s.Insert(v);
        } else if (op < 20) {
            shared.s.Remove(v);
        } else {
            shared.s.Contains(v);
        }
    }

    state.SetItemsProcessed(state.iterations());
}

// 100% contains
template <typename Set>
void BM_ReadOnly(benchmark::State& state) {
    struct SharedState {
        Set s;

        SharedState() {
            std::mt19937 rng(42);
            for (int i = 0; i < 500; ++i) {
                s.Insert(rng() % 1000);
            }
        }
    };

    static SharedState shared;

    std::mt19937 rng(state.thread_index() * 31337u + 13u);

    for (auto _ : state) {
        shared.s.Contains(rng() % 1000);
    }

    state.SetItemsProcessed(state.iterations());
}

// 50% insert / 50% remove
template <typename Set>
void BM_WriteHeavy(benchmark::State& state) {
    struct SharedState {
        Set s;

        SharedState() {
            std::mt19937 rng(42);
            for (int i = 0; i < 500; ++i) {
                s.Insert(rng() % 1000);
            }
        }
    };

    static SharedState shared;

    std::mt19937 rng(state.thread_index() * 31337u + 13u);

    for (auto _ : state) {
        int v = rng() % 1000;
        if (rng() % 2) {
            shared.s.Insert(v);
        } else {
            shared.s.Remove(v);
        }
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(BM_Mixed, RawSet)->ThreadRange(1, 8)->UseRealTime();
BENCHMARK_TEMPLATE(BM_Mixed, FineSet)->ThreadRange(1, 8)->UseRealTime();
BENCHMARK_TEMPLATE(BM_Mixed, OptimisticSet)->ThreadRange(1, 8)->UseRealTime();
BENCHMARK_TEMPLATE(BM_Mixed, LazySet)->ThreadRange(1, 8)->UseRealTime();

BENCHMARK_TEMPLATE(BM_ReadOnly, RawSet)->ThreadRange(1, 8)->UseRealTime();
BENCHMARK_TEMPLATE(BM_ReadOnly, FineSet)->ThreadRange(1, 8)->UseRealTime();
BENCHMARK_TEMPLATE(BM_ReadOnly, OptimisticSet)->ThreadRange(1, 8)->UseRealTime();
BENCHMARK_TEMPLATE(BM_ReadOnly, LazySet)->ThreadRange(1, 8)->UseRealTime();

BENCHMARK_TEMPLATE(BM_WriteHeavy, RawSet)->ThreadRange(1, 8)->UseRealTime();
BENCHMARK_TEMPLATE(BM_WriteHeavy, FineSet)->ThreadRange(1, 8)->UseRealTime();
BENCHMARK_TEMPLATE(BM_WriteHeavy, OptimisticSet)->ThreadRange(1, 8)->UseRealTime();
BENCHMARK_TEMPLATE(BM_WriteHeavy, LazySet)->ThreadRange(1, 8)->UseRealTime();

