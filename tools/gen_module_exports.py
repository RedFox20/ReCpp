#!/usr/bin/env python3
"""Writes the export list of a module interface unit from the header it wraps.

The generator owns one block between two markers, so a hand-written export list cannot drift.
Everything outside the markers survives a regeneration.

  tools/gen_module_exports.py strview.h            # write the block
  tools/gen_module_exports.py --all                # every header carrying a .cppm
  tools/gen_module_exports.py --all --check        # exit 1 when a block is stale
"""
import argparse
import functools
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rpp_decls as rd

BEGIN = '// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block'
END = '// GENERATED EXPORTS END'

# the surface changes with this macro, so the generator reads both and guards the difference
CONFIGS = (('RPP_ENABLE_UNICODE=1',), ('RPP_ENABLE_UNICODE=0',))
GUARD = 'RPP_ENABLE_UNICODE'

# a using-declaration cannot name these, and an importer never needs them
SKIP_KINDS = frozenset({'MACRO_DEFINITION', 'MACRO_INSTANTIATION', 'INCLUSION_DIRECTIVE',
                        'STATIC_ASSERT', 'NAMESPACE_ALIAS', 'USING_DIRECTIVE',
                        'USING_DECLARATION', 'FRIEND_DECL', 'UNEXPOSED_DECL'})

# the logging macros need these two, so they export despite the __ prefix
_MACRO_HELPERS = frozenset({'__wrap', '__clean_type'})
def _is_private(name: str) -> bool:
    return name.startswith('__') and name not in _MACRO_HELPERS


CONFIG_MODULE = 'rpp.config'


def module_name(header: str) -> str:
    """`strview.h` names module `rpp.strview`, and `config.types.h` names `rpp.config`."""
    if header == 'config.types.h': return CONFIG_MODULE
    return 'rpp.' + os.path.splitext(header)[0].replace('.', '_')


def cppm_path(header: str) -> str:
    """`strview.h` writes `src/rpp/rpp-strview.cppm`, and `config.types.h` writes `rpp-config.cppm`."""
    stem = 'config' if header == 'config.types.h' else os.path.splitext(header)[0]
    return os.path.join(rd.SRC, 'rpp-' + stem + '.cppm')


def _read(header: str) -> str:
    return open(os.path.join(rd.SRC, header), encoding='utf-8-sig', errors='replace').read()


@functools.lru_cache(maxsize=1)
def _macro_names() -> frozenset:
    """Every macro the rpp headers define, so a transitive include cannot hide one."""
    out = set()
    for h in sorted(os.listdir(rd.SRC)):
        if h.endswith('.h'): out |= set(re.findall(r'^\s*#\s*define\s+(\w+)', _read(h), re.M))
    return frozenset(out)


def macro_collision(name: str, cppm_src: str) -> bool:
    """True when the global module fragment leaves a macro named `name` defined.

    MSVC expands that name inside the module directive, so the fragment must undefine it.
    """
    gmf = cppm_src.partition('export module')[0]
    inc = gmf.rfind('#include')
    return inc < 0 or not re.search(rf'^\s*#\s*undef\s+{re.escape(name)}\s*$', gmf[inc:], re.M)


def _exported(header: str, defines: tuple) -> dict:
    """Namespace to ordered names, for one macro configuration."""
    out = {}
    for ns, kind, name in rd.declarations(header, defines):
        if kind in SKIP_KINDS or _is_private(name) or (ns and not ns.startswith('rpp')): continue
        names = out.setdefault(ns, [])
        if name not in names: names.append(name)
    return out


def export_block(header: str) -> str:
    """The generated block for one header, markers included."""
    on, off = (_exported(header, d) for d in CONFIGS)
    lines = [BEGIN]

    # one export import per rpp include. config.h maps to rpp.config, and a header never imports itself
    included = re.findall(r'^\s*#\s*include\s*"([^"]+)"', _read(header), re.M)
    imports = set()
    for path in included:
        inc = os.path.basename(path)  # a `./math.h` or `sub/x.h` include still names module rpp.math
        if not inc.endswith('.h'): continue
        if inc == 'config.h': imports.add(CONFIG_MODULE)
        elif inc not in rd.NO_MODULE: imports.add(module_name(inc))
    imports.discard(module_name(header))
    lines += [f'export import {imp};' for imp in sorted(imports)]

    for ns in sorted(set(on) | set(off)):
        both = [n for n in on.get(ns, []) if n in off.get(ns, [])]
        only = [n for n in on.get(ns, []) if n not in off.get(ns, [])]
        if not both and not only: continue
        if not ns:  # a C API keeps global scope, so a call site needs no change
            lines += [''] + [f'export using ::{n};' for n in both]
            continue
        # a literal operator lives in an inline namespace, so `using namespace` reaches it
        lines += ['', f'export namespace {ns.replace("::literals", "::inline literals")} {{']
        lines += [f'    using {ns}::{n};' for n in both]
        if only:
            lines.append(f'#if {GUARD}')
            lines += [f'    using {ns}::{n};' for n in only]
            lines.append('#endif')
        lines.append('}')

    lines.append(END)
    return '\n'.join(lines) + '\n'


def rewrite(header: str, check: bool) -> str:
    """Writes the block into the `.cppm`, or reports the difference. Returns a finding, or ''."""
    path = cppm_path(header)
    if not os.path.exists(path): return f'{path}: no module interface unit for {header}'
    old = open(path, encoding='utf-8-sig', errors='replace').read()
    if BEGIN not in old or END not in old:
        return f'{path}: carries no generated block, add the two markers first'
    name = module_name(header).rsplit('.', 1)[-1]
    if name in _macro_names() and macro_collision(name, old):
        return f'{path}: the module directive names macro {name}, add `#undef {name}` after the include'
    head, _, rest = old.partition(BEGIN)
    _, _, tail = rest.partition(END)
    new = head + export_block(header).rstrip('\n') + tail
    if new == old: return ''
    if check: return f'{path}: the export block is stale, run tools/gen_module_exports.py {header}'
    open(path, 'w', encoding='utf-8').write(new)
    print(f'  wrote {path}')
    return ''


def with_modules() -> list:
    """Every header which owns a module interface unit. The NO_MODULE filter drops config.h,
    which shares config.types.h's .cppm path."""
    return [h for h in sorted(os.listdir(rd.SRC))
            if h.endswith('.h') and h not in rd.NO_MODULE and os.path.exists(cppm_path(h))]


_GMF = 'module;\n#include "scope_guard.h"\n{}export module rpp.scope_guard;\n'


def selftest() -> list:
    """Crafts three module fragments and pins where an `#undef` counts."""
    bad = []
    if not macro_collision('scope_guard', _GMF.format('')):
        bad.append('a fragment with no #undef passed the module directive guard')
    if macro_collision('scope_guard', _GMF.format('#undef scope_guard\n')):
        bad.append('an #undef after the include reported a finding')
    # the include redefines the macro, so an #undef above it does nothing
    if not macro_collision('scope_guard', '#undef scope_guard\n' + _GMF.format('')):
        bad.append('an #undef before the include passed the guard')
    return bad


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('header', nargs='?', help='one header under src/rpp, such as strview.h')
    ap.add_argument('--all', action='store_true', help='every header carrying a .cppm')
    ap.add_argument('--check', action='store_true', help='exit 1 when a block is stale')
    ap.add_argument('--selftest', action='store_true', help='pin the module directive macro guard')
    a = ap.parse_args()
    if a.selftest:
        findings = selftest()
        for f in findings: print(f'  {f}')
        print(f'== gen_module_exports selftest: {len(findings)} finding(s) ==')
        return 1 if findings else 0
    if not a.header and not a.all: ap.error('name a header, or pass --all')

    try:
        targets = with_modules() if a.all else [a.header]
        bad = [f for f in (rewrite(h, a.check) for h in targets) if f]
    except rd.ClangMissing as e:
        print(f'cannot run: {e}')
        return 1
    for f in bad: print(f'  {f}')
    print(f'== gen_module_exports: {len(bad)} finding(s) over {len(targets)} module(s) ==')
    return 1 if bad else 0  # write mode still fails on a missing .cppm or marker


if __name__ == '__main__':
    sys.exit(main())
