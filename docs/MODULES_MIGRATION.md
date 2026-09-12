# ReCpp C++20 Modules Migration Plan

Revision 19. Forty-four modules exist, all of L0 to L8, plus eight group umbrellas and the
top umbrella `rpp`. The migration is complete. Only `rpp.tests` re-exports another module.

This document explains the pattern and records what real builds prove about it.

## Handover state

**Landed:** PR #57 the two modules, #63 changeset 1b, #64 the self-contained
gate, #65 changeset 6.

| Item | State |
|---|---|
| the forty-four modules | build and pass on gcc-14, and CI covers clang-21 and MSVC 14.52 |
| `debugging.macros.h` | split out, 50 preprocessed lines against 32893 |
| `BUILD_WITH_MODULES=AUTO` | on per toolchain, GCC 14 / Clang 21 / MSVC 19.34 |
| Include-order style rule | in AGENTS.md, and the `import-order` gate holds it |
| `tools/check_includes.py` | 6 checks. 4 gate CI, and `missing` and `unused` stay ungated |
| `tests/test_modules.cpp` | module consumer test, 41 cases. It includes `tests.h` and the macro header only, so what those two mask needs a module-only target |
| `tests/module_consumer/` | a real mama consumer, on gcc, clang and MSVC, with 8 module-only targets which `run_test.py` builds and runs |
| mama | 0.14.0 exports the `.cppm` files and strips the module objects |
| CI | 28 jobs on GitHub Actions, and CircleCI is gone |
| test counts | 584/584 on the modules build, 539/539 on the header build |

**Changeset state:** 1a is dropped, see section 4. 1b, 2, 3 and the mama half of
6 landed. The generator drives all forty-four modules. 4 is done through the generator
`--check`. 5 is finished: L0 to L8 and the umbrella. Section 11 lists what 7 owes.

**Next:** nothing in this plan. Section 11 holds the open follow-ups.

**Measured: grouping the small modules would save nothing.** The 88 module translation
units of a cold gcc-14 build cost 121 s, and the 14 cheapest cost 0.7 s of that, which is
0.6%. Fifty units over one second each carry 88%, and each one is slow because its header
is large. Merging those parses the same declarations and widens every rebuild, so the 1:1
mapping of D2 stands. The umbrella itself costs about 50 ms.

**A module re-exports nothing, and two headers ship with a workaround.** The generator
carries `NO_EXPORT` with three entries and `RE_EXPORT` with one, and `BUGS.md` **B8** names
the four shapes gcc-14 breaks. Shapes 2 and 3 both need a re-export, so neither can fire on
one library-wide entry. Shape 4 lives in `binary_stream.h`, whose destructor moved out of
line. The other two `NO_EXPORT` entries are internal names, a CRTP mixin and a unit-test
friend, and no gcc defect drives them. `NO_CONFIG` names the six headers which do not
compile on bare metal, see **B15**. The generator selftest still pins all three knobs.

**gcc-14 cannot compile `std::promise` in an importer, and six modules carried that before
L7.** Any module whose global module fragment includes `<future>` breaks such a consumer,
and `rpp.task` alone reproduces it. No export list changes the crash, so the L7 tests name
the `future.h` factories in an unevaluated context. **B16** holds the reproducer and the
list of ten headers which reach `<future>`. Nine of them ship as a module now.

**An importer which includes `<string>` first reads a different module.** `rpp.file_io`
passed every gate and still broke that consumer, so `std_string_module_only.cpp` holds the
shape now. Add a module which names `std::string` to that target when it lands.

An `is_detected_v` alias which names `std::to_string` was the first shape, so the C++20
concept replaced it. That did not help. A plain alias, a concept, and a concept which calls an
unexported helper all fail the same way. The export reaches a `std::__cxx11` function either
way. Only dropping the export works, so `has_std_to_string` left the module surface.
The concept stayed, because it reads better and skips one `is_detected_v` instantiation.

Delete the `NO_EXPORT` entry when a newer gcc reads the module back. The B8 reproducer builds
every `.cppm` into one `gcm.cache` outside cmake, which answers in seconds.

**What L1 and L2 exposed.** Six headers declared public API the compiler could not export,
and each fix landed with the layer. `math.h` marked every function `static` and left its
constants without `inline`. `traits.h` wrapped `function_traits` in an anonymous namespace,
so every translation unit held a different type. `timepoint.h` left nine constants without
`inline`, and `stack_trace.h` marked `CALLSTACK_MAX_DEPTH` static beside inline.
`type_traits.h` left nine variable templates without `inline`, which gcc gives one copy per
translation unit, so a module importer and a header includer read different addresses for
the same trait. `tests/test_modules_identity.cpp` takes that address through the module
alone, and `test_modules.cpp` compares it against the header address.

**The generator learned three configuration rules.** It used to emit an export list for the
configuration it happened to parse in, so any declaration a header hides under another
configuration became an unguarded export. Each rule carries a selftest.

| Rule | Reaches | Mechanism |
|---|---|---|
| a macro a define can toggle | `RPP_ENABLE_UNICODE`, `!RPP_BARE_METAL` | parse each configuration, union the names, guard the difference |
| a macro no define reaches | none today, `RPP_HAS_COROUTINES` until it went | read the `#if` span the header brackets, and match a declaration by line |
| an inline namespace | `rpp::literals`, `rpp::duration_literals` | read the `inline` keyword from the header |

The second rule matches by declaration location, not by name text. A text search also
matched a name a guarded block only mentions, which would have hidden the whole
`rpp::semaphore` and `rpp::concurrent_queue` classes wherever the macro is 0. Both headers
became modules in L5, and their export lists prove the rule holds.

The first rule used to subtract from the host configuration alone, so a name only another
configuration declares reached no export list. `mutex.h` declares `critical_section` and
`synchronized_critical` on FreeRTOS and on Cortex-M, and a sweep over all 31 headers names
it as the only one. The rule unions the configurations now, and it negates the guard for a
name the host build lacks. `ALT_GUARD` replaces that negation where the header states a
tighter condition, which for `mutex.h` is `RPP_HAS_CRITICAL_SECTION_MUTEX`, because a
Cortex-M build is not always a bare metal build.

The generator also mishandled two kinds of declaration. An out-of-line member definition
reached the export list and broke the build. A blanket skip of `UNEXPOSED_DECL` hid every
variable template. Both tools carry a selftest case for the shape they missed.

**A module can move a declaration.** `rpp::sort` lived in `collections.h`, so `rpp.collections`
carried it and an importer of `rpp.sort` could not call it. The generic overloads moved to
`sort.h`, which already declared `contiguous_container`. `collections.h` keeps the two shapes
only it can offer, the vector overload an explicit `rpp::sort<T>(v)` names, and the
`element_range` overload whose by-value parameter lets a const view sort its mutable
elements. Asking which module should carry a name is worth doing per layer.

**Open questions:** `BUGS.md` B2, B5, B9, B10, B15 and B16. B8 keeps three workarounds open,
and each one goes away when a newer gcc reads the module back. B6 closed as C25, so the TSAN jobs no
longer fail on the exception refcount race.

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

[`tests/test_strview.cpp`](../tests/test_strview.cpp) carries a preamble, and the
test cases below it never change:

```cpp
#include <rpp/tests.h>   // TestImpl, TestCase, AssertThat are macros, so they need the header
#include <cstring>       // strlen

#if RPP_BUILD_WITH_MODULES
import rpp.strview;      // the import goes last, see section 2.1
#endif
```

The experiment shipped this the other way round, with the import above the
includes. That order costs 1603 compile errors on gcc-14. Section 2.1 explains
why, and this branch fixes it.

The 34 test cases below that preamble are identical in both modes. CMake defines
`RPP_BUILD_WITH_MODULES=1` only on the `RppTests` target when the toolchain
carries modules. So the same source compiles two ways:

| Mode | How to build | What it proves |
|---|---|---|
| headers | `cmake -DBUILD_TESTS=ON` | the classic path still works |
| modules | `cmake -DBUILD_TESTS=ON` on GCC 14+, Clang 21+ or MSVC 19.34+ | the exported names the tests use resolve |

The test body is the same in both modes, so the module build proves the export
list carries what the tests use. It is not a full completeness gate: the file also
includes `<rpp/tests.h>`, so a forgotten name can still resolve through a header.
Section 8 says where the real gate belongs.

### 1.3 What CMake does

[`CMakeLists.txt:273-299`](../CMakeLists.txt#L273) adds the `.cppm` to a
`FILE_SET CXX_MODULES` on both targets, and sets `CXX_SCAN_FOR_MODULES ON` on
`RppTests` so that plain `.cpp` files that carry an `import` get scanned.

---

## 2. Ground truth, measured

Measured on this machine with gcc-14.2, clang-21.1.5, clang-18.1, CMake 3.28.3,
Ninja 1.11.1 and C++20, plus one MSVC 14.44 build elsewhere. gcc-14, clang-21 and
MSVC are tier 1 (D7). Every claim below is measured on at least one tier 1
compiler.

| # | Finding | Evidence |
|---|---|---|
| 1 | The modules build was **broken** before this plan. | `cmake --build` failed: `test_strview.cpp: error: missing '#include'; 'strlen' must be declared before it is used`. The headers-only build of the same tree passed 76/76. |
| 2 | The cause was a **conditional import inside a header**. | `sprint.h` swapped `#include "strview.h"` for `import rpp.strview;`. That dropped the transitive `<cstring>`, `<string>` and `<concepts>` from every file that includes `sprint.h`. `sprint.cpp` lost `memcpy` the same way. |
| 3 | Three added includes fixed it. | Modules build links, `RppTests test_strview` passes 34/34, and 498/498 pass in both modes. |
| 4 | **53 of 99 source files** rely on a transitive std include, and that is safe. | Scan over 8 std facilities. The earlier claim, that each one breaks when its provider chain becomes an `import`, is **disproved**. A facade includes its header in the global module fragment, which keeps those declarations reachable to an importer. Negative control on GCC 14.2: delete `#include <string>` from `tests/test_modules.cpp` and keep `import rpp.strview;`, and the modules build still passes. Row 2 is a different failure, an `import` inside a header, which AGENTS.md now forbids. |
| 5 | `export import` re-export chains work. | A prototype `rpp.sprint` that includes `sprint.h` in its global module fragment and adds `export import rpp.strview;` gave a consumer both `rpp::string_buffer` and `rpp::strview`, plus `operator==` and the `_sv` literal. |
| 6 | The `export import` is load-bearing. | Negative control: remove that one line and the same consumer fails with `error: missing '#include'; 'strview' must be declared before it is used`. Reachable is not visible. |
| 7 | A module interface unit emits a **strong symbol**. | `nm` shows `T initializer for module rpp.strview` in `rpp-strview.cppm.o`, and that object sits inside `libReCpp.a`. A consumer that compiles the same `.cppm` produces the same symbol. |
| 8 | **gcc-14, clang-21 and clang-18 all accept `import` inside a global module fragment.** | A two-module probe compiled clean on all three, and clang-21 accepted it with `-pedantic-errors`. gcc-14 emitted the real cross-module call, `U fa@a()`. Revision 1 of this document predicted a rejection. That prediction was wrong. P1857R3 still restricts the global module fragment to preprocessing directives, so this is compiler laxity, not a guarantee. D4 no longer rests on it. |
| 9 | **27 of 46 headers export zero public macros.** | Those are clean module candidates. Section 6 covers the rest. |
| 10 | The module build costs nothing and gains nothing today. | From-scratch `RppTests` at `-j8`: 35 s with headers, 36 s with modules. One imported translation unit cannot move the number. |
| 10b | ReCpp passes on every compiler on this machine. | Headers: clang-18 and gcc-13 pass 31 suites and 498/498. Modules: gcc-14 and clang-21 pass 32 suites and 501/501, the extra suite being `test_modules`. gcc-13 and clang-18 fall back to headers on their own and say why. |
| 11 | A **header unit does deliver macros**, and clang calls it experimental. | `import "rpp/endian.h";` gave a consumer the macro `RPP_BYTESWAP16` and the function `rpp::readBEU16` in one import. clang-18 warns `-Wexperimental-header-units`. See section 6, option M4. |
| 12 | **gcc-14 rejects a std library include that follows an import.** | See section 2.1. It is a compile error, and it broke the whole modules build until the preamble moved. |
| 13 | **Mixed import and include link and run correctly on gcc-14.** | One translation unit imports `rpp.strview`, another includes `<rpp/strview.h>`, both build a `rpp::strview`, and the program links against `libReCpp.a` and returns the right answer. Property 1 of section 1.1 holds in practice, not only on paper. |
| 14 | gcc-14 builds every module cleanly. | The full `RppTests` modules build passes with 0 errors and 498/498 test cases, once the preamble order is right. |
| 15 | **clang-21 builds every module cleanly, and it needs no include order.** | 30 suites and 454/454 test cases with 0 errors. Both preamble orders compile. See the D7 table. |
| 16 | **A `cfuture` passed through `std::future` is not portable, and this branch fixes it.** | `start_coro_on_background_thread` returned a `cfuture<void>` out of `rpp::async_task`. That instantiates `std::future<cfuture<void>>::get()`, which returns a `[[clang::coro_return_type]]` without being a coroutine, so clang-21 rejected it. A raw `std::thread` plus `join()` replaces it, matching the pattern the same file already uses at line 1079. clang-21 now passes 498/498 in both modes. |
| 17 | **A pre-existing race lives in `test_coroutines.cpp:135`, not in the module work.** | `AssertThat(e.what(), "aargh!"s)` reads the message of a `std::runtime_error` while another thread frees the future shared state that owns it. Under 6 parallel TSAN runs it fires 2 of 6 times, on the code before this branch and after it alike. Idle, both report 0 of 6. Track it apart from the migration. |

### 2.2 What an import costs a consumer, measured

Finding 10 said the module build costs and gains nothing. That measured the whole build.
This measures one translation unit, which is what a consumer feels. Every number is the
median of 7 alternating runs of `g++ -O2 -fsyntax-only` on gcc-14.2, with the binary module
interfaces already built, so it isolates the consumer side.

One facility per translation unit, the header against its module:

| Facility | Preprocessed header lines | Header | Import | Speedup |
|---|---|---|---|---|
| `timepoint` | 32,316 | 391 ms | 430 ms | 0.91x |
| `strview` | 33,902 | 424 ms | 440 ms | 0.96x |
| `delegate` | 34,226 | 406 ms | 431 ms | 0.94x |
| `sockets` | 50,471 | 613 ms | 490 ms | 1.25x |
| `sprint` | 60,853 | 613 ms | 471 ms | 1.30x |
| `paths` | 65,350 | 669 ms | 486 ms | 1.38x |
| `file_io` | 65,743 | 688 ms | 496 ms | 1.39x |
| `concurrent_queue` | 73,093 | 854 ms | 541 ms | 1.58x |
| `thread_pool` | 73,983 | 903 ms | 561 ms | 1.61x |
| `future` | 76,380 | 976 ms | 573 ms | 1.70x |
| **median** | | | | **1.34x** |

**An import costs about the same whatever it carries.** Over a 2.4x range of header size the
import moves from 430 ms to 573 ms, a factor of 1.33, while the header moves from 391 ms to
976 ms, a factor of 2.5. So the import has a floor near 430 ms and almost no slope. Break-even
sits near 40,000 preprocessed lines: below it the header wins, above it the import wins, and
the heaviest header in the library wins by 1.70x.

**Headers share better than modules when one file uses many.** `#pragma once` parses a shared
header once however many headers pull it, and each import pays its own load:

| Facilities in one TU | Headers | Imports | Ratio |
|---|---|---|---|
| 1 | 435 ms | 448 ms | 0.97x |
| 2 | 601 ms | 521 ms | 1.15x |
| 4 | 622 ms | 627 ms | 0.99x |
| 6 | 712 ms | 858 ms | 0.83x |
| 8 | 999 ms | 1120 ms | 0.89x |
| 10 | 1083 ms | 1438 ms | 0.75x |

The marginal cost of one more facility is about 72 ms as a header and about 110 ms as an
import. So the import advantage is real for a file which takes one or two heavy facilities,
and it inverts for a file which takes many.

**Size of the imported module decides the cost.** The same one-line body which calls
`rpp::to_string`:

| How rpp arrives | Time | Against the header |
|---|---|---|
| `#include <rpp/sprint.h>` | 611 ms | 1.00x |
| `import rpp.sprint;` | 398 ms | 1.53x |
| `import rpp.text;` | 489 ms | 1.25x |
| `import rpp;` | 2949 ms | 0.21x |

`rpp.text` adds `rpp.strview` and `rpp.obfuscated_string` to the same work and costs 23% more
than `rpp.sprint`. `import rpp;` costs 7.4x `rpp.sprint`. What counts is the transitive
closure, not the module file: a group umbrella interface is 1 to 3 KiB, because it holds only
`export import` lines, while `rpp.sprint` is 4.9 MiB.

**So name the narrowest module which covers the file.** A group umbrella buys organization,
and it costs whatever its members cost.

**The std parse was the floor, and `rpp.std` removes it.** gcc-14 ships no libstdc++ std
module, so `src/rpp/rpp-std.cppm` stands in: its global module fragment includes the std
headers ReCpp puts in a public signature, and it exports those names. Replacing
`#include <string>` and `#include <atomic>` with `import rpp.std;` changes every row:

| Facility | Header | Import, std as headers | Import, with `rpp.std` | Speedup |
|---|---|---|---|---|
| `timepoint` | 397 ms | 442 ms | 45 ms | 8.74x |
| `file_io` | 685 ms | 497 ms | 109 ms | 6.28x |
| `sockets` | 619 ms | 479 ms | 159 ms | 3.89x |
| `future` | 976 ms | 561 ms | 271 ms | 3.60x |
| `paths` | 690 ms | 503 ms | 196 ms | 3.52x |
| `strview` | 433 ms | 453 ms | 145 ms | 2.99x |
| **median over ten** | | **1.34x** | | **3.56x** |

The many-facility inversion nearly goes with it. Ten rpp imports beside two std headers took
1452 ms against 1074 ms for eleven headers, which is 0.74x. The same ten beside
`import rpp.std;` take 1149 ms, which is 0.94x. So most of what the import column lost to
header sharing was a std parse the header column shared for free.

Three names stay out of `rpp.std`, each because gcc-14 breaks on it. Exporting
`std::exception_ptr` writes an interface gcc cannot read back (B19). Including `<future>` in
the fragment kills `std::swap` lookup, so `std::future` and `std::promise` go with it (B20).
Exporting `std::get` breaks `std::unique_ptr` in every importer (B21). Section 11.1 has the
table, and `BUGS.md` has a reproducer for each.

### 2.3 What the linker sees

Same compiler, same sources, one cmake tree per mode, `-j4`:

| | Modules ON | Modules OFF | Delta |
|---|---|---|---|
| `libReCpp.a` | 17,117,276 B | 12,624,868 B | +35.6% |
| `RppTests` | 40,894,992 B | 37,903,232 B | +7.9% |
| Defined symbols in the archive | 1135 | 1081 | +54 |
| `RppTests` relink, median of 3 | 0.76 s | 0.64 s | +19% |
| Full build | 105.5 s | 64.4 s | +64% |

**Each module interface unit emits exactly one strong symbol**, `T initializer for module
rpp.X`. The 54 is 44 modules, 8 groups, the top umbrella, and `rpp.std`, so the symbol delta
is entirely those initializers. Finding 7 recorded the symbol. This says what a library of
them costs.

The archive grows more than the binary, because the linker drops the interface objects a
program never reaches. The full build grows most, and section 2.2 says why: 88 module
translation units, which is 44 modules compiled twice.

**A consumer must never compile ReCpp's `.cppm` and link `libReCpp.a` too.** Both define the
same initializer, so the link fails on a duplicate symbol. mama already handles it: it
exports the `.cppm` sources and strips the module objects out of the package, so a consumer
builds its own interfaces against its own flags. D6 records why shipping a binary interface
is not an option.

### 2.1 The gcc-14 ordering rule, characterized

The question this answers: does the failure show up as a build error, a link
error, or a silent duplicate symbol? **It is a compile error.** It is loud, and
it cannot corrupt a binary.

Each row is a complete translation unit compiled with `g++-14 -std=c++20 -fmodules-ts`.

| Case | Result | First diagnostic |
|---|---|---|
| `import rpp.strview;` then `#include <string>` | **960 errors** | `/usr/include/c++/14/type_traits:214: error: redefinition of 'template<class ... _Bn> constexpr const bool std::__and_v'` |
| `import rpp.strview;` then `#include <vector>` | **974 errors** | `bits/cpp_type_traits.h:97: error: template definition of non-template ...` |
| `import rpp.strview;` then `#include <cstdio>` | 0 errors | a thin C wrapper pulls no libstdc++ internal template |
| `import rpp.strview;` alone | 0 errors | |
| `#include <string>` then `import rpp.strview;` | 0 errors | |

Reading: GCC re-parses the standard library header textually, and the internal
libstdc++ templates collide with the same entities the module already made
reachable through its global module fragment. Order decides it. A C header such
as `<cstdio>` stays clean because it declares no template.

Two consequences:

1. **Every consumer puts its includes first and its imports last.** This
   reverses the preamble the strview experiment shipped. That preamble put
   `import rpp.strview;` above `#include <rpp/tests.h>`, and it produced 1603
   errors across the gcc-14 modules build. With the order flipped, the same build
   passes with 0 errors and 498/498 test cases.
2. **The rule needs no ODR audit.** Finding 13 shows that a program which mixes
   both styles across separate translation units links and runs correctly. There
   is no silent duplicate-symbol path to guard against, only a compile order to
   respect.

clang-18 accepts both orders in a minimal case. It fails on the real
`test_strview.cpp` in include-first order with
`error: use of overloaded operator '=' is ambiguous (with operand types 'rpp::ustring' and 'ustring')`,
which is a clang-18 defect in the facade's using-declarations. clang-18 is tier 2
(D7), so this does not gate the plan. **Verify it on clang-21 as the first task
of changeset 5.**

---

## 3. Architecture decisions

### D1. Keep the header-wrapper facade. Do not write native modules.

A native module moves the code into the `.cppm` and marks each declaration
`export`. A third option includes the header in the module **purview** instead of
the global module fragment, behind an `RPP_EXPORT` macro, as fmt does.

Both alternatives attach `rpp::strview` to the module. That creates a second,
distinct entity next to the one inside `libReCpp.a`. Every consumer of ReCpp
would then have to switch at the same time. A program that mixes an `#include`
and an `import` would violate the one-definition rule.

Reject both. Keep the facade. The cost is the hand-maintained export list, and
section 7 automates it away.

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

### D3. Named modules, not partitions.

A partition such as `rpp:strview` is private to module `rpp`. An outside
consumer can only write `import rpp;`. That removes selective import, which is
the main reason to use modules at all.

### D4. No `import` inside any header. Ever. **(accepted, applied)**

Three reasons, in order of severity:

1. It silently deletes transitive std includes from every consumer. This is
   measured, not predicted: it is what broke the build (finding 2).
2. A header that carries an `import` puts that import wherever the header is
   included, and the gcc-14 ordering rule then decides whether the consumer
   compiles (section 2.1). A header cannot know where a consumer includes it.
3. It makes the meaning of a header depend on a macro. Two translation units in
   one program then read the same header differently.

Revision 1 gave a fourth reason, that an `import` is illegal inside a global
module fragment. Finding 8 disproves it on both clang-18 and gcc-14. The rule
stands on the three reasons above.

ReCpp's own `.cpp` files keep using headers. The module facade exists for
consumers, not for the library's own build. This mirrors libstdc++, whose own
sources do not `import std`.

### D5. A module re-exports nothing, and an importer names every module it uses.

A header leaks whatever its includes pull in. A module owes an importer no such
leak, so `RE_EXPORT` starts empty and a surface has to earn its entry.

The first rule was the opposite one. Every `#include "X.h"` in `Y.h` gave
`rpp-Y.cppm` an `export import rpp.X;`, so the module graph mirrored the include
graph. That put 61 re-export lines in the library. With all 61 gone, the 571 case
suite stayed green and five of the six module consumers passed.
`RppTestsModuleOnly` was the sixth, and one line fixed it. `TestImpl` expands to a
constructor which takes `rpp::strview`, so `rpp.tests` re-exports `rpp.strview`.

A consumer which needs `rpp::strview` from `import rpp.sprint;` writes
`import rpp.strview;` too. One line names what that consumer depends on.

### D6. Ship `.cppm` sources. Never ship a binary module interface.

A binary module interface is tied to one compiler, one version, one standard
level, and one macro configuration. `RPP_ENABLE_UNICODE` alone changes the
export list of `rpp.strview`.

Consumers compile ReCpp's `.cppm` files themselves. Finding 7 adds a second
rule: **keep the `.cppm` files out of the installed static library**, so that a
consumer's own module object never collides with an archived copy.

### D7. Two compiler tiers. Only gcc-14 and clang-21 carry modules.

ReCpp builds on 8 compilers today, and their module support ranges from good to
absent. Splitting them into tiers keeps the migration honest, and it stops a
weak toolchain from setting the ceiling for the strong ones.

| Tier | Compilers | What they get |
|---|---|---|
| **1, modules** | gcc-14+, clang-21+, MSVC 19.34+ | every module, and the module-only consumer checks |
| **2, headers only** | clang-18, gcc-13, Android NDK clang, the MIPS gcc-12 cross build | the classic `#include` path, unchanged and fully supported |

This is a support decision, not a language one. D1 already makes it free:
`libReCpp.a` and every header behave the same either way, so a tier 2 compiler
loses nothing but the `import` syntax. `BUILD_WITH_MODULES` stays **OFF** by
default is `AUTO`. [`CMakeLists.txt`](../CMakeLists.txt#L274) hardcodes the first
supported version of each family, GCC 14, Clang 21 and MSVC 19.34, and it also
checks CMake 3.28, C++20 and the generator. `AUTO` turns modules on wherever the
toolchain supports them, so a CI job needs no flag. `ON` demands them and names
the exact reason when it cannot. Without that guard, clang-18 reports the
ambiguity of section 2.1 and gcc-13 fails deep inside a dyndep scan.

**Verification status: both tier 1 compilers are measured.** gcc-14.2 comes from
apt. clang-21.1.5 comes from the LLVM GitHub release tarball, because the agent
proxy blocks apt.llvm.org and `mama install-clang-21` needs a `sudo` this
container does not have.

| Claim | gcc-14.2 | clang-21.1.5 |
|---|---|---|
| the module interface unit builds | pass | pass |
| `export import` re-export chain | pass | pass |
| `export import` is load-bearing (negative control) | fails without it | fails without it, 4 errors |
| mixed import and include link and run in one binary | pass | pass |
| a std include after an import | **rejected, ~960 errors** | **accepted, 0 errors** |
| `import` inside a global module fragment | accepted | accepted, even with `-pedantic-errors` |
| the full modules test suite | **498/498** | **498/498** |

Two results need their footnote.

**Only gcc-14 needs the include order.** clang-21 compiles both orders cleanly.
The rule in AGENTS.md still holds for every file, because a portable file has to
satisfy the stricter compiler.

**clang-21 needed one fix outside the module work, and this branch carries it.**
`tests/test_event_loop.cpp` passed a `cfuture<void>` through `rpp::async_task`,
which instantiates `std::future<cfuture<void>>::get()`. clang-21 rejects that:
the function returns a `[[clang::coro_return_type]]` without being a coroutine.
The headers-only clang-21 build failed the same way, so modules were never the
cause. A raw `std::thread` plus `join()` replaces it, and clang-21 now passes
498/498 in both modes.

**Build note for a tarball toolchain.** A prebuilt LLVM release needs
`-DCMAKE_CXX_FLAGS=-resource-dir=$(clang++ -print-resource-dir)`. Without it
`clang-scan-deps` fails every `.ddi` scan with `'stddef.h' file not found`. A
distro-packaged clang does not need this.

MSVC is tier 1. A local build on MSVC 14.44 compiles both modules. It exposed one
rule the other compilers hide: **the same `.cppm` must not reach two targets that
also link each other.** MSVC then finds two IFCs for one module name and fails with
`C7684 module name 'rpp.strview' has an ambiguous resolution to IFC`.
`tests/test_modules.cpp` lives inside `RppTests` for that reason, not in a second
in-tree target. `tests/module_consumer/` is a separate project which links the
installed library, so each of its targets compiles the `.cppm` files once.

---

## 4. Changeset 1: include hygiene

**The `missing` check is dropped. It measures no defect.** The claim was that each
of its 52 findings breaks when its provider chain becomes an `import`. A negative
control on GCC 14.2 disproves it: delete `#include <string>` from
`tests/test_modules.cpp`, keep `import rpp.strview;`, and the modules build passes.
A facade includes its header in the global module fragment, so those declarations
stay reachable to an importer. See `BUGS.md` C16.

The header failure that did happen was an `import` inside a header. That is a
different defect, and D4 forbids it.

The other checks in [`tools/check_includes.py`](../tools/check_includes.py) survive.

```bash
tools/check_includes.py all                     # report every check
tools/check_includes.py self-contained --check  # CI gate, exit 1 on a finding
```

### 4.1 What the tool measures

| Check | How it works | Findings today |
|---|---|---|
| `self-contained` | compiles each header alone, twice, with nothing before it | **0**, and the CI gate holds it there |
| `missing` | a file uses a std facility it does not include itself | **dropped**, see above |
| `unused` | comment out one include, the header still compiles, **and** the header names nothing the include declares | **0**: this branch removed 3 and marked 2 re-exports |
| `redundant` | same, but the header does name something the include declares, so a sibling include leaks it | **49** |
| `std` | same, for a std header, where the scan cannot enumerate the declared names | **60** |
| `import-order` | an `#include` after an `import`, or an `import` in a header | **0**, and the CI gate holds it there |
| `selftest` | runs the scan over 65 crafted sources and pins the line each one names | **0**, and the CI gate holds it there |

`self-contained` protects a consumer that includes one header first. CI runs it
beside `import-order` and `selftest`, so a regression in any of the three fails a
build rather than a review.

The `unused` and `redundant` split is the important part of what remains. Both compile without
the line. Only the first is safe to delete. Removing a `redundant` line trades a
direct dependency for a hidden one, which is the opposite of hygiene.

### 4.2 The unused includes

The first count of 10 was wrong. The scan read only the names the included file
declared itself, and a header also hands over the names it re-exports.
`condition_variable.h` calls `LogError`, which `debugging.h` re-exports from
`debugging.macros.h`, so the scan called a used include unused. `declared_names`
now walks the quoted includes, and 11 of the 15 candidates turned out to be
`redundant` instead.

Four survive the corrected scan. Three are stale, and this branch removes them.

| Header | Unused include | Note |
|---|---|---|
| `atomic_shared_ptr.h` | `config.h` | removed, its one macro guard reads a std feature-test macro |
| `bitutils.h` | `config.h` | removed, it names only `<cstdint>` types |
| `traits.h` | `config.h` | removed, it names only `<type_traits>` and `<tuple>` |
| `debugging.h` | `log_colors.h` | kept, `debugging.h` re-exports the color macros |

The fourth is a re-export, and the deletion test cannot see the difference. A
re-export serves the consumer, so the including header names nothing from it and
looks stale every time the scan runs. Mark the line and no check reports it:

```cpp
#include "log_colors.h" // re-export, consumers color their own log text
```

`debugging.h` carries the marker on `log_colors.h` and on `debugging.macros.h`.
The scan now reports 0 unused includes, so this check can join the CI gate.

### 4.3 Changeset 1b, the removals

**The removals are approved.** ReCpp breaks the accidental include chain for its
consumers. Those includes are old backward-compatibility additions, and no
header needs them. A consumer that breaks was already depending on something
ReCpp never promised, and the fix belongs in the consumer.

Order the work so the risk falls, not rises:

1. Delete the unused includes. Three are gone, and the two re-exports carry a
   marker. The count is 0.
2. Work the 49 redundant lines. Each one needs the missing direct include added
   in the same commit, so the count usually stays the same and the dependency
   becomes honest.
3. Leave the 60 std candidates last. Some are platform-conditional
   (`byteswap.h`, `malloc.h`, `sanitizer/tsan_interface.h`, `QString`), and a
   removal that passes on Linux can break Windows or Android. Build every CI
   platform before you delete one of these.

Tell the downstream teams what landed. `debugging.h` has 15 direct includers
inside this repo, and `config.h` has 27, so the reach outside is larger. A
one-line note in the release text saves each team the bisect.

**Estimate: 1 day.**

---

## 5. Changeset 2: the rpp-header include check

The std half of this check went with changeset 1a. The rpp half is load-bearing
for modules, and for a different reason.

D5 first gave each module one `export import rpp.X;` per rpp include, and changeset
3 generated those lines from the include list. Finding 6 measured what a missing one
costs: the consumer fails with
`error: missing '#include'; 'strview' must be declared before it is used`.
**Reachable is not visible for an rpp name, even though it is for a std name.**

D5 dropped that rule later, and the check still earns its place. The global module
fragment of a `.cppm` includes one header, so a header which names `rpp::strview`
and skips `strview.h` breaks its own module. The check also turns the 43 redundant
findings into an exact list of the includes to add.

**Estimate: half a day.**

---

## 6. Macros: the decision table to vet

A named module cannot export a macro. This section states the options and the
evidence, and marks the recommendation. **Nothing here is settled.**

### 6.1 Which macros are actually public

README.md is the public API index, so a macro documented there is public.
Cross-referencing every `#define` against README gives:

| Header | Public macros | What they are |
|---|---|---|
| `config.h` | **51** | `RPPAPI`, `FINLINE`, `NOINLINE`, `NODISCARD`, `RPP_ENABLE_UNICODE`, `RPP_HAS_CXX23`, and the platform probes |
| `tests.h` | **10** | `TestImpl`, `TestCase`, `TestInit`, `AssertThat`, `AssertEqual`, `AssertThrows`, ... |
| `endian.h` | **9** | `RPP_BYTESWAP16/32/64`, `RPP_TO_BIG*`, `RPP_TO_LITTLE*` |
| `debugging.h` | **4** | `LogInfo`, `LogWarning`, `LogError`, `Assert` |
| `mutex.h` | 2 | `RPP_HAS_CRITICAL_SECTION_MUTEX`, `RPP_SYNC_T` |
| `close_sync.h`, `minmax.h`, `scope_guard.h` | 1 each | `try_lock_or_return`, `RPP_SSE_INTRINSICS`, `scope_guard` |

Everything else is an implementation macro (`DELEGATE_FINLINE`, `_rpp_wrap_args`,
`__log_format`) and needs no plan.

### 6.2 The four options

**M1. Do nothing. The consumer includes the header for macros.**
```cpp
import rpp.debugging;
#include <rpp/debugging.h>   // LogError, Assert
```
Zero churn. The include re-parses the whole header, so the import buys nothing
for that header. Correct, and it wastes the point of the module.

**M2. Split the macros into a dependency-free companion header.**
```cpp
import rpp.debugging;
#include <rpp/debugging_macros.h>   // parses in milliseconds, includes nothing
```
This works only when the macro body needs no declaration the companion has to
carry. Measured per header:

| Header | Splits cleanly? | Why |
|---|---|---|
| `endian.h` | **yes** | the macros expand to compiler builtins, `__builtin_bswap16` and `_byteswap_ushort` |
| `config.h` | **yes** | the macros are already dependency-free, and `config.h` includes nothing |
| `debugging.h` | **no, not without a cost** | `LogError` expands to `_LogError(__log_format(...))`, and that needs `_LogError`, `_LogFuncname` and `rpp::shorten_filename` visible. The module would have to export three names that look private. |
| `tests.h` | **no** | `TestImpl` expands to a class that derives from `rpp::test`, so the type must be visible first |

**M3. One shared `<rpp/macros.h>` for every public macro.** One include, one
place to look. It couples unrelated macros, and the consumer takes all 80 to get
one.

**M4. Header units.** `import "rpp/debugging.h";` exports the declarations **and**
the macros. Finding 11 proves it works: a consumer got `RPP_BYTESWAP16` and
`rpp::readBEU16` from a single `import "rpp/endian.h";` on clang-18.

The blockers are tooling, not language. clang warns
`-Wexperimental-header-units`. CMake has no stable file set for header units, so
every consumer would hand-roll the build rules. GCC support is incomplete.

### 6.3 The decisions, settled

Two rules, then the per-header calls.

**Rule 1. A header that is mostly macros stays out of the modules.** A module of
a macro header exports almost nothing, and the consumer still has to include the
header. `log_colors.h` (114 macros, 0 declarations) and `config.h` (64 macros,
14 declarations) both qualify.

**Rule 2. Split to a `_macros.h` case by case, never as a sweep.** A split earns
its place only when the module then covers the whole non-macro surface, and when
the macro header stays free of includes.

| Header | Module? | Split macros? | Why |
|---|---|---|---|
| `log_colors.h` | no | no | 114 macros, 0 declarations. Rule 1. |
| `config.h` | **no** | **the types split out** | Rule 1 for the macros. The integer aliases moved to `config.types.h`, which module `rpp.config` exports. `config.h` includes `config.types.h`, so header-mode consumers keep the aliases, and a module consumer writes one `import rpp.config` rather than re-listing ten. `rpp.strview`, `rpp.debugging` and `rpp.config` prove the pattern. |
| `config.types.h` | **yes, `rpp.config`** | n/a | The ten integer aliases, split from `config.h` so a module can export them. A macro cannot be exported, so the macros stay in `config.h`. |
| `debugging.h` | yes | **yes, and it pays** | The split is done and measured. `debugging.macros.h` costs 50 preprocessed lines, `debugging.h` costs 32893. Section 6.4 has the numbers. |
| `endian.h` | yes | **no** | The 9 byte-swap macros would split cleanly into compiler builtins, but 9 macros do not pay for a new header and a new name to remember. |
| `tests.h` | **yes** | **yes** | The only clear win, and the split is done. 41 macros against 45 declarations, so the module carries real weight. `tests.macros.h` carries five includes, because two of the names it expands to are macros an import cannot bring. |

Net effect on the module count: `config.h` leaves, and `config.types.h` and
`tests.h` join, so the total is 44.

**`tests.h`, the one split.** `TestImpl` expands to a class that derives from
`rpp::test`, so the type has to be visible where the macro expands. The consumer
writes:

```cpp
#include <rpp/tests.macros.h>   // TestImpl, TestCase, AssertThat
import rpp.tests;               // rpp::test and the rest of the framework
```

`tests.macros.h` declares nothing, and it includes five headers. `RPP_SOURCE_LOC_CURRENT`
is a macro, so no import can carry it, and `source_loc.h` brings it. `future_types.h`
brings `rpp::coro_handle`, which `TestCaseCoro` expands to. `<memory>`, `<typeinfo>` and
`<exception>` carry the three std names the macros expand to. Every other name comes from `import rpp.tests`, and macro
expansion happens later, at the use site.

`tests/module_consumer/tests_module_only.cpp` writes a suite from that pair alone, with no
`<rpp/tests.h>`, so the build fails when the split stops paying. Dropping either macro
header from `tests.macros.h` fails that target, and dropping any of the three std headers
does not. gcc reaches those three through the global module fragment of the module, so no
gcc build can pin them. They stay, because AGENTS.md asks a header to include what it uses,
and no other toolchain promises that reach.

**M4, header units, stays rejected for now.** It is the only mechanism that
carries macros and declarations together, and finding 11 shows it works. CMake
has no stable file set for it, and clang calls it experimental. Re-evaluate in
about two years.

### 6.4 The `debugging.h` split, done and measured

`debugging.h` is the hardest case, so it sets the pattern. The split is on this
branch, and the classic include path did not change.

| File | What it holds | Preprocessed lines | Parse |
|---|---|---|---|
| `debugging.h` | declarations, then it includes the macro header | 32893 | 325 ms |
| **`debugging.macros.h`** | every macro, and `config.h` | **50** | **11 ms** |

Measured with `g++-14 -std=c++20 -E`, and 5 runs of `-fsyntax-only` for the parse
time. The macro header is **30 times cheaper to parse**, so an importer pays 11 ms
instead of 325 ms for its macros.

**One include decides that number.** The first split kept `<stdexcept>` in the
macro header, because `ThrowErr` and `AssertEx` name `std::runtime_error`. That
version measured 32734 lines, a saving of 0.5 percent, which is no saving at all.
`<stdexcept>` alone costs 32690 lines and 425 ms, while `config.h` costs 46 lines
and 13 ms. Moving `<stdexcept>` out is the entire win.

So `debugging.macros.h` includes `config.h` and nothing else. A user of `ThrowErr`
or `AssertEx` adds `<stdexcept>`, which include-what-you-use asks for anyway.
`debugging.h` still includes it, so no existing file changes.

**What each consumer writes**

```cpp
// classic, unchanged
#include <rpp/debugging.h>
SetLogSeverityFilter(LogSeverityWarn);
LogInfo("Beautiful Soup %d", 42);

// module
#include <rpp/debugging.macros.h>   // 50 lines: LogInfo, LogError, Assert, ThrowErr
#include <stdexcept>                // only when the file uses ThrowErr or AssertEx
import rpp.debugging;               // includes first, the import last

SetLogSeverityFilter(LogSeverityWarn);
LogInfo("Beautiful Soup %d", 42);
```

Every call site keeps the spelling it has today. `SetLogSeverityFilter` keeps
global scope, because the module exports it with a global-scope using-declaration:

```cpp
export module rpp.debugging;
export using ::SetLogSeverityFilter;
export using ::LogSeverity;
export using ::LogSeverityInfo;   // an unscoped enum does NOT carry its enumerators
export using ::LogSeverityWarn;
export using ::LogSeverityError;
```

**The cost: the module exports six names that look private.** The macros expand to
`_LogInfo`, `_LogWarning`, `_LogError`, `_LogExcept`, `_FmtString` and
`_LogFuncname`, so an importer needs all six visible. `rpp::__wrap` and
`rpp::__clean_type` go the same way. This is the price of the split, and it is why
the question was worth asking before doing it.

**Exporting an unscoped enum does not export its enumerators.** The first
prototype exported `LogSeverity` and forgot `LogSeverityWarn`. The consumer failed
with `use of undeclared identifier \'LogSeverityWarn\'`. The generator of
changeset 3 has to walk every enumerator.

**The check that keeps this honest.** `tests/test_modules.cpp` imports
`rpp.debugging`, includes `<rpp/debugging.macros.h>`, and drives the logging API the
way a consumer does. Section 8 states what that catches and what it does not.
Whatever shape it takes, it must not get a target of its own: a second target that
also links `ReCpp` gives MSVC two IFCs for one module name, which is `C7684`.

This is the template for the other 41 modules.

---

## 7. Changeset 3: generate the export lists

A hand-written export list rots. `rpp-strview.cppm` already shows a defect: it
re-declares `using rpp::literals::operator""_sv;` under `#if
RPP_ENABLE_UNICODE`, which is a no-op because the first using-declaration
already brings in every overload of that name.

Build `tools/gen_module_exports.py` on libclang. `libclang-18.so` is present on
this machine, and `update_doc_linerefs.py` already proves the repo accepts a
Python declaration scanner.

The tool, as built:

1. Parses the header with fixed flags, `-x c++ -std=c++20 -I src` plus a compiler
   builtin include dir. It reads no `compile_commands.json`, so it needs no build dir.
2. Keeps every top-level declaration in namespace `rpp` and its nested namespaces,
   whose source location is that header. It also walks one `extern "C"` linkage
   spec, which is how the `RPPCAPI` logging names reach the list.
3. Drops a name which starts with `__`, and deduplicates by name, because one
   using-declaration covers every overload. `__wrap` and `__clean_type` are
   allowlisted, because the logging macros expand to them. A single leading
   underscore stays, because `_LogInfo` and `_FmtString` are part of that surface.
   It also drops an internal-linkage name, because clang rejects a using-declaration
   which exports a `static` function. gcc accepts one, so only clang-21 caught it.
4. Emits an `export import rpp.X;` only where `RE_EXPORT` names one, because a
   module re-exports nothing by default (D5).
5. Writes the `.cppm` between two marker comments, so hand-written parts survive.
6. `--check` mode re-generates into memory and diffs. A difference fails CI.
7. Reports a module whose name repeats a macro any rpp header defines, and `STEMS` holds
   the rename. MSVC expands that name inside `export module` and inside `import`, so an
   `#undef` in the fragment fixes the producer and leaves every importer broken.
   `scope_guard.h` names module `rpp.scopeguard` for that reason.
8. Reports a public name which internal linkage hides. clang refuses to export one, so
   the generator drops it, and a silent drop would let `--check` approve an empty facade.
   `INTERNAL_OK` names the helpers whose loss is intended, and it is empty.

**Internal linkage is a defect in a header a module wraps, not a case to work around.**
MSVC gave `obfuscated_string.h` C2129 when a module-only test instantiated `to_string()`,
because an importing TU cannot reach a `static` helper the header only declares to it. The
two helpers dropped `static`, which a `constexpr` function needs for external linkage, and
they export like every other name.

**L1 starts with `math.h`.** All 12 of its public names are `static constexpr` or a
namespace-scope `constexpr`, so every one has internal linkage and the gate reports it.
Give each one external linkage before you write `rpp-math.cppm`.

It runs under `RPP_ENABLE_UNICODE` on and off, and guards the difference with that
`#if`. **The Windows and POSIX axis is not implemented.** A declaration which exists
on one platform only is missed when the generator runs on the other. The compiler axis
has the same hole: libclang defines `__clang__`, so the generator never sees the GCC
`_obfuscated` literal operator. `rpp-obfuscated_string.cppm` exports it by hand, below
the markers, and `test_modules` covers it.

**A module carries no preprocessor state.** `minmax.h` undefines the Windows `min` and
`max` macros, and an `import` cannot repeat that. A Windows importer needs `NOMINMAX`
or the header. No export list closes this, so the README states it per module.

**Estimate: 1.5 days**, including the `--check` CI gate.

---

## 8. Changeset 4: the export-completeness gate

The dual-mode idea in section 1.2 stays: one test source, two build modes, and the
module mode proves the export list carries what the tests use.

**What shipped.** [`tests/test_modules.cpp`](../tests/test_modules.cpp) imports both
modules, includes `<rpp/debugging.macros.h>`, and drives the API the way a consumer
does. It compiles only when the toolchain carries modules. Three cases: the logging
macros against the module, `ThrowErr` through the module, and a `strview` that the
module and the header both name.

**What it does not prove.** The file includes `<rpp/tests.h>` for `TestImpl`, which
pulls in `<rpp/debugging.h>`. So a name the module forgets can still resolve through
the header, and the test passes anyway. `tests/module_consumer/` closes that hole
from the other side. `config_module_only.cpp` imports `rpp.config` and includes no
header, and `c_visibility.c` includes `config.h` from C.

**Where the real gate belongs: changeset 3.** `gen_module_exports.py --check`
compares the generated export list against the header's declarations statically. It
needs no special translation unit, it covers every name rather than the ones a test
happens to call, and it cannot be defeated by an include. Build that, and the
compile-time hole above stops mattering.

Two smaller notes for whoever writes more of these tests:

- `<rpp/tests.h>` defines its own one-argument `Assert`, which shadows the
  `Assert(expr, fmt, ...)` of `debugging.macros.h`. Use `AssertExpr` or `DbgAssert`
  in a test file.
- The mixed-mode link property of section 1.1 is worth its own case. `import` in one
  translation unit and `#include` in another, both passing an `rpp::strview` across,
  linked into one binary.

**Estimate: half a day**, once changeset 3 exists.

## 9. Changeset 5: write the modules, in dependency layers

Work the include graph bottom up. A module can only build after every module it
imports exists, and its export list follows its header. The layers below come from
the actual `#include` graph of `src/rpp/*.h`, so changeset 1b can move a header
between layers.

| Layer | Modules | Count |
|---|---|---|
| L0 | **config.types** ✓, **minmax** ✓, **obfuscated_string** ✓, **scope_guard** ✓ | 4 |
| L1 | **bitutils** ✓, **debugging** ✓, **delegate** ✓, **endian** ✓, **future_types** ✓, **math** ✓, **predicates** ✓, **proc_utils** ✓, **sort** ✓, **source_loc** ✓, **strview** ✓, **timepoint** ✓, **traits** ✓, **type_traits** ✓ | 14 |
| L2 | **atomic_timepoint** ✓, **collections** ✓, **sprint** ✓, **stack_trace** ✓, **task** ✓, **threads** ✓, **timer** ✓, **vec** ✓ | 8 |
| L3 | **load_balancer** ✓, **memory_pool** ✓, **mutex** ✓, **paths** ✓, **tests** ✓ | 5 |
| L4 | **atomic_shared_ptr** ✓, **close_sync** ✓, **condition_variable** ✓, **file_io** ✓, **sockets** ✓ | 5 |
| L5 | **binary_stream** ✓, **concurrent_queue** ✓, **semaphore** ✓ | 3 |
| L6 | **binary_serializer** ✓, **thread_pool** ✓ | 2 |
| L7 | **event_loop** ✓, **future** ✓ | 2 |
| L8 | **coroutines** ✓ | 1 |
| groups | **core** ✓, **text** ✓, **numeric** ✓, **time** ✓, **containers** ✓, **io** ✓, **threading** ✓, **testing** ✓ | 8 |
| top | umbrella **rpp** ✓ | 1 |

Every module ships. `BUILD_WITH_MODULES` builds all forty-four plus the nine umbrellas.

The eight groups partition the 44, and `rpp` imports the groups rather than the modules. So a
new module reaches an `import rpp;` consumer only by joining one group, and no module can sit
in two. `gen_module_exports.py --all --check` reports each of those three ways to drift, and
the selftest pins all three. The nine umbrellas cost about 245 ms of the 121 s module build.
`rpp.numeric` carries the math group, because `rpp.math` is already the module for `math.h`.
Excluded: `config.h`
and `log_colors.h` by rule 1 of section 6.3, and `jni_cpp.h` because it is
Android glue. `tests.h` is in, and it is the one header whose macros split into
`tests.macros.h`.

Per module, the loop is: generate the `.cppm`, add it to `RPP_MODULES_SRC`, add
the import preamble to its test, build `RppModuleTests`, fix what the compiler
reports.

Expect the template-heavy headers to be the slow ones: `delegate.h` (48
declarations), `future.h`, `concurrent_queue.h`, `event_loop.h` and
`thread_pool.h`. Watch two things there:

- **Hidden friends.** A friend operator declared inside a class in the global
  module fragment stays attached to the global module, so argument-dependent
  lookup should still find it from an importer. Compilers disagree here. The
  macro-free compile check of section 8 is the detector.
- **Deduction guides and variable templates.** A using-declaration re-exports a
  class template but not its deduction guides. Where a guide matters, restate it
  in the `.cppm`.

**Estimate: L0-L2 is 1 day. L3-L8 is 1.5 days.**

---

## 10. Changeset 6: make other projects able to consume this

**Landed for a mama consumer, PR #65.** mama 0.14.0 collects every `.cppm` under
an exported include dir, so `package()` needs no new call. `mama_target_modules()`
adds them to the consumer target and defines `MAMA_HAS_MODULES`, and the deployed
archive drops the module objects, so a whole-archive link finds no duplicate
initializer. `tests/module_consumer/` builds through mama on gcc, clang and MSVC,
and `run_test.py` compares the module path against the header path.

**Still open for a plain CMake consumer.** `CMakeLists.txt` names the file set on
the target, and it installs no file set, so `find_package(ReCpp)` reaches the
headers alone. Deliverable 2 below is what remains of this changeset.

Three deliverables:

1. ~~**mama.** Export the `.cppm` files and keep the module objects out of the
   exported archive (D6).~~ **Done.** mama collects them on its own, and
   `no_export_modules()` opts out. See the two paragraphs above.

2. **CMake.** Install the file set so a consumer can rebuild the binary module
   interfaces from source:
   ```cmake
   install(TARGETS ReCpp EXPORT ReCppTargets
           FILE_SET CXX_MODULES DESTINATION lib/cxx-modules/rpp)
   install(EXPORT ReCppTargets CXX_MODULES_DIRECTORY cxx-modules
           NAMESPACE ReCpp:: DESTINATION lib/cmake/ReCpp)
   ```
   This needs CMake 3.28 on the consumer, the same floor ReCpp already sets.

3. **A consumer example and a documented contract.** `tests/module_consumer/`
   covers the example half. The contract half remains. State it in README.md:
   - The consumer compiles ReCpp's `.cppm` files. ReCpp ships no binary module
     interface.
   - The consumer must use the same C++ standard level and the same
     configuration macros, in particular `RPP_ENABLE_UNICODE`.
   - Mixing `import rpp.X` and `#include <rpp/X.h>` in one program is supported.
   - Macros need a header (section 6).

Then port one real consumer. `krattcam` and `krattlink` both pull ReCpp through
`add_git`. Convert one file in one of them and measure.

**Estimate: 1.5 days.**

---

## 11. Changeset 7: CI, docs and measurement

1. Both modules jobs are in `.github/workflows/ci.yml`, gcc-14 and clang-21. The
   clang-21 one registers apt.llvm.org itself, because the runner image carries no
   such package. Five CI traps are handled and worth keeping: TSAN needs
   `setarch -R` to start, ninja ignores `jobs=` so a Ninja job needs `taskset`,
   `run_clang_tidy` has to find the compile database under `linux-clang`,
   `clang-scan-deps` ships in `clang-tools-N` and not in `clang-N`, and a modules
   job must pass `BUILD_WITH_MODULES=ON` so a silent AUTO fallback fails it.
   See `BUGS.md` C9 and C11.
2. ~~Wire `check_includes.py self-contained --check` as a gate.~~ **Done.** CI runs
   `selftest`, `self-contained` and `import-order`. `gen_module_exports.py --check`
   joins them with changeset 3.
3. ~~Rewrite the README modules section.~~ **Done.** It now points here.
4. ~~Publish a compile-time measurement.~~ **Done.** Section 2.2 measures one consumer
   translation unit both ways, which is what an importer feels. ReCpp's own build gains
   nothing, because its `.cpp` files keep using headers (D4).

**Estimate: half a day, because only the CI gate of item 2 remains.**

### 11.1 The fatal compiler defects, which block a consumer

Every entry here stops a build. Each has a reproducer in `BUGS.md` and a workaround in the
tree, so nothing is unguarded today. Each workaround costs a consumer something, so retest
all seven whenever the toolchain moves, and delete the workaround which the new compiler
makes unnecessary.

| Bug | Compiler | What dies | What guards it today |
|---|---|---|---|
| **B16** | gcc-14 | An importer of a module whose global module fragment includes `<future>` crashes on `std::promise`, at `propagate_necessity` | Every probe names `cfuture` unevaluated. Ten headers reach `<future>`, nine of them ship modules |
| **B18** | gcc-14 | An importer of `rpp.concurrent_queue` which never includes the header crashes at `-O1` and above, at `nonnull_arg_p` | No test hits it, because each includes `<rpp/tests.h>`. A module-only consumer at `-O1` is exposed and has no target yet |
| **B19** | gcc-14 | A module which exports `std::exception_ptr` writes an interface no importer can read | `rpp-std.cppm` leaves the name out |
| **B20** | gcc-14 | A module which exports `std::swap` after including `<future>` loses the generic `std::swap`, and the interface fails | `rpp-std.cppm` keeps `<future>` out, so `std::future` and `std::promise` stay out too |
| **B21** | gcc-14 | A module which exports `std::get` breaks `std::unique_ptr` in every importer | `rpp-std.cppm` leaves the name out |
| **B22** | gcc-14 | An importer which reaches `std::shared_ptr` through a module fails to link, because no object carries `_Sp_counted_base<_S_atomic>::_M_release()` | `RppStdModuleOnly` names `std::unique_ptr` instead. An importer which needs a shared pointer includes `<memory>` |
| **B8** | gcc-14 | Four separate shapes, each breaking one module | `NO_EXPORT` and `RE_EXPORT` in the generator carry the entries |

Four of the seven are silent traps rather than loud ones. B18 fires only for a shape no test
covers, B19 and B21 compile the interface and fail every consumer afterwards, and B22 reaches
the linker. So a green build here does not prove the next consumer compiles, and item 4 of
section 12 tracks that risk.

B19, B20 and B21 share one shape: an `export using` inside namespace `std` either poisons a
later instantiation or corrupts the interface. `RppStdModuleOnly` is the target which catches
the next one, because it is the only consumer which names no std type in a header.

Two more failures stop the same consumer, and neither is a compiler bug. A declaration in a
global module fragment reaches an importer only when an exported declaration names it:

| What dies | Where it surfaces | What carries it now |
|---|---|---|
| `s != "x"` and `s + "y"` on an exported `std::string` | The importer reports `no match for 'operator!='` | `rpp-std.cppm` exports `operator==`, `operator!=`, `operator+` and `operator<=>` |
| `std::vector` construction | `stl_construct.h` reports `no matching function for call to 'operator new(sizetype, void*)'` | The importer includes `<new>` |

So an export list of type names alone does not make a module usable. Every free function an
exported type needs by argument-dependent lookup belongs in the list beside the type.

---

## 12. Risks

| Risk | Impact | Response |
|---|---|---|
| Changeset 1b breaks a downstream build | krattcam, krattlink or krattgcs fails to compile after a dependency bump | Build one downstream project against the branch before merging 1b. A break there is a latent bug the removal exposed. |
| A `[[clang::coro_return_type]]` reaches a non-coroutine (finding 16) | clang-21 refuses the file, headers and modules alike | Never pass an `rpp::cfuture` through `std::future` or `std::async`. The fix is on this branch. A future case needs `RPP_CORO_WRAPPER`, which `config.h` already defines and nothing used until now. |
| The `test_coroutines.cpp` race (finding 17) | TSAN fires on 2 of 6 parallel runs, and it predates this work | Track it as its own bug. `e.what()` reads a message the future shared state may free on another thread. |
| Compiler divergence on reachability, hidden friends and argument-dependent lookup | A module works on gcc-14 and fails on clang-21 | Build both tier 1 compilers in CI from L0. The macro-free compile check finds it early. |
| A consumer writes its import above its includes | Hundreds of std redefinition errors on gcc-14 (section 2.1) | Document the order in README.md. The error is loud at compile time, so it never reaches a binary. |
| Export lists rot | A new API is invisible to importers, and nobody notices | `gen_module_exports.py --check` in CI. |
| Duplicate module initializer symbol (finding 7) | Link failure in a consumer that compiles the `.cppm` and links `libReCpp.a` | Keep `.cppm` objects out of the shipped archive. Cover it in `examples/module_consumer/`. |
| Sanitizer interaction | `BUILD_WITH_MEM_SAFETY` already disables `/fsanitize=address` on MSVC because of modules ([`CMakeLists.txt:154`](../CMakeLists.txt#L154)) | Keep the modules job separate from the sanitizer matrix. |

---

## 13. Acceptance criteria

1. `tools/check_includes.py self-contained --check` exits 0.
2. `cmake -DBUILD_TESTS=ON -DBUILD_WITH_MODULES=ON` builds `RppTests`, which carries
   the module checks, and it passes every test on gcc-14 and clang-21. Every
   tier 2 compiler still passes the headers-only build.
3. Every one of the 44 headers has a `.cppm`, and `gen_module_exports.py
   --check` reports no difference.
4. `tests/module_consumer/` builds against an installed ReCpp using only
   `import rpp;`, and links.
5. The mixed-mode link check passes.
6. README.md documents the contract of section 10 and carries a measured
   compile-time number.

## 14. Schedule

| Changeset | Work | Days | Blocks | State |
|---|---|---|---|---|
| 1b | remove 3 unused, review 49 redundant and 60 std | 1 | D5 | done, PR #63 |
| 2 | add the rpp-header include check | 0.5 | changeset 3 | done |
| 3 | generate the export lists | 1.5 | changeset 5 | done |
| 4 | dual-mode test harness | 0.5 | changeset 5 | done, the generator --check is the gate |
| 5 | 44 modules plus the umbrella | 2.5 | changeset 6 | 44 of 44 modules, umbrella remains |
| 6 | mama and CMake packaging, consumer example | 1.5 | changeset 7 | mama done, PR #65 |
| 7 | CI, docs, measurement | 0.5 | none | gates done |

**About 5 working days remain.** Changeset 5 is half of that. Every decision in
sections 3 and 6 is settled, and nothing blocks the start.
