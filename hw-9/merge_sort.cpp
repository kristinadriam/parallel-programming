#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

static void merge(
    std::vector<int> &a,
    std::vector<int> &buf,
    std::size_t left,
    std::size_t mid,
    std::size_t right
) {
    std::size_t l = left, m = mid, r = left;
    while (l < mid && m < right) {
        if (a[l] <= a[m]) {
            buf[r++] = a[l++];
        } else {
            buf[r++] = a[m++];
        }
    }

    while (l < mid) {
        buf[r++] = a[l++];
    }

    while (m < right) {
        buf[r++] = a[m++];
    }

    for (std::size_t t = left; t < right; ++t) {
        a[t] = buf[t];
    }
}

// однопоточная версия
static const std::size_t INSERTION_THRESHOLD = 32;

static void
insertion_sort(std::vector<int> &a, std::size_t left, std::size_t right) {
    for (std::size_t i = left + 1; i < right; ++i) {
        int key = a[i];

        std::size_t j = i;
        while (j > left && a[j - 1] > key) {
            a[j] = a[j - 1];
            --j;
        }

        a[j] = key;
    }
}

static void merge_sort_sequence(
    std::vector<int> &a,
    std::vector<int> &buf,
    std::size_t left,
    std::size_t right
) {
    if (right - left <= INSERTION_THRESHOLD) {
        insertion_sort(a, left, right);
        return;
    }

    std::size_t mid = left + (right - left) / 2;
    merge_sort_sequence(a, buf, left, mid);
    merge_sort_sequence(a, buf, mid, right);
    merge(a, buf, left, mid, right);
}

void merge_sort_single(std::vector<int> &a) {
    std::vector<int> buf(a.size());
    merge_sort_sequence(a, buf, 0, a.size());
}

// многопоточная версия
static const std::size_t PARALLEL_THRESHOLD = 1 << 15;

static void merge_sort_parallel(
    std::vector<int> &a,
    std::vector<int> &buf,
    std::size_t left,
    std::size_t right,
    int depth
) {
    if (depth <= 0 || (right - left) <= PARALLEL_THRESHOLD) {
        merge_sort_sequence(a, buf, left, right);
        return;
    }

    std::size_t mid = left + (right - left) / 2;

    std::thread thread([&] {
        merge_sort_parallel(a, buf, left, mid, depth - 1);
    });

    merge_sort_parallel(a, buf, mid, right, depth - 1);

    thread.join();
    merge(a, buf, left, mid, right);
}

void merge_sort_multi(std::vector<int> &a) {
    std::vector<int> buf(a.size());
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) {
        hw = 2;
    }

    int max_depth = 0;
    while ((1u << max_depth) < hw) {
        ++max_depth;
    }

    merge_sort_parallel(a, buf, 0, a.size(), max_depth);
}

// тест
template <typename F>
double time_ms(F &&f) {
    auto t0 = std::chrono::high_resolution_clock::now();
    f();
    auto t1 = std::chrono::high_resolution_clock::now();

    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

std::vector<int> make_data(size_t n, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 1'000'000'000);

    std::vector<int> v(n);
    for (auto &x : v) {
        x = dist(rng);
    }

    return v;
}

int main() {
    std::cout << "hardware_concurrency: " << std::thread::hardware_concurrency()
              << "\n\n";

    std::vector<std::size_t> sizes = {1000,      10'000,    100'000,
                                      1'000'000, 5'000'000, 20'000'000};
    const int RUNS = 3;

    std::cout << std::left << std::setw(12) << "N" << std::setw(16)
              << "single (ms)" << std::setw(16) << "multi (ms)" << std::setw(12)
              << "speedup"
              << "correctness\n";
    std::cout << std::string(68, '-') << "\n";

    for (std::size_t n : sizes) {
        double s_total = 0, m_total = 0;
        bool ok = true;

        for (int r = 0; r < RUNS; ++r) {
            std::vector<int> base = make_data(n, 42 + r);

            std::vector<int> a1 = base;
            s_total += time_ms([&] { merge_sort_single(a1); });

            std::vector<int> a2 = base;
            m_total += time_ms([&] { merge_sort_multi(a2); });

            if (!std::is_sorted(a1.begin(), a1.end())) {
                ok = false;
            }
            if (a1 != a2) {
                ok = false;
            }
        }

        double s = s_total / RUNS;
        double m = m_total / RUNS;
        std::cout << std::left << std::setw(12) << n << std::setw(16)
                  << std::fixed << std::setprecision(3) << s << std::setw(16)
                  << m << std::setw(12) << std::setprecision(2) << (s / m)
                  << (ok ? "OK" : "ERROR") << "\n";
    }
    return 0;
}
