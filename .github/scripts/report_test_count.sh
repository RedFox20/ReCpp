#!/usr/bin/env bash
# Writes the test count into the run summary, so a reader needs no scroll through the
# whole build log. Runs after a failure too, where it reports that no count exists.
# Usage: report_test_count.sh ubuntu-cpp20-asan-clang18
set -uo pipefail

JOB_NAME="$1"
LINE=""
if [ -f "$RUNNER_TEMP/test_output.txt" ]; then
    LINE=$(sed -e 's/\x1b\[[0-9;]*m//g' -e 's/\r//g' "$RUNNER_TEMP/test_output.txt" \
           | grep -a -m1 "SUCCESS: " || true)
fi
[ -n "$LINE" ] || LINE="no SUCCESS line: the tests failed, or the run stopped before them"

{ echo "### $JOB_NAME"; echo; echo '```'; echo "$LINE"; echo '```'; } >> "$GITHUB_STEP_SUMMARY"
