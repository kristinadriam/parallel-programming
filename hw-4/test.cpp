#include "set.h"

#include <gtest/gtest.h>
#include <thread>
#include <random>

template <typename T>
class IntSetTest: public ::testing::Test {};

using SetTypes = ::testing::Types<RawSet, FineSet, OptimisticSet, LazySet>;
TYPED_TEST_SUITE(IntSetTest, SetTypes);

TYPED_TEST(IntSetTest, InsertAndContains) {
    TypeParam s;

    EXPECT_TRUE(s.Insert(5));
    EXPECT_TRUE(s.Contains(5));
    EXPECT_FALSE(s.Contains(4));
    EXPECT_FALSE(s.Contains(6));
}

TYPED_TEST(IntSetTest, DuplicateInsert) {
    TypeParam s;

    EXPECT_TRUE(s.Insert(10));
    EXPECT_FALSE(s.Insert(10));
    EXPECT_TRUE(s.Contains(10));
}

TYPED_TEST(IntSetTest, RemovePresentElement) {
    TypeParam s;

    for (int i = 1; i <= 100; ++i) {
        s.Insert(i);
    }

    for (int i = 2; i <= 100; i += 2) {
        EXPECT_TRUE(s.Remove(i));
        EXPECT_FALSE(s.Contains(i));
    }

    for (int i = 1; i <= 100; i += 2) {
        EXPECT_TRUE(s.Contains(i));
    }
}

TYPED_TEST(IntSetTest, RemoveAbsentElement) {
    TypeParam s;
    EXPECT_FALSE(s.Remove(42));

    s.Insert(1);
    s.Insert(3);

    EXPECT_FALSE(s.Remove(2));
    EXPECT_TRUE(s.Contains(1));
    EXPECT_TRUE(s.Contains(3));
}

TYPED_TEST(IntSetTest, RemoveDuplicate) {
    TypeParam s;

    s.Insert(7);

    EXPECT_TRUE(s.Remove(7));
    EXPECT_FALSE(s.Remove(7));
    EXPECT_FALSE(s.Contains(7));
}

TYPED_TEST(IntSetTest, ReverseOrderInsertion) {
    TypeParam s;

    for (int i = 100; i >= 1; --i) {
        EXPECT_TRUE(s.Insert(i));
    }

    for (int i = 1; i <= 100; ++i) {
        EXPECT_TRUE(s.Contains(i));
    }
}

TYPED_TEST(IntSetTest, ConcurrentMixedOps) {
    TypeParam s;

    for (int i = 0; i < 100; ++i) {
        s.Insert(i);
    }

    constexpr int kRange = 200;
    constexpr int kThreads = 4;
    constexpr int kOpsPerThread = 50000;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&s, t]() {
            std::mt19937 rng(t * 12345u + 67890u);

            for (int i = 0; i < kOpsPerThread; ++i) {
                int v = rng() % kRange;
                int op = rng() % 100;

                if (op < 20) {
                    s.Insert(v);
                } else if (op < 40) {
                    s.Remove(v);
                } else {
                    s.Contains(v);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    for (int i = 0; i < kRange; ++i) {
        s.Remove(i);
    }

    for (int i = 0; i < kRange; ++i) {
        EXPECT_FALSE(s.Contains(i));
    }
}

