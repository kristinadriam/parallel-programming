#!/usr/bin/env bash

set -e
SRC=reorder_mp.cpp
ITERS=${1:-200000000}
CXX=${CXX:-g++}
command -v "$CXX" >/dev/null 2>&1 || CXX=clang++

echo "== native ($(uname -m)), компилятор: $CXX, ядер: $(nproc) =="
for M in FLAG_PLAIN FLAG_ATOMIC_RELAXED FLAG_ATOMIC_ACQREL; do
    $CXX -O2 -std=c++17 -pthread -D$M $SRC -o "native_${M}"
    echo "  собрано native_${M}"
done

if command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
    echo "== ARM aarch64 (кросс, статически, для qemu) =="
    for M in FLAG_PLAIN FLAG_ATOMIC_RELAXED FLAG_ATOMIC_ACQREL; do
        aarch64-linux-gnu-g++ -O2 -std=c++17 -static -pthread -D$M $SRC -o "arm_${M}"
        echo "  собрано arm_${M}"
    done
else
    echo "(нет aarch64-linux-gnu-g++ — ARM пропущен"
fi

echo; file native_FLAG_PLAIN | sed 's/^/  /'
[ "$(nproc)" -lt 2 ] && echo "!! ВНИМАНИЕ: 1 ядро — reordering невозможен в принципе, нужно >=2 ядер"

echo "== запуск native =="
for M in FLAG_PLAIN FLAG_ATOMIC_RELAXED FLAG_ATOMIC_ACQREL; do ./native_${M} $ITERS; echo; done

if ls arm_FLAG_PLAIN >/dev/null 2>&1 && command -v qemu-aarch64-static >/dev/null 2>&1; then
    echo "== запуск ARM под QEMU =="
    for M in FLAG_PLAIN FLAG_ATOMIC_RELAXED FLAG_ATOMIC_ACQREL; do qemu-aarch64-static ./arm_${M} $((ITERS/20)); echo; done
fi
