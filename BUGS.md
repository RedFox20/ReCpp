# ReCpp Bugs

Open issues first, then closed. Keep every entry terse: enough to start an
investigation, no more. A closed entry states the bug and the fix in one or two
lines.

## Open

### B1. TSAN reports a race on the exception a coroutine rethrows
`tests/test_coroutines.cpp:135`, `AssertThat(e.what(), "aargh!"s)`.
TSAN: write in `std::runtime_error::~runtime_error` from
`std::promise<std::string>::~promise` on one thread, read in `strlen` on another.
Fires on 2 of 6 parallel full-suite TSAN runs, 0 of 6 idle, 0 of 8 when only
`test_coroutines` runs. Predates the module work.
Open question: real use-after-free, or a TSAN blind spot, because libstdc++
refcounts the exception object in code TSAN does not instrument.
Next step: an ASAN run under the same 6-way parallel load. ASAN detects a real
use-after-free by shadow memory, so a clean ASAN run over many repeats points at
the blind spot. A first attempt did not finish, because ASAN at 6-way parallel on
8 cores takes over 10 minutes per run.
If it is a blind spot, the fix is a `tsan_suppressions.txt` entry naming
`std::__future_base::_Result_base::_Deleter`. ReCpp has no suppressions file yet.

### B2. `test_timer` and `test_threadpool` fail under CPU oversubscription
Both fail about 1 run in 6 when 6 test binaries share 8 cores. They pass every
idle run. Timing assertions with no slack. CI runs on shared runners, so this is
a real flake source.

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
