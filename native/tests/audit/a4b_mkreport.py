#!/usr/bin/env python3
"""Emit the per-source table rows for report_05.md."""
BASE = '/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/'

QUAL = {
 # id: (data quality, notes)
 'admin-boundaries': ('gated by env', 'Overpass; all 4 endpoints IP-banned from this host'),
 'OPENAQ_GLOBAL': ('gated', 'no OPENAQ_KEY; v2 keyless fallback now 410'),
 'OPENMETEO_AQ': ('real+complete', 'PM2.5/PM10/O3/NO2/SO2/CO + AQI, geo-pinned'),
 'WAQI_GLOBAL': ('gated', 'no AQICN_TOKEN'),
 'ASN_LOOKUP': ('real+complete', 'BGP/ASN + prefix; no geo (an ASN is not a place)'),
 'GBIF_OCCURRENCE': ('real+complete', '30 occurrences, all geo-pinned'),
 'INATURALIST': ('real+complete', '30 observations, all geo-pinned'),
 'OBIS_MARINE': ('real+complete', 'sweep used a vessel IMO as pivot; species name gives 30 geo rows'),
 'bird-makeup-jp': ('dead upstream', '6 handles fetch 200 OK but every outbox is empty; rc fixed to 0'),
 'BREACH_INDEX_EMAIL': ('real+complete', 'local 1,018-breach corpus, hit/count only'),
 'BREACH_INDEX_PASSWORD': ('real+complete', 'local corpus'),
 'BREACH_INDEX_PHONE': ('real+complete', 'local corpus'),
 'BREACH_INDEX_USERNAME': ('real+complete', 'local corpus'),
 'cam-camscape': ('real+thin (no link)', 'core/camera_store.c never sets it.link; camera page URL sits unused in props.url'),
 'cam-webcamera24': ('real+thin (no link)', 'same core/camera_store.c bug; 160 cams'),
 'cell-towers': ('gated by env', 'Overpass IP-ban'),
 'city-halls': ('gated by env', 'Overpass IP-ban; rc=-1 quarantines'),
 'EMAILREP_LOOKUP': ('gated', 'no EMAILREP_KEY'),
 'NUMVERIFY_PHONE': ('gated', 'no NUMVERIFY_KEY'),
 'OPENCORPORATES': ('gated', 'no OPENCORPORATES_API_KEY'),
 'CH_OPENDATA': ('real+complete', '20 datasets with a local-language pivot'),
 'FI_OPENDATA': ('real+complete', '20 datasets with a local-language pivot'),
 'UK_DATAGOV': ('real+complete', '10 datasets'),
 'crtsh-historical': ('unverifiable', 'crt.sh never returned inside 280 s'),
 'data-go-jp-ckan': ('real+complete', '10 CKAN datasets, no geo (datasets)'),
 'docomo-population': ('gated', 'no DOCOMO_POPULATION_API_KEY'),
 'egov-laws': ('real+complete', '200 laws, title/link/date all present'),
 'estat-household': ('gated', 'no ESTAT_APP_ID'),
 'ferry-routes': ('gated by env', 'Overpass IP-ban'),
 'kitco-news': ('dead upstream', 'every published Kitco RSS path 404s; left visible, not papered over'),
 'trading-econ': ('real+complete', 'rewritten onto ws/stream.ashx: 100 items with country/category/importance'),
 'ALPHAVANTAGE_SEARCH': ('gated', 'no ALPHAVANTAGE_KEY'),
 'FINNHUB_SEARCH': ('gated', 'no FINNHUB_KEY'),
 'fofa-jp': ('gated', 'no FOFA_API_KEY'),
 'gcom-w': ('real+thin (no pin)', 'granule catalogue not measurements; newest granule 2025-08-31 (~11 mo stale)'),
 'bom-au-warnings': ('dead upstream', 'bom.gov.au/rss/1.xml 404; only replacement is a licence-restricted private API'),
 'copernicus-ems': ('real+complete', 'URL repointed at mapping.emergency.copernicus.eu/latest/feed/'),
 'emsc-quakes': ('real+complete', 'rewritten onto FDSN: 300 quakes, all with hypocentre'),
 'iceland-quakes': ('real+complete', 'rewritten onto FDSN Iceland AOI: 300 quakes with hypocentre'),
 'metoffice-warnings': ('real+complete', 'feed 200 OK with an empty channel — no active UK warnings'),
 'nhc-cpac': ('real+thin (no geo)', '2 items; NHC georss not parsed by lib/rss_atom.c'),
 'pdc-disasters': ('WAF', 'pdc.org Cloudflare-challenges every path'),
 'volcanodiscovery': ('real+complete', 'URL repointed at /volcano-news.rss'),
 'gitlab-bitbucket-leaks': ('real+thin', '2 rows; GitLab/Bitbucket search is heavily rate-limited anonymously'),
 'RADIO_BROWSER': ('real+thin (no geo)', 'radio-browser publishes geo_lat/geo_long; not carried'),
 'grayhat-buckets': ('gated', 'no GRAYHAT_API_KEY'),
 'hatena-bookmark-extended': ('real+complete', '199 bookmarks'),
 'houjin-bangou': ('gated', 'no HOUJIN_BANGOU_KEY'),
 'IOC_LOOKUP': ('real+complete', 'rc bug fixed: a HIT used to return rc=1 and quarantine the source'),
 'japan-reit': ('real+complete', '/meigara/ retired; scrape moved to /list/rimawari/, 57 issues (was 3)'),
 'jma-earthquake': ('real+complete', 'per-eid dedupe + depth/max-intensity/link; geo 398 -> 439'),
 'jma-tide': ('real+complete', 'link + measurement summary added; 166/166 pinned'),
 'job-boards': ('gated by env', 'Overpass IP-ban; NO_TITLE + order-dependent uid already fixed in code'),
 'jr-boarding-stats': ('labels-only', 'reachability probe, not boarding numbers; reachable=false'),
 'kafun-pollen': ('labels-only', 'reachability probe, no pollen concentrations'),
 'BANKRUPTCY_SEARCH': ('real+complete', 'CourtListener; rc fixed from 15 to 0'),
 'CRIMINAL_RECORDS': ('real+complete', 'CourtListener; rc fixed'),
 'LEGAL_SEARCH': ('real+complete', 'CourtListener; rc fixed'),
 'LICENSE_PLATE_LOOKUP': ('gated', 'no lawful public API; now logs its gate'),
 'mapfan-api': ('gated', 'no MAPFAN_API_KEY'),
 'mhlw-health': ('labels-only', 'portal row; its own body admits there is no machine API'),
 'mlit-landprice': ('dead upstream', 'land.mlit.go.jp/webland/api refuses connections; reinfolib needs a key'),
 'mlit-transaction': ('dead upstream', 'same retired WebLand API'),
 'navitime-api': ('gated', 'no NAVITIME_API_KEY'),
 'nexco-roadwork': ('labels-only', '3 reachability rows, no roadwork records'),
 'nitter-mirrors': ('labels-only', '3 mirror-reachability rows, no posts'),
 'npa-special-fraud': ('real+thin (pin is NPA HQ)', '25 monthly national totals all pinned at NPA headquarters'),
 'odpt-train': ('gated', 'no ODPT token'),
 'ARCHIVE_ORG_SEARCH': ('real+complete', '20 items'),
 'GITHUB_COMMITS_BY_EMAIL': ('real+complete', '20 commits'),
 'MUSICBRAINZ': ('real+complete', '20 artists'),
 'PHOTON_GEOCODE': ('real+complete', '9 results, all geo'),
 'TOR_ONIONOO': ('real+thin', '1 relay'),
 'BIGDATACLOUD_REVERSE': ('real+complete', 'needs a "lat,lon" pivot; sweep used a keyword'),
 'HN_USER': ('real+complete', 'needs an HN username pivot'),
 'OPENVERSE': ('real+complete', '20 images'),
 'SEC_FULLTEXT': ('real+complete', '20 filings'),
 'STACKEXCHANGE_SEARCH': ('real+complete', '20 questions'),
 'WIKIDATA_ENTITY_SEARCH': ('real+complete', '20 entities'),
 'osm-changesets-jp': ('real+complete', 'uid was the OSM USER id (100 emitted -> 28 stored); title added; Tokyo fallback removed'),
 'overpass-subway-tracks': ('gated by env', 'Overpass IP-ban'),
 'peeringdb-jp': ('real+complete', '~1.1k rows; every IXP and ASN used to be pinned at Tokyo station — removed'),
 'PEP_CHECK': ('gated', 'no OPENSANCTIONS_API_KEY; rc bug fixed, gate now logged'),
 'WATCHLIST_CHECK_NEW': ('gated', 'no OPENSANCTIONS_API_KEY; rc bug fixed, gate now logged'),
 'CARRIER_LOOKUP': ('real+thin', 'libphonenumber-style metadata only'),
 'PHONE_LOOKUP': ('real+thin', 'rc fixed from 1 to 0'),
 'PHONE_REPUTATION': ('real+thin', 'rc fixed from 1 to 0'),
 'poc-in-github': ('real+complete', '94 PoC repos; 94 fake Tokyo pins removed, link/date/uid added'),
 'psbdmp-pastes': ('labels-only', 'reachability probe, no pastes'),
 'reddit-jp-subs': ('WAF', 'reddit.com/*.json returns 403 to this host'),
 'DK_CVR': ('real+thin', '1 company'),
 'HR_SUDREG': ('dead upstream', 'endpoint 404'),
 'HU_COMPANIES': ('honest empty', 'fetch OK, no match for the pivot'),
 'LT_REGISTRAI': ('dead upstream', 'endpoint 404'),
 'LU_LBR': ('WAF', 'HTTP 429'),
 'NL_KVK': ('gated', 'no KVK_API_KEY'),
 'PT_RCBE': ('dead upstream', 'endpoint 404'),
 'RS_APR': ('WAF', 'connection refused / status 0'),
 'IL_COMPANIES': ('real+complete', '2 companies'),
 'SEA2_REGISTRY': ('WAF', '6 of 6 SEA registries 404/refused, 73 s wall clock'),
 'reinfolib': ('gated', 'no subscription key'),
 'rice-paddies': ('gated by env', 'Overpass IP-ban'),
 'satellite-ground-stations': ('gated by env', 'Overpass IP-ban'),
 'sento-public-baths': ('gated by env', 'Overpass IP-ban'),
 'shrine-temple': ('gated by env', 'Overpass IP-ban'),
 'SSL_ANALYZER': ('real+complete', '101 rows (cert chain + SANs); 59 s -> duration_outlier'),
 'submarine-cables': ('gated by env', 'Overpass IP-ban'),
 'teikoku-failures': ('labels-only', 'reachability probe, no bankruptcy records'),
 'THREAT_FEED_LOOKUP': ('honest empty', '8.8.8.8 is genuinely clean in OTX/URLhaus/ThreatFox'),
 'tor-exit-nodes': ('real+complete', 'geo guard was dropping the whole feed; onionoo no longer publishes lat/lon'),
 'unesco-heritage': ('WAF', 'whc.unesco.org/en/list/xml/ Cloudflare-challenged; the RSS that works has no coords'),
 'unified-trains': ('gated by env', 'Overpass sub-collector + MLIT N02 both empty'),
 'vessel-finder': ('gated', 'no VESSELFINDER_API_KEY; rc fixed from -1 to 0'),
 'wanted-persons': ('dead upstream', 'NPA shimeitehai index 404s; extraction method is unsound anyway'),
 'WHALE_ALERT': ('real+thin', '1 row'),
 'wifi-networks-shodan': ('gated', 'no SHODAN_API_KEY'),
 'AFRICA_REGISTRY': ('real+thin', '1 company, 15.8 s'),
 'xrain-radar': ('labels-only', 'reachability probe, no rainfall values'),
}

rows = {}
for line in open(BASE + 'a4b_table.tsv', encoding='utf-8'):
    if line.startswith('id\t'):
        continue
    c = line.rstrip('\n').split('\t')
    rows[c[0]] = c

verd = {}
for line in open(BASE + 'a4b_verdicts.tsv', encoding='utf-8'):
    c = line.rstrip('\n').split('\t')
    verd[c[0]] = c

order = [l.split()[0] for l in open(BASE + 'a4_all.txt') if l.strip()]
out = []
for sid in order:
    v = verd.get(sid, [sid, '?', '0', '0'])
    q, note = QUAL.get(sid, ('real+complete', ''))
    if sid.startswith('gnews-mon-'):
        q = 'real+complete'
        note = 'Google News RSS, article rows; no geo (news)'
        if sid == 'gnews-mon-tw-pla-incursion':
            note = 'query retranslated to 解放軍 擾台 — 0 -> 100 items'
        if sid == 'gnews-mon-no-arctic-militarization':
            note = 'query retranslated to Arktis militær — 0 -> 81 items'
        if sid == 'gnews-mon-il-gaza':
            note = 'DB_ERROR class: lib/rss_atom.c:172 cuts summary at 240 BYTES'
    out.append('| %s | %s | %s | %s | %s | %s |' % (sid, v[1], v[2], v[3], q, note))
print('\n'.join(out))
