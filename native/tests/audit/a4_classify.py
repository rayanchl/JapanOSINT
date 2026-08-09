#!/usr/bin/env python3
import collections

BASE = '/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/'

KEY_GATED = {
 'OPENAQ_GLOBAL','WAQI_GLOBAL','EMAILREP_LOOKUP','NUMVERIFY_PHONE','OPENCORPORATES',
 'ALPHAVANTAGE_SEARCH','FINNHUB_SEARCH','fofa-jp','docomo-population','estat-household',
 'grayhat-buckets','houjin-bangou','mapfan-api','navitime-api','odpt-train','reinfolib',
 'vessel-finder','wifi-networks-shodan','NL_KVK','PEP_CHECK','WATCHLIST_CHECK_NEW',
 'LICENSE_PLATE_LOOKUP','IOC_LOOKUP','THREAT_FEED_LOOKUP',
}
WAF = {'pdc-disasters','bom-au-warnings','unesco-heritage','reddit-jp-subs','LU_LBR',
       'SEA2_REGISTRY','RS_APR'}
DEAD = {'kitco-news','mlit-transaction','mlit-landprice','HR_SUDREG','PT_RCBE'}
TIMEOUT_IDS = {'cell-towers','crtsh-historical','job-boards','overpass-subway-tracks',
               'rice-paddies','satellite-ground-stations','sento-public-baths',
               'shrine-temple','submarine-cables','unified-trains','city-halls',
               'ferry-routes','admin-boundaries'}

rows = {}
for line in open(BASE + 'a4_table.tsv', encoding='utf-8'):
    if line.startswith('id\t') or line.startswith('#'):
        continue
    c = line.rstrip('\n').split('\t')
    rows[c[0]] = c

order = [l.split()[0] for l in open(BASE + 'a4_all.txt') if l.strip()]
cnt = collections.Counter()
out = []
for sid in order:
    c = rows.get(sid)
    rc = c[1] if c else 'TIMEOUT'
    nrows = int(c[2]) if c else 0
    geo = int(c[3]) if c else 0
    if sid in TIMEOUT_IDS and nrows == 0:
        v = 'TIMEOUT'
    elif nrows > 0:
        v = 'DATA'
    elif sid in KEY_GATED:
        v = 'KEY_GATED'
    elif sid in WAF:
        v = 'WAF_BLOCKED'
    elif sid in DEAD:
        v = 'RC_ERROR'
    elif rc not in ('0',):
        v = 'RC_ERROR'
    else:
        v = 'EMPTY'
    cnt[v] += 1
    out.append((sid, v, nrows, geo))
for k, n in cnt.most_common():
    print('%-12s %d' % (k, n))
print('total', sum(cnt.values()))
with open(BASE + 'a4_verdicts.tsv', 'w') as f:
    for r in out:
        f.write('\t'.join(str(x) for x in r) + '\n')
