# docs/ prune, 2026-11-16

Removed files stay in git history: `git show f38cc7ad4e7c0d5db0e6b8346236e71bba94d4ef:<path>` recovers any of them.

## Rule used

Usefulness was decided by evidence, with a script, and conservatively (when in doubt, keep). A docs file is KEPT if any of these holds:

1. it is Markdown (all `.md` kept);
2. it is under `docs/audit-2026-11-14/`, `docs/source-pages-revamp/` or `docs/history/`;
3. its name marks batch evidence: `verified-sources`, `rejected-sources`, `removed-sources`, `measured-`, `probe-`/`probeNN_`, `emit-`/`emitNN_` (kept regardless of references);
4. its path or basename appears as a token in CLAUDE.md, README.md, `docs/history/*`, any `docs/**/*.md`, any tracked file under `native/`, `client/`, `ios/` or `.github/` (code comments cite evidence TSVs heavily). This includes `candidate-sources-batch*` manifests, which the batch gates and `gen_hp_batch.py`/probe/audit tools re-read and whose tables still exist in `native/collectors`.

A basename reference is a loose test, so each removal candidate was also re-checked by hand and by a second script (`no reference to a removed path remains in CLAUDE.md, docs/**/*.md, README.md, native/, client/, ios/, .github/`). One group the first pass proposed was KEPT on that check: `docs/batch{30,32,39,40}/existing_{ids,urls}.txt` (snapshot dumps, but each `BRIEF.md` tells its session to read them, so the fix is to keep the file).

Left-over candidates were: unreferenced scratch directories, one-off helper scripts, intermediate discovery/selection dumps and id lists, and one unreferenced binary archive. Nothing over 1 MB failed every test (the large files are all evidence TSVs or manifests that native/ cites), so no large file was removed.

## Totals

| | files | MB |
|---|---:|---:|
| docs/ before (at the commit above, incl. the 14 docs/history files) | 802 | 149.71 |
| removed | 81 | 1.72 |
| docs/ after (excluding this report) | 721 | 147.99 |

## Removed, by reason

| reason | files | KB |
|---|---:|---:|
| unreferenced binary archive of WSL scratch files salvaged on 2026-09-14 | 1 | 179.9 |
| intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* | 16 | 1225.0 |
| scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* | 20 | 144.0 |
| intermediate id list of batch 40 (ship/retry/other-ids working lists); the ids are in verified/rejected-sources-batch40 and the C tables | 6 | 5.5 |
| unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* | 38 | 167.3 |

## Every removed file

Recover with `git show f38cc7ad4e7c0d5db0e6b8346236e71bba94d4ef:<path>`.

| path | bytes | reason |
|---|---:|---|
| `docs/archive/wsl-scratch-salvage-2026-09-14.tar.gz` | 179851 | unreferenced binary archive of WSL scratch files salvaged on 2026-09-14 |
| `docs/batch32/count_oai.sh` | 674 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/feed_turnover.json` | 1943 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/gen_jps_manifest.py` | 4440 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/gen_oai_manifest.py` | 3483 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/identify.sh` | 349 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/irdb_rows.tsv` | 94401 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/irdb_uncovered.tsv` | 75295 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/jps_probe.json` | 239529 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/jps_probe.py` | 2239 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/jps_uncovered.json` | 644653 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/oai_counts.tsv` | 27606 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/oai_identify.tsv` | 9387 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/oai_ranked.tsv` | 30148 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/oai_top100.txt` | 3001 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/selection.json` | 27912 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch32/selection_pool.json` | 59971 | intermediate discovery/selection dump or generator helper of batch 32 (unreferenced); shipped rows and measurements are verified-sources-batch32.* and docs/batch32/probe32_*, emit32_* |
| `docs/batch39/cn39_scratch/scripts/amac_walk.py` | 974 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/bse_ann_walk.py` | 1286 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/bse_walk.py` | 1281 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/cnemcw_walk.py` | 689 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/cninfo_walk.py` | 909 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/dpm_sample.py` | 686 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/flk_walk.py` | 1108 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/neeq_walk.py` | 988 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/probe39_cn39_cninfo_nullfix.tsv` | 444 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/probe39_cn39_digitseg_patch.tsv` | 510 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/probe_digitseg_scratch.py` | 41412 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/probe_nullfix_scratch.py` | 43287 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/sse_ann_probe.py` | 1215 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/szse_rerun.tsv` | 403 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/walk_sse_susp.py` | 1391 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/write_manifest.py` | 39982 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/cn39_scratch/scripts/write_rejects.py` | 3384 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/jp39_scratch/measure_ids.py` | 1376 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/jp39_scratch/xlsxcsv.py` | 857 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch39/tbl39_scratch/tbl.py` | 1867 | scratch working directory of batch 39 (unreferenced helper scripts and per-run scratch TSVs); shipped measurements are docs/batch39/probe39_*, emit39_*, verified-sources-batch39.* |
| `docs/batch40/cngov_nfra_ids.txt` | 153 | intermediate id list of batch 40 (ship/retry/other-ids working lists); the ids are in verified/rejected-sources-batch40 and the C tables |
| `docs/batch40/cngov_other_ids.txt` | 865 | intermediate id list of batch 40 (ship/retry/other-ids working lists); the ids are in verified/rejected-sources-batch40 and the C tables |
| `docs/batch40/cnsar_hkma_ids.txt` | 193 | intermediate id list of batch 40 (ship/retry/other-ids working lists); the ids are in verified/rejected-sources-batch40 and the C tables |
| `docs/batch40/cnsar_other_ids.txt` | 710 | intermediate id list of batch 40 (ship/retry/other-ids working lists); the ids are in verified/rejected-sources-batch40 and the C tables |
| `docs/batch40/retry_ids.txt` | 355 | intermediate id list of batch 40 (ship/retry/other-ids working lists); the ids are in verified/rejected-sources-batch40 and the C tables |
| `docs/batch40/scripts/cngov/c.sh` | 336 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cngov/jsurls.py` | 771 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cngov/rss.py` | 1398 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cngov/xh.py` | 975 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cngov/xhe.py` | 830 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cnmkt/build_manifest.py` | 48365 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cnmkt/dupdiff.py` | 707 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cnmkt/em.py` | 1172 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cnmkt/emcheck.py` | 1077 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cnmkt/emtry.py` | 367 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cnmkt/j.py` | 540 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cnmkt/meas.py` | 2750 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cnmkt/sse.py` | 996 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cnmkt/sseall.py` | 2518 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cnmkt/szse.py` | 646 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cnmkt/xl.py` | 572 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/cnsar/hkcount.py` | 848 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/arc.py` | 1650 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/arccheck.py` | 1343 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/arcfull.py` | 2027 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/ckan.py` | 1654 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/count.py` | 2688 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/count2.py` | 1773 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/discover.py` | 1535 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/ds.py` | 1095 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/dsfull.py` | 992 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/gen_manifest.py` | 42731 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/gsp.py` | 1121 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/orgdir.py` | 1484 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/orglist.py` | 1203 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/pkg.py` | 665 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpgeo3/uniq.py` | 638 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpnat3/chk.sh` | 544 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpnat3/engsim.py` | 3568 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpnat3/rows.py` | 1616 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpnat3/rows_data.py` | 30284 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpnat3/xl.py` | 1279 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/scripts/jpnat3/xlpos.py` | 2574 | unreferenced one-off helper scripts of batch 40 authoring (no doc, tool or code comment cites them); results are docs/batch40/probe40_*, emit40_* |
| `docs/batch40/ship_ids.txt` | 3203 | intermediate id list of batch 40 (ship/retry/other-ids working lists); the ids are in verified/rejected-sources-batch40 and the C tables |
