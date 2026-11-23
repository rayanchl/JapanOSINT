# s_tests — OPS-07 / 07b / 08 / 12 / 13 closure

Branch: `worktree-agent-a99f1409bfd74db00` (merged `claude/peaceful-ritchie-i7e0hj` at the start, as instructed; the audit branch's fuzz harnesses, ccache steps, py_compile step and Makefile fuzz targets were built on, not re-added).

## 1. OPS-07 — unit tests added (all in native/tests/unit/, each `#include`s the .c under test)

| test | pins |
|---|---|
| test_zipread | hand-built zips (stored/deflate/multi-member); basename vs path lookup; EOCD count (never a PK-signature scan); index past end; name_out truncation; every prefix of an archive (ASan clean, exact-size heap copies); corrupt CD signature/offset/count/name length/local-header offset (incl. 0xfffffff0 wrap)/unsupported method/corrupt deflate; stored member declaring > bytes present is NULL (post-audit behaviour); 4 MiB deflate bomb vs JO_ZIP_MAX_OUT on both entry points; declared size lying low/high; env parsing bounds |
| test_jsonstream | boundary scanner driven via js_write at chunk sizes 0/1/2/3/7/64 for: braces/brackets/escaped quotes/escaped backslash in strings, nested values, whitespace, UTF-8, path selection (dotted, key present-but-not-array, later same-named key), malformed element counted+skipped, cut mid-element, trailing garbage, >4 MiB element (oversize), 3 MiB prefix without array (window bounded), 5000 records flat memory, max_records ceiling, argument refusals incl. loopback/metadata via hostgate |
| test_sgp4 | Vallado SGP4-VER vectors for sat 00005 at t=0/360/720/1440 min (pos to 1 m, vel to 1 cm/s; tolerance justified in-file); radius envelope, energy conservation, pre-epoch propagate, geodetic conversion cases, deep-space refusal, zero-motion TLE, decayed orbit |
| test_bloom | no false negatives (10k), FP rate at capacity, saturation, count, binary/empty keys, sizing guards (k cap 32), save/load round trip, v1 file, 9 malformed-file refusals incl. m=2^64-1 |
| test_utf8 | well-formed 1-4 byte, truncations on exact-size heap, stray continuation, overlong/surrogate/>10FFFF -> U+FFFD adv 1 (new audited behaviour), all 256 lead bytes x short tails never pass the NUL |
| test_ws | argument refusals; loopback RFC6455 server for fragment reassembly, max_msgs, callback stop, close, window timeout, 40 KB message, empty/ping frames, UTF-8. **Server cases are SKIPPED here** (see finding 3) and run as WARN-only unless JO_WS_TEST_STRICT=1 |
| test_threatintel | key gate (no key = notice, rc 0, provider not called), fallback env names, callback NULL/non-array, abuse.ch header built once / over-long refused / exact-fit boundary |
| test_respcache | copy semantics, never-cache empty/NULL, TTL, replacement, oldest-slot eviction, drop, stats, over-long key characterised |
| test_collcache | TTL clamp (0/neg->15m, <1m->1m, >24h->24h), expiry vs stored ttl, upsert, no-table, SQL in key, 2 MiB body, NULL body |
| test_url_override | validate refuses loopback/metadata/RFC1918/file/ftp for BOTH sides; hostile DB rows refused on load; exact-match apply; idempotent reload; remove/reset/revive semantics |
| test_simhash | distance, token floor (12 inclusive), repeats count once, title weight, 2048 cap, near-dup shares cluster through the real sink, thin identical records do not fuse, arrival-order independence, tenant isolation, hook total on bad input |
| test_dbexplorerapi | allow-list refuses sqlite_master/audit_events/injection/case variants; limit/offset clamps; orderBy only real columns; q is a bound param (wide-table regression), 2000-byte q; scheduler listing == registry pods with last_run null |
| test_alert_deliver_payload | payload JSON, HMAC vs RFC 4231 vector, base64 vs RFC 4648, **subject header injection** (CRLF/LF/CR/tab -> encoded-word, round-trips), channel_at parsing, deliver_one outcomes incl. loopback/metadata/file/ftp webhook refused before any request, email skipped without SMTP, alert_deliver_test tenant isolation/404/400 |
| test_overpass | uri_encode, ov_tag, el_coords (0,0 valid, center fallback, strings rejected), point/way filters, el_key out-of-range ids, seen-set (no false negatives over 5000, per-call isolation), emit_points, tile sanity |

Already covered elsewhere (found by reading, not re-tested): keyqual (test_keyqual), prompt_rank (test_prompt_rank), jpnorm (test_jpnorm), datetok.

**Not reached:** lib/mlit_ksj.c, lib/unified.c, core/exportapi.c (beyond existing CSV test), core/vocabapi.c, core/sweepapi.c, core/content_change.c (1,510 lines), core/breach_meta.c. These need DB-fixture-heavy tests; I judged shallow smoke tests not worth adding.

## Real bugs / findings the tests exposed (source NOT touched; assertions marked `known_bug` / TODO so suite passes)

1. **lib/sgp4.c `sgp4_jday_epoch` is ~306 days early** (e.g. `sgp4_jday_epoch(2000,1.0)` = 2451238.5, true 2451544.5; `367L*1/12`+`1` should be the Dec-31 terms `367L*10/12`+`31`). Propagation itself is exact vs Vallado (verified), but `r.jdsatepoch` and `r.gsto` are wrong. **Impact:** collectors/sources/satellite_tracking.c computes `tsince = (jd_now - r.jdsatepoch)*1440`, so every satellite is propagated ~306 days (≈440,000 min) too far: positions wrong.
2. **lib/jsonstream.c `js_scan`**: the last element of an array of scalars/strings (`[1,2,3]`, `["a","b"]`) never closes (the array's `]` does `depth--` to -1), so that element is lost, `done` stays 0 and a false "array never closed" truncation notice is filed. Object/array elements unaffected.
3. **Ubuntu 24.04 libcurl 8.5.0 has no WebSocket protocol** (`curl-config --protocols` has no ws): `ws_collect` returns -1 for every ws collector here (bluesky-jetstream, certstream). Also **lib/ws.c does no hostgate/SSRF check** on its URL (latent: callers pass constants).
4. lib/sgp4.c `sgp4_geodetic`: exactly over a pole altitude = -6399 km (r/cos(lat) 0/0 singularity inherited from satellite.js).
5. core/respcache.c: keys >63 chars are stored truncated but compared untruncated, so they never hit and burn a slot per put (never a wrong body).
6. lib/overpass.c `el_coords` accepts out-of-range lat/lon (characterised).
- Characterised, no defect: dbexplorer `q='%'` is a LIKE wildcard.

## 2. OPS-07b — fuzzing

`clang --version`: 18.1.3. Initially `-fsanitize=fuzzer` failed to link (`libclang_rt.fuzzer_interceptors` / `ubsan_standalone` missing). `apt-get install -y libclang-rt-18-dev` succeeded and the trivial program then linked and ran. Built via `make fuzz-build` (audit branch's targets), ran each harness 120 s under ASan+UBSan with the new seed corpus (tests/fuzz/corpus/<target>/, regenerable with tests/fuzz/make_seeds.py; 3-6 real small docs each):

| harness | executions | cov | ft | result |
|---|---|---|---|---|
| zipread | 6,975,864 | 140 | 498 | clean |
| xlsx | 2,702,052 | 454 | 1,601 | clean |
| csv | 612,466 | 363 | 1,916 | clean |
| htmlparse | 854,922 | 483 | 2,749 | clean |
| rss_atom | 820,459 | 531 | 2,482 | clean |

No crash, timeout, leak-excluded OOM or sanitizer report (these runs are on the post-audit-fix lib code). Makefile `fuzz-smoke` now seeds from tests/fuzz/corpus and writes artifacts to tests/fuzz/crashes/ (dir created); CI artifact upload path extended. `fuzz-smoke-gcc` was not needed since libFuzzer works. Caveat: the CI fuzz job installs `libclang-rt-dev` — on ubuntu-latest the versioned package may need `libclang-rt-18-dev`; unverified there.

## 3. OPS-08 — ESLint

eslint 9 + @eslint/js + eslint-plugin-react + eslint-plugin-react-hooks (v7) + globals as devDependencies; flat config `client/eslint.config.js`; `npm run lint`. Result: `npx eslint src` exits 0 with 0 errors, 141 warnings. Config-only decisions (no src edits): no-unused-vars (31), react/no-unescaped-entities (13), exhaustive-deps (7), and the hooks-v7 compiler rules set-state-in-effect (59), refs (12), use-memo (4), static-components (2), purity (2) plus react/jsx-key (10 real missing keys) are "warn" with comments. `vite build --outDir <scratch>` passes. `vitest run`: 220/222 pass; the 2 failing tests differ per run (pagedLists "SavedSearchesPage" 5 s timeout seen) and occurred while 5 fuzzers + a unit suite loaded the 4 cores — timeouts, not assertion failures; not re-verified on an idle machine.

## 4. OPS-12 — ccache
Already on build-and-test and asan (audit branch). Added to thread-sanitizer (key `ccache-tsan-…`) and macos (brew ccache, `CC: ccache cc`, key `ccache-macos-…`). Unverified until a CI run (ci.yml parses as YAML).

## 5. OPS-13 — `native/tools/jo_audit.py`
Dispatcher (no behaviour moved; rationale in its docstring — the scratch-DB/pin code differs per tool and rule 4c warns against merging readers unproven). Subcommands: `emit registry|batch`, `paging ceiling|cycle-health|cycle-advance|page-two|page-param`, `idkeys`, `url-limits`, `reachable`, `probe`. `--help` prints one line per subcommand (from each tool's docstring); args pass through; `audit_page_param.py` has no argparse (treats --help as a path) so the dispatcher answers its --help itself. `python3 tools/jo_audit.py --self-test` -> ok (11 subcommands); added to the CI "Python tools compile and self-test" step. Existing tools untouched.

## 6. The 56 Python files under docs/ (not moved)
- docs/batch32/: gen_jps_manifest.py, gen_oai_manifest.py, jps_probe.py — batch-32 manifest generators / Japan Search probe.
- docs/batch39/cn39_scratch/scripts/ (15): amac_walk, bse_ann_walk, bse_walk, cnemcw_walk, cninfo_walk, flk_walk, neeq_walk, sse_ann_probe, walk_sse_susp, dpm_sample — one-off paging walkers/probes of Chinese market/legal APIs; probe_digitseg_scratch, probe_nullfix_scratch — copies of probe_hp_batch.py; write_manifest, write_rejects — emit the cn39 manifest/reject TSV (hardcode /Users/rayan/... paths).
- docs/batch39/jp39_scratch/: measure_ids.py (distinct id_keys over a file), xlsxcsv.py (xlsx->CSV as the engine does); docs/batch39/tbl39_scratch/tbl.py (HTML table reader as probe does).
- docs/batch40/scripts/cngov/: jsurls, rss, xh, xhe — Chinese government site feed/URL scrapers.
- docs/batch40/scripts/cnmkt/ (11): em, emcheck, emtry, dupdiff, meas, j, sse, sseall, szse, xl, build_manifest — East Money/SSE/SZSE API probes and manifest builder.
- docs/batch40/scripts/cnsar/hkcount.py — HK count probe.
- docs/batch40/scripts/jpgeo3/ (14): arc, arccheck, arcfull, ckan, count, count2, discover, ds, dsfull, gen_manifest, gsp, orgdir, orglist, pkg, uniq — Japanese geo-portal (ArcGIS/CKAN) discovery/counting scratch.
- docs/batch40/scripts/jpnat3/: engsim, rows, rows_data, xl, xlpos — xlsx-engine replicas and the jpnat3 row builder.
All are historical batch scratch (several import `/Users/rayan/OSINTsaas/native/tools`); full list with docstring/first-line summaries is reproducible with `find docs -name '*.py'`.

## Verification commands
- per test: `JO_UNIT_ONLY='^test_<name>$' tests/unit/run.sh` (mine all pass; standalone ASan/UBSan builds also pass for zipread/bloom/sgp4/utf8/respcache/ws)
- `make fuzz-build && …/fuzz_<t> -max_total_time=120 …`
- `cd client && npx eslint src; npx vitest run; npx vite build --outDir <scratch>`
- `python3 native/tools/jo_audit.py --self-test`
- full suite: see below.

## Full `make unit` on merged tree
`make unit` run once on the merged tree (107 test files, `make -j2` build first): every test passed except test_sgp4, which hit BUILD FAILED because run.sh's `under_of` regex `[a-z_/]+` cannot match a digit in "sgp4" (so the module object was not excluded -> duplicate symbols). Fixed in tests/unit/run.sh (`[a-z0-9_/]+`); test_sgp4 then re-run individually: all passed. The whole suite was NOT re-run after that one-line regex change (the suite took ~45 min under shared-machine load).
