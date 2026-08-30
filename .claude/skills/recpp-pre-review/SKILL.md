---
name: recpp-pre-review
description: Second pass over your own prose before you hand work over. Reload the ste-writing rules, then rewrite every comment, doxygen block, log string, and markdown line the session just added. Always active via .claude/rules/; also invocable as /recpp-pre-review.
---

# recpp-pre-review

Run this pass on your own output before you call the work done. The first draft
almost never carries the STE rules. A model writes the code, then pads the prose
around it. This pass removes that padding, in the same session, with no subagent
and no wait.

## Run it BEFORE the write, not after

The first draft never carries the STE rules, and editing a draft is weaker than
composing under the constraint. Check the sentence before you splice it in.

A `PreToolUse` hook on `Edit` and `Write` enforces this. It lints only the lines an
edit introduces, and it blocks the write when one breaks a rule. A blocked write is
not a failure. Fix the line and retry.

The checks it runs mechanically:

| Check | Applies to |
|-------|------------|
| sentence over 25 words | markdown prose, comments |
| contraction, semicolon | markdown prose, comments |
| comment block over 2 lines | `.h`, `.cpp`, `.cppm` |
| comment ending on a dangling comma | `.h`, `.cpp`, `.cppm` |
| `sleep_ms(N)` with N over 10 | `.h`, `.cpp`, `.cppm` |

Tables, code fences, and headings are exempt, because they carry reference data.

## Prose never goes in through the shell

A second hook refuses a Bash command which writes a `.md`, `.h`, `.cpp`, or `.cppm`
file. Reading, building, and restoring stay allowed, and a scratch path under
`/tmp` is never project prose.

It reads shell lines and heredoc bodies apart, so text which quotes a redirect is
documentation, not an edit. A markdown code span is stripped for the same reason.

Three layers cover the whole path. The Bash hook pushes an edit to the right tool,
the Edit hook lints the text before it lands, and the Stop hook re-lints the
markdown diff at the end of the turn.

## When to run

Run it in every one of these cases:

1. Before you report a task as complete.
2. Before you write a commit message or PR text.
3. Before you invoke `/recpp-review` or the `recpp-reviewer` subagent.
4. After any edit which added a comment, a doxygen block, a `LogError()` string,
   a `ThrowErr()` string, or a markdown line.

The review subagent must never spend a round on a rule you can fix here in
seconds.

## Steps

1. **Reload the rules.** Invoke `/ste-writing` and read
   `.claude/skills/ste-writing/SKILL.md` again. Do not trust your memory of it.
   The rules leave the working set as soon as you start writing code.

2. **Collect the prose you added.**
   ```bash
   git diff -U0 -- '*.h' '*.cpp' | grep -E '^\+' | grep -E '//|/\*|\*|Log|Throw'
   git diff -- '*.md' AGENTS.md README.md BUGS.md
   ```

3. **Rewrite each line.** Apply the checks below. Edit in place.

4. **Report one line.** Name the count: `pre-review: 4 comments cut to 1 line, 2
   passive sentences fixed`. Then continue.

## Checks

Work through this list once per added line. Each check has a mechanical fix.

| # | Check | Fix |
|---|-------|-----|
| 1 | Comment over 2 lines | Cut to one line. Move the story to BUGS.md |
| 2 | Comment names a session, an agent, a date, or a fix history | Delete that clause. A `see BUGS.md C15` pointer may stay |
| 3 | Comment paraphrases the code below it | Delete the comment |
| 4 | Comment repeats a constant: `10s`, `50ms`, `4096` | Drop the number. Name the value in code if it needs a name |
| 5 | Sentence over 20 words in an instruction, 25 in a description | Split it |
| 6 | Semicolon | Write two sentences |
| 7 | Contraction | Expand it |
| 8 | Passive voice with a known actor | Make it active |
| 9 | `-ing` main verb, or "perform an analysis" | Use the plain verb |
| 10 | Long word: utilize, facilitate, ensure, prior to, regarding | Use the short one |
| 11 | Marketing word: seamless, robust, powerful, blazing | Delete it, or give the number |
| 12 | The same thing under two names | Pick one name and use it everywhere |
| 13 | A public declaration with no doxygen | Add a one-line `///` |
| 14 | Comment defends, justifies or overexplains a design decision | Cut to one sentence that states the reason |

## The comment budget

One line is the target. Two lines is the limit. The line says WHAT the code wants
to do, or WHY a subtle line exists. It never says who found the bug or when. It
never defends, justifies or overexplains a design decision. It states the reason
in one short sentence.

```cpp
// bad
// This was added because a previous session discovered that the delegate
// destructor could run on the worker thread after the future was already
// consumed by the main thread, which the TSAN run in CI reported as a race.

// good
// the worker frees the future state after the waiter released it, see BUGS.md C15
```

A workaround keeps its why, in one line. Never delete a comment which carries a
why. Rewrite it.

## Scope

This pass edits prose. It does not edit code, rename a symbol, or move a line. A
code finding belongs to `/recpp-review`, which runs after this pass.
