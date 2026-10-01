# Slice 08 — 206 sources across 82 files

## Summary

| verdict | n |
|---|---|
| DATA | 97 |
| DEAD_UPSTREAM (404/retired/NXDOMAIN, curl-verified) | 28 |
| KEY_GATED (correct behaviour) | 24 |
| EMPTY (2xx, zero records) | 19 |
| WAF_BLOCKED (403/429/503) | 16 |
| ENV_BLOCKED (Overpass unreachable from this host) | 14 |
| **PLACEHOLDER** (emits only a "portal reachable" row) | **8** |

`PLACEHOLDER` is broken out of `DATA` deliberately: those sources return rows and look
green on a dashboard while carrying no measurement.

Five most important findings, most severe first:

1. A positive `run()` return quarantines a working source — likely a fleet-wide pattern.
2. Two sources were writing non-UTF-8 into `intel_items` via shared `_jp_osint.inc`.
3. `wifi-hotspots-freespot` was scraping a page that no longer holds the data.
4. Eight sources are reachability probes wearing a data source's clothes.
5. All four Overpass endpoints are unreachable from this host — 13 sweep TIMEOUTs are
   environment, not broken collectors.

### 1. A positive `run()` return quarantines a working source

`core/scheduler.c:70` does `status = rc == 0 ? "ok" : "error"` and feeds it to
`anomaly_detect()`. `SOCIAL_USERNAME` and `PERSON_SEARCH` returned the *emitted row
count*, so 91 and 67 genuinely-fetched rows were logged as a failed run on **every**
execution. Fixed in both.

Worth grepping the whole tree for `return t;` / `return n;` at the end of a `run()`.

### 2. Two sources were writing non-UTF-8 into `intel_items`

`CUSTOMS_TRADE` (customs.go.jp) and `POLITICAL_FUNDS` (soumu.go.jp) serve Shift_JIS with
no charset header. The shared `jo_emit_anchors()` in `collectors/sources/_jp_osint.inc`
has no transcode step → un-decodable titles, corrupted FTS. Fixed locally in
`gov_money.c`.

**`_jp_osint.inc` is included by 134 collectors — the sniff-and-transcode belongs there,
centrally.** POLITICAL_FUNDS was *not* flagged by the sweep (wrong pivot filtered every
row out), so the corruption is wider than the sweep shows.

### 3. `wifi-hotspots-freespot` was scraping a dead page

It emitted one row titled `All rights reserved, Copyright © FREESPOT 2014` — the page
footer. The real directory is 47 per-prefecture KSGMap XMLs the picker links to, each
`<item>` carrying lat/lon. **1 junk row → 11,904 geo-pinned hotspots.**

### 4. Eight sources are reachability probes wearing a data source's clothes

`jcab-notams`, `jma-uv`, `jr-west-delay`, `michi-no-eki`, `ntt-fiber`, `steam-jp-users`,
`yahoo-crowd-map`, `influenza-surveillance` each fetch a portal solely to record
`reachable: true/false` and emit one hardcoded descriptive row. Honest in wording, but not
data sources, and none pins. This is the "shows names, not data" class. Not fixed — each
needs a real upstream chosen.

### 5. Overpass is unreachable from this host

`curl -4 https://overpass-api.de/api/status` → refused in 70 ms; kumi / private.coffee /
openstreetmap.ru time out; `openstreetmap.org` returns 200. IPv4 *and* IPv6.

**The 13 sweep "TIMEOUT" verdicts are this, not 13 broken collectors.** It degraded
*during* the audit: `embassies` scored DATA/164 in the 19:26 sweep and now returns
`no elements`. Nothing Overpass-backed could be functionally verified.

## Fixes applied

12 files, all in-slice, all re-run and read.

| file | source(s) | bug → fix | re-test |
|---|---|---|---|
| `wifi_hotspots_freespot.c` | wifi-hotspots-freespot | scraped dead `<tr>/<td>` table → follow picker to `gmap/xml/*.xml`, parse `<item/>` | **1 → 11,904 rows, 11,904 geo** |
| `social_username.c` | SOCIAL_USERNAME | returned row count as rc → `return t>=0?0:-1` | rc −1 → **0**, 91 rows kept |
| `people_finder.c` | PERSON_SEARCH, COMPANY_SEARCH | same | rc −1 → **0**, 67 rows kept |
| `gov_money.c` | CUSTOMS_TRADE, POLITICAL_FUNDS, GEPS, LOCAL_TENDERS, ASSET_DISCLOSURE | SJIS-as-UTF-8 + nav anchors → local `gm_fetch_utf8()` (strict UTF-8 validate, else `csv_decode_sjis`) + `#`-anchor filter | DB error gone; titles now `貿易統計検索ページ`, `ご意見・ご提案` |
| `ooni_jp.c` | ooni-jp | all 200 rows title-less; `published_at` always NULL (read `test_start_time`, a key OONI doesn't return); `scores.blocking_*` dropped | 200 titled rows, real dates, censorship scores carried |
| `court_records.c` | COURT_RECORDS | CourtListener REST **v3** retired (403/401) → v4 search, `v3/recap/`→`v4/search?type=r` | **0 → 10** real opinions w/ docket, citation, URL |
| `suumo_rental_density.c` | suumo-rental-density | 47 features title-less | 47 titled, 47/47 geo |
| `aviation_world2.c` | ADSB_LOL, AIRPLANES_LIVE, OPENSKY_STATES | space-padded 8-char callsign untrimmed | `APJ877   · A20N` → `APJ877 · A20N` |
| `gov_agencies_world.c` | us-doj-news, iaea-topnews, itu-news | 404 feed URLs replaced | 25 / 15 / 12 rows |
| `thinktanks_world.c` | csis, merics, hudson, heritage | 404 feed URLs replaced | 10 / 20 / 10 / 20 rows |
| `nerv_feed.c` | nerv-feed | `n>0?0:-1` — a quiet hazard feed reported as error → `n>=0?0:-1` | honest-empty path fixed |
| `gsi_active_fault.c` | gsi-active-fault | logged "honest empty" then returned **−1** → separate fetch-failure from empty | log now truthful |

## Findings not fixed (with reason)

| source / file | issue | why not fixed |
|---|---|---|
| `lib/overpass.c` | (a) `overpass_collect()`/`_tiled_`/`_ways_` **return −1 for an honest empty** — the exact quarantine bug the brief names, in shared code affecting every slice; (b) four endpoints tried **serially** at 60–120 s each with no overall budget (144 s observed on airport-infra); (c) `seen_has()` is a linear scan over a 200,000-slot array → O(n²) dedupe, a timeout cause independent of the network | `lib/` read-only; affects every slice |
| `collectors/sources/_jp_osint.inc` | no charset sniffing, no nav-anchor filter | 134 consumers — needs the central fix, not 134 workarounds |
| `lib/rss_atom.c` | raw RFC-822 `published_at` undecoded (unreliable timeline ordering); `body`/`summary` hold **raw HTML**, `summary` a blind 240-byte truncation (CSIS/MERICS/Hudson summaries are mid-tag markup, bodies reach 70 KB of Drupal chrome); `tag_text()` isn't nesting-aware, so `saigai-info` author comes out as the literal `<name>山形地方気象台</name>` | `lib/` read-only |
| 28 DEAD_UPSTREAM feeds | verified 404/NXDOMAIN with both collector and browser UA, with and without redirects; 2–3 replacements probed per site. Working replacements found for 7; the rest have no public feed any more | no honest replacement exists |
| `gsi-active-fault` | URL was marked "unverified candidate" in-source and has **never** worked | no replacement found |
| `nerv-feed` | host `unii-api.nerv.app` is NXDOMAIN | dead |
| `note-com-trending` | both endpoints 404 | dead |
| `suumo-rental-density` | title fix landed, but the listing count no longer parses for **any** of the 47 prefectures — 47 pins carry a null measurement | the fix makes it visible rather than silent; re-parse is a separate job |
| `CUSTOMS_TRADE`, `POLITICAL_FUNDS`, `GEPS_PROCUREMENT`, `LOCAL_TENDERS` | still **labels-only** after the encoding fix: index-page anchor text, not statistics/tenders/donations. Real data is behind e-Stat and the GEPS query forms | re-design, not a bug fix |
| `wifi-hotspots-freespot` (74 s), `suumo-rental-density` (87 s) | now trip `duration_outlier` (10 s threshold) because they fan out over 47 sequential requests | work is legitimate — needs a per-source threshold or concurrency |
| `JP_POSTAL`, `CELESTRAK_TLE`, `SATNOGS`, `geospatial-jp-ckan`, `saigai-info` | geo-less but physical — upstream supplies no coordinates (zipcloud returns none; TLEs have no ground point; catalogue records; JMA area codes) | deriving coordinates would be fabrication |
| `IP_REPUTATION`, `MALWARE_ANALYSIS` | correctly key-gated but emit **no log line at all** — an operator can't tell "gated" from "crashed" | trivial but belongs with the central gating decision |

## Sweep verdicts overturned

The sweep's on-demand pivots were wrong for a whole cluster. With correct entities,
`ADSB_LOL`/`AIRPLANES_LIVE`/`OPENSKY_STATES` (need an ICAO24 hex, not "Tokyo"),
`COINGECKO_SEARCH` (coin name), `DEFILLAMA_TVL` (protocol), `ETHPLORER_ADDR` (ETH
address), `JP_POSTAL` (postal code), `FRANKFURTER_FX`, `CELESTRAK_TLE`, `SATNOGS`,
`PUBMED` all return real data — 10 false EMPTY/WAF verdicts.
