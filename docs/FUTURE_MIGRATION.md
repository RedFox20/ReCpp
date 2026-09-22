# rpp::future, a module future which owns its own state

`rpp::cfuture<T>` derives from `std::future<T>`. That one line sets the cost of most
lines below it.

This plan adds `rpp::future<T>` and `rpp::promise<T>`, which own their shared state and
name no std type. The module exports those two. `rpp::cfuture<T>` and `rpp::cpromise<T>`
stay in the header, and they keep every behavior they have today.

**No existing consumer changes.** A project which includes `rpp/future.h` keeps `cfuture`
and compiles as before. A project which imports `rpp.future` gets the new type.

**A consumer does move.** `cfuture` takes a `[[deprecated]]` attribute in the last
changeset, so every remaining site names itself in the compiler output. See section 6.1.

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
| `share()` and `std::shared_future` | one owner, one result, and no shared type ever |
| the `std::future<T>` constructor and assignment | the adopting constructor is what pulls `<future>` back in |
| `std::promise<T>` as the producer | `rpp::promise<T>` replaces it, and `set_value_at_thread_exit` goes |
| `std::async` and `std::launch` interop | `rpp::async` is the only launcher |
| the implicit `<future>` include | the point of the whole change |

**`share()` never arrives, and nothing asks for it.** A grep over `src` and `tests` finds no
`.share()` call and no `std::shared_future`. A shared future needs a refcounted state and a
second type, which is the cost this plan exists to avoid. A consumer which wants one wraps
the result itself.

A consumer which needs any other row keeps `cfuture` and keeps including the header.

### 3.1 The launcher moves with the type

`rpp::async_task` returns `cfuture<T>`, so the module cannot redefine it. A return type alone
does not overload a function template. Two definitions under one name then break the one
definition rule, exactly as the type does.

So the new launcher takes its own name. `rpp::async` is free in `src/rpp`, and it pairs with
`rpp::future` the way `async_task` pairs with `cfuture`:

```cpp
rpp::cfuture<int> a = rpp::async_task([]{ return 7; }); // the header, unchanged
rpp::future<int>  b = rpp::async([]{ return 7; });      // the module
```

`then()`, `continue_with()` and `chain_async()` all call the launcher inside, so each one
returns the matching type without a caller naming it. A port is one word per call site.

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
| `future.h` | 107 | keeps `cfuture` and `async_task`, and the new type lands in its own header |
| `event_loop.h` | 11 | swaps `cfuture` for `rpp::future`, and drops its `std::future` constructor |
| `coroutines.h` | 11 | keeps the `rpp::future` awaiter, and the std awaiters move out |
| `task.h`, `thread_pool.h` | 8 | doc comments only |
| `future_types.h` | 1 | a second forward declaration |
| `rpp-future.cppm` | generated | the export list names the new type and drops `cfuture` |

### 5.1 Three headers, and no shared awaiter

The two implementations separate completely. No header holds both, so a reader always knows
which one a file uses, and the old half comes out one header at a time:

```
rpp/async.h           rpp::future, rpp::promise, rpp::async, no <future>
rpp/future.h          rpp::cfuture, rpp::cpromise, rpp::async_task, includes <future>
rpp/std_awaiter.h     std_future_awaiter, and operator co_await for std::future
```

`future.h:15` is the only direct `#include <future>` among the three headers today, so this
split alone takes it out of the group fragment. `event_loop.h` and `coroutines.h` then
include `rpp/async.h`, and `rpp-future.cppm` carries `rpp/async.h`. The group fragment holds
no `<future>` after that, which is what removes B28 condition 1.

### 5.2 The std interop, measured

An earlier draft left this open. A grep over the whole tree settles it, and the two halves
answer differently:

| Declaration | Callers in the tree | What happens to it |
|---|---|---|
| `event_loop.h`, `future_awaiter(std::future<T>&&)` | none | it goes |
| `coroutines.h`, `std_future_awaiter` and its two `operator co_await` | two cases | it moves to `rpp/std_awaiter.h` |

**Nothing in the tree hands a `std::future` to an `event_loop`.** The only construction site
is `run_async` at `event_loop.h:851`, and it reaches the std constructor only because
`IsFuture` matches `std::future` as well. No source file and no test takes that path. So
`event_loop` swaps to `rpp::future` in one step and needs no interop header.

The coroutine half is live. `test_coroutines.cpp` awaits a real `std::async` result in
`std_future_string_coro` and `std_future_lambda_coro`, and `test_modules_future.cpp` asserts
the module exports `std_future_awaiter`. That support keeps working, out of the module
fragment, for a consumer which includes `rpp/std_awaiter.h` by name.

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

### 6.1 The deprecation which moves a consumer

A plan nobody acts on leaves two types forever. `[[deprecated]]` names every remaining site
in the compiler output, and it breaks none of them. It is unconditional, with no opt-in
macro, because a warning nobody turns on moves nobody.

**The attribute goes on the type and on its factories.** A consumer reaches `cfuture` two
ways, and only both attributes cover both:

| Consumer line | The type alone | The type and the factories |
|---|---|---|
| `cfuture<int> f = async_task(...)` | warns | warns |
| `return async_task(...).get()` | silent | warns |
| `auto f = make_ready_future(7)` | silent | warns |

**A pragma region keeps the library itself quiet.** `future.h` names `cfuture` in its own
declarations. An unguarded attribute warns at ReCpp lines in every consumer build, which
buries the sites the consumer has to fix. GCC does not suppress a use inside a deprecated
entity, so the region is what does it:

```cpp
template<class T> class RPP_DEPRECATED_CFUTURE cfuture : public std::future<T> { ... };
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
// async_task, make_ready_future, wait_all, get_all and run_tasks declare in here
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
```

Measured on gcc-15, gcc-14 and clang-18. All three agree, every consumer line warns once,
and no line of the header warns at all. B30 does not reach this, because `future.h` is the
header path and no module carries it after changeset 3.

**MSVC needs its own half, and nothing has measured it.** `#pragma warning(push)` with
`disable: 4996` is the shape. Measure it before changeset 5 lands.

**It lands last, not first.** The attribute fires wherever a name is used, so `event_loop.h`
and `coroutines.h` must stop naming `cfuture` first. That is changeset 2, so the deprecation
is changeset 5. The legacy cases in `test_future.cpp` carry the same file scope suppression,
because they test the deprecated type on purpose.

`cpromise` takes the attribute beside the type, because a site which ports one ports both.

## 7. The changesets

Each one lands on its own and leaves the tree green.

1. **`rpp::future`, `rpp::promise` and `rpp::async` land in `rpp/async.h`**, with their own
   tests. The module exports nothing new, so no consumer sees them. This is the largest
   changeset, because the shared state, the exception path and the destructor are all new
   code.
2. **`event_loop` and `coroutines` move to the new type, and the std awaiters leave.**
   `event_loop` swaps `cfuture` for `rpp::future` and drops its unused `std::future`
   constructor. `std_future_awaiter` moves to `rpp/std_awaiter.h`, which no module carries.
   A header consumer keeps every name it has today.
3. **The module exports the new names and stops exporting `cfuture`.** A consumer target
   in `tests/module_consumer/` builds the new type on gcc-14, which measures whether B28
   condition 1 is really gone.
4. **A downstream project ports its own files**, one at a time, with the header still
   available for the files it has not reached.
5. **`cfuture`, `cpromise` and the legacy factories take `[[deprecated]]`**, unconditionally.
   Every consumer site then names itself on the next build. See 6.1.

Changeset 3 is the one which pays. Until it lands, the module still carries `<future>` and
gcc-14 still crashes an importer which names a `cfuture`.

**The header keeps `cfuture` with no end date.** A removal needs its own decision, and this
plan does not ask for one. The deprecation names the work. It does not schedule the delete.

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
2. `rpp/async.h` includes no `<future>`, which `tools/check_includes.py` reports. No header
   holds both implementations, and `event_loop.h` names no std future at all.
3. A module consumer target builds `rpp::future` on gcc-14 with `<memory>` live, and it
   compiles. That is the B28 measurement.
4. `rpp.future` exports `rpp::future`, `rpp::promise` and `rpp::async`, and it exports no
   `cfuture`, no `async_task` and no `std_future_awaiter`.
5. The header path still builds `cfuture` and passes every case it passes today. The two
   `std::future` coroutine cases still pass through `rpp/std_awaiter.h`.
6. A consumer build warns once at every `cfuture` site and at no line of `future.h` itself.
   The ReCpp build stays warning free, because the legacy cases suppress it by file.
7. One downstream project builds against the branch before changeset 3 merges.
