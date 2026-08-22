# ReCpp Instructions for Agents

The single source of instructions for every coding agent. Codex reads it directly.
Claude Code loads it through `.claude/rules/recpp.md`, and Copilot through
`.github/copilot-instructions.md`.

ReCpp is a standalone repo. A `CLAUDE.md` in any parent directory does NOT apply,
and `.claude/settings.json` excludes those files.

## MANDATORY: always-on skills

`.claude/rules/` loads these every session. Do not wait to be asked. An agent
which cannot read `.claude/rules/` reads `.claude/skills/*/SKILL.md` instead.

| Skill | Governs |
|-------|---------|
| `ste-writing` | prose: docs, commit and PR text, comments, doxygen, `Log*` and `ThrowErr()` strings. Not code, identifiers, or commands |
| `output-style` | responses to the user: lead with the next action, drop preamble and pleasantries |
| `recpp-pre-review` | your own prose, before you report, before a commit message, before you call the reviewer |
| `recpp-review` | the rule ids, the gates, and the report format |

Read `recpp-review` BEFORE you edit and again after. Name the rule ids your change
touches before you touch a file. The `recpp-reviewer` subagent is a second opinion,
never your first read of the rules.

If a style rule collides with the rest of this file, **this file wins. The style
shapes what fits inside it.** The gates in `recpp-review` R9 are not negotiable for
brevity.

## Editing Workflow

Every change runs these four steps in order. A later step never starts before the
one above it passes.

1. **Plan, and write the test first.** A bug needs a recipe which fails on demand,
   and the test must fail before the fix and pass after it. A new feature needs its
   test in the same change. A fix with no failing reproducer is a guess.
2. **Fix it, and make the tests pass.** Change the smallest thing which works.
   Remove the race first, correct an unprovable assertion second, change a number
   last.
3. **Document new API.** Run `python3 update_doc_linerefs.py` and
   `python3 update_doc_linerefs.py --check-undocumented`. Both must come back
   clean. A header README.md never mentions is a blind spot, and the check reports
   it.
4. **Edit prose with the Edit or Write tool, never through the shell.** A hook lints
   the text before it lands, and it cannot see a shell redirect. This covers
   markdown, comments, doxygen, and log strings.
5. **Run the pre-review pass.** `recpp-pre-review` over every comment, doxygen
   block, log string, and markdown line you added. Cut each one to the shortest
   form which still carries the why.

## Development Requirements

1. **Unit tests are mandatory** — a test in `tests/` for every new feature and every
   bug fix.
2. **Keep the tests fast** — 512 cases in about 4.3 seconds, 5.5 under TSAN. Wait
   on an event, not on the clock. See `recpp-review` R2 for the timing rules.
3. **Validate with clang-tidy** — `CXX23=1 mama gcc build clang-tidy test="nogdb -vv"`.
   Fix every warning before you call the task complete.
4. **Read the whole failure** — keep at least 80 lines when you trim build output. A
   warning is not the error, and a stale binary is not a pass. Check that the build
   reported success before you trust a test result.
5. **Run the full test suite** — `CXX23=1 mama gcc build test="nogdb -vv"`, plus the
   Clang, Android, and Windows gates in `recpp-review` R9.
6. **TSAN is a suggestion, not a gate** — it reports false positives. Add `tsan`
   when you hunt a race, and write the report into `BUGS.md` instead of patching it
   on the spot. See R10 and R11.

## Code Style

Examples and rationale: [`docs/CODE_STYLE.md`](docs/CODE_STYLE.md).

- **Prefer an explicit type when the type is not obvious.** Do not reach for `auto`
  merely to save typing. Use it when the initializer already makes the type plain.
- **Never wrap immediately after `=`.** Keep `type name = expression` on one line
  and wrap inside the argument list.
- **Use the ReCpp timing and readiness API.** `future.await_ready()`, `rpp::yield()`,
  `rpp::Duration`, `rpp::TimePoint`, `rpp::millis()`. Never add `std::chrono` or a
  `std::this_thread` timing helper to source or tests.
- **Includes come before imports. Always.** A misplaced import gives about 960
  compile errors, never a link error.
- **A header never contains an `import`.** Only a `.cpp`, a test, or a `.cppm` does.
- **Include what you use.** Do not rely on a transitive include. Run
  `tools/check_includes.py all` before you commit a header change. Mark a
  re-export `#include "x.h" // re-export`, and never delete one to clear a finding.

## After Modifying a Header

1. `python3 update_doc_linerefs.py` fixes the README.md line numbers. `--dry-run`
   previews.
2. Add a README.md row for each new public function, type, or constant:
   `| [\`name(params)\`](src/rpp/header.h#L123) | Description |`. The display text must
   match the declaration, or `update_doc_linerefs.py` loses track of it.
3. `python3 update_doc_linerefs.py --check-undocumented` lists what README.md misses.

## Reference

- Header index: [`docs/HEADERS.md`](docs/HEADERS.md)
- Build, sanitizers, clang-tidy: [`docs/BUILD.md`](docs/BUILD.md)
- Code style examples and rationale: [`docs/CODE_STYLE.md`](docs/CODE_STYLE.md)
- Modules migration plan: [`docs/MODULES_MIGRATION.md`](docs/MODULES_MIGRATION.md)
