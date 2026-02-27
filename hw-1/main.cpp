#include <immintrin.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

const std::size_t N = 1'000'000;

void naive_sum(const float *a, const float *b, float *res, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        res[i] = a[i] + b[i];
    }
}

void sse_sum(const float *a, const float *b, float *res, std::size_t n) {
    std::size_t i = 0;

    for (; i <= n - 4; i += 4) {
        __m128 va = _mm_loadu_ps(&a[i]);
        __m128 vb = _mm_loadu_ps(&b[i]);

        __m128 vres = _mm_add_ps(va, vb);

        _mm_storeu_ps(&res[i], vres);
    }

    for (; i < n; ++i) {
        res[i] = a[i] + b[i];
    }
}

void avx_sum(const float *a, const float *b, float *res, std::size_t n) {
    std::size_t i = 0;

    for (; i <= n - 8; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);

        __m256 vres = _mm256_add_ps(va, vb);

        _mm256_storeu_ps(&res[i], vres);
    }

    for (; i < n; ++i) {
        res[i] = a[i] + b[i];
    }
}

bool check_results(const float *res1, const float *res2, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        if (std::abs(res1[i] - res2[i]) > 1e-5) {
            return false;
        }
    }
    return true;
}

int main() {
    std::vector<float> a(N);
    std::vector<float> b(N);
    std::vector<float> naive_res(N);
    std::vector<float> sse_res(N);
    std::vector<float> avx_res(N);

    // load data
    {
        std::ifstream file("data/a.txt");
        if (!file.is_open()) {
            std::cerr << "Failed to open data/a.txt\n";
            return 1;
        }
        for (std::size_t i = 0; i < N && file; ++i) {
            file >> a[i];
        }
    }

    {
        std::ifstream file("data/b.txt");
        if (!file.is_open()) {
            std::cerr << "Failed to open data/b.txt\n";
            return 1;
        }
        for (std::size_t i = 0; i < N && file; ++i) {
            file >> b[i];
        }
    }

    // cache warming
    naive_sum(a.data(), b.data(), naive_res.data(), N);
    sse_sum(a.data(), b.data(), sse_res.data(), N);
    avx_sum(a.data(), b.data(), avx_res.data(), N);

    // time measuring
    const int RUNS = 10;
    auto measure_us = [](auto &&fn) {
        auto start = std::chrono::high_resolution_clock::now();
        fn();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   end - start
        )
            .count();
    };

    long long t_naive = 0, t_sse = 0, t_avx = 0;
    for (int r = 0; r < RUNS; ++r) {
        t_naive += measure_us([&]() {
            naive_sum(a.data(), b.data(), naive_res.data(), N);
        });
        t_sse += measure_us([&]() {
            sse_sum(a.data(), b.data(), sse_res.data(), N);
        });
        t_avx += measure_us([&]() {
            avx_sum(a.data(), b.data(), avx_res.data(), N);
        });
    }
    t_naive /= RUNS;
    t_sse /= RUNS;
    t_avx /= RUNS;

    std::cout << "Naive time: " << t_naive << " us\n";

    std::cout << "SSE time:   " << t_sse << " us";
    std::cout << check_results(naive_res.data(), sse_res.data(), N)
        ? " (OK)\n"
        : " (FAIL)\n";

    std::cout << "AVX time:   " << t_avx << " us";
    std::cout << check_results(naive_res.data(), avx_res.data(), N)
        ? " (OK)\n"
        : " (FAIL)\n";

    return 0;
}