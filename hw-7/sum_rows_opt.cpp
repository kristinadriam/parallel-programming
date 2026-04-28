#include <iostream>
#include <vector>
#include <cstdint>
#include <random>
#include <chrono>

using Matrix = std::vector<std::vector<int>>;

Matrix generateMatrix(int n) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(1, 100);

    Matrix m(n, std::vector<int>(n));

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            m[i][j] = dist(rng);
        }
    }

    return m;
}

int64_t sumRowsOptimized(const Matrix& m) {
    int n = static_cast<int>(m.size());
    int64_t total = 0;

    for (int i = 0; i < n; ++i) {
        const int* row = m[i].data();

        int64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
        int j = 0;

        for (; j + 3 < n; j += 4) {
            s0 += row[j];
            s1 += row[j + 1];
            s2 += row[j + 2];
            s3 += row[j + 3];
        }

        for (; j < n; j++) {
            s0 += row[j];
        }

        total += s0 + s1 + s2 + s3;
    }

    return total;
}

int main(int argc, char* argv[]) {
    int n = (argc > 1) ? std::atoi(argv[1]) : 8000;

    std::cout << "Matrix " << n << "x" << n << std::endl;
    Matrix m = generateMatrix(n);

    auto start = std::chrono::high_resolution_clock::now();
    int64_t result = sumRowsOptimized(m);
    auto end = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "sumRowsOptimized: " << result << ", time: " << ms << " ms" << std::endl;

    return 0;
}
