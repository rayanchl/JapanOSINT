# Batch 22 — beat `deeparchive` (Source Agent S5): 190 rows

Crawl indexes and web-archive APIs, the Internet Archive item catalogue,
Software Heritage, research-data repositories (Zenodo, OSF, Dryad, twenty
Dataverse installations, eleven NADA microdata catalogues), national-library
and archive catalogues (Europeana, UK National Archives Discovery, KB
Netherlands SRU, National Library of Norway, Library of Congress, Japan Search,
DigitalNZ, Digital Commonwealth, DOAB, HathiTrust), registry snapshot listings
(SEC EDGAR archive directories, the five RIRs' FTP-over-HTTPS listings, the
IANA registrar and root-zone registers, RIPE Database inverse and GRS-mirror
lookups, five RIPEstat calls, thirteen PeeringDB relations) and
scholarly/grey-literature indexes (CORE, OpenCitations v2, OpenAlex, Semantic
Scholar, DataCite, Crossref, CiNii Research, zbMATH, Gutendex, Open Library).

Scope rule, stated in the manifest header: **metadata and infrastructure
only** — no credential content, no per-person lookup path. One row was
authored and then removed on that rule alone (`OPENLIBRARY_AUTHORS_SEARCH`, a
name search for people).

Manifest (the source of truth): `docs/candidate-sources-batch22.deeparchive.txt`.
Generated table: `native/collectors/pivot/table/hp3b22_deeparchive.c`
(190 rows, never hand-edited). Rejects as data:
`docs/rejected-sources-batch22-deeparchive.tsv` (223 rows).

Every row was fetched over the wire from this machine, parsed in its declared
mode, run through the **real binary**, read back as `emitted N of M`, and then
run a second time against a **fresh per-row scratch database** to read
`stored=` off the run line (house rule 4b). Nothing was authored from
documentation.

## Attrition

| stage | in | out | dropped |
| --- | --- | --- | --- |
| candidate hosts pre-screened over the wire (own `urllib` pass, jobs=8 then jobs=2 serial for every timeout/429) | ~420 URLs | 203 rows authored | dead hosts, bot walls, key-gated APIs, empty result sets, hosts already in the tree |
| `probe_hp_batch.py` | 203 | 196 | 5 Common Crawl indexes (NDJSON), Chronicling America (path gone), irr.net (React shell) |
| `batch_exclusions.py` by hand | 196 | 193 | 3 fielded `advancedsearch.php` pivots sharing the existing `q={q}` row's endpoint |
| `audit_batch_emit.py` + rule 4b | 193 | 190 | EuropePMC Grist (page param unreachable), OAPEN (30 s timeout on every engine run), LoC maps (timeout 2 of 3 runs) |

Registry: 13,519 → **13,709** seeds in the built binary (`lint-sources` counts
`REGISTER_SOURCE` only and does not move).

## Gate results (quoted)

| gate | result |
| --- | --- |
| `gen_hp_batch.py --outdir /tmp/gen22d --prefix hp3b22 --batch 22` | `190 manifest rows` → `/tmp/gen22d/hp3b22_deeparchive.c 190 rows`; no duplicate opts, no swallowed `\;` |
| `probe_hp_batch.py --jobs 4` | first pass `# 191/203 PASS`; the 12 failures re-probed serially (`--jobs 1`): 3 recovered (LoC web-archives search, OAPEN, Open Library recent changes — all timeouts under load), 9 hard failures dropped. **190 of 190 shipped rows PASS** |
| `probe_hp_batch.py --check-filter --jobs 3` | `# 191/203 PASS`, **0 `FILTER_IGNORED`** across every `{q}`/`{qd}`/`{qh}` pivot; the three rows it could not reach under load (Wayback availability: `Network is unreachable`; PeeringDB orgs-by-website and CORE data providers: `429`) re-ran serially `# 6/6 PASS` |
| `batch_exclusions.py --check … --bin ./bin/japanosint --skip-prefix hp3b22_` (against the 13,519-id registry, before the table was installed) | `collisions: 31 near-misses needing a human read: 6`. All 31 read by hand: 23 are `DUP-ENDPOINT-COMPOSED` on `osint_extras.c`, which the scanner matched because the word **`date`** in my `fl[]=date` field is one of that collector's "dataset names" — its endpoint is the keyword search `advancedsearch.php?q=%s`, mine are per-collection feeds; 8 are `cyi_ripe_whois_search.c`, matched on the attribute words `origin`/`source` — that collector does a forward `query-string=AS…` lookup in the RIPE source, mine are inverse route-by-origin lookups and aut-num reads from the APNIC/ARIN/LACNIC/AFRINIC/RADB mirrors. The 6 near-misses: `bgp_lookup.c` composes `stat.ripe.net/data/%s/` but uses none of `address-space-usage`, `geoloc`, `ris-peerings`, `atlas-targets`, `rrc-info` (grepped); `sec_edgar.c` builds accession folders `edgar/data/%s/%s`, not the CIK `index.json`. The three IA fielded pivots the scanner did NOT flag were removed anyway (same endpoint and token position as the registered `advancedsearch.php?q={q}` row) |
| `audit_batch_pagination.py` | `0 of 190 rows declare no pagination but look paged (38 declared pagination_ok)` |
| `audit_batch_reachable.py` | `0 of 190 rows can never run` (first pass flagged the two SWH rows because the auditor does not know `{Q}`; switched to `{q}` after confirming the percent-encoded origin URL answers 200) |
| `audit_batch_emit.py --jobs 4 --timeout 300` | `EMPTY_UPSTREAM=1 HTTP_429=12 OK=180 — records emitted across the run: 185,602`, 0 `DROPS_EVERYTHING`, 0 `SLOW`. The `EMPTY_UPSTREAM` was my probe entity (`%22dark%20web%22` re-encoded by the entity path); changed to `darknet` → `OK 1000`. The 12 `429`s (7 Zenodo topic rows at 40 pages each, 4 PeeringDB, 1 CORE) re-ran serially: 4 recovered; the rest re-ran **spaced 75 s apart** and every one then emitted (Zenodo 34–750 each, PeeringDB 224/224 and 133/133, CORE 300/300). Zenodo's anonymous limit is per minute, not a refusal |
| **rule 4b** (`JO_DB=<fresh>` per row, `stored=` read off the run line) | 190 of 190 rows measured. **192,513 records emitted (including one `collector-truncation-notice` per page-ceiling row), 192,182 stored.** 13 rows show `UID-COLLISION`, all analysed below; none is an identity defect |
| `make` | 0 warnings |
| `make audit-sources` | `findings : 0` / `strict set (collectors/pivot/table/hp*_*.c): 0 finding(s)` |
| `make lint-sources` | `lint-sources: OK` |
| `make hptest` | `all passed` |
| `make unit` | `test_search_degradation: OK` (all tests pass) |

## Rule 4b — the 13 collision rows

Every one is the page-walk re-reading records it already wrote: newest-first
feeds shift under the walker, and two catalogues (TNA Discovery, Japan Search)
return overlapping windows between pages. The engine's collision guard keeps
any record whose CONTENT differs and collapses only byte-identical repeats, so
the gap is dedupe, not loss.

| row | emitted | stored | reading |
| --- | --- | --- | --- |
| `TNA_DISCOVERY_RECORD_SEARCH_PIVOT` | 1001 | 856 | page 1 holds 100 distinct ids (checked); later pages overlap page 1 |
| `NB_NO_LATEST_BOOKS` | 1001 | 939 | `sort=date,desc` over a live legal-deposit stream |
| `JAPAN_SEARCH_KEYWORD_PIVOT` | 1001 | 962 | overlapping `from=` windows |
| `CROSSREF_WORKS_BY_ROR_PIVOT` | 1001 | 975 | unsorted works walk (the sorted rows lose ≤1) |
| `NB_NO_LATEST_PERIODICALS` | 1001 | 980 | as books |
| `ARQUIVO_PT_TEXTSEARCH` | 1001 | 988 | offset walk over a relevance-ranked result |
| `DATACITE_DOIS_BY_PREFIX_PIVOT` | 1001 | 994 | newest-first stream |
| `IA_COLLECTION_ARCHIVETEAM`, `IA_COLLECTION_TVNEWS` | 5001 | 4995 | ten 500-row pages sorted `publicdate desc` while items land |
| `KB_NL_SRU_GGC_PIVOT` | 501 | 498 | SRU `startRecord` windows |
| `DIGITALNZ_RECORD_SEARCH_PIVOT`, `CROSSREF_WORKS_BY_MEMBER_PIVOT`, `ZENODO_TOPIC_TELEGRAM` | 1001 / 1001 / 750 | 1000 / 1000 / 749 | one repeat each |

Two collisions WERE defects and were fixed or removed before shipping:

* `DOAB_SEARCH_PIVOT` with `expand=metadata`: 907 emitted, 597 stored, and the
  stored uids were `dc.contributor.author`, `dc.date.issued`… — the engine's
  densest-array auto-detection had picked each item's metadata key/value list
  as the record set. Fixed by dropping `expand=metadata` (the root array is
  then the only array): **501 of 501 stored**.
* `EUROPEPMC_GRANTS_SEARCH_PIVOT`: 251 emitted, 27 stored. The Grist API takes
  its parameters in the path (`rest/get/query=…&format=json`), so the engine's
  appended `?page=2` was ignored and ten pages returned the same 25 grants.
  `page=2` does work when hand-built into the path (verified: different grant
  ids), but the engine cannot express it, and `rest/get?query=` is not JSON.
  Removed rather than shipped as a silent first-page slice.

## Largest results

`RIPESTAT_ATLAS_TARGETS_PIVOT` 7,720 of 7,720 (every Atlas measurement
against 193.0.0.0/21); sixteen Internet Archive collection feeds 5,000 each at
the page ceiling; `IANA_REGISTRAR_IDS_CSV` 4,202; `RIR_LISTING_LACNIC_IRR`
2,441 dated IRR dumps; `SEC_EDGAR_CIK_DIRECTORY_PIVOT` 2,239 accession folders
for CIK 320193; `FIGSHARE_CATEGORIES` 2,180; `OPENCITATIONS_V2_CITATIONS_BY_DOI_PIVOT`
1,806; `IANA_ROOT_ZONE_DATABASE` 1,595 TLD rows; 46 rows at 1,000 (the ten-page
ceiling, each carrying its truncation notice).

## Things worth knowing that the tools did not say

* **Common Crawl's index server answers `output=json` as NDJSON** (one object
  per line). The prober reports `UNPARSEABLE` and `lib/hpengine.c` has no
  NDJSON path, so no per-crawl CDX row can ship. The tree's existing
  `COMMONCRAWL_INDEX` row (hp_netintel_deep.c) declares the same shape and is
  worth an emit check. The server also answered 503/504/refused for 52 of 57
  crawls under `jobs=8` and still 504'd serially for the 2025 crawls.
* **figshare's `GET /v2/articles?search_for=` silently ignores the filter** —
  `example`, `dark web` and `osint` all returned the identical 113,011-byte
  page. The honouring form is `POST /articles/search`, which the prober cannot
  exercise. Research Square's `/api/search` ignores its query the same way. Both
  rejected `FILTER_IGNORED` at pre-screen; neither reached the manifest.
* `audit_batch_emit.py` derives the pivot entity by diffing probe against
  template and passes it raw, so a probe URL whose entity is percent-encoded
  (`%22dark%20web%22`) reaches the engine double-encoded and comes back
  `EMPTY_UPSTREAM`. Use a bare word as the probe entity.
* The `{Q}` raw token is dispatchable in `hp_uses_entity()` but
  `audit_batch_reachable.py` does not recognise it and reports the row as
  never-runs. `{q}` percent-encodes a full origin URL and Software Heritage
  accepts it, so `{Q}` was not needed.
* `xargs -L 1` treats a line ending in a tab (a static row with an empty entity
  column) as continued onto the next line — the first 4b sweep silently
  measured 84 of 193 rows. Re-driven from Python.
* Rate limits that look like refusals: Zenodo (per-minute, 40-page rows),
  PeeringDB (anonymous), CORE, Semantic Scholar snippet search (429 on the very
  first request, twice — that one IS a refusal and was rejected), KAKEN (403
  "Exceeds allowed rate" on request one — rejected).
* Hosts that refuse every self-identifying UA and were left out rather than
  spoofed (httpclient.h): Gallica SRU (403 "Access Interdit"), Finna, UK
  Government Web Archive (Human Verification), perma.cc and Library of Congress
  web archive (Cloudflare), archive.today (429 + bot wall), Trove/DPLA/NARA/
  Archives Portal Europe (key-gated, out of scope).

## Not verified

* `WAYBACK_AVAILABILITY_PIVOT`, `SWH_ARCHIVE_COUNTERS`, `RIPESTAT_GEOLOC_PIVOT`,
  `RIPESTAT_ADDRESS_SPACE_USAGE_PIVOT`, `LOC_FREE_TO_USE_SETS` and the four GRS
  aut-num rows emit a single record by nature (one document per entity); they
  pass every gate but a one-record source cannot show a collision defect.
* `LOC_FILM_LATEST` and `LOC_NOTATED_MUSIC_LATEST` emitted on 2 of 3 runs; the
  failing run was a 30 s transport timeout on a ~1 MB page. Kept because they
  degrade to an explicit run error, never to a wrong answer; `LOC_MAPS_LATEST`
  (1 of 3) was dropped.
* `batch_exclusions.py` cannot be re-run meaningfully after the table is
  installed (the binary's id set then contains the batch and every row
  "collides" with itself — 219). The 0-collision reading above is the
  pre-install run against the 13,519-id registry.
