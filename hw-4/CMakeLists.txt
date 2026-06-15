cmake_minimum_required(VERSION 3.20)
project(IntSet CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)
find_package(Threads REQUIRED)

add_executable(run_tests test.cpp)
target_link_libraries(run_tests PRIVATE GTest::gtest_main Threads::Threads)

add_executable(run_bench bench.cpp)
target_link_libraries(run_bench PRIVATE benchmark::benchmark_main Threads::Threads)

include(GoogleTest)
gtest_discover_tests(run_tests)
