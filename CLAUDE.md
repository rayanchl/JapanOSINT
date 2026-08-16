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

* **strict set (`collectors/sources/hp*_*.c`) — 0 findings.** This is the part
  the Makefile gates on, and it is held clean.
* **the rest of the tree — 55 findings across 42 of 1,239 files**: 28
  single-page, 26 record-cap, 1 first-only. These are heuristics and each needs
  a human read, but "zero audit findings" is true only of the strict set — do
  not read it as true of the tree.

Deliberate exceptions carry an inline `/* exhaustive-ok: <reason> */` marker
(`grep -rn exhaustive-ok`, currently 254).

**There is ONE page walk**: `lib/pagewalk.c`. `jsonlist_emit_paged()` is a
~20-line adapter onto it, not a second engine — the tree briefly carried two,
and they disagreed about when a page counts as full and whether a cursor
parameter absent from the URL may be invented. New paged collectors call
`pw_walk()` (or the jsonlist door onto it); nothing else re-derives "is there
more?".

```sh
cd native
make audit-sources   # scan every collector for discard patterns
make hptest          # offline check of the engine's guarantees
make pagewalktest    # offline check of the paging + disclosure engine
make lint-sources    # mechanical collector checks against tools/lint_baseline.json
make source-floor    # fails if a collector stopped registering (tools/source-floor.txt)
make                 # full build (-Wall -Wextra)
```

## Where things live

```
native/source.h                 the ONE data-acquisition ABI (source_def + intel_sink)
native/lib/hpengine.{c,h}       declarative deep-record collector engine
native/lib/pagewalk.{c,h}       the ONE page walk + truncation disclosure
native/collectors/**/*.c        one file per collector family; the Makefile glob
                                is RECURSIVE — sources/ plus feed/, pivot/, pod/
native/core/                    db, http, intel sink, dispatcher, pipeline, HTTP API
docs/                           plans, pipeline notes, and the two house rules above
```

Every source self-registers with `REGISTER_SOURCE` (or `HP_REGISTER_TABLE`) and
is both schedulable (`update_interval_sec > 0`) and dispatchable as an
entity pivot. There is no separate "service" type.
