"""One fetch per site root: declared feeds (<link rel=alternate type=rss/atom>),
anchor hints (rss/atom/feed/.xml/.rdf targets) and a WordPress marker, so a
second pass is not needed. Input: TSV with key, name, root columns (header).
Output: key name root http effective declared anchors wp"""
import sys, re, time, subprocess, concurrent.futures as cf
from urllib.parse import urljoin, urlparse

# Shelling out to curl still goes through THE trust set (jo_tls, rule 4c):
# system roots + session CA + the intermediates the engine loads.
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import jo_tls as _jo_tls
JO_CURL_TLS = _jo_tls.curl_args()

inp, out, jobs = sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 8
rows = [l.rstrip('\n').split('\t') for l in open(inp, encoding='utf-8').read().splitlines()[1:]]
LINK = re.compile(r'<link\b[^>]*>', re.I)
ATTR = re.compile(r'(\w+)\s*=\s*["\']([^"\']*)["\']')
A = re.compile(r'<a\b[^>]*href\s*=\s*["\']([^"\']+)["\'][^>]*>(.{0,120}?)</a>', re.I | re.S)
HINT = re.compile(r'(rss|atom|feed)', re.I)
# The request is the ENGINE's (tools/jo_http.py): JO_USER_AGENT, or the
# core/host_agents.def agent for a listed host, and the engine's Accept.
# This tool used to send an agent of its own (one of eight spellings in
# this directory, 2026-11-07), so a host walled against one agent read
# differently here than in the engine.
import jo_http as _jo_http
def get(url):
    r = subprocess.run(['curl', *JO_CURL_TLS, '-sS', '-L', '--compressed', '--max-time', '15', *_jo_http.curl_header_args(url),
                        '-o', '-', '-w', '\n%{http_code}\n%{url_effective}', url], capture_output=True)
    parts = r.stdout.rsplit(b'\n', 2)
    if len(parts) < 3: return 'ERR', '', b''
    return parts[1].decode(errors='ignore'), parts[2].decode(errors='ignore'), parts[0][:400000]
def one(r):
    key, name, root = r[0], r[1], r[2]
    try: http, eff, body = get(root)
    except Exception: return (key, name, root, 'ERR', '', '', '', '')
    text = body.decode('utf-8', 'ignore')
    base = eff or root
    declared, anchors = [], []
    for tag in LINK.findall(text):
        a = dict((k.lower(), v) for k, v in ATTR.findall(tag))
        t = a.get('type', '').lower()
        if ('rss' in t or 'atom' in t) and a.get('href'):
            declared.append(urljoin(base, a['href']))
    for href, label in A.findall(text):
        hl = href.lower()
        if hl.startswith(('javascript', 'mailto', 'tel:', '#')) or hl.endswith(('.pdf', '.xlsx', '.docx', '.zip', '.jpg', '.png')):
            continue
        if HINT.search(hl) or HINT.search(re.sub(r'<[^>]+>', '', label)) or hl.endswith(('.rdf', '.xml')):
            u = urljoin(base, href)
            if urlparse(u).netloc: anchors.append(u)
    wp = '1' if ('/wp-json/' in text or 'wp-content/' in text) else ''
    time.sleep(0.1)
    return (key, name, root, http, eff, '|'.join(dict.fromkeys(declared))[:3000],
            '|'.join(dict.fromkeys(anchors))[:3000], wp)
done = 0
with cf.ThreadPoolExecutor(jobs) as ex, open(out, 'w', encoding='utf-8') as fh:
    fh.write('key\tname\troot\thttp\teffective\tdeclared\tanchors\twp\n')
    for t in ex.map(one, rows):
        fh.write('\t'.join(x.replace('\t', ' ').replace('\n', ' ') for x in t) + '\n'); fh.flush()
        done += 1
        if done % 500 == 0: print('done', done, file=sys.stderr, flush=True)
