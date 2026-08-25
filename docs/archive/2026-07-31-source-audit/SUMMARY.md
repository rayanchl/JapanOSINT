# OSINTsaas source audit — consolidated summary

Run: 2026-07-31, 19:26 → 22:00 JST-equivalent local (CEST). 2,189 testable sources across
10 auditor slices. Per-slice detail in `report_01.md` … `report_10.md`.

## Coverage

All 10 slices completed. Tree builds clean (`make exit=0`). `native/lib/` untouched by
auditors (0 files); `native/core/` unchanged from its session-start baseline (11 files, all
pre-existing work). All audit edits are confined to `native/collectors/`: **182 files,
+5,703 / −2,459**.

## Cross-cutting defects in shared code (READ-ONLY to auditors — these need your decision)

Ranked by blast radius. Every one was found independently by 2+ auditors unless noted.

| # | location | defect | impact |
|---|---|---|---|
| 1 | `lib/rss_atom.c:180` | `published_at` stored as raw RFC-822 | `intelapi.c`, `exportapi.c`, `simhash.c` all `ORDER BY` this column as **text**. `'F' > '2'`, so every RSS row outranks every ISO-8601 row regardless of date, and RSS rows sort among themselves by weekday name. **The intel timeline is not chronological for any consumer.** |
| 2 | `lib/rss_atom.c:172-173` | `strncpy(summ, desc, 240)` cuts on a **byte** boundary, splitting UTF-8 | ~1,000+ rows of invalid UTF-8 in `summary`, mirrored into FTS by `core/intel.c:214`. This is the sweep's `DB_ERROR` verdict. Fix: back off any `0x80..0xBF` continuation byte. |
| 3 | `lib/rss_atom.c:165-171` | `properties_json` built in a fixed `char props[512]`, `snprintf`-truncated | ~5,200+ rows of malformed JSON (guids reach 1,930 chars). Any `cJSON_Parse` of `properties` fails. Fix: keep `cJSON_PrintUnformatted`'s heap string. |
| 4 | `core/httpclient.c:129` | HEAD sent via `CURLOPT_CUSTOMREQUEST` **without** `CURLOPT_NOBODY` | Every HEAD stalls until timeout. **All 73 "portal status" sources report their site unreachable** and burn 8 s each, though `curl -I` with the same UA returns 200. One-line fix. |
| 5 | `core/httpclient.c:123` | `CONNECTTIMEOUT_MS` hard-coded to `10000L`, ignoring the caller's `timeout_ms` (honored on line 122) | For HTTPS this covers the TLS handshake. `api.gdeltproject.org` handshakes in 9.6–10.8 s → unreachable regardless of the budget requested. Kills GDELT_TV and NEWS_AGGREGATOR's GDELT leg. Fix: `min(timeout_ms, 20000)`. |
| 6 | `lib/geojson.c:235` | `geom ? cJSON_PrintUnformatted(geom) : NULL` — no `cJSON_IsNull` guard | An explicit JSON `null` geometry serialises to the literal 4-byte string `"null"`, which passes every `geometry IS NOT NULL AND geometry<>''` test. 2,091 rows in slice 04 + 243/256 rows in slice 09. **Silently undoes the fix when an auditor replaces a fabricated pin with null.** |
| 7 | `core/camera_store.c:230-238` | `camera_upsert()` never sets `it.link` (nor `summary`/`published_at`/`tags`) though `properties.url` holds the camera page | **Every camera row in the fleet is a camera you cannot open.** Confirmed by 2 auditors across 4 camera sources. One-line fix at `:233`. |
| 8 | `lib/geojson.c:55` | featureless-ID rows uid'd by `sha1({geometry, properties})` | Any collector carrying a *changing measurement* in properties mints a new row every run instead of updating — upsert becomes unbounded growth. |
| 9 | `lib/geojson.c:212` | `T_TITLE = {title, name, name_ja, label}` only | GeoJSON sources naming the field otherwise persist title-less rows — blank in every UI. Cause of the `NO_TITLE` verdict. |
| 10 | `lib/overpass.c` | returns **−1 for an honest empty**; 4 endpoints tried serially with no overall budget; `seen_has()` is a linear scan over a 200,000-slot array (O(n²)) | Quarantines working sources; the serial retry is the `TIMEOUT` verdict's main cause. |
| 11 | `collectors/sources/_jp_osint.inc` | `jo_emit_anchors()` — no charset sniffing (Shift_JIS persisted as UTF-8), no left-trim, `it.lang` hardcoded `"ja"`; with `href_must=NULL` it harvests whole-page nav chrome | **134 collectors include this file.** 26 call sites across 18 files pass a NULL in that argument list. Filed `本文へ移動` ("skip to content") as a financial-penalty record. |
| 12 | `lib/rss_atom.c:157` | Atom `<author>` stored as raw inner XML | `author = "<name>/u/AutoModerator</name><uri>…</uri>"`. Same function leaves body/summary as raw HTML with unresolved entities. |
| 13 | `lib/rss_atom.c` | CDATA never unwrapped | China Daily titles persist as `<![CDATA[ … ]]>`, links never parse — 100 emitted, 41 persisted. |
| 14 | `lib/rss_atom.c:119` | UA is `japanosint-collector` | ReliefWeb blocklists any UA containing `japanosint` (406). Remedy is a descriptive UA + contact address, **not** browser spoofing. |
| 15 | `lib/probe.c:13` / `lib/probe.h` | the portal-probe pattern itself | ~73 files. Emits one row of `{operator, reachable}` with a hardcoded title — counts as DATA in every sweep while carrying no measurement. Compounded by #4. |
| 16 | `core/scheduler.c:70` | `status = rc == 0 ? "ok" : "error"` → `anomaly_detect()` | Any `run()` returning a row count quarantines the source **because it worked**. See below. |

## The quarantine-on-success class (defect #16)

A `run()` returning its emitted count is logged as a failed run on every execution. A
sweep-wide scan of `rc > 0` found 11 sources / 317 rows; auditors found 6 more that the scan
could not see, because they emit nothing under the sweep's pivot:

`CRIMINAL_RECORDS`, `LEGAL_SEARCH`, `BANKRUPTCY_SEARCH`, `PERSON_SEARCH`, `LEI_SEARCH`,
`BGP_LOOKUP`, `VAT_VALIDATOR`, `PHONE_LOOKUP`, `PHONE_REPUTATION`, `SOCIAL_EMAIL`,
`SOCIAL_USERNAME`, plus `IOC_LOOKUP`, `PEP_CHECK`, `WATCHLIST_CHECK_NEW`, `EXIF_EXTRACTOR`,
`PDF_ANALYZER` (**inverted**: `return rc >= 0 ? 1 : 0`, so success returned 1 and failure
looked healthy), `CREDENTIAL_LEAK_SEARCH`. All fixed.

The security cases are the sharp ones: an IOC lookup that **found** the indicator, or a PEP
screening that **matched** a sanctioned person, was recorded `status=error` and quarantined —
while a clean result looked healthy.

Related latent form: `return n > 0 ? 0 : -1` quarantines on an honest empty. Worth a fleet
grep for `? 0 : -1`.

## Fabricated geolocation — found and removed

Every one of these invented coordinates for things that have no location:

| source | what it was doing | after |
|---|---|---|
| `peeringdb-jp` | every IXP **and every ASN** stacked on Tokyo Station; coordinate-less facilities fell back to the same point. PeeringDB publishes no IXP coordinates at all | 1,112 rows, **0 fake pins** |
| `cisa-kev-jp` | 46 "actively exploited vulnerability" markers on ~10 Tokyo office buildings, from a `VENDOR_GEO` table of vendor head offices compiled into the binary, Tokyo Station as fallback | geometry removed |
| `poc-in-github` | 94 GitHub repos pinned at Tokyo Station | geometry removed |
| `classifieds` | **random ±0.015° jitter around a prefecture centroid, moving every run** | jitter removed, honest `geo_precision: "area-centroid"`, 285 real rows |
| `wanted-persons` | each wanted person scattered on a **jittered ring** around the police-HQ coordinate, while name/age/crime were cross-indexed from three *independent* regex match lists (a case could carry another person's charge) | upstream 404s; documented, not repaired |
| `mastodon-jp-instances` | 51 invented pins stacked on Tokyo Station | 0 geo |
| `trickest-cve` | 16 fake Tokyo pins | removed |
| `osm-changesets-jp`, `flight-adsb` | Tokyo Station fallback for bbox-less / fix-less records | removed |
| `hudson-rock-jp` | all 34 rows on one hardcoded Tokyo point | **left, flagged — product call** |

And the inverse — a source whose coordinates were real but wrong:

**`jma-intensity` was publishing every Japanese earthquake epicentre in the eastern
Mediterranean.** `split_plus()` read the ISO-6709 `cod` field transposed (`parts[1]` as lat,
`parts[0]` as lon), so every quake landed at lat 36.0, lon ≈30–45 — off the coast of Turkey.
Fixed and verified: 88 rows, all correctly in Japan.

## The "shows names, not data" class

~73 files fleet-wide follow the portal-probe pattern; auditors counted **~60 in their own
slices** (8 + 8 + 10 + 11 + 9 + 5 + 7). Each emits one row whose title and summary are
string literals in the .c file, and whose only fetched datum is `reachable: true|false`.
They score as DATA in every row-count sweep. Several oversell in their summary text ("Every
licensed AM/FM/TV transmitter" — zero transmitters). Because of defect #4 most now report
`reachable=false` against portals that answer 200 to `curl -I`.

Adjacent: `pref-police-crime` (36 `directory-only` rows, `"rows_available": 0`),
`moj-crime-whitepaper` (3 links + a pin on the MOJ building, no statistics),
`CUSTOMS_TRADE` / `POLITICAL_FUNDS` / `GEPS_PROCUREMENT` / `LOCAL_TENDERS` (index-page anchor
text, not statistics), `mlit-river` (titled "River Water Levels", emits OSM station
locations with no water level), `twitter-geo` (re-`SELECT`s existing rows; no fetch at all).

## Notable single-source recoveries

- **`jma-typhoon-json`** — a live category-TY typhoon (Dolphin, 910 hPa, 55 m/s) was
  **invisible** during the audit: it read coords off `targetTc.json`, an index carrying none.
  0 features for every storm since the port. Now pinned with a 5-day forecast track.
- **`OFAC_CRYPTO`** — checked a 404'd `sdn_advanced.xml`, so the sanctions check **never
  matched anything**. Now resolves to real SDN records.
- **Sanctions screening was three separate bugs**: substring matching made `PUTIN` hit
  `COM**PUTIN**G` (3 unrelated entities reported as hits) while `Vladimir Putin` matched
  *nothing* (OFAC stores surname-first) → CLEAR for a listed head of state; XML lists
  (UN/Canada) printed one field per line so a full name could never match; UK OFSI screened
  the *narrative* text, returning 708 records for `Putin` (everyone whose Statement of
  Reasons mentions him) where only 6 carry it as a name.
- **The ten power TSOs were reporting the minute of a timestamp as megawatts** —
  `strtok_r(",")` collapses empty CSV fields, sliding the column index. Now TEPCO 27,440 MW,
  Kansai 14,380 MW.
- **`UK_REGISTRY` segfaulted**: registry search-URL templates are not printf formats (Land
  Registry's carries `et%5B%5D=…`) and were fed to `snprintf()`; `%5B` is an undefined
  conversion reading a garbage vararg.
- **`gdacs`** reported `records=305` while storing 38 rows with 15 pins (one feature per
  geometry layer, last-layer-wins) → 39 events, all 39 pinned.
- **`SSLBL_GLOBAL`** discarded 87% of every run: the CSV parser stopped at the space inside
  the timestamp, so every certificate's title *and* dedupe key was a bare date — 200 SHA1s
  collapsed onto 27 rows.
- **`wifi-hotspots-freespot`** emitted one row titled `All rights reserved, Copyright ©
  FREESPOT 2014` (the page footer) → **11,904 geo-pinned hotspots**.
- **`atlas-jp`** 0 → 309 probes (RIPE Atlas v2 moved coords into `geometry`);
  **`jaxa-earth`** 0 → 119; **`note-com-profiles`** 0 → 100; **`HISTORICAL_WHOIS`** 0 → 124;
  **`emsc-quakes`/`iceland-quakes`** 0 → 300 each with hypocentres; **27 `gnews-*-nation`**
  0 → ~100 each.
- **`greynoise-jp`** was fabricating "Quiet" verdicts from discarded 404 bodies.
- **`npa-traffic-accidents`** — 46,216 rows with NULL title, no link, no date. All three fixed.

## Duplicate / mislabelled sources

**129 Google News sources are duplicate feeds under distinct country names** (54 in slice 01,
75 in slice 02). Google discards `gl=` when the locale has no edition and serves a generic
one — `gnews-dk-world` returns `<language>no</language>`, `gnews-is-world` returns
`<language>en-US</language>`, `gnews-ir-technology` returns US tech news tagged `iran` with
`language="fa"`. Verified at row level: Jaccard 1.00 against the sibling, 0.00–0.04 against
controls. Since `uid = source_id|guid` there is no cross-source dedupe, so each article is
stored 7–8 times under 7–8 country tags. **Retiring them is a product decision.**

Also: `google-my-maps` runs the byte-identical Overpass query as `famous-places`;
`kyiv-independent`/`-2`, `scmp-china`/`-china3`, `the-diplomat`/`-2` are duplicate pairs;
`DNS_RECORDS` is superseded by `DOH_RESOLVE`.

## Environment caveats — NOT defects

- **Overpass**: six auditors measured connection refusal / hangs and marked ~90 sources
  ENV_BLOCKED. **Slice 06 disproved the IP-ban theory**: the endpoints simply need more than
  the 60 s client budget. Proven with real data — ski-resorts 469 rows/138 s, marine-traffic
  559/100 s, odpt-transport 10,178/162 s, refineries 8/64 s. So most ENV_BLOCKED verdicts in
  this audit are a **client timeout artifact**, and `lib/overpass.c` needs an overall budget
  (defect #10), not another mirror. Only `resas-industry` genuinely fails (rc=−1 at 286 s;
  its query needs tiling). Note `overpass.osm.ch` answers in 0.14 s but is a Switzerland-only
  extract — 0 elements for a Tokyo query — so it is **not** a valid addition to `ENDPOINTS`.
- **Self-inflicted rate limits from the bulk sweep**: `BLOCKCHAIR` returns HTTP 430 *"IP
  temporarily blacklisted due to exceeding usage of API resources"*; `STOOQ_QUOTES` 404s to
  every UA; PeeringDB throttled 2 of 3 endpoints. The collectors are correct — re-test from a
  different address.
- **No credentials configured** (~86 empty vars): ~150 sources across the fleet are honestly
  KEY_GATED and were correctly left alone.
- **Wrong sweep pivots** produced dozens of false `EMPTY` verdicts. Auditors overturned ~50
  by supplying an entity of the right *kind* (ICAO24 hex, VIN, indicator code, DOI, postal
  code…). **The registry should declare a pivot type per on-demand source.**

## Decisions needed from you

1. `core/camera_store.c:233` — the one-line camera `link` fix (fleet-wide).
2. `core/httpclient.c:129` — add `CURLOPT_NOBODY` for HEAD (unblocks 73 portal probes).
3. `core/httpclient.c:123` — honor the caller's connect timeout.
4. `lib/geojson.c:235` — `cJSON_IsNull` guard (otherwise fake-pin removals silently regress).
5. `lib/rss_atom.c` — the three defects at `:165`, `:172`, `:180`, plus a max-items cap
   (`msrc-blog` pulls 4,995 items / 3,539 rows every run).
6. Whether to retire the 129 duplicate Google News sources.
7. Whether the ~73 portal-probe sources should be built out, retired, or excluded from
   "DATA" in dashboards.
8. `bom-au-warnings`: its only live replacement API returns *"You must not use, copy or share
   it."* — the auditor declined to wire it up. Your call.
9. `npa-special-fraud`: 25 national statistics pinned at NPA headquarters — keep or drop the
   pin.
10. `hudson-rock-jp` (34 rows on one Tokyo point) and `police-crime` (registered as `crime`,
    emits police *stations* not incidents) — rename/redesign decisions.

## Verification gaps — stated, not papered over

- `odpt-transport`'s title fix could not be re-verified live (the confirming run hit "no
  elements" after 299 s on a saturated Overpass mirror). The 10,178-NULL-title measurement is
  real; the fix follows a pattern proven on `marine_traffic.c`. Re-run when Overpass is idle.
- `police-crime`'s fix is not re-testable while Overpass is unavailable.
- `job-boards`' fix is correct in code but unverified for the same reason.
- Slice 03's 10 Overpass sources were reported BLOCKED/unverifiable rather than broken.
