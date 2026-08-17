# ReCpp Instructions for Agents

This file is the single source of instructions for every coding agent. Codex and other
agents read it directly. Claude Code loads it through `.claude/rules/recpp.md`.

Read the additional instructions in `.github/copilot-instructions.md`.

## Scope: ReCpp is a standalone repo

ReCpp is often checked out inside a larger tree. A `CLAUDE.md` in any parent directory
does NOT apply here, and `.claude/settings.json` excludes those files. ReCpp is the
project, not a dependency of the tree that contains it.

## MANDATORY: writing style and response shape (always on)

Two style skills are project defaults. `.claude/rules/` loads them every session. Do not
wait to be asked. Do not invoke them manually. Any agent that does not read
`.claude/rules/` must read both `SKILL.md` files under `.claude/skills/` instead.

- **`ste-writing`** governs *prose*: README.md, docs, commit messages, PR text,
  doxygen blocks, code comments, and `ThrowErr()` and `Log*` strings. It does not
  govern code, identifiers, or command syntax.
- **`output-style`** governs *responses to the user*: lead with the next action,
  number multi-step work, restate progress, suppress tangents, and drop all
  preamble and closing pleasantries.

If a style rule collides with the rest of this file, **this file wins. The style
shapes what fits inside it.** These gates are not negotiable for brevity:

- the mandatory unit test
- the TSAN build
- the clang-tidy pass
- the full test suite run
- the README.md line-reference update

## Development Requirements

1. **Unit tests are mandatory** — add a test in `tests/` for every new feature and
   every bug fix.
2. **Always build with TSAN** — put `tsan` in every build command. A switch between
   a regular build and a TSAN build forces a full rebuild.
3. **Validate with clang-tidy** — run `CXX20=1 mama gcc tsan build clang-tidy test="nogdb -vv"`.
   Fix every warning before you call the task complete.
4. **Run the full test suite** — run `CXX20=1 mama gcc tsan build test="nogdb -vv"`.
   All tests must pass with no data race. A task is not complete until they do.

## Code Style

### Prefer explicit types when the type is not obvious

Do not overuse `auto` merely to save typing. Use the explicit type name when it
communicates ownership, lifetime, precision, or the ReCpp abstraction being used.

```cpp
rpp::cfuture<> future = rpp::async_task(run_work);
rpp::TimePoint deadline = rpp::TimePoint::now() + rpp::millis(100);
```

Use `auto` when the type is already unmistakable from the initializer or when
spelling it would obscure the code, such as iterators and complex template
implementation details.

### Do not wrap immediately after `=`

Keep the declared name and the start of its initializer together. Never put `=`
at the end of a line and move the entire expression to the next line.

```cpp
// good
const bool future_was_ready_during_cleanup = future.await_ready();

// bad
const bool future_was_ready_during_cleanup =
    future.await_ready();
```

When an initializer is too long, wrap inside its argument list or another natural
expression boundary while keeping `type name = expression` on the first line.

### Use ReCpp timing, scheduling, and readiness APIs

Do not introduce `std::chrono` or standard thread timing helpers into ReCpp source
or tests. ReCpp provides its own consistent alternatives:

- Use `future.await_ready()` for a non-blocking future readiness check.
- Use `rpp::yield()` instead of `std::this_thread::yield()`.
- Use `rpp::Duration`, `rpp::TimePoint`, and ReCpp duration helpers such as
  `rpp::millis()` for durations, deadlines, waits, and sleeps.

Prefer the ReCpp overload whenever both a standard-library and a ReCpp timing API
are available.

### Includes come before imports. Always.

In any file that mixes both, put every `#include` first and every `import` last.

```cpp
#include <rpp/tests.h>   // 1. rpp headers
#include <cstring>       // 2. std headers
#include <limits>

#if RPP_BUILD_WITH_MODULES
import rpp.strview;      // 3. imports, last
#endif
```

GCC 14 re-parses a standard library header that follows an import. Its internal
templates then collide with the entities the module already made reachable. One
`#include <string>` after one `import` gives about 960 compile errors. The whole
`RppTests` module build gave 1603 errors from a single misplaced import.

`<cstdio>` and other C wrappers do not trigger it, because they declare no
template. Do not use that as a reason to break the order.

The error is loud and it stops the compiler. It never reaches the linker, and it
never becomes a duplicate symbol. A program that mixes `import rpp.strview` in
one translation unit and `#include <rpp/strview.h>` in another links and runs
correctly.

### A header never contains an `import`

A header does not control where a consumer includes it, so it cannot keep the
order above. An `import` in a header also removes every transitive std include
from each consumer, which breaks files that never mentioned modules.

Only a `.cpp`, a test, or a `.cppm` carries an `import`.

### Include what you use

Every file includes the headers for the names it uses. Do not rely on a
transitive include. Run `tools/check_includes.py all` before you commit a header
change. The migration plan is in [`docs/MODULES_MIGRATION.md`](docs/MODULES_MIGRATION.md).

## ReCpp Modules

All headers are in `src/rpp/`. Test files are in `tests/`.

| Header | Purpose |
|--------|---------|
| `config.h` | Platform detection, compiler macros, base types |
| `strview.h` | Non-owning string view with tokenization and search |
| `sprint.h` | String builder, type-safe formatting, to_string |
| `file_io.h` | Cross-platform file read and write, RAII file handles |
| `paths.h` | Path manipulation, directory listing, filesystem helpers |
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
| `binary_stream.h` | Buffered binary read and write streams |
| `binary_serializer.h` | Reflection-based binary and string serialization |
| `timepoint.h` | Duration, TimePoint, time constants, sleep utilities, duration literals |
| `atomic_timepoint.h` | Lock-free AtomicDuration, AtomicTimePoint (inherits std::atomic), AtomicTimeSource (time sync and fastforward) |
| `timer.h` | Timer, StopWatch, ScopedPerfTimer (includes timepoint.h) |
| `vec.h` | 2D/3D/4D vector math, matrices, rectangles |
| `math.h` | Clamp, lerp, deg/rad, epsilon compare |
| `minmax.h` | SSE-optimized min, max, abs, sqrt |
| `collections.h` | Container utilities, ranges, algorithms, erasure helpers |
| `debugging.h` | Logging, assertions, log handlers |
| `stack_trace.h` | Stack tracing and traced exceptions |
| `bitutils.h` | Fixed-length bit array |
| `endian.h` | Endian byte-swap read and write |
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

## After Modifying Headers

After you change a header in `src/rpp/`:

1. **Update README line references**: Run `python3 update_doc_linerefs.py` from the
   repo root. It fixes the line numbers in README.md. Add `--dry-run` to preview the
   changes first.
2. **Document new public API**: Add a README.md entry for each new public function,
   type, or constant. Use the format `| [\`name(params)\`](src/rpp/header.h#L123) | Description |`.
   The display text must match the declaration closely enough for
   `update_doc_linerefs.py` to track it.
3. **Check for undocumented API**: Run `python3 update_doc_linerefs.py --check-undocumented`
   to find the public declarations that README.md does not cover.

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

Put `tsan` in every build command. This keeps the build configuration consistent.
The `nogdb` argument reduces the output noise. Omit it if you want mama to attach
GDB when it starts the tests.

```bash
# basic build and test (C++20 with TSAN)
CXX20=1 mama gcc tsan build test="nogdb -vv"

# full reconfigure + rebuild + test
CXX20=1 mama gcc tsan rebuild test="nogdb -vv"

# build project with clang instead of gcc
CXX20=1 mama clang tsan build test="nogdb -vv"

# build project and run a specific test suite
CXX20=1 mama gcc tsan build test="nogdb -vv test_concurrent_queue"

# build project and run a specific test
CXX20=1 mama gcc tsan build test="nogdb -vv test_concurrent_queue::push_and_pop"

# build project and run a specific test until failure (up to N iterations)
CXX20=1 mama gcc tsan build test="nogdb -vv test_concurrent_queue::push_and_pop" test_until_failure=20
```

### Address Sanitizer (mama)
You cannot combine ASAN and TSAN. Use ASAN only when you debug a memory error. ASAN
needs a reconfigure. Omit `nogdb` here. mama then attaches GDB to the tests, and GDB
gives a full stack trace on a fatal crash.
```bash
CXX20=1 mama gcc asan configure build test="-vv"
```

### clang-tidy (mama)
```bash
CXX20=1 mama gcc tsan build clang-tidy test="nogdb -vv"
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
