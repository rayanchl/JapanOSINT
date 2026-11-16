# Scheduler structure fixes (ENG-07, ENG-17, ENG-18, design weaknesses 2, 3, 4, 8)

Worktree branch: see commit hash in the reply. Base: origin/main 152a492, merged with
claude/peaceful-ritchie-i7e0hj (2f2e3ba). Merge conflicts: scheduler.c (kept both
`collector_records` and my `disclosed`; the sources row gets data_records = stored minus the
disclosure, the empties streak gets collector_records) and test_hostgate.c (kept both new tests).
All verification below is on the merged tree.

## Design per item
1. Host-aware dispatch (scheduler.c, hostgate.c). `hostgate_peek_ready_ms(host)`: non-blocking,
   non-claiming (0 = ready; gap/penalty remaining; 100 when only the in-flight cap blocks).
   Scheduler keeps an interned host table; a source's host is learned at run time (first host the
   run contacted) and seeded before the first run from the row's portal url (hint only). q_pop skips
   an entry whose host is at its in-flight cap (1 for hosts paced >= 1 s, else JO_SCHED_HOST_INFLIGHT=2)
   or not ready, takes the next eligible, and sleeps (cond wait, 200 ms) if all are blocked; q_done
   wakes it. Never starves: skipped >= JO_SCHED_HOST_SKIP_MAX (1000) pops, or queued longer than its
   interval, is taken anyway. Heartbeat appends `hosts busy=N inflight=N top=h:n,.. skip_pops= forced=`.
2. Due-time heap. min-heap of (due, row) with lazy deletion (a pair is valid while next[i]==t); every
   change of next[i] goes through plan_set(). Deferred rows (due, no room) sit in per-lane FIFO lists and
   stay due; sched_publish, policy/override re-plan (sched_replan, still O(n) but only on a change) and
   the boot plan (heap built from next[] once) all feed it. Watchdog scans an active-run list instead of
   the registry. Extracted sched_tick() so it is testable. The 60 s heartbeat still walks the registry
   (once a minute, kept deliberately).
3. realtime_max_sec (cadence.c/.h, source_interval.c, main.c --schedule). Policy field, app_settings key
   `sched.realtime_max_sec`, default 300, 0 disables, env JO_SCHED_REALTIME_MAX_SEC. Row declared <= it is
   exempt from the GLOBAL interval (decided on declared, not the adaptive base); clamps and per-source
   override still apply. GET shows policy.realtime_max_sec and estimate.realtime_exempt_rows; estimate counts
   it. PUT works through cadence_parse/cadence_set which httpd.c already calls (now at httpd.c ~2659), so
   NO httpd.c change is needed. Pages show policy "realtime"; `--schedule` basis `realtime-exempt` (79 rows
   on the 105,229-source registry). A policy saved before the key existed reads 300, not off. CLAUDE.md paragraph added.
   Note for the client: sourceapi `policy` can now be "realtime" (new string).
4. Workers: default 8 -> 24 (see table).
5. Cancel window: max(deadline, min(interval, JO_SCHED_CANCEL_MAX_SEC)), cap default 900 s. A 6 h cadence
   now gives 900 s, a 5 s row still 300 s... (deadline 300), warn line prints the window.
6. ENG-07: scheduler-disclosed refusal: sources row records data_records (disclosure subtracted) so status is
   'degraded' (run ok) with the reason in error_message, records_count 0, never 'online'; a no-host-answered run
   stays 'offline'. consecutive_empties grows (via collector_records). run_status() and fetch_log untouched.

## Measurements
Dispatcher tick, 105,229 registered rows (102,992 scheduled), mean of 1,000 ticks, machine loaded by other builds
(load ~13): steady state (~5 due rows/tick) old whole-registry walk 637 us vs new heap tick 28.4 us (the new figure
includes queueing, i.e. DB reads per queued row); nothing due: old 499 us vs new 0.01 us.

Worker count, UNBATCHED sink, 300 s boots from fresh copies of one DB, same load conditions, network via proxy:

| workers | runs / 300 s | runs/s | failed= emits | SQLITE_BUSY/locked | deferred at end |
|---:|---:|---:|---:|---:|---:|
| 8  | 217 | 0.72 | 0 | 0 | 25,404 |
| 24 | 363 | 1.21 | 0 | 0 | 25,235 |

No contention seen at 24, gain 1.7x (not 3x: boot ramp, hostgate pacing, shared egress, other agents on the box).
Default changed to 24 (clamp 1-32). Short windows; re-measure after the sink batching lands.

Live boot (final binary, 190 s, 24 workers): 253 runs, heartbeat e.g.
`busy=24/24 queued=2048 | hostgate inflight=1 waited=2486 over_budget=0 | ... | deferred=18339 | hosts busy=18 inflight=19
top=api.sejm.gov.pl:2,gegevensmagazijn.tweedekamer.nl:1,www.nouvelobs.com:1 skip_pops=222 forced=0`.

## Tests (cd native; export JO_DB=$PWD/.scratch.db)
- `make -j2`: 0 warnings.
- `JO_UNIT_ONLY='^test_(sched_backoff|sched_pool|sched_runline|sched_dispatch|source_interval|cadence|hostgate|sourceapi|source_settings)$' tests/unit/run.sh`: all pass.
  New: tests/unit/test_sched_dispatch.c (heap, tick, deferral, stale pairs, publish, replan, host-aware pop, forced pops,
  cancel window, tick cost); test_cadence.c realtime section; test_hostgate.c peek; test_sched_runline.c ENG-07 asserts.
- `make hptest`: all passed. `tools/ci_concurrency_gate.sh ./bin/japanosint 1.0`: ok (worst 0.75 s).

## Not done / caveats
- Full `make unit` not run (only the listed tests). Heartbeat O(n) walk kept (once a minute).
- Host hint from portal url may be wrong for some rows until their first run teaches the real host.
- No httpd.c change needed.
