# ReCpp CI Migration Plan: CircleCI to GitHub Actions

Revision 1. Nothing has moved yet. This document is the plan, not a record.

## 1. Why

Visual Studio 2026 runs on no CircleCI machine runner. The project wants MSVC 14.5x and
drops every older MSVC, because a compiler upgrade is expensive and rare. GitHub Actions
carries `windows-2025-vs2026`, which is generally available since 2026-05-07.

The MSVC toolset is the trigger. The migration also removes a second CI vendor, and it puts
the build matrix beside the code in the same repository.

**One rule for the whole migration: no job changes what it builds.** A job moves to a new
runner and keeps its compiler, its standard, its sanitizer and its commands. MSVC is the
single exception, and section 6 names the risk.

## 2. What exists today

`.circleci/config.yml` is 469 lines. It defines 6 job templates and starts 29 jobs.

| Template | Executor | Jobs | What it does |
|---|---|---|---|
| `ubuntu-build` | docker `cimg/python:3.14` | 19 | compiler and sanitizer matrix, and the header gates |
| `android-build` | docker `cimg/android:*-ndk` | 5 | NDK r25b, r27, r28b, r29, and one Ninja run |
| `consumer-integration` | docker `cimg/python:3.14` | 2 | builds ReCpp as a dependency, not as the root |
| `consumer-integration-msvc` | `win/default` | 1 | the same, on Windows |
| `win64-cpp20-msvc2022` | `win/default` | 1 | the Windows build and test |
| `mipsel-cpp20-gcc12` | docker `cimg/python:3.14` | 1 | a cross build with no tests |

The 29 jobs, by group:

| Group | Count | Names |
|---|---|---|
| clang-18 | 6 | `ubuntu-cpp{20,23}-{asan,clang-tidy,tsan}-clang18` |
| gcc-13 | 6 | `ubuntu-cpp{20,23}-{asan,clang-tidy,tsan}-gcc13` |
| gcc-14 | 3 | `ubuntu-cpp26-{asan,clang-tidy,tsan}-gcc14` |
| clang-20 | 2 | `ubuntu-cpp{20,23}-asan-clang20` |
| modules | 2 | `ubuntu-cpp20-modules-gcc14`, `ubuntu-cpp20-modules-clang21` |
| consumer | 3 | `consumer-integration`, `consumer-clang21`, `consumer-msvc2022` |
| windows | 1 | `win64-cpp20-msvc2022` |
| cross | 1 | `mipsel-cpp20-gcc12` |
| android | 5 | `android-cpp20-r{25b,27,28b,29}-clang-tidy-clang*`, `android-cpp20-r29-ninja` |

Two details carry the build and must survive the move:

1. **`ubuntu-cpp20-modules-gcc14` alone sets `check_includes: true`.** That job runs all seven
   gates: the `check_includes` selftest, self-contained, import-order and rpp-includes, plus
   `rpp_decls.py selftest` and both `gen_module_exports.py` modes. Losing that flag silently
   removes every gate from CI.
2. **`NO_NINJA=1` is the default.** Ninja reads the host core count, not the container quota,
   so it starts 36 jobs on 3 cores and the machine runs out of memory. Only a modules build
   passes an empty string, because CMake scans module dependencies under Ninja alone.

## 3. Target shape

```
.github/workflows/ci.yml          the matrix and the job list
.github/actions/build-and-test/   a composite action: install mama, configure, build, test
```

A composite action replaces the CircleCI parameterized job template. GitHub Actions has no
job-level parameters, and `workflow_call` adds a second file for one caller. A composite
action keeps the shared steps in one place and takes the same inputs the template takes now.

Runner mapping:

| Today | On GitHub Actions | Note |
|---|---|---|
| docker `cimg/python:3.14` | `ubuntu-24.04` | 4 CPU and 16 GB, against 3 CPU and 6 GB now |
| docker `cimg/android:*-ndk` | `ubuntu-24.04` plus an NDK setup step | pin each NDK version, as the config does now |
| `win/default` (VS2022) | **`windows-2025-vs2026`** | the reason for the migration |
| MIPS cross | `ubuntu-24.04` plus `apt install g++-mipsel-linux-gnu` | unchanged commands |

Pin `windows-2025-vs2026` by name. Do not use `windows-latest`. The label already points at
Visual Studio 2026, and an explicit pin keeps the toolset stable when the label moves again.

## 4. Phases

Each phase ends in a state a reader can verify. Do not start a phase before the one above
it passes.

### Phase 1: one Linux job, end to end
Write the composite action and one job, `ubuntu-cpp20-asan-gcc13`. Prove that mama installs,
that the build runs, and that the test result reaches the GitHub Actions summary. Keep
CircleCI running the whole matrix.

**Verify:** the new job passes, and its test count matches the CircleCI job of the same name.

### Phase 2: the Linux matrix
Expand to all 19 `ubuntu-build` jobs with `strategy.matrix`, plus the 2 consumer Linux jobs
and the MIPS cross build. Set `fail-fast: false`, so one red job never cancels the rest.
Carry `check_includes` on `ubuntu-cpp20-modules-gcc14` and confirm all seven gates run.

**Verify:** 22 jobs pass, and the gate job prints all seven gate lines.

### Phase 3: Windows on VS2026
Move `win64-cpp20-msvc2022` and `consumer-msvc2022` to `windows-2025-vs2026`, and rename both
to drop `2022`. Add a step that prints the toolset version, so the log names the compiler that
built the run.

**Verify:** both jobs pass, and the log shows MSVC 14.5x. Section 6 covers a failure here.

### Phase 4: Android
Move the 5 NDK jobs. Pin the same NDK versions the config names now.

**Verify:** 29 jobs pass on GitHub Actions.

### Phase 5: cut over
Delete `.circleci/config.yml`. Point branch protection at the new check names.

**Verify:** a pull request shows the GitHub Actions checks as required, and no CircleCI check.

## 5. What the repository owner must do

These three steps need repository admin rights, and no agent can do them.

1. **Update branch protection.** Every check name changes. Until this happens, a pull request
   shows the old required checks as missing and never becomes mergeable.
2. **Turn off the CircleCI GitHub integration** after phase 5, so it stops posting statuses.
3. **Confirm the Actions minutes budget.** The repository is public, so hosted runners cost
   nothing. A move to private later makes Windows minutes cost ten times a Linux minute.

## 6. Risks

| Risk | Effect | What to do |
|---|---|---|
| MSVC 14.5x rejects code MSVC 14.44 accepted | phase 3 fails | expected, and it is the point. Fix the code, do not pin an older toolset |
| The gate flag does not survive the matrix rewrite | every gate stops running, and CI stays green | phase 2 checks the seven gate lines by name |
| Ninja reads the runner core count | out of memory, as on CircleCI | keep `NO_NINJA=1`, and change it in a separate commit |
| Check names change | pull requests block on missing required checks | do section 5 step 1 during phase 5, not after |
| A test that passes on 3 cores fails on 4 | a timing test turns flaky | run phase 2 three times before phase 3 |

## 7. A question the current CI already answered

`_obfuscated` uses a class type as a non-type template parameter, which is standard C++20.
GCC 14, Clang 18 and MSVC 14.44 all compile it, so `win64-cpp20-msvc2022` passes today. The
code needs no guard, and phase 3 moves the toolset forward from a green build.
