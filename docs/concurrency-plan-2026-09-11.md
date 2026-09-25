# Can the server handle concurrent work? — measured, and what to fix

*2026-09-11. Answer first: **collectors, yes — the dashboard routes, no.** A
single `/api/status` call froze every other connection on the server for
**24.8 seconds** in a controlled test, while five concurrent collector-running
requests left `/api/health` at **1.7 ms**. The difference is not load, it is
which side of the event loop the handler runs on.*

Measured against the real binary with real data: a 129 MB snapshot of
`japanmap.db`, 16,367 sources registered, the background scheduler running
collectors throughout (26 collector runs completed during the test). Isolated
on :4055 so nothing touched the production DB. Harness:
`native/tools/bench_concurrency.sh`, re-runnable.

The method is the point: every scenario runs a 10 Hz probe against
`/api/health` — pre-auth, constant work, 0.8 ms when the loop is free — and
reports the probe's WORST latency. A probe max that tracks the load's duration
means the loop was blocked. A flat probe means the handler really is off it.

---

## 1. What was measured

| scenario | `/api/health` p50 | `/api/health` MAX | the load itself |
|---|--:|--:|---|
| idle baseline | 0.8 ms | 2 ms | — |
| 10 × `/api/intel/items?limit=200` | 0.8 ms | **38 ms** | 518 KB each, 50–82 ms |
| 3 × `/api/status` | 0.8 ms | **24.8 s** | 20.5 MB each, 24.9 s each |
| 3 × `/api/intel/sources` | 0.8 ms | **7.8 s** | 10.1 MB each, 11.7 s each |
| 5 × `/api/data/<layer>` (collectors) | 0.8 ms | **1.7 ms** | castles 38 s, museums 39 s, aed-map >300 s |
| mixed (6 intel + 2 status + 3 layers + semantic + entities) | 0.7 ms | **7.4 s** | status 14.9 s each, layers 14.9 s |

Read that table twice. The row that runs **collectors** — the heaviest work in
the system, minutes of outbound HTTP and tens of thousands of records — is the
row where the server stayed perfectly responsive. The row that just builds a
JSON summary is the one that stopped everything.

**`/api/status` is the single worst thing in the system today.** 20.5 MB, ~8 s
of work each, serialized: three concurrent calls took 24.9 s each and blocked
every other request for 24.8 s. Both the web client and iOS call it on
startup — so two users opening the app at the same time is already this
scenario, and a third user's `/api/health` waits behind them.

---

## 2. Why: one event loop, and only four routes have left it

`core/httpd.c:3387` is a single-threaded `mg_mgr_poll()` loop. A handler that
does its work inline holds that thread, and every other connection — including
SSE, which only advances on `MG_EV_POLL` — waits.

Four routes have been moved off it, each with its own DB connection and its
reply delivered through `wakeup_reply_big()`'s parking path:
`/api/search/suggest`, `/api/intel/sources/<id>/run`, `/api/isochrone`,
`/api/data/<layer>`. That last one was the CRITICAL fixed in the 2026-09-04
audit, and scenario 4 above is the proof it worked: five cold collector runs,
health untouched at 1.7 ms.

**Everything else in the file is inline.** The ones that matter, ranked by
measured or code-proven cost:

1. **`/api/status`** (`httpd.c:1088` → `statusapi.c:521`) — unbounded
   `GROUP BY source_id` over `intel_items` with no LIMIT (`statusapi.c:463`)
   plus the whole `sources` table serialized per row. **Measured: 20.5 MB,
   24.8 s of loop block.**
2. **`/api/intel/sources`** (`httpd.c:1097` → `intelapi.c:980`) — same
   unbounded aggregate. **Measured: 10.1 MB, 7.8 s of loop block.**
3. **`/api/geocode`** (`httpd.c:1876` → `geoproxyapi.c:176`) — up to **three
   sequential outbound calls at 8 s each = ~24 s inline** on a cache miss, and
   `/api/geocode/reverse` two at 8 s = ~16 s. Not in any previous audit, not
   converted, and reachable by any authenticated client. This is the largest
   *undiscovered* stall in the file.
4. **`POST /api/status/<id>/probe`** (`statusapi.c:729`) — one live outbound
   request at a 10 s timeout, inline. One operator click freezes the server for
   up to 10 s.
5. **`/api/export/<kind>` and `POST /api/cases/<id>/report`** — chunked, but
   `mg_http_write_chunk` only appends to `c->send`, which drains on the same
   loop (`httpd.c:138-145` says so). The row cap is up to 2,000,000 rows.
6. **`/api/data/cameras/proxy`** (`cameraproxy.c:253`) — fetches an arbitrary
   internet camera image inline, 5 s timeout, on every cache miss.

## 3. Below the loop, the picture is much better than it looks

The background half of the system is properly built, and the audit confirms it
line by line:

- **WAL + per-connection handles.** `journal_mode=WAL`, `busy_timeout=5000`
  (30,000 on scheduler and dispatch workers), 64 MB cache and 256 MB mmap **per
  connection** (`core/db.c:255`). Every off-loop writer owns its connection via
  `db_worker_open()`/`db_attach()` — all twelve call sites audited, none shares
  the loop's handle while writing. Readers never block on a writer.
- **Writes are per-record, not per-run.** `core/intel.c:347-384` wraps each
  `emit()` in its own BEGIN/COMMIT, so a 50,000-record collector holds the
  write lock 50,000 times briefly rather than once for minutes. That is why the
  scheduler and the API can write at the same time at all.
- **The scheduler is bounded and non-overlapping**: 8 workers by default
  (`JO_SCHED_WORKERS`, clamped 1–32), and a source already running cannot be
  queued again (`scheduler.c:502`).
- **Outbound politeness is global**: `core/hostgate.c` is one process-wide
  table — 2 concurrent + 150 ms gap per host by default, with named overrides —
  and every `http_request` goes through it.
- **LLM calls are queued, not piled on**: one worker thread per base URL with a
  two-lane priority queue (`llm_worker.c:38`), so interactive suggests jump
  ahead of background work and llama-server is never hit by N callers at once.

So the concurrency model is sound where it was designed deliberately. The gap
is that the HTTP layer was never finished to match it.

---

## 4. What is genuinely unbounded

Three ceilings do not exist, and each is a real failure mode rather than a
theoretical one:

- **No cap on detached handler threads.** `datarun_thread` and `iso_thread`
  spawn per request with nothing stopping them; 50 concurrent cold
  `/api/data/<layer>` requests means 50 threads and 50 new SQLite connections.
  (`/api/intel/sources/<id>/run` is the exception: `run_begin()`'s 16-slot
  table, `httpd.c:292`.)
- **The parking table is 16 slots and EVICTS.** `wakeup_reply_big()` parks a
  large reply by ticket; slot 17 evicts the coldest entry, and that request
  then fails `reply_lost` 500 (`httpd.c:558`). Correct under pressure — it
  never misdelivers — but it means >16 concurrent big replies drop the oldest.
- **No cap on concurrent OSINT searches' fan-out.** Each search spawns its own
  pool of up to 16 dispatch threads (`pipeline.c:395`); two searches make 32.
  `/api/search/analyze` caps concurrent *runs* at 4 (`searchapi.c:45`), which
  bounds it in practice — but nothing bounds the aggregate directly.

---

## 5. The plan, in the order the measurements justify

**P0 — `/api/status` and `/api/intel/sources`: stop shipping 30 MB on the loop.**
Three changes, in this order, each independently useful:

1. **A summary projection by default.** Both routes exist to answer "is the
   fleet healthy" and both answer it by serializing every source. Return
   aggregates plus a bounded page (`?limit=`/cursor, default 200), and state
   the bound in-band — `shown` / `total` / `truncated`, the disclosure pattern
   `intelapi` already uses. This is an API contract change touching both
   clients, so keep the full projection available behind `?full=1` until they
   migrate.
2. **Cache the aggregate.** The expensive half is the `GROUP BY source_id` over
   `intel_items`, and its answer changes at scheduler cadence, not per request.
   A 30 s memo (or a materialised counter table the sink updates) removes the
   cost entirely for the common case.
3. **Move both to a worker** with the existing parked-reply path. Even at 200
   KB the aggregate can be slow on a cold cache, and a route on both clients'
   startup path must not be able to hold the loop.

Verification is already written: re-run `bench_concurrency.sh` and require
`/api/health` MAX under 50 ms in the `status` scenario.

**P1 — the inline outbound calls.** `/api/geocode`, `/api/geocode/reverse`,
`/api/status/<id>/probe`, `/api/data/cameras/proxy`. Every one is "make an HTTP
request to somebody else while holding the server's only thread". They all take
the same fix — the four-route worker pattern that already exists in this file —
and geocode is first because it is up to 24 s and needs no privileges.

**P2 — bound the fan-out.** A single semaphore for detached handler threads
(a count, a max, a `503 busy` with `Retry-After` when exceeded) applied to
`datarun_thread`, `iso_thread` and any route P1 moves off the loop. Pick the
cap from the parking table's own limit so the two agree: 16 in flight, 17th
gets a clean refusal instead of evicting someone else's reply.

**P3 — make exports socket-paced.** `/api/export` and the case report build
their body into `c->send` on the loop. The file already names the fix
(`mg_wakeup()` thread pattern). Until then, the practical mitigation is the
plan row cap, which is 2,000,000 rows for enterprise — worth lowering the
default while this is outstanding.

**P4 — a standing regression gate.** The harness is deterministic enough to be
one: run the `status`/`data` scenarios against a seeded DB and fail if
`/api/health` MAX exceeds a threshold. The 2026-09-04 CRITICAL (a cold
`/api/data/castles` freezing `/api/health` for 20 s) would have been caught by
this on the commit that introduced it. There is also **no TSan job in CI** —
`make tsan-sched` found the `strtok` races and does not run automatically.

**P5 — the one residual thread-safety item**: `core/embed_pod.c:74` still uses
bare `strtok()` on `JO_EMBED_RECORD_TYPES`. Env parsing, not a hot path, but it
is the last one the sweep missed.

---

## 5b. Implemented 2026-09-12, and measured again

All six items are done. The same harness, the same 129 MB database, the same
scheduler running collectors throughout:

| load | health MAX before | health MAX after | the load itself |
|---|--:|--:|---|
| 10 × `/api/intel/items` | 38 ms | **1.5 ms** | unchanged |
| 3 × `/api/status` | **24.8 s** | **3.8 ms** | 24.9 s → **1.4 s** each |
| 3 × `/api/intel/sources` | **7.8 s** | **8.3 ms** | 11.7 s → **2.0 s** each |
| 5 × `/api/data/<layer>` | 1.7 ms | 1.8 ms | unchanged (already off-loop) |
| mixed, everything at once | **7.4 s** | **1.3 ms** | — |

The requests got faster as well as stopping the blocking, and the reason is the
finding below: most of `/api/status`'s 24.9 s was never the query.

**The thing the code review would not have found.** Moving the build to a
worker was not enough — the gate still measured **21.3 s** of blocked
`/api/health`. `mg_http_reply()` formats its body through
`mg_vxprintf(mg_pfn_iobuf, …)`, and `mg_pfn_iobuf` appends **one byte at a
time with a resize check per byte** (`third_party/mongoose.c:11095`). A 20.5 MB
body is twenty million callbacks, and they run on the event loop no matter
which thread produced the string. Delivery was the bottleneck, not the
database. `reply_json()` now switches to `mg_printf` for the headers plus
`mg_send()` for the body — one memcpy — above 8 KB, and the parked-reply path
always uses it. That single change is what took the gate from 21.3 s to
**0.014 s**.

What shipped, item by item:

* **P0.** `core/respcache.{c,h}` (TTL body cache, keyed on route + operator
  variant + window, `Age`/`X-Cache` on every hit), `statusapi_build_view()` and
  `intelapi_intel_sources_view()` (`?limit=`, `?offset=`, `?summary=1`, with
  `view`/`meta` stating total, shown, offset, limit, truncated and why), and
  `fleet_serve()` in httpd.c: cache hit answers on the loop, a miss builds on a
  worker. **The bounded projection is opt-in, not the default** — both clients
  fetch the whole list and filter it themselves, and the iOS one cannot be
  rebuilt from this checkout, so flipping the default would silently show a
  client 200 of 16,367 sources. It flips when the clients ask for less.
* **P1.** `/api/geocode` (up to 3 × 8 s), `/api/geocode/reverse`,
  `/api/plateau/tilesets`, `POST /api/status/<id>/probe` and
  `/api/data/cameras/proxy` all run on workers now (`ob_thread`). The camera
  proxy returns image bytes, so the parking table gained a binary sibling that
  carries a header block (`wakeup_reply_hdr`). The probe also gained a 414 for
  an over-long id — it was about to truncate a 1023-byte path segment into a
  256-byte field and probe whichever source shared the prefix.
* **P2.** `worker_admit()`/`worker_release()`: at most `WBODY_SLOTS` (16)
  detached handler threads, the 17th gets `503` + `Retry-After: 1` instead of
  starting work that would evict someone else's finished reply from the parking
  table. Deliberately not a queue — queueing holds a connection open behind
  work that has not started.
* **P3.** `/api/export/<kind>` and `POST /api/cases/<id>/report` build on a
  worker into a buffer and deliver through the header-carrying parking path.
  Memory is unchanged (the chunks only drained on the loop, so the body was
  always buffered), and the property that had to survive the move — a
  disconnected client aborting a 250,000-row walk — is preserved by an explicit
  cancel channel the loop sets on `MG_EV_CLOSE`, since a worker must never read
  `c->is_closing`.
* **P4.** `native/tools/ci_concurrency_gate.sh` runs on every push: three
  concurrent `/api/status` while probing `/api/health`, fails over 1 s (it
  measures 0.014 s). No network — the scheduler is staggered out and the DB is
  a fresh schema. A second CI job runs the unit suite under ThreadSanitizer,
  the race class this repo has already been bitten by twice.
* **P5.** `core/embed_pod.c`'s `strtok()` → `strtok_r()`; the last bare one.

**What wiring TSan into CI immediately found** — not a race, but exactly the
kind of thing a job like this exists to surface. `tests/unit/test_service_vec.c`
started its stub embedding server on a FIXED port (18099). When a run aborted,
its python child outlived the process; the next run's stub then failed to bind
while the readiness poll happily succeeded against the ORPHAN — a server the
test could not stop, left at whatever dimension the crashed run had set. That
produced `indexed 1838 of 1846` and a "failed build" case that passed because
the stub it thought it had killed was still answering. Both look exactly like a
product regression and neither was one. The stub now refuses a port it does not
own and walks to a free one; verified by re-running the suite with a deliberate
squatter on 18099 — six of six cases still pass.

Measured for the record: `/api/status` full payload is now **1.6 s cold** and
**0.035 s cached** for the same 20.5 MB, with `X-Cache: hit; age=…` on the hit,
and three concurrent cached hits leave `/api/health` at 46 ms.

**And a second thing the TSan job found, which is worth more than the job
itself.** Under TSan the service-index test reported 1,846 entity pivots where
the source tree has 1,838, and the binary logged `[registry] DUPLICATE id` for
thirteen collectors — phantoms, since `grep` finds exactly one definition of
each and `make`'s own build is clean. Cause: `tests/unit/run.sh` linked
`find $OBJDIR -name '*.o'` — **every** object in the tree, including ones whose
`.c` moved during the collectors reorganisation and which the Makefile itself
never links. A moved collector's orphan object registers its ids a second time.
Measured on that tree: **150 orphan objects**, all under `collectors/sources/`
for files now in `collectors/pod/` and `collectors/feed/`. 1,812 objects − 150
= 1,662, exactly the current `.c` count.

`run.sh` now keeps an object only when its source still exists, and names the
ones it drops. This is the trap CLAUDE.md's build notes describe as producing
"phantom DUPLICATE ids and a unit-test segfault" — it is fixed at the root now
rather than worked around by purging a directory, and `make unit` on the repo's
own `obj/` tree was subject to it too. With it fixed, **all nine unit tests pass
under ThreadSanitizer with zero data races.**

## 6. What this means for the question as asked

*Collectors and intel at the same time*: yes, and it is measured — 26 collector
runs completed while the API served 1,100 health probes and a dozen concurrent
data requests, with no interference. WAL, per-connection handles, per-record
transactions and the host gate do their job.

*Several users hitting the dashboard at the same time*: **was** the problem —
two concurrent `/api/status` calls made the server unresponsive for half a
minute, and both clients call it on startup. As of 2026-09-12 that load leaves
`/api/health` at 3.8 ms and each dashboard answers in 1.4 s (§5b). A CI gate
now fails the build if any route goes back on the loop.

Still true, and worth keeping in view: `/api/data/aed-map` takes over 300 s on
a cold cache. It no longer blocks anyone else, but it is a slow source, and a
client waiting five minutes for a map layer is its own problem.
