#!/usr/bin/env python3
"""Writes a group module interface unit from the headers it carries.

The generator owns the whole `.cppm`, so a hand-written export list cannot drift.
`GROUP_HEADERS` says which headers each group carries.

  tools/gen_module_exports.py strview.h            # write the group which carries it
  tools/gen_module_exports.py --all                # every group
  tools/gen_module_exports.py --all --check        # exit 1 when a group is stale
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

# the headers a header re-exports, empty because a re-export breaks a `<string>` first
# importer on gcc-14. An importer names every group it uses, see BUGS.md B8
RE_EXPORT = {}

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


def _inline_parts(headers) -> set:
    """Every namespace component these headers declare inline."""
    out = set()
    for h in headers:
        out |= set(re.findall(r'^\s*inline\s+namespace\s+(\w+)', _read(h), re.M))
    return out


def _open_namespace(headers, ns: str) -> str:
    """`ns` with each component `headers` declares inline marked inline.

    A literal operator lives in an inline namespace, so `using namespace rpp` reaches it. The
    module reopens that namespace inline too, and does not lean on the fragment having done it.
    """
    inlines = _inline_parts(headers)
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


def header_group(header: str) -> str:
    """The group whose module carries this header, or '' when no group claims it."""
    return next((g for g, hs in GROUP_HEADERS.items() if header in hs), '')


def _namespace_groups(header: str) -> dict:
    """Namespace to condition to names, for one header, which a group module then merges."""
    base = _exported(header, BASE)
    reduced = {g: n for g, off in GUARDS if (n := _configuration(header, g, BASE + off)) is not None}
    spans = _guarded_spans(header)
    declared = _declared(base, reduced)
    alt_guard = ALT_GUARD.get(os.path.basename(header), '')
    out = {}
    for ns in declared:
        for name, line in declared[ns].items():
            cond = _condition(ns, name, line, base, reduced, spans, alt_guard)
            out.setdefault(ns, {}).setdefault(cond, []).append(name)
    return out


def group_export_block(group: str) -> str:
    """The generated block for one group module, merging every member header's exports."""
    lines = [BEGIN]

    # a member which re-exports a header names the group carrying it, and never its own group
    re_exports = {f'rpp.{g}' for h in GROUP_HEADERS[group] for dep in RE_EXPORT.get(h, ())
                  if (g := header_group(dep)) and g != group}
    lines += [f'export import {imp};' for imp in sorted(re_exports)]

    merged = {}  # namespace to condition to names, in member order, first declaration wins
    seen = {}
    for header in GROUP_HEADERS[group]:
        for ns, conds in _namespace_groups(header).items():
            for cond, names in conds.items():
                for name in names:
                    if (ns, name) in seen: continue
                    seen[(ns, name)] = cond
                    merged.setdefault(ns, {}).setdefault(cond, []).append(name)

    for ns in sorted(merged):
        if not ns:  # a C API keeps global scope, so a call site needs no change
            lines += [''] + [f'export using ::{n};' for n in merged[ns].get('', [])]
            continue
        lines += ['', f'export namespace {_open_namespace(GROUP_HEADERS[group], ns)} {{']
        for cond, names in sorted(merged[ns].items()):
            if cond: lines.append(f'#if {cond}')
            lines += [f'    using {ns}::{n};' for n in names]
            if cond: lines.append('#endif')
        lines.append('}')

    lines.append(END)
    return '\n'.join(lines) + '\n'


def write_group(group: str, check: bool) -> str:
    """Writes the whole group `.cppm`, fragment included. Returns a finding, or ''."""
    path = os.path.join(rd.SRC, f'rpp-{group}.cppm')
    for header in GROUP_HEADERS[group]:
        hidden = internal_names(header)
        if hidden:
            return (f'{header}: internal linkage hides {hidden} from rpp.{group}. Make each one '
                    f'`inline`, or name it in INTERNAL_OK when no importer needs it')
    includes = '\n'.join(f'#include "{h}"' for h in GROUP_HEADERS[group])
    new = (f'// C++20 module interface unit for the rpp.{group} headers, owned by tools/gen_module_exports.py.\n'
           f'// The headers stay in the global module fragment, so an importer and an includer share one entity.\n'
           f'module;\n\n{includes}\n\n'
           f'export module rpp.{group};\n\n'
           + group_export_block(group) + MANUAL_EXPORTS.get(group, ''))
    old = open(path, encoding='utf-8-sig', errors='replace').read() if os.path.exists(path) else ''
    if new == old: return ''
    if check: return f'{path}: stale, run tools/gen_module_exports.py --all'
    open(path, 'w', encoding='utf-8').write(new)
    print(f'  wrote {path}')
    return ''


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
        lines += ['', f'export namespace {_open_namespace([header], ns)} {{']
        for cond, names in sorted(groups.items()):
            if cond: lines.append(f'#if {cond}')
            lines += [f'    using {ns}::{n};' for n in names]
            if cond: lines.append('#endif')
        lines.append('}')

    lines.append(END)
    return '\n'.join(lines) + '\n'


UMBRELLA = 'rpp.cppm'
# the groups, which partition every header. A group is one module, and its fragment includes
# the headers below, because gcc-14 runs out of module source locations when one translation
# unit imports dozens of them, see BUGS.md C28. `import rpp;` imports the groups.
GROUP_HEADERS = {
    'core': ('config.types.h', 'debugging.h', 'source_loc.h', 'traits.h', 'type_traits.h',
             'predicates.h', 'scope_guard.h', 'delegate.h', 'proc_utils.h', 'stack_trace.h',
             'endian.h', 'bitutils.h'),
    'text': ('strview.h', 'sprint.h', 'obfuscated_string.h'),
    'numeric': ('math.h', 'minmax.h', 'vec.h', 'sort.h'),
    'time': ('timepoint.h', 'timer.h', 'atomic_timepoint.h'),
    'containers': ('collections.h', 'memory_pool.h', 'load_balancer.h'),
    'io': ('file_io.h', 'paths.h', 'sockets.h', 'binary_stream.h', 'binary_serializer.h'),
    'threading': ('mutex.h', 'condition_variable.h', 'semaphore.h', 'concurrent_queue.h',
                  'thread_pool.h', 'threads.h', 'task.h', 'future.h', 'future_types.h',
                  'event_loop.h', 'coroutines.h', 'atomic_shared_ptr.h', 'close_sync.h'),
    'testing': ('tests.h',),
}
GROUPS = tuple(GROUP_HEADERS)

# a group the umbrella leaves out. `rpp.testing` overflows the gcc-14 source location budget
# through `import rpp;` on C++23, so a test file imports it by name, see BUGS.md B25
UMBRELLA_OMITS = ('rpp.testing',)

# a name behind a guard this checkout cannot parse, because its configuration needs a toolkit
# the build does not carry. The group appends the text below the generated block, verbatim.
MANUAL_EXPORTS = {
    'core': '\n// QtPrintable sits behind RPP_HAS_QT, which the generator cannot parse without Qt\n'
            'export namespace rpp {\n'
            '#if RPP_HAS_QT\n'
            '    using rpp::QtPrintable;\n'
            '#endif\n'
            '}\n',
}


def _export_imports(cppm: str) -> set:
    path = os.path.join(rd.SRC, cppm)
    if not os.path.exists(path):
        return set()
    return set(re.findall(r'^export import\s+([\w.]+)\s*;', _read(cppm), re.M))


def umbrella_drift() -> list:
    """Every header no group carries, every header two groups carry, and every umbrella gap.

    `GROUP_HEADERS` is hand written, so a new header reaches no importer until someone adds a
    line. This compares it against the headers on disk and against what `rpp.cppm` imports.
    """
    if not os.path.exists(os.path.join(rd.SRC, UMBRELLA)):
        return [f'{UMBRELLA}: missing']

    want = {f'rpp.{g}' for g in GROUPS} - set(UMBRELLA_OMITS)
    top = _export_imports(UMBRELLA)
    omits = set(UMBRELLA_OMITS)
    bad = [f'{UMBRELLA}: does not export import {m}' for m in sorted(want - top)]
    bad += [f'{UMBRELLA}: exports {m}, which UMBRELLA_OMITS leaves out' for m in sorted(top & omits)]
    bad += [f'{UMBRELLA}: exports {m}, which is not a group' for m in sorted(top - want - omits)]

    seen = {}
    for g in GROUPS:
        for h in GROUP_HEADERS[g]:
            if h in seen: bad.append(f'rpp.{g}: carries {h}, which rpp.{seen[h]} already carries')
            else: seen[h] = g
    owned = set(module_headers())
    bad += [f'no group carries {h}' for h in sorted(owned - set(seen))]
    bad += [f'rpp.{seen[h]}: carries {h}, which src/rpp does not have' for h in sorted(set(seen) - owned)]
    return bad


STD_MODULE = 'rpp-std.cppm'

# The std stand-in sits in five parts, and `rpp-std.cppm` re-exports all five. One unit which
# carries <memory> beside the container headers is unreadable on C++23, see BUGS.md B24.
STD_PARTS = ('rpp-std-text.cppm', 'rpp-std-containers.cppm', 'rpp-std-memory.cppm',
             'rpp-std-threading.cppm', 'rpp-std-core.cppm')

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
    'chrono': 'a namespace, and rpp::Duration replaces it in every ReCpp signature',
    'declval': 'unevaluated, inside a trait, which no caller writes',
    'get_if': 'a call inside an inline body, not an argument a caller passes',
    'make_exception_ptr': 'the same, and B19 keeps std::exception_ptr out anyway',
}

_OPEN = re.compile(r'\b\w+\s*\(')
_STD_NAME = re.compile(r'\bstd::(\w+)')


def _parameter_lists(text: str):
    """Every `name(...)` list in one logical line, matched by counting parentheses.

    A regex cannot span a nested list, and a member function pointer parameter carries one.
    """
    for opener in _OPEN.finditer(text):
        i, depth = opener.end(), 1
        while i < len(text) and depth:
            depth += (text[i] == '(') - (text[i] == ')')
            i += 1
        if depth == 0:
            yield text[opener.end():i - 1]


def _parameter_lines(header: str) -> list:
    """The logical lines of a header which can declare a parameter, comments and directives cut.

    A declaration wraps its parameter list over several source lines, so join until the
    parentheses balance. A concept body and a static_assert name a trait no caller writes.
    """
    text = re.sub(r'/\*.*?\*/', '', _read(header), flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)
    text = re.sub(r'^[ \t]*#[^\n]*', '', text, flags=re.M)
    logical, pending, depth = [], '', 0
    for line in text.splitlines():
        pending = f'{pending} {line.strip()}' if pending else line
        depth = max(0, depth + line.count('(') - line.count(')'))
        if depth:
            continue
        if 'std::' in pending and not re.search(r'\b(requires|concept|static_assert)\b', pending):
            logical.append(pending)
        pending = ''
    return logical


def std_export_drift() -> list:
    """Every std name a public parameter list writes which `rpp.std` does not export.

    A consumer which imports instead of including has to spell each one, so a gap here is an
    API it cannot call. `STD_NOT_EXPORTED` carries the deliberate exclusions with a reason.
    """
    missing = [f'{p}: missing' for p in (STD_MODULE, *STD_PARTS)
               if not os.path.exists(os.path.join(rd.SRC, p))]
    if missing:
        return missing
    exported = set()
    for part in STD_PARTS:
        exported |= set(re.findall(r'^\s*using std::(\w+);', _read(part), re.M))
    found = {}
    for header in sorted(os.listdir(rd.SRC)):
        if not header.endswith('.h'): continue
        for line in _parameter_lines(header):
            for params in _parameter_lists(line):
                for name in _STD_NAME.findall(params):
                    if name not in exported and name not in STD_NOT_EXPORTED:
                        found.setdefault(name, header)
    return [f'rpp.std: {h} writes std::{n} in a parameter list. Export it from a part, or '
            f'name it in STD_NOT_EXPORTED with the reason' for n, h in sorted(found.items())]


def std_umbrella_drift() -> list:
    """Every std part `rpp-std.cppm` does not re-export, and every name it re-exports twice.

    A part no umbrella names reaches no consumer which writes `import rpp.std;`, and a name
    two parts export makes the importer report an ambiguity.
    """
    if not os.path.exists(os.path.join(rd.SRC, STD_MODULE)):
        return [f'{STD_MODULE}: missing']
    reexported = set(re.findall(r'^\s*export import (rpp\.std\.[\w.]+);', _read(STD_MODULE), re.M))
    wanted = {f'rpp.std.{p[len("rpp-std-"):-len(".cppm")]}' for p in STD_PARTS}
    bad = [f'{STD_MODULE}: does not re-export {m}' for m in sorted(wanted - reexported)]
    bad += [f'{STD_MODULE}: re-exports {m}, which STD_PARTS does not name'
            for m in sorted(reexported - wanted)]
    owner = {}
    for part in STD_PARTS:
        for name in re.findall(r'^\s*using std::(\w+);', _read(part), re.M):
            if name in owner: bad.append(f'{part}: exports std::{name}, which {owner[name]} also exports')
            else: owner[name] = part
    return bad


def manual_export_drift() -> list:
    """Every MANUAL_EXPORTS entry whose group went, or whose name no member header declares.

    The generator cannot parse these, so nothing else notices when one goes stale.
    """
    bad = [f'MANUAL_EXPORTS: {g} is not a group' for g in MANUAL_EXPORTS if g not in GROUP_HEADERS]
    for g, text in MANUAL_EXPORTS.items():
        if g not in GROUP_HEADERS: continue
        carried = ''.join(_read(h) for h in GROUP_HEADERS[g])
        bad += [f'MANUAL_EXPORTS: rpp.{g} exports {n}, which no member header declares'
                for n in re.findall(r'using rpp::(\w+);', text) if n not in carried]
    return bad


def name_collisions() -> list:
    """Every group whose name repeats a macro, because MSVC expands one inside a module directive."""
    return [f'rpp.{g}: the module name repeats a macro, so rename the group'
            for g in GROUPS if macro_collision(g)]


def module_headers() -> list:
    """Every header a group must carry. `NO_MODULE` names the ones a module cannot export."""
    return [h for h in sorted(os.listdir(rd.SRC)) if h.endswith('.h') and h not in rd.NO_MODULE]


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
    # MSVC expands a macro inside a module directive, and scope_guard.h defines one
    if not macro_collision('scope_guard'): bad.append('a macro name passed the module name guard')
    if macro_collision('core'): bad.append('a group name which repeats no macro reported one')
    if name_collisions(): bad.append('the group name gate reports a collision on the real groups')

    # a group which reopens an inline namespace without `inline` leans on the fragment having
    # declared it, so pin the qualifier on the group path and on a member which does not have one
    for group, ns in (('text', 'rpp::inline literals'), ('time', 'rpp::inline duration_literals')):
        if f'export namespace {ns} {{' not in group_export_block(group):
            bad.append(f'rpp.{group} reopens {ns} without the inline qualifier')
    if 'export namespace rpp::inline detail' in group_export_block('core'):
        bad.append('a namespace no header declares inline came out inline')

    # GROUP_HEADERS and the umbrella are hand written, so pin every way one can go stale
    if umbrella_drift(): bad.append('the umbrella gate reports drift on a correct list')
    real_read = _read
    first, second = GROUPS[0], GROUPS[1]
    def _umbrella_patched(text):
        globals()['_read'] = lambda h: text(real_read(h)) if h == UMBRELLA else real_read(h)
        return umbrella_drift()
    try:
        drift = _umbrella_patched(lambda t: t + f'export import {UMBRELLA_OMITS[0]};\n')
        if not any('UMBRELLA_OMITS' in f for f in drift):
            bad.append('an umbrella which exports an omitted group passed the umbrella gate')

        drift = _umbrella_patched(lambda t: t.replace(f'export import rpp.{first};\n', ''))
        if not any(f'does not export import rpp.{first}' in f for f in drift):
            bad.append('an umbrella which drops a group passed the umbrella gate')

        drift = _umbrella_patched(lambda t: t + 'export import rpp.no_such_group;\n')
        if not any('rpp.no_such_group' in f for f in drift):
            bad.append('an unknown export import passed the umbrella gate')
    finally:
        globals()['_read'] = real_read

    real_groups = dict(GROUP_HEADERS)
    def _groups_patched(group: str, headers: tuple):
        GROUP_HEADERS[group] = headers
        return umbrella_drift()
    try:
        # the same header in two groups declares one entity twice, which breaks `import rpp;`
        shared = GROUP_HEADERS[first][0]
        if not any('already carries' in f for f in _groups_patched(second, real_groups[second] + (shared,))):
            bad.append('a header in two groups passed the umbrella gate')
        GROUP_HEADERS[second] = real_groups[second]

        orphan = GROUP_HEADERS[first][0]
        drift = _groups_patched(first, real_groups[first][1:])
        if not any(f'no group carries {orphan}' in f for f in drift):
            bad.append('a header no group carries passed the umbrella gate')
    finally:
        GROUP_HEADERS.update(real_groups)

    # MANUAL_EXPORTS names what no parse here can see, so pin both ways it goes stale
    if manual_export_drift(): bad.append('the manual export gate reports drift on a correct list')
    real_manual = dict(MANUAL_EXPORTS)
    try:
        MANUAL_EXPORTS['no_such_group'] = 'export namespace rpp {}\n'
        if not any('no_such_group' in f for f in manual_export_drift()):
            bad.append('an entry for a missing group passed the manual export gate')
        del MANUAL_EXPORTS['no_such_group']

        MANUAL_EXPORTS[GROUPS[0]] = 'export namespace rpp { using rpp::no_such_name; }\n'
        if not any('no_such_name' in f for f in manual_export_drift()):
            bad.append('an entry no header declares passed the manual export gate')
    finally:
        MANUAL_EXPORTS.clear()
        MANUAL_EXPORTS.update(real_manual)

    # the std export list is hand written too, so pin every way one can go stale
    if std_export_drift(): bad.append('the std gate reports drift on a correct list')
    if std_umbrella_drift(): bad.append('the std umbrella gate reports drift on a correct list')
    def _std_patched(cppm, text):
        globals()['_read'] = lambda h: text(real_read(h)) if h == cppm else real_read(h)
        return std_export_drift()
    try:
        drift = _std_patched('rpp-std-containers.cppm', lambda t: t.replace('    using std::deque;\n', ''))
        if not any('std::deque' in f for f in drift):
            bad.append('a dropped std export passed the std gate')

        excluded = sorted(STD_NOT_EXPORTED)[0]
        reason = STD_NOT_EXPORTED.pop(excluded)
        try:
            if not any(excluded in f for f in std_export_drift()):
                bad.append('a std name with no exclusion reason passed the std gate')
        finally:
            STD_NOT_EXPORTED[excluded] = reason

        # a declaration which wraps its parameter list, so a regression in the parenthesis
        # counting of _parameter_lines reports nothing instead of the name it stopped joining
        wrapped = ('\nnamespace rpp {\n'
                   '    void selftest_probe(int first,\n'
                   '                        std::multiset<int>& wrapped) noexcept;\n}\n')
        drift = _std_patched('strview.h', lambda t: t + wrapped)
        if not any('std::multiset' in f for f in drift):
            bad.append('a std name on a continued parameter line passed the std gate')

        # the same name on one line, so the case above fails for the join and not for the name
        drift = _std_patched('strview.h', lambda t: t + wrapped.replace(',\n' + ' ' * 24, ', '))
        if not any('std::multiset' in f for f in drift):
            bad.append('a std name on one parameter line passed the std gate')

        # the umbrella gate, which the two cases below are the only ways to break
        def _umbrella_patched(cppm, text):
            globals()['_read'] = lambda h: text(real_read(h)) if h == cppm else real_read(h)
            return std_umbrella_drift()
        drift = _umbrella_patched(STD_MODULE, lambda t: t.replace('export import rpp.std.memory;\n', ''))
        if not any('rpp.std.memory' in f for f in drift):
            bad.append('an umbrella which drops a part passed the std umbrella gate')

        drift = _umbrella_patched('rpp-std-core.cppm',
                                  lambda t: t.replace('    using std::move;', '    using std::vector;'))
        if not any('std::vector' in f for f in drift):
            bad.append('a name two parts export passed the std umbrella gate')
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
    ap.add_argument('--all', action='store_true', help='every group module')
    ap.add_argument('--check', action='store_true', help='exit 1 when a group module is stale')
    ap.add_argument('--selftest', action='store_true', help='pin every gate against a stubbed list')
    a = ap.parse_args()
    if a.selftest:
        findings = selftest()
        for f in findings: print(f'  {f}')
        print(f'== gen_module_exports selftest: {len(findings)} finding(s) ==')
        return 1 if findings else 0
    if not a.header and not a.all: ap.error('name a header, or pass --all')

    try:
        # a header names the group which carries it, because a group is the module now
        targets = list(GROUPS) if a.all else [header_group(a.header)]
        if not targets[0]: ap.error(f'no group carries {a.header}, see GROUP_HEADERS')
        bad = [f for f in (write_group(g, a.check) for g in targets) if f]
        if a.all: bad += (name_collisions() + umbrella_drift() + std_export_drift()
                          + std_umbrella_drift() + manual_export_drift())
    except rd.ClangMissing as e:
        print(f'cannot run: {e}')
        return 1
    for f in bad: print(f'  {f}')
    print(f'== gen_module_exports: {len(bad)} finding(s) over {len(targets)} module(s) ==')
    return 1 if bad else 0  # write mode still fails on a hidden name or a drifted list


if __name__ == '__main__':
    sys.exit(main())
