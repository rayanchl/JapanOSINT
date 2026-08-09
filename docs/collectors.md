# Collectors

> **Rewritten 2026-08-09.** The previous version of this file was a hand-kept
> table of 206 collectors in `server/src/collectors/` with camelCase JS names
> (`jmaEarthquake`, `odptTransport`, `unifiedTrains`). That directory was
> deleted with the Node backend on 2026-05-17; the C engine registers
> **kebab-case ids** (`jma-earthquake`, `odpt-train`, `unified-trains`), and
> there are now an order of magnitude more of them than the table listed. A
> hand-maintained catalogue of ~2,500 sources would be wrong the week it was
> written, so this file explains how to *ask the tree* instead.

## Where they live

One source is one `source_def` in one `.c` file under
`native/collectors/sources/` (1,016 files as of 2026-08-09), self-registering
via `REGISTER_SOURCE`. The ABI is `native/source.h`; the run/emit contract is
described in [`pipeline.md`](./pipeline.md) §1; the rules a new source must
obey are in
[`../native/collectors/SOURCE_AUTHORING_CONTRACT.md`](../native/collectors/SOURCE_AUTHORING_CONTRACT.md).

`.inc` files in that directory (`_jp_osint.inc`, `av_common.inc`,
`trn_common.inc`, `sanc_common.inc`, `geoeo_common.inc`, …) are shared
`static inline` helper bodies, textually included and never compiled
standalone — the Makefile globs `*.c`, not `*.inc`.

## Enumerating them

```sh
native/bin/japanosint --list-sources          # id, collector, interval — from the built binary
make -C native source-count                   # macro-aware static count
python3 native/tools/lint_sources.py --list-ids
python3 native/tools/lint_sources.py --by-collector
curl -s localhost:4000/api/sources | jq       # with live status, from a running server
```

The static tools expand registration macros. Many files define a local macro
(`RSSX`, `RSS`, `DEF`, `TRN_DEF`, …) that produces a whole `source_def` plus
its own `REGISTER_SOURCE`; one `RSSX(...)` line in `arxiv_feeds.c` is 41
sources. `grep -c REGISTER_SOURCE` is therefore not a source count, and
believing it is produced six different wrong numbers in this repo's docs.

## Shape of the tree (2026-08-09)

`python3 native/tools/lint_sources.py --by-collector` — **2,563** registered
`source_def`s across 32 `collector` groups. (That number is exact: it matches
`bin/japanosint --list-sources` id-for-id, with zero difference in either
direction.)

| collector | sources | what it covers |
|---|---:|---|
| `osint` | 1,546 | entity-pivot investigation services and world OSINT feeds (the OSINT-search dispatch surface) |
| `cyber` | 203 | threat intel, CERT advisories, vendor research, scanning/exposure |
| `transport` | 150 | rail/bus/aviation/road — ODPT, MLIT KSJ, GTFS, OSM, unified fusion layers |
| `government` | 127 | official advisories, disaster/hazard feeds, regulators, open-data registries |
| `environment` | 97 | JMA seismic/weather/ocean, air quality, radiation, volcano |
| `infrastructure` | 54 | rivers, dams, power, water, telecom plant |
| `social` | 54 | social platforms, forums, community feeds |
| `sanctions` | 39 | sanctions and watchlist registries |
| `maritime` | 35 | AIS, ports, navigation warnings |
| `satellite` | 34 | imagery archives and tasking catalogues |
| `economy` | 32 | land prices, real estate, corporate/procurement registries |
| `telecom` | 30 | BGP/ASN/IP intelligence, spectrum, cell networks |
| `statistics` | 29 | e-Stat, RESAS, census and mesh statistics |
| `geospatial` | 20 | base maps, land use, boundaries, elevation |
| `industry` | 17 | industrial/facility registries |
| `camera-discovery` | 16 | public webcam channels (each writes under its own `source_id`, rolled up under a `parent_id` by `/api/intel/sources`) |
| `safety` | 13 | emergency services, warnings, shelters |
| `crime` | 10 | police incident maps and crime statistics |
| `tourism` | 9 | — |
| `_maint` | 8 | internal maintenance pods (repair, detect, url-override) — not user-facing |
| `culture` | 8 | — |
| `agriculture`, `marketplace` | 5 each | — |
| `defense`, `food` | 4 each | — |
| `_enrich`, `health`, `news` | 3 each | `_enrich` = LLM/entity enrichment pods |
| `classifieds` | 2 | — |
| `_breach`, `_test`, `intelligence` | 1 each | — |

Collector groups beginning with `_` are internal machinery, not data sources.

## Curated metadata

`native/core/source_registry.gen.c` holds the curated per-source metadata rows
(human name, Japanese name, category, type, canonical URL, description,
licence, **map layer**, free-tier flag, update interval). It was originally
generated from the Node `sourceRegistry.js`; that generator is gone, so the
file is now hand-edited and is the source of truth for anything it lists.
Sources absent from it carry their metadata inline in the `source_def` and
`src_meta_get()` synthesises a row.

`make -C native lint-sources` reports rows in that table with no corresponding
implementation. A row with no source can never report a status but still shows
up in `/api/sources` and `/api/layers` — as of 2026-08-09 there are 67 of them,
tracked in `native/tools/lint_baseline.json`.

## Map layers

A source appears on a map layer when its registry row (or its `source_def`)
sets `layer`. `/api/data/<layer>` is served generically by `dataapi_layer()`
(`native/core/httpd.c:2223`), with the fused sweep layers (`unified-*`,
`cameras`, `unified-stations`, …) served ahead of it by `sweepapi_data()`.
`layer` must stay NULL for entity-pivot services so they never appear in
`/api/layers`.
