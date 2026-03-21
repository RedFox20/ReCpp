# ReCpp Instructions for Claude

Additional instructions can be read from `.github/copilot-instructions.md`.

## Installing mama build tool

mama is a Python-based C++ build tool. Install it with pip:
```bash
pip install mama
```

Dependencies installed automatically: `colorama`, `distro`, `keyring`, `keyrings.cryptfile`, `psutil`, `python-dateutil`, `termcolor`.

On Linux, you also need `libdw-dev` for stack tracing support:
```bash
sudo apt-get install libdw-dev
```

## Building with mama

```bash
# basic build and test (C++20)
CXX20=1 mama gcc build test="-vv"

# full reconfigure + rebuild
CXX20=1 mama gcc rebuild test="-vv"

# build with clang instead of gcc
CXX20=1 mama clang build test="-vv"

# run a specific test suite
CXX20=1 mama gcc build test="-vv test_concurrent_queue"

# run a specific test
CXX20=1 mama gcc build test="-vv test_concurrent_queue::push_and_pop"
```

### Address Sanitizer (mama)
```bash
# full rebuild with ASAN (needs rebuild when toggling sanitizer flags)
CXX20=1 mama gcc asan rebuild test="nogdb -vv"

# subsequent builds after initial ASAN rebuild
CXX20=1 mama gcc asan build test="nogdb -vv"
```

### Thread Sanitizer (mama)
```bash
CXX20=1 mama gcc tsan rebuild test="nogdb -vv"

# run a specific test with TSAN until failure (up to N iterations)
CXX20=1 mama gcc tsan build test="nogdb -vv test_concurrent_queue" test_until_failure=20
```

### clang-tidy (mama)
```bash
CXX20=1 mama gcc build clang-tidy test
```

## Building with CMake directly

```bash
# configure and build
cmake -B build -DBUILD_TESTS=ON -DCXX20=ON
cmake --build build

# run tests
./bin/RppTests -vv
```

### Address Sanitizer (CMake)
```bash
cmake -B build -DBUILD_TESTS=ON -DCXX20=ON -DBUILD_WITH_MEM_SAFETY=ON
cmake --build build
./bin/RppTests nogdb -vv
```

### Thread Sanitizer (CMake)
TSAN is only available via mama (`mama gcc tsan`). The CMake `BUILD_WITH_MEM_SAFETY` option enables AddressSanitizer, not ThreadSanitizer.

### clang-tidy (CMake)
```bash
cmake -B build -DBUILD_TESTS=ON -DCXX20=ON -DCMAKE_CXX_CLANG_TIDY=clang-tidy
cmake --build build
```
The project has a `.clang-tidy` config file in the repo root, and `CMAKE_EXPORT_COMPILE_COMMANDS` is enabled automatically.
