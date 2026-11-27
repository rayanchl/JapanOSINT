#!/usr/bin/env python3
"""audit_scheduled_filter.py — rule 4d for SCHEDULED rows.

`probe_hp_batch.py --check-filter` asks an entity pivot about an impossible
entity. It never looks at a scheduled row, because a scheduled row has no `{q}`
token: its filter value is a LITERAL frozen at authoring time
(`trans_country_code=BW`). Eleven `af-dportal-trans-XX` rows stored the same
global transactions under eleven country names that way, every gate green.

This tool finds such rows by their SIBLINGS. A family is a set of >= 3
registered rows on one host and path shape whose request URLs differ in exactly
ONE query-parameter value or ONE path segment. For one member of each family
(two for a family of 20 or more) it makes three page-1 requests:

    real        the row's own URL
    sibling     the same URL with a sibling's value substituted
    impossible  the same URL with a value that cannot exist

and compares status, body bytes, record count and the first five records'
content hashes.

    FILTER_HONOURED      impossible -> error / zero records / clearly smaller,
                         AND real differs from sibling
    FILTER_IGNORED       all three return the same records. THE finding.
    SIBLINGS_IDENTICAL   real == sibling although impossible differs
    UNMEASURED           a fetch failed, a body would not parse, or the three
                         answers fit none of the above — always with a reason,
                         never a verdict by default.

Rows and URLs come from audit_distinctness.py's readers (hp tables and the
generated VJSON/VGEO macro rows), as do the paced fetch, the CSV/XML readers
and the record-array resolution: one copy of each (rule 4c). What that means
for coverage is stated rather than hidden: bespoke REGISTER_SOURCE collectors,
VRSS/VCSV rows, HP_XLSX/HP_HTML/HP_NDJSON rows and POST-body rows are not
read here. A row with any `{token}` in its URL is skipped.

A FILTER_IGNORED verdict is a question for a human: re-ask it by hand once
before acting on it (a wrong accusation is worse than a miss).

    python3 tools/audit_scheduled_filter.py --families-out fam.tsv --out res.tsv
    python3 tools/audit_scheduled_filter.py --self-test
"""
import argparse
import collections
import hashlib
import json
import os
import re
import sys
import threading
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audit_distinctness as D      # noqa: E402  row readers, fetch, parsers
AK = D.AK

UA = 'JapanOSINT/1.0'
ALT_IMPOSSIBLE = 'ZZQQXJKZZQQ'      # no digit: a search analyser cannot split it

# A differing piece with one of these names is the row's PAGE, not its filter.
PAGING = re.compile(r'^(\$?(offset|skip|top|limit)|page|start|startindex|'
                    r'resultoffset|resultrecordcount|rows|count|per_page|'
                    r'page_size|pagesize|perpage|maxfeatures|_limit|_offset|'
                    r'page\[number\]|page%5bnumber%5d|cursor)$', re.I)
# Names that read like a place / agency / category filter: measured first.
FILTERISH = re.compile(r'(country|state|region|code|agency|org|dept|depart|'
                       r'categor|lang|pref|province|county|city|iso|geo|area|'
                       r'sector|ministry|municip|district|locale|nation|lg_|'
                       r'jurisd|topic|subject|tag|type|group|publisher|owner)',
                       re.I)
# Names that select a DATASET rather than filter one: measured last.
DATASETISH = re.compile(r'^(resource_id|id|dataset|dataset_id|layer|layers|'
                        r'typename|typenames|feed|file|uuid|guid)$', re.I)


# ───────────────────────────── URL decomposition ─────────────────────────────
def split_url(url):
    """-> (origin, [path segments], [(name, value) raw query pairs]) or None.
    Kept RAW (no decoding, order preserved) so a URL can be rebuilt byte for
    byte with one piece swapped."""
    m = re.match(r'^(https?://[^/?#]+)([^?#]*)(?:\?([^#]*))?', url)
    if not m:
        return None
    origin, path, q = m.group(1), m.group(2), m.group(3)
    segs = path.split('/')[1:] if path else []
    pairs = []
    if q:
        for p in q.split('&'):
            k, eq, v = p.partition('=')
            pairs.append((k, v if eq else None))
    return origin, segs, pairs


def join_url(origin, segs, pairs):
    u = origin + ''.join('/' + s for s in segs)
    if pairs:
        u += '?' + '&'.join(k if v is None else '%s=%s' % (k, v)
                            for k, v in pairs)
    return u


def swap(url, kind, pos, value):
    origin, segs, pairs = split_url(url)
    if kind == 'query':
        pairs = list(pairs)
        pairs[pos] = (pairs[pos][0], value)
    else:
        segs = list(segs)
        segs[pos] = value
    return join_url(origin, segs, pairs)


def piece(url, kind, pos):
    origin, segs, pairs = split_url(url)
    return pairs[pos][1] if kind == 'query' else segs[pos]


SKIP_TOKEN = re.compile(r'\{')      # {q, {date, {ago, {wstart, {key}, {page} …


def families_of(rows, min_size=3):
    """Group rows whose URLs differ in exactly one query value or one path
    segment. A row is given to the LARGEST family it could belong to, so a row
    is measured once. The host is part of every key, so rows that differ in
    their hostname never form a family."""
    cands = collections.defaultdict(dict)        # key -> {value: row}
    parsed = {}
    for r in rows:
        url = r['url']
        if SKIP_TOKEN.search(url):
            continue
        sp = split_url(url)
        if not sp:
            continue
        origin, segs, pairs = sp
        parsed[r['id']] = sp
        mode = r.get('mode') or r['engine']
        for i, s in enumerate(segs):
            if not s:
                continue
            key = ('path', i, mode, origin,
                   tuple(segs[:i]) + ('\0',) + tuple(segs[i + 1:]),
                   tuple(pairs))
            cands[key].setdefault(s, r)
        for i, (k, v) in enumerate(pairs):
            if v is None or v == '' or PAGING.match(k):
                continue
            key = ('query', i, mode, origin, tuple(segs),
                   tuple(pairs[:i]) + ((k, '\0'),) + tuple(pairs[i + 1:]))
            cands[key].setdefault(v, r)
    fams, taken = [], set()
    for key, members in sorted(cands.items(),
                               key=lambda kv: (-len(kv[1]), str(kv[0]))):
        free = {v: r for v, r in members.items() if r['id'] not in taken}
        if len(free) < min_size:
            continue
        kind, pos = key[0], key[1]
        origin, segs, pairs = key[3], key[4], key[5]
        if kind == 'query':
            name = pairs[pos][0]
            shape = join_url(origin, segs,
                             [(k, '*' if v == '\0' else v) for k, v in pairs])
        else:
            name = 'path[%d]' % pos
            shape = join_url(origin, ['*' if s == '\0' else s for s in segs],
                             pairs)
        ordered = sorted(free.items(), key=lambda kv: kv[1]['id'])
        for _v, r in ordered:
            taken.add(r['id'])
        fams.append(dict(key=shape, kind=kind, pos=pos, param=name,
                         members=[r for _v, r in ordered],
                         values=[v for v, _r in ordered]))
    return fams


def priority(f):
    """Lower sorts first. Tier 0: a parameter NAMED like a place / agency /
    category filter, or a path segment of 2-3 letter codes. Tier 1: any other
    named query parameter (q, fq, where …). Tier 2: a path slug. Tier 3: a
    piece that selects a DATASET or collection rather than filtering one (a
    resource id, a WordPress REST post type, an opaque path id). Then size."""
    vals = f['values']
    if f['kind'] == 'query':
        if DATASETISH.match(f['param']):
            tier = 3
        elif FILTERISH.search(f['param']):
            tier = 0
        else:
            tier = 1
    else:
        core = [v.rsplit('.', 1)[0] for v in vals]
        if '/wp-json/' in f['key']:
            tier = 3
        elif all(re.fullmatch(r'[A-Za-z]{2,3}', c) for c in core):
            tier = 0
        elif all(re.fullmatch(r'[A-Za-z][A-Za-z_\-]{1,24}', c) for c in core):
            tier = 2
        else:
            tier = 3
    return (tier, -len(f['members']), f['key'])


def impossible(values, kind):
    """A value that cannot exist, in the family's own style. A query value is
    `ZZQQ9` (digits when every real value is numeric); a path segment keeps the
    siblings' length class, case and file extension."""
    ext = ''
    cores = list(values)
    m = [re.search(r'(\.[A-Za-z0-9]{1,5})$', v) for v in values]
    if all(m) and len({x.group(1) for x in m}) == 1:
        ext = m[0].group(1)
        cores = [v[:-len(ext)] for v in values]
    if all(re.fullmatch(r'\d+', c) for c in cores):
        n = max(len(c) for c in cores) if kind == 'path' else \
            max(9, max(len(c) for c in cores))
        for d in '98':
            if d * n not in cores:
                return d * n + ext
        return '9' * (n + 3) + ext
    if kind == 'path':
        n = sorted(len(c) for c in cores)[len(cores) // 2]
        n = max(2, min(n, 12))
        out = ('ZZQQ9' * 3)[:n]
        if n >= 5 and not out[-1].isdigit():
            out = out[:-1] + '9'
    else:
        out = 'ZZQQ9'
    if all(c == c.lower() for c in cores):
        out = out.lower()
    while out in cores:
        out += 'q'
    return out + ext


# ───────────────────────────────── observation ───────────────────────────────
_req_lock = threading.Lock()
_requests = [0]


def observe(url, row, args):
    """One page-1 request -> dict(status, bytes, count, hashes, err).
    status: '2xx' HTTP code string, or '' for a transport failure (err says
    which). count is -1 when the body could not be read as records."""
    with _req_lock:
        _requests[0] += 1
    raw, err = D.fetch_raw(url, args)
    if raw is None:
        m = re.match(r'HTTP (\d+)', err or '')
        if m:
            return dict(status=m.group(1), bytes=-1, count=-1, hashes=[], err='')
        if err == 'empty body':
            return dict(status='200', bytes=0, count=0, hashes=[], err='')
        return dict(status='', bytes=-1, count=-1, hashes=[],
                    err=err or 'fetch failed')
    mode = row.get('mode') or 'HP_JSON'
    recs, perr = None, ''
    if mode == 'HP_CSV':
        recs, perr = D.parse_csv(raw, row)
    elif mode == 'HP_XML':
        recs, perr = D.parse_xml(raw, row)
    else:
        try:
            doc = json.loads(raw.decode('utf-8', 'replace'))
        except Exception as e:
            doc, perr = None, 'unparsed: %s' % str(e)[:50]
        if doc is not None:
            recs = D.features_of(doc) if row['engine'] == 'vgeo' else None
            if recs is None:
                recs, _how = AK.pick_array(doc, row.get('path', ''))
            recs = [x for x in (recs or []) if isinstance(x, dict)]
    if recs is None:
        return dict(status='200', bytes=len(raw), count=-1, hashes=[],
                    err=perr or 'not parseable')
    hashes = [hashlib.sha1(D.canon(x).encode('utf-8', 'replace')).hexdigest()[:12]
              for x in recs[:5]]
    return dict(status='200', bytes=len(raw), count=len(recs), hashes=hashes,
                err='')


def _ok(o):
    return o['status'].startswith('2') and o['count'] >= 0


def same_records(a, b):
    """'' when the two answers are NOT the same records, else the basis.
    Both must hold records: two empty answers are not 'the same collection'."""
    if not (_ok(a) and _ok(b)) or a['count'] <= 0 or b['count'] <= 0:
        return ''
    if a['hashes'] == b['hashes']:
        return 'first-5 hashes equal'
    if a['count'] == b['count'] and a['bytes'] > 0 and \
            abs(a['bytes'] - b['bytes']) <= 0.01 * max(a['bytes'], b['bytes']):
        return 'count equal and bytes within 1%'
    return ''


def judge(real, sib, imp):
    """-> (verdict, reason). The whole decision, with no I/O, so it can be
    pinned offline."""
    for name, o in (('real', real), ('sibling', sib), ('impossible', imp)):
        if not o['status']:
            return 'UNMEASURED', '%s request failed: %s' % (name, o['err'])
    if not real['status'].startswith('2'):
        return 'UNMEASURED', 'real value answered HTTP %s' % real['status']
    if real['count'] < 0:
        return 'UNMEASURED', 'real body not parseable: %s' % real['err']
    if real['count'] == 0:
        return 'UNMEASURED', ('real value returned zero records, so there is '
                              'nothing to compare an impossible value against')
    if imp['status'] in ('429', '503') or sib['status'] in ('429', '503'):
        return 'UNMEASURED', 'rate-limited or unavailable (HTTP %s/%s) mid-comparison' % (
            sib['status'], imp['status'])
    if not sib['status'].startswith('2'):
        return 'UNMEASURED', 'sibling value answered HTTP %s' % sib['status']
    if sib['count'] < 0:
        return 'UNMEASURED', 'sibling body not parseable: %s' % sib['err']

    rs = same_records(real, sib)
    ri = same_records(real, imp)
    if ri and rs:
        return 'FILTER_IGNORED', ('real, sibling and impossible return the same '
                                  'records (real~impossible: %s; real~sibling: %s)'
                                  % (ri, rs))
    # was the impossible value rejected?
    if not imp['status'].startswith('2'):
        rej = 'impossible -> HTTP %s' % imp['status']
    elif imp['count'] < 0:
        rej = 'impossible -> a body that is not records (%s)' % imp['err']
    elif imp['count'] == 0:
        rej = 'impossible -> zero records'
    elif not ri and (imp['count'] * 2 <= real['count'] or
                     (imp['count'] <= real['count'] and
                      0 < imp['bytes'] * 2 <= real['bytes'])):
        rej = 'impossible -> clearly smaller (%d records, %d bytes)' % (
            imp['count'], imp['bytes'])
    else:
        rej = ''
    if rej:
        # Only HASH equality makes two siblings identical. "Same count, bytes
        # within 1%" is two FULL PAGES of different records as often as not
        # (measured on the first whole-tree run: 14 of 17 SIBLINGS_IDENTICAL
        # verdicts were that) — it is evidence only when the impossible value
        # matches too, which is the FILTER_IGNORED case above.
        if rs.startswith('first-5'):
            return 'SIBLINGS_IDENTICAL', '%s, but real == sibling (%s)' % (rej, rs)
        if sib['count'] == 0:
            return 'UNMEASURED', ('%s, but the sibling value returned zero '
                                  'records too, so real-vs-sibling is not shown'
                                  % rej)
        return 'FILTER_HONOURED', '%s; real and sibling differ' % rej
    if ri:
        return 'UNMEASURED', ('AMBIGUOUS: impossible returns what real returns '
                              '(%s) while the sibling differs — read by hand' % ri)
    return 'UNMEASURED', ('AMBIGUOUS: impossible value returned %d records that '
                          'are neither the real answer nor clearly smaller — '
                          'read by hand' % imp['count'])


def fmt(o):
    return '%s/%s/%s' % (o['status'] or 'ERR',
                         o['bytes'] if o['bytes'] >= 0 else '-',
                         o['count'] if o['count'] >= 0 else '-')


def test_member(fam, idx, args):
    row = fam['members'][idx]
    real_v = fam['values'][idx]
    sib_i = (idx + 1) % len(fam['members'])
    sib_v = fam['values'][sib_i]
    imp_v = impossible(fam['values'], fam['kind'])
    u_real = row['url']
    u_sib = swap(u_real, fam['kind'], fam['pos'], sib_v)
    u_imp = swap(u_real, fam['kind'], fam['pos'], imp_v)
    real = observe(u_real, row, args)
    if not _ok(real) or real['count'] == 0:
        # nothing can be concluded; do not spend the other two requests
        blank = dict(status='-', bytes=-1, count=-1, hashes=[], err='not asked')
        v, why = judge(real, dict(blank, status='200', count=0),
                       dict(blank, status='200', count=0))
        sib = imp = blank
    else:
        sib = observe(u_sib, row, args)
        imp = observe(u_imp, row, args)
        v, why = judge(real, sib, imp)
        if v == 'UNMEASURED' and why.startswith('AMBIGUOUS') and \
                fam['kind'] == 'query' and not imp_v.isdigit():
            # A full-text search can SPLIT `zzqq9` at the letter/digit seam and
            # match every record carrying a 9 (measured on 20 CKAN `q=` rows:
            # the "impossible" word returned 100 records, more than the real
            # one). One more request with a letters-only word settles it.
            alt = ALT_IMPOSSIBLE.lower() if imp_v == imp_v.lower() else ALT_IMPOSSIBLE
            imp2 = observe(swap(u_real, fam['kind'], fam['pos'], alt), row, args)
            v2, why2 = judge(real, sib, imp2)
            if not (v2 == 'UNMEASURED' and why2.startswith('AMBIGUOUS')):
                first = fmt(imp)
                imp, imp_v, u_imp = imp2, alt, swap(u_real, fam['kind'], fam['pos'], alt)
                v, why = v2, why2 + ' [letters-only impossible value; `%s` had answered %s]' % (
                    impossible(fam['values'], fam['kind']), first)
    return dict(fam=fam, id=row['id'], file=row['file'], real_v=real_v,
                sib_id=fam['members'][sib_i]['id'], sib_v=sib_v, imp_v=imp_v,
                verdict=v, why=why, real=real, sib=sib, imp=imp,
                u_real=u_real, u_imp=u_imp)


# ───────────────────────── self-test (no network, no DB) ──────────────────────
def self_test():
    bad = []

    def check(name, cond):
        print('  %-4s %s' % ('ok' if cond else 'FAIL', name))
        if not cond:
            bad.append(name)

    def R(i, url, mode='HP_JSON'):
        return dict(id=i, file='t.c', url=url, engine='hp', mode=mode, path='')

    rows = [
        R('c-bw', 'https://x.org/q?from=trans&country=BW&limit=50'),
        R('c-gh', 'https://x.org/q?from=trans&country=GH&limit=50'),
        R('c-ke', 'https://x.org/q?from=trans&country=KE&limit=50'),
        R('s-ca', 'https://y.gov/api/state/CA/list.json'),
        R('s-tx', 'https://y.gov/api/state/TX/list.json'),
        R('s-ny', 'https://y.gov/api/state/NY/list.json'),
        R('h-1', 'https://a.example.com/feed/'),
        R('h-2', 'https://b.example.com/feed/'),
        R('h-3', 'https://c.example.com/feed/'),
        R('two-1', 'https://z.io/v?a=1&b=x'), R('two-2', 'https://z.io/v?a=2&b=y'),
        R('two-3', 'https://z.io/v?a=3&b=z'),
        R('tok-1', 'https://t.io/s?c=AA&q={q}'), R('tok-2', 'https://t.io/s?c=BB&q={q}'),
        R('tok-3', 'https://t.io/s?c=CC&q={q}'),
        R('d-1', 'https://t.io/d?day={date:%Y}&c=AA'), R('d-2', 'https://t.io/d?day={date:%Y}&c=BB'),
        R('d-3', 'https://t.io/d?day={date:%Y}&c=CC'),
        R('pg-1', 'https://p.io/l?page=1'), R('pg-2', 'https://p.io/l?page=2'),
        R('pg-3', 'https://p.io/l?page=3'),
        R('sm-1', 'https://s.io/l?k=1'), R('sm-2', 'https://s.io/l?k=2'),
    ]
    fams = families_of(rows)
    by = {f['param']: f for f in fams}
    check('1  query family found (country), 3 members',
          'country' in by and len(by['country']['members']) == 3)
    check('1b its key shows the varying piece as *',
          'country' in by and by['country']['key'] ==
          'https://x.org/q?from=trans&country=*&limit=50')
    check('2  path-segment family found', 'path[2]' in by and
          sorted(by['path[2]']['values']) == ['CA', 'NY', 'TX'])
    check('3  rows differing only in HOSTNAME are not a family',
          not any('example.com' in f['key'] for f in fams))
    check('4  rows differing in TWO parameters are not a family',
          not any('z.io' in f['key'] for f in fams))
    check('5  entity / date token rows are skipped',
          not any('t.io' in f['key'] for f in fams))
    check('6  a paging parameter is not a filter',
          not any('p.io' in f['key'] for f in fams))
    check('7  a family of two is below the minimum',
          not any('s.io' in f['key'] for f in fams))
    check('8  exactly the two families', len(fams) == 2)
    check('9  swap rebuilds the URL with one piece changed',
          swap(rows[0]['url'], 'query', 1, 'ZZQQ9') ==
          'https://x.org/q?from=trans&country=ZZQQ9&limit=50' and
          swap(rows[3]['url'], 'path', 2, 'ZZ') ==
          'https://y.gov/api/state/ZZ/list.json')
    check('10 impossible: query -> ZZQQ9; path keeps length class',
          impossible(['BW', 'GH', 'KE'], 'query') == 'ZZQQ9' and
          impossible(['CA', 'TX', 'NY'], 'path') == 'ZZ')
    check('10b impossible keeps a shared extension and case',
          impossible(['ca.json', 'tx.json', 'ny.json'], 'path') == 'zz.json')
    check('10c impossible numeric stays numeric and is not a real value',
          impossible(['13', '27', '99'], 'path') == '88' and
          impossible(['13', '27'], 'query') == '999999999')
    check('11 priority: country family before an opaque dataset family',
          priority(dict(kind='query', param='country_code', values=['a'], members=[1], key='k'))
          < priority(dict(kind='query', param='resource_id', values=['a'] * 9, members=[1] * 9, key='k')))
    check('11b a WordPress REST post-type path is a collection selector (last tier)',
          priority(dict(kind='path', param='path[3]', values=['posts', 'news', 'event'], members=[1] * 3,
                        key='https://h.jp/wp-json/wp/v2/*?per_page=100'))[0] == 3)

    def O(status='200', nbytes=1000, count=10, h='a', err=''):
        return dict(status=status, bytes=nbytes, count=count,
                    hashes=[h + str(i) for i in range(min(5, max(count, 0)))],
                    err=err)

    # THE finding: three values, one answer.
    v, _ = judge(O(h='g'), O(h='g'), O(h='g'))
    check('20 IGNORED: same first-5 hashes for real, sibling, impossible',
          v == 'FILTER_IGNORED')
    # the hash comparison on its own: same records inside envelopes of
    # different size, so the count+bytes route cannot be what decides it.
    # (20 alone passed with the hash test disabled — it did not discriminate.)
    v, why = judge(O(h='g', nbytes=1000), O(h='g', nbytes=1300), O(h='g', nbytes=1700))
    check('20c IGNORED on hashes alone (bytes differ by more than 1%)',
          v == 'FILTER_IGNORED' and 'hashes' in why)
    # unordered upstream: hashes move, count and size do not (d-portal's shape)
    v, _ = judge(O(nbytes=100000, count=500, h='a'), O(nbytes=100300, count=500, h='b'),
                 O(nbytes=99800, count=500, h='c'))
    check('20b IGNORED: hashes differ but count equal and bytes within 1%',
          v == 'FILTER_IGNORED')
    v, _ = judge(O(h='bw'), O(h='gh', nbytes=1400, count=14), O(count=0, nbytes=40))
    check('21 HONOURED: impossible -> zero records, siblings differ',
          v == 'FILTER_HONOURED')
    v, _ = judge(O(h='bw'), O(h='gh', nbytes=1400, count=14), O(status='404', nbytes=-1, count=-1))
    check('21b HONOURED: impossible -> HTTP 404', v == 'FILTER_HONOURED')
    v, _ = judge(O(h='bw', nbytes=90000, count=100), O(h='gh', nbytes=50000, count=60),
                 O(h='zz', nbytes=900, count=2))
    check('21c HONOURED: impossible clearly smaller', v == 'FILTER_HONOURED')
    v, _ = judge(O(h='g'), O(h='g'), O(count=0, nbytes=40))
    check('22 SIBLINGS_IDENTICAL: real == sibling, impossible empty',
          v == 'SIBLINGS_IDENTICAL')
    v, _ = judge(O(h='bw', nbytes=46273, count=50), O(h='bj', nbytes=46304, count=50),
                 O(count=0, nbytes=390))
    check('22b HONOURED, not SIBLINGS_IDENTICAL: two full pages of similar size '
          'but different records', v == 'FILTER_HONOURED')
    v, _ = judge(O(status='', err='CONNECT tunnel failed, response 403'), O(), O())
    check('23 UNMEASURED: proxy refusal on the real request', v == 'UNMEASURED')
    v, _ = judge(O(), O(h='b', nbytes=3000, count=30), O(status='', err='timeout'))
    check('23b UNMEASURED: impossible request failed in transport', v == 'UNMEASURED')
    v, _ = judge(O(count=-1, err='unparsed'), O(), O())
    check('23c UNMEASURED: real body not parseable', v == 'UNMEASURED')
    v, _ = judge(O(count=0, nbytes=30), O(count=0, nbytes=30), O(count=0, nbytes=30))
    check('23d UNMEASURED (not IGNORED): three EMPTY answers prove nothing',
          v == 'UNMEASURED')
    v, _ = judge(O(), O(h='b', nbytes=3000, count=30), O(status='429', count=-1))
    check('23e UNMEASURED: a 429 is not a rejection of the value', v == 'UNMEASURED')
    v, _ = judge(O(h='a', nbytes=5000, count=50), O(h='b', nbytes=9000, count=50),
                 O(h='c', nbytes=7000, count=50))
    check('24 UNMEASURED/AMBIGUOUS: impossible returns OTHER full records',
          v == 'UNMEASURED')
    v, why = judge(O(h='a'), O(h='b', nbytes=3000, count=30), O(count=-1, nbytes=7000, err='unparsed: html'))
    check('25 HONOURED: impossible -> a body that is not records',
          v == 'FILTER_HONOURED' and 'not records' in why)
    v, _ = judge(O(h='a'), O(status='500', count=-1), O(count=0))
    check('26 UNMEASURED: the sibling request errored', v == 'UNMEASURED')

    print('\n%s (%d failing)' % ('SELF-TEST FAILED' if bad else 'self-test passed',
                                 len(bad)))
    return 1 if bad else 0


def main():
    if '--self-test' in sys.argv[1:]:
        return self_test()
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--families-out')
    ap.add_argument('--out')
    ap.add_argument('--host-gap-ms', type=int, default=1000)
    ap.add_argument('--limit-families', type=int, default=0)
    ap.add_argument('--match', default='',
                    help='regex on the family key or any member id')
    ap.add_argument('--max-requests', type=int, default=1500,
                    help='stop STARTING families once this many requests are '
                         'committed; the rest are reported NOT_RUN')
    ap.add_argument('--jobs', type=int, default=8)
    ap.add_argument('--timeout', type=int, default=60)
    ap.add_argument('--max-bytes', type=int, default=40_000_000)
    ap.add_argument('--list-only', action='store_true',
                    help='write the family list and make no request')
    ap.add_argument('--self-test', action='store_true')
    args = ap.parse_args()
    AK.UA = UA                       # D.fetch_raw reads it at call time

    rows = D.all_rows('')
    n_tok = sum(1 for r in rows if SKIP_TOKEN.search(r['url']))
    fams = families_of(rows)
    if args.match:
        rx = re.compile(args.match)
        fams = [f for f in fams if rx.search(f['key']) or
                any(rx.search(r['id']) for r in f['members'])]
    fams.sort(key=priority)
    print('%d readable row(s) (%d skipped for a {token}); %d familie(s) '
          'covering %d row(s)' % (len(rows), n_tok, len(fams),
                                  sum(len(f['members']) for f in fams)),
          file=sys.stderr)
    if args.families_out:
        with open(args.families_out, 'w', encoding='utf-8', newline='\n') as fh:
            fh.write('rank\ttier\tfamily_key\tkind\tparam\tn_members\t'
                     'example_values\texample_ids\tmember_ids\n')
            for i, f in enumerate(fams):
                fh.write('\t'.join(str(c).replace('\t', ' ') for c in [
                    i + 1, priority(f)[0], f['key'], f['kind'], f['param'],
                    len(f['members']), ','.join(f['values'][:5]),
                    ','.join(r['id'] for r in f['members'][:4]),
                    ','.join(r['id'] for r in f['members'])]) + '\n')
    if args.list_only:
        return 0

    # Plan inside the budget, in priority order. One member (two for >= 20).
    plan, skipped, committed = [], [], 0
    for f in fams:
        if args.limit_families and len({id(p[0]) for p in plan}) >= args.limit_families:
            skipped.append(f)
            continue
        picks = [0] if len(f['members']) < 20 else [0, len(f['members']) // 2]
        if committed + 3 * len(picks) > args.max_requests:
            skipped.append(f)
            continue
        committed += 3 * len(picks)
        plan.extend((f, i) for i in picks)
    print('%d test(s) planned (<= %d requests); %d familie(s) NOT_RUN'
          % (len(plan), committed, len(skipped)), file=sys.stderr)

    res = []
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for out in ex.map(lambda p: test_member(p[0], p[1], args), plan):
            res.append(out)
            print('%-18s %-44s real=%s sib=%s imp=%s  %s' % (
                out['verdict'], out['id'][:44], fmt(out['real']),
                fmt(out['sib']), fmt(out['imp']), out['why'][:90]), flush=True)
    if args.out:
        cols = ['verdict', 'family_key', 'kind', 'param', 'n_members',
                'tested_id', 'file', 'real_value', 'sibling_id', 'sibling_value',
                'impossible_value', 'real_status/bytes/count',
                'sibling_status/bytes/count', 'impossible_status/bytes/count',
                'real_first5', 'sibling_first5', 'impossible_first5', 'reason',
                'real_url', 'impossible_url']
        with open(args.out, 'w', encoding='utf-8', newline='\n') as fh:
            fh.write('\t'.join(cols) + '\n')
            for x in res:
                f = x['fam']
                line = [x['verdict'], f['key'], f['kind'], f['param'],
                        len(f['members']), x['id'], x['file'], x['real_v'],
                        x['sib_id'], x['sib_v'], x['imp_v'], fmt(x['real']),
                        fmt(x['sib']), fmt(x['imp']),
                        ' '.join(x['real']['hashes']), ' '.join(x['sib']['hashes']),
                        ' '.join(x['imp']['hashes']), x['why'], x['u_real'],
                        x['u_imp']]
                fh.write('\t'.join(str(c).replace('\t', ' ').replace('\n', ' ')
                                   for c in line) + '\n')
            # A family the budget did not reach is a ROW, never an absence.
            for f in skipped:
                line = ['NOT_RUN', f['key'], f['kind'], f['param'],
                        len(f['members']), f['members'][0]['id'],
                        f['members'][0]['file']] + [''] * 10 + \
                       ['request budget / --limit-families: unmeasured, not clean',
                        f['members'][0]['url'], '']
                fh.write('\t'.join(str(c).replace('\t', ' ') for c in line) + '\n')
    c = collections.Counter(x['verdict'] for x in res)
    print('\n%d request(s) made' % _requests[0], file=sys.stderr)
    for v, n in c.most_common():
        print('%6d %s' % (n, v), file=sys.stderr)
    print('%6d NOT_RUN (families)' % len(skipped), file=sys.stderr)
    return 1 if c.get('FILTER_IGNORED') else 0


if __name__ == '__main__':
    sys.exit(main())
