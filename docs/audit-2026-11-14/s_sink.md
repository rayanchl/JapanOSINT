# Batched ingest sink (native/core/intel.c)

Branch: worktree branch of agent-ae595edcaa910e66d (see commit hash in the reply). Merged `claude/peaceful-ritchie-i7e0hj` (audit fixes: translation clearing in the upsert, jo_iso_now) before final build/measurement; both kept. All numbers below are on the merged tree.

## Design
- DEFERRED WRITE, not an open transaction. emit() does everything that needs no DB on the collector thread (uid, timestamp, content hash, MeCab of title/body/summary/link/author/tags/props) and parks a deep copy (`pend`). Nothing is written and NO transaction is open while the collector waits on HTTP.
- flush_buf(): one `BEGIN IMMEDIATE`; per record the unchanged probe / upsert / FTS statements (identical SQL, `rec_write`); `COMMIT`; then, post-commit, `seen_add` (stored/notices counted only for committed uids), `alert_eval_on_item` for NEW rows, then simhash for every written row inside one more `BEGIN IMMEDIATE..COMMIT` (simhash joins an open txn). Order of kinds is as before (alert, then simhash); a simhash batch failure leaves fingerprints NULL for the backfill, as before.
- Flush triggers: JO_SINK_BATCH records (200), JO_SINK_BATCH_BYTES (8 MB), oldest record age >= JO_SINK_BATCH_MS (500, checked at each emit), intel_sink_stored(), intel_sink_stored_notices(), intel_sink_flush(), intel_sink_free(). JO_SINK_BATCH=1 = old per-record path with exact emit() returns.
- Dispatcher: osint_dispatch only counts emit()'s return (refused/new) and keeps records in memory; it never reads rows back. Rebound sinks (intel_sink_rebind, used by dispatch) are forced to batch=1 so its record_count/any_new/n_refused stay exact. Same connection reads its own uncommitted writes anyway; no other-connection reader depends on the sink mid-run.
- Failure: any failure in a batch (statement refused, COMMIT refused) -> ROLLBACK, redo record by record each in its own txn; only records failing alone are counted failed. Two consecutive BEGIN failures (lock storm) fail the rest at once instead of 5 s each.
- failed= truthfulness: a refusal is learned at flush time, so it is returned as -1 from a LATER emit of the same kind (record/notice), once per refused record; scheduler's per-call `failed` counter therefore sees it. Refusals learned at the final flush (no later emit) are exposed by new `intel_sink_failed()` (total) and `intel_sink_failed_pending()` (not yet returned). NOT wired into scheduler.c (not mine): add `cs.failed += intel_sink_failed_pending(&inner)` after intel_sink_stored(); until then such records show as stored<records (UID-COLLISION wording) and a stderr line `[intel] ... write failed`.
- Trace: JO_SINK_TRACE=1 prints each flush's write-txn and post-commit cost.

## Behaviour changes
- emit() on a batched sink returns 0 (accepted), not 1 for new; -1 for no uid, and deferred -1 for DB refusals. Nothing in the tree branched on 1 except tests (test_fts_rebuild now sets JO_SINK_BATCH=1; test_intel_sink_write sets 1 for its per-record assertions and has a batched section 9).
- Rows are visible to other connections only after a flush (<= 500 ms of age at the next emit, or end of run). A collector that emits then waits a long time leaves its last records unwritten until the next emit/end of run (visibility only, no lock, nothing lost: scheduler always reads counters and frees).
- Unchanged re-fetches now run MeCab at emit() time (the probe happens later), costing ~12 us/record more (20 -> 32 us) but moving it outside the lock.
- fetched_at is taken at emit time as before.

## Worst-case lock hold
The write lock is held only inside flush: write txn then simhash txn (alerts run between, lock free). Measured with JO_SINK_TRACE on the bench (200-record flushes, all-new Japanese/English, heavily clustering text): write_txn 1-72 ms (typ. 5-25), post (alerts+simhash txn) 0-170 ms. So worst observed ~0.2 s total, <= 2 lock acquisitions per 200 records. Idle (parked) time holds no lock; the old per-record path took 2 commits per record. Bounded by batch size (200) and by the 5 s busy_timeout on acquisition.

## Measurements (bench_sink_rewrite.c, 20,000 records, WAL autocheckpoint off, clock includes final flush)
| pass | old (audit-branch intel.c) | new, JO_SINK_BATCH=1 | new, batched (default) |
|---|---|---|---|
| (a) 20k new | 75.7 s, 264 rec/s, 165,757 B WAL/rec | 85.8 s, 233 rec/s, 165,757 | 50.8 s, 393 rec/s, 8,524 |
| (b) 20k unchanged | 0.40 s, 50,072 rec/s, 4,120 B | 0.56 s, 35,507, 4,120 | 0.64 s, 31,239, 547 |
| (c) 20k changed | 42.8 s, 468 rec/s, 162,162 B | 35.6 s, 562, 162,162 | 17.0 s, 1,177, 11,530 |
WAL per record down 19x (new), 7.5x (unchanged), 14x (changed); throughput 1.5x/2.5x for new/changed; unchanged is ~25-35% slower in CPU (segmentation moved before the probe) but writes 7.5x less WAL. (Machine noisy: other agents building; the WAL numbers are exact, times are indicative.)

Real sources, JO_SINK_BATCH=1 vs default (fresh DB each): gnews-mon-aviation-incident 89 records/89 stored, 89 simhash, 58 clusters, FTS 89 both (661 -> 263 ms); JO63_EUODS_D151D660E1 2000 rec, stored 2002 (2 notices), 2002 FTS both (19.1 s -> 9.4 s); bird-flu-outbreaks 24/24/24 both (1.76 s -> 0.58 s). Identical counts.

## Tests (from native/, JO_DB=$PWD/.scratch.db)
- `make -j2`: 0 warnings, 0 errors (merged tree).
- `JO_UNIT_ONLY='^test_(intel_sink_batch|intel_sink_write|intel_tenancy|sched_runline|sched_pool|collector_run_gate|dispatch_pool|dispatch_resolve|fts_rebuild|fts_segment_body|search_tenancy|search_runs_workspace|corpus_hybrid|relevance|geojson|ckan_paging|alert_tenancy|embed_pod)$' tests/unit/run.sh`: all pass after two test fixes (test_fts_rebuild asserted emit()==1 -> JO_SINK_BATCH=1; my test's DBs now start from the seeded template because a bare db_open lacks the translate columns the merged upsert names).
- test_intel_sink_batch pins: identical intel_items/FTS/uid_map/simhash bands/clusters/counts batch=1 vs 200 (700 records incl. notices, geo, Japanese, near-duplicates, in-batch repeated uid, unchanged and changed re-fetch); mid-batch refused record (trigger RAISE) loses only itself, counts/failed/-1 correct (batch 200 and batch 3, record and notice kinds); alerts once per new row batched==unbatched, none on re-fetch; flush on limit/age/flush()/counters/free; parked batch holds no write lock (second connection BEGIN IMMEDIATE succeeds, sees rows only after flush); rebound sink exact 1/0/-1.
- `make hptest`: all passed.
- Not run: the full 88-test suite (only intel/sink/scheduler/dispatch/FTS/search related ones, individually), TSan/ASan builds.

## Not done
- scheduler.c wiring of intel_sink_failed_pending (out of scope; one line, above).
- COMMIT-failure path is exercised only through the record-failure fallback (no fault injection for a failing COMMIT).
- No time-based flush while a collector idles (would need a thread or lib/ hooks): visibility only, by design.
