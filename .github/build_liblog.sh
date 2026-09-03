#!/bin/bash
# Builds the custom liblog.so for QEMU user-mode testing.
# Redirects __android_log_* to stdout/stderr with colored priority labels.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NDK_HOME="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"

if [[ -z "$NDK_HOME" ]]; then
    echo "ERROR: ANDROID_NDK_HOME or ANDROID_NDK_ROOT must be set" >&2
    exit 1
fi

CC="$NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang"
if [ ! -f "$CC" ]; then
    echo "ERROR: NDK clang not found at $CC" >&2
    exit 1
fi

OUTPUT="$SCRIPT_DIR/android-qemu-sysroot-api29/system/lib64/liblog.so"
$CC -shared -o "$OUTPUT" "$SCRIPT_DIR/liblog_stderr.c" -O2 -Wl,-soname,liblog.so
echo "Built $OUTPUT"
