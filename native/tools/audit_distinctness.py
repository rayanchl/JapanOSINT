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
NO_KEY_FALLBACK_AT_CEILING,
NO_KEY_FALLBACK_IS_DIMENSION (the other finding), UPSTREAM_DUPLICATES, ONE_PAGE_ONLY,
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
PAGERS = [('$limit', '$offset', 'offset'), ('limit', 'offset', 'offset'),
          ('rows', 'start', 'offset'), ('count', 'start', 'offset'),
          ('resultRecordCount', 'resultOffset', 'offset'),
          ('maxFeatures', 'startIndex', 'offset'), ('$top', '$skip', 'offset'),
          ('_limit', '_offset', 'offset'),
          # A page NUMBER advances by one, not by the size. Treating these as
          # offsets asks for page 101 as the second request — the
          # page-size-on-page-number defect `make lint-sources` exists for, and
          # this tool's first version had it.
          ('per_page', 'page', 'page'), ('page_size', 'page', 'page'),
          ('pageSize', 'page', 'page'), ('pagesize', 'page', 'page'),
          ('perpage', 'page', 'page'), ('perPage', 'page', 'page'),
          ('c', 'sp', 'page')]

# Where a server publishes its own next link. Checked after every page when the
# URL itself gave nothing to advance (lib/jsonlist.c next_link's list).
NEXT_PATHS = ['next', 'links.next', 'links.next.href', 'meta.next',
              'meta.nextLink', '@odata.nextLink', 'pagination.next',
              'next_page', 'nextLink']

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
        elif c == '/' and text[i:i + 2] == '/*':
            # This tree comments INSIDE argument lists, and a comment holds
            # commas, parentheses and apostrophes: gen_detail_hop_upgrade.py
            # parsed 86 rows as 15 arguments for want of this, and this tool
            # read 19 rows as NOT_READABLE the hour a note was written into one.
            j = text.find('*/', i + 2)
            i = len(text) if j < 0 else j + 2
            continue
        elif c == '/' and text[i:i + 2] == '//':
            j = text.find('\n', i)
            i = len(text) if j < 0 else j
            continue
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

            def iopt(name, blk=blk):
                mm = re.search(r'\.%s\s*=\s*(\d+)' % name, blk)
                return int(mm.group(1)) if mm else 0

            mode = re.search(r'\.mode\s*=\s*(HP_[A-Z]+)', blk)
            url = opt('url')
            if not url.startswith('http'):
                continue
            # HP_JSON is enum 0, so a row that declares no .mode IS a JSON
            # row — and requiring the word dropped every such row unread
            # (the whole hp16/hp17 generated fleet), silently.
            mname = mode.group(1) if mode else 'HP_JSON'
            if mname not in READ_MODES:
                continue
            if AK.ENTITY_TOKEN.search(url):
                continue
            psz = re.search(r'\.page_size\s*=\s*(\d+)', blk)
            out.append(dict(id=sid, file=os.path.basename(f),
                            key=opt('id_keys'), path=opt('array_path'),
                            url=url, engine='hp',
                            page_param=opt('page_param'),
                            page_size=int(psz.group(1)) if psz else 0,
                            next_path=opt('next_path'), mode=mname,
                            csv_delim=opt('csv_delim'),
                            csv_comment=opt('csv_comment'),
                            csv_skip=iopt('csv_skip_lines'),
                            csv_no_header=iopt('csv_no_header'),
                            occurrence=iopt('occurrence_rows')))
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


# VJSON's fallback uid when no id resolves is NOT a hash of the record. It is
# hash(title, link, date) — lib/jsonlist.c row_hash — and the whole record joins
# the hash ONLY when the in-page guard flagged a collision. So across pages, two
# records that differ but share those three values collapse. This tool's first
# version called every unkeyed row "content hash, which IS the ceiling"; for
# VJSON and hpengine that was false, and it was found the usual way: a dataset
# read whole in one request held 216 rows and 216 distinct whole records, and
# the row still collapsed six.
VJ_TITLE = ["title", "name", "headline", "subject", "label", "event_name",
            "common_name", "display_name", "full_name", "short_name", "station",
            "site", "location", "place", "area", "region", "country_name",
            "country", "indicator", "event", "nome", "nom", "nombre", "naam",
            "navn", "namn", "nimi", "nazwa", "nazev", "n\u00e1zev", "denumire",
            "naziv", "titulo", "t\u00edtulo", "titre", "titel", "tytul",
            "summary", "description", "text"]
VJ_LINK = ["url", "link", "href", "permalink", "web_url", "uri", "detail"]
VJ_DATE = ["published_at", "published", "pubDate", "pub_date",
           "date_published", "updated_at", "updated", "created_at", "datetime",
           "date", "timestamp", "acq_date", "obs_date", "observed_at",
           "observed", "time_tag", "time-tag", "captured_at", "measured_at",
           "start_time", "event_time", "seen", "time", "last_modified"]


def _top_pick(rec, names):
    """lib/jsonlist.c pick(): a TOP-LEVEL key, exact then case-insensitive,
    whose value is a scalar. Nested values are skipped and the walk continues."""
    if not isinstance(rec, dict):
        return None
    low = {k.lower(): k for k in rec}
    for n in names:
        k = n if n in rec else low.get(n.lower())
        if k is None:
            continue
        v = rec[k]
        if isinstance(v, (str, int, float, bool)) and v != '':
            return str(v)
    return None


def _first_scalar(rec, skip_underscore=False):
    if not isinstance(rec, dict):
        return ''
    for k, v in rec.items():
        if skip_underscore and k.startswith('_'):
            continue
        if isinstance(v, (str, int, float, bool)) and v != '':
            return str(v)
    return ''


def effective_fallback(rec, flat_fields, engine):
    """(what the engine keys on, the value) for a row declaring no id_keys."""
    if engine == 'vjson':
        rid = _top_pick(rec, FALLBACK['vjson'])
        if rid is not None:
            return 'the K_ID list', ('id', rid)
        title = _top_pick(rec, VJ_TITLE) or _first_scalar(rec)
        return ('hash(title, link, date)',
                (title, _top_pick(rec, VJ_LINK) or '', _top_pick(rec, VJ_DATE) or ''))
    if engine == 'vgeo':
        props = rec.get('properties') if isinstance(rec.get('properties'), dict) else {}
        for n in FALLBACK['vgeo']:
            if props.get(n) not in (None, ''):
                return 'properties.%s' % n, ('p', str(props[n]))
        if rec.get('id') not in (None, ''):
            return 'the Feature id', ('fid', str(rec['id']))
        return 'hash(geometry, properties)', ('h', canon(rec))
    # hpengine: ID_FALLBACK through hp_flat_get, else the record's first scalar
    # (CLAUDE.md, the Web NDL Authorities case: "the no-id_keys fallback ends at
    # the record's FIRST SCALAR").
    for n in FALLBACK['hp']:
        if AK.present(n, flat_fields):
            return 'ID_FALLBACK `%s`' % n, ('f', _val(flat_fields, n))
    return "the record's first scalar", ('s', _first_scalar(rec, True))


def features_of(doc):
    """VGEO reads a FeatureCollection; its records are the features."""
    if isinstance(doc, dict) and isinstance(doc.get('features'), list):
        return doc['features']
    return None


def _set_param(url, name, value):
    pat = r'([?&]%s=)[^&]*' % re.escape(name)
    if re.search(pat, url):
        return re.sub(pat, lambda mm: mm.group(1) + str(value), url)
    return url + ('&' if '?' in url else '?') + '%s=%s' % (name, value)


def _get_param(url, name):
    m = re.search(r'[?&]%s=(\d+)' % re.escape(name), url)
    return int(m.group(1)) if m else None


def page_urls(url, pages, row=None):
    """The row's URL, then the same URL advanced the way the ENGINE advances it.
    An hp row's own page_param wins (with page_size it is an offset, without it
    a page number — lib/hpengine.h); else a known size/cursor pair in the URL.
    Returns one URL when nothing can be moved; the caller then tries the
    server's own next link before settling for ONE_PAGE_ONLY."""
    urls = [url]
    row = row or {}
    pp = row.get('page_param')
    if pp:
        stride = row.get('page_size') or 0
        cur = _get_param(url, pp)
        for p in range(1, pages):
            if stride:
                urls.append(_set_param(url, pp, (cur or 0) + stride * p))
            else:
                urls.append(_set_param(url, pp, (cur if cur is not None else 1) + p))
        return urls
    for size_p, cur_p, kind in PAGERS:
        size = _get_param(url, size_p)
        if not size:
            continue
        cur = _get_param(url, cur_p)
        for p in range(1, pages):
            if kind == 'offset':
                urls.append(_set_param(url, cur_p, (cur or 0) + size * p))
            else:
                urls.append(_set_param(url, cur_p, (cur if cur is not None else 1) + p))
        return urls
    # A bare page cursor with no size beside it (ROR's ?page=1).
    cur = _get_param(url, 'page')
    if cur is not None:
        for p in range(1, pages):
            urls.append(_set_param(url, 'page', cur + p))
    return urls


def next_link(doc, row):
    """The server's own next-page URL, or ''. The hp row's next_path is tried
    first when it is a plain dotted path; `$last` cursors need a template and
    are not followed here."""
    cands = list(NEXT_PATHS)
    np_ = (row or {}).get('next_path') or ''
    if np_ and '$' not in np_ and '=' not in np_:
        cands.insert(0, np_)
    for path in cands:
        node = doc
        if isinstance(doc, dict) and path in doc:
            node = doc[path]
        else:
            for seg in path.split('.'):
                node = node.get(seg) if isinstance(node, dict) else None
                if node is None:
                    break
        if isinstance(node, dict):
            node = node.get('href')
        if isinstance(node, str) and node.startswith('http'):
            return node
    links = doc.get('links') if isinstance(doc, dict) else None
    if isinstance(links, list):
        for l in links:
            if isinstance(l, dict) and l.get('rel') == 'next' and \
                    str(l.get('href', '')).startswith('http'):
                return l['href']
    return ''


def fetch_raw(url, args):
    AK.paced(url.split('/')[2], args.host_gap_ms / 1000.0)
    r = subprocess.run(['curl', '-sSgL', '--compressed', '--max-time', str(args.timeout),
                        '--max-filesize', str(args.max_bytes), '-A', AK.UA,
                        '-w', '\n%{http_code}', url],
                       capture_output=True)
    if r.returncode != 0:
        return None, r.stderr.decode('utf-8', 'replace').strip()[:70] or 'curl failed'
    b = r.stdout
    i = b.rfind(b'\n')
    code = b[i + 1:].decode('ascii', 'replace')
    b = b[:i]
    if not code.startswith('2'):
        return None, 'HTTP %s' % code
    if not b.strip():
        return None, 'empty body'
    return b, ''


def fetch(url, args):
    b, err = fetch_raw(url, args)
    if b is None:
        return None, err
    try:
        return json.loads(b.decode('utf-8', 'replace')), ''
    except Exception as e:
        return None, 'unparsed: %s' % str(e)[:50]


# ── CSV and XML (hpengine HP_CSV / HP_XML) ────────────────────────────────────
# The first version of this tool read JSON only, and its row reader dropped
# every other mode without a word — so "the tool measured the queue" was true of
# what it could open and silent about the rest, the shape CLAUDE.md keeps
# correcting in tools. These two readers mirror what the engine does to a body
# BEFORE the key is resolved; the arithmetic after that is the same.
READ_MODES = ('HP_JSON', 'HP_CSV', 'HP_XML')
_DELIMS = {'': ',', 'comma': ',', 'tab': '\t', 'pipe': '|', 'semi': ';'}


def csv_delim_of(name):
    """lib/hpengine.c's resolver: a named delimiter, else ONE literal
    character. `ws` and `lit:<token>` are not modelled and return None, so the
    row reads UNMEASURED rather than being parsed on a guess."""
    if name in _DELIMS:
        return _DELIMS[name]
    if len(name) == 1:
        return name
    return None


def parse_csv(raw, row):
    """-> (records, err). Records are dicts in column order, as hp flattens a
    CSV line: header names, or col0..colN under csv_no_header."""
    import csv, io
    delim = csv_delim_of(row.get('csv_delim', ''))
    if delim is None:
        return None, 'csv_delim %r is not modelled by this tool' % row.get('csv_delim')
    try:
        text = raw.decode('utf-8-sig')
    except UnicodeDecodeError:
        try:
            text = raw.decode('cp932')
        except UnicodeDecodeError:
            text = raw.decode('utf-8', 'replace')
    lines = text.splitlines(True)
    com = row.get('csv_comment', '')
    if com:
        lines = [l for l in lines if not l.startswith(com)]
    lines = lines[row.get('csv_skip', 0):]
    rd = list(csv.reader(io.StringIO(''.join(lines)), delimiter=delim,
                         skipinitialspace=True))
    rd = [r for r in rd if any(c.strip() for c in r)]
    if not rd:
        return [], ''
    if row.get('csv_no_header'):
        hdr, data = None, rd
    else:
        hdr, data = [h.strip() for h in rd[0]], rd[1:]
    out = []
    for r in data:
        rec = {}
        for i, c in enumerate(r):
            name = hdr[i] if hdr and i < len(hdr) and hdr[i] else 'col%d' % i
            rec[name] = c.strip()
        out.append(rec)
    return out, ''


def _xml_local(tag):
    return tag.rsplit('}', 1)[-1] if isinstance(tag, str) else ''


def _xml_flatten(el, prefix, out):
    """hp_xml_flatten's keyspace: child elements dotted, a repeated child
    indexed (`name`, `name.1`), an attribute as `@attr`, and text beside
    children as `#text`. Namespace prefixes are dropped — see xml_spec()."""
    for a, v in el.attrib.items():
        k = (prefix + '.' if prefix else '') + '@' + _xml_local(a)
        out.setdefault(k, v.strip())
    kids = list(el)
    text = (el.text or '').strip()
    if not kids:
        if prefix and text:
            out.setdefault(prefix, text)
        return
    if text:
        out.setdefault((prefix + '.' if prefix else '') + '#text', text)
    seen = {}
    for c in kids:
        n = _xml_local(c.tag)
        if not n:
            continue
        i = seen.get(n, 0)
        seen[n] = i + 1
        k = (prefix + '.' if prefix else '') + n + ('.%d' % i if i else '')
        _xml_flatten(c, k, out)


def parse_xml(raw, row):
    """-> (records, err). The record element is the row's array_path (its last
    segment), else `item`, else `entry` — RSS and Atom, which is what an
    undeclared HP_XML row in this tree reads."""
    import xml.etree.ElementTree as ET
    try:
        root = ET.fromstring(raw)
    except Exception as e:
        return None, 'xml unparsed: %s' % str(e)[:50]
    want = (row.get('path') or '').split('.')[-1].split(':')[-1]
    names = [want] if want else ['item', 'entry']
    for n in names:
        els = [e for e in root.iter() if _xml_local(e.tag) == n]
        if els:
            recs = []
            for e in els:
                d = {}
                _xml_flatten(e, '', d)
                if d:
                    recs.append(d)
            return recs, ''
    return [], ''


def xml_spec(spec):
    """KNOWN LIMIT, found 2026-11-23 on FPDS: the ENGINE keeps a namespace
    prefix in every flattened segment (`content.ns1:award.ns1:awardID…`) and
    matches a dotted token exactly, while this reader cannot see prefixes at
    all (ElementTree resolves them away). So a dotted spec written WITHOUT the
    prefix reads as resolving here and matches 0 records in the engine. On a
    prefixed feed, confirm any key in the engine and read its
    id-keys-unmatched notice before believing this tool.

    An id_keys token may name a prefixed element (`dc:identifier`). This
    reader drops prefixes, so the token loses its own the same way — otherwise
    every such row would read KEY_UNMATCHED for a reason that is this tool's."""
    return re.sub(r'(^|[+,.])\s*[A-Za-z_][\w-]*:(?=[A-Za-z_])', r'\1', spec or '')


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
        eff = [effective_fallback(r, f, engine) for r, f in zip(records, flats)]
        res['fallback_field'] = collections.Counter(
            how for how, _ in eff).most_common(1)[0][0] if eff else ''
        res['fallback_distinct'] = len({v for _, v in eff})
        groups = collections.defaultdict(set)
        for (_, v), r in zip(eff, records):
            groups[v].add(canon(r))
        res['key_groups_differing'] = sum(len(g) - 1 for g in groups.values()
                                          if len(g) > 1)
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
    fd, how = res['fallback_distinct'], res['fallback_field']
    if fd >= ceil:
        return ('NO_KEY_FALLBACK_AT_CEILING',
                'no id_keys declared; the engine keys on %s, which is %d '
                'distinct and reaches the ceiling over these pages. Nothing is '
                'lost here. That is a reading of THESE pages — a fallback is a '
                'key nobody chose, so it is worth one --try-key against a field '
                'the publisher calls an identifier.' % (how, fd))
    return ('NO_KEY_FALLBACK_IS_DIMENSION',
            'no id_keys declared, and what the engine keys on instead — %s — is '
            'only %d distinct against a ceiling of %d (%d groups hold records '
            'that DIFFER). The in-page guard keeps differing records that '
            'collide inside ONE page, so the run line shows less than this gap; '
            'the loss is ACROSS pages, where two different records sharing that '
            'value collapse to one and nothing reports it. Declare an id_keys '
            'that reaches %d (--try-key measures a candidate).'
            % (how, fd, ceil, res.get('key_groups_differing', 0), ceil))


def one(row, args):
    sid, engine = row['id'], row['engine']
    # --try-key measures a CANDIDATE instead of what the row declares, so an
    # author can compare two specs before editing a live row. It exists because
    # the alternative is editing, rebuilding and running the engine to find out
    # — which is how a key gets chosen off one page and a sibling's convention.
    spec = args.try_key if args.try_key else row['key']
    urls = page_urls(row['url'], args.pages, row)
    records, note, pages_read = [], '', 0
    queue, seen_urls = list(urls), set()
    while queue and pages_read < args.pages:
        u = queue.pop(0)
        if u in seen_urls:
            break
        seen_urls.add(u)
        mode = row.get('mode', 'HP_JSON')
        if mode in ('HP_CSV', 'HP_XML'):
            raw, err = fetch_raw(u, args)
            doc = None
            arr = None
            if raw is not None:
                arr, err = (parse_csv if mode == 'HP_CSV' else parse_xml)(raw, row)
            if arr is None:
                if not records:
                    return dict(row, verdict='UNMEASURED', detail=err, res={})
                note = 'page %d: %s' % (pages_read + 1, err)
                break
            if records and arr and len(arr) == len(records[-len(arr):]) and \
                    [canon(x) for x in arr] == [canon(x) for x in records[-len(arr):]]:
                break            # the server re-served the page: not a second page
        else:
            doc, err = fetch(u, args)
            if doc is None:
                if not records:
                    return dict(row, verdict='UNMEASURED', detail=err, res={})
                note = 'page %d: %s' % (pages_read + 1, err)
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
        pages_read += 1
        if len(urls) == 1 and not queue and doc is not None:
            nl = next_link(doc, row)
            if nl:
                queue.append(nl)
    if not records:
        return dict(row, verdict='NO_RECORDS', detail='no records', res={})
    if row.get('mode') == 'HP_XML':
        spec = xml_spec(spec)
    res = measure(records, spec, engine)
    v, detail = verdict(res, spec)
    whole_file = row.get('mode') == 'HP_CSV' or \
        (row.get('mode') == 'HP_XML' and not row.get('page_param') and not row.get('next_path'))
    if row.get('occurrence') and res.get('dup_identical'):
        detail += ('  [occurrence_rows=1: the %d byte-identical repeat(s) are '
                   'KEPT by the engine, keyed on their occurrence number]'
                   % res['dup_identical'])
    if pages_read == 1 and whole_file:
        detail = 'one request is the whole file. ' + detail
    elif pages_read == 1 and len(records) > 1 and args.pages > 1 and not note \
            and len(urls) == 1:
        v = 'ONE_PAGE_ONLY' if v in ('KEY_IS_IDENTITY', 'NO_KEY_FALLBACK_AT_CEILING') else v
        detail = ('neither the URL nor the body gave this tool a next page, so '
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

    # 5. THE FIXTURE THIS TOOL'S FIRST VERSION GOT WRONG. An unkeyed VJSON row
    #    does not hash the record: it hashes (title, link, date). Two records
    #    that differ only in a field outside those three share a uid.
    recs = [{'title': 'A', 'url': 'u', 'territory': 'north'},
            {'title': 'A', 'url': 'u', 'territory': 'south'}]
    r = measure(recs, '', 'vjson')
    check('VJSON fallback is hash(title, link, date)', r['fallback_field'],
          'hash(title, link, date)')
    check('...so records differing elsewhere COLLIDE', r['fallback_distinct'], 1)
    check('...and that is a finding, not "at the ceiling"',
          verdict(r, '')[0], 'NO_KEY_FALLBACK_IS_DIMENSION')
    recs = [{'title': 'A', 'url': 'u1'}, {'title': 'B', 'url': 'u2'}]
    check('CONTROL: distinct triples reach the ceiling',
          verdict(measure(recs, '', 'vjson'), '')[0], 'NO_KEY_FALLBACK_AT_CEILING')

    # 6. VJSON's K_ID list is TOP-LEVEL only (pick()), unlike hp_flat_get.
    recs = [{'code': 'A', 'v': 1}, {'code': 'A', 'v': 2}, {'code': 'B', 'v': 3}]
    r = measure(recs, '', 'vjson')
    check('VJSON K_ID field found at top level', r['fallback_field'], 'the K_ID list')
    check('a K_ID dimension is a finding',
          verdict(r, '')[0], 'NO_KEY_FALLBACK_IS_DIMENSION')
    recs = [{'attributes': {'id': 1}, 'n': 'x'}, {'attributes': {'id': 2}, 'n': 'x'}]
    check('VJSON does NOT see a nested id (hp would)',
          measure(recs, '', 'vjson')['fallback_field'], 'hash(title, link, date)')
    check('hpengine DOES, through hp_flat_get',
          measure(recs, '', 'hp')['fallback_field'], 'ID_FALLBACK `id`')

    # 6b. An unkeyed hp row with nothing on ID_FALLBACK keys on its FIRST
    #     SCALAR (the Web NDL Authorities case: @context on every record).
    recs = [{'@context': 'c', 'label': 'x'}, {'@context': 'c', 'label': 'y'}]
    r = measure(recs, '', 'hp')
    check('hp falls to the first scalar', r['fallback_field'], "the record's first scalar")
    check('...which collapses a constant-first-field API', r['fallback_distinct'], 1)

    # 6c. VGEO's last resort really IS a whole-feature hash, so it is the one
    #     engine where "the fallback reaches the ceiling" holds by construction.
    recs = [{'type': 'Feature', 'geometry': None, 'properties': {'a': 1}},
            {'type': 'Feature', 'geometry': None, 'properties': {'a': 2}}]
    check('CONTROL: VGEO hash fallback reaches the ceiling',
          verdict(measure(recs, '', 'vgeo'), '')[0], 'NO_KEY_FALLBACK_AT_CEILING')

    # 7. Paging: the offset is advanced the way the engine advances it, and a
    #    URL with nothing to advance yields ONE page rather than an invented
    #    parameter.
    check('offset appended when absent',
          page_urls('https://h/x?$limit=500', 2)[1],
          'https://h/x?$limit=500&$offset=500')
    check('existing offset advanced, not appended',
          page_urls('https://h/x?$limit=500&$offset=0', 2)[1],
          'https://h/x?$limit=500&$offset=500')
    check('a page NUMBER advances by one, not by the size',
          page_urls('https://h/x?per_page=100&page=1', 2)[1],
          'https://h/x?per_page=100&page=2')
    check('an hp row page_param with page_size is an offset',
          page_urls('https://h/x?a=1', 2, dict(page_param='$offset', page_size=1000))[1],
          'https://h/x?a=1&$offset=1000')
    check('an hp row page_param without page_size is a page number',
          page_urls('https://h/x?a=1', 2, dict(page_param='sp'))[1],
          'https://h/x?a=1&sp=2')
    check('the server next link is followed',
          next_link({'next': 'https://h/x?page=2', 'results': []}, {}),
          'https://h/x?page=2')
    check('a links[] rel=next array is followed',
          next_link({'links': [{'rel': 'self', 'href': 'https://h/a'},
                               {'rel': 'next', 'href': 'https://h/b'}]}, {}),
          'https://h/b')
    check('CONTROL: no next link -> empty', next_link({'results': []}, {}), '')
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

    # 10. CSV. A pipe-delimited body read as comma puts the whole line in ONE
    #     field — the FDSN and `csv_delim = "comma"` mistakes CLAUDE.md records
    #     — and a key then matches nothing while the record count looks fine.
    body = b'# banner\nnet|sta|lat\nIM|AB01|1.0\nIM|AB02|2.0\nIU|AB01|3.0\n'
    recs, _e = parse_csv(body, {'csv_delim': 'pipe', 'csv_comment': '#'})
    check('csv: pipe + comment -> named fields', sorted(recs[0]), ['lat', 'net', 'sta'])
    r = measure(recs, 'sta', 'hp')
    check('csv: a station code is a DIMENSION across networks',
          verdict(r, 'sta')[0], 'KEY_IS_DIMENSION')
    check('csv: net+sta is the identity',
          verdict(measure(recs, 'net+sta', 'hp'), 'net+sta')[0], 'KEY_IS_IDENTITY')
    wrong, _e = parse_csv(body, {'csv_delim': '', 'csv_comment': '#'})
    check('csv: the same body as COMMA is one field (the trap)', len(wrong[0]), 1)
    nh, _e = parse_csv(b'a,b\nc,d\n', {'csv_no_header': 1})
    check('csv: no header -> col0..colN, first line is DATA',
          (len(nh), sorted(nh[0])), (2, ['col0', 'col1']))
    sk, _e = parse_csv(b'title block\n\nk,v\n1,2\n', {'csv_skip': 1})
    check('csv: csv_skip_lines drops the title block', sorted(sk[0]), ['k', 'v'])
    check('csv: an unmodelled delimiter is refused, not guessed',
          parse_csv(b'a b\n', {'csv_delim': 'ws'})[0], None)
    sj, _e = parse_csv('\u7f6a\u540d,\u65e5\n\u7a83\u76d7,1\n'.encode('cp932'), {})
    check('csv: a Shift_JIS file is decoded, not mangled', sorted(sj[0])[0], '\u65e5')

    # 11. XML. Records by element name, repeated children indexed, attributes
    #     in the same keyspace — and a prefixed key token must still resolve.
    xml = (b'<rss xmlns:dc="http://purl.org/dc/elements/1.1/"><channel>'
           b'<item><title>a</title><guid>g1</guid><category>x</category>'
           b'<category>y</category><dc:identifier>d1</dc:identifier></item>'
           b'<item><title>b</title><guid>g1</guid><dc:identifier>d2</dc:identifier></item>'
           b'</channel></rss>')
    recs, _e = parse_xml(xml, {'path': ''})
    check('xml: items found without a declared path', len(recs), 2)
    check('xml: a repeated child is indexed, not overwritten',
          recs[0].get('category.1'), 'y')
    check('xml: a guid shared by two different items -> DIMENSION',
          verdict(measure(recs, 'guid', 'hp'), 'guid')[0], 'KEY_IS_DIMENSION')
    sp = xml_spec('dc:identifier')
    r = measure(recs, sp, 'hp')
    check('xml: a prefixed token resolves (not KEY_UNMATCHED)',
          (verdict(r, sp)[0], r.get('key_unmatched', 0)), ('KEY_IS_IDENTITY', 0))
    recs, _e = parse_xml(b'<r><Channel code="BHZ"><Lat>1</Lat></Channel>'
                         b'<Channel code="BHN"><Lat>1</Lat></Channel></r>',
                         {'path': 'Channel'})
    # key_distinct alone does NOT discriminate here (checked by removing the
    # attribute reader: the key then matches nothing and the count is still 2,
    # for the opposite reason) — the field itself has to be asserted.
    check('xml: the record element\'s own attribute is a field',
          [x.get('@code') for x in recs], ['BHZ', 'BHN'])
    check('xml: ...and `code` resolves to it, as hp_flat_get skips the @',
          measure(recs, 'code', 'hp').get('key_unmatched', 0), 0)
    check('xml: a body that is not XML is refused',
          parse_xml(b'<html><body>Just a moment', {})[0], None)

    # 11b. A comment INSIDE a macro's argument list must not split it.
    src = 'VJSON(f, "x", "n", /* a note, with (parens), commas and an apostrophe\'s */ "u")'
    args, _end = _args_of(src, src.index('('))
    check('macro args: a comment inside the list is skipped',
          (len(args), _lit(args[3])), (4, 'u'))

    # 12. CONTROL: an hp row that declares no .mode is HP_JSON (enum 0) and
    #     must be read. Confirmed to fail against the reader that required
    #     the word.
    check('CONTROL: hp reader sees a row with no .mode',
          any(r['id'] == 'af-dhs-data-gh' for r in rows_from_hp('af-dhs-data-gh')), True)

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
    else:
        missing = set()
    print('%d row(s) to measure, %d page(s) each' % (len(rows), args.pages),
          file=sys.stderr)

    res = []
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for out in ex.map(lambda r: one(r, args), rows):
            res.append(out)
            print('%-56s %-24s %s' % (out['id'], out['verdict'],
                                      out['detail'][:96]), flush=True)
    # An id this tool could not open is a ROW IN THE OUTPUT, never a line on
    # stderr: "skipped" and "checked and clean" must not look alike.
    for sid in sorted(missing):
        res.append(dict(id=sid, file='', engine='', key='', verdict='NOT_READABLE',
                        detail='not an HP_JSON/HP_CSV/HP_XML/VJSON/VGEO row with a '
                               'literal GET URL (a bespoke collector, VRSS/RSSX, VCSV, '
                               'VJSON_PREP, HP_XLSX, or an entity/date token): '
                               'unmeasured by this tool, not clean', res={}))
        print('%-56s %-24s' % (sid, 'NOT_READABLE'), flush=True)
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
    return 1 if (c.get('KEY_IS_DIMENSION') or c.get('NO_KEY_FALLBACK_IS_DIMENSION')
                or c.get('KEY_UNMATCHED')) else 0


if __name__ == '__main__':
    sys.exit(main())
