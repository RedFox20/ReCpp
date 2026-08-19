---
name: recpp-review
description: Review a ReCpp change against the project rules — mandatory tests, fast tests, cross-platform support, 1-2 line comments, STE wording, compact 130-column style, and the four build gates. Runs in the recpp-reviewer subagent. Also invocable as /recpp-review.
---

# recpp-review

Review a ReCpp change. Report every rule it breaks, with the file, the line, the
rule id, and the smallest fix. Do not rewrite the change during the review.

Run the review in the `recpp-reviewer` subagent. The subagent reads this file and
returns the findings. Use the main session only to apply the fixes the review asks
for.

## Why these rules exist

ReCpp replaces the slow and inflexible parts of the standard library. A user who
pays for a heavy or slow ReCpp path gains nothing, and uses `std::` directly. Every
rule below protects that one claim: lighter and faster than the alternative.

Read a slow cleanup, a slow sync, or a multi-second wait as a design defect in the
library, not as a timeout to tune. Report it as a defect and name the path.

## Input

Default target: the working tree and the branch.

```bash
git status --short
git diff                  # unstaged
git diff --cached         # staged
git diff master...HEAD    # the whole branch
```

A path, a commit range, or a PR number replaces the default.

## Rules

Cite one rule id per finding.

### R1. Every new feature and every bug fix carries a test

Tests live in `tests/`. A new public symbol also needs a README.md row and a run
of `python3 update_doc_linerefs.py`.

- Block a `src/rpp/` change which adds behavior and touches no test file.
- One test case must fail without the change. Say which case that is.
- A bug fix needs a case which replicates the bug, not a case which repeats an
  existing path.

### R2. Tests stay fast

The whole suite runs 512 cases in about 4.3 seconds, and 5.5 seconds under TSAN.
Protect that number.

- A case over 100ms needs a reason in the review. A case over 500ms is a defect.
- Sleeps in this repo are 1ms to 10ms. `rpp::sleep_ms(50)` and above needs a
  reason. Never put a sleep inside a loop.
- Wait on an event, not on the clock: `rpp::semaphore`, `future.get()`,
  `task.wait()`, `pool.wait_until_idle()`. Each one returns as soon as the work
  ends.
- A timeout is an upper bound, not a cost, but it is also the price of a hang.
  One second is the cap for every wait in a test. A slower bound only makes a
  broken case take longer to fail.
- Use the ReCpp timing API. Do not add `std::chrono`, `std::this_thread::sleep_for`,
  or `std::this_thread::yield` to source or tests.

### R3. Every change works on every target

Targets: Windows MSVC 2019 to 2026, Android NDK r25b to r29, Yocto AArch64
(i.MX8M+, Xilinx Zynq UltraScale+, Ambarella CV25), Ubuntu g++ 11 to 15 and
clang++ 14 to 20, iOS and macOS Apple Clang, Raspberry Pi, MIPSEL g++ 11.
`config.h` also carries `RPP_FREERTOS`, `RPP_BARE_METAL`, and `RPP_CORTEX_M_ARCH`.

- A platform API needs a guard from `config.h`: `_MSC_VER`, `RPP_MSVC_WIN`,
  `RPP_ANDROID`, `__APPLE__`, `YOCTO_LINUX`, `MIPS`, `RASPI`, `RPP_ENABLE_UNICODE`,
  `RPP_HAS_COROUTINES`, `RPP_HAS_CXX20`. Do not invent a new macro name.
- Prefer the ReCpp abstraction over the platform call: `rpp::TimePoint`,
  `rpp::Duration`, `rpp::mutex`, `rpp::semaphore`, `rpp::file_io`, `rpp::paths`,
  `rpp::threads`.
- A C++20 or C++23 feature in a header which C++17 consumers include needs an
  `RPP_HAS_CXX20` guard.
- The old targets have a small libc. Check every new libc call against Bionic,
  Yocto, and MIPSEL. Unicode paths are off on macOS, MIPSEL, Yocto, and Raspberry Pi.
- No new third party dependency.
- Includes come before imports. A header never contains an `import`.

### R4. A comment is one line, and two lines at most

- The comment states WHAT the code wants to do, or WHY a line is subtle.
- No history. A chat session, an agent name, a date, and a "was broken before"
  story all go stale. `BUGS.md` holds that record.
- One `see BUGS.md C15` pointer is allowed. The entry there carries the story.
- No constants in a comment. A `10s`, a `50ms`, or a `4096` in prose goes stale on
  the first tune. The code holds the value, and the comment holds the reason.
- Do not paraphrase the code below the comment.
- A lock-free or memory-order line needs the race it stops, in one line.
- Never delete a comment which carries a why. Rewrite it.

```cpp
// bad: 5 lines of chronicle
// Regression for BUGS.md C15: the worker drops the LAST reference to the state.
// It does that after the waiter consumed the future. libc++ hides that refcount
// inside libc++.so, so TSAN reports the delete as a race. tests/main.cpp
// suppresses it. This case forces the order on demand.

// good: what the case pins
// the worker frees the future state after the waiter released it, see BUGS.md C15
```

A name carries a constant better than a comment does.

```cpp
// bad: the number rots as soon as somebody tunes the wait
task.wait(rpp::seconds(10)); // symbolizing a TSAN report is slow, so wait up to 10s

// good: the code states the value, the comment states the reason
task.wait(TSAN_REPORT_TIMEOUT); // symbolizing a TSAN report on the worker is slow
```

### R5. Prose follows STE

`.claude/skills/ste-writing/SKILL.md` governs comments, doxygen, README.md,
commit text, and every `LogError()` and `ThrowErr()` string. Apply the strict mode
to log strings and to any comment on a workaround.

### R6. The code matches the file it lands in

`.clang-format` and `.clang-tidy` are the reference. Read the file around the
change before you judge it.

- Allman braces, 4 space indent, indented namespace, `PointerAlignment: Left`.
- Public API gets a `/**` doxygen block or a one-line `///`. State what it returns.
- Keep `noexcept` on the API which the neighbors mark `noexcept`.
- **A method is always `snake_case`.** This one is locked. No exception.
- A type is `snake_case`, like `rpp::concurrent_queue<T>`. Some older types use
  CapitalCase, like `rpp::TimePoint`. Follow the file you edit.
- **Every field goes to the top of the class or the struct**, before the
  constructors and before the methods. Public and private fields alike.
- A field carries no prefix and no suffix. `m_head` and `head_` are both wrong.
  Change the name instead when it collides with a parameter.
- CapitalCase fields such as `concurrent_queue::Head` and `thread_pool::Workers`
  read better than any prefix, and many files use them. Keep them, and match the
  file you edit. A new file prefers `snake_case` with a distinct name.
- Name a type once. Do not add a second name for a thing which already has one.
- `auto` only when the initializer already names the type.
- Never wrap right after `=`. Keep `type name = expression` on one line.

### R7. Reduce vertical height

Scrolling is the bottleneck. The column limit is 130.

- One statement per line, but no padding. Keep a single blank line between
  functions, and none after an opening brace.
- A single statement `if` needs no braces. `readability-braces-around-statements`
  is off on purpose.
- Do not put one argument per line when the call fits on one line.
- Do not split a short guard clause over three lines.

### R8. A line which does not fit needs a local variable

The fix for a 140 column line is a named local, not a wrap.

```cpp
// bad
AssertThat(pool.wait_until_idle(rpp::seconds(1)) == wait_result::finished && pool.total_tasks() == 1, true);

// good
const wait_result idle = pool.wait_until_idle(rpp::seconds(1));
AssertThat(idle, wait_result::finished);
AssertThat(pool.total_tasks(), 1);
```

### R9. The gates

Run every gate which the change can reach. Report the exact command and the
result. A gate which does not run is a gate which failed.

```bash
# 1. Linux GCC
CXX23=1 mama gcc build test="nogdb -vv"
# 2. Linux Clang, plus the clang-tidy pass which AGENTS.md requires
CXX23=1 mama clang build test="nogdb -vv"
CXX23=1 mama clang build clang-tidy test="nogdb -vv"
# 3. Android NDK, aarch64 under QEMU
CXX23=1 mama android verbose build with_tests && ./run_android_tests -vv
# 4. Windows MSVC, see the mirror steps below
```

mama builds each compiler and sanitizer into its own directory, such as
`packages/ReCpp/linux`, `linux-clang`, `linux-tsan`, and `linux-clang-tsan`. A
switch keeps the other directory, so it rebuilds nothing there, and it never mixes
sanitized objects with regular ones. Only the first build of a new combination is
a full one.

#### Windows from WSL

The Windows checkout is a separate clone. Treat it as somebody else's tree.

1. Find the mirrors and pick the one which shares the remote:
   ```bash
   find /mnt/c/Projects -maxdepth 5 -type d -name ReCpp
   git -C <mirror> remote get-url origin   # https://github.com/RedFox20/ReCpp.git
   ```
2. Read its state before you touch it:
   ```bash
   git -C <mirror> rev-parse --abbrev-ref HEAD
   git -C <mirror> status --short
   ```
3. A dirty mirror stops the gate. Report it and ask the owner. Never discard that
   work.
4. Move the branch with git, which refuses to overwrite dirty files:
   ```bash
   git -C <mirror> fetch /home/jorma/krattcam/packages/ReCpp/ReCpp <branch>
   git -C <mirror> checkout -B <branch> FETCH_HEAD
   ```
5. Build and test through PowerShell. `mama.exe` sits on the Windows PATH, at
   `C:\Python313\Scripts\mama.exe` on this machine:
   ```bash
   powershell.exe -NoProfile -Command "cd C:\Projects\KrattGCS\packages\ReCpp\ReCpp; \$env:CXX20='1'; mama windows verbose build with_tests jobs=4"
   powershell.exe -NoProfile -Command "cd C:\Projects\KrattGCS\packages\ReCpp\ReCpp; .\bin\RppTests.exe nogdb -vv"
   ```
6. Forbidden in the mirror: `rm -rf`, `cp -r` over it, `git checkout -f`,
   `git reset --hard`, `git clean`.

### R10. TSAN is a suggestion, not a gate

A TSAN run is welcome. A TSAN report never blocks the change on its own.

- TSAN reports false positives. An uninstrumented `libc++.so` hides the
  `std::promise` refcount, and every such delete looks like a race. See BUGS.md C15.
- Do not fix a TSAN report inside a review. Every past automated attempt grew
  dozens of lines around a simple cause.

### R11. A TSAN report is deferred work

1. Write the report into `BUGS.md` with the two stacks and the thread names.
2. Analyze it later, on its own. Name the missing happens-before edge.
3. Write the regression test FIRST. It must replicate the report on demand.
4. Then fix it in 3 to 5 lines. A larger fix needs the owner to agree first.
5. A rewrite of the pool, the future, the delegate, or the test framework is
   forbidden as a race fix.

## Report format

Order the findings by severity. Keep one line per finding where possible.

```
BLOCK  R1  tests/            no test covers thread_pool::steal_task()
BLOCK  R3  src/rpp/paths.cpp:88   readlink() has no guard, Windows and CV25 fail
WARN   R7  tests/test_future.cpp:120  6 blank lines, 3 one-argument-per-line calls
NOTE   R10 test_future        TSAN reports 1 race, deferred to BUGS.md B12

Gates: gcc PASS 512/512 5.5s | clang PASS | clang-tidy PASS | android NOT RUN | windows SKIP (mirror dirty)
Verdict: BLOCK, 2 findings
```

- `BLOCK` breaks R1, R2, R3, R8, or R9.
- `WARN` breaks R4, R5, R6, or R7.
- `NOTE` records a TSAN report or deferred work.
- A clean review says `Verdict: PASS` and lists the gates.

## Self-lint before you report

1. Does every finding name a file, a line, and a rule?
2. Does every finding carry the smallest fix, in one sentence?
3. Did you run every gate the change can reach, and report the ones you skipped?
4. Did you avoid proposing a rewrite?
5. Is the report itself STE, with no sentence over 25 words?
