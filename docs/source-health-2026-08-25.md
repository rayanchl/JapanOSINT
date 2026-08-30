# Source health sweep — 2026-08-25 (after the multi-agent repair phase)

`audit_registry_emit.py --scheduled --jobs 4 --timeout 220` over every
scheduled row in the registry, each against a fresh copy of a warm template
DB, reading `stored` back out of that DB as well as off the run line. Raw
result: `docs/source-health-2026-08-25.tsv` (11,891 rows). The baseline it is
compared against is `docs/source-health-2026-08-24.tsv`, taken before agents
A/B/S1–S5/M1 ran. Comparison tool: `native/tools/compare_sweep.py`.

The binary measured carries the regenerated batch 18–21 tables and the
hostgate per-host gap; it predates the CWFIS/p2pquake/geojson coordinate
fixes and batch 22 deeparchive (those change what is geocoded and what is
registered, not whether a row emits). Sweep wall clock: 7 h 40 min.

## Numbers

| | |
| --- | --- |
| scheduled sources swept | **11,891** (11,507 in both sweeps, 384 registered since) |
| verdicts today | OK 9,980 · COLLISION 922 · EMITS_NOTHING 925 · SLOW 45 · NEEDS_ENTITY 15 · NO_RUN_LINE 4 |
| **repaired** (not OK on 08-24 → OK today) | **438 sources, 2,262,684 records/pass** |
| regressed (OK on 08-24 → not OK today) | 221 sources — see below, most are sweep-load artefacts |
| stored per pass over the common 11,507 | **11,500,601 → 12,931,595** (+1.43 M) |
| new since 08-24 (batch 22 + reddit groups) | 384 sources, 341 OK, 165,167 records/pass |

Transitions over the common set:

| 08-24 → today | sources |
| --- | --- |
| OK → OK | 9,201 |
| COLLISION → COLLISION | 806 |
| EMITS_NOTHING → EMITS_NOTHING | 650 |
| EMITS_NOTHING → OK | 211 |
| COLLISION → OK | 203 |
| OK → EMITS_NOTHING | 158 |
| OK → COLLISION | 55 |
| credential-gated → OK | 16 |
| honest-empty → OK | 8 |
| everything else | < 45 each |

## The 221 "regressions", read

| class | n | what it is |
| --- | --- | --- |
| EMITS_NOTHING | 158 | **Mostly the sweep, not the source.** A serial re-run of a 16-row sample (`docs/source-health-2026-08-25-recheck.tsv`) passed **14 of 16** — every `gnews-*` row, the CKAN Trentino/Málaga rows, the gkukan rows. Under `--jobs 4` the sweep fires same-host rows together and the host answers 429/empty; run alone they emit. The two that stayed dead (`afr-ci-aip`, `sci-stac-inpe-modisa-ocsmart-poc-daily-1`) are upstream. 167,476 records/pass sit in this class on paper; the sample says most of it is not lost |
| COLLISION, < 1 % of records collapsed | 31 | byte-identical records repeated across pages (`1 of 10001`, `3 of 10001`) — real dedupe by the content-hash guard, not an identity defect |
| COLLISION, ≥ 1 % collapsed | 24 | worth a per-row `id_keys` look; largest are `sas-ckan-datavic` (29/2001) and `nam-calgary-air-quality-data-near-real-time` |
| SLOW | 5 | hit the 220 s ceiling with rows already stored (`ANFR_MOBILE_TRANSMITTERS` had 1,000 in the DB when killed) — unmeasured, not failed |
| NEEDS_ENTITY / NO_RUN_LINE | 3 | `RIPENCC_DELEGATED_STATS` (260,294 records/pass) died before its run line — re-run alone before believing it |

The honest reading: the phase repaired 438 sources and added 341, and the
221 the sweep marks as regressed are dominated by a measurement artefact this
sweep's own `--jobs 4` creates. The fix for that artefact is the same one
that took the reddit groups from 2/17 to 17/17 — a per-host gap in
`core/hostgate.c` — and `news.google.com` is the next candidate for it.

## What each agent's work measured as

* **A (hpengine rows)** — 195 of 242 repaired rows OK, 479,375 records/pass
  (`docs/source-fix-2026-08-25-hpengine-emit.tsv`).
* **B (source_def / feed rows)** — reddit 2/17 → 17/17 after the hostgate
  gap (`docs/source-fix-2026-08-25-reddit-groups.tsv`); the COLLISION → OK
  203 above is largely B's `url`/array-path work on vsrc rows.
* **S1–S5 (batch 22)** — 540 shipped (350 + 190), every row probed,
  filter-checked, emit- and store-verified; see
  `docs/verified-sources-batch22.md` and `-deeparchive.md`.
* **M1/M2 (map)** — not a source-count matter; verified separately (Σ layer
  FCs = COUNT(lat IS NOT NULL) = 82,028 on a warm DB; web + iOS render by
  declared modality).
