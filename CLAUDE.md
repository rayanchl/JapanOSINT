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

One batch is registered but **not** proof-of-life verified: the 1,001 candidate
sources defined across the 20 `collectors/sources/csrc14_*.c` files, authored
without egress. They obey
this rule (a dead endpoint returns an explicit error, never invented content)
but carry no 2xx/parse proof. See `docs/candidate-sources-batch14.md`; promote
them with `make verify-candidates`.

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

`make audit-sources` holds the **hp_\* engine rows at zero findings** — that is
the set it gates strictly, and it is the set to write new deep-record collectors
into. The wider tree is **not** at zero: the same run scans 1,211 files and
reports 66 heuristic findings across 50 of them (record caps, first-array-
element-only, single-page fetches of paged endpoints). They are heuristics that
each need a human read, not proven violations — but do not read "audit-sources
passes" as "nothing is being discarded". Deliberate exceptions carry an inline
`/* exhaustive-ok: <reason> */` marker (`grep -rn exhaustive-ok`).

```sh
cd native
make audit-sources   # scan every collector for discard patterns (hp*_*.c: expect 0;
                     # the wider tree currently reports 66 findings across 50 files)
make hptest          # offline check of the engine's guarantees
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
