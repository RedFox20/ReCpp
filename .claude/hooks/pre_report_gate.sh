#!/usr/bin/env bash
# Stop hook: reports mechanical defects only. Silent when clean, so it terminates.
# exit 0 allows the stop. exit 2 blocks once and hands stderr back to the agent.
set -uo pipefail
cd "${CLAUDE_PROJECT_DIR:-.}" 2>/dev/null || exit 0
command -v git >/dev/null 2>&1 || exit 0
git rev-parse --git-dir >/dev/null 2>&1 || exit 0

STAMP="${TMPDIR:-/tmp}/.recpp_stop_gate"
STATE=$(git status --porcelain 2>/dev/null | sha1sum | cut -d' ' -f1)
if [ -f "$STAMP" ] && [ "$(cat "$STAMP" 2>/dev/null)" = "$STATE" ]; then
    exit 0
fi

MSGS=()

# The end of turn net. prose_rules.py is the one definition, so this layer can
# never disagree with the two PreToolUse hooks and block a correct edit.
if command -v python3 >/dev/null 2>&1; then
    PROSE=$(git diff HEAD -- '*.md' '*.h' '*.cpp' '*.cppm' 2>/dev/null \
            | python3 "$(dirname "$0")/prose_rules.py" 2>/dev/null)
    [ -n "$PROSE" ] && MSGS+=("Prose defects in the diff: $(echo "$PROSE" | tr '\n' ' | ')")
fi

# README line refs, only when a header under src/rpp/ actually changed.
# Scoped to the changed headers, so pre-existing debt never blocks a turn.
CHANGED_H=$(git diff HEAD --name-only -- 'src/rpp/*.h' 2>/dev/null)
if [ -n "$CHANGED_H" ] && command -v python3 >/dev/null 2>&1 && [ -f update_doc_linerefs.py ]; then
    STALE=$(python3 update_doc_linerefs.py --dry-run 2>/dev/null \
            | grep -oE '[0-9]+ reference\(s\) would be updated' | grep -oE '^[0-9]+')
    if [ -n "${STALE:-}" ] && [ "$STALE" -gt 0 ]; then
        MSGS+=("README: $STALE line reference(s) are stale. Run: python3 update_doc_linerefs.py")
    fi
    # only a symbol this diff ADDED, never pre-existing debt in a header you touched
    ADDED_H=$(git diff HEAD -- 'src/rpp/*.h' 2>/dev/null | grep -E '^\+' | sed 's/^+//')
    UNDOC=$(python3 update_doc_linerefs.py --check-undocumented 2>/dev/null \
            | grep -oE '^[[:space:]]+[A-Za-z0-9_]+\.h:[0-9]+: [A-Za-z0-9_]+' | awk '{print $NF}')
    NEWUNDOC=""
    for sym in $UNDOC; do
        if printf '%s\n' "$ADDED_H" | grep -qE "(^|[^A-Za-z0-9_])${sym}([^A-Za-z0-9_]|$)"; then
            NEWUNDOC="$NEWUNDOC $sym"
        fi
    done
    if [ -n "${NEWUNDOC# }" ]; then
        MSGS+=("README: this change adds public API which README.md does not document:${NEWUNDOC}")
    fi
fi

# R9b: a closed BUGS.md entry is exactly two sentences
if git diff HEAD --name-only 2>/dev/null | grep -qx 'BUGS.md' && command -v python3 >/dev/null 2>&1; then
    LONG=$(python3 - <<'PY' 2>/dev/null
import re
try: s=open('BUGS.md').read()
except OSError: raise SystemExit
if '## Closed' not in s: raise SystemExit
for b in re.split(r'\n### ', s[s.index('## Closed'):])[1:]:
    title=b.split('\n')[0].split('.')[0]
    body=re.sub(r'`[^`]*`','X',' '.join(b.split('\n')[1:])).strip()
    sents=[x for x in re.split(r'(?<=[.!?])\s+', body) if x.strip()]
    if len(sents)!=2: print(f"{title}={len(sents)} sentences")
    for w in (len(x.split()) for x in sents):
        if w>25: print(f"{title}={w} words in one sentence")
PY
)
    [ -n "$LONG" ] && MSGS+=("R9b: a closed BUGS.md entry is 2 sentences, max 25 words each. Violations: $(echo $LONG)")
fi

if [ ${#MSGS[@]} -eq 0 ]; then
    exit 0   # clean: never blocks, so no loop
fi

echo "$STATE" > "$STAMP"
printf '%s\n' "${MSGS[@]}" >&2
exit 2
