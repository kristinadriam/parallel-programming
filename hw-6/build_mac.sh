#!/usr/bin/env bash

set -e
SRC=reorder_mp.cpp
ITERS=${1:-200000000}
 
echo "uname -m : $(uname -m)"
echo "ядер     : $(sysctl -n hw.ncpu)"
echo
 
for M in FLAG_PLAIN FLAG_ATOMIC_RELAXED FLAG_ATOMIC_ACQREL; do
    clang++ -O2 -std=c++17 -D$M $SRC -o "arm_${M}"
done
file arm_FLAG_PLAIN | sed 's/^/  /'
echo
 
for M in FLAG_PLAIN FLAG_ATOMIC_RELAXED FLAG_ATOMIC_ACQREL; do ./arm_${M} $ITERS; echo; done