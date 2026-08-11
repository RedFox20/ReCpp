# ReCpp C++20 Modules Migration Plan

Status: plan. The `rpp.strview` experiment is the only module that exists today.

This document explains the existing experiment and records what a real build
proves about it. It then fixes the architecture and gives a phased plan for all
43 public headers.

---

## 1. The existing experiment

### 1.1 The umbrella-include pattern

[`src/rpp/rpp-strview.cppm`](../src/rpp/rpp-strview.cppm) wraps an existing
header. It does not replace it. The shape is three parts:

```cpp
module;                       // 1. global module fragment
#include "strview.h"          //    the umbrella include: one header, all its transitive headers

export module rpp.strview;    // 2. the module declaration

export namespace rpp {        // 3. the export list: make reachable names visible
    using rpp::strview;
    using rpp::line_parser;
    // ~50 more using-declarations
}
```

The include sits in the global module fragment, so every declaration it brings
stays attached to the **global module**. The module adds no new entity. It only
makes existing names visible to an importer.

Three properties follow, and they are why this pattern is correct for ReCpp:

1. **One definition.** `rpp::strview` from `import rpp.strview` and
   `rpp::strview` from `#include <rpp/strview.h>` are the same type. A program
   can mix both in one binary, and two libraries can disagree about which one
   they use.
2. **The static library still works.** `libReCpp.a` is built from `.cpp` files
   that include headers. The module changes no symbol in it.
3. **The header stays the single source of truth.** The `.cppm` carries no
   logic, only a list of names.

libstdc++ and libc++ implement `import std;` the same way.

### 1.2 The dual-mode unit test

[`tests/test_strview.cpp`](../tests/test_strview.cpp) starts with:

```cpp
// when building with modules, there shouldn't be any differences in the test code
// this validates our export modules are alright
#if RPP_BUILD_WITH_MODULES
import rpp.strview;
#endif

#include <rpp/tests.h>   // TestImpl, TestCase, AssertThat are macros, so they need the header
```

The 34 test cases below that preamble are identical in both modes. CMake defines
`RPP_BUILD_WITH_MODULES=1` only on the `RppTests` target when
`BUILD_WITH_MODULES=ON`. So the same source compiles two ways:

| Mode | How to build | What it proves |
|---|---|---|
| headers | `cmake -DBUILD_TESTS=ON` | the classic path still works |
| modules | `cmake -DBUILD_TESTS=ON -DBUILD_WITH_MODULES=ON` | the export list is complete |

The test body is the completeness gate. A name the export list forgets is a
compile error in module mode and a silent pass in header mode. This is a good
design and the plan keeps it. Section 6 makes it stronger.

### 1.3 What CMake does

[`CMakeLists.txt:273-299`](../CMakeLists.txt#L273) adds the `.cppm` to a
`FILE_SET CXX_MODULES` on both targets, and sets `CXX_SCAN_FOR_MODULES ON` on
`RppTests` so that plain `.cpp` files that carry an `import` get scanned.

---

## 2. Ground truth, measured

Measured on this machine with clang-18, CMake 3.28.3, Ninja 1.11.1, C++20.

| # | Finding | Evidence |
|---|---|---|
| 1 | The modules build was **broken** before this plan. | `cmake --build build-mod` failed: `tests/test_strview.cpp:58: error: missing '#include'; 'strlen' must be declared before it is used`. The headers-only build of the same tree passed 76/76. |
| 2 | The cause is a **conditional import inside a header**. | [`sprint.h:6`](../src/rpp/sprint.h#L6) swaps `#include "strview.h"` for `import rpp.strview;`. That drops the transitive `<cstring>`, `<string>` and `<concepts>` from every file that includes `sprint.h`. `sprint.cpp` lost `memcpy` the same way. |
| 3 | Three added includes fix it. | Added `<cstring>` to `sprint.cpp`, `<string>` to `sprint.h`, `<cstring>` to `test_strview.cpp`. The modules build then linked, and `RppTests test_strview` passed 34/34. |
| 4 | **53 of 99 source files** rely on a transitive std include. | Scan over 8 std facilities (`<cstring>`, `<string>`, `<vector>`, `<memory>`, `<cstdio>`, `<algorithm>`, `<atomic>`, `<mutex>`). Every one of them breaks the moment its provider chain becomes an `import`. |
| 5 | `export import` re-export chains work. | A prototype `rpp.sprint` that includes `sprint.h` in its global module fragment and adds `export import rpp.strview;` gave a consumer both `rpp::string_buffer` and `rpp::strview`, plus `operator==` and the `_sv` literal, from one `import rpp.sprint;`. |
| 6 | The `export import` is load-bearing. | Negative control: remove that one line and the same consumer fails with `error: missing '#include'; 'strview' must be declared before it is used`. Reachable is not visible. |
| 7 | A module interface unit emits a **strong symbol**. | `nm` shows `T initializer for module rpp.strview` in `rpp-strview.cppm.o`, and that object is inside `libReCpp.a`. A downstream project that compiles the same `.cppm` produces the same symbol. |
| 8 | clang-18 accepts `import` inside a global module fragment. | A two-module probe compiled clean with `-pedantic-errors`. P1857R3 ends the preamble at the module declaration, so GCC and MSVC are expected to reject this. Do not rely on it. |
| 9 | **27 of 46 headers export zero public macros.** | Those are clean module candidates. `log_colors.h` (114 macros, 0 declarations), `config.h` (62), `tests.h` (39) and `debugging.h` (13) are not. |
| 10 | The module build costs nothing and gains nothing today. | From-scratch `RppTests` at `-j8`: 35 s with headers, 36 s with `BUILD_WITH_MODULES=ON`. Both pass 498/498 test cases over 31 suites. One imported translation unit cannot move the number. Section 9 explains where the real measurement belongs. |

Finding 4 is the size of the work. Finding 2 is the design error to remove.

---

## 3. Architecture decisions

### D1. Keep the header-wrapper facade. Do not write native modules.

A native module moves the code into the `.cppm` and marks each declaration
`export`. A third option includes the header in the module **purview** instead of
the global module fragment, behind an `RPP_EXPORT` macro, as fmt does.

Both alternatives attach `rpp::strview` to the module. That creates a second,
distinct entity next to the one inside `libReCpp.a`. Every consumer of ReCpp
would then have to switch at the same time, and C++17 support would end. A
program that mixes an `#include` and an `import` would violate the
one-definition rule.

Reject both. Keep the facade. The cost is the hand-maintained export list, and
section 5 automates it away.

### D2. One module per header, plus one umbrella module.

Keep the existing `rpp-<header>.cppm` naming and the 1:1 mapping. A consumer
imports only what it uses, and a header edit rebuilds one binary module
interface, not all of them.

Add `src/rpp/rpp.cppm` as the umbrella:

```cpp
export module rpp;
export import rpp.strview;
export import rpp.sprint;
// ... every other module
```

`import rpp;` then replaces a page of includes for consumers who want
everything.

### D3. Named modules, not partitions.

A partition such as `rpp:strview` is private to module `rpp`. An outside
consumer can only write `import rpp;`. That removes selective import, which is
the main reason to use modules at all.

### D4. No `import` inside any header. Ever.

This reverses the current [`sprint.h:6`](../src/rpp/sprint.h#L6) design. Three
reasons, in order of severity:

1. A header that contains `import` **cannot be included in a global module
   fragment** (finding 8). That breaks the facade pattern for every module whose
   header depends on it. It is the reason `rpp-sprint.cppm` cannot exist today.
2. It silently deletes transitive std includes from every consumer (finding 2).
3. It makes the meaning of a header depend on a macro. Two translation units in
   one program then read the same header differently.

ReCpp's own `.cpp` files keep using headers. The module facade exists for
consumers, not for the library's own build. This mirrors libstdc++, whose own
sources do not `import std`.

### D5. Each module re-exports its header's rpp dependencies.

`sprint.h` includes `strview.h`, so `rpp.sprint` must give the importer
`rpp::strview`. Otherwise `import rpp.sprint;` returns a `string_buffer` the
caller cannot feed a `strview` into. Findings 5 and 6 confirm both directions.

The rule: **for every `#include "X.h"` in `Y.h`, `rpp-Y.cppm` gets one
`export import rpp.X;`.** The module graph then matches the include graph, and a
script can check it.

### D6. Ship `.cppm` sources. Never ship a binary module interface.

A binary module interface is tied to one compiler, one version, one standard
level, and one macro configuration. `RPP_ENABLE_UNICODE` alone changes the
export list of `rpp.strview`.

Consumers compile ReCpp's `.cppm` files themselves. Finding 7 adds a second
rule: **keep the `.cppm` files out of the installed static library**, so that a
consumer's own module object never collides with an archived copy.

### D7. Macros stay in headers, and each module names its macro header.

A module cannot export a macro. `LogError`, `Assert`, `AssertThat`, `RPPAPI`,
`FINLINE` and `NOINLINE` are macros. 27 of 46 headers have none (finding 9), and
those need no companion. The rest document one line:

```cpp
import rpp.debugging;         // rpp::LogEvent, rpp::set_log_handler, ...
#include <rpp/debugging.h>    // LogError, Assert, DbgAssert (macros)
```

`log_colors.h`, `tests.h` and `jni_cpp.h` get no module at all. They are macro
frameworks or platform glue, and a module adds nothing.

---

## 4. Phase 0: include hygiene (blocking prerequisite)

Nothing else can start until every file includes what it uses. 53 of 99 files
fail that today (finding 4).

Steps:

1. Run the scan in [`tools/check_std_includes.py`](#appendix-a-the-scan) over
   `src/rpp/*.h`, `src/rpp/*.cpp` and `tests/*.cpp`.
2. Add the missing include to each file, with a trailing comment naming the
   symbol that needs it, matching the existing style in
   [`sprint.cpp:4`](../src/rpp/sprint.cpp#L4).
3. Build headers-only on clang-18, gcc-13 and MSVC. An added include must not
   change behavior.
4. Add the scan to CI as a hard gate, so the debt cannot come back.

The scan covers 8 std facilities. Extend it as the migration finds more. The
count is an upper bound on files to edit, not on lines: most files need one line.

**Estimate: 1 day.** 53 edits are mechanical. The build cycles dominate.

---

## 5. Phase 1: generate the export lists

A hand-written export list rots. `rpp-strview.cppm` already shows two defects:
it re-declares `using rpp::literals::operator""_sv;` under `#if
RPP_ENABLE_UNICODE`, which is a no-op because the first using-declaration
already brings in every overload of that name.

Build `tools/gen_module_exports.py` on libclang. `libclang-18.so` is present on
this machine, and `update_doc_linerefs.py` already proves the repo accepts a
Python declaration scanner.

The tool:

1. Reads the compile flags for the header from `compile_commands.json`.
2. Parses the header and keeps every top-level declaration in namespace `rpp`,
   `rpp::literals` and any other nested namespace, whose source location is that
   header and not a transitively included one.
3. Drops names that start with `_`, and deduplicates by name, because one
   using-declaration covers every overload.
4. Reads the include list of the header and emits one `export import rpp.X;` per
   rpp include (D5).
5. Writes the `.cppm` between two marker comments, so hand-written parts survive.
6. `--check` mode re-generates into memory and diffs. A difference fails CI.

Run it under each macro configuration that changes the surface
(`RPP_ENABLE_UNICODE` on and off, Windows and POSIX) and emit the union under
the matching `#if`.

**Estimate: 1.5 days**, including the `--check` CI gate.

---

## 6. Phase 2: dual-mode tests, both modes in one build

Today the two modes need two separate CMake configurations, so drift can sit in
the tree until someone runs the second one. Fix that: when
`BUILD_WITH_MODULES=ON`, build **two executables from the same sources**.

```cmake
if(BUILD_WITH_MODULES)
    add_executable(RppModuleTests ${RPP_TESTS} ${RPP_SRC})
    target_compile_definitions(RppModuleTests PRIVATE RPP_DEBUG=1 RPP_TESTS=1 RPP_BUILD_WITH_MODULES=1)
    target_sources(RppModuleTests PRIVATE FILE_SET CXX_MODULES FILES ${RPP_MODULES_SRC})
    set_target_properties(RppModuleTests PROPERTIES CXX_SCAN_FOR_MODULES ON)
endif()
```

`RppTests` keeps its current meaning and stops carrying
`RPP_BUILD_WITH_MODULES`. `mama build test` then runs both binaries and one
command covers both modes.

Every test file gets the same preamble, and the test body never changes:

```cpp
#if RPP_BUILD_WITH_MODULES
import rpp.<module under test>;
#endif
#include <rpp/tests.h>   // the test framework is macros
#include <string>        // whatever std facilities the test itself uses
```

Two extra checks belong in the module-mode binary only:

1. **A macro-free compile check.** One translation unit per module that imports
   the module, includes no rpp header, and names every exported symbol once.
   This is the real export-completeness gate, because the normal tests only
   cover what they happen to call. Generate it from the same libclang data as
   the export list.
2. **A mixed-mode link check.** One translation unit that imports `rpp.strview`
   next to another that includes `<rpp/strview.h>`, both in one binary, both
   passing a `rpp::strview` across. This pins property 1 of section 1.1.

**Estimate: 0.5 day.**

---

## 7. Phase 3-4: write the modules, in dependency layers

Work the include graph bottom up. A module can only build after every module it
`export import`s exists.

The layers below come from the actual `#include` graph of `src/rpp/*.h`, not from
a guess. Every module in layer N depends only on layers below N.

| Layer | Modules | Count |
|---|---|---|
| L0 | config, minmax, obfuscated_string, scope_guard | 4 |
| L1 | bitutils, debugging, delegate, endian, future_types, math, predicates, proc_utils, sort, source_loc, **strview** ✓, timepoint, traits, type_traits | 14 |
| L2 | atomic_timepoint, collections, sprint, stack_trace, task, threads, timer, vec | 8 |
| L3 | load_balancer, memory_pool, mutex, paths | 4 |
| L4 | atomic_shared_ptr, close_sync, condition_variable, file_io, sockets | 5 |
| L5 | binary_stream, concurrent_queue, semaphore | 3 |
| L6 | binary_serializer, thread_pool | 2 |
| L7 | event_loop, future | 2 |
| L8 | coroutines | 1 |
| top | umbrella `rpp` | 1 |

43 modules and one umbrella. Excluded: `log_colors.h`, `tests.h`, `jni_cpp.h`
(D7). `rpp.strview` already exists, so 42 remain.

Per module, the loop is: generate the `.cppm`, add it to `RPP_MODULES_SRC`, add
the import preamble to its test, build `RppModuleTests`, fix what the compiler
reports. Most of the fixes are missing `export import` lines and missing std
includes the module build exposes.

Expect the template-heavy headers to be the slow ones: `delegate.h` (48
declarations), `future.h`, `concurrent_queue.h`, `event_loop.h` and
`thread_pool.h`. Watch two things there:

- **Hidden friends.** A friend operator declared inside a class in the global
  module fragment stays attached to the global module, so argument-dependent
  lookup should still find it from an importer. Compilers disagree here. The
  macro-free compile check of section 6 is the detector.
- **Deduction guides and variable templates.** A using-declaration re-exports a
  class template but not its deduction guides. Where a guide matters, restate it
  in the `.cppm`.

**Estimate: L0-L2 is 1 day. L3-L7 is 1.5 days.**

---

## 8. Phase 5: make other projects able to consume this

This is the point of the migration, and it is unwired today. `mamafile.py`
exports only `.h` and `.natvis`, and `CMakeLists.txt` installs no file set.

Three deliverables:

1. **mama.** Extend `package()` to export `.cppm` next to the headers:
   ```python
   self.export_include('src/rpp', build_dir=False,
                       includes_filter=['.h','.cppm','.natvis'], as_includes_root=True)
   ```
   Keep the module objects out of the exported archive (D6). Add a
   `BUILD_WITH_MODULES` passthrough that downstream mamafiles can set.

2. **CMake.** Install the file set so a consumer can rebuild the binary module
   interfaces from source:
   ```cmake
   install(TARGETS ReCpp EXPORT ReCppTargets
           FILE_SET CXX_MODULES DESTINATION lib/cxx-modules/rpp)
   install(EXPORT ReCppTargets CXX_MODULES_DIRECTORY cxx-modules NAMESPACE ReCpp:: DESTINATION lib/cmake/ReCpp)
   ```
   This needs CMake 3.28 on the consumer, the same floor ReCpp already sets.

3. **A consumer example and a documented contract.** Add
   `examples/module_consumer/` with its own `CMakeLists.txt`. State the contract
   in README.md:
   - The consumer compiles ReCpp's `.cppm` files. ReCpp ships no binary module
     interface.
   - The consumer must use the same C++ standard level and the same
     configuration macros, in particular `RPP_ENABLE_UNICODE`.
   - Mixing `import rpp.X` and `#include <rpp/X.h>` in one program is supported.
   - Macros need the header (D7).

Then port one real consumer. `krattcam` and `krattlink` both pull ReCpp through
`add_git`. Convert one file in one of them and measure.

**Estimate: 1.5 days.**

---

## 9. Phase 6: CI, docs and measurement

1. Add a modules job to `.circleci/config.yml`. Use clang-18 and gcc-14. gcc-13
   is the current default and does not carry usable module support, so the
   modules job must pin gcc-14, which the C++26 jobs already use.
2. Wire `tools/check_std_includes.py --check` and
   `tools/gen_module_exports.py --check` as gates.
3. Rewrite the README modules section. It is already stale: it names a test
   suite `test_strview_module` that does not exist.
4. Publish a compile-time measurement. The claim "faster compilation" needs a
   number from `examples/module_consumer/`, not from ReCpp's own build. ReCpp's
   own build gains nothing, because its `.cpp` files keep using headers (D4).

**Estimate: 1 day.**

---

## 10. Risks

| Risk | Impact | Response |
|---|---|---|
| Compiler divergence on reachability, hidden friends and argument-dependent lookup | A module works on clang and fails on gcc or MSVC | Build all three in CI from L0. The macro-free compile check finds it early. |
| gcc-13 has no usable module support | The default CI compiler cannot run the modules job | Pin gcc-14 for that job only. Keep `BUILD_WITH_MODULES` off by default. |
| Export lists rot | A new API is invisible to importers, and nobody notices | `gen_module_exports.py --check` in CI. |
| Duplicate module initializer symbol (finding 7) | Link failure in a consumer that compiles the `.cppm` and links `libReCpp.a` | Keep `.cppm` objects out of the shipped archive. Cover it in `examples/module_consumer/`. |
| Binary module interface build is serial along the dependency chain | A deep module graph slows a cold build | Measure at L7. Merge leaf modules only if a real number justifies it. |
| Sanitizer interaction | `BUILD_WITH_MEM_SAFETY` already disables `/fsanitize=address` on MSVC because of modules ([`CMakeLists.txt:154`](../CMakeLists.txt#L154)) | Keep the modules job separate from the sanitizer matrix. |

---

## 11. Acceptance criteria

The migration is done when all of these hold:

1. `cmake -DBUILD_TESTS=ON -DBUILD_WITH_MODULES=ON` builds `RppTests` and
   `RppModuleTests`, and both pass every test, on clang-18, gcc-14 and MSVC 2022.
2. Every one of the 43 headers has a `.cppm`, and `gen_module_exports.py
   --check` reports no difference.
3. `check_std_includes.py` reports zero files that rely on a transitive std
   include.
4. `examples/module_consumer/` builds against an installed ReCpp using only
   `import rpp;`, and links.
5. The mixed-mode link check passes.
6. README.md documents the contract of section 8 and carries a measured
   compile-time number.

**Total estimate: about 8 working days.** Phase 0 and phase 1 are half of it and
have no module-specific risk.

---

## Appendix A: the scan

The measurement behind finding 4. Save as `tools/check_std_includes.py`, and
extend `RULES` as the migration finds more facilities.

```python
RULES = [
    (r'\b(memcpy|memmove|memset|memcmp|strlen|strcmp|strncmp|strcpy|strstr)\s*\(', ('cstring','string.h')),
    (r'\bstd::(string|wstring|u16string|to_string|stoi|stod)\b', ('string',)),
    (r'\bstd::vector\b', ('vector',)),
    (r'\bstd::(shared_ptr|unique_ptr|make_shared|make_unique|weak_ptr)\b', ('memory',)),
    (r'\b(printf|fprintf|snprintf|fopen|fclose|fwrite|fread|FILE)\s*[\(\*]', ('cstdio','stdio.h')),
    (r'\bstd::(sort|find|min_element|max_element|copy|fill|remove_if)\s*\(', ('algorithm',)),
    (r'\bstd::(atomic|atomic_int64_t|memory_order)\b', ('atomic',)),
    (r'\bstd::(mutex|lock_guard|unique_lock|recursive_mutex)\b', ('mutex',)),
]
```

For each file, strip comments, then report a facility that the file uses and
does not directly include.

## Appendix B: the immediate fixes already applied

Three one-line includes make the current experiment build and pass. They are the
first three edits of phase 0.

| File | Added |
|---|---|
| [`src/rpp/sprint.h`](../src/rpp/sprint.h) | `#include <string>` |
| [`src/rpp/sprint.cpp`](../src/rpp/sprint.cpp) | `#include <cstring>` |
| [`tests/test_strview.cpp`](../tests/test_strview.cpp) | `#include <cstring>` |

Result: `RppTests test_strview` passes 34/34 with `BUILD_WITH_MODULES=ON`.
