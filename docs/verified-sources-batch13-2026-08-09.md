# 2,628 more intel sources — every one proven live twice before it landed

_2026-08-09 · branch `feat/osint-batch12-c-port`_

This is the second bulk addition to the collector fleet, and it follows the
same rule as the first: **an endpoint counts only if it was fetched, parsed and
found to contain at least one real record.** The difference this time is that
the check was run **twice, by different parties** — once by the discovery agent
that proposed the endpoint, and again centrally, over the merged set, by the
orchestrator. Only rows that passed the *central* run shipped.

That second pass is not ceremony. It caught 12 endpoints that an agent had seen
alive and that were dead, cookie-walled or unstable by the time the batch was
assembled, and it re-measured every row's wire format so the generated C is
built from what the endpoint returns *now*.

Fleet effect: **5,451 → 8,079 registered sources**, 0 duplicate ids, 0
overflow, `--selftest` PASS, `make lint-sources` OK with no baseline
regression.

---

## What landed

| Domain | Sources |
|---|---|
| Europe (national + sub-national, non-anglophone estates) | 735 |
| Africa & Middle East | 352 |
| East Asia (Japan sub-national, Korea, Taiwan, Hong Kong) | 319 |
| Latin America & Caribbean | 307 |
| Earth observation, ocean, cryosphere, climate, space | 267 |
| South/SE Asia & Oceania | 265 |
| US states/counties + Canadian provinces | 244 |
| Cyber, internet infrastructure, transparency, finance | 139 |
| **Total** | **2,628** |

By wire format: 1,197 JSON APIs with a resolved array path, 566 RSS, **426
GeoJSON**, 413 bare JSON arrays, 15 Atom, 8 single-object JSON, 3 CSV.

**426 of these carry real per-record geometry**, which is what becomes a map
layer. Coverage spans **199 country tags and 45 content languages** — English
first, then French, Spanish, Japanese, German, Portuguese, Dutch, Turkish,
Italian, Korean, Chinese, Hebrew and 33 more.

At verification time these endpoints collectively returned **546,536 records**.

### A note on the source count

The counter reported `4310` at the start of this work and `5451` for the same
tree later, with no source files added or removed in between. The delta this
batch contributes was therefore measured as a controlled A/B on one tool and
one tree — move the 61 generated files out (`5451`), move them back (`8079`) —
giving **+2,628**, which matches the manifest row count exactly. The runtime
registry agrees: a clean rebuild seeds `8079`.

`tools/lint_sources.py`'s absolute number should be treated with the suspicion
this repo already documents ("six different wrong source counts"); the
trustworthy figures are the delta and the runtime seed count from a **clean**
build. Incremental builds are not trustworthy here at all: stale object files
for generated collectors that had been deleted stayed linked, and the binary
registered 130 sources whose `.c` no longer existed.

---

## How they were found

Eight discovery agents ran in parallel, one per region/theme, each with a
distinct id prefix so their outputs could not collide. Every agent followed the
same loop the first batch established: draft candidates → verify → repair the
fixable failures → re-verify → replace the unfixable.

**Roughly 7,500 candidates were tried to yield these 2,758.** The single most
useful finding, reported independently by five of the eight agents, is the
difference between two discovery methods:

| Method | Pass rate |
|---|---|
| Guessing a publisher's hostname or feed path | ~36 % |
| Confirming a portal is alive, then enumerating **its own catalogue** | **~95 %** |

Every agent that ran out of budget ran out of it on guessed hostnames. The
enumerable surfaces that paid off were CKAN (`package_search`,
`organization_list`), ArcGIS FeatureServer layer directories, Socrata's
discovery API (`api.us.socrata.com/api/catalog/v1?domains=…`, which returns
real dataset ids instead of guessed 4x4s), Opendatasoft v2.1, ERDDAP dataset
indexes, OGC API `collections → items`, and STAC.

---

## Verified twice, and what the second pass caught

Central re-verification of all 2,844 delivered rows: **2,832 PASS (99.6 %)**.

The 12 failures are worth listing, because they are the reason the second pass
exists:

- 2 **cookie-wall interstitials served as HTTP 200** — the exact "live-looking
  source that fetches nothing" failure this pipeline is built to prevent.
- 5 HTTP errors (a 404, a 503, three others) on endpoints alive an hour earlier.
- 3 transport errors, 2 timeouts.

A further **8 rows initially failed the central pass and were then recovered**.
They had answered `429 Too Many Requests` because the recheck fires 24 requests
at once and several endpoints shared a host — an artefact of how we measured,
not a property of the source. Re-testing them serially at 4-second pacing
returned PASS. Measuring a fleet concurrently and then blaming the endpoint is
an easy way to discard working sources, so the retry is part of the procedure
rather than a one-off.

### Three classes rejected at merge, not by the verifier

**60 rows were dropped for pointing at a metadata array rather than records.**
`verify_feeds.py` selects the *longest* array in a document, which is not
always the data:

- A CKAN `datastore_search` returning 22 schema columns and 11 rows resolves to
  `result.fields`. The generated collector would then have emitted **column
  definitions as intel rows** — the "names, not data" pattern R1 forbids,
  arriving through the back door. 5 rows.
- JMA's per-prefecture warning JSON resolves to `areaTypes`, an array of 2
  containers shaped `{"areas":[…]}` with no label and no coordinates.
  `jsonlist_emit` would emit nothing from them. 55 rows.

Both are now rejected by a metadata-path guard in the merge step. The JMA
warning endpoints are real and valuable; they need a bespoke collector that
understands `areaTypes[].areas[].warnings[]`, not the generic JSON toolkit, and
they were left out rather than shipped as sources that fetch fine and produce
nothing.

**14 rows were cross-agent duplicate URLs** — two agents independently finding
the same endpoint, which is a sign the domain split was imperfect, not a defect.

**130 rows were dropped after the first end-to-end run** for having no record
structure the collector layer can use. CKAN's `tag_list`, `organization_list`,
`group_list` and `package_list` return arrays of **bare strings**, and ERDDAP's
`/info/index.json` returns rows as **arrays**, not objects. Both fetch
perfectly and parse perfectly; neither can ever produce a titled row. This is
the same reasoning that excluded plain-text IP blocklists from the first batch,
applied to a shape that happens to be valid JSON.

---

## What was deliberately excluded

**Publishers that refuse automated access were respected, not worked around.**
`lib/rss_atom.c` carries a standing decision: a descriptive UA with a contact
URL is the correct remedy for a block, and *"spoofing a browser to evade a
publisher's block is not."* Every agent was briefed on this and every one
followed it. Well over 200 endpoints returning 403 to the declared bot UA were
dropped rather than retried behind a fake Chrome UA.

Notable losses to that rule, recorded so nobody re-spends candidates on them:
`open.africa`'s entire API surface, `open.alberta.ca`, the Texas Register,
`www.datosabiertos.gob.pe` (418 to any declared bot — the reason Peru is thin),
most Philippine government hosts, the `gov.br` subdomain family, and a long
list of national newspapers.

**The six previously WAF-blocked national CERTs are still blocked** from this
egress and were re-tested to confirm it: CERT-EU, CCCS Canada, ACSC Australia
(silent timeout rather than 403), CERT NZ, CSA Singapore, MyCERT. They should
be treated as unreachable from here, and re-tested from the production
collector host.

**Key-gated services were dropped rather than shipped with a placeholder key** —
including the near-entirety of the US state 511 traffic estate, Korea's
`data.go.kr` family, Taiwan's CWA and MOENV, India's IMD, Japan's e-Stat and
RESAS, and Seoul's `openapi.seoul.go.kr`. Two agents specifically noted that a
publisher's *documented sample key* is still a credential and declined to use
one.

**Endpoints with a hard-coded date or year were dropped even when they passed** —
they work today and pin the collector to 2026 forever. This was the largest
single category of self-rejection: 87 Opendatasoft datasets whose slug pins a
year, 29 IGN Géoplateforme layers on `ADMINEXPRESS-COG.<year>` typenames (the
`.LATEST` variants were kept instead), AusTender's OCDS API (which has no
rolling form at all), and others.

**Zero-hit CKAN search envelopes were dropped.** A `package_search?q=<topic>`
that matches nothing returns HTTP 200 with `result.results: []`, which the
verifier scores as `json-object`/1 item and PASSes. One agent found this,
dropped all 27 of its own, and flagged it; the merge guard now catches the
class. **This is a real weakness in `verify_feeds.py` and is the most valuable
single finding of the run** — see below.

Also excluded: ArcGIS layers returning `null` geometry (13 Cape Town layers, 14
US/Canadian layers — found by two agents who independently wrote geometry
checkers and re-fetched every GeoJSON row to inspect actual coordinates rather
than trusting the item count), responses over the 8 MB ceiling, endpoints
behind broken TLS chains that OpenSSL rejects and so the C collector would fail
identically, and community/coursework layers on ArcGIS Online that mirror
official data without being authoritative.

---

## Rate-limit discipline

The ceilings from the first batch were respected: **zero** new endpoints on
`api.github.com` (60 req/hr per IP, shared fleet-wide), **zero** on
`www.peeringdb.com`, and nothing added to `news.google.com`, `reddit.com`,
`api.energy-charts.info` or `api.adsb.lol`.

Three new rate-limited hosts were found and are recorded here:

- **`data.bodik.jp`** — hosts the open data of ~380 Japanese municipalities and
  was the single richest find of the run. All 85 drafted endpoints passed on
  first contact, then the host IP-blocked us: 403 on every request, still 403
  when retried sequentially at 2-second pacing, and still 403 on lone probes
  minutes later. **83 confirmed-live rows were dropped rather than shipped**,
  and were not retried behind a spoofed UA. Recovering them needs a slow pass
  (≤1 req/5 s, no concurrency) from a different egress.
- **`api-open.data.gov.sg`** — 429s when several of its endpoints share a tick.
  Its rows are spread across 900–10800 s and should not be scheduled together.
- **`thespacedevs`-style launch APIs** — 3 endpoints 429 even at 4-second
  pacing; only the one that passed serially was kept.

The generator clamps every interval to a 900 s floor. In this batch 1,159
sources poll daily, 537 hourly, 317 six-hourly, 234 twice-daily; only a handful
of genuine real-time hazard feeds sit at the floor.

`www.jma.go.jp` now carries 116 rows from this batch. They passed
re-verification cleanly under 24-way concurrency, but that host is the one to
watch if the fleet grows further there.

---

## Measured end-to-end, not just compiled

Registering is not delivering. A sample of **150** was run through the real
`--run` path against live upstreams (R4), which found that **31 % emitted zero
rows** — fetching fine, parsing fine, and producing nothing.

Diagnosing every zero-emitter rather than accepting the number found three
distinct causes, two of which were defects in shared code:

| Cause | Fix |
|---|---|
| `OBJECT_NAME` never matched — the `_name` suffix test was **case-sensitive** | relaxed to `strcasecmp` (the underscored form only; matching a bare `…name` case-insensitively would swallow `surname`, `filename`, `hostname`) |
| The label exists but is **not in English** — `nome`, `nom`, `nombre`, `naam` | added after the English keys, so precedence is unchanged for records carrying both |
| The label sits one level down under **`attributes`** (JSON:API, Esri) or **`properties`** | descend into the envelope, but only when the top level yielded nothing |

All three are additive: they run only when no label was found, so a record that
already emitted cannot change. Re-running the **same 142 sources** confirms it:

| | Before | After |
|---|---|---|
| `OK` (emitted ≥1 row) | 101 (71 %) | **113 (80 %)** |
| `EMPTY` | 38 (27 %) | **26 (18 %)** |
| `FAIL` | 3 | 3 |
| **Regressions (`OK` → `EMPTY`)** | — | **0** |

20,176 rows collected from 142 sources. Twelve sources went from silent zero to
working, including CelesTrak's satellite catalogues (696 rows across three),
Brazil's PIX participant registry (895), MBTA routes and stops (200), and
France's département registry (101). **The fix applies to the 5,451
pre-existing sources on the same code path, not only to this batch.**

The 26 still emitting nothing were left honest rather than force-labelled: they
are statistical tables keyed by numeric codes, Socrata extracts of pure
measurements, and registries whose only label-ish column is a domain
abbreviation. Inventing a title for those is the failure this contract exists
to prevent.

The 3 `FAIL`s are all `data.humdata.org` country slices — the host throttles
when several of its endpoints are hit in one pass, which is a scheduling
concern rather than a dead source.

---

## Implementation

No new machinery. This batch reuses the first batch's three files —
`verify_feeds.py` and `gen_verified_sources.py` unchanged, and
`lib/jsonlist.c` improved as above — which is good evidence the pipeline
generalises.

Regenerate with:

```sh
python3 native/collectors/gen_verified_sources.py \
    docs/verified-sources-batch13.tsv \
    --outdir native/collectors/sources \
    --reserved native/collectors/existing_ids.txt \
    --prefix vsrc13
```

`docs/verified-sources-batch13.tsv` is the recorded proof: per source, the URL,
the centrally re-measured wire format, and the record count observed.

`native/collectors/existing_ids.txt` was regenerated and grew from **987 to
6,650** ids. The old snapshot only captured ids written as `.id = "…"`
literals, so it missed every macro-expanded source — i.e. most of the fleet.
An agent checking a candidate id against it would have been told "probably
free" for thousands of ids that were already taken. It now includes
macro-expanded ids.

### `MAX_SOURCES` raised before the sources landed, not after

`registry.c`'s cap is now **32768** (was 16384; 256 KB of pointers in `.bss`).
It was raised *first*, deliberately. When this cap is too low the failure is
silent in the only way that matters: the build succeeds, the fleet looks
healthy, and every source past the cap does not exist at runtime. The first
batch hit exactly this — 4,096 registered of 4,310 needed — and the audit
before it predicted it. At 7,068 sources there is now room to more than
quadruple again.

---

## Honest limits

- **Verified twice, but still from one IP and at one moment.** A source that
  passed here can break tomorrow, and some will.
  `native/tools/fleet_probe.sh` is the standing re-measurement.
- **`jsonlist`'s field mapping is name-based, not semantic.** For a JSON API
  with unusual field names it picks a real label the record supplied or emits
  nothing; it never invents one. The multilingual key list is now 12 languages
  deep and will still miss others — each addition is a judgement call, and a
  wrong title is worse than no row.
- **~18 % of this batch emits nothing on any given run** and is counted here as
  delivered because the endpoint is live and the collector is correct. That is
  an honest zero, not a working source; the count that matters operationally is
  the `OK` figure above, not the registered total.
- **`verify_feeds.py` cannot see an empty envelope.** A CKAN search with zero
  hits, and any API that returns a well-formed "no results" document, passes
  its bar. The merge guard catches the known shapes; the general problem is
  open.
- **The 8 MB ceiling silently shapes coverage.** Several large national
  datasets were dropped or fetched at reduced `rows=` rather than in full.

## Pre-existing breakage found along the way

Not introduced by this batch, and not fixed here — recorded so it is not lost:

- **Red Hat security data is dead in the tree.**
  `access.redhat.com/hydra/rest/securitydata/csaf.json` (used by
  `collectors/sources/cert_distro_json.c:47`) and the per-CVE endpoint (used by
  `collectors/sources/netintel_world2.c:99`) both return **403** to a declared
  bot UA. Confirmed directly, not just reported.
- **The IRIS/EarthScope FDSN event service returns HTTP 410 Gone** at both
  `service.iris.edu` and `service.earthscope.org`. The tree does not currently
  reference it, so this cost nothing — but it is the canonical seismic event
  API and any future work should reach for RESIF, INGV or GEOFON instead.
- **`api.nilu.no`** (Norwegian air quality) has become key-gated while still
  being widely documented as keyless.
