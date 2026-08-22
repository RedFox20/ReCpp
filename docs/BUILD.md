# Building ReCpp

## Installing mama build tool

mama is a Python-based C++ build tool. Install it with pip:
```bash
pip install mama
```

pip also installs these dependencies: `colorama`, `distro`, `keyring`,
`keyrings.cryptfile`, `psutil`, `python-dateutil`, `termcolor`.

On Linux, you also need `libdw-dev` for stack tracing support:
```bash
sudo apt-get install libdw-dev
```

## Building with mama

The build picks C++23 on gcc 13, clang 16, Apple clang 15, and MSVC 19.35 or newer.
Every older compiler gets C++20. Set `CXX20=1` to pin the older standard, and
`CXX26=1` to test the newest one.

The `nogdb` argument reduces the output noise. Omit it if you want mama to attach
GDB when it starts the tests.

```bash
# basic build and test, with TSAN
mama gcc tsan build test="nogdb -vv"

# full reconfigure + rebuild + test
mama gcc tsan rebuild test="nogdb -vv"

# build project with clang instead of gcc
mama clang tsan build test="nogdb -vv"

# build project and run a specific test suite
mama gcc tsan build test="nogdb -vv test_concurrent_queue"

# build project and run a specific test
mama gcc tsan build test="nogdb -vv test_concurrent_queue::push_and_pop"

# build project and run a specific test until failure (up to N iterations)
mama gcc tsan build test="nogdb -vv test_concurrent_queue::push_and_pop" test_until_failure=20
```

### Windows

MSVC is the default compiler on Windows, so the command names no compiler and no
`nogdb`. The sanitizer options still work.

```cmd
mama build test="-vv"
```

### Address Sanitizer (mama)
You cannot combine ASAN and TSAN. Use ASAN only when you debug a memory error. ASAN
needs a reconfigure. Omit `nogdb` here. mama then attaches GDB to the tests, and GDB
gives a full stack trace on a fatal crash.
```bash
mama gcc asan configure build test="-vv"
```

### clang-tidy (mama)
```bash
mama gcc tsan build clang-tidy test="nogdb -vv"
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
mama is the only way to get TSAN (`mama gcc tsan`). The CMake `BUILD_WITH_MEM_SAFETY`
option enables AddressSanitizer, not ThreadSanitizer.

### clang-tidy (CMake)
```bash
cmake -B build -DBUILD_TESTS=ON -DCXX20=ON -DCMAKE_CXX_CLANG_TIDY=clang-tidy
cmake --build build
```
The repo root holds a `.clang-tidy` config file. CMake enables
`CMAKE_EXPORT_COMPILE_COMMANDS` automatically.
