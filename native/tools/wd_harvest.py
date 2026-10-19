"""Worldwide seed from Wikidata: official websites (P856) of public bodies,
one cheap SPARQL query per (country, class) so no single query times out,
paced at one request every 2 s. Output: key name root country class."""
import sys, time, subprocess, re, collections
from urllib.parse import urlparse, quote
out = sys.argv[1]
UA = 'JapanOSINT-discovery/1.0 (https://github.com/rayanchl/JapanOSINT)'
def sparql(q, timeout=90):
    r = subprocess.run(['curl', '-sS', '--max-time', str(timeout), '-G', 'https://query.wikidata.org/sparql',
                        '-H', 'Accept: text/tab-separated-values', '-A', UA, '--data-urlencode', 'query=' + q,
                        '-w', '\n%{http_code}'], capture_output=True)
    b = r.stdout; i = b.rfind(b'\n'); code = b[i+1:].decode(errors='ignore'); body = b[:i].decode('utf-8', 'ignore')
    return code, body
# every sovereign state (and a few territories with ISO codes) with its ISO-2 code
code, body = sparql('SELECT ?c ?cc WHERE { ?c wdt:P31 wd:Q3624078; wdt:P297 ?cc. }')
countries = []
for line in body.splitlines()[1:]:
    f = line.split('\t')
    if len(f) == 2:
        countries.append((f[0].strip('<>').rsplit('/', 1)[-1], f[1].strip('"').split('"')[0]))
print('countries', len(countries), file=sys.stderr, flush=True)
CLASSES = {'Q327333': 'government-agency', 'Q192350': 'ministry', 'Q2659904': 'government-organization',
           'Q41487': 'court', 'Q35535': 'police', 'Q66344': 'central-bank', 'Q11204': 'legislature',
           'Q1639780': 'regulatory-agency', 'Q15911314': 'association', 'Q3977784': 'public-prosecutor'}
seen = set(); n = 0
with open(out, 'w', encoding='utf-8') as fh:
    fh.write('key\tname\troot\tcountry\tclass\n')
    for qid, cc in sorted(countries, key=lambda x: x[1]):
        for cls, cname in CLASSES.items():
            q = ('SELECT ?item ?itemLabel ?web WHERE { ?item wdt:P17 wd:%s; wdt:P31 wd:%s; wdt:P856 ?web. '
                 'OPTIONAL { ?item rdfs:label ?itemLabel FILTER(LANG(?itemLabel) = "en") } }' % (qid, cls))
            for attempt in range(3):
                code, body = sparql(q)
                if code == '200': break
                time.sleep(10 * (attempt + 1))
            if code != '200':
                print('FAIL', cc, cname, code, file=sys.stderr, flush=True); time.sleep(2); continue
            for line in body.splitlines()[1:]:
                f = line.split('\t')
                if len(f) < 3: continue
                item = f[0].strip('<>').rsplit('/', 1)[-1]
                label = f[1].strip().strip('"').split('"@')[0] if f[1].strip() else item
                web = f[2].strip().strip('<>')
                try: u = urlparse(web); host = u.netloc.lower()
                except Exception: continue
                if not host or '.' not in host or host in seen: continue
                seen.add(host)
                fh.write(f"{item}\t{label} ({cname}, {cc})\t{u.scheme}://{host}/\t{cc}\t{cname}\n".replace('\n', ' ', 0)); n += 1
            fh.flush()
            time.sleep(2)
        print(cc, n, file=sys.stderr, flush=True)
print('hosts', n, file=sys.stderr)
