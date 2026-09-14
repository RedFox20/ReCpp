#!/usr/bin/env python3
"""The one definition of a prose violation. Every hook imports this module.

Three hooks enforce the same rules at different moments, so the rules live here.
A second copy always drifts, and two layers which disagree block a correct edit.

Run it directly to lint a unified diff on stdin:  git diff HEAD | prose_rules.py
"""
import os
import re
import sys
import tempfile

CONTRACTIONS = ("don't", "doesn't", "it's", "can't", "won't", "isn't",
                "we're", "that's", "you're")
MAX_WORDS = 25          # R5, a descriptive sentence
MAX_COMMENT_LINES = 2   # R4, the comment budget
MAX_SLEEP_MS = 10       # R2, the sanctioned sleep band
NEARBY = 4              # R4, how far a comment may sit from the literal it repeats

FENCE = "```"
FENCE_RE = re.compile(r'^(`{3,}|~{3,})') # markdown accepts either delimiter, at any length

TIME_UNIT = re.compile(r'\b(\d+(?:\.\d+)?)\s*(?:ns|us|ms|s|sec|secs|seconds)\b')
TABLE_RULE = re.compile(r'^\|[\s:|-]+\|?\s*$')
LIST_ITEM = re.compile(r'^(?:[-*+]|\d+[.)])\s+(?:\[[ xX]\]\s+)?')
RULE = re.compile(r'^(?:-{3,}|={3,}|\*{3,}|_{3,})\s*$')
SETEXT = re.compile(r'^(?:-{3,}|={3,})\s*$') # under prose it makes a heading, not a break


def looks_like_code(body):
    """Commented-out code is not prose. A trailing semicolon means a statement."""
    return bool(re.search(r'[;{}]\s*$', body) or re.match(r'^[A-Za-z_][\w:<>]*\s*\(', body))


def trailing_comment(line):
    """The `// ...` of a code line, or None. Most comments in this repo sit here."""
    q = pos = 0
    while pos < len(line):
        ch = line[pos]
        if ch == '\\':
            pos += 2
            continue
        if ch in '"\'':
            q ^= 1
        elif not q and line.startswith('//', pos):
            if pos and line[pos - 1] == ':':  # a URL, not a comment
                return None
            return line[pos + 2:].strip()
        pos += 1
    return None


def repeats_a_nearby_literal(lines, i, body):
    """R4: the code holds the value and the comment holds the reason.

    A platform fact such as a 15.6ms timer granularity names no local literal, so it
    stays. A comment which echoes the number on the line beside it goes stale on the
    first tune, and that is what this finds.
    """
    for m in TIME_UNIT.finditer(body):
        pat = r'(?<![\w.])' + re.escape(m.group(1)) + r'(?![\w.])'
        for w in lines[max(0, i - NEARBY):i + NEARBY + 1]:
            if not w.strip().startswith("//") and re.search(pat, w):
                return m.group(0)
    return None


def _cell_rules(row, where, out):
    """A markdown table cell is prose. The row around it is reference data."""
    if TABLE_RULE.match(row):
        return
    t = re.sub(r'`[^`]*`', 'X', row)
    for c in CONTRACTIONS:
        if c in t.lower():
            out.append(f"{where}: contraction '{c}' in a table cell, expand it")
            return
    if ';' in t:
        out.append(f"{where}: semicolon in a table cell, write two sentences")


def _sentence_rules(text, where, out):
    t = re.sub(r'`[^`]*`', 'X', text).strip()
    t = re.sub(r'([.!?])[*_]+', r'\1', t) # a bold or italic closer still ends the sentence
    if not t:
        return
    for c in CONTRACTIONS:
        if c in t.lower():
            out.append(f"{where}: contraction '{c}', expand it")
            break
    if ';' in t:
        out.append(f"{where}: semicolon, write two sentences")
    for s in re.split(r'(?<=[.!?])\s+', t):
        w = len(s.split())
        if w > MAX_WORDS:
            out.append(f"{where}: {w} words in one sentence, split it -> {s[:55]}...")


def lint_markdown(lines, label="md"):
    """Prose lines only. A table, a code fence, and a heading carry reference data.

    Prose wraps, so a sentence spans lines. The paragraph is the unit which carries one,
    and a per-line check misses every sentence the wrap split.
    """
    out, fence, para, start = [], None, [], 0
    for i, l in enumerate(lines):
        st = l.lstrip()
        mark = fence_mark(st)
        if not fence and not mark and para and SETEXT.match(st):
            para = [] # the lines above a setext underline are a heading, not prose
            continue
        new_para = bool(mark) or st.startswith(("|", ">", "#")) or not st \
                or bool(LIST_ITEM.match(st)) or bool(RULE.match(st))
        if new_para and para:
            _sentence_rules(" ".join(para), f"{label}+{start}", out)
            para = []
        if mark:
            fence = next_fence(fence, mark)
            continue
        if fence or st.startswith((">", "#")) or not st or RULE.match(st):
            continue
        if st.startswith("|"):
            _cell_rules(st, f"{label}+{i+1}", out)
            continue
        if not para:
            start = i + 1
        para.append(LIST_ITEM.sub("", st)) # a list marker is not a word of the sentence
    if para:
        _sentence_rules(" ".join(para), f"{label}+{start}", out)
    return out


def lint_code(lines, label="src", creating=False):
    """Comment lines in a C++ file, plus the R2 sleep band."""
    out, run = [], 0
    for i, l in enumerate(lines):
        st = l.strip()
        if st.startswith("//"):
            body = st.lstrip('/').strip()
            # `///` is API documentation and `// ─` is a section banner: both may run long
            explanatory = not st.startswith("///") and not st.startswith("// ─")
            header = creating and i < 6      # a file's own license or title block
            if explanatory and not header:
                run += 1
                if run == MAX_COMMENT_LINES + 1:
                    out.append(f"{label}+{i+1}: comment block over {MAX_COMMENT_LINES} lines, "
                               f"say it in one line and move the story to BUGS.md")
            if re.search(r',\s*$', st) and not looks_like_code(body):
                out.append(f"{label}+{i+1}: comment ends on a dangling comma, it is cut off")
            if not looks_like_code(body):
                _sentence_rules(body, f"{label}+{i+1}", out)
                echo = repeats_a_nearby_literal(lines, i, body)
                if echo:
                    out.append(f"{label}+{i+1}: comment names '{echo}', which the code beside it "
                               f"already sets. Keep the reason, drop the value (R4)")
        else:
            run = 0
            body = trailing_comment(l)
            if body and not looks_like_code(body):
                _sentence_rules(body, f"{label}+{i+1}", out)
                echo = repeats_a_nearby_literal(lines, i, body)
                if echo:
                    out.append(f"{label}+{i+1}: comment names '{echo}', which the code beside it "
                               f"already sets. Keep the reason, drop the value (R4)")
        for m in re.finditer(r'sleep_ms\((\d+)\)', l):
            if int(m.group(1)) > MAX_SLEEP_MS:
                prev = lines[i-1].strip() if i else ""
                if not prev.startswith("//"):
                    out.append(f"{label}+{i+1}: sleep_ms({m.group(1)}) over {MAX_SLEEP_MS} ms "
                               f"needs a reason in a comment above it (R2)")
    return out


def fence_mark(line):
    """The fence delimiter a line opens or closes with, or None."""
    m = FENCE_RE.match(line.lstrip())
    return m.group(1) if m else None


def next_fence(fence, mark):
    """The open delimiter after this line. Only a same, no shorter run closes a block."""
    if fence and fence[0] == mark[0] and len(mark) >= len(fence):
        return None
    return fence or mark


def kept_marker(line):
    """A line an edit keeps is a break, unless it is a fence or an underline."""
    s = line.strip()
    return s if fence_mark(s) or RULE.match(s) else ""


def fenced(lines):
    """The delimiter of the block which follows these lines, or None if none is open."""
    fence = None
    for l in lines:
        mark = fence_mark(l)
        if mark: fence = next_fence(fence, mark)
    return fence


def lint_path(path, lines, creating=False):
    if path.endswith(".md"):
        return lint_markdown(lines, path)
    if path.endswith((".h", ".cpp", ".cppm")):
        return lint_code(lines, path, creating)
    return []


def dedupe(findings, limit=8):
    return list(dict.fromkeys(findings))[:limit]


# (path, lines, the text a finding must contain, or None when the lines are clean)
SELFTEST = [
    ("t.cpp", ["loop->pump_until_ready(fut, rpp::millis(20));",
               "AssertThat(ready, false); // the worker cannot finish in a 20ms budget"], "20ms"),
    ("t.cpp", ["// the granularity of WinAPI SleepConditionVariable is ~15.6ms"], None),
    ("t.cpp", ["int x = 1; // https://example.com/a//b is one comment"], None),
    ("t.cpp", ['const char* s = "a // b"; // a string holding slashes stays quiet'], None),
    ("t.cpp", ["constexpr int READERS = 2; // more readers cost the retire more, see BUGS.md B26"], None),
    ("t.cpp", ["rpp::sleep_ms(1); // wall-clock poll step"], None),
    ("t.cpp", ["rpp::sleep_ms(50);"], "sleep_ms(50)"),
    ("t.cpp", ["// one line", "// two lines", "// three lines"], "over 2 lines"),
    ("t.cpp", ["// the loop can't reach this"], "contraction"),
    ("README.md", ["| [`f()`](a.h#L1) | Attach a clock; null reverts to wall time |"], "semicolon in a table cell"),
    ("README.md", ["| [`f()`](a.h#L1) | Attach a clock. Null reverts to wall time |"], None),
    ("README.md", ["|---|---|"], None),
    ("README.md", ["```", "| a; b |", "```"], None),
    # prose wraps, so the paragraph is the unit which carries a sentence
    ("README.md", ["A sentence which the wrap splits over two lines still counts every word it",
                   "carries, so this one goes over the cap and the lint reports it here."], "in one sentence"),
    ("README.md", ["1. A short numbered item.", "2. Another short numbered item.",
                   "- a short bullet", "- another short bullet"], None),
    # a list marker and a task checkbox are punctuation, so neither counts against the cap
    ("README.md", ["- " + " ".join(["word"] * MAX_WORDS)], None),
    ("README.md", ["- [ ] " + " ".join(["word"] * MAX_WORDS)], None),
    ("README.md", ["- [x] " + " ".join(["word"] * (MAX_WORDS + 1))], "in one sentence"),
    ("README.md", ["- " + " ".join(["word"] * (MAX_WORDS + 1))], "in one sentence"),
    # a bold lead-in ends its own sentence, so the two never join into one
    ("README.md", ["**A bold lead-in ends here.** " + " ".join(["word"] * MAX_WORDS)], None),
    # a setext underline makes a heading, and a rule with a blank above it is a break
    ("README.md", [" ".join(["word"] * MAX_WORDS), "---"], None),
    ("README.md", [" ".join(["word"] * MAX_WORDS), "", "---"], None),
    ("README.md", [" ".join(["word"] * (MAX_WORDS + 1)), "", "***"], "in one sentence"),
]


# (diff text, the text a finding must contain, or None when the diff is clean)
_P14 = "One paragraph here which carries exactly fourteen plain simple ordinary words and then stops"
_P13 = "Another separate paragraph which also carries exactly fourteen plain simple ordinary words here"
SELFTEST_DIFFS = [
    # two hunks are two paragraphs, so neither addition joins the other
    (f"+++ b/X.md\n@@ -1,2 +1,3 @@\n+{_P14}\n@@ -40,2 +41,3 @@\n+{_P13}\n", None),
    # the same two lines inside one hunk are one paragraph, and that sentence is over the cap
    (f"+++ b/X.md\n@@ -1,2 +1,4 @@\n+{_P14}\n+{_P13}\n", "in one sentence"),
    # an unchanged blank line between them splits them again
    (f"+++ b/X.md\n@@ -1,3 +1,5 @@\n+{_P14}\n \n+{_P13}\n", None),
    # so does the line an edit replaced
    (f"+++ b/X.md\n@@ -1,3 +1,4 @@\n+{_P14}\n-gone\n+{_P13}\n", None),
    # a kept fence delimiter marks the additions between them as code, not prose
    (f"+++ b/X.md\n@@ -1,4 +1,6 @@\n ```\n+{_P14}\n+{_P13}\n ```\n", None),
    (f"+++ b/X.md\n@@ -1,4 +1,6 @@\n ~~~\n+{_P14}\n+{_P13}\n ~~~\n", None),
    # a shorter run does not close a longer one, so the code between stays code
    (f"+++ b/X.md\n@@ -1,5 +1,7 @@\n ````\n ```\n+{_P14}\n+{_P13}\n ````\n", None),
    # a kept underline still makes the added line above it a heading
    (f"+++ b/X.md\n@@ -1,2 +1,3 @@\n+{_P14} {_P13}\n ---\n", None),
    # a blank line below makes the same line prose, and the rule below it only ends it
    (f"+++ b/X.md\n@@ -1,3 +1,4 @@\n+{_P14} {_P13}\n \n ---\n", "in one sentence"),
    # only the delimiter which opened a block closes it, so the other one is content
    (f"+++ b/X.md\n@@ -1,5 +1,7 @@\n ```\n ~~~\n+{_P14}\n+{_P13}\n ```\n", None),
    # an added `++count;` line makes a diff record which starts like a file header
    (f"+++ b/X.cpp\n@@ -1,2 +1,4 @@\n+++count;\n+// a comment which doesn't belong\n", "contraction"),
]


# (before, old, new, the text a finding must contain, or None when the edit is clean)
_W26 = " ".join(["word"] * (MAX_WORDS + 1))
SELFTEST_EDITS = [
    # the delimiters sit in old_string, so the filter must not drop them
    ("", FENCE + "\nfoo\n" + FENCE, f"{FENCE}\nfoo\n{_P14}\n{_P13}\n{FENCE}", None),
    # the block opened above the edit, so only the file says the added lines are code
    (FENCE + "\n", "foo", f"foo\n{_P14}\n{_P13}", None),
    # a kept underline still makes the line above it a heading
    ("", "old heading\n---", f"{_W26}\n---", None),
    ("", "anchor", f"anchor\n{_P14} {_P13}", "in one sentence"),
]


def _seeded_fence_diffs(tmp_name):
    """A hunk which starts inside a fenced block, then a later hunk which does not."""
    with open(tmp_name, "w", encoding="utf-8") as f:
        f.write(FENCE + "\n" + "code\n" * 4 + FENCE + "\n" + "prose\n" * 20)
    inside = f"+++ b/{tmp_name}\n@@ -4,2 +4,4 @@\n code\n+{_P14}\n+{_P13}\n code\n"
    after = "@@ -12,2 +14,3 @@\n prose\n+A sentence which doesn't belong here.\n prose\n"
    return [(inside, None), (inside + after, "contraction")]


def selftest():
    """Pin every check, so a refactor cannot turn one into a silent false negative."""
    bad = 0
    tmp = tempfile.NamedTemporaryFile(suffix=".md", delete=False)
    tmp.close()
    diffs = SELFTEST_DIFFS + _seeded_fence_diffs(tmp.name)
    for diff, want in diffs:
        got = lint_diff(diff.splitlines(keepends=True))
        if want is None and got:
            bad += 1
            print(f"FAIL clean diff reported {got[0]}")
        elif want is not None and not any(want in g for g in got):
            bad += 1
            print(f"FAIL expected '{want}' from a diff, got {got or 'nothing'}")
    for before, old, new, want in SELFTEST_EDITS:
        got = lint_path("X.md", edit_lines("X.md", old, new, before))
        if want is None and got:
            bad += 1
            print(f"FAIL clean edit reported {got[0]}")
        elif want is not None and not any(want in g for g in got):
            bad += 1
            print(f"FAIL expected '{want}' from an edit, got {got or 'nothing'}")
    for path, lines, want in SELFTEST:
        got = lint_path(path, lines)
        if want is None and got:
            bad += 1
            print(f"FAIL clean lines reported {got[0]}\n     {lines}")
        elif want is not None and not any(want in g for g in got):
            bad += 1
            print(f"FAIL expected '{want}', got {got or 'nothing'}\n     {lines}")
    os.unlink(tmp.name)
    print(f"== prose_rules selftest: {bad} finding(s) over "
          f"{len(SELFTEST) + len(diffs) + len(SELFTEST_EDITS)} case(s) ==")
    return 1 if bad else 0


def _hunk_opens_a_fence(path, header):
    """The fence delimiter a hunk which starts at this header lands inside, or None."""
    start = re.search(r'\+(\d+)', header)
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return fenced(f.read().splitlines()[:int(start.group(1)) - 1]) if start else None
    except OSError:
        return None # a deleted or renamed file has no state to read


def added_lines(diff):
    """Added lines of a unified diff, per file, with a break at every boundary."""
    per_file, path, in_hunk = {}, None, False
    for raw in diff:
        if raw.startswith("diff --git"):
            path, in_hunk = None, False
        elif not in_hunk and raw.startswith("+++"):
            # a file header sits outside a hunk, so an added `++count;` line is never one
            path = raw[6:].strip() if raw.startswith("+++ b/") else None
            if path: per_file.setdefault(path, [])
        elif not path:
            continue
        elif raw.startswith("+"):
            per_file[path].append(raw[1:].rstrip("\n"))
        elif raw.startswith("@@"):
            in_hunk = True
            open_mark = fenced(per_file[path])
            if open_mark:
                per_file[path].append(open_mark) # a hunk never inherits the fence above it
            per_file[path].append("") # text this edit did not add ends the paragraph
            seed = _hunk_opens_a_fence(path, raw) if path.endswith(".md") else None
            if seed: per_file[path].append(seed)
        elif raw.startswith(" "):
            per_file[path].append(kept_marker(raw[1:]))
        elif raw.startswith("-"):
            per_file[path].append("") # a removed line is not in the file the lint measures
    return per_file


def edit_lines(path, old, new, before=""):
    """The lines an edit introduces, plus the fence it lands inside."""
    oldset = set(old.split("\n"))
    lines = [l if l not in oldset else kept_marker(l) for l in new.split("\n")]
    seed = fenced(before.split("\n")) if path.endswith(".md") else None
    return [seed] + lines if seed else lines


def lint_diff(diff):
    out = []
    for p, lines in added_lines(diff).items():
        out += lint_path(p, lines)
    return out


def _main():
    """Lint added lines of a unified diff read from stdin."""
    if "--selftest" in sys.argv:
        return selftest()
    out = lint_diff(sys.stdin)
    if out:
        print("\n".join(dedupe(out)))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(_main())
