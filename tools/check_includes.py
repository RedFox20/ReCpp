#!/usr/bin/env python3
"""Include hygiene checker for src/rpp.

Three checks, each with a --check gate for CI:

  self-contained  every header compiles alone, twice, with nothing included before it
  unused          an #include that the header still compiles without
  missing         a file that uses a std facility it does not include itself

The `unused` check works by deletion. It copies the tree, comments out one
#include, and recompiles that header alone. A header that still compiles did not
need the line. Run `missing` first: a header can look self-contained only because
a sibling header leaks the declaration into it.

Usage:
  tools/check_includes.py self-contained [--check]
  tools/check_includes.py unused [--check] [--only strview.h]
  tools/check_includes.py missing [--check]
"""
import argparse, os, re, shutil, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

SRC = 'src/rpp'
# jni_cpp.h needs the Android NDK jni.h, which a host build does not have
SKIP = {'jni_cpp.h'}
CXX = os.environ.get('CXX', 'clang++')
STD = os.environ.get('CXXSTD', 'c++20')

# a std facility, the symbols that need it, and the headers that provide it
RULES = [
    (r'\b(memcpy|memmove|memset|memcmp|strlen|strcmp|strncmp|strcpy|strstr)\s*\(', ('cstring', 'string.h')),
    (r'\bstd::(string|wstring|u16string|to_string|stoi|stod)\b', ('string',)),
    (r'\bstd::vector\b', ('vector',)),
    (r'\bstd::(shared_ptr|unique_ptr|make_shared|make_unique|weak_ptr)\b', ('memory',)),
    (r'\b(printf|fprintf|snprintf|fopen|fclose|fwrite|fread|FILE)\s*[\(\*]', ('cstdio', 'stdio.h')),
    (r'\bstd::(sort|find|min_element|max_element|copy|fill|remove_if)\s*\(', ('algorithm',)),
    (r'\bstd::(atomic|atomic_int64_t|memory_order)\b', ('atomic',)),
    (r'\bstd::(mutex|lock_guard|unique_lock|recursive_mutex)\b', ('mutex',)),
]
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)
# a quoted include is an rpp header, an angled one is not. <math.h> is not src/rpp/math.h.
QUOTED_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)


def headers() -> list[str]:
    return sorted(f for f in os.listdir(SRC) if f.endswith('.h') and f not in SKIP)


def compile_header(header: str, include_root: str) -> tuple[bool, str]:
    """Compiles a translation unit that includes the header twice and nothing else."""
    with tempfile.TemporaryDirectory() as td:
        tu = os.path.join(td, 'tu.cpp')
        # the second include proves the include guard works
        with open(tu, 'w') as f:
            f.write(f'#include <rpp/{header}>\n#include <rpp/{header}>\n')
        cmd = [CXX, f'-std={STD}', '-fsyntax-only', '-I', include_root, tu]
        p = subprocess.run(cmd, capture_output=True, text=True)
        return p.returncode == 0, p.stderr


def strip_comments(src: str) -> str:
    return re.sub(r'//[^\n]*|/\*.*?\*/', '', src, flags=re.S)


def check_self_contained() -> list[str]:
    root = os.path.abspath('src')
    bad = []
    with ThreadPoolExecutor(max_workers=os.cpu_count()) as ex:
        for h, (ok, err) in zip(headers(), ex.map(lambda h: compile_header(h, root), headers())):
            if not ok:
                first = next((l for l in err.splitlines() if ' error: ' in l), err.splitlines()[0] if err else '?')
                bad.append(f'{SRC}/{h}: {first.strip()}')
    return bad


# names a header declares itself: types, aliases, concepts, macros, exported functions
DECL_RES = [
    re.compile(r'\b(?:class|struct|union|enum(?:\s+class)?)\s+(?:RPPAPI\s+)?([A-Za-z_]\w*)'),
    re.compile(r'\busing\s+([A-Za-z_]\w*)\s*='),
    re.compile(r'\bconcept\s+([A-Za-z_]\w*)'),
    re.compile(r'^\s*#\s*define\s+([A-Za-z_]\w*)', re.M),
    re.compile(r'\bRPPAPI\s+[\w:<>*&\s]+?\b([A-Za-z_]\w*)\s*\('),
    re.compile(r'\b(?:constexpr|inline)\s+[\w:<>,*&\s]+?\b([A-Za-z_]\w*)\s*[=({]'),
]


def declared_names(path: str) -> set[str]:
    src = strip_comments(open(path, encoding='utf-8', errors='replace').read())
    names = set()
    for r in DECL_RES:
        names |= set(r.findall(src))
    return {n for n in names if len(n) > 2}


def _probe_unused(args) -> list[tuple[str, str]]:
    """Returns the includes of one header that the header compiles without."""
    header, worktree = args
    path = os.path.join(worktree, 'rpp', header)
    original = open(path, encoding='utf-8', errors='replace').read()
    found = []
    quoted_spans = {m.start() for m in QUOTED_RE.finditer(original)}
    try:
        for m in INCLUDE_RE.finditer(original):
            patched = original[:m.start()] + '//' + original[m.start():]
            open(path, 'w', encoding='utf-8').write(patched)
            ok, _ = compile_header(header, worktree)
            if ok:
                found.append((header, m.group(1), m.start() in quoted_spans))
    finally:
        open(path, 'w', encoding='utf-8').write(original)
    return found


def check_unused(only: str | None) -> list[str]:
    """Splits the removable includes into truly unused and merely redundant.

    A header can compile without an include because a sibling include leaks the
    same declarations. Removing that one trades a direct dependency for a hidden
    one, so report it apart from an include the header never uses.
    """
    hs = [h for h in headers() if not only or h == only]
    workers = min(os.cpu_count() or 4, len(hs)) or 1
    with tempfile.TemporaryDirectory() as td:
        trees = [shutil.copytree('src', os.path.join(td, f'w{i}')) for i in range(workers)]
        with ThreadPoolExecutor(max_workers=workers) as ex:
            groups = ex.map(_probe_unused, ((h, trees[i % workers]) for i, h in enumerate(hs)))
            candidates = [c for g in groups for c in g]

    unused, redundant, std = [], [], []
    for header, inc, quoted in candidates:
        provider = os.path.join(SRC, inc)
        line = f'{SRC}/{header}: #include {inc}'
        if not (quoted and os.path.isfile(provider)):
            std.append(line)  # this scan cannot enumerate what a std header declares
            continue
        body = INCLUDE_RE.sub('', strip_comments(open(f'{SRC}/{header}', encoding='utf-8',
                                                      errors='replace').read()))
        used = any(re.search(rf'\b{re.escape(n)}\b', body) for n in declared_names(provider))
        (redundant if used else unused).append(line)

    out = [f'-- unused ({len(unused)}): the header names nothing from it, remove'] + unused
    out += [f'-- redundant ({len(redundant)}): a sibling include leaks it, keep unless verified'] + redundant
    out += [f'-- std ({len(std)}): removable, judge by hand'] + std
    return out, len(unused)


def check_missing() -> list[str]:
    files = ([f'{SRC}/{f}' for f in sorted(os.listdir(SRC)) if f.endswith(('.h', '.cpp'))]
             + [f'tests/{f}' for f in sorted(os.listdir('tests')) if f.endswith('.cpp')])
    bad = []
    for f in files:
        src = open(f, encoding='utf-8', errors='replace').read()
        body = strip_comments(src)
        incs = set(INCLUDE_RE.findall(src))
        need = {p[0] for pat, p in RULES if re.search(pat, body) and not set(p) & incs}
        if need:
            bad.append(f'{f}: needs {", ".join(sorted(need))}')
    return bad


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('mode', choices=['self-contained', 'unused', 'missing', 'all'])
    ap.add_argument('--check', action='store_true', help='exit 1 when a finding remains')
    ap.add_argument('--only', help='limit the unused scan to one header')
    a = ap.parse_args()

    modes = ['self-contained', 'missing', 'unused'] if a.mode == 'all' else [a.mode]
    gated = 0
    for mode in modes:
        if mode == 'self-contained': found, gate = (lambda r: (r, len(r)))(check_self_contained())
        elif mode == 'missing':      found, gate = (lambda r: (r, len(r)))(check_missing())
        else:                        found, gate = check_unused(a.only)
        print(f'== {mode}: {gate} findings that fail the gate ==')
        for line in found: print(f'  {line}')
        gated += gate
    return 1 if (a.check and gated) else 0


if __name__ == '__main__':
    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    sys.exit(main())
