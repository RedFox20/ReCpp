---
name: ste-writing
description: Write prose in ASD-STE100 Simplified Technical English. Applies to docs, README.md, commit messages, PR text, code comments, doxygen, and error/log strings. Always active in ReCpp via .claude/rules/, also invocable as /ste-writing.
---

# ste-writing

Write prose in ASD-STE100 Simplified Technical English: docs, README.md, commit and PR text, release notes,
comments, doxygen, `Log*` and `ThrowErr()` strings. Not code, identifiers or command syntax. Not marketing copy,
an essay or other text which needs a voice, because STE strips voice. Adapted from
[woosal1337/blog](https://github.com/woosal1337/blog), ep01.

- **One name for one thing.** "future", not "promise" or "async result". "event loop", not "loop thread" or
  "dispatcher". "delegate", not "callback" or "functor". "TimePoint", not "timestamp".
- **Short common words.** start, use, help, make sure, before, after, about, get, show, also. Not begin, initiate,
  utilize, leverage, facilitate, ensure, prior to, subsequent to, regarding, obtain, acquire, demonstrate,
  additionally or furthermore. One meaning per word. No marketing adjectives (seamless, robust, powerful). A
  benchmark number replaces "blazing fast". American spelling.
- **Active voice, plain verbs.** "the pool steals the task". No nominalizations ("perform a parse"), no stacked
  auxiliaries, no phrasal verbs ("spin up"), no "-ing" main verb where a simple tense works.
- **Short sentences.** One instruction per sentence. Max 20 words for an instruction, 25 for a description.
  No semicolons, no contractions. Use articles.
- **Paragraphs and steps.** One topic per paragraph, max six sentences. Steps are a numbered list, one
  imperative action per item, the condition before the command.
- **Strict mode** for `ThrowErr()` and every `Log*` string, build and test steps in AGENTS.md, and every
  workaround comment: all rules apply. README sections, PR text and commit bodies may relax the vocabulary,
  not the sentence, paragraph, active-voice or phrasal-verb rules.

**Comments.** STE sets the wording, never the length. It never pads a comment, and it never deletes one which
carries a WHY. Rewrite a stale comment, and fold a new fact into the comment already there. Every public
declaration gets a `/**` block or a one-line `///` which says what it does and returns. A lock-free or
memory-order line names the race it stops. Never defend, justify or overexplain a design decision. State the
reason in one sentence.

```cpp
// acquire pairs with the release store in signal(), so the waiter sees the result
```

**README.md is the API index.** A table cell is prose, and a semicolon there is the most frequent error. Write one
sentence per row, max 25 words, and start with the verb. Keep the display text equal to the declaration, so
`update_doc_linerefs.py` tracks it: ``| [`rpp::sleep_us(int)`](src/rpp/timepoint.h#L120) | Sleeps the thread |``

Self-lint before returning text. Look for sentences over 20 words, semicolons, contractions and passive voice
with a known actor. Also look for "-ing" main verbs, phrasal verbs, nominalizations and one thing named two
ways. Write only the requested text.
