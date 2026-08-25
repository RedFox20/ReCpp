#!/usr/bin/env python3
"""Reads the rpp declarations of a header through libclang.

Two module tools share this layer. `check_includes.py rpp-includes` asks which rpp headers a
header references, and the export generator asks which names a header declares. A regex cannot
answer either one: it reports `size` and `operator` as declarations, and it cannot tell the
`strview` a header names from the `strview` a sibling include leaked to it.

Install the bindings with `pip install libclang`.
"""
import functools
import glob
import os

SRC = 'src/rpp'

# rule 1 of MODULES_MIGRATION.md section 6.3: every rpp header includes config.h to drive the
# compiler feature probes, and log_colors.h rides along with it. jni_cpp.h is Android glue.
NO_MODULE = frozenset({'config.h', 'log_colors.h', 'jni_cpp.h', 'debugging.macros.h'})


class ClangMissing(Exception):
    """Raised when the libclang bindings or the builtin include dir are absent."""


@functools.lru_cache(maxsize=1)
def _cindex():
    """The clang.cindex module, or ClangMissing when the bindings are absent."""
    try:
        import clang.cindex
    except ImportError as e:
        raise ClangMissing('the libclang bindings are missing, run `pip install libclang`') from e
    return clang.cindex


@functools.lru_cache(maxsize=1)
def _index():
    return _cindex().Index.create()


@functools.lru_cache(maxsize=1)
def _builtin_include() -> str:
    """A compiler builtin include dir holding `stddef.h`, which every parse needs.

    A clang resource dir is first choice. A gcc-only image carries none, so this asks the
    compilers for their own builtin dir, which libclang reads too.
    """
    import subprocess
    found = sorted(glob.glob('/usr/lib/llvm-*/lib/clang/*/include'))
    for cc in ('clang++', 'g++', 'cc', os.environ.get('CXX', '')):
        if not cc: continue
        try:
            out = subprocess.run([cc, '-print-file-name=include'], capture_output=True, text=True)
        except OSError:
            continue
        d = out.stdout.strip()
        if d and os.path.isabs(d): found.append(d)
    for d in found:
        if os.path.exists(os.path.join(d, 'stddef.h')): return d
    raise ClangMissing('no compiler builtin include dir with stddef.h, install clang or gcc')


def flags(defines: tuple = ()) -> list:
    """The parse flags for one rpp header. Each define arrives without its `-D`."""
    return ['-x', 'c++', '-std=c++20', '-I', 'src', '-isystem', _builtin_include(),
            *(f'-D{d}' for d in defines)]


def _resolve(header: str) -> str:
    """The full path of a header. An absolute path, from the selftest, passes through."""
    return header if os.path.isabs(header) else os.path.join(SRC, header)


def parse(header: str, defines: tuple = ()):
    """Parses one header alone. Returns the translation unit, or raises on a fatal diagnostic.

    The `self-contained` gate already proves every header compiles alone, so a failure here
    means a flag is wrong rather than the header.
    """
    tu = _index().parse(_resolve(header), args=flags(defines))
    fatal = [d for d in tu.diagnostics if d.severity >= 3]
    if fatal: raise RuntimeError(f'{header}: {fatal[0]}')
    return tu


def _cursors_in(tu, path: str):
    """Every cursor whose own location is `path`, so a cursor from an include never counts."""
    me = os.path.abspath(path)
    stack = list(tu.cursor.get_children())
    while stack:
        c = stack.pop()
        f = c.location.file
        if f and os.path.abspath(f.name) == me: yield c
        stack += list(c.get_children())


def declarations(header: str, defines: tuple = ()) -> list:
    """The declarations this header makes, as `(namespace, kind, name)`, in source order.

    A name reaches an importer only through a using-declaration, and one declaration carries
    every overload of that name, so the caller deduplicates.
    """
    cindex = _cindex()
    me = os.path.abspath(_resolve(header))
    out = []

    def walk(cursor, ns):
        for k in cursor.get_children():
            if k.kind == cindex.CursorKind.NAMESPACE:
                walk(k, ns + [k.spelling] if k.spelling else ns)
                continue
            # RPPCAPI is `extern "C"`, so the C logging API sits one linkage spec deep
            if k.kind == cindex.CursorKind.LINKAGE_SPEC:
                walk(k, ns)
                continue
            f = k.location.file
            if not (f and os.path.abspath(f.name) == me and k.spelling): continue
            out.append(('::'.join(ns), k.kind.name, k.spelling))
            # an unscoped enum puts its enumerators at namespace scope, and a using of the
            # enum type does not carry them, so each one needs its own using-declaration
            if k.kind == cindex.CursorKind.ENUM_DECL and not k.is_scoped_enum():
                for e in k.get_children():
                    if e.kind == cindex.CursorKind.ENUM_CONSTANT_DECL and e.spelling:
                        out.append(('::'.join(ns), e.kind.name, e.spelling))

    walk(parse(header, defines).cursor, [])
    return out


def names_from(header: str, other: str, defines: tuple = ()) -> set:
    """The names `header` references which `other` declares. Used for a header carrying no module."""
    target = os.path.abspath(os.path.join(SRC, other))
    found = set()
    for c in _cursors_in(parse(header, defines), os.path.join(SRC, header)):
        r = c.referenced
        if r is None or not r.location.file or not r.spelling: continue
        if os.path.abspath(r.location.file.name) == target: found.add(r.spelling)
    return found


_SELFTEST_HEADER = '''#pragma once
namespace rpp {
    enum Sev { SevInfo, SevWarn };                 // unscoped, its enumerators sit at rpp scope
    enum class Scoped { A, B };                    // scoped, its enumerators do not
    template<class T> struct __wrap {};            // a macro helper, exported by allowlist
    template<class T> struct __hidden {};          // a private double-underscore name
    struct Public {};
}
extern "C" { void c_api(); }                       // one linkage spec deep, like RPPCAPI
'''


def selftest() -> list:
    """Crafts a header and checks the four extraction rules the module surface depends on.

    A regression in the linkage-spec walk, the enum-constant walk, or the name filters changes a
    real module surface, so this fails before that reaches a `.cppm`.
    """
    import tempfile
    bad = []
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, 'probe.h')
        open(path, 'w').write(_SELFTEST_HEADER)
        got = {(ns, name) for ns, kind, name in declarations(path)}
        want = {('rpp', 'Sev'), ('rpp', 'SevInfo'), ('rpp', 'SevWarn'),  # unscoped enum + members
                ('rpp', 'Scoped'), ('rpp', '__wrap'), ('rpp', '__hidden'),
                ('rpp', 'Public'), ('', 'c_api')}                        # extern "C" reaches c_api
        missing = want - got
        if missing: bad.append(f'declarations dropped {sorted(missing)}')
        # a scoped enum keeps its members out of the namespace
        if ('rpp', 'A') in got: bad.append('a scoped enum leaked its enumerator A')
    return bad


def referenced_rpp_headers(header: str, defines: tuple = ()) -> dict:
    """The rpp headers this header names, mapped to the names it takes from each.

    libclang resolves each reference to its declaration, so this reports the header that really
    declares the name. A `config.h` style header carries no module and never appears here.
    """
    path = os.path.join(SRC, header)
    src_root = os.path.abspath(SRC)
    out = {}
    for c in _cursors_in(parse(header, defines), path):
        r = c.referenced
        if r is None or not r.location.file: continue
        rp = os.path.abspath(r.location.file.name)
        name = os.path.basename(rp)
        if rp == os.path.abspath(path) or not rp.startswith(src_root) or name in NO_MODULE:
            continue
        if r.spelling: out.setdefault(name, set()).add(r.spelling)
    return out


if __name__ == '__main__':
    import sys
    if len(sys.argv) == 2 and sys.argv[1] == 'selftest':
        try:
            findings = selftest()
        except ClangMissing as e:
            print(f'rpp_decls selftest skipped, install libclang: {e}')
            sys.exit(0)
        for f in findings: print(f'  {f}')
        print(f'== rpp_decls selftest: {len(findings)} finding(s) ==')
        sys.exit(1 if findings else 0)
    sys.exit('usage: rpp_decls.py selftest')
