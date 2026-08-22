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
* **the rest of the tree — ~127 findings across ~74 files**: first-only,
  single-page, record-cap, loop-break, limit-one and dedupe-ring. These are
  heuristics and each needs a human read, but "zero audit findings" is true
  only of the strict set — do not read it as true of the tree.

Deliberate exceptions carry an inline `/* exhaustive-ok: <reason> */` marker
(`grep -rn exhaustive-ok`).

```sh
cd native
make audit-sources   # scan every collector for discard patterns
make hptest          # offline check of the engine's guarantees
make lint-sources    # dup ids/endpoints, quarantine-empty, snprintf guards
make                 # full build (-Wall -Wextra)
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
| `tools/probe_hp_batch.py` | proof of life: 2xx, parses in its declared mode, ≥1 real record. Honours each row's own headers, handles JSON/CSV/XML/HTML, rejects empty result sets, HTTP-200 refusals, one-element error arrays and bot-wall challenge pages |
| `tools/batch_exclusions.py` | no duplicate id or endpoint against the existing tree or within the batch (normalising `{q}` and `%s` to one form; `.portal` is documentation and is excluded) |
| `tools/audit_batch_pagination.py` | a paged endpoint declares `page_param` or `next_path` |
| `tools/audit_batch_reachable.py` | rule 3 above |
| `tools/gen_hp_batch.py` | manifest → C, one table per beat, `--prefix`/`--batch` so batches never collide |

Two engine subtleties worth knowing before writing a paged row:

* `page_start`'s unset value is 0, which is also a legitimate first page. Set
  **`page_zero_based=1`** for a 0-based API — otherwise the engine coerces the
  start to 1 and silently never fetches page 1.
* `next_path` accepts a `key=value` segment: write **`links.rel=next.href`**,
  not `links.1.href`. Indexing a hypermedia link array positionally breaks when
  a server reorders it, and the failure mode is the engine refetching page 1
  until the page ceiling — every later page lost, with the run still looking
  successful.

Every source self-registers with `REGISTER_SOURCE` (or `HP_REGISTER_TABLE`) and
is both schedulable (`update_interval_sec > 0`) and dispatchable as an
entity pivot. There is no separate "service" type.
