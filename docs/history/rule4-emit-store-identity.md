<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): Rules 4, 4b and 4d: emitting, storing, identity keys, collisions, filters. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

## 4. Fetching is not emitting — prove the second one

`probe_hp_batch.py` proves an endpoint answers and that its body parses. It says
nothing about whether the **engine** turns those records into `intel_items`, and
the gap between the two is not small. Batch 18 was fully probe-verified, and
when its 223 rows were first run through the actual binary, **59 of them fetched
real records and stored none** — `DATAPLANE_TELNET` fetched 36,166 and emitted
0, exit code 0, run reported successful. That is the same invisible nothing as
an `EMPTY_RESULTSET` source.

```sh
cd native && make batch-gates MANIFEST='../docs/candidate-sources-batch<N>.*.txt'   # exclusions, pagination, reachability, probe → .run/batch-gates/pass-ids.txt
python3 tools/audit_batch_emit.py ../docs/candidate-sources-batch<N>.*.txt --bin ./bin/japanosint --jobs 6
```

Run it before believing a batch. `DROPS_EVERYTHING` is the verdict that matters;
`diagnose_emit_keys.py` then separates the three causes, which want different
fixes (an engine bug, a per-row `title_keys`, or a row that is not a record
source at all).

### 4b. Emitting is not storing either

`records=N` counts `emit()` CALLS. The sink upserts on `remote_key`, so a source
whose records key onto each other reports a healthy N and stores one row — and
**every check in this file is blind to it**, because emit really was called.

Measured over a 1,197-source sweep: 46 hpengine rows losing 114,795 records per
pass. `ECDC_RESPIRATORY` emitted 12,648 and stored **31**.

**The same defect existed independently in all three record paths** —
`lib/jsonlist.c` (`us-openfda-device-pma-detail`: 109 emitted, 1 stored),
`lib/hpengine.c`, and `lib/geojson.c` (474 rows rescued across 45 geo sources,
454 of them from one). Fixing one left the others losing data, which is the
strongest argument in this repo for looking for the *other copies* of any bug
you fix. All three now carry a collision guard that flags records colliding
within one page/array and disambiguates them by CONTENT hash — so byte-identical
records still collapse (real dedupe) while records that merely share a key are
all kept. Nothing is invented, and nothing that differs is merged.

The trap that makes this easy to introduce: `id_keys` is a MANIFEST DECLARATION,
not the upstream's identity. Declaring a dimension (`id_keys=country_code` on a
weekly time series) as the record id silently discards the series. When you add
a row, check that its `id_keys` is unique per record, not per group:

```sh
./bin/japanosint --run <ID>          # the run line now states BOTH numbers
sqlite3 $JO_DB "select count(*) from intel_items where source_id='<ID>'"
```

**In `id_keys`, `+` COMPOSES and `,` CHOOSES.** `year+quarter+item` joins all
three into one key; `year,quarter,item` keys on `year` and never reads the
rest. They nest: `a+b,c` = "a+b, or else c". Use `+` whenever identity is a
tuple of dimensions, which is most statistical and regulatory tables.

Getting this backwards fails SILENTLY, and it is the most repeated mistake in
this file's history: 26 rows written in the 2026-09 fix pass used commas
meaning composites. They looked right because the in-page collision guard
content-hashes records that collide inside ONE page — the same key recurring
on a LATER page still collapsed at the sink. `FINRA_OTC_BLOCKS` emitted 2,000
and stored 476 (its distinct first-token values) until the commas became
pluses, then stored 2,000. A single-page `--run` cannot see this; only the
registry sweep can.

The generated `VJSON` fleet has the same question and a smaller answer:
`VJSON` keys on a fixed precedence list (`id`, `uid`, `guid`, `uuid`, `_id`,
`identifier`, `code`, `key`), and **`VJSON_KEYED(..., IDFIELD)`** overrides it
with ONE named top-level field. **`VJSON_IDKEYS(..., IDKEYS)`** takes a
COMPOSITE, with the same `+` semantics as hpengine's `id_keys`
(`collectors/sources/_vjson_idkeys.inc`, a shim sink that replaces the uid and
touches nothing else) — this paragraph used to say "there is no composite form:
hand-write a `run()`", which was true when it was written and has been false
since 2026-09-14. A stale capability claim costs a session the work of
hand-writing what a macro already does. The hand-written form is still there
for a row that needs more than an identity (`geo_tidesandcurrents_currents` at
the top of `collectors/feed/generated/vsrc_environment_3.c`: `pw_walk` +
`jsonlist_emit_ex`). Delete the `VJSON(...)` block you replace; leaving both
defines the source twice and breaks the build.

**And the precedence list finding NOTHING is the expensive case, because it
then keys on the record's first scalar.** Measured 2026-10-22 by the
registry-wide sweep: Catalonia's two climate atlases
(`eur-soc-cat-tqdx-tz5i`, `eur-soc-cat-9ehv-pmf5`) carry
`codi_municipi, nom_municipi, cap_municipi, variable, unitat, valor` and not one
name on the list, so every municipality's whole series collapsed onto the
municipality — 15,152 emitted / 4,615 stored and 3,788 / 1,265, **14,060
records a run**, rc=0 both times. The upstream was measured before the key was
touched, as a collision verdict requires: 947 municipalities (Catalonia's own
count) x 16 and x 4 variables = exactly the emitted counts, and
`codi_municipi+variable` is distinct on all of them. After:
15,152 / 15,152 and 3,788 / 3,788.

**The same sweep then found eleven more of it on one portal**, Colombia's
`datos.gov.co`, and ten are fixed for **16,545 more records a run** — 30,605
with Catalonia's two (`docs/OPEN_ITEMS_2026-10-20.md`, item E7). Three things
from that pass generalise:

* **The minimal key a search finds is not the record's identity.**
  `fechaobservacion` alone is distinct over 1,000 IDEAM weather observations and
  is the WRONG key: a station network records many stations at one instant, and
  the sample only looks unique because it is ordered by `:id`. The right key is
  `codigoestacion+codigosensor+fechaobservacion`, which is also 1,000 of 1,000.
* **A longer key is not automatically a better one.** The one row left unchanged
  is `lat-co-dispositivos-med`: its best reachable key is 961 of 1,000, no
  better than the 3.85% it already loses, and reaching 1,000 of 1,000 needs nine
  fields of which **539 of 1,000 rows are missing at least one** — a key that
  degrades for half the collection. INVIMA's device register does not expose its
  own record granularity, and writing that down is the result.
* **Measure the key over TWO pages, not one.** A key unique inside one page and
  repeating on the next is the exact shape rule 4b's comma-vs-plus note
  describes, and a single `--run` cannot see it.

**A THIRD portal family then taught the opposite lesson, from the same
symptom** (2026-10-22): 32 North American Socrata city rows reading COLLISION
for 24,536 records a run. Nineteen ship on the dataset's own documented row id
for **22,676 records a run** — 53,281 across the three families — and the three
findings that generalise are:

* **TWO PAGES CAN BE TOO FEW.** `id` is distinct on all 800 sampled Chicago
  energy-benchmarking records and collapses 4,500 of 8,000 in the engine,
  because the first 800 are all `data_year` 2019 and the dimension that breaks
  the key does not appear until page four. `id+data_year` stores 8,000 of 8,000.
  A sample that never reaches the breaking dimension reads exactly like a unique
  key.
* **THE CONTENT-HASH FALLBACK CAN BEAT A "REAL" ID.** Four of the 32 were
  changed, measured, and REVERTED because the id made it worse:
  `nam-chicago-crimes-recent` (`case_number` 6 collisions against the
  fallback's 2 — CPD files several offences under one case),
  `nam-chicago-towed-vehicles` (140 against 78),
  `nam-norfolk-streetlights-and-outages-reports` (168 against 155) and
  `nam-providenceri-providence-tree-dataset` (52 against 4). Where a record has
  no stable identity, VJSON's fallback hash of (title, link, date) separates
  rows that a weak id merges. **Compare the proposed key against the row's
  CURRENT loss and keep the fallback when it wins** — the opposite of what
  Catalonia and Colombia needed, reached from an identical starting symptom.
  Nine of the 32 keep their fallback for that reason, each with its arithmetic
  in `docs/OPEN_ITEMS_2026-10-20.md`.
* **A minimal-key search offered `shape_len` (a geometry LENGTH),
  `offense_date` (a timestamp) and `link` (a URL)** as unique keys on three of
  these datasets. Same rule as the IDEAM timestamps, three more times.

**TWO SIBLING ROWS ON ONE PORTAL, ONE NEEDING A KEY AND ONE NEEDING TO KEEP
THE FALLBACK** (2026-10-25, Colombia's SECOP I and II on `datos.gov.co`). Both
read COLLISION, and the reading that decided each is the row's CURRENT loss
rather than how good the candidate key looks:

* **`gov-co-secop1` lost 294 of 1,000 a run.** No field on a SECOP record is in
  VJSON's uid precedence list, so it fell back to hashing (title, link, date) —
  and no TITLE key matches either, so the label is the record's FIRST SCALAR,
  which is `entidad`: the contracting authority. Every procurement DANE
  publishes shared one uid. Measured over twenty pages, 1,000 records: 979
  distinct whole records and `id_del_proceso` is **979**, equal to it, so it is
  the identity and the 21 repeats are the upstream's own bytes. Keyed:
  **707 → 981 stored** (979 records plus two notices).
* **`gov-co-secop2` lost 2 of 1,000 and KEEPS the fallback.** Same portal, same
  shape, opposite answer: 1,000 records, 1,000 distinct whole, and the best
  field key is `numero_del_contrato` at **943** —
  `codigo_entidad_en_secop+numero_del_contrato` and
  `numero_de_proceso+numero_del_contrato` are also 943. Declaring the "real" id
  would lose 57 records a run where the content hash loses 2. **A contract
  number is not unique in that register and the register publishes nothing that
  is**, which is the result, and the row says so.

Two process notes from doing it, both of which nearly cost a wrong change.
`numero_del_contrato` is 250 of 250 over 250 records and 943 of 1,000 over a
thousand — **"two pages can be too few" is not a special case, it is the normal
case**. And the engine run that "proved" the first fix stored 705, two fewer
than before: there was a COMPILE ERROR in the file, so the binary was the
previous one. `VJSON_IDKEYS` lives in its own include
(`collectors/sources/_vjson_idkeys.inc`), not in `_verified_macros.inc`, and a
file using it has to pull it in — the error names a column in the macro's
argument list, which reads like a bad argument count rather than a missing
definition. **Check the build's own exit before believing a measurement taken
after it.**

**A FIELD CALLED "UNIQUE IDENTIFYING NUMBER" IS UNIQUE WITHIN A SESSION**
(2026-10-24, `collectors/pivot/table/hp3b20_parliament.c`). UK Parliament's
linked-data API gives every question a `uin`, which is what Hansard calls it
and what the row keyed on — and it RESTARTS each parliamentary session, so it
identifies a question within a session and not a record. The upstream proves it
on its own filter endpoint in one request: `commonsoralquestions.json?uin=901763`
answers `totalResults: 8`, eight different questions tabled between 2024-02-27
and 2026-10-22; `uin=HL1000` answers 11 Lords written questions. `_about` — the
resource URI the envelope gives every record — is distinct in each of those
answers and over a 10,000-record walk. **Ask the upstream for ONE value of the
key you are about to trust**; a name is not a measurement, and a dimension that
is genuinely unique per session looks unique in every sample taken from one.

Three things about it generalise past Westminster:

* **The fix already existed, in the same file, for one of seven rows.**
  `UKPARL_LDA_EDMS` carried `id_keys = "_about"` under the comment "edmNumber
  restarts every session" — written a month earlier, applied to that row, and
  not to its six siblings, four of which key on `uin`. This file's "look for
  the other copies" lesson at its smallest scale: the other copies were
  twenty lines away.
* **The symptom's SIZE depends on the page order, so it is not a measure of
  the defect.** The sweep read 1,018 of 10,000 collapsing under the upstream's
  default (newest-first) order; the ascending walk the cycle needs makes it
  **1,910**, because sorting by date groups each session's reused numbers
  together. A collision count is a signal that the key is wrong, never a
  budget of what it costs.
* **Three of the four rows read OK, and were just as broken.** Their first
  twenty pages are one chronological slice where a session's numbers do not
  repeat, so `stored == emitted` on run 1 of a cycle whose later runs would
  collapse. A `page_resume` row's key must be unique over the WHOLE
  collection; measuring run 1 cannot see that.

Measured after: 10,000 of 10,000 on all four, and 20,000 distinct over two
consecutive runs of the oral-questions row against one database with the
position advancing `_page=20` → `_page=40`.

**AND THE SAME LESSON AGAIN A DAY LATER, WITH ITS ANSWER TEN LINES AWAY**
(2026-10-25). `TX_TCEQ_NOTICES_OF_ENFORCEMENT` keyed on `noe_id`, and a NOTICE
of enforcement covers several investigations — so it identifies a notice and
not a record, exactly as a `uin` identifies a question within a session. The
upstream proves it in one request: noe_id 459,430,322,011,133.00 carries two
rows differing in `invstn_no` (920618 against 1120136) and in the docket's own
date suffix — two real investigations under one notice, not a repeat. Over the
whole 20,000-row read: 20,000 distinct WHOLE records against 19,595 distinct
noe_id, and the best composite the columns admit (`noe_id+invstn_no`) is still
three short. The fix is `:id`, Socrata's own row identifier, 20,000 of 20,000
distinct — **which the sibling `TX_TCEQ_VIOLATION_CITATIONS` ten lines below in
the same file had keyed on all along**, the `_about` case repeating at the same
tiny scale. It needs `$select=*,:id`: Socrata does not serve `:id` unless it is
asked for. Measured after: 20,000 emitted, 20,001 stored, the +1 being the
notice that discloses the 9,092 the cycle still owes.

**The opposite verdict on the same symptom, measured the same hour.**
`PUBDEV_ALL_PACKAGES` loses ~655 of 93,353 a run and its key is RIGHT:
`/api/package-names` declares exactly the 93,353 the row emits, and a registry
namespace cannot repeat a name. The cause is this file's newest-first hazard —
page 1 carries packages published today and page 900 carries 2019 — so each
package published during the four-minute walk is inserted at the HEAD and
shifts one record across every page boundary still below it; seven
publications with ~93 boundaries left each is the 655. No sort exists to pin
it, established with controls: four sort spellings AND `frobnicate=7` AND
`sort=notacolumn` each return a page byte-identical to the plain one, so the
endpoint discards its query string entirely. Re-keying would turn one
package's two sightings into two packages. **So a collision rate says nothing
about which of the two findings you have** — one key was a dimension and one
was correct, and only the upstream's own count told them apart.

**AND THE DECLARED TOTAL WAS ONE LEVEL DOWN IN THE ENVELOPE.** All seven rows
said "the upstream declares no total", which was true of what the ENGINE could
see: `lib/jsonlist.c`'s shared list carried `totalResults` at the document root
and `result.count`, not the Linked Data API's `result.totalResults`. Added
there rather than in a row, for the reason `response.numFound` is there — it is
the API family's envelope — and the rows now disclose their real remainder:
71,323 Commons oral questions, 649,660 Commons written, 107,034 Lords written,
604,935 answers, 45,235 early day motions. It also exposed a page ceiling that
had been invisible for exactly the reason this file names: **`_pageSize` is
IGNORED on the `edms` endpoint alone** (it echoes `itemsPerPage: 100` for every
value from 100 to 1,000, where `commonsoralquestions` honours 1,000), so the
row asked for 500, got 100, and twenty pages came to **2,000 — ceiling x page
size, never an upstream's own total** — against the 45,235 it now names. At 453
pages that is over the 200-page cost ceiling, so it keeps its cycle with
`page_max = 200`: 20,000 records a run and three runs to the whole series,
measured at 200 requests and 8.3 MB.

The two divisions rows are the **control** and keep `uin`: their collections
read whole (5,553 and 2,089, each equal to its own declared total) with every
uin distinct, so a division number IS a record identity. The sort parameter was
confirmed the way batch 55's was — `_sort=notacolumn` and `frobnicate=7` are
both HTTP 400, so this endpoint validates its query string and an accepted
`_sort` is being honoured rather than ignored.

If the second number is smaller than the first, the row's identity is wrong.

**The run line says it without being asked.** `core/intel.c` counts the DISTINCT
uids a run upserts and `core/scheduler.c` prints it, so the two numbers arrive
together and the defect is legible without a second command:

```
[sched] ANTARES_LOCI run rc=0 records=1001 7388ms stored=1001
[sched] WHO_XMART_WHSA_FACT run rc=0 records=10001 7935ms stored=2 \
        UID-COLLISION: 9999 of 10001 emitted records collapsed onto a uid already written this run
```

`stored` is DISTINCT-UID, not rows-inserted, and the difference matters: a
scheduled source re-fetching an unchanged feed inserts nothing and updates
everything, so rows-inserted would report total loss on every ordinary re-run.
A metric that cries wolf on every re-run is one nobody reads when a real
discard happens. Distinct-uid is stable across re-runs and moves only when a
run's own records collapse onto each other. It also lands in
`fetch_log.stored` (NULL = not measured, negative = a floor), so the history is
comparable and not just whatever run a human happened to watch.

The `stored=` field is appended AFTER the duration deliberately — eight parsers
in `tests/audit/` and `tools/` match `records=(-?\d+) (\d+)ms` as one unit.
Two later fields follow it the same way. `notices=N` counts the
collector-*-notice records, and UID-COLLISION is judged against stored MINUS
notice uids (a notice used to hide one collision). `failed=N EMIT-FAILED: …`
counts emits the sink refused (SQLITE_BUSY, a failed COMMIT): before it, a
lock held by the WAL checkpoint pod lost records with nothing on the run line.
`stored=` itself still equals the DB's distinct-uid count.

**A UID-COLLISION of exactly one page size on a `page_walk` row is the walk
colliding with itself, not a key defect.** A walk whose URL carries no page
number fetches it bare and then asks for `page=1`, because which base the API
uses is not knowable from the URL (the probe under `page_walk` above); a
1-based server answers page 1 with the page already read, the repeat guard
recognises it and the walk goes on — but BOTH pages were emitted, so one page
size's worth of records collapse onto uids this run already wrote.
`us-sf-311-open311-requests` reported 107 collapsed across 39 pages and
**exactly 100 across 16** (measured 2026-10-24), and binding `&page=1` into the
URL took it to 14. So subtract one page size before reading a collision on such
a row, or bind the first page and remove the artifact — and do NOT widen
`id_keys` to chase it, which is the same mistake the inclusive-cursor note
makes one mechanism over.

**That shape needs THREE conditions together, and the fix is in the probe, not
in rows.** A full first page (a short page ends the walk, so no probe is ever
issued — `us-austin-apd-crime-arcgis` reads 297 of a declared 1,000 and is
therefore immune, which also means RAISING a page size below a collection's
true size can turn a safe row into an exposed one); no page number bound in the
URL; and a delay between the two requests long enough for the collection to
move, which on an hpengine row means a detail hop, because `hp_emit_record`
spends the detail budget per record INSIDE the page loop. Measured on the
registry: of 351 hp rows declaring `resultRecordCount`/`maxFeatures` with a
page cursor, only THREE also declare a `detail_url` — `CA_CALGEM_WELLS` and the
two `IMF_PORTWATCH_*` rows, all at `page_size=1000`, so the false collapse
would have been 1,000 records rather than 100. Their retained `fetch_log` shows
it never fired: every recorded run has `stored` at or just above
`records_fetched` (the surplus is notice uids). `log-retention` prunes that
table, so absence there is not proof of never — but it is the only evidence
available and it points one way.
The remedy is `jsonlist_page_fps()`: the probe's verdict is settled by record
IDENTITY — each record's own content hash, as a SET — because a reorder cannot
move a set, while the ordered 8-record `jsonlist_page_fp()` keeps its original
and correct job of asking "has the server re-served the page we just read".
And the re-served page is NOT skipped wholesale: a record that arrived between
the two requests is on that response and on nothing read so far, so dropping
the page would discard it. The probe emits the complement. `hptest` 28c (pure
reorder), 28d (a new record at the head of the re-served page) and 31e (the
same in `jsonlist_emit_paged`, because a shared decision needs both copies
fixed) pin all three halves.

**What is left after that subtraction is told apart by the run's DURATION, not
by the count.** The same row read 98 distinct of 100 in 26 s and 66 of 100 over
164 s: an index with no stable order reshuffles under a slower walk, so a key
defect gives the same number twice and a reshuffle does not. The `secs` column
in `audit_registry_emit.py`'s output therefore separates "the upstream
reordered under us" from "this row's identity is wrong" with no sort probe at
all — and it is the reason a fast probe cannot certify a slow walk. When the
answer is the reshuffle, check that a sort is actually available before
declaring it fixed: **a 200 is not evidence a sort landed.**
`mobile311.sfgov.org` accepts `sort`, `order` and `order_by`, answers 200 to
each, and honours none.

Sweep the whole registry for both failures, not just a batch:

```sh
python3 native/tools/audit_registry_emit.py --bin ./bin/japanosint \
        --scheduled --jobs 6 --timeout 220 --out sweep.tsv
```

It runs each source against a fresh copy of a warm template DB and reads the
stored count back out of that database as well as off the run line — two
independent readings, because a checker that believes one self-report is how
260 silent sources got through in the first place. Verdicts: `OK`,
`EMITS_NOTHING`, `COLLISION`, `SLOW` (hit `--timeout`; unmeasured, not failed),
`NEEDS_ENTITY`, `SINK_MISMATCH`.

### 4d. Answering is not answering THE QUESTION

Every gate above counts records. None of them asks whether the records are about
the thing you asked for. Three APIs in batch 21 accept a filter, **silently
ignore it, and return the whole unfiltered collection with HTTP 200**:

* EPA Envirofacts, given a column that does not exist
  (`tri_reporting_form/facility_name`) → 10,000 unrelated records
* the German BMJ portal, on `court` / `documentNumber` / `dateFrom`
* Health Canada MDALL, on an unvalidated query → the entire 21 MB table

probe PASS. emit OK. `stored == emitted`. Every gate green — and the row is an
ENTITY PIVOT, so an analyst asking "what do we have on X" gets thousands of
records about everything else, attributed to X. **That is worse than a source
that returns nothing: it is a confident wrong answer**, and no amount of record
counting can see it.

The check is one extra request: ask the same endpoint about an entity that
cannot exist. A working filter returns nothing, or something much smaller. A
filter being ignored returns the same collection it just returned for the real
entity.

```sh
python3 native/tools/probe_hp_batch.py docs/candidate-sources-batch<N>.*.txt --check-filter
```

Opt-in because it doubles the request count for pivot rows. Run it at least once
per batch, and treat `FILTER_IGNORED` as fatal — the row must be dropped or
re-pointed at a parameter the upstream actually honours.

