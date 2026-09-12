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

# BASE declares the widest surface. Each guard names the condition and the defines which
# turn it off, so the generator reads every configuration and guards what only BASE declares
BASE = ('RPP_ENABLE_UNICODE=1',)
GUARDS = (('RPP_ENABLE_UNICODE', ('RPP_ENABLE_UNICODE=0',)),
          ('!RPP_BARE_METAL', ('RPP_FREERTOS=1',)),
          ('!defined(RPP_BINARY_READWRITE_NO_SOCKETS)', ('RPP_BINARY_READWRITE_NO_SOCKETS=1',)),
          ('!defined(RPP_BINARY_READWRITE_NO_FILE_IO)', ('RPP_BINARY_READWRITE_NO_FILE_IO=1',)))

# a macro no define reaches, because the header derives it from __has_include. The generator
# reads the region the header guards with it instead of parsing a second configuration.
# Empty since RPP_HAS_COROUTINES went, and the selftest still drives the rule
TEXT_GUARDS = ()

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

# a name a module keeps off its surface. gcc-14 writes an unreadable .gcm for a std::__cxx11
# export (BUGS.md B8), and the other two entries are internal names no importer needs
NO_EXPORT = {'type_traits.h': frozenset({'has_std_to_string'}),
             'memory_pool.h': frozenset({'pool_types_constructor'}),
             'thread_pool.h': frozenset({'test_threadpool'})}

# the modules a module re-exports, empty until a surface forces an entry
# tests.h earns the one: TestImpl expands to a constructor taking rpp::strview
RE_EXPORT = {'tests.h': ('rpp.strview',)}

# a header which does not compile in a guard configuration, so no export list exists to
# reduce against. Every entry below names a bare-metal gap, see BUGS.md B15
NO_CONFIG = {'semaphore.h': frozenset({'!RPP_BARE_METAL'}),
             'concurrent_queue.h': frozenset({'!RPP_BARE_METAL'}),
             'thread_pool.h': frozenset({'!RPP_BARE_METAL'}),
             'future.h': frozenset({'!RPP_BARE_METAL'}),
             'event_loop.h': frozenset({'!RPP_BARE_METAL'}),
             'coroutines.h': frozenset({'!RPP_BARE_METAL'})}

# the condition a header declares for the names only an alternate configuration has, when the
# guard the generator would negate is wider. mutex.h also declares critical_section on Cortex-M,
# which RPP_BARE_METAL does not have to cover
ALT_GUARD = {'mutex.h': 'RPP_HAS_CRITICAL_SECTION_MUTEX'}


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
    """Namespace to name to declaration line, for one macro configuration, in source order."""
    out = {}
    blocked = NO_EXPORT.get(os.path.basename(header), frozenset())
    for ns, kind, name, internal, line in rd.declarations(header, defines):
        if _skip(ns, kind, name) or name in blocked: continue
        if internal: continue  # clang rejects a using-declaration which exports one
        out.setdefault(ns, {}).setdefault(name, line)
    return out


def _configuration(header: str, guard: str, defines: tuple):
    """The export map for one guard configuration, or None when `NO_CONFIG` allows the failure.

    A header the allowlist names does not compile in that configuration, so no export list
    exists to reduce against. Any other parse error reaches the caller and fails the run.
    """
    try:
        return _exported(header, defines)
    except RuntimeError:
        if guard in NO_CONFIG.get(os.path.basename(header), frozenset()): return None
        raise


def internal_names(header: str, allow: frozenset = None) -> list:
    """The public names this header hides behind internal linkage, in source order.

    A `static constexpr` function and a namespace-scope `constexpr` both land here, and no
    module can export either. `allow` names the helpers whose loss is intended.
    """
    allow = INTERNAL_OK if allow is None else allow
    out = []
    for ns, kind, name, internal, line in rd.declarations(header, BASE):
        if _skip(ns, kind, name): continue
        if internal and name not in allow and name not in out: out.append(name)
    return out


def _guarded_spans(header: str, macros: tuple = None) -> dict:
    """Macro to the line spans its `#if` blocks cover, for every macro in `macros`.

    A span holds line numbers, not text. A block which only mentions an unconditional class
    would otherwise guard that whole class, and hide its API wherever the macro is 0.
    """
    lines = _read(header).split('\n')
    out = {}
    for macro in (TEXT_GUARDS if macros is None else macros):
        spans, depth, start = [], 0, None
        for i, line in enumerate(lines, 1):
            s = line.strip()
            if start is None:
                if re.match(rf'#\s*if\s+{macro}\s*$', s): start, depth = i, 1
                continue
            if s.startswith('#if'): depth += 1
            elif s.startswith('#endif'):
                depth -= 1
                if depth == 0: spans.append((start, i)); start = None
        if spans: out[macro] = spans
    return out


def _open_namespace(header: str, ns: str) -> str:
    """`ns` with each component the header declares inline marked inline.

    A literal operator lives in an inline namespace, so `using namespace rpp` reaches it. The
    module has to reopen that namespace inline too, or the importer loses the lookup.
    """
    inlines = set(re.findall(r'^\s*inline\s+namespace\s+(\w+)', _read(header), re.M))
    return '::'.join(f'inline {p}' if p in inlines else p for p in ns.split('::'))


def _negate(guard: str) -> str:
    """The condition which holds where `guard` does not."""
    return guard[1:] if guard.startswith('!') else '!' + guard


def _condition(ns: str, name: str, line: int, base: dict, reduced: dict, spans: dict,
               alt_guard: str = '') -> str:
    """The `#if` this name needs, or '' when every configuration declares it."""
    in_base = name in base.get(ns, {})
    needs = []
    for g, names in reduced.items():
        in_alt = name in names.get(ns, {})
        # a guard label states the condition which holds in BASE, so an alternate-only name negates it
        if in_base and not in_alt: needs.append(g)
        elif in_alt and not in_base: needs.append(alt_guard or _negate(g))
    needs += [m for m, sp in spans.items() if any(a < line < b for a, b in sp)]
    return ' && '.join(needs)


def _declared(base: dict, reduced: dict) -> dict:
    """Every name any configuration declares, with the line of the first one which does."""
    out = {ns: dict(names) for ns, names in base.items()}
    for names in reduced.values():
        for ns, m in names.items():
            dst = out.setdefault(ns, {})
            for name, line in m.items(): dst.setdefault(name, line)
    return out


def export_block(header: str) -> str:
    """The generated block for one header, markers included."""
    base = _exported(header, BASE)
    reduced = {g: n for g, off in GUARDS if (n := _configuration(header, g, BASE + off)) is not None}
    spans = _guarded_spans(header)
    lines = [BEGIN]

    # a module re-exports only what RE_EXPORT names, so an importer spells the rest itself
    lines += [f'export import {imp};' for imp in sorted(RE_EXPORT.get(os.path.basename(header), ()))]

    declared = _declared(base, reduced)
    alt_guard = ALT_GUARD.get(os.path.basename(header), '')
    for ns in sorted(declared):
        groups = {}  # condition to the names it guards. '' sorts first, so the guards follow it
        for name, line in declared[ns].items():
            groups.setdefault(_condition(ns, name, line, base, reduced, spans, alt_guard), []).append(name)
        if not groups: continue
        if not ns:  # a C API keeps global scope, so a call site needs no change
            lines += [''] + [f'export using ::{n};' for n in groups.get('', [])]
            continue
        lines += ['', f'export namespace {_open_namespace(header, ns)} {{']
        for cond, names in sorted(groups.items()):
            if cond: lines.append(f'#if {cond}')
            lines += [f'    using {ns}::{n};' for n in names]
            if cond: lines.append('#endif')
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


UMBRELLA = 'rpp.cppm'
# the group umbrellas, which partition every module. `import rpp;` imports these, never a
# module directly, so a new module reaches a consumer only by joining one group
GROUPS = ('core', 'text', 'numeric', 'time', 'containers', 'io', 'threading', 'testing')


def _export_imports(cppm: str) -> set:
    path = os.path.join(rd.SRC, cppm)
    if not os.path.exists(path):
        return set()
    return set(re.findall(r'^export import\s+([\w.]+)\s*;', _read(cppm), re.M))


def umbrella_drift() -> list:
    """Every module no group carries, every name no header owns, and every module in two groups.

    The lists are hand written, so a new module reaches no `import rpp;` consumer until
    someone adds a line. This compares them against the modules on disk.
    """
    bad = []
    for cppm in [UMBRELLA] + [f'rpp-{g}.cppm' for g in GROUPS]:
        if not os.path.exists(os.path.join(rd.SRC, cppm)): bad.append(f'{cppm}: missing')
    if bad:
        return bad

    want_groups = {f'rpp.{g}' for g in GROUPS}
    top = _export_imports(UMBRELLA)
    bad += [f'{UMBRELLA}: does not export import {m}' for m in sorted(want_groups - top)]
    bad += [f'{UMBRELLA}: exports {m}, which is not a group' for m in sorted(top - want_groups)]

    owned = {module_name(h) for h in with_modules()}
    seen = {}
    for g in GROUPS:
        for m in sorted(_export_imports(f'rpp-{g}.cppm')):
            if m in seen: bad.append(f'rpp.{g}: exports {m}, which rpp.{seen[m]} already carries')
            else: seen[m] = g
    bad += [f'no group exports {m}' for m in sorted(owned - set(seen))]
    bad += [f'rpp.{seen[m]}: exports {m}, which no header owns' for m in sorted(set(seen) - owned)]
    return bad


STD_MODULE = 'rpp-std.cppm'

# Every std name a public parameter list writes which `rpp.std` does not export. The reason
# is what the next reader needs, because `std_export_drift` reports anything absent from both
STD_NOT_EXPORTED = {
    **{n: 'a trait in a default argument, which no caller writes' for n in (
        'is_enum_v', 'is_function', 'is_void_v', 'is_nothrow_copy_constructible_v',
        'is_nothrow_move_constructible_v', 'is_trivially_copy_assignable_v',
        'is_trivially_destructible_v', 'is_trivially_move_assignable_v')},
    'exception_ptr': 'gcc-14 writes an interface no importer can read, see BUGS.md B19',
    'future': 'its header kills std::swap lookup in the fragment, see BUGS.md B20',
    'future_status': 'the same header, and rpp::cfuture::await_ready() answers without it',
    'promise': 'gcc-14 crashes an importer which instantiates it, see BUGS.md B16',
    'get': 'gcc-14 breaks std::unique_ptr in every importer, see BUGS.md B21',
    'size_t': '<cstddef> declares it at global scope too, so a mixed importer reports an ambiguity',
    'nullptr_t': 'the same global scope ambiguity as size_t',
    'nothrow_t': 'an importer already includes <new>, which carries it',
    'basic_string_view': 'rpp::strview replaces it, and std::string_view covers a caller',
}

# a declaration, not a call: a name, its parameter list, and what closes the declaration
_DECL = re.compile(r'\b\w+\s*\(([^()]*)\)\s*(?:const\s*)?(?:noexcept\w*\s*)?(?:->|\{|;|$)')
_STD_NAME = re.compile(r'\bstd::(\w+)')


def _parameter_lines(header: str) -> list:
    """The lines of a header which can declare a parameter, with comments and directives cut.

    A concept body and a static_assert name a trait the caller never writes, so both go too.
    """
    text = re.sub(r'/\*.*?\*/', '', _read(header), flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)
    text = re.sub(r'^[ \t]*#[^\n]*', '', text, flags=re.M)
    return [ln for ln in text.splitlines()
            if 'std::' in ln and not re.search(r'\b(requires|concept|static_assert)\b', ln)]


def std_export_drift() -> list:
    """Every std name a public parameter list writes which `rpp.std` does not export.

    A consumer which imports instead of including has to spell each one, so a gap here is an
    API it cannot call. `STD_NOT_EXPORTED` carries the deliberate exclusions with a reason.
    """
    path = os.path.join(rd.SRC, STD_MODULE)
    if not os.path.exists(path):
        return [f'{STD_MODULE}: missing']
    exported = set(re.findall(r'^\s*using std::(\w+);', _read(STD_MODULE), re.M))
    found = {}
    for header in sorted(os.listdir(rd.SRC)):
        if not header.endswith('.h'): continue
        for line in _parameter_lines(header):
            for decl in _DECL.finditer(line):
                for name in _STD_NAME.findall(decl.group(1)):
                    if name not in exported and name not in STD_NOT_EXPORTED:
                        found.setdefault(name, header)
    return [f'{STD_MODULE}: {h} writes std::{n} in a parameter list. Export it, or name it '
            f'in STD_NOT_EXPORTED with the reason' for n, h in sorted(found.items())]


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
#define RPP_HAS_COROUTINES 1                       // the real header derives this from __has_include
#include "rpp/minmax.h"                            // an rpp include, so the block names rpp.minmax
#include "rpp/config.h"                            // RPP_BARE_METAL, which the guard below reads
namespace rpp {
    constexpr double PI = 3.14159;                 // const at namespace scope, so internal
    static constexpr float radf(float d);          // static, so internal
    static constexpr char obfuscate(char c);       // internal, and INTERNAL_OK covers it
    inline constexpr double TAU = 6.28318;         // inline, so external
    struct Public {};
    template<int N> struct Sized { char c[N]; };
    template<int N> Sized(const char (&)[N]) -> Sized<N>;   // a deduction guide has no name
#if RPP_HAS_COROUTINES
    struct Awaited {};                             // a guard span covers this declaration
#endif
    class Unconditional {                          // the span below names it, and must not guard it
    public:
#if RPP_HAS_COROUTINES
        void await(Unconditional& u);
#endif
    };
#if RPP_BARE_METAL
    struct BareOnly {};                            // only the alternate configuration declares it
#else
    struct HostOnly {};                            // only the base configuration declares it
#endif
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

    # the group lists are hand written, so pin every way one can go stale
    if umbrella_drift(): bad.append('the umbrella gate reports drift on a correct list')
    real_read = _read
    first = f'rpp-{GROUPS[0]}.cppm'
    dropped = sorted(_export_imports(first))[0]
    def _patched(text: str):
        globals()['_read'] = lambda h: text(real_read(h)) if h == first else real_read(h)
        return umbrella_drift()
    try:
        drift = _patched(lambda t: t.replace(f'export import {dropped};\n', ''))
        if not any(f'no group exports {dropped}' in f for f in drift):
            bad.append('a module no group carries passed the umbrella gate')

        drift = _patched(lambda t: t + 'export import rpp.no_such_module;\n')
        if not any('rpp.no_such_module' in f for f in drift):
            bad.append('an unknown export import passed the umbrella gate')

        # the same module in two groups makes `import rpp;` ambiguous about who owns it
        twice = sorted(_export_imports(f'rpp-{GROUPS[1]}.cppm'))[0]
        drift = _patched(lambda t: t + f'export import {twice};\n')
        if not any('already carries' in f for f in drift):
            bad.append('a module in two groups passed the umbrella gate')
    finally:
        globals()['_read'] = real_read

    # the std export list is hand written too, so pin both directions the same way
    if std_export_drift(): bad.append('the std gate reports drift on a correct list')
    def _std_patched(text):
        globals()['_read'] = lambda h: text(real_read(h)) if h == STD_MODULE else real_read(h)
        return std_export_drift()
    try:
        drift = _std_patched(lambda t: t.replace('    using std::deque;\n', ''))
        if not any('std::deque' in f for f in drift):
            bad.append('a dropped std export passed the std gate')

        excluded = sorted(STD_NOT_EXPORTED)[0]
        reason = STD_NOT_EXPORTED.pop(excluded)
        try:
            if not any(excluded in f for f in std_export_drift()):
                bad.append('a std name with no exclusion reason passed the std gate')
        finally:
            STD_NOT_EXPORTED[excluded] = reason
    finally:
        globals()['_read'] = real_read

    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, 'probe.h')
        open(path, 'w').write(_SELFTEST_HEADER)
        hidden = internal_names(path, frozenset())
        rpp_ns = _exported(path, BASE).get('rpp', {})
        shown = list(rpp_ns)
        for name in ('PI', 'radf', 'obfuscate'):
            if name not in hidden: bad.append(f'{name} hides from the module and the gate stayed quiet')
        if 'obfuscate' in internal_names(path, frozenset({'obfuscate'})):
            bad.append('the allowlist did not silence obfuscate')
        for name in ('PI', 'radf', 'obfuscate'):
            if name in shown: bad.append(f'{name} has internal linkage and reached the export list')
        for name in ('TAU', 'Public'):
            if name not in shown: bad.append(f'{name} has external linkage and left the export list')
        if any(n.startswith('<') for n in shown): bad.append('a deduction guide reached the export list')
        spans = _guarded_spans(path, ('RPP_HAS_COROUTINES',))
        base = _exported(path, BASE)
        if _condition('rpp', 'Awaited', rpp_ns['Awaited'], base, {}, spans) != 'RPP_HAS_COROUTINES':
            bad.append('a declaration inside a guard span took no #if')
        if _condition('rpp', 'Public', rpp_ns['Public'], base, {}, spans):
            bad.append('a declaration outside every guard span took an #if')
        # the guarded member names its own class, and a text search would guard the class too
        if _condition('rpp', 'Unconditional', rpp_ns['Unconditional'], base, {}, spans):
            bad.append('a class a guarded member names took an #if')

        # a name only the alternate configuration declares still has to reach the export list
        reduced = {g: _exported(path, BASE + off) for g, off in GUARDS}
        declared = _declared(base, reduced)
        block = export_block(path)
        for name, cond in (('BareOnly', 'RPP_BARE_METAL'), ('HostOnly', '!RPP_BARE_METAL')):
            if name not in declared.get('rpp', {}):
                bad.append(f'{name} left the export list, so a configuration went unread')
            elif _condition('rpp', name, declared['rpp'][name], base, reduced, spans) != cond:
                bad.append(f'{name} took the wrong #if for the configuration which declares it')
            if f'using rpp::{name};' not in block:
                bad.append(f'{name} reached no using-declaration in the block')
        ALT_GUARD[os.path.basename(path)] = 'RPP_PROBE_GUARD'
        try:
            if '#if RPP_PROBE_GUARD' not in export_block(path):
                bad.append('ALT_GUARD did not replace the negated guard')
        finally:
            del ALT_GUARD[os.path.basename(path)]
        # an rpp include is not a re-export, and only RE_EXPORT puts one in the block
        probe = os.path.basename(path)
        if 'export import' in export_block(path):
            bad.append('an rpp include became an export import on its own')
        NO_EXPORT[probe], RE_EXPORT[probe] = frozenset({'Public'}), ('rpp.minmax',)
        try:
            block = export_block(path)
            if 'using rpp::Public;' in block: bad.append('a NO_EXPORT name reached the export list')
            if 'export import rpp.minmax;' not in block: bad.append('a RE_EXPORT module left the block')
        finally:
            del NO_EXPORT[probe], RE_EXPORT[probe]
        # a header the allowlist does not name must report its parse error, never drop the guard
        broken = os.path.join(d, 'broken.h')
        open(broken, 'w').write('#pragma once\n#if RPP_FREERTOS\n#error this header needs a host\n#endif\n')
        try:
            _configuration(broken, '!RPP_BARE_METAL', BASE + ('RPP_FREERTOS=1',))
            bad.append('an unlisted parse failure returned instead of raising')
        except RuntimeError:
            pass
        NO_CONFIG['broken.h'] = frozenset({'!RPP_BARE_METAL'})
        try:
            if _configuration(broken, '!RPP_BARE_METAL', BASE + ('RPP_FREERTOS=1',)) is not None:
                bad.append('an allowlisted parse failure did not drop the guard')
        finally:
            del NO_CONFIG['broken.h']
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
        if a.all: bad += name_collisions() + umbrella_drift() + std_export_drift()
    except rd.ClangMissing as e:
        print(f'cannot run: {e}')
        return 1
    for f in bad: print(f'  {f}')
    print(f'== gen_module_exports: {len(bad)} finding(s) over {len(targets)} module(s) ==')
    return 1 if bad else 0  # write mode still fails on a missing .cppm or marker


if __name__ == '__main__':
    sys.exit(main())
