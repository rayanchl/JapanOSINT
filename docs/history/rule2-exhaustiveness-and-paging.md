<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): Rule 2 (never discard data): exhaustiveness audit, URL limits, shared helpers, detail hops. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

## 2. Never discard data — a source that is called must be used exhaustively

**If we spend a request on a source, we use everything it gave back.** Every
record, every field, every page, and the detail endpoint behind each list hit.
Nothing is dropped at a seam between collector, sink, API and client.

A consumer that physically cannot take everything (an LLM prompt, a mobile list)
may bound its own *view* — but the full data must still be fetched, stored and
served, and the bounded view must state in-band how much it is showing out of
how much exists. Silent slicing is the violation.

If anything was left unused, it is reported as data (a
`collector-truncation-notice` record), not as a log line nobody reads.

Rule, examples of violations, and what the shared machinery guarantees:
`docs/SOURCE_EXHAUSTIVENESS.md`.

VJSON collectors page through `jsonlist_emit_paged()`, whose per-page decision
— a next link the server published, else a cursor paired with a page size the
URL declares, else a page number the URL carries when the upstream's own total
says more remains — is `jsonlist_next_page()` in `lib/jsonlist.c`. hpengine rows
declaring `page_walk=1` call that same function, so a row moved from VJSON onto
hpengine pages exactly as it did. Without that (or an explicit `next_path` /
`page_param` / `{page}`) an hp row makes ONE request.

Where the tree actually stands, as `make audit-sources` reports it:

* **strict set — 0 findings across 342 files** (2026-10-20; 308 on 2026-10-07,
  256 on 2026-09-27):
  `collectors/pivot/table/hp*_*.c` plus the generated deep-record tables
  `collectors/feed/generated/hp1[0-9]_*.c`. This is the part the Makefile
  gates on, and it is held clean. Run `make audit-sources`
  after adding a table: batch 18 introduced two `single-page` findings here (a
  paged endpoint declared without `page_param`) and they had to be fixed before
  the gate would pass again.
* **the rest of the tree — 0 findings** (1,744 files scanned, 2026-10-22; 1,678
  files on 2026-10-07; it first reached zero on 2026-08-24, from ~127 across
  ~74 files). **It had drifted to 50 findings across 27 files and nobody
  noticed**, which is the exact property this entry exists to protect — and
  TWO sessions found the same 50 on the same day, independently: 49 URLs handed
  to `pw_walk_max`, `od_walk_rows_max`, `od_ckan_walk` or `od_ods_catalog_walk`
  — walkers the scanner did not name — plus the `.url` copy of each in its
  `source_def`. See the note below. Every first-only, single-page, record-cap, loop-break,
  limit-one and dedupe-ring finding has been read and closed one of three ways:
  the discard was real and was fixed, the line carries an `exhaustive-ok`
  marker with a reason, or it was a scanner false positive and is marked as
  such. The recoveries were not small — IRS exempt-orgs went from 5,000 rows to
  278,014, ESMA from 1 to 1,377, Homebrew from 300 to 23,133, and twelve
  scheduled feeds were asking their upstream for `limit=1`.

  So "zero audit findings" is now true of the whole tree, not just the strict
  set — which makes any NEW finding a regression rather than a number in a
  backlog. Keep it that way: `make audit-sources` is cheap and takes seconds.

  **And it DID drift, for thirteen days, because the scanner stopped seeing
  what the tree had learned.** On 2026-10-20 it reported 50 findings across 27
  files, every one a false positive, in two shapes:

  * `pw_walk_max` and `od_walk_rows_max` — the explicit-page-ceiling variants
    added on **2026-10-12**, in the same pass that fixed those very portals —
    never reached `_WALKER_CALL`, whose `\bpw_walk\s*\(` cannot match
    `pw_walk_max(`. Nor did `od_ckan_walk` / `od_ods_catalog_walk`, whose
    bodies hand the caller's URL straight to the walker. 29 findings. This is
    the same shape as `gen_hp_batch.py`'s forgotten opt table: **a new piece
    reached the engine and the collectors without reaching the tool that
    checks them.** When you add a walker, add it there in the same change.
  * a `source_def`'s **`.url` is the human-facing portal**, as `source.h` says
    and as rule 3 above repeats — not the request, which a bespoke collector
    names in its own `#define`. Flagging it was a category error worth 20
    findings, several in files whose request line is handed to a walker a dozen
    lines below.

  Both are fixtures now (5 and 6 in `SELFTEST`), and the fixtures were checked
  against the OLD behaviour rather than only the new: restoring it fails
  exactly the five shapes they cover. The 50th finding was `int cap = 0` in
  `lib/htmlparse.c` — a row's GROWING allocated capacity, the opposite of a
  record cap — which now carries an `exhaustive-ok` marker on its own line.

* **A bound in a loop's own CONDITION was invisible until `loop-cap`.**
  `while (cJSON_GetArraySize(akas) < 24 && …)` is neither a `#define …MAX`
  (`record-cap`) nor a `break` (`loop-break`), so the tree read "zero" while
  the OFAC consolidated parser dropped a designated person's aliases past the
  24th, TDnet stopped at 100 disclosures a day and EDINET-x at 25 filings. The
  check exists now; all of those are fixed.

* **A SEVENTH shape the six checks cannot see: a record limit the row asks
  the UPSTREAM for.** Every one of the six looks for a bound written in C — a
  `#define`, a `break`, a loop condition, a `seen[]` ring, a first-element
  read, a single request. A limit spelled into a URL is none of them, so the
  tree read "0 findings" while `go-module-index` asked index.golang.org for a
  six-hour window with `&limit=200` and no paging, and stored **200 of the
  22,248 versions that window holds** — 22,048 Go module publications dropped
  per hourly run, at `rc=0`, `records=200`, `stored=200`. Both of rules 4 and
  4b's "the numbers look fine" traps at once. Measured 2026-10-05; that row is
  retired in favour of `GO_MODULE_INDEX`, which pages on
  `next_path=$last.Timestamp`.

  **And the worst instances of it are a PROBE's limit that shipped** (2026-10-20,
  `docs/probe-sized-pages-2026-10-20.md`). Found while measuring something else:
  `us-legistar-seattle-matters` carried `$top=2` and read **40 records over 60
  requests**. Asking how many rows share the shape found **39** — `$top=1`,
  `$top=2` (17 rows), `limit=3`, `count=3`, `per_page=5`, `size=2`, `rows=3` —
  and **every one is an `hp16_*` detail-hop row**, where a tiny limit is right
  in the authoring PROBE and was carried into the row verbatim. None of them is
  in the 2026-10-11 file, which covered only the 42 candidates whose URL is a
  complete literal, so this was unexamined rather than triaged.

  Two things generalise. **A generated row inherits its probe's assumptions**,
  and a probe is written to be cheap — the same mistake as a date frozen into a
  URL at authoring time (43 rows) and batch 36's generator keying every row on
  the record's first field. **And the right page size is set by the DETAIL
  BUDGET, not by the API's ceiling**: `hp_detail_admit()` spends 1,000 hops a
  run, so a page size raised to the maximum with `page_max` left at 20 stores
  20,000 records of which 1,000 are deepened and 19,000 are stamped
  `_detail_pending` — and never deepened again, because the budget is spent in
  page order so the same first thousand get the hops every run. Each row is set
  to page size x `page_max` ~ 1,000 instead, every record stored complete.
  Nominatim's `limit=5` on a single structured address stays (a ranked geocode:
  the limit IS the question) and so does `us-sf-311`'s `page_size=5`, whose host
  429s at page 20 even under a 1,200 ms gap.

  **42 of those candidates are now MEASURED** (2026-10-11,
  `docs/measured-url-limits-2026-10-11.tsv`): each upstream was asked the same
  question with a tenfold limit. **18 are truncating**, 5 are complete, 19 are
  unreachable from here. The first fix was the highest-leverage one:
  `od_udata_collect()` in `collectors/sources/od_shared.inc` made ONE request
  while the envelope published both `next_page` and the upstream's own `total`
  — so three national open-data catalogues stored 20 datasets each. It pages
  now, and discloses a ceiling or a `total` it did not reach:

      data.public.lu   20 -> 3,193 stored (= its declared total)
      dados.gov.pt     20 -> 20,933
      data.gov.rs      20 -> 3,527

  27,653 records from three rows that together stored 60. **All 18 are fixed as
  of 2026-10-12**, each re-measured after, and the readings are in that file.
  Eleven rows went through `lib/pagewalk.c` rather than growing a loop of their
  own, so the continuation rules, the repeat-page guards and the disclosure
  stay in one place. Ten are now COMPLETE (`stored == emitted`, no notice):
  EONET 50 → 7,208, EMSC moment tensors 50 → 48,729, IFRC GO 50 → 6,088,
  OpenAerialMap 50 → 21,437, the PeerTube index 100 → 1,785, Argentina's series
  100 → 271, ECCC hydrometric 500 → 3,366, and Statistics Netherlands' CPI
  table 20 → 159,120, which is exactly its own `$count`.
  Five are bounded and DISCLOSED,
  because their corpus is larger than anything wants: Spaceflight News 2,000 of
  36,457, ECCC SWOB 10,000 of 18,950, and the STAC catalogues.

  **`pw_walk_max()`** is the new piece: an explicit page ceiling for a source
  whose total the UPSTREAM itself publishes. The 20-page default is a brake on
  an unknown upstream and raising it globally would lift that brake for all
  2,727 of them; `JO_PAGE_MAX` still overrides in both directions, and the
  notice now names which ceiling bit.

  **Two of the 15 were not truncation bugs at all, and no record count could
  have shown it.** `dea-stac`'s default order is OLDEST FIRST, so `?limit=20`
  stored the twenty oldest Sentinel-2 scenes over Africa — acquired 2015-10-22
  — and re-stored those same twenty every six hours while 6,594,309 items
  accumulated behind them: 20 of 20 emitted, 20 of 20 stored, every gate green.
  All four STAC rows and both ECCC OGC rows now ask for newest-first
  (`sortby=-datetime`, `sortby=-DATETIME`, `sortby=-date_tm-value` — the SWOB
  key keeps its `-value` suffix or it is a 400). **When a row carries a limit,
  ask what the upstream's DEFAULT ORDER is before deciding the limit is the
  problem** — a bound over the newest records is a window, a bound over an
  arbitrary slice is a lie about what the source holds. And
  `od_stats_nl_cbs` read `/ODataApi/`, which answers `$skip` with HTTP 500 and
  the remedy in its own body: skip exists only on **`/ODataFeed/`**, which
  additionally needs `$format=json` or the 200 is Atom XML. Every CBS row that
  needs to page needs that path.

  **AND THE SAME SHAPE WAS STILL IN THE TREE FIVE MONTHS LATER, FOUND FROM A
  COLLISION COUNT** (2026-10-25). `geo-copernicus-odata-s2` declared no
  `$orderby`, and an OData archive's unordered default is near INSERTION order
  — so its twenty pages were an arbitrary slice of 2015 products, re-stored
  every hour while 40 million accumulated behind them. The only symptom was
  2,000 emitted / 1,703 stored, and that is the harmless half: an unordered
  walk that serves one record twice skips another, and **the skip leaves no
  trace**. So a collision count is worth reading as a question about ORDER, not
  only about identity.
  Three things the fix measured, one request each. The endpoint VALIDATES its
  query string (`frobnicate=7` is HTTP 400), which is what makes an accepted
  `$orderby` evidence rather than a guess — batch 55's control, in the
  direction that licenses trusting a parameter. `$top` is honoured to 1,000 and
  422s at 2,000, and `@odata.nextLink` had been paging the row all along, so
  the defect was never the paging. And ASCENDING is the half that matters:
  Sentinel-2 publishes 1,915 products an hour, new products land at the TAIL of
  an ascending walk where the walk has not been yet, and a three-page read of a
  two-hour window came back **2,737 distinct of 2,737**. Descending would have
  kept the drift. The window is `{ago:25200}` — seven hours, not the row's
  declared one — because **a row's declared interval is not the cadence that
  runs**: the system-wide policy above it defaults to six, so a window sized on
  3600 drops five hours of publications on an unmodified install. With
  `$count=true` the window states its own total, so an overflow discloses
  itself instead of looking like a quiet hour. Measured: 8,620 emitted, 8,620
  stored, 9 requests, no notices, records dated yesterday and today instead of
  2015.

  Two more generalisable findings from the pass:

  * **A dedupe keyed on a different field from the row's `remote_key` is a
    silent 4b failure.** `gc-swob-observations` deduped on `tc_id-value` while
    keying rows on `tc_id-value` OR `stn_nam-value`, so every station without a
    tc_id passed the dedupe and collapsed at the sink — 25 of 384, measured.
    Dedupe on the identity, not on a field near it.
  * **`lib/seenset.c` already exists**; do not hand-roll a second one. It grows,
    which is the property a fixed ring lacks, and it replaced an O(n²)
    scan-the-whole-array dedupe that was tolerable at 500 records and is not at
    10,000.

  And a bound can be honest and still be a bug: `html_table_parse` allocated
  `width + 1000` cells for EVERY row, so the 1,000,000-row `htmlparsetest`
  fixture asked for **16 GB** and `make` printed `*** Killed` — which reads
  like a flaky runner, not an allocation bug (see the "that job is just flaky"
  note below). Rows grow towards the bound as their cells arrive; peak RSS is
  now 350 MB.

  **Then the SHARED HELPERS, which are where the leverage is** (2026-10-12).
  Three of them took one page while the caller's URL hard-coded a tiny size,
  and because they are shared the same discard was multiplied by every portal
  behind them. Each is now one `pw_walk` call, and the callers' URLs carry the
  offset the engine advances (`od_shared.inc` — the engine never invents a
  parameter, so a caller that forgets `start=0`/`offset=0` gets one page AND a
  notice rather than a silent single read):

  * `od_ckan_collect()` — **11 CKAN portals**, every one asking
    `rows=20&sort=id asc`, so each stored the first twenty datasets by id
    forever while `result.count` sat in the same envelope. Measured and
    re-measured live: Zurich 20 → 942, energydata.info → 1,191, London → 1,306,
    South Australia → 1,929, Berlin → 2,627, Western Australia → 2,917,
    Ontario → 2,964, NSW → 17,192, Queensland → 188,880 emitted / 188,879
    stored, its own `result.count` exactly, across 945 pages. (München 503s and
    open.africa 403s behind bot walls, at `rows=20` as much as at 200 — checked,
    not assumed.)
  * `od_ods_catalog_collect()` — **7 OpenDataSoft catalogues** at
    `limit=20&order_by=dataset_id`. ODS caps `limit` at 100, so the default
    ceiling covers all seven: Vancouver 20 → 200, Brussels → 207, Namur → 295,
    Basel-Stadt → 364, Thurgau → 458, Rennes → 672, Toulouse → 845. Every one
    equals its own `total_count` exactly.
  * `canada-hydrometric-realtime` — the OBSERVATION-level twin of
    `gc-hydrometric` on the same collection, `limit=500` of 19,108,759 with no
    window at all. A one-hour window, paged: 500 → 7,821. The two are a
    deliberate pair (newest-per-station for the map, every observation for the
    series) and must be fixed together.

  **A declared page size is not evidence the server honours it.** `pw_walk`'s
  same-body guard — stop when the next page comes back byte for byte — used to
  run on the INFERRED-size walk only, on the reasoning that a URL stating its
  own size documents an author who knew the paging contract. The London
  Datastore disproves it: `?rows=200&start=0` declares both, and that server
  honours neither, answering every request with its whole 1,306-dataset
  catalogue. 1,306 ≥ 200 reads as a full page, so the walk would have advanced
  `start` to the ceiling, re-emitted the same records every time, and published
  the multiple in the notice **as fact** — the disclosure becoming the
  fabrication. The guard runs on every walk now; it can only ever stop a walk
  early, because two pages of genuinely different records cannot be
  byte-identical. `pagewalk_test.c` pins the shape, and its fixtures now stamp
  a page serial so a multi-page test models a server that actually pages (they
  were arrays of empty objects, hence byte-identical, hence silently testing
  the broken case).

  **An offset or page walk with no STABLE SORT silently skips records, and the
  only visible symptom is a collision.** Measured twice on 2026-10-12.
  OpenAerialMap reported 21,437 emitted / 21,291 stored, and 10 contiguous
  pages of 100 really do hold 6 repeated `_id`s — so the first reading of this
  was "the upstream's own duplicates, nothing to fix". That was a true
  observation and the wrong diagnosis: an index that reorders between requests
  serves one record twice and another NOT AT ALL, and the skip leaves no trace.
  `order_by=_id&sort=asc` takes the duplicates to 0 per 1,000 and the run to
  21,437 / 21,437. Velib was the same: 1,519 / 1,508, and a
  `where=stationcode="13022"` query proved the dataset holds exactly ONE record
  for the code that had supposedly appeared twice. **A collision that is really
  a skip is the worst shape in this file** — the visible half is harmless and
  the invisible half is lost data. So: whenever a row pages by `offset` or
  `page` against a live dataset, give it `order_by` on the row's own id, and
  treat a small collision count on such a walk as a skip until measured
  otherwise. The four ODS records rows and OpenAerialMap carry it now; the CKAN
  rows already had `sort=id asc` and the ODS catalogues `order_by=dataset_id`.

  **And a walk's FIRST page can sit on a different ordering from the rest, for
  the plainest reason there is: it did not ask.** A row declaring
  `page_param`/`page_size` makes request 1 with the URL AS WRITTEN and only
  then starts advancing the parameter — so if the URL does not bind it, page 1
  is "whatever you serve by default" while pages 2..N are offsets into
  something else. Measured 2026-10-22 on the four Lechner Hungary INSPIRE
  layers: GetFeature WITHOUT `startIndex` is not the same page as
  `startIndex=0`, and `HU_INSPIRE_CADASTRAL_PARCELS` served
  `CP.CadastralParcels.107` on BOTH page 1 and page 2 — the same feature on
  every one of four readings — reporting 1,774 emitted / 1,773 stored. One
  parcel of Hungary's cadastre served twice and one never served at all, every
  run, with a one-record collision as the only symptom. The full 18-page walk
  by hand: as shipped 1,773 distinct, with an explicit `startIndex=0` 1,774,
  with a declared `sortBy` and no `startIndex=0` 1,774. **Bind the offset in
  the row's own URL and declare the sort.**

  **AND WHERE THE UPSTREAM PUBLISHES A KEYSET CURSOR, THE ANSWER IS NOT A
  SORT AT ALL** (2026-10-25, `collectors/pivot/table/hp3_inat.c`). The three
  iNaturalist rows read COLLISION in the sweep — 375 of 4,000, 200 of 4,000,
  116 of 2,000 — and the cause was the drift this file names: page 1 read twice
  a second apart is byte-identical, and **page 1 against page 2 overlaps by 10
  of 200**, because that API takes on the order of 4,000 observations an hour
  and a descending order over a live head moves under the walk. No ordering
  fixes that: ascending would read the 2008 observations for ever, and
  descending is the point of the rows.

  iNaturalist publishes `id_below` for exactly this, and it measures clean: 200
  ids strictly descending, then `id_below` of that page's last id returning 200
  more whose maximum is one below it, **overlap 0** — with the control
  `frobnicate=7` returning the identical set, so the cursor is doing the work
  rather than any added parameter moving the answer. The rows read down it now:
  4,000 / 4,000, 4,000 / 4,000 and 2,000 / 2,000, with every record carrying
  its coordinates (`latlon_key = "location"`, 4,000 of 4,000 with a latitude).

  **`page_resume` is deliberately NOT used there**, and the reason generalises:
  a resumed keyset would march backwards through 385,217,394 observations and
  never return to the head, which is what the cycling rules already exclude for
  a row whose purpose is recency. Each run starts at the head again and walks
  twenty pages down from it. The remainder is disclosed from the envelope's own
  `total_results`.

  Three things it cost, each worth knowing:

  * **`next_path` accepted a STRING only, so a numeric cursor was invisible** —
    and an auto-increment row id is the commonest keyset cursor there is. The
    row declared `next_path = "results.$last.id"` and made ONE request, storing
    200 of a declared 385 million. This is the blindness `hptest`'s `T_NUM_ID`
    already pins for identifier FIELDS ("cJSON_IsString alone was blind to
    every one of them"), in the paging path, found the same way: by watching a
    walk that should have paged make one request. A number is accepted now
    **only with a `next_tmpl`**, because without one the value is used AS the
    next URL and a bare number is not one; an integral value prints as an
    integer, never `4.1e+08`. `hptest` 56–56d, including that control.
  * **A `.url` built from a `#define` reads to `lint_sources.py` as the bare
    host and path**, so three rows whose queries differ only by a filter looked
    like one duplicate of a bespoke collector on the same endpoint
    (`dup-endpoint`, 1 above baseline). Inlining the URLs fixed it. The lint is
    right to normalise, and a row's endpoint is worth spelling out where a
    human will read it anyway.
  * **A move must not quietly change a row's cost.** Raising the research-grade
    row to 200 a page doubled it to 205 MB and 143 s a run against an hourly
    cadence; it keeps the 100 it had, measured at 101 MB. Fixing the drift was
    the job, not re-sizing the row.

  **A sort parameter can be IGNORED, which is rule 4d wearing different
  clothes.** Two of the eleven NADA microdata catalogues — DataFirst South
  Africa and Armenia — return identical first rows for `sort_order=asc`, for
  `desc` and for a different `sort_by` entirely. Those rows had been declaring
  an ordering the server does not give for as long as they existed. Ask twice
  with opposite directions before believing a sort, and drop the parameters
  where it is ignored rather than leaving the row claiming it.

  **Asking twice is not enough where the ANSWER ITSELF drifts: send a control.**
  The Austrian Parliament's filter API (batch 55, 2026-10-23) made 14 of 26 sort
  spellings look like they worked — and `frobnicate=7` reordered the first page
  identically, because that endpoint ignores its query string entirely and its
  index has no stable ordering. A parameter no implementation could honour is
  one request, and it is the only thing that separates "this sort works" from
  "this answer moves".

  **AND A PARAMETER THAT VALIDATES IS NOT A PARAMETER THAT WORKS** (2026-10-24,
  `gov-dataeuropa-search`). An hour after using "a bad spelling answers 400, so
  this endpoint checks its query string" as the evidence that UK Parliament's
  `_sort` is real — where it held — data.europa.eu's search showed the other
  half. Its GET ignores `sort` outright; its POST form **type-checks the key and
  then discards the value**: `{"sort":"id"}` answers a 400 naming the schema
  ("input don't match type ARRAY"), which reads like proof the parameter is
  live, and `{"sort":["id+asc"]}`, `["id+desc"]` and the control
  `["frobnicate+asc"]` then all return the identical first three records. A 400
  tells you a SHAPE was rejected; only an unknown field inside a well-typed
  value tells you the ordering is honoured.

  **The cost of not asking is a record count that measures the drift, not the
  source.** That row paged on the portal's own 0-based `page`, verified in
  September as "page=0 and page=1 are 100 distinct ids" — one lucky draw. Page 0
  IS stable across two reads; **page 1 is not**. So the walk's length came to
  depend on whether a reshuffled page happened to come back byte-identical and
  trip the repeat guard, and three CONSECUTIVE runs of the unchanged row
  emitted **500, 200 and 100 records, storing 103, 102 and 101** — same binary,
  same URL, same minute. It reads one stable page now (`pw_walk_max(…, 1)`,
  100 of 100 stored, one request, two runs identical) and discloses the rest.

  Closing it found three things in shared code, each the "other copies" lesson:

  * **VJSON could not disclose a declared remainder at all.**
    `jsonlist_emit_paged`'s notice fires on OUR ceiling or a full last page with
    no cursor — and the commonest other ending, the repeat-page guard, `break`s
    out before either is set. So a row whose upstream said it held 9,079 and
    which read 100 said NOTHING. That is the same gap hpengine closed on
    2026-10-20, in the other engine. There is a third trigger now
    (`available > fetched`), with `more_pages_pending: false` because the walk
    held no next page, and `records_available_basis` beside it. **Measured
    quiet**: of 14 healthy rows sampled from the registry sweep, 13 file no
    notice and the 14th was already filing one for a page-3 HTTP 500.
  * **`lib/pagewalk.c` keeps a THIRD declared-total list** beside jsonlist's and
    hpengine's XML one, and it reached neither `result.count` (CKAN's and
    data.europa's envelope) nor `result.totalResults` (the Linked Data API's) —
    so its notice read `records_available: null` with the number in the body.
    Both spellings were added to both lists in one change.
  * **A remedy must not name a knob that cannot move**, which this file already
    says and pagewalk still did: "give this source an offset/page parameter" is
    wrong for a row that declares a one-page ceiling ON PURPOSE. A
    `page_max_declared == 1` walk now says so and points at the row's comment.

  **A CEILING CAN BE THE UPSTREAM'S, SERVED AS AN EMPTY SUCCESS — and then
  "raise page_max" is the wrong advice for 130 rows** (2026-10-24, Japan
  Search's `jps-cross` item API; the family note is in
  `collectors/pivot/table/hp3b30_jplibrary.c`). The sweep read 39 of those rows
  COLLISION, about a fifth of each row's 2,000 records, and neither half of the
  cause is a property of any row:

  * **`from + size <= 2000` is a hard bound.** `from=1999&size=1` answers
    normally; `from=2000&size=1`, `from=1995&size=10` and `from=1900&size=200`
    each answer HTTP **200 with `hit: 0` and an empty list**. So every row's
    `page_max = 4` at `size = 500` is the API's ceiling and not ours, and the
    engine CANNOT tell the difference — it stops before asking for the page
    that would have refused. hpengine's remedy no longer asserts the knob
    works: it says to request one record past the ceiling and read an empty
    success as the upstream's own window, with slicing the QUERY as the route
    to the rest.
  * **The index reorders between requests and no sort pins it.** Two readings
    of the same four pages disagreed — in one, `from=0` and `from=500` were
    disjoint; in another they shared **266 of 500**. Six sort spellings
    (`sort=id`, `-id`, `common.title`, `-common.lastUpdatedDate`, `sortBy=id`,
    `order=id`) each returned exactly what the controls `sort=frobnicate` and
    `frobnicate=7` returned. The 2,000 records hold 1,732 distinct ids and
    **1,732 distinct whole records**, so the surplus is byte-identical and
    nothing distinguishable is lost at the sink — but a record the shuffle kept
    out of all four pages was never fetched.

  So the rows are a 2,000-record sample of a larger collection, redrawn each
  run, and **`hit` had to be added to the declared-total lists** before any of
  them could say so: it is the whole match set in that API (5,409 beside a
  500-record page), singular, so it does not share bare `count`'s ambiguity. A
  row that read 2,000 of 5,409 in silence now files "2,000 of 5,409 — upstream
  declared this total" every run. **Do not re-key these rows**: `id` is the
  item id and is already right.

  **And DCAT titles are language maps, which `pick()` was built to skip.**
  `title: {"en": …, "fr": …}` is an object, so the title picker skipped it, and
  `description` is the same shape, so nothing further down the list resolved
  either: 100 of 100 records a day stored as "procurement &lt;slug&gt;", with the
  engine's own shape notice saying so and nobody reading it.
  `lang_map_pick()` reads one such object — `en`, else the first member — and
  the discriminator is deliberately narrow, because `pick()`'s whole reason for
  skipping objects is that `{"lat":…,"lon":…}` must not become a title: at
  least two members, every value a non-empty STRING, every key a bare language
  subtag (two or three lower-case letters, optional region), and `id` excluded
  by name because it is two letters. `tests/unit/test_jsonlist_title.c` pins all
  of that plus the additive guarantee, and the 14-row sample re-run after the
  change is identical row for row.

  **And read the body on a 200 before trusting it: `sortBy=id` is a BACKEND
  400** on inspire.lechnerkozpont.hu, reported inside an HTTP **200** as a PHP
  warning page naming `localhost:8085`. That host is a proxy in front of a
  local GeoServer, so every upstream error arrives as a successful fetch of an
  error page — the Archive-It shape in a new place. The feature id is not a
  sortable attribute there; the sort key has to be measured per layer, with its
  own uniqueness (`kod,area,perimeter` is 5,000 distinct triples over 5,000
  consecutive land-cover features, where `kod` alone is 20 values).

  **Two large portals got a hostgate gap rather than a lower ceiling — and the
  first gap was wrong.** `data.qld.gov.au` is a complete read of ~945 pages and
  `data.nsw.gov.au` ~86. At 250 ms the QLD walk got 76 pages in 47 s and page
  77 answered **HTTP 429**; `lib/pagewalk.c` stopped and filed a notice naming
  the status and pointing at `core/hostgate.c`, which is the only reason the
  pacing was identified as the problem rather than the ceiling. The budget is
  not a per-second rate — twelve requests at 250 ms all answered 200 — so at
  **1,500 ms** the complete read finishes in 72 minutes with no 429 and no
  notice: 188,880 emitted / 188,879 stored, its own `result.count` exactly.
  A public portal is the reason to pace, not the reason to truncate; and a gap
  is a measurement, not a guess you leave in.

  **Six more individual rows, then the rest TRIAGED** (2026-10-12). WHO GHO
  100 → 14,112 (`@odata.count`), openFDA enforcement 100 → 18,002
  (`meta.results.total`), the ArcGIS layer 100 → 900 (`returnCountOnly`), BC
  mineral tenure 500 → 42,234 (`numberMatched`), FediDB 40 → 4,320 (a
  `links.next` cursor nobody followed), and the two EMSC FDSN rows and IODA.
  Two of them had the capability named **in their own header and unused** —
  openFDA's note said `total` "supports skip/limit paging" two lines after "the
  URL is used EXACTLY as verified", and the ArcGIS row emitted
  `exceededTransferLimit` into its properties "so truncation is visible". **A
  capability a comment claims is not a capability anything uses.**

  BC's WFS is the same unstable-order hazard from the other side: it **refuses**
  `startIndex` without `sortBy` ("Cannot do natural order without a primary
  key"). A hard 400 is the better failure — it cannot be mistaken for success.

  And **an honest empty must not read as a dead source.** Windowing the EMSC
  rows surfaced it: FDSN answers an empty result set with HTTP **204 and no
  body**, so `feed_get_json` returns NULL where rule 1's honest empty belongs,
  and `fdsn_quakes` reported `rc=-1`. The Iceland AOI held zero events for the
  last day, so the narrow window would have marked a working source dead every
  quiet day. Check what an upstream does with an EMPTY result before narrowing
  a window.

  The remaining 89 candidates are triaged **with their reasons** in
  `docs/measured-url-limits-2026-10-11.tsv` — ranked/keyed searches where the
  limit is the question, recency windows whose count is generous against a
  measured interval, rows already paged on a stride, and one
  (`trickest_cve`) recorded as **unmeasurable from here** rather than fixed on
  inference. Do not re-litigate them; read the file.

  `tools/audit_url_limits.py` finds the shape, and is deliberately NOT in
  `make audit-sources`: **a URL limit is not automatically a discard and the
  script cannot tell.** A relevance-ranked search pivot asking for the top 15
  has no complete answer to truncate — that limit IS the question. A scheduled
  read of a collection that has an end stores `limit` of however many exist,
  forever, and that is the violation. They are identical in C. The narrow scope
  reported **103 sites across 78 files** when it was written; after this pass it
  reports 89, and every one of those 89 is either a shape the lint cannot see
  through (a row that now pages via `pw_walk`, whose `.url` and page-ceiling
  lines carry `exhaustive-ok` markers) or a triaged non-discard recorded in
  `docs/measured-url-limits-2026-10-11.tsv`. It still is not gated, for the
  reason above: the script cannot tell a question from a bound, so a number
  from it is the START of the work and never the result.

Deliberate exceptions carry an inline `/* exhaustive-ok: <reason> */` marker
(`grep -rn exhaustive-ok`). The marker must sit **on the flagged line itself** —
the scanner matches per line, so a marker in the comment block above the line it
explains is silently ignored and the finding stays. That is easy to get wrong,
because the explanation naturally wants to be a paragraph: put the paragraph
above and a one-line `/* exhaustive-ok: … */` on the line.

Two amendments from the deep-record batch:

* **The gated set is now two globs**, `collectors/pivot/table/hp*_*.c` plus the
  generated deep-record tables `collectors/feed/generated/hp1[0-9]_*.c` — 342
  files as of 2026-10-20 (290 on 2026-10-05, 262 when this was written),
  0 findings. `--strict` had to be made repeatable to say that honestly:
  it took a single glob, so passing two kept only the LAST one and the gate
  printed "0 findings" for a set it had never opened.
* **`loop-cap` is a sixth check, and it was not redundant.** A bound written in
  a loop's own CONDITION is neither a `#define …MAX` (`record-cap`) nor a
  `break` (`loop-break`), so nothing could see it. Added after the tree reached
  zero, it immediately found four live caps — all in the OFAC sanctions
  collectors: aliases and sanctions programs capped at 24, addresses at 12, and
  SDN "features" (date and place of birth, nationality, passport and
  national-ID numbers, crypto addresses) at 40. On a sanctions list those are
  the fields screening matches on, so each dropped entry is a false negative on
  a designated person, produced in silence. All four are gone.

  Its own first regex missed the very line it was written for: `\b` before the
  counter list cannot match `cJSON_GetArraySize`, because the underscore is a
  word character. Zero findings from a new check deserves one suspicious look.

* **And it still missed ten (audit of 2026-10-02).** `loop-cap` required a
  trailing `&&` and could not reach a `for` condition; `loop-break` wanted
  `n >=` and missed `if (n++ >= N) break`; `single-page` excused a row by its
  NEIGHBOUR's paging within ±16 lines. Ten live caps passed as "0 findings" —
  OpenCitations stored 50 of 72,181, bike-share GBFS 1,000 of 17,273, J-STAGE
  text search 10 of 9,309. The checks now cover those shapes, scope context to
  the row's own `{ .id = … }` block, and the script runs its own fixtures first
  and exits 2 if any check stops seeing its case.

**Detail hops: 359 rows, and what a generator that rewrites this tree must not
do.** `collectors/gen_detail_hop_upgrade.py` moves a row that has a PROVEN
per-record detail endpoint off VJSON onto hpengine, so the record behind each
list hit is fetched. It reads the `.c` files, never the manifest, for the reason
the batch-tooling section gives: the C is the maintained copy.

It cost 73 sources to learn three things, all the same mistake wearing different
hats — the script could not see something, and deleted it anyway:

* `^(V[A-Z]+)\(` cannot match an underscore, so `VJSON_KEYED`, `VJSON_IDKEYS`,
  `VJSON_PREP` and friends were invisible — and the "no registrations left,
  delete the file" step could not see them either, so files were removed with
  those rows still inside. 20 sources.
* It writes one table per (batch, collector) group of the CURRENT run, so
  running it twice with different row sets rewrote a group's file with only the
  second run's rows. 53 sources. **Regenerate from a clean checkout of the
  directory in ONE run**, never incrementally.
* Its argument splitter did not skip comments, and this tree comments INSIDE
  argument lists; 86 rows parsed as 15 arguments instead of 12 and were passed
  over. That one failed safe, but it failed.

So it now asserts that no id registered in the directory before the run — from
the vsrc macros AND from any hp table already there — is missing after it, and
exits non-zero naming them. The only reason any of this was caught is an id-set
diff against the base branch; `--list-ids | sort | comm` costs seconds and is
the check that actually works.

`VJSON_KEYED`'s IDFIELD and `VJSON_IDKEYS`'s IDKEYS carry over verbatim into
`.id_keys` — `_vjson_idkeys.inc` promises the same `+` composes semantics, and
`hptest` now pins it, because rule 4b's failure is silent. The 45 rows that
moved on that promise are in `docs/detail-hops-need-emit-check.tsv`, and they
were **MEASURED on 2026-10-06**: the promise held. The five 8-part composites
(`af-dportal-trans-*`) store 90,785–96,154 rows each — had `+` been read as
`,`, keying on `aid`, each would have collapsed to its few thousand distinct
activities. Across all 19 COLLISION verdicts the loss is 218,769 emitted
against 218,692 stored, 0.035%, which is the upstreams' own duplicates.

What the pass found instead is what reasoning about `id_keys` could never have
found: **nine rows that fetch nothing at all** — eight `il-knesset-*` whose
OData service 303s to an HTML maintenance page, and `us-courtlistener-*`
answering 429 — and one real defect that was not identity either.
`us-sf-311-open311-requests` lost 5 of 20 because SF's Open311 server
rate-limited both the page walk and the per-record detail hop; pacing
`mobile311.sfgov.org` at 1200 ms in `core/hostgate.c` took it from 20 records
across 5 pages to 90 across 19. **A collision verdict is a question, not a
diagnosis** — measure the upstream before touching a key.

**A SECOND HOP IS A CLAIM, AND SIXTEEN OF THEM WERE FALSE** (2026-10-25,
`docs/detail-self-hops-2026-10-25.tsv`). The open item on
`us-sf-311-open311-requests` was "measure a hostgate gap for the detail hops",
because the row's own notice reported them refused — and the prior question is
what the hop BRINGS BACK. Open311 GeoReport v2 defines ONE representation of a
service request, so `/requests/<id>.json` is its single-resource form and not a
deeper record: asked of four SF cases chosen to differ (open and closed, with
and without `media_url`, with and without `status_notes`), the list and detail
representations are the same 13 keys with identical values every time, and the
other Open311 server in the tree answers the same way on 12 keys. **A gap was
about to be measured to pace a hop that had nothing to fetch.**

**The cost is set by the per-host GAP, because the budget is TIME.**
`hp_detail_admit()` spends 1,000 hops OR 120 s, so at this host's 1,200 ms gate
120 s buys about 48 hops: 48 of 1,074 records were deepened, always the first 48
in page order because the budget is spent in that order, and the other 1,026
carried `_detail_pending` — a disclosure that there was more behind the record
when there was not. 48 of the run's 59 requests. The row is 4 requests now, and
the 1,200 ms gap STAYS because it was measured for the page walk.

**Then the other copies, and the scan cost nothing.** A hop that re-reads its
own url is visible without asking any server, so the question went to all 627
rows declaring a `detail_url`: **16 have a detail_url that IS their url.**
`make lint-sources` fails the shape now (`detail-self-hop`, with a
`detail-self-hop-ok: <reason>` marker for a row that means it, as
`page-offset-ok` has). Four things about it generalise:

* **"Your detail_url is your own endpoint" is NECESSARY for a worthless hop
  and not SUFFICIENT**, so each of the nine outside the Open311 family was
  measured, one request. Three verdicts: **SELF** (every record's
  `detail_key` equals the id already in the url — `us-ma-ocpf-report-list`
  asked for THIS LIST fifty times a run), **NO-KEY** (the key resolves on no
  record, which is not a free no-op: `hp_deepen()` spends the budget first and
  then stamps `_detail_error: list record carries no detail key`, so three
  rows disclosed a failed detail fetch 7, 9 and 53 times a run for a detail
  that does not exist), and **WRONG NAMESPACE**.
* **"It reaches another id" is not "it reaches another record."**
  `openalex-institution-detail` first measured as REACHES — 25 distinct key
  values, none the url's own — which reads like a legitimate hop. The 25
  records are the institution's `topics`, so `id` is a TOPIC id and the hop
  asked the INSTITUTIONS endpoint for it: `institutions/T10009` is HTTP 404.
  25 requests, 25 404s, 25 `_detail_error` stamps a run. The id has to belong
  to the endpoint's namespace, and only a request says whether it does.
* **The check's first predicate was OVER-BROAD and the tree said so at once:
  110 findings**, the first ones read being CKAN `package_search` beside
  `package_show?id={v}` — a correct hop in a defect bucket, which is the one
  thing this file's verdicts must not do (`POD`, `GATED`, `WINDOW_EMPTY`
  again). On a query URL the last `/`-segment is `package_search?rows=100`,
  whose `100` passed a "contains four digits" test for an identifier.
  Narrowed to identical query strings, the difference in the last PATH
  segment, and that segment digit-DENSE. Ten fixtures run before the check on
  every lint run, and **the over-broad version was re-applied to them: it gets
  3 of the 10 wrong**, so the set discriminates rather than merely passing.
* **FOURTEEN OF THE SIXTEEN ALSO CARRIED ONE ENTITY FROZEN INTO THE URL on a
  schedule** (the two exceptions are the SF and Brookline list rows), which is
  the openFDA shape this file records under rule 3, found by the same scan: the
  authoring probe's URL shipped as the row, re-reading one subject for ever and
  emitting that record's nested array (`historiskenavn`, `sideactivities`,
  `altlabel`, `topics`, `proceedingsandorder`). Eleven are pivots now —
  `{q}` in the path plus `interval = 0`, no collector relabel — and
  **declaring `id_keys` was the fix rather than polish**: none of them had one,
  and Web NDL Authorities is JSON-LD whose only top-level scalars are
  `@context`, `created`, `label`, `modified` and `uri`, so the no-`id_keys`
  fallback ends at the record's FIRST SCALAR — `@context`, identical on every
  record in the API. Every person that pivot was ever asked about would have
  collapsed onto one uid, silently. **A scheduled row reading ONE entity can
  never show that; a pivot shows it on the second question.** One of them paid
  rule 4d back for free: `reportList/16928` answers an EMPTY items array where
  a server ignoring its path parameter would have returned filer 15021's fifty
  reports.

Three not converted, and the reason is a decision rather than a measurement:
`no-brreg-underenhet-detail`, `br-cnpja-open-office` and `br-cnpjws-publica`
mine a nested array of their one frozen record, so a conversion has to settle
what the record should be AND whether a company register that answers for any
CNPJ belongs on a 24 h schedule at all. One of the nine,
`br-ibge-municipio-detalhe`, is converted on the row's own authoring evidence
and **not on a reading** — its host timed out twice from the measuring
container, which is written down rather than filed as a verdict.

`VJSON_PREP` is deliberately NOT converted: its extra argument is a C function
that reshapes each page before emit, and a declarative row cannot hold code.
VRSS and VGEO stay out too — hpengine has no RSS or FeatureCollection mode.

