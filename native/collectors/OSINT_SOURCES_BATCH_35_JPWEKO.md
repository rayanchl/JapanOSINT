# Batch 35, beat `jpweko` — WEKO3 entity pivots on the 100 largest JAIRO Cloud repositories

`collectors/pivot/table/hp3b35_jpweko.c` · manifest
`docs/candidate-sources-batch35.jpweko.txt` · readings
`docs/verified-sources-batch35.tsv`, `docs/rejected-sources-batch35.tsv`

## What a row is

A `/api/records/?q={q}` search on one JAIRO Cloud (WEKO3) institutional
repository — the repositories batch 25 wired for OAI harvesting but could not
be ASKED about an entity. Endpoint and keys were copied from batch 25's rows,
which had been through the emit audit and `--check-filter`; the beat itself
was authored on 2026-10-04 in a session whose network policy allowed no fetch,
and shipped as "unverified" with that said in CLAUDE.md.

## Measured 2026-10-20, live

`probe_hp_batch.py --check-filter` (probe entity 研究), then
`audit_batch_emit.py` with the manifest, then one re-run of each engine
failure.

| gate | reading |
| --- | --- |
| probe | 87 PASS, 12 HTTP 500/502/504, 1 connection closed — all on the repository side, none a bot wall |
| `--check-filter` | 0 `FILTER_IGNORED`: an impossible entity returns an empty result set on every repository that answered |
| engine, first run | 57 OK, 38 `EMPTY_UPSTREAM` (the repository answered and holds nothing for 研究 — an honest empty on a pivot, not a dead row), 2 HTTP 500, 3 HTTP 502 |
| engine, re-run of the five | Shizuoka 71 / 71, YNU 28 / 28 recovered; STARS answered and was empty; Niigata University and University of the Ryukyus answered 5xx a second time and are REMOVED |

**98 rows ship**, every one with a measured reading in the verified TSV; the
two removed are in the rejects TSV with both readings. JAIRO Cloud's shared
hosting answers 5xx to a burst — the probe's twelve concurrent requests saw
twelve of them, the engine at six saw five — which is the same shape batch 44
measured on municipal hosting, and the reason the probe now paces per host.
