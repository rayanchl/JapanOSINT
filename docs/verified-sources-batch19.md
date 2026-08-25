# Batch 19 — 1,692 verified sources across 27 beats

Assembled by a fleet of per-beat agents, consolidated 2026-08-22. Row data is in
`candidate-sources-batch19.<beat>.txt`; the evidence for each row is in
`verified-sources-batch19.tsv`, and every rejection is kept with its reason in
`rejected-sources-batch19.tsv`.

| | |
| --- | --- |
| candidate rows | 1,810 |
| **verified** | **1,692** |
| rejected | 118 |
| generated tables | 27 (`native/collectors/pivot/table/hp3b19_*.c`) |
| records emitted in one full pass | **4,632,444** |

## How the verification was done, and what that is worth

**Probing is multi-pass, because one pass lies.** A parallel probe rate-limits
some hosts into a false rejection. Pass 1 called 167 rows dead; a serial retry
recovered 33 of them and a third pass another 4. Any single number from a single
pass understates the batch by ~2%.

**Fetching is not emitting.** Every row was then run through the real engine
(`tools/audit_batch_emit.py`), which is the only check that proves the engine
turns a response into `intel_items`. Batch 18 had been fully probe-verified and
59 of its 223 rows stored nothing. Batch 19's final run: **1,581 OK, 0 partial,
0 dropping everything**.

**Probing now uses the User-Agent the engine sends.** It previously used its
own, so the 1,623 rows that declare no header of their own were proven against
a string they will never send. Re-probed under the real UA: 1,675 of the 1,692
re-confirmed, and not one of the differences was a 403 or 406 — which is what
a UA refusal looks like. The change cost no sources; it made the claim true.

## Read the rejections with these caveats

* **71 of the 84 network-error rejections are two hosts** — `overpass-api.de`
  (55) and `celestrak.org` (16) — and neither accepts a connection from the
  machine this ran on. DNS resolves, every request returns 000, including their
  own status pages. Those rows are **unproven here, not proven dead**, and
  should be re-probed from another network before anyone deletes them.
* **17 verified rows rest on the older-UA evidence**, having failed to
  re-confirm in the final window. Every one has a transient signature and none
  is a refusal: `Too Many Requests` (OpenFEC, FCC ECFS, two Overpass rows),
  `Bad Gateway` (NGX ×2, Jordan open data), connection resets and TLS timeouts
  (`AP19_IN_*`, `EG_MSA_REPOSITORY`, `EIDA_WFCATALOG_INGV`), and two that were
  simply empty at that moment — `AUTOBAHN_WARNINGS` returns `{"warning":[]}`
  when no autobahn warning is in force, which is an honest empty rather than a
  structurally empty source.

## Two beats are recovered work

`fisheries` and `humanrights` had no manifest at all: their agents were killed
mid-discovery by a session limit and left only raw downloads. The FAO and FIRMS
WFS layer names here are read out of the GetCapabilities documents those agents
had already fetched, and every row was re-fetched and re-confirmed before being
written.

What could not be recovered is recorded in each file's header with its reason,
because a source left out silently is indistinguishable from one nobody thought
of: ACLED and UCDP require access tokens, ReliefWeb and the whole of HDX now
answer 406 to a bot (HDX answered for that agent five days earlier), IDMC
requires a client id, and the ICCAT/IOTC/IATTC vessel records publish no
machine-readable export.
