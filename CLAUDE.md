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

The tree is at **zero audit findings**; deliberate exceptions carry an inline
`/* exhaustive-ok: <reason> */` marker (`grep -rn exhaustive-ok`).

```sh
cd native
make audit-sources   # scan every collector for discard patterns (expect 0)
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
