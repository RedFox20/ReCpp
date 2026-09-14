#!/usr/bin/env python3
"""PreToolUse on Edit|Write: lint prose BEFORE it enters a file.
exit 0 allows the write. exit 2 blocks it and hands stderr back."""
import json, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from prose_rules import lint_path, dedupe, fenced, FENCE

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
added = [l if l not in oldset else (FENCE if l.lstrip().startswith(FENCE) else "")
         for l in new.split("\n")]


def lands_inside_a_fence():
    """An edit inside a fenced block adds code, and the lint must not read it as prose."""
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError:
        return False
    at = text.find(old) if old else -1
    return at >= 0 and fenced(text[:at].split("\n"))


if path.endswith(".md") and lands_inside_a_fence():
    added.insert(0, FENCE)
findings = lint_path(path, added, creating=not old)

if findings:
    sys.stderr.write("Prose check failed BEFORE the write. Fix these, then retry:\n  "
                     + "\n  ".join(dedupe(findings)) + "\n")
    sys.exit(2)
