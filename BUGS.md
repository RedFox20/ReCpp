# ReCpp Bugs

Open issues first, then closed. An open entry carries enough to start an
investigation, no more.

**A closed entry is exactly two sentences.** The first names the bug. The second
names the fix. Git holds the story, and a longer entry is noise every agent reads.

## Open

### B2. Timing bounds have no slack for a loaded machine
Nearly every timing assertion sets its bound just above the delay it measures. A
sanitizer, an emulator, or a busy CI runner erases that margin.
Reproduce it without CI. Pin CPU hogs to the test core:
```bash
for h in 1 2; do taskset -c 0 bash -c 'while :; do :; done' & done
taskset -c 0 ./bin/RppTests nogdb test_concurrent_queue
kill %1 %2
```


### B6. The C15 TSAN suppression covers libc++ only, so gcc still reports the future race
C15 closed the same false positive on clang. `tests/main.cpp` guards
`__tsan_default_suppressions` with `#if defined(__clang__)`, and the pattern it returns
is `race:std::__1::promise`, which is the libc++ spelling. Under gcc the entity is
`std::__future_base::_State_baseV2`, so no pattern matches and no suppression compiles.
`ubuntu-cpp20-tsan-gcc13` reports it intermittently, in `~_State_baseV2` and in
`exception_ptr::_M_release`, both inside an uninstrumented `libstdc++.so`.
Read C15 first. A fix adds the gcc branch and a libstdc++ pattern, and it needs a run
which proves the suppression hides this race and hides no other.

### B5. `update_doc_linerefs.py` matches a macro name inside another macro body
It pointed `LogError` at `debugging.macros.h:162`, which is the `LogError` call
inside `DbgAssert`, not the `#define LogError` at line 139. Corrected by hand.
The script's own docstring already warns that it has mistakes.

## Closed

### C24. `_va_comma` dropped the argument list when the first argument started with `(`
The one-probe fallback let `_spaces_on_empty_token` consume that leading paren, so the
list read as empty and printf then read an unwritten stack slot. The 4-probe emptiness
test replaces it, and `test_debugging` pins the fallback on every compiler.

### C23. `udp_poll_multi_stress_test` never sized its receive queue (CI flake)
The default 208 KB queue holds only 270 of the 500 datagrams, so a pre-empted CI
receiver dropped packets. Both sockets now take a 512 KB buffer, and `available()`
separates a kernel drop from a `poll()` defect.

### C22. A module-only formatted log macro was reported to redefine `__wrap` (was B13)
A finding claimed a module-only formatted log macro redefines the exported `__wrap`
against the textual config.h copy. The consumer test now formats int, string and
strview on the module path on gcc and clang, and `__wrap` moved to config.types.h.

### C21. A local dependency never shipped its module objects (was B12)
The report read the build tree archive `libReCpp.a`, which no consumer links.
mama exports a stripped `mama-nomodules/libReCpp.a`, so a whole-archive link
finds no duplicate initializer.

### C20. A seeded compiler cache hid clang-scan-deps from CMake (was B11)
`consumer-clang21` reported no `clang-scan-deps` while three copies sat on the box,
because a seeded `CMakeCXXCompiler.cmake` made CMake skip the `find_program`. mama
fixed the seed, so the job no longer carries `nocache`.

### C19. mamabuild cannot export C++20 modules (was B3)
`package()` exported `.h` and `.natvis` only, so no `.cppm` reached a consumer and
nobody outside ReCpp could import `rpp.strview`. The latest mama release and the
ReCpp pull request beside it make the module sources reachable.

### C18. TSAN races under CPU load never reproduced again (was B1)
TSAN reported 4 races in 33 loaded runs, at `thread_pool.cpp:351` and
`semaphore.h:99`. 120 runs on 32 saturated cores reported nothing, so both sites
are closed as unreproducible.

### C17. An event_loop outliving a borrowed pool is an API limit, not a defect (was B10)
`test_event_loop::custom_thread_pool` gave the loop a pointer to a local pool, so
Android aborted on a destroyed mutex. Not a defect, because an `event_loop` never
outlives what it borrows, so the contract is documented and the pool is a fixture
member.

### C16. The 52 missing std includes were not a defect (was B4)
B4 claimed 52 files break when a provider chain becomes an `import`. A negative
control on GCC 14.2 disproved it, so changeset 1a left the migration plan.

### C15. TSAN blamed a pool worker for freeing a promise the waiter still used (was B9)
TSAN reported `operator delete` in `~promise()` on a pool worker against main
thread mutex traffic, three times on clang. libc++ keeps the future refcount
inside an uninstrumented `libc++.so`, so `tests/main.cpp` suppresses
`race:std::__1::promise` and a regression test forces the order on demand.

### C14. `num_physical_cores()` reported host cores inside a container
`std::thread::hardware_concurrency()` answers for the host, so a 3 CPU CircleCI
container sized the pool to 8 and `parallel_for` ran 1.46x slower than serial.
`max_usable_cores()` now reads the cgroup quota and the affinity mask, and
`num_physical_cores()` never reports more than that.

### C13. `pump_until_ready_times_out_without_blocking` aborted on Windows
The job died with no assertion text, and the cause was never proven. The test now
prints a diagnostic before it asserts and drains with the non-throwing
`pump_until_ready`, and every CI job passes.

### C12. Every Windows thread name read back as "true"
`get_thread_name()` passed a `PWSTR` to `rpp::to_string()`, no UTF-16 overload
matched under MSVC, so the pointer bound to `to_string(bool)`. `strview.h` gained
`to_string(const wchar_t*, int)` under `_WIN32`.

### C11. A modules build broke every consumer on a toolchain without clang-scan-deps
The Android NDK ships no `clang-scan-deps`, so CMake wrote a scan rule with an
empty command and every `.cppm` scan exited 127. AUTO now demands an existing
`clang-scan-deps` on Clang, and the `android-cpp20-r29-ninja` and
`consumer-integration` jobs cover the gap CI had.

### C9. CI was red in 5 job classes, and all 24 jobs pass now
Five unrelated causes: a TSAN memory-mapping abort, a missing `clang-21` package, a
clang-tidy build-directory mismatch, a ninja out-of-memory, and a coarse-clock
bound on MSVC. Each got its own fix, in order `setarch -R`, clang-20, a
`linux*/compile_commands.json` fallback, `taskset` for ninja, and an 80 ms spin.

### C8. MSVC failed the modules build with C7684 ambiguous IFC resolution
`RppModuleChecks` carried its own copy of the module file set and also linked
`ReCpp`, so MSVC saw two IFCs for the same module. The module-only checks joined
`RppTests`, because a `.cppm` must not reach two targets which link each other.

### C1. The modules build did not compile
`sprint.h` swapped `#include "strview.h"` for `import rpp.strview;`, which dropped
the transitive std includes from every consumer. Headers never import now, and
three missing std includes went in.

### C2. The dual-mode test preamble put the import first
`test_strview.cpp` placed `import rpp.strview;` above `#include <rpp/tests.h>`, and
gcc-14 gave 1603 redefinition errors. Includes come first and imports last, now a
style rule in AGENTS.md.

### C3. clang-21 refused `tests/test_event_loop.cpp`
`start_coro_on_background_thread` returned `rpp::cfuture<void>` out of
`rpp::async_task`, which instantiates a `[[clang::coro_return_type]]` getter that is
not a coroutine. A raw `std::thread` plus `join()` replaced it.

### C4. `traits.h` did not compile on its own
It used `std::tuple` without including `<tuple>`. The include went in, and
`tools/check_includes.py self-contained` reports 0.

### C5. `BUILD_WITH_MODULES` failed with an unreadable error on an old compiler
clang-18 reported an ambiguity between `rpp::ustring` and `ustring`, and gcc-13
failed inside a dyndep scan. `CMakeLists.txt` now rejects anything below GCC 14 or
Clang 21 at configure time, with a message which names the compiler.

### C7. `BUILD_WITH_MODULES` needed a manual flag on every build
A CI job had to know which compilers support modules. `BUILD_WITH_MODULES` defaults
to `AUTO`, which checks the compiler family version, CMake 3.28+, C++20, and the
generator, then turns modules on by itself.

### C6. The `RppTests` source glob was recursive
`file(GLOB_RECURSE RPP_TESTS tests/*.cpp)` pulled `tests/modulecheck/` in and gave
the binary two `main` functions. The glob is non-recursive, and
`tests/modulecheck/` is its own target.
