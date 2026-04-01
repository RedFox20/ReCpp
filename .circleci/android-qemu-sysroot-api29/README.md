# Android QEMU Sysroot (API 29)

Minimal sysroot for running Android aarch64 binaries under QEMU user-mode emulation.

## Contents

- `system/bin/linker64` — bionic dynamic linker from Android 10 (API 29)
- `system/lib64/libc.so` — bionic libc (jemalloc allocator, compatible with QEMU)
- `system/lib64/libm.so` — bionic math library
- `system/lib64/libdl.so` — bionic dynamic loader stubs
- `system/lib64/liblog.so` — custom replacement that redirects `__android_log_*` to stdout/stderr with colored priority labels
- `system/lib64/libandroid.so` — NDK stub (satisfies linker, functions are no-ops)
- `system/lib64/libc++_shared.so` — copied from NDK at runtime by `run_android_tests`

## Why API 29?

Android 11+ (API 30+) switched to the Scudo memory allocator, which crashes under
QEMU user-mode due to address space layout differences. API 29 uses jemalloc which
works correctly.

## Rebuilding liblog.so

Source is at `.circleci/liblog_stderr.c`. Rebuild with:

```bash
.circleci/build_liblog.sh
```

## Extracting bionic libs from a system image

```bash
# Download and extract API 29 system image
curl -fSL -o sysimg.zip "https://dl.google.com/android/repository/sys-img/android/arm64-v8a-29_r08.zip"
unzip sysimg.zip "arm64-v8a/system.img"

# Find ext4 partition (partition 2, ext4 starts at 1MB offset within partition)
fdisk -l arm64-v8a/system.img
dd if=arm64-v8a/system.img of=part.img bs=512 skip=4096
dd if=part.img of=ext4.img bs=1M skip=1

# Mount with fuse2fs (no root needed) and copy bootstrap binaries
fuse2fs -o ro ext4.img /tmp/mnt
cp /tmp/mnt/system/bin/bootstrap/linker64 system/bin/
cp /tmp/mnt/system/lib64/bootstrap/{libc,libm,libdl}.so system/lib64/

# Strip debug symbols
$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip system/bin/linker64 system/lib64/*.so
```
