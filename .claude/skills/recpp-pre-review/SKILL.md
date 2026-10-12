---
name: recpp-pre-review
description: Second pass over your own prose before you hand work over. Reload the ste-writing rules, then rewrite every comment, doxygen block, log string, and markdown line the session just added. Always active via .claude/rules/, also invocable as /recpp-pre-review.
---

# recpp-pre-review

Run this pass on your own prose before you call the work done. A first draft pads the prose around the code,
and this pass removes the padding in the same session, with no subagent. Check each sentence before the write,
not after, because editing a draft is weaker than composing under the constraint. The review subagent must
never spend a round on a rule you can fix here in seconds.

## Hooks

Three hooks share `.claude/hooks/prose_rules.py`. Run `.claude/hooks/prose_rules.py --selftest` after you edit
it, because a linter which stops detecting reports nothing.

1. The `Edit` and `Write` hook lints the lines an edit adds, and blocks a line which breaks a rule. Fix it and retry.
2. The `Bash` hook refuses a command which writes a `.md`, `.h`, `.cpp` or `.cppm` file. A `/tmp` path is exempt.
3. The `Stop` hook lints the markdown diff at the end of the turn.

They check sentence length in prose and comments, and contractions and semicolons also in table cells. In
code they check comment blocks over 2 lines and `sleep_ms(N)` over 10. A comment fails when it ends on a comma, or when it
names a number the code beside it sets. Code fences and headings are exempt.

## When to run

1. Before you report a task as complete.
2. Before you write a commit message or PR text.
3. Before you call `/recpp-review` or the `recpp-reviewer` subagent.
4. After an edit which adds a comment, a doxygen block, a `LogError()` or `ThrowErr()` string, or a markdown line.

## Steps

1. Read `.claude/skills/ste-writing/SKILL.md` again, because its rules leave the working set during code work.
2. Collect the added prose with `git diff -U0 -- '*.h' '*.cpp' | grep -E '^\+' | grep -E '//|/\*|\*|Log|Throw'`
   and `git diff -- '*.md'`.
3. Rewrite each line by the checks below.
4. Report one line, such as `pre-review: 4 comments cut to 1 line, 2 passive sentences fixed`.

## Checks

| # | Check | Fix |
|---|-------|-----|
| 1 | Comment over 2 lines | Cut to one line. Move the story to BUGS.md |
| 2 | Comment names a session, an agent, a date or a fix history | Delete that clause. A `see BUGS.md C15` pointer may stay |
| 3 | Comment paraphrases the code below it | Delete the comment |
| 4 | Comment repeats a constant the code beside it sets | Drop the number, keep the reason. A platform fact which no line sets may stay |
| 5 | Sentence over 20 words in an instruction, 25 in a description | Split it |
| 6 | Semicolon or contraction | Write two sentences, or the full form |
| 7 | Passive voice with a known actor, an "-ing" main verb or a nominalization | Use active voice and the plain verb |
| 8 | Long word (utilize, facilitate, ensure, prior to, regarding) or marketing word | Use the short word, or give the number |
| 9 | One thing under two names | Pick one name and use it everywhere |
| 10 | Public declaration with no doxygen | Add a one-line `///` |
| 11 | Comment defends a design decision, or breaks off mid clause | Write one complete sentence with the reason |
| 12 | Comment asserts which thread runs a callback | Name the reentrant case, or drop the claim. See R13 |
| 13 | New timing value which no sibling case uses | Reuse the sibling value. See R2 |

One line is the target, and two lines is the limit. A workaround keeps its why in one line:

```cpp
// the worker frees the future state after the waiter released it, see BUGS.md C15
```

This pass edits prose only. A code finding belongs to `/recpp-review`.
