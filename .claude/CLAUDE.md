# ReCpp Instructions for Claude

Additional instructions can be read from `.github/copilot-instructions.md`.

## ReCpp Modules

All headers are in `src/rpp/`. Test files are in `tests/`.

| Header | Purpose |
|--------|---------|
| `config.h` | Platform detection, compiler macros, base types |
| `strview.h` | Non-owning string view with tokenization and search |
| `sprint.h` | String builder, type-safe formatting, to_string |
| `file_io.h` | Cross-platform file read/write, RAII file handles |
| `paths.h` | Path manipulation, directory listing, filesystem utils |
| `delegate.h` | Fast function delegates and multicast events |
| `future.h` | Composable futures with continuations and coroutines |
| `future_types.h` | Supporting types for futures |
| `coroutines.h` | C++20 coroutine awaiters and co_await operators |
| `event_loop.h` | Single-threaded coroutine event loop |
| `thread_pool.h` | Thread pool, parallel_for, parallel_foreach, async tasks |
| `threads.h` | Thread naming, ID queries, CPU core info |
| `mutex.h` | Mutex, spin locks, synchronized<T> wrapper |
| `semaphore.h` | Counting semaphore, semaphore flag, one-shot flag |
| `condition_variable.h` | Condition variable with high-res timeout |
| `concurrent_queue.h` | Thread-safe FIFO queue |
| `close_sync.h` | Read-write sync for safe async destruction |
| `sockets.h` | TCP/UDP sockets, IP addresses, network interfaces |
| `binary_stream.h` | Buffered binary read/write streams |
| `binary_serializer.h` | Reflection-based binary and string serialization |
| `timer.h` | High-precision timers, Duration, TimePoint, StopWatch |
| `vec.h` | 2D/3D/4D vector math, matrices, rectangles |
| `math.h` | Clamp, lerp, deg/rad, epsilon compare |
| `minmax.h` | SSE-optimized min, max, abs, sqrt |
| `collections.h` | Container utilities, ranges, algorithms, erasure helpers |
| `debugging.h` | Logging, assertions, log handlers |
| `stack_trace.h` | Stack tracing and traced exceptions |
| `bitutils.h` | Fixed-length bit array |
| `endian.h` | Endian byte-swap read/write |
| `memory_pool.h` | Linear bump-allocator memory pools |
| `sort.h` | Minimal insertion sort |
| `scope_guard.h` | RAII scope cleanup guard |
| `load_balancer.h` | UDP send rate limiter |
| `obfuscated_string.h` | Compile-time string obfuscation |
| `proc_utils.h` | Process memory and CPU usage info |
| `tests.h` | Minimal unit testing framework |
| `log_colors.h` | ANSI terminal color macros |
| `type_traits.h` | Detection idiom and type trait helpers |
| `traits.h` | Function type traits for callables |
| `jni_cpp.h` | Android JNI C++ utilities |

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
