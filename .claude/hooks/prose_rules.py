#!/usr/bin/env python3
"""The one definition of a prose violation. Every hook imports this module.

Three hooks enforce the same rules at different moments, so the rules live here.
A second copy always drifts, and two layers which disagree block a correct edit.

Run it directly to lint a unified diff on stdin:  git diff HEAD | prose_rules.py
"""
import re
import sys

CONTRACTIONS = ("don't", "doesn't", "it's", "can't", "won't", "isn't",
                "we're", "that's", "you're")
MAX_WORDS = 25          # R5, a descriptive sentence
MAX_COMMENT_LINES = 2   # R4, the comment budget
MAX_SLEEP_MS = 10       # R2, the sanctioned sleep band
NEARBY = 4              # R4, how far a comment may sit from the literal it repeats

TIME_UNIT = re.compile(r'\b(\d+(?:\.\d+)?)\s*(?:ns|us|ms|s|sec|secs|seconds)\b')
TABLE_RULE = re.compile(r'^\|[\s:|-]+\|?\s*$')


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
    """Prose lines only. A table, a code fence, and a heading carry reference data."""
    out, fence = [], False
    for i, l in enumerate(lines):
        if l.lstrip().startswith("```"):
            fence = not fence
            continue
        st = l.lstrip()
        if fence or st.startswith((">", "#")):
            continue
        if st.startswith("|"):
            _cell_rules(st, f"{label}+{i+1}", out)
        else:
            _sentence_rules(l, f"{label}+{i+1}", out)
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


def lint_path(path, lines, creating=False):
    if path.endswith(".md"):
        return lint_markdown(lines, path)
    if path.endswith((".h", ".cpp", ".cppm")):
        return lint_code(lines, path, creating)
    return []


def dedupe(findings, limit=8):
    return list(dict.fromkeys(findings))[:limit]


def _main():
    """Lint added lines of a unified diff read from stdin."""
    per_file, path = {}, None
    for raw in sys.stdin:
        if raw.startswith("+++ b/"):
            path = raw[6:].strip()
            per_file.setdefault(path, [])
        elif raw.startswith("+") and not raw.startswith("+++") and path:
            per_file[path].append(raw[1:].rstrip("\n"))
    out = []
    for p, lines in per_file.items():
        out += lint_path(p, lines)
    if out:
        print("\n".join(dedupe(out)))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(_main())
