# ReCpp Bugs

Open issues first, then closed. An open entry carries enough to start an
investigation, no more.

**A closed entry is exactly two sentences.** The first names the bug. The second
names the fix. Git holds the story, and a longer entry is noise every agent reads.

## Open

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

### B10. A pool worker reads its semaphore after the pool destroyed it
TSAN reports `heap-use-after-free` at shutdown, 1 run in 80. The main thread runs
`~unique_ptr<pool_worker>` out of the worker vector, while `pool_worker::run()` is still
inside `rpp::semaphore::spin_lock()` at `semaphore.h:102`. A second report reads the
`pool_task_state` shared pointer the same way.

This is a lifecycle order defect, not a refcount TSAN cannot see, so C25 does not cover it
and no suppression should. Reproduce it with the C25 loop below, and read the
`heap-use-after-free` reports instead of the races.

### B8. gcc-14 writes a module for sprint.h and task.h that no importer can read
Both `.cppm` files compile, and the `.gcm` lands. An importer then stops with
`failed to read compiled module cluster N: Bad file data`, followed by
`fatal error: failed to load pendings for 'std::_Mutex_base'`. The message names a
libstdc++ internal, so this is a compiler defect and not an export list mistake.
Neither module ships until a toolchain reads them back.

Each one fails alone, so no pair of modules causes it. A consumer that includes
`<mutex>` before the import fails the same way. The other eighteen modules import.

Reproduce it. No `rpp-sprint.cppm` ever landed, so write the skeleton first. The generator
fills the block, and it refuses a file which carries no markers.
```bash
cat > src/rpp/rpp-sprint.cppm <<'EOF'
module;
#include "sprint.h"
export module rpp.sprint;
// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
// GENERATED EXPORTS END
EOF
python3 tools/gen_module_exports.py sprint.h
# add src/rpp/rpp-sprint.cppm to RPP_MODULES_SRC in CMakeLists.txt
# add `import rpp.sprint;` to tests/module_consumer/masked_module_only.cpp
cd tests/module_consumer
CXX20=1 python3 run_test.py --compiler gcc --expect modules --jobs 4
```
Retry it on clang-21 and on a gcc newer than 14.2. Only gcc 14.2 ran this check.

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
B6 blamed the libc++ spelling of the C15 pattern. The real cause is one attribute. gcc links
`libtsan.so`, which reads `__tsan_default_suppressions` through the global dynamic symbol
table, and `-fvisibility=hidden` kept the definition out of it. `dlsym` then answered with the
weak hook inside `libtsan.so`, which returns null, so every pattern was dead. clang links its
runtime statically, which is why C15 worked there.

The hook and its patterns moved to `tests/test_sanitizers.cpp`, beside the test which calls
`dlsym(RTLD_DEFAULT, ...)` and reads the string back. That test fails without the attribute.

Both reports name `test_future::test_except_handler_chaining`, which shares one exception
object between two pool workers. The refcount which orders them sits in an uninstrumented
`libstdc++.so`, so TSAN sees no edge.

Measured on 2 pinned cores: 3 reports in 120 runs before, 0 in 120 after, and the two patterns
matched 3 times each. Ten full-suite runs matched no suppression at all and passed every case.
```bash
CXX20=1 mama gcc tsan build
for i in $(seq 1 60); do for j in 1 2; do
  taskset -c 0,1 env TSAN_OPTIONS="halt_on_error=0 print_suppressions=1" \
    packages/ReCpp/linux-tsan/RppTests test_future > /tmp/w_${i}_$j.log 2>&1 &
done; wait; done
grep -l 'WARNING: ThreadSanitizer' /tmp/w_*.log | wc -l
```

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
