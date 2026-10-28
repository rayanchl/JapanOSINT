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

## The long tail: 62 single-host rows, asked the same question

No family covers these, so each was asked the one question that is cheap and
mechanical: send the row's own URL with a DELIBERATELY BOGUS sort value under
nine candidate parameter names, and read what comes back. **29 answered that
they parse such a parameter, 28 answered the baseline byte for byte, and 5
were unreachable.** Per-row readings: `docs/measured-seam-tail-2026-10-24.tsv`.

**The error body is the part worth reading.** The probe established only that
the server READ the parameter; the body it answered names the field it will
sort on, which is the one thing a per-row guess cannot supply. Three APIs
handed over their whole allowed set — DERIB
(`must match "^-?(|default|date|temporalCoverageFrom|legislationIdentifier|
DATUM)$"`), USAspending (`Field 'sort' is outside valid values ['name',
'obligated_amount', 'gross_outlay_amount']`) and Misskey (an enum validation
error) — and the OData services each answered "could not find a property named
… on type …", which doubles as a schema check.

**19 rows carry a measured sort now**, each verified accepted, honoured (ASC
and DESC answer different records), total-preserving, and disjoint across two
real runs against one database:

| family | rows | parameter |
| --- | --- | --- |
| OData (WHO xMart, Traficom, SK RPVS, BR BCB, SYKE, MSRC) | 12 | `$orderby=<the entity's own key>` |
| GOV.UK Search | 1 | `order=public_timestamp` |
| GeoNetwork (AODN) | 1 | `sortBy=_id` |
| CKAN group listing (Miyazaki, HTML) | 1 | `sort=metadata_created asc` |
| Socrata discovery (ACT) | 1 | `order=createdAt` + the domain scope below |
| Misskey federation | 1 | `sort=+firstRetrievedAt` |
| DERIB legislation | 1 | `sort=legislationIdentifier` |
| USAspending | 1 | `sort=name&order=asc` |

Three readings from that set generalise:

* **The row's own `id_keys` is usually the sort key**, and asking for it twice
  checks the row as well as the order. `$orderby=_RecordID` on WHO's FluNet
  answered *"Could not find a property named '_RecordID' on type
  'FLUMART.VIW_FNT'"* — the field the row had been keying every record on since
  it shipped. The engine was already saying so: the run files a
  `shape:id-keys-unmatched` notice and content-hashes each uid, so a week WHO
  later revises is stored as a NEW row instead of updating the one it corrects.
  Identity measured on 1,000 mid-collection records: `ISO2+ISOYW` is 795
  distinct of 1,000 (three origin sources report the same country-week),
  `ISO2+ISOYW+ORIGIN_SOURCE` is 1,000. **A notice nobody reads is still true,
  for the second time in three days** — and a sweep of that one notice across
  the registry is the obvious follow-up this did not do.
* **Prefer a key whose ties are impossible over a key whose ties reshuffle.**
  MSRC's advisories sort by `releaseDate` or by `id`, not both (the composite
  is refused). Hundreds share a release date and the order inside a tie is
  undefined, so a tie block can reorder under a resumed walk and lose a record
  for good; a guid order is total, and an advisory inserted mid-collection is
  simply read on the next cycle. `id` it is.
* **A measurement is not a code change.** The applier appended `&sort=…` to
  the one row whose URL had no query string at all, so Misskey's row asked for
  `/instances&sort=+firstRetrievedAt` — one record, 135 bytes, `rc=0`. Only
  the two-run engine reading caught it. A sweep of every URL literal in the
  tree for a `&` preceding any `?` found nine more and all nine are correct:
  five JMA portal links with `#fragment` parameters, an open-meteo docs link,
  and two N2YO calls whose API genuinely documents `&apiKey=` appended to the
  path.

**12 rows were measured and deliberately not changed**: GLEIF ×3 (the API
refuses `page[number]=51`, so these are bounded at 10,000 whatever the order —
France alone declares 193,375 — and a stable sort would merely freeze which
10,000 are read; the fix is a narrower filter per request), UK Land Registry's
PPI (`_sort` answers nothing in 35 s and an internal-error page at 95 s against
a sub-second unsorted page), TronScan (only `balance` is accepted, the most
volatile field on the record, and the ranking is the row's subject), the two
Mastodon directories (`order` takes only `active` or `new`), ENTSOG
(`orderBy` parsed — a bogus value draws a 500 — and not honoured), the two
PNCP rows, and the two WordPress.org registries, whose `request[browse]`
offers rankings and a descending recency but no ascending key.

**Two of the five "unreachable" rows were the probe's own fault**, which is
worth more than the two rows: `curl` reads the brackets in
`request[per_page]` as a glob range without `-g`, so both WordPress.org rows
came back as transport failures on a URL that answers 200 with 76,587 plugins
over 766 pages. The remaining three (IODA's unverifiable certificate chain, an
empty reply, a reset) are readings of this path, not of the rows.

## A rule-4d defect found while probing: 40 Socrata rows answering about every portal

`AP19_AU_ACT_SOCRATA`'s probe answered in Spanish. A Socrata domain's own
`/api/catalog/v1` is **not** scoped to that domain — the batch-34 finding —
and 40 rows named after one domain passed no `domains=`/`search_context=`.
Measured both ways on every one of them
(`docs/measured-socrata-scope-2026-10-24.tsv`): unscoped, each answers
`resultSetSize` 10000 and 39 of the 40 answer another portal's dataset first
(Dallas, for 38 of them); scoped, each answers its own catalogue — Honolulu
453, ACT 784, NYC 3,022, Maryland 3,606. Every record those rows stored was
attributed to the domain in the row's name, whoever owned it, with every gate
green: that is rule 4d exactly, and no record count could see it.

All 40 now carry `domains=`, `search_context=` and `order=createdAt` (the
order the cross-portal rows in `vsrc14_us_state_3.c` already carry, with its
own measurement). The cross-portal rows on `api.us.socrata.com` are
deliberately unscoped and were left alone — they claim no domain.

**And `chronicdata.cdc.gov` is now an alias of `data.cdc.gov`.** Scoped to
itself it answers 1,478 datasets whose owning domain is `data.cdc.gov`, with
the same ids as `data.cdc.gov` at offset 0 and at offset 1000, and
`/api/views/metadata/v1` answers identically on both hosts. So the five
chronicdata rows in the tree collect what the data.cdc.gov rows collect. They
are left registered and recorded here rather than removed: the data is
identical, the cost is duplicate requests and duplicate rows under a second
`source_id`, and the removal is a registry-count decision for the repository
owner.

## Still unexamined

The 166 `OK_NODIR` rows below, and nothing else from the 273: every family and
every single-host row above has a reading now.

The 166 `OK_NODIR` rows — an order parameter with no direction — are a separate
and smaller question: most APIs default such a sort to ascending, but "most"
is not a measurement either.

## One thing the Socrata scope fix does not fix

Those 40 are VJSON rows (except ACT's), and `page_resume` is an hpengine
field, so they cannot read a catalogue over a cycle: two runs against one
database store the same 200 twice, which is correct for a non-cycling row and
is what the truncation notice says. Scoped, 32 of the 40 hold fewer datasets
than one run reads and are now complete. **Eight do not** — healthdata.gov
10,000+, www.dati.lombardia.it 5,073, opendata.maryland.gov 3,606,
data.cityofnewyork.us 3,022, data.austintexas.gov 2,285, data.wa.gov 2,178,
data.colorado.gov 2,109, data.cityofchicago.org 2,036 — and under
`order=createdAt` they read the oldest-created 2,000 each run. That is a
coherent, disclosed read where the unsorted walk was an incoherent one (the
batch-15 comment measured 2 of 100 records on page 2 repeating, with as many
skipped), but it is still a bound over one end of the collection. Moving those
eight onto an hpengine table with `page_resume = 1` is the fix, and it is not
done here.

## Two unrelated items measured the same day: SEC and LegCo

**SEC's filter is about a contact mailbox, and the agent lived in nine
places.** Bisected 2026-10-24 against `www.sec.gov/files/company_tickers.json`
and `data.sec.gov/api/xbrl/frames/…`: the engine's own agent — which names the
product and gives the repository — is **403 on both** ("your request
originates from an undeclared automated tool"), while every agent carrying an
`@` address answered 200. So this is not a token the filter dislikes (INE's
"collector", HDX's "OSINT"): it is the absence of a mailbox, which SEC's
automated-access policy asks every client to declare.

Before this the agent was in nine places — `#define SEC_USER_AGENT` in
`collectors/sources/sec_edgar.c`, `#define SEC_DEFAULT_UA` in
`sec_filing_diff.c`, and a `.headers` User-Agent on each of seven rows — and
the two collectors announced **`contact@example.com`**, which passes the filter
and is not a contact route. It is now one line per host in
`core/host_agents.def` (`HA_SEC`, carrying the repository owner's address by
their decision), and `$SEC_CONTACT_UA` replaces it for a deployment that is not
this one. That variable used to be read inside `sec_filing_diff.c`, where the
per-host table would have overruled it — an operator knob a later change
silently defeated, the shape `$JO_PAGE_MAX` was in until 2026-10-22 — so it is
resolved in exactly one function now (`host_agent_ua()`), which the table dump,
the override accessor, the request builder and `probe_hp_batch.py` all read.

Measured after, all nine SEC sources, each against a scratch database:
`SEC_EDGAR_FRAMES` 2,679 records, `SEC_EDGAR_CIK_DIRECTORY_PIVOT` 2,270,
`SEC_EDGAR_DAILY_INDEX_CURRENT_QTR_LISTING` 320, `SEC_COMPANY_FACTS` 338 (for
CIK 320193), `SEC_EDGAR_SEARCH` 1,260, `SEC_EDGAR_FULL_INDEX_LISTING` 53,
`SEC_EDGAR_DAILY_INDEX_LISTING` 37, `SEC_EDGAR_FEED_LISTING` 32,
`SEC_EDGAR_OLDLOADS_LISTING` 31, `SEC_RISK_FACTOR_DIFF` 1 (a diff is one
record), and `SEC_EDGAR_FRAMES` again with `$SEC_CONTACT_UA` set, unchanged at
2,679.

**LegCo is not unreachable — its HTTPS is, from this container.** (Written
before the merge of 2026-10-25, which moved these three feeds into
`collectors/pivot/table/hp3_legco.c` and reads them WHOLE — bills 6,884 in 69
pages a day, votes 542,997 on an `id gt` keyset, speeches 551,619 by `$skip` —
and dropped `gov-hk-legco-bills`, which read the bills feed a second time at 25
a page. The scheme reading below stands, and is why those URLs stay on plain
`http`.) All four rows as they then were are `http://app.legco.gov.hk` OData, and
they answer: `gov-hk-legco-bills` 500 records, `eas-hk-legco-bills` 2,000,
`eas-hk-legco-votes` 2,000, `eas-hk-legco-speeches` 100 of one page in 13.3 s
(each measured 2026-10-24 through the engine). The speeches row is simply
SLOW, not broken: 13 s a page puts a full 20-page run past four minutes, which
is why it did not finish inside a 560 s budget on the first attempt and why
the 2026-08-24 health sweep had to put it in its own "slow-400s" pass.
What fails is `https`: `curl` to `app.legco.gov.hk:443` and
`www.legco.gov.hk:443` ends in a TLS `unknown CA` alert under the CCR bundle
AND under the system bundle, while `http://www.legco.gov.hk/` 307s to that
same unreachable `https`. The agent proxy reports `bundleCoversEveryHost:
true` and lists no relay failure for the host, so this is a reading of this
path, not of the source.

**Controlled the next day** (2026-10-25, prompted by the PR #51 session's
note that TLS terminates at the egress gateway in these containers): `openssl
s_client`, which does not use the proxy, verifies github.com, api.gleif.org AND
app.legco.gov.hk — the last as `CN = *.legco.gov.hk`, `Verify return code: 0
(ok)` — while `curl` through the proxy answers 200 for gleif, 400 for github's
bare root and 000 for LegCo. So the publisher's chain is sound and the proxy is
not broken in general: this path mishandles that one host. Two controls settle
such a reading, never one — a host known to work through the proxy, and the
publisher read directly.

One useful corollary: `api.data.gov.hk` IS reachable over https from here (it
answers `{"message":"REQUEST ERROR: start parameter missing"}` to a malformed
historical-archive request), so Hong Kong's own portal is a path that exists if
LegCo's plain-http endpoints ever stop. Nothing is registered on it here —
that would need the usual probe-and-emit measurement first.

The operational point: **those four URLs are on plain `http` deliberately, and
"upgrading" them to `https` would take all four rows dark.** A reader who sees
`http://` in a 2026 collector will want to fix it; the measurement above is why
not, and it should be re-taken from an environment with ordinary egress before
anyone does.

## The id_keys sweep this file asked for, done — 538 rows, 5 real hits

The FluNet finding above named its own follow-up: a sweep for rows whose
`id_keys` names a field the upstream does not publish. It is checkable without
the engine — fetch one page, flatten the first record, look up each key token —
so all 538 cycling JSON rows that declare an `id_keys` were measured
(`docs/measured-idkeys-2026-10-24.tsv`): **494 OK, 5 ID_KEYS_UNMATCHED**, 21
unmeasured (hosts that reset this path, the ODS set among them), 15 unparsed
(POST-only rows a GET probe cannot ask), 3 with no record array.

**The first run of that sweep said 35, and 30 of them were my checker's fault.**
`IMF_PORTWATCH_AIRPORTS` was flagged for keying on `iata_code` when the ArcGIS
feature nests every field under `attributes` — and the engine stores uids
`RFN`, `YOA`, `MVA`, real IATA codes, filing no notice at all. The reason is
`hp_flat_get()`: an exact flat-key miss falls back to **any key whose LAST
dotted segment matches**, case-insensitively, skipping a leading `@`. So
`iata_code` resolves against `attributes.iata_code`, and a checker without that
rule indicts most of the ArcGIS and GeoJSON fleet. Both readings are kept
(`idkeys_check.prefix-only.tsv` in the scratch set) rather than the first
quietly replaced. Third time in this session that a new check's first number
needed verifying before it needed acting on.

Note what the fallback canNOT save: a key token that itself contains a dot
(`municipality.demarcation_code`) only ever matches an exact flat key, because
the fallback compares the whole token against one segment.

The five real hits, and what the measurement said:

| row | was | is | measured |
| --- | --- | --- | --- |
| `ZA_MUNIMONEY_{INCOME_EXPENDITURE,CAPITAL,CASHFLOW,AGED_DEBTORS}` | `municipality.demarcation_code` | the cube's own dimension tuple | no v2 cube publishes that field; the engine said so per run (`id_keys … matched 0 of 100 record(s)`) and content-hashed every uid, so a figure National Treasury restates landed as a NEW row. Each cube's dimensions were read from the cube: the full tuple is 1,000 distinct of 1,000 facts |
| `SI_GURS_HOUSE_NUMBERS` | `properties.HS_MID` | `properties.EID_HISNA_STEVILKA` | that layer publishes FEATUREID, EID_HISNA_STEVILKA, HS_STEVILKA, HS_DODATEK, EID_STAVBA, EID_NASELJE, EID_ULICA, TIP_TABLICE, EID_POSTNI_OKOLIS, DATUM_SYS, ST_HS. The EID is what the feature's own `id` is built from and what this row already sorts on, so identity and order now agree |

**A page can hide the dimension that matters.** A minimal-distinct-key search
over one Municipal Money page returns a tuple WITHOUT `demarcation.code`,
because a page covers one municipality. The key has to be the cube's
dimensions, not the smallest tuple that happens to separate one page.

And two rows the sweep passed were still wrong, which marks the sweep's limit:
it asks whether a field EXISTS, not whether it is unique.
`ZA_MUNIMONEY_AUDIT_OPINIONS` keyed on `demarcation.code` (one row per
municipality per year, so every year but one was being overwritten) and
`ZA_MUNIMONEY_OFFICIALS` on `municipality.demarcation_code` (one official per
role per municipality). Both are measured composites now —
`+financial_year_end.year` and `+role.role`, each distinct over 1,000 facts.
Uniqueness is the registry sweep's COLLISION verdict, and these two would
never have shown it while their cubes' pages each covered one municipality.
