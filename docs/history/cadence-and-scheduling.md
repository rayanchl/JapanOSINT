<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): Cadence policy, per-source interval override, sched_next_due. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

## Cadence policy (2026-10-20)

Every source still declares its own `update_interval_sec` — that is the
measured value it was authored with — but ONE policy sits above all of them:
`core/cadence.{c,h}`. It is the operator's, system-wide, and lives in the
database (`app_settings`, keys `sched.global_interval_sec`,
`sched.min_interval_sec`, `sched.max_interval_sec`), so it survives restarts
and is edited from the Sources page (`GET`/`PUT /api/sources/cadence`, PUT
gated like `/api/sources/:id/schedule`: platform operator). Three shapes:

* **global** — every scheduled row runs at one interval;
* **clamped** — a floor and/or a ceiling the declared values are held inside;
* **declared** — zeros saved, each row as authored.

**The compiled default is global 6 h** (`CAD_DEFAULT_GLOBAL_SEC`, the owner's
decision of 2026-10-22: "this interval system should be osint software wide").
An empty database with a silent environment runs every scheduled row every
21600 s and the pages say `source: default`; "declared" is a choice an operator
makes by saving zeros from the Sources page, not the silence of an unset knob.
A source that must be fresher is set from its own page (Settings › Collection
interval), which outranks the policy. Measured when it shipped (2026-10-22, the registry at 57,529 with 55,174
scheduled rows): 594,367 runs/day as declared against 220,696 under this
default — the figure `GET /api/sources/cadence` reports on an empty database. `JO_SCHED_INTERVAL_SEC` / `JO_SCHED_MIN_INTERVAL_SEC` /
`JO_SCHED_MAX_INTERVAL_SEC` bootstrap an EMPTY database only; a stored policy
always wins. The scheduler reads `cadence_apply(declared)` at every site that
used the declared value (due time, boot ramp, backoff base, cancellation
window, search-only and quarantine holds), and re-plans on a change without a
restart: a row whose next due time is further away than its new interval is
pulled in to now + interval. Entity pivots (interval 0) are never touched.

**It composes with the adaptive interval in ONE order** (the two met in a
merge on 2026-10-20, each written without knowing the other existed):
`sched_row_interval()` in `core/scheduler.c` is
`cadence_apply(sched_state_base(...))` — the row's own measurement inside,
the operator's policy outside. A feed whose items say "hourly" runs hourly
under no policy, six-hourly under an operator floor of six hours, and at the
global cadence when one is set; a person's floor outranks a measurement, a
global outranks both, and the measurement is still taken and stored so the
page can say "measured hourly, held at 6 h by policy". Every site the
scheduler reads a row's interval goes through that one function; the source
pages show `declaredInterval`, `measuredInterval`, `policyInterval` and
`effectiveInterval` side by side; `test_sched_backoff.c` pins the order.

**And ONE source can be set by hand, from its own page** (2026-10-21,
`core/source_interval.{c,h}`; `GET`/`PUT`/`DELETE /api/sources/:id/interval`,
PUT and DELETE platform-operator like the policy). An enabled per-source
interval outranks both the measurement and the system-wide policy — specific
beats general: a floor of six hours is a statement about the fleet, "this one
source every hour" is a statement about that source made by the same operator,
later and more precisely, and it would be useless if the floor silently held
it. So `sched_row_interval()` is now: the override when one is enabled, else
`cadence_apply(sched_state_base(...))`. It cannot create a schedule (a pivot is
refused, as the measurement cannot create one either), it is stored in
`source_interval_override` and cached because the scheduler asks for every row
every tick, disabling keeps the number, clearing deletes the row, and a change
re-plans the running scheduler exactly as a policy change does. The source
page's Frequency card shows declared, measured, policy and "set for this
source" side by side and names which one decided (`schedule.cadenceBasis`);
`/api/status` rows carry `overrideInterval`. **It is edited from the page's
Settings section as the key `interval_sec`** (2026-10-22): `core/source_settings.c`
lists it beside the body ceiling, but as a FACADE — reads, writes and the JSON
for that key delegate to `source_interval.{c,h}`, so there is one row, one
cache and one generation, not a copy in `source_settings`. Its "default" in
the JSON is per source (what runs without an override), the key is absent for
an entity pivot, and the same `PUT /api/sources/:id/settings` the body ceiling
uses writes it (audited `source.setting.put`). `tests/unit/test_source_interval.c`
pins the refusals, the precedence under a global policy, disable, persistence
and clear.

**WHEN a row is next due is ONE function too: `sched_next_due()`**
(2026-11-14, audit and report in `docs/source-scheduling-audit-2026-11-14.md`).
The dispatcher, the boot plan and the source pages all read it, and
`./bin/japanosint --schedule` prints the result for every source as TSV — the
interval that runs, what decided it, health and next due. What the audit found
by reading the dispatcher rather than the interval arithmetic, each fixed and
pinned in `test_sched_backoff.c` / `test_sched_pool.c`:

* **A maintenance pod is exempt from the cadence policy.** The policy is about
  collection; under the compiled global 6 h the WAL checkpoint, the embedding
  backfill and anomaly triage ran once at boot and then every six hours.
* **The plan is corrected from a run's OUTCOME** (`sched_publish`). The next
  run used to be fixed at dispatch from the state before the run, so every
  change landed a cycle late: a released quarantine waited 7 more days, a
  recovered source its last backoff.
* **A row the queue has no room for STAYS DUE.** It used to lose the cycle —
  and with 83,511 scheduled rows against a 2,048 queue, the same rows lost
  every cycle. The heartbeat's `deferred=N` is the standing backlog.
* **A restart does not re-fetch the fleet**: a source whose last run is less
  than one interval old keeps its own time.
* **A health hold survives a cadence edit**, and a healthy row's stored
  interval is not a floor (6 h → 1 h used to run once and then wait 6 h).

Two numbers it measured that are decisions rather than defects: at a mean run
of 6.2 s (uniform sample of 400) the default 8 workers lap the fleet in about
18 h, so **the 6 h policy needs `JO_SCHED_WORKERS=24`** (27 on the 90,907
scheduled rows main held by the time this merged); and the policy holds
the 22 real-time feeds (`wolfx-eew` 10 s, `jma-tsunami` 60 s) at six hours
until each is given its own interval.

**Real-time rows are exempt from the GLOBAL interval** (2026-11-16, audit
ENG-17): the policy field `sched.realtime_max_sec` (default 300, `0` disables,
`JO_SCHED_REALTIME_MAX_SEC` bootstraps an empty database, edited through the
same `PUT /api/sources/cadence` as `realtime_max_sec`) exempts a scheduled row
whose DECLARED interval is at or below it from `global_interval_sec`, so
`wolfx-eew` (10 s) and `jma-tsunami` (60 s) no longer run every six hours. It is
decided on the declared value, never on an adaptive measurement; the min/max
clamps still apply and a per-source override still wins. The pages show
`policy: "realtime"`, `--schedule` prints basis `realtime-exempt`, and the
runs/day estimate counts the rows (`realtime_exempt_rows`). A policy saved
before the key existed reads as the default, not as off.

What a choice costs is in the response, measured over the registry, not
guessed: `estimate.runs_per_day_declared` against
`estimate.runs_per_day_effective`. Measured 2026-10-20 on 19,954 scheduled
rows: 240,318 runs/day as declared; a global 1 h would be 479,000, 6 h 80,000,
daily 20,000. The 25 rows under 5 min (webcams, grid telemetry, Tor exits)
alone make 42,000 of today's runs, which is what a global cadence trades away.
Every source's pages show `declaredInterval`, `policyInterval` and `policy`
(`/api/sources/:id/detail` → `sched`, `/api/status` rows → `policyInterval`).
`tests/unit/test_cadence.c` pins the rules, the persistence and the refusals.


