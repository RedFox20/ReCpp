# ReCpp Bugs

Open issues first, then closed. An open entry carries enough to start an
investigation, no more.

**A closed entry is exactly two sentences.** The first names the bug. The second
names the fix. Git holds the story, and a longer entry is noise every agent reads.

## Open

### B29. `udp_load_balancer` misses its throughput floor under full-suite load
`test_sockets::udp_load_balancer` asserts the balancer reaches 75 percent of the target rate
over the run. One full-suite run reported 91 KB against a 153 KB floor, at
`test_sockets.cpp:1232`.

The sender loop runs on the calling thread and shares the container with the rest of the
suite. A starved sender sends less, so the floor measures the machine as much as it measures
the balancer.

Measured in this container, on a docs-only tree:

| Run shape | Result |
|---|---|
| full suite, 4 runs | 1 failure |
| `test_sockets` alone, 3 runs | 0 failures |

R10 says to record a timing report rather than patch it on the spot. A `best_of_3` around
the send loop would pin the balancer instead of the load. Issue #70 took that same repair for
`test_concurrent_queue`.

### B28. gcc-14 crashes an importer which reaches `exception_ptr.h` and calls `future::get()`
gcc-14 needs three conditions at once. Remove any one of them and the importer compiles.

1. The module fragment carries `<future>`, which carries `bits/exception_ptr.h`.
2. The importer includes a header which reaches `bits/exception_ptr.h` by text.
3. The importer instantiates `std::future<T>::get()`.

The compiler reports `internal compiler error: Segmentation fault` at
`bits/exception_ptr.h`. This predates the `rpp.future` split, and `import rpp.threading`
reproduced it before that.

**No optimization level escapes it, and the crash is in the front end.** An earlier version
of this entry said the `cddce` pass dies, so `-O0` and `-Og` compile. That belongs to B16 and
not here. These are two crashes, and the measurement separates them:

| Shape | `-O0` and `-Og` | `-O2` | Dies in |
|---|---|---|---|
| B16, the importer includes `<future>` and builds a `std::promise` | compiles | ICE | the optimizer |
| B28, the importer includes `<memory>` and calls `future::get()` | ICE | ICE | the front end |

The B28 trace names the instantiation of `_M_get_result()` under `future::get()`, so the
compiler dies while it merges declarations, not while it optimizes them.

**Condition 3 is `get()`, and neither the factory nor the template matters.** `wait()`
compiles where `get()` crashes. A non-template factory crashes the same way. An earlier
version of this entry blamed the templated factory, and that was wrong.

| Importer body, on a module which exports a future factory | Result |
|---|---|
| imports the module and calls nothing | compiles |
| calls the factory and never reads the result | compiles |
| calls `wait()` | compiles |
| calls `get()` | ICE |

**Condition 2 names one header, and every other one only reaches it.** Beside `<typeinfo>`
and `<new>`, which the fragment requires anyway:

| Added include | Reaches `exception_ptr.h` | Result |
|---|---|---|
| `<vector>`, `<string>`, `<functional>` | no | compiles |
| `<memory>`, `<chrono>`, `<thread>` | yes | ICE |
| `<exception>`, `<stdexcept>` | yes | ICE |

The rule predicts the column on the right. `<string>` and `<functional>` were predicted
from the middle column first, then measured.

**`exception_ptr.h` is the crash site, not the cause.** Condition 1 names `<future>`, and no
other header stands in for it. Measured on the same compiler:

| Shape | Result |
|---|---|
| a `<future>` module, imported and never called | compiles |
| a `<future>` module, `get()` called, no `exception_ptr.h` include | compiles |
| an `<exception>` module, importer includes `<memory>`, `exception_ptr` returned | compiles |
| a `<memory>` module, importer includes `<memory>` | compiles |

So a plain import of this group is safe. The three conditions must meet, and dropping
`<future>` from the fragment drops the crash even when `exception_ptr` stays on both sides.

**The compiler is gcc 14.2.0**, Ubuntu package `14.2.0-4ubuntu2~24.04.1`, from August 2024.

**gcc 14.4 does not close this.** No distribution packages 14.4, because noble, questing and
the `ubuntu-toolchain-r` archive all stop at 14.3. So 14.4.0 was built from source and
measured:

| Compiler | `-O0` | `-Og` | `-O1` | `-O2` |
|---|---|---|---|---|
| gcc 14.2.0 | ICE | ICE | ICE | ICE |
| gcc 14.4.0 | ICE | ICE | ICE | ICE |

That build is sound, because every safe shape above compiles on it. Only this shape crashes.

**`~cfuture()` carries condition 3 on its own.** The destructor calls `get()` to drain a
ready future, so a consumer instantiates `get()` by holding a `cfuture<T>` at all. Naming
the type is enough, and no explicit `get()` call has to appear.

So the restriction bounds one type, not the group. A consumer which names no `cfuture`
imports `rpp.future` beside any include. `RppCoroModuleOnly` pins that on gcc-14. It
includes `<memory>` and drives `event_loop`, `time_awaiter` and `functor_awaiter`.
`RppFutureModuleOnly` holds the other half, where a `cfuture` restricts the include set.

**This is a gcc-14 limit, not a limit of the module.** clang-21 and MSVC build and run
`RppFutureModuleOnly` with no such restriction, and the `RPP_B28_FREE` block in that file
drives the exact shape gcc-14 rejects. So the group works on both other tier 1 compilers,
and it keeps `<future>` out of the eight modules beside it either way.

MSVC needs the opposite. It parses `<thread>` from the fragment and fails without
`<chrono>`, which gcc-14 forbids here, so `future_module_only.cpp` guards that include on
`_MSC_VER`.

**Reduced to libstdc++, with no rpp code.** Report this one upstream. gcc 14.2.0, `-O2`:

```cpp
// m.cppm
module;
#include <future>
export module m;
export template<class T> inline std::future<T> mk(T v)
{ std::promise<T> p; p.set_value(v); return p.get_future(); }

// c.cpp -- g++ -std=c++20 -fmodules-ts -O2 -c c.cpp
#include <memory>
import m;
int main() { return mk(11).get() == 11 ? 0 : 1; }
```

The crash needs the include and the call together. Either one alone compiles.

**One mitigation works, and it moves the cost onto the consumer.** Both sides must consume
the header as a header unit, so one copy reaches the merge instead of two:

```
g++ -fmodule-header=system -xc++-system-header future
g++ -fmodule-header=system -xc++-system-header memory
```

The module then writes `import <future>;` and the consumer writes `import <memory>;`. A
consumer which includes `<memory>` by text still crashes, so this repairs nothing for a
consumer which will not rewrite its own includes.

**Everything else fails.** Each of these still crashes:

| Attempt | Result |
|---|---|
| the importer includes `<future>` itself, in either order | ICE |
| the fragment carries `<exception>` before `<future>` | ICE |
| the fragment also carries `<memory>`, `<chrono>`, `<thread>` and `<stdexcept>` | ICE |
| the module imports `<future>` as a header unit, the importer includes by text | ICE |
| `template class std::promise<int>;` in the module | ICE |
| the module primes the call path for one type | ICE |
| `-fno-tree-dce`, `-fno-tree-builtin-call-dce`, `-fno-module-lazy`, `-fno-inline` | ICE |
| `-fno-lifetime-dse`, `-fno-ipa-icf`, `-fno-devirtualize`, `-fno-strict-aliasing` | ICE |
| one LTO partition | ICE |

**A second gcc-14 defect blocks the obvious repair, and it is upstream PR 108080.** An
`optimize` attribute on an exported template would carry a weaker pass list into the
importer. Writing one crashes the module writer instead, at `cp/module.cc:6334`:

```cpp
export template<class T> __attribute__((optimize("O0"))) inline std::future<T> mk(T v);
```

That crash site is `core_vals`, which PR 108080 reports for `#pragma GCC target` and
`#pragma GCC optimize`. PR 120406 is a duplicate of it. gcc 15.1 carries the fix, and the
fix replaces the crash with `sorry, optimize attribute not supported in modules`. So a
newer gcc diagnoses this repair rather than granting it, and the repair stays unavailable.

gcc-13 does not reach this bug, because it fails the same module without `<memory>`. So
gcc-14 is the oldest gcc which builds this group at all.

A consumer which throws across the boundary needs no `<exception>`. A thrown `int` and a
`catch (int)` cross it, and `future_module_only.cpp` holds that shape.

`RppFutureModuleOnly` drives this group, and it includes no `<exception>` for this reason.
Give it one to watch the build fail.

### B27. A `then()` exception handler reads a string the async state frees
`ubuntu-cpp20-tsan-clang18` reported one race in `test_future::except_handlers_catch_first`.
Thread T107 runs `~invalid_argument` inside the libc++ `std::async` state and calls `free`.
Thread T100 reads the same address through `bcmp`, under the `e.what()` compare of the
handler. All 557 cases passed, and TSAN alone sets exit code 66.

The handler takes `std::domain_error e` by value (`test_future.cpp:143`). libc++ holds the
message in a refcounted buffer which a copy shares, so the copy and the async state point at
one allocation. Whether the refcount is the defect or the report is a false positive needs a
read of `__libcpp_refstring`.

This is not B17. That one names `thread_pool.cpp:359` and `tests.cpp:726` in
`test_threadpool::parallel_task_reentrance`, and neither stack appears here.

Four other TSAN jobs passed on the same commit, which are `cpp20-tsan-gcc13`,
`cpp23-tsan-gcc13`, `cpp23-tsan-clang18` and `cpp26-tsan-gcc14`. Only libc++ reports it.

The job passed on c5a9d9b, the next commit, so this is one sighting and the rate is below one
run. B17 reported on the same commit instead, which is a different race in another test.

### B26. `~event_loop()` can return while a detached worker still holds the loop
`~event_loop()` waits two seconds in `wait_on_all()`, then reports a timeout through
`__assertion_failure`. That macro does not act the same on every platform. On gcc, clang and
an MSVC release build it reaches `RppAssertFail`, which terminates. An MSVC `_DEBUG` build
calls `_CrtDbgReport`, which returns, so the destructor finishes under a live worker. The
owner accepts that one, and the comment states it now instead of promising a graceful exit.

A live worker holds three borrowed things: the loop, the time source and the pool. C30 closed
the `delay()` half. A poll step reads the offset under a reader guard, and
`stop_and_wait_all_ready()` retires the pointer before the owner frees it. Every loop wait
now goes through `wait_next_event()` with a `time_frame`, so no wait hands the raw pointer to
`concurrent_queue` any more. Two gaps stay open.

1. `set_time_source(other_clock)` during a pending `delay()` overwrites the captured offset
   with the offset of the new clock. A retire is safe. A swap re-arms the same stranding.
2. An owner which frees a clock it swapped out for another still reaches freed memory,
   because only a clear to null retires. Clear it before the free.

So the destructor must never return while a task is live. A shared pointer is not the fix,
because it changes the borrow contract of every consumer.

The drain counts every reader, so a steady stream of new readers can hold it up. Over 20000
detach cycles on 4 cores it measured 2 readers at 21.8ms, 4 at 315ms and 8 at 88.8s. A poll
step holds the guard for nanoseconds and then sleeps, so real usage never reaches that shape.

`stop_and_wait_all_ready_retires_the_clock_before_the_owner_frees_it` is a stress reproducer,
not a recipe which fails on demand. Without the drain, ASAN catches the use after free 4 runs
in 10, and three times the cycles only reach 6 in 10. The guarded region is four atomic
operations with nothing a test can block inside, because `AtomicTimeSource::total_offset()` is
a non-virtual read. A deterministic version needs a test callback on a hot path, which costs
every reader a load and a branch.

**A generation flip does not fix it.** Two counts, with a bump on each `set_time_source()`,
send a later reader to the other count. That shape reports a use after free 5 runs out of 12
under ASAN. A reader picks its count before it loads the pointer. So a reader which picked
count `g` and then stalled can hold the pointer an attach stored. The detach after it retires
the other count, reads zero, and lets the caller free the clock. A correct split has to
publish the pointer each reader holds, which is a hazard pointer, not a counter.

**The pool window is measured.** `post_resume_from_suspension()` pushes the resume first and
decrements the count second. The count reaches zero while the worker is still inside a loop
member function. Three probes over 2000 destroy cycles under ASAN answer what that costs:

| Probe | Post-decrement code | Result |
|---|---|---|
| A | a 200us delay, no access | clean |
| B | a 200us delay, then one member read | **heap-use-after-free, at once** |
| C | one member read, no delay | clean, 5 runs out of 5 |

So the owner really does free the loop under the worker, and B proves it. The window is
harmless today only because no awaiter touches the loop after the decrement. Every awaiter
makes `post_resume_from_suspension()` the last statement of its lambda, and `join_forks`
inlines the same two steps in the same order. Nothing enforces that.

C is the part which matters for a fix. The natural window is too narrow to catch a real
violation, so no test can pin this. A structural fix can. Move the decrement out of the
awaiters and into the wrapper `start_in_background()` hands the pool. It then runs after the
task returns, and no awaiter can add code after it. That costs one delegate move per
background task, which is a hot path, so it needs the owner to agree.

`the_background_count_is_a_workers_last_touch_of_the_loop` covers the shutdown path rather
than the invariant. Probe B is what gives it teeth, and probe B needs a hook this repo does
not have. B17 reports a detached task which outlives the suite that started it.

### B22. gcc-14 emits no `_M_release` for a `std::shared_ptr` an importer reaches through a module
The interface compiles and so does the importer. The link then fails:

```cpp
module;
#include <memory>
export module probe;
export namespace std { using std::shared_ptr; using std::make_shared; }
```

```cpp
#include <new>
#include <typeinfo>
import probe;
int main() { auto p = std::make_shared<int>(3); return *p == 3 ? 0 : 1; }
```

`undefined reference to 'std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_release()'`.
That function is an inline explicit specialization in `shared_ptr_base.h`, and gcc emits it
into neither object. `#include <memory>` in the importer fixes it. `std::unique_ptr` and
`std::make_unique` link without the header, so the defect is specific to the shared count.

### B21. gcc-14 loses the real `std::get` in every importer of a module which exports it
The interface compiles. An importer which instantiates `std::unique_ptr` then fails, because
`unique_ptr.h` calls `std::get<0>` and only the exported overload set stays visible:

```cpp
module;
#include <memory>
#include <tuple>
export module probe;
export namespace std { using std::unique_ptr; using std::make_unique; using std::get; }
```

```cpp
#include <new>
import probe;
int main() { std::unique_ptr<int> p = std::make_unique<int>(3); return *p; }
```

The error is `no matching function for call to 'get<0>(std::tuple@probe<int*,
std::default_delete@probe<int> >&)`, so the exported set does not match the module-owned
`std::tuple`. No module exports a std name now, so a consumer includes `<tuple>` and never meets this.

### B20. gcc-14 breaks `std::swap` lookup when the fragment includes `<future>`
The interface does not compile. Four lines reproduce it:

```cpp
module;
#include <future>
export module probe;
export namespace std { using std::swap; }
```

`shared_ptr_base.h:1687` reports `no matching function for call to 'swap(T*&, T*&)'`, so the
generic `std::swap` left overload resolution. Only `<future>` triggers it. The same export
with `<memory>`, `<mutex>` or `<thread>` compiles.

No module exports a std name now, so nothing in ReCpp reaches this. B16 blocks `std::promise`
in an importer anyway.

### B19. gcc-14 writes an unreadable module when it exports `std::exception_ptr`
The interface compiles. Every importer then fails with `failed to read compiled module: Bad
file data`, which is fatal, so nothing downstream builds. Four lines reproduce it:

```cpp
module;
#include <exception>
export module probe;
export namespace std { using std::exception_ptr; }
```

The name is what matters, not the header and not the size of the export list.
`std::exception`, `std::runtime_error`, `std::current_exception`, `std::rethrow_exception`
and `std::make_exception_ptr` all export cleanly from the same file.

No module exports a std name now, so nothing in ReCpp reaches this. A consumer which catches
through a pointer includes `<exception>`.

### B17. A pool worker frees the generic task a test still reads (C18 recurred)
`ubuntu-cpp23-tsan-gcc13` reported one race in `test_threadpool::parallel_task_reentrance`.
A worker calls `free` through `generic.reset()` at `thread_pool.cpp:359`, and the main
thread read the same address in `rpp::test::run_test_func()` at `tests.cpp:726`. All
538 cases passed, and TSAN alone sets exit code 66.

That statement is what C18 closed. It sat at `thread_pool.cpp:351` then, so the line moved
and the code did not. C18 closed it as unreproducible after 120 runs on 32 saturated cores.
This is the first report since, so the rate is far below what the old hunt covered.

The thread which frees comes from an earlier suite. Its creation stack names
`parallel_task_detached` under `test_sockets::test_udp_poll_nonblocking_select`, so a
detached task outlives the suite which started it. Suite shutdown order is the place to
look, not the `catch` block which reports the write.

Four other TSAN jobs pass on the same commit: `cpp20-tsan-gcc13`, `cpp20-tsan-clang18`,
`cpp23-tsan-clang18` and `cpp26-tsan-gcc14`.

Second sighting on fd9c088, and this time it was `ubuntu-cpp20-tsan-gcc13`. Same test, same
two stacks, same two lines. So the race is not specific to one standard, and the job which
reports it moves between runs. A re-run of the same job passed.

Third sighting on 8dc779d, again on `ubuntu-cpp20-tsan-gcc13`, and again the same two
lines. That commit edits three markdown files, so the rate alone moved it, not the code.
All 556 cases passed and TSAN set exit 66 on its own.

Third sighting on bebb416, back on `ubuntu-cpp23-tsan-gcc13`. All 540 cases passed, TSAN
reported one warning, and the four other TSAN jobs passed on the same commit.

Fourth sighting on cab12a8, again on `ubuntu-cpp23-tsan-gcc13`. Same test, same two stacks,
same two lines, and all 553 cases passed. The job passes on the next commit, so the rate is
still far below one run.

Fifth sighting on c5a9d9b, again on `ubuntu-cpp23-tsan-gcc13`. Same test, same two lines, and
the same creation stack under `test_sockets::test_udp_poll_nonblocking_select`. All 557 cases
passed, and three other TSAN jobs passed on the same commit.

### B15. Six headers do not compile on bare metal
`condition_variable.h:62` gives every non-MSVC target a `condition_variable` which
inherits `std::condition_variable`. That base waits on a `std::unique_lock<std::mutex>`
only. `mutex.h:155` makes `rpp::mutex` a `critical_section` on bare metal, so every
`cv.wait(lock)` in `semaphore.h` and `concurrent_queue.h` reports `no matching member
function for call to 'wait'`. `thread_pool.h`, `future.h`, `event_loop.h` and
`coroutines.h` reach one of those two, so they report the same.

The MSVC branch at `condition_variable.h:179` is the one which would work. It is a
hand-rolled `condition_variable` templated on the mutex type. A fix widens the `#if` so
bare metal takes that branch too, and it needs a target which can run the result.

`event_loop.h` carries a second gap of its own. It calls `rpp::get_thread_id()` at lines
445 and 517, and `threads.h:31` declares that name only when `!RPP_BARE_METAL`.

All six headers carry a `NO_CONFIG` entry in `tools/gen_module_exports.py` until then. A
bare-metal build never reaches the module either, so the export list stays unguarded.

### B16. gcc-14 crashes an importer which instantiates `std::promise` at `-O1` and above
A module whose global module fragment includes `<future>` breaks every importer which
instantiates `std::promise`. gcc-14 reports `internal compiler error: in
propagate_necessity, at tree-ssa-dce.cc:1001`, in GIMPLE pass `cddce`.

The crash needs no export. An empty purview is enough.

Measured on gcc 14.2.0. The earlier entry said `-O0` crashes the same as `-O2`, and that
is wrong. The pass which crashes does not run below `-O1`.

| Optimization | Result |
|---|---|
| `-O0`, `-Og` | compiles |
| `-O1`, `-O2`, `-O3`, `-Os` | ICE in `cddce` |

`<future>` is the only trigger. A fragment which includes `<memory>`, `<thread>` or
`<mutex>` instead compiles. No flag avoids it either. `-fno-tree-dce`,
`-fno-tree-builtin-call-dce` and `-fno-module-lazy` each still crash.

```cpp
// m.cppm -- an empty purview is enough
module;
#include <future>
export module m;

// c.cpp -- g++ -std=c++20 -fmodules-ts -O2 -c c.cpp
#include <future>
import m;
int main() { std::promise<int> p; p.set_value(7); return p.get_future().get() == 7 ? 0 : 1; }
```

**Narrowed to one module.** `future_types.h` reached every rpp header and carried `<future>`
into all ten. It needed the include for one concept, and `IsFuture` moved to `future.h`.
`rpp.future` now carries the three headers which reach `<future>`, and the other eight
modules are clean. Measured after the split, each at `-O2`:

| Importer | Result |
|---|---|
| `rpp.threading`, `rpp.testing`, `rpp.io` | compiles and runs |
| `rpp.future` | ICE |

So a consumer meets this defect only when it imports `rpp.future`. Two ways around it.
Import another module, or include `<rpp/future.h>` in the unit which names `std::promise`.

`RppPromiseModuleOnly` in `tests/module_consumer/` is the gate. It imports `rpp.threading`,
instantiates `std::promise` and runs at `-O2`. It failed to build before the split.

`test_modules_future.cpp` imports `rpp.future`, so it keeps naming the factories unevaluated.
Delete that workaround when a newer gcc compiles the reproducer above.

Ten headers reached `<future>` before the split, and `future_types.h` was the only direct
includer. Three reach it now: `future.h`, `event_loop.h` and `coroutines.h`. `future.h`
includes `<future>` itself, which it always needed, and the other two include `future.h`.

No export list removes the crash. Only the fragment which carries `<future>` decides it.

### B2. A test which trusts the clock fails on a loaded machine
Nearly every timing assertion sets its bound just above the delay it measures. A
sanitizer, an emulator, or a busy CI runner erases that margin.
This has three shapes. A bound too tight reports the overrun, as
`test_concurrent_queue::wait_pop_until` did with 219 ms against a 10 ms ceiling. A
sleep used to order two threads reports a wrong result instead, as
`test_close_sync::basic_close_prevention` did on MSVC with
`~ImportantState: data != "aaaabbbbcccc"`, and again on `win64-cpp20-msvc` for #97 at
13fd878, which touched no close_sync code. A third shape compares two measured times, as
`test_threadpool::parallel_for_performance` did on `ubuntu-cpp26-tsan-gcc14` with
`parallel_elapsed => '0.111749' must be less or equal than '0.107670'`. A two core runner
gives a parallel loop no margin over a single thread. AGENTS.md R2 already says to wait on an
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

### B8. gcc-14 breaks a module for five shapes, and each one has a workaround
`tools/gen_module_exports.py` carries `NO_EXPORT` with one entry. Every module ships now.
Delete that entry when a newer gcc reads the module back.

Shapes 2, 3 and 5 all need a re-export, and `RE_EXPORT` is empty, so none can fire today.
Read all three before you add an entry. Shape 4 lives in a header, not in a list.

`NO_CONFIG` is a third list, and it is not a gcc defect. It names the two headers which do
not compile on bare metal, see B15. Any parse failure the list does not name reaches the
caller and fails the run.

The importer stops with `failed to read compiled module cluster N: Bad file data`, then
`failed to load pendings for` a libstdc++ internal. That name changes per run, and the
cluster number does too, so neither one identifies the shape.

Shape 1, in `rpp.sprint`. An export naming a function in the `std::__cxx11` inline namespace
makes the module unreadable. `std::to_string` and `std::stoi` both do it, and `std::swap`
does not. The form does not matter. An `is_detected_v` alias, a plain alias template and a
C++20 concept all fail the same way. So does a concept which calls an unexported helper that
names it. `NO_EXPORT` drops `has_std_to_string`, so `rpp.core` leaves it out today. A header
includer still gets the trait.

Shape 2 reached `rpp.task`, `rpp.tests` and `rpp.file_io` while every rpp include became a
re-export. A re-export makes the `.gcm` unreadable, and only an importer which included
`<string>` first sees it. `import rpp.file_io` alone passed, and `#include <string>` before
it failed. Each half of the module was innocent. Dropping every `export import` fixed it
with the whole export list in place, and dropping every export fixed it with all four
re-exports in place. `rpp.sprint` was the re-export which carried it.

`tests/module_consumer/std_string_module_only.cpp` is the gate. It includes `<string>` and
then imports the six modules which name `std::string`, so this shape fails the build instead
of a downstream consumer. Giving `file_io.h` its old re-exports makes that target report 2
errors.

Shape 3, in the old `rpp.tests`. gcc runs out of imported source locations and stops with
`internal compiler error: in write_location, at cp/module.cc:16271`. It prints
`unable to represent further imported source locations` first. The count is what matters,
not one module. Seven re-exports pass, and `rpp.sprint` as the eighth crashes it, while
`rpp.sprint` alone passes. The dependency `.gcm` files have to come from an `-O2` build to
reach the limit, so a `-O0` bisect hides it. `RE_EXPORT` is empty now, so the count sits at
zero. C28 is the same budget hit from the import side rather than the re-export side.

Shape 4, in `rpp.binary_stream`. A defaulted virtual destructor crashes the importer with
`internal compiler error: Segmentation fault`, and the message names it
`constexpr rpp::stream_source@rpp.binary_stream::~stream_source()`. The importer only has
to name `rpp::binary_buffer`, and a bare `import` passes. No export list reduces it, and
`-O0` crashes the same as `-O2`. The destructor moved out of line, so `binary_stream.cpp`
carries the `= default` and no importer reads one. An empty body in the header also works,
and clang-tidy rejects that one with `modernize-use-equals-default`. An isolated struct of
the same shape does not crash, so the reduced case is still open.

Shape 5, in `rpp.testing`, and it is shape 2 at the scale of a group. `rpp.testing` carried
`export import rpp.text;`, because `TestImpl` expands to a constructor taking an
`rpp::strview`. `#include <string>` and then `import rpp.testing;` alone reported
`failed to read compiled module cluster 1818: Bad file data`. The same module with that one
line removed reads fine, and so does `import rpp.text; import rpp.testing;` after the
removal. `rpp.text` alone, `rpp.io` alone and `rpp.threading` alone all pass after
`<string>`, so no member header carries it. `RE_EXPORT` is empty now, and an importer of
`rpp.testing` names `rpp.text` too.

Reproduce any of these shapes in seconds, outside cmake. Build every `.cppm` in the
`RPP_MODULES_SRC` order into one `gcm.cache`, then compile a consumer:
```bash
cd $(mktemp -d)
for f in $(sed -n '/set(RPP_MODULES_SRC/,/^    )/p' ~/ReCpp/CMakeLists.txt | grep -o 'src/rpp/rpp-[a-z_]*\.cppm'); do
  g++ -std=c++20 -fmodules-ts -I ~/ReCpp/src -c -x c++ ~/ReCpp/$f -o $(basename $f .cppm).o
done
printf '#include <rpp/tests.h>\nimport rpp.task;\nint main(){return 0;}\n' > u.cpp
g++ -std=c++20 -fmodules-ts -I ~/ReCpp/src -c u.cpp -o u.o    # 0 errors under RE_EXPORT
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

### C30. `set_time_source()` wrote a plain pointer a `delay()` worker still read (was B23)
A poll step re-read the raw pointer, so a detach dropped the warp offset and left the worker
waiting on a virtual deadline. A waiter now captures the offset once and refreshes it under a
reader guard, which `delay_survives_a_detached_time_source` and
`stop_and_wait_all_ready_retires_the_clock_before_the_owner_frees_it` pin.

### C29. `delegate::copy` leaked the destination functor when the source was a function
The function branch of `copy()` overwrote `f` and `obj` and never freed the functor the
destination owned. It calls `to.reset()` first now, which `copy_assign_from_function_frees_the_old_functor` pins.


### B24. gcc-14 wrote an `rpp.std` on C++23 which no importer could read
The module compiled, every importer then stopped with `failed to read compiled module: Bad
file data`, and the same source passed on C++20. A unit which carries `<memory>` beside the
container headers is the shape gcc cannot write, and no module exports a std name now.

### B25. A C++23 consumer of the whole module graph broke on `std::packaged_task`
The consumer build stopped with `conflicting declaration of template 'std::packaged_task@
rpp.testing'`, which the C28 source location budget causes through the umbrella. The umbrella
is gone now, and a file names each group it uses.

### C28. gcc-14 ran out of module source locations on 44 modules (was B23)
A clean C++23 build reported `unable to represent further imported source locations` six
times, then failed with `conflicting global module declaration` in three modules. Eight header
groups cut one translation unit from 39 imports to 8, and `ubuntu-cpp23-modules-gcc14` now
covers that standard.

### C27. gcc-14 crashed any importer which built a concurrent queue at `-O1` (was B18)
gcc attached its own builtin `memmove` to `rpp.concurrent_queue`, so `nonnull_arg_p` crashed
every importer which built a queue, through `rpp.threading` and `rpp` too. `__builtin_memmove`
names no declaration to attach, and `RppQueueModuleOnly` builds that shape at `-O2`.

### C26. `pool_types_constructor::allocate<T>()` is unreachable, and that is intended (was B14)
Each pool declares `allocate(int size, int align)`, which hides the template of the mixin and
leaves `construct<T>()` and the array forms reachable. The mixin is internal, so `NO_EXPORT`
drops it from `rpp.memory_pool`.

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
