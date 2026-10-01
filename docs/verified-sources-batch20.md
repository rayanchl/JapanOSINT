# Batch 20 — 112 rows across five under-covered beats

Every row below was fetched over the wire on this machine, parsed in its
declared mode, run through the **real binary**, and read back as
`emitted N of M available`. Nothing here was authored from documentation.

Manifests (the source of truth) —
`docs/candidate-sources-batch20.{airspace,parliament,scholarly,standards,tradestat}.txt`.
Generated tables — `native/collectors/pivot/table/hp3b20_*.c`. Edit the manifest.
Rejects are kept as data in `docs/rejected-sources-batch20.tsv` (146 rows).

## What shipped

| beat | rows | what it is | why it was under-covered |
| --- | --- | --- | --- |
| `airspace` | 41 | the **pending** side of the FAA's 28-day NASR cycle — the next chart cycle's airspace, routes, navaids, frequencies and airport surfaces as they *will* be — plus the six current-cycle layers the tree did not already carry, plus two OurAirports reference tables | the tree already registers the FAA's *current* layers three times over. Nobody had taken the `Pending_*` family, which is the same register published weeks ahead of its effective date. Diffed against the current layer, each pending row is forward notice of a change: a restricted area being created, a VOR being decommissioned, a runway closing, a new runway-incursion hot spot |
| `parliament` | 18 | UK Parliament linked data (16 endpoints), the Riksdag chamber speech register, the Canadian House of Commons member export | `lda.data.parliament.uk` and `www.ourcommons.ca` had **zero** endpoints in the tree. The Nordic, Irish, Swiss, Brazilian and European Parliament APIs were already well covered, so the gap was specifically the UK's linked-data platform — where divisions, early day motions, written questions, election results and Library research briefings actually live |
| `scholarly` | 26 | SciELO's Ibero-American national collections (13 rows), Europe PMC's book stream and citation graph, INSPIRE seminars and datasets, PLOS's Solr index, dblp author and venue indexes, OpenAIRE software | Crossref (26 endpoints), OpenAlex (42), DataCite (25), OSF (28) and Zenodo (11) are saturated. `articlemeta.scielo.org` and `api.plos.org` had **zero**. The gap was non-Anglophone and discipline-specific publishing |
| `standards` | 10 | the BIPM key comparison database, the RFC Editor's errata register and RFC index, four IETF datatracker process tables | `www.bipm.org` had **zero** — the world's measurement-traceability register was not in the tree at all. `www.rfc-editor.org` had one RSS feed and neither the errata register nor the index |
| `tradestat` | 17 | HMRC uktradeinfo (10 OData entity sets), Singapore TableBuilder, the BIS SDMX structure registry | `api.uktradeinfo.com` and `tablebuilder.singstat.gov.sg` had **zero**. The UK trader register — the names and addresses behind UK customs declarations — was not reachable from anywhere in the tree |

`lint-sources` counts `REGISTER_SOURCE` only, so these 112 `HP_REGISTER_TABLE`
rows do not move the number it prints (10,564). The built binary's own seed
count is the real one.

## Gate results

| gate | result |
| --- | --- |
| `tools/probe_hp_batch.py` | **112 of 112 PASS.** Two rows needed a spaced retry — `data.riksdagen.se` drops the connection under repeated hits, and `articlemeta.scielo.org` returned one transient `Network is unreachable` |
| `tools/batch_exclusions.py` | **0 collisions** — but only after being pointed at the whole tree with `--skip-prefix hp3b20_`. See the tooling defect below; its default hides `hp3_*.c` |
| `tools/audit_batch_pagination.py` | **0 of 112 flagged.** 7 rows declare `pagination_ok` with the measured reason |
| `tools/audit_batch_reachable.py` | **0 of 112 can never run.** Every non-pivot row declares `interval > 0` |
| `tools/audit_batch_emit.py` | **0 `DROPS_EVERYTHING`.** 261,423 records emitted of 261,427 available |

```
airspace     41 rows:  OK=41
tradestat    17 rows:  OK=17
parliament + standards + scholarly
             54 rows:  OK=46  PARTIAL=3  TIMEOUT=4  TRANSPORT_FAIL=1
total       112 rows:  OK=104 PARTIAL=3  TIMEOUT=4  TRANSPORT_FAIL=1
records emitted across the runs: 261,423 of 261,427 available
```

**`PARTIAL` is not a failure here.** In every case the engine's own shortfall
line accounts for the gap in-band — `[0 empty, 0 duplicate, 0 filtered out,
1 refused by sink]` — the sink deduplicated an identical record; the row
discarded nothing.

**The 5 `TIMEOUT` / `TRANSPORT_FAIL` rows are a harness bound, not a source
defect.** `audit_batch_emit` caps a row at 180 s. Each was re-run individually
against the same binary and every one emitted:

| row | runtime alone | result |
| --- | --- | --- |
| `DBLP_VENUE_SEARCH` | 22 s | emitted 300 of 300 |
| `RFC_EDITOR_INDEX` | 62 s | emitted 9830 of 9830 |
| `RFC_EDITOR_ERRATA` | 368 s | emitted 8018 of 8018 |
| `UKPARL_LDA_LORDS_WRITTEN_Q` | 458 s | emitted 5000 of 5000 (TRUNCATED at page_max) |
| `UKPARL_LDA_COMMONS_WRITTEN_Q` | 499 s | emitted 5000 of 5000 (TRUNCATED at page_max) |

The slow rows are slow in the sink, not on the wire: they are 5,000- to
10,000-record walks and the cost is per-record insertion.

## Rows written, then removed

**42 rows were written, generated, compiled and emit-audited, then deleted from
the batch** because the endpoint is already registered elsewhere in the tree.
Forty-one of them were FAA current-cycle ArcGIS layers. `batch_exclusions.py`
reported no collision on any of them for two independent reasons:

* `collectors/sources/av_faa_arcgis.c` composes its URLs at runtime with
  `snprintf` from a layer-name macro argument, and the exclusion scanner only
  harvests literal string URLs — fourteen registered layers were invisible to it;
* the `vsrc_aviation_*` feeds request the same layers with `f=geojson&
  resultRecordCount=500`, which does not normalise to `f=json&
  returnGeometry=false&resultRecordCount=1000`, so twenty-seven more read as
  different endpoints.

The duplication was found by diffing **layer names**, not URLs. The 41 rows are
in `docs/rejected-sources-batch20.tsv` under `DUP_LAYER`, each naming the file
that already carries it. The forty-second removal was
`STATFIN_SUBJECT_INDEX` — the only collision the tool *did* report once it was
pointed at the whole tree.

Two more rows were written and pulled on their own merits:

* `OURAIRPORTS_COMMENTS` — parses and carries real records, but a single engine
  run over the full comment corpus did not finish inside 600 seconds. Shipping a
  row that never completes a scheduled slot is the same invisible nothing the
  emit gate exists to catch.
* Six rows were dropped at the exclusion stage before probing: `EUROPEPMC_SEARCH`
  (id already registered), three datatracker endpoints already in
  `hp3b19_legalip.c`, and two Europe PMC rows that were an existing endpoint
  under different query parameters.

## Build gates, quoted

From a tree rsynced fresh and built from zero:

```
$ make
make exit=0
warnings/errors: 0

$ make audit-sources
files scanned      : 1527
files with findings: 0
findings           : 0
strict set (collectors/pivot/table/hp*_*.c): 0 finding(s)

$ make lint-sources
registered source_defs: 10564  (1114 direct + 9450 macro-expanded)
lint-sources: OK

$ make hptest
all passed
```

## Engine traps this batch hit, and how each row answers them

* **`array_path` is mandatory against ArcGIS.** An ArcGIS query response carries
  both `features` (the data) and `fields` (the layer schema). On a small page
  `fields` is the denser array of objects, so the engine's auto-detection keys
  every record off the schema and emits the column definitions as findings.
  All 39 FAA rows declare `array_path=features`.
* **`resultRecordCount` must not exceed the layer's `maxRecordCount`.** The
  engine steps the offset by `page_size`, so a page shorter than the step skips
  records silently. Every one of the 39 layers was queried for its
  `maxRecordCount` before its row was written; the lowest is 1000, so
  `resultRecordCount=1000` / `page_size=1000` never over-steps.
* **`$skip`, `start`, `_page` and `f` are 0-based but do not contain the
  substring `offset`.** `hp_run` coerces `page_start` to 1 for any other
  parameter name, so an OData, Solr, linked-data or dblp row without
  `page_zero_based=1` steps past the first record of every page. All 10
  uktradeinfo rows, both PLOS rows, both dblp rows and all 16 UK Parliament rows
  declare it.
* **`resultRecordCount=1` returns zero features** on this ArcGIS deployment
  while `resultRecordCount=2` returns two. Worth knowing before probing a layer
  with a minimal request and concluding it is empty.
* **The FAA service enforces a 6,000 request-unit-per-minute quota.** Over it,
  the service answers **HTTP 200** with `{"error":{"code":429,…}}` — see defect
  #1. The airspace emit audit was therefore run at `--jobs 1`.

## Defects found in the engine and in the tooling

### 1. An HTTP-200 error document is stored as a finding (`lib/hpengine.c`)

`hp_run_json` falls through to a "root IS the record" path when the declared
`array_path` does not resolve:

```c
if (!arr) {                       /* root IS the record */
    st->available += 1;
    cJSON *flat = cJSON_CreateObject();
    hp_flatten(doc, "", flat, 0);
    hp_emit_record(st, flat, st->deep_left > 0);
```

ArcGIS Online answers a rate-limited query with **HTTP 200** and
`{"error":{"code":429,"message":"Unable to perform query. Too many requests."}}`
(verified: 40 concurrent requests, every one HTTP 200, the over-quota ones
carrying the error document). There is no `features` key, so the fallback fires,
`hp_flatten` produces `error.code = 429`, `hp_pick_s` matches `code` in
`ID_FALLBACK`, and the run stores one record. Observed in this batch's own
scratch database during an early `--jobs 3` audit:

```
FAA_NASR_ROUTE_PORTION   ['airway-record 429', '0A1A7A0A-0C6C-4EFA-9469-9CD66D66AD45']
FAA_NASR_ENROUTE_INFO    ['airway-record 429']
FAA_NASR_CHANGEOVER      ['airway-record 429']
FAA_NASR_RADIAL_BEARING  ['navaid-record 429']
```

The second element is a real record. The first is an error message filed as
intelligence, which is the no-fabrication rule's exact failure mode. It is not
specific to this batch — any hp JSON row against any API that reports errors in
an HTTP-200 body is exposed. `collectors/verify_feeds.py` already guards against
precisely this shape in the *prober* ("ArcGIS and several OGC servers answer
HTTP 200 with an error DOCUMENT"); the engine has no equivalent guard.

Suggested fix, in `hp_run_json`, before the root-record fallback: when the row
**declared** an `array_path` and it did not resolve, emit nothing and say so — a
declared path that is missing is a shape mismatch, not a single record. That
alone closes the case above, because every affected row declares one.

### 2. `hp_xml_flatten` drops XML attributes (`lib/hpengine.c:948`)

It walks child *elements* only. For SDMX, AIXM, WITS and a large share of
government XML the identity lives in attributes:

```xml
<wits:country countrycode="004" isreporter="1" ispartner="1" isgroup="No">
  <wits:iso3Code>AFG</wits:iso3Code>
  <wits:name>Afghanistan</wits:name>
</wits:country>
```

`countrycode`, `isreporter` and `ispartner` are silently discarded — the record
emits, looking complete, missing three of its five fields. The same applies to
every SDMX 2.1 structure document, where `<structure:Dataflow id=… agencyID=…
version=…>` carries the whole primary key in attributes.

Two live, record-bearing WITS endpoints were **rejected for this reason alone**
rather than shipped as silent partials (`ENGINE_XML_ATTRS_DROPPED` in the
rejects file), ready to ship the day the flattener reads attributes. It also
puts the whole SDMX structural-metadata family out of reach — ILO, OECD, ABS,
Istat, ECB, Eurostat, IMF and the SDMX Global Registry were all verified live
during this batch's discovery and all publish only as SDMX-ML.

### 3. `batch_exclusions.py` cannot see two whole classes of existing endpoint

This is the defect that cost this batch 42 rows, and it will cost the next batch
the same unless it is fixed.

* **Runtime-composed URLs are invisible.** The scanner harvests
  `.url = "…"` and bare `https?://…` literals. `av_faa_arcgis.c` builds every
  one of its fourteen FAA endpoints with `snprintf(url, n, "%s/%s/FeatureServer/
  0/query?…", BASE, svc)` where `svc` comes from a macro argument, so none of
  them appear in the dump and none of them can collide.
* **Query-parameter differences defeat normalisation.** `?…&f=geojson&
  resultRecordCount=500` and `?…&f=json&returnGeometry=false&
  resultRecordCount=1000` are the same endpoint asked two ways. The tool
  normalises `{q}` and `%s` to one form but nothing else, so twenty-seven more
  FAA layers read as new.
* **The default `--skip-prefix hp3_` hides batch 18's tables from
  `--dump-urls`.** A discovery pass built on that dump — which is exactly what
  `--dump-urls` is for — under-reports existing coverage by every `hp3_*.c` row.
  `STATFIN_SUBJECT_INDEX` was written because of this and had to be removed.

Suggested fix: compare **host + path** (query stripped) as a second, weaker
signal reported as a warning rather than a hard collision; and make
`--dump-urls` default to skipping nothing.

### 4. `audit_batch_emit.py`'s 180-second cap is not distinguishable from a defect

`TIMEOUT` is reported identically whether the row is broken or merely large.
Five rows here reported `TIMEOUT` or `TRANSPORT_FAIL` and every one emitted when
re-run alone; three of them exceed the cap even on an idle machine, purely
because of per-record sink insertion cost. A `--timeout` flag, or reporting
`SLOW` with the partial `emitted N of M` the engine had already printed, would
make the verdict actionable.

## Discovery: what was probed and rejected

146 rejects are recorded in `docs/rejected-sources-batch20.tsv`, most with the
prober's own verdict. The largest populations:

* **68 `HTTP_ERR`** — the procurement beat was abandoned for this reason. Of
  roughly thirty national and multilateral OCDS / e-procurement endpoints
  probed (Finland Hilma, Norway Doffin, Portugal BASE, Dominican Republic DGCP,
  Ecuador SERCOP, Chile, Kosovo, Mongolia, Estonia, Poland, Slovakia, Romania,
  Nigeria, Indonesia, Taiwan, UNGM, IADB, AfDB, the Global Fund, UNDP,
  Grants.gov, CanadaBuys), exactly one answered with usable open records the
  tree did not already have. Public procurement is widely *published* and very
  sparsely *served*.
* **41 `DUP_LAYER`** — the FAA current-cycle layers described above.
* **13 `UNPARSEABLE`** — a server-rendered SPA shell or a bot wall behind an
  advertised "API".
* **13 `NET_ERR`** — hostnames that no longer resolve, several still linked from
  the publishing agency's own documentation.
* **3 `DUP_ENDPOINT`**, **2 `ENGINE_XML_ATTRS_DROPPED`**,
  **1 `UNLABELLED_FACT_TABLE`** (uktradeinfo `Import`: every field is a bare
  foreign key, so the row would emit records titled by an integer),
  **1 `HTTP_500_UPSTREAM`** (uktradeinfo `Trade` is published in `$metadata` and
  answers 500 to every query), **1 `FLAKY_UPSTREAM`**, **1 `EMIT_TOO_SLOW`**.
