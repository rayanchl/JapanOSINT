#!/usr/bin/env python3
"""Audit collectors against the exhaustive-use rule (docs/SOURCE_EXHAUSTIVENESS.md).

    cd native && make audit-sources
    python3 tests/audit_source_exhaustiveness.py --strict 'collectors/sources/hp_*.c'
    python3 tests/audit_source_exhaustiveness.py --file collectors/sources/foo.c -v

A source that is called must be used exhaustively: every record, every field,
every page. This script greps for the patterns that in practice mean fetched
data was thrown away, and reports them per file.

It is a *report*, not a gate — the pre-existing collectors carry a known
backlog, and the rule applies to code as it is touched. --strict turns findings
into a non-zero exit for a chosen set of paths (used for the hp_* engine rows,
which are expected to stay clean).

Heuristics flag, they do not prove: a cap may be the upstream's own page size,
and a [0] may be reading a scalar envelope. Every finding needs a human read,
which is why the output cites the line.
"""
import argparse
import glob
import re
import sys

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

    ("loop-break",
     "break out of a record loop on a counter — records after it are discarded",
     re.compile(r'if\s*\([^)]*\b(?:n|i|count|emitted|nrec|rows?)\b\s*(?:>=|>)\s*'
                r'(?:\d+|[A-Z_]{3,})\s*\)\s*break'),
     "emit every record; if a bound is unavoidable, stamp it on the output"),

    ("first-only",
     "first array element only — the rest of the array is ignored",
     re.compile(r'cJSON_GetArrayItem\s*\([^,]+,\s*0\s*\)'),
     "iterate with cJSON_ArrayForEach"),

    ("single-page",
     "paged endpoint fetched once — every later page is discarded",
     re.compile(r'"[^"]*[?&](?:page|pagina|p)=1(?:&[^"]*)?"|'
                r'"[^"]*[?&](?:offset|start|start_index|skip)=0(?:&[^"]*)?"'),
     "walk next/offset pages until the upstream stops producing records"),

    ("limit-one",
     "request asks the upstream for a single record",
     re.compile(r'[?&](?:limit|per_page|rows|size|maxResults|items_per_page|'
                r'page_size|resultPerPage)=1\b'),
     "request the full page size and paginate"),

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


def audit(path, verbose=False):
    findings = []
    try:
        lines = open(path, encoding='utf-8', errors='replace').read().splitlines()
    except OSError as e:
        print(f"cannot read {path}: {e}", file=sys.stderr)
        return findings
    for n, line in enumerate(lines, 1):
        if SKIP_LINE.match(line) or WAIVER.search(line):
            continue
        for cid, desc, pat, hint in CHECKS:
            if not pat.search(line):
                continue
            # A page-1 URL is only a discard if nothing nearby continues the
            # walk. hpengine rows declare .page_param / .next_path a couple of
            # lines away, and bespoke collectors usually loop within ~15 lines.
            if cid == 'first-only':
                # tuple/geometry component read, or a paired [0]/[1] on one line
                if GEOM_VAR.search(line) and (GEOM_CTX.search(line) or
                                              ', 1)' in line or ',1)' in line):
                    continue
                ctx2 = '\n'.join(lines[max(0, n - 4):n + 4])
                if GEOM_VAR.search(line) and GEOM_CTX.search(ctx2):
                    continue
            if cid == 'single-page':
                ctx = '\n'.join(lines[max(0, n - 16):n + 16])
                if re.search(r'page_param|next_path|page\+\+|\+\+page|'
                             r'for\s*\(\s*int\s+page|while\s*\([^)]*page', ctx):
                    continue
            findings.append((cid, n, line.strip()[:120], desc, hint))
    return findings


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--file', action='append', default=[],
                    help='audit these files instead of the default glob set')
    ap.add_argument('--strict', default=None,
                    help='glob whose findings make the exit code non-zero')
    ap.add_argument('-v', '--verbose', action='store_true',
                    help='print every finding, not just per-file counts')
    args = ap.parse_args()

    paths = args.file or sorted(
        glob.glob('collectors/sources/*.c') + glob.glob('lib/*.c') +
        glob.glob('core/pipeline.c') + glob.glob('core/osint_dispatch.c'))
    strict_paths = set(glob.glob(args.strict)) if args.strict else set()

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
        print(f'\nstrict set ({args.strict}): {strict_hits} finding(s)')
        return 1 if strict_hits else 0
    return 0


if __name__ == '__main__':
    sys.exit(main())
