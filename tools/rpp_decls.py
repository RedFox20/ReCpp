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
def _index():
    try:
        import clang.cindex
    except ImportError as e:
        raise ClangMissing('the libclang bindings are missing, run `pip install libclang`') from e
    return clang.cindex.Index.create()


@functools.lru_cache(maxsize=1)
def _builtin_include() -> str:
    """The clang builtin include dir. Without it every parse fails on `stddef.h`."""
    found = sorted(glob.glob('/usr/lib/llvm-*/lib/clang/*/include'))
    if not found: raise ClangMissing('no clang builtin include dir under /usr/lib/llvm-*')
    return found[-1]


def flags(defines: tuple = ()) -> list:
    """The parse flags for one rpp header. Each define arrives without its `-D`."""
    return ['-x', 'c++', '-std=c++20', '-I', 'src', '-isystem', _builtin_include(),
            *(f'-D{d}' for d in defines)]


def parse(header: str, defines: tuple = ()):
    """Parses one header alone. Returns the translation unit, or raises on a fatal diagnostic.

    The `self-contained` gate already proves every header compiles alone, so a failure here
    means a flag is wrong rather than the header.
    """
    path = os.path.join(SRC, header)
    tu = _index().parse(path, args=flags(defines))
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
    import clang.cindex as cindex
    path = os.path.join(SRC, header)
    me = os.path.abspath(path)
    out = []

    def walk(cursor, ns):
        for k in cursor.get_children():
            if k.kind == cindex.CursorKind.NAMESPACE:
                walk(k, ns + [k.spelling] if k.spelling else ns)
                continue
            f = k.location.file
            if f and os.path.abspath(f.name) == me and k.spelling:
                out.append(('::'.join(ns), k.kind.name, k.spelling))

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
