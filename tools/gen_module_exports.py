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
# this set omits UNEXPOSED_DECL, because libclang reports a variable template under that
# kind and a using-declaration names one. The filters below drop the unnamed and private
SKIP_KINDS = frozenset({'MACRO_DEFINITION', 'MACRO_INSTANTIATION', 'INCLUSION_DIRECTIVE',
                        'STATIC_ASSERT', 'NAMESPACE_ALIAS', 'USING_DIRECTIVE',
                        'USING_DECLARATION', 'FRIEND_DECL'})

# the logging macros need these two, so they export despite the __ prefix
_MACRO_HELPERS = frozenset({'__wrap', '__clean_type'})
def _is_private(name: str) -> bool:
    return name.startswith('__') and name not in _MACRO_HELPERS


def _skip(ns: str, kind: str, name: str) -> bool:
    """True for a declaration no using-declaration can name, or which stays private."""
    # a deduction guide spells as `<deduction guide for X>`, which no using-declaration names
    return kind in SKIP_KINDS or name.startswith('<') or _is_private(name) \
        or (ns and not ns.startswith('rpp'))


# a private helper no importer needs. Empty, because every module gives its public names
# external linkage instead, which is what MSVC needs to define one in an importing TU
INTERNAL_OK = frozenset()


CONFIG_MODULE = 'rpp.config'

# a header whose module name is not its stem. scope_guard.h drops the underscore, because
# MSVC expands the scope_guard macro inside a module directive
STEMS = {'config.types.h': 'config', 'scope_guard.h': 'scopeguard'}


def module_stem(header: str) -> str:
    """The last component of the module name, which is the `.cppm` stem too."""
    return STEMS.get(header) or os.path.splitext(header)[0].replace('.', '_')


def module_name(header: str) -> str:
    """`strview.h` names module `rpp.strview`, and `config.types.h` names `rpp.config`."""
    return 'rpp.' + module_stem(header)


def cppm_path(header: str) -> str:
    """`strview.h` writes `src/rpp/rpp-strview.cppm`, and `config.types.h` writes `rpp-config.cppm`."""
    return os.path.join(rd.SRC, 'rpp-' + module_stem(header) + '.cppm')


def _read(header: str) -> str:
    return open(os.path.join(rd.SRC, header), encoding='utf-8-sig', errors='replace').read()


@functools.lru_cache(maxsize=1)
def _macro_names() -> frozenset:
    """Every macro the rpp headers define, so a transitive include cannot hide one."""
    out = set()
    for h in sorted(os.listdir(rd.SRC)):
        if h.endswith('.h'): out |= set(re.findall(r'^\s*#\s*define\s+(\w+)', _read(h), re.M))
    return frozenset(out)


def macro_collision(name: str) -> bool:
    """True when a module named `name` repeats a macro an rpp header defines.

    MSVC expands that name inside `export module` and inside `import`, so an importer which
    included the header breaks too. Only a rename fixes both sides.
    """
    return name in _macro_names()


def _exported(header: str, defines: tuple) -> dict:
    """Namespace to ordered names, for one macro configuration."""
    out = {}
    for ns, kind, name, internal in rd.declarations(header, defines):
        if _skip(ns, kind, name): continue
        if internal: continue  # clang rejects a using-declaration which exports one
        names = out.setdefault(ns, [])
        if name not in names: names.append(name)
    return out


def internal_names(header: str, allow: frozenset = None) -> list:
    """The public names this header hides behind internal linkage, in source order.

    A `static constexpr` function and a namespace-scope `constexpr` both land here, and no
    module can export either. `allow` names the helpers whose loss is intended.
    """
    allow = INTERNAL_OK if allow is None else allow
    out = []
    for ns, kind, name, internal in rd.declarations(header, CONFIGS[0]):
        if _skip(ns, kind, name): continue
        if internal and name not in allow and name not in out: out.append(name)
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
    name = module_stem(header)
    if macro_collision(name):
        return f'{header}: module rpp.{name} repeats a macro, so rename it in STEMS'
    hidden = internal_names(header)
    if hidden:
        return (f'{header}: internal linkage hides {hidden} from the module. Make each one `inline`, '
                f'or name it in INTERNAL_OK when no importer needs it')
    head, _, rest = old.partition(BEGIN)
    _, _, tail = rest.partition(END)
    new = head + export_block(header).rstrip('\n') + tail
    if new == old: return ''
    if check: return f'{path}: the export block is stale, run tools/gen_module_exports.py {header}'
    open(path, 'w', encoding='utf-8').write(new)
    print(f'  wrote {path}')
    return ''


def name_collisions() -> list:
    """Every rpp header whose module name would repeat a macro, so `STEMS` must rename it.

    This reads the headers, not the `.cppm` files. A wrong `STEMS` entry drops a module from
    `with_modules`, and a check which only walks those would go quiet instead of reporting.
    """
    return [f'{h}: module {module_name(h)} repeats a macro, so rename it in STEMS'
            for h in sorted(os.listdir(rd.SRC))
            if h.endswith('.h') and h not in rd.NO_MODULE and macro_collision(module_stem(h))]


def with_modules() -> list:
    """Every header which owns a module interface unit. The NO_MODULE filter drops config.h,
    which shares config.types.h's .cppm path."""
    return [h for h in sorted(os.listdir(rd.SRC))
            if h.endswith('.h') and h not in rd.NO_MODULE and os.path.exists(cppm_path(h))]


_SELFTEST_HEADER = '''#pragma once
namespace rpp {
    constexpr double PI = 3.14159;                 // const at namespace scope, so internal
    static constexpr float radf(float d);          // static, so internal
    static constexpr char obfuscate(char c);       // internal, and INTERNAL_OK covers it
    inline constexpr double TAU = 6.28318;         // inline, so external
    struct Public {};
    template<int N> struct Sized { char c[N]; };
    template<int N> Sized(const char (&)[N]) -> Sized<N>;   // a deduction guide has no name
}
'''


def selftest() -> list:
    """Crafts a header and pins both gates, the macro name and the linkage."""
    import tempfile
    bad = []
    # scope_guard.h defines the macro, and the rename to rpp.scopeguard is what clears it
    if not macro_collision('scope_guard'): bad.append('a macro name passed the module name guard')
    if macro_collision('scopeguard'): bad.append('the renamed module still reports a macro')
    if macro_collision('strview'): bad.append('a module which repeats no macro reported one')

    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, 'probe.h')
        open(path, 'w').write(_SELFTEST_HEADER)
        hidden = internal_names(path, frozenset())
        shown = _exported(path, CONFIGS[0]).get('rpp', [])
        for name in ('PI', 'radf', 'obfuscate'):
            if name not in hidden: bad.append(f'{name} hides from the module and the gate stayed quiet')
        if 'obfuscate' in internal_names(path, frozenset({'obfuscate'})):
            bad.append('the allowlist did not silence obfuscate')
        for name in ('PI', 'radf', 'obfuscate'):
            if name in shown: bad.append(f'{name} has internal linkage and reached the export list')
        for name in ('TAU', 'Public'):
            if name not in shown: bad.append(f'{name} has external linkage and left the export list')
        if any(n.startswith('<') for n in shown): bad.append('a deduction guide reached the export list')
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
        if a.all: bad += name_collisions()
    except rd.ClangMissing as e:
        print(f'cannot run: {e}')
        return 1
    for f in bad: print(f'  {f}')
    print(f'== gen_module_exports: {len(bad)} finding(s) over {len(targets)} module(s) ==')
    return 1 if bad else 0  # write mode still fails on a missing .cppm or marker


if __name__ == '__main__':
    sys.exit(main())
