# 1,747 new intel sources — every one proven live before it landed

_2026-08-09 · branch `feat/osint-batch12-c-port`_

This documents a bulk addition to the collector fleet. The number that matters
is not 1,747; it is that **every one of these endpoints was fetched, parsed and
found to contain at least one real record before a line of C was written for
it**. Across the set, 623,200 records came back at verification time.

That entry condition is the entire point. The 2026-07-31 fleet audit found that
a large share of then-existing sources passed every row-count sweep while
carrying no fetched data at all, and `collectors/SOURCE_AUTHORING_CONTRACT.md`
exists as that audit's findings turned into acceptance criteria. A source that
looks real and fetches nothing is worse than no source: it inflates the
dashboard and teaches the analyst to distrust the ones that are real.

---

## What landed

| Domain | Sources | Notes |
|---|---|---|
| Hazards, environment, earth observation | 527 | **344 carry real per-record geometry** |
| Government, sanctions, transparency, legal | 365 | |
| Cyber, CERT/CSIRT, threat intel, internet infra | 315 | **44 distinct national CERTs across 42 countries** |
| Economy, energy, health, transport, regional wires | 305 | 132 publisher-own news wires, zero Google-proxied |
| Conflict, defence, aviation, maritime | 235 | 77 geometry-bearing |
| **Total** | **1,747** | |

By wire format: 576 JSON APIs, 543 RSS, 260 GeoJSON, 154 Atom, 139 bare JSON
arrays, 61 CSV, 14 single-object JSON.

Fleet effect: **2,563 → 4,310 registered sources**, and the geometry-bearing
count grows by 421 — which matters because geometry is what becomes a map
layer.

---

## How they were verified

`native/collectors/verify_feeds.py` fetches each candidate and requires **2xx
AND parses AND ≥1 record**. Anything else is rejected. Discovery ran as five
parallel passes, each iterating: draft candidates → verify → repair the
fixable failures (wrong path, `http`→`https`, missing `?f=json`, ArcGIS
`/query?where=1%3D1&outFields=*&f=geojson`, OGC API `/items?f=json&limit=500`)
→ re-verify → replace the unfixable.

**4,419 candidates were tried to yield these 1,747.** The attrition is the
evidence that the bar was real.

Three classes of "HTTP 200 but not actually a feed" were found during the run
and are now caught by the verifier itself:

1. **ArcGIS/OGC error documents.** `{"error":{"code":400,"details":[…]}}`
   returns 200 and parses as a healthy feed named `json:error.details`.
2. **Auth refusals served as 200.** GIE's gas-storage APIs answer
   `{"error":"access denied","message":"Invalid or missing API key"}` with a
   200. Catalogued naively, that is a keyless source that fetches nothing.
3. **Endpoints that answer 200 with a zero-byte body**, e.g. NGA's ASAM piracy
   product.

The refusal check is deliberately consulted only for documents small enough to
be a bare status envelope, so a real feed whose headline contains the word
"unauthorized" is not rejected.

Two upstreams were caught emitting **fabricated-looking geometry** and were
dropped rather than shipped: DWD's `Gewitterzellen`/`Gewittercluster` layers
return `coordinates: [0,0]` or null geometry when no storm cells are active,
and 13 USGS HANS `volcano/*` endpoints return
`{"error":"Did not find volcano/getX"}` as a parseable 1-item document.

---

## What was deliberately excluded

**Publishers that refuse automated access were respected, not worked around.**
`lib/rss_atom.c` carries a standing decision: a descriptive UA with a contact
URL is the correct remedy for a block, and *"spoofing a browser to evade a
publisher's block is not."* Every discovery pass followed that. Endpoints
returning 403 to a declared bot UA were dropped, including ReliefWeb v2,
Interpol notices, SEC EDGAR RSS, FTC, BLS, IMF, OECD, Transparency
International, ICAO, IMO, and a cluster of APAC national CERTs.

That last cluster is worth a note: CERT-EU, CCCS Canada, ACSC Australia, CERT
NZ, CSA Singapore, MyCERT Malaysia and several others were WAF-blocked from
*this* egress IP specifically. They are probably reachable from the production
collector host and **are worth re-testing there** — they were excluded as
unverifiable, not as dead.

Also excluded: key-gated services (Space-Track, OpenSky auth, EIA v2, ENTSO-E,
UCDP GED, ACLED, VirusTotal, PhishTank, GreyNoise, OpenAQ v3 …) rather than
shipped with a placeholder key; plain-text IP/domain blocklists, which have no
record structure for the collector layer; endpoints requiring a JS-minted
cookie or a Visualforce/JSF round-trip, which are not a bare GET; four national
CERTs with broken TLS chains that both curl and OpenSSL reject, so the C
collector would fail identically; and every URL with a hard-coded date, which
passes today and would pin the collector to 2026-08-08 forever.

No keyless, permitted AIS/vessel-position source and no keyless piracy-incident
feed were found. Those gaps are real and are recorded here rather than filled
with something plausible.

---

## Rate-limit discipline

The audit's central operational finding is that over-polling a shared host
quarantines whole cohorts. Three specific ceilings were respected:

- **GitHub**: unauthenticated is 60 req/hr **per IP** and the fleet already
  holds 18 `api.github.com` URLs. The cyber pass drafted 31 more, blew the
  quota during testing, and cut to 10 endpoints at 7200/21600 s — **2.33
  req/hr total**. Keep future GitHub additions inside that shared ceiling.
- **PeeringDB** throttles anonymous clients hard: all 8 endpoints passed on
  first contact, then 429'd across the board. Reduced to one endpoint at
  86400 s.
- **Fraunhofer `energy-charts`** and **adsb.lol** both 429 under burst;
  intervals raised and endpoint counts trimmed.

The generator enforces a hard floor of 900 s regardless of what a manifest
claims, so an over-eager row is clamped rather than trusted.

---

## Implementation

Three files, all regenerable:

- **`native/collectors/verify_feeds.py`** — the verifier. Stdlib only, so it
  runs in CI and in a bare WSL image.
- **`native/lib/jsonlist.{c,h}`** — a new toolkit, the missing fourth alongside
  `rss_atom.h`, `geojson.h` and `csv.h`. Plain JSON APIs have no shared schema,
  which is why they previously needed a bespoke ~80-line collector each or went
  uncollected. `jsonlist` maps field *names* by a fixed precedence list and
  emits nothing for a record with no title (R1). Geometry follows R2 strictly:
  `has_geo` is set only from a finite in-range coordinate pair the record
  itself carried, or an embedded GeoJSON Point. `0,0` is rejected as Null
  Island. **There is no fallback centroid anywhere in it.**
- **`native/collectors/gen_verified_sources.py`** — renders the manifests into
  collectors via four macros (`VRSS`/`VGEO`/`VJSON`/`VCSV`). All four return
  `0` on a successful fetch that yielded nothing and `-1` only on a real
  fetch/parse failure — R3, the most-violated rule in the existing fleet.

Regenerate with:

```sh
python3 native/collectors/gen_verified_sources.py \
    docs/verified-sources-manifest.tsv \
    --outdir native/collectors/sources \
    --reserved native/collectors/existing_ids.txt
```

`docs/verified-sources-manifest.tsv` is the recorded proof: per source, the
URL, the detected wire format, and the record count observed at verification.

---

## Honest limits

- **Verified once, at one moment, from one IP.** A source that passed here can
  still break tomorrow, and a few certainly will. `native/tools/fleet_probe.sh`
  is the standing re-measurement; see `docs/fleet-test-2026-08-09.md`.
- **`jsonlist`'s field mapping is name-based, not semantic.** For a JSON API
  with unusual field names it will pick a real label the record supplied or
  emit nothing; it will not silently invent one.

## Measured end-to-end, not just compiled

**All 1,747** were later run through the real `scheduler_run_source` path
against live upstreams as part of the fleet regression probe
(`docs/fleet-test-2026-08-09.md`):

| | |
|---|---|
| `OK` (emitted ≥1 row) | **1,327** |
| honest empty (`rc=0`, 0 rows) | 372 |
| `FAIL` | 37 |
| `TIMEOUT` | 11 |
| **Healthy** | **97.3%** |
| Rows collected in one pass | **364,030** |

By domain: `geo` 372/527 OK · `gov` 267/365 · `cyb` 255/315 · `eco` 233/305 ·
`sec` 200/235.

A second discovery wave later added **1,141 more** (747 science/space, 396
Japan prefectural/municipal — all 47 prefectures covered, 74 geometry-bearing).
A 100-source sample of that batch returned **zero failures**: 69 emitting, 31
honest-empty, 10,348 rows.

`--selftest` PASS; `make lint-sources` OK; 0 duplicate ids; 0 registry
overflow throughout.

### The 372 empties were not empty — and that mattered

The probe re-fetched **every one** of the 372 `EMPTY_OK` sources and re-ran
`jsonlist`'s logic against the payloads. **Not one was a genuinely empty
upstream.** 279 of them were dropping **232,792 records per pass** purely
because no field matched the title precedence:

| cause | sources |
|---|---|
| records present, no label found | **279** |
| array of scalars, not objects | 54 |
| path/shape mismatch | 19 |
| HTTP 206/429/202 or not JSON | 20 |
| genuinely empty upstream | **0** |

That is the whole justification for widening the composed-label rule. The
dominant shape was a pure time series — NOAA SWPC's `{time_tag, kp}` — which
carries a real timestamp and a real measurement but no name and no position.
Requiring *coordinates* for a composed label was an accident of the first case
that needed it (NASA FIRMS), not a principle.

The rule now fires when a record has a timestamp **and** at least one numeric
measurement, and the label names that measurement:

```
spaceweather 1749-01 ssn=96.7
```

Every part of that string came from the record. Nothing is invented, and a
record with neither a name nor a measurement is still not an intel row (R1).
Measured effect on the exact sources the probe named:

| source | before | after |
|---|---|---|
| `geo-swpc-solar-cycle` | 0 | **3,331** |
| `eco-who-gho-cholera` | 0 | **2,469** |
| `geo-swpc-solar-radio-flux` | 0 | 120 |

Two field-name bugs were found the same way and fixed: NOAA serves both
`time_tag` and `time-tag` (hyphen) across its own products, and ADS-B feeds
name their timestamp `seen`.

**UNHCR is a source-URL defect, not a `jsonlist` defect.** The `*_name` suffix
fallback works; the configured URLs return the global aggregate series where
every `coo_name`/`coa_name` is literally `"-"` (rows start at 1951 — the
`yearFrom` parameter is ignored). Those URLs want re-specifying.

**Adding these hit a real ceiling.** `registry.c`'s `MAX_SOURCES` was 4096, so
the first build registered 4,096 of the needed 4,310 and logged `OVERFLOW` for
the remainder. The 2026-08-07 audit predicted exactly this ("`MAX_SOURCES 4096`
has far less headroom than the '1,186' reading implies"). Raised to 16384 —
one pointer per slot, 128 KB in `.bss`.

The first sample run exposed a real defect worth recording, because it is the
same class of failure this whole exercise is about. 25% of sampled sources
emitted zero rows — not because the endpoints were empty, but because
`jsonlist`'s title precedence was too narrow:

- SWPC's solar-flux records label their station `common_name` — now 120 rows.
- UNHCR names its label columns `coo_name`/`coa_name` (country of origin / of
  asylum) and has nothing called "title", so a fixed key list can never cover
  it — a `*_name` suffix fallback was added.
- NASA FIRMS active-fire CSV has **no label column at all**: just latitude,
  longitude, `acq_date`, satellite, brightness, FRP. Its timestamp is named
  after the act of observing, not publication, so it also looked undated.
  Adding the observation-style date keys and a strictly-guarded composed label
  took it from 0 to **9,489 fire detections**.

That composed label is worth being explicit about, since this codebase has a
history of fabricated data: it is built **only** from values the record itself
supplied (`record_type`, the real timestamp, the real coordinates), and only
when a genuine coordinate pair *and* a genuine timestamp are both present.
Nothing is guessed. Refusing the row would have discarded real geolocated
observations; inventing a coordinate would have been the opposite sin.

Some sources still emit zero and legitimately so — WHO GHO returns numeric
indicator codes with no human label, and UNHCR's oldest pages are `"-"`
aggregates. Those need bespoke collectors; they were left honest rather than
force-labelled.

- **Verified once, from one IP.** Some will break; `native/tools/fleet_probe.sh`
  is the standing re-measurement.
