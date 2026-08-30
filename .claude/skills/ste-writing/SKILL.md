---
name: ste-writing
description: Write prose in ASD-STE100 Simplified Technical English. Applies to docs, README.md, commit messages, PR text, code comments, doxygen, and error/log strings. Always active in ReCpp via .claude/rules/; also invocable as /ste-writing.
---

# ste-writing

Write prose in ASD-STE100 Simplified Technical English. This applies to
documentation, README.md, pull-request text, commit messages, error and log
strings, release notes, doxygen comments, and code comments. It does not apply
to code, identifiers, or command syntax. It is not for marketing copy, essays,
or anything that needs a voice — STE strips voice on purpose.

Adapted from https://github.com/woosal1337/blog (ep01, "the cure for AI slop").

## Rules

WORDS
- Use one name for one thing. Do not call the same item by two different names.
  In this repo that means: "future", not "promise"/"async result". "event loop",
  not "loop thread"/"dispatcher". "delegate", not "callback"/"functor".
  "TimePoint", not "timestamp".
- Use the short common word: start (not begin/commence/initiate), use (not
  utilize/leverage), help (not facilitate), make sure (not ensure), before (not
  prior to), after (not subsequent to), about (not regarding/concerning), get
  (not obtain/acquire), show (not demonstrate), also (not
  additionally/furthermore/moreover).
- Give each word one meaning. "fall" means to move down, not to decrease.
- No marketing adjectives: seamless, robust, powerful, cutting-edge,
  effortless, world-class, next-generation, revolutionary. A benchmark number
  replaces "blazing fast".
- American spelling.

VERBS
- Active voice. "the pool steals the task", not "the task is stolen by the
  pool".
- Use a verb for an action. "parse the string", not "perform a parse of the
  string".
- No stacked auxiliaries. Not "it is important to note that this may help to
  improve". Write "this improves X".
- No "-ing" main verb where a simple tense works.

SENTENCES
- One instruction per sentence. Max 20 words (instruction), max 25
  (descriptive).
- No contractions. Use articles: a, an, the, this, these.

PUNCTUATION
- No semicolons. Write two sentences.

STRUCTURE
- One topic per paragraph, max six sentences. For steps, use a numbered
  vertical list, one action per item, imperative form. Put a condition before
  its command.

Write only the requested text. No preamble, no summary, no closing remarks.

## Modes

- **strict** — error and log strings, `ThrowErr()` and `LogError()` messages,
  build and test instructions in AGENTS.md, and any comment on a workaround:
  apply every rule and both length caps.
- **STE-flavored** — general prose (README.md tables and sections, PR
  descriptions, commit bodies): apply the sentence, paragraph, active-voice,
  and no-phrasal-verb discipline. Relax the ~900-word dictionary lockdown so
  the text keeps enough range to read naturally.

## Interaction with the ReCpp comment rules

STE sets the WORDING. It never pads a comment to reach a length, and never
deletes a comment that carries a WHY.

- Every public API declaration gets a `/**` doxygen block or a one-line `///`.
  It states what the function does and what it returns.
- A lock-free or memory-order-sensitive line needs the WHY, not the WHAT. Name
  the race the barrier stops.
- Consolidate an existing comment instead of appending a second one.
- Never delete an existing comment. If it goes stale, rewrite it.
- Never defend, justify or overexplain a design decision. State the reason in one
  short sentence.

Comment slop — prose that paraphrases the code below it — is the failure mode
this skill removes. STE sharpens the surviving line. It never licenses more
lines.

```cpp
// Bad: "It should be noted that we are performing an acquire load at this
//       point in order to potentially ensure the correct visibility."
// Good:
// acquire pairs with the release store in signal(), so the waiter sees the result
```

## README.md rules

README.md is the public API index. AGENTS.md requires a line reference for each
documented symbol.

- Keep the display text equal to the declaration, so `update_doc_linerefs.py`
  tracks it: `| [`rpp::sleep_us(int)`](src/rpp/timepoint.h#L120) | Description |`
- Write one descriptive sentence per table row. Max 25 words. No period needed.
- Do not write "this function ...". Start with the verb: "Sleeps the current
  thread for the given microseconds."

## Self-lint (run before returning text)

1. Any sentence over 20 words? Split it.
2. Any semicolon? Replace with a period.
3. Any contraction? Expand it.
4. Any passive voice with a known actor? Make it active.
5. Any "-ing" main verb, nominalization ("perform an analysis"), or phrasal
   verb ("spin up")? Replace with a plain verb.
6. Same thing named two ways? Pick one name.

The mechanical rules above are lintable and are what removes slop. Full STE
also needs human judgment (the right technical noun, whether a sentence "makes
good sense") — a checker cannot certify that, and slop is not about that. This
skill fixes the FORM of slop. It cannot make a hollow paragraph true.

Free official standard (do not paste it in full, it is copyrighted):
https://asd-ste100.org
