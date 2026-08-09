#!/usr/bin/env python3
"""Final slice-05 verdicts."""
import collections, os

BASE = '/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/'

KEY_GATED = {
 'OPENAQ_GLOBAL','WAQI_GLOBAL','EMAILREP_LOOKUP','NUMVERIFY_PHONE','OPENCORPORATES',
 'ALPHAVANTAGE_SEARCH','FINNHUB_SEARCH','fofa-jp','docomo-population','estat-household',
 'grayhat-buckets','houjin-bangou','mapfan-api','navitime-api','odpt-train','reinfolib',
 'vessel-finder','wifi-networks-shodan','NL_KVK','PEP_CHECK','WATCHLIST_CHECK_NEW',
 'LICENSE_PLATE_LOOKUP','mlit-landprice','mlit-transaction',
}
WAF = {'pdc-disasters','unesco-heritage','reddit-jp-subs','LU_LBR','SEA2_REGISTRY',
       'RS_APR','EGOV_PUBLIC_COMMENT','WASTE_HAULERS','BUSINESS_LICENSES'}
DEAD = {'kitco-news','bom-au-warnings','wanted-persons','ADVISORY_COUNCILS',
        'HR_SUDREG','PT_RCBE','LT_REGISTRAI','bird-makeup-jp'}
ENV_BLOCKED = {'cell-towers','city-halls','ferry-routes','rice-paddies',
               'satellite-ground-stations','sento-public-baths','shrine-temple',
               'submarine-cables','overpass-subway-tracks','admin-boundaries',
               'job-boards','unified-trains'}
TIMEOUT = {'crtsh-historical'}
EMPTY_OK = {'metoffice-warnings','THREAT_FEED_LOOKUP','IOC_LOOKUP','HU_COMPANIES'}

rows = {}
for line in open(BASE + 'a4b_table.tsv', encoding='utf-8'):
    if line.startswith('id\t'):
        continue
    c = line.rstrip('\n').split('\t')
    rows[c[0]] = c

order = [l.split()[0] for l in open(BASE + 'a4_all.txt') if l.strip()]
cnt = collections.Counter()
out = []
for sid in order:
    c = rows.get(sid)
    nrows = int(c[2]) if c else 0
    geo = int(c[3]) if c else 0
    rc = c[1] if c else 'TIMEOUT'
    if sid in ENV_BLOCKED:
        v = 'ENV_BLOCKED'
    elif sid in TIMEOUT:
        v = 'TIMEOUT'
    elif nrows > 0:
        v = 'DATA'
    elif sid in KEY_GATED:
        v = 'KEY_GATED'
    elif sid in WAF:
        v = 'WAF_BLOCKED'
    elif sid in DEAD:
        v = 'DEAD_UPSTREAM'
    elif sid in EMPTY_OK:
        v = 'EMPTY'
    elif rc not in ('0',):
        v = 'RC_ERROR'
    else:
        v = 'EMPTY'
    cnt[v] += 1
    out.append((sid, v, nrows, geo))
for k, n in cnt.most_common():
    print('%-15s %d' % (k, n))
print('total', sum(cnt.values()))
with open(BASE + 'a4b_verdicts.tsv', 'w', encoding='utf-8') as f:
    for r in out:
        f.write('\t'.join(str(x) for x in r) + '\n')
