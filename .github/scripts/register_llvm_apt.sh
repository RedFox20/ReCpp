#!/usr/bin/env bash
# Registers apt.llvm.org for one clang release, for a release the runner image does not carry.
# Usage: register_llvm_apt.sh clang-21
set -euo pipefail

LLVM_VER="${1#*-}" # clang-21 names release 21

wget -qO "$RUNNER_TEMP/llvm.sh" https://apt.llvm.org/llvm.sh
chmod +x "$RUNNER_TEMP/llvm.sh"
sudo "$RUNNER_TEMP/llvm.sh" "$LLVM_VER"

# CMake scans module dependencies with clang-scan-deps, which ships in clang-tools.
# Without it a modules build configures and then fails.
sudo apt-get install -y "clang-tools-$LLVM_VER"

# CMake also accepts the unversioned name, so alias it the way mama aliases clang
sudo update-alternatives --install /usr/bin/clang-scan-deps clang-scan-deps \
                         "/usr/bin/clang-scan-deps-$LLVM_VER" 100

# fail here, where the cause is obvious, instead of at the modules check later
clang-scan-deps --version
