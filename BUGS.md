# ReCpp Bugs

Open issues first, then closed. An open entry carries enough to start an
investigation, no more.

**A closed entry is exactly two sentences.** The first names the bug. The second
names the fix. Git holds the story, and a longer entry is noise every agent reads.

## Open

### B14. No caller can reach `pool_types_constructor::allocate<T>()`
Both pool classes declare their own `allocate(int size, int align)`, which hides the
`allocate<T>()` of the base. `pool.allocate<int>()` reports `expected primary-expression
before 'int'`, because the name resolves to the two-argument function. Every sibling
(`construct`, `allocate_array`, `construct_array`) stays reachable, because no derived
class reuses those names.

A fix adds `using pool_types_constructor::allocate;` to each pool class. Nothing in the
repository calls the template today, so the change breaks no caller.

### B2. A test which trusts the clock fails on a loaded machine
Nearly every timing assertion sets its bound just above the delay it measures. A
sanitizer, an emulator, or a busy CI runner erases that margin.
This has two shapes. A bound too tight reports the overrun, as
`test_concurrent_queue::wait_pop_until` did with 219 ms against a 10 ms ceiling. A
sleep used to order two threads reports a wrong result instead, as
`test_close_sync::basic_close_prevention` did on MSVC with
`~ImportantState: data != "aaaabbbbcccc"`. AGENTS.md R2 already says to wait on an
event, not on the clock.
Reproduce it without CI. Pin CPU hogs to the test core:
```bash
for h in 1 2; do taskset -c 0 bash -c 'while :; do :; done' & done
taskset -c 0 ./bin/RppTests nogdb test_concurrent_queue
kill %1 %2
```


### B7. No test pins the volatile read in obfuscated_string::to_string()
The read is load-bearing, and a small translation unit proves it. Build a `main` which
decodes five obfuscated strings straight into one `printf`, and `clang++ -O2` folds the
round trip and writes the plaintext into the binary. The volatile read stops that on
gcc and clang at `-O0` through `-O3`.
`test_obfuscated_string::the_binary_holds_no_plaintext` scans the running executable
and asserts the end result, which is worth having. It stays green when the volatile
goes, because the shape inside a large test binary does not fold. Three caller shapes
were tried, including a block-scope object feeding a printf sink.
A fix needs a translation unit which folds on demand, so a separate tiny target under
`tests/` is the likely answer.

### B11. The documented clang-tidy gate analyzes nothing on a warm build tree
`CXX20=1 mama gcc build clang-tidy test="nogdb -vv"` exits 0 and reports no finding, while the
CI job of the same name fails. AGENTS.md names that command as the gate, so a session which
trusts it pushes a red build. Two rounds on PR #73 went red this way.

`packages/ReCpp/linux/CMakeCache.txt` carries no `CMAKE_CXX_CLANG_TIDY` after that command.
mama reuses the build directory a plain build configured, so the analysis never turns on.
Adding `configure` sets the variable, and then a gcc-14 build stops instead, because
clang-tidy is clang and cannot parse `-fmodules-ts`, `-fmodule-mapper=` or `-fdeps-format=`.
The CI gcc-13 jobs never hit that, because gcc-13 builds no modules.

Until this is fixed, check one finding against clang-tidy directly:
```bash
clang-tidy-18 --checks='-*,performance-enum-size' tests/test_sprint.cpp -- -std=c++23 -Isrc
```
A fix makes the documented command reconfigure, and turns modules off for the analysis.

### B10. A pool worker reads its semaphore after the pool destroyed it
TSAN reports `heap-use-after-free` at shutdown, 1 run in 80. The main thread runs
`~unique_ptr<pool_worker>` out of the worker vector, while `pool_worker::run()` is still
inside `rpp::semaphore::spin_lock()` at `semaphore.h:102`. A second report reads the
`pool_task_state` shared pointer the same way.

This is a lifecycle order defect, not a refcount TSAN cannot see, so C25 does not cover it
and no suppression should. Two pinned cores reproduce it, and the `race:` patterns never hide
a `heap-use-after-free`:
```bash
CXX20=1 mama gcc tsan build
for i in $(seq 1 40); do for j in 1 2; do
  taskset -c 0,1 env TSAN_OPTIONS="halt_on_error=0" \
    packages/ReCpp/linux-tsan/RppTests test_future > /tmp/b10_${i}_$j.log 2>&1 &
done; wait; done
grep -l 'heap-use-after-free' /tmp/b10_*.log
```

### B8. gcc-14 breaks a module for three shapes, and each one has a workaround
`tools/gen_module_exports.py` carries `NO_EXPORT` with one entry and `NO_IMPORT` with two.
Every module ships now. Delete an entry when a newer gcc reads the module back.

`NO_CONFIG` is a third list, and it is not a gcc defect. It is empty, because `sprint.h`
now guards its `std::to_string` branch the way `type_traits.h` guards the trait. Any parse
failure the list does not name reaches the caller and fails the run.

The importer stops with `failed to read compiled module cluster N: Bad file data`, then
`failed to load pendings for` a libstdc++ internal. That name changes per run, and the
cluster number does too, so neither one identifies the shape.

Shape 1, in `rpp.sprint`. An export naming a function in the `std::__cxx11` inline namespace
makes the module unreadable. `std::to_string` and `std::stoi` both do it, and `std::swap`
does not. The form does not matter. An `is_detected_v` alias, a plain alias template and a
C++20 concept all fail the same way. So does a concept which calls an unexported helper that
names it. `NO_EXPORT` drops `has_std_to_string` from `rpp.type_traits`, which is what
`rpp.sprint` imports. A header includer still gets the trait.

Shape 2, in `rpp.task` and `rpp.tests`. A module which includes `future_types.h` in its
global module fragment and also imports `rpp.future_types` writes an unreadable `.gcm`.
Either half alone is fine. The importer only fails when it also includes `<rpp/tests.h>`.
`NO_IMPORT` drops that one re-export, so an importer which needs `rpp::coro_handle` imports
`rpp.future_types` itself.

Shape 3, in `rpp.tests`. gcc runs out of imported source locations and stops with
`internal compiler error: in write_location, at cp/module.cc:16271`. It prints
`unable to represent further imported source locations` first. The count is what matters,
not one module. Seven re-exports pass, and `rpp.sprint` as the eighth crashes it, while
`rpp.sprint` alone passes. The dependency `.gcm` files have to come from an `-O2` build to
reach the limit, so a `-O0` bisect hides it. `NO_IMPORT` drops `rpp.sprint`, and an importer
of `rpp.tests` which needs `rpp::string_buffer` imports `rpp.sprint` itself.

Reproduce either shape in seconds, outside cmake. Build every `.cppm` in the
`RPP_MODULES_SRC` order into one `gcm.cache`, then compile a consumer:
```bash
cd $(mktemp -d)
for f in $(sed -n '/set(RPP_MODULES_SRC/,/^    )/p' ~/ReCpp/CMakeLists.txt | grep -o 'src/rpp/rpp-[a-z_]*\.cppm'); do
  g++ -std=c++20 -fmodules-ts -I ~/ReCpp/src -c -x c++ ~/ReCpp/$f -o $(basename $f .cppm).o
done
printf '#include <rpp/tests.h>\nimport rpp.task;\nint main(){return 0;}\n' > u.cpp
g++ -std=c++20 -fmodules-ts -I ~/ReCpp/src -c u.cpp -o u.o    # 0 errors with NO_IMPORT
```
Put the offending line back into the generated block by hand to watch it fail. Only gcc 14.2
ran this check, so retry on clang-21 and on a newer gcc.

### B9. `--check-undocumented` reads 29 of the 48 headers and reports the rest as clean
`extract_public_decls` returns nothing for 19 headers, so the gate never asks whether
README.md documents them. It reported "All public declarations are documented" while the
`sort.h` table listed 1 of its 4 functions.

```
headers the extractor reads: 29
headers it returns nothing for: 19
  bitutils.h close_sync.h concurrent_queue.h condition_variable.h coroutines.h debugging.h
  debugging.macros.h future.h jni_cpp.h log_colors.h math.h memory_pool.h obfuscated_string.h
  predicates.h proc_utils.h semaphore.h sort.h task.h traits.h
```

Reproduce it with the loop which produced that count:
```bash
python3 -c "
import importlib.util, os
spec = importlib.util.spec_from_file_location('u','update_doc_linerefs.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
for h in sorted(os.listdir('src/rpp')):
    if h.endswith('.h') and not m.extract_public_decls(f'src/rpp/{h}'): print(h)
"
```
A fix teaches the extractor the declaration shapes it misses, and it needs a count of what
the 19 headers then owe README.md. The count decides whether the gate can stay green.

### B5. `update_doc_linerefs.py` matches a macro name inside another macro body
It pointed `LogError` at `debugging.macros.h:162`, which is the `LogError` call
inside `DbgAssert`, not the `#define LogError` at line 139. Corrected by hand.
The script's own docstring already warns that it has mistakes.

## Closed

### C25. libtsan.so never read the suppression hook, because it was hidden (was B6)
`-fvisibility=hidden` kept `__tsan_default_suppressions` out of the dynamic symbol table gcc's
`libtsan.so` reads, so every pattern was dead. The hook took a default-visibility attribute and
moved to `tests/test_sanitizers.cpp`, beside a `dlsym` test which fails without it.

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
