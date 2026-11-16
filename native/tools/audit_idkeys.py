#!/usr/bin/env python3
"""Does each row's declared `id_keys` name a field the upstream actually sends?

The defect this finds is rule 4b's quietest: `id_keys` is a MANIFEST
DECLARATION, not the upstream's identity, and when it names a field the records
do not carry, nothing fails. The engine content-hashes every uid instead, so the
row stores its records — `stored == emitted`, every gate green — and a record
the upstream later REVISES arrives as a new row rather than an update of the one
it corrects. WHO's FluNet had been keying on `_RecordID`, which FLUMART does not
publish, since the row shipped; four Municipal Money cubes on
`municipality.demarcation_code`, which no v2 cube publishes.

The engine discloses it per run (a `collector-shape-notice` saying `id_keys "X"
matched 0 of N record(s)`), so the authoritative sweep is to run every row and
read the notices back — which costs a full registry pass. This asks the same
question for one request per row and no database: fetch one page, flatten the
first record the way `hp_json_flat` flattens it, and look up each key token the
way `hp_flat_get` looks it up.

MIRRORING THE ENGINE IS THE WHOLE JOB. The first version of this check reported
35 rows and 30 were its own fault: `hp_flat_get()` falls back from an exact
flat-key miss to any key whose LAST dotted segment matches, case-insensitively
(skipping a leading `@`), so `iata_code` resolves against an ArcGIS record's
`attributes.iata_code` — and a checker without that rule indicts most of the
ArcGIS and GeoJSON fleet. `IMF_PORTWATCH_AIRPORTS` stores uids `RFN`, `YOA`,
`MVA` and files no notice at all. What the fallback cannot save is a key token
that itself contains a dot: it compares the whole token against one segment.

WHAT IT DOES NOT ANSWER: whether the field is UNIQUE. A key that exists and is a
dimension (one row per municipality per year, keyed on the municipality) loses
records just as silently, and only the registry sweep's COLLISION verdict or a
distinctness measurement over a real page can see it. Two Municipal Money rows
passed this check and were still wrong that way. So a clean run here is a
narrower statement than it looks.

Rows are read from the C, never from a manifest, because the C is the maintained
copy (see the batch-tooling section of CLAUDE.md). Scope: hpengine rows with
`mode = HP_JSON` and a declared `id_keys`, skipping any URL carrying an entity
or date token — those need a dispatch to render. `--all` drops the
`page_resume` restriction that the first sweep used.

    python3 tools/audit_idkeys.py --out /tmp/idkeys.tsv
    python3 tools/audit_idkeys.py --match ZA_MUNIMONEY --jobs 2

Verdicts: OK, ID_KEYS_UNMATCHED (the finding: no record on the page carried
the key — the engine's own `id-keys-unmatched` condition), ID_KEYS_PARTIAL
(some records did; the rest fall back — informational), UNMEASURED
(transport), HTTP_ERROR (a 4xx/5xx: its JSON error body is NOT judged as
records), UNPARSED (not JSON through this path), NO_RECORDS. Nothing is
edited, and nothing is retired on one reading.

REBASED ON THE ENGINE'S RULES (2026-11-07), because this tool's first model
disagreed with lib/hpengine.c in four ways, each pinned by --self-test:
  * a `+` composite counts when AT LEAST ONE part is present (hp_pick_s joins
    a missing part as ""); this required ALL of them;
  * the flatten holds only non-empty strings, numbers and bools
    (hp_flatten_c); this kept null and "", so a key that is always null read
    OK while the engine fell back to another key;
  * a declared array_path that does not resolve EMITS NOTHING; this fell back
    to the densest array and judged records the engine never reads — and it
    walked dict keys only (no index, `key=value`, `$last`, literal-dot key);
  * the fetch ignored the HTTP status (a 4xx error body was judged as a
    record) and the row's own `.headers`.
The model is tools/hpjson.py, the request tools/jo_http.py, the row reader
tools/ctable.py — each shared with the other tools that need it.
"""
import argparse
import os
import re
import sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ctable                                                    # noqa: E402
import hpjson                                                    # noqa: E402
import jo_http                                                   # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
# Tokens this one-request check cannot render: an entity (a pivot answers
# about one), a date/window (its own rules), and the rest are rendered for the
# FIRST page as hp_run renders them ({page} {offset} {offset1} {offset_end}
# {size}) or, for {key}, from the row's key_env when it is set — a row whose
# key is not set is skipped and counted, never fetched with a literal `{key}`.
UNRENDERABLE = re.compile(r'\{(q[a-zA-Z]*|Q|date|ago|wstart|wend)\b')


def first_page(text, r, key=None):
    size = r.i('page_size')
    page = r.i('page_start') or (0 if r.i('page_zero_based') else 1)
    for tok, val in (('{page}', page), ('{offset}', 0), ('{offset1}', 1),
                     ('{offset_end}', max(size, 1) - 1), ('{size}', size)):
        text = text.replace(tok, str(val))
    if key is not None:
        text = text.replace('{key}', key)
    return text


SKIPPED = Counter()


def rows_from_tree(match, want_resume, tree=None):
    """(sid, file, id_keys, array_path, url, headers, post_body, content_type,
    title_keys) for every HP_JSON row with an id_keys, read with ctable."""
    out = []
    for r in ctable.iter_rows(tree or os.path.join(NATIVE, 'collectors'), need='.id_keys'):
        sid = r.s('id')
        if not sid or (match and match not in sid):
            continue
        if want_resume and r.i('page_resume') != 1:
            continue
        idk, url = r.s('id_keys'), r.s('url')
        if not idk or not url or r.s('mode') != 'HP_JSON':
            continue
        body = r.s('post_body')
        hdrs = r.get('headers') if isinstance(r.get('headers'), list) else []
        if UNRENDERABLE.search(url) or UNRENDERABLE.search(body):
            SKIPPED['entity/date token'] += 1
            continue
        key = None
        if '{key}' in url + body + ' '.join(hdrs):
            key = os.environ.get(r.s('key_env') or '\0')
            if not key:
                SKIPPED['needs %s' % (r.s('key_env') or 'a key')] += 1
                continue
        out.append((sid, os.path.basename(r.file), idk, r.s('array_path'),
                    first_page(url, r, key), [first_page(h, r, key) for h in hdrs],
                    first_page(body, r, key), r.s('content_type'), r.s('title_keys')))
    return out


def judge(doc, idk, apath, title_keys=''):
    """(verdict, how, detail) for one parsed body, by the engine's rules."""
    recs, how = hpjson.select_records(doc, apath or None)
    if how == 'declared-missing':
        return ('NO_RECORDS', how, 'declared array_path %r does not resolve: '
                'the engine emits nothing from this body' % apath)
    if how == 'root-record' and hpjson.json_error_doc(doc):
        return ('NO_RECORDS', how, 'HTTP 200 error document, no records')
    lists = (idk, title_keys)
    flats = [f for f in (hpjson.json_flat(r, lists) for r in recs) if f]
    if not flats:
        return ('NO_RECORDS', how, 'no record with any content in the body')
    hit, alts, falls = 0, Counter(), Counter()
    for f in flats:
        v, alt = hpjson.pick(f, idk)
        if v is not None:
            hit += 1
            alts[alt] += 1
        else:
            basis, _v, k = hpjson.record_key(f, None, title_keys)
            falls['%s:%s' % (basis, k or '')] += 1
    n = len(flats)
    if hit == n:
        return ('OK', how, 'matched %d of %d (%s)' % (hit, n, ','.join(a for a, _ in alts.most_common(2))))
    fb = '; falls back to %s' % ', '.join('%s x%d' % kv for kv in falls.most_common(2))
    if hit:
        return ('ID_KEYS_PARTIAL', how, 'matched %d of %d%s' % (hit, n, fb))
    sample = ','.join(sorted({k for k, _ in flats[0]})[:10])
    return ('ID_KEYS_UNMATCHED', how, 'matched 0 of %d%s; record carries %s' % (n, fb, sample))


def one(row, args):
    sid, f, idk, apath, url, hdrs, body, ctype, tkeys = row
    jo_http.PACER.gap_ms = args.host_gap_ms
    h = jo_http.parse_header_lines(hdrs)
    hd = {}
    for k, v in h:
        hd.setdefault(k, v)
    if body and not any(k.lower() == 'content-type' for k in hd):
        n, _, v = (ctype or 'Content-Type: application/json').partition(':')
        hd[n.strip()] = v.strip()
    st, _ct, raw, err = jo_http.fetch_any(url, hd, args.timeout, body or None,
                                          args.max_bytes, mode='json')
    if st < 0 or (st == 200 and not raw.strip()):
        return (sid, f, 'UNMEASURED', idk, '', err[:70] or 'empty body')
    if st != 200:
        # hp_run reads a body only on success; a 4xx JSON error envelope is
        # not a page of records and must not be judged as one.
        wk = jo_http.wall_kind(raw.decode('utf-8', 'replace'), st)
        return (sid, f, 'HTTP_ERROR', idk, '', 'HTTP %d%s' % (st, (' ' + wk) if wk else ''))
    if len(raw) > args.max_bytes:
        return (sid, f, 'UNMEASURED', idk, '', 'body over --max-bytes')
    doc = hpjson.parse_body(raw.decode('utf-8', 'replace'))
    if doc is hpjson.MISSING:
        wk = jo_http.wall_kind(raw.decode('utf-8', 'replace'), st)
        return (sid, f, 'UNPARSED', idk, '', wk or 'not JSON')
    v, how, detail = judge(doc, idk, apath, tkeys)
    return (sid, f, v, idk, how, detail)


def self_test():
    """Each case is one of the four disagreements with the engine named in the
    module docstring; the old model gets every one of them wrong."""
    cases = [
        ('a composite with ONE part present matches (hp_pick_s)',
         {'r': [{'year': 2020}]}, 'year+age', 'r', 'OK'),
        ('an always-null key does NOT match (hp_flatten_c drops null)',
         {'r': [{'k': None, 'id': 1}, {'k': None, 'id': 2}]}, 'k', 'r', 'ID_KEYS_UNMATCHED'),
        ('an always-"" key does NOT match',
         {'r': [{'k': '', 'n': 'x'}]}, 'k', 'r', 'ID_KEYS_UNMATCHED'),
        ('a declared path that does not resolve emits nothing (no densest fallback)',
         {'other': [{'k': 1}]}, 'k', 'results', 'NO_RECORDS'),
        ('a path through a list index resolves',
         {'d': [{'items': [{'k': 1}]}], 'decoy': [{'j': 1}, {'j': 2}]}, 'k', 'd.0.items', 'OK'),
        ('a key=value selector resolves',
         {'s': [{'name': 'a', 'v': [{'z': 1}]}, {'name': 'b', 'v': [{'k': 2}]}]},
         'k', 's.name=b.v', 'OK'),
        ('the literal-dot key resolves (OData)', {'odata.value': [{'k': 1}]}, 'k', 'odata.value', 'OK'),
        ('the last-segment fallback still holds (ArcGIS attributes.*)',
         {'features': [{'attributes': {'iata_code': 'RFN'}}]}, 'iata_code', 'features', 'OK'),
        ('partial: some records carry it', {'r': [{'k': 1}, {'j': 2}]}, 'k', 'r', 'ID_KEYS_PARTIAL'),
    ]
    bad = 0
    for label, doc, idk, ap, want in cases:
        got = judge(doc, idk, ap)[0]
        if got != want:
            bad += 1
            print('audit_idkeys SELF-TEST FAIL  %-66s got=%s want=%s' % (label, got, want))
    return bad == 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out')
    ap.add_argument('--match', default='', help='substring of the source id')
    ap.add_argument('--all', action='store_true',
                    help='every HP_JSON row, not only the ones that cycle')
    ap.add_argument('--jobs', type=int, default=6)
    ap.add_argument('--host-gap-ms', type=int, default=1000)
    ap.add_argument('--timeout', type=int, default=45)
    ap.add_argument('--max-bytes', type=int, default=40_000_000)
    ap.add_argument('--self-test', action='store_true', help='run the fixtures and exit')
    ap.add_argument('--count', action='store_true', help='print how many rows are selected and exit')
    args = ap.parse_args()

    if not self_test():
        return 2
    if args.self_test:
        print('audit_idkeys fixtures: all pass')
        return 0
    rows = rows_from_tree(args.match, not args.all)
    print('%d row(s) to check' % len(rows), file=sys.stderr)
    for why, n in SKIPPED.most_common():
        print('  skipped %d: %s' % (n, why), file=sys.stderr)
    if args.count:
        return 0
    res = []
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for i, out in enumerate(ex.map(lambda r: one(r, args), rows)):
            res.append(out)
            if out[2] != 'OK':
                print('%-42s %-18s %s' % (out[0], out[2], out[5][:70]), flush=True)
            if (i + 1) % 50 == 0:
                print('  ... %d/%d' % (i + 1, len(rows)), file=sys.stderr, flush=True)
    if args.out:
        with open(args.out, 'w', encoding='utf-8', newline='\n') as fh:
            fh.write('id\tfile\tverdict\tid_keys\tarray\tdetail\n')
            for x in res:
                fh.write('\t'.join(str(y).replace('\t', ' ') for y in x) + '\n')
    counts = Counter(x[2] for x in res)
    for v, n in counts.most_common():
        print('%6d %s' % (n, v))
    # A transport failure is this path's reading, not the row's, so it is not
    # an exit code. An unmatched key is a property of the row.
    return 1 if counts.get('ID_KEYS_UNMATCHED') else 0


if __name__ == '__main__':
    sys.exit(main())
