# Batch 16 — 413 high-penetrancy US / EU / JP government and civilian sources

**Status: VERIFIED, AND PROVEN TO EMIT.** Two different bars, and this batch is
the first to clear the second one explicitly.

Manifest: `docs/verified-sources-batch16.tsv`
Detail hops: `docs/candidate-sources-batch16.detail-hops.tsv` (495 proven)
Did not emit: `docs/batch16-fetched-but-unparsed.tsv` (328, kept as data)
Collectors: `native/collectors/feed/generated/vsrc16_*.c` (59 files)

## The number, honestly

The target was 1,000. What shipped is **413**. The funnel:

| stage | count | what it means |
|---|---|---|
| probed by discovery agents | 3,189 | requests actually made |
| returned by agents as live | 1,196 | agent personally fetched and read a body |
| after dedup against the tree | 1,114 | 21 duplicate URLs, 27 duplicate ids, 34 key-gated dropped |
| independently re-probed → verified | 741 | 2xx **and** parses **and** ≥1 record |
| **ran and actually emitted intel rows** | **413** | the only number that matters to a user |

**The last row is the one this batch added to the process.** Every previous
batch stopped at "verified" — 2xx, parses, ≥1 record — and shipped. Measuring
what the collectors *emit* rather than what the probe *sees* is what turned 741
into 413, and the 328 in between are the reason the fleet already carries 577
sources that have never produced a record.

Between the measuring run and this manifest, the 413 produced **105,866 real
records**. **188 of them carry a verified per-record detail hop.**

## Why 1,000 was the wrong target for this batch

Batch 15 hit ~1,000 because CKAN / Socrata / OpenDataSoft expose a dozen fixed
endpoints per host: find one live portal, get twelve sources. Deep government
registries do not multiply that way. SEC EDGAR, Norway's Brreg, Czech ARES,
gBizINFO and CourtListener are each bespoke — real penetrancy comes one
endpoint at a time. Padding to 1,000 was available (the 328 non-emitters would
have done it) and was not taken.

## Composition of what shipped

| country | n | | category | roughly |
|---|---|---|---|---|
| US | 240 | | corporate / securities | SEC EDGAR deep hops, FDIC, FEC, USAspending awards→subawards |
| EU-level | 46 | | enforcement / safety | EPA ECHO, openFDA, CPSC, MSHA, CMS |
| JP | 26 | | legal | CourtListener, GovInfo, Federal Register, national court APIs |
| DE | 23 | | procurement | TED detail hops, national portals |
| GB | 17 | | research / civilian | OpenAlex, Crossref, ClinicalTrials, GLEIF, PeeringDB, RDAP |
| CH, UA, ES, SK, PL, FR, GR, FI, DK | 55 | | | |

## What broke on the way, and what it taught

**Three bugs of my own, caught by measuring instead of assuming.**

1. **Regeneration silently restored the agents' guesses.** Rebuilding the
   verified manifest by filtering candidate rows on verified ids put the
   *discovering agent's* predicted `kind` back over the *verifier's observed*
   one. OpenAlex was recorded `json:root` when its densest array is
   `json:topics`, so the generated row looked for an array at `root` and emitted
   zero. `promote_candidates.py` exists precisely to overwrite the guess with
   the observation; the fix is to re-promote, never to re-filter.

2. **Detail URLs embedded in descriptions claimed a hop the code cannot
   follow.** `VJSON` fetches exactly one URL — it has no `detail_url` concept.
   A description reading "Second hop: <url>" promised depth the collector does
   not have. It also tripped this repo's own `dup-endpoint` lint check with 12
   phantom collisions, because a bare URL in prose is indistinguishable from a
   fetched endpoint to anything that greps the tree. The hops now live in a
   side-car TSV, which is the input for the hpengine rows that *can* follow
   them.

3. **A genuine engine inconsistency.** `lib/hpengine.c` labels a record with no
   title as `"<record_type> <id>"`; `lib/jsonlist.c` dropped it. Government
   tables are routinely keyed on a code with no human-readable column anywhere —
   EPA Envirofacts `tri_transfer_qty` is `{doc_ctrl_num, transfer_loc_num,
   type_of_waste_management, …}`: real regulatory data, no name, no timestamp,
   so the measurement composer could not fire either. Composing from the
   record's own id is what hpengine already did and is presentation, not
   invention. Emit rate 44% → 53% on a fixed sample.

**Why the remaining 328 do not emit.** Overwhelmingly the verifier's
"densest array of objects" heuristic picking *metadata* rather than records:
`json:query.properties` is a schema description, `json:folders` is an ArcGIS
folder list, `json:boundingbox` is a bounding box. The endpoint is real and the
data is there; the auto-detected path points at the wrong part of it. Each
would need a hand-set array path or a bespoke collector, so they are parked in
`batch16-fetched-but-unparsed.tsv` with their observed kind rather than shipped
as sources that report a clean empty forever.

## Rerunning

```sh
cd native
python3 collectors/gen_sources_batch16.py --specs <specs.json> \
    --out ../docs/candidate-sources-batch16.tsv \
    --exclude-urls <tree urls> --exclude-ids <tree ids>
python3 tools/promote_candidates.py ../docs/candidate-sources-batch16.tsv \
    --verified ../docs/verified-sources-batch16.tsv \
    --rejects  ../docs/rejected-sources-batch16.tsv
# then RUN them and keep only what emits -- do not skip this step
python3 collectors/gen_verified_sources.py ../docs/verified-sources-batch16.tsv \
    --outdir collectors/feed/generated --reserved <ids> --prefix vsrc16
```
