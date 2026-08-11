# ReCpp Bugs

Open issues first, then closed. Keep every entry terse: enough to start an
investigation, no more. A closed entry states the bug and the fix in one or two
lines.

## Open

### B1. TSAN fires on about 1 in 4 plain sequential runs
4 races in 13 sequential `RppTests` runs, gcc-14 TSAN, no parallelism. Two
distinct sites, both in ReCpp code, both on a test teardown boundary.

**B1a, `thread_pool.cpp:351`, 3 of the 4.**
Write of 8 bytes by a pool worker in `rpp::pool_worker::run`, on the
`generic.reset()` inside the `catch` handler. Previous read of the same address by
the **main thread** in `rpp::test::run_test_func`, `tests.cpp:723`. So the harness
advances to the next test while a worker is still unwinding the exception path of
the last one. Read this as a lifecycle ordering problem, not a missing lock.

**B1b, `semaphore.h:99`, 1 of the 4.**
heap-use-after-free in `rpp::semaphore::spin_lock`, through
`rpp::spin_lock<std::mutex>` at `mutex.h:197`, reported inside
`__gthread_mutex_trylock`. The semaphore outlives its own mutex, or a waiter
touches it after the owner is gone.

**Not the same as the earlier coroutine sighting.** A single run once reported a
race on the exception a coroutine rethrows, `test_coroutines.cpp:135`. 94
sequential ASAN runs found no use-after-free anywhere, so that one looks like a
TSAN blind spot in uninstrumented libstdc++ exception refcounting. B1a and B1b are
the real work.

### B2. Timing bounds flaked sequential ASAN at 7 percent
Five causes, all fixed on this branch. 3 of 44 runs failed before, 1 of 50 after
the first three fixes, 0 of 25 so far after the last two. Kept open until a longer
sweep confirms zero.
- `test_sockets.cpp:425`, elapsed 9.9 and 12.8 ms against a 9 ms bound.
- `test_concurrent_queue.cpp:376`, elapsed 30.4 ms against a 15 ms bound.
- `test_concurrent_queue.cpp:569`, a 15 ms `wait_pop` timed out and shifted every
  later item, so one miss cascaded into four failed assertions.
- `test_coroutines.cpp:49` and `:56`, a 50 ms sleep measured 56.5 ms against a
  56 ms bound, and a 15 ms sleep measured 28.3 ms against a 20 ms bound.
- `test_semaphore.cpp:132`, 4 notifies counted of 10 sent.
Every timing bound sat just below the delay it measured, so a sanitizer erased the
margin. Each bound now sits just under its own timeout, which is the only property
the assertion needs. The semaphore one was different: the producer set
`working = false` while the worker still had notifies to drain, so it now waits for
the count first.

### B3. mamabuild cannot export C++20 modules
`mamafile.py` `package()` exports `.h` and `.natvis` only, so no `.cppm` reaches
a consumer. `CMakeLists.txt` has no `install(TARGETS ... FILE_SET CXX_MODULES)`.
Nobody outside ReCpp can import `rpp.strview` or `rpp.debugging` yet.
A binary module interface is not portable, so a consumer must compile the
producer's `.cppm` inside its own target. mama has no way to express that.
Tracked upstream: https://github.com/RedFox20/Mama/issues/41

### B4. 52 files use a std facility they do not include
`tools/check_includes.py missing`. Each one breaks the moment its provider chain
becomes an `import`. Also 10 unused and 43 redundant rpp includes, and 59 std
include candidates, from `tools/check_includes.py unused`.
Plan: `docs/MODULES_MIGRATION.md` changeset 1.

### B5. `update_doc_linerefs.py` matches a macro name inside another macro body
It pointed `LogError` at `debugging.macros.h:151`, which is the `LogError` call
inside `DbgAssert`, not the `#define LogError` at line 128. Corrected by hand.
The script's own docstring already warns that it has mistakes.

## Closed

### C1. The modules build did not compile
`sprint.h` swapped `#include "strview.h"` for `import rpp.strview;` under
`RPP_BUILD_WITH_MODULES`. That dropped the transitive `<cstring>`, `<string>` and
`<concepts>` from every consumer, and `test_strview.cpp` lost `strlen`.
Fixed: headers never import. `sprint.h` includes again, and three missing std
includes went in.

### C2. The dual-mode test preamble put the import first
`test_strview.cpp` had `import rpp.strview;` above `#include <rpp/tests.h>`.
gcc-14 re-parses a std header that follows an import and gave 1603 redefinition
errors.
Fixed: includes first, import last. The rule is now a style rule in CLAUDE.md.

### C3. clang-21 refused `tests/test_event_loop.cpp`
`start_coro_on_background_thread` returned `rpp::cfuture<void>` out of
`rpp::async_task`, which instantiates `std::future<cfuture<void>>::get()`. That
returns a `[[clang::coro_return_type]]` without being a coroutine. The
headers-only build failed the same way, so modules were never the cause.
Fixed: a raw `std::thread` plus `join()`, the pattern the same file already used.

### C4. `traits.h` did not compile on its own
It used `std::tuple` without including `<tuple>`.
Fixed: added the include. `tools/check_includes.py self-contained` now reports 0.

### C5. `BUILD_WITH_MODULES` failed with an unreadable error on an old compiler
clang-18 reported an ambiguity between `rpp::ustring` and `ustring`, and gcc-13
failed inside a dyndep scan.
Fixed: `CMakeLists.txt` rejects anything below GCC 14 or Clang 21 at configure
time with a message that names the compiler.

### C7. `BUILD_WITH_MODULES` needed a manual flag on every build
A CI job had to know which compilers support modules.
Fixed: `BUILD_WITH_MODULES` now defaults to `AUTO`. `CMakeLists.txt` hardcodes the
first supported version per family (GCC 14, Clang 21, MSVC 19.34), also checks
CMake 3.28+, C++20 and a Ninja or Visual Studio generator, and turns modules on by
itself. `ON` still demands them and names the exact reason when it cannot.

### C6. The `RppTests` source glob was recursive
`file(GLOB_RECURSE RPP_TESTS tests/*.cpp)` pulled `tests/modulecheck/` in and gave
the binary two `main` functions.
Fixed: non-recursive glob. `tests/modulecheck/` is its own target.
