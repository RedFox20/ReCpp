#!/usr/bin/env python3
"""Builds this consumer with mama and runs it. Asserts the app links and prints the right token sets.

The exported module needs cmake 3.28, Ninja or Visual Studio, and a compiler that reports its import
graph. A toolchain that misses one compiles the header instead, and the app must still run. Pass
`--expect modules` or `--expect headers` to pin which path the toolchain took.
"""
import argparse
import os
import shutil
import stat
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def run(cmd, env=None, **kw) -> subprocess.CompletedProcess:
    print('+', ' '.join(cmd), flush=True)
    return subprocess.run(cmd, cwd=HERE, text=True, env=env, **kw)


def find_exe() -> str:
    """The built consumer, whatever build dir name the platform chose."""
    for root, _, files in os.walk(os.path.join(HERE, 'packages', 'RppModuleConsumer')):
        for name in ('RppModuleConsumer', 'RppModuleConsumer.exe'):
            path = os.path.join(root, name)
            if name in files and os.access(path, os.X_OK): return path
    raise SystemExit('FAILED: no RppModuleConsumer executable under packages/RppModuleConsumer')


def _drop_readonly(func, path, _exc):
    """Windows refuses to unlink a read-only file, which a build tree leaves behind."""
    os.chmod(path, stat.S_IWRITE)
    func(path)


def build_and_run(compiler, jobs, env=None) -> str:
    """Build this consumer and return what it printed. Raises SystemExit when either step fails."""
    if os.path.isdir(os.path.join(HERE, 'packages')):
        shutil.rmtree(os.path.join(HERE, 'packages'), onerror=_drop_readonly)
    build = ['mama'] + ([compiler] if compiler else []) + ['build', f'jobs={jobs}']
    if run(build, env=env).returncode != 0: raise SystemExit('FAILED: mama build')
    result = run([find_exe()], capture_output=True, env=env)
    print(result.stdout, end='')
    if result.stderr: print(result.stderr, end='', file=sys.stderr)
    if result.returncode != 0: raise SystemExit(f'FAILED: the consumer exited {result.returncode}')
    if 'OK:' not in result.stdout: raise SystemExit('FAILED: the consumer printed no OK line')
    return result.stdout


def check_unicode_off(cxx: str) -> str:
    """Compiles the exported module with `RPP_ENABLE_UNICODE=0` and uses a numeric `to_string`.

    Every consumer job runs on Linux or MSVC, where `config.h` turns unicode on, so no build
    reaches the guarded half of the module surface. `strview.h` declares the numeric overloads
    outside that guard, and one using-declaration carries a whole overload set, so a guarded
    `using rpp::to_string;` would drop them on Yocto, MIPS and Raspberry Pi.
    """
    root = os.path.dirname(os.path.dirname(HERE))
    with tempfile.TemporaryDirectory() as d:
        use = os.path.join(d, 'use.cpp')
        with open(use, 'w') as f:
            f.write('import rpp.strview;\n'
                    'int main() { char b[32]; return rpp::to_string(b, 42) == 2 ? 0 : 1; }\n')
        flags = [cxx, '-std=c++20', '-fmodules-ts', '-DRPP_ENABLE_UNICODE=0', '-I', 'src']
        mapper = f'-fmodule-mapper=|@g++-mapper-server -r {d}'
        # rpp.strview imports rpp.config, so the config module compiles first
        for step in ([*flags, mapper, '-x', 'c++', '-c', 'src/rpp/rpp-config.cppm', '-o', f'{d}/c.o'],
                     [*flags, mapper, '-x', 'c++', '-c', 'src/rpp/rpp-strview.cppm', '-o', f'{d}/m.o'],
                     [*flags, mapper, '-c', use, '-o', f'{d}/u.o']):
            p = subprocess.run(step, cwd=root, capture_output=True, text=True)
            if p.returncode != 0:
                return next((l for l in p.stderr.splitlines() if 'error:' in l), p.stderr[:200])
    return ''


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--compiler', default='', help='mama compiler arg, eg clang, gcc or windows')
    ap.add_argument('--expect', choices=['modules', 'headers', 'any'], default='any')
    ap.add_argument('--jobs', default='3')
    ap.add_argument('--compare-paths', action='store_true',
                    help='also build through the header fallback and compare the two reports')
    ap.add_argument('--unicode-off', default='',
                    help='g++ binary that compiles the module with RPP_ENABLE_UNICODE=0')
    args = ap.parse_args()

    if args.unicode_off:
        print(f'--- compiling the module with RPP_ENABLE_UNICODE=0 using {args.unicode_off} ---')
        err = check_unicode_off(args.unicode_off)
        if err:
            print(f'FAILED: the module drops a name when unicode is off: {err}')
            return 1
        print('the unicode-disabled module still exports the numeric to_string')

    out = build_and_run(args.compiler, args.jobs)

    took = 'modules' if 'built with MODULES' in out else 'headers'
    print(f'consumer took the {took} path')
    if args.expect != 'any' and took != args.expect:
        print(f'FAILED: expected the {args.expect} path')
        return 1

    # One compiler, both paths, one report. A facade that re-exports through using-declarations is
    # where a compiler tends to differ, and only this comparison can see that.
    if args.compare_paths and took == 'modules':
        print('--- rebuilding through the header fallback to compare ---')
        other = build_and_run(args.compiler, args.jobs, env={**os.environ, 'NO_MODULES': '1'})
        # the comparison drops the build-mode line, so without this it can compare modules
        # with modules and report parity it never tested
        if 'built with HEADERS' not in other:
            print('FAILED: NO_MODULES still built the module path, so the two paths never differed')
            return 1
        if other.splitlines()[1:] != out.splitlines()[1:]:
            print('FAILED: the module path and the header path disagree')
            return 1
        print('the module path and the header path report the same result')
    return 0


if __name__ == '__main__':
    sys.exit(main())
