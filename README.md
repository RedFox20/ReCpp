# ReCpp

[![CircleCI](https://circleci.com/gh/RedFox20/ReCpp.svg?style=shield)](https://circleci.com/gh/RedFox20/ReCpp)
[![Linux GCC](https://img.shields.io/badge/Linux-GCC_13--14-blue?logo=linux)](https://circleci.com/gh/RedFox20/ReCpp)
[![Linux Clang](https://img.shields.io/badge/Linux-Clang_18-blue?logo=llvm)](https://circleci.com/gh/RedFox20/ReCpp)
[![Windows MSVC](https://img.shields.io/badge/Windows-MSVC_2022-blue?logo=windows)](https://circleci.com/gh/RedFox20/ReCpp)
[![Android Clang](https://img.shields.io/badge/Android-NDK_r29b_Clang_18-blue?logo=android)](https://circleci.com/gh/RedFox20/ReCpp)
[![MIPS GCC](https://img.shields.io/badge/MIPS-GCC_12-blue)](https://circleci.com/gh/RedFox20/ReCpp)
## Reusable Standalone C++ Libraries and Modules

The goal of this project is to provide C++20 and C++23 developers with convenient and minimal cross-platform C++ utility libraries.
All of the modules are heavily performance oriented and provide the least amount of overhead possible.

### Supported Platforms

| Platform | Compilers | Notes |
|----------|-----------|-------|
| **Windows** | MSVC++ 2019, 2022, 2026 | x64 |
| **Ubuntu Linux** | clang++ 14–20, g++ 11–15 | x86_64, ASan/TSan/clang-tidy CI |
| **Android** | NDK r25b–r29 (clang++ 14–20) | arm64-v8a, armeabi-v7a, QEMU aarch64 CI tests |
| **iOS / macOS** | Apple Clang | arm64, x86_64 |
| **AArch64 Embedded** | g++ 11–15 | i.MX8M+, Xilinx Zynq UltraScale+, Ambarella CV25 |
| **MIPSEL** | g++ 11 | Cross-compiled, no tests |


## Building

ReCpp uses [mama](https://github.com/RedFox20/mama) build system with CMake.
You can either build it as a regular CMake project or use the provided [mamafile.py](mamafile.py) for more convenient builds and testing.

```bash
cmake -B build -S .
cmake --build build
```

```bash
# Build with tests (defaults to GCC on Linux or MSVC on Windows)
mama build with_tests

# Build debug with tests and run with gdb
mama build debug test

# Run specific tests with clang and sanitizers
mama clang clang-tidy asan build test="test_strview"

# Run tests until failure with GCC and ThreadSanitizer
mama gcc tsan build test test_until_failure
```

### Clang-Tidy Analysis

Run clang-tidy locally without building with `clang-tidy` enabled:
```bash
./run_clang_tidy              # quiet mode, uses all cores
./run_clang_tidy -j 4         # limit to 4 jobs
./run_clang_tidy -v           # verbose (show per-file commands)
./run_clang_tidy --android    # analyze Android NDK build
```

### Android Testing (QEMU)

Run Android aarch64 unit tests locally using QEMU user-mode emulation.
Requires `ANDROID_NDK_HOME` to be set. Installs `qemu-user` automatically on first run.

```bash
./run_android_tests --build -vv                   # build and run all tests
./run_android_tests -vv test_delegate             # run specific test suite
./run_android_tests -vv test_delegate::virtuals   # run specific test case
```

Uses a pre-built bionic sysroot from Android API 29 ([`.circleci/android-qemu-sysroot-api29/`](.circleci/android-qemu-sysroot-api29/))
with a custom `liblog.so` that redirects `__android_log_*` to stdout/stderr with colored priority labels.


## Experimental: C++20 Modules

ReCpp has experimental support for C++20 named modules. This allows importing ReCpp headers as modules instead of using `#include`, providing faster compilation and better isolation.

### Requirements

| Requirement | Minimum |
|-------------|---------|
| C++ standard | C++20 |
| CMake | 3.28+ |
| MSVC | 19.34+ (VS 2022 17.4) |
| Clang | 16+ (18+ recommended) |
| GCC | 14+ |
| Generator | Ninja (recommended) |

### Building with modules

Enable the `BUILD_WITH_MODULES` option alongside C++20:
```bash
# mama
CXX20=1 BUILD_WITH_MODULES=1 mama clang tsan rebuild test="-vv test_strview_module"

# CMake
cmake -B build -G Ninja -DCXX20=ON -DBUILD_TESTS=ON -DBUILD_WITH_MODULES=ON
cmake --build build
```

### Using modules

Module interface units are in `src/rpp/` with the naming convention `rpp-<module>.cppm`.
Each module wraps an existing header using the global module fragment pattern, so the traditional `#include` approach continues to work alongside modules.

```cpp
// Traditional header include (still works as before)
#include <rpp/strview.h>

// C++20 module import
import rpp.strview;
```

Macros (`RPPAPI`, `LogError`, `Assert`, `_sv` literal, etc.) cannot be exported from C++20 modules by design. If you need macros, combine the module import with a traditional include:
```cpp
import rpp.strview;
#include <rpp/config.h>      // for RPPAPI, RPP_ENABLE_UNICODE, etc.
#include <rpp/debugging.h>   // for LogError, Assert, etc.
```

### Available modules

| Module | Header | Exported types |
|--------|--------|----------------|
| `rpp.strview` | [`strview.h`](src/rpp/strview.h) | `strview`, `ustrview`, `line_parser`, `keyval_parser`, `bracket_parser`, `concat`, `to_lower`, `to_upper`, `replace`, `_sv` literal, and more |

### How it works

The module interface units use the **global module fragment** to include existing headers, then re-export selected types via `using`-declarations. This is the same pattern used by `libstdc++` and `libc++` to implement `import std;`.

```cpp
// src/rpp/rpp-strview.cppm
module;
#include "strview.h"          // included in the global module fragment
export module rpp.strview;

export namespace rpp {
    using rpp::strview;       // re-export as "visible" to importers
    using rpp::line_parser;
    // ...
}
```

---

## Table of Contents

| Module | Header | Description |
|--------|--------|-------------|
| [Config](#rppconfigh) | [`config.h`](src/rpp/config.h) | Platform detection, compiler macros, base type definitions |
| [String View](#rppstrviewh) | [`strview.h`](src/rpp/strview.h) | Lightweight non-owning string view with fast tokenization, parsing, and search |
| [String Formatting](#rppsprinth) | [`sprint.h`](src/rpp/sprint.h) | String builder, type-safe formatting, and `to_string` conversions |
| [File I/O](#rppfile_ioh) | [`file_io.h`](src/rpp/file_io.h) | Cross-platform file read/write with RAII file handles |
| [Paths](#rpppathsh) | [`paths.h`](src/rpp/paths.h) | Path manipulation, directory listing, and filesystem utilities |
| [Delegate](#rppdelegateh) | [`delegate.h`](src/rpp/delegate.h) | Fast function delegates and multicast events |
| [Futures](#rppfutureh) | [`future.h`](src/rpp/future.h) | Composable futures with continuations and C++20 coroutine support |
| [Coroutines](#rppcoroutinesh) | [`coroutines.h`](src/rpp/coroutines.h) | C++20 coroutine awaiters and `co_await` operators |
| [Event Loop](#rppevent_looph) | [`event_loop.h`](src/rpp/event_loop.h) | Single-threaded coroutine event loop for serializing async completions |
| [Thread Pool](#rppthread_poolh) | [`thread_pool.h`](src/rpp/thread_pool.h) | Thread pool with `parallel_for`, `parallel_foreach`, and async tasks |
| [Threads](#rppthreadsh) | [`threads.h`](src/rpp/threads.h) | Thread naming, ID queries, and CPU core info |
| [Mutex](#rppmutexh) | [`mutex.h`](src/rpp/mutex.h) | Cross-platform mutex, spin locks, and `synchronized<T>` wrapper |
| [Semaphore](#rppsemaphoreh) | [`semaphore.h`](src/rpp/semaphore.h) | Counting semaphore, semaphore flag, and one-shot flag |
| [Condition Variable](#rppcondition_variableh) | [`condition_variable.h`](src/rpp/condition_variable.h) | Condition variable with high-resolution timeout support |
| [Sockets](#rppsocketsh) | [`sockets.h`](src/rpp/sockets.h) | Cross-platform TCP/UDP/AF_UNIX sockets, IP addresses, network interfaces, fd passing (SCM_RIGHTS) |
| [Binary Stream](#rppbinary_streamh) | [`binary_stream.h`](src/rpp/binary_stream.h) | Buffered binary read/write streams |
| [Binary Serializer](#rppbinary_serializerh) | [`binary_serializer.h`](src/rpp/binary_serializer.h) | Reflection-based binary and string serialization |
| [Atomic Timepoint](#rppatomic_timepointh) | [`atomic_timepoint.h`](src/rpp/atomic_timepoint.h) | Lock-free atomic Duration and TimePoint |
| [Atomic Shared Ptr](#rppatomic_shared_ptrh) | [`atomic_shared_ptr.h`](src/rpp/atomic_shared_ptr.h) | Portable atomic shared_ptr and weak_ptr |
| [Timer](#rpptimer) | [`timer.h`](src/rpp/timer.h) | High-precision timers, Duration, TimePoint, and StopWatch |
| [Vectors & Math](#rppvech) | [`vec.h`](src/rpp/vec.h) | 2D/3D/4D vector math, matrices, rectangles, and geometry |
| [Math](#rppmath) | [`math.h`](src/rpp/math.h) | Basic math utilities: clamp, lerp, deg/rad, epsilon compare |
| [Min/Max](#rppminmaxh) | [`minmax.h`](src/rpp/minmax.h) | SSE-optimized min, max, abs, sqrt |
| [Collections](#rppcollectionsh) | [`collections.h`](src/rpp/collections.h) | Container utilities, ranges, algorithms, and erasure helpers |
| [Concurrent Queue](#rppconcurrent_queueh) | [`concurrent_queue.h`](src/rpp/concurrent_queue.h) | Thread-safe FIFO queue |
| [Debugging](#rppdebugging) | [`debugging.h`](src/rpp/debugging.h) | Cross-platform logging, assertions, and log handlers |
| [Stack Trace](#rppstack_traceh) | [`stack_trace.h`](src/rpp/stack_trace.h) | Stack tracing and traced exceptions |
| [Bit Utils](#rppbitutilsh) | [`bitutils.h`](src/rpp/bitutils.h) | Fixed-length bit array |
| [Close Sync](#rppclose_synch) | [`close_sync.h`](src/rpp/close_sync.h) | Read-write synchronization for safe async destruction |
| [Endian](#rppendianh) | [`endian.h`](src/rpp/endian.h) | Endian byte-swap read/write utilities |
| [Memory Pool](#rppmemory_poolh) | [`memory_pool.h`](src/rpp/memory_pool.h) | Linear bump-allocator memory pools |
| [Sort](#rppsorth) | [`sort.h`](src/rpp/sort.h) | Minimal insertion sort |
| [Scope Guard](#rppscope_guardh) | [`scope_guard.h`](src/rpp/scope_guard.h) | RAII scope cleanup guard |
| [Load Balancer](#rppload_balancerh) | [`load_balancer.h`](src/rpp/load_balancer.h) | UDP send rate limiter |
| [Obfuscated String](#rppobfuscated_stringh) | [`obfuscated_string.h`](src/rpp/obfuscated_string.h) | Compile-time string obfuscation |
| [Process Utils](#rppproc_utilsh) | [`proc_utils.h`](src/rpp/proc_utils.h) | Process memory and CPU usage info |
| [Tests](#rpptestsh) | [`tests.h`](src/rpp/tests.h) | Minimal unit testing framework |
| [Log Colors](#rpplog_colorsh) | [`log_colors.h`](src/rpp/log_colors.h) | ANSI terminal color macros |
| [Type Traits](#rpptype_traitsh) | [`type_traits.h`](src/rpp/type_traits.h) | Detection idiom and type trait helpers |
| [Traits](#rpptraitsh) | [`traits.h`](src/rpp/traits.h) | Function type traits for callables |
| [JNI C++](#rppjni_cpph) | [`jni_cpp.h`](src/rpp/jni_cpp.h) | Android JNI C++ utilities |

---

## rpp/config.h

Platform detection, compiler macros, and base type definitions. This is the foundational header included by all other ReCpp modules.

### Compiler Detection

| Macro | Description |
|-------|-------------|
| [`RPP_MSVC`](src/rpp/config.h#L5) | Evaluates to `_MSC_VER` when compiling with MSVC, regardless of standard library |
| [`RPP_MSVC_WIN`](src/rpp/config.h#L11) | `1` when compiling with MSVC on Windows (`_WIN32 && _MSC_VER`) |
| [`RPP_GCC`](src/rpp/config.h#L20) | `1` when compiling with GCC (excludes Clang) |
| [`RPP_CLANG_LLVM`](src/rpp/config.h#L29) | `1` when compiling with Clang using GCC/LLVM standard libs |

### C++ Standard Detection

| Macro | Description |
|-------|-------------|
| [`RPP_HAS_CXX17`](src/rpp/config.h#L75) | `1` if C++17 or later is available |
| [`RPP_HAS_CXX20`](src/rpp/config.h#L83) | `1` if C++20 or later is available |
| [`RPP_HAS_CXX23`](src/rpp/config.h#L91) | `1` if C++23 or later is available |
| [`RPP_HAS_CXX26`](src/rpp/config.h#L99) | `1` if C++26 or later is available |
| [`RPP_INLINE_STATIC`](src/rpp/config.h#L107) | `inline static` on C++17+, `static` otherwise |
| [`RPP_CXX17_IF_CONSTEXPR`](src/rpp/config.h#L115) | `if constexpr` when available, falls back to `if` |

### API Export / Linkage

| Macro | Description |
|-------|-------------|
| [`RPPAPI`](src/rpp/config.h#L55) | DLL visibility attribute: `__declspec(dllexport)` on MSVC, `__attribute__((visibility("default")))` on GCC/Clang |
| [`RPP_EXTERNC`](src/rpp/config.h#L63) | `extern "C"` when compiling as C++, empty otherwise |
| [`RPPCAPI`](src/rpp/config.h#L70) | Combined `RPP_EXTERNC RPPAPI` for C-compatible exported functions |

### Sanitizer Detection

| Macro | Description |
|-------|-------------|
| [`RPP_ASAN`](src/rpp/config.h#L123) | `1` if AddressSanitizer is enabled |
| [`RPP_TSAN`](src/rpp/config.h#L131) | `1` if ThreadSanitizer is enabled |
| [`RPP_UBSAN`](src/rpp/config.h#L140) | `1` if UndefinedBehaviorSanitizer is enabled (Clang only) |
| [`RPP_SANITIZERS`](src/rpp/config.h#L148) | `1` if any sanitizer (ASAN, TSAN, UBSAN) is enabled |

### Platform & Architecture

| Macro | Description |
|-------|-------------|
| [`RPP_BARE_METAL`](src/rpp/config.h#L188) | `1` if targeting a bare-metal/embedded platform (FreeRTOS or STM32 HAL) |
| [`RPP_FREERTOS`](src/rpp/config.h#L176) | `1` if targeting FreeRTOS |
| [`RPP_STM32_HAL`](src/rpp/config.h#L180) | `1` if targeting STM32 HAL (requires `RPP_STM32_HAL_H` path) |
| [`RPP_USE_EYALROZ_PRINTF`](src/rpp/config.h#L194) | `1` to use [eyalroz/printf](https://github.com/eyalroz/printf) (`printf_`/`snprintf_`) on bare-metal instead of standard `printf` |
| [`RPP_CORTEX_M_ARCH`](src/rpp/config.h#L199) | `1` if targeting ARM Cortex-M architecture |
| [`RPP_ARM_ARCH`](src/rpp/config.h#L215) | `1` if compiling for ARM (`__thumb__` or `__arm__`) |
| [`RPP_64BIT`](src/rpp/config.h#L240) | `1` if compiling for a 64-bit target |
| [`RPP_LITTLE_ENDIAN`](src/rpp/config.h#L390) | `1` if target is little-endian |
| [`RPP_BIG_ENDIAN`](src/rpp/config.h#L398) | `1` if target is big-endian |
| [`RPP_HAS_EXCEPTIONS`](src/rpp/config.h#L408) | `1` if C++ exceptions are enabled. Auto-detected via `_CPPUNWIND` (MSVC), `__EXCEPTIONS`/`__cpp_exceptions` (GCC/Clang). Defaults to `1` on unknown compilers. Can be overridden manually. |

### Feature Detection

| Macro | Description |
|-------|-------------|
| [`RPP_HAS_QT`](src/rpp/config.h#L157) | `1` if Qt framework is detected (`QT_VERSION` or `QT_CORE_LIB`) |
| [`RPP_ENABLE_UNICODE`](src/rpp/config.h#L167) | `1` if UTF-16/wstring support is enabled (auto-detected per platform) |

### Function Attributes

| Macro | Description |
|-------|-------------|
| [`FINLINE`](src/rpp/config.h#L233) | Force inline: `__forceinline` on MSVC, `__attribute__((always_inline))` on GCC/Clang |
| [`NOINLINE`](src/rpp/config.h#L224) | Prevent inlining: `__declspec(noinline)` on MSVC, `__attribute__((noinline))` on GCC/Clang |
| [`NODISCARD`](src/rpp/config.h#L264) | Portable `[[nodiscard]]` with fallback to empty on older compilers |
| [`RPP_NORETURN`](src/rpp/config.h#L323) | Portable `[[noreturn]]` / `__declspec(noreturn)` / `__attribute__((noreturn))` |
| [`NOCOPY_NOMOVE(T)`](src/rpp/config.h#L255) | Delete copy and move constructors and assignment operators |

### Printf Format Validation

| Macro | Description |
|-------|-------------|
| [`PRINTF_FMTSTR`](src/rpp/config.h#L287) | MSVC `_Printf_format_string_` annotation (empty on GCC/Clang) |
| [`PRINTF_CHECKFMT1..8`](src/rpp/config.h#L298) | GCC/Clang `__format__(__printf__)` attribute for validating printf args at compile time. Number indicates format string argument position |

### Lifetime & Coroutine Annotations

| Macro | Description |
|-------|-------------|
| [`RPP_LIFETIMEBOUND`](src/rpp/config.h#L338) | Annotates parameters whose lifetime must outlive the return value. `[[msvc::lifetimebound]]` / `[[clang::lifetimebound]]` |
| [`RPP_CORO_RETURN_TYPE`](src/rpp/config.h#L354) | Marks a type as a coroutine return type (`[[clang::coro_return_type]]`) |
| [`RPP_CORO_WRAPPER`](src/rpp/config.h#L355) | Marks a non-coroutine function that returns a CRT (`[[clang::coro_wrapper]]`) |
| [`RPP_CORO_LIFETIMEBOUND`](src/rpp/config.h#L356) | Coroutine-specific lifetime annotation (`[[clang::coro_lifetimebound]]`) |

### Integer Size Constants

| Macro | Description |
|-------|-------------|
| [`RPP_SHORT_SIZE`](src/rpp/config.h#L367) | Size of `short` in bytes (platform-dependent) |
| [`RPP_INT_SIZE`](src/rpp/config.h#L368) | Size of `int` in bytes |
| [`RPP_LONG_SIZE`](src/rpp/config.h#L369) | Size of `long` in bytes |
| [`RPP_LONG_LONG_SIZE`](src/rpp/config.h#L370) | Size of `long long` in bytes |
| [`RPP_INT64_MIN`](src/rpp/config.h#L379) | 64-bit signed integer limits |
| [`RPP_INT64_MAX`](src/rpp/config.h#L378) | 64-bit signed integer limits |
| [`RPP_UINT64_MIN`](src/rpp/config.h#L381) | 64-bit unsigned integer limits |
| [`RPP_UINT64_MAX`](src/rpp/config.h#L380) | 64-bit unsigned integer limits |
| [`RPP_INT32_MIN`](src/rpp/config.h#L383) | 32-bit signed integer limits |
| [`RPP_INT32_MAX`](src/rpp/config.h#L382) | 32-bit signed integer limits |
| [`RPP_UINT32_MIN`](src/rpp/config.h#L385) | 32-bit unsigned integer limits |
| [`RPP_UINT32_MAX`](src/rpp/config.h#L384) | 32-bit unsigned integer limits |

### C++ Type Aliases (namespace `rpp`)

| Type | Description |
|------|-------------|
| [`byte`](src/rpp/config.h#L428) | `unsigned char` |
| [`ushort`](src/rpp/config.h#L429) | `unsigned short` |
| [`uint`](src/rpp/config.h#L430) | `unsigned int` |
| [`ulong`](src/rpp/config.h#L431) | `unsigned long` |
| [`int16`](src/rpp/config.h#L433) | `short` (16-bit signed) |
| [`uint16`](src/rpp/config.h#L434) | `unsigned short` (16-bit unsigned) |
| [`int32`](src/rpp/config.h#L437) | `int` or `long` depending on `RPP_INT_SIZE` (32-bit signed) |
| [`uint32`](src/rpp/config.h#L438) | `unsigned int` or `unsigned long` (32-bit unsigned) |
| [`int64`](src/rpp/config.h#L444) | `long long` (64-bit signed) |
| [`uint64`](src/rpp/config.h#L445) | `unsigned long long` (64-bit unsigned) |

---

## rpp/strview.h

An extremely fast and powerful set of string-parsing utilities. Includes fast tokenization, blazing fast number parsers, extremely fast number formatters, and basic string search functions and utilities, all extremely well optimized.

Works on UTF-8 strings. For unicode-aware parsing, use `rpp::ustrview` and define `RPP_ENABLE_UNICODE=1` before including.

The string view header also provides utility parsers such as `line_parser`, `keyval_parser` and `bracket_parser`.

```cpp
rpp::strview text = buffer;
rpp::strview line;
while (text.next(line, '\n'))
{
    line.trim();
    if (line.empty() || line.starts_with('#'))
        continue;
    if (line.starts_withi("numObJecTS")) // ignore case
    {
        objects.reserve(line.to_int());
    }
    else if (line.starts_with("object")) // "object scene_root 192 768"
    {
        line.skip(' ');
        rpp::strview name = line.next(' ');
        int posX = line.next_int();
        int posY = line.next_int();
        objects.push_back(scene::object{name.to_string(), posX, posY});
    }
}
```

### Global Utilities

| Function | Description |
|----------|-------------|
| [`to_double(str, len, end)`](src/rpp/strview.h#L113) | C-locale specific, simplified atof that also outputs the end of parsed string |
| [`to_int(str, len, end)`](src/rpp/strview.h#L123) | Fast locale-agnostic atoi |
| [`to_inthx(str, len, end)`](src/rpp/strview.h#L144) | Fast locale-agnostic atoi for HEX strings |
| [`_tostring(buffer, value)`](src/rpp/strview.h#L165) | Fast locale-agnostic itoa/ftoa for int, float, double |
| [`replace(str, len, chOld, chNew)`](src/rpp/strview.h#L1533) | Replaces characters in a string buffer |
| [`concat(a, b, ...)`](src/rpp/strview.h#L1403) | Concatenates multiple strviews into std::string |
| [`to_lower(str, len)`](src/rpp/strview.h#L1513) | Converts string to lowercase |
| [`to_upper(str, len)`](src/rpp/strview.h#L1518) | Converts string to uppercase |
| [`operator""_sv`](src/rpp/strview.h#L1297) | String literal for creating `strview` |
| [`strcontains(str, len, ch)`](src/rpp/strview.h#L78) | Checks if character is found within a string |
| [`strcontainsi(str, len, ch)`](src/rpp/strview.h#L79) | Case-insensitive character search |
| [`strequals(s1, s2, len)`](src/rpp/strview.h#L85) | Case-sensitive string equality for given length |
| [`strequalsi(s1, s2, len)`](src/rpp/strview.h#L86) | Case-insensitive string equality for given length |
| [`to_int64(str, len, end)`](src/rpp/strview.h#L133) | Fast locale-agnostic string to 64-bit integer |
| [`RPP_CONSTEXPR_STRLEN`](src/rpp/strview.h#L220) | Marks strlen as constexpr when compiler supports it |
| [`utf8len(c_str)`](src/rpp/strview.h#L231) | Returns int length of a UTF-8 C-string |
| [`utf16len(u_str)`](src/rpp/strview.h#L237) | Returns int length of a UTF-16 C-string |

### Utility Parsers

| Class | Description |
|-------|-------------|
| [`line_parser`](src/rpp/strview.h#L1550) | Parses an input buffer for individual lines, returned trimmed of `\r` or `\n` |
| [`keyval_parser`](src/rpp/strview.h#L1588) | Parses a buffer for `Key=Value` pairs, returned one by one with `read_next` |
| [`bracket_parser`](src/rpp/strview.h#L1637) | Parses a buffer for balanced-parentheses structures |

### strview Class

[`struct strview`](src/rpp/strview.h#L254) — Non-owning string token for efficient parsing. Represents a weak reference string with start pointer and length.

| Method | Description |
|--------|-------------|
| [`to_int()`](src/rpp/strview.h#L340) | Parses this strview as an integer |
| [`to_int_hex()`](src/rpp/strview.h#L342) | Parses this strview as a HEX integer (`0xff` or `0ff` or `ff`) |
| [`to_long()`](src/rpp/strview.h#L346) | Parse as long |
| [`to_float()`](src/rpp/strview.h#L348) | Parses as a float |
| [`to_double()`](src/rpp/strview.h#L348) | Parses as a double |
| [`to_bool()`](src/rpp/strview.h#L357) | Relaxed parsing as boolean: `"true"`, `"yes"`, `"on"`, `"1"` |
| [`clear()`](src/rpp/strview.h#L307) | Clears the strview |
| [`size()`](src/rpp/strview.h#L310) | Returns the length |
| [`empty()`](src/rpp/strview.h#L312) | True if empty |
| [`is_whitespace()`](src/rpp/strview.h#L360) | True if all whitespace |
| [`is_nullterm()`](src/rpp/strview.h#L362) | True if the referenced string is null-terminated |
| [`trim_start()`](src/rpp/strview.h#L368) | Trims whitespace (or given char/chars) from the start |
| [`trim_end()`](src/rpp/strview.h#L384) | Trims whitespace (or given char/chars) from the end |
| [`trim()`](src/rpp/strview.h#L400) | Trims both start and end |
| [`chomp_first()`](src/rpp/strview.h#L411) | Consumes the first character if possible |
| [`chomp_last()`](src/rpp/strview.h#L413) | Consumes the last character if possible |
| [`pop_front()`](src/rpp/strview.h#L416) | Pops and returns the first character |
| [`pop_back()`](src/rpp/strview.h#L418) | Pops and returns the last character |
| [`contains(char c)`](src/rpp/strview.h#L445) | True if contains a char or substring |
| [`contains_any(const char* chars, int nchars)`](src/rpp/strview.h#L455) | True if contains any of the given chars |
| [`find(char c)`](src/rpp/strview.h#L463) | Pointer to start of substring if found, NULL otherwise |
| [`rfind(char c)`](src/rpp/strview.h#L493) | Reverse search for a character |
| [`findany(const char* chars, int n)`](src/rpp/strview.h#L499) | Forward search for any of the specified chars |
| [`rfindany(const char* chars, int n)`](src/rpp/strview.h#L508) | Reverse search for any of the specified chars |
| [`count(char ch)`](src/rpp/strview.h#L517) | Count occurrences of a character |
| [`indexof(char ch)`](src/rpp/strview.h#L520) | Index of character, or -1 |
| [`rindexof(char ch)`](src/rpp/strview.h#L525) | Reverse iterating index of character |
| [`indexofany(const char* chars, int n)`](src/rpp/strview.h#L528) | First index of any matching char |
| [`starts_with(const char* s, int len)`](src/rpp/strview.h#L534) | True if starts with the string |
| [`starts_withi(const char* s, int len)`](src/rpp/strview.h#L545) | Case-insensitive starts_with |
| [`ends_with(const char* s, int slen)`](src/rpp/strview.h#L556) | True if ends with the string |
| [`ends_withi(const char* s, int slen)`](src/rpp/strview.h#L567) | Case-insensitive ends_with |
| [`equals(const char* s, int len)`](src/rpp/strview.h#L578) | Exact equality |
| [`equalsi(const char* s, int len)`](src/rpp/strview.h#L584) | Case-insensitive equality |
| [`compare(const char* s, int n)`](src/rpp/strview.h#L605) | Compare to another string |
| [`split_first(char delim)`](src/rpp/strview.h#L627) | Split into two, return the first part |
| [`split_second(char delim)`](src/rpp/strview.h#L643) | Split into two, return the second part |
| [`next(strview& out, char delim)`](src/rpp/strview.h#L651) | Gets next token; advances ptr to next delimiter |
| [`next(char delim)`](src/rpp/strview.h#L674) | Returns next token directly |
| [`next_notrim(strview& out, char delim)`](src/rpp/strview.h#L718) | Gets next token without trimming; stops on delimiter |
| [`substr(int index, int length)`](src/rpp/strview.h#L849) | Creates a substring from index with given length |
| [`substr(int index)`](src/rpp/strview.h#L849) | Creates a substring from index to end |
| [`next_double()`](src/rpp/strview.h#L862) | Parses next double from current position |
| [`next_float()`](src/rpp/strview.h#L863) | Parses next float from current position |
| [`next_int()`](src/rpp/strview.h#L869) | Parses next int from current position |
| [`skip(int nchars)`](src/rpp/strview.h#L880) | Safely skips N characters |
| [`skip_until(char ch)`](src/rpp/strview.h#L886) | Skips until the specified char is found |
| [`skip_after(char ch)`](src/rpp/strview.h#L910) | Skips past the specified char |
| [`to_lower()`](src/rpp/strview.h#L934) | Modifies the referenced string to lowercase |
| [`as_lower()`](src/rpp/strview.h#L939) | Returns a lowercase copy as std::string |
| [`to_upper()`](src/rpp/strview.h#L950) | Modifies the referenced string to uppercase |
| [`as_upper()`](src/rpp/strview.h#L955) | Returns an uppercase copy as std::string |
| [`replace(char chOld, char chNew)`](src/rpp/strview.h#L969) | Replaces all occurrences of chOld with chNew |
| [`decompose(delim, T& outFirst, Rest&... rest)`](src/rpp/strview.h#L831) | Decomposes strview into multiple typed outputs |

### Example: Splitting Paths and URLs

```cpp
// split_first / split_second for quick one-shot splits
rpp::strview url = "https://example.com/api/v2/users?active=true";
rpp::strview protocol = url.split_first("://"); // "https"
rpp::strview query    = url.split_second('?');   // "active=true"

// extract file extension from a path
rpp::strview path = "/home/user/documents/report.final.pdf";
rpp::strview ext  = path.split_second('.'); // gets everything after first '.' --> "final.pdf"
// for the last extension, use rfind:
if (const char* dot = path.rfind('.'))
    rpp::strview lastExt = { dot + 1, path.end() }; // "pdf"
```

### Example: CSV Row Parsing with next()

```cpp
// next() tokenizes and advances the strview — perfect for CSV-style data
rpp::strview row = "Alice,30,Engineering,true";
rpp::strview name = row.next(',');       // "Alice"
int age            = row.next(',').to_int();  // 30
rpp::strview dept  = row.next(',');      // "Engineering"
bool active        = row.next(',').to_bool(); // true
```

### Example: Extracting Numbers from Mixed Text

```cpp
// next_int / next_float skip non-numeric chars and parse the next number
rpp::strview mixed = "x=42 y=-7.5 z=100";
int x    = mixed.next_int();   // 42
float y  = mixed.next_float(); // -7.5
int z    = mixed.next_int();   // 100
```

### Example: Decompose Structured Data

```cpp
// decompose auto-parses into typed variables in a single call
rpp::strview input = "user@email.com;27;3486.37;true"_sv;
rpp::strview email;
int age;
float coins;
bool unlocked;
input.decompose(';', email, age, coins, unlocked);
// email="user@email.com"  age=27  coins=3486.37  unlocked=true
```

### Example: Skip and Search

```cpp
rpp::strview header = "Content-Type: application/json; charset=utf-8";
header.skip_after(':');   // now " application/json; charset=utf-8"
header.trim_start();      // now "application/json; charset=utf-8"
rpp::strview mime = header.split_first(';'); // "application/json"
```

### Example: Using line_parser

```cpp
rpp::line_parser parser { fileContents };
rpp::strview line;
while (parser.read_line(line)) // reads next line, trimmed of \r and \n
{
    line.trim(); // trim any space or tab whitespace from the line
    if (line.empty() || line.starts_with('#'))
        continue; // skip blanks and comments
    LogInfo("%s", line); // debug macros can print strviews safely via rpp::__wrap<strview> which calls line.to_cstr() under the hood
}
```

### Example: Using keyval_parser

```cpp
const char* config = "# database settings\n"
                     "  host = localhost \n"
                     "  port = 5432 \n"
                     "  name = mydb \n";
rpp::keyval_parser parser { config };
rpp::strview key, value;
// reads next key=value pair, each key and value are trimmed of whitespace, comment # and blank lines skipped
while (parser.read_next(key, value))
{
    // key="host" value="localhost", key="port" value="5432", etc.
    if (key == "port")
        settings.port = value.to_int();
}
```

### Example: strview as HashMap Key

```cpp
// strview is hashable — use it directly in unordered_map
std::unordered_map<rpp::strview, int> counts;
rpp::strview words = "the quick brown fox jumps over the lazy dog";
rpp::strview word;
while (words.next(word, ' '))
    counts[word]++;
// counts["the"] == 2
```

### Example: Contains, Starts/Ends With

```cpp
rpp::strview filename = "screenshot_2026-03-13.png";

// contains - check for chars or substrings
if (filename.contains('.'))            // true — has a dot
if (filename.contains("screenshot"))   // true — has substring

// contains_any - check for any of the given chars
if (filename.contains_any("!@#$"))     // false — no special chars

// starts_with / ends_with
if (filename.starts_with("screenshot")) // true
if (filename.ends_with(".png"))         // true

// case-insensitive variants
rpp::strview cmd = "GET /index.html HTTP/1.1";
if (cmd.starts_withi("get"))    // true — ignores case
if (cmd.ends_withi("http/1.1")) // true
```

### Example: Equals and Case-Insensitive Comparison

```cpp
rpp::strview ext = "PNG";

// exact equality via operator== or equals()
if (ext == "PNG")            // true
if (ext == "png")            // false
if (ext.equals("PNG"))       // true

// case-insensitive equality
if (ext.equalsi("png"))      // true
if (ext.equalsi("Png"))      // true

// works with std::string too
std::string expected = "PNG";
if (ext == expected)          // true
if (ext.equalsi("pNg"))      // true

// compare() for ordering (returns <0, 0, >0 like strcmp)
rpp::strview a = "apple", b = "banana";
if (a < b) // true — lexicographic ordering via compare()
```

### ustrview Class

[`struct ustrview`](src/rpp/strview.h#L980) — UTF-16 (`char16_t`) counterpart of `strview`. Requires `RPP_ENABLE_UNICODE=1` to be defined before including the header. Provides the same non-owning, non-null-terminated string view semantics as `strview`, but operates on `char16_t` data. Implicitly converts from `std::u16string` and `std::u16string_view`. On MSVC, also accepts `const wchar_t*`.

| Method | Description |
|--------|-------------|
| [`size()`](src/rpp/strview.h#L1013) | Returns the length |
| [`empty()`](src/rpp/strview.h#L1015) | True if empty |
| [`clear()`](src/rpp/strview.h#L1010) | Clears the ustrview |
| [`is_nullterm()`](src/rpp/strview.h#L1033) | True if the referenced string is null-terminated |
| [`trim_start()`](src/rpp/strview.h#L1037) | Trims whitespace (or given char/chars) from the start |
| [`trim_end()`](src/rpp/strview.h#L1044) | Trims whitespace (or given char/chars) from the end |
| [`trim()`](src/rpp/strview.h#L1051) | Trims both start and end |
| [`chomp_first()`](src/rpp/strview.h#L1058) | Consumes the first character |
| [`chomp_last()`](src/rpp/strview.h#L1060) | Consumes the last character |
| [`pop_front()`](src/rpp/strview.h#L1063) | Pops and returns the first character |
| [`pop_back()`](src/rpp/strview.h#L1065) | Pops and returns the last character |
| [`starts_with(const char16_t* s, int length)`](src/rpp/strview.h#L1090) | True if starts with the string |
| [`starts_withi(const char16_t* s, int length)`](src/rpp/strview.h#L1101) | Case-insensitive starts_with |
| [`ends_with(const char16_t* s, int slen)`](src/rpp/strview.h#L1112) | True if ends with the string |
| [`ends_withi(const char16_t* s, int slen)`](src/rpp/strview.h#L1123) | Case-insensitive ends_with |
| [`equals(const char16_t* s, int length)`](src/rpp/strview.h#L1133) | Exact equality |
| [`compare(const char16_t* s, int n)`](src/rpp/strview.h#L1150) | Compare to another string |
| [`rfind(char16_t c)`](src/rpp/strview.h#L1169) | Reverse search for a character |
| [`findany(const char16_t* chars, int n)`](src/rpp/strview.h#L1175) | Forward search for any of the specified chars |
| [`rfindany(const char16_t* chars, int n)`](src/rpp/strview.h#L1184) | Reverse search for any of the specified chars |
| [`substr(int index, int length)`](src/rpp/strview.h#L1193) | Creates a substring from index with given length |
| [`substr(int index)`](src/rpp/strview.h#L1193) | Creates a substring from index to end |
| [`next(ustrview& out, char16_t delim)`](src/rpp/strview.h#L1207) | Gets next token; advances ptr to next delimiter |
| [`next(char16_t delim)`](src/rpp/strview.h#L1230) | Returns next token directly |
| [`to_string()`](src/rpp/strview.h#L1003) | Convert to `std::u16string` |
| [`to_cstr(char16_t* buf, int max)`](src/rpp/strview.h#L1084) | Copy to null-terminated C-string buffer |

```cpp
#define RPP_ENABLE_UNICODE 1
#include <rpp/strview.h>
using namespace rpp::literals;

// construct from char16_t literal or std::u16string
rpp::ustrview name = u"日本語テキスト";
rpp::ustrview greeting = u"こんにちは世界"_sv;

// tokenize CSV-style UTF-16 data
rpp::ustrview csv = u"名前,年齢,都市";
rpp::ustrview token;
while (csv.next(token, u','))
{
    std::u16string value = token.to_string();
}

// trimming and comparison
rpp::ustrview padded = u"  hello  ";
padded.trim();
if (padded.starts_with(u"hel"))
    LogInfo("starts with hel");

// convert to UTF-8 std::string and back to UTF-16
std::string utf8 = rpp::to_string(name);
rpp::ustring utf16 = rpp::to_ustring(utf8); // rpp::ustring is alias for std::u16string
```

---

## rpp/sprint.h

Fast string building and type-safe formatting. `string_buffer` is an always-null-terminated string builder that auto-converts most types.

| Item | Description |
|------|-------------|
| [`string_buffer`](src/rpp/sprint.h#L77) | Fast, always-null-terminated string builder |
| [`format_opt`](src/rpp/sprint.h#L66) | Format options enum: `none`, `lowercase`, `uppercase` |

### string_buffer Methods

| Method | Description |
|--------|-------------|
| [`write(const T& v)`](src/rpp/sprint.h#L124) | Write a value (auto-converts most types) |
| [`writeln(const Args&... args)`](src/rpp/sprint.h#L339) | Write values followed by newline |
| [`writef(const char* format, ...)`](src/rpp/sprint.h#L122) | Printf-style formatted write |
| [`write_hex(const void* data, int numBytes)`](src/rpp/sprint.h#L285) | Write data as hex string |
| [`write_cont(const Container& c)`](src/rpp/sprint.h#L264) | Write container contents |
| [`prettyprint(const T& value)`](src/rpp/sprint.h#L347) | Pretty-print a value |
| [`clear()`](src/rpp/sprint.h#L113) | Clear the buffer |
| [`reserve(int capacity)`](src/rpp/sprint.h#L114) | Reserve capacity |
| [`resize(int size)`](src/rpp/sprint.h#L115) | Resize buffer |
| [`append(const char* data, int len)`](src/rpp/sprint.h#L118) | Append raw data |
| [`emplace_buffer(int n)`](src/rpp/sprint.h#L121) | Get writable buffer of N bytes |

### Free Functions

| Function | Description |
|----------|-------------|
| [`to_string(char)`](src/rpp/sprint.h#L37) | Locale-agnostic char to string |
| [`to_string(int)`](src/rpp/sprint.h#L45) | Locale-agnostic int to string |
| [`to_string(float)`](src/rpp/sprint.h#L51) | Locale-agnostic float to string |
| [`to_string(double)`](src/rpp/sprint.h#L52) | Locale-agnostic double to string |
| [`to_string(bool)`](src/rpp/sprint.h#L55) | Bool to `"true"` or `"false"` |
| [`print(args...)`](src/rpp/sprint.h#L461) | Print to stdout |
| [`println(args...)`](src/rpp/sprint.h#L481) | Print to stdout with newline |
| [`to_hex_string(s, opt)`](src/rpp/sprint.h#L407) | Converts string bytes to hexadecimal representation |

### Example: Basic String Building

```cpp
#include <rpp/sprint.h>

rpp::string_buffer sb;
sb.write("Hello");
sb.write(' ');
sb.write(42);
sb.write(' ');
sb.write(3.14f);
// sb.view() == "Hello 42 3.14"

// multi-arg write auto-inserts separator (default: " ")
rpp::string_buffer sb2;
sb2.write("score:", 100, "time:", 5.5f);
// sb2.view() == "score: 100 time: 5.5"
```

### Example: writef (Printf-Style)

```cpp
rpp::string_buffer sb;
sb.writef("Player %s has %d points (%.1f%%)", "Alice", 1500, 98.7);
// sb.view() == "Player Alice has 1500 points (98.7%)"
```

### Example: writeln

```cpp
rpp::string_buffer sb;
sb.writeln("first line");
sb.writeln("second line");
// sb.view() == "first line\nsecond line\n"

// multi-arg writeln
sb.clear();
sb.writeln("x=", 10, "y=", 20);
// sb.view() == "x= 10 y= 20\n"
```

### Example: Custom Separator

```cpp
rpp::string_buffer sb;
sb.separator = ", ";
sb.write("apple", "banana", "cherry");
// sb.view() == "apple, banana, cherry"

sb.clear();
sb.separator = " | ";
sb.write(1, 2, 3);
// sb.view() == "1 | 2 | 3"
```

### Example: write_hex

```cpp
rpp::string_buffer sb;
uint32_t value = 0xDEADBEEF;
sb.write_hex(&value, sizeof(value)); // lowercase by default
// sb.view() == "efbeadde" (little-endian byte order)

sb.clear();
sb.write_hex("Hello", rpp::uppercase);
// sb.view() == "48656C6C6F"
```

### Example: Pretty-Printing Containers

```cpp
std::vector<int> nums = {1, 2, 3, 4, 5};
rpp::string_buffer sb;
sb.write(nums);
// sb.view() == "[5] { 1, 2, 3, 4, 5 }"

// prettyprint adds quotes around strings and apostrophes around chars
sb.clear();
std::vector<std::string> names = {"Alice", "Bob"};
sb.write(names);
// sb.view() == "[2] { "Alice", "Bob" }"
```

### Example: operator<< Streaming

```cpp
rpp::string_buffer sb;
sb << "Temperature: " << 36.6f << "°C";
// sb.view() == "Temperature: 36.6°C"

// any type with to_string() or operator<< works automatically
struct Point {
    int x, y;
    std::string to_string() const { return rpp::string_buffer{}.write(x, ",", y), ""; }
};
```

### Example: to_string and print/println

```cpp
// locale-agnostic to_string (no trailing zeros for floats)
std::string s1 = rpp::to_string(3.14f);  // "3.14" (not "3.140000")
std::string s2 = rpp::to_string(42);     // "42"
std::string s3 = rpp::to_string(true);   // "true"

// print / println write directly to stdout
rpp::println("Hello, world!");  // prints "Hello, world!\n"
rpp::print(42);                 // prints "42"
```

---

## rpp/file_io.h

Cross-platform file I/O with RAII file handles. Provides efficient file reading, writing, and file-backed parser utilities.

### Classes

| Class | Description |
|-------|-------------|
| [`load_buffer`](src/rpp/file_io.h#L30) | RAII buffer for loading file contents into memory |
| [`file`](src/rpp/file_io.h#L93) | Random-access file with read, write, seek, and truncate |
| [`buffer_parser`](src/rpp/file_io.h#L465) | Generic file-backed parser (line, bracket, keyval variants) |

### file::mode Enum

| Value | Description |
|-------|-------------|
| [`READONLY`](src/rpp/file_io.h#L96) | Open for reading only |
| [`READWRITE`](src/rpp/file_io.h#L97) | Open for reading and writing |
| [`CREATENEW`](src/rpp/file_io.h#L98) | Create a new file (truncates existing) |
| [`APPEND`](src/rpp/file_io.h#L99) | Open for appending |

### load_buffer Methods

| Method | Description |
|--------|-------------|
| [`size()`](src/rpp/file_io.h#L50) | Size of the loaded data |
| [`data()`](src/rpp/file_io.h#L52) | Pointer to the data |
| [`c_str()`](src/rpp/file_io.h#L53) | Null-terminated C string |
| [`view()`](src/rpp/file_io.h#L56) | Get a `strview` over the buffer |
| [`steal_ptr()`](src/rpp/file_io.h#L47) | Release ownership of the buffer |
| [`operator bool()`](src/rpp/file_io.h#L54) | True if buffer contains data |

### file Methods

| Method | Description |
|--------|-------------|
| [`open(strview filename, mode openMode)`](src/rpp/file_io.h#L141) | Open a file using an UTF-8 filepath (internally converted to ustring) |
| [`open(ustrview filename, mode openMode)`](src/rpp/file_io.h#L143) | Open a file using an UTF-16 filepath |
| [`close()`](src/rpp/file_io.h#L145) | Close the file |
| [`good()`](src/rpp/file_io.h#L150) / [`is_open()`](src/rpp/file_io.h#L152) | True if file is open and valid |
| [`bad()`](src/rpp/file_io.h#L162) | True if file is not open |
| [`size()`](src/rpp/file_io.h#L167) / [`sizel()`](src/rpp/file_io.h#L172) | File size (32-bit / 64-bit) |
| [`read(void* buffer, int bytesToRead)`](src/rpp/file_io.h#L182) | Read bytes into buffer |
| [`read_all()`](src/rpp/file_io.h#L197) | Read entire file as `load_buffer` |
| [`read_text()`](src/rpp/file_io.h#L202) | Read entire file as `std::string` |
| [`write(const void* buffer, int bytesToWrite)`](src/rpp/file_io.h#L248) | Write bytes from buffer |
| [`writef(const char* format, ...)`](src/rpp/file_io.h#L280) | Printf-style write |
| [`writeln(strview str)`](src/rpp/file_io.h#L285) | Write line with newline |
| [`seek(int filepos, int seekmode)`](src/rpp/file_io.h#L409) | Seek to filepos |
| [`seekl(int64 filepos, int seekmode)`](src/rpp/file_io.h#L410) | Seek to 64-bit filepos |
| [`tell()`](src/rpp/file_io.h#L415) | Tell file position |
| [`tell64()`](src/rpp/file_io.h#L420) | Tell 64-bit file position |
| [`flush()`](src/rpp/file_io.h#L352) | Flush buffered writes |
| [`truncate(int64 size)`](src/rpp/file_io.h#L337) | Truncate file to size |
| [`truncate_front(int64 bytes)`](src/rpp/file_io.h#L323) | Remove bytes from front |
| [`truncate_end(int64 bytes)`](src/rpp/file_io.h#L331) | Remove bytes from end |
| [`preallocate(int64 size)`](src/rpp/file_io.h#L346) | Preallocate file space |
| [`save_as(const char* newPath)`](src/rpp/file_io.h#L208) | Copy contents to a new file |
| [`time_created()`](src/rpp/file_io.h#L430) | File creation time |
| [`time_accessed()`](src/rpp/file_io.h#L435) | Last access time |
| [`time_modified()`](src/rpp/file_io.h#L440) | Last modification time |

### file Static Methods

| Method | Description |
|--------|-------------|
| [`file::read_all(strview filename)`](src/rpp/file_io.h#L214) | Read entire file into `load_buffer` |
| [`file::read_all(ustrview filename)`](src/rpp/file_io.h#L219) | Read entire file into `load_buffer` |
| [`file::read_all_text(strview filename)`](src/rpp/file_io.h#L229) | Read entire file as `std::string` |
| [`file::read_all_text(ustrview filename)`](src/rpp/file_io.h#L234) | Read entire file as `std::string` |
| [`file::write_new(strview filename, const void* data, int size)`](src/rpp/file_io.h#L365) | Create/overwrite file with data |
| [`file::write_new(ustrview filename, const void* data, int size)`](src/rpp/file_io.h#L379) | Create/overwrite file with data |

### Example using file_io for reading and writing files:

```cpp
#include <rpp/file_io.h>

void fileio_read_sample(rpp::strview filename = "README.md"_sv)
{
    if (rpp::file f = { filename, rpp::file::READONLY }) // open the file explicitly, or use static file::read_all() util
    {
        // reads all data in the most efficient way
        rpp::load_buffer data = f.read_all(); 

        // use the data as a binary blob
        for (char& ch : data) { }
    }
}
void fileio_writeall_sample(rpp::strview filename = "test.txt"_sv)
{
    std::string someText = "/**\n * A simple self-expanding buffer\n */\nstruct";
    
    // write a new file with the contents of someText
    rpp::file::write_new(filename, someText.data(), someText.size());

    // or just write it as a string 
    rpp::file::write_new(filename, someText);
}
void fileio_modify_sample(rpp::strview filename = "test.txt"_sv)
{
    if (rpp::file f = { filename, rpp::file::READWRITE })
    {
        // append a line to the file
        f.seek(0, rpp::file::END);
        f.writeln("This line was appended to the file.");

        // read the first 100 bytes
        char buffer[101] = {};
        f.seek(0, rpp::file::BEGIN);
        f.read(buffer, 100);
        LogInfo("First 100 bytes:\n%s", buffer);

        // truncate the last 50 bytes from the end
        f.truncate_end(50);

        // truncate the first 10 bytes from the beginning
        f.truncate_front(10);
    }
}
```

### Example: Quick One-Liners

```cpp
#include <rpp/file_io.h>

// read entire file into a managed buffer (auto-freed)
rpp::load_buffer buf = rpp::file::read_all("config.bin");
if (buf) process(buf.data(), buf.size());

// read entire file as std::string
std::string text = rpp::file::read_all_text("README.md");

// create/overwrite a file in one call
rpp::file::write_new("output.txt", "Hello, world!");

// write binary data
std::vector<float> data = {1.0f, 2.0f, 3.0f};
rpp::file::write_new("data.bin", data);
```

### Example: Appending with writeln

```cpp
// APPEND mode opens or creates the file for appending
if (rpp::file log = { "app.log", rpp::file::APPEND })
{
    log.writeln("Application started");
    log.writef("[%s] event=%s code=%d\n", timestamp, "click", 42);

    // multi-arg writeln auto-separates with spaces (like Python print)
    log.writeln("user:", username, "action:", "login", "ip:", clientIp);
    // writes: "user: alice action: login ip: 192.168.1.1\n"
}
```

### Example: Line-by-Line File Parsing

```cpp
// buffer_line_parser loads the file and parses lines
if (auto parser = rpp::buffer_line_parser::from_file("data.csv"))
{
    rpp::strview line;
    while (parser.read_line(line))
    {
        if (line.empty() || line.starts_with('#'))
            continue;
        rpp::strview name = line.next(',');
        int value = line.next(',').to_int();
        LogInfo("%s = %d\n", name, value);
    }
}
```

### Example: Using load_buffer with strview

```cpp
// load_buffer integrates seamlessly with strview for parsing
if (rpp::load_buffer buf = rpp::file::read_all("scene.txt"))
{
    rpp::strview content = buf.view();
    rpp::strview header = content.next('\n'); // first line
    int objectCount = content.next('\n').to_int(); // second line
    for (int i = 0; i < objectCount; ++i)
    {
        rpp::strview objLine = content.next('\n');
        // parse each object line...
    }
}
```

---

## rpp/paths.h

Cross-platform path manipulation, directory listing, and filesystem utilities.

### Existence & Info

| Function | Description |
|----------|-------------|
| [`file_exists(path)`](src/rpp/paths.h#L29) | True if the file exists |
| [`folder_exists(path)`](src/rpp/paths.h#L45) | True if the folder exists |
| [`is_symlink(path)`](src/rpp/paths.h#L37) | True if the path is a symlink |
| [`file_size(path)`](src/rpp/paths.h#L100) | Short file size (32-bit) |
| [`file_sizel(path)`](src/rpp/paths.h#L108) | Long file size (64-bit) |
| [`file_created(path)`](src/rpp/paths.h#L116) | File creation date |
| [`file_accessed(path)`](src/rpp/paths.h#L124) | Last file access date |
| [`file_modified(path)`](src/rpp/paths.h#L132) | Last file modification date |
| [`file_or_folder_exists(fileOrFolder)`](src/rpp/paths.h#L53) | True if a file or folder exists at the given path |
| [`file_info(filename, filesize, created, accessed, modified)`](src/rpp/paths.h#L77) | Gets file info: size, creation, access and modification dates |

### File Operations

| Function | Description |
|----------|-------------|
| [`delete_file(path)`](src/rpp/paths.h#L141) | Delete a single file |
| [`copy_file(sourceFile, destinationFile)`](src/rpp/paths.h#L166) | Copy file, overwriting destination |
| [`copy_file_if_needed(sourceFile, destinationFile)`](src/rpp/paths.h#L184) | Copy only if destination doesn't exist |
| [`copy_file_into_folder(sourceFile, destinationFolder)`](src/rpp/paths.h#L193) | Copy file into a folder |
| [`create_symlink(target, link)`](src/rpp/paths.h#L63) | Create a symbolic link |
| [`copy_file_mode(sourceFile, destinationFile)`](src/rpp/paths.h#L175) | Copies file access permissions from source to destination |

### Folder Operations

| Function | Description |
|----------|-------------|
| [`create_folder(foldername)`](src/rpp/paths.h#L203) | Create folder recursively |
| [`delete_folder(foldername, mode)`](src/rpp/paths.h#L221) | Delete folder (recursive or non-recursive) |

### Path Manipulation

| Function | Description |
|----------|-------------|
| [`full_path(path)`](src/rpp/paths.h#L230) | Resolve relative path to absolute |
| [`merge_dirups(path)`](src/rpp/paths.h#L239) | Merge all `../` inside a path |
| [`file_name(path)`](src/rpp/paths.h#L254) | Extract filename without extension |
| [`file_nameext(path)`](src/rpp/paths.h#L266) | Extract filename with extension |
| [`file_ext(path)`](src/rpp/paths.h#L280) | Extract file extension |
| [`file_replace_ext(path, ext)`](src/rpp/paths.h#L293) | Replace file extension |
| [`file_name_append(path, add)`](src/rpp/paths.h#L305) | Append to filename (before extension) |
| [`file_name_replace(path, newFileName)`](src/rpp/paths.h#L317) | Replace only the filename |
| [`file_nameext_replace(path, newFileNameAndExt)`](src/rpp/paths.h#L329) | Replace filename and extension |
| [`folder_name(path)`](src/rpp/paths.h#L342) | Extract folder name from path |
| [`folder_path(path)`](src/rpp/paths.h#L356) | Extract full folder path from file path |
| [`normalize(path, sep)`](src/rpp/paths.h#L368) | Normalize slashes in-place |
| [`normalized(path, sep)`](src/rpp/paths.h#L379) | Return normalized copy |
| [`path_combine(path1, path2, ...)`](src/rpp/paths.h#L391) | Safely combine path segments (up to 4) |

### Directory Listing

| Function | Description |
|----------|-------------|
| [`list_dirs(out, dir, flags)`](src/rpp/paths.h#L640) | List all folders inside a directory |
| [`list_files(out,dir, suffix, flags)`](src/rpp/paths.h#L694) | List files with optional extension filter |
| [`dir_iterator`](src/rpp/paths.h#L572) | Range-based UTF-8 directory iterator |
| [`udir_iterator`](src/rpp/paths.h#L577) | Range-based UTF-16 directory iterator |
| [`dir_iter_base`](src/rpp/paths.h#L411) | Common base class for directory iteration |
| [`directory_iter`](src/rpp/paths.h#L460) | Basic non-recursive directory iterator |
| [`list_alldir(outDirs, outFiles, dir, flags)`](src/rpp/paths.h#L738) | Lists all directories and files in a directory |

### System Directories

| Function | Description |
|----------|-------------|
| [`working_dir()`](src/rpp/paths.h#L751) | Current working directory (with trailing slash) |
| [`module_dir(moduleObject)`](src/rpp/paths.h#L781) | Directory of the current linked module |
| [`temp_dir()`](src/rpp/paths.h#L808) | System temporary directory |
| [`home_dir()`](src/rpp/paths.h#L819) | User home directory |
| [`module_path(moduleObject)`](src/rpp/paths.h#L789) | Full path to enclosing binary including filename |
| [`change_dir(new_wd)`](src/rpp/paths.h#L796) | Changes the application working directory |
| [`home_diru()`](src/rpp/paths.h#L821) | User home directory as Unicode ustring |

### Example using paths.h for path manipulation and directory listing:

```cpp
#include <rpp/paths.h>

void fileio_info_sample(rpp::strview file = "README.md"_sv)
{
    if (rpp::file_exists(file))
    {
        LogInfo(" === %s === ", file.str);
        LogInfo("  file_size     : %d",   rpp::file_size(file));
        LogInfo("  file_modified : %llu", rpp::file_modified(file));
        LogInfo("  full_path     : %s",   rpp::full_path(file));
        LogInfo("  file_name     : %s",   rpp::file_name(file));
        LogInfo("  folder_name   : %s",   rpp::folder_name(rpp::full_path(file)));
    }
}
void fileio_path_manipulation(rpp::strview file = "/root/../path\\example.txt"_sv)
{
    LogInfo(" === %s === ", file.str);
    LogInfo("  full_path     : %s", rpp::full_path(file));
    LogInfo("  merge_dirups  : %s", rpp::merge_dirups(file));
    LogInfo("  file_name     : %s", rpp::file_name(file));
    LogInfo("  file_nameext  : %s", rpp::file_nameext(file));
    LogInfo("  file_ext      : %s", rpp::file_ext(file));
    LogInfo("  folder_name   : %s", rpp::folder_name(file));
    LogInfo("  folder_path   : %s", rpp::folder_path(file));
    LogInfo("  normalized    : %s", rpp::normalized(file));
    LogInfo("  path_combine  : %s", rpp::path_combine(rpp::folder_name(file), "another.txt"));
}
void fileio_listing_dirs(rpp::strview path = "../"_sv)
{
    LogInfo(" working dir   : %s", rpp::working_dir());
    LogInfo(" relative path : %s", path);
    LogInfo(" full path     : %s", rpp::full_path(path));

    std::vector<std::string> cppFiles = rpp::list_files_recursive(path, "cpp");
    for (auto& relativePath : cppFiles)
        LogInfo("  source  : %s", relativePath);

    std::vector<std::string> headerFiles = rpp::list_files_fullpath("./rpp", "h");
    for (auto& fullPath : headerFiles)
        LogInfo("  header  : %s", fullPath);
}
void fileio_directory_iterator(rpp::strview path = "../"_sv)
{
    for (rpp::dir_entry e : rpp::dir_iterator(path))
    {
        LogInfo("  %s  : %s", e.is_folder() ? "dir " : "file", e.path());
    }
}
void fileio_unicode_support(rpp::ustrview filename = u"文件.txt"_usv)
{
    for (rpp::udir_entry e : rpp::udir_iterator(filename))
    {
        // convert the Unicode path to UTF-8 for logging
        LogInfo("  %s  : %s", e.is_folder() ? "dir " : "file", rpp::to_string(e.path()));
    }
}

```

### Example: Path Decomposition

```cpp
#include <rpp/paths.h>

rpp::strview path = "/home/user/projects/app/src/main.cpp";

rpp::file_name(path);     // "main"
rpp::file_nameext(path);  // "main.cpp"
rpp::file_ext(path);      // "cpp"
rpp::folder_name(path);   // "src"
rpp::folder_path(path);   // "/home/user/projects/app/src/"
```

### Example: Path Manipulation

```cpp
// replace extension
rpp::file_replace_ext("photo.raw", "jpg");     // "photo.jpg"

// append to filename (before extension)
rpp::file_name_append("log.txt", "_backup");   // "log_backup.txt"

// replace just the filename
rpp::file_name_replace("src/old.cpp", "new");  // "src/new.cpp"

// normalize mixed slashes
rpp::normalized("C:\\Users\\dev/project\\src"); // "C:/Users/dev/project/src"

// combine paths safely (handles duplicate slashes)
rpp::path_combine("build/", "/output/", "app.bin"); // "build/output/app.bin"

// resolve ../ segments
rpp::merge_dirups("src/../lib/../bin/tool"); // "bin/tool"
```

### Example: File Operations

```cpp
if (rpp::file_exists("config.json"))
{
    rpp::int64 size = rpp::file_sizel("config.json");
    time_t modified = rpp::file_modified("config.json");
    LogInfo("config.json: %lld bytes, modified %s", size, ctime(&modified));
}

// copy, delete, create folders
rpp::copy_file("template.conf", "app.conf");
rpp::copy_file_if_needed("defaults.ini", "settings.ini"); // only if missing

rpp::create_folder("output/logs/2026");  // creates all intermediate dirs
rpp::delete_folder("tmp/cache", rpp::delete_mode::recursive); // rm -rf equivalent
```

### Example: Directory Listing

```cpp
// list all .h files in src/ (non-recursive, relative paths)
auto headers = rpp::list_files("src", "h");
// headers: {"strview.h", "sprint.h", "config.h", ...}

// list all .cpp files recursively with full paths
auto sources = rpp::list_files("src", "cpp", rpp::dir_fullpath_recursive);

// list only directories
auto dirs = rpp::list_dirs("packages", rpp::dir_recursive);
```

### Example: Range-Based Directory Iterator

```cpp
for (rpp::dir_entry e : rpp::dir_iterator{"src/rpp"})
{
    if (e.is_file())
        LogInfo("file: %s %s", e.name(), e.path()); // e.path() returns "src/rpp/{name}"
    else if (e.is_dir())
        LogInfo("dir:  %s %s", e.name() e.full_path()); // e.full_path() returns absolute path
}
```

---

## rpp/delegate.h

Fast function delegates as an optimized alternative to `std::function`. Supports static functions, instance member functions, lambdas, and functors with single virtual call overhead. Compared to std::function, `delegate` pays an upfront cost for allocation, but calls are devirtualized and completely optimized, leading to much faster invocation (ideal for hot code paths and event systems).

| Class | Description |
|-------|-------------|
| [`delegate<Ret(Args...)>`](src/rpp/delegate.h#L170) | Single-target function delegate |
| [`multicast_delegate<Ret(Args...)>`](src/rpp/delegate.h#L746) | Multi-target event delegate (`event<>` alias) |

### delegate Methods

| Method | Description |
|--------|-------------|
| [`operator()(Args... args)`](src/rpp/delegate.h#L687) | Invoke the delegate |
| [`operator bool()`](src/rpp/delegate.h#L648) | True if delegate is bound |
| [`reset()`](src/rpp/delegate.h#L593) | Unbind the delegate |

### multicast_delegate Methods

| Method | Description |
|--------|-------------|
| [`add(delegate)`](src/rpp/delegate.h#L831) / [`operator+=`](src/rpp/delegate.h#L892) | Register a callback |
| [`operator-=`](src/rpp/delegate.h#L902) | Unregister a callback |
| [`operator()(Args... args)`](src/rpp/delegate.h#L912) | Invoke all registered callbacks |
| [`clear()`](src/rpp/delegate.h#L751) | Remove all callbacks |
| [`size()`](src/rpp/delegate.h#L799) | Number of registered callbacks |
| [`multicast_fwd<T>`](src/rpp/delegate.h#L916) | Trait to deduce forwarding reference type for multicast args |

### Example

```cpp
rpp::delegate<void(int)> fn = [](int a) { LogInfo("lambda %d!", a); };
fn(42);

rpp::event<void(int,int)> onMouseMove;
onMouseMove += [](int x, int y) { LogInfo("mx %d, my %d", x, y); };
onMouseMove(22, 34);
```

---

## rpp/future.h

Composable futures with C++20 coroutine support. Uses `rpp/thread_pool.h` for background execution.

| Item | Description |
|------|-------------|
| [`cfuture<T>`](src/rpp/future.h#L126) | Extended `std::future` with composition and coroutine support |
| [`async_task(task)`](src/rpp/future.h#L32) | Launch a task on the thread pool, returns `cfuture<T>` |
| [`make_ready_future(value)`](src/rpp/future.h#L951) | Create an already-completed future |
| [`make_exceptional_future(e)`](src/rpp/future.h#L968) | Create an already-errored future |
| [`wait_all(futures)`](src/rpp/future.h#L1036) | Block until all futures complete |
| [`get_all(futures)`](src/rpp/future.h#L990) | Block and gather results from all futures |

### cfuture Methods

| Method | Description |
|--------|-------------|
| [`~cfuture()`](src/rpp/future.h#L140) | **Fail-fast destructor**: if a valid future is not awaited before destruction, calls `std::terminate()`. If the future was already completed, any stored exception is caught and triggers an assertion failure. This is a deliberate deviation from `std::future` which silently blocks in the destructor — ReCpp terminates immediately to surface programming bugs. |
| [`then()`](src/rpp/future.h#L168) | Downcast `cfuture<T>` to `cfuture<void>` (discard return value) |
| [`then(Task task)`](src/rpp/future.h#L185) | Chain a continuation that receives the result (runs via `async_task`) |
| [`then(Task task, ExceptHA a, ...)`](src/rpp/future.h#L212) | Chain with 1–4 typed exception recovery handlers |
| [`then(cfuture<U>&& next)`](src/rpp/future.h#L264) | Chain by waiting for this future, then returning the result of `next` |
| [`continue_with(Task task)`](src/rpp/future.h#L280) | Fire-and-forget continuation (moves `*this` into background) |
| [`continue_with(Task task, ExceptHA a, ...)`](src/rpp/future.h#L288) | Fire-and-forget continuation with 1–4 typed exception handlers |
| [`detach()`](src/rpp/future.h#L343) | Abandon future, wait in background (swallows exceptions) |
| [`chain_async(Task task)`](src/rpp/future.h#L375) | Sequential chaining: if invalid, starts a new async task; if valid, appends as continuation (swallows prior exceptions) |
| [`chain_async(cfuture&& next)`](src/rpp/future.h#L388) | Sequential chaining with another future |
| [`await_ready()`](src/rpp/future.h#L402) | Non-blocking check if the future is already finished |
| [`collect_ready(T* result)`](src/rpp/future.h#L433) | If already finished, collects the result into `*result` (non-blocking). Returns `true` if collected |
| [`collect_wait(T* result)`](src/rpp/future.h#L451) | If valid, blocks until finished and collects the result into `*result`. Returns `true` if collected |
| [`await_suspend(coro_handle<>)`](src/rpp/future.h#L463) | C++20 coroutine suspension point — waits on background thread, then resumes |
| [`await_resume()`](src/rpp/future.h#L475) | C++20 coroutine resume — returns the result, rethrows exceptions |
| [`promise_type`](src/rpp/future.h#L502) | C++20 coroutine promise enabling `rpp::cfuture<T>` as a coroutine return type |
| [`RPP_HAS_COROUTINES`](src/rpp/future_types.h#L12) | Detects whether C++20 coroutine headers are available |
| [`RPP_CORO_STD`](src/rpp/future_types.h#L13) | Namespace alias for coroutine types (std or std::experimental) |

### Example: Composable Futures

```cpp
rpp::async_task([=]{
    return downloadZipFile(url);
}).then([=](std::string zipPath) {
    return extractContents(zipPath);
}).continue_with([=](std::string extractedDir) {
    callOnUIThread([=]{ jobComplete(extractedDir); });
});
```

### Example: Exception Recovery with `then()`

```cpp
rpp::async_task([=] {
    return loadCachedScene(getCachePath(file));
}, [=](invalid_cache_state e) {
    return loadCachedScene(downloadAndCacheFile(file)); // recover from bad cache
}).then([=](std::shared_ptr<SceneData> sceneData) {
    setCurrentScene(sceneData);
}, [=](scene_load_failed e) {
    loadDefaultScene(); // recover from failed scene load
});
```

### Example: Sequential Chaining with `chain_async()`

```cpp
cfuture<void> tasks;
tasks.chain_async([=]{
    downloadAssets(manifest);
}).chain_async([=]{
    parseAssets(manifest);   // runs after download completes, even if previous task throws an exception
}).chain_async([=]{
    initializeRenderer();    // runs after parsing completes
});
tasks.get(); // wait for the full chain
```

### Example: Non-blocking Result Polling with `collect_ready()`

```cpp
auto f = rpp::async_task([=]{ return computeResult(data); });

// poll in a game loop without blocking
int result;
while (!f.collect_ready(&result)) {
    renderLoadingScreen();
}
useResult(result);
```

### Example: `detach()` for Fire-and-Forget

```cpp
auto f = rpp::async_task([=]{
    sendAnalyticsEvent(event);
});

if (f.wait_for(rpp::millis(100)) == std::future_status::timeout) {
    LogWarning("Analytics event is taking too long to send, abandoning to unblock UI");
    f.detach(); // don't care about the result, avoid terminate on destruction
}
```

### Example: C++20 Coroutines

```cpp
using namespace rpp::coro_operators;
rpp::cfuture<void> downloadAndUnzipDataAsync(std::string url)
{
    std::string zipPath = co_await [=]{ return downloadZipFile(url); }; // spawns a new task to exec lambda on thread pool
    std::string extractedDir = co_await [=]{ return extractContents(zipPath); };
    co_await callOnUIThread([=]{ jobComplete(extractedDir); });
    co_return;
}
```

### Example: Coroutine Returning a Value

```cpp
using namespace rpp::coro_operators;
rpp::cfuture<UserProfile> fetchUserProfileAsync(std::string userId)
{
    std::string json = co_await [=]{ return httpGet("/api/users/" + userId); };
    UserProfile profile = co_await [=]{ return parseJson<UserProfile>(json); };
    co_return profile;
}
```

### Example: Coroutine with Async Sleep

```cpp
using namespace rpp::coro_operators;
rpp::cfuture<std::string> retryDownloadAsync(std::string url, int maxRetries)
{
    for (int i = 0; i < maxRetries; ++i) {
        try {
            co_return co_await [=]{ return httpGet(url); };
        } catch (network_error e) {
            if (i + 1 == maxRetries) throw;
            co_await std::chrono::seconds{1 << i}; // exponential backoff
        }
    }
    throw network_error{"retries exhausted"};
}
```

---

## rpp/coroutines.h

C++20 coroutine awaiters and `co_await` operators. Supports MSVC++, GCC, and Clang.

| Class | Description |
|-------|-------------|
| [`functor_awaiter<T>`](src/rpp/coroutines.h#L30) | Awaiter for lambdas/delegates via `parallel_task()` |
| [`functor_awaiter_fut<F>`](src/rpp/coroutines.h#L112) | Awaiter for lambdas returning futures |
| [`time_awaiter`](src/rpp/coroutines.h#L192) | Awaiter for `rpp::Duration` durations (async sleep) |

### co_await Operators (namespace `coro_operators`)

| Operator | Description |
|----------|-------------|
| [`operator co_await(delegate<T()>&&)`](src/rpp/coroutines.h#L245) | Run delegate async on thread pool |
| [`operator co_await(lambda&&)`](src/rpp/coroutines.h#L276) | Run lambda async on thread pool |
| [`operator co_await(cfuture<T>&)`](src/rpp/coroutines.h#L282) | Await a composable future |
| [`operator co_await(rpp::Duration)`](src/rpp/coroutines.h#L312) | Async sleep for a duration |

```cpp
using namespace rpp::coro_operators;
rpp::cfuture<void> example() {
    std::string zip = co_await [&]{ return downloadFile(url); };
    co_await std::chrono::milliseconds{100};
    co_await [&]{ unzipArchive(zip, "."); };
}
```

---

## rpp/event_loop.h

Single-threaded event loop that serializes coroutine completions. Unlike `thread_pool`, which resumes coroutines on background threads, `event_loop` ensures all coroutine resumes happen on the thread that drives the loop (typically the main thread).

### Classes

| Class | Description |
|-------|-------------|
| [`event_loop`](src/rpp/event_loop.h#L138) | Main event loop class with `run_loop()`, `run_once()`, `run_until_idle()`, `run_until_done(task)` |
| [`event_task`](src/rpp/event_loop.h#L48) | Lightweight top-level coroutine return type for event-loop-driven coroutines |

### event_loop Methods

| Method | Description |
|--------|-------------|
| [`run_loop()`](src/rpp/event_loop.h#L234) | Run the loop until `stop()` is called, then drain remaining work |
| [`run_once(Duration timeout)`](src/rpp/event_loop.h#L243) | Process at most one pending resume event; `Duration::zero()` for non-blocking poll |
| [`run_until_idle()`](src/rpp/event_loop.h#L251) | Run until no background tasks and no pending resume events remain |
| [`run_until_done(event_task& task)`](src/rpp/event_loop.h#L263) | Drive the loop until the given `event_task` completes, then rethrow on failure |
| [`run_async(Func&& fut_or_cb)`](src/rpp/event_loop.h#L532) | Dispatch future or lambda to thread pool, resume coroutine on the loop thread |
| [`fork(Func&& coro_factory)`](src/rpp/event_loop.h#L292) | Fork a concurrent coroutine path (fire-and-forget, tracked internally) |
| [`join_forks(Duration timeout)`](src/rpp/event_loop.h#L712) | Event-driven join: suspend until all forks complete or timeout expires |
| [`num_forks()`](src/rpp/event_loop.h#L336) | Number of active forked coroutines |
| [`drain_forks()`](src/rpp/event_loop.h#L344) | Check completed forks for exceptions and clear them |
| [`await(semaphore&, Duration)`](src/rpp/event_loop.h#L563) | Wait for semaphore signal, resume on loop thread |
| [`await(concurrent_queue<T>&, T&, Duration)`](src/rpp/event_loop.h#L577) | Pop from queue, resume on loop thread |
| [`await_pop(concurrent_queue<T>&, Duration)`](src/rpp/event_loop.h#L591) | Pop from queue returning `optional<T>`, resume on loop thread |
| [`post(delegate<void()> callback)`](src/rpp/event_loop.h#L358) | Post a callback to execute on the loop thread (like `run_on_main_thread`) |
| [`post_resume(coro_handle<> handle)`](src/rpp/event_loop.h#L350) | Post a raw coroutine handle resume to the loop thread |
| [`delay(Duration duration)`](src/rpp/event_loop.h#L631) | Sleep on a background thread, resume on the loop thread |
| [`delay_until(TimePoint until)`](src/rpp/event_loop.h#L635) | Sleep until a time point, resume on the loop thread |
| [`stop()`](src/rpp/event_loop.h#L210) | Signal the loop to stop and finalize pending tasks |
| [`wait_on_all(Duration timeout)`](src/rpp/event_loop.h#L217) | Block until all pending work drains, with timeout |
| [`set_except_handler(handler)`](src/rpp/event_loop.h#L223) | Set custom exception handler for unhandled background errors |
| [`has_pending_work()`](src/rpp/event_loop.h#L202) | True if any background tasks or resume events are pending |
| [`background_tasks()`](src/rpp/event_loop.h#L193) | Number of tasks currently suspended in background work |
| [`pending_completions()`](src/rpp/event_loop.h#L199) | Number of pending resume events queued for the loop thread |
| [`main_thread_id()`](src/rpp/event_loop.h#L205) | Thread ID of the loop's owner thread |

### event_task Methods

| Method | Description |
|--------|-------------|
| [`done()`](src/rpp/event_loop.h#L89) | True if the coroutine has finished or was never started |
| [`rethrow_if_exception()`](src/rpp/event_loop.h#L92) | Rethrow any unhandled exception captured by the coroutine |
| `on_complete` | Optional completion callback in `promise_type`, called at `final_suspend` (used by `fork()`) |

### event_loop Example

API showcase: [tests/test_event_loop.cpp:64](tests/test_event_loop.cpp#L64)

```cpp
rpp::event_loop loop;

rpp::cfuture<std::string> fetchData(rpp::event_loop& loop) {
    std::string raw = co_await loop.run_async([&]{
        return downloadFile(url);  // runs on thread pool
    });
    // resumed on event loop thread
    co_return parseData(raw);
}

auto future = fetchData(loop);
loop.run_until_idle();
auto result = future.get();
```

### event_task Example

`event_task` is a fire-and-forget coroutine handle driven by the event loop. Unlike `cfuture<T>`, it carries no return value — use it for top-level coroutines that coordinate work and write results into shared state.

API showcase: [tests/test_event_loop.cpp:123](tests/test_event_loop.cpp#L123)

```cpp
rpp::event_task updateUI(rpp::event_loop* loop) {
    int val = co_await loop->run_async([]{ return heavyWork(); });
    // resumed on main loop thread — safe to touch UI state
    widget.setText(std::to_string(val));

    co_await loop->run_async([]{ saveResults(); });
    // still on main loop thread
    statusBar.show("Done");
}

rpp::event_loop loop;
auto task = updateUI(&loop);   // starts immediately, suspends at first co_await
loop.run_until_done(task);     // drives the loop until task completes, rethrows on failure
```

### fork() Example

`fork()` launches concurrent coroutine paths on the event loop. Forks are fire-and-forget — tracked internally, cleaned up automatically by `run_until_idle()`. Use `join_forks()` for event-driven join with timeout.

```cpp
rpp::event_loop loop;

// fork two concurrent paths — no manual future tracking needed
loop.fork([&]() -> rpp::event_task {
    auto data = co_await loop.run_async([&]{ return fetchData(); });
    state.data = data;  // safe: resumes on event loop thread
});
loop.fork([&]() -> rpp::event_task {
    co_await loop.run_async([&]{ return processVideo(); });
    state.videoReady = true;
});

loop.run_until_idle();  // drives both forks, cleans up on completion
```

Inside a coroutine, use `join_forks()` for structured concurrency with timeout:

```cpp
auto main_task = [&]() -> rpp::event_task {
    loop.fork([&]() -> rpp::event_task { /* path 1 */ });
    loop.fork([&]() -> rpp::event_task { /* path 2 */ });
    int remaining = co_await loop.join_forks(rpp::seconds(5));
    if (remaining > 0) { /* timeout: some forks still running */ }
};
```

### Semaphore / Queue Await Example

Event-loop-aware `co_await` for semaphores and queues — resumes on the loop thread:

```cpp
// wait for semaphore signal
auto wr = co_await loop.await(sem, rpp::millis(100));
if (wr == rpp::semaphore::notified) { /* signaled */ }

// pop from queue (returns bool)
std::string item;
bool got = co_await loop.await(queue, item, rpp::millis(100));

// pop from queue (returns optional<T>)
auto opt = co_await loop.await_pop(queue, rpp::millis(100));
if (opt) { use(*opt); }
```

---

## rpp/thread_pool.h

Thread pool with `parallel_for`, `parallel_foreach`, and async task support.

### Classes

| Class | Description |
|-------|-------------|
| [`thread_pool`](src/rpp/thread_pool.h#L396) | Thread pool manager with auto-scaling workers |
| [`pool_task_handle`](src/rpp/thread_pool.h#L144) | Waitable, reference-counted handle for pool tasks |
| [`pool_worker`](src/rpp/thread_pool.h#L118) | Individual worker thread in the pool |

### thread_pool Methods

| Method | Description |
|--------|-------------|
| [`parallel_for(int rangeStart, int rangeEnd, int rangeStride, TaskFunc&& func)`](src/rpp/thread_pool.h#L569) | Split work across threads |
| [`parallel_task(Task task)`](src/rpp/thread_pool.h#L519) | Run a single async task, returns `pool_task_handle` |
| [`set_max_parallelism(int max)`](src/rpp/thread_pool.h#L433) | Set max concurrent workers |
| [`max_parallelism()`](src/rpp/thread_pool.h#L436) | Get max concurrent workers |
| [`active_tasks()`](src/rpp/thread_pool.h#L449) | Number of currently running tasks |
| [`idle_tasks()`](src/rpp/thread_pool.h#L452) | Number of idle workers |
| [`total_tasks()`](src/rpp/thread_pool.h#L455) | Total number of workers |
| [`clear_idle_tasks()`](src/rpp/thread_pool.h#L459) | Remove idle workers |

### Free Functions (Global Pool)

| Function | Description |
|----------|-------------|
| [`parallel_for(rangeStart, rangeEnd, maxRangeSize, func)`](src/rpp/thread_pool.h#L569) | Parallel for on the global thread pool |
| [`parallel_foreach(items, forEach)`](src/rpp/thread_pool.h#L590) | Parallel foreach on the global pool |
| [`parallel_task(task)`](src/rpp/thread_pool.h#L519) | Run async task on the global pool |
| [`action<TArgs...>`](src/rpp/thread_pool.h#L48) | Lightweight non-owning delegate for blocking call contexts |

### Example: parallel_for

```cpp
#include <rpp/thread_pool.h>

std::vector<Image> images = loadImages();

// process images in parallel using the global thread pool
// callback receives [start, end) range — efficient for lightweight per-item work
rpp::parallel_for(0, (int)images.size(), 0, [&](int start, int end) {
    for (int i = start; i < end; ++i)
        processImage(images[i]);
});
// blocks until all tasks complete

// with explicit max_range_size=1 for heavy per-item tasks
rpp::parallel_for(0, (int)images.size(), 1, [&](int start, int end) {
    for (int i = start; i < end; ++i)
        renderScene(images[i]); // each item gets its own thread
});
```

### Example: parallel_foreach

```cpp
// parallel_foreach is a convenience wrapper — one callback per item
std::vector<std::string> files = rpp::list_files("textures", "png", rpp::dir_fullpath_recursive);

rpp::parallel_foreach(files, [](std::string& path) {
    compressTexture(path);
});
// blocks until all items are processed
```

### Example: parallel_task (Fire-and-Forget)

```cpp
// launch an async task on the global pool — returns immediately
rpp::pool_task_handle handle = rpp::parallel_task([=] {
    uploadAnalytics(report);
});

// optionally wait for completion
handle.wait();
```

### Example: Using a Dedicated Thread Pool

```cpp
// create a pool with limited parallelism for IO-bound work
rpp::thread_pool ioPool;
ioPool.set_max_parallelism(4);

ioPool.parallel_for(0, (int)urls.size(), 1, [&](int start, int end) {
    for (int i = start; i < end; ++i)
        downloadFile(urls[i]);
});
```

---

## rpp/threads.h

Thread naming and information utilities.

| Function | Description |
|----------|-------------|
| [`set_this_thread_name(name)`](src/rpp/threads.h#L16) | Set current thread debug name (max 15 chars on Linux) |
| [`get_this_thread_name()`](src/rpp/threads.h#L21) | Get current thread name |
| [`get_thread_id()`](src/rpp/threads.h#L31) | Get current thread ID (uint64) |
| [`get_process_id()`](src/rpp/threads.h#L36) | Get current process ID (uint32) |
| [`num_physical_cores()`](src/rpp/threads.h#L41) | Number of physical CPU cores |
| [`yield()`](src/rpp/threads.h#L47) | Yield execution to another thread |
| [`get_thread_name(thread_id)`](src/rpp/threads.h#L26) | Returns the debug name of a thread by its ID |

---

## rpp/mutex.h

Cross-platform mutex, spin locks, and synchronized value wrappers.

### Classes

| Class | Description |
|-------|-------------|
| [`mutex`](src/rpp/mutex.h#L13) | Platform-specific mutex (custom on Windows/FreeRTOS, `std::mutex` on Linux/Mac) |
| [`recursive_mutex`](src/rpp/mutex.h#L34) | Recursive mutex variant |
| [`unlock_guard<Mutex>`](src/rpp/mutex.h#L171) | RAII unlock guard: unlocks on construction, relocks on destruction |
| [`synchronized<T>`](src/rpp/mutex.h#L410) | Thread-safe value wrapper, accessed via `sync()` → `synchronize_guard` |

### Free Functions

| Function | Description |
|----------|-------------|
| [`spin_lock(Mutex m)`](src/rpp/mutex.h#L195) | Spin-lock with fallback to blocking lock |
| [`spin_lock_for(Mutex m, timeout)`](src/rpp/mutex.h#L231) | Spin-lock with timeout |
| [`RPP_HAS_CRITICAL_SECTION_MUTEX`](src/rpp/mutex.h#L126) | Indicates platform provides native critical_section mutex |
| [`RPP_SYNC_T`](src/rpp/mutex.h#L258) | Template constraint placeholder for SyncableType concept |

### Example: Basic Mutex and Spin Lock

```cpp
#include <rpp/mutex.h>

rpp::mutex mtx;

// standard lock_guard usage
{
    std::lock_guard<rpp::mutex> lock{mtx};
    sharedData.push_back(value);
}

// spin_lock: spins briefly before blocking — much faster under contention
{
    auto lock = rpp::spin_lock(mtx);
    sharedData.push_back(value);
}

// spin_lock_for: spin with a timeout, check if lock was acquired
{
    auto lock = rpp::spin_lock_for(mtx, rpp::Duration::from_millis(50));
    if (lock.owns_lock())
        sharedData.push_back(value);
}
```

### Example: synchronized\<T\> Wrapper

```cpp
#include <rpp/mutex.h>

// wrap any type for automatic thread-safe access
rpp::synchronized<std::vector<int>> items;

// arrow operator returns a synchronize_guard that acquires a temporary lock for safe access:
items->push_back(10); // locks, pushes, unlocks immediately after the statement

// operator* returns a synchronize_guard that locks on construction
// and unlocks on destruction — like a combined lock_guard + accessor
{
    auto guard = *items;
    guard->push_back(42);
    guard->push_back(99);
} // mutex released here

// direct assignment through the guard
*items = std::vector<int>{1, 2, 3};

// read through the guard
{
    for (int val : *items)
        LogInfo("item: %d", val);
}
```

---

## rpp/semaphore.h

Counting semaphore and lightweight notification flags.

| Class | Description |
|-------|-------------|
| [`semaphore`](src/rpp/semaphore.h#L30) | Counting semaphore with spin-lock optimization |
| [`semaphore_flag`](src/rpp/semaphore.h#L404) | Lighter semaphore using a single atomic flag |
| [`semaphore_once_flag`](src/rpp/semaphore.h#L442) | One-shot semaphore that can only be set once |

### semaphore Methods

| Method | Description |
|--------|-------------|
| [`notify()`](src/rpp/semaphore.h#L109) | Increment and wake one waiter |
| [`notify_all()`](src/rpp/semaphore.h#L146) | Wake all waiters |
| [`notify_once()`](src/rpp/semaphore.h#L181) | Notify only if not already signaled |
| [`try_wait()`](src/rpp/semaphore.h#L218) | Non-blocking wait attempt |
| [`wait()`](src/rpp/semaphore.h#L238) | Blocking wait |
| [`wait(Duration timeout)`](src/rpp/semaphore.h#L273) | Wait with timeout |
| [`await(Duration timeout)`](src/rpp/semaphore.h#L393) | C++20 coroutine `co_await` — dispatches wait to background thread |
| [`count()`](src/rpp/semaphore.h#L69) | Current count |
| [`reset()`](src/rpp/semaphore.h#L60) | Reset to zero |

### Example: Coroutine co_await

```cpp
#include <rpp/semaphore.h>
#include <rpp/coroutines.h>

rpp::semaphore sem;

rpp::cfuture<void> waitForSignal() {
    auto result = co_await sem.await(rpp::millis(100));
    if (result == rpp::semaphore::notified) { /* handle signal */ }
}
```

### Example: Producer-Consumer with semaphore

```cpp
#include <rpp/semaphore.h>

rpp::semaphore workReady;
std::vector<Task> taskQueue;

// producer thread
taskQueue.push_back(newTask);
workReady.notify(); // wake one consumer

// consumer thread
workReady.wait(); // blocks until notified, then decrements count
Task t = taskQueue.pop_back();
process(t);

// non-blocking check
if (workReady.try_wait())
    handleImmediateWork();

// wait with timeout, uses rpp::condition_variable internally for more accurate waits
if (workReady.wait(rpp::Duration::from_millis(100)) == rpp::semaphore::notified)
    handleWork();
else
    handleTimeout();
```

### Example: semaphore_flag and semaphore_once_flag

```cpp
#include <rpp/semaphore.h>

// semaphore_flag: a simple set/unset signal (max count = 1)
rpp::semaphore_flag shutdownFlag;

// worker thread
while (!shutdownFlag.try_wait())
{
    doWork();
    rpp::sleep_ms(100);
}

// main thread signals shutdown
shutdownFlag.notify();

// semaphore_once_flag: set once, wait() never unsets — ideal for "init done" signals
rpp::semaphore_once_flag initDone;

// init thread
loadResources();
initDone.notify(); // signal that init is complete

// any number of threads can wait — once set, wait() returns immediately
initDone.wait(); // does NOT consume the signal
initDone.wait(); // still returns immediately
```

---

## rpp/condition_variable.h

Extended condition variable with `rpp::Duration` and `rpp::TimePoint` overloads. On Windows provides a custom implementation with spin-wait for sub-15.6ms timeouts. This is a significant upgrade for MSVC++ where `std::condition_variable` cannot do fine-grained waits.

| Class | Description |
|-------|-------------|
| [`condition_variable`](src/rpp/condition_variable.h#L60) | Condition variable with high-resolution timeout support |

### Example

```cpp
#include <rpp/condition_variable.h>

rpp::mutex mtx;
rpp::condition_variable cv;
bool dataReady = false;

// waiting thread
{
    std::unique_lock<rpp::mutex> lock{mtx};
    cv.wait(lock, [&]{ return dataReady; });
    processData();
}

// notifying thread
{
    std::lock_guard<rpp::mutex> lock{mtx};
    dataReady = true;
}
cv.notify_one();

// high-resolution wait with rpp::Duration (sub-15.6ms on Windows), does not exceed the specified timeout
{
    std::unique_lock<rpp::mutex> lock{mtx};
    if (cv.wait_for(lock, rpp::Duration::from_millis(5)) == std::cv_status::timeout)
        handleTimeout();
}
```

---

## rpp/sockets.h

Full cross-platform TCP/UDP socket wrapper with IPv4/IPv6 support.

### Enums

| Enum | Description |
|------|-------------|
| [`address_family`](src/rpp/sockets.h#L23) | `AF_DontCare`, `AF_IPv4`, `AF_IPv6`, `AF_Bth` |
| [`socket_type`](src/rpp/sockets.h#L32) | `ST_Stream`, `ST_Datagram`, `ST_Raw`, `ST_RDM`, `ST_SeqPacket` |
| [`socket_category`](src/rpp/sockets.h#L42) | `SC_Unknown`, `SC_Listen`, `SC_Accept`, `SC_Client` |
| [`ip_protocol`](src/rpp/sockets.h#L50) | `IPP_TCP`, `IPP_UDP`, `IPP_ICMP`, `IPP_BTH`, etc. |
| [`socket_option`](src/rpp/sockets.h#L62) | `SO_None`, `SO_ReuseAddr`, `SO_Blocking`, `SO_NonBlock`, `SO_Nagle` |

### Classes

| Class | Description |
|-------|-------------|
| [`raw_address`](src/rpp/sockets.h#L104) | IP address without port (IPv4/IPv6) |
| [`ipaddress`](src/rpp/sockets.h#L227) | IP address + port, constructible from `"ip:port"` strings |
| [`ipaddress4`](src/rpp/sockets.h#L400) | IPv4 convenience wrapper |
| [`ipaddress6`](src/rpp/sockets.h#L435) | IPv6 convenience wrapper |
| [`ipinterface`](src/rpp/sockets.h#L467) | Network interface info (name, addr, netmask, broadcast, gateway) |
| [`socket`](src/rpp/sockets.h#L516) | Full TCP/UDP socket with send, recv, select, etc. |

### socket Methods

| Method | Description |
|--------|-------------|
| [`close()`](src/rpp/sockets.h#L603) | Close the socket |
| [`send(const void* data, int numBytes)`](src/rpp/sockets.h#L722) | Send data |
| [`recv(void* buf, int maxBytes)`](src/rpp/sockets.h#L846) | Receive data |
| [`sendto(const ipaddress& addr, const void* data, int numBytes)`](src/rpp/sockets.h#L758) | UDP send to address |
| [`recvfrom(ipaddress& addr, void* buf, int maxBytes)`](src/rpp/sockets.h#L876) | UDP receive with source address |
| [`flush()`](src/rpp/sockets.h#L796) | Flush pending data |
| [`peek(void* buf, int maxBytes)`](src/rpp/sockets.h#L869) | Peek at incoming data without consuming |
| [`skip(int bytes)`](src/rpp/sockets.h#L819) | Skip incoming bytes |
| [`available()`](src/rpp/sockets.h#L830) | Bytes available to read |
| [`select(int millis)`](src/rpp/sockets.h#L1221) | Wait for socket readability |
| [`good()`](src/rpp/sockets.h#L622) / [`bad()`](src/rpp/sockets.h#L624) | Socket state |
| [`set_nagle(bool enable)`](src/rpp/sockets.h#L1070) | Enable/disable Nagle's algorithm |

### socket Static Methods

| Method | Description |
|--------|-------------|
| [`socket::listen(localAddr, ipp, opt)`](src/rpp/sockets.h#L1314) | Create a listening server socket |
| [`socket::listen_to(localAddr, ipp, opt)`](src/rpp/sockets.h#L1323) | Create a listening server socket |
| [`socket::accept(timeoutMillis)`](src/rpp/sockets.h#L1367) | Accept an incoming connection |
| [`socket::connect(remoteAddr, opt)`](src/rpp/sockets.h#L1375) | Connect to an address |
| [`socket::connect_to(remoteAddr, opt)`](src/rpp/sockets.h#L1398) | Connect by hostname and port |
| [`protocol_info`](src/rpp/sockets.h#L89) | Describes socket protocol version, address family, type and protocol |
| [`make_udp_randomport(opt, bind_address)`](src/rpp/sockets.h#L1497) | Creates an INADDR_ANY UDP socket bound to a random port |
| [`make_tcp_randomport(opt, bind_address)`](src/rpp/sockets.h#L1503) | Creates an INADDR_ANY TCP listener bound to a random port |

### Example: IP Addresses

```cpp
#include <rpp/sockets.h>

// construct from "ip:port" string — auto-detects IPv4 vs IPv6
rpp::ipaddress addr{"192.168.1.100:8080"};
LogInfo("addr: %s port: %d", addr.str(), addr.port()); // "192.168.1.100:8080"

// construct with explicit family
rpp::ipaddress4 v4{"10.0.0.1", 5000};
rpp::ipaddress6 v6{"::1", 5000};

// listener address (any interface, specific port)
rpp::ipaddress listener{rpp::AF_IPv4, 14550};

// resolve hostname
rpp::ipaddress resolved{rpp::AF_IPv4, "example.com", 80};
LogInfo("resolved example.com to %s", resolved.str());
```

### Example: TCP Client

```cpp
// connect to a remote server (non-blocking by default, TCP_NODELAY enabled)
rpp::socket client = rpp::socket::connect_to(rpp::ipaddress4{"127.0.0.1", 8080});
if (client.bad())
{
    LogError("connect failed: %s", client.last_err());
    return;
}

// send data — accepts const char*, std::string, strview, vector<uint8_t>
client.send("GET / HTTP/1.0\r\nHost: localhost\r\n\r\n");

// wait for response with timeout
std::string response = client.wait_recv_str(/*millis=*/2000);
if (!response.empty())
    LogInfo("response: %s", response);

// socket auto-closes on destruction
```

### Example: TCP Server (Listen + Accept)

```cpp
// create a TCP listener on port 9000
rpp::socket server = rpp::socket::listen_to(rpp::ipaddress4{9000});
if (server.bad())
{
    LogError("listen failed: %s", server.last_err());
    return;
}
LogInfo("listening on %s", server.str());

// accept loop — timeout=100ms for non-blocking polling
while (running)
{
    rpp::socket client = server.accept(/*timeoutMillis=*/100);
    if (client.good())
    {
        LogInfo("client connected from %s", client.str());

        // echo server: receive and send back
        char buf[1024];
        int n = client.recv(buf, sizeof(buf));
        if (n > 0) client.send(buf, n);
    }
    // client auto-closes when it goes out of scope
}
```

### Example: UDP Send/Receive

```cpp
// create a UDP socket bound to port 5000
rpp::socket udp = rpp::socket::listen_to_udp(5000);

// send a datagram to a specific address
rpp::ipaddress4 target{"192.168.1.255", 5000};
udp.sendto(target, "hello from UDP");

// receive datagrams
rpp::ipaddress from;
char buf[1024];
int n = udp.recvfrom(from, buf, sizeof(buf));
if (n > 0) LogInfo("received %d bytes from %s", n, from.str());

// convenience: wait with timeout and get as string
std::string msg = udp.wait_recvfrom_str(from, /*millis=*/1000);
```

### Example: UDP Broadcast

```cpp
rpp::socket udp = rpp::socket::listen_to_udp(7000);
udp.enable_broadcast();

rpp::ipaddress4 broadcast{"255.255.255.255", 7000};
udp.sendto(broadcast, "discovery ping");
```

### Example: Non-Blocking I/O with poll()

```cpp
rpp::socket client = rpp::socket::connect_to(rpp::ipaddress4{"10.0.0.1", 8080},
                                             rpp::SO_NonBlock);

// poll() is faster than select() — returns true when data is available
if (client.poll(/*timeoutMillis=*/500, rpp::socket::PF_Read))
{
    std::string data = client.recv_str();
    process(data);
}

// poll multiple sockets at once
std::vector<rpp::socket*> sockets = {&sock1, &sock2, &sock3};
std::vector<int> ready;
int numReady = rpp::socket::poll(sockets, ready, /*timeoutMillis=*/100);
for (int idx : ready)
    handleSocket(*sockets[idx]);
```

### Example: Network Interfaces

```cpp
// list all IPv4 network interfaces
// WARNING: this is inherently slow on both Linux and Windows, so always cache the results !
auto interfaces = rpp::ipinterface::get_interfaces(rpp::AF_IPv4);
for (auto& iface : interfaces)
    LogInfo("%-10s  addr=%-16s  gw=%s", iface.name, iface.addr.str(), iface.gateway.str());

// get the system's main IP and broadcast addresses
std::string myIp = rpp::get_system_ip();         // e.g. "192.168.1.42"
std::string bcast = rpp::get_broadcast_ip();      // e.g. "192.168.1.255"

// bind a socket to a specific network interface
rpp::socket udp = rpp::socket::listen_to_udp(5000);
udp.bind_to_interface("eth0");
```

### AF_Unix (local) sockets

`rpp::socket` natively supports message-oriented AF_UNIX sockets. Supported on
Linux (`SOCK_SEQPACKET` in the abstract namespace -- no filesystem object,
released by the kernel on close) and Windows (Win10 1803+, framed `SOCK_STREAM`:
messages carry a 2-byte little-endian length prefix internally and names map to
`%TEMP%\<name>.sock`). Other platforms are unsupported -- `create(AF_Unix, ...)`
fails with `SE_SOCKFAMILY`. The maximum message size is `max_unix_message_size`
(65535 bytes); `send()` rejects larger messages on all platforms for uniformity.
fd passing (`send_fd` / `recv_fd`) and `pair()` are Linux-only. AF_Unix sockets
follow the socket's normal blocking mode (use `set_blocking(false)` for
non-blocking I/O).

| Method | Description |
|--------|-------------|
| [`max_unix_message_size`](src/rpp/sockets.h#L527) | Largest AF_Unix message send()/recv() can carry (65535 bytes) |
| [`ipaddress::unix_addr(strview name)`](src/rpp/sockets.h#L309) | Build an AF_Unix ipaddress from a local socket name |
| [`raw_address::from_unix_name(strview name)`](src/rpp/sockets.h#L160) | Build an AF_Unix raw_address (rejects over-long names) |
| [`raw_address::is_unix()`](src/rpp/sockets.h#L150) | True if this raw_address is an AF_Unix local socket name |
| [`socket::create(AF_Unix)`](src/rpp/sockets.h#L1171) | Create an AF_Unix socket (SEQPACKET on Linux, framed stream on Windows) |
| [`socket::listen_unix(strview name, int backlog = 1, socket_option opt = SO_None)`](src/rpp/sockets.h#L1461) | Create a listening AF_Unix socket bound to a name |
| [`socket::connect_unix(strview name, socket_option opt = SO_None)`](src/rpp/sockets.h#L1470) | Connect to an AF_Unix listener by name |
| [`socket::pair(socket& a, socket& b)`](src/rpp/sockets.h#L1451) | Create a connected AF_Unix socket pair (Linux only) |
| [`socket::send_fd(uint32_t tag, int fd)`](src/rpp/sockets.h#L1479) | Pass an fd + tag via SCM_RIGHTS (Linux only) |
| [`socket::recv_fd(uint32_t& out_tag, int& out_fd)`](src/rpp/sockets.h#L1488) | Receive a passed fd (1 = fd, 0 = nothing/skip, -1 = EOF; Linux only) |
| [`wait_readable(std::initializer_list<int> fds, int timeout_ms)`](src/rpp/sockets.h#L1541) | Block until one of up to 8 socket fds is readable, or timeout |

```cpp
rpp::socket listener = rpp::socket::listen_unix("my-service");
rpp::socket client = rpp::socket::connect_unix("my-service");
rpp::socket server = listener.accept(2000);

client.send("hello", 5);
char buf[64];
int n = server.recv(buf, sizeof(buf)); // n == 5, message boundary preserved
```

---

## rpp/binary_stream.h

Buffered binary read/write stream with abstract source interface.

| Class | Description |
|-------|-------------|
| [`stream_source`](src/rpp/binary_stream.h#L27) | Abstract stream interface (implement for custom sources) |
| [`binary_stream`](src/rpp/binary_stream.h#L128) | Buffered binary stream with typed read/write |
| [`file_writer`](src/rpp/binary_stream.h#L641) | File-backed stream_source |
| [`binary_buffer`](src/rpp/binary_stream.h#L567) | Binary stream that doesn't flush data |
| [`socket_writer`](src/rpp/binary_stream.h#L582) | Binary socket writer for UDP/TCP |
| [`socket_reader`](src/rpp/binary_stream.h#L608) | Binary socket reader for UDP/TCP |
| [`file_reader`](src/rpp/binary_stream.h#L708) | File-backed binary stream reader |

### binary_stream Methods

| Method | Description |
|--------|-------------|
| [`write(const void* data, int numBytes)`](src/rpp/binary_stream.h#L219) | Write raw bytes |
| [`write<T>(const T& value)`](src/rpp/binary_stream.h#L260) | Write a typed value |
| [`write(strview s)`](src/rpp/binary_stream.h#L285) | Write a string |
| [`read(void* dst, int max)`](src/rpp/binary_stream.h#L354) | Read raw bytes |
| [`read<T>()`](src/rpp/binary_stream.h#L386) | Read a typed value |
| [`write_byte()`](src/rpp/binary_stream.h#L260) / [`write_int16()`](src/rpp/binary_stream.h#L262) / [`write_int32()`](src/rpp/binary_stream.h#L266) / [`write_int64()`](src/rpp/binary_stream.h#L270) | Write specific integer sizes |
| [`write_float()`](src/rpp/binary_stream.h#L274) / [`write_double()`](src/rpp/binary_stream.h#L276) | Write floating point |
| [`read_byte()`](src/rpp/binary_stream.h#L391) / [`read_int16()`](src/rpp/binary_stream.h#L393) / [`read_int32()`](src/rpp/binary_stream.h#L397) / [`read_int64()`](src/rpp/binary_stream.h#L401) | Read specific integer sizes |
| [`read_float()`](src/rpp/binary_stream.h#L405) / [`read_double()`](src/rpp/binary_stream.h#L407) | Read floating point |
| [`read_string()`](src/rpp/binary_stream.h#L473) | Read a length-prefixed string |
| [`peek(void* buf, int numBytes)`](src/rpp/binary_stream.h#L366) | Peek without consuming |
| [`skip(int numBytes)`](src/rpp/binary_stream.h#L380) | Skip bytes |
| [`flush()`](src/rpp/binary_stream.h#L205) | Flush write buffer |
| [`good()`](src/rpp/binary_stream.h#L193) | True if stream is valid |
| [`size()`](src/rpp/binary_stream.h#L170) / [`capacity()`](src/rpp/binary_stream.h#L171) | Buffer metrics |
| [`data()`](src/rpp/binary_stream.h#L163) / [`view()`](src/rpp/binary_stream.h#L172) | Access buffer data |

### Example: In-Memory Binary Read/Write

```cpp
#include <rpp/binary_stream.h>

// binary_stream without a stream_source works as an in-memory buffer
rpp::binary_stream bs;

// typed write methods
bs.write_int32(42);
bs.write_float(3.14f);
bs.write("hello"); // writes [uint16 len][data]

// read back in the same order
bs.rewind(); // reset read position to beginning
rpp::int32 id    = bs.read_int32();   // 42
float value      = bs.read_float();   // 3.14
std::string name = bs.read_string();  // "hello"
```

### Example: operator<< and operator>> Streaming

```cpp
rpp::binary_stream bs;

// write with operator<<
bs << rpp::int32(1) << rpp::int32(2) << 3.14f << std::string("world");

// read with operator>>
bs.rewind();
rpp::int32 a, b;
float f;
std::string s;
bs >> a >> b >> f >> s;
// a=1, b=2, f=3.14, s="world"
```

### Example: Vectors and Containers

```cpp
rpp::binary_stream bs;

// write a vector — stored as [int32 count][elements...]
std::vector<float> positions = {1.0f, 2.0f, 3.0f, 4.0f};
bs << positions;

// read it back
bs.rewind();
std::vector<float> loaded;
bs >> loaded;
// loaded == {1.0f, 2.0f, 3.0f, 4.0f}
```

### Example: File-Backed Stream with file_writer

```cpp
#include <rpp/binary_stream.h>

// write binary data to a file
{
    rpp::file_writer out{"data.bin"};
    out << rpp::int32(100) << 2.5f << std::string("payload");
    // auto-flushed on destruction
}

// read it back with a file_reader (rpp::file + rpp::binary_stream)
{
    rpp::file_reader fr{"data.bin"};
    rpp::int32 id = fr.read_int32();
    float val     = fr.read_float();
    std::string s = fr.read_string();
}
```

### Example: Socket Streams

```cpp
#include <rpp/binary_stream.h>

rpp::socket sock = rpp::socket::connect_to(rpp::ipaddress4{"127.0.0.1", 9000});

// socket_writer buffers writes and flushes to the socket
// this is inherently a STREAM based utility
rpp::socket_writer out{sock};
out << rpp::int32(42) << std::string("request data");
out.flush(); // or out << rpp::endl;

// socket_reader buffers reads from the socket
rpp::socket_reader in{sock};
if (sock.wait_available(2000))
{
    rpp::int32 code = in.read_int32();
    std::string response = in.read_string();
}
```

---

## rpp/binary_serializer.h

Reflection-based binary and string serialization using CRTP.

| Class | Description |
|-------|-------------|
| [`member_serialize<T>`](src/rpp/binary_serializer.h#L15) | Per-member serialization metadata |
| [`serializable<T>`](src/rpp/binary_serializer.h#L29) | CRTP base class — inherit and call `bind()` to auto-serialize |

### serializable Methods

| Method | Description |
|--------|-------------|
| [`bind(first, args, ...)`](src/rpp/binary_serializer.h#L84) | Bind struct members for serialization |
| [`bind_name(name, elem)`](src/rpp/binary_serializer.h#L78) | Bind with explicit name |
| [`serialize(binary_stream& out)`](src/rpp/binary_serializer.h#L92) | Serialize to binary stream |
| [`deserialize(binary_stream& in)`](src/rpp/binary_serializer.h#L100) | Deserialize from binary stream |
| [`serialize(string_buffer& out)`](src/rpp/binary_serializer.h#L110) | Serialize to string |
| [`deserialize(strview& in)`](src/rpp/binary_serializer.h#L124) | Deserialize from string |

### Example: Defining a Serializable Struct

```cpp
#include <rpp/binary_serializer.h>

struct Player : rpp::serializable<Player>
{
    std::string name;
    rpp::int32 score = 0;
    float health = 100.0f;

    // bind members for automatic serialization
    void introspect()
    {
        bind(name, score, health);
    }
};
```

### Example: Binary Serialization

```cpp
Player alice;
alice.name = "Alice";
alice.score = 1500;
alice.health = 87.5f;

// serialize to binary stream
rpp::binary_stream bs;
bs << alice;

// deserialize from binary stream
bs.rewind();
Player loaded;
bs >> loaded;
// loaded.name == "Alice", loaded.score == 1500, loaded.health == 87.5
```

### Example: String Serialization

```cpp
// serialize to string — produces "name;Alice;score;1500;health;87.5;\n"
rpp::string_buffer sb;
alice.serialize(sb);

// deserialize from string
rpp::strview sv = sb.view();
Player parsed;
parsed.deserialize(sv);
```

### Example: Named Members with bind_name

```cpp
struct Config : rpp::serializable<Config>
{
    std::string host;
    rpp::int32 port = 0;
    bool verbose = false;

    void introspect()
    {
        bind_name("host", host);
        bind_name("port", port);
        bind_name("verbose", verbose);
    }
};

// string serialization uses the bound names as keys:
// "host;localhost;port;8080;verbose;1;\n"
```

---

## rpp/timepoint.h

Nanosecond-precision `Duration` and `TimePoint` types, time constants, sleep utilities, and convenience duration literals. This header is the foundation for all time-related functionality in ReCpp.

### Types

| Type | Description |
|------|-------------|
| [`Duration`](src/rpp/timepoint.h#L27) | Unified nanosecond-precision duration (int64 nsec) |
| [`TimePoint`](src/rpp/timepoint.h#L296) | System's most accurate time point measurement |
| [`ClockType`](src/rpp/timepoint.h#L73) | Clock source selector for `TimePoint::now(ClockType)` |

### ClockType Values

| Value | Linux API | Linux precision | Windows API | Windows precision |
|-------|-----------|-----------------|-------------|-------------------|
| `Realtime` | `CLOCK_REALTIME` | ~1ns | `GetSystemTimePreciseAsFileTime` | ~100ns |
| `RealtimeCoarse` | `CLOCK_REALTIME_COARSE` | ~1-4ms | `GetSystemTimeAsFileTime` | ~15.6ms |
| `TAI` | `CLOCK_TAI` | ~1ns | falls back to Realtime | ~100ns |
| `Monotonic` | `CLOCK_MONOTONIC` | ~1ns | `QueryPerformanceCounter` | ~100ns-1us |
| `MonotonicRaw` | `CLOCK_MONOTONIC_RAW` | ~1ns | `QueryPerformanceCounter` | ~100ns-1us |
| `MonotonicCoarse` | `CLOCK_MONOTONIC_COARSE` | ~1-4ms | `GetTickCount64` | ~15.6ms |
| `Boottime` | `CLOCK_BOOTTIME` | ~1ns | `GetTickCount64` | ~15.6ms |
| `ProcessCPU` | `CLOCK_PROCESS_CPUTIME_ID` | ~1ns | `GetProcessTimes` | ~15.6ms |
| `ThreadCPU` | `CLOCK_THREAD_CPUTIME_ID` | ~1ns | `GetThreadTimes` | ~15.6ms |

macOS uses `clock_gettime()` but lacks Coarse, Boottime, and TAI variants — these fall back to their regular equivalents. Other platforms fall back to default `now()`.

Monotonic clocks (`Monotonic`, `MonotonicRaw`, `MonotonicCoarse`, `Boottime`) are automatically synchronized to the Realtime epoch on first use. The offset `(Realtime - Monotonic)` is captured once per clock type and applied to all subsequent reads, so monotonic TimePoints are directly printable with `to_string()` while retaining monotonic guarantees (never going backwards). CPU time clocks (`ProcessCPU`, `ThreadCPU`) are not synchronized since they measure CPU consumption, not wall-clock time.

### Duration Factory & Accessors

| Method | Description |
|--------|-------------|
| [`Duration::from_seconds(double s)`](src/rpp/timepoint.h#L143) | Create from fractional seconds |
| [`Duration::from_millis(double ms)`](src/rpp/timepoint.h#L415) | Create from milliseconds |
| [`Duration::from_micros(double us)`](src/rpp/timepoint.h#L417) | Create from microseconds |
| [`Duration::from_nanos(int64 ns)`](src/rpp/timepoint.h#L158) | Create from nanoseconds |
| [`Duration::from_hours(double h)`](src/rpp/timepoint.h#L169) | Create from hours |
| [`Duration::from_minutes(int32 m)`](src/rpp/timepoint.h#L165) | Create from minutes |
| [`sec()`](src/rpp/timepoint.h#L176) / [`msec()`](src/rpp/timepoint.h#L180) | Convert to fractional seconds / milliseconds |
| [`seconds()`](src/rpp/timepoint.h#L184) / [`millis()`](src/rpp/timepoint.h#L188) / [`micros()`](src/rpp/timepoint.h#L192) / [`nanos()`](src/rpp/timepoint.h#L197) | Convert to integer time units |
| [`to_string()`](src/rpp/timepoint.h#L261) | Human-readable duration string |
| [`to_stopwatch_string()`](src/rpp/timepoint.h#L278) | Stopwatch-style format |

### TimePoint Methods

| Method | Description |
|--------|-------------|
| [`TimePoint::now()`](src/rpp/timepoint.h#L331) | Current OS high-accuracy time point |
| [`TimePoint::now(ClockType clock)`](src/rpp/timepoint.h#L341) | Time point from a specific clock source (monotonic clocks auto-synced to epoch) |
| [`TimePoint::system_now()`](src/rpp/timepoint.h#L344) | Shorthand for `now(ClockType::Realtime)` — wall clock, subject to NTP adjustments |
| [`TimePoint::monotonic_now()`](src/rpp/timepoint.h#L347) | Shorthand for `now(ClockType::Monotonic)` — monotonic, NTP-immune, epoch-synced |
| [`TimePoint::local()`](src/rpp/timepoint.h#L350) | Current time with timezone offset |
| [`elapsed(const TimePoint& end)`](src/rpp/timepoint.h#L362) | Duration between two time points |
| [`elapsed_sec(const TimePoint& end)`](src/rpp/timepoint.h#L365) | Fractional seconds between two points |
| [`time_of_day()`](src/rpp/timepoint.h#L359) | Extract HH:MM:SS.nanos part |
| [`to_epoch_us()`](src/rpp/timepoint.h#L322) | Convert to UNIX epoch microseconds |
| [`utc_to_local()`](src/rpp/timepoint.h#L353) | Add timezone offset to this timepoint |

### Global Time Utilities

| Function | Description |
|----------|-------------|
| [`sleep_ms(millis)`](src/rpp/timepoint.h#L19) | Sleep for milliseconds |
| [`sleep_us(micros)`](src/rpp/timepoint.h#L22) | Sleep for microseconds |
| [`sleep_ns(nanos)`](src/rpp/timepoint.h#L25) | Sleep for nanoseconds |
| [`sleep_for(const Duration& d)`](src/rpp/timepoint.h#L31) | Sleep for a Duration |
| [`sleep_until(const TimePoint& tp)`](src/rpp/timepoint.h#L33) | Sleep until a TimePoint |
| [`time_now_seconds()`](src/rpp/timepoint.h#L437) | Returns current time in fractional seconds (C linkage) |

### Duration Literals

| Literal | Description |
|---------|-------------|
| `100_ms` | 100 milliseconds |
| `2_s` | 2 seconds |
| `500_us` | 500 microseconds |
| `1000_ns` | 1000 nanoseconds |

### Example: Sleep Utilities

```cpp
#include <rpp/timepoint.h>

// sleep for various durations, high precision
rpp::sleep_ms(100);   // sleep 100 milliseconds
rpp::sleep_us(500);   // sleep 500 microseconds
rpp::sleep_ns(50000); // sleep 50,000 nanoseconds
```

### Example: Duration — Creation, Arithmetic & Formatting

```cpp
#include <rpp/timepoint.h>

// factory methods for creating durations
rpp::Duration d1 = rpp::Duration::from_seconds(2.5);
rpp::Duration d2 = rpp::Duration::from_millis(300);
rpp::Duration d3 = rpp::Duration::from_micros(1500);
rpp::Duration d4 = rpp::Duration::from_nanos(1'000'000);
rpp::Duration d5 = rpp::Duration::from_hours(1);
rpp::Duration d6 = rpp::Duration::from_minutes(30);

// multi-component constructor: 1 hour, 30 minutes, 45 seconds, 0 nanos
rpp::Duration composite { 1, 30, 45, 0 };

// accessors — fractional and integer conversions
double  secs  = d1.sec();     // 2.5
double  ms    = d1.msec();    // 2500.0
int64_t s_int = d1.seconds(); // 2
int64_t ms_i  = d1.millis();  // 2500
int64_t us_i  = d1.micros();  // 2'500'000
int64_t ns_i  = d1.nanos();   // 2'500'000'000

// arithmetic
rpp::Duration sum  = d1 + d2;        // 2.8s
rpp::Duration diff = d1 - d2;        // 2.2s
rpp::Duration half = d1 / 2;         // 1.25s
rpp::Duration doubled = d1 * 2;      // 5.0s
rpp::Duration scaled  = d1 * 0.1;    // 0.25s
rpp::Duration neg     = -d1;         // -2.5s
rpp::Duration abs_val = neg.abs();   // 2.5s

// clamping
rpp::Duration clamped = d1.clamped(rpp::Duration::from_seconds(0),
                                   rpp::Duration::from_seconds(1)); // clamped to [0; 1s]

// human-readable formatting
std::string clock = composite.to_string();           // "01:30:45.000"
std::string watch = composite.to_stopwatch_string();  // "[1h 30m 45s 0ms]"

// duration literals
using namespace rpp::duration_literals;
rpp::Duration timeout = 100_ms;
rpp::Duration interval = 2_s;
```

### Example: TimePoint — Measurement & Elapsed

```cpp
#include <rpp/timepoint.h>

// capture the current high-accuracy time point
rpp::TimePoint start = rpp::TimePoint::now();
doWork();
rpp::TimePoint finish = rpp::TimePoint::now();

// compute elapsed duration between two time points
rpp::Duration elapsed = start.elapsed(finish);
LogInfo("elapsed: %.3f ms", elapsed.msec());

// or use integer accessors directly
int64_t ms = start.elapsed_ms(finish);
int64_t us = start.elapsed_us(finish);
int64_t ns = start.elapsed_ns(finish);

// TimePoint arithmetic with Duration offsets
rpp::TimePoint deadline = start + rpp::Duration::from_seconds(5);
bool expired = rpp::TimePoint::now() > deadline;

// duration between two TimePoints via subtraction
rpp::Duration delta = finish - start;

// get current local time and extract time-of-day
rpp::TimePoint local = rpp::TimePoint::local();
rpp::Duration tod = local.time_of_day();
LogInfo("time of day: %s", tod.to_string());
```

---

## rpp/atomic_timepoint.h

Lock-free atomic wrappers for `Duration` and `TimePoint`. Both types are 8 bytes (a single `int64`) and inherit from `std::atomic<Duration>` / `std::atomic<TimePoint>`, which are always lock-free on 64-bit platforms. Provides atomic `+=` / `-=` operators via CAS loops that reuse the underlying Duration/TimePoint arithmetic.

### Types

| Type | Description |
|------|-------------|
| [`AtomicDuration`](src/rpp/atomic_timepoint.h#L23) | Lock-free atomic Duration with atomic arithmetic |
| [`AtomicTimePoint`](src/rpp/atomic_timepoint.h#L88) | Lock-free atomic TimePoint with atomic arithmetic |
| [`AtomicTimeSource`](src/rpp/atomic_timepoint.h#L134) | Lock-free time source with sync and warp offsets |

### AtomicDuration Methods

Inherits `load()`, `store()`, `exchange()`, `compare_exchange_weak()`, `compare_exchange_strong()` from `std::atomic<Duration>`.

| Method | Description |
|--------|-------------|
| [`operator+=(Duration d)`](src/rpp/atomic_timepoint.h#L40) | Atomically add a Duration, returns new value |
| [`operator-=(Duration d)`](src/rpp/atomic_timepoint.h#L51) | Atomically subtract a Duration, returns new value |
| [`fetch_add(Duration d, memory_order)`](src/rpp/atomic_timepoint.h#L62) | Atomically add, returns old value |
| [`fetch_sub(Duration d, memory_order)`](src/rpp/atomic_timepoint.h#L71) | Atomically subtract, returns old value |

### AtomicTimePoint Methods

Inherits `load()`, `store()`, `exchange()`, `compare_exchange_weak()`, `compare_exchange_strong()` from `std::atomic<TimePoint>`.

| Method | Description |
|--------|-------------|
| [`operator+=(Duration d)`](src/rpp/atomic_timepoint.h#L105) | Atomically add a Duration, returns new TimePoint |
| [`operator-=(Duration d)`](src/rpp/atomic_timepoint.h#L116) | Atomically subtract a Duration, returns new TimePoint |

### AtomicTimeSource

Lock-free time source for simulation time warping and time synchronization with external sources. Maintains a `combined_offset` (sync + warp) applied to system time. All mutations are atomic via `fetch_add`/`exchange`; reads use `memory_order_relaxed` for minimal overhead on the hot path (`time_now()`).

| Method | Description |
|--------|-------------|
| [`time_now()`](src/rpp/atomic_timepoint.h#L151) | Returns system time plus combined offset (sync + warp) |
| [`total_offset()`](src/rpp/atomic_timepoint.h#L173) | Returns the combined sync + warp offset |
| [`warp_offset()`](src/rpp/atomic_timepoint.h#L183) | Returns the current warp offset (diagnostic) |
| [`sync_offset()`](src/rpp/atomic_timepoint.h#L193) | Returns the current sync offset (diagnostic) |
| [`warp_forward(Duration delta)`](src/rpp/atomic_timepoint.h#L201) | Atomically advances time by delta |
| [`warp_backward(Duration delta)`](src/rpp/atomic_timepoint.h#L210) | Atomically rewinds time by delta |
| [`set_sync_offset(Duration new_offset)`](src/rpp/atomic_timepoint.h#L219) | Sets sync offset, adjusting combined offset by the difference |

### Example: AtomicTimeSource for Simulation Time

```cpp
#include <rpp/atomic_timepoint.h>

rpp::AtomicTimeSource timeSource;

// sync with an external NTP-like source
timeSource.set_sync_offset(rpp::Duration::from_millis(42));

// warp forward for simulation fast-forward
timeSource.warp_forward(rpp::Duration::from_seconds(60));

// get virtual time (system time + sync + warp offsets)
rpp::TimePoint virtualNow = timeSource.time_now();

// inspect offsets
rpp::Duration total = timeSource.total_offset();   // 60.042s
rpp::Duration sync  = timeSource.sync_offset();    // 42ms
rpp::Duration warp  = timeSource.warp_offset();    // 60s
```

### Example: Atomic Duration Accumulation

```cpp
#include <rpp/atomic_timepoint.h>
#include <thread>
#include <vector>

// thread-safe duration accumulator
rpp::AtomicDuration totalElapsed;

void worker()
{
    rpp::TimePoint start = rpp::TimePoint::now();
    doWork();
    rpp::Duration elapsed = start.elapsed(rpp::TimePoint::now());
    totalElapsed += elapsed; // atomic CAS loop
}

std::vector<std::thread> threads;
for (int i = 0; i < 8; ++i)
    threads.emplace_back(worker);
for (auto& t : threads)
    t.join();

LogInfo("total work: %.3f ms", totalElapsed.load().msec());
```

### Example: Atomic TimePoint Deadline

```cpp
#include <rpp/atomic_timepoint.h>

// shared deadline that can be updated from any thread
rpp::AtomicTimePoint deadline = rpp::TimePoint::now() + rpp::Duration::from_seconds(5);

// extend the deadline atomically
deadline += rpp::Duration::from_seconds(2);

// check expiry from another thread
if (rpp::TimePoint::now() > deadline.load())
    LogInfo("deadline expired!");
```

---

## rpp/atomic_shared_ptr.h

Portable atomic `shared_ptr` and `weak_ptr` wrappers. Uses native `std::atomic<shared_ptr<T>>` / `std::atomic<weak_ptr<T>>` when available (GCC 12+, MSVC 19.28+), otherwise falls back to a mutex-based implementation for platforms without `__cpp_lib_atomic_shared_ptr` (notably libc++/Clang).

### Types

| Type | Description |
|------|-------------|
| [`atomic_shared_ptr<T>`](src/rpp/atomic_shared_ptr.h#L50) | Thread-safe atomic shared_ptr with load/store/exchange |
| [`atomic_weak_ptr<T>`](src/rpp/atomic_shared_ptr.h#L110) | Thread-safe atomic weak_ptr with load/store/exchange |

### atomic_shared_ptr Methods

| Method | Description |
|--------|-------------|
| [`load(memory_order)`](src/rpp/atomic_shared_ptr.h#L78) | Atomically load a copy of the shared_ptr |
| [`store(shared_ptr, memory_order)`](src/rpp/atomic_shared_ptr.h#L86) | Atomically replace the stored shared_ptr |
| [`exchange(shared_ptr, memory_order)`](src/rpp/atomic_shared_ptr.h#L94) | Atomically replace and return the old shared_ptr |

### atomic_weak_ptr Methods

| Method | Description |
|--------|-------------|
| [`load(memory_order)`](src/rpp/atomic_shared_ptr.h#L138) | Atomically load a copy of the weak_ptr |
| [`store(weak_ptr, memory_order)`](src/rpp/atomic_shared_ptr.h#L146) | Atomically replace the stored weak_ptr |
| [`exchange(weak_ptr, memory_order)`](src/rpp/atomic_shared_ptr.h#L154) | Atomically replace and return the old weak_ptr |

### Example

```cpp
#include <rpp/atomic_shared_ptr.h>

// atomic_shared_ptr: thread-safe shared ownership
rpp::atomic_shared_ptr<Config> config{std::make_shared<Config>()};
auto current = config.load();          // atomic copy
config.store(std::make_shared<Config>()); // atomic replace

// atomic_weak_ptr: thread-safe weak observation
rpp::atomic_weak_ptr<Session> session;
session.store(activeSession);          // store weak_ptr
if (auto locked = session.load().lock()) {
    locked->process();                 // use if still alive
}
```

---

## rpp/timer.h

High-precision timers and performance profiling utilities. Includes `rpp/timepoint.h` for backwards compatibility, so existing code using `#include <rpp/timer.h>` for `Duration` and `TimePoint` continues to work unchanged.

### Types

| Type | Description |
|------|-------------|
| [`Timer`](src/rpp/timer.h#L18) | High-accuracy timer for profiling or deltaTime |
| [`StopWatch`](src/rpp/timer.h#L105) | Start/stop/resume event timer |
| [`ScopedPerfTimer`](src/rpp/timer.h#L171) | Auto-logs elapsed time from ctor to dtor |

### Timer Methods

| Method | Description |
|--------|-------------|
| [`Timer()`](src/rpp/timer.h#L34) | Construct and auto-start with default (Realtime) clock |
| [`Timer(ClockType clock)`](src/rpp/timer.h#L37) | Construct and auto-start with a specific clock type |
| [`Timer(ClockType clock, StartMode mode)`](src/rpp/timer.h#L43) | Construct with specific clock and start mode |
| [`start()`](src/rpp/timer.h#L52) | Start / restart the timer |
| [`time_now()`](src/rpp/timer.h#L49) | Get current time using this timer's clock type |
| [`elapsed()`](src/rpp/timer.h#L58) | Fractional seconds since start |
| [`elapsed_millis()`](src/rpp/timer.h#L60) | Fractional milliseconds since start |
| [`next()`](src/rpp/timer.h#L65) | Get elapsed time and restart |
| [`measure(Func&& func)`](src/rpp/timer.h#L84) | Measure a block's execution time (seconds) |
| [`measure_millis(Func&& func)`](src/rpp/timer.h#L92) | Measure a block's execution time (ms) |

### Example: Timer — Profiling & Delta Time

```cpp
#include <rpp/timer.h>

// Timer auto-starts on construction
rpp::Timer t;
doWork();
double secs = t.elapsed();        // fractional seconds
double ms   = t.elapsed_millis();  // fractional milliseconds

// next() returns elapsed time and restarts the timer
rpp::Timer frame_timer;
for (int i = 0; i < 3; ++i) {
    doFrame();
    double dt = frame_timer.next();  // delta time since last next() call
    LogInfo("frame %d: %.4f s", i, dt);
}

// static measure() — wraps a callable and returns its execution time
double work_time = rpp::Timer::measure([] {
    doExpensiveWork();
});
LogInfo("expensive work: %.3f s", work_time);

double work_ms = rpp::Timer::measure_millis([] {
    doExpensiveWork();
});
LogInfo("expensive work: %.1f ms", work_ms);

// NoStart mode — create without starting, then start manually
rpp::Timer deferred { rpp::Timer::NoStart };
// ... setup ...
deferred.start();
doWork();
LogInfo("deferred: %.3f s", deferred.elapsed());
```

### Example: StopWatch — Start, Stop & Resume

```cpp
#include <rpp/timer.h>

// create and start a stopwatch
rpp::StopWatch sw = rpp::StopWatch::start_new();
doPhaseOne();
sw.stop();                       // freeze the elapsed time
double phase1 = sw.elapsed();    // stopped time is preserved
LogInfo("phase 1: %.3f s", phase1);

sw.resume();                     // continue timing
doPhaseTwo();
sw.stop();
double total = sw.elapsed();     // cumulative time for both phases
LogInfo("total: %.3f s", total);

// restart — clears and starts fresh
sw.restart();
doWork();
sw.stop();
LogInfo("restarted: %.3f ms", sw.elapsed_millis());
```

### Example: ScopedPerfTimer — RAII Performance Logging

```cpp
#include <rpp/timer.h>

void processFrame()
{
    // logs elapsed time automatically when the scope exits
    rpp::ScopedPerfTimer perf;
    doFrameWork();
    // output: "[perf] processFrame elapsed 4.23ms"
}

void loadAsset(const char* name)
{
    // custom prefix and detail string
    rpp::ScopedPerfTimer perf { "[asset]", __FUNCTION__, name };
    doLoad(name);
    // output: "[asset] loadAsset 'texture.png' elapsed 12.5ms"
}

void renderScene()
{
    // only log if elapsed time exceeds the 500 µs threshold
    rpp::ScopedPerfTimer perf { "[render]", __FUNCTION__, 500 };
    drawScene();
    // output only if > 500 µs elapsed
}
```

---

## rpp/vec.h

2D/3D/4D vector math, matrices, rectangles, and geometry primitives with SSE2 intrinsics and GLSL-style swizzling.

### Types

| Type | Description |
|------|-------------|
| [`Vector2`](src/rpp/vec.h#L20) | 2D float vector |
| [`Vector2d`](src/rpp/vec.h#L146) | 2D double vector |
| [`Point`](src/rpp/vec.h#L274) | 2D integer point |
| [`RectF`](src/rpp/vec.h#L329) | Float rectangle (x, y, w, h) |
| [`Rect``](src/rpp/vec.h#L329) | Alias of Float rectangle |
| [`Recti`](src/rpp/vec.h#L421) | Integer rectangle |
| [`Vector3`](src/rpp/vec.h#L523) | 3D float vector |
| [`Vector3d`](src/rpp/vec.h#L764) | 3D double vector |
| [`Vector4`](src/rpp/vec.h#L904) | 4D float vector / quaternion |
| [`Matrix4`](src/rpp/vec.h#L1235) | 4x4 transformation matrix |
| [`point2(int xy)`](src/rpp/vec.h#L314) | Creates a Point with both x and y set to same value |
| [`AngleAxis`](src/rpp/vec.h#L886) | Rotation axis and angle (degrees) pair |
| [`Matrix3`](src/rpp/vec.h#L1114) | 3x3 row-major rotation matrix |
| [`PerspectiveViewport`](src/rpp/vec.h#L1451) | Viewport utility for perspective projection and 2D/3D mapping |
| [`BoundingBox`](src/rpp/vec.h#L1536) | 3D axis-aligned bounding box with min/max corners |
| [`BoundingSphere`](src/rpp/vec.h#L1633) | Bounding sphere defined by center point and radius |
| [`Ray`](src/rpp/vec.h#L1672) | 3D ray with origin and direction for intersection tests |

### Vector2/Vector3 Common Methods

| Method | Description |
|--------|-------------|
| [`length()`](src/rpp/vec.h#L564) | Vector magnitude |
| [`sqlength()`](src/rpp/vec.h#L567) | Squared magnitude |
| [`normalize()`](src/rpp/vec.h#L576) | Normalize in-place |
| [`normalized()`](src/rpp/vec.h#L580) | Return normalized copy |
| [`dot(const Vector3& b)`](src/rpp/vec.h#L587) | Dot product |
| [`cross(const Vector3& b)`](src/rpp/vec.h#L584) | Cross product (Vector3) |
| [`distanceTo(const Vector3& v)`](src/rpp/vec.h#L570) | Distance to another vector |
| [`almostZero()`](src/rpp/vec.h#L629) | Near-zero check |
| [`almostEqual(const Vector3& b)`](src/rpp/vec.h#L632) | Approximate equality |
| [`toString()`](src/rpp/vec.h#L615) | String representation |

### Vector3 Extras

| Method | Description |
|--------|-------------|
| [`toEulerAngles()`](src/rpp/vec.h#L601) | Convert to Euler angles |
| [`parseColor(strview str)`](src/rpp/vec.h#L672) | Parse color from string |
| [`convertGL2CV()`](src/rpp/vec.h#L687) | OpenGL to OpenCV coordinate conversion |
| [`convertMax2GL()`](src/rpp/vec.h#L699) | 3ds Max to OpenGL conversion |

### Rect Methods

| Method | Description |
|--------|-------------|
| [`area()`](src/rpp/vec.h#L354) | Rectangle area |
| [`center()`](src/rpp/vec.h#L362) | Center point |
| [`hitTest(float px, float py)`](src/rpp/vec.h#L373) | Point containment test |
| [`intersectsWith(const Rect& r)`](src/rpp/vec.h#L378) | Intersection test |
| [`joined(const Rect& r)`](src/rpp/vec.h#L393) | Union of two rectangles |
| [`clip(const Rect& bounds)`](src/rpp/vec.h#L400) | Clip to bounds |

### Example: Vector2 — Basics, Dot Product & Directions

```cpp
#include <rpp/vec.h>

// construction and named constants
rpp::Vector2 a = { 1.5f, 2.5f };
rpp::Vector2 b = { 3.0f, -1.0f };
rpp::Vector2 zero = rpp::Vector2::Zero();
rpp::Vector2 one  = rpp::Vector2::One();

// arithmetic
rpp::Vector2 sum  = a + b;          // { 4.5, 1.5 }
rpp::Vector2 diff = a - b;          // { -1.5, 3.5 }
rpp::Vector2 scaled = a * 2.0f;     // { 3.0, 5.0 }

// length, normalize, dot
float len = a.length();              // ~2.915
rpp::Vector2 dir = (b - a).normalized();
float d = a.dot(b);                  // 1.5*3.0 + 2.5*(-1.0) = 2.0

// perpendicular directions (OpenGL coordsys)
rpp::Vector2 rightDir = a.right(b);  // perpendicular right of A→B
rpp::Vector2 leftDir  = a.left(b);   // perpendicular left of A→B

// approximate comparison
bool near = a.almostEqual(a + rpp::Vector2{0.00005f, 0.00005f}); // true

// lerp and clamp
rpp::Vector2 mid = rpp::lerp(0.5f, a, b);  // midpoint
rpp::Vector2 clamped = rpp::clamp(a, rpp::Vector2::Zero(), rpp::Vector2::One());

LogInfo("dir: %s", dir.toString()); // "dir: {0.83205;-0.5547}"
```

### Example: Vector2d — Double Precision

```cpp
#include <rpp/vec.h>

rpp::Vector2d a = { 1.0, 2.0 };
rpp::Vector2d b = { 4.0, 6.0 };
double dist = (b - a).length();             // 5.0
rpp::Vector2d dir = (b - a).normalized();   // { 0.6, 0.8 }
double d = a.dot(b);                         // 16.0
LogInfo("dir: %s  dist: %f", dir.toString(), dist); // "dir: {0.6;0.8}  dist: 5.000000"
```

### Example: Point — Integer 2D Coordinates

```cpp
#include <rpp/vec.h>

rpp::Point p = { 10, 20 };
rpp::Point q = { 30, 40 };

rpp::Point sum  = p + q;     // { 40, 60 }
rpp::Point diff = p - q;     // { -20, -20 }
rpp::Point scaled = p * 3;   // { 30, 60 }

bool isOrigin = rpp::Point::Zero().isZero(); // true
LogInfo("point: %s", p.toString()); // "point: {10, 20}"
```

### Example: Rect / RectF — Float Rectangles

```cpp
#include <rpp/vec.h>

// construction: x, y, w, h
rpp::Rect panel = { 10.0f, 20.0f, 200.0f, 100.0f };

// construction from Vector2 pos + size
rpp::Rect panel2 { rpp::Vector2{10.0f, 20.0f}, rpp::Vector2{200.0f, 100.0f} };

// basic properties
float a = panel.area();                     // 20000
rpp::Vector2 center = panel.center();       // { 110, 70 }
float l = panel.left();                     // 10
float r = panel.right();                    // 210
float t = panel.top();                      // 20
float b = panel.bottom();                   // 120

// hit testing
bool inside = panel.hitTest(50.0f, 50.0f);  // true
bool outside = panel.hitTest(0.0f, 0.0f);   // false

// intersection and union of rects
rpp::Rect other = { 100.0f, 60.0f, 200.0f, 80.0f };
bool overlaps = panel.intersectsWith(other);    // true
rpp::Rect combined = panel.joined(other);       // bounding rect of both

// clip to a viewport
rpp::Rect viewport = { 0.0f, 0.0f, 150.0f, 100.0f };
panel.clip(viewport);  // panel is now clipped to fit inside viewport

// scale the size
rpp::Rect bigger = panel * 2.0f;  // double the width and height
```

### Example: Recti — Integer Rectangles

```cpp
#include <rpp/vec.h>

rpp::Recti screen = { 0, 0, 1920, 1080 };
rpp::Point mid = screen.center();          // { 960, 540 }
int pixels = screen.area();                // 2073600
bool hit = screen.hitTest(100, 200);       // true

rpp::Recti window = { 100, 100, 800, 600 };
bool overlaps = screen.intersectsWith(window);  // true
rpp::Recti merged = screen.joined(window);      // bounding rect
```

### Example: Vector3 — 3D Math, Cross & Dot Product

```cpp
#include <rpp/vec.h>

// construction and named constants
rpp::Vector3 pos   = { 1.0f, 2.0f, 3.0f };
rpp::Vector3 up    = rpp::Vector3::Up();       // { 0, 1, 0 }
rpp::Vector3 right = rpp::Vector3::Right();    // { 1, 0, 0 }
rpp::Vector3 fwd   = rpp::Vector3::Forward();  // { 0, 0, 1 }

// arithmetic
rpp::Vector3 moved = pos + fwd * 5.0f;         // { 1, 2, 8 }
float dist = pos.distanceTo(moved);             // 5.0

// length, normalize
float len = pos.length();
rpp::Vector3 dir = pos.normalized();

// dot and cross product
float d = up.dot(right);                         // 0 (perpendicular)
rpp::Vector3 normal = right.cross(fwd);           // { 0, -1, 0 }

// from vec2 helpers
rpp::Vector3 from2d = rpp::vec3(rpp::Vector2{1.0f, 2.0f}, 0.0f);  // { 1, 2, 0 }

// lerp and clamp
rpp::Vector3 a = { 0.0f, 0.0f, 0.0f };
rpp::Vector3 b = { 10.0f, 10.0f, 10.0f };
rpp::Vector3 mid = rpp::lerp(0.5f, a, b);  // { 5, 5, 5 }

// color constants and parsing
rpp::Vector3 red = rpp::Vector3::Red();                  // { 1, 0, 0 }
rpp::Vector3 parsed = rpp::Vector3::parseColor("#FF8800"); // orange
rpp::Vector3 fromRGB = rpp::Vector3::RGB(255, 128, 0);    // { 1.0, 0.502, 0.0 }

// coordinate system conversions
rpp::Vector3 glPos = { 1.0f, 2.0f, 3.0f };
rpp::Vector3 cvPos = glPos.convertGL2CV();  // OpenGL → OpenCV: { 1, -2, 3 }
rpp::Vector3 uePos = glPos.convertGL2UE();  // OpenGL → Unreal Engine

LogInfo("pos: %s  normal: %s", pos.toString(), normal.toString()); // "pos: {1;2;3}  normal: {0;-1;0}"
```

### Example: Vector4 — RGBA Colors & Quaternion Rotations

```cpp
#include <rpp/vec.h>

// RGBA color constants
rpp::Vector4 white   = rpp::Vector4::White();    // { 1, 1, 1, 1 }
rpp::Vector4 red     = rpp::Vector4::Red();      // { 1, 0, 0, 1 }
rpp::Vector4 custom  = rpp::Vector4::RGB(86, 188, 57);  // from integer RGB

// color parsing
rpp::Vector4 hex   = rpp::Vector4::HEX("#FF880080");
rpp::Vector4 named = rpp::Vector4::NAME("orange");
rpp::Vector4 any   = rpp::Vector4::parseColor("#55AAFF");

// smooth color transition
rpp::Vector4 blended = rpp::Vector4::smoothColor(red, white, 0.5f);

// XYZW swizzle access via union
rpp::Vector4 v = { 1.0f, 2.0f, 3.0f, 4.0f };
rpp::Vector3 xyz = v.xyz;    // { 1, 2, 3 }
rpp::Vector2 xy  = v.xy;     // { 1, 2 }

// quaternion rotations
rpp::Vector4 quat = rpp::Vector4::fromAngleAxis(90.0f, rpp::Vector3::Up());
rpp::Vector3 euler = quat.quatToEulerAngles();   // back to degrees

// compose rotations by quaternion multiply
rpp::Vector4 q1 = rpp::Vector4::fromAngleAxis(45.0f, rpp::Vector3::Up());
rpp::Vector4 q2 = rpp::Vector4::fromAngleAxis(30.0f, rpp::Vector3::Right());
rpp::Vector4 combined = q1.rotate(q2);  // or: q1 * q2

// create quaternion from Euler angles
rpp::Vector4 fromEuler = rpp::Vector4::fromRotationAngles({ 10.0f, 20.0f, 30.0f });

// vec4 construction helpers
rpp::Vector4 a = rpp::vec4(1.0f, 2.0f, 3.0f, 1.0f);
rpp::Vector4 b = rpp::vec4(rpp::Vector3{1.0f, 2.0f, 3.0f}, 1.0f);

// lerp
rpp::Vector4 mid = rpp::lerp(0.5f, rpp::Vector4::Red(), rpp::Vector4::Blue());
```

### Example: Matrix4 — Transforms, Affine & 2D

```cpp
#include <rpp/vec.h>

// identity matrix
rpp::Matrix4 identity = rpp::Matrix4::Identity();

// translation
rpp::Matrix4 trans = rpp::Matrix4::createTranslation({ 5.0f, 0.0f, -3.0f });
rpp::Vector3 pos = trans.getTranslation();  // { 5, 0, -3 }

// rotation (Euler angles in degrees)
rpp::Matrix4 rot = rpp::Matrix4::createRotation({ 0.0f, 45.0f, 0.0f });
rpp::Vector3 angles = rot.getRotationAngles();

// scale
rpp::Matrix4 sc = rpp::Matrix4::createScale({ 2.0f, 2.0f, 2.0f });
rpp::Vector3 scale = sc.getScale();  // { 2, 2, 2 }

// chained transform: translate, then rotate, then scale
rpp::Matrix4 model = rpp::Matrix4::Identity();
model.translate({ 1.0f, 0.0f, 0.0f });
model.rotate(45.0f, rpp::Vector3::Up());
model.scale({ 0.5f, 0.5f, 0.5f });

// full 3D affine from pos + scale + rotation
rpp::Matrix4 affine = rpp::Matrix4::createAffine3D(
    /*pos=*/   { 10.0f, 0.0f, -5.0f },
    /*scale=*/ { 1.0f, 1.0f, 1.0f },
    /*rot=*/   { 0.0f, 90.0f, 0.0f }
);

// 2D affine for UI/sprite rendering
rpp::Matrix4 sprite = rpp::Matrix4::Identity();
sprite.setAffine2D(
    /*pos=*/       { 400.0f, 300.0f },
    /*zOrder=*/    0.5f,
    /*rotDegrees=*/45.0f,
    /*scale=*/     { 1.0f, 1.0f }
);

// transform a point through a matrix
rpp::Vector3 worldPos = model * rpp::Vector3{ 1.0f, 0.0f, 0.0f };

// matrix multiply
rpp::Matrix4 mvp = model * rot;

// inverse for unprojection
rpp::Matrix4 inv = mvp.inverse();

// transpose
rpp::Matrix4 t = model.transposed();
```

### Example: Matrix4 — View & Projection Matrices

```cpp
#include <rpp/vec.h>

// perspective projection
rpp::Matrix4 perspective = rpp::Matrix4::createPerspective(
    /*fov=*/  60.0f,
    /*width=*/1920, /*height=*/1080,
    /*zNear=*/0.1f, /*zFar=*/1000.0f
);

// orthographic projection (GUI: 0,0 at top-left)
rpp::Matrix4 ortho = rpp::Matrix4::createOrtho(1920, 1080);

// orthographic with custom bounds
rpp::Matrix4 ortho2 = rpp::Matrix4::createOrtho(
    /*left=*/-10.0f, /*right=*/10.0f, /*bottom=*/-10.0f, /*top=*/10.0f
);

// camera / view matrix with lookAt
rpp::Vector3 eye    = { 0.0f, 5.0f, -10.0f };
rpp::Vector3 target = { 0.0f, 0.0f, 0.0f };
rpp::Vector3 up     = rpp::Vector3::Up();
rpp::Matrix4 view = rpp::Matrix4::createLookAt(eye, target, up);

// model-view-projection
rpp::Matrix4 model = rpp::Matrix4::createTranslation({ 2.0f, 0.0f, 0.0f });
rpp::Matrix4 mvp = model * view * perspective;

// transform a vertex through MVP
rpp::Vector4 clipPos = mvp * rpp::Vector4{ 0.0f, 0.0f, 0.0f, 1.0f };
```

### Example: PerspectiveViewport — Screen ↔ World Projection

```cpp
#include <rpp/vec.h>

// create a viewport matching the screen dimensions
rpp::PerspectiveViewport viewport {
    /*fov=*/60.0f,
    /*width=*/1920.0f, /*height=*/1080.0f,
    /*zNear=*/0.1f, /*zFar=*/1000.0f
};

// camera
rpp::Matrix4 cameraView = rpp::Matrix4::createLookAt(
    /*eye=*/   { 0.0f, 10.0f, -20.0f },
    /*center=*/{ 0.0f, 0.0f, 0.0f },
    /*up=*/    rpp::Vector3::Up()
);

// project a 3D world position to 2D screen coordinates
rpp::Vector3 worldPos = { 5.0f, 0.0f, 0.0f };
rpp::Vector2 screenPos = viewport.ProjectToScreen(worldPos, cameraView);
LogInfo("screen: %s", screenPos.toString());

// project a 2D screen click back to 3D world (at near plane depth=0)
rpp::Vector2 click = { 960.0f, 540.0f };  // center of screen
rpp::Vector3 worldClick = viewport.ProjectToWorld(click, /*depth=*/0.0f, cameraView);
LogInfo("world: %s", worldClick.toString());
```

### Example: BoundingBox — 3D Volumes

```cpp
#include <rpp/vec.h>

// create from min/max corners
rpp::BoundingBox box { { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f } };

float w = box.width();     // 2.0
float h = box.height();    // 2.0
float d = box.depth();     // 2.0
float vol = box.volume();  // 8.0

rpp::Vector3 center = box.center();  // { 0, 0, 0 }
float radius = box.radius();         // bounding sphere radius

// point containment
bool inside = box.contains({ 0.5f, 0.5f, 0.5f });  // true

// expand to include a new point
box.join({ 3.0f, 0.0f, 0.0f });  // max.x is now 3.0

// merge with another bounding box
rpp::BoundingBox other { { 2.0f, 2.0f, 2.0f }, { 5.0f, 5.0f, 5.0f } };
box.join(other);

// create from a point cloud
std::vector<rpp::Vector3> points = {
    { 0.0f, 0.0f, 0.0f },
    { 10.0f, 5.0f, 3.0f },
    { -2.0f, 8.0f, -1.0f }
};
rpp::BoundingBox cloud = rpp::BoundingBox::create(points);

// grow all sides uniformly
cloud.grow(1.0f);  // expand by 1 unit on each side
```

---

## rpp/math.h

Basic math utilities and constants.

| Item | Description |
|------|-------------|
| [`PI` / `PIf`](https://github.com/RedFox20/ReCpp/blob/master/src/rpp/math.h#L11) | Pi as `double` / `float` |
| [`SQRT2` / `SQRT2f`](https://github.com/RedFox20/ReCpp/blob/master/src/rpp/math.h#L13) | Square root of 2 |
| [`radf(degrees)`](src/rpp/math.h#L17) | Degrees to radians |
| [`degf(radians)`](src/rpp/math.h#L27) | Radians to degrees |
| [`clamp(value, min, max)`](src/rpp/math.h#L38) | Clamp value to range |
| [`lerp(t, start, end)`](src/rpp/math.h#L50) | Linear interpolation |
| [`lerpInverse(value, start, end)`](src/rpp/math.h#L66) | Inverse linear interpolation |
| [`nearlyZero(value, epsilon)`](src/rpp/math.h#L75) | Near-zero check with epsilon |
| [`almostEqual(a, b, epsilon)`](src/rpp/math.h#L81) | Approximate equality |

### Example: Constants, Conversions & Interpolation

```cpp
#include <rpp/math.h>

// pi constants
float  pi_f = rpp::PIf;    // 3.14159...
double pi_d = rpp::PI;     // 3.14159...
float  s2   = rpp::SQRT2f; // 1.41421...

// degrees ↔ radians
float rad = rpp::radf(90.0f);   // 1.5708 (π/2)
float deg = rpp::degf(rad);     // 90.0
double rad_d = rpp::radf(180.0); // 3.14159...

// clamp a value to [min, max]
int   ci = rpp::clamp(15, 0, 10);          // 10
float cf = rpp::clamp(-0.5f, 0.0f, 1.0f);  // 0.0

// linear interpolation: lerp(position, start, end)
float mid = rpp::lerp(0.5f, 30.0f, 60.0f);    // 45.0
float quarter = rpp::lerp(0.25f, 0.0f, 100.0f); // 25.0

// inverse lerp: find where a value=45.0 sits in [start, end]
float t = rpp::lerpInverse(/*value*/45.0f, /*start*/30.0f, /*end*/60.0f);  // 0.5

// combine: remap a value from one range to another
float srcT    = rpp::lerpInverse(75.0f, 50.0f, 100.0f); // 0.5
float remapped = rpp::lerp(srcT, 0.0f, 4.0f);           // 0.5 * (4.0-0.0) -> 2.0

// floating-point comparison
bool zero = rpp::nearlyZero(0.0001f);                   // true (default eps=0.001)
bool eq   = rpp::almostEqual(1.0f, 1.0005f);            // true
bool eq2  = rpp::almostEqual(1.0f, 1.01f, 0.001f);      // false
```

---

## rpp/minmax.h

SSE-optimized min, max, abs, and sqrt with template fallbacks. These are preferred to std::min()/std::max(), because those require the notoriously bloated `algorithm` header.

| Function | Description |
|----------|-------------|
| [`min(a, b)`](src/rpp/minmax.h#L22) | Minimum (SSE for float/double) |
| [`max(a, b)`](src/rpp/minmax.h#L31) | Maximum (SSE for float/double) |
| [`min3(a, b, c)`](src/rpp/minmax.h#L69) | Three-way minimum |
| [`max3(a, b, c)`](src/rpp/minmax.h#L73) | Three-way maximum |
| [`abs(a)`](src/rpp/minmax.h#L40) | Absolute value (SSE for float/double) |
| [`sqrt(f)`](src/rpp/minmax.h#L49) | Square root (SSE for float/double) |
| [`RPP_SSE_INTRINSICS`](src/rpp/minmax.h#L7) | Enables SSE2 SIMD intrinsics for optimized min/max operations |

### Example: SSE-Optimized Math Primitives

```cpp
#include <rpp/minmax.h>

// min / max — SSE-accelerated for float and double
float  fmin = rpp::min(3.0f, 7.0f);    // 3.0
float  fmax = rpp::max(3.0f, 7.0f);    // 7.0
double dmin = rpp::min(1.5, 2.5);      // 1.5

// works with any comparable type via template fallback
int imin = rpp::min(10, 20);   // 10
int imax = rpp::max(10, 20);   // 20

// three-way min / max
float smallest = rpp::min3(5.0f, 2.0f, 8.0f);  // 2.0
float largest  = rpp::max3(5.0f, 2.0f, 8.0f);  // 8.0
int   imin3    = rpp::min3(10, 3, 7);           // 3

// abs — SSE-accelerated for float/double
float  fa = rpp::abs(-4.5f);   // 4.5
double da = rpp::abs(-9.9);    // 9.9
int    ia = rpp::abs(-42);     // 42

// sqrt — SSE-accelerated for float/double
float  fs = rpp::sqrt(16.0f);  // 4.0
double ds = rpp::sqrt(2.0);    // 1.41421...
```

---

## rpp/collections.h

Container utilities, non-owning ranges, and ergonomic algorithms.

### Range Types

| Type | Description |
|------|-------------|
| [`element_range<T>`](src/rpp/collections.h#L18) | Non-owning view over contiguous elements |
| [`index_range`](src/rpp/collections.h#L129) | Integer range with optional step |

### Range Construction

| Function | Description |
|----------|-------------|
| [`range(data, size)`](src/rpp/collections.h#L71) | Create element range from pointer + size |
| [`range(vector)`](src/rpp/collections.h#L74) | Create element range from vector |
| [`range(count)`](src/rpp/collections.h#L174) | Create integer range `[0, count)` |
| [`slice(container, start, count)`](src/rpp/collections.h#L116) | Sub-range of a container |

### Container Algorithms

| Function | Description |
|----------|-------------|
| [`pop_back(vector)`](src/rpp/collections.h#L182) | Pop last element with return value |
| [`push_unique(vector, item)`](src/rpp/collections.h#L199) | Push only if not already present |
| [`erase_item(vector, item)`](src/rpp/collections.h#L207) | Erase first matching item |
| [`erase_back_swap(vector, i)`](src/rpp/collections.h#L258) | O(1) erase by swapping with last element |
| [`contains(container, item)`](src/rpp/collections.h#L330) | Check membership |
| [`find(vector, item)`](src/rpp/collections.h#L345) | Find element (returns iterator) |
| [`find_smallest(range, selector)`](src/rpp/collections.h#L426) | Find minimum by selector |
| [`find_largest(range, selector)`](src/rpp/collections.h#L448) | Find maximum by selector |
| [`any_of(vector, predicate)`](src/rpp/collections.h#L466) | True if any element matches predicate |
| [`all_of(vector, predicate)`](src/rpp/collections.h#L475) | True if all elements match predicate |
| [`none_of(vector, predicate)`](src/rpp/collections.h#L484) | True if no elements match predicate |
| [`sum_all(vector)`](src/rpp/collections.h#L494) | Sum all elements |
| [`append(vector, other)`](src/rpp/collections.h#L335) | Append one vector to another |

### Example: element_range & index_range

```cpp
#include <rpp/collections.h>

std::vector<int> nums = { 10, 20, 30, 40, 50 };

// create a non-owning view over a vector
rpp::element_range<int> all = rpp::range(nums);
for (int v : all) LogInfo("%d", v);  // 10 20 30 40 50

// sub-range: skip first 2 elements
rpp::element_range<int> tail = rpp::slice(nums, 2);
// tail: 30, 40, 50

// sub-range with count: 3 elements starting at index 1
rpp::element_range<int> mid = rpp::slice(nums, 1, 3);
// mid: 20, 30, 40

// convert range back to vector
std::vector<int> copy = mid.to_vec();  // { 20, 30, 40 }

// integer index range [0..5)
for (int i : rpp::range(5))
    LogInfo("i=%d", i);  // 0 1 2 3 4

// index range with start, end, step: [0, 10) step 2
for (int i : rpp::range(0, 10, 2))
    LogInfo("i=%d", i);  // 0 2 4 6 8

// range from raw pointer + size
int arr[] = { 1, 2, 3 };
rpp::element_range<int> raw = rpp::range(arr, 3);
```

### Example: Push, Pop & Erase Utilities

```cpp
#include <rpp/collections.h>

std::vector<std::string> names = { "alice", "bob", "carol" };

// pop_back returns the removed element
std::string last = rpp::pop_back(names);  // "carol", names is now { "alice", "bob" }

// push_unique — only adds if not already present
rpp::push_unique(names, std::string{"dave"});  // added
rpp::push_unique(names, std::string{"alice"}); // not added (already exists)

// erase_item — remove first matching element
rpp::erase_item(names, std::string{"bob"});    // names: { "alice", "dave" }

// erase_back_swap — O(1) erase by swapping with last (doesn't preserve order)
std::vector<int> ids = { 10, 20, 30, 40 };
rpp::erase_back_swap(ids, 1);  // removes index 1: swap 40→[1], pop → { 10, 40, 30 }

// erase_if — remove ALL elements matching a predicate
std::vector<int> values = { 1, 2, 3, 4, 5, 6 };
rpp::erase_if(values, [](int v) { return v % 2 == 0; });
// values: { 1, 3, 5 }

// append one vector to another
std::vector<int> a = { 1, 2 };
std::vector<int> b = { 3, 4 };
rpp::append(a, b);  // a: { 1, 2, 3, 4 }
```

### Example: Find, Contains & Predicates

```cpp
#include <rpp/collections.h>

struct Item {
    std::string name;
    float weight;
};

std::vector<Item> items = {
    { "sword", 3.5f },
    { "shield", 5.0f },
    { "potion", 0.5f },
    { "armor", 12.0f }
};

// contains
std::vector<int> nums = { 1, 2, 3, 4, 5 };
bool has3 = rpp::contains(nums, 3);  // true

// find — returns pointer or nullptr
int* found = rpp::find(nums, 4);  // pointer to the element 4

// find_if with predicate
const Item* potion = rpp::find_if(items, [](const Item& it) {
    return it.name == "potion";
});

// find_smallest / find_largest by selector
Item* lightest = rpp::find_smallest(items, [](const Item& it) { return it.weight; });
Item* heaviest = rpp::find_largest(items, [](const Item& it) { return it.weight; });
// lightest->name == "potion", heaviest->name == "armor"

// any_of / all_of / none_of
bool anyHeavy  = rpp::any_of(items,  [](const Item& it) { return it.weight > 10.0f; }); // true
bool allLight   = rpp::all_of(items,  [](const Item& it) { return it.weight < 20.0f; }); // true
bool noneEmpty = rpp::none_of(items, [](const Item& it) { return it.name.empty(); });    // true

// index_of
int idx = rpp::index_of(nums, 3);  // 2

// sum_all
int total = rpp::sum_all(nums);  // 15
```

### Example: Sort & Transform

```cpp
#include <rpp/collections.h>

// sort a vector (uses insertion sort)
std::vector<int> vals = { 5, 3, 8, 1, 4 };
rpp::sort(vals);  // { 1, 3, 4, 5, 8 }

// sort with custom comparator (descending)
rpp::sort(vals, [](int& a, int& b) { return a > b; });
// vals: { 8, 5, 4, 3, 1 }

// transform — map elements to a new vector
std::vector<int> numbers = { 1, 2, 3, 4 };
auto doubled = rpp::transform(numbers, [](const int& n) { return n * 2; });
// doubled: { 2, 4, 6, 8 }

auto strings = rpp::transform(numbers, [](const int& n) {
    return std::to_string(n);
});
// strings: { "1", "2", "3", "4" }
```

---

## rpp/concurrent_queue.h

Thread-safe FIFO queue with notification support.

| Class | Description |
|-------|-------------|
| [`concurrent_queue<T>`](src/rpp/concurrent_queue.h#L45) | Thread-safe queue with push/pop/wait |

### Methods

| Method | Description |
|--------|-------------|
| [`push(T&& item)`](src/rpp/concurrent_queue.h#L314) | Push an item |
| [`push(T&&... items)`](src/rpp/concurrent_queue.h#L314) | Push multiple items |
| [`try_pop(T& out)`](src/rpp/concurrent_queue.h#L1019) | Non-blocking pop attempt |
| [`try_pop_all(std::vector<T>& out)`](src/rpp/concurrent_queue.h#L298) | Pop all items at once |
| [`wait_pop(Duration timeout)`](src/rpp/concurrent_queue.h#L538) | Blocking pop with timeout |
| [`wait_pop(T& outItem, Duration timeout)`](src/rpp/concurrent_queue.h#L631) | Blocking pop with predicate and timeout |
| [`clear()`](src/rpp/concurrent_queue.h#L196) | Clear the queue |
| [`empty()`](src/rpp/concurrent_queue.h#L128) | True if empty |
| [`size()`](src/rpp/concurrent_queue.h#L147) | Number of items |
| [`reserve(int n)`](src/rpp/concurrent_queue.h#L228) | Reserve capacity |
| [`notify()`](src/rpp/concurrent_queue.h#L156) / [`notify_one()`](src/rpp/concurrent_queue.h#L165) | Wake waiting consumers |
| [`await(Duration timeout)`](src/rpp/concurrent_queue.h#L1000) | C++20 coroutine `co_await` — wait for items available |
| [`await_pop(T& out, Duration timeout)`](src/rpp/concurrent_queue.h#L1031) | C++20 coroutine `co_await` — pop item (returns bool) |
| [`await_pop(Duration timeout)`](src/rpp/concurrent_queue.h#L1065) | C++20 coroutine `co_await` — pop item (returns `optional<T>`) |

### Example: Coroutine co_await

```cpp
#include <rpp/concurrent_queue.h>
#include <rpp/coroutines.h>

rpp::concurrent_queue<std::string> queue;

rpp::cfuture<void> consumeMessages() {
    // pop with bool result
    std::string item;
    bool got = co_await queue.await_pop(item, rpp::millis(100));

    // pop with optional result
    auto opt = co_await queue.await_pop(rpp::millis(100));
    if (opt) { process(*opt); }

    // check availability without popping
    bool available = co_await queue.await(rpp::millis(100));
}
```

### Example: Producer-Consumer with wait_pop

```cpp
#include <rpp/concurrent_queue.h>
#include <rpp/thread_pool.h>

rpp::concurrent_queue<std::string> queue;
std::atomic_bool done = false;

// producer thread
rpp::parallel_task([&] {
    queue.push("hello");
    queue.push("world");
    queue.notify([&] { done = true; });  // safely set flag + notify
});

// consumer — blocks until an item is available or notified
std::string item;
while (queue.wait_pop(item)) {
    LogInfo("got: %s", item);
}
// wait_pop returns false when notified with no items (done==true)
```

### Example: Non-Blocking try_pop Polling

```cpp
#include <rpp/concurrent_queue.h>

rpp::concurrent_queue<int> workQueue;
workQueue.reserve(64);  // pre-allocate

// producer pushes work items
for (int i = 0; i < 10; ++i)
    workQueue.push(i);

// consumer polls without blocking
int task;
while (workQueue.try_pop(task)) {
    LogInfo("processing task %d", task);
}

// batch pop: grab all pending items at once
workQueue.push(100);
workQueue.push(200);

std::vector<int> batch;
if (workQueue.try_pop_all(batch)) {
    LogInfo("got %zu tasks at once", batch.size());  // 2
}
```

### Example: Timed wait_pop with Timeout

```cpp
#include <rpp/concurrent_queue.h>

rpp::concurrent_queue<std::string> events;

// wait up to 100ms for an event
std::string event;
if (events.wait_pop(event, rpp::Duration::from_millis(100))) {
    LogInfo("event: %s", event);
} else {
    LogInfo("no event within 100ms");
}

// process events until a time limit
auto until = rpp::TimePoint::now() + rpp::Duration::from_seconds(5);
while (events.wait_pop_until(event, until)) {
    LogInfo("event: %s", event);
}
// loop exits when TimePoint::now() > until
```

### Example: Cancellable wait_pop with Predicate

```cpp
#include <rpp/concurrent_queue.h>

rpp::concurrent_queue<std::string> messages;
std::atomic_bool cancelled = false;

// consumer with cancellation support
rpp::parallel_task([&] {
    std::string msg;
    auto timeout = rpp::Duration::from_millis(500);
    while (messages.wait_pop(msg, timeout,
           [&] { return cancelled.load(); }))
    {
        LogInfo("msg: %s", msg);
    }
    // exits when cancelled==true or timeout with no items
});

// cancel from another thread — safely sets flag and wakes waiter
messages.notify([&] { cancelled = true; });
```

### Example: Safe Iteration & Atomic Pop

```cpp
#include <rpp/concurrent_queue.h>

rpp::concurrent_queue<int> queue;
queue.push(10);
queue.push(20);
queue.push(30);

// safe iteration — holds lock for the duration of the loop
{
    for (int val : queue.iterator()) {
        LogInfo("val: %d", val);
    }
    // lock released when iter goes out of scope
}

// atomic copy — snapshot the queue without modifying it
std::vector<int> snapshot = queue.atomic_copy();

// pop_atomic — process and remove in two steps (single-consumer only)
int item;
if (queue.pop_atomic_start(item)) {
    // item is moved out but still in queue until end()
    processItem(item);        // slow operation while queue is unlocked
    queue.pop_atomic_end();   // now remove from queue, any waiting threads are notified that the item is fully processed
}

// or use the callback version
queue.pop_atomic([](int&& item) {
    processItem(item);
});

// peek without removing
int front;
if (queue.peek(front)) {
    LogInfo("front: %d (still in queue)", front);
}
```

---

## rpp/debugging.h

Cross-platform logging with severity filtering, log handlers, timestamps, and assertions.

### Log Macros

| Macro | Description |
|-------|-------------|
| [`LogInfo(format, ...)`](src/rpp/debugging.h#L283) | Log informational message |
| [`LogWarning(format, ...)`](src/rpp/debugging.h#L289) | Log warning |
| [`LogError(format, ...)`](src/rpp/debugging.h#L295) | Log error (debug assert) |
| [`Assert(expression, format, ...)`](src/rpp/debugging.h#L309) | Conditional error log |

### Configuration Functions

| Function | Description |
|----------|-------------|
| [`SetLogHandler(callback)`](src/rpp/debugging.h#L31) | Set custom log message handler |
| [`SetLogSeverityFilter(filter)`](src/rpp/debugging.h#L43) | Set minimum log severity |
| [`LogEnableTimestamps(enable, precision, time_of_day)`](src/rpp/debugging.h#L53) | Add timestamps to log entries |
| [`LogDisableFunctionNames()`](src/rpp/debugging.h#L37) | Strip function names from logs |

### Enum

| Enum | Values |
|------|--------|
| [`LogSeverity`](src/rpp/debugging.h#L25) | `LogSeverityInfo`, `LogSeverityWarn`, `LogSeverityError` |

### Example: Logging with Severity & Configuration

```cpp
#include <rpp/debugging.h>

// basic logging at different severity levels
LogInfo("server started on port %d", 8080);
LogWarning("disk usage at %d%%", 92);
LogError("failed to open file: %s", path.c_str());

// conditional assert — logs error if expression is false
Assert(connection != nullptr, "connection was null for user %s", username.c_str());

// configure severity filter — suppress info messages
SetLogSeverityFilter(LogSeverityWarn); // only warnings and errors

// enable timestamps: hh:mm:ss.MMMms
LogEnableTimestamps(true, 3, /*time_of_day=*/true);

// strip function names from log output
LogDisableFunctionNames();

// works with std::string, rpp::strview, QString directly — no .c_str() needed
std::string name = "sensor-01";
LogInfo("device: %s", name);
```

### Example: Custom Log Handler

```cpp
#include <rpp/debugging.h>

// C-style callback — receives all log messages
void myLogHandler(LogSeverity severity, const char* message, int len)
{
    if (severity >= LogSeverityError)
        writeToFile("errors.log", message, len);
}
SetLogHandler(myLogHandler);

// C++ style — add additional handler with context pointer
struct Logger {
    FILE* file;
    static void handler(void* ctx, LogSeverity sev, const char* msg, int len) {
        auto* self = static_cast<Logger*>(ctx);
        fwrite(msg, 1, len, self->file);
    }
};
Logger logger { fopen("app.log", "w") };
rpp::add_log_handler(&logger, Logger::handler);

// later: remove when done
rpp::remove_log_handler(&logger, Logger::handler);
```

### Example: Exceptions with ThrowErr & AssertEx

```cpp
#include <rpp/debugging.h>

// throw a formatted runtime_error
ThrowErr("invalid config key '%s' on line %d", key, lineNum);

// throw only if assertion fails
AssertEx(value > 0, "value must be positive, got %d", value);

// log and re-throw an exception
try {
    riskyOperation();
} catch (const std::exception& e) {
    LogExcept(e, "riskyOperation failed");
}
```

---

## rpp/stack_trace.h

Cross-platform stack tracing and traced exceptions.

| Item | Description |
|------|-------------|
| [`CallstackEntry`](src/rpp/stack_trace.h#L18) | Single callstack frame with addr, line, name, file, module |
| [`traced_exception`](src/rpp/stack_trace.h#L160) | `runtime_error` with embedded stack trace |
| [`ThreadCallstack`](src/rpp/stack_trace.h#L73) | Callstack + thread_id pair |

### Functions

| Function | Description |
|----------|-------------|
| [`stack_trace(maxDepth)`](src/rpp/stack_trace.h#L128) | Get formatted stack trace string |
| [`stack_trace(message, maxDepth)`](src/rpp/stack_trace.h#L128) | Stack trace with error message |
| [`print_trace(maxDepth)`](src/rpp/stack_trace.h#L143) | Print stack trace to stderr |
| [`print_trace(message, maxDepth)`](src/rpp/stack_trace.h#L147) | Print stack trace to stderr with message |
| [`error_with_trace(message)`](src/rpp/stack_trace.h#L153) | Create `runtime_error` with stack trace |
| [`get_address_info(addr)`](src/rpp/stack_trace.h#L46) | Look up address info → `CallstackEntry` |
| [`get_callstack(maxDepth)`](src/rpp/stack_trace.h#L60) | Walk the stack, return addresses |
| [`get_all_callstacks(maxDepth)`](src/rpp/stack_trace.h#L85) | Get callstacks from all threads |
| [`register_segfault_tracer()`](src/rpp/stack_trace.h#L171) | Install SIGSEGV handler that throws `traced_exception` |
| [`format_trace(message, callstack, depth)`](src/rpp/stack_trace.h#L94) | Formats a pre-walked callstack with optional error message prefix |

### Example: Stack Traces & Traced Exceptions

```cpp
#include <rpp/stack_trace.h>

// get a formatted stack trace from the current location
std::string trace = rpp::stack_trace();
LogInfo("trace:\n%s", trace);

// stack trace with an error message prepended
std::string errTrace = rpp::stack_trace("something went wrong", /*maxDepth=*/16);

// print stack trace directly to stderr
rpp::print_trace();
rpp::print_trace("crash context");

// create a runtime_error with embedded stack trace
throw rpp::error_with_trace("fatal: corrupted state");

// traced_exception — exception with automatic stack trace in what()
try {
    throw rpp::traced_exception{"null pointer dereference"};
} catch (const std::exception& e) {
    LogError("%s", e.what());  // includes full stack trace
}

// install SIGSEGV handler — crashes become traced_exceptions, this is quite dangerous, only for throw-away utilities, not full production apps
rpp::register_segfault_tracer();

// or: print trace and terminate instead of throwing
rpp::register_segfault_tracer(std::nothrow);

// low-level: walk the stack, then inspect individual frames
auto callstack = rpp::get_callstack(/*maxDepth=*/32);
for (uint64_t addr : callstack) {
    rpp::CallstackEntry entry = rpp::get_address_info(addr);
    LogInfo("%s", entry.to_string());
}
```

---

## rpp/bitutils.h

Fixed-length bit array.

| Class | Description |
|-------|-------------|
| [`bit_array`](src/rpp/bitutils.h#L13) | Dynamic bit array with set/unset/test operations |

### Methods

| Method | Description |
|--------|-------------|
| [`set(int bit)`](src/rpp/bitutils.h#L63) | Set a bit |
| [`set(int bit, bool value)`](src/rpp/bitutils.h#L70) | Set a bit to a value |
| [`unset(int bit)`](src/rpp/bitutils.h#L76) | Clear a bit |
| [`isSet(int bit)`](src/rpp/bitutils.h#L81) | Test a bit |
| [`checkAndSet(int bit)`](src/rpp/bitutils.h#L87) | Test and set atomically |
| [`reset(int numBits)`](src/rpp/bitutils.h#L57) | Reset with new size |
| [`sizeBytes()`](src/rpp/bitutils.h#L47) / [`sizeBits()`](src/rpp/bitutils.h#L49) | Size queries |
| [`getBuffer()`](src/rpp/bitutils.h#L51) / [`getByte(int i)`](src/rpp/bitutils.h#L92) | Raw access |
| [`copy()`](src/rpp/bitutils.h#L101) / [`copyNegated()`](src/rpp/bitutils.h#L110) | Copy operations |

### Example: Bit Array Operations

```cpp
#include <rpp/bitutils.h>

// create a fixed-size bit array with 128 bits (all cleared)
rpp::bit_array bits { 128 };

// set and test individual bits
bits.set(0);       // set bit 0
bits.set(42);      // set bit 42
bits.set(7, true); // set bit 7 to true
bits.unset(42);    // clear bit 42

bool b0 = bits.isSet(0);   // true
bool b42 = bits.isSet(42); // false

// checkAndSet — returns true if the bit was newly set
bool wasNew = bits.checkAndSet(10); // true (bit 10 was 0, now 1)
bool again  = bits.checkAndSet(10); // false (already set)

// size queries
uint32_t totalBits  = bits.sizeBits();   // 128
uint32_t totalBytes = bits.sizeBytes();  // 16

// raw byte access
uint8_t byte0 = bits.getByte(0); // first 8 bits as a byte

// copy bytes out
uint8_t buf[4];
uint32_t copied = bits.copy(0, buf, 4); // copy first 4 bytes

// copy negated (inverted) bytes
uint32_t negCopied = bits.copyNegated(0, buf, 4);

// reset to a new size (clears all bits)
bits.reset(256);
```

---

## rpp/close_sync.h

Read-write synchronization helper for safe async object destruction.

| Class | Description |
|-------|-------------|
| [`close_sync`](src/rpp/close_sync.h#L84) | Destruction synchronization with shared/exclusive locking |

### Methods

| Method | Description |
|--------|-------------|
| [`lock_for_close()`](src/rpp/close_sync.h#L129) | Acquire exclusive lock for destruction |
| [`try_readonly_lock()`](src/rpp/close_sync.h#L144) | Try to acquire shared read lock |
| [`acquire_exclusive_lock()`](src/rpp/close_sync.h#L151) | Acquire exclusive write lock |
| [`is_alive()`](src/rpp/close_sync.h#L113) | True if object is not being destroyed |
| [`is_closing()`](src/rpp/close_sync.h#L116) | True if destruction is in progress |

### Macros

| Macro | Description |
|-------|-------------|
| [`try_lock_or_return(closeSync)`](src/rpp/close_sync.h#L182) | Early return if object is being destroyed |

### Example: Safe Async Destruction with close_sync

```cpp
#include <rpp/close_sync.h>
#include <rpp/thread_pool.h>

class StreamProcessor
{
    rpp::close_sync CloseSync; // must be first for explicit lock_for_close
    std::vector<char> Buffer;

public:
    ~StreamProcessor()
    {
        // blocks until all async operations holding shared locks finish
        CloseSync.lock_for_close();
    }

    void processAsync()
    {
        rpp::parallel_task([this] {
            // acquire shared lock — returns early if destructor is running
            try_lock_or_return(CloseSync);

            // safe to use members — destructor is blocked
            Buffer.resize(64 * 1024);
            doWork(Buffer);
            // shared lock released at scope exit
        });
    }

    void exclusiveReset()
    {
        // exclusive lock — blocks until all shared locks are released
        auto lock = CloseSync.acquire_exclusive_lock();
        Buffer.clear();
    }

    bool isAlive() const { return CloseSync.is_alive(); }
};
```

---

## rpp/endian.h

Endian byte-swap read/write utilities for big-endian and little-endian data.

### Big-Endian

| Function | Description |
|----------|-------------|
| [`writeBEU16(out, value)`](src/rpp/endian.h#L58) | Write 16-bit big-endian |
| [`writeBEU32(out, value)`](src/rpp/endian.h#L64) | Write 32-bit big-endian |
| [`writeBEU64(out, value)`](src/rpp/endian.h#L70) | Write 64-bit big-endian |
| [`readBEU16(in)`](src/rpp/endian.h#L76) | Read 16-bit big-endian |
| [`readBEU32(in)`](src/rpp/endian.h#L83) | Read 32-bit big-endian |
| [`readBEU64(in)`](src/rpp/endian.h#L90) | Read 64-bit big-endian |

### Little-Endian

| Function | Description |
|----------|-------------|
| [`writeLEU16(out, value)`](src/rpp/endian.h#L101) | Write 16-bit little-endian |
| [`writeLEU32(out, value)`](src/rpp/endian.h#L107) | Write 32-bit little-endian |
| [`writeLEU64(out, value)`](src/rpp/endian.h#L113) | Write 64-bit little-endian |
| [`readLEU16(in)`](src/rpp/endian.h#L119) | Read 16-bit little-endian |
| [`readLEU32(in)`](src/rpp/endian.h#L126) | Read 32-bit little-endian |
| [`readLEU64(in)`](src/rpp/endian.h#L133) | Read 64-bit little-endian |
| [`RPP_BYTESWAP16(x)`](src/rpp/endian.h#L18) | Platform-specific 16-bit byte swap |
| [`RPP_BYTESWAP32(x)`](src/rpp/endian.h#L19) | Platform-specific 32-bit byte swap |
| [`RPP_BYTESWAP64(x)`](src/rpp/endian.h#L20) | Platform-specific 64-bit byte swap |
| [`RPP_TO_BIG16(x)`](src/rpp/endian.h#L30) | Convert 16-bit value to big-endian byte order |
| [`RPP_TO_BIG32(x)`](src/rpp/endian.h#L31) | Convert 32-bit value to big-endian byte order |
| [`RPP_TO_BIG64(x)`](src/rpp/endian.h#L32) | Convert 64-bit value to big-endian byte order |
| [`RPP_TO_LITTLE16(x)`](src/rpp/endian.h#L33) | Convert 16-bit value to little-endian byte order |
| [`RPP_TO_LITTLE32(x)`](src/rpp/endian.h#L34) | Convert 32-bit value to little-endian byte order |
| [`RPP_TO_LITTLE64(x)`](src/rpp/endian.h#L35) | Convert 64-bit value to little-endian byte order |

### Example: Big-Endian & Little-Endian Read/Write

```cpp
#include <rpp/endian.h>

uint8_t buf[8];

// write big-endian values (network byte order)
rpp::writeBEU16(buf, 0x1234);       // buf: 12 34
rpp::writeBEU32(buf, 0xDEADBEEF);   // buf: DE AD BE EF
rpp::writeBEU64(buf, 0x0102030405060708ULL);

// read big-endian values back to native format
uint16_t v16 = rpp::readBEU16(buf);  // 0x1234 on any platform
uint32_t v32 = rpp::readBEU32(buf);  // 0xDEADBEEF
uint64_t v64 = rpp::readBEU64(buf);

// write little-endian values
rpp::writeLEU16(buf, 0xABCD);       // buf: CD AB
rpp::writeLEU32(buf, 0x12345678);   // buf: 78 56 34 12
rpp::writeLEU64(buf, 0x0807060504030201ULL);

// read little-endian values back
uint16_t le16 = rpp::readLEU16(buf);
uint32_t le32 = rpp::readLEU32(buf);
uint64_t le64 = rpp::readLEU64(buf);

// typical use: parsing a network protocol header
struct PacketHeader {
    uint16_t type;
    uint32_t length;
};

PacketHeader parseHeader(const uint8_t* data) {
    return {
        .type   = rpp::readBEU16(data),
        .length = rpp::readBEU32(data + 2)
    };
}
```

---

## rpp/memory_pool.h

Linear bump-allocator memory pools for arena-style allocation (no per-object deallocation). This is ideal for quickly burning through many short-lived objects without the overhead of `new`/`delete` or `malloc`/`free`. The dynamic pool variant supports multiple blocks that grow as needed. This is ideal for tree/graph data structures that only ever grow and are destroyed all at once.

| Class | Description |
|-------|-------------|
| [`linear_static_pool`](src/rpp/memory_pool.h#L76) | Fixed-size bump allocator |
| [`linear_dynamic_pool`](src/rpp/memory_pool.h#L158) | Growing bump allocator with configurable block growth |
| [`pool_types_constructor<Pool>`](src/rpp/memory_pool.h#L16) | CRTP mixin adding typed `allocate<T>()` and `construct<T>(args...)` |

### Common Methods

| Method | Description |
|--------|-------------|
| [`capacity()`](src/rpp/memory_pool.h#L126) | Total capacity |
| [`available()`](src/rpp/memory_pool.h#L127) | Remaining capacity |
| [`allocate(int size, int align)`](src/rpp/memory_pool.h#L129) | Allocate raw memory |
| [`allocate<T>()`](src/rpp/memory_pool.h#L18) | Allocate typed memory |
| [`construct<T>(Args&&... args)`](src/rpp/memory_pool.h#L24) | Allocate and construct |
| [`allocate_range<T>(int count)`](src/rpp/memory_pool.h#L54) | Allocate array |
| [`construct_range<T>(int count, Args&&... args)`](src/rpp/memory_pool.h#L61) | Allocate and construct array |

### Example: Static Pool — Fixed-Size Arena

```cpp
#include <rpp/memory_pool.h>

// create a 4KB fixed-size pool (no per-object deallocation)
rpp::linear_static_pool pool { 4096 };

// allocate raw memory
void* raw = pool.allocate(128);  // 128 bytes, default 8-byte alignment

// allocate typed memory (zero-initialized in pool constructor)
float* floats = pool.allocate<float>();

// construct a C++ object in the pool
struct Particle { float x, y, z; float life; };
Particle* p = pool.construct<Particle>(1.0f, 2.0f, 3.0f, 1.0f);

// allocate an array
int* ids = pool.allocate_array<int>(16);

// construct a range of objects (returns element_range)
rpp::element_range<Particle> particles = pool.construct_range<Particle>(10,
    0.0f, 0.0f, 0.0f, 1.0f  // all 10 particles initialized with these args
);
for (Particle& pt : particles) {
    pt.life -= 0.1f;
}

// query remaining space
int remaining = pool.available();
int total     = pool.capacity();
```

### Example: Dynamic Pool — Growing Arena

```cpp
#include <rpp/memory_pool.h>

// starts at 128KB, doubles when full
rpp::linear_dynamic_pool pool { 128 * 1024, /*blockGrowth=*/2.0f };

// allocate objects — pool grows automatically
for (int i = 0; i < 10000; ++i) {
    auto* node = pool.construct<std::pair<int, float>>(i, float(i) * 0.5f);
}

// total capacity across all blocks
int cap = pool.capacity();
```

---

## rpp/sort.h

Minimal insertion sort for smaller binary sizes compared to `std::sort`. The insertion sort algorithm is efficient for small arrays (typically < 20 elements) and has minimal overhead, making it ideal for sorting small collections without the code size of a full quicksort implementation. The `insertion_sort` function is provided with a custom comparator for flexible sorting criteria. The code size difference in embedded environments can be extremely significant

| Function | Description |
|----------|-------------|
| [`insertion_sort(data, count, comparison)`](src/rpp/sort.h#L59) | In-place insertion sort with comparator |

### Example: Insertion Sort

```cpp
#include <rpp/sort.h>

// sort a raw array (ascending)
int data[] = { 5, 3, 8, 1, 4 };
rpp::insertion_sort(data, 5, [](int& a, int& b) { return a < b; });
// data: { 1, 3, 4, 5, 8 }

// sort descending
rpp::insertion_sort(data, 5, [](int& a, int& b) { return a > b; });
// data: { 8, 5, 4, 3, 1 }

// sort structs by a field
struct Enemy { std::string name; float distance; };
Enemy enemies[] = {
    { "goblin", 15.0f },
    { "dragon", 5.0f },
    { "troll",  30.0f }
};
rpp::insertion_sort(enemies, 3, [](Enemy& a, Enemy& b) {
    return a.distance < b.distance;
});
// enemies sorted by distance: dragon(5), goblin(15), troll(30)
```

---

## rpp/scope_guard.h

RAII scope cleanup guard.

| Item | Description |
|------|-------------|
| [`scope_finalizer<Func>`](src/rpp/scope_guard.h#L40) | Calls function on destruction |
| [`scope_guard(lambda)`](src/rpp/scope_guard.h#L73) | Macro to create anonymous scope guard |

```cpp
auto* resource = acquire();
scope_guard([&]{ release(resource); });
// resource is released when scope exits
```

---

## rpp/load_balancer.h

UDP send rate limiter for throttling network traffic.

| Class | Description |
|-------|-------------|
| [`load_balancer`](src/rpp/load_balancer.h#L12) | Rate-limiter for byte sending |

### Methods

| Method | Description |
|--------|-------------|
| [`can_send()`](src/rpp/load_balancer.h#L38) | True if send budget is available |
| [`wait_to_send(int bytes)`](src/rpp/load_balancer.h#L45) | Block until bytes can be sent |
| [`notify_sent(TimePoint now, int bytesToSend)`](src/rpp/load_balancer.h#L50) | Report bytes sent |
| [`get_max_bytes_per_sec()`](src/rpp/load_balancer.h#L26) | Get rate limit |
| [`set_max_bytes_per_sec(int n)`](src/rpp/load_balancer.h#L29) | Set rate limit |

### Example: UDP Rate Limiting

```cpp
#include <rpp/load_balancer.h>

// limit to 1 MB/s
rpp::load_balancer limiter { 1'000'000 };

void sendPackets(rpp::socket& sock, const uint8_t* data, int totalBytes)
{
    int offset = 0;
    while (offset < totalBytes) {
        int chunkSize = std::min(1400, totalBytes - offset); // typical MTU

        // blocks until the rate budget allows sending
        limiter.wait_to_send(chunkSize);
        sock.send(data + offset, chunkSize);
        offset += chunkSize;
    }
}

// manual control: check before sending, then notify after
void sendManual(rpp::socket& sock, const void* data, int len)
{
    if (limiter.can_send()) {
        sock.send(data, len);
        limiter.notify_sent(rpp::TimePoint::now(), len);
    }
}

// adjust rate limit dynamically
limiter.set_max_bytes_per_sec(500'000);  // throttle to 500 KB/s
```

---

## rpp/obfuscated_string.h

Compile-time string obfuscation to prevent strings from appearing in binaries.

| Item | Description |
|------|-------------|
| [`obfuscated_string<chars...>`](src/rpp/obfuscated_string.h#L19) | Compile-time obfuscated string (GCC/Clang) |
| [`macro_obfuscated_string<indices...>`](src/rpp/obfuscated_string.h#L66) | Cross-platform obfuscated string |
| [`make_obfuscated("str")`](src/rpp/obfuscated_string.h#L116) | Macro to create obfuscated string |
| [`operator ""_obfuscated`](src/rpp/obfuscated_string.h#L110) | String literal operator (GCC only) |

### Example: Compile-Time String Obfuscation

```cpp
#include <rpp/obfuscated_string.h>

// cross-platform macro (works on GCC, Clang, MSVC)
constexpr auto apiKey = make_obfuscated("sk-live-abc123secret");

// the string is stored obfuscated in the binary
std::string garbled  = apiKey.obfuscated();  // unreadable bytes
std::string original = apiKey.to_string();   // "sk-live-abc123secret"

// use the deobfuscated string at runtime
curl_set_header("Authorization", original.c_str());

// GCC-only: literal operator syntax
// constexpr auto secret = "super@secret.com"_obfuscated;
// std::string email = secret.to_string();
```

---

## rpp/proc_utils.h

Process memory and CPU usage information.

| Item | Description |
|------|-------------|
| [`proc_mem_info`](src/rpp/proc_utils.h#L13) | Process memory stats: `virtual_size`, `physical_mem` (with `_kb()`, `_mb()` helpers) |
| [`cpu_usage_info`](src/rpp/proc_utils.h#L38) | CPU time stats: `cpu_time_us`, `user_time_us`, `kernel_time_us` (with `_ms()`, `_sec()`) |
| [`proc_current_mem_used()`](src/rpp/proc_utils.h#L32) | Get current process memory usage |
| [`proc_total_cpu_usage()`](src/rpp/proc_utils.h#L63) | Get total CPU time used by process |

### Example: Process Memory & CPU Profiling

```cpp
#include <rpp/proc_utils.h>

// query current process memory usage
rpp::proc_mem_info mem = rpp::proc_current_mem_used();
LogInfo("physical: %.1f MB  virtual: %.1f MB",
    mem.physical_mem_mb(), mem.virtual_size_mb());

// raw bytes also available
uint64_t rssBytes = mem.physical_mem;
uint64_t vszBytes = mem.virtual_size;

// CPU usage profiling: measure over a time interval
rpp::cpu_usage_info start = rpp::proc_total_cpu_usage();
doExpensiveWork();
rpp::cpu_usage_info end = rpp::proc_total_cpu_usage();

double cpuMs    = end.cpu_time_ms()    - start.cpu_time_ms();
double userMs   = end.user_time_ms()   - start.user_time_ms();
double kernelMs = end.kernel_time_ms() - start.kernel_time_ms();

LogInfo("CPU: %.1f ms (user: %.1f ms, kernel: %.1f ms)",
    cpuMs, userMs, kernelMs);
```

---

## rpp/tests.h

Minimal unit testing framework with test discovery, assertions, and verbose output control.

| Item | Description |
|------|-------------|
| [`test`](src/rpp/tests.h#L39) | Base test class with lifecycle hooks |
| [`test_info`](src/rpp/tests.h#L40) | Test registration metadata |
| [`TestVerbosity`](src/rpp/tests.h#L67) | `None`, `Summary`, `TestLabels`, `AllMessages` |

### Key Macros

| Macro | Description |
|-------|-------------|
| [`TestImpl(ClassName)`](src/rpp/tests.h#L713) | Register a test class |
| [`TestInit(...)`](src/rpp/tests.h#L728) | Test initialization method |
| [`TestCase(name)`](src/rpp/tests.h#L745) | Define a test case |
| [`AssertThat(expr, expected)`](src/rpp/tests.h#L585) | Assert equality |
| [`AssertEqual(a, b)`](src/rpp/tests.h#L601) | Assert exact equality |
| [`AssertNotEqual(a, b)`](src/rpp/tests.h#L637) | Assert inequality |
| [`AssertTrue(expr)`](src/rpp/tests.h#L706) | Assert expression is true |
| [`AssertFalse(expr)`](src/rpp/tests.h#L576) | Assert expression is false |
| [`AssertThrows(expr)`](src/rpp/tests.h#L610) | Assert expression throws |

### Running Tests

| Method | Description |
|--------|-------------|
| [`test::run_tests(patterns)`](src/rpp/tests.h#L283) | Run tests matching patterns |
| [`test::run_tests(argc, argv)`](src/rpp/tests.h#L294) | Run tests from command line args |
| [`test::run_tests()`](src/rpp/tests.h#L283) | Run all registered tests |
| [`register_test(name, factory, autorun)`](src/rpp/tests.h#L65) | Registers a unit test with given name, factory and autorun flag |

### Example: Defining a Test Class with TestCase

```cpp
// test_math.cpp
#include <rpp/tests.h>

TestImpl(test_math)
{
    TestInit(test_math)
    {
        // called once when the test class is initialized
        // setup shared state here
    }

    TestCleanup()
    {
        // called once after all test cases finish
    }

    TestCase(addition)
    {
        AssertThat(1 + 1, 2);
        AssertThat(10 + 20, 30);
        AssertNotEqual(1 + 1, 3);
    }

    TestCase(comparisons)
    {
        AssertTrue(5 > 3);
        AssertFalse(3 > 5);
        AssertGreater(10, 5);
        AssertLess(3, 7);
        AssertGreaterOrEqual(5, 5);
        AssertLessOrEqual(5, 5);
    }

    TestCase(range_checks)
    {
        // inclusive range [0, 100]
        AssertInRange(50, 0, 100);
        // exclusive range (0, 100)
        AssertExRange(50, 0, 100);
    }

    TestCase(string_equality)
    {
        rpp::strview s = "hello";
        AssertThat(s, "hello");
        AssertThat(s.length(), 5);
        AssertNotEqual(s, "world");
    }

    TestCase(vectors)
    {
        std::vector<int> actual   = { 1, 2, 3 };
        std::vector<int> expected = { 1, 2, 3 };
        AssertEqual(actual, expected);
    }
};
```

### Example: Exception & Error Assertions

```cpp
#include <rpp/tests.h>

TestImpl(test_exceptions)
{
    TestInit(test_exceptions) {}

    TestCase(throws_expected)
    {
        // assert that an expression throws a specific exception type
        AssertThrows(throw std::runtime_error{"oops"}, std::runtime_error);
    }

    TestCase(no_throw)
    {
        // assert that no exception is thrown
        AssertNoThrowAny(int x = 1 + 2; (void)x);
    }

    TestCase(custom_message)
    {
        int value = 42;
        // assert with a custom formatted error message
        AssertMsg(value > 0, "value must be positive, got %d", value);
    }

    // entire test case expects an exception — if it doesn't throw, the test fails
    TestCaseExpectedEx(must_throw, std::runtime_error)
    {
        throw std::runtime_error{"this is expected"};
    }
};
```

### Example: Test Runner & main()

```cpp
// main.cpp
#include <rpp/tests.h>

int main(int argc, char** argv)
{
    // run tests from command line args:
    //   ./RppTests test_math          — run all cases in test_math
    //   ./RppTests test_math.addition — run only the "addition" case
    //   ./RppTests math string        — run all tests matching "math" or "string"
    //   ./RppTests                    — run ALL registered tests
    return rpp::test::run_tests(argc, argv);
}
```

```cpp
// or run programmatically with patterns
// these can be embedded into your embedded binaries to run tests before main() on actual devices
rpp::test::set_verbosity(rpp::TestVerbosity::AllMessages);
int result = rpp::test::run_tests("test_math");

// run all tests
int result = rpp::test::run_tests();

// cleanup all test state (for leak detection)
rpp::test::cleanup_all_tests();
```

### Example: Per-TestCase Setup & Cleanup

```cpp
#include <rpp/tests.h>

TestImpl(test_database)
{
    Database* db = nullptr;

    TestInit(test_database)
    {
        // called once before all test cases
        db = Database::connect("test.db");
    }

    TestCleanup()
    {
        // called once after all test cases
        delete db;
    }

    TestCaseSetup()
    {
        // called before EACH test case
        db->beginTransaction();
    }

    TestCaseCleanup()
    {
        // called after EACH test case
        db->rollback();
    }

    TestCase(insert)
    {
        db->exec("INSERT INTO users VALUES ('alice')");
        AssertThat(db->count("users"), 1);
    }

    TestCase(empty_table)
    {
        // previous insert was rolled back
        AssertThat(db->count("users"), 0);
    }
};
```

---

## rpp/log_colors.h

ANSI terminal color macros for colorized console output.

**Color wrappers:** `RED(text)`, `GREEN(text)`, `BLUE(text)`, `YELLOW(text)`, `CYAN(text)`, `MAGENTA(text)`, `WHITE(text)`, `DARK_RED(text)`, `BOLD_RED(text)`, etc.

**Color codes:** `COLOR_RED`, `COLOR_GREEN`, `COLOR_BLUE`, `COLOR_RESET`, etc.

See [`log_colors.h`](https://github.com/RedFox20/ReCpp/blob/master/src/rpp/log_colors.h) for the full list.

---

## rpp/type_traits.h

Detection idiom and type trait helpers for SFINAE.

| Trait | Description |
|-------|-------------|
| [`is_detected<Op, Args...>`](src/rpp/type_traits.h#L26) | Detection idiom |
| [`has_to_string<T>`](src/rpp/type_traits.h#L42) | True if `to_string(T)` is valid |
| [`has_to_string_memb<T>`](src/rpp/type_traits.h#L43) | True if `T::to_string()` exists |
| [`has_std_to_string<T>`](src/rpp/type_traits.h#L40) | True if `std::to_string(T)` is valid |
| [`is_iterable<T>`](src/rpp/type_traits.h#L52) | True if T supports range-for |
| [`is_container<T>`](src/rpp/type_traits.h#L57) | True if T is a container with `size()` |
| [`is_stringlike<T>`](src/rpp/type_traits.h#L55) | True if T is string-like |
| [`Operation`](src/rpp/type_traits.h#L25) | Template alias used with `is_detected` for expression validity checks |

---

## rpp/traits.h

Function type traits for extracting return types and argument types from callables.

| Trait | Description |
|-------|-------------|
| [`function_traits<T>`](src/rpp/traits.h#L16) | Extracts `ret_type` and `arg_types` from functions, lambdas, member functions |
| [`first_arg_type<T>`](src/rpp/traits.h#L50) | First argument type of a callable |

---

## rpp/jni_cpp.h

Android JNI C++ utilities (Android-only).

### Initialization

| Function | Description |
|----------|-------------|
| [`initVM(vm, env)`](src/rpp/jni_cpp.h#L18) | Initialize JVM reference |
| [`getEnv()`](src/rpp/jni_cpp.h#L38) | Get JNIEnv for current thread |
| [`getMainActivity(const char* className)`](src/rpp/jni_cpp.h#L46) | Get Android main Activity |

### Classes

| Class | Description |
|-------|-------------|
| [`JniRef`](src/rpp/jni_cpp.h#L74) | Smart JNI object reference (local/global) |
| [`JString`](src/rpp/jni_cpp.h#L230) | JNI jstring wrapper with `str()`, `getLength()` |
| [`Class`](src/rpp/jni_cpp.h#L371) | JNI class wrapper |
| [`Method`](src/rpp/jni_cpp.h#L414) | JNI method wrapper |
| [`Field`](src/rpp/jni_cpp.h#L503) | JNI field wrapper |

### Example: Initialization with JNI_OnLoad

```cpp
// Option 1: Define RPP_DEFINE_JNI_ONLOAD before including the header
// This automatically defines JNI_OnLoad for you
#define RPP_DEFINE_JNI_ONLOAD 1
#include <rpp/jni_cpp.h>

// Option 2: Manually initialize in your own JNI_OnLoad
#include <rpp/jni_cpp.h>
extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    return rpp::jni::initVM(vm);
}
```

### Example: Smart References with Ref\<T\>

```cpp
// Ref<T> automatically cleans up JNI local/global references via RAII
using namespace rpp::jni;

// wrap an untracked local ref for automatic cleanup
Ref<jobject> localObj = makeRef(env->NewObject(cls, ctor));

// convert to a global ref that can be stored across JNI calls
Ref<jobject> globalObj = makeGlobalRef(env->NewObject(cls, ctor));

// or convert an existing local ref to global
Ref<jobject> obj = makeRef(env->CallObjectMethod(...));
Ref<jobject> stored = obj.toGlobal(); // safe for static/global storage
```

### Example: JString Conversions

```cpp
using namespace rpp::jni;

// create a JNI string from C++
JString greeting = JString::from("Hello from C++!");
int len = greeting.getLength(); // 15

// convert JNI string back to C++ std::string
std::string cppStr = greeting.str(); // "Hello from C++!"

// wrap a jstring received from Java
void handleJavaString(jstring javaStr) {
    JString s{javaStr}; // takes ownership, auto-freed
    std::string text = s.str();
}
```

### Example: Looking Up Classes, Methods, and Fields

```cpp
using namespace rpp::jni;

// find a Java class (always stored as GlobalRef internally)
Class activity{"com/myapp/MainActivity"};

// look up instance and static methods using JNI signatures
Method getName = activity.method("getName", "()Ljava/lang/String;");
Method getInstance = activity.staticMethod("getInstance", "()Lcom/myapp/MainActivity;");

// look up fields
Field appVersion = activity.field("appVersion", "Ljava/lang/String;");
Field instanceCount = activity.staticField("instanceCount", "I");
```

### Example: Calling Java Methods

```cpp
using namespace rpp::jni;

Class player{"com/unity3d/player/UnityPlayer"};
Method sendMessage = player.staticMethod("UnitySendMessage",
    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");

// call static void method with JString arguments
JString obj  = JString::from("GameManager");
JString func = JString::from("OnEvent");
JString data = JString::from("{\"score\":100}");
sendMessage.voidF(nullptr, obj, func, data);

// call an instance method that returns a string
Class context{"android/content/Context"};
Method getPackageName = context.method("getPackageName", "()Ljava/lang/String;");

jobject mainActivity = getMainActivity("com/myapp/MainActivity");
JString packageName = getPackageName.stringF(mainActivity);
std::string name = packageName.str(); // "com.myapp"
```

### Example: Accessing Java Fields

```cpp
using namespace rpp::jni;

Class config{"com/myapp/AppConfig"};

// read a static int field
Field maxRetries = config.staticField("MAX_RETRIES", "I");
jint retries = maxRetries.getInt(); // static: no instance needed

// read an instance string field
Field userName = config.field("userName", "Ljava/lang/String;");
JString name = userName.getString(configInstance);
std::string user = name.str();
```

### Example: Working with JArray

```cpp
using namespace rpp::jni;

// create a Java String[] from C++ strings
std::vector<const char*> tags = {"debug", "network", "ui"};
JArray tagArray = JArray::from(tags);

// access array elements
int len = tagArray.getLength(); // 3
JString first = tagArray.getStringAt(0); // "debug"

// work with primitive arrays (e.g. byte[])
Method getData = myClass.method("getData", "()[B");
JArray bytes = getData.arrayF(JniType::Byte, instance);
{
    ElementsRef elems = bytes.getElements();
    int size = elems.getLength();
    for (int i = 0; i < size; ++i) {
        jbyte b = elems.byteAt(i);
    }
} // elements released automatically
```
---

## Development > CircleCI Local
Running CircleCI locally on WSL/Ubuntu. 
If running on WSL, you need Docker Desktop with WSL2 integration enabled.

You will need to configure a personal API token to use the CircleCI CLI.
Read more at: https://circleci.com/docs/local-cli/#configure-the-cli
```
curl -fLSs https://raw.githubusercontent.com/CircleCI-Public/circleci-cli/master/install.sh | sudo bash
circleci update && circleci setup
```
Executing the build locally, targeting the `clang-android-cpp20` Job:
```
circleci config process .circleci/config.yml > process.yml && \
circleci local execute -c process.yml clang-android-cpp20
```
