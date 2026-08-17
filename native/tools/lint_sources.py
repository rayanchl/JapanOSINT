#!/usr/bin/env python3
"""lint_sources.py — mechanical checks over the collector tree.

The audit's core finding was not that fixes are missing: it is that *every*
fix already exists somewhere in this tree and the dominant failure mode is
silence — a bad pattern is fixed in one file and left in forty others, and
nothing anywhere notices. Nine of those patterns are mechanically checkable.
This is that checker.

Standard library only (no pip): it has to run in CI, in WSL, and on a
developer box with nothing installed.

Usage:
  tools/lint_sources.py                 # run every check against the baseline
  tools/lint_sources.py --count         # print the real registered-source count
  tools/lint_sources.py --list-ids      # every registered source id, one per line
  tools/lint_sources.py --by-collector  # counts per .collector group
  tools/lint_sources.py --check dup-id  # run one check (repeatable)
  tools/lint_sources.py --verbose       # list every finding, not just the first 20
  tools/lint_sources.py --write-baseline  # re-record the current counts

Exit status: 0 when every check is at or below its recorded baseline,
1 when any check regressed, 2 on a usage/内部 error.

WHY A BASELINE.  These patterns have thousands of pre-existing instances.
A checker that fails on day one gets disabled on day one. So each check
records a known-bad count in tools/lint_baseline.json and only fails when the
count goes UP. The number can only ratchet down; every fix that lands should
be followed by --write-baseline so the new floor is locked in.

WHY MACRO EXPANSION.  A naive `grep -c REGISTER_SOURCE` is wrong and its
wrongness is already enshrined in six tracked documents with six different
source counts. `RSSX()` in collectors/sources/arxiv_feeds.c expands to a full
source_def *plus* its own REGISTER_SOURCE, so one line there is 41 sources.
This tool inlines local `.inc` includes and expands the file's own
function-like and object-like macros before counting, which is why its number
disagrees with every count previously written down.
"""

import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)                 # native/
REPO = os.path.dirname(NATIVE)                 # repo root
COLLECTORS = os.path.join(NATIVE, "collectors")
SRC_DIR = os.path.join(COLLECTORS, "sources")  # the shared .inc/.h still live here
GEN_REGISTRY = os.path.join(NATIVE, "core", "source_registry.gen.c")
BASELINE_PATH = os.path.join(HERE, "lint_baseline.json")

MAX_EXPANDED = 12 * 1024 * 1024   # per-TU guard against a runaway expansion


# --------------------------------------------------------------------------
# Source reading / comment stripping
# --------------------------------------------------------------------------

def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return fh.read()


_COMMENT_SCAN = re.compile(
    r'"(?:\\.|[^"\\\n])*"'          # string literal
    r"|'(?:\\.|[^'\\\n])*'"         # char literal
    r"|/\*.*?\*/"                   # block comment
    r"|//[^\n]*",                   # line comment
    re.S)


def strip_comments(text):
    """Drop comments, keep line count stable so reported lines stay true."""
    def sub(m):
        s = m.group(0)
        if s.startswith("/*") or s.startswith("//"):
            return "\n" * s.count("\n")
        return s
    return _COMMENT_SCAN.sub(sub, text)


def join_continuations(text):
    return re.sub(r"\\\n", " ", text)


# --------------------------------------------------------------------------
# A very small C preprocessor: local #include "*.inc", #define, expansion.
#
# This is deliberately not a real preprocessor. It only has to be right about
# the one construct that matters — a macro whose body contains a source_def
# and a REGISTER_SOURCE — and honest about everything it cannot resolve.
# --------------------------------------------------------------------------

_DEFINE_RE = re.compile(
    r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)(\([^)]*\))?[ \t]*(.*)$", re.M)


def inline_includes(path, seen=None):
    """Textually inline local `#include "…​.inc"` (never compiled standalone)."""
    if seen is None:
        seen = set()
    real = os.path.abspath(path)
    if real in seen:
        return ""
    seen.add(real)
    text = read(path)
    base = os.path.dirname(real)
    out = []
    pos = 0
    for m in re.finditer(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+\.inc)"[ \t]*$',
                         text, re.M):
        out.append(text[pos:m.start()])
        # Mirror the compiler's -iquote search: the including file's own
        # directory first, then collectors/sources where the shared .inc files
        # live. Without the second path a collector that has MOVED out of
        # sources/ silently fails to inline _verified_macros.inc, its VJSON
        # invocations never expand, and the file reports ZERO sources while
        # compiling and registering all of them correctly. Measured: the count
        # collapsed 9,717 -> 1,801 the moment 181 generated files moved into
        # feed/generated/, with the binary still registering 10,323.
        inc = os.path.join(base, m.group(1))
        if not os.path.exists(inc):
            inc = os.path.join(SRC_DIR, m.group(1))
        out.append(inline_includes(inc, seen) if os.path.exists(inc) else "")
        pos = m.end()
    out.append(text[pos:])
    return "".join(out)


def collect_defines(text):
    """Pull #define out of `text`; return (obj_macros, fn_macros, rest)."""
    obj, fn = {}, {}
    rest = []
    pos = 0
    for m in _DEFINE_RE.finditer(text):
        rest.append(text[pos:m.start()])
        name, params, body = m.group(1), m.group(2), m.group(3).strip()
        if params is None:
            obj[name] = body
        else:
            plist = [p.strip() for p in params[1:-1].split(",") if p.strip()]
            fn[name] = (plist, body)
        pos = m.end()
    rest.append(text[pos:])
    return obj, fn, "".join(rest)


def _split_args(text, open_idx):
    """Parse a macro argument list starting at text[open_idx] == '('.

    Returns (args, index_after_close) or (None, None) when unbalanced.
    Respects nesting, string and char literals — collector macros routinely
    pass JSON tag arrays containing commas inside a string.
    """
    depth = 0
    i = open_idx
    start = open_idx + 1
    args = []
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            q = c
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == q:
                    break
                i += 1
            i += 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
            if depth == 0:
                args.append(text[start:i])
                return [a.strip() for a in args], i + 1
        elif c == "," and depth == 1:
            args.append(text[start:i])
            start = i + 1
        i += 1
    return None, None


_TOKEN = re.compile(
    r'"(?:\\.|[^"\\])*"'
    r"|'(?:\\.|[^'\\])*'"
    r"|\b[A-Za-z_]\w*\b")


def _substitute(body, params, args):
    """Substitute macro arguments, honouring `#x` stringify and `a##b` paste."""
    amap = dict(zip(params, args))
    out = []
    pos = 0
    for m in _TOKEN.finditer(body):
        tok = m.group(0)
        out.append(body[pos:m.start()])
        pos = m.end()
        if tok[0] in "\"'":
            out.append(tok)
            continue
        if tok in amap:
            prev = body[:m.start()].rstrip()
            if prev.endswith("#") and not prev.endswith("##"):
                out[-1] = out[-1].rstrip()[:-1]
                out.append('"%s"' % amap[tok].replace("\\", "\\\\")
                           .replace('"', '\\"'))
            else:
                out.append(amap[tok])
        else:
            out.append(tok)
    out.append(body[pos:])
    return re.sub(r"\s*##\s*", "", "".join(out))


def _apply(params, body, args):
    """Bind args to params (handling variadics) and substitute. None on arity
    mismatch — the invocation is then left alone rather than mangled.

    Variadic registration macros are real here: `SOC_SOURCE(sym, ID, …, IVAL,
    ...)` in collectors/sources/reg2_socrata_registries.c registers 14 sources
    that a fixed-arity expander silently misses."""
    variadic = bool(params) and params[-1].endswith("...")
    if not variadic:
        if len(args) != len(params):
            return None
        return _substitute(body, params, args)
    fixed = params[:-1]
    vname = "__VA_ARGS__" if params[-1] == "..." else params[-1][:-3].strip()
    if len(args) < len(fixed):
        return None
    rest = ", ".join(args[len(fixed):])
    if not rest.strip():
        # GNU `, ##__VA_ARGS__` comma elision: drop the comma with the arg.
        body = re.sub(r",\s*##\s*" + re.escape(vname) + r"\b", "", body)
    return _substitute(body, fixed + [vname], args[:len(fixed)] + [rest])


def expand_macros(text, obj, fn, depth=0):
    """Expand the TU's own macros until stable (bounded)."""
    if depth > 10 or len(text) > MAX_EXPANDED:
        return text
    changed = False
    out = []
    i = 0
    n = len(text)
    while i < n:
        m = _TOKEN.search(text, i)
        if not m:
            out.append(text[i:])
            break
        out.append(text[i:m.start()])
        tok = m.group(0)
        if tok[0] in "\"'":
            out.append(tok)
            i = m.end()
            continue
        if tok in fn:
            j = m.end()
            while j < n and text[j] in " \t\r\n":
                j += 1
            if j < n and text[j] == "(":
                args, end = _split_args(text, j)
                params, body = fn[tok]
                sub = _apply(params, body, args) if args is not None else None
                if sub is not None:
                    out.append(sub)
                    i = end
                    changed = True
                    continue
            out.append(tok)
            i = m.end()
            continue
        if tok in obj:
            out.append(obj[tok])
            i = m.end()
            changed = True
            continue
        out.append(tok)
        i = m.end()
    result = "".join(out)
    if changed:
        return expand_macros(result, obj, fn, depth + 1)
    return result


def _relevant_macros(obj, fn):
    """Keep only macros worth expanding.

    Expanding everything is both slow and pointless: we only need macros that
    (transitively) produce a source_def/REGISTER_SOURCE, plus short object-like
    macros, which are how collector files parametrise COLL/INTERVAL/category
    inside those very definitions.
    """
    keep_fn = {}
    for _ in range(4):
        grew = False
        for name, (params, body) in fn.items():
            if name in keep_fn:
                continue
            if ("REGISTER_SOURCE" in body or "source_def" in body
                    or any(k + "(" in body.replace(" ", "") for k in keep_fn)):
                keep_fn[name] = (params, body)
                grew = True
        if not grew:
            break
    keep_obj = {k: v for k, v in obj.items() if len(v) <= 200 and k != v}
    return keep_obj, keep_fn


def preprocess_tu(path):
    """.c file -> macro-expanded, comment-free text."""
    text = join_continuations(strip_comments(inline_includes(path)))
    obj, fn, rest = collect_defines(text)
    obj, fn = _relevant_macros(obj, fn)
    return expand_macros(rest, obj, fn)


# --------------------------------------------------------------------------
# Registration extraction
# --------------------------------------------------------------------------

_STRLIT = re.compile(r'\s*"((?:\\.|[^"\\])*)"')


def _read_string_concat(text, pos):
    """Read one or more adjacent string literals; None if not a literal."""
    parts = []
    while True:
        m = _STRLIT.match(text, pos)
        if not m:
            break
        parts.append(m.group(1))
        pos = m.end()
    return "".join(parts) if parts else None


def _brace_block(text, open_idx):
    depth = 0
    i = open_idx
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            q = c
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == q:
                    break
                i += 1
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[open_idx:i + 1]
        i += 1
    return text[open_idx:]


_DEF_RE = re.compile(r"\bsource_def\s+([A-Za-z_]\w*)\s*=\s*\{")
_REG_RE = re.compile(r"\bREGISTER_SOURCE\s*\(\s*([A-Za-z_]\w*)\s*\)")


_HP_REG_RE = re.compile(r"\bHP_REGISTER_TABLE\s*\(\s*([A-Za-z_]\w*)\s*\)")


def hp_table_registrations(text):
    """-> list of (id, symbol, collector) for every hp_source table row.

    THE BLIND SPOT THIS CLOSES. hp rows do not go through REGISTER_SOURCE: a
    table declares N rows and HP_REGISTER_TABLE hands them to hp_register(),
    which builds the source_defs at startup. So the counter above saw ZERO of
    them — 30 shipped tables' worth — and `make source-floor`, whose entire job
    is to fail when a source vanishes, could not see an hp row disappear.

    It showed up when 362 verified rows moved from VJSON onto hpengine to pick
    up their detail hops: the count fell by exactly 362 while nothing had been
    lost, and the floor gate stayed green through a change it should have had an
    opinion about."""
    out = []
    for m in _HP_REG_RE.finditer(text):
        tbl = m.group(1)
        dm = re.search(r"\bhp_source\s+" + re.escape(tbl) + r"\s*\[\s*\]\s*=\s*\{",
                       text)
        if not dm:
            continue
        block = _brace_block(text, dm.end() - 1)
        # _brace_block hands back the table's OWN braces too, so the rows are
        # one level in from here.
        inner = block[1:-1] if block.startswith("{") else block
        for row in _split_top_level_braces(inner):
            idm = re.search(r"\.id\s*=", row)
            if not idm:
                continue
            sid = _read_string_concat(row, idm.end())
            cm = re.search(r"\.collector\s*=", row)
            coll = _read_string_concat(row, cm.end()) if cm else None
            if sid:
                out.append((sid, tbl, coll))
    return out


def _split_top_level_braces(text):
    """Yield each `{...}` element of an initialiser list, skipping over string
    literals. String-awareness is not optional here: an hp row's own fields
    carry braces INSIDE quotes — `.detail_url = "https://…/catalog/{v}"` — and a
    scan that counts those as nesting splits rows in the wrong places and loses
    most of the table."""
    depth, start, i, n = 0, None, 0, len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            q = c
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == q:
                    break
                i += 1
        elif c == "{":
            depth += 1
            if depth == 1:
                start = i
        elif c == "}":
            depth -= 1
            if depth == 0 and start is not None:
                yield text[start:i + 1]
                start = None
        i += 1


def registrations(path):
    """-> (list of (id, symbol, collector), list of unresolved symbols)."""
    text = preprocess_tu(path)
    defs = {}
    for m in _DEF_RE.finditer(text):
        block = _brace_block(text, m.end() - 1)
        idm = re.search(r"\.id\s*=", block)
        cm = re.search(r"\.collector\s*=", block)
        sid = _read_string_concat(block, idm.end()) if idm else None
        coll = _read_string_concat(block, cm.end()) if cm else None
        defs[m.group(1)] = (sid, coll)
    found, unresolved = [], []
    for m in _REG_RE.finditer(text):
        sym = m.group(1)
        sid, coll = defs.get(sym, (None, None))
        if sid:
            found.append((sid, sym, coll))
        else:
            unresolved.append(sym)
    found.extend(hp_table_registrations(text))
    return found, unresolved


def source_files():
    """Every TU that can register a source.

    Not just collectors/sources/: core/db.c, core/media.c, core/translate.c and
    core/camera_stills.c each REGISTER_SOURCE an internal maintenance/enrich
    pod. Scanning only the collector directory undercounted the registry by 17
    against what the built binary reports from --list-sources.

    RECURSIVE under collectors/, because a collector organised into a
    subdirectory still registers. A flat listdir() here is the same trap the
    Makefile had: it reported 1,801 sources the moment 221 files moved, while
    the binary went on registering 10,323. The other three roots stay flat --
    core/, lib/ and native/ itself have no collector subtree.
    """
    out = []
    for root, _dirs, names in os.walk(COLLECTORS):
        out.extend(os.path.join(root, f) for f in names if f.endswith(".c"))
    for d in (os.path.join(NATIVE, "core"), os.path.join(NATIVE, "lib"), NATIVE):
        if not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            p = os.path.join(d, f)
            if f.endswith(".c") and os.path.isfile(p):
                out.append(p)
    return sorted(out)


def collector_files():
    """Every .c under collectors/ — the files that emit intel rows.

    RECURSIVE, and it has to stay that way. This used to list SRC_DIR flatly,
    which is the same trap the Makefile had: once collectors are organised into
    subdirectories a listdir() stops seeing them, --count silently drops, and
    the number every doc quotes (and `make source-floor` asserts) is wrong while
    the binary registers all of them correctly. Walk COLLECTORS, not SRC_DIR.
    """
    out = []
    for root, _dirs, names in os.walk(COLLECTORS):
        for f in names:
            if f.endswith(".c"):
                out.append(os.path.join(root, f))
    return sorted(out)


_REG_CACHE = {}


def all_registrations():
    """path -> [(id, symbol)] for the whole collector tree (cached)."""
    if not _REG_CACHE:
        for p in source_files():
            found, unresolved = registrations(p)
            _REG_CACHE[p] = (found, unresolved)
    return _REG_CACHE


# --------------------------------------------------------------------------
# Checks. Each returns a list of "file:line: message" strings.
# --------------------------------------------------------------------------

def check_dup_id():
    """Duplicate source ids. registry_get() returns the first match, so the
    second definition is scheduled but never dispatchable — a silent failure
    registry.c already warns about at runtime; this catches it at build time."""
    seen = {}
    out = []
    for path, (found, _) in sorted(all_registrations().items()):
        rel = os.path.relpath(path, REPO)
        for sid, _sym, _coll in found:
            if sid in seen:
                out.append("%s: duplicate source id '%s' (also in %s)"
                           % (rel, sid, seen[sid]))
            else:
                seen[sid] = rel
    return out


def check_unresolved_id():
    """REGISTER_SOURCE'd symbols whose .id this tool could not resolve.
    Not a defect by itself — it means the count below is a lower bound, so it
    is reported rather than hidden."""
    out = []
    for path, (_, unresolved) in sorted(all_registrations().items()):
        rel = os.path.relpath(path, REPO)
        for sym in unresolved:
            out.append("%s: REGISTER_SOURCE(%s) — .id not statically resolvable"
                       % (rel, sym))
    return out


def _gen_registry_ids():
    text = strip_comments(read(GEN_REGISTRY))
    ids = []
    for m in re.finditer(r"^\s*\{\s*\"((?:\\.|[^\"\\])*)\"", text, re.M):
        ids.append((m.group(1), text[:m.start()].count("\n") + 1))
    return ids


def check_registry_orphan():
    """Rows in core/source_registry.gen.c with no implementation.

    That table is the curated metadata for the map layers. A row with no
    registered source_def describes a source that cannot ever run, so it
    inflates /api/sources and /api/layers with entries that can never report
    a status — exactly the failure the file's own header comment records
    having cleaned up once for traffic-cameras/public-webcams."""
    live = {sid for _, (found, _) in all_registrations().items()
            for sid, _sym, _coll in found}
    rel = os.path.relpath(GEN_REGISTRY, REPO)
    return ["%s:%d: registry row '%s' has no registered source_def"
            % (rel, line, sid)
            for sid, line in _gen_registry_ids() if sid not in live]


# `return n > 0 ? 0 : -1` and its casted variants. An honest empty fetch
# (upstream had nothing new) returns -1, which the scheduler records as a
# failure and eventually quarantines the source — so a *working* collector
# gets switched off for succeeding quietly.
_QUARANTINE_RE = re.compile(
    r"return\s*\(?\s*(?:\(\s*(?:int|long|size_t|ssize_t|unsigned)"
    r"(?:\s+\w+)?\s*\)\s*)?[A-Za-z_]\w*\s*\)?\s*>\s*0\s*\?\s*0\s*:\s*-\s*1")


def _scan_lines(paths, regex, message):
    out = []
    for path in paths:
        text = strip_comments(read(path))
        for m in regex.finditer(text):
            line = text[:m.start()].count("\n") + 1
            out.append("%s:%d: %s" % (os.path.relpath(path, REPO), line, message))
    return out


def _tree_c_files():
    roots = [SRC_DIR, os.path.join(NATIVE, "core"), os.path.join(NATIVE, "lib")]
    out = []
    for r in roots:
        if not os.path.isdir(r):
            continue
        for f in sorted(os.listdir(r)):
            if f.endswith(".c") or f.endswith(".inc"):
                out.append(os.path.join(r, f))
    return out


def check_quarantine_empty():
    return _scan_lines(_tree_c_files(), _QUARANTINE_RE,
                       "`return n > 0 ? 0 : -1` — an honest empty fetch is "
                       "reported as failure and quarantines the source")


# `off += snprintf(buf + off, sizeof buf - off, …)`: snprintf returns the
# length it WOULD have written, so once it truncates, `off` runs past the
# buffer, `buf + off` is out of bounds and `sizeof buf - off` underflows to a
# huge size_t. The fix in this tree is a clamp on the accumulator.
_SNPRINTF_ACC = re.compile(
    r"([A-Za-z_]\w*)\s*\+=\s*(?:\(\s*\w+(?:\s+\w+)?\s*\)\s*)?snprintf\s*\(",
    re.M)


# A guard is any comparison of the accumulator against something that is a
# capacity: sizeof, an ALL_CAPS constant, an identifier that reads like a size
# (cap/len/sz/size/max/lim), or a literal. `o + 4 < cap` and
# `sw + 2 < sizeof summary` are both real guards and both appear in this tree.
_GUARD_TMPL = (r"\b%s\b\s*(?:[-+]\s*\w+\s*)?(?:>=|>|<|<=)\s*\(?\s*"
               r"(?:\(\s*\w+(?:\s+\w+)?\s*\)\s*)?"
               r"(?:sizeof\b|[A-Z][A-Z_0-9]{1,}\b"
               r"|\w*(?:cap|len|sz|size|max|lim|room|rem)\w*\b|[1-9]\d*)")
_CLAMP_TMPL = r"\b%s\b\s*=\s*\(?\s*(?:\(\s*\w+\s*\)\s*)?(?:sizeof|[A-Z][A-Z_0-9]+)"


def check_snprintf_guard():
    out = []
    for path in _tree_c_files():
        text = strip_comments(read(path))
        lines = text.split("\n")
        for m in _SNPRINTF_ACC.finditer(text):
            var = m.group(1)
            ln = text[:m.start()].count("\n")
            # Look at the surrounding block: the guard is usually the enclosing
            # loop condition just above, or an explicit clamp just below.
            window = "\n".join(lines[max(0, ln - 8):ln + 12])
            guard = re.search(_GUARD_TMPL % re.escape(var), window)
            clamp = re.search(_CLAMP_TMPL % re.escape(var), window)
            if not guard and not clamp:
                out.append("%s:%d: `%s += snprintf(...)` with no remaining-space "
                           "guard on %s" % (os.path.relpath(path, REPO),
                                            ln + 1, var, var))
    return out


# sqlite3_last_insert_rowid() returns the rowid of the last SUCCESSFUL insert
# on the connection — which, after an sqlite3_step() whose status was thrown
# away, may be some other statement's row entirely. core/intel.c:109 already
# carries the comment explaining this; everywhere else has to be checked.
# A *statement-position* sqlite3_step(): its status goes nowhere. Steps whose
# result is assigned or compared (`inserted = (sqlite3_step(s) == SQLITE_DONE)`,
# as core/intel.c does) are correct and must not be flagged.
_STEP_UNCHECKED = re.compile(r"(?:^|[;{}])[ \t]*sqlite3_step\s*\([^;)]*\)\s*;", re.M)
_ROWID = re.compile(r"sqlite3_last_insert_rowid\s*\(")


def check_rowid_unchecked():
    out = []
    for path in _tree_c_files():
        text = strip_comments(read(path))
        lines = text.split("\n")
        for m in _ROWID.finditer(text):
            ln = text[:m.start()].count("\n")
            window = "\n".join(lines[max(0, ln - 10):ln])
            if _STEP_UNCHECKED.search(window):
                out.append("%s:%d: last_insert_rowid() read after an unchecked "
                           "sqlite3_step()" % (os.path.relpath(path, REPO), ln + 1))
    return out


# Any row carrying a coordinate has to say how good that coordinate is.
# Without geo_precision the map cannot tell an exact camera fix from a
# prefecture centroid, and the audit's fabricated-geometry findings all look
# identical to real geometry at the API boundary.
_EMITS_GEO = re.compile(r"\.has_geo\s*=\s*1|\.geometry_geojson\s*=\s*[^N]|"
                        r"\bhas_geo\s*=\s*1\b")


def check_geo_precision():
    out = []
    for path in collector_files():
        text = strip_comments(read(path))
        if "geo_precision" in text:
            continue
        m = _EMITS_GEO.search(text)
        if m:
            ln = text[:m.start()].count("\n") + 1
            out.append("%s:%d: emits geometry with no geo_precision property"
                       % (os.path.relpath(path, REPO), ln))
    return out


def check_dup_endpoint():
    """The same upstream endpoint fetched by more than one collector file.

    Why this is a real defect and not just untidiness: core/intel.c keys every
    row as uid = "<source_id>|<remote_key>", so two collectors hitting one
    endpoint do NOT dedupe — the same upstream record is stored twice in
    intel_items and twice in the FTS mirror, and we spend two requests to get
    it. At the time this check was added, 531 endpoints were fetched by 2+
    files. The dominant pattern is cross-batch: a later generated batch
    re-discovered endpoints an earlier one already had (vsrc13 vs vsrc2), plus
    the world_reg_* / reg_* registry families covering the same ground.

    Normalised on host+path+query, lowercased host, trailing slash dropped, so
    a scheme change or a stray slash is not counted as a second endpoint.

    Baselined rather than hard-failing: untangling 531 pairs is a per-pair
    judgement (some are genuinely different query shapes against one base), so
    this ratchets down instead of blocking. Adding a NEW duplicate fails.
    """
    from urllib.parse import urlsplit
    # A comma is LEGAL inside a query value and must not end the match.
    # Excluding it truncated `?from=act,trans&trans_country_code=BW` to
    # `?from=act`, so eleven distinct per-country IATI endpoints collapsed onto
    # one key and were reported as duplicates of each other. The closing quote
    # already terminates a URL in a C string literal, so `,` never needed to.
    url_re = re.compile(r'https?://[^\s"\')\\]+')
    by_url = {}
    for root, _dirs, names in os.walk(COLLECTORS):
        if os.sep + "obj" in root:
            continue
        for f in names:
            if not f.endswith((".c", ".inc")):
                continue
            p = os.path.join(root, f)
            for m in url_re.findall(read(p)):
                u = m.rstrip('".,)\\')
                try:
                    s = urlsplit(u)
                except ValueError:
                    continue
                if not s.netloc:
                    continue
                # A {v} template is a PER-RECORD detail route, not an endpoint.
                # It expands to one URL per id drawn from its own row's list, so
                # 24 country rows sharing ".../catalog/{v}" fetch 24 disjoint
                # record sets — the opposite of the duplicated work this check
                # looks for. Counting them collapsed every shared platform hop
                # (IHSN, GLEIF, d-portal, UNDP) into a false duplicate.
                # Note {v} specifically and not the other tokens: two rows
                # sharing a {q} SEARCH url really would answer the same query
                # twice, and that is still worth reporting.
                if "{v}" in u:
                    continue
                key = (s.netloc.lower(), s.path.rstrip("/"), s.query)
                by_url.setdefault(key, set()).add(os.path.relpath(p, REPO))
    out = []
    for (host, path, q), files in sorted(by_url.items()):
        if len(files) < 2:
            continue
        # a URL that only ever appears in shared .inc helpers is one definition
        if all(f.endswith(".inc") for f in files):
            continue
        out.append("%s%s%s: fetched by %d files (%s)"
                   % (host, path, ("?" + q) if q else "", len(files),
                      ", ".join(sorted(files)[:3])))
    return out


CHECKS = [
    ("dup-id", check_dup_id),
    ("dup-endpoint", check_dup_endpoint),
    ("unresolved-id", check_unresolved_id),
    ("registry-orphan", check_registry_orphan),
    ("quarantine-empty", check_quarantine_empty),
    ("snprintf-guard", check_snprintf_guard),
    ("rowid-unchecked", check_rowid_unchecked),
    ("geo-precision", check_geo_precision),
]
CHECK_NAMES = [n for n, _ in CHECKS]


# --------------------------------------------------------------------------
# Counting / driver
# --------------------------------------------------------------------------

def count_sources():
    """-> (total, direct, macro_expanded, unresolved)."""
    total = unresolved = 0
    direct = 0
    for path, (found, unres) in all_registrations().items():
        raw = strip_comments(read(path))
        # A REGISTER_SOURCE written literally in the file (not produced by a
        # macro body) is a "direct" registration; the rest came from expansion.
        literal = len(_REG_RE.findall(
            _DEFINE_RE.sub(lambda m: "", join_continuations(raw))))
        total += len(found)
        direct += min(literal, len(found))
        unresolved += len(unres)
    return total, direct, total - direct, unresolved


def load_baseline():
    if not os.path.exists(BASELINE_PATH):
        return {}
    try:
        with open(BASELINE_PATH, "r", encoding="utf-8") as fh:
            return json.load(fh).get("checks", {})
    except (OSError, ValueError):
        return {}


def write_baseline(results):
    total, direct, macro, unres = count_sources()
    doc = {
        "_comment": [
            "Known-bad counts for native/tools/lint_sources.py. CI fails only",
            "when a count goes UP, so the numbers can only ratchet down.",
            "Regenerate with: python3 native/tools/lint_sources.py --write-baseline",
            "Every count here is a defect that still needs fixing, not an",
            "approved exception.",
        ],
        "registered_sources": {
            "total": total, "direct": direct, "macro_expanded": macro,
            "unresolved": unres,
        },
        "checks": {name: len(f) for name, f in results},
    }
    with open(BASELINE_PATH, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, indent=2, sort_keys=True)
        fh.write("\n")


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--count", action="store_true",
                    help="print the registered source count and exit")
    ap.add_argument("--list-ids", action="store_true",
                    help="print every registered source id and exit")
    ap.add_argument("--by-collector", action="store_true",
                    help="print registered source counts per .collector group")
    ap.add_argument("--check", action="append", metavar="NAME",
                    choices=CHECK_NAMES, help="run only this check (repeatable)")
    ap.add_argument("--verbose", action="store_true",
                    help="print every finding, not just the first 20 per check")
    ap.add_argument("--write-baseline", action="store_true",
                    help="record the current counts as the new baseline")
    args = ap.parse_args(argv)

    if not os.path.isdir(SRC_DIR):
        sys.stderr.write("lint_sources: no %s\n" % SRC_DIR)
        return 2

    if args.count:
        total, direct, macro, unres = count_sources()
        print("registered source_defs: %d  (%d direct + %d macro-expanded)"
              % (total, direct, macro))
        if unres:
            print("unresolved registrations (count is a lower bound): %d" % unres)
        return 0

    if args.list_ids:
        for path, (found, _) in sorted(all_registrations().items()):
            for sid, _sym, _coll in found:
                print(sid)
        return 0

    if args.by_collector:
        tally = {}
        for _path, (found, _) in all_registrations().items():
            for _sid, _sym, coll in found:
                key = coll or "(none)"
                tally[key] = tally.get(key, 0) + 1
        for coll, n in sorted(tally.items(), key=lambda kv: (-kv[1], kv[0])):
            print("%6d  %s" % (n, coll))
        print("%6d  TOTAL across %d collectors" % (sum(tally.values()), len(tally)))
        return 0

    selected = [(n, f) for n, f in CHECKS if not args.check or n in args.check]
    results = [(n, f()) for n, f in selected]

    if args.write_baseline:
        if args.check:
            sys.stderr.write("lint_sources: --write-baseline needs the full run\n")
            return 2
        write_baseline(results)
        print("wrote %s" % os.path.relpath(BASELINE_PATH, REPO))
        return 0

    baseline = load_baseline()
    failed = False
    for name, findings in results:
        base = baseline.get(name)
        n = len(findings)
        if base is None:
            status = "NEW"
            bad = n > 0
        elif n > base:
            status = "REGRESSED (baseline %d)" % base
            bad = True
        elif n < base:
            status = "improved (baseline %d — run --write-baseline)" % base
            bad = False
        else:
            status = "at baseline"
            bad = False
        failed = failed or bad
        print("%-18s %5d  %s" % (name, n, status))
        show = findings if args.verbose else findings[:20]
        for f in show:
            print("    %s" % f)
        if len(findings) > len(show):
            print("    … %d more (--verbose)" % (len(findings) - len(show)))

    total, direct, macro, unres = count_sources()
    print("\nregistered source_defs: %d  (%d direct + %d macro-expanded)"
          % (total, direct, macro))
    if failed:
        print("lint-sources: FAILED — a check went above its baseline")
        return 1
    print("lint-sources: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
