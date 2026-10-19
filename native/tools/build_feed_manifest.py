"""Validate discovered feed URLs live and write batch-44 manifest rows.
argv: discovery.tsv registered_urls.txt out.txt ID_PREFIX beat_tag
Each candidate is fetched once, classified by its root element (Atom / RSS 1.0
RDF / RSS 2.0), its records counted at the ELEMENT START (<item[\s>], never a
bare substring — RSS 1.0's <items> container taught batch 43 that), and
written only when it holds >= 1 record. Everything else goes to the rejects
TSV with its reading."""
import sys, re, time, subprocess, datetime, collections, html, concurrent.futures as cf
from urllib.parse import urlparse
disc, registered, out, prefix, beat = sys.argv[1:6]
batch = re.sub(r'\D', '', prefix.split('_')[0]) or '?'
jobs = int(sys.argv[6]) if len(sys.argv) > 6 else 8
# category|record_type|tags for the rows; the default was hard-coded as government
# until batch 46 shipped news outlets labelled government-notice
CAT, RTYPE, TAGS = (sys.argv[7].split('|') + ['government', 'government-notice', '{beat},government,notice,rss'])[:3] if len(sys.argv) > 7 else ('government', 'government-notice', '{beat},government,notice,rss')
reg = set(l.strip().rstrip('/').lower() for l in open(registered, encoding='utf-8'))
def normurl(u):
    u = re.sub(r'^(https://[^/]+):443/', r'\1/', re.sub(r'^(http://[^/]+):80/', r'\1/', u))
    return u.split('#', 1)[0]
cands, seen = [], set()
for line in open(disc, encoding='utf-8').read().splitlines()[1:]:
    f = line.split('\t')
    if len(f) < 7: continue
    key, name, root, http, eff, declared, anchors = f[:7]
    for u in [x for x in (declared + '|' + anchors).split('|') if x]:
        u = normurl(u)
        # a WordPress site declares its COMMENTS feed beside its posts feed;
        # comment threads on a government site are not a register of anything
        if re.search(r'/comments/feed/?$|feed=comments-rss2', u, re.I): continue
        # a page can link a URL carrying ITS OWN credential (a Mapbox feedback link
        # with an access_token, found on a French commune's site); such a URL is
        # never a feed and must never be written into a repository file
        if re.search(r'[?&](access_token|api_key|apikey|token|secret|auth)=', u, re.I): continue
        if u.lower() in seen: continue
        seen.add(u.lower()); cands.append((key, name, u))
print('candidate feed urls', len(cands), file=sys.stderr)
UA = 'Mozilla/5.0 (compatible; JapanOSINT)'
def get(url):
    r = subprocess.run(['curl', '-sS', '-L', '--compressed', '--max-time', '25', '-A', UA,
                        '-o', '-', '-w', '\n%{http_code}', url], capture_output=True)
    b = r.stdout; i = b.rfind(b'\n'); return b[i + 1:].decode(errors='ignore'), b[:i]
def classify(body):
    head = body[:3000].lower()
    if b'<feed' in head: return 'entry', rb'<entry[\s>]', 'atom'
    if b'<rdf:rdf' in head: return 'item', rb'<item[\s>]', 'rss1'
    if b'<rss' in head: return 'channel.item', rb'<item[\s>]', 'rss2'
    return None, None, None
DATE = re.compile(rb'<(pubDate|dc:date|updated|published)>([^<]{8,40})<')
TITLE = re.compile(rb'<title[^>]*>\s*(?:<!\[CDATA\[)?([^<\]]{1,80})', re.I)
def one(c):
    key, name, url = c
    if url.rstrip('/').lower() in reg: return (key, name, url, 'DUP_URL', '', 0, '', '')
    try: http, body = get(url)
    except Exception: return (key, name, url, 'ERR', '', 0, '', '')
    if http != '200': return (key, name, url, f'HTTP_{http}', '', 0, '', '')
    ap, tag, kind = classify(body)
    if not ap: return (key, name, url, 'NOT_FEED', '', 0, '', '')
    n = len(re.findall(tag, body))
    m = DATE.search(body); newest = m.group(2).decode('utf-8', 'ignore').strip().replace('|', ' ') if m else ''  # a date cell with a pipe split three rows
    t = TITLE.search(body); ftitle = html.unescape(re.sub(r'\s+', ' ', t.group(1).decode('utf-8', 'ignore'))).strip() if t else ''  # a title spanning lines broke a row
    time.sleep(0.1)
    return (key, name, url, 'OK', kind, n, newest, ftitle)
def slug(s):
    return re.sub(r'[^A-Z0-9]+', '_', s.upper()).strip('_')
res = []
with cf.ThreadPoolExecutor(jobs) as ex:
    for i, t in enumerate(ex.map(one, cands)):
        res.append(t)
        if (i + 1) % 500 == 0: print('validated', i + 1, file=sys.stderr, flush=True)
seenid = collections.defaultdict(int)
with open(out, 'w', encoding='utf-8') as fh, open(out + '.rejects.tsv', 'w', encoding='utf-8') as rj:
    fh.write(f'# Batch {batch} beat {beat}: RSS/RDF/Atom feeds each organisation declares or links on its own site root,\n')
    fh.write('# discovered from a public register of official domains and fetched live. Counts and newest dates in each\n')
    fh.write('# description are from that fetch. Feeds whose URL is already in the tree were dropped as DUP_URL.\n')
    fh.write('# id|mode|want|category|record_type|tags|portal|name|name_ja|url|probe|description|opts\n')
    rj.write('key\tname\turl\tverdict\tkind\titems\n')
    for key, name, url, v, kind, n, newest, ftitle in res:
        if not (v == 'OK' and n > 0):
            rj.write(f'{key}\t{name}\t{url}\t{v}\t{kind}\t{n}\n'); continue
        base = f'{prefix}_{slug(key)[:40]}'
        seenid[base] += 1
        rid = base + (f'_{seenid[base]}' if seenid[base] > 1 else '')
        ap = {'atom': 'entry', 'rss1': 'item', 'rss2': 'channel.item'}[kind]
        if kind == 'atom': opts = f'array_path={ap};id_keys=id,link.@href;title_keys=title;date_keys=updated,published;link_keys=link.@href;interval=3600;timeout_ms=30000'
        else: opts = f'array_path={ap};id_keys=guid,link;title_keys=title;date_keys=pubDate,dc:date;link_keys=link;interval=3600;timeout_ms=30000'
        desc = (f'Fetched {datetime.date.today()}: {n} items, {kind.upper()} feed, newest {newest or "(no date field)"}. '
                f'A feed the organisation itself declares or links on its site root.')
        host = urlparse(url).netloc
        portal = f'{urlparse(url).scheme}://{host}/'
        label = (ftitle or url.rsplit('/', 1)[-1]).replace('|', ' ')
        nm = f'{name} - {label}'.replace('|', ' ')
        fh.write(f'{rid}|xml|any|{CAT}|{RTYPE}|{TAGS.format(beat=beat)}|{portal}|{nm}|{nm}|{url}|{url}|{desc}|{opts}\n')
ok = [t for t in res if t[3] == 'OK' and t[5] > 0]
print('OK rows', len(ok), 'rejects', len(res) - len(ok), file=sys.stderr)
print(collections.Counter(t[3] for t in res), file=sys.stderr)
