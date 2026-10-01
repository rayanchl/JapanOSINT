# Slice 07 — 206 sources (build slot a6)

## Summary

| verdict | count |
|---|---|
| DATA | 120 (incl. the 17 repaired tonight) |
| KEY_GATED | 22 — all verified, each logs `gated (no …)` |
| WAF_BLOCKED | 15 — 403 to any UA, +1 IP blacklist |
| ENV_BLOCKED | 14 — 13 Overpass + `unified-ais-ships` |
| DEAD_UPSTREAM | 9 — 404/410/expired-TLS, no replacement found |
| STUB (portal-probe) | 11 — emits "is this site up?" instead of data |
| EMPTY | 8 — honest, or wrong sweep pivot |
| TIMEOUT | 1 |
| CRASH / DB_ERROR | 0 |

### Five findings that need attention

**1. `unified-ais-ships` never returns and blocks its 300 s scheduler slot.** No `[sched]`
line, no stderr, killed at 150 s. Root cause is *not* in that file: it `unified_capture()`s
three sub-collectors and **`marine-traffic` and `maritime-ais` each hang >50 s on their own**
(`vessel-finder` is honestly key-gated). Those two files are in other slices. Anything fusing
them inherits the hang. `lib/unified.c` needs an overall budget.

**2. A "portal probe" family fakes coverage.** 11 sources here (73 files fleet-wide
`#include "lib/probe.h"`) emit one row per portal saying *"here is a URL and whether it
answered"*: `fsa-crypto-exchanges`, `mic-elections`, `ndl-search`, `saibansyo-rulings`,
`yahoo-chiebukuro`, `nhk-relay-towers`, `jr-east-delay`, `vics-traffic`, `whoisxml-reverse`,
`shinkansen-status` (5 rows), `grid-usage-realtime` (10 rows, exactly 1 with a real number).
Not fabricated — but placeholders that count as DATA in every sweep, and 8 of 11 report
`reachable=false` against portals that are themselves 403/404 now. `ndl-search` is the
cheapest to make real: `iss.ndl.go.jp/api/opensearch` returns 430 KB of live records today.

**3. A live typhoon was invisible.** `jma-typhoon-json` read `lat`/`lon` off
`targetTc.json`, which is only an index (`{"tropicalCyclone":"TC2615",…}`) with no
coordinates — 0 features for every storm since the port. Category-TY *Dolphin* (910 hPa,
55 m/s) was active during the audit. **Fixed** → 1 row, pinned, with intensity + 5-day
forecast track.

**4. Silent row loss via uid collisions.** `GVP_VOLCANOES` gave all 22 weekly volcano
reports the same `remote_key` (every `<link>` is the same `reports_weekly.cfm`) → 21 of 22
overwritten; it also ignored the `<georss:point>` on every item, so erupting volcanoes never
pinned. Same class in `AT_POSTAL` (4 fetched → 1 persisted). Both fixed and re-verified.

**5. Dead feed URLs quietly quarantining sources.** 9 RSS sources pointed at URLs that now
404/410; `rss_collect()` returns −1 on non-2xx so each tripped `anomaly_detect()` hourly.
Live replacements found and verified for all 9; 14 more are genuinely WAF-blocked and 4
genuinely dead.

### Read-only bugs (reported, not patched)

- **`lib/geojson.c:212`** `T_TITLE={title,name,name_ja,label}` — cause of both NO_TITLE
  sources here. Consider widening (`area`, `station_name`, `domain`); fixed in-collector.
- **`lib/geojson.c:55`** no-NATIVE_ID_KEY features are uid'd by `sha1({g,p})`. Any collector
  carrying a *changing measurement* in `properties` mints a new row every run instead of
  updating — silently converts upsert into unbounded growth. `hudson-rock-jp` would have hit
  this the moment real counts were carried in (a `uid` property was added).
- **`core/scheduler.c:70`** + the `return n > 0 ? 0 : -1` idiom = an honest empty quarantines
  the source. Fixed the two in-slice (`note-com-profiles`, `jaxa-earth`); worth a fleet grep
  for `? 0 : -1`.
- **anomaly `duration_outlier` threshold 10 s** now trips on `jaxa-earth` (47 s, 119 catalog
  children, daily) and `moj-crime-whitepaper` (10.2 s). Both legitimately slow.
- `lib/rss_atom.c` 240-byte truncation / `char props[512]` / RFC-822 `published_at` —
  already reported; **89 sources in this slice are affected**.

## Fixes applied

12 files, all in-slice; build green.

| file | source(s) | bug | re-test |
|---|---|---|---|
| `hudson_rock_jp.c` | hudson-rock-jp | NO_TITLE (34 rows); asked for 6 keys **that don't exist in the v2 API** so 5 of 9 fields were permanently `null` while real counts/dates/stealer families were dropped; emitted null-only rows on fetch failure | ✔ 34 titled rows, e.g. `mufg.jp — 21025 compromised credentials (13 employee, 20979 user)`, `published_at 2026-07-30T…`, stable `uid`=domain |
| `jma_typhoon_json.c` | jma-typhoon-json | read coords from an index that has none | ✔ `TY Dolphin (No. 2613)` @19.8,159.2, 910 hPa, forecast track |
| `jaxa_earth.c` | jaxa-earth | all 3 `/api/collection*` endpoints 404 → `rc=-1` daily | ✔ 0 → **119 rows** via the live STAC catalog, full metadata (bbox/temporal/keywords/license/providers/bands) |
| `note_com_profiles.c` | note-com-profiles | `api/v2` retired (HTML 404); `followerCount`/`noteCount` don't exist in v3; `n>0?0:-1` | ✔ 0 → **100 rows**, follower counts populated |
| `hazard_world.c` | GVP_VOLCANOES, GDACS | shared `<link>` as key; georss ignored | ✔ GVP 1 → **22 rows / 22 geo**; GDACS unchanged 60/60 |
| `crypto_world.c` | OFAC_CRYPTO | `sdn_advanced.xml` 404 → sanctions check never matched | ✔ `12QtD5BF…` → `OFAC-sanctioned XBT address — YAN, Xiaobing` (SDN.CSV, 5.6 MB vs 125 MB; party name now from the matched record, not a "nearest preceding name" guess) |
| `crypto_world.c` | TRON_SCAN | tronscanapi 401s keyless | ✔ TronGrid: `1082580.39 TRX, 485 TRC-20 tokens` |
| `country_geo2.c` | AT_POSTAL, CH_POSTAL | `district` unreachable behind `municipality` in the same fallback list, and it's the only distinguishing field → 4 fetched, 1 persisted | ✔ 1 → **4 rows** |
| `intel_geoint_conflict.c` | cfr / kyiv-independent / maritime-executive / rferl | dead feed URLs | ✔ 24 / 100 / 85 / 20 rows |
| `world_outlets_2.c` | irish-times / mail-guardian-2 / kyiv-independent-2 / nzherald / brussels-times | dead feed URLs (nzherald needed `?outputType=xml`; brussels-times feed lives on the `api.` host) | ✔ 100 / 50 / 100 / 10 / 10 rows |
| `police_crime.c` | police-crime | NO_TITLE (9,903 rows: name only under `area`); `incident_id` is the **array index** so the sha1 uid re-keys whenever Overpass reorders; address/contact tags dropped | **not re-testable** — Overpass ENV_BLOCKED |
| `crypto_onchain.c`, `global_aleph.c` | DEFI_TRACKER, EXCHANGE_FLOW, ALEPH_SEARCH | returned 0 silently → misfiled EMPTY forever | ✔ now log `gated (no …)` → KEY_GATED |

## Findings not fixed (with reason)

| source(s) | issue | why not |
|---|---|---|
| 13 Overpass sources — `auto-plants`, `electrical-grid`, `estat-population`, `fire-station-map`, `lighthouse-map`, `onsen-map`, `osm-transport-ports`, `pachinko-density`, `petrochemical`, `public-cameras`, `stadiums`, `transmission-towers`, `water-infra` | all four endpoints time out from this host | ENV_BLOCKED — do **not** read as broken collectors |
| `nra-radiation` | genuinely dead: `radioactivity.nra.go.jp/cont/json/*` → S3 `403 AccessDenied`, `kankyo-hoshano.go.jp` no route, `emdb.jaea.go.jp` → 500. The site is now a Nuxt CMS whose `api/v1/*` serves page content, not dose rates | its `rc=-1` is honest |
| `mlit-n05-rail-history` | both `N05-2x.geojson` URLs 404; replacement is a zipped shapefile behind a different datalist page | needs the `lib/mlit_ksj` route, not a URL swap |
| `bluesky-jetstream-jp` | `ws_collect()` returns in **0 ms** with an 8 s window | failure is inside `lib/ws.c` — read-only |
| `caixin` | only advertised feed is 406 to everyone | dead |
| `caucasus-watch` | 404, no feed advertised | dead |
| `swissinfo` | 410 Gone | dead |
| `tuoitre` | **upstream TLS certificate expired** | dead |
| `fiji-times`, `nation-thailand` | feed URL returns the HTML shell | dead |
| `ES_BORME` | `buscar/borme.php` 404 — company search would become a date-range scrape | redesign, not repair |
| `STOOQ_QUOTES` | 404 to every UA — likely an IP block from tonight's sweep | re-test elsewhere before touching code |
| `BLOCKCHAIR` | HTTP 430 *"IP temporarily blacklisted due to exceeding usage of API resources"* | self-inflicted by the bulk sweep; collector is correct |
| 14 WAF_BLOCKED — `38north`, `isw` (301s to the blocked `understandingwar.org`), `addis-standard`, `daily-mirror-lk`, `euractiv`, `eurasianet`, `greek-reporter`, `ice-news`, `indian-express`, `irrawaddy`, `mees`, `politico-eu`, `the-east-african`, `thecable-ng` | 403 with both the collector UA and a full Chrome UA | no live alternative |
| `hudson-rock-jp` | pins all 34 rows on one hardcoded Tokyo point — a domain has no location, so the pin is invented and 34 rows stack on one coordinate | pre-existing and documented in the file header; left rather than silently drop the layer. **Product call** |
| `police-crime` | registered under `crime` and emits `incident_id`/`severity`, but every row is a police *station*, not an incident | a rename is a product decision |
| `moj-crime-whitepaper` | emits 3 links + a pin on the MOJ building, no statistics | labels-only; needs a real parser |

## Sweep verdicts that were wrong

Several EMPTY verdicts were just the wrong pivot, verified working with a correct entity:
`GITHUB_GISTS` (20), `GITLAB_USER` (1), `GRAVATAR_GLOBAL` (1), `KEYBASE` (1), `NO_ADDRESS`
(10 geo), `DK_ADDRESS` (10 geo), `CH_POSTAL`, `UK_POSTCODES`, `UK_POLICE_CRIME` (35 geo),
`SUN_TIMES`, `ELEVATION_LOOKUP`, `BTC_MEMPOOL`, `ENS_RESOLVE`, `ETH_BLOCKSCOUT`,
`SOLANA_RPC`, `EUROSTAT` (400), `IMF_DATA` (229), `FR_DATAGOUV` (20), and
**`WORLDBANK_INDICATORS` (261) — its pivot is an *indicator* code, not a country**.
`rappler-ph`'s RC_ERROR was transient (now 10 rows).

Note also `UK_POLICE_CRIME` emits 50 but persists 35 (upstream repeats ids), and there are
three duplicate-source pairs in the slice: `kyiv-independent`/`-2`, `scmp-china`/`-china3`,
`the-diplomat`/`-2`.

Per-source detail for all 206 is reconstructible from the run logs left in
`native/tests/audit/a6_o*.txt`. The healthy families not individually re-run are
`science_space_tech.c` (15/15 DATA, 5–75 rows each) and the ~37 remaining `world_outlets_2.c`
outlets, which the sweep showed at 10–588 rows.
