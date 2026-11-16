<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): batch numbering, batch measurements, ceilings, CSV delimiter, batch 31 and the Batch tooling table. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

**And a THIRD branch hit the same renumbering, which is why batch 46 is the
ArcGIS beat.** PR #51 generated it as batch 45 when 44 was the highest
number in the tree; by the time it came to merge, main had merged PR #50's batch
45 AND renumbered its own 45 and 46 to 47 and 48 — so 45, 47 and 48 were all
taken and only 46 was free. `docs/verified-sources-batch45.tsv` was a direct
collision, the same file this rule's own note predicts. Renumbering BEFORE
merging main, rather than after, is what kept it to one pass: the merge then had
no both-added file to resolve. Three sessions have now been renumbered by this,
so treat the check as a merge-time step and not a generation-time one.

The lessons from batches 43–48 that a new batch must not relearn:

* **Ask the endpoint what it SERVES, rather than probing the paths you can
  name.** Batch 42 found this with WordPress route tables and batch 46 repeated
  it on a different family: the tree already reached 195 ArcGIS service roots,
  each by ONE row naming ONE layer, and asking each root for its own directory
  (`?f=json`) found thousands of services — one root alone published 899. The
  method generalises; the family does not matter.
* **Attribution keyed on a HOST is wrong wherever the host is multi-tenant.**
  `services1.arcgis.com` is ArcGIS Online's front for hundreds of unrelated
  organisations, told apart only by a 32-character org id in the PATH, so batch
  46's generator named a Wyoming EMS layer after "Wakayama City traffic accident
  record points". Live endpoint, real records, agreeing counts, false name — and
  no record-counting gate can see it. Key on the whole root, and where the
  publisher cannot be established, name the HOST (a fact about where the data
  came from) rather than guessing whose it is.
* **Classify on what the publisher named the collection.** Inferring `category`
  from COLUMN names filed a bridge layer carrying `FLOODPRONE` under `safety`
  and a power-outage layer under `health`. A column is an attribute of one
  record and says nothing about the whole, and a category is user-visible
  wherever sources are grouped. A guess read off a column is not a measurement.
* **An ArcGIS layer's `objectIdField` and `maxRecordCount` are its own**, not
  `OBJECTID` and not a constant: several batch-46 layers key on `FID`, and the
  published page size ranges from 1,000 to 16,000. `resultOffset` is a record
  OFFSET so `page_size` belongs on it, and a cycling layer needs
  `orderByFields=<oid> ASC` because ArcGIS promises no stable ordering without
  one. **`returnCentroid` works only on POLYGON layers** — ArcGIS says so with
  HTTP *200* and a 216-byte error body, so 18 polyline rows came back
  `PATH_UNRESOLVED` from a rule tested on four layers that happened to all be
  polygons. Polygon rings cost 64x the payload (125 KB -> 8.0 MB per 200
  records) and a centroid 1.7% of that; polyline paths cost 1.3-9.3x, so they
  keep full geometry. Measure per geometry type.

* **A registration count is not a website, and a build is not a registry.**
  A quarter of any official-domain register has no server at the root; and
  `MAX_SOURCES` (`registry.c`, now 131,072) silently dropped 2,392 rows at
  43,453 with a clean build — compare a beat's registration count with its
  table, and run `make registry-floor`, before believing an audit.
* **The engine's measurement outranks the probe, in both directions.** Generate
  a beat with every probe candidate and let `audit_registry_emit.py` decide:
  shared hosting that answers 502/403 to twelve concurrent probes stores its
  feed at six, and a 403 that refuses the engine too is a bot wall. Both
  readings go in the verified TSV. Re-run each FETCH_FAILED once; a row that
  fails twice leaves — and a retry pass a day later recovers a few: 63 of the
  2,724 rows batches 44–46 dropped were alive on 2026-10-19 and are back in
  their beats. **"Never rebuild the binary while an audit runs" is now
  ENFORCED rather than remembered**: `audit_registry_emit.py` copies the binary
  at startup and runs the copy (`--no-pin` opts out). It was a discipline
  nobody can hold for the two hours a 290-row sweep takes — a `make` in another
  shell replaces the file underneath it and every remaining row comes back
  shell `rc=126`, reported as `NO_RUN_LINE`, "the process died before the run
  returned". Measured 2026-10-20: **229 of 290 batch-53 rows read that way**
  from one unrelated rebuild made in the middle, and the output looks exactly
  like a batch of broken sources.

  **And when the GATE and the ENGINE disagree, the upstream can be the answer.**
  Batch 51's audit called `JO53_AGS_GIS_COASTAL_COVERAGE_AREA` `EMITS_NOTHING`
  while a direct `--run` stored 371 of 371 — the dangerous direction, since a
  gate that contradicts the engine is how a working row gets "fixed".
  Reproducing the audit's exact invocation (its `ulimit`, its env, its scratch
  database) showed the cause was neither tool: `gis.charttools.noaa.gov` FLAPS,
  answering HTTP **200** carrying `code=500 "Could not find service. Service
  may be stopped or it may not be configured."` It served 371 twice and that
  twice, inside ten minutes. **Four readings, two outcomes, one endpoint** — so
  reproduce the TOOL's invocation before believing either of them about the
  row, and a single verdict is still not a diagnosis.

  The other 289 of that batch account for themselves in
  `docs/verified-sources-batch53.tsv`: 259 store exactly their upstream count,
  25 cycle because their layer exceeds one cycle's 40,000, and 5 store slightly
  MORE because the discovery-pass count is a day stale and the layers grew
  (`returnCountOnly` re-asked agrees with the engine exactly). **1,706,171
  records stored in one pass** against 25,132,703 the layers declare.

  One of those 259 is a reminder that **`SLOW` means unmeasured, not failed**:
  it had stored 22,000 of 35,204 when the tool's 200s default kill line hit,
  and at 600s it stores 35,204 of 35,204 in 298.8s. A kill line is a property
  of the tool.

  **And a 403 is usually a wall against the default User-Agent, not against
  us** (2026-10-20): of the 523 dropped rows that had answered 403 to the
  engine's client, 495 answered the probe and **491 store their feed in the
  engine** — 11,159 emitted, 11,159 stored — under a declared
  `header1=User-Agent: Mozilla/5.0 (compatible\; JapanOSINT/1.0\; +https://github.com/rayanchl/JapanOSINT)`:
  the same identified client with the browser-compatible prefix the walls
  key on, nothing hidden. They are back in their beats (86 UK, 328 FR, 58 IT,
  8 Wikidata, 4 IGO, 7 press) with the agent their hosts were proven under —
  first as a header on each row, since 2026-10-21 as an `HA_COMPAT` line in
  `core/host_agents.def`; the four that refused twice stay out. Try that
  agent before calling a 403 dead.
* **A URL changed after validation is unvalidated** (an unescaped `&amp;`
  made Joomla serve Atom where rows declared RSS), **a well-formed feed with
  zero items is `EMPTY_RESULTSET`**, **a substring count of `<item` counts the
  RSS 1.0 `<items>` container**, and **WordPress declares a comments feed
  beside every posts feed**.
* **A collision is a question**: measure the feed against itself (distinct
  keys, byte-identical surplus) before touching a key. Every collision in
  25,863 feed rows was the upstream's own duplicate; a single page cannot skip.
* **Generator defaults and Wikidata classes are claims**: the builder takes
  category, record type and tags as arguments, and a class Wikidata asserts
  goes in the description, never the name. Titles are unescaped,
  whitespace-normalised and pipe-free; a URL carrying a credential is dropped.
* **The interval follows the measured freshness** of each feed (newest item
  within 3 days → hourly, 30 days → 6 h, a year → daily, else weekly); the
  press stays hourly. **And the scheduler re-measures it** (2026-10-20): the
  four feed batches carry `adaptive_interval=1` (`hpengine.h`, through
  `source_def`), and after every run that WORKED `core/scheduler.c` re-reads
  the newest ISO `published_at` the row has stored and re-buckets it by the
  same rule (`sched_freshness_measure`, persisted as
  `source_sched_state.freshness_interval`, and what the row's
  `effective_interval` then shows). Both directions, bounded by the buckets:
  a parish council that goes quiet drifts to weekly, one that wakes before an
  election is hourly on its next run. An undated feed keeps its declared
  weekly (no evidence, no change) and a refused fetch changes nothing (a 403
  says nothing about how often the upstream publishes). It is opt-in because
  a row's author may have set a cadence the item dates cannot see — a
  forecast feed, a window the row exists to be fresh for.
  `tools/tune_intervals.py` is the authoring-time half of the same rule and
  marks the rows it tunes; `test_sched_backoff.c` pins the bucket, the base
  and the measurement. The operator's system-wide cadence policy (the
  "Cadence policy" section) applies ON TOP of this measurement, never under
  it: `sched_row_interval()` is the one composition.

**Batch 45's method is worth more than its 619 rows.** Every `.jp` host a
registered row already fetched was asked whether it also runs WordPress REST —
the homepage's `Link: rel="https://api.w.org/"` names the root (232 of 245),
a bare `/wp-json` only 13 — then for its post types and one page of each
collection; the tool is `tools/wp_routes_sweep.py`. The same sweep over 5,123
federation-member hosts (chambers of commerce, JA, medical associations, fire
chiefs, welfare councils, co-ops, professional bodies) found 5,678 live
collections on 1,372 hosts, kept in `docs/staging-sources-batch45.federations.tsv`
until they are registered as their own batch.

**Batch 33 is the one batch measured end to end against live upstreams** —
43 supply-chain and package-registry rows
(`collectors/pivot/table/hp3b33_supplychain.c`,
`native/collectors/OSINT_SOURCES_BATCH_33_SUPPLYCHAIN.md`). The environment it
was authored in reaches package-registry hosts and only those, so probe
`--check-filter`, `audit_batch_emit`, per-pivot `--run` and the registry sweep
all ran for real: every row has an emitted-AND-stored reading, and the counts
in its descriptions are what the bodies held. The same is true of the two small
beats that followed it — batch 34's `ndjson` and batch 35's `strings` — which
is why those three carry `docs/verified-sources-batch3{3,4,5}.tsv`. Note what
that environment constrains: `docs/rejected-sources-batch33.tsv` lists the hosts
the policy refused, OSV, deps.dev, libraries.io and ecosyste.ms among them, and
the staging set now at `docs/candidate-sources-batch37.*.txt` (144 rows on 140
hosts when this was measured, 80 after its dedupe) has **zero** hosts reachable
from it, so it stays unprobed.

**Batch numbers 34 and 35 each hold beats from TWO sessions, and the
`verified-sources-batch3{4,5}.tsv` files describe only one of them.** Batch 34
is `ndjson` (1 row, measured) plus `ckanbulk`/`fdsnstation`/`fdsnevent`/
`socratacat` (122 rows, measured 2026-10-20 — see below); batch 35 is `strings`
(4 rows, measured) plus `jpweko` (98 rows, measured 2026-10-20). Since that
day the per-batch TSVs cover every beat of the number, each row saying which
measurement it carries. Nothing collides — the beat names keep
every file, id and table symbol distinct — but a per-batch TSV or note is no
longer a statement about the whole number, so read the BEAT, not the batch.
This is the same hazard the paragraph below is about, hit a second time by the
session that had already renumbered itself out of it once.

**Batch numbers are claimed by whoever merges first, so check before you
generate.** Two sessions authored a "batch 33" on 2026-10-04; the one that
merged first kept the number and the other was renumbered to 34 and 35 at merge
time (ids, table symbols, file names, manifests and every textual reference).
The collision would have made `verified-sources-batch33.tsv` describe one batch
while another batch's headers said that file did not exist. The renumbering
then landed on 34 and 35, which a third session had taken in the meantime — so
the rule is not "renumber once and you are safe", it is **re-check at merge
time**, because the number you picked when you generated may have been claimed
while you worked. The same collision existed in docs: a docs-only STAGING set
of 144 global candidates had been written as `docs/candidate-sources-batch31.*.txt`
after the unrelated `hp3b31_*.c` tables had merged under 31. Once those tables
got manifests of their own, a `batch31.*.txt` glob would have fed both sets to
every gate, so the staging set moved — to 36, which a live-measured batch then
merged under first, and so to **batch 37** (`docs/batch37/STAGING_README.md`,
80 rows kept, 64 rejected as duplicates of the tree into
`docs/rejected-sources-batch37.<beat>.tsv`). **It has since been probed and
generated** — this file said "still not generated into C, still unprobed" for
a fortnight after it was, which would have cost a session a re-probe of a
settled set. Commit `5d5e1f99`: **15 of the 80 pass**, 16 rows measured into
`collectors/pivot/table/hp3b37_{eu,fr,jp,row,us}.c`, 70 rejected into the
per-beat TSVs. Before picking `<N>`:
`git ls-tree -r --name-only origin/main | grep -oE '(batch|hp3b)[0-9]+' | sort -u`.

**Batches 34 and 35 were authored unfetched (2026-10-04, a session whose
network policy allowed only Anthropic APIs, package registries and GitHub) and
MEASURED on 2026-10-20** — probe with `--check-filter`, the registry sweep for
the scheduled rows, `audit_batch_emit.py` with the manifest for the pivots, one
re-run of each `FETCH_FAILED`. `docs/verified-sources-batch3{4,5}.tsv` now
carry every beat of both numbers, one reading per row;
`docs/rejected-sources-batch3{4,5}.tsv` the rows that left.

* **Batch 34: 139 → 122 rows** (`hp3b34_{ckanbulk,fdsnstation,fdsnevent,socratacat}.c`,
  `native/collectors/OSINT_SOURCES_BATCH_34_SPEC_FAMILIES.md`). Rule 4d
  held: 0 `FILTER_IGNORED` across the 24 Socrata pivots. Removed: the three
  FDSN `event/1` rows whose node does not run that service (the batch's own
  review warned of the shape), and 14 CKAN portals that refused twice. **The
  three COLLISION verdicts were skips**: `current_package_list_with_resources`
  orders by `metadata_modified desc` with no sort parameter, so an edit during
  the walk shifts every later page. All 81 surviving CKAN rows read
  `package_search?sort=id+asc&rows=100` with `page_resume=1` now, re-measured
  the same day: Zürich 942 / 942 (was 942 / 926), govdata's 169,254 datasets
  over a cycle instead of its first 2,000 forever.
* **Batch 35: 100 → 98 rows** (`hp3b35_jpweko.c`,
  `native/collectors/OSINT_SOURCES_BATCH_35_JPWEKO.md`): 0 `FILTER_IGNORED`;
  59 store records for the probe entity, 39 answer an honest empty for it,
  two repositories answered 5xx twice and left. JAIRO Cloud's shared hosting
  answers a burst with 5xx — the probe at twelve saw twelve, the engine at six
  saw five — the reason the probe now paces per host.

Three things this branch measured on 2026-10-21 that the readings above predate:

* **A `page_max` of 20 on every surviving CKAN row reads 2,000 datasets a run
  and cycles, and 14 of them do not need to.** `package_search?rows=0` answers
  `result.count` on the same endpoint those rows now read, so the pages each
  needs is arithmetic: 19 portals measured, **792,984 datasets**, 14 needing
  ≤200 pages and now completing in ONE run (opendata.nhsbsa 2,166, Berlin
  2,627, Aragón 2,425, data.ca.gov 4,675 — each its declared total exactly,
  `stored == emitted`), 5 keeping the cycle at a 200-page cost ceiling
  (Hamburg 246,666, geoscience QLD 192,703, govdata.de 169,142). `page_resume`
  stays on all of them: a walk that completes stores no position, so it is a
  genuine no-op. `docs/measured-page-ceilings-batch34-2026-10-21.tsv`.
* **A `page_max` set to EXACTLY the pages the total needs manufactures a FALSE
  truncation notice.** data.ca.gov at `page_max = 47` stored all 4,675 records
  *and* filed "the page ceiling or a cancel stopped the walk" with
  `more_pages_pending: true`: its last page was short (75 of 100, the
  upstream's own end of data) and our ceiling bit in the same breath, which the
  engine cannot distinguish. At 52 the same 4,675 arrive with no notice. Leave
  headroom — a notice that fires when nothing was lost teaches a reader to
  ignore notices, and that is the one thing rule 2's disclosures cannot afford.
* **NOA was removed as "does not run event/1" and it does.** An FDSN
  `nodata=404` means NO DATA MATCHED, not NO SUCH SERVICE, and that parameter
  is in the row's own URL; a 400 can mean the query was too broad. The test is
  `/fdsnws/<service>/1/version`: NOA answers **1.2.4** and refuses an unbounded
  query with 400, so a rolling 30-day window — `starttime={date:%Y-%m-%d:-30}`,
  never a frozen date — stores **1,985 events** where the row stored nothing.
  The row is back. KOERI really has no event service (404 on `version` over
  https and http and on `application.wadl`, while `station/1/version` is 200)
  and ICGC's is deployed but answers "no matching events found" for every
  window including `starttime=1900-01-01`; both stay removed, and both hosts'
  station services were already wired. **Ask the `version` endpoint before
  concluding a service is absent**, and read a status against the parameters
  the row itself sent. The two surviving catalogues were also still
  `orderby=time` (FDSN's newest-first) at `page_max = 20`: ETHZ is `time-asc`
  at 52 now and stores **46,620 events**, exactly the count an offset bisection
  predicted, and SCEDC cycles ascending.

**21 of the 179 stopped dead on the ceiling, and all 21 are now measured and
fixed** (2026-10-21, `docs/measured-page-ceilings-batch34-2026-10-21.tsv`): 19
CKAN portals at exactly 2,000 (20 pages x 100) and the two surviving FDSN event
catalogues at exactly 20,000 (20 x 1,000). **A round number equal to ceiling x
page size is never the upstream's own total.** Their declared totals come to
**792,984 datasets** for the CKAN 19 alone.

Finding them needed a count the row's own endpoint does not publish:
`current_package_list_with_resources` returns a BARE LIST, which is precisely
why 19 rows could sit at 2,000 looking healthy — there was no total in the
envelope for any gate to compare against. `package_search?rows=0` on the same
host answers `result.count`, so the total comes from the upstream itself.
Offset safety was measured too, not assumed, on all 19: stable (the same
offset twice gives the same ids), disjoint, and the offset honoured.

Fixes follow this file's own 200-page split: 14 rows ≤200 pages get a measured
`page_max` (opendata.nhsbsa 2,000 → 2,166, Berlin → 2,627, Aragón → 2,425,
data.ca.gov → 4,675 — each its declared total exactly, `stored == emitted`);
5 rows over 200 get `page_max = 200` plus **`page_resume = 1`**, so the per-run
cost stays at the count this file calls sane and the collection is read over a
cycle. Verified over two runs against one database: `data.gov.ie` 20,000 then
2,881 = **22,881, its declared total exactly**, and run 2 filed no notice
because the cycle completed and cleared.

* **A `page_max` set to EXACTLY the pages the total needs manufactures a false
  truncation notice.** Measured on `data.ca.gov`: at `page_max = 47` the walk
  stored all 4,675 records and filed `"the page ceiling or a cancel stopped the
  walk"` with `more_pages_pending: true`. The last page was SHORT (75 of 100) —
  the upstream's own end of data — but our ceiling bit in the same breath, and
  the engine cannot tell the two apart. At 52 the same 4,675 records arrive with
  **no notice at all**. So leave headroom above the measured need: a notice that
  fires when nothing was lost teaches a reader to ignore notices, which is the
  one thing rule 2's disclosures cannot afford. (The cycling rows' notices are
  TRUE — they really did stop mid-collection — and say where.)
* **FDSN publishes no total, so the catalogue was sized by bisecting the
  offset** — ~20 one-row requests. ETHZ holds ~46,620 events, oldest ECOS-09's
  year-0250 entry; the walk then emitted exactly 46,620, so the probe and the
  engine agree to the record. SCEDC answers at offset 200,000 with a 1987 event
  and times out at 1,000,000, so its depth is recorded as **unmeasured rather
  than absent** and it cycles. Both were `orderby=time` — FDSN's newest-first —
  which is the drift hazard this file names: new events land at the HEAD of the
  collection the walk is reading. Both are `time-asc` now, verified accepted
  per host.

Two review findings from batch 34 that generalise:

* **A Socrata domain's own `/api/catalog/v1` is NOT scoped to that domain.**
  Without `domains=` and `search_context=`, a pivot returns matching datasets
  from every Socrata portal and attributes them to this one — rule 4d's
  confident wrong answer. 64 existing rows already passed `domains=`; batch
  34's 24 did not until review.
* **The FDSN spec defines services; a node runs only some of them.** EIDA nodes
  serve dataselect/station/availability, not event; USGS ComCat and EMSC serve
  event, not station; IRIS retired fdsnws-event. 11 batch-34 rows asking a host
  for a service it does not run were dropped before any probe.

**A CSV delimiter that defaults to comma silently unparsed a working row.**
`lib/hpengine.c` sets `char delim[64] = ","` and does NOT sniff. FDSN's
`format=text` is PIPE-delimited, so batch 31's `FDSN_STATION_INVENTORY` — cited
below as storing 151,303 records — was putting each whole line into ONE cell:
`col1`, `col2`, `col3` and `col5` all resolved to nothing, the title fell back
to the raw line, and no station got coordinates. The record COUNT was real,
which is precisely why it read as healthy for weeks. Fixed 2026-10-04 with
`csv_delim=pipe;csv_comment=#;csv_no_header=1`, and its `id_keys` moved from
`col1` to `col0+col1+col6` because a station code is not unique across networks.
**A row's record count says nothing about whether its FIELDS were parsed** —
check one record's `properties` the first time a non-comma CSV row ships.

**AND IT HAPPENED AGAIN TO 87 ROWS, THROUGH THE OPPOSITE MISTAKE: THEY SAID
`csv_delim = "comma"`** (2026-10-24). The resolver named `tab`, `pipe`, `semi`,
`ws` and `lit:<token>` and took the **first character** of anything else — so
`comma` meant **`c`**. The Japanese crime and accident files these rows read
contain no ASCII `c` at all, so every line became ONE FIELD whose NAME was the
entire header line and whose value was the entire row: 20 columns of offence,
method, police station, municipality, date, hour, place, victim age and lock
status, in one unqueryable cell, with a correct record count on every run.
77 rows in `hp3b25_jppolice.c`, 8 in `hp3b28_jppolice.c`, 2 in
`hp3b25_jpinfra.c`.

**What makes this worth a paragraph rather than a line is that a careful hand
triage of those very rows walked straight past it.** The 2026-09-08 note at the
head of `hp3b25_jppolice.c` works through the collision verdicts, fetches four
of the files, matches the duplicate-line count to the loss exactly, and
concludes "two incidents that match on all thirteen columns are, in the
published file, the same thirteen bytes" — reasoning about thirteen columns
while the engine was reading one. It reached a true conclusion about the FILE
and never asked what the engine had made of it. The cheap check its own
predecessor paragraph prescribes — read one record's `properties` — would have
shown it in a second.

So: **`comma` is accepted now**, a single character is still taken literally,
`lit:` is still how a multi-character delimiter is declared, and a WORD that
names nothing reads as the default comma with a line on stderr rather than as
its first letter. `make lint-sources`' `csv-delim-name` fails the build on the
shape (checked by introducing one: 0 findings clean, 1 and FAILED with
`"kamma"`), and `hptest` 55 and 55b pin both halves on a body containing
neither `c` nor `k`, so either mistake would leave the whole line in one field.

Measured after, through the engine: `JO28_JCG_VESSEL_ACCIDENT_CSV` 19,370 of
19,370 with **22 named fields** a record, the Gifu bicycle-theft file 1,630 of
1,630 with **20**, and both JEPX power-market rows 9,984 and 9,953 complete
with 24 and 14. Every shape notice those rows had been filing about unmatched
title keys is gone, because the keys now match.

**The two JEPX rows then showed the next layer**: with their fields finally
readable, `id_keys = "受渡日"` is the DELIVERY DAY on a file of half-hour slots
— a dimension, kept honest only by the content hash, which means an upstream
price CORRECTION would have landed as a second row instead of updating the
first. `受渡日+時刻コード` and `年月日+時刻コード` are 9,984 of 9,984 and 9,953
of 9,953. A delimiter fix is not the end of a row's repair; it is the first
time anyone can see the row.

**AND THE SAME FILES NEEDED A SECOND, INDEPENDENT FIX: `occurrence_rows = 1`.**
Once the columns were readable the collision came back smaller and real — 1,615
of Gifu's 1,630 — because **a register of EVENTS whose columns are all
categorical publishes two different events as the same bytes.** Three bicycles
taken from one parking area in Ōgaki on 2024-05-17 at 12:00, all unlocked,
victims in their twenties: three crimes, one row in the database. Japan Coast
Guard's file has one tuple appearing **forty times** — forty pleasure-boat
drift incidents in September 2018, in harbour, mooring failure, under typhoon
conditions.

The guard's content hash is exactly right for a CATALOGUE — `sec-nga-tod`
serves one product 14 times byte for byte and storing it once is real dedupe —
and exactly wrong here. **No record count can tell a catalogue from an
occurrence register**, so the row's author declares which it is
(`hp_source.occurrence_rows`, `hptest` 54–54e). The flag keys a repeat on its
occurrence number WITHIN the file, which is a fact about the published document
and not an invented identity; the first of a group keeps the uid it already
had, so turning it on adds rows and re-keys none. 85 rows carry it — every
HP_CSV row in those two files, because the flag states what the FILE is rather
than what one sampled run showed.

**That reverses the September decision, and the argument is in the file rather
than silently applied.** Its objection was that distinguishing the rows would
mean fabricating an identity; its own last paragraph is the answer — "the
incident count is therefore the EMITTED number, not the stored one, and the run
line prints both". A run line is not what anyone queries. Rule 2 says what was
left unused is reported AS DATA, and here it was not even that: the database
held fewer crimes than the police published, with nothing in it saying so.

**Batch 31 has now had its `--check-filter` run (2026-10-06), and 65 rows
failed it.** All 360 rows of `docs/candidate-sources-batch31.*.txt` were probed
live; the full result is `docs/probe-sources-batch31.tsv`. 51 PASS. Sixty-five
came back `FILTER_IGNORED` — the upstream takes the entity parameter, ignores
it, and returns the same collection for a real entity and for one that cannot
exist — which rule 4d calls a confident wrong answer and this file calls fatal.
They were REMOVED (registry 18,643 → 18,578, both floors re-recorded), after two
were confirmed by hand first, because 65 live sources is not something to delete
on one tool's say-so: `UK_INSOLVENCY_INDIVIDUAL` answered 9,213 bytes for a real
surname and 9,203 for an impossible one; `KE_BRS_BUSINESS_SEARCH` 348,130 and
348,402. The lesson generalises past batch 31: **65 of 360 means roughly one
entity pivot in six was answering about the wrong thing**, and nothing but
`--check-filter` can see it.

The rows that failed for other reasons — 146 HTTP_ERR, 39 UNPARSEABLE, 31
NET_ERR, and the rest — were NOT removed. One probe is one reading and those
verdicts can be transient, a user-agent or a bad afternoon, whereas
FILTER_IGNORED is a property of the endpoint's query handling. They disclose
themselves now instead; see the fetch-failure notice under rule 1.

**Rules 4 and 4b are now measured too (2026-10-11), and the tool you pick
decides whether the answer means anything.** `docs/emit-sources-batch31.tsv`
holds it: of the 295 live rows, **53 OK** (they fetch and store real records
for their probe entity), 99 `EMPTY_UPSTREAM` — which is not "broken", the
endpoint answered and held nothing for that particular entity — and 48 404s,
42 403s and 37 transport failures. The 65 `UNREGISTERED` are exactly the 65
removed above, which is also the proof the removal was complete.

**Use `audit_batch_emit.py` with the MANIFEST for pivot rows, never the
registry sweep.** `audit_registry_emit.py` runs every row with no entity, so
it reported **268 of these 295 as EMITS_NOTHING** — a verdict about sources
that had never been asked anything. The sweep has a `NEEDS_ENTITY` verdict for
exactly this and it could not fire, because an on-demand pivot asked with no
entity returned SILENTLY from `hp_run`: the run line read
`records=0 0ms stored=0`, byte-identical to a row that went and found nothing.
The engine says it now and the sweep matches on it.

Nothing is removed on that reading. One probe is one reading, 403 and 404 can
be a user-agent or a bad afternoon, and each of these rows now files a
collector-fetch-failure record naming its status. The 27 scheduled rows were run on
2026-10-02: 13 store real records (FDSN 151,303; Safecast 30,000; Sejm 15,000;
USGS NWIS 53,789 before the audit timeout …) and 14 need an API key and store a
"gated" notice saying so. Ten rows were removed when the branches merged: seven
dead endpoints (400/401/404 on every run) and three that re-fetch what an
existing collector already reads. See
`native/collectors/OSINT_SOURCES_BATCH_31_GOV_PUBLIC_SURVEILLANCE.md`. Treat
those pivots as registered, not proven, until they are verified.

Batch 31 was hand-written and had no manifest, so no manifest-driven gate could
reach it — and the command its own headers gave instead,
`audit_registry_emit.py --match hp3b31`, measured nothing: **`--match` is a
regex on SOURCE ID, not file name**, and no batch-31 id contains `hp3b31`.
Zero rows selected reads exactly like a clean sweep. Its 16 manifests
(`docs/candidate-sources-batch31.<beat>.txt`) were reconstructed from the C on
2026-10-05 and proven by round trip (360 rows regenerate with 0 field
mismatches); the C stays the maintained copy. Reconstructing them found
`EU_VIES_VAT_VALIDATION` unable to form a valid request for any entity (removed;
`corp_identifiers.c` already does VIES correctly), `JP_NTA_INVOICE_ISSUER`
stripping the `T` the API requires, three Socrata pivots without `domains=`, and
four rows that stopped at page 1. With network:

```sh
cd native
python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch31.*.txt --check-filter
python3 tools/audit_batch_emit.py ../docs/candidate-sources-batch31.*.txt --bin ./bin/japanosint --jobs 6
```

`key_env` rows need the key set (the probe sends `{key}` literally) and
`post_body` rows need `audit_batch_emit.py` (the probe only GETs).

Three verifier/engine traps that pass exposed — check for them before trusting
any "verified" number:

* **Non-ASCII URLs.** `urllib` puts the URL in the request line, which must be
  ASCII, so a CKAN `?q=防災` died inside `fetch()` and was logged as a dead
  endpoint. 196 live sources were being discarded by that alone.
* **Empty result sets.** `{"total_count":0,"results":[]}` used to count as
  `json-object`/1 item and PASS — a source verified live that emits nothing on
  every run, forever. Now `EMPTY_RESULTSET` (172 rows across the two batches).
* **Two-level envelopes.** `lib/jsonlist.c` descended one level looking for a
  label; OpenDataSoft puts the title at `metas.default.title`, so every ODS
  catalogue record was dropped as unlabelled despite carrying a real title and
  licence. The envelope list now takes dotted paths.


## Batch tooling

New sources are AUTHORED as a pipe-delimited manifest and scaffolded, not
hand-written: `docs/candidate-sources-batch<N>.<beat>.txt` goes through the
probe/exclusion/reachability gates below and then through `gen_hp_batch.py`,
which is how a beat of 100+ rows gets written at all.

**After a beat ships, the C is the maintained copy and the manifest is the
record of how it was probed.** That is a correction of what this file used to
say ("the manifest is the source of truth; edit the manifest"), and it is a
correction to match reality rather than a change of policy: measured
2026-09-11 across every batch manifest, **48 of 79 beats already differ from
what their manifest generates — 62,461 lines** — and the direction is always
the same. The C holds `id_keys = "公司代號+姓名+職稱"` with the live counts
that proved it (27,528 rows, 26,789 distinct triples); the manifest still says
`公司代號`, the key that lost 97% of them. The manifest format has nowhere to
put that evidence, and a repair without its evidence is unreviewable.

So: **regenerating a shipped beat reverts verified repairs.** Both generators
now refuse to overwrite an existing file and report how many lines would have
changed; `--force` is for the case where the manifest really is the newer copy
(a whole beat re-authored), and a `diff` is the answer every other time. Mirror
an opts change back into the manifest when it is cheap — it keeps the probe
gates honest for a future re-probe — but never at the cost of the comment that
says why.

| tool | what it enforces |
| --- | --- |
| `tools/manifest.py` | THE manifest parser — line split, field count, and `opts` resolution — imported by every tool below. It is one file because it used to be seven, and they disagreed: see rule 4c |
| `tools/probe_hp_batch.py` | proof of life: 2xx, parses in its declared mode, ≥1 real record. Honours each row's own headers, handles JSON/CSV/XML/HTML, rejects empty result sets, HTTP-200 refusals, one-element error arrays and bot-wall challenge pages. A **declared `array_path` is resolved and judged** — it used to hunt for the densest array and once counted a response's own 252-key *schema block* as records, passing a row whose result set was empty. **`--check-filter`** additionally asks each pivot row about an IMPOSSIBLE entity and fails it `FILTER_IGNORED` when the answer is the same size — see rule 4d. Both answers are judged by the same counter (XML/CSV/HTML/xlsx/`array_path` aware); a comparison that could not be made is `FILTER_UNCHECKED`, never PASS. It waits each row's own `timeout_ms`, renders `{date:}` tokens and matches keys case-insensitively, as the engine does. **Paced per host** (2026-10-20, `--host-gap-ms`, default 1000): one request per host per gap across all workers, a 429 or `Retry-After` widening that host's gap for the rest of the run — the probe used to put every worker on one shared host at once and then report ITS OWN burst (502s, 403s) as the row's verdict. Rows on distinct hosts are not slowed |
| `tools/tune_intervals.py` | the authoring-time interval rule (newest item ≤3 d → hourly, ≤30 d → 6 h, ≤1 y → daily, else weekly), applied from the `newest <date>` each feed row's description records, and `adaptive_interval=1` so the scheduler keeps re-measuring it |
| `tools/feed_batch_tsv.py` | assembles `docs/verified-sources-batch<N>.tsv` (the probe's AND the engine's reading per shipped row) and `docs/rejected-sources-batch<N>.tsv` (every candidate that did not ship, by stage) from a feed batch's per-beat artefacts |
| `tools/feed_collision_check.py` | measures a feed row's COLLISION verdict against the feed itself — distinct keys, repeated keys, byte-identical surplus — so a collision is read as the upstream's own duplicate or as a wrong key, never guessed (rule 4b) |
| `tools/feed_reclassify.py` | re-fetches rows whose URL changed after validation and rewrites kind/opts/description from what the server serves NOW (the `&amp;` lesson) |
| `tools/manifest_repair_rows.py` | re-joins manifest rows a multi-line field split, and names the ones still malformed |
| `tools/discover_feeds.py`, `tools/build_feed_manifest.py`, `tools/wd_harvest.py` | the discovery half of batches 43–48: a register of official domains (or a Wikidata class per country) → each root fetched once → declared/linked feeds validated live and written as manifest rows |
| `tools/batch_exclusions.py` | no duplicate id or endpoint against the existing tree or within the batch (normalising `{q}` and `%s` to one form; `.portal` is documentation and is excluded). Sees **runtime-composed** endpoints too — it resolves string macros, joins adjacent literals, follows `#include "*.inc"`, and matches a `%s` URL family on its layer/dataset NAME. Pass **`--bin ./bin/japanosint`**: without it the id set is a regex approximation (4,754 of 13,193) and it says so |
| `tools/audit_batch_pagination.py` | a paged endpoint declares `page_param` or `next_path` — read from the parsed opts, not as a substring of the whole field |
| `tools/audit_batch_reachable.py` | rule 3 above. A row whose opts are ambiguous is reported UNVERIFIABLE, never "never runs" |
| `tools/audit_url_limits.py` | the seventh discard shape — a record limit asked of the UPSTREAM in a URL, with no paging to follow it. Read, not gated: 70 narrow candidates as of 2026-10-20 (103 when written, 89 after the 2026-10-11 pass), and a limit on a ranked pivot is legitimate |
| `tools/audit_cycle_health.py` | whether a `page_resume` row can actually ADVANCE its cycle. Three outcomes, one healthy: `CYCLING` (a position was stored), `COMPLETE` (the walk ended and cleared it — the measured no-op that makes page_resume safe to apply broadly), `STUCK` (a page failed, so no position was stored and the next run re-reads the same slice; the collection past that point is unreachable). Found as the generalisation of the ENA shape. It reports the failing page's STATUS and URL rather than a bare verdict, because a 400 naming a parameter is a property of the endpoint while a 429 is one reading. Walks to the real ceiling rather than probing shallow — the failure can be at any page — so it costs ~20 requests a row. Reads the `page_resume` rows out of the C, which is the maintained copy |
| `tools/audit_idkeys.py` | rule 4b's quietest shape: an `id_keys` naming a field the upstream does not send, which the engine answers by content-hashing every uid — so the row stores its records and a REVISED record lands as a new row instead of an update. One request per row, no database: one page, the first record flattened as `hp_json_flat` flattens it, each token resolved as **`hp_flat_get`** resolves it — exact flat key, else any key whose LAST dotted segment matches case-insensitively. Mirroring that fallback is the whole job: without it the check indicts most of the ArcGIS and GeoJSON fleet (its first version reported 35 rows and 30 were its own fault). It does NOT answer whether the key is UNIQUE — a key that exists and is a dimension loses records just as silently, and that is the registry sweep's COLLISION verdict. Rows come from the C, HP_JSON only, entity/date-token URLs skipped; `--all` drops the `page_resume` restriction |
| `tools/audit_page_ceiling.py` | which scheduled rows stop at the PAGE CEILING, and how many pages their own upstream says they need. Probes SHALLOW (`--probe-pages`, default 3) and then does arithmetic, because the engine's truncation notice already carries the only number that settles it: a `records_available` whose basis is "upstream declared this total". `CEILING_DECLARED` is measured (pages_needed = total / page_size, the page size derived from the probe when the row declares none); `CEILING_UNKNOWN` means the upstream publishes no total and only a full walk can size it, which is a decision to take deliberately, not a finding. Reads `collector-fetch-failure` too, so a 429 is `FETCH_FAILED` and not "this source returns nothing" |
| `tools/audit_cycle_advance.py` | which `page_resume` rows can never advance. Runs every cycling row through the real binary under **`JO_PAGE_MAX=2`** (which brakes a declared `page_max` too since 2026-10-22: `jo_page_max_for`, hptest 40f) and reads the truncation notice back: `STUCK` (page 2 refused with a 4xx that is the endpoint's — re-point the paging), `CUT` (transport/5xx/429/wall — re-run, then a hostgate gap), `ADVANCES`, `COMPLETES`, `FETCH_FAILED`, `NEEDS_ENTITY`, `SLOW`. The engine has disclosed the stuck shape per run since 2026-10-18; this is the first thing that READS the disclosures across the registry. `--from-tables collectors` takes the ids from the C, `--ids-file` for a re-run |
| `tools/audit_page_param.py` | rows whose URL already binds their own `page_param`. The engine used to APPEND (`…&pagina=1&pagina=2`) and a server binding the first occurrence then served page 1 for the whole walk — N pages emitted, one stored, `rc=0`. Fixed in `hp_url_set_param` and pinned by `hptest` "9f-bis"; the lint stays because 103 of 1,431 paged rows are that shape and their paging depends on the replacement being right |
| `tools/audit_page_two.py` | whether page TWO actually returns something different from page 1 — the one question no record count asks. Page 1 is the row's URL as written; page 2 is that URL with the row's own parameter advanced the way `hp_url_set_param` advances it. `SAME_AS_PAGE1` is THE finding (an upstream that takes a page parameter and ignores it answers every page with page 1: N pages emitted, one stored, `rc=0`), `REFUSED` carries a remedy classification, and `PAGE1_IS_ALL` is the benign majority. **`LOCAL_EGRESS` is a verdict about THIS ENVIRONMENT and never about a source** — see the note below on what the first run did to itself. Full run: `docs/page-two-2026-10-20.tsv` |
| `tools/discover_search_endpoints.py` | where a stale SEARCH path went, asked of the HOST rather than guessed: its own search form with the field names a pivot must send, its API/developer links, its declared sitemaps. Batch 42's and 46's lesson one level over. **Read, never gated, and a candidate is not a fix** — of the eight it found for B2's 44 rows that answered 200 with a plausible page, SIX ignore the query parameter (`docs/b2-path-stale-triage-2026-10-21.md`). Everything it finds goes through `--check-filter` first |
| `tools/audit_batch_emit.py` | rule 4 below: runs each MANIFEST row through the real binary and reads back `emitted N of M`. `--timeout S` moves the kill line; a run that hits it is **`SLOW`**, carrying its partial counts — unmeasured, not failed. Each run gets a scratch DB from a warm template (it used to write the developer's live DB), and it exits non-zero on UNREGISTERED, UNPARSEABLE, NO_RUN_LINE or when nothing was measured |
| `tools/audit_registry_emit.py` | rule 4 **and** 4b for the whole REGISTRY, manifest or not — `--list-sources` is the source list, so nothing registered can hide. Measures emitted *and* stored, per run, against a fresh copy of a warm template DB. `--scheduled`/`--match`/`--only`/`--ids-file`, `--jobs`, `--timeout`, TSV out, `--resume`. A MAINTENANCE POD is reported `POD` and not run (2026-10-22): a collector whose name begins with `_` writes its own tables and nothing to `intel_items` by design, so the first registry-wide sweep reported eight of them — the embedding backfill, the translator, the camera stills lane, the vector indexers — as EMITS_NOTHING, which is this tool making the very confusion that verdict exists to expose. **`GATED`** is the same correction for a key-gated row (2026-10-23): it stores one `collector-status-notice` naming the env var it needs — rule 1's gated note — and until the database read mirrored the ENGINE's definition of a notice (`collector-%-notice` plus `collector-fetch-failure`, not a copied list of three kinds) the run line counted that row as a notice while this tool counted it as a RECORD, and the verdict was `SINK_MISMATCH`: "a collector emitting under another id, or a bug", about two rows behaving exactly as designed |
| `tools/diagnose_emit_keys.py` | why a row emitted nothing, and which `title_keys`/`id_keys` fix it |
| `tools/cleanup_stale_uids.py` | the rows identity fixes left in an OLD database: a plain-key row whose qualified siblings now hold the same record (the keyqual collectors and FRA), and wayback_jp's captures from before digest+timestamp+URL. Reports by default. `--delete` removes, in one transaction, only rows whose replacement is proven to exist and that nothing references. `--self-test` runs it on a scratch DB (also from `make unit`) |
| `tools/wp_routes_sweep.py` | the WordPress method as a tool: `sweep` asks each host for its REST root (the homepage's `Link rel="https://api.w.org/"`, else `/wp-json`), its post types and one page of each collection, writing routes.tsv/probe.tsv; `manifest` turns probe.tsv into host-type beats and a rejected TSV. Hosts are normalised (`www.` dropped, one row per REST root) because `funq.jp` and `www.funq.jp` reached the same site and only `batch_exclusions.py` noticed |
| `./bin/japanosint --host-agents` | the per-host User-Agent table (`core/host_agents.def`) as the engine resolved it: host, agent kind, the agent string, and the extra headers an `HA_BROWSER` host gets. The probe reads the same `.def`, so what it prints is what both send |
| `./bin/japanosint --discover-feeds <sites> [--jobs N]` | RSS/Atom autodiscovery in C (`core/feeddisc.c`, `test_feeddisc`): reads each homepage's own `<link rel="alternate">` declarations (relative hrefs resolve against the URL asked for, so a homepage redirecting to a squatter's domain yields nothing), skips WordPress comment feeds, decodes `&amp;`, then fetches every feed through the engine's HTTP client — same agent, per-host overrides and pacing — and prints a TSV verdict per feed: `PASS`, `TAKEOVER_SUSPECT` (gambling/pharma phrases or an unexpected script in the item titles — a question for a human, not a verdict), `REGISTERED` (any source's portal or hp row URL), or the failure. No database |
| `tools/gen_hp_batch.py` | manifest → C, one table per beat, `--prefix`/`--batch` so batches never collide — but the batch NUMBER is yours to keep unique: main shipped a batch 32 while a branch was open with its own, both created `docs/verified-sources-batch32.tsv`, and renumbering the branch to 33 after the fact meant touching the manifest, both TSVs, the table, its array name and two batch citations that had landed in `lib/hpengine.h` and `core/hostgate.c`. Rejects duplicate opts, non-integer int opts, and an opt whose value **swallowed the next one** through a stray `\;`. **Its opt table is a THIRD place a new engine field has to be added**, and it is the one that gets forgotten: `page_resume` reached the engine and 533 rows of C without reaching here, so batch 46 — the first batch to need it — had every such row rejected as an unknown opt. `page_walk` had been missed the same way before it. When you add an `hp_source` field, add it to `STR_OPTS`/`INT_OPTS` in the same change |

