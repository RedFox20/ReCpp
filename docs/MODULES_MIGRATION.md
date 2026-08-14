# ReCpp C++20 Modules Migration Plan

Revision 3. Two modules exist: `rpp.strview` and `rpp.debugging`.

This document explains the pattern, records what real builds prove about it, and
gives the phased plan for the remaining 41 headers.

## Handover state

**Landed on `claude/cpp-modules-migration-3d1pu3`, PR #57.**

| Item | State |
|---|---|
| `rpp.strview`, `rpp.debugging` | build and pass on gcc-14, clang-21, MSVC 14.44 |
| `debugging.macros.h` | split out, 50 preprocessed lines against 32893 |
| `BUILD_WITH_MODULES=AUTO` | on per toolchain, GCC 14 / Clang 21 / MSVC 19.34 |
| Include-order style rule | in CLAUDE.md |
| `tools/check_includes.py` | 3 checks, 0 self-contained failures left |
| `tests/test_modules.cpp` | module consumer test, 3 cases |
| CI | green, all 24 jobs. 5 causes fixed, see `BUGS.md` C9 |

**Not started:** changesets 1 through 7 below. Changeset 1a is the next action and
blocks nothing else.

**Open questions carried into the next session:** `BUGS.md` B1 through B5. B1 and
B2 are the two that can bite this work.

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
| 4 | **53 of 99 source files** rely on a transitive std include, and that is safe. | Scan over 8 std facilities. The earlier claim, that each one breaks when its provider chain becomes an `import`, is **disproved**. A facade includes its header in the global module fragment, which keeps those declarations reachable to an importer. Negative control on GCC 14.2: delete `#include <string>` from `tests/test_modules.cpp` and keep `import rpp.strview;`, and the modules build still passes. Row 2 is a different failure, an `import` inside a header, which CLAUDE.md now forbids. |
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
would then have to switch at the same time, and C++17 support would end. A
program that mixes an `#include` and an `import` would violate the
one-definition rule.

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

### D5. Each module re-exports its header's rpp dependencies.

`sprint.h` includes `strview.h`, so `rpp.sprint` must give the importer
`rpp::strview`. Otherwise `import rpp.sprint;` returns a `string_buffer` the
caller cannot feed a `strview` into. Findings 5 and 6 confirm both directions.

The rule: **for every `#include "X.h"` in `Y.h`, `rpp-Y.cppm` gets one
`export import rpp.X;`.** The module graph then matches the include graph, and a
script can check it. Changeset 1 is what makes that include graph honest.

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
The rule in CLAUDE.md still holds for every file, because a portable file has to
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
`C7684 module name 'rpp.strview' has an ambiguous resolution to IFC`. The
module-only consumer checks live inside `RppTests` for that reason, not in a target
of their own.

---

## 4. Changeset 1: include hygiene

This lands first, on its own, before any module work. It is worth doing even if
the module migration never happens. [`tools/check_includes.py`](../tools/check_includes.py)
measures three defects and gates each one in CI.

```bash
tools/check_includes.py all              # report
tools/check_includes.py all --check      # CI gate, exit 1 on a finding
```

### 4.1 What the tool measures

| Check | How it works | Findings today |
|---|---|---|
| `self-contained` | compiles each header alone, twice, with nothing before it | **0 left**: `traits.h` used `std::tuple` without `<tuple>`, and this branch fixes it |
| `missing` | a file uses a std facility it does not include itself | **52 of 99 files** |
| `unused` | comment out one include, the header still compiles, **and** the header names nothing the include declares | **10** |
| `redundant` | same, but the header does name something the include declares, so a sibling include leaks it | **43** |
| `std` | same, for a std header, where the scan cannot enumerate the declared names | **59** |

The `unused` and `redundant` split is the important part. Both compile without
the line. Only the first is safe to delete. Removing a `redundant` line trades a
direct dependency for a hidden one, which is the opposite of hygiene.

### 4.2 The 10 unused includes

Every one of these compiles away, and the including header names nothing from
it. Three were checked by hand and confirmed.

| Header | Unused include | Note |
|---|---|---|
| `sockets.h` | `load_balancer.h` | carries the comment `// rpp::load_balancer`, and no use |
| `thread_pool.h` | `threads.h` | no thread-naming call left in the header |
| `sprint.h` | `debugging.h` | |
| `debugging.h` | `log_colors.h` | debugging.h names no color macro |
| `condition_variable.h`, `coroutines.h`, `event_loop.h` | `timer.h` | three copies of the same stale include |
| `atomic_shared_ptr.h`, `bitutils.h`, `traits.h` | `config.h` | |

### 4.3 Stage it in two commits

**The removals are approved.** ReCpp breaks the accidental include chain for its
consumers. Those includes are old backward-compatibility additions, and no
header needs them. A consumer that breaks was already depending on something
ReCpp never promised, and the fix belongs in the consumer.

**Commit 1a, additions only.** Add the 52 missing std includes. This breaks
nothing, downstream or in-repo. Land it and turn on the `self-contained` and
`missing` gates. The `<tuple>` in `traits.h` and the three includes that fixed
the modules build are already on this branch.

**Commit 1b, removals.** Order the work so the risk falls, not rises:

1. Delete the 10 unused includes. They are dead weight, and no header names
   anything from them.
2. Work the 43 redundant lines. Each one needs the missing direct include added
   in the same commit, so the count usually stays the same and the dependency
   becomes honest.
3. Leave the 59 std candidates last. Some are platform-conditional
   (`byteswap.h`, `malloc.h`, `sanitizer/tsan_interface.h`, `QString`), and a
   removal that passes on Linux can break Windows or Android. Build every CI
   platform before you delete one of these.

Tell the downstream teams what landed. `debugging.h` has 15 direct includers
inside this repo, and `config.h` has 27, so the reach outside is larger. A
one-line note in the release text saves each team the bisect.

**Estimate: 1a is half a day. 1b is 1 day.**

---

## 5. Changeset 2: the missing-include gate goes wider

The `missing` check knows 8 std facilities. Extend it in two ways before the
module work starts:

1. Add the std facilities the module builds report as the migration proceeds.
2. Add the same check for **rpp headers**: a header that names `rpp::strview`
   and does not include `strview.h` has the same defect. That check turns the
   43 redundant findings into an exact list of the includes to add.

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
| `config.h` | **51** | `RPPAPI`, `FINLINE`, `NOINLINE`, `NODISCARD`, `RPP_ENABLE_UNICODE`, `RPP_HAS_CXX17`, and the platform probes |
| `tests.h` | **10** | `TestImpl`, `TestCase`, `TestInit`, `AssertThat`, `AssertEqual`, `AssertThrows`, ... |
| `endian.h` | **9** | `RPP_BYTESWAP16/32/64`, `RPP_TO_BIG*`, `RPP_TO_LITTLE*` |
| `debugging.h` | **4** | `LogInfo`, `LogWarning`, `LogError`, `Assert` |
| `future_types.h` | 2 | `RPP_CORO_STD`, `RPP_HAS_COROUTINES` |
| `mutex.h` | 2 | `RPP_HAS_CRITICAL_SECTION_MUTEX`, `RPP_SYNC_T` |
| `close_sync.h`, `minmax.h`, `obfuscated_string.h`, `scope_guard.h`, `strview.h` | 1 each | `try_lock_or_return`, `RPP_SSE_INTRINSICS`, `make_obfuscated`, `scope_guard`, `RPP_CONSTEXPR_STRLEN` |

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
| `config.h` | **no** | **no** | Rule 1. Every rpp header includes it to drive the compiler feature probes, so a consumer already has it. Its integer aliases (`rpp::byte`, `rpp::uint`, `rpp::int64`) reach importers anyway, because every module that includes `config.h` re-exports them. `rpp.strview` does this today. |
| `debugging.h` | yes | **yes, and it pays** | The split is done and measured. `debugging.macros.h` costs 50 preprocessed lines, `debugging.h` costs 32893. Section 6.4 has the numbers. |
| `endian.h` | yes | **no** | The 9 byte-swap macros would split cleanly into compiler builtins, but 9 macros do not pay for a new header and a new name to remember. |
| `tests.h` | **yes** | **yes** | The only clear win. 41 macros against 45 declarations, so the module carries real weight, and `tests_macros.h` needs no include of its own. |

Net effect on the module count: `config.h` leaves and `tests.h` joins, so the
total holds at 43.

**`tests.h`, the one split.** `TestImpl` expands to a class that derives from
`rpp::test`, so the type has to be visible where the macro expands. The consumer
writes:

```cpp
#include <rpp/tests_macros.h>   // TestImpl, TestCase, AssertThat: no includes of its own
import rpp.tests;               // rpp::test and the rest of the framework
```

`tests_macros.h` includes nothing and declares nothing. It only needs the names
that `import rpp.tests;` already brings, and macro expansion happens later, at
the use site.

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

The tool:

1. Reads the compile flags for the header from `compile_commands.json`.
2. Parses the header and keeps every top-level declaration in namespace `rpp`
   and its nested namespaces, whose source location is that header.
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
the header, and the test passes anyway. An earlier revision kept a separate
translation unit that included no rpp header to close that hole. That file was an
outlier, and it is gone.

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
`export import`s exists. The layers below come from the actual `#include` graph
of `src/rpp/*.h`, so changeset 1 can move a header between layers.

| Layer | Modules | Count |
|---|---|---|
| L0 | minmax, obfuscated_string, scope_guard | 3 |
| L1 | bitutils, **debugging** ✓, delegate, endian, future_types, math, predicates, proc_utils, sort, source_loc, **strview** ✓, timepoint, traits, type_traits | 14 |
| L2 | atomic_timepoint, collections, sprint, stack_trace, task, threads, timer, vec | 8 |
| L3 | load_balancer, memory_pool, mutex, paths, tests | 5 |
| L4 | atomic_shared_ptr, close_sync, condition_variable, file_io, sockets | 5 |
| L5 | binary_stream, concurrent_queue, semaphore | 3 |
| L6 | binary_serializer, thread_pool | 2 |
| L7 | event_loop, future | 2 |
| L8 | coroutines | 1 |
| top | umbrella `rpp` | 1 |

43 modules and one umbrella. `rpp.strview` and `rpp.debugging` exist, so 41 remain. Excluded:
`config.h` and `log_colors.h` by rule 1 of section 6.3, and `jni_cpp.h` because
it is Android glue. `tests.h` is in, and it is the one header whose macros split
into `tests_macros.h`.

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
   install(EXPORT ReCppTargets CXX_MODULES_DIRECTORY cxx-modules
           NAMESPACE ReCpp:: DESTINATION lib/cmake/ReCpp)
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
   - Macros need a header (section 6).

Then port one real consumer. `krattcam` and `krattlink` both pull ReCpp through
`add_git`. Convert one file in one of them and measure.

**Estimate: 1.5 days.**

---

## 11. Changeset 7: CI, docs and measurement

1. Both modules jobs are in `.circleci/config.yml`, gcc-14 and clang-21. The
   clang-21 one registers apt.llvm.org itself, because the CI image carries no
   such package. Five CI traps are handled and worth keeping: TSAN needs
   `setarch -R` to start, ninja ignores `jobs=` so a Ninja job needs `taskset`,
   `run_clang_tidy` has to find the compile database under `linux-clang`,
   `clang-scan-deps` ships in `clang-tools-N` and not in `clang-N`, and a modules
   job must pass `BUILD_WITH_MODULES=ON` so a silent AUTO fallback fails it.
   See `BUGS.md` C9 and C11.
2. Wire `tools/check_includes.py --check` and
   `tools/gen_module_exports.py --check` as gates.
3. Rewrite the README modules section. It is already stale: it names a test
   suite `test_strview_module` that does not exist.
4. Publish a compile-time measurement from `examples/module_consumer/`, not from
   ReCpp's own build. ReCpp's own build gains nothing, because its `.cpp` files
   keep using headers (D4).

**Estimate: 1 day.**

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

1. `tools/check_includes.py all --check` exits 0.
2. `cmake -DBUILD_TESTS=ON -DBUILD_WITH_MODULES=ON` builds `RppTests` and
   `RppModuleTests`, and both pass every test, on gcc-14 and clang-21. Every
   tier 2 compiler still passes the headers-only build.
3. Every one of the 43 headers has a `.cppm`, and `gen_module_exports.py
   --check` reports no difference.
4. `examples/module_consumer/` builds against an installed ReCpp using only
   `import rpp;`, and links.
5. The mixed-mode link check passes.
6. README.md documents the contract of section 10 and carries a measured
   compile-time number.

## 14. Schedule

| Changeset | Work | Days | Blocks |
|---|---|---|---|
| 1a | add 52 missing includes, turn on 2 gates | 0.5 | everything |
| 1b | remove 10 unused, review 43 redundant and 59 std | 1 | D5 |
| 2 | widen the missing-include check to rpp headers | 0.5 | changeset 3 |
| 3 | generate the export lists | 1.5 | changeset 5 |
| 4 | dual-mode test harness | 0.5 | changeset 5 |
| 5 | 42 modules plus the umbrella | 2.5 | changeset 6 |
| 6 | mama and CMake packaging, consumer example | 1.5 | changeset 7 |
| 7 | CI, docs, measurement | 1 | none |

**Total: about 9 working days.** Changesets 1 and 2 are two of them, and they
stand on their own value even if the module work stops. Every decision in
sections 3 and 6 is settled, so changeset 1a can start now.
