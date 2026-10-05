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

**The registered count is 18,214** (2026-10-05, after batch 33's 43
supply-chain rows and batch 34's one NDJSON row; 18,170 was the count on
2026-10-02 after batch 32's 340 Japanese rows — `docs/verified-sources-batch32.md`):
`make lint-sources` prints it, counting hp_source table rows as well as
`REGISTER_SOURCE`, and `./bin/japanosint --list-sources` agrees.

**Batch 33 is the one batch measured end to end against live upstreams** —
43 supply-chain and package-registry rows
(`collectors/pivot/table/hp3b33_supplychain.c`,
`native/collectors/OSINT_SOURCES_BATCH_33_SUPPLYCHAIN.md`). The environment it
was authored in reaches package-registry hosts and only those, so probe
`--check-filter`, `audit_batch_emit`, per-pivot `--run` and the registry sweep
all ran for real: every row has an emitted-AND-stored reading, and the counts
in its descriptions are what the bodies held. Note what that constrains —
`docs/rejected-sources-batch33.tsv` lists the hosts the policy refused, OSV,
deps.dev, libraries.io and ecosyste.ms among them, and the 144 rows staged in
`docs/candidate-sources-batch31.*.txt` are on 140 hosts of which **zero** are
reachable from that environment, so they stay unprobed.

**One exception to "every source is proof-of-life verified":** batch 31 — the
government, public-record and surveillance tables from PR #23
(`collectors/pivot/table/hp3b31_*.c`) — holds 361 rows, and 334 of them are
entity pivots that have not been run against a real entity, nor
`--check-filter`ed (rules 4 and 4d). The 27 scheduled rows were run on
2026-10-02: 13 store real records (FDSN 151,303; Safecast 30,000; Sejm 15,000;
USGS NWIS 53,789 before the audit timeout …) and 14 need an API key and store a
"gated" notice saying so. Ten rows were removed when the branches merged: seven
dead endpoints (400/401/404 on every run) and three that re-fetch what an
existing collector already reads. See
`native/collectors/OSINT_SOURCES_BATCH_31_GOV_PUBLIC_SURVEILLANCE.md`. Treat
those pivots as registered, not proven, until they are verified.

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

VJSON collectors page through `jsonlist_emit_paged()`, whose per-page decision
— a next link the server published, else a cursor paired with a page size the
URL declares, else a page number the URL carries when the upstream's own total
says more remains — is `jsonlist_next_page()` in `lib/jsonlist.c`. hpengine rows
declaring `page_walk=1` call that same function, so a row moved from VJSON onto
hpengine pages exactly as it did. Without that (or an explicit `next_path` /
`page_param` / `{page}`) an hp row makes ONE request.

Where the tree actually stands, as `make audit-sources` reports it:

* **strict set — 0 findings across 285 files** (2026-10-05; 256 on 2026-09-27):
  `collectors/pivot/table/hp*_*.c` plus the generated deep-record tables
  `collectors/feed/generated/hp1[0-9]_*.c`. This is the part the Makefile
  gates on, and it is held clean. Run `make audit-sources`
  after adding a table: batch 18 introduced two `single-page` findings here (a
  paged endpoint declared without `page_param`) and they had to be fixed before
  the gate would pass again.
* **the rest of the tree — 0 findings** (1,642 files scanned, 2026-09-27; it
  first reached zero on 2026-08-24, from ~127 across ~74 files). Every first-only, single-page, record-cap, loop-break,
  limit-one and dedupe-ring finding has been read and closed one of three ways:
  the discard was real and was fixed, the line carries an `exhaustive-ok`
  marker with a reason, or it was a scanner false positive and is marked as
  such. The recoveries were not small — IRS exempt-orgs went from 5,000 rows to
  278,014, ESMA from 1 to 1,377, Homebrew from 300 to 23,133, and twelve
  scheduled feeds were asking their upstream for `limit=1`.

  So "zero audit findings" is now true of the whole tree, not just the strict
  set — which makes any NEW finding a regression rather than a number in a
  backlog. Keep it that way: `make audit-sources` is cheap and takes seconds.

* **A bound in a loop's own CONDITION was invisible until `loop-cap`.**
  `while (cJSON_GetArraySize(akas) < 24 && …)` is neither a `#define …MAX`
  (`record-cap`) nor a `break` (`loop-break`), so the tree read "zero" while
  the OFAC consolidated parser dropped a designated person's aliases past the
  24th, TDnet stopped at 100 disclosures a day and EDINET-x at 25 filings. The
  check exists now; all of those are fixed.

Deliberate exceptions carry an inline `/* exhaustive-ok: <reason> */` marker
(`grep -rn exhaustive-ok`). The marker must sit **on the flagged line itself** —
the scanner matches per line, so a marker in the comment block above the line it
explains is silently ignored and the finding stays. That is easy to get wrong,
because the explanation naturally wants to be a paragraph: put the paragraph
above and a one-line `/* exhaustive-ok: … */` on the line.

Two amendments from the deep-record batch:

* **The gated set is now two globs**, `collectors/pivot/table/hp*_*.c` plus the
  generated deep-record tables `collectors/feed/generated/hp1[0-9]_*.c` — 285
  files as of 2026-10-05 (262 when this was written), 0 findings. `--strict` had to be made repeatable to say that honestly:
  it took a single glob, so passing two kept only the LAST one and the gate
  printed "0 findings" for a set it had never opened.
* **`loop-cap` is a sixth check, and it was not redundant.** A bound written in
  a loop's own CONDITION is neither a `#define …MAX` (`record-cap`) nor a
  `break` (`loop-break`), so nothing could see it. Added after the tree reached
  zero, it immediately found four live caps — all in the OFAC sanctions
  collectors: aliases and sanctions programs capped at 24, addresses at 12, and
  SDN "features" (date and place of birth, nationality, passport and
  national-ID numbers, crypto addresses) at 40. On a sanctions list those are
  the fields screening matches on, so each dropped entry is a false negative on
  a designated person, produced in silence. All four are gone.

  Its own first regex missed the very line it was written for: `\b` before the
  counter list cannot match `cJSON_GetArraySize`, because the underscore is a
  word character. Zero findings from a new check deserves one suspicious look.

**Detail hops: 359 rows, and what a generator that rewrites this tree must not
do.** `collectors/gen_detail_hop_upgrade.py` moves a row that has a PROVEN
per-record detail endpoint off VJSON onto hpengine, so the record behind each
list hit is fetched. It reads the `.c` files, never the manifest, for the reason
the batch-tooling section gives: the C is the maintained copy.

It cost 73 sources to learn three things, all the same mistake wearing different
hats — the script could not see something, and deleted it anyway:

* `^(V[A-Z]+)\(` cannot match an underscore, so `VJSON_KEYED`, `VJSON_IDKEYS`,
  `VJSON_PREP` and friends were invisible — and the "no registrations left,
  delete the file" step could not see them either, so files were removed with
  those rows still inside. 20 sources.
* It writes one table per (batch, collector) group of the CURRENT run, so
  running it twice with different row sets rewrote a group's file with only the
  second run's rows. 53 sources. **Regenerate from a clean checkout of the
  directory in ONE run**, never incrementally.
* Its argument splitter did not skip comments, and this tree comments INSIDE
  argument lists; 86 rows parsed as 15 arguments instead of 12 and were passed
  over. That one failed safe, but it failed.

So it now asserts that no id registered in the directory before the run — from
the vsrc macros AND from any hp table already there — is missing after it, and
exits non-zero naming them. The only reason any of this was caught is an id-set
diff against the base branch; `--list-ids | sort | comm` costs seconds and is
the check that actually works.

`VJSON_KEYED`'s IDFIELD and `VJSON_IDKEYS`'s IDKEYS carry over verbatim into
`.id_keys` — `_vjson_idkeys.inc` promises the same `+` composes semantics, and
`hptest` now pins it, because rule 4b's failure is silent. The 45 rows that
moved on that promise are listed in `docs/detail-hops-need-emit-check.tsv`:
their identity is a faithful translation, but emitted-vs-stored has not been
measured on them, and that needs egress this session did not have.

`VJSON_PREP` is deliberately NOT converted: its extra argument is a C function
that reshapes each page before emit, and a declarative row cannot hold code.
VRSS and VGEO stay out too — hpengine has no RSS or FeatureCollection mode.

```sh
cd native
make                 # full build (-Wall -Wextra); the tree is at 0 warnings, keep it there
make selftest        # boot self-test: DB integrity, schema objects, llm probe
make unit            # tests/unit/run.sh against a scratch DB
make hptest          # offline check of the engine's guarantees
make lint-sources    # dup ids/endpoints, quarantine-empty, snprintf guards
make audit-sources   # scan every collector for discard patterns
make pagewalktest    # offline check of the paging + disclosure engine
make source-floor    # fails if a collector stopped registering (tools/source-floor.txt)
```

Those eight are the CORE of what CI runs, not all of it, and the count in this
sentence used to say "six" while listing eight. `.github/workflows/ci.yml` also
runs `make authtest`, `make htmlparsetest`, `make registry-floor`,
`tools/ci_concurrency_gate.sh`, a TSAN scheduler job, an ASAN job and the
client's `vitest` + `vite build`. Run the three extra `make` targets locally —
they are seconds each — and expect CI to be the first thing that exercises
TSAN, ASAN and the client.

**And treat "CI is red" as a question, not an answer.** `ci.yml` carried
`JO_DB: ${{ runner.temp }}/ci.db` as a JOB-LEVEL `env:` entry, where the
`runner` context does not exist yet. That is an unrecognised named-value, which
invalidates the whole FILE rather than the one job: GitHub created each run,
failed it immediately and scheduled ZERO jobs. **Every one of the first 74 runs
failed that way, from this file's first commit** — so for 74 runs "CI is red"
was never a test result, because nothing had ever been compiled or executed.
PR #28 moved it to a `$GITHUB_ENV` step and the runs after it are the first
real ones. A failing run with `total_count: 0` in its jobs list is this shape,
not a gate.

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

New sources are AUTHORED as a pipe-delimited manifest and scaffolded, not
hand-written: `docs/candidate-sources-batch<N>.<beat>.txt` goes through the
probe/exclusion/reachability gates below and then through `gen_hp_batch.py`,
which is how a beat of 100+ rows gets written at all.

**After a beat ships, the C is the maintained copy and the manifest is the
record of how it was probed.** That is a correction of what this file used to
say ("the manifest is the source of truth; edit the manifest"), and it is a
correction to match reality rather than a change of policy: measured
2026-09-11 across every batch manifest, **48 of 79 beats already differ from
what their manifest generates — 62,461 lines** — and the direction is always
the same. The C holds `id_keys = "公司代號+姓名+職稱"` with the live counts
that proved it (27,528 rows, 26,789 distinct triples); the manifest still says
`公司代號`, the key that lost 97% of them. The manifest format has nowhere to
put that evidence, and a repair without its evidence is unreviewable.

So: **regenerating a shipped beat reverts verified repairs.** Both generators
now refuse to overwrite an existing file and report how many lines would have
changed; `--force` is for the case where the manifest really is the newer copy
(a whole beat re-authored), and a `diff` is the answer every other time. Mirror
an opts change back into the manifest when it is cheap — it keeps the probe
gates honest for a future re-probe — but never at the cost of the comment that
says why.

| tool | what it enforces |
| --- | --- |
| `tools/manifest.py` | THE manifest parser — line split, field count, and `opts` resolution — imported by every tool below. It is one file because it used to be seven, and they disagreed: see rule 4c |
| `tools/probe_hp_batch.py` | proof of life: 2xx, parses in its declared mode, ≥1 real record. Honours each row's own headers, handles JSON/CSV/XML/HTML, rejects empty result sets, HTTP-200 refusals, one-element error arrays and bot-wall challenge pages. A **declared `array_path` is resolved and judged** — it used to hunt for the densest array and once counted a response's own 252-key *schema block* as records, passing a row whose result set was empty. **`--check-filter`** additionally asks each pivot row about an IMPOSSIBLE entity and fails it `FILTER_IGNORED` when the answer is the same size — see rule 4d |
| `tools/batch_exclusions.py` | no duplicate id or endpoint against the existing tree or within the batch (normalising `{q}` and `%s` to one form; `.portal` is documentation and is excluded). Sees **runtime-composed** endpoints too — it resolves string macros, joins adjacent literals, follows `#include "*.inc"`, and matches a `%s` URL family on its layer/dataset NAME. Pass **`--bin ./bin/japanosint`**: without it the id set is a regex approximation (4,754 of 13,193) and it says so |
| `tools/audit_batch_pagination.py` | a paged endpoint declares `page_param` or `next_path` — read from the parsed opts, not as a substring of the whole field |
| `tools/audit_batch_reachable.py` | rule 3 above. A row whose opts are ambiguous is reported UNVERIFIABLE, never "never runs" |
| `tools/audit_page_param.py` | rows whose URL already binds their own `page_param`. The engine used to APPEND (`…&pagina=1&pagina=2`) and a server binding the first occurrence then served page 1 for the whole walk — N pages emitted, one stored, `rc=0`. Fixed in `hp_url_set_param` and pinned by `hptest` "9f-bis"; the lint stays because 103 of 1,431 paged rows are that shape and their paging depends on the replacement being right |
| `tools/audit_batch_emit.py` | rule 4 below: runs each MANIFEST row through the real binary and reads back `emitted N of M`. `--timeout S` moves the kill line; a run that hits it is **`SLOW`**, carrying its partial counts — unmeasured, not failed |
| `tools/audit_registry_emit.py` | rule 4 **and** 4b for the whole REGISTRY, manifest or not — `--list-sources` is the source list, so nothing registered can hide. Measures emitted *and* stored, per run, against a fresh copy of a warm template DB. `--scheduled`/`--match`/`--only`/`--ids-file`, `--jobs`, `--timeout`, TSV out, `--resume` |
| `tools/diagnose_emit_keys.py` | why a row emitted nothing, and which `title_keys`/`id_keys` fix it |
| `tools/gen_hp_batch.py` | manifest → C, one table per beat, `--prefix`/`--batch` so batches never collide — but the batch NUMBER is yours to keep unique: main shipped a batch 32 while a branch was open with its own, both created `docs/verified-sources-batch32.tsv`, and renumbering the branch to 33 after the fact meant touching the manifest, both TSVs, the table, its array name and two batch citations that had landed in `lib/hpengine.h` and `core/hostgate.c`. Rejects duplicate opts, non-integer int opts, and an opt whose value **swallowed the next one** through a stray `\;` |

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

**In `id_keys`, `+` COMPOSES and `,` CHOOSES.** `year+quarter+item` joins all
three into one key; `year,quarter,item` keys on `year` and never reads the
rest. They nest: `a+b,c` = "a+b, or else c". Use `+` whenever identity is a
tuple of dimensions, which is most statistical and regulatory tables.

Getting this backwards fails SILENTLY, and it is the most repeated mistake in
this file's history: 26 rows written in the 2026-09 fix pass used commas
meaning composites. They looked right because the in-page collision guard
content-hashes records that collide inside ONE page — the same key recurring
on a LATER page still collapsed at the sink. `FINRA_OTC_BLOCKS` emitted 2,000
and stored 476 (its distinct first-token values) until the commas became
pluses, then stored 2,000. A single-page `--run` cannot see this; only the
registry sweep can.

The generated `VJSON` fleet has the same question and a smaller answer:
`VJSON` keys on a fixed precedence list (`id`, `uid`, `guid`, `uuid`, `_id`,
`identifier`, `code`, `key`), and **`VJSON_KEYED(..., IDFIELD)`** overrides it
with ONE named top-level field. There is no composite form: when a generated
row needs a two-field key, hand-write a `run()` — the worked example is
`geo_tidesandcurrents_currents` at the top of
`collectors/feed/generated/vsrc_environment_3.c` (`pw_walk` +
`jsonlist_emit_ex`, composing the id from two fields the record already
carries). Delete the `VJSON(...)` block you replace; leaving both defines the
source twice and breaks the build.

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

Two more of the same family turned up in batch 33, both in
`tools/probe_hp_batch.py`, and both made it report a source DEAD that the
engine measurably handles — which matters more than the reverse, because the
probe is the tool that decides whether a row ships:

* its anchor regex was `href\s*=\s*["\']([^"\'#]+)["\']`, and that class stops
  at the `#` and then demands the closing quote right there, so an href
  carrying a fragment matched NOTHING. Every anchor in a PEP 503 simple index
  ends `...tar.gz#sha256=<64 hex>`, so `pypi.org/simple/requests/` scored 0
  anchors on a page holding 244 and came back UNPARSEABLE. hpengine reads the
  whole attribute and keeps them.
* its declared-`array_path` resolver walked dict keys only, so it understood
  neither `.` (the root) nor a path that crosses an array, and four live rows
  came back `PATH_UNRESOLVED`. `_resolve_array_path()` now mirrors `hp_path`
  and `hp_path_multi`.

Unifying the parser immediately found three live defects of the same family, where a
stray `\;` escaped the separator and the following opt was swallowed into the
previous VALUE — shipped into committed C as
`.date_keys = "exchangedate;pagination_ok=start/end are a DATE range…"`, a
`.detail_key` that names no field, and a User-Agent header with 130 characters
of prose glued to it. `gen_hp_batch.py` now rejects that shape by name.

Engine subtleties worth knowing before writing a row:

* `page_start`'s unset value is 0, which is also a legitimate first page. Set
  **`page_zero_based=1`** for a 0-based API — otherwise the engine coerces the
  start to 1 and silently never fetches page 1.
* **A bare root array must be DECLARED: `array_path=.`** is the document
  itself. With no `array_path` the engine picks the densest array of OBJECTS,
  which is not the root as soon as a root record carries a longer nested array
  of its own. hex.pm returns 100 packages per page, each with its own
  `releases` list, so on a 60-page walk the engine mined `[33].releases` (133)
  over the root (100) on 34 of the 60 pages: 9,643 records emitted where the
  pages hold 6,000, keyed on a field the release entries do not carry, 2,881
  collapsing at the sink. It is disclosed — a `collector-shape-notice` naming
  the array mined and the runner-up — but a notice is a row in the database,
  not a build error, so a row nobody reads after its first run keeps doing it.
  Six rows in batch 33 were that shape. `hptest` pins both halves, the hijack
  and the fix.
* **An array of bare JSON STRINGS is a record set** — `title_keys=value`,
  `id_keys=value`. `hp_json_flat()` maps a string WITH CONTENT to
  `{"value": "<the string>"}`; the `empty` counter is for a NULL, a number or
  an EMPTY string (the trailing newline of a CSV), which is what the comment
  beside it is about. Four live sources — the Microsoft Container Registry
  catalogue, NuGet's flat version list, and the complete Packagist and pub.dev
  name lists — were written off in `docs/rejected-sources-batch34.tsv` on that
  comment read as a capability claim, and all four store exactly what they
  emit (3,851 / 86 / 464,521 / 91,388). **A capability claim read off a comment
  is not a measurement**, and the measurement is one `--run` away.
* **`mode=ndjson`** (`HP_NDJSON`) for one complete JSON value per line, no
  enclosing array and no commas — the shape `index.golang.org/index` and
  `index.crates.io` publish. Declared `json` the body dies at line 2 and the
  row emits nothing forever; declared `csv` every line is one unqueryable
  cell, which is the mistake `IANA_LANGUAGE_SUBTAGS` made. It works by
  rewriting the lines into ONE array and handing that to the JSON path, so
  every key list, the collision guard, detail hops, caps, paging and the
  disclosure notices apply unchanged. A line that is not valid JSON is a LOST
  RECORD, counted into `malformed`, reported in the run line and disclosed as
  a truncation notice. Note the parse is `require_null_terminated`: plain
  `cJSON_Parse` accepts `{"a":1} oops` and copying that line's text into the
  array then poisons the whole array, taking every good line down with the
  one bad one.
* **`next_path` takes a `$last` segment** — the final element of an array —
  for an upstream whose cursor is the last record's own field rather than
  anything in an envelope. The Go module index publishes no next link at all
  and pages by `since=<the Timestamp of the last record it gave you>`, and no
  numeric index can name that element because the array's length is not known
  when the row is written.
* **`{ago:<seconds>}`** expands to RFC 3339 UTC of (now - seconds), the one
  token not derived from the entity. It is for an upstream whose only ordering
  is "everything at or after this instant", with no reverse order and no
  `latest`: the Go index is append-only from 2019-04-10, so a row without the
  anchor walks forward from 2019 on every run, emits 40,000 records an hour
  and never reaches today. `since={ago:7200}` makes the same row the live
  publish tail. **Check this when a feed's URL has no entity token and its
  interval is short** — a source that emits plenty and tells you nothing new
  is as invisible as one that emits nothing.
* An **INCLUSIVE cursor** repeats one record per page boundary (the Go index
  hands back page N's last record as page N+1's first), and the sink collapses
  it as the byte-identical duplicate it is. Do not widen `id_keys` to "fix" a
  `stored` that is a handful short of `emitted` on a cursor-paged feed —
  measure the upstream first.
* A row that declares no paging does ONE request. **`page_walk=1`** (HP_JSON
  only) walks it the way a VJSON collector is walked instead — the server's own
  next link, else the cursor its declared page size pairs with, advanced while
  pages come back full — through `jsonlist_next_page()` in `lib/jsonlist.c`,
  the one copy of that decision. The 322 batch-16/17 rows moved onto hpengine for
  their detail hops (`collectors/gen_detail_hop_upgrade.py`) carry it so they
  did not trade their later pages for the second hop.
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
