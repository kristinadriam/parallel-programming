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

int64_t sumColsOptimized(const Matrix& m) {
    int n = static_cast<int>(m.size());
    std::vector<int64_t> colSums(n, 0);

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            colSums[j] += m[i][j];

    int64_t total = 0;
    for (int j = 0; j < n; ++j) {
        total += colSums[j];
    }

    return total;
}

int main(int argc, char* argv[]) {
    int n = (argc > 1) ? std::atoi(argv[1]) : 8000;

    std::cout << "Matrix " << n << "x" << n << std::endl;
    Matrix m = generateMatrix(n);

    auto start = std::chrono::high_resolution_clock::now();
    int64_t result = sumColsOptimized(m);
    auto end = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "sumColsOptimized: " << result << ", time: " << ms << " ms" << std::endl;

    return 0;
}
