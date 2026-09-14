#!/usr/bin/env python3
"""PreToolUse on Edit|Write: lint prose BEFORE it enters a file.
exit 0 allows the write. exit 2 blocks it and hands stderr back."""
import json, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from prose_rules import lint_path, dedupe, edit_lines

try:
    ev = json.load(sys.stdin)
except Exception:
    sys.exit(0)

ti = ev.get("tool_input") or {}
path = ti.get("file_path", "")
new = ti.get("new_string", ti.get("content", "")) or ""
old = ti.get("old_string", "") or ""
if not path or not new:
    sys.exit(0)

try:
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()                             # the markers above the edit decide its meaning
except OSError:
    text = ""

at = text.find(old) if old else -1
findings = lint_path(path, edit_lines(path, old, new, text[:at] if at >= 0 else ""),
                     creating=not old)

if findings:
    sys.stderr.write("Prose check failed BEFORE the write. Fix these, then retry:\n  "
                     + "\n  ".join(dedupe(findings)) + "\n")
    sys.exit(2)
