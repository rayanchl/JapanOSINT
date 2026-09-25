# Batch 29 (agent B) — deepweb + govosint

Agent B of batch 29. Ids `JO29B_*`, generated tables `hp3b29b_deepweb.c` / `hp3b29b_govosint.c`,
manifests `docs/candidate-sources-batch29.{deepweb,govosint}.txt`.

Every row was found by live discovery (Socrata catalog API searches, GOV.UK search-API format facets,
direct API probing), fetched by hand while authoring, and then pushed through every gate in CLAUDE.md
against a private build tree (`/home/rayan/jo_b29b`). Record counts in row descriptions are what the
upstream reported at authoring time (Socrata `count(*)`, an API `total`, or the length of a full
document), never estimates.

## Gate results

| gate | tool | result |
| --- | --- | --- |
| 1 probe + filter | `probe_hp_batch.py --check-filter` | 155 authored, 149 PASS first pass; 4 repaired and re-probed 4/4 PASS; 2 dropped; 0 FILTER_IGNORED |
| 2 exclusions | `batch_exclusions.py --bin ./bin/japanosint --check` | 0 collisions, 0 near-misses (id set 16,367 from the binary). The build tree excluded agent A's in-flight `hp3b29a*` tables, so the 144 final rows were also compared directly with agent A's two batch-29 manifests (198 rows): 0 id overlaps, 0 exact endpoint collisions, 0 same-path-different-query matches. The only shared host is `data.winnipeg.ca`, on a different dataset |
| 3 pagination / reachability | `audit_batch_pagination.py`, `audit_batch_reachable.py` | 0 of 144 look paged without paging (12 carry `pagination_ok`); 0 of 144 can never run |
| 4 generate + build | `gen_hp_batch.py --prefix hp3b29b --batch 29` | first pass 153 rows; final regeneration 54 + 90 = 144 rows; `make -j12` exit 0, 0 warnings; 144 `JO29B_` ids in `--list-sources` |
| 5 emit + store | `audit_batch_emit.py` via a per-run-DB wrapper, plus solo re-runs | 153 rows run; 9 dropped (6 unstable paging or overlapping pages, 3 rate-limited walks); shipped rows below. All 144 shipped rows ended with stored = emitted, except two documented cases: +1 for the engine's own truncation notice on 17 pivots at their page ceiling, and −1 for one byte-identical upstream duplicate (Represent). Every scheduled row matched its live upstream total. Seven rows were re-measured on the final binary after repairs: 2 WA walk timeouts, the City Record timeout, and 4 `-notice` renames |
| 6 lint / audit | `make lint-sources`, `make audit-sources` (re-run on the final 144-row build) | lint-sources OK: dup-id 0, unresolved-id 0, registry-orphan 0, quarantine-empty 0, snprintf-guard 0; dup-endpoint 425 and geo-precision 430, both below baseline; 16,511 source_defs registered. audit-sources: 0 findings in 1,579 files, strict `hp*_*.c` set 0 |

### How "stored" was measured (house rule 4b)

`audit_batch_emit.py` reads `emitted N of M` only. It was given a wrapper as `--bin` that runs each
`--run` against its own fresh SQLite DB, so parallel jobs never share a writer, and records two
independent readings per row: the run line's `stored=` field and `count(*)` read back from
`intel_items` for that `source_id`. A row ships only if both equal `emitted`.

## Repairs made during the gates

* **IAPD (SEC adviser search) answered 403 to the engine User-Agent** while answering 200 to the
  same identity without the repository URL token. Both IAPD rows declare
  `header1=User-Agent: JapanOSINT/1.0 (feed collector, contact via repo issues)`. It still names the
  project and never impersonates a browser. BrokerCheck (same vendor, different host) accepts the
  default UA and needed nothing. All three were later dropped anyway, at the emit/store gate, for
  unstable paging (see below). The UA finding is recorded because it applies to any future IAPD row.
* **A `;` inside a header value split the opts field.** The first version of that UA string
  contained `feed collector; contact…`; `manifest.py` rejected it before any C was generated.
  Changed to a comma.
* **FMCSA revocations: Socrata `$q` does not index docket tokens.** `$q=MC1809530` for a docket that
  exists returned `[]`. The row was re-pointed at the exact column filter `docket_number={qU}`, which
  returns the record, and `[]` for an impossible docket.
* **ARIN Whois-RWS returns XML unless asked for JSON.** Added `header1=Accept: application/json`.

## Paging conventions used (and why)

* Socrata: `$select=:*,*&$order=:id` plus `page_param=$offset`. `:id` is the platform row id, so it
  is the identity (`id_keys=:id`) and a stable sort, so pages never shift under the walk.
  Registers up to about 200k rows are scheduled full walks (`$limit=5000`, `page_max` set from the
  measured count). Multi-million-row ledgers are `$q={q}` pivots instead.
* `start` / `skip` / `$skip` offsets carry `page_zero_based=1`. Without it the engine coerces the
  start to 1 and the second page begins one record late.
* Page-number APIs (HowTheyVote `page`, Wellcome `page`) deliberately declare no `page_size`,
  because declaring one switches the engine to offset arithmetic.

## Paging stability: the check that removed six rows

Four are in the table below. Two more, `JO29B_OPENPARLIAMENT_BILLS` and `JO29B_EP_MEP_DECLARATIONS`,
were caught later by the same stored-below-emitted signal and are listed in the rejected-rows table.

A search API that repeats a record on a later page is invisible to the probe and to `emitted N of M`.
The engine really did emit M records. Only the stored count shows it, because repeated ids collapse
at the sink. Each row flagged that way was walked by hand, counting distinct ids across pages:

| row | walk | fetched | distinct | outcome |
| --- | --- | --- | --- | --- |
| SEC IAPD individuals | `query=smith`, 10 x 100, sort=score desc / none / score+source_id | 1000 | 969–971 | dropped |
| SEC IAPD firms | engine run | 1000 | 997 stored | dropped with its sibling (same backend) |
| FINRA BrokerCheck individuals | `query=smith`, 10 x 100, three sorts | 1000 | 967–987 | dropped |
| 360Giving grants_made | GB-GOR-PB188, 10 x 100 via `next` | 1000 | 344 | dropped: `ordering`, `order_by`, `sort` all ineffective, `limit=1000` still 1005/2000, `limit=5000` 504 |
| 360Giving grants_received | GB-CHC-219279 / 207076 / 1089464 | 445 / 393 / 154 | all distinct | kept |
| NZ charity officers (`$skip`) | Smith, 10 x 100, with and without `$orderby` | 1000 | 1000 | kept |
| Tweede Kamer Zaak (`$skip`) | defensie, 10 x 250, with and without `$orderby` | 2500 | 2500 | kept |

Where duplicates appeared, the repeated records were the same person or grant served twice. Nothing
distinct was merged. But a page that repeats a record is also a page that fails to serve one, so the
walk cannot be exhaustive, and those rows were dropped rather than shipped.

## Rejected rows

Listed here rather than in a separate TSV, because this batch was limited to the manifests, the
generated C and this document.

| id | stage | reason |
| --- | --- | --- |
| JO29B_EU_SEDIA_PORTAL_SEARCH | probe | HTTP 405 on GET; the endpoint needs POST, which `probe_hp_batch.py` cannot send, so it cannot be proven by the gate |
| JO29B_LA_CHECKBOOK_SEARCH | probe | timeout: `$q` + `$order=:id` over 6.5M rows did not answer in 120 s; 50 s without the order, beyond the 20 s engine default |
| JO29B_SEC_IAPD_INDIVIDUALS | emit/store | unstable paging (above); emitted 1000, stored 976 |
| JO29B_SEC_IAPD_FIRMS | emit/store | unstable paging; emitted 1000, stored 997 |
| JO29B_FINRA_BROKERCHECK_INDIVIDUALS | emit/store | unstable paging; emitted 1000, stored 967 |
| JO29B_360GIVING_GRANTS_MADE | emit/store | unstable paging; emitted 1000, stored 341 |
| JO29B_DEMOCRACYCLUB_BALLOTS | emit/completeness | rate-limited walk: HTTP 429 at page 5; emitted 400 against an upstream count of 42,411 |
| JO29B_DEMOCRACYCLUB_ELECTIONS | emit/completeness | rate-limited walk: HTTP 429 at page 7; emitted 600 against 4,201 |
| JO29B_DEMOCRACYCLUB_RESULTS | emit/completeness | rate-limited walk: HTTP 429 on page 1 in the batch run, and at page 11 when re-run alone after a 60 s pause; emitted 2,000 against 37,875 |
| JO29B_OPENPARLIAMENT_BILLS | emit/store | unstable paging: all 9 repeats straddle a page boundary (e.g. `/bills/37-3/C-34/` at offsets 493 and 502); `order_by`, `ordering`, `sort`, `order` all ignored; emitted 5,727, stored 5,718 |
| JO29B_EP_MEP_DECLARATIONS | emit/store | unstable paging (2,500 fetched, 2,025 distinct) and a hard API cap (404 past offset 10,000; 404 at 2,500 on a later retry); emitted 9,999, stored 8,583 |

Counts by reason: unstable paging 6, rate-limited walk 3, not provable by the probe 2 (405 on GET, timeout).
155 authored, 11 dropped, 144 shipped.

## Checks beyond `emitted N of M`

`emitted N of M` counts what was *fetched*. A walk that dies mid-way on a 429 or a timeout still
prints `emitted N of N` with no truncation mark, so a clean ratio is not proof of a complete walk.
Two extra checks were run over every row.

1. **Every engine output was scanned for a non-200 status or `transport failure` inside a walk.**
   That found the Democracy Club 429s and the EP 404 (above), plus three timeouts that were
   repaired, not dropped:
   * `JO29B_WA_PDC_LOBBYIST_REPORTS` stopped at offset 55,000 of 201,542. The 5,000-row page there
     is 68.7 MB and took 20.7 s by curl, because `report_data` carries each filing's full JSON. That
     is just past the engine's 20 s default. Now `$limit=1000`, `page_size=1000`, `page_max=215`,
     `timeout_ms=120000`.
   * `JO29B_WA_PDC_F1_FINANCIAL_AFFAIRS`: the 5,000-row page at offset 130,000 is 33.4 MB and took
     18.0 s. Now `$limit=1000`, `page_size=1000`, `page_max=145`, `timeout_ms=120000`.
   * `JO29B_NYC_CITY_RECORD_SEARCH` (pivot): offset 8,000 of a `$q` query took 42.2 s. Now
     `timeout_ms=120000`.

   These two WA rows are the only Socrata full walks not on 5,000-row pages.
2. **Every scheduled row's emitted count was compared with the upstream's live total**
   (Socrata `count(*)`, GOV.UK `total`, `meta.total_count`, a full independent walk for
   OpenParliament, document length for single-document sources, and the `<item>` count for RSS).
   All 113 shipped scheduled rows were compared:
   * **102 matched exactly.**
   * **The 7 German case-law RSS feeds matched their live `<item>` count:** BGH 179, BVerwG 70,
     BFH 30, BAG 19, BSG 26, BPatG 1, BVerfG 17.
   * **The 2 Hawaii candidate ledgers matched on a solo re-run.** The batch audit killed them at its
     900 s limit with no progress, because each walk takes 8–10 minutes. Re-run alone, they emitted and
     stored exactly the upstream count: contributions 136,398 in 503 s, expenditures 165,116 in 615 s.
     The engine has no whole-run timeout, so these are slow scheduled sources, not failures.
   * **The 2 repaired WA PDC rows were re-measured on the rebuilt binary, and both walks now run to
     the end.** `JO29B_WA_PDC_LOBBYIST_REPORTS` emitted and stored 201,548 across 203 pages in
     977 s, against a live `count(*)` of 201,548; before the repair it stopped at 55,000.
     `JO29B_WA_PDC_F1_FINANCIAL_AFFAIRS` emitted and stored 135,858 across 137 pages in 637 s,
     against 135,858; before the repair it was killed by the audit timeout.

   The other 31 shipped rows are entity pivots, so their size depends on the entity asked for and
   there is no fixed total to compare against.

## A reserved suffix: `record_type` must not end in `-notice`

`core/scheduler.c` (`is_notice_record`) treats any record whose `record_type` ends in `-notice` as
the engine's own run notice (`collector-truncation-notice`, `collector-shape-notice`). It counts
those under `notices=`, not `records=`. A source whose records are *about* notices, and were named
that way, therefore reports `records=0` on every run while storing everything. A registry sweep
reading `records=` would call it `EMITS_NOTHING`.

Four rows in this batch were named that way, and their audit run lines showed it:

| row | old `record_type` | audit run line | new `record_type` |
| --- | --- | --- | --- |
| JO29B_GOVUK_NOTICES | `official-notice` | `records=0 … stored=6252 notices=6252` | `official-notice-entry` |
| JO29B_GOVUK_FATALITY_NOTICES | `fatality-notice` | `records=0 … stored=511 notices=511` | `fatality-report` |
| JO29B_CO_PAID_SOLICITOR_NOTICES | `solicitation-notice` | `records=0 … stored=8696 notices=8696` | `solicitation-filing` |
| JO29B_NYC_CITY_RECORD_SEARCH | `official-notice` | `records=0 … stored=10001 notices=10001` | `official-notice-entry` |

After renaming, regenerating and rebuilding, each row was re-run alone on the new binary. Every run
line now counts its records as records:

| row | run line after the rename | DB count |
| --- | --- | --- |
| JO29B_GOVUK_NOTICES | `records=6252 … stored=6252` | 6,252 |
| JO29B_GOVUK_FATALITY_NOTICES | `records=511 … stored=511` | 511 |
| JO29B_CO_PAID_SOLICITOR_NOTICES | `records=8696 … stored=8696` | 8,696 |
| JO29B_NYC_CITY_RECORD_SEARCH | `records=10000 … stored=10001 notices=1` (the one notice is the real truncation notice of a pivot at its page ceiling) | 10,001 |

None of the 144 shipped `record_type` values ends in `-notice`. This was checked in both the manifests
and the generated C.

The same shape is already widespread in the tree, outside this batch and not changed here:
798 `record_type = "*-notice"` declarations across 32 `collectors/pivot/table/*.c` files (for
example `hp3b23_jpmuni.c` 190, `hp3b25_jpmuni2.c` 121, `hp3b25_jppref.c` 91). Every such row
reports `records=0` whatever it stores.

### Emit/store summary for the shipped rows

For the 141 shipped rows whose opts did not change after the batch audit:

| outcome | rows |
| --- | --- |
| stored = emitted | 124 |
| stored = emitted + 1 (the engine's own `collector-truncation-notice` on a pivot that hit its 10-page ceiling) | 16 |
| stored = emitted − 1 (Represent: one byte-identical upstream duplicate, see below) | 1 |
| emitted 0, or stored < emitted for any other reason | 0 |

Across those rows: 2,290,657 records emitted, 2,290,672 stored (read back from each run's own DB).
The three rows repaired afterwards are reported separately above.

The batch audit printed 20 `(TRUNCATED)` runs. Four belong to rows dropped for unstable paging (both
IAPD rows, BrokerCheck, 360Giving grants_made). The 16 on shipped rows are all pivots that hit the
default 10-page ceiling on a broad probe term (`smith`, `verizon`, `robocall`). Each stored the
engine's own `collector-truncation-notice`, which is why those runs show `stored = emitted + 1`. The
bound is disclosed in-band, as house rule 2 requires, and a narrower entity walks to its end.

`JO29B_REPRESENT_REPRESENTATIVES` stores 3,810 of 3,811 on purpose. Upstream serves Charles Fournier
(Edmundston councillor, New Brunswick municipal councils) twice as byte-identical records, so the
sink's collapse is real dedupe and loses nothing distinct.
