# Source scheduling audit — 2026-11-14

How every registered source is scheduled, as the dispatcher actually plans it,
with the sources themselves run through the binary. Every number below was
measured on this tree (main at `3579f06` plus this branch); the per-source
table is reproducible with one command:

```sh
cd native && ./bin/japanosint --schedule > schedule.tsv   # new in this change
```

`--schedule` prints one row per source from the same functions the dispatcher
plans with (`sched_next_delay`, `sched_next_due`): declared interval, adaptive
flag, measured interval, per-source override, **the interval that runs**, what
decided it, failures, last run and seconds until next due.

> **After merging main at `61929b56` (batches 64 and 65) the registry is
> 93,163 sources — 90,907 scheduled, 2,237 pivots, 19 pods** (`--schedule` on
> the merged build). The tables below are the measurement as taken, on
> 85,767; the only conclusion the larger fleet moves is the worker count: at
> the same 6.2 s mean run, 90,907 sources every 6 h need **27 workers or
> more**, and the default 8 lap the fleet in about 20 h.

> **Follow-up, same day: the real-time feeds are now outside the policy.** A
> source declaring an interval under 300 s runs as declared under any policy
> (`CAD_REALTIME_SEC`, `core/cadence.h`), with 64 queue slots reserved for
> such rows and first place in the queue. Live boot at 8 workers under a full
> backlog: `wolfx-eew` (10 s) ran 21 times in 240 s, the 60 s feeds 4 times.
> Section 2's "22 real-time feeds" paragraph describes the state before this.

## 1. The registry, by how it is reached

| kind | sources | how it runs |
| --- | ---: | --- |
| scheduled collector | 83,511 | dispatcher, collector lane (`JO_SCHED_WORKERS`, default 8) |
| entity pivot (interval 0) | 2,237 | on demand only, never by the scheduler |
| maintenance pod (`_` collector) | 19 | dispatcher, maintenance lane (2 workers) |
| **total** | **85,767** | |

## 2. What interval each scheduled source runs at

One function decides (`sched_row_interval`), in this order: a per-source
override set by an operator → else the system-wide policy applied over the
row's base (its freshness measurement when the row is adaptive, else its
declared interval). Health then stretches it: each failure doubles the wait up
to 24 h, and 12 consecutive failures quarantine the source to one probe a week.

On an unmodified install (empty database, no environment) the compiled policy
is **global 6 h**, so:

* **all 83,511 scheduled sources run every 21,600 s**, whatever they declare;
* the 25,846 adaptive feeds still measure their freshness, but the global
  policy outranks the measurement, so it changes nothing until the policy is
  switched to "clamped" or "declared";
* runs per day: **334,044** under the policy against **662,936** as declared.

What the rows declare, and therefore what the policy overrides:

| declared interval | sources | effect of the 6 h policy |
| --- | ---: | --- |
| under 5 min | 22 | slowed 180x–2,160x |
| 5–59 min | 1,002 | slowed |
| 1 h | 16,369 | slowed 6x |
| 1–6 h | 2,444 | slowed |
| 6 h | 21,934 | unchanged |
| 6–24 h | 803 | sped up |
| daily | 31,646 | sped up 4x |
| weekly | 9,077 | sped up 28x |
| monthly or slower | 214 | sped up 120x+ |

**The 22 real-time feeds are the decision to look at.** `wolfx-eew`
(earthquake early warning, declared 10 s), `jma-earthquake`, `jma-tsunami`,
`jma-intensity`, `p2pquake-jma`, `nerv-feed` (60 s), `odpt-train` / `odpt-bus`
(30 s), the flight and train trackers — each runs **once every six hours**
under the default policy. The mechanism for an exception exists (Settings ›
Collection interval on the source's page, which outranks the policy); nothing
ships set. Conversely 41,740 daily/weekly/monthly sources are fetched 4x–120x
more often than they were authored for.

## 3. Can the fleet actually keep that cadence? No, not at the default pool

A uniform random sample of 400 scheduled sources run through the binary
(12 at a time, 120 s cut-off): mean run **6.2 s**, median 1.6 s, 90th
percentile 9.4 s.

| workers | runs/s | one lap of 83,511 sources |
| ---: | ---: | --- |
| 8 (default) | ~1.3 | **~18 h** |
| 16 | ~2.6 | ~9 h |
| 24 | ~3.9 | ~6 h |
| 32 (the maximum) | ~5.2 | ~4.5 h |

A live boot at 16 workers agreed: 372 runs in 200 s (1.9/s, while the sample
sweep was competing for the same egress), queue full at 2,048 within the first
minute, backlog growing by ~90 rows a second through the 900 s boot ramp.
**The 6 h policy needs `JO_SCHED_WORKERS=24` or more**; at the default 8 the
real cadence is about 18 h. That default is unchanged here — database write
contention at 24 workers is unmeasured, so it is the owner's call.

## 4. What the sources did when run

| verdict | one per collector (579) | uniform random (400) | meaning |
| --- | ---: | ---: | --- |
| OK | 463 | 368 (92.0%) | fetched and stored records |
| FETCH_FAILED | 46 | 24 (6.0%) | refused; one disclosure record stored |
| SLOW | 44 | 5 (1.3%) | still running at the cut-off (unmeasured, not failed) |
| COLLISION | 9 | 1 | a handful of records share a uid |
| EMITS_NOTHING | 8 | 2 | answered with no records |
| GATED | 8 | 0 | needs an API key and says so |
| WINDOW_EMPTY | 1 | 0 | first date window is empty by design |

The 400 uniform runs stored 507,322 records. Rows: `docs/source-scheduling-sample-2026-11-14.tsv`.

Reading the failures: of the 24 in the uniform sample, 12 are batch 63's
Chinese government sites (HTTP 503/504/403 or no answer — this container's
egress is a likely cause, so unmeasured rather than dead), 4 are TLS
handshakes refused before any certificate was judged (same caveat), and the
rest are single 400/401/403/404/500 readings. One probe is one reading; none
is removed on it. Most SLOW rows are detail-hop walks (`dportal-*`,
`il-knesset-*`) that spend their 120 s detail budget by design.

## 5. Defects found in the scheduler, and fixed here

Each was read out of the code by an auditor and then either pinned by a unit
test or observed on the live boot.

1. **Maintenance pods were stretched to 6 h.** The collection policy applied
   to every row with an interval, so the WAL checkpoint (300 s), embedding
   backfill (120 s), anomaly triage (60 s), collector repair and log retention
   each ran once at boot and then every six hours. Pods are now exempt.
   Live: anomaly-triage ran 4 times in 200 s.
2. **A full queue cost a source its whole cycle — and the same sources every
   cycle.** The boot ramp makes ~92 sources a second due against a queue of
   2,048; everything that did not fit was skipped for six hours, came due in
   the same cluster next time, and lost again. A source that does not fit now
   stays due, the walk resumes where it ran out of room (round robin), and the
   heartbeat line reports `deferred=N`. Live: 14,299 deferred after 3 minutes,
   none dropped.
3. **Every outcome landed one cycle late.** The next run was fixed at dispatch
   from the state before the run. A source released from quarantine by a
   successful probe still waited 7 more days; a recovered source waited out
   its last backoff (up to 24 h); a first failure was retried at 1x, not 2x.
   The finished run now re-plans the row (`sched_publish`).
4. **A faster interval took effect one old interval late.** After a policy or
   override change from 6 h to 1 h, a healthy row ran once at 1 h and then
   waited 6 h, because its stored interval was read as a backoff floor.
5. **Editing one source's interval released every backoff and quarantine
   hold** in the fleet (the re-plan ignored health holds).
6. **A restart re-fetched the whole fleet** inside the boot ramp regardless of
   what had run minutes before. A source whose last run is less than one
   interval old now keeps its own time. Live: 348 of 348 kept on restart.
7. **Equal-priority queue entries ran newest first**, and the "most overdue
   first" term never grew. Now first-in-first-out, with a real overdue ratio.
8. **Manual runs never reached the dispatcher**: a manual success on a
   quarantined source left it on the weekly probe cadence in memory.
9. **`JO_SCHED_*=abc` read as 0**: quarantine after one failure, one worker,
   or the watchdog switched off. Unparsable values now mean the default.
10. **The pages disagreed with the scheduler.** "Next due" on the source page
    was re-derived from stale data (a 60 s source showed overdue for hours
    under the 6 h policy; a quarantined one showed overdue for six days);
    `/api/status` ignored the freshness measurement; the runs-per-day estimate
    ignored overrides. All now call the scheduler's own functions and carry a
    `runInterval` field — the interval that actually runs.
11. **`max_interval_sec` of 1–9 was accepted**, which would run all 83,511
    sources continuously; hand-edited or environment policies were not
    validated at load; `interval_sec: 0.5` silently became 0.

Timing code outside the scheduler:

12. **Host pacing**: a 429 penalty set while a request was already waiting was
    ignored; the first request to a slow-paced host waited for machine uptime
    to exceed the gap; the source page reported a 0 ms gap before the first
    fetch; override host names were matched case- and space-sensitively.
13. **Date tokens**: `{date:%Y-%m-%dT00:00:00}` lost its `:00` (read as a day
    offset; no shipped row uses the shape); an unterminated token swallowed the
    next token's brace; a huge shift overflowed; `{ago:N}` could render a
    pre-1970 date. `lib/datetok.c` had no unit test; it has one now.
14. **Date-window rows with a `window_from` lagged one run behind today** once
    caught up (the stored position was this run's start, not today).

## 6. Left as found — decisions, not defects

* **The 6 h global default over real-time feeds** (section 2) and **the worker
  default of 8** (section 3). Both are one setting each.
* Scheduling uses the wall clock; a clock step moves every due time. Moving to
  a monotonic clock touches persisted timestamps and was not attempted.
* No sleep on the fetch path checks the cancel flag, so a cancelled run can
  stay parked up to 300 s in the host gate.
* About 25 private copies of `iso_now` and 9 of "monotonic ms" exist across
  `core/`; consolidating them is mechanical and was left out of this change.
