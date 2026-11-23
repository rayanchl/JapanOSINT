<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): Where things live and Rule 3 (a registered source must be reachable). -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

## Where things live

```
native/source.h                    the ONE data-acquisition ABI (source_def + intel_sink)
native/lib/hpengine.{c,h}          declarative deep-record collector engine
native/collectors/sources/*.c      hand-written collectors, one file per family
native/collectors/feed/generated/  generated scheduled feed collectors (vsrc*)
native/collectors/pivot/table/*.c  hpengine tables (hp*, hp2*, hp3*) — the strict set
native/collectors/pod/*.c          enrichment/maintenance pods
native/core/                       db, http, intel sink, dispatcher, pipeline, HTTP API
native/data/tls/                   the intermediates our upstreams omit (core/catrust.c)
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

**NO LONGER TRUE AS WRITTEN — read the next paragraph before acting on this
one.** The dispatcher stopped asking about the collector label when the `osint`
collector was removed (round 5, 2026-10-20): `is_entity_pivot()` in
`core/osint_dispatch.c` is now a one-line delegation to
`registry_is_entity_pivot()`, so there is ONE rule, it DOES test for an entity
token, and the label plays no part beyond being non-empty and not starting with
`_`. `registry.c` says why in its own comment — "the collector NAME is no
longer part of either rule: hpengine rows take their table's name as collector
(2026-10-12), so `collector != "osint"` would call 1,864 live pivots dead."
**So converting a scheduled row into a pivot needs `interval = 0` and a `{q`
token, and NOT a relabel.** This text cost a session a wrong plan on
2026-10-25 (it budgeted a collector relabel for `us-sf-311-open311-request-detail`
that the code does not require), which is the whole reason the history below is
kept rather than deleted: the FAILURE it records is still real and still the
best warning in this file about a repair that lands and does nothing. What has
changed is only the mechanism.

**The DISPATCHER did not test for an entity token, and a wrong `collector`
label silently retired 16 live pivots.** `is_entity_pivot()`
(`core/osint_dispatch.c`) asked `collector == "osint" && update_interval_sec == 0`
— a proxy for "takes an entity", and it cannot do better, because
`source_def.url` is the human-facing portal, not the URL template, so nothing
at that layer can see `{q}`. The proxy holds only because every hand-written
osint service is labelled that way. A row authored as a pivot under a FAMILY
label satisfies neither reach path: not the scheduler (interval 0) and not the
dispatcher (collector ≠ "osint"). Found 2026-10-12: the nine openFDA identifier
pivots (`us_health`), CVE Program and MITRE CWE lookup (`cyber`), two elevation
lookups and NASA POWER (`geospatial`/`environment`), Diavgeia by issuing body
and MLIT Kawabou detail. `core/service_vec.c` asks the same question, so they
were out of the service-vector index too.

The openFDA nine are the lesson: `hp18_us_openfda_ids.c`'s header documents
converting them from scheduled feeds that re-fetched one 2009 device clearance
twice a day into lookups answering for any identifier. **That repair landed and
did nothing**, because dropping the interval to 0 while keeping the feed-era
label removed them from the only path left that could run them.

Two things now hold it:

* `registry.c` prints `[registry] NEVER-RUNS id '…'` for any source with
  interval 0 under a collector that is neither `"osint"` nor an internal `_`
  lane, beside the DUPLICATE and OVERFLOW reports it already makes for the same
  reason, and **`make registry-floor` fails on a non-zero count**. The `_`
  exemption is the convention `source.h` documents; `PORT_SCANNER` is
  deliberately `_probe` because it opens raw sockets, and keeping it out of the
  LLM's schema enum is a security decision, not an oversight.
* `osint_lookup()` resolves a UNIQUE case-fold match after an exact miss.
  `osint_canon()` uppercases and `registry_get()` is a strcmp hash, so a
  lower-case id — 9,941 of 18,884 — could never be found by name at all, and
  `osint_resolve_near()` could not rescue it either: a pure case difference
  costs one edit per letter, 22 for `cve-program-record-api`, against a ceiling
  of 2. An AMBIGUOUS fold is refused rather than guessed, because `RELIEFWEB`
  (an osint pivot) and `reliefweb` (a scheduled feed) are two different sources.

