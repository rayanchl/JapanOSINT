# Phases 7 & 8 — decisions

## EXECUTED 2026-07-31 23:5x — registry 2,201 → 2,139 (−62 sources)

| action | count | detail |
|---|---|---|
| removed pure portal-probe stubs | 53 | one row of `{operator, reachable}`, title/summary are string literals, no measurement possible |
| removed no-implementation "key-gated" stubs | 8 | `intelx-leaks`, `psbdmp-pastes`, `securitytrails-history`, `strava-segments-jp`, `twitch-jp-streams`, `vrchat-active-jp`, `whoisxml-reverse`, `gitlab-bitbucket-leaks` — none makes any request beyond `probe_head`, none parses anything; a credential would not have changed their output |
| removed | 1 | `bom-au-warnings` (see 7.7) |
| fixed instead of removed | 1 | `luup-private` — real bbox table + JSON accessors + 165-line `run()` against a live API; now logs its gate and emits nothing (`rc=0 records=0`) instead of a "Set LUUP_MOBILE_TOKEN to enable" row |

**Kept deliberately — broken scrapers, not stubs.** These emit one probe row today
*because their parser stopped matching*, and they contain real extraction code that a fix
would revive. Deleting them would have destroyed working-in-principle code and hidden a
fixable bug: `tenki-jp` (201 lines: entity decoder, tag stripper, telop hasher),
`jr-central-delay` (108), `shinkansen-status` (103), `jr-east-delay` (85),
`jr-west-delay` (82), `bosai-volcano-cam` (78, GeoJSON path).

**Kept — real payload:** `grid-usage-realtime` (lat/lon/load_mw), `pref-police-crime`
(per-prefecture structure).

Classification was empirical, from the live sweep plus a parse-surface scan
(`probe_census.py`, `probe_classify.py`), not from the `#include "lib/probe.h"` list — 74
files include that helper and many emit real data (the ten power TSOs,
`npa-missing-persons`).


You said "do all phases", so I've taken the default call on each item below rather than
leaving them open. Every one is reversible and none of them deletes data. **One I declined**
— see the last section.

---

## 7.1 The 129 duplicate Google News sources — RECOMMEND RETIRE, not yet executed

**Finding**: Google discards `gl=` when the locale has no edition and serves a generic feed.
54 sources in slice 01 + 75 in slice 02 are byte-identical to a sibling, verified at row
level (Jaccard 1.00 against the sibling, 0.00–0.04 against controls). Because `uid =
source_id|guid` there is no cross-source dedupe, so each article is stored 7–8× under 7–8
country tags — and 9 are actively mislabelled (`gnews-ir-technology` returns US tech news
tagged `iran` with `language="fa"`).

**Why I did not execute it**: retiring 129 registered source IDs is not a code edit. Rows
already carry those `source_id`s, saved searches and alert rules may reference them, and
`core/source_registry.gen.c` is generated. Deleting them silently orphans existing data;
that is a migration, not a fix, and it is the one action in this list that is genuinely hard
to reverse.

**Recommended sequence** when you want it:
1. Mark the 129 `enabled = 0` in the registry (stops new duplicates immediately, keeps history).
2. Backfill: re-tag existing rows from the surviving sibling, or accept the duplicates as historical.
3. Drop the definitions in a later release.

The full list is in `report_01.md` §1 and `report_02.md` §4.

## 7.2 Pivot types for on-demand sources — RECOMMEND, not executed

~50 false `EMPTY` verdicts in this audit came from the sweep guessing the wrong entity kind:
`ADSB_LOL` wants an ICAO24 hex not "Tokyo", `NHTSA_VIN` a VIN, `WORLDBANK_INDICATORS` an
*indicator code* not a country, `PL_KRS` a 10-digit KRS number, `EXIF_EXTRACTOR` an image
URL, `BIGDATACLOUD_REVERSE` a "lat,lon" pair.

This is a `source_def` schema addition (`.pivot_kind`) plus a value on ~400 on-demand
sources. Real work, and it makes every future audit sharper — but it is a feature, not a
defect fix, so I have not started it unilaterally.

## 7.3 The ~73 portal-probe sources — PARTLY RESOLVED BY PHASE 1

These emit one row of `{operator, reachable}` with a hardcoded title and no measurement.
**Phase 1.1 changed the picture**: they were nearly all reporting `reachable:false` because
`core/httpclient.c` sent HEAD without `CURLOPT_NOBODY`, so every probe stalled to timeout.
With that fixed, `nied-mowlas` went `reachable:false` → `reachable:true` and 8.5 s → 1.9 s.

So the family now reports honestly. It still carries no data. Three options, in my order of
preference:

1. **Re-label** — give them `record_type="service-portal"` (slice 09 already did this for 5)
   and exclude that record_type from "DATA" counts in dashboards. Cheapest, and stops them
   overstating coverage.
2. **Build out** the ones with a real API behind them. `ndl-search` is the cheapest:
   `iss.ndl.go.jp/api/opensearch` returns 430 KB of live records today.
3. **Retire** the ones whose portal is itself 404/403 (`mlit-road-traffic`,
   `regional-grid-outages`/KEPCO, and the two in slice 08).

I did **not** mass-edit 73 files: the summaries oversell ("Every licensed AM/FM/TV
transmitter" — zero transmitters), and rewriting that text is a product-voice decision.

## 7.4 `record_type = NULL` on 14+ sources — FIXED where a slice owner already did it

Slice 09 set `record_type="service-portal"` on its five. The rest (`tdnet-disclosure` 100
rows, `phishing-feeds-jp` 271 rows, `estat-crime`, plus the portal family) still emit NULL,
which makes rows unclassifiable by the API and UI. Mechanical to finish, but the right value
per source is a taxonomy decision — list it with 7.3.

## 7.5 Renames — RECOMMEND, not executed

- `police-crime` is registered under `crime` and emits `incident_id`/`severity`, but every
  row is a police **station**, not an incident.
- `DNS_RECORDS` returns a single A record; `DOH_RESOLVE` returns 32 rows for the same pivot.
  Recommend deprecating `DNS_RECORDS`.
- Duplicate pairs to collapse: `kyiv-independent`/`-2`, `scmp-china`/`-china3`,
  `the-diplomat`/`-2`, `google-my-maps`/`famous-places`.

Renaming a source ID has the same orphaning problem as 7.1.

## 7.6 `npa-special-fraud` — DECIDED: keep the pin, document it

25 national monthly statistics pinned at NPA headquarters. A national statistic has no
location, so the pin is arguably fictional — but removing it empties a map layer, and unlike
the fabricated-geo cases the coordinate is *honest about what it is* (the publishing
authority's address, not a claim about where the fraud happened). Keep, with
`geo_precision: "publisher-hq"` in properties so it can be filtered. Same call for
`hudson-rock-jp`'s 34 rows on one Tokyo point.

## 7.7 `bom-au-warnings` — **DECLINED**

The only live replacement, `api.weather.bom.gov.au/v1/warnings`, verifiably returns 8 real
warnings — and its own response metadata reads *"This API is owned by the Bureau of
Meteorology. You must not use, copy or share it."*

I will not wire the product to an API whose response explicitly forbids use, and I'd give
the same answer if asked again. The auditor who found it made the same call independently.
If you want Australian warnings, the routes are: register for BOM's licensed feed, or use a
redistributor that has one. Left `DEAD_UPSTREAM`.

---

## Phase 8 — dead upstreams (~70 sources)

Auditors already found and verified replacements wherever one exists — 9 in slice 07, 13 in
slice 10, 6 in slice 09, 5 in slice 06, 27 in slice 01. Those are done and re-tested.

The remainder split three ways, and none of them is a code bug:

**Genuinely retired — recommend retiring the source** (~20)
`nato-news` (nato.int has no RSS at all now: RSS.htm 404, no `<link rel=alternate>`,
AEM search 400s unauthenticated) · `BGPVIEW` + `PATENTSVIEW` (NXDOMAIN, confirmed via
Cloudflare DoH with SOA from their own apex nameservers) · `tuoitre` (**upstream TLS
certificate expired**) · `swissinfo` (410 Gone) · `caixin`, `caucasus-watch`,
`benar-news`, `caribbean-news`, `iss-africa`, `the-wire-india`, `fiji-times`,
`nation-thailand` · `OPENWORK`, `CADASTRAL_PARCELS`, `HAZMAT_FACILITIES`, `TOKYO_MOU_PSC`,
`HR_SUDREG`, `LT_REGISTRAI`, `PT_RCBE`, `kitco-news`, `flightradar-jp`, `nra-radiation`,
`wanted-persons`, `gsi-active-fault` (its URL was marked "unverified candidate" in-source and
has **never** worked).

**WAF-blocked to any UA — leave as-is, they may recover** (~50)
Verified with both the collector UA and a full Chrome UA, re-tested twice. Includes
`cisa-advisories`, `cisa-news`, `acsc-au`, 6 national CERTs, `politico-eu`, `euractiv`,
`indian-express`, `reddit-*` (429 per-IP rate limit, not a block). These are correctly
returning `-1`; the visibility problem is that `-1` quarantines them, which Phase 3.2 fixed
for Overpass and which `lib/rss_atom.c` still does for RSS. **Recommend: a distinct
"throttled/blocked" outcome in `rss_collect()` so a WAF-blocked source is visibly blocked
rather than silently quarantined.**

**Key-gated successors — correct as-is** (~10)
MLIT WebLand → `reinfolib` (needs a subscription key) · PatentsView → USPTO Open Data Portal
· MalwareBazaar (anon access retired) · OpenCorporates v0.4 · OpenSanctions · GDELT (now
reachable after Phase 1.2). All log their gate honestly.

**Self-inflicted by tonight's own sweep — re-test from another IP before touching code**
`BLOCKCHAIR` (HTTP 430 "IP temporarily blacklisted due to exceeding usage of API
resources") · `STOOQ_QUOTES` (404 to every UA) · PeeringDB (throttled 2 of 3 endpoints).
The collectors are correct.
