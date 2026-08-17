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

**The registered count is 11,170**, and `tools/lint_sources.py` is the only
thing that counts it. It used to say 10,564, because hp rows register through
`hp_register()` rather than `REGISTER_SOURCE` and it could not see a single one
of them — 30 shipped tables' worth. `make source-floor`, whose entire job is to
fail when a source stops registering, was therefore blind to every hp row.

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

Both engines page on the upstream's own evidence, and they share one
implementation of it: `lib/pager.c`. A next link the server published, or a
cursor whose page size the URL declares or the response proves, or — when the
URL gives no page size at all — the upstream's own declared total saying records
remain. Never a page that was never offered. hpengine used to do none of this
(one request unless a row spelled out `page_param`), which meant moving a row
onto it to wire a detail hop paid for that hop with every later page.

Where the tree actually stands, as `make audit-sources` reports it:

* **strict set — 0 findings across 159 files.** `collectors/pivot/table/hp*_*.c`
  plus the generated deep-record tables `collectors/feed/generated/hp1[0-9]_*.c`.
  This is the part the Makefile gates on, and it is held clean.
* **the rest of the tree — 120 findings across 76 of 1,523 files**: 51
  first-only, 32 record-cap, 26 loop-break, 11 single-page. `limit-one` and
  `dedupe-ring` are now zero. These are heuristics and each needs a human read,
  but "zero audit findings" is true only of the strict set — do not read it as
  true of the tree.

Two traps in reading that number, both of which cost real coverage:

* **The audit does not know what the engines do.** It flagged `?page=1&
  per_page=100` as a discard on 21 rows that `jsonlist_emit_paged` had been
  walking all along. An audit that cries wolf is one whose other findings go
  unread; it now models both halves — is this row on a paging engine, and can
  the pager actually move this URL.
* **`limit-one` was worse than it looked.** Nine of the eleven were probe URLs
  shipped as collectors — `search=k_number:"K092877"&limit=1` re-fetching one
  2009 device clearance twice a day, on rows whose own descriptions called them
  detail hops. A row authored as a lookup and shipped as a feed is useless as
  both. They are pivots now (`collectors/pivot/table/hp18_us_openfda_ids.c`).

Deliberate exceptions carry an inline `/* exhaustive-ok: <reason> */` marker
(`grep -rn exhaustive-ok`, currently 147). The marker must sit on the flagged
line itself — the audit reads it per line, so one on the line above is ignored.

```sh
cd native
make audit-sources   # scan every collector for discard patterns
make hptest          # offline check of the engine's guarantees (63 assertions)
make                 # full build (-Wall -Wextra)
```

## Where things live

```
native/source.h                 the ONE data-acquisition ABI (source_def + intel_sink)
native/lib/hpengine.{c,h}       declarative deep-record collector engine
native/collectors/sources/*.c   one file per collector family; Makefile globs them
native/core/                    db, http, intel sink, dispatcher, pipeline, HTTP API
docs/                           plans, pipeline notes, and the two house rules above
```

Every source self-registers with `REGISTER_SOURCE` (or `HP_REGISTER_TABLE`) and
is both schedulable (`update_interval_sec > 0`) and dispatchable as an
entity pivot. There is no separate "service" type.
