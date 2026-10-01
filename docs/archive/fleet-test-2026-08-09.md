# Fleet test — every scheduled source run against its real upstream

_2026-08-09 · branch `feat/osint-batch12-c-port` · `native/tools/fleet_probe.sh`_

This is the first end-to-end measurement of what the collector fleet actually
does against the live internet. Not a code sweep, not a row-count grep: every
scheduled source was executed once through the real `scheduler_run_source`
path, against the real endpoint, and the scheduler's own per-run summary line
was recorded.

Reproduce with:

```sh
BIN=<path to japanosint> bash native/tools/fleet_probe.sh
python3 native/tools/analyze_fleet.py ~/jotest/fleet/results.tsv
```

Raw results are kept at `docs/fleet-results-2026-08-09.tsv` (one row per
source: id, verdict, rc, records, wall ms).

---

## Headline

| | |
|---|---|
| Sources registered in the binary | **2,563** |
| — scheduled (`update_interval_sec > 0`), i.e. probed | **2,087** |
| — entity-pivot (`interval = 0`, need an entity argument) | 476 |
| **Healthy** (`OK` + honest empty) | **1,735 — 83.1%** |
| Rows actually collected in one pass | **583,666** |
| Sources emitting ≥ 1,000 rows | 106 |

| Verdict | Count | Share | Meaning |
|---|---|---|---|
| `OK` | 1,542 | 73.9% | fetched and emitted ≥ 1 row |
| `EMPTY_OK` | 193 | 9.2% | fetched fine, upstream had nothing right now |
| `FAIL` | 240 | 11.5% | `rc < 0`, no rows |
| `TIMEOUT` | 112 | 5.4% | exceeded 45 s |

**2,563 is the true source count.** It matches the audit's re-derived ≈2,562
and settles the six contradictory numbers enshrined in tracked files (313, 286,
318, 476, ~551, "150+"). The authority is the built binary
(`--list-sources`), because some sources compute their ids rather than writing
them as literals; a `grep REGISTER_SOURCE` count is structurally wrong, since
`RSSX()` expands to a `source_def` *plus* its own `REGISTER_SOURCE`.

---

## The Reddit cohort: the harness measured its own pacing, not the collectors

The raw numbers look alarming and are misleading. They are worth walking
through, because the trap caught this run and will catch the next one.

| Cohort | OK | FAIL | Raw reading |
|---|---|---|---|
| `gnews-*` (615 probed) | **608** | 0 | healthy |
| `reddit-*` (143 probed) | 24 | **118** | "dead" |

The first, wrong conclusion was that Reddit had closed anonymous RSS to
datacenter ranges. Rapid back-to-back checks appeared to confirm it: HTTP 403
on a default curl UA, 429 on a browser UA, 429 on `old.reddit.com`, on bare
`.rss`, on `/new/.rss`, and on a third-party bridge.

**That was an artefact of the checks themselves.** Re-measured properly — one
request, after 60–90 s idle:

```
https://www.reddit.com/r/worldnews/.rss
    UA "JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; …)"  → HTTP 200, 44,789 B
    UA "Mozilla/5.0 … Chrome/120 …"                              → HTTP 200, 44,789 B
    UA "feedbot/1.0 (+https://example.org/bot)"                  → HTTP 429
```

Reddit serves **the engine's own production User-Agent** without complaint. The
403/429 storm was self-inflicted: 143 Reddit sources inside a ~35 min probe is
one Reddit request every ~15 s, and `reddit_world_geo.c:3-26` records the
measured floor as roughly **one request per 30 s per IP**. The probe ran at
twice the tolerated rate and got exactly the documented result.

So:

1. **The in-tree post-mortem is correct and remains correct.** 7200 s (~51 s
   spacing) is above the floor. These 142 sources are healthy under their real
   schedule; nothing about them needs gating, slowing or retiring. An earlier
   draft of this document recommended key-gating them — that recommendation was
   wrong and is withdrawn.
2. **The 429 is invisible to the engine, and that is the real defect.**
   `rss_collect` maps any non-2xx to `-1`, so a throttle is indistinguishable
   from a broken collector and `anomaly_detect` quarantines a source that is
   working. The cure is a distinct "throttled" outcome plus a shared per-host
   rate limiter — `core/hostgate.h` exists but `rss_collect` never consults it.
   Both live in shared code and are the correct next piece of work.
3. **The Google News backoff is still worth keeping**, but not because Google
   News is failing — it returns HTTP 200 throughout. 681 req/h from one IP to
   one host is simply aggressive, and the risk is host-correlated across ~24%
   of the fleet. It is insurance, not a fix for a fault in evidence. Recorded
   so the next reader does not "verify" the backoff by observing that Google
   News works; it already did.

The general lesson is the one this harness exists to enforce: **a fleet-wide
probe perturbs the thing it measures.** Any cohort sharing a host is measured
together, and a red column may say more about the harness's concurrency than
about the collectors. Confirm per-host failures with a single idle request
before concluding anything.

---

## Where the remaining failures are

Excluding Reddit, 234 failures and timeouts are spread thinly across the whole
fleet — the largest non-Reddit family is 7. That is the signature of a healthy
fleet with ordinary upstream churn, not of systemic breakage.

| Family | Failures |
|---|---|
| `reddit-*` | 118 |
| `unified-*` | 7 |
| `osm-*`, `faa-*`, `cert-*` | 5 each |
| `us-*`, `mlit-*`, `cisa-*` | 4 each |
| everything else | ≤ 3 each |

Failures by collector, worst first: `osint` 166/1076, `government` 33/127,
`transport` 32/150, `cyber` 17/201, `infrastructure` 17/54.

Only **5 sources** failed in isolation on a host that was otherwise fine —
these are the likeliest genuine per-source defects rather than upstream
collateral: `amateur-radio-repeaters`, `fire-department`,
`japan-api-prefectures`, `mercari-trending`, `tochi-info`.

The 112 timeouts are mostly large payloads rather than dead hosts; several of
the fleet's biggest producers sit just inside the 45 s budget. A production
schedule does not run 10 sources concurrently on one machine, so the real-world
timeout rate will be lower.

---

## The quarantine-on-honest-empty bug, observed live

The probe reproduced the audit's §4.3 finding in the wild on the first attempt:

```
[yahoo-news-jp-rss] emitted 0
[sched] yahoo-news-jp-rss run rc=-1 records=0 4075ms
[detect] anomaly opened: yahoo-news-jp-rss verdict=status_bad — run errored (status=error)
```

The fetch succeeded. The upstream simply had nothing new. The collector
returned `-1`, the scheduler classified it as an error, and `anomaly_detect`
opened a quarantine record — for a source that worked correctly.

**193 sources returned `EMPTY_OK`** in this run. Every one of them is a source
that would have been quarantined under the old classification if it used the
`return n > 0 ? 0 : -1` shape. That is the scale of the bug: it is not 44 files,
it is 44 files × every quiet hour.

---

## Top row producers

The fleet's volume is concentrated: 106 sources emit ≥1,000 rows.

| Rows | Source |
|---|---|
| 29,308 | `mwgg-airports` |
| 20,000 | `faa-obstacles-dof` |
| 19,608 | `stats-pt-ine-indicator` |
| 18,660 | `digitraffic-port-locations` |
| 12,000 | `ofcom-wtr` |
| 12,000 | `brandmeister-devices` |
| 11,110 | `fcc-tv-query` |
| 11,008 | `ourairports-navaids` |
| 10,798 | `radioid-dmr-repeaters` |
| 10,298 | `mbta-gtfs-static` |

Two of these (`faa-obstacles-dof`, `ofcom-wtr`) land on suspiciously round
numbers, which usually means an upstream page cap rather than a true total —
worth confirming before anyone quotes them as coverage figures.

---

## Method and its limits

Each source ran once, with a 45 s timeout, 10 concurrent workers, each worker
holding its own SQLite file. The id list was **shuffled** deliberately: 615
sources point at `news.google.com` and 143 at `reddit.com`, so an alphabetical
order would have fired each cohort as a single burst and measured the
upstream's rate limiter instead of the collector.

What this measures: *does this source, run once right now, fetch and parse real
data.* What it does **not** measure: steady-state behaviour under the real
schedule. A source that passes here can still be throttled in production if its
cohort shares a host — which is exactly the Google News risk above. Those are
different questions and this harness answers only the first.

Key-gated sources with no credential present correctly return 0 rows and appear
as `EMPTY_OK`; they are not failures and are not counted as coverage.
