# ReCpp Code Style — rationale and examples

`AGENTS.md` carries the rules. This file carries the why and the examples.

## Prefer explicit types when the type is not obvious

Use the explicit type name when it communicates ownership, lifetime, precision,
or the ReCpp abstraction in use.

```cpp
rpp::cfuture<> future = rpp::async_task(run_work);
rpp::TimePoint deadline = rpp::TimePoint::now() + rpp::millis(100);
```

Use `auto` when the initializer already makes the type unmistakable, or when
spelling it would obscure the code, such as an iterator or a template detail.

## Do not wrap immediately after `=`

```cpp
// good
const bool future_was_ready_during_cleanup = future.await_ready();

// bad
const bool future_was_ready_during_cleanup =
    future.await_ready();
```

When an initializer is too long, wrap inside its argument list or another natural
expression boundary. Keep `type name = expression` on the first line.

## Includes come before imports

```cpp
#include <rpp/tests.h>   // 1. rpp headers
#include <cstring>       // 2. std headers
#include <limits>

#if RPP_BUILD_WITH_MODULES
import rpp.strview;      // 3. imports, last
#endif
```

GCC 14 re-parses a standard library header which follows an import. Its internal
templates then collide with the entities the module already made reachable. One
`#include <string>` after one `import` gives about 960 compile errors. The whole
`RppTests` module build gave 1603 errors from a single misplaced import.

`<cstdio>` and other C wrappers do not trigger it, because they declare no
template. Do not use that as a reason to break the order.

The error is loud and it stops the compiler. It never reaches the linker, and it
never becomes a duplicate symbol. A program which mixes `import rpp.strview` in
one translation unit and `#include <rpp/strview.h>` in another links and runs
correctly.

## A header never contains an `import`

A header does not control where a consumer includes it, so it cannot keep the
order above. An `import` in a header also removes every transitive std include
from each consumer, which breaks files that never mentioned modules.

## Include what you use

A header which re-exports another one for its consumers names nothing from it, so
the scan reads it as stale. Mark the line `#include "x.h" // re-export` and no
check reports it. Do not delete a re-export to clear a finding.
