#!/usr/bin/env python3
"""PreToolUse on Bash: keep prose out of the shell, and lint what still goes in.

Markdown must go through Edit or Write, where the text is linted before it lands.
A C++ file may be written by the shell, because that is usually codegen or a new
file, so this lints the comment lines it carries instead of refusing the write.
"""
import json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from prose_rules import lint_code, dedupe

try:
    ev = json.load(sys.stdin)
except Exception:
    sys.exit(0)

cmd = (ev.get("tool_input") or {}).get("command", "")
if not cmd:
    sys.exit(0)

MD = r'[^\s\'"|;&)>]+\.md'
CPP = r'[^\s\'"|;&)>]+\.(?:h|cpp|cppm)'


def is_scratch(path):
    return path.startswith("/tmp/") or "$SP" in path or "TMPDIR" in path


# A heredoc body is data, not shell. Prose inside it may quote a redirect or an
# in-place edit, and that is documentation, never an edit. Split the two apart.
shell_lines, bodies, delim, body = [], [], None, []
for line in cmd.split("\n"):
    if delim is not None:
        if line.strip() == delim:
            bodies.append(body)
            body, delim = [], None
        else:
            body.append(line)
        continue
    shell_lines.append(line)
    m = re.search(r'<<-?\s*[\'"]?([A-Za-z_][A-Za-z0-9_]*)[\'"]?', line)
    if m:
        delim = m.group(1)
if body:
    bodies.append(body)


def strip_spans(text):
    """A markdown code span is prose about a command, not a command."""
    return re.sub(r'`[^`]*`', '', text)


shell = strip_spans("\n".join(shell_lines))
whole = strip_spans(cmd)
hits = []

# 1. markdown authored through the shell: a redirect, a tee, sed -i, or a script write
for m in re.finditer(r'(?:>>?|\btee\b\s+(?:-a\s+)?)\s*(' + MD + r')', shell):
    if not is_scratch(m.group(1)):
        hits.append(f"redirect into {m.group(1)}")
for m in re.finditer(r'\bsed\b[^|;&\n]*?\s-i\b[^|;&\n]*?(' + MD + r')', shell):
    if not is_scratch(m.group(1)):
        hits.append(f"in-place edit of {m.group(1)}")
for m in re.finditer(r'''open\(\s*['"]([^'"]+\.md)['"]\s*,\s*['"][wa]''', whole):
    if not is_scratch(m.group(1)):
        hits.append(f"a script writing {m.group(1)}")

if hits:
    sys.stderr.write(
        "Prose must not be authored through Bash: " + "; ".join(dedupe(hits, 5)) + ".\n"
        "Use the Edit or Write tool instead, so the prose linter sees the text "
        "before it lands. Reading, building, and restoring through Bash stay fine.\n")
    sys.exit(2)

# 2. a C++ file may be written here, so lint the comment lines it carries
writes_cpp = bool(re.search(r'(?:>>?|\btee\b\s+(?:-a\s+)?)\s*' + CPP, shell)
                  or re.search(r'''open\(\s*['"][^'"]+\.(?:h|cpp|cppm)['"]\s*,\s*['"][wa]''', whole)
                  or re.search(r'\bsed\b[^|;&\n]*?\s-i\b[^|;&\n]*?' + CPP, shell))
if writes_cpp:
    findings = []
    for b in bodies:
        findings += lint_code(b, "written C++")
    # an echo or a printf carries comments too, in any of its quoted arguments
    for m in re.finditer(r'''(?<![\w\\])(?:'([^']*)'|"([^"]*)")''', cmd, re.S):
        text = m.group(1) if m.group(1) is not None else m.group(2)
        if "//" in text:
            findings += lint_code(text.replace("\\n", "\n").split("\n"), "written C++")
    if findings:
        sys.stderr.write("The C++ this command writes carries comment defects:\n  "
                         + "\n  ".join(dedupe(findings))
                         + "\nSay it in one line, or use the Edit tool.\n")
        sys.exit(2)
