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
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)                 # native/
REPO = os.path.dirname(NATIVE)                 # repo root
SRC_DIR = os.path.join(NATIVE, "collectors", "sources")
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
        inc = os.path.join(base, m.group(1))
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

# The SECOND registration path. lib/hpengine.h's HP_REGISTER_TABLE(TBL) takes a
# `static const hp_source TBL[]` row table and turns every row into a
# source_def at constructor time (lib/hpengine.c hp_register). There is no
# `source_def X = {` and no REGISTER_SOURCE(X) anywhere in those files, so the
# two regexes above see nothing — which is how 601 registered ids across 29
# hp_*.c files stayed invisible to every check in this tool, and why
# `--count` disagreed with the built binary's --list-sources by exactly that
# number.
_HP_TBL_RE = re.compile(r"\bhp_source\s+([A-Za-z_]\w*)\s*\[\s*\]\s*=\s*\{")
_HP_REG_RE = re.compile(r"\bHP_REGISTER_TABLE\s*\(\s*([A-Za-z_]\w*)\s*\)")


def _hp_rows(block):
    """Split an hp_source[] initialiser into its top-level `{ ... }` rows.

    Brace counting, not a regex: rows nest braces and carry string literals
    holding both braces and escaped quotes."""
    rows, depth, start = [], 0, None
    in_str = esc = False
    for i, ch in enumerate(block):
        if in_str:
            if esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == '"':
                in_str = False
            continue
        if ch == '"':
            in_str = True
        elif ch == "{":
            depth += 1
            if depth == 2:
                start = i
        elif ch == "}":
            if depth == 2 and start is not None:
                rows.append(block[start:i + 1])
                start = None
            depth -= 1
            if depth == 0:
                break
    return rows


def _hp_registrations(text):
    """-> [(id, table_symbol, collector)] for every HP_REGISTER_TABLE row.

    Mirrors hp_register() exactly, including its two skip rules: a row with no
    .id or no .url is never registered, so it must not be counted here."""
    tables = {m.group(1): _brace_block(text, m.end() - 1)
              for m in _HP_TBL_RE.finditer(text)}
    found = []
    for m in _HP_REG_RE.finditer(text):
        block = tables.get(m.group(1))
        if not block:
            continue
        for row in _hp_rows(block):
            idm = re.search(r"\.id\s*=", row)
            if not idm or not re.search(r"\.url\s*=", row):
                continue                      # hp_register skips these rows
            sid = _read_string_concat(row, idm.end())
            if not sid:
                continue
            cm = re.search(r"\.collector\s*=", row)
            coll = _read_string_concat(row, cm.end()) if cm else None
            # hp_register's default when the row does not name one.
            found.append((sid, m.group(1), coll or "osint"))
    return found


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
    found.extend(_hp_registrations(text))
    return found, unresolved


def source_files():
    """Every TU that can register a source.

    Not just collectors/sources/: core/db.c, core/media.c, core/translate.c and
    core/camera_stills.c each REGISTER_SOURCE an internal maintenance/enrich
    pod. Scanning only the collector directory undercounted the registry by 17
    against what the built binary reports from --list-sources."""
    out = []
    for d in (SRC_DIR, os.path.join(NATIVE, "core"),
              os.path.join(NATIVE, "lib"), NATIVE):
        if not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            p = os.path.join(d, f)
            if f.endswith(".c") and os.path.isfile(p):
                out.append(p)
    return out


def collector_files():
    """The files that emit intel rows: collectors/sources/*.c AND *.inc.

    The .inc files are not headers-with-declarations, they are collector bodies
    that happen to be textually included — od_shared.inc alone is 817 lines
    ending in an emit(), sanc_common.inc 931, trn_common.inc 770, and
    _verified_macros.inc is the emit path for 144 files. Scanning only *.c
    meant every line-based check below (geo-precision, quarantine-empty,
    snprintf-guard, rowid-unchecked) was blind to ~3k lines of exactly the code
    they exist to police."""
    return sorted(os.path.join(SRC_DIR, f)
                  for f in os.listdir(SRC_DIR)
                  if f.endswith(".c") or f.endswith(".inc"))


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


# Source ids the ENGINE writes under, which therefore have curated metadata in
# source_registry.gen.c but deliberately no source_def. Keep this list to ids
# that a `grep -n '"<id>"' core/*.c` shows being passed to intel_sink_make().
ENGINE_SOURCE_IDS = {
    "osint-search",     # core/pipeline.c:227,358 — the entity-pivot result bucket
}


def check_registry_orphan():
    """Rows in core/source_registry.gen.c with no implementation.

    That table is the curated metadata for the map layers. A row with no
    registered source_def describes a source that cannot ever run, so it
    inflates /api/sources and /api/layers with entries that can never report
    a status — exactly the failure the file's own header comment records
    having cleaned up once for traffic-cameras/public-webcams.

    EXCEPT for a source_id that is written by the engine rather than collected.
    core/pipeline.c builds its sink with intel_sink_make(db, "osint-search"),
    so those rows really are in intel_items and really do need a name, a
    description and a category to render — but there is nothing to schedule or
    dispatch, so a source_def would be wrong in the other direction: it would
    put a non-collector into /api/sources' run lists and the entity-pivot menu.
    Carrying it as a permanent baseline of 1 was the worse option again: a check
    whose output is a constant is a check nobody reads."""
    live = {sid for _, (found, _) in all_registrations().items()
            for sid, _sym, _coll in found}
    rel = os.path.relpath(GEN_REGISTRY, REPO)
    return ["%s:%d: registry row '%s' has no registered source_def"
            % (rel, line, sid)
            for sid, line in _gen_registry_ids()
            if sid not in live and sid not in ENGINE_SOURCE_IDS]


# `return n > 0 ? 0 : -1` and its casted variants. An honest empty fetch
# (upstream had nothing new) returns -1, which the scheduler records as a
# failure and eventually quarantines the source — so a *working* collector
# gets switched off for succeeding quietly.
#
# WHAT THE IDENTIFIER MEANS IS THE WHOLE CHECK.  The shape is only a defect
# when the thing being tested is a ROW count. The identical shape testing a
# *host-success* counter is the documented FIX for this very bug, and it is
# what most of this tree does:
#
#     fprintf(stderr, "[resas-population] emitted %d (%d/47 prefectures …)", …);
#     /* STATUS code, not a row count: a prefecture that returns no series is
#      * an honest empty. Only a total fetch failure is a real error. */
#     return fetched > 0 ? 0 : -1;
#
# Flagging those inverted the check: it reported the cure as the disease, in
# every one of the eleven files it matched. So the name is now part of the
# pattern — it must plausibly mean "rows I emitted", and must not be one of
# the transport-health words.
_QUARANTINE_RE = re.compile(
    r"return\s*\(?\s*(?:\(\s*(?:int|long|size_t|ssize_t|unsigned)"
    r"(?:\s+\w+)?\s*\)\s*)?([A-Za-z_]\w*)\s*\)?\s*>\s*0\s*\?\s*0\s*:\s*-\s*1")

# Names that mean "how many rows did I emit". Zero of these is the honest
# empty the scheduler must not punish.
_ROW_EXACT = {"n", "nn", "cnt", "num", "tot", "total", "rows", "row", "hit",
              "hits", "added", "emitted", "count", "items", "records", "rec",
              "recs", "out", "written", "kept"}
_ROW_SUBSTR = ("count", "cnt", "rows", "emit", "added", "hits", "total",
               "item", "record", "written")

# Names that mean "how many endpoints answered me". Zero of these IS a real
# error — every host was down — and returning -1 is correct.
_HEALTH_EXACT = {"fetched", "ok", "oks", "tok", "done", "transport_ok",
                 "reachable", "live", "alive", "up", "got", "tried", "hosts",
                 "feeds", "pages", "queries", "urls", "success", "successes",
                 "responses", "resp", "http_ok", "any", "seen_ok"}
_HEALTH_SUBSTR = ("fetch", "reachab", "transport", "_ok", "ok_", "alive",
                  "http", "status", "respond", "connect")

# An accumulator legitimately reaches zero; a 0/1 status flag cannot.
# `int n = (sink->emit(sink, &it) >= 0) ? 1 : 0;` in a collector that always
# emits exactly one row is not a row count at all — n==0 means the SINK
# rejected the write, which is a genuine error and must return -1.
_FLAG_ASSIGN = r"\b%s\b\s*=\s*[^;]*\?\s*1\s*:\s*0\s*;"
_ACCUMULATES = r"(?:\b%s\b\s*(?:\+\+|\+=)|\+\+\s*\b%s\b)"


def _is_row_name(name):
    low = name.lower()
    if low in _HEALTH_EXACT or any(s in low for s in _HEALTH_SUBSTR):
        return False
    return low in _ROW_EXACT or any(s in low for s in _ROW_SUBSTR)


def _function_window(text, pos):
    """Text of the enclosing top-level function (a `}` in column 0 ends one)."""
    s = text.rfind("\n}", 0, pos)
    s = s + 2 if s >= 0 else 0
    e = text.find("\n}", pos)
    e = e + 2 if e >= 0 else len(text)
    return text[s:e]


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


_UNCLASSIFIED = []


def check_quarantine_unclassified():
    """Visible coverage gap for check_quarantine_empty's name vocabulary.

    Reported as its own row so the vocabulary's reach is measured instead of
    assumed. Populated as a side effect of check_quarantine_empty, which the
    CHECKS order guarantees runs first.
    """
    return list(_UNCLASSIFIED)


def check_quarantine_empty():
    del _UNCLASSIFIED[:]
    out = []
    for path in _tree_c_files():
        text = strip_comments(read(path))
        for m in _QUARANTINE_RE.finditer(text):
            var = m.group(1)
            if not _is_row_name(var):
                # Not in the known row-counter vocabulary. That is usually a
                # host-success counter (correct), but it is also how `nrec`,
                # `wrote` and `found` slipped through — so the skip is COUNTED
                # rather than silent. A check reporting 0 must not be read as
                # "the pattern is eradicated" when its own vocabulary is what
                # bounds it; see the quarantine-empty-unclassified row.
                _UNCLASSIFIED.append(
                    "%s:%d: `return %s > 0 ? 0 : -1` — counter name outside the "
                    "known vocabulary, not classified either way"
                    % (os.path.relpath(path, REPO),
                       text[:m.start()].count("\n") + 1, var))
                continue
            fn = _function_window(text, m.start())
            if (re.search(_FLAG_ASSIGN % re.escape(var), fn)
                    and not re.search(_ACCUMULATES % (re.escape(var),
                                                      re.escape(var)), fn)):
                continue                     # 0/1 emit-succeeded flag, not a count
            line = text[:m.start()].count("\n") + 1
            out.append("%s:%d: `return %s > 0 ? 0 : -1` — an honest empty fetch "
                       "is reported as failure and quarantines the source"
                       % (os.path.relpath(path, REPO), line, var))
    return out


# `off += snprintf(buf + off, sizeof buf - off, …)`: snprintf returns the
# length it WOULD have written, so once it truncates, `off` runs past the
# buffer, `buf + off` is out of bounds and `sizeof buf - off` underflows to a
# huge size_t. The fix in this tree is a clamp on the accumulator.
_SNPRINTF_ACC = re.compile(
    r"([A-Za-z_]\w*)\s*\+=\s*(?:\(\s*\w+(?:\s+\w+)?\s*\)\s*)?snprintf\s*\(",
    re.M)


# A guard is any comparison of the accumulator against something that is a
# capacity: sizeof, an ALL_CAPS constant, an identifier that reads like a size
# (cap/len/sz/size/max/lim, or a bare `n`/`avail`/`space`, which is what a
# `(char *out, size_t n)` writer calls its capacity), or a literal.
# `o + 4 < cap`, `j + 4 < n` and `sw + 2 < sizeof summary` are all real guards
# and all appear in this tree.
_GUARD_TMPL = (r"\b%s\b\s*(?:[-+]\s*\w+\s*)?(?:>=|>|<|<=)\s*\(?\s*"
               r"(?:\(\s*\w+(?:\s+\w+)?\s*\)\s*)?"
               r"(?:sizeof\b|[A-Z][A-Z_0-9]{1,}\b"
               r"|\w*(?:cap|len|sz|size|max|lim|room|rem)\w*\b"
               r"|n\b|nn\b|avail\b|space\b|remaining\b|[1-9]\d*)")
_CLAMP_TMPL = r"\b%s\b\s*=\s*\(?\s*(?:\(\s*\w+\s*\)\s*)?(?:sizeof|[A-Z][A-Z_0-9]+)"

# --- proving a sequence of appends cannot overflow -------------------------
#
# The other half of this check's false-positive rate is the append chain that
# needs no runtime guard because it is bounded at compile time:
# core/camera_store.c builds a WHERE clause from three fixed SQL literals whose
# only conversions are `%d` — ~470 bytes worst case into `char w[1200]`. No
# clamp will ever fire there, and demanding one is noise.
#
# So: when every snprintf writing into the buffer has an all-literal format
# whose worst-case expansion is computable, and the sum of those fits the
# declared buffer, the chain is proven safe and is not reported.
#
# `%s` and `%f` are deliberately NOT computable. `%.1f` of a double read from
# an upstream JSON body is up to ~310 characters — a bound that comes from the
# data, not from the code, is not a bound.
_STRING_ARG = re.compile(r'\s*(?:"(?:\\.|[^"\\])*"\s*)+\Z')
_CONV = re.compile(r"%([-+ #0']*)(\d+|\*)?(?:\.(\d+|\*))?(hh|h|ll|l|j|z|t|L)?"
                   r"([diouxXeEfFgGaAcspn%])")


def _literal_len(s):
    """Bytes a C string literal's non-conversion text expands to (escape == 1)."""
    return len(re.sub(r"\\(?:x[0-9A-Fa-f]+|[0-7]{1,3}|.)", "E", s))


def _fmt_bound(fmt):
    """Worst-case bytes `fmt` can produce, or None when it is unbounded."""
    total = 0
    pos = 0
    for m in _CONV.finditer(fmt):
        total += _literal_len(fmt[pos:m.start()])
        pos = m.end()
        width, prec, length, conv = m.group(2), m.group(3), m.group(4), m.group(5)
        if width == "*" or prec == "*":
            return None                       # width supplied at runtime
        if conv == "%":
            b = 1
        elif conv in "diouxX":
            b = 20 if length in ("l", "ll", "j", "z", "t") else 11
        elif conv == "c":
            b = 1
        elif conv == "p":
            b = 20
        elif conv == "n":
            b = 0
        elif conv in "eEgGaA":
            b = int(prec or 6) + 12           # exponent form is bounded
        else:
            return None                       # %s, %f: magnitude/length is data
        total += max(b, int(width or 0))
    total += _literal_len(fmt[pos:])
    return total


def _string_literal_value(arg):
    """Concatenated contents of an all-string-literal argument, else None."""
    if not _STRING_ARG.match(arg):
        return None
    return "".join(re.findall(r'"((?:\\.|[^"\\])*)"', arg))


def _snprintf_calls(region):
    for m in re.finditer(r"\bsnprintf\s*\(", region):
        args, _end = _split_args(region, m.end() - 1)
        if args and len(args) >= 3:
            yield args


def _provably_bounded(region, buf):
    """True when every snprintf into `buf` in `region` is compile-time bounded
    and their total worst case fits `buf`'s declared size."""
    decl = re.search(r"\b%s\s*\[\s*(\d+)\s*\]" % re.escape(buf), region)
    if not decl:
        return False                          # size not visible: prove nothing
    cap = int(decl.group(1))
    total = 0
    for args in _snprintf_calls(region):
        dst = args[0].strip()
        if dst != buf and not re.match(r"^%s\s*\+" % re.escape(buf), dst):
            continue
        fmt = _string_literal_value(args[2])
        if fmt is None:
            return False
        b = _fmt_bound(fmt)
        if b is None:
            return False
        total += b
    return total > 0 and total + 1 <= cap


def _enclosing_conditions(text, pos):
    """Conditions of the if/for/while constructs that ENCLOSE `pos`.

    Walks backwards counting braces. Each time the depth rises (an unmatched
    '{' to our left), the text immediately before that brace is the head of the
    construct we are inside; if it ends in `)` we take the matching
    parenthesised condition. Returns outermost-last. This is what makes the
    difference between a guard that actually protects the call and one that
    merely happens to sit nearby.
    """
    def _cond_ending_at(end):
        """If text[end] is ')' closing an if/for/while head, return (cond, kw_start)."""
        if end <= 0 or text[end] != ')':
            return None, None
        d, j = 0, end
        while j > 0:
            if text[j] == ')':
                d += 1
            elif text[j] == '(':
                d -= 1
                if d == 0:
                    break
            j -= 1
        if j <= 0:
            return None, None
        k = j - 1
        while k > 0 and text[k] in " \t\n\r":
            k -= 1
        kw_end = k + 1
        while k > 0 and (text[k - 1].isalpha() or text[k - 1] == '_'):
            k -= 1
        if text[k:kw_end] not in ("if", "for", "while"):
            return None, None
        return text[j:end + 1], k

    conds = []

    # 1. UNBRACED bodies: `if (has_dc && w < sizeof buf)\n  w += snprintf(...);`
    #    is the dominant idiom in this tree and has no '{' at all, so a
    #    brace-walk alone never sees the guard. Walk back to the start of this
    #    statement, then peel off any chain of unbraced heads above it.
    i, depth = pos, 0
    while i > 0:
        ch = text[i]
        if ch in ')]':
            depth += 1
        elif ch in '([':
            depth -= 1
        elif depth <= 0 and ch in ';{}':
            break
        i -= 1
    j = i
    # Anything between that boundary and the call that is an if/for/while head
    # WITHOUT a following '{' is an unbraced construct enclosing this statement.
    # Scanning forward from the boundary is what makes this correct: walking
    # backwards sails straight past the head and anchors on the previous block's
    # closing brace, which is how the guard on the line directly above the call
    # went unseen.
    for hm in re.finditer(r"\b(if|for|while)\s*\(", text[i + 1:pos]):
        st = i + 1 + hm.end() - 1
        d, e = 0, st
        while e < pos:
            if text[e] == '(':
                d += 1
            elif text[e] == ')':
                d -= 1
                if d == 0:
                    break
            e += 1
        if d == 0 and e < pos:
            tail = text[e + 1:pos]
            if '{' not in tail:
                conds.append(text[st:e + 1])

    # 2. BRACED enclosures, outward.
    depth, i = 0, j
    while i > 0 and len(conds) < 8:
        ch = text[i]
        if ch == '}':
            depth += 1
        elif ch == '{':
            if depth == 0:
                head_end = i - 1
                while head_end > 0 and text[head_end] in " \t\n\r":
                    head_end -= 1
                cond, _ = _cond_ending_at(head_end)
                if cond:
                    conds.append(cond)
            else:
                depth -= 1
        i -= 1
    return conds


def _zero_before_first_append(text, pos, var):
    """True when `var` is provably 0 at `pos`.

    The first append of an accumulator chain is `w += snprintf(buf + w,
    sizeof buf - w, ...)` with `w` freshly initialised to 0, so the size
    argument is the full buffer and no guard is possible or needed. Only the
    APPENDS AFTER it need one.
    """
    head = text[:pos]
    init = list(re.finditer(r"\b(?:int|size_t|long|unsigned)?\s*\b%s\s*=\s*0\s*;"
                            % re.escape(var), head))
    if not init:
        return False
    after = head[init[-1].end():]
    return not re.search(r"\b%s\s*(?:\+=|=|\+\+|--)" % re.escape(var), after)


def check_snprintf_guard():
    out = []
    for path in _tree_c_files():
        text = strip_comments(read(path))
        lines = text.split("\n")
        for m in _SNPRINTF_ACC.finditer(text):
            var = m.group(1)
            ln = text[:m.start()].count("\n")
            # The guard must DOMINATE the call, not merely co-occur near it.
            #
            # This used to scan a +-20 line window, which any incidental
            # comparison of the same variable disarmed:
            #
            #     if (off > 3) { puts("x"); }              /* unrelated */
            #     off += snprintf(buf + off, sizeof buf - off, "%s", junk);
            #
            # went undetected. A real guard is either the condition of the
            # construct ENCLOSING the call (`for (...; w < sizeof buf; ...)`,
            # `if (off < cap)`) or a clamp on the lines immediately after it.
            # Both are checked below; nothing else counts.
            if _zero_before_first_append(text, m.start(), var):
                continue
            guard = None
            for cond in _enclosing_conditions(text, m.start()):
                if re.search(_GUARD_TMPL % re.escape(var), cond):
                    guard = cond
                    break
            # A clamp belongs to this call only if it is right underneath it.
            clamp = re.search(_CLAMP_TMPL % re.escape(var),
                              "\n".join(lines[ln:ln + 4]))
            if guard or clamp:
                continue
            args, _end = _split_args(text, m.end() - 1)   # regex ends at '('
            dst = args[0].strip() if args else ""
            bm = re.match(r"^([A-Za-z_]\w*)\s*(?:\+|$)", dst)
            if bm and _provably_bounded(_function_window(text, m.start()),
                                        bm.group(1)):
                continue
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
#
# TWO WAYS TO EMIT GEOMETRY, and for a long time this check only saw one.
#
#   1. By hand, on the intel_item:  `it.has_geo = 1; it.geometry_geojson = …`.
#   2. Through the library: build `gj_point_feature(lon, lat)`, attach
#      properties, and hand the array to geojson_emit_features() /
#      geojson_emit_doc(); or hand a camera Feature to camera_upsert().
#
# In form (2) `has_geo` is set inside lib/geojson.c (geojson_emit_features →
# `it.has_geo = geo`) and core/camera_store.c, never in the collector file, so
# neither of the two patterns above appears anywhere in the ~225 collectors
# that take that path. They were invisible to this check — which is the whole
# population the check exists for, since a library-emitted Point looks exactly
# as authoritative at the API boundary as a hand-built one. Matching the call
# sites is what makes the reported population the real one.
_EMITS_GEO = re.compile(r"\.has_geo\s*=\s*1|\.geometry_geojson\s*=\s*[^N]|"
                        r"\bhas_geo\s*=\s*1\b|"
                        r"\bgj_point_feature\s*\(|"
                        r"\bgeojson_emit_features\s*\(|"
                        r"\bgeojson_emit_doc\s*\(|"
                        r"\bcamera_upsert\s*\(")


def check_geo_precision():
    """One emit site is exempted by ITS OWN function, not by the whole file.

    A file-wide `if "geo_precision" in text: continue` meant one compliant emit
    hid every other emit in the same file. That matters most exactly where the
    file is shared: _verified_macros.inc is the emit path for 144 collectors,
    and av_common.inc / od_shared.inc / sanc_common.inc are similar.
    """
    out = []
    for path in collector_files():
        text = strip_comments(read(path))
        for m in _EMITS_GEO.finditer(text):
            if "geo_precision" in _function_window(text, m.start()):
                continue
            ln = text[:m.start()].count("\n") + 1
            out.append("%s:%d: emits geometry with no geo_precision property"
                       % (os.path.relpath(path, REPO), ln))
    return out


_IDX_NAME    = re.compile(r"\bidx_[a-z0-9_]+")
_IDX_CREATE  = re.compile(r"CREATE\s+(?:UNIQUE\s+)?INDEX\s+(?:IF\s+NOT\s+EXISTS\s+)?"
                          r"(idx_[a-z0-9_]+)", re.I)


def check_phantom_index():
    """An index a header PROMISES must actually be created somewhere.

    21 headers name 57 different indexes, usually to explain why a query is
    fast ("this probe hits idx_entities_normkey, never a prefix scan"). That is
    durable, useful knowledge — but it is an assertion about state owned by a
    DIFFERENT file, so nothing stops schema.sql changing underneath it. A header
    that claims an index which no longer exists does not fail a build or a test;
    it just quietly misinforms the next person reasoning about a slow endpoint.

    Creation may legitimately live in schema.sql OR in a .c (several indexes are
    built at runtime by migrations that must run after an ensure_column). Both
    count. Matching must allow CREATE UNIQUE INDEX — omitting that is exactly
    how this check first reported three false phantoms.
    """
    created = set()
    for path in [os.path.join(REPO, "native", "core", "schema.sql")] + _tree_c_files():
        try:
            text = read(path)
        except OSError:
            continue
        created.update(m.group(1) for m in _IDX_CREATE.finditer(text))

    out = []
    hdrs = sorted(glob.glob(os.path.join(REPO, "native", "core", "*.h")) +
                  glob.glob(os.path.join(REPO, "native", "lib", "*.h")))
    for path in hdrs:
        text = read(path)
        lines = text.split("\n")
        for ln, line in enumerate(lines, 1):
            for name in set(_IDX_NAME.findall(line)):
                if name not in created:
                    out.append("%s:%d: header names `%s`, which no CREATE INDEX "
                               "anywhere creates" % (os.path.relpath(path, REPO), ln, name))
    return out


CHECKS = [
    ("dup-id", check_dup_id),
    ("unresolved-id", check_unresolved_id),
    ("registry-orphan", check_registry_orphan),
    ("quarantine-empty", check_quarantine_empty),
    ("quarantine-empty-unclassified", check_quarantine_unclassified),
    ("snprintf-guard", check_snprintf_guard),
    ("rowid-unchecked", check_rowid_unchecked),
    ("geo-precision", check_geo_precision),
    ("phantom-index", check_phantom_index),
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


def _baseline_value(findings):
    """A scalar for a clean check, a per-file map for one with a real floor."""
    if not findings:
        return 0
    per = {}
    for f in findings:
        p = f.split(":", 1)[0]
        per[p] = per.get(p, 0) + 1
    return per


def write_baseline(results):
    total, direct, macro, unres = count_sources()
    doc = {
        "_comment": [
            "Known-bad counts for native/tools/lint_sources.py. CI fails only",
            "when a count goes UP, so the numbers can only ratchet down.",
            "Regenerate with: python3 native/tools/lint_sources.py --write-baseline",
            "Every count here is a defect that still needs fixing, not an",
            "approved exception.",
            "",
            "A check with a NONZERO floor is recorded PER FILE, not as one",
            "number. With a single total, a new offending file lands unnoticed",
            "whenever an unrelated file is fixed — the total stays flat and CI",
            "stays green. Per file, a new offender is a regression regardless.",
            "Checks sitting at 0 stay scalar; there is nothing to mask.",
        ],
        "registered_sources": {
            "total": total, "direct": direct, "macro_expanded": macro,
            "unresolved": unres,
        },
        "checks": {name: _baseline_value(f) for name, f in results},
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

        # PER-FILE for any check with a nonzero floor. A single global counter
        # lets a brand-new offending file land unnoticed whenever an unrelated
        # file is fixed — and geo-precision is the only check not at 0, so it is
        # the only one where that masking is possible. Comparing the set of
        # offending FILES makes a new offender a regression even when the total
        # falls. Checks at a 0 floor need none of this.
        if isinstance(base, dict):
            cur = {}
            for f in findings:
                cur[f.split(":", 1)[0]] = cur.get(f.split(":", 1)[0], 0) + 1
            worse = sorted(p for p, c in cur.items() if c > base.get(p, 0))
            if worse:
                print("%-18s %5d  REGRESSED in %d file(s) (per-file baseline)"
                      % (name, n, len(worse)))
                for p in worse[:20]:
                    print("    %s: %d (baseline %d)" % (p, cur[p], base.get(p, 0)))
                if len(worse) > 20:
                    print("    … %d more" % (len(worse) - 20))
                failed = True
            else:
                total_base = sum(base.values())
                print("%-18s %5d  %s" % (name, n,
                      "at per-file baseline" if n == total_base
                      else "improved (per-file baseline %d — run --write-baseline)"
                           % total_base))
            continue

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
