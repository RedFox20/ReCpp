#!/usr/bin/env python3
"""Include hygiene checker for src/rpp.

Three checks, each with a --check gate for CI:

  self-contained  every header compiles alone, twice, with nothing included before it
  import-order    an #include that follows an import, or an import inside a header
  unused          an #include that the header still compiles without
  missing         a file that uses a std facility it does not include itself

The `unused` check works by deletion. It copies the tree, comments out one
#include, and recompiles that header alone. A header that still compiles did not
need the line. Run `missing` first: a header can look self-contained only because
a sibling header leaks the declaration into it.

A header that re-exports another one for its consumers names nothing from it, and
the deletion test cannot tell that apart from a stale include. Mark the line
`#include "x.h" // re-export` and no check reports it.

Usage:
  tools/check_includes.py self-contained [--check]
  tools/check_includes.py import-order [--check]
  tools/check_includes.py unused [--check] [--only strview.h]
  tools/check_includes.py missing [--check]
"""
import argparse, os, re, shutil, subprocess, sys, tempfile
from functools import lru_cache
from concurrent.futures import ThreadPoolExecutor

SRC = 'src/rpp'
# jni_cpp.h needs the Android NDK jni.h, which a host build does not have
SKIP = {'jni_cpp.h'}
CXX = os.environ.get('CXX', 'clang++')
STD = os.environ.get('CXXSTD', 'c++20')


def _read(path: str) -> str:
    """The file as text. A kept UTF-8 BOM hides the import on line 1, so this drops it."""
    return open(path, encoding='utf-8-sig', errors='replace').read()


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
# a header may re-export another one for its consumers, and then names nothing from it
REEXPORT_RE = re.compile(r'^\s*#\s*include\s*[<"][^>"]+[>"][^\n]*//[^\n]*\bre-export\b', re.M)
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


def describe_failure(header: str, err: str) -> str:
    """One line: the header, the location the compiler stopped at, and its reason."""
    line = next((l for l in err.splitlines() if ' error: ' in l), '')
    if not line:
        return f'{SRC}/{header} does not compile on its own, and the compiler printed no error'
    where, _, why = line.partition(' error: ')
    # the compiler prints an absolute path, which is noise next to the repo-relative header
    where = where.strip().rstrip(':').replace(os.getcwd() + os.sep, '')
    return f'{SRC}/{header} does not compile on its own -- {where}: {why.strip()}'


def strip_comments(src: str) -> str:
    return re.sub(r'//[^\n]*|/\*.*?\*/', '', src, flags=re.S)


def check_self_contained() -> list[str]:
    root = os.path.abspath('src')
    bad = []
    with ThreadPoolExecutor(max_workers=os.cpu_count()) as ex:
        for h, (ok, err) in zip(headers(), ex.map(lambda h: compile_header(h, root), headers())):
            if not ok:
                bad.append(describe_failure(h, err))
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


@lru_cache(maxsize=None)
def own_names(path: str) -> frozenset:
    """Returns the names one file declares itself."""
    src = strip_comments(_read(path))
    names = set()
    for r in DECL_RES:
        names |= set(r.findall(src))
    return frozenset(n for n in names if len(n) > 2)


def declared_names(path: str) -> set[str]:
    """Returns the names a header offers, including the ones it re-exports.

    A header hands the consumer every name its own includes declare. debugging.h
    declares no LogError and re-exports it from debugging.macros.h. A scan that
    stops at the first file therefore reports a used include as unused.
    """
    names, seen, todo = set(), set(), [path]
    while todo:
        f = todo.pop()
        if f in seen or not os.path.isfile(f):
            continue
        seen.add(f)
        names |= own_names(f)
        src = _read(f)
        todo += [os.path.join(SRC, i) for i in QUOTED_RE.findall(src)]
    return names


def _probe_unused(args) -> list[tuple[str, str]]:
    """Returns the includes of one header that the header compiles without."""
    header, worktree = args
    path = os.path.join(worktree, 'rpp', header)
    original = open(path, encoding='utf-8', errors='replace').read()  # written back, so keep the BOM
    found = []
    quoted_spans = {m.start() for m in QUOTED_RE.finditer(original)}
    reexports = {m.start() for m in REEXPORT_RE.finditer(original)}
    try:
        for m in INCLUDE_RE.finditer(original):
            if m.start() in reexports:
                continue  # the author states the consumer needs it, so removing it is not the fix
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
        body = INCLUDE_RE.sub('', strip_comments(_read(f'{SRC}/{header}')))
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
        src = _read(f)
        body = strip_comments(src)
        incs = set(INCLUDE_RE.findall(src))
        need = {p[0] for pat, p in RULES if re.search(pat, body) and not set(p) & incs}
        if need:
            bad.append(f'{f}: needs {", ".join(sorted(need))}')
    return bad


# `%:` is the alternative token for `#`, and a compiler reads a directive written either way
_HASH = r'(?:\#|%:)'
# a directive takes any whitespace that is not a newline, form feed and vertical tab included
_SP = r'[^\S\n]'
# `[^\W\d]` is a C++ identifier start, which `[A-Za-z_]` is not, see module `éclair`. A name
# may also carry a universal-character-name, which is how `import éclair;` spells the same.
_UCN = r'\\[uU][0-9a-fA-F]+'
_NAME = rf'(?:[^\W\d]|{_UCN})(?:[\w.]|{_UCN})*'
# a declaration ends on the same line, at `;` or at the `[` of an attribute. Every operand
# takes that whole shape, so an expression opening with `import` stays an expression.
_END = rf'{_SP}*[;\[]'
IMPORT_RE = re.compile(rf'^{_SP}*(?:export{_SP}+)?import'
                       rf'(?:{_SP}+{_NAME}{_END}|{_SP}*<[^>\n]+>{_END}'
                       rf'|{_SP}*"[^"\n]+"{_END}|{_SP}*:{_SP}*{_NAME}{_END})', re.M)
# `#import` is the Objective-C include-once, and file_io.mm carries no other form
_HEADER_KW = r'(?P<kw>include|import)'
# the ordering scan needs the directive, not its operand, so a macro form still counts
ANY_INCLUDE_RE = re.compile(rf'^{_SP}*{_HASH}{_SP}*{_HEADER_KW}\b', re.M)


_DIRECTIVE_RE = re.compile(rf'{_SP}*(?:{_HASH}{_SP}*(?:include|import)\b|(?:export{_SP}+)?import\b)')

# the longest raw-string delimiter C++ allows
_MAX_RAW_DELIM = 16


def _skip_quoted(src: str, i: int, quote: str) -> int:
    """The index past the literal that opens at `i`, or the newline that leaves it unterminated."""
    j, n = i + 1, len(src)
    while j < n:
        c = src[j]
        if c == '\\':   j += 2       # an escape hides the next character, a splice included
        elif c == '\n':  return j     # a literal does not cross a raw newline
        elif c == quote: return j + 1
        else:            j += 1
    return n


def _skip_raw(src: str, i: int) -> int:
    """The index past the raw string whose quote sits at `i`. Only its own delimiter closes it."""
    open_paren = src.find('(', i + 1)
    if open_paren < 0 or open_paren - (i + 1) > _MAX_RAW_DELIM:
        return _skip_quoted(src, i, '"')  # no delimiter, so read it as an ordinary string
    close = src.find(')' + src[i + 1:open_paren] + '"', open_paren)
    return len(src) if close < 0 else close + (open_paren - i) + 1


# a character literal may carry one of these, and u8 is tried first so u does not win
_CHAR_PREFIXES = ('u8', 'L', 'u', 'U')

# the raw-string prefixes, longest first so u8R wins over R
_RAW_PREFIXES = ('u8R', 'LR', 'uR', 'UR', 'R')


def _opens_raw_string(src: str, i: int) -> bool:
    """True when the quote at `i` carries a raw-string prefix, and not the tail of a name.

    A prefix opens its own token, so `fooR"(a"` holds an ordinary string. A raw read runs to
    the next `)"`, which can blank every include after it.
    """
    for prefix in _RAW_PREFIXES:
        if i >= len(prefix) and src.startswith(prefix, i - len(prefix)):
            start = i - len(prefix)
            return start == 0 or not (src[start - 1].isalnum() or src[start - 1] == '_')
    return False


def _opens_char_literal(src: str, i: int) -> bool:
    """True when the quote at `i` opens a literal, and False when it separates digits, see 1'000."""
    start = i
    for prefix in _CHAR_PREFIXES:
        if i >= len(prefix) and src.startswith(prefix, i - len(prefix)):
            start = i - len(prefix)
            break
    if start == 0 or not (src[start - 1].isalnum() or src[start - 1] == '_'):
        return True
    # walk to the start of the token before the quote. A token that opens with a digit is a
    # number, so the quote separates digits. Anything else is a name, as in `return'x'`.
    j = start - 1
    while j > 0 and (src[j - 1].isalnum() or src[j - 1] in "_'"):
        j -= 1
    return not src[j].isdigit()


def _code_only(src: str) -> str:
    """The source with every comment and literal blanked, the newlines inside them included.

    A regex cannot decide which construct opens first, so this reads the file once and
    tracks what it is inside. A quote inside a comment opens no string, a `/*` inside any
    literal opens no comment, and a raw string ends only at its own delimiter. A directive
    keeps its operand, because `#include "x.h"` carries a path the scan must still read.
    A comment and a raw string are single tokens, so a newline inside one starts no line.
    A `#` that follows one is a stray token, not a directive.
    """
    out, i, n = list(src), 0, len(src)
    while i < n:
        c = src[i]
        if c == '/' and i + 1 < n and src[i + 1] in '/*':
            if src[i + 1] == '/':
                end = src.find('\n', i)
                end = n if end < 0 else end
            else:
                end = src.find('*/', i + 2)
                end = n if end < 0 else end + 2
        elif c == '"':
            end = _skip_raw(src, i) if _opens_raw_string(src, i) else _skip_quoted(src, i, '"')
            # `#include "x.h"` and `import "x.h"` name a header, so that operand is not a literal.
            # the line reads back from `out`, where a comment before the operand is already gone.
            line = ''.join(out[src.rfind('\n', 0, i) + 1:i])
            directive = _DIRECTIVE_RE.match(line)
            if directive and not line[directive.end():].strip():
                i = end
                continue
        elif c == "'" and _opens_char_literal(src, i):
            end = _skip_quoted(src, i, "'")
        else:
            i += 1
            continue
        for k in range(i, end):
            out[k] = ' '
        i = end
    return ''.join(out)


def _translate(src: str) -> tuple:
    """Phase 2 and phase 3 of translation: splice, then blank every comment and literal.

    A compiler joins a backslash-newline before it reads anything else, so `// note \\` keeps
    the next line inside the comment and a keyword may be split across two lines. The returned
    list gives each character its index in `src`, so a finding still names the line it came from.
    """
    spliced, origin, i, n = [], [], 0, len(src)
    while i < n:
        if src[i] == '\\' and src.startswith('\n', i + 1):
            i += 2
            continue
        spliced.append(src[i])
        origin.append(i)
        i += 1
    return _code_only(''.join(spliced)), origin


def scan_import_order(name: str, src: str) -> str:
    """The finding for one file, or an empty string. Takes the text, so a case needs no tree."""
    code, origin = _translate(src)
    imports = [m.start() for m in IMPORT_RE.finditer(code)]
    if not imports:
        return ''
    line_of = lambda k: src.count('\n', 0, origin[k]) + 1
    if name.endswith(('.h', '.inl')):  # a .inl is header content, so a consumer inherits its import
        return (f'{name}:{line_of(imports[0])}: a header carries an import, which every '
                'consumer inherits and cannot reorder')
    late = [m for m in ANY_INCLUDE_RE.finditer(code) if m.start() > imports[0]]
    if not late:
        return ''
    return (f'{name}:{line_of(late[0].start())}: this #{late[0].group("kw")} follows the import '
            f'on line {line_of(imports[0])}, move every #include and #import above it')


def check_import_order() -> list[str]:
    """Every #include must come before every import, and a header carries no import.

    A module makes the declarations of its own included headers reachable in the importing
    file. A std header parsed after the import re-declares them, and GCC 14 stops with about
    a thousand redefinition errors. Clang accepts the same file, so only GCC catches it.
    """
    # a header under tests/ carries the same rule, so the walk reads every source extension
    files = ([f'{SRC}/{f}' for f in sorted(os.listdir(SRC)) if f.endswith(SOURCES)]
             + sorted(f'{r}/{f}' for r, _, fs in os.walk('tests') for f in fs
                      if f.endswith(SOURCES)))
    return [f for f in (scan_import_order(p, _read(p)) for p in sorted(set(files))) if f]


# every extension the walk reads. A case below names one, so the table drives this list.
SOURCES = ('.h', '.inl', '.cpp', '.cppm', '.mm')

# Each case is a file name, its exact bytes, and the line a finding must name. 0 means the
# scan must stay silent. The name carries the extension, because the header rule reads it.
SELFTEST = [
    ('late_include.cppm',        b'import m;\n#include <string>\n', 2),
    ('includes_first.cppm',      b'#include <string>\nimport m;\n', 0),
    ('header_import.h',          b'import m;\n', 1),
    ('inline_import.inl',        b'import m;\n', 1),
    ('digraph.cppm',             b'import m;\n%:include <string>\n', 2),
    ('export_import.cppm',       b'export import m;\n#include <string>\n', 2),
    ('unit_angled.cppm',         b'import <string>;\n#include <vector>\n', 2),
    ('unit_quoted.cppm',         b'import "x.h";\n#include <vector>\n', 2),
    ('partition.cppm',           b'import :part;\n#include <string>\n', 2),
    ('partition_tight.cppm',     b'import:part;\n#include <string>\n', 2),
    ('label_named_import.h',     b'inline int f(int x){ if(x) goto import;\nimport: return 1;\n}\n', 0),
    ('important.cppm',           b'import m;\nint important = 1;\n', 0),
    ('compare_named_import.h',   b'inline bool f(int import) {\n  return\nimport < 5;\n}\n', 0),
    ('compare_chain.h',          b'inline bool f(int import) {\n  return\nimport < 5 > 0;\n}\n', 0),
    ('operator_named_import.h',  b'inline int f(int import,int x) {\n  return\nimport xor x;\n}\n', 0),
    ('unicode_module.cppm',      b'import \xc3\xa9clair;\n#include <string>\n', 2),
    ('ucn_module.cppm',          b'import \\u00e9clair;\n#include <string>\n', 2),
    ('unit_angled_space.cppm',   b'import <foo bar>;\n#include <string>\n', 2),
    ('unit_attribute.cppm',      b'import m [[deprecated]];\n#include <string>\n', 2),
    ('objective_cpp.mm',         b'import m;\n#include <string>\n', 2),
    ('objc_import.mm',           b'import m;\n#import <Foundation/Foundation.h>\n', 2),
    ('objc_import_first.mm',     b'#import <Foundation/Foundation.h>\nimport m;\n', 0),
    # phase 2, the line splice
    ('keyword_splice.cppm',      b'imp\\\nort m;\n#include <string>\n', 3),
    ('comment_splice.cppm',      b'import m;\n// note \\\n#include <string>\n', 0),
    ('double_backslash.cppm',    b'import m;\n// note \\\\\n', 0),
    # the reader decodes in text mode, so a CRLF file reaches the scan as LF
    ('crlf_keyword.cppm',        b'imp\\\r\nort m;\r\n#include <string>\r\n', 3),
    ('crlf_comment.cppm',        b'import m;\r\n// note \\\r\n#include <string>\r\n', 0),
    # phase 3, the comment and the literal
    ('comment_in_import.cppm',   b'import/* split\n*/m;\n#include <string>\n', 3),
    ('joined_comment.cppm',      b'import m; /* multi\nline */ #include <string>\n', 0),
    ('comment_before_operand.cppm', b'import/**/"foo.h";\n#include <string>\n', 2),
    ('comment_before_keyword.cppm', b'/*c*/import "x.h";\n#include <vector>\n', 2),
    ('include_in_comment.cppm',  b'import m;\n/* #include <string> */\n', 0),
    ('include_in_raw.cppm',      b'import m;\nconst char* s = R"(\n#include <string>\n)";\n', 0),
    ('include_in_lr_raw.cppm',   b'import m;\nconst char* s = LR"d(\n#include <string>\n)d";\n', 0),
    ('name_ends_in_r.cppm',      b'import m;\nconst char* s = fooR"(a";\n#include <string>\n', 3),
    ('char_after_keyword.cppm',  b"int g(){ return'/*'; }\nimport m;\n#include <string>\n", 3),
    ('char_prefixed.cppm',       b"int g(){ return L'/*'; }\nimport m;\n#include <string>\n", 3),
    ('digit_separator.cppm',     b"import m;\nint g(){ return 1'000'000; }\n", 0),
    ('quote_in_char.cppm',       b'import m;\nchar g(){ return \'"\'; }\n', 0),
    # whitespace and encoding
    ('form_feed.cppm',           b'import m;\n\f#include <string>\n', 2),
    ('form_feed_in_import.cppm', b'import\fm;\n#include <string>\n', 2),
    ('vertical_tab.cppm',        b'import m;\n\v#include <string>\n', 2),
    ('utf8_bom.cppm',            b'\xef\xbb\xbfimport m;\n#include <string>\n', 2),
]


def check_selftest() -> list[str]:
    """Runs the scan over crafted sources, so a regression in the scan fails the gate.

    The import-order gate only reads a conforming tree, where a scan that finds nothing still
    passes. Each case names one construct the scan has to report, or one it must not, and it
    pins the reported line so a splice or a comment cannot shift it.
    """
    bad = []
    with tempfile.TemporaryDirectory() as d:
        for name, body, want in SELFTEST:
            # the scan reads the text, so only this check sees an extension the walk skips
            if not name.endswith(SOURCES):
                bad.append(f'{name}: the walk reads no {os.path.splitext(name)[1]} file')
            path = os.path.join(d, name)
            open(path, 'wb').write(body)
            found = scan_import_order(name, _read(path))
            got = int(found.split(':')[1]) if found else 0
            if got != want:
                bad.append(f'{name}: the scan named line {got}, not {want}')
    return bad


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('mode', choices=['self-contained', 'unused', 'missing', 'import-order',
                                 'selftest', 'all'])
    ap.add_argument('--check', action='store_true', help='exit 1 when a finding remains')
    ap.add_argument('--only', help='limit the unused scan to one header')
    a = ap.parse_args()

    every = ['selftest', 'self-contained', 'import-order', 'missing', 'unused']
    modes = every if a.mode == 'all' else [a.mode]
    gated = 0
    for mode in modes:
        if mode == 'selftest':       found, gate = (lambda r: (r, len(r)))(check_selftest())
        elif mode == 'self-contained': found, gate = (lambda r: (r, len(r)))(check_self_contained())
        elif mode == 'import-order': found, gate = (lambda r: (r, len(r)))(check_import_order())
        elif mode == 'missing':      found, gate = (lambda r: (r, len(r)))(check_missing())
        else:                        found, gate = check_unused(a.only)
        noun, verb = ('finding', 'fails') if gate == 1 else ('findings', 'fail')
        print(f'== {mode}: {gate} {noun} that {verb} the gate ==')
        for line in found: print(f'  {line}')
        gated += gate
    return 1 if (a.check and gated) else 0


if __name__ == '__main__':
    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    sys.exit(main())
