# ReCpp Bugs

Open issues first, then closed. Keep every entry terse: enough to start an
investigation, no more. A closed entry states the bug and the fix in one or two
lines.

## Open

### B6. A consumer builds modules that no consumer can import
`package()` exports `.h` and `.natvis` only, so a downstream project compiles two
BMIs and then cannot `import` either one. See B3. AUTO still turns modules on in
a dependency build, which is where C11 broke KrattGCS.
The owner chose to keep AUTO as the default, so this stays open until B3 lands and
makes the modules reachable. Until then a consumer pays the build cost for nothing.

### B9. A pool thread tears down a coroutine frame after `.get()` returned
TSAN, `test_semaphore::co_await_semaphore_already_signaled`, on clang-18:
`operator delete` from `~promise()` on `rpp_task_20`, against a main-thread read
of the same address. The trace names `test_co_await_semaphore_timeout`, which is
the **previous** test case.
`semaphore::co_await_handle::await_suspend` posts the resume to a detached pool
task. That task sets the promise, which releases `.get()`, and only then destroys
the coroutine frame. The main thread runs the next case while the pool thread is
still freeing, and the allocator hands the same address out again.
Nothing joins that teardown, so no consumer can know the frame is gone.
Worked around in the test: `test_semaphore` waits for `active_tasks()` to reach 0
between cases. The library ordering is unchanged and still unsynchronized.

### B1. TSAN races reproduce only under CPU load, cause unknown
Two sites, both seen in one loaded sweep of 33 sequential runs, 4 reports:
- `thread_pool.cpp:351`, a pool worker writing in the `catch` handler against a
  main-thread read at `tests.cpp:723`, which is `typeid(e).hash_code()`.
- `semaphore.h:99`, heap-use-after-free in `rpp::semaphore::spin_lock`.

**Measurement state, stated plainly.** The 4 of 33 came from a machine that was
also building and running other suites. Two later sweeps on an idle machine, 67
runs in total, reported nothing. A before-and-after comparison was attempted and
failed: the `git stash` had nothing to stash, because the test fixes were already
committed, so both sweeps measured the same tree. **There is no evidence either
way about whether the B2 fixes changed this.**

So the only positive signal needs CPU contention. CI cannot supply the evidence
either. TSAN aborted at startup on those runners until C9 fixed it, so no TSAN job
before that says anything about races. Reproduce it locally on purpose: run one suite while a
separate load generator saturates the cores, and count over 50 runs. Do not run
several suites at once, because ReCpp does not support that and the result means
nothing.

**A review of `pool_worker::run` found the code correct.** The task moves to a
local that outlives the try block, the handler destroys it before
`unhandled_exception` runs, and that call cleans up the worker state. Do not add a
lock. A wrong fix here is worse than the report.

**Disproved.** Concurrent throw and catch on two threads with nothing shared
produces no TSAN report, over 6 runs of 60 rounds each. libstdc++ exception
storage is not the explanation.

### B2. Timing bounds flake sequential ASAN, now 1 run in 30
Measured on an idle machine, gcc-14 ASAN, one suite at a time.
Before: 3 of 44 runs. After the six fixes below: 1 of 30.
The rate fell but did not reach zero, and the survivors are at new sites
(`test_concurrent_queue.cpp:443` and `test_sockets.cpp:398`), so this is a long
tail and not six isolated defects. Nearly every timing assertion in the suite sets
its bound just above the delay it measures, and a sanitizer erases that margin.
A per-assertion widening will keep finding new sites. The suite needs one slack
policy for sanitizer builds, for example a multiplier applied to every upper
bound when `RPP_ASAN` or the TSAN equivalent is set.
Six causes fixed so far:
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

### C14. `num_physical_cores()` reported host cores inside a container
`std::thread::hardware_concurrency()` answers for the host, not for the cores a
container may use, so the thread pool oversubscribes. On a 3 CPU CircleCI
container it sized the pool to 8, and `parallel_for` ran 1.46x slower than the
serial loop, which failed `test_threadpool.cpp:239`.
`threads.cpp:169` already carries a `CIRCLECI` mitigation, and it sits inside the
`MIPS || RASPI || YOCTO_LINUX || RPP_ANDROID` branch, so no x86 build reaches it.
Fixed: `max_usable_cores()` in `threads.cpp` reads the cgroup quota, v2 `cpu.max`
then v1 `cpu.cfs_quota_us`, and falls back to the affinity mask.
`num_physical_cores()` never reports more than that. Linux covers Android and
Yocto. Windows reads the process affinity mask. macOS and iOS cap nothing.
The `CIRCLECI` guess that divided by 4 is gone.
Verified: `taskset -c 0` gives 1 core where the machine reports 4.
The test also skips the parallel-against-serial bound when `CIRCLECI` is set,
because a shared runner cannot measure that ratio.
Tracked: https://github.com/RedFox20/ReCpp/issues/60

### C13. `pump_until_ready_times_out_without_blocking` aborted on Windows
The job printed the test name and `Exited with code exit status 1`, with no
assertion text, so the process died instead of failing an assertion. Not
reproduced on Linux, and CircleCI logs are not reachable from the dev container.
Cause unknown. `~event_loop()` calls `std::terminate()` when a thread other than
the owner destroys it, which matches an exit with no output, but nothing proves
that path ran.
Hardened, not fixed: the test prints `ready` and the elapsed before it asserts,
drains with the non-throwing `pump_until_ready` instead of `run_until_ready`, and
waits for `has_background_tasks()` to clear. The old drain threw on timeout, and
unwinding left a pool task mid-sleep with a continuation into a loop that the next
`TestCaseSetup()` destroys.
Next Windows failure should print the diagnostic line first. That names which half
of the test died.
All 27 CI jobs pass on the merge, Windows included, so the abort is gone.
The root cause was never proven. Reopen with the printed diagnostic if it returns.

### C12. Every Windows thread name read back as "true"
`get_thread_name()` passed a `PWSTR` to `rpp::to_string()`. MSVC keeps `wchar_t`
distinct from `char16_t`, and `ustring` is `std::u16string`, so no UTF-16 overload
matched. A pointer converts to `bool`, so `to_string(bool)` won and every thread
reported "true".
It predates this branch. It surfaced only after the B8 fix stopped Windows from
aborting in `test_event_loop`, which used to end the run before `test_threadpool`.
Fixed at the root: `strview.h` gains `to_string(const wchar_t*, int)` under
`_WIN32`, matching the `_MSC_VER` guard on `ustrview(const wchar_t*)`. A wide
string can no longer bind to the bool overload.
Verified with `-fshort-wchar`, which gives Linux a 16 bit `wchar_t`: the overload
returns "TestThread", not "true". `test_strview` covers it.

### C11. A modules build broke every consumer on a toolchain without clang-scan-deps
KrattGCS failed its Android build: `"" -format=p1689 --`, then
`sh: line 1: : command not found`, exit 127, on every `.cppm` scan.
Clang scans module dependencies with a separate `clang-scan-deps` binary, and the
Android NDK ships none. CMake's `find_program` leaves
`CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS` empty and still writes the scan rule, so the
configure passes and every ninja scan runs an empty command. AUTO checked the CMake
version, the C++ standard, the generator and the compiler version, but never the
scanner.
Fixed: AUTO also requires an existing `clang-scan-deps` when the compiler is Clang.
Reproduced and verified on one tree: the old CMakeLists printed `Modules enabled`
and failed the build, the new one prints
`RPP MODULES: off, the Clang toolchain ships no clang-scan-deps` and builds clean.

CI missed it for two reasons, and both now have a job:
- Every Android job forced `NO_NINJA=1`, so CI never ran the NDK under the one
  generator that scans modules. `android-cpp20-r29-ninja` runs it.
- CI only ever built ReCpp as the root target. `consumer-integration` builds
  `tests/consumer`, which declares ReCpp through `add_local` and links the exported
  package, on gcc-14 with Ninja so modules turn on inside a dependency build.

Both modules jobs now pass `BUILD_WITH_MODULES=ON` instead of trusting AUTO. AUTO
turns modules off and keeps going, so a job named modules could pass while it built
headers only. `ubuntu-cpp20-modules-clang21` also installs `clang-tools-21`, because
that package, not `clang-21`, carries `clang-scan-deps`.

### C9. CI was red in 5 job classes, and all 24 jobs pass now
The baseline is PR #56, the last merge into master. Five separate causes:
- 4 TSAN jobs. TSAN aborted before the first test:
  `FATAL: ThreadSanitizer: unexpected memory mapping`, exit 66. The runner kernel
  hands out a mapping outside the layout TSAN needs. **This was never B1.**
  Fixed: the test step runs under `setarch $(uname -m) -R`, which turns off
  address space randomization without a privilege.
- 5 clang-21 jobs. `mama install-clang-21` runs `apt-get install clang-21`, and the
  image carries no such package. Fixed: the matrix uses clang-20. `CMakeLists.txt`
  needs Clang 21 for modules, so clang-20 falls back to headers by itself.
- 2 clang-tidy jobs: `packages/ReCpp/linux/compile_commands.json not found`. A
  clang build lands in `linux-clang`. Fixed: `run_clang_tidy` falls back to the
  newest `linux*/compile_commands.json`.
- `ubuntu-cpp20-modules-gcc14` died during BUILD with no output. Modules need
  Ninja, and mama passes `jobs=` to make and msbuild but never to ninja, so ninja
  sized itself from the host core count and the 6 GB box ran out of memory. Fixed:
  the build step prefixes `taskset -c 0-2`, because ninja reads the affinity mask.
  Reported upstream.
- `win64-cpp20-msvc2022`: `test_timer.cpp:540 clock_type_monotonic_coarse` measured
  0 ms against a 10 ms bound. `GetTickCount64` ticks every ~15.6 ms, so a 20 ms
  spin can land inside one tick. Fixed: both coarse-clock tests spin 80 ms.

`ubuntu-cpp23-asan-gcc13` never reproduced and is green. B2 is the first suspect
if it flakes again.

### C8. MSVC failed the modules build with C7684 ambiguous IFC resolution
`RppModuleChecks` carried its own copy of the module file set and also linked
`ReCpp`, which carries the same one. MSVC 14.44 then saw two IFCs for
`rpp.strview` and `rpp.debugging` and refused both.
Fixed: the module-only checks join `RppTests` instead of a target of their own,
and the separate executable is gone. A `.cppm` must not reach two targets that
link each other.

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
