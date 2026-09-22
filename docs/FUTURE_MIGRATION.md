# rpp::future, a module future which owns its own state

`rpp::cfuture<T>` derives from `std::future<T>`. That one line sets the cost of most
lines below it.

This plan adds `rpp::future<T>` and `rpp::promise<T>`, which own their shared state and
name no std type. The module exports those two. `rpp::cfuture<T>` and `rpp::cpromise<T>`
stay in the header, and they keep every behavior they have today.

**No existing consumer changes.** A project which includes `rpp/future.h` keeps `cfuture`
and compiles as before. A project which imports `rpp.future` gets the new type.

---

## 1. Why the derivation costs so much

| Force | What it costs today |
|---|---|
| gcc-14 crashes an importer which names a `cfuture`, see `BUGS.md` B28 | the module path binds the include set of every consumer |
| `<future>` sits in the module fragment | one group carries the whole threading stack |
| `std::future` owns the shared state | ReCpp cannot change the wait, the allocation or the exception path |

The third one is the quiet cost. `cfuture::wait_for(rpp::Duration)` converts a
`std::future_status` back into an `rpp::wait_result`, and `await_ready()` asks the std
future for a zero timeout. Every such line pays for a state ReCpp does not own.

The first one is the loud cost, and it is what makes this plan urgent. B28 needs three
conditions at once, and the first is a module fragment which carries `<future>`. Remove
that one and gcc-14 stops crashing. `future.h:15` is the only direct include of `<future>`
in the three headers, so one split removes condition 1 for the whole group.

## 2. Why two types and not one rename

A module purview which defines `rpp::cfuture` under the name the header already uses
declares two different entities with one name. A binary which links a module consumer
beside a header consumer then breaks the one definition rule. No compiler diagnoses that,
and the failure appears as a corrupt object at runtime.

So the module type takes its own name. `rpp::future` and `rpp::promise` are those names.

## 3. What `rpp::future` drops

Each row is a std behavior which no longer exists, not one which moved.

| Dropped | Why |
|---|---|
| `std::future_status` | `wait_for` and `wait_until` return `rpp::wait_result` alone |
| `share()` and `std::shared_future` | one owner, one result, no second type |
| the `std::future<T>` constructor and assignment | the adopting constructor is what pulls `<future>` back in |
| `std::promise<T>` as the producer | `rpp::promise<T>` replaces it, and `set_value_at_thread_exit` goes |
| `std::async` and `std::launch` interop | `rpp::async_task` is the only launcher |
| the implicit `<future>` include | the point of the whole change |

A consumer which needs any of these keeps `cfuture` and keeps including the header.

## 4. What it keeps

The names below read the same on both types, so a mechanical port compiles.

- `get()`, `wait()`, `valid()`
- `wait_for(rpp::Duration)` and `wait_until(rpp::TimePoint)`, both returning `rpp::wait_result`
- `await_ready()`, `collect_ready()`, `collect_wait()`
- `then()`, `continue_with()`, `detach()`, `chain_async()`, each with the four exception handler arities
- the destructor which drains a ready result and terminates on an unawaited one
- `co_await`, through the same operator set

`rpp::task<T>` does not overlap this. A task resumes on the loop thread and spawns nothing.
A future blocks a thread and carries `.then()`. See `task.h`.

## 5. The ReCpp surface which moves

Twenty files in this repository name `cfuture`, `cpromise`, `std::future` or `std::promise`.
Eight are source and twelve are tests.

| File | Names | Work |
|---|---|---|
| `future.h` | 107 | the new type lands beside `cfuture`, in a new header |
| `event_loop.h` | 11 | `pump_until_ready`, `run_until_ready`, `future_awaiter`, the `run_async` dispatch |
| `coroutines.h` | 11 | `operator co_await` for the new type, and `std_future_awaiter` moves |
| `task.h`, `thread_pool.h` | 8 | doc comments only |
| `future_types.h` | 1 | a second forward declaration |
| `rpp-future.cppm` | generated | the export list names the new type and drops `cfuture` |

### 5.1 The header split

`future.h:15` is the only direct `#include <future>` among the three. A new header holds
the new type and includes none:

```
rpp/async.h        rpp::future, rpp::promise, rpp::async_task, no <future>
rpp/future.h       rpp::cfuture, rpp::cpromise, the std interop, includes <future>
```

`event_loop.h` and `coroutines.h` then include `rpp/async.h`, and `rpp-future.cppm`
carries `rpp/async.h` in its fragment. The group fragment holds no `<future>` after that,
which is what removes B28 condition 1.

**One question stays open.** `event_loop.h` declares `future_awaiter(std::future<T>&&)`
and `coroutines.h` declares `std_future_awaiter` plus two `operator co_await` overloads
for `std::future`. Those must leave the module fragment, and an includer of `event_loop.h`
alone must keep them. Either `future.h` includes `event_loop.h` and declares the std
overloads after it, or a guard keeps them out of the module build. Measure both before
changeset 2.

## 6. The consumer migration, by shape

A survey of six downstream projects found 37 sites which name `rpp::cfuture` or a std
future beside it. Each site falls into one of three shapes.

| Shape | Sites | What it looks like | Cost |
|---|---|---|---|
| mechanical | 31 | names `cfuture<T>` as a return type or a local, then calls `get()`, `then()` or `wait_for()` | a rename |
| contained | 5 | converts between `std::future` and `cfuture`, or stores a `std::future` member | one file |
| invasive | 1 | hands a `cfuture` to an API which expects a `std::future` | a component boundary |

The mechanical majority is the reason this plan is worth running. A project ports its own
files at its own pace, because the header keeps working the whole time.

**The adopting constructor is what the survey had to find.** `cfuture(std::future<T>&&)`
makes every conversion implicit, so a site which relies on it reads like a plain `cfuture`
site. The five contained sites are the ones which rely on it. Each one needs a read before
anybody renames it.

## 7. The changesets

Each one lands on its own and leaves the tree green.

1. **`rpp::future` and `rpp::promise` land in `rpp/async.h`**, with their own tests. The
   module exports nothing new, so no consumer sees them. This is the largest changeset,
   because the shared state, the exception path and the destructor are all new code.
2. **`event_loop` and `coroutines` gain the overload set for the new type**, and the std
   interop moves per the open question in 5.1. The header consumer keeps every name.
3. **The module exports the new names and stops exporting `cfuture`.** A consumer target
   in `tests/module_consumer/` builds the new type on gcc-14, which measures whether B28
   condition 1 is really gone.
4. **A downstream project ports its own files**, one at a time, with the header still
   available for the files it has not reached.
5. **The header keeps `cfuture` with no end date.** A removal needs its own decision, and
   this plan does not ask for one.

Changeset 3 is the one which pays. Until it lands, the module still carries `<future>` and
gcc-14 still crashes an importer which names a `cfuture`.

## 8. The compiler floor

gcc-15 drives this work. `RPP_MODULES_MIN_GCC` is 15 already, and
`RPP_MODULES_ALLOW_GCC14` opens the door for a project which keeps to the B28 include set.

Changeset 3 is the test of whether that door can open wider. If the group fragment carries
no `<future>`, then a gcc-14 importer of `rpp.future` meets no B28 condition, and the
include set restriction goes. That is a measurement, not a promise. Nothing has run it.

`MODULES_MIGRATION.md` section 11.2 holds the other open follow-up, which is `import std;`
in place of the std includes a modules build still writes. That one also needs gcc-15.

## 9. Acceptance criteria

1. `rpp::future<T>` passes the `test_future.cpp` case set, ported name for name.
2. `rpp/async.h` includes no `<future>`, which `tools/check_includes.py` reports.
3. A module consumer target builds `rpp::future` on gcc-14 with `<memory>` live, and it
   compiles. That is the B28 measurement.
4. `rpp.future` exports `rpp::future` and `rpp::promise`, and it exports no `cfuture`.
5. The header path still builds `cfuture` and passes every case it passes today.
6. One downstream project builds against the branch before changeset 3 merges.
