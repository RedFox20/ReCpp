# ReCpp Code Style — rationale and examples

`AGENTS.md` carries the rules. This file carries the why and the examples.

## Prefer explicit types when the type is not obvious

Use the explicit type name when it communicates ownership, lifetime, precision,
or the ReCpp abstraction in use.

```cpp
rpp::future<> future = rpp::async(run_work);
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

## One statement per line

A packed line hides a branch, a side effect or a loop body from a reader who scans.
The owner flagged each shape below in review, so R7 reports them.

```cpp
// bad: a block folded onto one line
if (len > 0) { buffer[len] = '\0'; return load_buffer{ buffer, len }; }

// good
if (len <= 0)
{
    free(buffer);
    return load_buffer{ nullptr, 0 };
}
buffer[len] = '\0';
return load_buffer{ buffer, len };
```

```cpp
// bad: the read and the work hide in the for header
for (int n; len < capacity && (n = input.read(buffer + len, capacity - len)) > 0;) len += n;

// good: the header iterates, and the body reads
while (len < capacity)
{
    int bytes_read = input.read(buffer + len, capacity - len);
    if (bytes_read <= 0) // EOF or a read error
        return len;
    len += bytes_read;
}
```

```cpp
// bad: an assignment inside a ternary, and a lambda on one line
auto grow = [&](int n) { char* p = (char*)realloc(buffer, n); return p ? (buffer = p) : nullptr; };

// good
auto grow = [&](int n) -> char* {
    char* p = static_cast<char*>(realloc(buffer, size_t(n)));
    if (p)
        buffer = p;
    return p;
};
```

## Two peer branches take `if` and `else`

A guard clause rejects input and leaves. Two branches which both do work are peers,
and peers read as `if` and `else`. The reader sees one choice, at one indent level.

```cpp
// good: the two waits are peers, and the shape says so
if (frame.warpable)
{
    // warpable clock: poll so warp_forward() can release the wait early
    while (current_time(frame) < deadline)
        rpp::sleep_ms(1); // wall-clock poll step
}
else
{
    rpp::sleep_until(deadline); // wall clock: one efficient sleep
}

// bad: an early return dressed as a guard, which hides the second branch below it
if (!frame.warpable)
{
    rpp::sleep_until(deadline);
    return;
}
while (current_time(frame) < deadline)
    rpp::sleep_ms(1);
```

The bad shape is two lines shorter, and that is not a reason. R7 asks for vertical
height, and this rule outranks it. Never trade the `else` for an early return.

## Includes come before imports

```cpp
#include <rpp/tests.h>   // 1. rpp headers
#include <cstring>       // 2. std headers
#include <limits>

#if RPP_BUILD_WITH_MODULES
import rpp.text;         // 3. imports, last
#endif
```

GCC 14 re-parses a standard library header which follows an import. Its internal
templates then collide with the entities the module already made reachable. One
`#include <string>` after one `import` gives about 960 compile errors. The whole
`RppTests` module build gave 1603 errors from a single misplaced import.

`<cstdio>` and other C wrappers do not trigger it, because they declare no
template. Do not use that as a reason to break the order.

The error is loud and it stops the compiler. It never reaches the linker, and it
never becomes a duplicate symbol. A program which mixes `import rpp.text` in
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
