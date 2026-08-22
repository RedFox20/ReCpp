#!/usr/bin/env python3
"""PreToolUse on Edit|Write: lint prose BEFORE it enters a file.
exit 0 allows the write. exit 2 blocks it and hands stderr back."""
import json, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from prose_rules import lint_path, dedupe

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

oldset = set(old.split("\n"))                       # only lines this edit introduces
added = [l for l in new.split("\n") if l not in oldset]
findings = lint_path(path, added, creating=not old)

if findings:
    sys.stderr.write("Prose check failed BEFORE the write. Fix these, then retry:\n  "
                     + "\n  ".join(dedupe(findings)) + "\n")
    sys.exit(2)
