#!/usr/bin/env python3
"""PreToolUse on Edit|Write: lint prose BEFORE it enters a file.
exit 0 allows the write. exit 2 blocks it and hands stderr back."""
import json, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from prose_rules import lint_path, dedupe, fenced, fence_mark

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
added = [l if l not in oldset else (fence_mark(l) or "") for l in new.split("\n")]


def lands_inside_a_fence():
    """The fence an edit lands inside, so the lint reads the added code as code."""
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError:
        return None
    at = text.find(old) if old else -1
    return fenced(text[:at].split("\n")) if at >= 0 else None


seed = lands_inside_a_fence() if path.endswith(".md") else None
if seed: added.insert(0, seed)
findings = lint_path(path, added, creating=not old)

if findings:
    sys.stderr.write("Prose check failed BEFORE the write. Fix these, then retry:\n  "
                     + "\n  ".join(dedupe(findings)) + "\n")
    sys.exit(2)
