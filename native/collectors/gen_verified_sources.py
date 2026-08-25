#!/usr/bin/env python3
"""Turn verified-live endpoint manifests into collector .c files.

Input: the TSVs produced by the discovery pass (tools/verify_feeds.py decided
which endpoints are live; this only renders the survivors). Every row here has
already been fetched and parsed successfully — that is the entry condition,
and it is why these collectors do not need a "does it work?" caveat.

Three shapes are rendered, one macro each, mirroring the toolkit split the
tree already uses:

  rss / atom        -> lib/rss_atom.h   rss_collect()
  geojson           -> lib/geojson.h    geojson_emit_doc()
  json* / csv-less  -> lib/jsonlist.h   jsonlist_emit()

All three return 0 on a successful fetch that happened to yield nothing, and
-1 only on a genuine fetch/parse failure. That is SOURCE_AUTHORING_CONTRACT R3
and it is the single most-violated rule in the existing fleet (44 collectors
were quarantining themselves for working correctly on a quiet day).

Usage:
  python3 gen_verified_sources.py MANIFEST.tsv [MANIFEST.tsv ...] \
      --outdir collectors/sources --reserved existing_ids.txt
"""
import argparse, os, re, sys, csv, collections

MACRO_INC = '_verified_macros.inc'

# The macro bodies themselves are NOT here. They live in exactly one place,
# collectors/sources/_verified_macros.inc, reached from any output directory
# through the Makefile's `-iquote collectors/sources`.
#
# This module used to carry a second, authoritative-looking copy of them in a
# module-level MACROS string. Nothing read it — the writer below deletes a
# stale copy rather than emitting one — but by the time it was removed it had
# drifted three fixes behind the real file: no jsonlist_emit_paged (so ~6,500
# sources would have gone back to reading page 1 and discarding the rest), no
# geojson_emit_paged, no VJSONBIG, and no CSV comment prefix. A dead copy that
# still looks canonical is a regression waiting for someone to re-enable it,
# which is why it is gone rather than updated. Change the .inc.

def cesc(s):
    return (s.replace('\\', '\\\\').replace('"', '\\"')
             .replace('\n', ' ').replace('\r', ' ').replace('\t', ' ')).strip()

def sym_of(sid, used):
    base = re.sub(r'_+', '_', re.sub(r'[^a-z0-9_]', '_', sid.lower())).strip('_')
    if not base or base[0].isdigit():
        base = 'v_' + base
    cand, i = base, 1
    while cand in used:
        i += 1
        cand = f'{base}_{i}'
    used.add(cand)
    return cand

def tags_json(tags):
    parts = [t.strip() for t in (tags or '').split(',') if t.strip()]
    return '[' + ','.join('\\"%s\\"' % cesc(t) for t in parts) + ']'

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('manifests', nargs='+')
    ap.add_argument('--outdir', required=True)
    ap.add_argument('--reserved', required=True,
                    help='file of source ids already registered')
    ap.add_argument('--prefix', default='vsrc',
                    help='output filename prefix')
    ap.add_argument('--per-file', type=int, default=60)
    ap.add_argument('--unverified', action='store_true',
                    help='the manifest rows were NOT fetched at generation '
                         'time; emit a header that says so instead of the '
                         'verified-live assertion')
    args = ap.parse_args()

    reserved = {l.strip() for l in open(args.reserved, encoding='utf-8') if l.strip()}
    used_ids, used_syms = set(), set()
    by_collector = collections.defaultdict(list)
    stats = collections.Counter()

    for man in args.manifests:
        with open(man, encoding='utf-8', newline='') as fh:
            for row in csv.DictReader(fh, delimiter='\t'):
                sid = (row.get('id') or '').strip()
                url = (row.get('url') or '').strip()
                if not sid or not url:
                    stats['skip_incomplete'] += 1
                    continue
                if sid in reserved or sid in used_ids:
                    stats['skip_dup_id'] += 1
                    continue
                if not url.startswith(('http://', 'https://')):
                    stats['skip_bad_url'] += 1
                    continue
                kind = (row.get('kind') or '').strip().lower()
                # Interval floor. Over-polling is what rate-limited ~24% of the
                # existing fleet into quarantine, so a too-eager manifest value
                # is clamped rather than trusted.
                try:
                    ival = int(float(row.get('interval') or 3600))
                except ValueError:
                    ival = 3600
                ival = max(ival, 900)

                if kind in ('rss', 'atom'):
                    shape, path = 'VRSS', None
                elif kind == 'geojson':
                    shape, path = 'VGEO', None
                elif kind.startswith('json'):
                    shape = 'VJSON'
                    if kind == 'json-array':
                        path = ''
                    elif kind == 'json-object':
                        # One record, not a list — jsonlist_emit's "." mode.
                        path = '.'
                    elif kind.startswith('json:'):
                        path = kind.split(':', 1)[1]
                    else:
                        path = '*'
                elif kind == 'csv':
                    shape, path = 'VCSV', None
                else:
                    stats['skip_unsupported_kind_' + (kind or 'blank')] += 1
                    continue

                used_ids.add(sid)
                stats[shape] += 1
                by_collector[(row.get('collector') or 'osint').strip()].append(
                    dict(sid=sid, url=url, shape=shape, path=path,
                         name=row.get('name') or sid,
                         name_ja=row.get('name_ja') or row.get('name') or sid,
                         collector=(row.get('collector') or 'osint').strip(),
                         category=(row.get('category') or 'osint').strip(),
                         lang=(row.get('lang') or 'en').strip(),
                         tags=row.get('tags') or '',
                         ival=ival,
                         desc=row.get('description') or ''))

    os.makedirs(args.outdir, exist_ok=True)
    # The macros live in ONE place: collectors/sources/_verified_macros.inc,
    # reached from any output directory through the Makefile's
    # `-iquote collectors/sources`.
    #
    # This used to write a second copy of MACROS into --outdir. Because an
    # #include "..." searches the including file's own directory FIRST, that
    # copy shadowed the canonical file for every generated collector — so
    # adding VJSONBIG to the real one had no effect and the build failed with
    # "expected ')' before string constant", which reads like a syntax error in
    # the collector rather than two competing copies of a header. Emit nothing
    # here and let -iquote resolve it.
    stale = os.path.join(args.outdir, MACRO_INC)
    if os.path.exists(stale) and os.path.abspath(args.outdir) != \
       os.path.abspath(os.path.dirname(__file__) + '/sources'):
        os.remove(stale)
    written = []
    for coll, rows in sorted(by_collector.items()):
        rows.sort(key=lambda r: r['sid'])
        chunks = [rows[i:i + args.per_file]
                  for i in range(0, len(rows), args.per_file)]
        safe = re.sub(r'[^a-z0-9]+', '_', coll.lower()).strip('_') or 'misc'
        for ci, chunk in enumerate(chunks, 1):
            fname = f'{args.prefix}_{safe}_{ci}.c'
            body = []
            for r in chunk:
                sym = sym_of(r['sid'], used_syms)
                common = (f'"{cesc(r["sid"])}", "{cesc(r["name"])}", '
                          f'"{cesc(r["name_ja"])}",\n  "{cesc(r["collector"])}", '
                          f'"{cesc(r["category"])}",\n  "{cesc(r["url"])}",')
                if r['shape'] == 'VJSON':
                    common += f'\n  "{cesc(r["path"] or "")}",'
                body.append(
                    f'{r["shape"]}({sym}, {common}\n'
                    f'  "{cesc(r["lang"])}", "{tags_json(r["tags"])}", {r["ival"]},\n'
                    f'  "{cesc(r["desc"])}");')
            if args.unverified:
                # The verified header is an assertion of fact. When the batch
                # was authored without egress there is no proof to point at,
                # so say that plainly rather than inherit a claim that is not
                # true of these rows.
                hdr = (f'/* UNVERIFIED candidate {coll} sources ({len(chunk)}), part {ci}.\n'
                       f' * These endpoints were NOT fetched at generation time — the\n'
                       f' * authoring environment had no outbound egress — so unlike the\n'
                       f' * vsrc_* files there is no recorded 2xx/parse proof behind them.\n'
                       f' * Each one is a documented platform API contract on a real host;\n'
                       f' * see docs/candidate-sources-batch14.tsv for provenance.\n'
                       f' * A dead endpoint here degrades to an explicit fetch error at\n'
                       f' * runtime and never to invented content. Promote to verified with\n'
                       f' * `make verify-candidates` (tools/promote_candidates.py).\n'
                       f' * Generated by collectors/gen_verified_sources.py — regenerate\n'
                       f' * rather than hand-editing. */\n'
                       f'#include "{MACRO_INC}"\n')
            else:
                hdr = (f'/* Verified-live {coll} sources ({len(chunk)}), part {ci}.\n'
                       f' * Every endpoint in this file returned 2xx and parsed to at\n'
                       f' * least one record at generation time; see\n'
                       f' * docs/verified-sources-manifest.tsv for the recorded proof.\n'
                       f' * Generated by collectors/gen_verified_sources.py — regenerate\n'
                       f' * rather than hand-editing. */\n'
                       f'#include "{MACRO_INC}"\n')
            with open(os.path.join(args.outdir, fname), 'w',
                      encoding='utf-8', newline='\n') as out:
                out.write(hdr + '\n' + '\n\n'.join(body) + '\n')
            written.append((fname, len(chunk)))

    total = sum(n for _, n in written)
    for f, n in written:
        print(f'{f}\t{n}')
    print(f'--- {total} sources across {len(written)} files', file=sys.stderr)
    for k, v in sorted(stats.items()):
        print(f'    {k}: {v}', file=sys.stderr)

if __name__ == '__main__':
    main()
