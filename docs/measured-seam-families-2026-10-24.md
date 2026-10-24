# Seam risk by API family — what was measured, fixed and refused (2026-10-24)

`docs/measured-seam-risk-2026-10-22.tsv` ranked all 665 cycling rows and found
**273 that page by offset or page number with no order parameter at all**. A
cycle only lands where it meant to if the collection's order is stable between
runs; if the upstream reorders, a record crosses the page boundary the other
way and is never stored. The visible half is a small collision, the invisible
half is the loss.

This file is the follow-through: one verdict per API family, each from a live
measurement, including the families that were deliberately **not** changed.
Per-row readings are in `docs/measured-arcgis-oid-2026-10-24.tsv`.

## Fixed — 99 rows

| family | rows | parameter | how it was verified |
| --- | --- | --- | --- |
| ArcGIS FeatureServer/MapServer | 47 of 50 | `orderByFields=<the layer's own objectIdField> ASC` | the layer metadata names the field, then two single-record pages (offsets 0 and 1) answer different records. Across the 50 the field is `OBJECTID` 40 times but also `ObjectId` (4), `objectid` (2), `FID` and `oid_permit` — assuming one name would have broken eight rows. 48 of 50 declare `supportsPagination`. Two runs on `IMF_PORTWATCH_AIRPORTS`: 2,000 + 2,000 = **4,000 distinct** |
| CKAN `datastore_search` | 21 | `sort=_id asc` | `_id` is the datastore's own sequential row id, in every resource. data.zagreb.hr: offset 0 → `_id` 1,2,3; offset 3 → 4,5,6. Two runs on `HR_ZAGREB_STREETS`: 200 + 200 = **400 distinct** |
| IETF datatracker (Tastypie) | 14 | `order_by=id` | five endpoints checked (relateddocument, person, reviewrequest, iprevent, session): offsets 0 and 2 answer ids 1,2 then 3,4, and `meta.total_count` is published. Two runs on `IETF_DT_PERSON`: 200 + 200 = **400 distinct** |
| Socrata (SODA) | 13 | `$order=:id` | SODA guarantees no order without `$order`; `:id` is Socrata's own row id, so no per-dataset knowledge is needed. data.cdc.gov: accepted percent-encoded, stable over two reads, offsets 0 and 3 share no row, **and DESC answers a different set** — so the parameter is honoured, not silently ignored. Two runs on `CDC_EXCESS_DEATHS_BY_WEEK`: 2,000 + 2,000 = **4,000 distinct**. `HHS_HOSPITAL_CAPACITY_FACILITY` got `$order=hospital_pk,collection_week` instead — the pair its own `composite_id` is built from |
| CKAN `current_package_list_with_resources` | 3 of 4 | re-pointed to `package_search?sort=id+asc&rows=100` | that action takes no sort parameter and orders by `metadata_modified desc`, the loss batch 34 measured. Each now stores its portal's declared count exactly: Armenia **823 / 823**, Hong Kong **797 / 797**, Izmir **250 / 250** |
| WFS 2.0 (Slovenia house numbers) | 1 | `sortBy=EID_HISNA_STEVILKA` | 580,322 features by its own `numberMatched`; startIndex 0/2/4 answer strictly ascending, disjoint pairs |

Two things the re-points taught, both caught only because the rows were run:

* **A URL changed after validation is unvalidated, and its siblings move with
  it.** The CKAN re-point stored ONE record on all three rows until
  `array_path` went from `result` to `result.results` and `page_param` from
  `offset` to `start` — the bulk action and `package_search` differ in both.
* **A notice nobody reads is still true.** The Hong Kong row's
  `title_keys = "author"` matched nothing — `author` is present on every
  package and null on every one (797 seen, 0 matched) — so every dataset was
  titled from the fallback list while the shape notice asked for exactly this
  repair, on every run since the row shipped. `title,name` now.

## Measured and deliberately NOT changed — 54 rows

| family | rows | why not |
| --- | --- | --- |
| **UK Parliament Linked Data API** | 14 | **sorting degrades the record.** `bills.json?_sort=identifier` answers page 0 as bare URI strings (`http://data.parliament.uk/resources/0`) where the unsorted page answers full bill objects with billType, date, sponsors and title. A sort here would turn records into hyperlinks — the "read what it stores" rule. No stable order is available without that cost |
| **GBIF occurrence search** | 11 | the API exposes no sort parameter at all; `limit`/`offset` walk an index order GBIF does not promise to hold. Its own `count` (803,619 for Cameroon alone) is published, so the shortfall is at least disclosed |
| **FPDS ezSearch ATOM** | 16 | an append-at-head contract-award feed with no sort parameter. The honest options are to stop cycling it (making it a recency row) or to accept the drift; both change what the row collects, so this is a decision to take deliberately rather than a parameter to add |
| **OpenDataSoft** | 9 | 7 of the 9 hosts do not answer this container (ANFR 503, Melbourne/KAPSARC/Qatar connection reset), and the v1 and v2.1 sort parameters both need a per-dataset field, which needs a reachable host. Unmeasurable from here rather than fixed on inference |
| **SA Treasury cubes** | 7 | `order=<field>:asc` is accepted (and a malformed spec returns zero rows, so it is parsed), but the host stopped answering mid-probe, so whether it is HONOURED was never established. One undecided reading is not grounds to change seven rows |
| **HU INSPIRE WFS** | 2 | GeoServer accepts `sortBy=name` and that server does allow `startIndex` without a sort — but `name` has ties (two features named "1. övcsatorna"), so it is not a total order, and no unique property is exposed. The feature id is stable; WFS `sortBy` cannot address it |
| ArcGIS, unverified | 3 | CH_BAKOM ×2 answer no features at offset 1; CR_HEREDIA_PATENTES answers this container nothing |
| CKAN bulk, unverified | 1 | `data.ibb.gov.tr` (Istanbul) does not answer at all today, so a re-point has nothing to verify against |

## Four more families probed the same day — all four refuse a stable order

| family | rows | reading |
| --- | --- | --- |
| `api.uktradeinfo.com` (OData v4) | 4 | **`$orderby` is refused with HTTP 403**: "use only the allowed operations as described in the API documentation". The plain row URL answers 200. So an OData service does not necessarily allow OData's own sort |
| `repository.overheid.nl` (SRU 2.0) | 4 | **`sortKeys` is silently ignored.** `sortKeys=identifier,,1` and `,,0` (ascending and descending) answer the SAME two records, and a bogus sort key — `zzznotafield` — draws no SRU diagnostic at all. Rule 4d's shape, in a sort parameter |
| `environment.data.gov.uk` flood-monitoring | 8 | **`_sort` is accepted and ignored**: `_sort=notation` and `_sort=-notation` answer the same records. The records stay whole (unlike Parliament's), so this is a dead parameter rather than a harmful one. The `asset-management` endpoint among these rows is the Parliament-style Linked Data API and has NOT been checked for the record-degradation hazard — do that before sorting it |
| `www.thegazette.co.uk` | 5 | `sort=publication-date` is ignored — and `sort-by=id` **changed the result set**, from 9,321,677 notices to 4,882,776 with zero entries returned. An unrecognised parameter on this API filters rather than erroring |

**That last one is the generalisable warning: an unrecognised parameter is not
always ignored.** On most of the APIs here a bad sort key is dropped; on The
Gazette it silently narrowed the corpus by 4.4 million notices. So when a sort
is added to a row, compare the upstream's own total before and after — if the
total moves, the parameter was not read as a sort.

## Still unexamined

~60 single-host rows, listed with their host and URL in
`docs/measured-seam-risk-2026-10-22.tsv`. Nothing about them is claimed here.

The 166 `OK_NODIR` rows — an order parameter with no direction — are a separate
and smaller question: most APIs default such a sort to ascending, but "most"
is not a measurement either.
