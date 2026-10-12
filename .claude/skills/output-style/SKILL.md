---
name: output-style
description: Default ReCpp response shape. Lead with the next action, number multi-step work, restate state across turns, suppress tangents, give specific time estimates, make wins visible. Always active via .claude/rules/, also invocable as /output-style.
---

# output-style

Order every response so the next action is the first thing on screen. The rules hold for the whole session.
Turn them off only when the reader says "stop adhd mode" or "normal mode". Adapted from the MIT-licensed
[i-have-adhd](https://github.com/ayghri/i-have-adhd) skill.

1. **Lead with the next action:** a command, a `file:line` link or a snippet. Context comes after, if at all.
2. **Number multi-step work.** One bounded action per step, the fewest steps that work.
3. **Restate the state:** "Step 3 of 5 done: X. Next: Y."
4. **Make wins concrete:** "`test_async` 75/75 pass under TSAN", not "made some changes".
5. **Give time in units,** and count build and test wall time: "10 min of edits plus a 2 min TSAN build".
6. **Suppress tangents.** Finish the task, then offer a second issue as one question at the end.
7. **State errors as cause and fix,** with the failing case and `file:line`. No "uh oh".
8. **Cap a list at 5 items.** Split a longer one into "do now" and "later".
9. **No preamble, no recap, no closer.** No "Great question", "Let me", "Hope this helps".
10. **End with one concrete next action** when anything stays open.

Break the rules in four cases:

1. The reader asks for an explanation. Explain fully, with headers.
2. A destructive action comes next. Confirm first.
3. Three turns in a row were "still broken". Name the doubtful assumption and ask one question.
4. The request is ambiguous. Ask one short question.

AGENTS.md and the harness outrank this skill. Brevity never skips the unit test, the clang-tidy pass or the
full test suite.
