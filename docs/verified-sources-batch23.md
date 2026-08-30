# Batch 23 — Japan, deep: municipal portals, the geospatial catalogue, the social layer

411 rows. Every one was fetched over the wire, parsed in its declared mode,
run through the real binary, and read back out of a database. The registry went
**13,704 → 14,115**, which is exactly +411: no id collapsed onto another.

| beat | rows | what it is |
| --- | --- | --- |
| `jpmuni` | 190 | one RSS feed per town hall, 190 distinct hosts |
| `jpopendata` | 155 | one per publishing organisation on geospatial.jp |
| `jpsocial` | 42 | fediverse, developer and writing platforms, bookmarking |
| `jpregistry` | 24 | Diet proceedings, NDL deposit, statute, disclosure, address |

30 rows are entity pivots; 381 are scheduled.

## How the municipal layer was found

Guessing `/rss/index.rdf` across a thousand hosts is what produces a wall of
404s, and the first attempt at this beat produced exactly that. So the sites
were asked instead of assumed:

1. **Wikidata** for the official website of every Japanese municipality —
   **780** with a `P856` website.
2. **Autodiscovery**: fetch each home page and read its own
   `<link rel="alternate" type="application/rss+xml">`. **179 feeds on 100
   hosts.** Only 13% of municipal sites advertise their feed.
3. **Conventional paths** on the 680 that advertised nothing — the twenty fixed
   paths the Japanese municipal CMS vendors ship (`rss/10/list1.xml`,
   `shinchaku.xml`, `kinkyu/kinkyu.xml`, …), keeping only what parsed as a feed
   with ≥2 items. **142 more feeds on 137 hosts.** The feed is usually there;
   the site just never says so.
4. 321 feeds → **299** after dropping URLs already in the tree → **267** usable
   after re-fetching each one and classifying `item` vs `entry` from its own
   bytes → **190 shipped**, one per municipality for maximum breadth.

**215 of these hosts were entirely absent from the tree.** This is the layer
where a planning permission, a subsidy award, a licence suspension or a local
committee appointment is actually recorded, and almost none of it is reprinted
anywhere a national search reaches.

## What was rejected, and why

Rejects are kept as data in `docs/rejected-sources-batch23.tsv`.

**Filter ignored (house rule 4d)** — an API that accepts a filter, ignores it,
and returns the whole collection with HTTP 200. Every gate goes green and the
row is a confident wrong answer attributed to the entity:

| dropped | measured |
| --- | --- |
| `www.data.go.jp` per-organisation, 23 rows | `fq=organization:<impossible>` → **18,141**, the entire catalogue, same as every real organisation |
| Tokyo catalogue free-text search | `q=<impossible>` → **9,660** (whole catalogue) vs 1,411 for a real term |
| Sapporo free-text search | `q=<impossible>` → **101** (whole catalogue) vs 34 |
| J-STAGE article search | `service=3&article=` **and** `service=3&text=` both return 20 of 20 for an impossible term — the whole free-text family |

The same check on **geospatial.jp returns 0** for the impossible term against
1,307 for a real one, which is why that portal's 155 rows are kept and these
are not.

**Other exclusions:**

* **data.bodik.jp — 255 organisations, all holding datasets.** BODIK began
  answering 403 to every request from this host partway through discovery.
  The rows could not be honestly verified, so they were cut rather than shipped
  on the strength of the earlier partial sweep.
* **Tokyo (46) and Sapporo (4) per-organisation catalogues** — already
  registered in `vsrc13_jp_local_2.c`, `vsrc15_jp_opendata_1.c` and
  `vsrc2_government_3.c`. `jpopendata` is therefore geospatial.jp only.
* **JMA per-office forecasts.** 58 offices probed, 56 live. Dropped, but the
  reason first recorded here was **wrong and is corrected in batch 24**: the
  stated reason was that `hp_path()` cannot descend through an array, which is
  true and has since been fixed. The actual reason these rows do not belong is
  that **all 112 forecast and warning endpoints are already registered** in
  `vsrc13_jp_local_1.c` and `vsrc14_jp_prefectural_3.c`, and those rows store
  the complete document — the `properties` of each record carries the whole
  `timeSeries` array nested inside it. Nothing is lost. Registering these would
  have been 112 duplicate endpoints for a record-granularity preference.
* **The 2ch family.** 15 `machi.to` boards and 3 `open2ch` boards are live and
  parse cleanly; 5ch and Shitaraba answer 403 to this host. The live ones were
  still dropped because their `subject.txt` is Shift_JIS and `lib/hpengine.c`
  has no transcode on its fetch path — `lib/feedlib.c` has one, gated on a `.jp`
  host, and hpengine does not use it. They would store mojibake titles.
* A first pass registered `machi.to` boards across 12 regional subdomains and
  got 19 "passes". They are 4 distinct boards: **the host is irrelevant and
  every subdomain serves the same content.** Caught before writing rows.

## Defects the verification found in rows that had already passed the probe

The probe proves an endpoint answers. These were only visible from the run line:

| row | symptom | cause and fix |
| --- | --- | --- |
| `NDL_SRU_SEARCH` | `emitted 500 … stored=1` | SRU's default `recordPacking=string` returns the record as **escaped text** no XML path can address, so every record fell back to the same key. `recordPacking=xml` fixes the packing but the dcndl payload then contains its own `record` elements and `array_path` matches the wrong nodes — **dropped**; the three NDL OpenSearch rows cover the same catalogue at 500 emitted / 500 stored. |
| `HATENA_BOOKMARK_SEARCH_*` | `emitted 400 … stored=201` | the search feed pages in steps of **40, not 20**: `of=0` and `of=20` return the identical 40 items. `page_size=20` refetched page 1 as page 2. Fixed → **401/401**. |
| `ESTAT_LOD_SPARQL` | HTTP 406 | `lib/hpengine.c:1557` appends `Accept: application/json` to every JSON row, and this endpoint answers 406 to exactly that while returning `sparql-results+json` to an Accept naming it. Fixed with a row header → **100/100**. |
| `OPENBD_ISBN` | `records=0` | the probe ISBN simply is not in openBD, which answers `[null]`. Probe re-pointed at an ISBN the database holds → **3/3**. |
| `JSTAGE_MATERIAL_SEARCH` | `emitted 0 of 10` | `service=2` returns **`ERR_001` inside an HTTP 200** with one empty `<entry>`. **Dropped.** |

Two collision reports were investigated and are **correct behaviour**, not loss:

* `EGOV_LAW_KEYWORD` — 136 emitted, 128 stored. `offset` there counts
  **sentences, not items**, so a page of `limit=100` returns a variable 13–30
  laws and a law whose matching sentences straddle a boundary is returned on
  both pages. Measured over four pages: 76 rows, 73 distinct law ids. The
  record is the law, so collapsing them is dedupe.
* `NAROU_SEARCH` / `NAROU_USERS` — the response array's first element is an
  `{allcount:N}` **envelope**, repeated on every page. The collapsed records are
  that envelope. Every real result is stored.

## Gates, quoted

```
$ python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch23.*.txt --check-filter
probing 411 rows
# 411/411 PASS

$ python3 tools/audit_batch_reachable.py ../docs/candidate-sources-batch23.*.txt
0 of 411 rows can never run

$ python3 tools/audit_batch_pagination.py ../docs/candidate-sources-batch23.*.txt
0 of 411 rows declare no pagination but look paged  (222 declared pagination_ok)

$ python3 tools/batch_exclusions.py --check ../docs/candidate-sources-batch23.*.txt \
      --bin ./bin/japanosint            # against the pre-batch 13,704-id registry
collisions: 0   near-misses needing a human read: 0

$ make                 warnings/errors: 0
$ make selftest        [selftest] PASS
$ make unit            test_search_degradation: OK
$ make hptest          all passed
$ make lint-sources    lint-sources: OK
$ make audit-sources   strict set: 0 findings   rest of the tree: 0 findings

$ python3 tools/audit_batch_emit.py ../docs/candidate-sources-batch23.*.txt \
      --bin ./bin/japanosint --jobs 6 --timeout 200
OK=390 … records emitted across the run: 32,289
```

### Reading the emit audit honestly

`audit_batch_emit.py` recovers a row's pivot entity by diffing the URL template
against the probe URL. Every probe URL in this batch carries a **percent-encoded
Japanese term**, so the recovered entity is the encoded string and the engine
then encodes it again. All 20 rows the auditor did not mark OK are entity
pivots, and the verdict is an artifact of that double encoding, not a property
of the row.

They were therefore re-run by hand against the real binary with a real entity —
every one of the 30 pivots, not a sample. All emit and store:

```
KOKKAI_SPEECH_ANY      半導体   records=1001 stored=1001
KOKKAI_MEETING_LIST    予算     records=1001 stored=1001
NDL_OPENSEARCH_CREATOR 夏目漱石 records=500  stored=500
NICONICO_SNAPSHOT_DESC 企業     records=1001 stored=1001
HEARTRAILS_GEO_TOWNS   千代田区 records=400  stored=400
GSI_ADDRESS_SEARCH     銀座     records=102  stored=102
```

The one row the auditor flagged that was **not** an artifact was
`ESTAT_LOD_SPARQL` — scheduled, so the artifact could not explain it — and it
was a real defect, fixed above. That is the whole reason the split was worth
making rather than waving the 20 away together.

## Engine gaps recorded, not fixed

Out of scope for a source batch; each is a real limitation this batch measured.

1. **`lib/hpengine.c` has no Shift_JIS transcode.** `lib/feedlib.c` does, gated
   on a `.jp` host, with a documented rationale. hpengine does not use it, which
   costs the whole 2ch-family BBS layer and would corrupt any `.jp` source
   serving legacy encodings.
2. **`hp_path()` does not descend through arrays.** A document that is a
   top-level array of objects each holding the real record list is unreachable
   except by a positional index, which discards the rest. **Fixed in batch 24**
   (`hp_path_multi`, three regression tests). Note that fixing it did NOT
   recover the JMA offices — see the correction above; they were already
   registered elsewhere and complete.
3. **`audit_batch_emit.py` double-encodes a percent-encoded probe entity**, so
   every non-ASCII pivot row reports a false failure.
