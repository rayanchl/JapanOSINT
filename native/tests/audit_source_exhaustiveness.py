#!/usr/bin/env python3
"""Audit collectors against the exhaustive-use rule (docs/SOURCE_EXHAUSTIVENESS.md).

    cd native && make audit-sources
    python3 tests/audit_source_exhaustiveness.py \
        --strict 'collectors/pivot/table/hp*_*.c' \
        --strict 'collectors/feed/generated/hp1[0-9]_*.c'
    python3 tests/audit_source_exhaustiveness.py --file collectors/sources/foo.c -v

A source that is called must be used exhaustively: every record, every field,
every page. This script greps for the patterns that in practice mean fetched
data was thrown away, and reports them per file.

Every finding is printed. --strict (repeatable) turns findings in the matching
paths into a non-zero exit: `make audit-sources` gates on the hp tables and the
generated deep-record tables, and CI fails on any finding there. The rest of
the tree is reported, not gated — it is held at zero by review, so a new
finding anywhere is a regression to read, not backlog.

Before scanning anything the script runs its own fixtures (SELFTEST below)
through every check and exits 2 if a check stopped seeing the discard it was
written for — a weakened regex reads as "0 findings", which is exactly what a
clean tree reads as.

Heuristics flag, they do not prove: a cap may be the upstream's own page size,
and a [0] may be reading a scalar envelope. Every finding needs a human read,
which is why the output cites the line.
"""
import argparse
import glob
import re
import sys

# Counter names that bound RECORDS in a loop condition. `chars < 280` bounding
# a UTF-8 buffer is a byte guard, not a record cap, so i/j/k are deliberately
# absent here: an index variable is only flagged when it carries a SECOND,
# constant bound next to its real one (`i < n && i < 50`), or when a constant
# bound on it reads a cJSON array (_const_index_loop).
_COUNTER = (r'(?:\b(?:count|counted|considered|emitted|n|nf|nrec|nrows|'
            r'nitems|nseen|rows|items|recs|records|found|hits)\b'
            r'|\w*GetArraySize\s*\([^)]*\))')
# The real bound of a sized loop: a name, a member, or a call
# (`n`, `ctx->n`, `cJSON_GetArraySize(arr)`).
_BOUND = r'[A-Za-z_][\w.]*(?:->\w+)*(?:\([^()]*\))?'

# (id, human description, compiled pattern, hint)
CHECKS = [
    ("record-cap",
     "hardcoded record cap — the upstream, not us, decides how many records exist",
     # Only names that bound RECORDS. Buffer sizes (MAX_LINE, URL_MAX, MAX_HOSTS)
     # are allocation limits, not editorial ones, and are not discards.
     re.compile(r'#define\s+\w*(?:ITEM|REC|RESULT|ROW|HIT|ENTR|FEATURE|EMIT|'
                r'PER_REG|PER_SOURCE|TOTAL|PAGE|CAND|MATCH)\w*(?:MAX|CAP|LIMIT)\w*\s+\d+|'
                r'#define\s+\w*(?:MAX|CAP|LIMIT)\w*(?:ITEM|REC|RESULT|ROW|HIT|ENTR|'
                r'FEATURE|EMIT|PER_REG|TOTAL|PAGE|CAND|MATCH)\w*\s+\d+|'
                r'\b(?:int|const int)\s+\w*(?:max|cap|limit)\w*\s*=\s*\d+\s*;',
                re.I),
     "drop the cap, or make it the upstream's page size and paginate"),

    # `if (n++ >= 25) break` is the commonest spelling of this cap in the tree
    # and the pattern used to require the comparison to follow the counter
    # directly, so a post-increment walked straight past it: ROR, OpenCitations,
    # bgpview and PeeringDB each stopped at 25-50 records with this check
    # reading clean. `==` is the same cap written as an equality; it needs a
    # two-digit bound so `if (n == 0) break` (an exhausted upstream) is not one.
    ("loop-break",
     "break out of a record loop on a counter — records after it are discarded",
     re.compile(r'if\s*\([^)]*?\b(?:n|i|count|emitted|nrec|rows?)\b(?:\+\+|--)?\s*'
                r'(?:(?:>=|>)\s*(?:\d+|[A-Z_]{3,})|==\s*(?:\d{2,}|[A-Z_]{3,}))'
                r'\s*\)\s*break'),
     "emit every record; if a bound is unavoidable, stamp it on the output"),

    ("first-only",
     "first array element only — the rest of the array is ignored",
     re.compile(r'cJSON_GetArrayItem\s*\([^,]+,\s*0\s*\)'),
     "iterate with cJSON_ArrayForEach"),

    ("single-page",
     "paged endpoint fetched once — every later page is discarded",
     # Case-insensitive: `Pagina=1` (SNIFA) and `pageNo=1` / `pageNumber=0`
     # are the same first page. A 0-based page (`page=0`, SAM.gov), an OData
     # `$skip=0`, a 1-based `start=1` / `first=1` (SEC's `first=1&last=100`)
     # and an Elasticsearch `from=0` are all "the first slice of a paged
     # result" and used to read clean.
     re.compile(r'"[^"]*[?&](?:page|pagina|p|pageno|page_no|pagenumber|'
                r'pageindex|currentpage|pagenum)=[01](?!\d)(?:&[^"]*)?"|'
                r'"[^"]*[?&]\$?(?:offset|skip|from|resultoffset)=0(?!\d)'
                r'(?:&[^"]*)?"|'
                r'"[^"]*[?&]\$?(?:start|start_index|startindex|first)=[01](?!\d)'
                r'(?:&[^"]*)?"', re.I),
     "walk next/offset pages until the upstream stops producing records"),

    ("limit-one",
     "request asks the upstream for a single record",
     re.compile(r'[?&](?:limit|per_page|rows|size|maxResults|items_per_page|'
                r'page_size|resultPerPage)=1\b'),
     "request the full page size and paginate"),

    # Found by hand in sanc_ofac_consolidated.c, which bounded a SANCTIONS
    # entry's alias list with `while (cJSON_GetArraySize(akas) < 24 && …)`. On a
    # sanctions list an alias is the thing screening matches on, so a dropped
    # one is a silent false negative on a designated person — and neither
    # `record-cap` (which wants a #define) nor `loop-break` (which wants a
    # `break`) could see it, because the bound sat in the loop CONDITION.
    #
    # Only counter-ish names are flagged: `chars < 280` bounding a UTF-8 buffer
    # is a byte guard, not a record cap.
    ("loop-cap",
     "record loop bounded in its own condition — records past it never happen",
     # The \b before the counter list used to sit outside the alternation, which
     # silently excluded the very line this check was written for:
     # `cJSON_GetArraySize` has no word boundary before "GetArraySize" (the
     # underscore is a word character), so the OFAC alias cap never matched and
     # had to be found by hand. The prefix is now explicit.
     #
     # It also used to demand a trailing `&&` and to stop at the first `;`, so
     # it saw only `while (count < 24 && …)`. `while (emitted < 60)` (GDACS),
     # `while (line && *line && emitted < 200)` (FIRMS, a global daily fire
     # CSV) and every `for (…; i < n && i < 50; …)` were invisible. Three
     # shapes now:
     #   while (… counter < N …)            anywhere in the condition
     #   for (init; … counter < N …; step)  the condition sits after the 1st ';'
     #   for (…; i < n && i < 50; …)         a loop over a SIZED collection with
     #                                       a second, constant bound on the
     #                                       same index — whatever the name
     # A constant-bound `for (i = 0; i < 24; i++)` is flagged separately, and
     # only when its body reads a cJSON array at that index or emits
     # (see _const_index_loop): the same shape walks hex digits and months.
     re.compile(r'\bwhile\s*\([^;{]*?' + _COUNTER + r'\s*<=?\s*\d{2,}\b'
                r'|\bfor\s*\([^;]*;[^;{]*?' + _COUNTER + r'\s*<=?\s*\d{2,}\b'
                r'|\b(?:for\s*\([^;]*;|while\s*\()[^;{]*?\b(?P<v>\w+)\s*<=?\s*'
                + _BOUND + r'\s*&&\s*(?P=v)\s*<=?\s*\d{2,}\b'
                r'|\b(?:for\s*\([^;]*;|while\s*\()[^;{]*?\b(?P<w>\w+)\s*<=?\s*'
                r'\d{2,}\s*&&\s*(?P=w)\s*<=?\s*' + _BOUND),
     "drop the bound, or bound it and emit a collector-truncation-notice"),

    ("dedupe-ring",
     "fixed-size seen[] ring — entries past it are mis-deduped or dropped",
     re.compile(r'\*\s*seen\s*\[\s*\d+\s*\]|char\s+\*\s*seen\s*\[\s*\d+\s*\]'),
     "grow the table instead of wrapping a fixed ring"),
]

# A [0] that reads one component of a fixed-shape tuple is not a discard:
# GeoJSON coordinates are [lon,lat], a Polygon's coordinates[0] is its outer
# ring, a bbox is [minx,miny,maxx,maxy]. Recognise those by the identifier
# being indexed and by a sibling [1] read nearby.
GEOM_VAR = re.compile(r'cJSON_GetArrayItem\s*\(\s*(?:const\s+)?\**\s*'
                      r'(coords?|co|c|pt|point|p|gc|ring|rings|poly|polys|polygon|'
                      r'geom|geometry|bbox|box|xy|latlng|lonlat|pos|position|'
                      r'coordinates)\s*,\s*0\s*\)', re.I)
GEOM_CTX = re.compile(r'\b(lat|lon|lng|latitude|longitude|coord|geometry|ring|'
                      r'polygon|bbox|point|linestring|multipolygon)\b', re.I)


# Lines that are documentation/rationale rather than behaviour.
SKIP_LINE = re.compile(r'^\s*(\*|//|/\*)')

# An explicit, greppable waiver for a bound that is genuinely NOT a discard —
# a memory guard whose overrun is reported in-band, a scalar envelope read, a
# fixed-size protocol field. Put the reason on the same line:
#
#     #define HP_MAX_PROPS 2048   /* exhaustive-ok: memory guard, stamped */
#
# `grep -rn exhaustive-ok` then lists every deliberate exception in the tree.
WAIVER = re.compile(r'exhaustive-ok:')


# Macros whose body walks pages, so a page-1 URL passed to one is not a
# single-page read. Kept as a name list rather than inferred, because being
# wrong in this direction hides a real discard — add a macro here only after
# reading its body in collectors/sources/_verified_macros.inc.
# VJSON_KEYED added 2026-09-11: its body is jsonlist_emit_paged_keyed
# (_verified_macros.inc:175), the same walk VJSON uses with one named id field
# instead of the precedence list — so a page-1 URL inside it is walked.
# VJSON_IDKEYS / VGEO_IDKEYS / VJSON_PREP added 2026-09-15
# (collectors/sources/_vjson_idkeys.inc): their bodies call jsonlist_emit_paged,
# geojson_emit_paged and pw_walk respectively — the same walks as VJSON / VGEO,
# behind a sink that only re-keys uids (and, for PREP, a page-shaping hook).
# SOC_SOURCE added 2026-10-02 (collectors/sources/reg2_socrata_registries.c):
# its run is soc_run, which hands the URL to pw_walk — the `$offset=0` in each
# row seeds that offset walk. EU_SOURCE added the same day
# (collectors/sources/reg2_eu_procurement.c): its run is eu_run, which hands
# the URL to pw_walk.
PAGED_MACROS = ('VJSON', 'VJSONBIG', 'VGEO', 'VJSON_KEYED',
                'VJSON_IDKEYS', 'VGEO_IDKEYS', 'VJSON_PREP', 'SOC_SOURCE',
                'EU_SOURCE')

# Functions that walk pages of the URL they are handed. A bespoke collector
# usually names its URL once (`#define CHI_LIC_URL "…$offset=0…"`) and hands it
# to one of these far below; the ±16-line window cannot see that, and the
# `$offset=0` seed is exactly what pw_walk advances (lib/pagewalk.c only ever
# advances a parameter the URL already carries). od_walk_rows is a thin
# wrapper over pw_walk (collectors/sources/od_shared.inc).
_WALKER_CALL = re.compile(r'\b(?:pw_walk|od_walk_rows|jsonlist_emit_paged\w*|'
                          r'geojson_emit_paged\w*)\s*\(')


def _walker_call_args(lines):
    """The argument text of every page-walker call in the file."""
    text = _strip_comments('\n'.join(lines))
    out = []
    for m in _WALKER_CALL.finditer(text):
        depth, i = 1, m.end()
        while i < len(text) and depth:
            depth += {'(': 1, ')': -1}.get(text[i], 0)
            i += 1
        out.append(text[m.end():i - 1])
    return out


def _url_binding(lines, n):
    """The name a URL literal on line `n` is bound to: a `#define NAME`, a
    `NAME =` initialiser, a `.field =` designator, or the buffer of the
    `snprintf(NAME, …)` that formats it — walking back over the lines of the
    same statement only."""
    for i in range(n, max(0, n - 6), -1):
        line = lines[i - 1]
        if i != n and line.rstrip().endswith((';', '}', '},')):
            break
        m = re.match(r'\s*#\s*define\s+(\w+)', line)
        if m:
            return m.group(1)
        m = re.search(r'\b(?:snprintf|sprintf)\s*\(\s*(\w+)\s*,', line)
        if m:
            return m.group(1)
        m = re.search(r'\b(\w+)\s*(?:\[[^\]]*\])?\s*=\s*(?:"|\\?\s*$)', line)
        if m:
            return m.group(1)
    return None


def _handed_to_walker(lines, n):
    name = _url_binding(lines, n)
    if not name:
        return False
    pat = re.compile(r'\b' + re.escape(name) + r'\b')
    return any(pat.search(a) for a in _walker_call_args(lines))
MACRO_OPEN = re.compile(r'^\s*([A-Z][A-Z0-9_]*)\s*\(')


def _paged_macro_at(lines, n):
    """Is line `n` inside a call to a macro that pages?

    Walks back to the nearest macro invocation at the start of a line; a vsrc
    row is one such call spanning a handful of lines. Stops at a blank line so
    the previous row's macro is never credited to this one.
    """
    for i in range(n - 1, max(0, n - 25), -1):
        line = lines[i - 1] if i - 1 < len(lines) else ''
        if not line.strip():
            break
        m = MACRO_OPEN.match(line)
        if m:
            return m.group(1) in PAGED_MACROS
    return False



_ROW_START = re.compile(r'\s*\{\s*\.id\s*=')
_TABLE_END = re.compile(r'\s*\}\s*;')


def _hp_row_block(lines, n):
    """The hp_source row (`{ .id = …` up to the next row or the table's `};`)
    that line `n` sits in, as (first, last) 1-based line numbers, or None.

    The single-page excuse used to read ±16 lines around the URL, so a row
    with NO paging was excused by its neighbour's `.page_param` — SAM.gov
    (`page=0`), TW GCIS (`$skip=0&$top=40`) and SEC ALJ (`first=1&last=100`)
    each sat next to a paged row. A row's paging is the row's own business."""
    start = None
    for i in range(n, 0, -1):
        line = lines[i - 1]
        if _ROW_START.match(line):
            start = i
            break
        if i != n and _TABLE_END.match(line):
            return None
    if start is None:
        return None
    end = len(lines)
    for i in range(start + 1, len(lines) + 1):
        line = lines[i - 1]
        if _ROW_START.match(line) or _TABLE_END.match(line):
            end = i - 1
            break
    return start, end


def _strip_comments(text):
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    return re.sub(r'//[^\n]*', ' ', text)


def _hp_row_pages(lines, block):
    """Does the hp_source row in `block` declare a walk the engine honours?

    lib/hpengine.c (hp_run) pages a row when it declares next_path or
    page_param, or carries a `{page}` path token; `.page_walk = 1` is honoured
    ONLY for HP_JSON rows (`s->page_walk && s->mode == HP_JSON`), so a page-1
    URL on an HP_CSV/HP_XML/HP_HTML row with page_walk set is still one
    request. Comments are stripped first: a row whose rationale merely
    mentions page_param is not paged."""
    first, last = block
    body = _strip_comments('\n'.join(lines[first - 1:last]))
    if re.search(r'\.(?:page_param|next_path)\s*=\s*"[^"]+"', body):
        return True
    if '{page}' in body:
        return True
    if re.search(r'\.page_walk\s*=\s*1\b', body):
        mode = re.search(r'\.mode\s*=\s*(\w+)', body)
        return mode is None or mode.group(1) == 'HP_JSON'
    return False


# `for (int i = 0; i < 24; i++)` — a constant bound on an index. That shape
# walks hex digits, months and tile grids as often as it walks records, so it
# is only a finding when the loop body (this line and the next few) reads a
# cJSON array AT that index, or emits.
_CONST_INDEX_LOOP = re.compile(
    r'\bfor\s*\(\s*(?:(?:unsigned\s+)?(?:int|size_t|long)\s+)?(?P<c>\w+)\s*=\s*0'
    r'\s*;\s*(?P=c)\s*<=?\s*\d{2,}\s*;')


def _const_index_loop(lines, n):
    m = _CONST_INDEX_LOOP.search(lines[n - 1])
    if not m:
        return False
    v = re.escape(m.group('c'))
    body = '\n'.join(lines[n - 1:n + 6])
    return bool(re.search(r'GetArrayItem\s*\([^,]+,\s*' + v + r'\s*\)'
                          r'|\bemit\w*\s*\(', body))


def audit(path, verbose=False):
    try:
        lines = open(path, encoding='utf-8', errors='replace').read().splitlines()
    except OSError as e:
        print(f"cannot read {path}: {e}", file=sys.stderr)
        return []
    return audit_lines(lines)


def audit_lines(lines):
    findings = []
    for n, line in enumerate(lines, 1):
        if SKIP_LINE.match(line) or WAIVER.search(line):
            continue
        for cid, desc, pat, hint in CHECKS:
            if not pat.search(line):
                if cid == 'loop-cap' and _const_index_loop(lines, n):
                    findings.append((cid, n, line.strip()[:120], desc, hint))
                continue
            if cid == 'first-only':
                # tuple/geometry component read, or a paired [0]/[1] on one line
                if GEOM_VAR.search(line) and (GEOM_CTX.search(line) or
                                              ', 1)' in line or ',1)' in line):
                    continue
                ctx2 = '\n'.join(lines[max(0, n - 4):n + 4])
                if GEOM_VAR.search(line) and GEOM_CTX.search(ctx2):
                    continue
            if cid == 'single-page':
                # Inside an hp_source row, only that row's own declarations
                # count — see _hp_row_block for what the neighbour excuse hid.
                block = _hp_row_block(lines, n)
                if block is not None:
                    if _hp_row_pages(lines, block):
                        continue
                    findings.append((cid, n, line.strip()[:120], desc, hint))
                    continue
                # A bespoke collector: a page-1 URL is only a discard if
                # nothing nearby continues the walk; they usually loop within
                # ~15 lines.
                ctx = '\n'.join(lines[max(0, n - 16):n + 16])
                if re.search(r'page_param|next_path|page\+\+|\+\+page|'
                             r'for\s*\(\s*int\s+page|while\s*\([^)]*page', ctx):
                    continue
                # The walk may not be anywhere near the URL. A vsrc row is one
                # macro call, and the paging lives inside the macro:
                # VJSON -> jsonlist_emit_paged, VGEO -> geojson_emit_paged,
                # VJSONBIG -> jsonstream_emit. Reading only the surrounding
                # lines, this check reported every one of them as a discard —
                # including `api.dane.gov.pl/1.4/datasets?page=1&per_page=100`,
                # which is the exact URL jsonlist.h cites as the case the paged
                # walk was written to fix. 44 findings that were all already
                # fixed is not a backlog, it is noise that hides the real ones.
                if _paged_macro_at(lines, n) or _handed_to_walker(lines, n):
                    continue
            findings.append((cid, n, line.strip()[:120], desc, hint))
    return findings


# Each check must still see the discard it was written for. Every entry is
# (C source, {(check, line)} that MUST be reported, {(check, line)} that must
# NOT be). Each one is a shape that has actually shipped silently: the fixtures
# are the regressions, kept as data so a weakened pattern fails loudly.
SELFTEST = [
    # loop-break: the post-increment spelling (ROR / OpenCitations / bgpview /
    # PeeringDB, 25-50 records each) and the `==` spelling; `n == 0` is an
    # exhausted upstream, not a cap.
    ("cJSON_ArrayForEach(r, items) {\n"
     "  if (n++ >= 25) break;\n"
     "  if (count == 50) break;\n"
     "  if (n == 0) break;\n"
     "}\n",
     {("loop-break", 2), ("loop-break", 3)}, {("loop-break", 4)}),
    # loop-cap: no trailing &&, a for condition, a sized loop with a second
    # constant bound, a constant index loop that reads/emits — and the same
    # constant shape over hex digits, which is not a record loop.
    ("while (p && count < 100) { emit(); }\n"
     "for (int i = 0; i < 24; i++) { emit(arr[i]); }\n"
     "for (int i = 0; i < n && i < 50; i++) { emit(arr[i]); }\n"
     "while (emitted < 500) { emit(); }\n"
     "while (cJSON_GetArraySize(akas) < 24 && x) { add(); }\n"
     "for (int i = 0; i < 10; i++) sprintf(h + i*2, \"%02x\", d[i]);\n"
     "while (k < 11 && id[k]) k++;\n",
     {("loop-cap", 1), ("loop-cap", 2), ("loop-cap", 3), ("loop-cap", 4),
      ("loop-cap", 5)},
     {("loop-cap", 6), ("loop-cap", 7)}),
    # single-page: an unpaged row is not excused by its paged NEIGHBOUR; a
    # page_walk row is walked only in HP_JSON; Pagina=1 / $skip=0 / page=0 /
    # first=1 are all a first page.
    ("static const hp_source T[] = {\n"
     "  { .id = \"A\", .url = \"https://x.example/a?q={q}&page=1\",\n"
     "    .description = \"no paging\" },\n"
     "  { .id = \"B\", .url = \"https://x.example/b?q={q}\",\n"
     "    .page_param = \"page\", .page_max = 40 },\n"
     "  { .id = \"C\", .mode = HP_CSV, .page_walk = 1,\n"
     "    .url = \"https://x.example/c.csv?page=1\" },\n"
     "  { .id = \"D\", .page_walk = 1,\n"
     "    .url = \"https://x.example/d?page=1\" },\n"
     "  { .id = \"E\", .url = \"https://x.example/e?Pagina=1\" },\n"
     "  { .id = \"F\", .url = \"https://x.example/f\"\n"
     "    \"?$filter=x&$skip=0&$top=40\" },\n"
     "  { .id = \"G\", .url = \"https://x.example/g?first=1&last=100\" },\n"
     "  { .id = \"H\", .url = \"https://x.example/h?q={q}&page=0\",\n"
     "    /* page_param would be nice */ .description = \"x\" },\n"
     "};\n",
     {("single-page", 2), ("single-page", 7), ("single-page", 10),
      ("single-page", 12), ("single-page", 13), ("single-page", 14)},
     {("single-page", 4), ("single-page", 9)}),
    # single-page in a bespoke collector: a URL handed to pw_walk is walked;
    # the same shape handed to a plain fetch is not.
    ("#define WALKED \"https://x.example/r.json?$limit=100&$offset=0\"\n"
     "#define FETCHED \"https://x.example/s.json?size=50&page=0\"\n"
     "static int run(const source_ctx *c, intel_sink *s) {\n"
     "  return pw_walk(c, s, \"id\", WALKED, pw_fetch_json, emit_page, NULL)\n"
     "       + one_fetch(c, s, FETCHED);\n"
     "}\n",
     {("single-page", 2)}, {("single-page", 1)}),
]


def selftest():
    """-> list of failure strings (empty when every check still works)."""
    bad = []
    for i, (src, must, mustnot) in enumerate(SELFTEST, 1):
        got = {(cid, n) for cid, n, _l, _d, _h in audit_lines(src.splitlines())}
        for want in sorted(must - got):
            bad.append("fixture %d: %s on line %d was NOT reported" % (i, *want))
        for unwanted in sorted(mustnot & got):
            bad.append("fixture %d: %s on line %d was reported but is not a "
                       "discard" % (i, *unwanted))
    return bad


def main():
    broken = selftest()
    if broken:
        print("audit self-test FAILED — a check no longer sees the discard it "
              "was written for, so a clean scan would mean nothing:")
        for b in broken:
            print("  " + b)
        return 2
    ap = argparse.ArgumentParser()
    ap.add_argument('--file', action='append', default=[],
                    help='audit these files instead of the default glob set')
    # Repeatable. It took a single glob, and the gated set is now two
    # directories (the pivot tables and the generated deep-record tables).
    # Passing --strict twice kept only the LAST one, so the gate printed
    # "0 findings" for a set it had never opened — the exact failure mode a
    # gate exists to prevent.
    ap.add_argument('--strict', action='append', default=[],
                    help='glob whose findings make the exit code non-zero; '
                         'repeatable')
    ap.add_argument('-v', '--verbose', action='store_true',
                    help='print every finding, not just per-file counts')
    args = ap.parse_args()

    # recursive: a collector that moved into a subdirectory must still be
    # scanned. A flat 'collectors/sources/*.c' silently stopped seeing 221
    # moved files and reported 121 findings instead of 146 — an audit that
    # under-reports because of a glob is worse than no audit, because the
    # smaller number reads as progress.
    paths = args.file or sorted(
        glob.glob('collectors/**/*.c', recursive=True) + glob.glob('lib/*.c') +
        glob.glob('core/pipeline.c') + glob.glob('core/osint_dispatch.c'))
    strict_paths = set()
    for g in args.strict:
        strict_paths |= set(glob.glob(g))

    total = 0
    per_check = {}
    dirty_files = 0
    strict_hits = 0

    for p in paths:
        f = audit(p, args.verbose)
        if not f:
            continue
        dirty_files += 1
        total += len(f)
        if p in strict_paths:
            strict_hits += len(f)
        counts = {}
        for cid, n, line, desc, hint in f:
            counts[cid] = counts.get(cid, 0) + 1
            per_check[cid] = per_check.get(cid, 0) + 1
        summary = ' '.join(f'{k}={v}' for k, v in sorted(counts.items()))
        mark = '  <-- STRICT' if p in strict_paths else ''
        print(f'{p}: {len(f):3d}  {summary}{mark}')
        if args.verbose or p in strict_paths:
            for cid, n, line, desc, hint in f:
                print(f'    {p}:{n}  [{cid}] {line}')
                print(f'        {desc}')
                print(f'        fix: {hint}')

    print()
    print(f'files scanned      : {len(paths)}')
    print(f'files with findings: {dirty_files}')
    print(f'findings           : {total}')
    for cid, desc, _, _ in CHECKS:
        if per_check.get(cid):
            print(f'  {cid:<13} {per_check[cid]:5d}  {desc}')
    print()
    print('Rule: docs/SOURCE_EXHAUSTIVENESS.md — a source that is called must be')
    print('used exhaustively. Findings are heuristics; each needs a human read.')

    if strict_paths:
        print(f'\nstrict set ({", ".join(args.strict)}): '
              f'{len(strict_paths)} files, {strict_hits} finding(s)')
        return 1 if strict_hits else 0
    return 0


if __name__ == '__main__':
    sys.exit(main())
