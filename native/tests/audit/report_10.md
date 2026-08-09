# Slice 10 — 206 sources across 83 files (build slot a9)

## Summary

| verdict | count |
|---|---|
| **DATA** (real rows this run) | 140 |
| **KEY_GATED** (honest, credential absent) | 16 |
| **WAF_BLOCKED** (403 / reset / hang for *both* collector UA and browser UA) | 16 |
| **DEAD_UPSTREAM** (404 / NXDOMAIN / retired) | 11 |
| **EMPTY** (fetch OK, nothing matched) | 11 |
| **ENV_BLOCKED** (6 Overpass + GDELT_TV) | 7 |
| **TIMEOUT** (gtfs-jp, unified-highway) | 2 |
| **RC_ERROR** remaining (jshis-seismic, sakura-front — `-1` was correct this run) | 2 |
| CRASH / DB_ERROR / NO_TITLE | **0** (5 fixed) |

## The previous auditor's uncommitted work — all verified, none reverted

It touched 14 files. **All 13 of its RSS URL swaps re-run live and return rows**:
msrc-blog 3539, cert-pl 100, ncsc-nl 25, cert-at 50, cert-ee 100, ncsc-ch 94, cert-ee2 100,
cert-fi 286, krcert 10, ncsc-ie 223, cert-no 10, cert-dk 10, small-wars 10.

Its non-URL work also verified: `alos-palsar` 25→**50 rows with real ASF WKT footprints**
(was 0 geo + a fabricated nationwide bbox on every row); `flight-adsb` **90 rows,
notitle=0** (and it no longer invents a Tokyo position for a state vector with no fix);
`jamstec-argo` **84/0 notitle**; `jma-warnings` **16/0 notitle**; `mastodon-jp-instances`
**51 rows, 0 geo** (was 51 invented pins stacked on Tokyo Station); `my-jvn` **50 rows,
DB_ERROR gone**; `cam-skylinewebcams` 36→**101 rows** (uid was a 60-char URL prefix, so
every camera in a city collided); `chaos-bugbounty-jp` **8/0 notitle, 0 geo**;
`GREYNOISE_COMMUNITY` **1 row**; `IATI_REGISTRY` 0→**25 rows**.

**Two of its changes did not fix their target**, both re-diagnosed: RELIEFWEB (still gated)
and GDELT_TV (blocked by a core bug, not by the query).

## Five findings that need attention

**1. `core/httpclient.c:123` hard-codes `CONNECTTIMEOUT_MS = 10000` and ignores the
caller's `timeout_ms`.** For HTTPS, libcurl's connect timeout covers the TLS handshake.
`api.gdeltproject.org` handshakes in **9.6–10.8 s** (measured 3×: `tls=10.817`, `9.616`,
`10.799`), so it is **unreachable from this process regardless of the budget the collector
asks for** — GDELT_TV requests 45,000 ms and still dies at 10,813 ms with `status=0`. Kills
GDELT_TV outright and NEWS_AGGREGATOR's GDELT leg. Read-only file, **not patched**. Fix:
`CONNECTTIMEOUT_MS = min(timeout_ms, 20000)`. Fleet-wide, not slice-local.

**2. `SSLBL_GLOBAL` was silently discarding 87 % of every run.** `tf_lines()` took the
indicator as "first token up to comma/space/tab", but SSLBL ships
`Listingdate,SHA1,Listingreason` — the token stopped at the space inside
`2026-07-30 19:52:58`, so **every row's title *and* dedupe uid was a bare date**. 200
certificates collapsed onto 27 date rows. **Fixed → 200/200 distinct SHA1s.**

**3. All six `gov_enforcement.c` scrapers filed site navigation chrome as regulatory
records.** `jo_emit_anchors(..., href_must=NULL, ...)` harvested the whole page:
FSA_ENFORCEMENT with no pivot emitted 30 rows of `record_type="fsa-penalty"` titled
`本文へ移動` (skip-to-content), `English`, `金融庁について`; PMDA_APPROVALS on 承認 emitted
the menu entries `承認審査関連業務` as `drug-device-approval`. **Fixed.** Note
`jo_emit_anchors()` lives in the shared `_jp_osint.inc` — **every collector fleet-wide that
passes `href_must=NULL` has this bug**; worth a grep.

**4. A "portal-status" family of 10 sources emits a hardcoded label row instead of data**:
`tepco-outage`, `jcg-navarea`, `bosai-volcano-cam`, `comiket-events`, `nict-atlas`,
`jpo-jplatpat`, `tmp-protests`, `tsr-closures`, `securitytrails-history`, `vnet`. Each does
one HTTP HEAD and emits exactly one row whose `title`/`summary` are string literals in the
.c file; the only fetched datum is `reachable: true|false`. **9 of the 10 logged
`reachable=0` tonight and still emitted the row.** Not fixed — each needs a real collector.

**5. Two sources are dead at DNS, confirmed authoritatively.** `api.bgpview.io` (BGPVIEW)
and `search.patentsview.org` (PATENTSVIEW) both return NXDOMAIN from Cloudflare DoH with an
SOA from their own apex nameservers. `api.patentsview.org` 301s to
`data.uspto.gov/support/transition-guide/patentsview` — PatentsView was retired into
USPTO's Open Data Portal, whose successor API is key-gated. Documented in-file with the
evidence; deliberately **not** repointed at a different service under the old name.

### Secondary

- `msrc-blog` works but pulls the **entire MSRC CVE backlog: 4,995 items → 3,539 rows,
  10.2 s**, tripping `duration_outlier` every run. `rss_collect()` has no max-items cap
  (read-only lib) — recommend one.
- `record_type` is **NULL** on 14 sources (`tdnet-disclosure` 100 rows,
  `phishing-feeds-jp` 271 rows, `estat-crime`, plus the whole portal-status family). Those
  rows can't be classified by the API or UI.
- `WAYBACK_MACHINE` emits 50, stores 39 — uid collisions on CDX rows (timestamp not in the
  remote_key). `project-zero` emits 10, stores 9 (duplicate feed guid).
- `gtfs-jp` is **not broken**: it processed 175 feeds / 29,889 stops / 510,691 stop_times
  and simply exceeds the 180 s harness timeout.
- MalwareBazaar's anonymous API is gone (`401 {"error":"Unauthorized"}`); the other four
  abuse.ch feeds are static downloads and still keyless.
- **The sweep's `EMPTY` verdicts for on-demand services were mostly wrong-pivot.**
  MAC_VENDOR_LOOKUP, DE_POSTAL, NL_ADDRESS, WIKIDATA_SPARQL (40 rows), OPENDATA_SOCRATA
  (65), OCHA_FTS (25), PEERINGDB_GLOBAL, BR_CNPJ and WAYBACK_MACHINE all return real rows
  given an entity of the right *kind*.
- **No `rc > 0` quarantine bug anywhere in these 83 files** — every `run()` entrypoint
  scanned, including those returning through a `run_svc()` helper.

## Fixes applied

| file | source(s) | bug | fix | re-test |
|---|---|---|---|---|
| `threatfeeds_world.c` | SSLBL_GLOBAL | CSV indicator = first token, stopped at the space in the timestamp; title+uid were a bare date; 173/200 rows overwritten | added `ind_field`/`date_field` to `tf_row` + `tf_csv_field()`; SSLBL uses col 1 (SHA1), col 0 → `published_at` | `recs=200 rows=200` (was 200→27); `title=014d51d7…`, `published_at=2026-07-30 19:52:58` |
| `threatfeeds_world.c` | MALWAREBAZAAR | abuse.ch retired anon access; collector POSTed, got 401, logged an opaque status | declare the gate (`ABUSECH_AUTH_KEY`/`MALWAREBAZAAR_AUTH_KEY`), send `Auth-Key:` when set, else no fetch + honest log | `gated (no ABUSECH_AUTH_KEY) — … unauthenticated = HTTP 401`, 0 rows, rc=0 |
| `gov_enforcement.c` | all 6 | `href_must=NULL` → whole-page anchor harvest; nav chrome filed as enforcement records | per-source content path derived from an href histogram of each live page (`/content/11300000/` 916 hits, `/nega-inf/cgi-bin/` 23, `/review-services/…/p-drugs/` 38, `/status/` 21) | PMDA 30 real approval docs; MHLW 30 `…/content/11300000/*.pdf`; MLIT 23 nega-inf endpoints; FSA 26 |
| `kyodo_rss.c` | kyodo-rss | `/rss/news.xml` 404s since the rebuild (both UAs) | `english.kyodonews.net/list/feed/rss4kyodonews-fzone` — the only feed the homepage still advertises | `recs=25 rows=25` (was rc=-1) |
| `aid_world.c` | RELIEFWEB | default UA earns HDX bot-block `406 {"error":"Blocked due to bot activity."}`, masking the actionable 403 | browser-shaped UA, same treatment OCHA_FTS already had | now `403` with "not using an approved appname"; a supplied `RELIEFWEB_APPNAME` can now actually work |
| `infra_world.c` | BGPVIEW | `api.bgpview.io` NXDOMAIN — permanently dead, looked like a transient `status=0` | documented dead-upstream with DoH evidence; left inert (RIPESTAT_GLOBAL in the same file already covers prefix/ASN) | 0 rows, rc=0, honest empty |
| `patents_world.c` | PATENTSVIEW | `search.patentsview.org` NXDOMAIN; `api.patentsview.org` 301s to the USPTO transition guide | documented dead-upstream + named the key-gated successor; not blind-repointed | 0 rows, rc=0 |

Net diff: 19 files, +621/−111 (7 files this auditor's, 12 inherited and verified).

## Findings not fixed (with reason)

| source(s) | issue | why not |
|---|---|---|
| GDELT_TV, NEWS_AGGREGATOR | 10 s TLS vs hard-coded 10 s connect timeout | `core/httpclient.c:123` — read-only |
| msrc-blog | 4,995-item run, `duration_outlier` | needs a cap in `rss_collect()` — `lib/` read-only |
| all RSS | 240-**byte** summary cut, `props[512]`, raw RFC-822 `published_at`, non-nesting `tag_text()` | `lib/rss_atom.c` — already reported |
| gas-stations, mlit-bridge, osm-transport-trains, radar-sites, utility-poles, wineries-craftbeer | Overpass IP-ban | ENV_BLOCKED, unverifiable tonight; not rewritten |
| the 10 portal-status sources | 1 hardcoded row + a HEAD probe | rewrite, not repair |
| acsc-au, cert-es, cert-it, cert-lt, cert-pt, cisa-ics, cisa-advisories, cisa-news, csa-sg, sophos-news, lawfare, maritime-bulletin, yahoo-news-jp-rss, JFTC_ENFORCEMENT, CAA_ENFORCEMENT, FSA_FINBIZ_REGISTRY | 403 / reset / hang for **both** UAs | WAF at the upstream edge; no live alternative found |
| rsf, transparency-intl, acled-blog, avherald, global-witness, cert-nz, cert-in, argus-energy, GR_GEMI | feed 404s or serves HTML with zero `<item>`; no `<link rel=alternate>` advertised; 3–5 candidate paths probed each | genuinely dead upstream |
| FSA_ENFORCEMENT | `/status/index.html` is a hub; the real 行政処分事例 listing (`/status/s_jirei/`) 404s/403s | after the fix it emits the page's own section links — better than nav chrome, still not penalty records |
| MHLW_LABOR_VIOLATIONS | URL is the 安全衛生 guidance page, but `.description` promises the "black company" violation list | source definition and URL disagree; correct page unknown |
| jshis-seismic, sakura-front | return `-1` on honest empty | both actually failed the *fetch* tonight so `-1` was right; the latent honest-empty→`-1` path remains |
| WAYBACK_MACHINE, project-zero | 50→39, 10→9 uid collisions | low severity, out of budget |
| 14 sources with `record_type=NULL` | rows unclassifiable | cross-cutting schema decision |

## Per-source table

Grouped; `quality` is the auditor's judgement.

**`intel_threat_world.c` (28)** — DATA: bleepingcomputer 15, cert-eu 10, checkpoint-research
15, cisco-talos 15, cybersecurity-dive 10, dark-reading 50, dfir-report 10,
google-security-blog 25, graham-cluley 20, hacker-news-sec 50, intel471 567,
krebs-on-security 10, malwarebytes-labs 20, **msrc-blog 3539 (FIXED)**, ncsc-uk 20,
project-zero 9, rapid7-blog 20, recorded-future 50, schneier 10, securelist 10,
securityweek 10, the-record 5, troy-hunt 15, unit42 15, welivesecurity 100 — all
`real+complete`, 0 geo (articles). WAF_BLOCKED: cisa-advisories, cisa-news (403 Akamai),
sophos-news (301→reset).

**`world_cert_advisories.c` (30)** — DATA: cccs-ca 50, **cert-at 50\***, cert-be 10,
cert-br 12, **cert-dk 10\***, **cert-ee 100\***, **cert-ee2 100\***, **cert-fi 286\***,
cert-fr 40, cert-gov-uk 20, **cert-no 10\***, **cert-pl 100\***, cert-se 20, cert-si 10,
cert-ua 9, circl-lu 15, jpcert-en 6, **krcert 10\***, **ncsc-ch 94\***, **ncsc-ie 223\***,
**ncsc-nl 25\*** (\* = URL fix). WAF_BLOCKED: acsc-au, cert-es, cert-it, cert-lt, cert-pt
(517), cisa-ics. DEAD: cert-in (938-byte JS-challenge page), cert-nz, csa-sg.

**`osint_thematic_global.c` (20)** — DATA: amnesty 12, bellingcat-2 10, cidrap 10,
crisis-group 10, flightglobal 10, gcaptain 12, hrw-news 20, intelnews 30, nasaspaceflight
10, oilprice 15, rigzone 20, **small-wars 10 (FIXED)**, spacenews 24, splash247 10, statnews
20. DEAD: acled-blog, avherald. WAF: lawfare, maritime-bulletin. EMPTY/dead: argus-energy.

**`ngo_rights_extra.c` (13)** — DATA: access-now 15, article19 9, bellingcat-3 10,
citizen-lab 10, cpj 10, eff-deeplinks 50, forbidden-stories 10, freedom-house 10,
insight-crime 11, privacy-intl 10. DEAD: global-witness, rsf, transparency-intl.

**`threatfeeds_world.c` (8)** — DATA: FEODO 5, OPENPHISH 200, SPAMHAUS 200, **SSLBL 200
(FIXED, was 27)**, THREATFOX 200, TOR_EXITS 200, URLHAUS 200. KEY_GATED: **MALWAREBAZAAR
(FIXED)**.

**`gov_enforcement.c` (6)** — DATA: **PMDA_APPROVALS 30**, **MHLW_LABOR_VIOLATIONS 30**,
**MLIT_NEGATIVE_INFO 23**, **FSA_ENFORCEMENT 26** (all FIXED; FSA still `labels-only`).
WAF: CAA_ENFORCEMENT, JFTC_ENFORCEMENT.

**Portal-status placeholders (10, all `labels-only`, 1 row each, 0 geo)** —
bosai-volcano-cam, comiket-events, jcg-navarea, jpo-jplatpat, nict-atlas,
securitytrails-history, tepco-outage, tmp-protests, tsr-closures (`reachable=0`), vnet
(`reachable=1`).

**Overpass, ENV_BLOCKED (6)** — gas-stations, mlit-bridge, osm-transport-trains,
radar-sites, utility-poles, wineries-craftbeer.

**Everything else** — DATA `real+complete`: **alos-palsar 50/50 geo**, BR_CNPJ 1,
CA_PARLIAMENT 20, **cam-skylinewebcams 101/101**, **chaos-bugbounty-jp 8**, COURT_WORLD 25,
DBPEDIA 15, DE_POSTAL 1, dnstwist-jp-targets 50, EARTHQUAKE_MONITOR 20/20, **flight-adsb
90/90**, geothermal-springs 6867/6867, **GREYNOISE_COMMUNITY 1**, **IATI_REGISTRY 25**,
IPAPI_GLOBAL 1/1, **jamstec-argo 84/84**, **jma-warnings 16/16**, **kyodo-rss 25**,
LITTLESIS 10, MAC_VENDOR_LOOKUP 1, **mastodon-jp-instances 51**, **my-jvn 50**, NL_ADDRESS
10/10, **OCHA_FTS 25**, OPENDATA_SOCRATA 65, OURAIRPORTS 40/40, PEERINGDB_GLOBAL 3,
phishing-feeds-jp 271, PRTIMES 30, REDHAT_CVE 1, REVERSE_DNS 1, RIPESTAT_GLOBAL 1,
SHODAN_INTERNETDB 1, strava-heatmap-bases 14/14, tdnet-disclosure 100, WIKIDATA_SPARQL 40,
WIKIMEDIA_COMMONS 25, WIKINEWS 17.

DATA `real+thin`: ADDRESS_RESOLVER 1/1, DARK_WEB_MONITOR 1 (real Ahmia onion hit, no page
title/snippet), himawari-realtime 1, LATAM_REGISTRY 1, npa-cyber-threat-obs 2/1 (4 graphs
fetched, 2 emitted), PORT_SCANNER 2, US_STATES_SOS 3, WAYBACK_MACHINE 39,
WORLDBANK_PROJECTS 25 (no geo despite country data). `labels-only`: estat-crime 1.

KEY_GATED: ABUSEIPDB_CHECK, abuseipdb-jp, bgp-tools-jp, cam-youtube_live, EPO_OPS,
facebook-geo, google-dorking, instagram-locations, IPQUALITYSCORE_IP, LENS_PATENTS, odpt-bus,
PULSEDIVE_INDICATOR, RELIEFWEB, shodan-iot, softbank-crowd.

DEAD: BGPVIEW, PATENTSVIEW, GR_GEMI. WAF: FSA_FINBIZ_REGISTRY, GOOGLE_BOOKS (429),
yahoo-news-jp-rss.

EMPTY: ADSB_FI, COMMONCRAWL_CDX, HEXDB_AIRCRAFT (404 for both a reg and a hex — likely
dead), ITOWNPAGE, jma-ocean-temp, mlit-p11-bus-stops, NEWS_AGGREGATOR, NZ_COMPANIES,
PASTE_SITE_SEARCH, WIFI_LOOKUP.

TIMEOUT: gtfs-jp (works, just >180 s), unified-highway (144 s, 0 rows — aggregates the
Overpass-blocked highway feeds).

Build slot `a9` green; every fix re-run against the live upstream and the real output read.
