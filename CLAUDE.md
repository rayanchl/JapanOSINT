# JapanOSINT — house rules

Read these before writing collector, pipeline or API code. Both rules are about
the same thing: what we display is exactly what we actually obtained.

## 1. Never fabricate data

Real fetch or honest empty. No fixture rows, no placeholder records, no
hardcoded registry/source names emitted as if they were findings, no seeded
values standing in for a failed fetch. A failure degrades to an explicit
`error` / `not_found` / "needs credential" note, never to invented content.

Full audit of how each collector behaves today:
`native/collectors/SOURCE_REALITY_REPORT.md`.

**Every registered source is now proof-of-life verified.** Batch 14's 1,001
unverified `csrc14_*` candidates were probed and promoted (594 PASS →
`vsrc14_*`); batch 15 added 1,029 more (`vsrc15_*`, see
`docs/verified-sources-batch15.md`). Rejects are kept as data in
`docs/rejected-sources-batch{14,15}.tsv`. No `csrc14_*` file remains.

Three verifier/engine traps that pass exposed — check for them before trusting
any "verified" number:

* **Non-ASCII URLs.** `urllib` puts the URL in the request line, which must be
  ASCII, so a CKAN `?q=防災` died inside `fetch()` and was logged as a dead
  endpoint. 196 live sources were being discarded by that alone.
* **Empty result sets.** `{"total_count":0,"results":[]}` used to count as
  `json-object`/1 item and PASS — a source verified live that emits nothing on
  every run, forever. Now `EMPTY_RESULTSET` (172 rows across the two batches).
* **Two-level envelopes.** `lib/jsonlist.c` descended one level looking for a
  label; OpenDataSoft puts the title at `metas.default.title`, so every ODS
  catalogue record was dropped as unlabelled despite carrying a real title and
  licence. The envelope list now takes dotted paths.

## 2. Never discard data — a source that is called must be used exhaustively

**If we spend a request on a source, we use everything it gave back.** Every
record, every field, every page, and the detail endpoint behind each list hit.
Nothing is dropped at a seam between collector, sink, API and client.

A consumer that physically cannot take everything (an LLM prompt, a mobile list)
may bound its own *view* — but the full data must still be fetched, stored and
served, and the bounded view must state in-band how much it is showing out of
how much exists. Silent slicing is the violation.

If anything was left unused, it is reported as data (a
`collector-truncation-notice` record), not as a log line nobody reads.

Rule, examples of violations, and what the shared machinery guarantees:
`docs/SOURCE_EXHAUSTIVENESS.md`.

Where the tree actually stands, as `make audit-sources` reports it:

* **strict set (`collectors/pivot/table/hp*_*.c`) — 0 findings.** This is the
  part the Makefile gates on, and it is held clean. Run `make audit-sources`
  after adding a table: batch 18 introduced two `single-page` findings here (a
  paged endpoint declared without `page_param`) and they had to be fixed before
  the gate would pass again.
* **the rest of the tree — 0 findings** (2026-08-24; was ~127 across ~74 files,
  then 91 across 54). Every first-only, single-page, record-cap, loop-break,
  limit-one and dedupe-ring finding has been read and closed one of three ways:
  the discard was real and was fixed, the line carries an `exhaustive-ok`
  marker with a reason, or it was a scanner false positive and is marked as
  such. The recoveries were not small — IRS exempt-orgs went from 5,000 rows to
  278,014, ESMA from 1 to 1,377, Homebrew from 300 to 23,133, and twelve
  scheduled feeds were asking their upstream for `limit=1`.

  So "zero audit findings" is now true of the whole tree, not just the strict
  set — which makes any NEW finding a regression rather than a number in a
  backlog. Keep it that way: `make audit-sources` is cheap and takes seconds.

Deliberate exceptions carry an inline `/* exhaustive-ok: <reason> */` marker
(`grep -rn exhaustive-ok`). The marker must sit **on the flagged line itself** —
the scanner matches per line, so a marker in the comment block above the line it
explains is silently ignored and the finding stays. That is easy to get wrong,
because the explanation naturally wants to be a paragraph: put the paragraph
above and a one-line `/* exhaustive-ok: … */` on the line.

```sh
cd native
make                 # full build (-Wall -Wextra); the tree is at 0 warnings, keep it there
make selftest        # boot self-test: DB integrity, schema objects, llm probe
make unit            # tests/unit/run.sh against a scratch DB
make hptest          # offline check of the engine's guarantees
make lint-sources    # dup ids/endpoints, quarantine-empty, snprintf guards
make audit-sources   # scan every collector for discard patterns
```

Those six are exactly what `.github/workflows/ci.yml` runs, in that order.

If `make unit` dies with `tests/unit/run.sh: No such file or directory` (exit
127) on a tree that came from a Windows checkout, the script has CRLF line
endings and the kernel is reading `#!/bin/bash\r` as the interpreter. The error
names a script that is sitting right there and is executable, so it reads as
"missing file". `.gitattributes` forces LF for `*.sh`/`*.py` on checkout, but it
cannot rewrite files that were checked out BEFORE it existed — and because git
normalises CRLF away on check-in, `git status` stays clean forever while the
working tree stays broken. Repair the tree, don't re-clone:

```sh
git ls-files -z '*.sh' '*.py' | xargs -0 sed -i 's/\r$//'   # content-identical to HEAD
```

Note that `make source-count` (and the `source-floor` gate built on it) counts
`REGISTER_SOURCE` registrations only. Rows registered through
`HP_REGISTER_TABLE` are invisible to it, so the number it prints is a floor on
the registry, not its size — the built binary's own seed count is the real one.

## Where things live

```
native/source.h                    the ONE data-acquisition ABI (source_def + intel_sink)
native/lib/hpengine.{c,h}          declarative deep-record collector engine
native/collectors/sources/*.c      hand-written collectors, one file per family
native/collectors/feed/generated/  generated scheduled feed collectors (vsrc*)
native/collectors/pivot/table/*.c  hpengine tables (hp*, hp2*, hp3*) — the strict set
native/collectors/pod/*.c          enrichment/maintenance pods
native/core/                       db, http, intel sink, dispatcher, pipeline, HTTP API
native/tools/                      lint_sources.py, gen_hp_batch.py, probe_hp_batch.py
docs/                              plans, pipeline notes, and the two house rules above
```

The Makefile globs `collectors/` RECURSIVELY, so a collector registers from any
depth. Do not flatten it back.

## 3. A registered source must actually be reachable

`lib/hpengine.c` (hp_run) reaches a row in exactly two ways:

* it references an entity token (`{q}`, `{qd}`, `{qh}`, …) in its URL or POST
  body, so it is dispatchable as an entity pivot; or
* it declares `interval > 0`, so the scheduler picks it up.

A row with **neither** — a static URL and no interval — is registered, appears
in `/api/status`, and never executes. It emits nothing, forever, which is the
same silent-nothing as an `EMPTY_RESULTSET` source and just as invisible. This
is easy to introduce by accident because `hp_source.interval` defaults to 0 and
0 means "on-demand pivot", which is right for a `{q}` row and wrong for a bulk
file. 763 rows across batches 18 and 19 were in that state before it was
checked for.

```sh
python3 native/tools/audit_batch_reachable.py docs/candidate-sources-batch*.txt
```

## Batch tooling

New sources are authored as a pipe-delimited manifest and generated, not
hand-written. `docs/candidate-sources-batch<N>.<beat>.txt` is the source of
truth; `collectors/pivot/table/hp3*_<beat>.c` is generated. Edit the manifest.

| tool | what it enforces |
| --- | --- |
| `tools/manifest.py` | THE manifest parser — line split, field count, and `opts` resolution — imported by every tool below. It is one file because it used to be seven, and they disagreed: see rule 4c |
| `tools/probe_hp_batch.py` | proof of life: 2xx, parses in its declared mode, ≥1 real record. Honours each row's own headers, handles JSON/CSV/XML/HTML, rejects empty result sets, HTTP-200 refusals, one-element error arrays and bot-wall challenge pages. A **declared `array_path` is resolved and judged** — it used to hunt for the densest array and once counted a response's own 252-key *schema block* as records, passing a row whose result set was empty. **`--check-filter`** additionally asks each pivot row about an IMPOSSIBLE entity and fails it `FILTER_IGNORED` when the answer is the same size — see rule 4d |
| `tools/batch_exclusions.py` | no duplicate id or endpoint against the existing tree or within the batch (normalising `{q}` and `%s` to one form; `.portal` is documentation and is excluded). Sees **runtime-composed** endpoints too — it resolves string macros, joins adjacent literals, follows `#include "*.inc"`, and matches a `%s` URL family on its layer/dataset NAME. Pass **`--bin ./bin/japanosint`**: without it the id set is a regex approximation (4,754 of 13,193) and it says so |
| `tools/audit_batch_pagination.py` | a paged endpoint declares `page_param` or `next_path` — read from the parsed opts, not as a substring of the whole field |
| `tools/audit_batch_reachable.py` | rule 3 above. A row whose opts are ambiguous is reported UNVERIFIABLE, never "never runs" |
| `tools/audit_batch_emit.py` | rule 4 below: runs each MANIFEST row through the real binary and reads back `emitted N of M`. `--timeout S` moves the kill line; a run that hits it is **`SLOW`**, carrying its partial counts — unmeasured, not failed |
| `tools/audit_registry_emit.py` | rule 4 **and** 4b for the whole REGISTRY, manifest or not — `--list-sources` is the source list, so nothing registered can hide. Measures emitted *and* stored, per run, against a fresh copy of a warm template DB. `--scheduled`/`--match`/`--only`/`--ids-file`, `--jobs`, `--timeout`, TSV out, `--resume` |
| `tools/diagnose_emit_keys.py` | why a row emitted nothing, and which `title_keys`/`id_keys` fix it |
| `tools/gen_hp_batch.py` | manifest → C, one table per beat, `--prefix`/`--batch` so batches never collide. Rejects duplicate opts, non-integer int opts, and an opt whose value **swallowed the next one** through a stray `\;` |

## 4. Fetching is not emitting — prove the second one

`probe_hp_batch.py` proves an endpoint answers and that its body parses. It says
nothing about whether the **engine** turns those records into `intel_items`, and
the gap between the two is not small. Batch 18 was fully probe-verified, and
when its 223 rows were first run through the actual binary, **59 of them fetched
real records and stored none** — `DATAPLANE_TELNET` fetched 36,166 and emitted
0, exit code 0, run reported successful. That is the same invisible nothing as
an `EMPTY_RESULTSET` source.

```sh
python3 native/tools/audit_batch_emit.py docs/candidate-sources-batch<N>.*.txt \
        --bin ./bin/japanosint --jobs 6
```

Run it before believing a batch. `DROPS_EVERYTHING` is the verdict that matters;
`diagnose_emit_keys.py` then separates the three causes, which want different
fixes (an engine bug, a per-row `title_keys`, or a row that is not a record
source at all).

### 4b. Emitting is not storing either

`records=N` counts `emit()` CALLS. The sink upserts on `remote_key`, so a source
whose records key onto each other reports a healthy N and stores one row — and
**every check in this file is blind to it**, because emit really was called.

Measured over a 1,197-source sweep: 46 hpengine rows losing 114,795 records per
pass. `ECDC_RESPIRATORY` emitted 12,648 and stored **31**.

**The same defect existed independently in all three record paths** —
`lib/jsonlist.c` (`us-openfda-device-pma-detail`: 109 emitted, 1 stored),
`lib/hpengine.c`, and `lib/geojson.c` (474 rows rescued across 45 geo sources,
454 of them from one). Fixing one left the others losing data, which is the
strongest argument in this repo for looking for the *other copies* of any bug
you fix. All three now carry a collision guard that flags records colliding
within one page/array and disambiguates them by CONTENT hash — so byte-identical
records still collapse (real dedupe) while records that merely share a key are
all kept. Nothing is invented, and nothing that differs is merged.

The trap that makes this easy to introduce: `id_keys` is a MANIFEST DECLARATION,
not the upstream's identity. Declaring a dimension (`id_keys=country_code` on a
weekly time series) as the record id silently discards the series. When you add
a row, check that its `id_keys` is unique per record, not per group:

```sh
./bin/japanosint --run <ID>          # the run line now states BOTH numbers
sqlite3 $JO_DB "select count(*) from intel_items where source_id='<ID>'"
```

If the second number is smaller than the first, the row's identity is wrong.

**The run line says it without being asked.** `core/intel.c` counts the DISTINCT
uids a run upserts and `core/scheduler.c` prints it, so the two numbers arrive
together and the defect is legible without a second command:

```
[sched] ANTARES_LOCI run rc=0 records=1001 7388ms stored=1001
[sched] WHO_XMART_WHSA_FACT run rc=0 records=10001 7935ms stored=2 \
        UID-COLLISION: 9999 of 10001 emitted records collapsed onto a uid already written this run
```

`stored` is DISTINCT-UID, not rows-inserted, and the difference matters: a
scheduled source re-fetching an unchanged feed inserts nothing and updates
everything, so rows-inserted would report total loss on every ordinary re-run.
A metric that cries wolf on every re-run is one nobody reads when a real
discard happens. Distinct-uid is stable across re-runs and moves only when a
run's own records collapse onto each other. It also lands in
`fetch_log.stored` (NULL = not measured, negative = a floor), so the history is
comparable and not just whatever run a human happened to watch.

The `stored=` field is appended AFTER the duration deliberately — eight parsers
in `tests/audit/` and `tools/` match `records=(-?\d+) (\d+)ms` as one unit.

Sweep the whole registry for both failures, not just a batch:

```sh
python3 native/tools/audit_registry_emit.py --bin ./bin/japanosint \
        --scheduled --jobs 6 --timeout 220 --out sweep.tsv
```

It runs each source against a fresh copy of a warm template DB and reads the
stored count back out of that database as well as off the run line — two
independent readings, because a checker that believes one self-report is how
260 silent sources got through in the first place. Verdicts: `OK`,
`EMITS_NOTHING`, `COLLISION`, `SLOW` (hit `--timeout`; unmeasured, not failed),
`NEEDS_ENTITY`, `SINK_MISMATCH`.

### 4d. Answering is not answering THE QUESTION

Every gate above counts records. None of them asks whether the records are about
the thing you asked for. Three APIs in batch 21 accept a filter, **silently
ignore it, and return the whole unfiltered collection with HTTP 200**:

* EPA Envirofacts, given a column that does not exist
  (`tri_reporting_form/facility_name`) → 10,000 unrelated records
* the German BMJ portal, on `court` / `documentNumber` / `dateFrom`
* Health Canada MDALL, on an unvalidated query → the entire 21 MB table

probe PASS. emit OK. `stored == emitted`. Every gate green — and the row is an
ENTITY PIVOT, so an analyst asking "what do we have on X" gets thousands of
records about everything else, attributed to X. **That is worse than a source
that returns nothing: it is a confident wrong answer**, and no amount of record
counting can see it.

The check is one extra request: ask the same endpoint about an entity that
cannot exist. A working filter returns nothing, or something much smaller. A
filter being ignored returns the same collection it just returned for the real
entity.

```sh
python3 native/tools/probe_hp_batch.py docs/candidate-sources-batch<N>.*.txt --check-filter
```

Opt-in because it doubles the request count for pivot rows. Run it at least once
per batch, and treat `FILTER_IGNORED` as fatal — the row must be dropped or
re-pointed at a parameter the upstream actually honours.

### 4c. Two tools reading one manifest must read it the same way

`OSM_API_CHANGESETS_BLACKSEA` carried `interval=86400\;pagination_ok=…;interval=86400`.
`gen_hp_batch.py` resolved the duplicated key last-wins and generated correct C;
`audit_batch_reachable.py` resolved it first-wins, and reported a row that runs
daily as one that can never run (house rule 3). Seven tools read these
manifests and each brought its own parser.

There is now exactly one: **`native/tools/manifest.py`**. It does not resolve a
duplicate at all — it reports it, and every reader refuses to guess
(`gen_hp_batch.py` rejects the row, the auditors call it UNVERIFIABLE). It also
hands back the lines that LOOK like rows and are not, because "skipped" and
"checked and clean" used to be the same output.

Unifying it immediately found three live defects of the same family, where a
stray `\;` escaped the separator and the following opt was swallowed into the
previous VALUE — shipped into committed C as
`.date_keys = "exchangedate;pagination_ok=start/end are a DATE range…"`, a
`.detail_key` that names no field, and a User-Agent header with 130 characters
of prose glued to it. `gen_hp_batch.py` now rejects that shape by name.

Engine subtleties worth knowing before writing a row:

* `page_start`'s unset value is 0, which is also a legitimate first page. Set
  **`page_zero_based=1`** for a 0-based API — otherwise the engine coerces the
  start to 1 and silently never fetches page 1.
* `next_path` accepts a `key=value` segment: write **`links.rel=next.href`**,
  not `links.1.href`. Indexing a hypermedia link array positionally breaks when
  a server reorders it, and the failure mode is the engine refetching page 1
  until the page ceiling — every later page lost, with the run still looking
  successful.
* A "CSV" feed is often not comma-separated. DataPlane.org publishes
  `ASN | AS name | ip | lastseen | category`; declare **`csv_delim=pipe`** (or
  `tab`, or `semi` — named, because the manifest is itself pipe-delimited) or
  the whole line lands as one unqueryable cell. **`csv_comment=#`** strips a
  banner before the parse, which matters because URLhaus keeps its column names
  in a `#` line and header parsing would otherwise name every column after a
  comment.
* Titles: the engine will key a record on its first non-empty scalar rather than
  drop it, so a row without `title_keys` still emits — but it emits titled
  `<record_type> <whatever came first>`. Declare `title_keys`/`id_keys` for
  anything whose fields are not named `name`/`title`/`id`.

Every source self-registers with `REGISTER_SOURCE` (or `HP_REGISTER_TABLE`) and
is both schedulable (`update_interval_sec > 0`) and dispatchable as an
entity pivot. There is no separate "service" type.
