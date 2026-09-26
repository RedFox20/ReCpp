<team-rules version="1">

# Team rules for coding agents

This file is the same in every repo that carries it, as `.claude/rules/team-rules.md`, and in `~/.claude/`. A session
that sees the `<team-rules` tag has these rules loaded.

## Precedence

The project `CLAUDE.md` wins over this file. Read it first. These rules fill the gaps it leaves.

## Communication

Give the facts. Nothing else.

- Answer first. No preamble, no restating the question, no "Let me...".
- No closing pleasantries, no recap of what you just did.
- State a decision. Do not justify it unless asked. One line of reasoning at most,
  and only when the choice is not obvious.
- Do not defend a decision I question. Check whether I am right, then say which.
- Do not explain a concept I did not ask about. Depth on request only.
- Report a failure as cause and fix. No apology, no hedging, no "unfortunately".
- Uncertainty is a fact. State it in a few words. Do not manufacture confidence
  and do not pad with "might" or "perhaps" where you know the answer.
- Show the command, the `file:line`, or the diff. Prose is the fallback, not the default.

## Never print secrets

Never print a secret value to the terminal, not even while debugging. Check its length or whether it is set, never its
content. This covers passwords, tokens, API keys, private keys and connection strings. The same applies to a key file:
report the byte count or a checksum prefix. If a secret does leak, say so plainly and recommend rotating it.

## Code style

Brief, flat, few lines. A reader scans code more often than they read it.

- Before you add a feature, search for one that already does the job. Extend or fix
  that one. A second path for the same job is a defect.
- No em-dash (U+2014) in code, comments, docs or chat. Use `-`.
- Keep an expression on one or two lines. Extract a named local instead of writing a third
  line. The project formatter overrides this.
- Prefer a flat function to a nested one. Guard at the top, then the work.
- Write a comment only when the WHY is not obvious: a hidden constraint, an invariant, a
  workaround for a named bug, or behavior that surprises the reader. Where the project
  requires a comment on every public symbol, that requirement wins.
- Cap a comment or a docstring at 2 lines. Sacrifice grammar for brevity.
- A comment never restates the symbol name, names the task or chat that produced it, lists
  callers, or tells bug history. That text goes in the commit message.

## Terseness

Less code means fewer bugs. Delete on sight:
- a helper called once (inline it at the one call site)
- a test that asserts a getter or a tautology
- a stub builder copied across test files (hoist it into the shared test helper)
- a docstring that repeats the test name, the class contents, or the bug history
- dead weight: a compat shim, an unused placeholder, a `# removed` note, a stale `# noqa`,
  an import the target version does not need
- an abstraction for a requirement nobody asked for

Cut only inside the change you were asked to make. Do not rewrite adjacent code for line count.

## Work cycle

Edit -> test -> review -> fix -> commit.

1. Make the change. Run the project test suite.
2. Review the diff. Use the project review skill if one exists.
   Without one, run a sub-agent with the project `CLAUDE.md` rules plus this file.
   A finding reads `<file>:<line> - <rule>: <fix>`.
3. Apply every finding you agree with. Answer the rest `wontfix` with one line of reason.
   A finding is `wontfix` when it is incorrect, already handled, or scope creep.
4. Re-run the suite. Run a second review round only after large fixes. Two rounds is the
   limit. Report what is left as known issues.
5. Commit one logical change, prefixed `feat:`, `fix:`, `refactor:`, `cleanup:`, `docs:`,
   `test:`, `ci:`, `build:` or `perf:`.

Review once before the commit, not after every edit.

## Subagents

- Only spawn a minimal amount of subagents if necessary
- Never spawn a Fable subagent unless specifically asked
- Default subagent thinking level should be Medium
- For reviews use Sonnet with Medium thinking level
- Keep review subagent alive and send message with new changes to review

## Pull requests

- Name the PR branch for its change: `fix/`, `feat/`, `docs/` or `perf/` plus a short description, such as
  `fix/imx8mp-sdk-version-url`. Use it instead of a generated session branch name.
- One PR per session. A session that changes more than one repository gets one PR per repository.

## PR replies and review threads

Handle the review threads of a PR you open or push to, without being asked. Check them after each push, and fetch them
again before you report. Start every reply with `Claude:`. Do not over explain.

1. Check each finding against the code first. A bot review is a hypothesis.
2. A finding you fixed: reply `Claude: fixed <sha>` with one sentence of evidence, then resolve the thread.
3. A wrong finding, or one whose fix has a worse side effect: reply `Claude: wontfix <reason>`.
   Resolve it, unless the repository rule keeps a declined thread open for the owner.
4. A finding only the owner can decide: reply `Claude: needsinput <question>`, and leave it open.

</team-rules>
