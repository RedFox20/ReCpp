#!/usr/bin/env bash
# Registers ppa:ubuntu-toolchain-r/test for one gcc release, which the runner image does not carry.
# Usage: register_gcc_ppa.sh gcc-15
set -euo pipefail

GCC_VER="${1#*-}" # gcc-15 names release 15

sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get update

# fail here, where the cause is obvious, instead of at the compiler search later
if ! apt-cache policy "g++-$GCC_VER" | grep -qE 'Candidate: [0-9]'; then
    echo "g++-$GCC_VER: the PPA publishes no candidate for this runner image" >&2
    exit 1
fi
apt-cache policy "g++-$GCC_VER" | head -2
