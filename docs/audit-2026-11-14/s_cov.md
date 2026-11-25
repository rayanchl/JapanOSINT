# OPS-07 coverage report (branch of worktree agent-a0ad88d078ba71867)

Head commit: 3b46603 (on top of a91503d, on top of 5513544 = claude/peaceful-ritchie-i7e0hj)

## 1. Measure
`native/tools/audit_test_coverage.py` (offline). A module counts as tested when a test file
(tests/unit/*.c, tests/*.c) #includes its .c or header, or calls a non-static function its .c defines
(comments stripped). `make test-coverage-list` runs it; `make tools-selftest` runs its `--self-test`;
both are in `.github/workflows/ci.yml` beside the other Python tool checks.
The audit measures 145 modules (116 core/*.c + 29 lib/*.c), not the 141 in the audit item.

### Before (24 untested of 145)
core/aoiapi.c core/breach_jobs.c core/camera_store.c core/collectortab.c core/docmeta.c
core/entity_stats.c core/geoproxyapi.c core/isochrone.c core/linegeom.c core/maint_detect.c
core/metricsapi.c core/miscapi.c core/pagination_probe.c core/qembed.c core/relevance.c
core/service_vec_pod.c core/station_clusterer.c core/station_footprints.c core/transitapi.c
lib/arcgis_dir.c lib/bigfile.c lib/camfeature.c lib/linecolor.c lib/probe.c

### After
`modules: 145  tested: 145  untested: 0  allow-listed: 0`

## 2. Allow-list (native/tools/test-coverage-allow.txt)
Empty. No module was skipped. The script also fails on a STALE allow-list line (listed but tested),
so the file can only shrink.

## New tests (all in native/tests/unit/)
| test file | modules |
|---|---|
| test_small_modules.c | lib/bigfile, lib/linecolor, lib/camfeature, lib/probe, core/collectortab, core/linegeom |
| test_ops_db_modules.c | core/maint_detect, core/metricsapi, core/miscapi |
| test_relev_check.c | core/relevance, core/qembed |
| test_entity_stats.c | core/entity_stats |
| test_breach_jobs.c | core/breach_jobs |
| test_geoproxy.c | core/geoproxyapi (cache-only, no network) |
| test_aoiapi.c | core/aoiapi (/api/aoi + /api/watchlists, role + tenant gates) |
| test_docmeta.c | core/docmeta (hand-built ZIPs, inflate bomb, every-truncation sweep) |
| test_station_footprints.c | core/station_footprints |
| test_camera_store.c | core/camera_store |
| test_isochrone.c | core/isochrone (fixture GTFS) |
| test_pagination_probe.c | core/pagination_probe (no network) |
| test_arcgis_dir.c | lib/arcgis_dir (local python stub) |
| test_transitapi.c | core/transitapi |
| test_station_clusterer.c | core/station_clusterer |
| test_service_vec_pod.c | core/service_vec_pod |

## 3. Bugs a test exposed (all fixed, minimal)
1. core/aoiapi.c, aoi_list and wl_list: keyset paging bound `k.p` twice, so `?2` was the date,
   `?3` was the date again (as the id), `?4` (LIMIT) was the uuid string and `lim` went to a
   nonexistent `?5`. Page 2 of /api/aoi and /api/watchlists never returned rows. Removed the duplicate bind.
2. core/breach_jobs.c, breach_job_fetch: `source_id` became the staging file name
   (`<staging>/<source_id>.dat`), so `../../x` wrote the downloaded body outside the breach dir.
   Now only `[A-Za-z0-9._-]`, not leading `.`, else 400.
3. core/maint_detect.c + core/schema.sql + core/db.c: the `never_produced` verdict (the silent-empty
   fleet cohort) violated `collector_anomaly.verdict`'s CHECK. The INSERT failed unchecked and the log
   said "anomaly opened". schema.sql widened for fresh DBs; new boot migration `anomaly_verdict_migrate`
   (db.c) widens the stored CHECK on deployed DBs in place (writable_schema + schema_version bump,
   idempotent; tested against a recreated pre-fix table incl. data survival); insert_anomaly now logs
   "NOT stored" on failure instead of "opened".
4. lib/camfeature.c: the 44-byte uid prefix cut left a lone UTF-8 lead byte (the old loop only
   trimmed continuation bytes). Now backs up over the whole partial sequence.
5. lib/linecolor.c: `normalize_hex` stopped reading at 7 chars, so `#1234567` / `#12345678` were
   accepted as `#123456`. Reads 8 and rejects.
6. core/pagination_probe.c: the CSV fingerprint set was not sorted (JSON's was), but overlap() walks
   sorted runs, so a reordered repeat read as low overlap, i.e. "paginates", and queued a url_swap.
   Sorted.
7. core/transitapi.c station_summary: `uid LIKE '%|' || ?1` with the caller's path segment; `%` or
   `_` were wildcards (/station/%25/summary returned the first station). Replaced with an exact
   suffix compare.
8. core/camera_feed.c: `-Wformat-truncation` warning on the toolchain here (tree was not at 0
   warnings). Explicit `%.*s` bound, same behaviour.

Test-only discoveries worth knowing (no product change): intel sink de-duplicates a uid repeated within
one sink (camera_upsert merge needs a fresh sink per run, as the scheduler does); aoiapi.h says rings
are "auto-closed on the way in" but it is alert_eval's compile that closes them (stored bytes are the
caller's); arcgis_dir emits a typeless service record without an invented id (nothing dropped);
`miscapi_list_layers` is slow on 105k sources (its test takes minutes: sources_has/find_layer are
linear scans per source) - a perf observation, not changed.

## 4. Verbatim final results (from native/, JO_DB scratch under scratchpad, prefix cov_)
- `make -j6`: no warning/error lines, rc=0
- `make lint-sources`: `registered source_defs: 105228  (1179 direct + 104049 macro-expanded)` / `lint-sources: OK` /
  `lint_env_readers: 0 findings (one environment reader: lib/joenv.h)`
- `make tools-selftest`: `Ran 6 tests ... OK`, `audit_test_coverage self-test ok`
- `make test-coverage-list`: `modules: 145  tested: 145  untested: 0  allow-listed: 0`
- `make unit` (one uninterrupted run): rc=0, 142 test programs + cleanup_stale_uids and
  measure_thresholds self-tests, no FAIL/BUILD FAILED lines. Summary: scratchpad/unit_summary.txt
  (last lines: `test_zipread: all passed`, `--- measure_thresholds self-test ---`, `all passed`).

Environment note: mid-task the shared volume hit 0 bytes free (many parallel worktree builds); I deleted
and rebuilt only my own worktree's obj/ to recover.
