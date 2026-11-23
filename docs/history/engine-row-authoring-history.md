<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): engine subtleties for hpengine rows, page_resume / window_days measurements, paging. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

* `page_start`'s unset value is 0, which is also a legitimate first page. Set
  **`page_zero_based=1`** for a 0-based API — otherwise the engine coerces the
  start to 1 and silently never fetches page 1.
* **`page_size` makes `page_param` an OFFSET.** Declare it only for offset
  parameters. On a page-NUMBER API (`page`, `pageNo`, `PAGENO`) it requests
  pages 1, 1+N, 1+2N … and drops N−1 of every N pages with the run green.
  83 shipped rows were in that state until 2026-10-05; `AP19_CN_SZSE_LISTED`
  stored ~160 of 2,904 listings. Put the page size in the URL instead.
  `make lint-sources` now fails the shape (`page-size-on-page-number`), and
  on the merge with main it caught 97 more that batch 36 had shipped the
  same day: every World Bank (`per_page`) and Socrata `views.json` (`limit`)
  row. `US_SOCRATA_CAMBRIDGEMA` stored 100 datasets and reported "emitted 100
  of 100 available"; without `page_size` it stores 446. An
  API whose `page` really is a record offset (MapRoulette) says so with a
  `page-offset-ok: <measurement>` marker in the row. And check where a
  page-number API STARTS: a 0-based one (Spring, DSpace) needs
  `page_zero_based=1`, or the walk goes page 0 → page 2.
* **A file re-published under a new name each edition** (monthly attachment
  ids) is not a static `url`: declare **`index_url`** (the landing page) and
  **`index_href_must`** (a substring of the right link). The engine follows it
  every run, and the generator and probe accept both. Batch 32's Niigata
  register 404'd three days after shipping without it.
* **Never freeze a date into a URL.** `{date}`, `{date-7d}`, `{date-1m:%Y%m}`,
  `{date:@ms}`, main's `{date:%Y%m%d:-2}` and `{ago:N}` (lib/datetok.c — one implementation for hp rows AND the VJSON
  fleet's `vsrc_url_dates`) render in UTC+**`tz_minutes`** (540 Japan, 480
  China). **`date_back=N`** steps the date back when the first page is a 4xx
  or empty (weekends, holidays, not yet published), in units of
  **`date_step`** (d/w/m/y — `y` for EPA's TRI reporting year). 43 rows had a
  window frozen at authoring and re-fetched the same past week forever. A
  dated file whose ROWS carry no date (SHFE's daily kx, SZSE margin) re-keys
  every day onto yesterday's uids: **`date_field=<name>`** stamps each record
  with the rendered date (after any step back) so `id_keys` can include it.
* **JSONP is unwrapped automatically** (`cb(…)`, `null(…)`, `(…)`, `)]}'`).
* **A page that lives in a POST body or a URL path** is `{page}` / `{offset}` /
  `{offset1}` / `{offset_end}` / `{size}`, re-expanded every page in the url
  AND the body (EPA Envirofacts: `rows/{offset}:{offset_end}`). A body with a
  literal `"page":1` reads page 1 forever: four pivots did, 40 records each.
* A failure on the page AFTER a short page (fewer than `page_size` records) is
  the upstream's end of data, not a truncation; later pages get 5 retries.
* **`page_delay_ms=N`** paces a walk (a pause before every page after the
  first) for a firewall that answers a burst with 403: the Palace Museum's
  blocked us after 31 of 9,300 pages.
* **A register published as an HTML table** is `mode=html` +
  **`table_must=<caption or header text>`** (`table_index`,
  `table_skip_rows`): every row becomes a record keyed by its header cells
  (lib/htmlparse.c `html_table_parse` + hp_run_table; colspan/rowspan,
  entities, nested tables handled; the row's first link is `link`). Before it,
  html mode read anchors only, and ~20 tabular registers were rejected.
* **A bare root array must be DECLARED: `array_path=.`** is the document
  itself. With no `array_path` the engine picks the densest array of OBJECTS,
  which is not the root as soon as a root record carries a longer nested array
  of its own. hex.pm returns 100 packages per page, each with its own
  `releases` list, so on a 60-page walk the engine mined `[33].releases` (133)
  over the root (100) on 34 of the 60 pages: 9,643 records emitted where the
  pages hold 6,000, keyed on a field the release entries do not carry, 2,881
  collapsing at the sink. It is disclosed — a `collector-shape-notice` naming
  the array mined and the runner-up — but a notice is a row in the database,
  not a build error, so a row nobody reads after its first run keeps doing it.
  Six rows in batch 33 were that shape. `hptest` pins both halves, the hijack
  and the fix.
* **An array of bare JSON STRINGS is a record set** — `title_keys=value`,
  `id_keys=value`. `hp_json_flat()` maps a string WITH CONTENT to
  `{"value": "<the string>"}`; the `empty` counter is for a NULL, a number or
  an EMPTY string (the trailing newline of a CSV), which is what the comment
  beside it is about. Four live sources — the Microsoft Container Registry
  catalogue, NuGet's flat version list, and the complete Packagist and pub.dev
  name lists — were written off in `docs/rejected-sources-batch34.tsv` on that
  comment read as a capability claim, and all four store exactly what they
  emit (3,851 / 86 / 464,521 / 91,388). **A capability claim read off a comment
  is not a measurement**, and the measurement is one `--run` away.
* **`mode=ndjson`** (`HP_NDJSON`) for one complete JSON value per line, no
  enclosing array and no commas — the shape `index.golang.org/index` and
  `index.crates.io` publish. Declared `json` the body dies at line 2 and the
  row emits nothing forever; declared `csv` every line is one unqueryable
  cell, which is the mistake `IANA_LANGUAGE_SUBTAGS` made. It works by
  rewriting the lines into ONE array and handing that to the JSON path, so
  every key list, the collision guard, detail hops, caps, paging and the
  disclosure notices apply unchanged. A line that is not valid JSON is a LOST
  RECORD, counted into `malformed`, reported in the run line and disclosed as
  a truncation notice. Note the parse is `require_null_terminated`: plain
  `cJSON_Parse` accepts `{"a":1} oops` and copying that line's text into the
  array then poisons the whole array, taking every good line down with the
  one bad one.
* **A CONTAINER'S OTHER MEMBERS ARE RECORDS TOO: `xlsx_sheet_all = 1` and
  `zip_all_entries = 1`** (2026-10-22). One fetch buys the whole container and
  the engine was reading one thing out of it — the same shape twice, found while
  closing open item B2's long tail:
  * PMDA publishes its annual drug approvals as ONE WORKBOOK WITH ONE SHEET PER
    MONTH, and a new sheet appears every month, so a row pinned to a sheet name
    goes stale in weeks and a row pinned to index 0 reads May for ever. Neither
    failure shows in a record count. `xlsx_sheet_all` reads every sheet, each
    with its OWN header row — which matters because the two sheets of one
    workbook can have different geometry, as the ECB's do.
    `JP_PMDA_NEW_DRUG_APPROVALS` stores 95 of 95, the sum of its four sheets'
    data rows exactly.
  * The Assemblée nationale publishes its written questions as 19,165 JSON
    documents inside one 50 MB ZIP, so `zip_first_entry` was 19,164 records
    dropped — **and the engine's own `zip-extra-entries` notice had been saying
    so for as long as the row existed**, offering remedies ("a named-entry read,
    or one row per member") that do not reach 19,165 members. A notice nobody
    acts on is not a disclosure. `zip_all_entries` (HP_JSON / HP_NDJSON only,
    because concatenating CSV members would read the second header as a record)
    reads every member, one inflate at a time: `FR_AN_QUESTIONS_ECRITES` stores
    19,165 of 19,165 in one request. `lib/zipread.c`'s named and indexed lookups
    are ONE central-directory walk, not two.

  Both file a notice naming a member they could not read, and the records of
  every member land in ONE page so the collision guard sees the whole container.
  The row's `id_keys` must be unique across the whole container, not within one
  sheet or member. `hptest` 50–50e and 51–51c.

  **And looking for them found the old path lying.** HP_XLSX's single-sheet
  failure set `out = -1` under a comment calling it an honest failure, and
  `out = st.emitted` three lines below overwrites every mode driver's return —
  deliberately, so a later page's failure cannot erase the earlier pages' count.
  So a workbook that would not convert AT ALL reported `rc=0 records=0`. The
  run's own `hard_error` flag is what survives a mode driver's return value;
  `hptest` 50e fails under the old line.
* **`row_header_path` keys a JSON TABLE by name** (2026-10-21). Eduskunta's
  `avoindata` API and the Austrian Parliament's filter API both answer with an
  array of ARRAYS and put the column names in a sibling array, and the engine
  flattened such a record POSITIONALLY — so a row would key on positions whose
  meaning the upstream can change silently. It is the `csv_no_header` shape in
  JSON clothing. Name the sibling (`row_header_path = "columnNames"`) and the
  cells are named; a cell past the header's end keeps its positional name rather
  than being dropped. `hptest` 49–49f.
* **`next_path` takes a `$last` segment** — the final element of an array —
  for an upstream whose cursor is the last record's own field rather than
  anything in an envelope. The Go module index publishes no next link at all
  and pages by `since=<the Timestamp of the last record it gave you>`, and no
  numeric index can name that element because the array's length is not known
  when the row is written.
* **`{ago:<seconds>}`** expands to RFC 3339 UTC of (now - seconds), the one
  token not derived from the entity. It is for an upstream whose only ordering
  is "everything at or after this instant", with no reverse order and no
  `latest`: the Go index is append-only from 2019-04-10, so a row without the
  anchor walks forward from 2019 on every run, emits 40,000 records an hour
  and never reaches today. `since={ago:7200}` makes the same row the live
  publish tail. **Check this when a feed's URL has no entity token and its
  interval is short** — a source that emits plenty and tells you nothing new
  is as invisible as one that emits nothing.
* An **INCLUSIVE cursor** repeats one record per page boundary (the Go index
  hands back page N's last record as page N+1's first), and the sink collapses
  it as the byte-identical duplicate it is. Do not widen `id_keys` to "fix" a
  `stored` that is a handful short of `emitted` on a cursor-paged feed —
  measure the upstream first.
* **A later page that fails is disclosed, not a quiet end** (2026-10-06,
  `hp_later_page_cut`). After a SHORT page any failure is the end of data.
  Otherwise transport errors, 3xx/304, 401/403/407, 408, 429, 5xx and an
  unreadable 200 always mean the walk was cut short, and other 4xx do when the
  upstream showed more (its total, a next link, a full previous page). A cut
  walk files a truncation notice naming `failed_page`, `failed_page_status` and
  `failed_page_url`; VJSON, geojson and `pw_walk` name the same three. A
  row-declared conditional header goes on page 1 only.
* **A detail hop carries the row's headers** (token, UA, Accept), minus the
  POST body's content type. It used to carry none, so a header-authenticated
  row stored `_detail_error` on every record (PY_AIP: 500 without, 200 with).
* **A detail hop that answers HTML** is `detail_html_kv=1`: the record page's
  label/value cells merge under `detail.` — every cell with a non-empty
  `data-label` (the accessible responsive-table pattern; an EMPTY data-label
  is the visible label column and is skipped), and a `<th>` paired with the
  `<td>` after it. A repeated label is indexed (`Expiration Date.1`), never
  overwritten; a page with no such cells is `_detail_error` on the record.
  Opt-in, because navigation tables are cells too. Without it an HTML detail
  body is "not JSON; nothing merged", which is why the FAA owner search
  stored a summary row and no registration. `hptest` 41 pins it.
* **XML records keep every field**: repeated children are indexed (`name`,
  `name.1`, `author.1.name`), text beside children is `<key>.#text`, and
  whatever the depth (32) / field (`HP_MAX_PROPS`) bounds keep out is counted
  into `_fields_dropped`. **An HTML link takes its strongest label** (own text
  > image alt > borrowed text, a label that contains another is kept), the
  others the page gave it land in `other_labels.N`.
* A row that declares no paging does ONE request. **`page_walk=1`** (HP_JSON
  only) walks it the way a VJSON collector is walked instead — the server's own
  next link, else the cursor its declared page size pairs with, advanced while
  pages come back full — through `jsonlist_next_page()` in `lib/jsonlist.c`,
  the one copy of that decision. The 322 batch-16/17 rows moved onto hpengine for
  their detail hops (`collectors/gen_detail_hop_upgrade.py`) carry it so they
  did not trade their later pages for the second hop.
* **`page_walk` moves only a cursor whose page-size sibling it KNOWS**
  (`PAGERS` in `lib/jsonlist.c`: `rows`/`start`, `limit`/`offset`,
  `per_page`/`page` …). `count`/`start` — the GOV.UK Search API's pair, also
  J-STAGE's and CiNii's, with WFS 2.0's `startIndex` as the alternate — was
  not in it until 2026-10-20, so six GOV.UK rows declaring `page_walk=1` AND
  `page_resume=1` read ONE page of three records a run, and cycled nothing,
  because a one-page walk "completes". The ceiling audit had said so
  (`pages_read 1` against `pages_needed 44,885` for the employment-tribunal
  row) and it read as a row waiting for its cycle. Measured after:
  `uk-govuk-search-cma-cases` resumes at `start=60`, then `start=120`; the
  six are at `count=100` with an ascending sort now. **A walk that reads
  exactly one page of a declared size deserves one look at that table.**
* **And `jl_declared_size()` used to consult a SECOND list that
  `jsonlist_next_page()` never walked — so "the engine knows this size" and
  "the engine can move this size" were two different facts that read as one.**
  `PAGERS[]` pairs a size spelling with a cursor and is the only list the walk
  iterates; an `EXTRA[]` list held the spellings "that have no cursor sibling
  this walk can move, because 'the page came back exactly full' is evidence of
  more data whether or not we know how to ask for it". A row whose size sat
  there made ONE request per run, forever, and disclosed the full page as
  truncation — honest, and the data past page 1 never fetched.
  `resultRecordCount`/`resultOffset` (ArcGIS REST) and
  `maxFeatures`/`startIndex` (WFS 1.x) are PAGERS families as of 2026-10-25,
  measured one URL per host first (`resultOffset=0` against `=2`, bodies then
  feature ids: disjoint on 47 hosts, overlapping on 4, ignored on 4, refused on
  5 — `docs/arcgis-resultoffset-probe-2026-10-24.tsv`), and EXTRA is empty and
  gone. **The lesson is not the two entries, it is how badly the population was
  mis-read before it was counted.** 659 scheduled rows declare one of those
  sizes, and the first pass concluded all 659 therefore read one page. They do
  not: 351 are hpengine rows that declare `page_param=resultOffset` themselves,
  300 are VGEO rows walked by `geojson_emit_paged()` — which is not
  `jsonlist_emit_paged()` and already advances `resultOffset` on ArcGIS's own
  `exceededTransferLimit` — and **three** rows reached the table. Those three
  were each reading a single page: two VJSON rows asking for
  `resultRecordCount=1` of 6,072 and of 29,208 records, and the one
  `page_walk=1` row asking for 2. So **read the row's own opts and its
  CONSTRUCT before concluding anything from its URL**: a size parameter says
  what the row asked the server for, never which engine will walk it.
  `tools/audit_url_limits.py` tells its reader to discount `--generated`
  because "the `vsrc*` fleet pages through `jsonlist_emit_paged()` at the macro
  level"; that is true for a size in PAGERS, was false for one in EXTRA, and is
  false for a VGEO row either way. **"The macro pages for it" is a claim about
  the PARAMETER and the CONSTRUCT, not about the macro.**
  Full re-derivation, both of its own defects included:
  `docs/jsonlist-nonadvanceable-page-size-2026-10-24.tsv`.
* **An offset walk needs a stable sort, and the sort field is the layer's to
  declare — across 639 ArcGIS layers the object id has TWELVE spellings.**
  `OBJECTID` 323, `ObjectId` 67, `FID` 49, `objectid` 35, `ESRI_OID` 7, `OID`
  6, `OBJECTID_1` 4, `fid` 2, plus `OBJECTID_12_13`, `OBJECTID_12`, `ObjectID`
  and `RowID`. A batch writing `orderByFields=OBJECTID` would have been wrong
  on 35% of them, and a wrong field name is an ArcGIS ERROR — it turns a row
  that reads one page into a row that reads nothing, which is worse than the
  bug being fixed. The layer publishes the answer (`<layer>?f=json` →
  `objectIdField`, `advancedQueryCapabilities.supportsPagination` /
  `supportsOrderBy`, and `maxRecordCount`, above which ArcGIS clamps silently
  so every page comes back short and a walk reads a short page as the end of
  the data). 110 layers publish no `objectIdField` key at all and **all of them
  declare one by TYPE** (a field whose `type` is `esriFieldTypeOID`), so the
  count of unsortable layers is zero — asking the second question is the
  difference between 110 "cannot be fixed" and 110 fixable.
  **And prophylaxis is not a repair.** 122 rows page by offset with no order
  clause; one row per host, four requests each, object ids compared, measured
  **ALREADY_STABLE on all 15 hosts that have a seam** (unsorted overlap 0 of
  50) with the sort accepted and honoured everywhere and **0 records
  recovered**. Adding the clause is still what this file asks for and it is
  free, but it must not be reported as data recovered. One honest limit on that
  reading: those requests completed in 10 s, and a collision on an unordered
  index grows with walk DURATION (below, rule 4b), so a fast probe is the fast
  case and cannot clear a walk that takes minutes.
* **The page ceiling is one number with one knob: `jo_page_max()`, 20, under
  `$JO_PAGE_MAX`** (`lib/pagewalk.c`). It was three, with two numbers and two
  knobs, one of which did not exist — `pagewalk.c` 20/`$JO_PAGE_MAX`,
  `jsonlist.c` 20/`$JO_JSONLIST_PAGE_MAX`, and `hpengine.c` **10 with no
  variable at all**. So `JO_PAGE_MAX=50` raised the generated fleet, silently
  did nothing for `page_walk` rows, and could not move a row that declares its
  own paging at any value. That third group is the largest: 1,919 scheduled rows
  declare `page_param`/`next_path`/`{page}` with no per-row `page_max`, and
  stopped at ten pages on a constant none of them mentions.
  `JSTAGE_CORPUS_BULK` went from 1,000 records to 2,000 on the unification, and
  its own notice had been saying `declared_max_items: 20000` the whole time.
  The legacy variable still works, so an operator who set it is honoured rather
  than ignored. Twenty-two comments in 14 collector files cite measurements made
  under "the default 10-page ceiling": those are dated records, not claims about
  today.

  **`$JO_PAGE_MAX` is the operator's brake and reaches a row that declares its
  own ceiling — in both directions** (fixed 2026-10-22; it is one function,
  `jo_page_max_for(declared)` in `lib/pagewalk.c`, called by `pagewalk.c` and
  `hpengine.c` alike). It did not until that day: `hpengine.c` read
  `s->page_max > 0 ? s->page_max : jo_page_max(NULL)`, so the variable decided
  only what a row WITHOUT a ceiling got, while `hpengine.h`'s own comment said
  `page_max` was "raised or lowered for the whole tree by `$JO_PAGE_MAX`" and
  `pagewalk.c`'s `pw_page_max` implemented exactly that for its rows — **a
  capability two headers claimed and one of the two engines did not have**, the
  unification of the three ceilings having missed the DOWNWARD direction.
  It cost a whole run once (an attempt to shrink
  `JO34_FDSNEVT_SERVICE_SCEDC_CALTECH_EDU`'s declared 200 pages to 5 read
  **80 pages** before the clock stopped it, 2026-10-21) and, worse, it silently
  defeated `tools/audit_page_ceiling.py`, whose entire shallow-probe design is
  to bound a run by setting this variable: all 669 rows that cycle declare a
  `page_max`, so a probe whose flag said 3 pages would have walked up to 200
  per row. That is why the 114 SLOW verdicts of 2026-10-12 are SLOW. `hptest`
  40f pins the brake, the resume position it leaves behind, and the unset case.
* **The ceiling is now MEASURED across every paged scheduled row**
  (2026-10-12, `docs/measured-page-ceilings-2026-10-12.tsv`): 2,137 rows,
  3-page probe. 1,111 COMPLETE, 534 `CEILING_UNKNOWN` (the upstream publishes
  no total, so only a full walk can size them), 282 `CEILING_DECLARED`, 114
  SLOW, 89 `FETCH_FAILED`, 7 EMPTY. **149 declare a total needing more than 20
  pages**, and they split on cost, which is the whole decision:
  * **60 rows need ≤200 pages** — a per-row `page_max` completes them at a
    sane request count. Done, with the measurement in each comment: 634,915
    declared records against 148,840 the 20-page default reached, so **486,075
    recovered**. `BGS_LANDSLIDE_INDEX` 2,000 → 18,698 (its whole total);
    `CMS_NURSING_HOME_PROVIDER_INFO` → 14,690.
  * **89 rows need more than 200**, up to 55,971 pages for a 5.6-million-item
    STAC catalogue, and 1.93 BILLION declared records between them. Raising
    `page_max` there is not exhaustiveness, it is tens of thousands of requests
    per run against one host. **81 of them now read their collection over a
    CYCLE** — see `page_resume` below; the other eight are named there.
* **`page_resume = 1` reads a collection over a CYCLE**, one ceiling's worth a
  run, instead of re-reading its first pages forever. The walk starts where the
  last run stopped and stores where this one stopped; when it reaches the end
  the position is CLEARED and the next run starts over. This is what
  `collectors/sources/wayback_jp.c` has always done by hand (mhlw's 2,064-page
  index over ~207 daily runs); it is now a field, so 81 rows got it without 81
  hand-written collectors.

  The position is a URL — the one the engine would have requested next — so
  `page_param`, `next_path` and `page_walk` all resume from one stored string.
  A URL is NOT enough on its own for `page_param`: the next page is computed as
  `page_start + (page+1)*step` from the ROW's url, so a run resumed at
  `offset=4` with `page_start` still 0 asks for `offset=2` next and walks
  BACKWARDS over ground it covered, every page full and the run green. The
  engine reads the number back out of the stored URL for that reason.

  Persistence is NOT in the engine: `lib/` holds no database code (hptest links
  `lib/hpengine.c` with no sqlite3 at all), so the position crosses the ABI as
  `source_ctx`'s `resume_in` / `resume_out`, and `core/scheduler.c` keeps it in
  `source_sched_state.resume_token`. That also makes the mechanism available to
  every collector rather than to one engine.

  Measured live, three mechanisms, two runs each against one database:
  `BGS_ONSHORE_BOREHOLE_INDEX` (next_path) 300 → 600 distinct rows,
  `IA_COLLECTION_ARCHIVETEAM` (page_param) 1,000 → 2,000,
  `gleif-lei-filter-lapsed` (page_walk) 20 → 40. `hptest` 40–40e pins the
  cycle, the clearing, the backwards-walk trap, and all three refusals.

  **It is refused, with a line on stderr, for an ENTITY run** ("what do we have
  on X" must answer from the start of X's results, not page 37 of the last
  entity asked about — the scheduler also passes no position on a pivot run),
  for a `{date…}`/`{ago…}` row (a stored URL would pin one day forever; those
  rows already walk forward by date), for a `{page}`/`{offset}` token in the
  path or body (the position is not readable back out of the URL), and for a
  stored position whose HOST is not the row's own — a reading position moves us
  through a collection, never to another server.

  **Two things a cycle does not fix, and what each got instead (2026-10-20).**
  Five ELIA grid rows ran every 900–3,600 s over millions of quarter-hourly
  points, where a cycle would blind them to the current grid for days: four
  now carry a **date window** sized on their MEASURED publication lag
  (`where=datetime>=now(days=-5)` for wind, whose newest record was 41 hours
  old when read; 48 hours for asset loading, published ~14 hours behind in
  ~6,000-record days; three days for recent generation and for the system
  imbalance) with `order_by=datetime desc`, 100 a page and a `page_max` that
  holds the window. Engine readings the same day: asset loading 7,003 / 7,003
  in its 48 hours, recent generation 2,115 / 2,115 in its three days; the wind
  and imbalance windows were validated by direct reads (2,068,300 total and
  212 in-window records respectively) because opendata.elia.be refused the
  engine's two attempts from the measuring environment, as it refused nine
  requests in ten all day — a reading of that path, not of the rows. One of
  the five was not truncated at all: **`ods045` ENDED
  in May 2024** (newest record 2024-05-21, total unchanged between audits) and
  the row had re-read its first 150 points of 2019 every 15 minutes for 17
  months — it reads the successor `ods134` now, 212 records in its three-day
  window, measured. The fifth, `ods147`, was not the dataset its row said it
  was (2026-10-21): the row called it a renewable intraday forecast; its own
  metadata — the first answer the host gave in two days — titles it "System
  imbalance forecast next quarter hour (near real-time)", one record a
  MINUTE since 2023-10-06 (1,437 in the last day, 1,576,655 in all), time
  field `predictiontimeutc`, which is why `datetime` was "Unknown field".
  It is now the deliberate pair `ELIA_SI_FORECAST_NEXT_QH` (the last six
  hours newest first, every 15 minutes: 359 of 359 stored across four pages)
  and `ELIA_SI_FORECAST_NEXT_QH_HISTORY`, the whole series — and because the
  records endpoint caps `offset` at 10,000, an offset walk could never reach
  past the first 10,000 of 1.58 million, so the history row is a **keyset
  cursor**: `order_by=predictiontimeutc asc` with
  `next_path=results.$last.predictiontimeutc` spent through a `next_tmpl`
  whose `where=predictiontimeutc>date'{v}'` asks for everything strictly
  after the last record (the Go-module-index shape), 100 pages an hour under
  `page_resume=1`. Measured: 10,000 of 10,000 in 77 s, 2023-10-06T12:57 to
  2023-10-13T11:36, the resume URL stored; the second run resumed from that
  URL and stored the next 10,000 (to 2023-10-20T10:32) — 20,000 distinct
  rows over two runs against one database, no overlap, 72 s each. About a
  week per cycle against 1,440 new records a day. **And the same question
  asked of every ODS row the next day** (2026-10-22,
  `docs/measured-ods-titles-2026-10-22.tsv`: 237 datasets, 118 titles read,
  124 unanswered through this path, 3 gone and removed) caught the row
  re-pointed two days earlier: `ELIA_SYSTEM_IMBALANCE` had been moved onto
  ods134, whose own title is "Imbalance prices per quarter-hour"; the
  one-minute successor of ods045 is ods126 ("Current system imbalance
  (Historical data as of 22/05/2024)", 1,271,520 minutes, a day behind),
  and ods134 is `ELIA_IMBALANCE_PRICES_QH` now. A repair made from a
  dataset's FIELDS is still a guess about its subject; the title is one
  request. The
  `/exports/csv` route (the whole dataset as one ~210 MB stream, read to three
  rows) is the other complete read; the cursor was chosen because a cut walk
  resumes where it stopped and a cut export starts over. **A row's name is a
  claim about its dataset too — read the metadata endpoint once**, and
  **before windowing a row, read its newest record: a frozen series looks
  exactly like a truncated one from the count alone.** The two GOV.UK report
  searches (MAIB, RAIB) were not a window problem but the `count`/`start`
  pager above: at `count=100` each reads its whole register in 12 / 7 pages.
  And `PLOS_RECENT` — 417,719 records against a 20-page ceiling —
  deliberately does NOT cycle: its name, tags and `publication_date desc` sort
  all say recency is the point, and a cycle would cost it exactly that.
  `PLOS_CORPUS` is the second row that note asked for: the same query with
  `publication_date asc,id asc` and `page_resume`, 418,125 articles over a
  daily cycle (2,000 a run, resuming at `start=2000`, then `start=4000` —
  measured).

  **A descending sort and a cycle do not compose, and the fix is the sort.**
  New items land at the HEAD of a newest-first collection, so every page
  boundary shifts under a walk that is halfway down and a seam can re-read or
  skip records — the drift `wayback_jp.c` documents. Ascending puts new items at
  the TAIL, where a forward walk has not been yet, so a resumed run asks for the
  pages it meant to. **22 rows were re-sorted on 2026-10-18**, each verified
  against its own upstream first: 17 Internet Archive collections
  (`sort[]=publicdate+asc` — same 5,430,347 total, oldest first), four FR BODACC
  gazettes (`order_by=dateparution asc` — same 6,641,689 total, from
  2008-01-02), and `CZ_CRO_PUBLIC_OFFICIALS`, which already sorted on `id`, the
  most stable key there is, and only needed `order=ASC`. Measured after: 2,000 /
  400 / 40 distinct rows over two runs each, no overlap.

  `UK_EC_DONATIONS` kept `order=desc`: ascending leads with records whose
  `AcceptedDate` is null, so that sort is not a stable ordering, and a drifting
  head is the documented trade rather than a silent one. A row that does NOT
  cycle keeps `desc` deliberately — reading the newest pages is then the
  question being asked.

  **The 534 `CEILING_UNKNOWN` rows: 452 of them now cycle too** (2026-10-18).
  No arithmetic can size them, which is exactly why a cycle fits — the walk does
  not need to know how big the collection is, only where it stopped. Three
  measured properties make it safe to apply without probing each one:
  * a row that COMPLETES inside its ceiling stores no position, so cycling is a
    genuine no-op for it. `JPN_EGOV_DATA_ORG_ORG_0100`: 13 records on both runs,
    token `(none)`. `hptest` 40b-ii pins the same thing offline;
  * a row that does not complete reads a further slice instead of the same first
    pages — strictly more of the collection;
  * every stop is still disclosed, now naming the slice.

  Four exclusions, each a case where a cycle costs something: **cadence under
  six hours** (44 rows — the row exists to be fresh), **recency in its own
  identity** (29 — its id or record_type says `recent`/`latest`/`newest`/
  `daily`), **a declared descending sort** (8 — the fix is an ascending sort
  verified per host, which was done for the 22 above and is not extended to
  hundreds of unverified hosts), and **a `{page}` token** (1 — the engine
  refuses it). **A 24-row sample was run twice each against one database and
  every row behaved correctly**: 15 cycling mid-collection
  (`TX_TCEQ_VIOLATION_CITATIONS` 20,000 → 40,000, `FAA_ARCGIS_OBSTACLES`
  10,000 → 20,000, `FMCSA_MOTUS_AUTHORITY_HISTORY` 19,997 → 39,997), 2 finishing
  the collection and clearing (`MD_LOBBYIST_CAMPAIGN_CONTRIBUTIONS`
  20,000 → 20,387; `au-asic-smsf-auditors` 2,000 → 2,400), and 7 no-ops that
  complete inside their ceiling and store nothing (`WA_CRIME_NIBRS` 3,471 both
  runs, `UK_HYDROLOGY_STATIONS` 9,537, `CA_CALGEM_FACILITY_TANKS` 15,732).

  **One row in that sample was not a cycle at all, and it found a false
  disclosure.** `ENA_PATHOGEN_SARSCOV2_RUNS` read 1,000 of millions on both
  runs with no position stored, which reads like "completes in its ceiling" —
  and a round 1,000 against a `limit=1000` is worth one suspicious look.
  ebi.ac.uk answers **HTTP 400 to `offset=1000`**: page 2 is refused, so the
  position is cleared (bookmarking a page that is refused would retry that one
  page forever) and the cycle can never advance. The notice then said
  `next_run_resumes_at: "the start — this run completed the cycle"`, which is
  exactly the kind of tidy falsehood these notices exist to prevent. It now
  distinguishes the three outcomes, and a failed page says the position was NOT
  advanced and the row re-reads the same slice until its paging is re-pointed
  (`hptest` 40b-iii).

  **All 530 cycling rows have now been asked that question** (2026-10-19,
  `docs/cycle-health-2026-10-19.tsv`, `tools/audit_cycle_health.py`): **318
  CYCLING, 190 COMPLETE, 12 STUCK, 6 FETCH_FAILED, 4 SLOW**. The 190 are the
  measured no-op that makes `page_resume` safe to apply broadly, now counted
  rather than sampled.

  **Read the STATUS, not the verdict.** Of the 12 STUCK, exactly ONE is stuck
  for a reason that is a property of its endpoint: `BC_MINERAL_TENURE`
  answering 400 with *"Cannot do natural order without a primary key, please
  add it or specify a manual sort over existing attributes"*. GeoServer refuses
  `startIndex` on a feature type with no primary key, so that row read **50 of
  42,234** for ever — the ascending-sort rule above, enforced by the server
  rather than by us. `sortBy=TENURE_NUMBER_ID` fixes it: 1,000 records a run
  over 20 pages, position advancing 1000 → 2000. **It was NOT applied to the
  16 other sortBy-less WFS `startIndex` rows in the tree** — they were measured
  too, and 12 of the 13 reachable ones serve page 2 fine, because GeoServer
  objects only where the type has no primary key. A blanket fix would have been
  a change made on reasoning.

  **Those 12 findings were RE-MEASURED on 2026-10-20 with the sweep paced one
  row at a time per host, and SEVENTEEN of the 22 non-healthy verdicts changed**
  (`docs/cycle-health-recheck-2026-10-20.tsv`). Six of the twelve STUCK rows
  were the sweep's own load and now read CYCLING — both FAA rows,
  `us-sf-311-open311-requests`, and two of the five `au-asic`. `audit_cycle_health.py`
  serialises per host now: `core/hostgate.c` paces requests inside ONE process
  and cannot pace twelve that each think they are alone, and a cycling row
  makes ~20 requests to one host. **A verdict that changes with `--jobs` is not
  a measurement** — the same reasoning as `audit_page_ceiling.py`'s scratch-database
  slot pool, one layer up. Only the findings needed re-reading, because load
  makes a page FAIL and so can only manufacture a false STUCK or FETCH_FAILED,
  never a false CYCLING or COMPLETE.

  **The `au-asic` family is the honest remainder.** Five rows share
  data.gov.au; host-paced, two cycle, two read FETCH_FAILED (403 on page 1) and
  one is still STUCK (403 on page 2). So those 403s are NOT purely concurrency
  — which is what the by-hand test suggested when data.gov.au served 8 rapid
  serial and then 12 concurrent requests, all 200. The difference between that
  test and these runs is the CLIENT, since the engine sends its own headers.
  That is the next thing to measure on that host, and it is written down rather
  than papered over with a hostgate entry.

  The other 11 were 429/403/500, one reading each, **and several were load the
  sweep itself created**: 12 jobs in parallel while a 6-job batch-46 emit audit
  hit `services6.arcgis.com`, where both failing FAA rows live.
  `us-sf-311-open311-requests` is already paced at 1,200 ms in
  `core/hostgate.c` and still 429'd at page 20 under that load, which says the
  same thing. The five `au-asic` 403s were re-tested by hand and are **not
  reproducible** — data.gov.au served 8 rapid serial requests and then 12
  concurrent ones, all 200 — so no pacing entry was added. A tool that adds a
  gap on an unreproducible reading is guessing.

  **A THIRD sweep manufactured its own findings, in a shape the second one's
  fix could not have caught** (2026-10-20, `docs/page-two-2026-10-20.tsv`).
  `tools/audit_page_two.py` asked page 2 of all 5,854 paged scheduled rows and
  reported **781 `PAGE1_FAILED`**. 569 of those are `Connection refused`, every
  one in the last 15% of the run and spread across wholly unrelated hosts — a
  Japanese university's WordPress, the EIB, NHTSA — and both of the two
  re-tested by hand answered HTTP 200 immediately afterwards. A socket that was
  never opened says nothing about the server at the other end.

  **And the second shape was one I first called correctly and then got wrong.**
  About a hundred more read `Connection reset by peer`, which LOOKS like the
  upstream's own refusal, so the first reading was that a reset belongs to the
  row. One command settled it the other way: `curl` reports HTTP 000 for the
  same hosts and the agent proxy names the cause itself —
  **`ws_closed_mid_exchange`**, "the tunnel to the egress proxy closed before
  the exchange completed", visible under
  `curl -sS "$HTTPS_PROXY/__agentproxy/status"`. The hosts are not denied by
  policy; each answers 200 on its own. **So in this container a peer reset can
  be OURS**, and a sweep that attributes it to the upstream writes down dead
  sources that are alive. Both errors are retried with a back-off and reported
  as `LOCAL_EGRESS` when they persist, worded so nobody can read one as a
  finding; a row that only ever comes back `LOCAL_EGRESS` is **unmeasured from
  here**, which is a thing to write down rather than a verdict to act on.

  **And that sweep's RECORD COUNTER was the array hijack this file already
  records for the engine, in a tool that never got the engine's fix.** It took
  the longest list ANYWHERE in the body, so an ArcGIS polygon's coordinate RING
  was counted as records: `EAD_PROTECTED_AREAS` holds 21 features — its own
  `returnCountOnly` says 21, with no `exceededTransferLimit` — and was counted
  as **3,748**. Its docstring excused this as "a floor, only ever compared with
  ITSELF", and both halves were false: the short-page rule compares the count
  with the page SIZE the URL asked for, and THAT comparison is what separates a
  benign `PAGE1_IS_ALL` from a `REFUSED` or `SAME_AS_PAGE1` finding, so
  over-counting turns a complete collection into a finding. It resolves the
  declared envelope first now, then the longest array of OBJECTS, like
  `hp_path`.

  The other half of the same bug FABRICATED. A body truncated at the tool's
  400 KB read cap will not parse, and the fallback `s.count("},") + 1` did not
  decline — it answered **1,232** for a CKAN page holding 48 packages whose own
  `result.count` says 48. The cap is 8 MB now and an uncountable body returns
  **-1 for UNKNOWN**, which every comparison guards on, rather than a number
  nobody can tell from a measurement. Verified after: 21, 48 and 300 where the
  tool had said 3,748, 1,232 and 1,232. **A count a tool cannot make is not a
  count it may estimate** — that is rule 1 applied to the gates themselves.

  A by-catch of the same pass: `CR_SANCARLOS_PREDIOS` had been documented dead
  since round 8 (2026-09-21) because GetCapabilities listed no `predio*` layer,
  "so the parcels were withdrawn, not renamed". That followed from **searching
  for the name we expected**. The server's 40 types include
  `geonode:Catastro_sc` — the same cadastre under its other Spanish name,
  99,222 features carrying `Propietario`. Re-pointed (plus WFS 2.0.0 for
  `count`/`startIndex`, plus `sortBy=id` for the same GeoServer refusal):
  measured **20,000 records a run** over two runs with no overlap, position
  advancing 20000 → 40000.

  **That row's real problem was not its paging flag: ENA honours NO offset at
  all**, and `{"message":"Unsupported param offset"}` is what the 400 says
  (measured 2026-10-19). There is no second page to ask for, so nothing in the
  paging section above could ever have reached the other 7,555,500 records.
  What that endpoint does honour is a DATE predicate, which is the next bullet.

* **`window_days = N` reads a collection whose only slicing axis is a DATE.**
  `page_resume` needs the upstream to page; some do not. With `window_days` the
  row carries `{wstart}` / `{wend}` tokens instead (rendering like `{date}`,
  `{wstart:%Y%m%d}` through strftime) and reads one window per run, the
  position stored through the same `resume_in`/`resume_out` as `w=<ISO date>`.
  ENA's `first_public>=A AND first_public<=B` with `limit=0` returns a window
  COMPLETE: 15,710 and 20,232 records stored on two consecutive runs, matching
  the endpoint's own `/count` for those two weeks exactly.

  **The walk goes FORWARD, from `window_from` toward today**, and that is the
  design rather than a preference — a backward walk would not see a record
  published this morning until it had finished the history, the same mistake as
  cycling a newest-first collection. Walking forward the cycle CATCHES UP and
  then pins to the trailing window, so one row is both the backfill and the live
  feed. A window that was NOT SERVED does not advance (skipping it drops its
  records permanently; re-reading one we have is cheap), which is the opposite
  of a failed PAGE clearing its position — a page can be reached again from the
  start of the same collection, a skipped window cannot.

  **A date predicate is only exhaustive where every record is dated, and that is
  the author's measurement, not something the engine can check.** ENA's
  influenza SAMPLE collection holds 61,537 records of which only 60,213 answer
  `first_public>=1990-01-01` and 0 answer `first_public<1990-01-01` — 1,324 carry
  no publication date, and no `window_days` would ever find them. That row reads
  its collection whole instead (`limit=0`, one request, 61,537 stored). **Ask the
  upstream for its own count both with and without the predicate before
  windowing a row.** `hptest` 41–41g pin the rest.

  533 rows cycle on pages, 50 on a date window. One shape to keep in mind:
  `interval = 2592000` is monthly, so a 500-page collection on that cadence
  takes about two years to come round — better than never, and the notice says
  where it is.

  **Sizing a window is a DAILY measurement, and a weekly one says the wrong
  thing in both directions.** The 47 KKJ prefecture rows are the case
  (2026-10-20). Weekly counts across eight whole prefecture-years put the peak
  week at 2.48x to 4.24x the average week, which suggested sizing every row so
  a 4.3x week fits. Measuring the three worst weeks day by day refuted it
  twice:
  * **Tokyo's peak is a SEASON, not a spike** — 978, 1,055 and 979 in three
    consecutive weeks, about 143 a weekday and nothing at weekends. So its
    rolling maxima are max3d 643, **max4d 806**, max5d 1,055: four days fit a
    1,000-record read and five do not, and the weekly ratio had been
    conservative by a third. Calibrating the burst factor on that one measured
    number (3.26) rather than on the weekly ratio took the whole set from 5,509
    windows to 4,142. Verified live at the measured peak: 806 of 806, complete.
  * **Toyama publishes in BULK** — single days of 1,449 and 1,161 notices,
    which is why its max3d and max7d are the SAME number. A record cap is per
    READ, so no window width can take such a day whole, and a narrow window
    there loses the same records at seven times the requests. **Check whether
    an overflow is a day before narrowing a window**; the notice's own
    arithmetic assumes an even spread and says so.

  A window size does not have to be perfect, and that is what makes this
  shippable on an estimate: **a window whose own count exceeds one read
  discloses itself.** The reason used to say "read its window in full" from one
  condition only — that the row was behind — so an under-sized window's
  overflow read as a tidy deferral. A deferred record comes back on a later
  run and an overflowed one never does.

  And **on a window row the upstream's count is the size of the SLICE**, not of
  the collection: `records_available_basis` said "upstream declared this total"
  there, inviting Toyama's 1,458 to be read as its 30,512. `hptest` 41h pins
  both halves.
* **`filter_query = 1` on a row the SERVER already filters discards the
  upstream's own matches.** It means "keep only records mentioning the entity",
  which is right for a bulk file filtered locally (OFAC's sdn.csv) and wrong
  the moment the URL carries `{q}` and the endpoint honours it. Measured
  2026-10-21: `US_DATA_GOV_CKAN` re-pointed at data.gov's search kept **1 of
  the 8** records the page returned, because only one dataset's link TEXT
  repeats the word. Worse on a gazette, where a notice is titled for the
  instrument and not the company — "Competition Act: approved merger —
  Barloworld SA (Pty) Ltd and Leatoy (Pty) Ltd" is a real hit for an entity
  its own slug never names, and `ZA_GOVERNMENT_GAZETTE` had carried the flag
  since it was written. Check which side filters before declaring it.

* **A row with no `.url` was dropped at REGISTRATION in silence**, and
  `index_url` is the trap: such a row still needs a `.url` (the engine replaces
  it each run with the link `index_href_must` finds). `hp_register()` skipped
  it with a bare `continue`, so two rows of one batch compiled, sat in their
  table, registered nothing and said nothing — the only symptom was `make
  registry-floor` counting one fewer source than the day before, which reads
  like somebody else's removal. It names the file, the row's index and the
  missing field now; `hptest` 48 and 48b pin that the unusable row stays out
  AND that its complete neighbour still registers, so one bad row cannot take
  a table down.

* **Run `make lint-sources` before believing a re-point is a NEW source.**
  Open item B2's triage named `data.stortinget.no/eksport/saker` as the
  replacement for a 404'd pivot; `dup-endpoint` answered at once, because
  `eur-stortinget-saker` has read exactly that URL every six hours all along
  (197 records, rc=0, measured). That would have been the `data.gov.uk` PROXY
  row again: two rows, one upstream, no new data. Asking the inverse question
  — which of a publisher's exports the tree does NOT read — found three live
  Stortinget endpoints missing, and all three are registered now.

* **An anchor COUNT is not the data, any more than a record count is.** The
  same triage recorded "288 anchors, the actual list of supervised entities"
  for the ECB page. They are site navigation (`/about/working-at-the-ecb` 23,
  `/press/other-publications` 10); the list is really a workbook renamed every
  month, over FOUR sheets (SIs 844, LSIs 2,175, Appendix 39, Changes 11), which
  is the `index_url`/`index_href_must` case and — because `HP_XLSX` reads one
  sheet — a deliberate PAIR of rows rather than 2,175 banks lost at a seam.
  Copernicus EMS was the same error: 38 anchors, all of them chrome, and the
  one feed the page declares is two news items rather than the activation
  register.

* **A spreadsheet laid out for HUMANS needs `csv_no_header = 1`, and a record
  count cannot see that it does not.** The ECB's supervised-entity workbook
  took four readings to parse (2026-10-21): the first run reported 831 records
  and `rc=0` with every field named `col6` and the workbook's TITLE BLOCK
  stored as supervised banks. Three things had to be measured, none of them
  guessable: the header is not row 1 (sheet row **12** on one sheet, **8** on
  the other) and `csv_skip_lines` counts PHYSICAL lines, so the header's own
  quoted `\r\n` — the cell reads "LEI / MFI code for branches" — makes it span
  two of them; **the two sheets of ONE workbook have different geometry** (LEI
  `col2` / name `col11` on `SIs`, `col1` / `col9` on `LSIs`), and declaring one
  sheet's positions on the other emitted 2,166 records titled by their LEI plus
  two shape notices saying 0 of 2,166 carried the declared keys; and the header
  must not be read as a header at all, because its names live in MERGED cells,
  so depending on which row the skip landed on the parser alternated between
  positional `colN` names and a field literally named `""`. `csv_no_header = 1`
  makes it deterministic. The cost is a row pinned to column POSITIONS, which a
  re-laid-out workbook would change without changing the record count — so the
  row says so.

* **A JSON TABLE needs `row_header_path`, or its row keys are POSITIONS.**
  Some APIs answer an array of ARRAYS with the column names in a sibling array
  — Finland's Eduskunta (`rowData` + `columnNames`) and the Austrian Parliament
  (`rows` + `header`, whose entries are objects carrying a `label`). The engine
  flattened such a record positionally (`.0`, `.1`, …), so a row's
  `title_keys`/`id_keys` would be positions whose meaning the upstream can
  change without anything here noticing — the hazard this file already records
  for batch 46's generator and for the ECB workbook, where two sheets of ONE
  file have different geometry. `row_header_path` names where the header is; a
  cell past its end keeps its positional name rather than being dropped, and a
  row that declares no header path behaves byte for byte as before. It is
  registered in `gen_hp_batch.py`'s `STR_OPTS` in the same change, which is the
  third place this file keeps saying gets forgotten. `hptest` 49–49f, six of
  the eight confirmed to fail without it and the two that pass being the
  controls for the additive guarantee.

  **Finland ships on it and Austria deliberately does not**, and the difference
  is one measurement. `FI_EDUSKUNTA_RECORDS` stores 1 of 1 for a real surname
  and an honest empty for an impossible one, its record carrying `personId`,
  `lastname`, `firstname` and `minister` by name. The Austrian filter API has
  **no stable ordering**: two identical requests for its unpaged first page
  returned different rows (`5,7,9,314,512`, then `1,3,4,8,2`) while `page=2`
  was stable across two reads, and of ten parameter spellings probed six
  appeared to "move" the answer with most moving it to the same place — which
  is what an unstable index looks like, not six working parameters. Its
  `showAll` is a PRESENCE and not a value (`showAll=false` behaves like `true`:
  4 pages of 100,000 at 58 MB, against 19,565 pages of 20 with the parameter
  omitted) and its `pageSize` is ignored. A 19,565-page cycle over a head that
  reshuffles is exactly the skip-with-a-collision-as-its-only-symptom shape
  this file calls the worst it records, so the row keeps failing honestly until
  a sort parameter is MEASURED.
  (`hptest` 40b-iii). Any other row in this shape says so on its next run —
  and **the sweep that reads those disclosures across the registry now
  exists** (2026-10-22, `tools/audit_cycle_advance.py`,
  `docs/measured-cycle-advance-2026-10-22.tsv`): every one of the 669
  `page_resume` rows run through the binary under `JO_PAGE_MAX=2` — the
  operator's brake, which reaches a declared ceiling too, so a run is page
  1 plus the attempt at page 2 (`jo_page_max_for`, hptest 40f) — and its
  notice read back. 600 advance, 20 complete inside two pages, 43 were
  refused page 1 from the measuring environment (39 of them on a second
  pass; ODS hosts and a few others reset this path), **2 answered 429 to
  page 2** and **4 could never advance**:
  * `BC_MINERAL_TENURE` — GeoServer refuses `startIndex` without `sortBy`,
    the very hazard noted above for BC's WFS, in a row that never got the
    sort. `sortBy=TENURE_NUMBER_ID`: startIndex 0/50/100 answer disjoint
    ascending pages, 150 of 150 across three pages in the engine, resumes.
  * the three **ENA Portal API** rows — `offset` answers HTTP 400
    "Unsupported param offset", full stop; no cycle can exist. The API's
    complete read is **`limit=0`**: ONE request, the whole result set.
    `ENA_PATHOGEN_INFLUENZA_SAMPLES` reads 61,625 of 61,625 (= its count,
    19 MB, 82 s in the engine). `ENA_PATHOGEN_SARSCOV2_RUNS` is 7.5 million
    runs (~2 GB), so it is a 30-day `first_public` window at `limit=0`
    (6,362 of 6,362 in the engine; 0 runs in the last week, 47,270 in a
    year — the trade is stated in the row). And `ENA_PATHOGEN_MTB_RUNS`
    found a third shape: **one `limit=0` read of 360,690 runs came back
    HTTP 200 with 331,580 rows — 8% short in EVERY date half**, a silent
    cut of an accession-ordered stream, not a tail. Split on `first_public`
    the same day, every block answers exactly its count (76,189 / 95,032 /
    85,283 / 57,476 / 46,710, summing to 360,690), so it is FIVE
    single-request TSV rows now: four static history blocks weekly and the
    live 90-day tail daily (10,255 of 10,255 in the engine), the 2025-on
    block ending at `{date-90d}` where the tail begins. **A 200 with fewer
    rows than the count endpoint is a cut; a window that equals its count
    is complete** — measure both before trusting a bulk read.
  * `ENERGINET_CAPACITY_MUNICIPALITY` and `TRONSCAN_TOP_ACCOUNTS` — 429 on
    page 2, so per-host gaps in `core/hostgate.c` (1,000 / 1,500 ms; both
    read three pages and resume now, Tronscan's 78 requests with its detail
    hops in 116 s). **Which found that the table's live-entry count was a
    hand-maintained `7` while it held nine** — the same `1`-for-two trap
    its own comment recorded in September — so the two new gaps were
    compiled in and never consulted; the count is sizeof arithmetic now.

  **That sweep is done: 669 cycling rows measured, SEVEN stalled, and all
  seven are fixed** (2026-10-22, `docs/measured-cycle-stalls-2026-10-22.tsv`;
  `tools/audit_page_ceiling.py` gained the `CYCLE_STALLED` verdict — a notice
  carrying `failed_page` on a `page_resume` row — and `WALK_CUT` for the same
  failed page on a row that does not cycle). 467 `CEILING_UNKNOWN`, 136
  `CEILING_DECLARED`, 15 COMPLETE, 43 `FETCH_FAILED`, 1 SLOW. **Read the
  FETCH_FAILED against the path, not the row**: the agent proxy's own
  `recentRelayFailures` listed tunnel closures for five of those hosts while
  the sweep ran.

  The seven say something the stall count does not: **a refused page 2 is
  rarely a page-2 problem.** Each needed a different fix, and five of them were
  wrong in a way no ceiling arithmetic could reach.
  * **ENA ×3** — `www.ebi.ac.uk` does not refuse `offset=1000`, it refuses
    `offset` at all: `offset=1` answers the same 400
    `{"message":"Unsupported param offset"}`. The note above was written from
    one boundary reading and generalised wrongly. **Both branches found this
    and wrote different rows**, and the merge keeps the one that reads the
    archive: a rolling 30-day `first_public` window is the live deposit tail
    and says so — its own author recorded that the 7,556,500 runs already in
    the archive are NOT backfilled by it, and that date tokens cannot express
    the backfill because they step back on failure rather than iterate. That
    is what `window_days` is: the two SARS-CoV-2 and MTB rows walk
    `first_public` FORWARD from 2020-01-01 and 2010-01-01 in 7- and 90-day
    slices, catch up, and then pin to the trailing window, so one row is both
    the backfill and the live feed. Measured both ways and kept as data:
    15,710 and 20,232 stored for two consecutive weeks (equal to ENA's own
    `/count` for those weeks), against 6,362 / 3,436 / 289 for a 30-day tail.
    The influenza row is NOT windowed in either reading, and that half is this
    file's own rule rather than a preference: 1,324 of its 61,537 samples
    carry no `first_public` at all, so a date predicate could never reach them
    — it reads whole at `limit=0`, 61,537 of 61,537. Main's `timeout_ms` is
    kept on all three: the SARS-CoV-2 query measured **38.6 s cold** against
    the engine's 20 s `HP_HTTP_TIMEOUT`, so the first fixed run timed out at
    `bytes=0` and filed a fetch-failure for a query that works. And main's
    `first_public` in the influenza row's FIELD list is kept too, for a reason
    worth generalising: a field absent from the list is absent from the stored
    record, so `date_keys` had nothing to order on.
  * **`BC_MINERAL_TENURE`** — the WFS refuses `startIndex` without `sortBy`
    ("Cannot do natural order without a primary key", the hazard this file
    already records from the other side). With `sortBy=TENURE_NUMBER_ID` and
    `count=10000`: 50 records → **42,327**, its own `numberMatched`.
  * **`ENERGINET_CAPACITY_MUNICIPALITY`** and **`TRONSCAN_TOP_ACCOUNTS`** both
    answered 429 and wanted OPPOSITE fixes. Energinet limits by request
    FREQUENCY, not size — 429 at `limit=1000` and `limit=10000`, then
    `limit=50000` served in full — so it stopped paging and reads its whole
    12,771-record dataset in one request. TronScan's cost is per RECORD (a
    detail hop per listed account, so a page of 50 is 51 requests in a burst):
    measured 2 of 10 requests 429 at the global 150 ms gap and 0 of 10 at
    1,000 ms, so it got that gap in `core/hostgate.c` and KEPT its cycle,
    because the per-run detail budget of 1,000 reaches all 10,000 accounts only
    if the slice moves.
  * **`SI_GURS_CADASTRAL_MUN`** was not broken: the 404 was transient and a
    re-read stores 2,698 / 2,698. One probe is one reading, in this direction
    too.

  **Two of the seven had no `id_keys` at all** (BC's GeoJSON features, and
  Energinet, which was keying on `Month` — a dimension every municipality in
  that month shares). A stalled cycle hides a key defect behind it, because a
  row that only ever reads one page never shows the collision.

  **Two sessions ran this sweep on the same day and merged the same evening**
  (PR #68 first, with `audit_page_ceiling.py`'s `CYCLE_STALLED`; then this
  branch's `audit_cycle_advance.py`, above). Where their fixes differed, the
  better-measured one stands: BC reads 10,000 a page in five pages (theirs),
  Energinet reads its 12,771 rows in one request (theirs), Tronscan's gap is
  the 1,000 ms measured ten-for-ten (theirs); the ENA influenza samples read
  the WHOLE set at `limit=0` (61,625 = the count, ours) and the tuberculosis
  runs read whole as five windows (ours), because a 30-day slice of a
  collection the API hands over complete in one request is a discard; the
  SARS-CoV-2 row is a 30-day window in both, at `limit=0` with their cold
  timeout. And `$JO_PAGE_MAX` now brakes a declared `page_max` (theirs,
  `jo_page_max_for`, hptest 40f), which is what this branch's
  `JO_PAGE_MAX_CAP` existed for — so that knob is gone and
  `audit_cycle_advance.py` sets `JO_PAGE_MAX=2`.

  **ADVANCING IS NOT ADVANCING CORRECTLY, and 273 cycling rows cannot say
  which they do** (2026-10-22, `docs/measured-seam-risk-2026-10-22.tsv`). Both
  sweeps above ask whether page 2 can be fetched. Neither asks the other half:
  a resumed walk only lands where it meant to if the collection's ORDER is
  stable between runs. If the upstream reorders, a record moves out of the
  slice the next run will read into the slice the last run already read, and it
  is never stored — the skip this file calls its worst shape, whose visible
  half is a small collision and whose invisible half is the loss. That is a
  property of the row's URL, so it is readable without a request: of the 665
  rows that cycle, **273 page by offset or page number with NO order parameter
  at all** (ArcGIS 50, WordPress REST 28, CKAN 25, Socrata 13, ODS 9, WFS 3,
  145 other), 166 name an order with no direction, 52 are keyset cursors (safe
  by construction) and 173 sort ascending already.

  **The first version of that count said 388, and 115 of them were false.**
  The pattern matched `order`/`sort` after `?` or `&` and so could not see
  Socrata's `&$order=`, OData's `&%24orderby=` or ArcGIS's `&orderByFields=` —
  a third of the finding was rows that already sort. A new check's big number
  deserves the same suspicious look this file gives a new check's zero.

  **WordPress REST is the family measured end to end** (28 of those 273,
  `docs/measured-wordpress-cycle-totals-2026-10-22.tsv`). Its default order is
  `orderby=date&order=desc` — verified live, newest first — so every new post
  pushes the whole collection down one place under a cycling walk. But a risk
  ranking does not say which rows HAVE a seam, and `X-WP-Total` does: **8 of
  the 28 hold more than one run reads** (kyoiku-press 77,291 posts, lnews
  71,028, yakuji 58,422, japanmetal 57,496, ANCOM 5,705, ANM 3,831, AEK 3,138,
  sorabatake 2,521) and 15 complete inside one run, where the cycle is a
  genuine no-op and the descending order costs nothing. The 8 carry
  `orderby=id&order=asc` now (accepted on every host, measured, ids ascending
  — and note the dates are NOT monotonic with id, which is its own reason to
  key the order on the id). Two runs against one database on sorabatake:
  1,000 then 1,000, **2,000 distinct rows, zero overlap at the seam**. Four
  rows answer no `X-WP-Total` at all and are recorded as unmeasured.
  Recency is the trade: three of the 8 hosts have a registered RSS row that
  covers it, so reading their archive forward is exactly the row's job; for the
  other five the pair this wants (the `PLOS_RECENT`/`PLOS_CORPUS` shape) is
  named in the row as a follow-up rather than implied. The other six families
  are ranked and NOT fixed — ArcGIS needs each layer's own `objectIdField`,
  which is one metadata read per row.

  **The families behind those 273 are now measured one by one, and 99 rows
  carry a stable sort** (2026-10-24,
  `docs/measured-seam-families-2026-10-24.md`, per-layer readings in
  `docs/measured-arcgis-oid-2026-10-24.tsv`). One verdict per API family, each
  from a live probe, because a family's answer covers dozens of rows where a
  per-row guess covers none:
  * **ArcGIS, 47 of 50** — `orderByFields=<the layer's own objectIdField> ASC`.
    The field is NOT assumable: across the 50 it is `OBJECTID` forty times but
    also `ObjectId`, `objectid`, `FID` and `oid_permit`, so the probe reads
    each layer's metadata first and then checks that offsets 0 and 1 answer
    different records. `IMF_PORTWATCH_AIRPORTS`, two runs: 4,000 distinct.
  * **CKAN `datastore_search`, 21** — `sort=_id asc`, the datastore's own
    sequential row id. **Socrata, 13** — `$order=:id`, and DESC answering a
    different set is what proves the parameter is honoured rather than
    ignored. **IETF datatracker, 14** — `order_by=id`, the Django primary key.
    **CKAN bulk list, 3** — re-pointed to `package_search?sort=id+asc`, each
    now storing its portal's declared count exactly (823, 797, 250).
    **One WFS**, Slovenia's 580,322 house numbers, on its own EID.
  * **Three things the re-points taught.** A URL changed after validation is
    unvalidated AND ITS SIBLINGS MOVE WITH IT: the CKAN re-point stored ONE
    record on all three rows until `array_path` went `result` →
    `result.results` and `page_param` `offset` → `start`. A notice nobody
    reads is still true: the Hong Kong row's `title_keys = "author"` matched
    nothing — present on every package, null on every one — so every dataset
    was titled from the fallback list while the shape notice asked for this
    exact repair. And the applier reads each URL out of the C, because the
    risk TSV truncates its url column and the match failed on the one row
    longer than 200 characters.

  **54 rows were measured and deliberately NOT changed**, which is the part of
  this pass worth keeping. **UK Parliament's Linked Data API degrades the
  RECORD when sorted**: `bills.json?_sort=identifier` answers page 0 as bare
  URI strings where the unsorted page answers full bill objects — a sort there
  would have turned 14 rows' records into hyperlinks. GBIF exposes no sort at
  all; FPDS's ATOM feed has none either, so stability there means deciding
  whether the row is a recency feed, not adding a parameter. OpenDataSoft's 9
  are 7-of-9 unreachable from this environment and need a per-dataset field.
  SA Treasury's `order=` is parsed (a malformed spec returns zero rows) but the
  host stopped answering before HONOURED could be established — one undecided
  reading is not grounds to change seven rows. Hungary's INSPIRE WFS accepts
  `sortBy=name` and that name has TIES, so it is not a total order.
  `environment.data.gov.uk`'s flood-monitoring `_sort` is accepted and
  IGNORED (`notation` and `-notation` answer the same records), and the
  `asset-management` row among those 8 is the same Linked Data API as
  Parliament's: **read what it stores before sorting it.** `api.uktradeinfo.com`
  REFUSES OData's own `$orderby` with HTTP 403 ("use only the allowed
  operations"), and `repository.overheid.nl`'s SRU ignores `sortKeys` in both
  directions while drawing no diagnostic for a bogus key at all.

  **And an unrecognised parameter is not always ignored — on some APIs it
  FILTERS.** The Gazette dropped `sort=publication-date` silently, but
  `sort-by=id` took its own declared total from **9,321,677 notices to
  4,882,776** and returned nothing: the parameter was read as a search term,
  not a sort. So when a sort goes onto a row, compare the upstream's own total
  before and after — a total that moves means the parameter was not read as a
  sort, and the row is now about a different collection.

  **The long tail is measured too, and 19 more rows carry a sort** (2026-10-24,
  `docs/measured-seam-tail-2026-10-24.tsv`). The 62 single-host rows no family
  covers were each sent their own URL with a BOGUS sort value under nine
  candidate parameter names: 29 parse such a parameter, 28 answer the baseline
  byte for byte, 5 were unreachable. **The error body is the part worth
  reading** — DERIB, USAspending and Misskey each hand over their whole allowed
  set in it, and the OData services answer "could not find a property named …
  on type …", which doubles as a schema check. Applied, each verified accepted,
  honoured, total-preserving and disjoint over two runs against one database:
  twelve OData rows on `$orderby=<the entity's own key>`, GOV.UK Search on
  `order=public_timestamp` (the same ascending order the six rows in
  `hp16_us_legal_1.c` carry), AODN on `sortBy=_id`, Miyazaki's CKAN group page
  on `sort=metadata_created asc`, ACT's Socrata catalogue on `order=createdAt`,
  Misskey on `sort=+firstRetrievedAt` (its enum's only append-at-tail order; a
  page is its own 30), DERIB on `sort=legislationIdentifier` and USAspending on
  `sort=name&order=asc` (209 of 209, its declared total). Twelve are measured
  and NOT changed: GLEIF ×3 (`page[number]=51` is refused, so they are bounded
  at 10,000 whatever the order — France declares 193,375 — and a stable sort
  would only freeze which 10,000; the fix is a narrower filter per request), UK
  Land Registry's PPI (`_sort` answers nothing in 35 s and an internal-error
  page at 95 s, against a sub-second unsorted page), TronScan (only the
  volatile `balance` is accepted, and that ranking is the row's subject), the
  two Mastodon directories (`order` is `active|new`), ENTSOG (parsed, not
  honoured), the two PNCP rows, and the two WordPress.org registries, whose
  `request[browse]` offers rankings and a descending recency and no ascending
  key. Three lessons:

  * **Ask for the row's own `id_keys` and you check the row as well as the
    order.** `$orderby=_RecordID` on WHO's FluNet answered "Could not find a
    property named '_RecordID' on type 'FLUMART.VIW_FNT'" — the field that row
    had keyed every record on since it shipped. The engine was already saying
    so: the run files a `shape:id-keys-unmatched` notice and content-hashes
    each uid, so a week WHO later revises is stored as a NEW row instead of
    updating the one it corrects. Identity measured on 1,000 mid-collection
    records: `ISO2+ISOYW` is 795 distinct of 1,000, `ISO2+ISOYW+ORIGIN_SOURCE`
    is 1,000, and the row carries that now (2,000 + 2,000 = 4,000 distinct over
    two runs). **That sweep is now done: 538 rows, 5 real hits**
    (`docs/measured-idkeys-2026-10-24.tsv`) — every cycling JSON row that
    declares an `id_keys`, one page each, the first record flattened as
    `hp_json_flat` flattens it and each key token looked up as `hp_flat_get`
    looks it up. 494 OK, 21 unmeasured (hosts that reset this path), 15
    unparsed (POST-only rows a GET probe cannot ask), 3 with no record array.
    The five: four Municipal Money v2 cubes keyed on
    `municipality.demarcation_code`, which no cube publishes (the engine had
    been saying `matched 0 of 100 record(s)` on every run), now on each cube's
    own dimension tuple, measured 1,000 distinct of 1,000 facts; and
    `SI_GURS_HOUSE_NUMBERS` on `properties.HS_MID`, which that layer does not
    publish, now on `properties.EID_HISNA_STEVILKA` — the EID the feature's
    own `id` is built from and the field the row already sorts on.
    **And the first run of that sweep said 35, of which 30 were the checker's
    fault**: `hp_flat_get()` falls back from an exact flat-key miss to any key
    whose LAST dotted segment matches (case-insensitively, skipping a leading
    `@`), so `iata_code` resolves against an ArcGIS record's
    `attributes.iata_code` — `IMF_PORTWATCH_AIRPORTS` stores uids `RFN`,
    `YOA`, `MVA` and files no notice. A checker that does not mirror that rule
    indicts most of the ArcGIS and GeoJSON fleet. What the fallback cannot
    save is a key token that itself contains a dot: it compares the whole
    token against one segment, which is why the Municipal Money rows were real.
    **The sweep asks whether a field EXISTS, not whether it is unique**, and
    two rows it passed were wrong the other way: `ZA_MUNIMONEY_AUDIT_OPINIONS`
    keyed on `demarcation.code` (one row per municipality per YEAR) and
    `ZA_MUNIMONEY_OFFICIALS` on the municipality (one official per ROLE), both
    measured composites now. A minimal-distinct-key search over one page would
    have dropped `demarcation.code` entirely, because a Municipal Money page
    covers one municipality — **the key is the cube's dimensions, not the
    smallest tuple that separates one page**.
  * **Prefer a key whose ties are impossible over one whose ties reshuffle.**
    MSRC takes `releaseDate` or `id`, not both (the composite is refused).
    Hundreds of advisories share a release date and the order inside a tie is
    undefined, so a tie block can reorder under a resumed walk and lose a
    record for good; a guid order is total, and an advisory inserted
    mid-collection is simply read next cycle.
  * **A measurement is not a code change.** The applier appended `&sort=…` to
    the one row whose URL had no query string, so Misskey asked for
    `/instances&sort=…`: one record, 135 bytes, `rc=0`, and only the two-run
    engine reading caught it. A sweep of every URL literal for a `&` before any
    `?` found nine more, all correct — five JMA `#fragment` portal links, an
    open-meteo docs link, and two N2YO calls whose API really does document
    `&apiKey=` appended to the path.

  **And it found a rule-4d defect in 40 rows: a Socrata domain's own
  `/api/catalog/v1` is not scoped to that domain** (2026-10-24,
  `docs/measured-socrata-scope-2026-10-24.tsv`). This is the batch-34 finding
  in 39 rows that shipped after it plus one that predates it, and the ACT row's
  probe gave it away by answering in Spanish. Measured both ways on all 40:
  unscoped each answers `resultSetSize` 10000 and 39 of 40 answer another
  portal's dataset FIRST (Dallas, for 38); scoped each answers its own
  catalogue — Honolulu 453, ACT 784, NYC 3,022, Maryland 3,606. Every record
  they stored was attributed to the domain in the row's name whoever owned it,
  with every gate green. All 40 carry `domains=`, `search_context=` and
  `order=createdAt` now (the order, and the reason, that the cross-portal rows
  in `vsrc14_us_state_3.c` already carry); the cross-portal rows on
  `api.us.socrata.com` are deliberately unscoped and were left alone, because
  they claim no domain. Two things to know about that set: **eight of the 40
  hold more datasets than one run reads** and, being VJSON rows, they cannot
  declare `page_resume` — they read the oldest-created 2,000 and file a
  truncation notice, and moving them onto hpengine for a cycle is the named
  follow-up; and **`chronicdata.cdc.gov` is now an alias of `data.cdc.gov`**
  (same 1,478 ids at offset 0 and 1000, identical `/api/views/metadata/v1`), so
  the five chronicdata rows collect what the data.cdc.gov rows collect. They
  are recorded rather than removed: identical data, duplicate requests, and the
  removal is a registry-count decision for the repository owner.

  **And the 44 unresolved verdicts of the first sweep were re-run**
  (`docs/measured-cycle-retry-2026-10-22.tsv`), because "re-run each
  FETCH_FAILED once" is this file's own rule and inferring 44 rows from five
  proxy log lines is not a measurement. 12 recovered on the second reading (one
  of them, `ELIA_GENERATION_BY_FUEL_HIST`, only when run ALONE — its failure
  was the sweep's own concurrency), 7 are the upstream speaking (4 × 503, a
  429, a 403 and a 401 that wants a credential), and **22 fail for plain `curl`
  from this container exactly as they fail for the engine** — connection reset,
  empty reply, an untrusted certificate — so they are a reading of this path,
  not of the rows, and nothing is retired on them. Three flap: they answered
  `curl` 200 once and 000 minutes later, which is why the first reading's
  "answered curl but failed in the engine" theory was dropped rather than
  built on. **A second probe of a host that just answered is the cheapest way
  to find out whether you measured the host or the moment.**

  533 rows cycled when this was written; 669 do on 2026-10-22, and the sweep
  above is how to know they still can. One shape to keep in mind: `interval = 2592000` is
  monthly, so a 500-page collection on that cadence takes about two years to
  come round — better than never, and the notice says where it is.
* **A deep-paging cap is the upstream's, and it is disclosed by name.** GLEIF
  refuses `page[number]=51` with HTTP 400 — 50 pages × 200 = 10,000 records —
  so `GLEIF_LEI_HK` reaches 10,000 of 13,419 and the rest needs a narrower
  filter per request. The notice says exactly that, including
  `failed_page_url`. Leaving `page_max` ABOVE the cap is deliberate there: the
  notice then names GLEIF's refusal rather than our own ceiling, for the price
  of one 400 a run.

  **A THIRD SPELLING: HTTP 408, AND A 408 THAT RETURNS IN 0.4 s IS NOT A
  TIMEOUT** (2026-10-25, `EBI_INTERPRO_PROTEINS_REVIEWED`). Walking
  `www.ebi.ac.uk/interpro/api/protein/reviewed/` down the API's OWN published
  `next` cursors: pages 1 to 10 each serve 500 rows in about 0.7 s, and page 11
  answers **408 in 0.4 s** — repeatably, for that cursor and every later
  attempt. So the cap is 5,000 records of the 573,015 it declares, wearing the
  status of the one thing it is not; the three spellings now on record are
  GLEIF's 400, the empty HTTP 200 of Japan Search and HUDOC, and this.

  **And `page_resume` on a capped cursor is WORSE than no cycle at all**, which
  is the half worth remembering. The engine correctly does not advance the
  position when a page fails — so run 1 stored the REFUSED cursor as its
  position, and run 2 resumed from it, read one record and cleared. The row
  would have ground over the same 5,000 for ever, and the 292-second first run
  was retries against the 408 rather than slow pages. Before giving a
  cursor-paged row a cycle, walk the cursor by hand to the ceiling: if the
  position it would store is refused, the row wants `page_max` plus the
  disclosure, not `page_resume`.
* **`page_size` on a bracketed page-number parameter hid from the lint that
  exists for it.** `_PAGENUM_NAME` in `tools/lint_sources.py` anchored the name
  list with `$`, so JSON:API's `page[number]` — written percent-encoded in a
  URL as `page%5Bnumber%5D` — matched nothing, and **63 rows shipped three days
  after that check was added** with page_size on a page NUMBER. Every one asked
  for page 201 (GLEIF) or 501 (US Treasury Fiscal Data) as its SECOND request,
  got HTTP 400, and stored one page: `GLEIF_LEI_HK` 200 of 13,419,
  `US_TREAS_MTS_TABLE_2` 500 of 2,070. Removing `page_size` (the size is
  already in the URL as `page[size]`) took those two to 10,000 and 2,070. The
  pattern now carries the bracketed and encoded spellings, and reintroducing
  one `page_size` fails the gate by name.
* **`more_pages_pending` is not the test for "data remains".** It reports
  whether the walk still held a next-page URL when it stopped, and it is FALSE
  at the ceiling. The authoritative signal is the upstream's own total —
  `records_available` with `records_available_basis: "upstream declared this
  total"` — exceeding `records_used`. `JPN_EGOV_DATA_ORG_ORG_0300` files 300 of
  a declared 325 with `more_pages_pending: false`, so a checker keyed on the
  flag calls it complete while 25 declared records are dropped every run.
  `tools/audit_page_ceiling.py` keys on the totals for that reason.

  **And until 2026-10-20 that authoritative signal could not fire for two
  whole classes of row.** The trigger was `page_walk && declared_total >
  available`, so a row making ONE request against an upstream that said it
  held fifty times more disclosed NOTHING; and `hp_run_json` was the only
  place a declared total was ever read, so no XML row in the tree could state
  a remainder whatever its upstream published — 27,258 scheduled XML rows with
  no paging mechanism at all. Both are fixed; `hp_xml_declared_total()` reads
  only the text BEFORE the first record, because a name like `TotalCount` can
  occur as a field OF a record and reading one record's field as the
  collection's size makes the disclosure itself a fabrication.

  Finding that first record needs the record loop's own rule that **a tag name
  is only that name when the next character ends it**: KKJ wraps
  `<SearchResult>` records in a `<SearchResults>` envelope and puts the count
  inside it, so a plain `strstr` for `"<SearchResult"` stops at the WRAPPER and
  the searchable span is the four lines above it. `hptest` 42, 42b and 42c.

* **Probe every offset spelling before concluding a row can page — or that it
  cannot.** 48 KKJ rows declared `page_param = "Start"` and walked 20 pages
  that each re-emitted page 1. `Start`, `Offset`, `Skip`, `From`, `Page`,
  `PageNo`, `Index`, `StartIndex`, `Start_Index`, `First`, `Begin` and
  `Position` were each sent with a real value and each returned the same first
  page, so the row's shape was decided by twelve requests rather than by a
  guess in either direction. **A remedy that names a knob which cannot move is
  worse than none**: "raise max_items/page_max" is right for a walk stopped at
  a ceiling and useless for a row that never walked, and a row with no paging
  is now told to ask the endpoint what it honours and to slice the QUERY where
  no offset works.

  A corollary worth the one request it costs: **count the members of a set
  that looks complete.** These rows covered 46 of Japan's 47 prefectures —
  三重県 was simply absent, and LG_Code=24 answers 5,468 notices.
* `next_path` accepts a `key=value` segment: write **`links.rel=next.href`**,
  not `links.1.href`. Indexing a hypermedia link array positionally breaks when
  a server reorders it, and the failure mode is the engine refetching page 1
  until the page ceiling — every later page lost, with the run still looking
  successful.
* **A KEY MAY CONTAIN A DOT, and a dotted-path resolver that always splits
  cannot see one.** `lib/jsonlist.c`'s `dotted()` split every path
  unconditionally, so `"@odata.nextLink"` — which sat in its next-link list,
  under a comment naming OData as a supported dialect — was looked up as
  `doc["@odata"]["nextLink"]`, which no OData server has ever published. Every
  OData v4 row in the tree stopped at page 1, with the capability claimed in a
  comment two lines above the code that could not deliver it; the same split
  hid `odata.count`, so no OData row could state its remainder either. Both
  resolvers now fall back to the literal key **after** the split fails, and
  only for a path that had a dot to split, which makes it strictly additive:
  every path that resolved before resolves to the same node. Both OData
  spellings of the total are in the declared-total list. Measured on LegCo
  Hong Kong, whose `$inlinecount=allpages` answers `odata.count: 551619`
  beside 200 rows.

  **And `hp_path` in `lib/hpengine.c` is the OTHER COPY**, fixed in the same
  change rather than left to be found later — it resolves the path a ROW
  declares, so `next_path = "@odata.nextLink"` would have split identically.
  This file already records the uid-collision guard existing in three record
  paths and being repaired in one; look for the other copies.

  `hptest` 47, 47b and 47c, all three confirmed to fail under the old
  behaviour — and 47's FIRST version passed under both, because `$top`/`$skip`
  let the cursor inference reach page 2 without the link at all. It names a
  URL with no size parameter now, so the published link is the only route
  there.
* A "CSV" feed is often not comma-separated. DataPlane.org publishes
  `ASN | AS name | ip | lastseen | category`; declare **`csv_delim=pipe`** (or
  `tab`, or `semi` — named, because the manifest is itself pipe-delimited) or
  the whole line lands as one unqueryable cell. **`csv_comment=#`** strips a
  banner before the parse, which matters because URLhaus keeps its column names
  in a `#` line and header parsing would otherwise name every column after a
  comment.
* **A generated row's `id_keys` is a guess until it is measured.** Batch 36's
  generator keyed every openFDA row on the record's FIRST field — an identity
  on four endpoints and a dimension on the rest (`status`, `third_party_flag`,
  `update_type`, whole arrays). The in-page collision guard content-hashes
  such rows, so `stored` matched `emitted` and nothing looked wrong; the cost
  is that every uid is a content hash, so a recall whose status changes is a
  NEW row instead of an update. A key naming a bare ARRAY
  (`establishment_type`) matches no flattened field at all — name the element
  (`establishment_type.0`). The same batch's World Bank `country/all` rows
  read 200 of 17,490 country-years, one request per run: the seventh discard
  shape, a limit asked of the upstream (`tools/audit_url_limits.py`).
* Titles: the engine will key a record on its first non-empty scalar rather than
  drop it, so a row without `title_keys` still emits — but it emits titled
  `<record_type> <whatever came first>`. Declare `title_keys`/`id_keys` for
  anything whose fields are not named `name`/`title`/`id`.
* **`{date:FORMAT}` / `{date:FORMAT:±N}`** in any templated URL is today's UTC
  date shifted N days, through strftime (`{date:%d-%m-%Y:-2}`). For an
  upstream that answers one day at a time; without it a row pinned its authoring
  day and fetched that one day forever while reporting success.
* **`array_path = "a+b+c"`** (HP_JSON) emits several sibling arrays of one
  response — SIDOF's morning, evening and extraordinary editions. An absent one
  is not an error; none at all is the declared-path-missing case.
* A walk whose URL carries no page number (or `page=0`) asks for page 1 next and
  compares it with the first page: identical means 1-based, continue at 2;
  different means 0-based and page 1 was new data. It used to jump to 2, so
  every 0-based API lost its second page (Diavgeia, 99 of 100 records).
  Paged JSON rows stop on repeated RECORDS, not a repeated body, so an ignored
  cursor plus a changing envelope (a timestamp) no longer re-emits 20 pages.
* CSV: the unterminated-quote repair fires only on a real malformation — a long
  cell whose closing quote is followed by the delimiter or a line end is a cell
  (`csv_parse_wellformed()` skips the repair entirely, for xlsx-written text).
  Blanks before an opening quote are padding in every mode: abuse.ch writes
  `"a", "b"`, and comma mode used to keep the quotes and split tags on commas.
* **A record's `body` is text, never its JSON** — and the sink no longer
  trusts that it is. `socrata-sf-business-registrations` passed each
  licence's JSON as `body` while stamping the same "DataSF open data" /
  "San Francisco, CA" values on every record, and `core/simhash.c`
  fingerprinted all 368,735 as near-duplicates: one false cluster, and a band
  bucket each insert had to scan (23 records a second, until the body became
  the business's name, owner, address and industry; then 28 min for all of
  them, in 26,883 real clusters). 205 hand-written collectors pass the JSON
  as `body` (309 sites, 2026-10-25), so the sink now indexes and fingerprints
  a JSON body by its VALUES, the way it indexes `properties`
  (`fts_segment_body()`, core/fts_schema.c, in all five places a body is
  segmented); the stored body is unchanged. Measured on two of them, the
  JSON body alone made no false cluster — IRS exempt orgs' largest cluster
  is 267 chapters of "PTA NEW YORK CONGRESS" before and after, and New
  Zealand charities is 100 records in 100 clusters — so what the change buys
  is keys out of search and fingerprints; a collector that stamps constant
  values on every record still needs a real text body of its own.
* Use `jo_strcasestr`/`jo_memmem` (`lib/jocore.h`), never `strcasestr`/`memmem`:
  glibc declares them only under `_GNU_SOURCE`, which this tree never defines,
  so on Linux CI they were implicitly declared with a truncated return.

Every source self-registers with `REGISTER_SOURCE` (or `HP_REGISTER_TABLE`) and
is both schedulable (`update_interval_sec > 0`) and dispatchable as an
entity pivot. There is no separate "service" type. Dispatch resolves a
misspelled id to the nearest ENTITY PIVOT only, and runs it under the resolved
id; it used to run under the misspelling and log the failure against the
healthy source.

