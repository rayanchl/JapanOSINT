#!/usr/bin/env python3
"""Is a row's declared id_keys the record's IDENTITY, and does it beat the
content hash it replaces?

THE QUESTION NOTHING ELSE IN THIS TREE ASKS. `tools/audit_idkeys.py` asks
whether a declared key names a field the upstream SENDS, and says in its own
header that it "does not answer whether the field is UNIQUE ... only the
registry sweep's COLLISION verdict or a distinctness measurement over a real
page can see it". The registry sweep then reports a COLLISION count, which
CLAUDE.md calls "a question, not a diagnosis" — either the key is a dimension
(real loss) or the upstream repeats identical records (correct dedupe). Nothing
answered the question, so every fix on record did this arithmetic BY HAND:
Catalonia, Colombia, TWSE, TX TCEQ, UK Parliament, awc-obstacles, d-portal.
This is that arithmetic.

TWO COMPARISONS, AND THE SECOND ONE IS THE ONE PEOPLE SKIP.

1. Against the CEILING. The most any key can reach is the count of distinct
   WHOLE records: two byte-identical records are one record, and no key
   separates them. A declared key equal to the ceiling is an identity for this
   body; below it, the key is a dimension and the gap is real loss.

2. Against WHAT THE ROW DOES TODAY. A row that declares NO key does not store
   nothing — hpengine, VJSON and VGEO each fall back to a precedence list and
   then to a CONTENT HASH, and a content hash collapses only byte-identical
   records, i.e. it already achieves the ceiling. So declaring a "real" id can
   make a row WORSE, and CLAUDE.md records four Socrata rows measured, applied
   and REVERTED for exactly that, plus gov-co-secop2 where the documented
   contract number loses 57 where the hash loses 2. A tool that reports only
   comparison 1 invites that mistake. This one reports both and says KEEP or
   REVERT.

   (That is not hypothetical: the six af-dportal-trans-* rows were keyed on
   2026-11-14 off a ONE-PAGE probe and a sibling's convention, and the
   comparison that should have decided it was made afterwards. It happened to
   come out equal — 99,915 both ways over 100,000 rows — but it was not known
   at the time.)

WHY TWO PAGES, NOT ONE. "Two pages can be too few" is this tree's own rule and
one page is strictly worse: Chicago's `id` is distinct on all 800 sampled
records and collapses 4,500 of 8,000, because the dimension that breaks it does
not appear until page four. A key measured on one page is not measured.

WHAT IT DELIBERATELY DOES NOT DO. It does not decide whether a repeat is two
EVENTS or one event published twice — that is `occurrence_rows`, a statement
about what the published file IS, and it is the author's reading of the
publisher's documentation, not something a counter can see. Where every column
is categorical and the repeats are byte-identical, the tool says so and names
occurrence_rows as the other possible answer rather than picking one.

MIRRORING THE ENGINE IS THE WHOLE JOB. It imports audit_idkeys for the C row
parser, the hp_flatten flattener, the hp_flat_get resolver (exact flat key,
else any key whose LAST dotted segment matches, case-insensitively, a leading
`@` skipped) and the `+` composes / `,` chooses semantics — one copy of each,
for rule 4c's reason. audit_idkeys' own first run reported 35 rows of which 30
were its own fault, for want of that resolver.

    python3 tools/audit_distinctness.py --only gov-co-secop1 --pages 2
    python3 tools/audit_distinctness.py --ids-file queue.txt --jobs 3 --out d.tsv
    python3 tools/audit_distinctness.py --self-test      # no network, no DB

Verdicts: KEY_IS_IDENTITY, KEY_IS_DIMENSION (the finding), KEY_UNMATCHED,
NO_KEY_HASH_AT_CEILING,
NO_KEY_FALLBACK_LOSES (the other finding), UPSTREAM_DUPLICATES, ONE_PAGE_ONLY,
UNMEASURED, UNPARSED, NO_RECORDS.
"""
import argparse
import collections
import glob
import hashlib
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audit_idkeys as AK          # noqa: E402  the shared mirrors live there

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)

# The three engines' uid precedence lists, each copied from the engine that
# owns it so a row with no declared key can be modelled rather than guessed.
# hpengine lib/hpengine.c ID_FALLBACK; VJSON lib/jsonlist.c K_ID; VGEO
# lib/geojson.c NATIVE_ID_KEYS then the Feature's own `id`.
FALLBACK = {
    'hp': ["id", "uid", "uuid", "number", "company_number", "companyNumber",
           "lei", "cik", "siren", "registrationNumber", "registration_number",
           "organisasjonsnummer", "businessId", "doc_id", "docId",
           "accession", "identifier", "key", "code"],
    'vjson': ["id", "uid", "guid", "uuid", "_id", "identifier", "code", "key"],
    'vgeo': ["camera_uid", "station_uid", "line_uid", "cluster_uid",
             "footprint_id", "post_uid", "event_id", "earthquake_id",
             "buoy_id", "station_id", "spring_id", "uid", "id", "uuid"],
}

# Offset/size parameter pairs the engine itself can advance, so page 2 is asked
# for the way the engine would ask. Mirrors lib/jsonlist.c PAGERS and
# lib/geojson.c's Socrata and ArcGIS branches; a row whose URL carries none of
# these is read as ONE_PAGE_ONLY rather than guessed at.
PAGERS = [('$limit', '$offset'), ('limit', 'offset'), ('rows', 'start'),
          ('per_page', 'page'), ('page_size', 'page'), ('count', 'start'),
          ('resultRecordCount', 'resultOffset'), ('maxFeatures', 'startIndex'),
          ('$top', '$skip'), ('pageSize', 'page')]

MACRO = re.compile(
    r'\b(VJSON|VJSONBIG|VJSON_KEYED|VJSON_IDKEYS|VGEO|VGEO_IDKEYS)\s*\(',)


def _args_of(text, open_paren):
    """Split one macro call's argument list at top-level commas, string-aware."""
    i, depth, cur, out, instr = open_paren + 1, 1, [], [], False
    while i < len(text):
        c = text[i]
        if instr:
            if c == '\\':
                cur.append(text[i:i + 2]); i += 2; continue
            if c == '"':
                instr = False
            cur.append(c)
        elif c == '"':
            instr = True; cur.append(c)
        elif c in '([{':
            depth += 1; cur.append(c)
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                out.append(''.join(cur)); return out, i
            cur.append(c)
        elif c == ',' and depth == 1:
            out.append(''.join(cur)); cur = []
        else:
            cur.append(c)
        i += 1
    return out, i


def _lit(arg):
    """Join a (possibly multi-part) C string literal; '' if not a literal."""
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', arg)
    return ''.join(parts).replace('\\"', '"') if parts else ''


def rows_from_macros(match):
    """Generated VJSON/VGEO rows. Positional, per _verified_macros.inc:
       VJSON (SYM,ID,NAME,NAMEJA,COLL,CAT,URL,PATH,LANG,TAGS,IVAL,DESC[,IDKEYS])
       VGEO  (SYM,ID,NAME,NAMEJA,COLL,CAT,URL,     LANG,TAGS,IVAL,DESC[,IDKEYS])
    """
    out = []
    for f in sorted(glob.glob(os.path.join(NATIVE, 'collectors', '**', '*.c'),
                              recursive=True)):
        txt = open(f, encoding='utf-8', errors='replace').read()
        if 'VJSON' not in txt and 'VGEO' not in txt:
            continue
        for m in MACRO.finditer(txt):
            kind = m.group(1)
            args, _ = _args_of(txt, m.end() - 1)
            if len(args) < 11:
                continue
            sid = _lit(args[1])
            if not sid or (match and match not in sid):
                continue
            url = _lit(args[6])
            if not url.startswith('http'):
                continue
            geo = kind.startswith('VGEO')
            path = '' if geo else _lit(args[7])
            key = ''
            if kind in ('VJSON_IDKEYS', 'VJSON_KEYED') and len(args) >= 13:
                key = _lit(args[12])
            elif kind == 'VGEO_IDKEYS' and len(args) >= 12:
                key = _lit(args[11])
            out.append(dict(id=sid, file=os.path.basename(f), key=key,
                            path=path, url=url,
                            engine='vgeo' if geo else 'vjson'))
    return out


def rows_from_hp(match):
    """hp table rows, through audit_idkeys' own parser so there is ONE copy of
    it — then widened, because that parser requires a declared id_keys and the
    unkeyed rows are exactly the ones comparison 2 is about."""
    out = []
    for f in sorted(glob.glob(os.path.join(NATIVE, 'collectors', '**', '*.c'),
                              recursive=True)):
        txt = open(f, encoding='utf-8', errors='replace').read()
        if '{ .id = "' not in txt:
            continue
        starts = [mm.start() for mm in re.finditer(r'\{ \.id = "', txt)]
        for i, s in enumerate(starts):
            blk = txt[s:starts[i + 1] if i + 1 < len(starts) else len(txt)]
            sid = re.match(r'\{ \.id = "([^"]+)"', blk).group(1)
            if match and match not in sid:
                continue

            def opt(name, blk=blk):
                mm = re.search(r'\.%s\s*=\s*((?:\s*"(?:[^"\\]|\\.)*")+)' % name, blk)
                return ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', mm.group(1))) if mm else ''

            mode = re.search(r'\.mode\s*=\s*(HP_[A-Z]+)', blk)
            url = opt('url')
            if not url.startswith('http'):
                continue
            if (mode.group(1) if mode else '') != 'HP_JSON':
                continue
            if AK.ENTITY_TOKEN.search(url):
                continue
            out.append(dict(id=sid, file=os.path.basename(f),
                            key=opt('id_keys'), path=opt('array_path'),
                            url=url, engine='hp'))
    return out


def all_rows(match):
    seen, out = set(), []
    for r in rows_from_hp(match) + rows_from_macros(match):
        if r['id'] in seen:
            continue
        seen.add(r['id']); out.append(r)
    return out


def canon(rec):
    return json.dumps(rec, sort_keys=True, ensure_ascii=False)


def key_of(fields, spec):
    """The value tuple for a `+`/`,` spec, resolved as hp_flat_get resolves it.
    None when no alternative is satisfied — i.e. the engine would fall back."""
    for alt in spec.split(','):
        parts = [p.strip().lower() for p in alt.split('+') if p.strip()]
        if parts and all(AK.present(p, fields) for p in parts):
            return tuple(_val(fields, p) for p in parts)
    return None


def _val(fields, name):
    n = name.lower()
    if n in fields:
        return str(fields[n])
    for k, v in fields.items():
        last = k.rsplit('.', 1)[-1]
        if last.startswith('@') and not n.startswith('@'):
            last = last[1:]
        if last == n:
            return str(v)
    return ''


def fallback_key(fields, engine):
    """What the engine keys on when the row declares nothing: the first name on
    its precedence list that resolves, else None (meaning a content hash)."""
    for name in FALLBACK.get(engine, []):
        if AK.present(name, fields):
            return name, _val(fields, name)
    return None, None


def features_of(doc):
    """VGEO reads a FeatureCollection; its records are the features."""
    if isinstance(doc, dict) and isinstance(doc.get('features'), list):
        return doc['features']
    return None


def page_urls(url, pages):
    """The row's URL, then the same URL with its own offset advanced the way the
    engine advances it. Returns fewer than `pages` when nothing can be moved —
    the caller reports ONE_PAGE_ONLY rather than inventing a parameter."""
    urls = [url]
    for size_p, off_p in PAGERS:
        m = re.search(r'[?&]%s=(\d+)' % re.escape(size_p), url)
        if not m:
            continue
        size = int(m.group(1))
        if size <= 0:
            continue
        cur = re.search(r'[?&]%s=(\d+)' % re.escape(off_p), url)
        base = int(cur.group(1)) if cur else 0
        for p in range(1, pages):
            off = base + size * p
            if cur:
                nxt = re.sub(r'([?&]%s=)\d+' % re.escape(off_p),
                             lambda mm: mm.group(1) + str(off), url)
            else:
                nxt = url + ('&' if '?' in url else '?') + '%s=%d' % (off_p, off)
            urls.append(nxt)
        return urls
    return urls


def fetch(url, args):
    AK.paced(url.split('/')[2], args.host_gap_ms / 1000.0)
    r = subprocess.run(['curl', '-sSg', '--max-time', str(args.timeout),
                        '--max-filesize', str(args.max_bytes), '-A', AK.UA, url],
                       capture_output=True)
    if r.returncode != 0:
        return None, r.stderr.decode('utf-8', 'replace').strip()[:70] or 'curl failed'
    body = r.stdout.decode('utf-8', 'replace')
    if not body.strip():
        return None, 'empty body'
    try:
        return json.loads(body), ''
    except Exception as e:
        return None, 'unparsed: %s' % str(e)[:50]


def measure(records, spec, engine):
    """The arithmetic. Everything downstream is formatting."""
    n = len(records)
    flats = [AK.flat(r) for r in records]
    whole = collections.Counter(canon(r) for r in records)
    ceiling = len(whole)

    res = dict(records=n, ceiling=ceiling, dup_identical=n - ceiling)

    if spec:
        keys = [key_of(f, spec) for f in flats]
        if any(k is None for k in keys):
            res['key_unmatched'] = sum(1 for k in keys if k is None)
        resolved = [k for k in keys if k is not None]
        res['key_distinct'] = len(set(resolved)) + (n - len(resolved))
        groups = collections.defaultdict(set)
        for k, r in zip(keys, records):
            if k is not None:
                groups[k].add(canon(r))
        res['key_groups_differing'] = sum(len(v) - 1 for v in groups.values()
                                          if len(v) > 1)
        # Report the differing fields FLATTENED, not at the top level. An
        # ArcGIS record is {attributes:{...}, geometry:{...}} and a GeoJSON
        # feature {properties:{...}, geometry:{...}}, so a top-level comparison
        # says "attributes" for every row in those fleets and names nothing a
        # reader could put in an id_keys. Flattened, it names the candidate.
        diffs = collections.Counter()
        for v in groups.values():
            if len(v) < 2:
                continue
            fa, fb = [AK.flat(json.loads(x)) for x in list(v)[:2]]
            for fk in set(fa) | set(fb):
                if fa.get(fk) != fb.get(fk):
                    diffs[fk] += 1
        res['differing_fields'] = [f for f, _ in diffs.most_common(6)]
    else:
        name, _ = fallback_key(flats[0], engine) if flats else (None, None)
        res['fallback_field'] = name or '(content hash)'
        if name:
            vals = [_val(f, name) for f in flats]
            res['fallback_distinct'] = len(set(vals))
        else:
            res['fallback_distinct'] = ceiling
    return res


def verdict(res, spec):
    n, ceil = res['records'], res['ceiling']
    if spec:
        kd = res['key_distinct']
        # A key that resolves on NO record is the defect audit_idkeys exists
        # for, and without this test it reads as KEY_IS_IDENTITY: every record
        # falls through to a content hash, so kd == n == the ceiling and the row
        # looks perfect. Found by breaking the resolver and watching the fixture
        # NOT change — the check that a passing fixture is actually testing
        # something.
        unm = res.get('key_unmatched', 0)
        if unm >= n:
            return ('KEY_UNMATCHED',
                    'the declared key resolves on NONE of the %d records, so '
                    'every uid is a content hash and the key is decorative. The '
                    'engine says so per run with a shape notice reading '
                    '`id_keys matched 0 of N`. Name a field the upstream sends '
                    '(tools/audit_idkeys.py finds these fleet-wide).' % n)
        if unm:
            return ('KEY_UNMATCHED',
                    'the declared key resolves on only %d of %d records; the '
                    'other %d fall through to a content hash, so this row has '
                    'two identity schemes at once and a revised record in the '
                    'hashed part lands as a new row.' % (n - unm, n, unm))
        if kd >= ceil:
            if res['dup_identical']:
                return ('KEY_IS_IDENTITY',
                        'reaches the ceiling (%d of %d records); the %d that '
                        'collapse are byte-identical upstream duplicates, which '
                        'a content hash would also collapse. KEEP. If this file '
                        'is a register of EVENTS whose columns are all '
                        'categorical, those %d may be distinct occurrences — '
                        'that is occurrence_rows and is the author\'s reading of '
                        'the published file, not something this count can see.'
                        % (kd, n, res['dup_identical'], res['dup_identical']))
            return ('KEY_IS_IDENTITY',
                    'every record distinct (%d of %d) and the key separates all '
                    'of them. KEEP.' % (kd, n))
        # NOT "loses ceil-kd records". The in-page collision guard
        # content-hashes same-key records that DIFFER inside one page/array
        # (lib/hpengine.c hp_collision_map, lib/jsonlist.c, lib/geojson.c
        # gj_collision_map), so they are stored, suffixed `|c:<hash>`, and the
        # run line shows a SMALL collision count rather than this gap —
        # FAA_AMD_PENDING_APRON reads 115 distinct of 2,000 here and the
        # registry sweep measured it losing 3. Reporting the gap as a loss
        # would send a reader after records that are in the database, which is
        # the opposite of this tool's job. The real cost is the PAGE SEAM, the
        # same finding awc-obstacles records: the guard's scope is one
        # response, so a colliding pair split across two pages stores bare in
        # one and suffixed in the other — and the uid then depends on which
        # page a record arrived in, which is INFLATION, invisible to the
        # emitted/stored arithmetic in both directions.
        return ('KEY_IS_DIMENSION',
                'the declared key gives %d distinct where distinct WHOLE records '
                'are %d, so it is a DIMENSION and not an identity (%d same-key '
                'groups hold records that differ%s). It is NOT losing %d a run: '
                'the in-page guard content-hashes the differing ones within each '
                'page, which is why the run line shows a much smaller number. '
                'The cost is at the PAGE SEAM — the guard sees one response, so '
                'a colliding pair split across pages stores twice and a record\'s '
                'uid depends on which page it arrived in. Widen the key until it '
                'reaches %d; if no field does, the content hash is the honest '
                'answer and CLAUDE.md records four Socrata rows reverted to it.'
                % (kd, ceil, res['key_groups_differing'],
                   (', differing in: %s' % ', '.join(res['differing_fields']))
                   if res.get('differing_fields') else '',
                   ceil - kd, ceil))
    fd = res['fallback_distinct']
    if res['fallback_field'] == '(content hash)':
        return ('NO_KEY_HASH_AT_CEILING',
                'no id_keys declared and no precedence field resolves, so every '
                'uid is a content hash: %d distinct, which IS the ceiling. '
                'Nothing is lost by count. Declaring a key can only help if it '
                'also reaches %d — measure it before declaring, because a key '
                'below the ceiling makes the row worse. What a hash costs is '
                'stability: a record the publisher REVISES arrives as a new row '
                'instead of updating the one it corrects.' % (fd, ceil))
    if fd >= ceil:
        return ('NO_KEY_HASH_AT_CEILING',
                'no id_keys declared; the engine falls back to `%s`, which is '
                '%d distinct and reaches the ceiling.'
                % (res['fallback_field'], fd))
    return ('NO_KEY_FALLBACK_LOSES',
            'no id_keys declared, and the engine\'s precedence list resolves '
            '`%s`, which is only %d distinct against a ceiling of %d — so this '
            'row silently loses %d a run to a field nobody chose. Declare an '
            'id_keys that reaches the ceiling.'
            % (res['fallback_field'], fd, ceil, ceil - fd))


def one(row, args):
    sid, engine = row['id'], row['engine']
    # --try-key measures a CANDIDATE instead of what the row declares, so an
    # author can compare two specs before editing a live row. It exists because
    # the alternative is editing, rebuilding and running the engine to find out
    # — which is how a key gets chosen off one page and a sibling's convention.
    spec = args.try_key if args.try_key else row['key']
    urls = page_urls(row['url'], args.pages)
    records, note = [], ''
    for u in urls:
        doc, err = fetch(u, args)
        if doc is None:
            if not records:
                return dict(row, verdict='UNMEASURED', detail=err, res={})
            note = 'page %d: %s' % (len(records) and urls.index(u) + 1, err)
            break
        arr = features_of(doc) if engine == 'vgeo' else None
        if arr is None:
            arr, _how = AK.pick_array(doc, row['path'])
        if not arr:
            if not records:
                return dict(row, verdict='NO_RECORDS',
                            detail='no array of objects in the body', res={})
            break
        recs = [x for x in arr if isinstance(x, dict)]
        if not recs:
            break
        records.extend(recs)
        if len(recs) < 2:
            break
    if not records:
        return dict(row, verdict='NO_RECORDS', detail='no records', res={})
    res = measure(records, spec, engine)
    v, detail = verdict(res, spec)
    if len(urls) == 1 and args.pages > 1:
        v = 'ONE_PAGE_ONLY' if v.startswith('KEY_IS') or v.startswith('NO_KEY') else v
        detail = ('the URL carries no offset/size pair this tool can advance, so '
                  'this is ONE page and "two pages can be too few" applies. '
                  + detail)
    if note:
        detail += '  [%s]' % note
    return dict(row, verdict=v, detail=detail, res=res)


# ───────────────────────── self-test (no network, no DB) ──────────────────────
def self_test():
    """The arithmetic and the two comparisons. Each fixture is a case this tool
    gets WRONG if a piece is missing, and the last two are controls."""
    bad = 0

    def check(label, got, want):
        nonlocal bad
        ok = got == want
        if not ok:
            bad += 1
        print('  %-4s %-58s got %-26s want %s'
              % ('ok' if ok else 'FAIL', label, got, want))

    # 1. A key that reaches the ceiling, with byte-identical upstream repeats.
    recs = [{'a': 1, 'b': 'x'}, {'a': 2, 'b': 'y'}, {'a': 2, 'b': 'y'}]
    r = measure(recs, 'a', 'hp')
    check('ceiling counts distinct WHOLE records', r['ceiling'], 2)
    check('byte-identical repeats counted', r['dup_identical'], 1)
    check('key at the ceiling -> KEEP', verdict(r, 'a')[0], 'KEY_IS_IDENTITY')

    # 2. A key that is a DIMENSION: same key, records differ. The finding.
    recs = [{'country': 'NA', 'yr': 2020}, {'country': 'NA', 'yr': 2021},
            {'country': 'BW', 'yr': 2020}]
    r = measure(recs, 'country', 'hp')
    check('dimension: ceiling', r['ceiling'], 3)
    check('dimension: key distinct', r['key_distinct'], 2)
    check('dimension -> KEY_IS_DIMENSION', verdict(r, 'country')[0], 'KEY_IS_DIMENSION')
    check('names the differing field', 'yr' in r['differing_fields'], True)

    # 3. `+` COMPOSES and `,` CHOOSES — getting this backwards fails silently,
    #    and it is the most repeated mistake in CLAUDE.md's history.
    recs = [{'y': 2020, 'q': 1}, {'y': 2020, 'q': 2}]
    check('a+b composes', measure(recs, 'y+q', 'hp')['key_distinct'], 2)
    check('a,b chooses the first that resolves',
          measure(recs, 'y,q', 'hp')['key_distinct'], 1)

    # 4. hp_flat_get's last-dotted-segment fallback. Without it this tool
    #    indicts most of the ArcGIS and GeoJSON fleet (audit_idkeys' own first
    #    run: 35 reported, 30 its own fault).
    recs = [{'attributes': {'iata_code': 'RFN'}},
            {'attributes': {'iata_code': 'YOA'}}]
    r = measure(recs, 'iata_code', 'hp')
    check('nested key resolves by last segment', r['key_distinct'], 2)
    # key_distinct ALONE does not discriminate here: without the resolver the
    # key matches nothing, every record falls to a content hash, and the count
    # is 2 either way for the opposite reason. `key_unmatched` is what the
    # resolver buys, so that is what the fixture asserts.
    check('...and nothing was left unmatched', r.get('key_unmatched', 0), 0)
    r_broken = dict(r, key_unmatched=2)
    check('a key matching NO record is not called an identity',
          verdict(r_broken, 'iata_code')[0], 'KEY_UNMATCHED')

    # 5. No declared key, nothing on the precedence list -> content hash, which
    #    IS the ceiling. The comparison people skip.
    recs = [{'entidad': 'DANE', 'v': 1}, {'entidad': 'DANE', 'v': 2}]
    r = measure(recs, '', 'vjson')
    check('unkeyed: no precedence field -> hash', r['fallback_field'], '(content hash)')
    check('unkeyed hash reaches the ceiling',
          verdict(r, '')[0], 'NO_KEY_HASH_AT_CEILING')

    # 6. No declared key but the precedence list DOES resolve, onto a dimension.
    recs = [{'code': 'A', 'v': 1}, {'code': 'A', 'v': 2}, {'code': 'B', 'v': 3}]
    r = measure(recs, '', 'vjson')
    check('unkeyed fallback field found', r['fallback_field'], 'code')
    check('fallback below ceiling -> finding',
          verdict(r, '')[0], 'NO_KEY_FALLBACK_LOSES')

    # 7. Paging: the offset is advanced the way the engine advances it, and a
    #    URL with nothing to advance yields ONE page rather than an invented
    #    parameter.
    check('offset appended when absent',
          page_urls('https://h/x?$limit=500', 2)[1],
          'https://h/x?$limit=500&$offset=500')
    check('existing offset advanced, not appended',
          page_urls('https://h/x?$limit=500&$offset=0', 2)[1],
          'https://h/x?$limit=500&$offset=500')
    check('CONTROL: no size param -> a single page',
          len(page_urls('https://h/x?q=1', 2)), 1)

    # 8. CONTROL: a key equal to the ceiling on a body with NO duplicates must
    #    not be reported as a finding, or every healthy row enters a defect
    #    bucket — the POD / GATED / WINDOW_EMPTY correction a fifth time.
    recs = [{'id': 1}, {'id': 2}, {'id': 3}]
    check('CONTROL: healthy row is not a finding',
          verdict(measure(recs, 'id', 'hp'), 'id')[0], 'KEY_IS_IDENTITY')

    # 9. CONTROL: the macro reader must find real rows in the real tree, or a
    #    clean sweep could mean "nothing was read".
    found = len(rows_from_macros('lat-co-'))
    check('CONTROL: macro reader sees lat-co- rows', found > 5, True)

    print('self-test: %s' % ('%d FAILURES' % bad if bad else 'ok'))
    return 1 if bad else 0


def main():
    if '--self-test' in sys.argv[1:]:
        return self_test()
    ap = argparse.ArgumentParser(
        description='Measure whether a row\'s id_keys is the record identity, '
                    'and whether it beats the content hash it replaces.')
    ap.add_argument('--out')
    ap.add_argument('--match', default='')
    ap.add_argument('--only', default='', help='comma-separated ids')
    ap.add_argument('--ids-file', default='')
    ap.add_argument('--pages', type=int, default=2,
                    help='pages to read per row (default 2; one page is not a '
                         'measurement)')
    ap.add_argument('--jobs', type=int, default=3)
    ap.add_argument('--host-gap-ms', type=int, default=1000)
    ap.add_argument('--timeout', type=int, default=120)
    ap.add_argument('--max-bytes', type=int, default=80_000_000)
    ap.add_argument('--try-key', default='',
                    help='measure THIS id_keys spec instead of the one the row \n                          declares, to compare candidates before editing')
    ap.add_argument('--self-test', action='store_true')
    args = ap.parse_args()

    want = set()
    if args.only:
        want |= {x.strip() for x in args.only.split(',') if x.strip()}
    if args.ids_file:
        want |= {l.strip() for l in open(args.ids_file) if l.strip()}
    rows = all_rows(args.match)
    if want:
        rows = [r for r in rows if r['id'] in want]
        missing = want - {r['id'] for r in rows}
        if missing:
            print('not found as an HP_JSON/VJSON/VGEO row with a literal URL: %s'
                  % ', '.join(sorted(missing)), file=sys.stderr)
    print('%d row(s) to measure, %d page(s) each' % (len(rows), args.pages),
          file=sys.stderr)

    res = []
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for out in ex.map(lambda r: one(r, args), rows):
            res.append(out)
            print('%-56s %-24s %s' % (out['id'], out['verdict'],
                                      out['detail'][:96]), flush=True)
    if args.out:
        cols = ['id', 'file', 'engine', 'verdict', 'key', 'records', 'ceiling',
                'key_distinct', 'fallback_distinct', 'dup_identical', 'detail']
        with open(args.out, 'w', encoding='utf-8', newline='\n') as fh:
            fh.write('\t'.join(cols) + '\n')
            for x in res:
                r = x.get('res', {})
                row = [x['id'], x['file'], x['engine'], x['verdict'],
                       args.try_key or x['key'],
                       r.get('records', ''), r.get('ceiling', ''),
                       r.get('key_distinct', ''), r.get('fallback_distinct', ''),
                       r.get('dup_identical', ''), x['detail']]
                fh.write('\t'.join(str(c).replace('\t', ' ').replace('\n', ' ')
                                   for c in row) + '\n')
    c = collections.Counter(x['verdict'] for x in res)
    print('', file=sys.stderr)
    for v, n in c.most_common():
        print('%6d %s' % (n, v), file=sys.stderr)
    # A finding is a property of the row; a transport failure is a property of
    # this path and is NOT an exit code (audit_idkeys' own rule).
    return 1 if (c.get('KEY_IS_DIMENSION') or c.get('NO_KEY_FALLBACK_LOSES')
                or c.get('KEY_UNMATCHED')) else 0


if __name__ == '__main__':
    sys.exit(main())
