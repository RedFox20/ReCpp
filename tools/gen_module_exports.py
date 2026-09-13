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


def _is_detail_ns(ns: str) -> bool:
    """True for an implementation namespace, which no module surface carries.

    The entities stay reachable through the global module fragment, so a public template
    which names one still instantiates in an importer.
    """
    return 'detail' in ns.split('::')


def _skip(ns: str, kind: str, name: str) -> bool:
    """True for a declaration no using-declaration can name, or which stays private."""
    # a deduction guide spells as `<deduction guide for X>`, which no using-declaration names
    return kind in SKIP_KINDS or name.startswith('<') or _is_private(name) \
        or (ns and not ns.startswith('rpp')) or _is_detail_ns(ns)


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


# the groups, which partition every header. A group is one module whose fragment includes
# them, because gcc-14 runs out of locations when a unit imports dozens, see BUGS.md C28
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


def group_partition_drift() -> list:
    """Every header no group carries, and every header two groups carry.

    `GROUP_HEADERS` is hand written, so a new header reaches no importer until someone adds a
    line. This compares it against the headers on disk.
    """
    bad = []
    seen = {}
    for g in GROUPS:
        for h in GROUP_HEADERS[g]:
            if h in seen: bad.append(f'rpp.{g}: carries {h}, which rpp.{seen[h]} already carries')
            else: seen[h] = g
    owned = set(module_headers())
    bad += [f'no group carries {h}' for h in sorted(owned - set(seen))]
    bad += [f'rpp.{seen[h]}: carries {h}, which src/rpp does not have' for h in sorted(set(seen) - owned)]
    return bad


def module_name_drift() -> list:
    """Every `rpp.<name>` in the sources which no `export module` declares.

    A comment naming a module the tree dropped sends a reader to an import which does not
    exist, and the grouping renamed most of them at once.
    """
    real = set()
    for f in sorted(os.listdir(rd.SRC)):
        if f.endswith('.cppm'):
            real |= set(re.findall(r'^export module ([\w.]+);', _read(f), re.M))
    if not real:
        return ['no .cppm declares a module']
    bad = []
    # a build file tells a maintainer which module a target proves, so a stale name there is a
    # wrong instruction. BUGS.md and docs stay out, because they record the name of their own day
    paths = ['CMakeLists.txt']
    for root in (rd.SRC, 'tests', os.path.join('tests', 'module_consumer')):
        if not os.path.isdir(root): continue
        paths += [os.path.join(root, f) for f in sorted(os.listdir(root))
                  if f.endswith(('.h', '.cpp', '.cppm', '.py')) or f == 'CMakeLists.txt']
    for path in paths:
        if os.path.exists(path):
            for name in sorted(set(re.findall(r'\brpp\.[a-z][a-z_.]*', _readfile(path)))):
                name = name.rstrip('.')
                if name.endswith(_NOT_A_MODULE) or name in real: continue
                bad.append(f'{path}: names {name}, which no module declares')
    return bad


BUGS = 'BUGS.md'

# every file which may cite a BUGS.md entry. A citation which resolves to nothing sends the
# next reader to a reproducer somebody deleted
CITING = (BUGS, 'README.md', 'AGENTS.md', 'CMakeLists.txt')


# a suffix which makes an `rpp.x` a file name and not a module name
_NOT_A_MODULE = ('.cppm', '.cpp', '.h', '.py', '.tmp', '.txt', '.md')


def bugs_citation_drift() -> list:
    """Every `BUGS.md <id>` citation which names no entry, and every id the file defines twice.

    A rewrite of BUGS.md can drop an entry while the comments which point at it stay, and
    nothing else notices. This reads the citations and the headings and compares them.
    """
    if not os.path.exists(BUGS):
        return [f'{BUGS}: missing']
    ids = re.findall(r'^### ([BC]\d+)\.', _readfile(BUGS), re.M)
    bad = [f'{BUGS}: defines {i} more than once' for i in sorted({i for i in ids if ids.count(i) > 1})]
    known = set(ids)
    files = list(CITING)
    # a workflow cites an entry to tell a maintainer which failure a red job means
    for root in (rd.SRC, 'docs', 'tests', os.path.join('tests', 'module_consumer'),
                 os.path.join('.github', 'workflows'), os.path.join('.github', 'actions', 'consumer-build'),
                 os.path.join('.github', 'actions', 'ubuntu-build')):
        if not os.path.isdir(root): continue
        files += [os.path.join(root, f) for f in sorted(os.listdir(root))
                  if f.endswith(('.h', '.cpp', '.cppm', '.md', '.py', '.txt', '.yml'))]
    for path in files:
        if not os.path.exists(path): continue
        for cited in sorted(set(re.findall(r'BUGS\.md\s+([BC]\d+)', _readfile(path)))):
            if cited not in known: bad.append(f'{path}: cites {cited}, which {BUGS} does not define')
    return bad


def _readfile(path: str) -> str:
    return open(path, encoding='utf-8-sig', errors='replace').read()



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

    # a rename can drop a module while the comments which name it stay, so pin that too
    if module_name_drift(): bad.append('the module name gate reports drift on a correct tree')
    real_read2 = _readfile
    try:
        globals()['_readfile'] = lambda f: (real_read2(f) + '\n// rpp.no_such_module\n'
                                            if f.endswith('tests.h') else real_read2(f))
        if not any('rpp.no_such_module' in f for f in module_name_drift()):
            bad.append('a name no module declares passed the module name gate')
        # a build file carries the same names and no source extension, so pin that reach
        for build_file in ('CMakeLists.txt', 'mamafile.py'):
            globals()['_readfile'] = lambda f, b=build_file: (real_read2(f) + '\n# rpp.no_such_module\n'
                                                             if f.endswith(b) else real_read2(f))
            if not any('rpp.no_such_module' in f for f in module_name_drift()):
                bad.append(f'a stale module name in a {build_file} passed the module name gate')
    finally:
        globals()['_readfile'] = real_read2

    # a BUGS.md rewrite can drop an entry while the comments which cite it stay, so pin both ways
    if bugs_citation_drift(): bad.append('the citation gate reports drift on a correct BUGS.md')
    real_readfile = _readfile
    def _bugs_patched(text):
        globals()['_readfile'] = lambda f: text(real_readfile(f)) if f == BUGS else real_readfile(f)
        return bugs_citation_drift()
    try:
        cited = re.search(r'BUGS\.md\s+([BC]\d+)', real_readfile(os.path.join(rd.SRC, 'binary_stream.h')))
        dropped = cited.group(1)
        drift = _bugs_patched(lambda s: s.replace(f'### {dropped}.', f'### {dropped}_gone.'))
        if not any(dropped in f for f in drift):
            bad.append('a deleted BUGS.md entry passed the citation gate')

        drift = _bugs_patched(lambda s: s + f'\n### {dropped}. a second heading\n')
        if not any('more than once' in f for f in drift):
            bad.append('a duplicated BUGS.md id passed the citation gate')
    finally:
        globals()['_readfile'] = real_readfile

    # a group which reopens an inline namespace without `inline` leans on the fragment having
    # declared it, so pin the qualifier on the group path and on a member which does not have one
    for group, ns in (('text', 'rpp::inline literals'), ('time', 'rpp::inline duration_literals')):
        if f'export namespace {ns} {{' not in group_export_block(group):
            bad.append(f'rpp.{group} reopens {ns} without the inline qualifier')
    # an implementation namespace reaches an importer through the fragment, never the surface
    for group in GROUP_HEADERS:
        if 'detail' in group_export_block(group):
            bad.append(f'rpp.{group} exports a detail namespace')
    if not _is_detail_ns('rpp::detail') or _is_detail_ns('rpp::detailed'):
        bad.append('the detail namespace test matches the wrong names')

    # GROUP_HEADERS is hand written, so pin both ways the partition goes stale
    if group_partition_drift(): bad.append('the partition gate reports drift on correct groups')

    real_groups = dict(GROUP_HEADERS)
    first, second = GROUPS[0], GROUPS[1]
    def _groups_patched(group: str, headers: tuple):
        GROUP_HEADERS[group] = headers
        return group_partition_drift()
    try:
        # the same header in two groups declares one entity twice, so an importer of both fails
        shared = GROUP_HEADERS[first][0]
        if not any('already carries' in f for f in _groups_patched(second, real_groups[second] + (shared,))):
            bad.append('a header in two groups passed the partition gate')
        GROUP_HEADERS[second] = real_groups[second]

        orphan = GROUP_HEADERS[first][0]
        drift = _groups_patched(first, real_groups[first][1:])
        if not any(f'no group carries {orphan}' in f for f in drift):
            bad.append('a header no group carries passed the partition gate')
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
        if a.all: bad += (name_collisions() + group_partition_drift() + manual_export_drift()
                          + bugs_citation_drift() + module_name_drift())
    except rd.ClangMissing as e:
        print(f'cannot run: {e}')
        return 1
    for f in bad: print(f'  {f}')
    print(f'== gen_module_exports: {len(bad)} finding(s) over {len(targets)} module(s) ==')
    return 1 if bad else 0  # write mode still fails on a hidden name or a drifted list


if __name__ == '__main__':
    sys.exit(main())
