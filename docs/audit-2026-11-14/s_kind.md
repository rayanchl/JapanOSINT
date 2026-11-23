# Explicit source kind (replaces the collector `_` prefix)

## Enum and derivation
`native/source.h`: `typedef enum { SOURCE_KIND_FEED = 0, SOURCE_KIND_PIVOT, SOURCE_KIND_POD, SOURCE_KIND_PROBE } source_def_kind;`
Named `source_def_kind` (not `source_kind`) because `core/source_card.h` already owns `source_kind` (SC_KIND_*, the index vocabulary, now derived from this).
Field `source_def.kind` (last member, default FEED = "undeclared").
Accessors in `registry.c`: `source_def_kind_of()` (declared kind if non-default, else PIVOT when `registry_is_entity_pivot()`, else FEED), `source_def_is_internal()` (POD or PROBE), `source_def_kind_name()`, `registry_kind_mismatch_count()`.
PROBE is distinct from POD: it has no schedule or lane (interval 0, on-demand operator tool, e.g. PORT_SCANNER raw sockets); it shares every POD exemption through `source_def_is_internal`.

## Sites converted
- scheduler.c: `sched_is_pod` (so `lane_of`, cadence exemption in `sched_row_interval`, source_interval.c, cadence.c, httpd.c run gates, metricsapi.c, main.c all follow), host-learn guard, `real_collector` guard.
- registry.c: `source_never_runs`, `registry_is_entity_pivot`, new KIND-MISMATCH self-check in `index_build` (the only remaining prefix read).
- osint_dispatch.c: fetch_log/anomaly guard. source_card.c: pod kind. sourceapi.c `derive_kind` ("internal"). statusapi.c `kind` ("internal"). dbexplorerapi.c `dbexplorer_scheduler`.
- main.c: `--list-sources` prints `kind=`; `--schedule` has a new `source_kind` column (legacy `kind` column kept; pod/probe now print "pod").
- Pod/probe registrations given explicit kind: 12 collectors/pod files, core/{camera_refframe,camera_stills,embed_pod,entity_vec,image_vec,media,service_vec_pod,translate}.c (POD), port_scanner.c (PROBE).
- API: existing `kind` strings unchanged; additive `sourceKind` (feed|pivot|pod|probe) in /api/status rows and source detail `registry` block.
- Makefile registry-floor fails on KIND-MISMATCH. tools/audit_registry_emit.py reads `kind=` (fallback to prefix).
- CLAUDE.md rule 3 line added; docs/ENVIRONMENT.md regenerated (was stale after the merge, unrelated to this change).

## Counts (./bin/japanosint --list-sources)
feed 102,973 / pivot 2,235 / pod 20 / probe 1 (total 105,229); 21 collectors start with `_`, all POD/PROBE; 0 KIND-MISMATCH.

## Left as cosmetic / untouched
registry.c self-check (reads prefix to compare), collectors.def and tools/gen_collectors_def.py (grouping/labels), statusapi.c "_breach" literal (label), tools/audit-harness/slice.py (inventory filter), comments in headers (entity_stats.h, simhash.h, content_change.h, uploadapi.h, etc. still say collector "_maint"; true as labels). audit_cycle_*.py never classified by prefix. No lib/ edits.

## Tests / gates (from native/, JO_DB scratch)
`make -j8` 0 warnings; hptest, pagewalktest, lint-sources, source-floor, registry-floor (0 kind-mismatches), audit-sources (0), collectors-def-check, env-doc-check (after regen) all pass.
New tests/unit/test_source_kind.c (derivation; every `_` registry collector POD/PROBE; non-underscore POD exempt from cadence + maintenance lane; `_` FEED and plain POD reported as exactly 2 KIND-MISMATCH). Fixtures in test_collector_run_gate, test_sched_pool, test_sched_backoff, test_sched_runline updated.
Targeted run: `JO_UNIT_ONLY='source_kind|cadence|sched_|...' tests/unit/run.sh` all passed. Full suite: 89 of 111 tests in `make unit` (killed by the 600 s tool timeout inside test_source_category, which passed) + remaining 22 run via JO_UNIT_ONLY: 0 failures, all 111 ran.
