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

Verdicts: OK, ID_KEYS_UNMATCHED (the finding), UNMEASURED (transport), UNPARSED
(not JSON through this path — a `post_body` row cannot be asked with a GET),
NO_RECORDS. Nothing is edited, and nothing is retired on one reading.
"""
import argparse
import glob
import json
import os
import re
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
UA = 'Mozilla/5.0 (compatible; JapanOSINT/1.0)'
ENTITY_TOKEN = re.compile(r'\{(q|qd|qh|qu|date|ago|page|offset|size)')

_gap, _lock = {}, threading.Lock()


def paced(host, gap_s):
    """One request per host per gap, across workers — the probe's own rule."""
    with _lock:
        last = _gap.get(host, 0.0)
        wait = max(0.0, gap_s - (time.time() - last))
        _gap[host] = time.time() + wait
    if wait:
        time.sleep(wait)


def rows_from_tree(match, want_resume):
    out = []
    for f in sorted(glob.glob(os.path.join(NATIVE, 'collectors', '**', '*.c'),
                              recursive=True)):
        txt = open(f, encoding='utf-8', errors='replace').read()
        if '.id_keys' not in txt:
            continue
        starts = [m.start() for m in re.finditer(r'\{ \.id = "', txt)]
        for i, s in enumerate(starts):
            blk = txt[s:starts[i + 1] if i + 1 < len(starts) else len(txt)]
            sid = re.match(r'\{ \.id = "([^"]+)"', blk).group(1)
            if match and match not in sid:
                continue
            if want_resume and '.page_resume = 1' not in blk:
                continue

            def opt(name, blk=blk):
                m = re.search(r'\.%s\s*=\s*((?:\s*"(?:[^"\\]|\\.)*")+)' % name, blk)
                return ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))) if m else ''

            mode = re.search(r'\.mode\s*=\s*(HP_[A-Z]+)', blk)
            idk, url = opt('id_keys'), opt('url')
            if not idk or not url or (mode.group(1) if mode else '') != 'HP_JSON':
                continue
            if ENTITY_TOKEN.search(url):
                continue
            out.append((sid, os.path.basename(f), idk, opt('array_path'), url))
    return out


def flat(node, prefix='', out=None):
    """hp_flatten(): nested objects dotted, array elements indexed."""
    if out is None:
        out = {}
    if isinstance(node, dict):
        items = node.items()
    elif isinstance(node, list):
        items = ((str(i), v) for i, v in enumerate(node))
    else:
        return out
    for k, v in items:
        key = '%s%s' % (prefix, k)
        if isinstance(v, (dict, list)):
            flat(v, key + '.', out)
        else:
            out[key.lower()] = v
    return out


def present(name, fields):
    """hp_flat_get(): the exact flat key, else any key whose LAST dotted
    segment matches case-insensitively, a leading `@` on that segment
    skipped."""
    n = name.lower()
    if n in fields:
        return True
    for k in fields:
        last = k.rsplit('.', 1)[-1]
        if last.startswith('@') and not n.startswith('@'):
            last = last[1:]
        if last == n:
            return True
    return False


def satisfied(spec, fields):
    """`a+b,c`: a comma alternative counts when ALL its `+` parts are there."""
    for alt in spec.split(','):
        parts = [p.strip().lower() for p in alt.split('+') if p.strip()]
        if parts and all(present(p, fields) for p in parts):
            return True, alt
    return False, ''


def pick_array(doc, path):
    node = doc
    if path == '.':
        node = doc
    else:
        for seg in (path.split('.') if path else []):
            node = node.get(seg) if isinstance(node, dict) else None
    if isinstance(node, list):
        return node, 'declared'
    best, bl = None, 0

    def walk(n):
        nonlocal best, bl
        if isinstance(n, list):
            k = sum(1 for x in n if isinstance(x, dict))
            if k > bl:
                best, bl = n, k
            for x in n:
                walk(x)
        elif isinstance(n, dict):
            for v in n.values():
                walk(v)

    walk(doc)
    return best, ('fallback' if path else 'densest')


def one(row, args):
    sid, f, idk, apath, url = row
    paced(url.split('/')[2], args.host_gap_ms / 1000.0)
    # bytes, not text: one body that is not UTF-8 (Shift_JIS, Latin-1, a
    # mislabelled page) killed the whole sweep when subprocess decoded it.
    r = subprocess.run(['curl', '-sSg', '--max-time', str(args.timeout),
                        '--max-filesize', str(args.max_bytes), '-A', UA, url],
                       capture_output=True)
    body = r.stdout.decode('utf-8', 'replace')
    err = r.stderr.decode('utf-8', 'replace').strip()[:70]
    if r.returncode != 0 or not body.strip():
        return (sid, f, 'UNMEASURED', idk, '', err or 'empty body')
    try:
        doc = json.loads(body)
    except Exception as e:
        return (sid, f, 'UNPARSED', idk, '', str(e)[:70])
    arr, how = pick_array(doc, apath)
    rec = next((x for x in arr if isinstance(x, dict)), None) if arr else None
    if rec is None:
        return (sid, f, 'NO_RECORDS', idk, how, 'no array of objects in the body')
    fields = flat(rec)
    ok, alt = satisfied(idk, fields)
    return (sid, f, 'OK' if ok else 'ID_KEYS_UNMATCHED', idk, how,
            ('matched %s' % alt) if ok else
            ('no part present; record carries %s' % ','.join(sorted(fields)[:10])))


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
    args = ap.parse_args()

    rows = rows_from_tree(args.match, not args.all)
    print('%d row(s) to check' % len(rows), file=sys.stderr)
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
    from collections import Counter
    counts = Counter(x[2] for x in res)
    for v, n in counts.most_common():
        print('%6d %s' % (n, v))
    # A transport failure is this path's reading, not the row's, so it is not
    # an exit code. An unmatched key is a property of the row.
    return 1 if counts.get('ID_KEYS_UNMATCHED') else 0


if __name__ == '__main__':
    sys.exit(main())
