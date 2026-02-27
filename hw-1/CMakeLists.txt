cmake_minimum_required(VERSION 3.10)

set(CMAKE_OSX_ARCHITECTURES "x86_64")

project(VectorSum)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vector_sum main.cpp)

if(MSVC)
    target_compile_options(vector_sum PRIVATE /arch:AVX /O2)
else()
    target_compile_options(vector_sum PRIVATE -mavx -O3)
endif()