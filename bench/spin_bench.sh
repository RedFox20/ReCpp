#!/bin/bash
# Builds bench/spin_bench.cpp against the futex_mutex with each spin length, then runs each one twice
set -euo pipefail
cd "$(dirname "$0")/.."
SRC=src/rpp/condition_variable.cpp
SPINS=${SPINS:-"100 20 0"}
lscpu | grep -E "^Architecture|^CPU\(s\)|^Model name|^Vendor ID|^Hypervisor"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_LIBDW=OFF > /dev/null
for spin in $SPINS; do
  sed -i "s/static constexpr int SPIN_ROUNDS = [0-9]*;/static constexpr int SPIN_ROUNDS = $spin;/" $SRC
  grep -q "SPIN_ROUNDS = $spin;" $SRC
  cmake --build build --target ReCpp -j4 > /dev/null
  g++ -std=gnu++20 -O2 -DNDEBUG -Isrc bench/spin_bench.cpp -o build/spin_bench$spin build/libReCpp.a -lpthread -ldl
done
git checkout $SRC
for round in 1 2; do
  for spin in $SPINS; do
    echo "== spin=$spin round=$round"
    build/spin_bench$spin
  done
done
