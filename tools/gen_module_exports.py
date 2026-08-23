#!/usr/bin/env python3
"""Writes the export list of a module interface unit from the header it wraps.

A hand-written export list rots. `rpp-strview.cppm` proved it: a second
`using rpp::literals::operator""_sv;` sat under the unicode guard, where it did nothing,
because the first using-declaration already carried every overload of that name.

The generator owns one block between two markers. Everything outside stays, so a comment or a
deduction guide a person wrote survives a regeneration.

  tools/gen_module_exports.py strview.h            # write the block
  tools/gen_module_exports.py --all                # every header carrying a .cppm
  tools/gen_module_exports.py --all --check        # exit 1 when a block is stale
"""
import argparse
import os
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


def module_name(header: str) -> str:
    """`strview.h` names module `rpp.strview`."""
    return 'rpp.' + os.path.splitext(header)[0].replace('.', '_')


def cppm_path(header: str) -> str:
    """`strview.h` writes `src/rpp/rpp-strview.cppm`."""
    return os.path.join(rd.SRC, 'rpp-' + os.path.splitext(header)[0] + '.cppm')


def _exported(header: str, defines: tuple) -> dict:
    """Namespace to ordered names, for one macro configuration."""
    out = {}
    for ns, kind, name in rd.declarations(header, defines):
        # `rpp::__wrap` carries the debugging macros, so a leading underscore hides nothing here
        if kind in SKIP_KINDS or (ns and not ns.startswith('rpp')): continue
        names = out.setdefault(ns, [])
        if name not in names: names.append(name)
    return out


def export_block(header: str) -> str:
    """The generated block for one header, markers included."""
    on, off = (_exported(header, d) for d in CONFIGS)
    lines = [BEGIN]

    # D5: one import per rpp include, so an importer sees the names this header takes from it
    src = open(os.path.join(rd.SRC, header), encoding='utf-8-sig', errors='replace').read()
    import re
    included = re.findall(r'^\s*#\s*include\s*"([^"]+)"', src, re.M)
    for inc in sorted({i for i in included if i not in rd.NO_MODULE and i.endswith('.h')}):
        lines.append(f'export import {module_name(inc)};')

    # a header carrying no module reaches an importer only through this re-export. It ships its
    # whole rpp surface, because a consumer names an alias this header happens not to use.
    for plain in sorted(rd.NO_MODULE):
        if plain not in included: continue
        taken = sorted({n for names in _exported(plain, CONFIGS[0]).values() for n in names})
        if not taken: continue
        lines += ['', f'export namespace rpp {{ // from {plain}, which carries no module']
        lines += [f'    using rpp::{n};' for n in taken]
        lines.append('}')

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
    head, _, rest = old.partition(BEGIN)
    _, _, tail = rest.partition(END)
    new = head + export_block(header).rstrip('\n') + tail
    if new == old: return ''
    if check: return f'{path}: the export block is stale, run tools/gen_module_exports.py {header}'
    open(path, 'w', encoding='utf-8').write(new)
    print(f'  wrote {path}')
    return ''


def with_modules() -> list:
    """Every header which already carries a module interface unit."""
    return [h for h in sorted(os.listdir(rd.SRC))
            if h.endswith('.h') and os.path.exists(cppm_path(h))]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('header', nargs='?', help='one header under src/rpp, such as strview.h')
    ap.add_argument('--all', action='store_true', help='every header carrying a .cppm')
    ap.add_argument('--check', action='store_true', help='exit 1 when a block is stale')
    a = ap.parse_args()
    if not a.header and not a.all: ap.error('name a header, or pass --all')

    try:
        targets = with_modules() if a.all else [a.header]
        bad = [f for f in (rewrite(h, a.check) for h in targets) if f]
    except rd.ClangMissing as e:
        print(f'cannot run: {e}')
        return 1
    for f in bad: print(f'  {f}')
    print(f'== gen_module_exports: {len(bad)} finding(s) over {len(targets)} module(s) ==')
    return 1 if (a.check and bad) else 0


if __name__ == '__main__':
    sys.exit(main())
