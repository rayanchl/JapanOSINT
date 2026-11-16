<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): the long "The registered count was ..." series and the merge decisions behind it. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

**The registered count is MEASURED ON THIS MERGE** (2026-10-25, the fifth merge
of main into this branch): `make source-count` is the number, never the
arithmetic, and the removals on both sides are why. This merge took main's
2026-10-25 HTML reachability sweep, and resolving it per row is the lesson
worth more than the count — **main's sweep measured ten of this branch's rows
at a URL they no longer carry.** Of the 23 rows in conflict, 13 had been
re-pointed here and measured on the new endpoint (the ZA gazette 3 anchors for
a real entity and 0 for an impossible one, Municipal Money 39 facts of a
declared 39, the ECB workbook, four Stortinget exports, Eduskunta's
`avoindata` API, and the three bulk-file stand-ins), so main's 404 is a true
reading of a dead path and a false one about the row that replaced it. The
other 10 were byte-identical to what main measured dead in two passes with the
page, the bare path AND the site root each checked, which is stronger than
this file's own "one probe is one reading" — so those removals stand, four of
them overturning a decision taken here to keep a dead pivot "disclosing".
**Both sessions had measured the same fact** (this branch's own note says
PMDA's search "answers the form page to every POST constructible here" and the
Assemblée's "404s with no form"); they differed only on the policy, and the
tree's own precedent settles it, from the FAA case: "Three sources removed
rather than left registered and failing every run." The three stand-in rows
that replaced them survive, so no coverage is lost. **A sweep's verdict is
about the URL it sent, not about the row's name** — re-read a removal against
the row as it stands now.

**The count measured on 2026-11-14 is 85,767** (batches 58–63 landed after the text below was written; `make source-count` and `tools/registry-floor.txt` are authoritative, and the figures below are dated history).

**The registered count is 61,527** (2026-10-30, `make source-count`, both
floors re-recorded from that build): 57,905 plus **batch 57's 3,622 Japanese
rows** — eight beats in `collectors/pivot/table/hp3b57_*.c`, every one measured
through the engine (3,580 OK, 346,732 records emitted in one pass, and NOT ONE
`DROPS_EVERYTHING`). Full note: `native/collectors/OSINT_SOURCES_BATCH_57_JAPAN.md`.

The method is batch 42's and 46's at national scale — **ask the publisher's own
homepage what it declares** — seeded from Wikidata's P856 for 1,797
administrations and 5,186 institutions, with every class id resolved through
Wikidata's own search API rather than recalled, because a wrong QID returns an
empty result set that reads exactly like a class with no members. Four things
it cost, each a mistake this file already names, caught before it shipped:

* **An exclusion list built by regex missed 1,499 duplicates.** A scan of the
  tree's C found 89,119 URL literals and looked thorough;
  `batch_exclusions.py --bin` then reported 1,499 municipal rows already held
  by batch 50 and the `vsrc` government feeds, and printed why — *"id set:
  57905 from the binary's registry (regex scan of the tree finds 46142)"*. That
  beat went 2,069 → 570. **Pass `--bin`: an approximation of the registry is
  not the registry.**
* **The first cut repeated the batch-47 mistake with the answer already in
  hand.** The feed pass had put museums, hospitals, libraries and universities
  in ONE beat labelled `science`, and the WordPress sweep grouped by host
  suffix, so copying batch 45/54's `jpwporg → grp_legal` would have filed 697
  hospitals and museums under Legal. Every row was re-beated by the class the
  HARVEST measured for its host — and, where an institution's feed lives on
  blogspot or tumblr, by the Wikidata QID in the row's own id. 0 left
  unclassified. **A beat named after the tool that found it is not a
  classification.**
* **`science` and `organization` are not categories this tree resolves**, which
  is what `collectors-def-check` was reporting as `UNASSIGNED`. No `OVERRIDES`
  entry was needed in the end: once each beat carried the category it really is
  (`research`, `education`, `health`, `culture`, `infrastructure`, `police`),
  all eight keys were grouped automatically — 640 collectors, 0 unassigned.
* **A sweep's cost must be MEASURED, not extrapolated from one run.** The emit
  sweep was abandoned at 8 jobs after three runs took 35-66 s each and
  projected to five hours. A 341-row stratified sample then measured the real
  throughput, and the full 3,622 ran in **20 minutes at 20 jobs**: these rows
  are network-bound across ~2,500 distinct hosts, so concurrency buys almost
  linearly and a single run's duration predicts nothing. Same error family as
  reading a file's size as progress.

Two beats are MEASURED BUT NOT REGISTERED, in
`docs/staging-sources-batch57.jpwp{media,corp}.txt` (2,094 rows), held back for
one reason: they are the beats still grouped by host suffix rather than by a
measured publisher class, so registering them would re-introduce the second
correction above. They are a record, not a to-do list.

**The registered count is 57,905** (2026-10-28, the merge of the two
readings below). MEASURED on the merged tree with `make source-count`, and
only then cross-checked, which is the order this file accepts: 57,921 less
the twenty twin-group rows, plus one, plus batch 56's three = 57,905, and the
binary's own registry agrees at 57,905 with 0 duplicates, 0 dropped and 0
never-runs. Both floors re-recorded from that build. Neither side's number was
wrong and neither was the merge's: a count is a reading of one tree, so two
branches that each measured correctly still cannot be added together.

**The registered count is 57,924** (2026-10-27, `make source-count`, both
floors re-recorded from that build): 57,921 plus **batch 56's three rows**,
the rest of the New Zealand Charities Register beside its organisations —
every officer (589,506), every annual return (430,937) and the charity groups
(357), `collectors/pivot/table/hp3b56_nzcharities.c`. The organisations row
itself had read the same first 100 of 46,144 charities every run, its links
had all redirected to the search page, and the register's other 22 OData sets
were unread. **A DECLARED TOTAL CAN BE A STRING**: OData v2's verbose JSON
writes `d.__count: "46144"`, and both declared-total lists accepted numbers
only, so every row on such a service read "the upstream declares no total"
with the number in its envelope. `pw_odata_v2_count()` (lib/pagewalk.c) reads
it, digits only, for both lists; `hptest` 58–58c, 58 and 58c checked to fail
without it.

**The registered count is 57,901** (2026-10-25): 57,921 less the twenty rows
of the nineteen twin groups below — one read registered twice, nineteen times
over. MEASURED from that build: `make source-count` 57,901 and the binary's own
`--list-sources` 57,901 with 0 duplicates, 0 dropped and 0 never-runs, both
floors re-recorded at 57,901. The number was predicted by arithmetic and then
read, which is the only order this file accepts.

**Measured 2026-10-25 on the merged tree at 005e4ee8** (main's 57,923 plus the two IOC sea-level retirements rebased onto it, `sci-ocean-ioc-sealevel-tab` and `geo-ioc-sealevel-stations`): registry 57,921, `make source-count` 57,921, both floors re-recorded at 57,921 from that build, not by arithmetic.

**Main's count was 57,394** (2026-10-25): 57,393 plus `US_FCC_ENFORCEMENT`
back as a hand-written collector (`collectors/sources/fcc_enforcement.c`) on
the FCC Content API's server-side title search (`search_string`, a PREFIX
match: "Verizon" 401 documents, an impossible term none). 57,393 was 57,395
less two — `US_FCC_ENFORCEMENT` in its old form, a search whose query never
reached the server (rule 4d), and `gov-hk-legco-bills`, a second reading of
the LegCo bills feed (`docs/removed-sources-2026-10-25-legco.tsv`). 57,395 was
57,529 less **134 dead HTML rows** found by its reachability sweep — the page
and its bare path 404/410 while the site answers, or the domain gone — each
with its evidence in `docs/removed-sources-2026-10-25-html-dead.tsv` (the
sweep itself: `docs/html-reachability-2026-10.md`).

**The registered count is 58,055** (2026-10-23): 58,054 plus `cyb-levelblue`,
the successor of a press feed whose host now redirects to another company —
see rule 1's note on a feed that has stopped being a feed.

**The registered count was 58,054** (2026-10-23): 58,047 plus **batch 55's
seven rows**, the Austrian Parliament's whole materials index read as seven complete
slices (`collectors/pivot/table/hp3b55_atparl.c`,
`docs/verified-sources-batch55.tsv`, `docs/at-parlament-filter-api-2026-10-23.md`).
It closes open item B2's last line, and the answer to "one measured sort
parameter away" was **there is no sort parameter**, with the row shipping anyway.
Four things in it generalise:

* **Probe a CONTROL before believing a parameter works.** 26 sort spellings were
  sent in the query string and 14 "moved" the first page — which is what the
  2026-10-21 triage reported as "six appeared to work". Then `frobnicate=7` and
  `sortColumn=99999` moved it exactly the same way, and no reading was monotonic
  in the column it named. The cause: **this endpoint ignores its query string
  entirely** — the filter is a POST JSON body (`{"GP":["XXVII"]}` answers 76,457
  where `?GP=XXVII` answers the unfiltered 391,354) — so every sort spelling was
  discarded before the server looked at it. An unknown key in the BODY is
  silently ignored too, so the same control was needed on both sides.
* **A complete single-request read beats a sort.** `page` really pages (page
  19,568 returns the last 14 rows) and really drifts: page 1 and page 2 of ONE
  filtered query overlap by **8 of 20 rows**, and two reads of `page=1` return
  disjoint sets. But `showAll=true` plus a filter answers `pages: 1` with
  `rows == count`, byte-identical across two reads — so the row is a PARTITION
  and no ordering is involved, the ENA `limit=0` shape again.
* **Enumerate the dimension you slice on, then check the slices SUM to the
  whole.** `GP` is the legislative period; enumerating I..XXVIII gives 24
  non-empty codes summing to 389,139 against 391,354 — **2,215 rows no Roman
  numeral reaches**. Sampling 30 pages for the missing codes found `KN` (1,897)
  and `PN` (316) and **missed `BR` (2)**, which only a direct count found. A
  sample finds the common gaps, not the rare ones; the 27 codes then sum to
  391,354 exactly.
* **Three keys agreeing on the same wrong answer is not corroboration.**
  `WENTRY_ID` (the site's own web-item id), `HIS_URL` and `GP_CODE+ITYP+INR` all
  give 75,485 of XXVII's 76,457, which invites writing the 972 off as the
  upstream's own duplicates — and all 972 **differ**, in exactly one column,
  `NRBR`: a material that went through both chambers is served once per chamber
  under one web-item id. `GP_CODE+ITYP+INR+NRBR` is 76,457 of 76,457 and holds on
  every slice. Measured through the engine: **391,357 emitted, 391,357 stored,
  7 requests, 222 MB, zero notices.**

**The registered count was 58,047** (2026-10-23): main's 57,529 merged with this
branch's 58,045, MEASURED with `make source-count`. Third merge of main in a
day; what it cost and what each side won is in `docs/SOURCE_COUNT_LEDGER.md`.

**This branch's count was 58,045** (2026-10-22): main's 57,527 merged with this
branch's 58,055 on the **57,536** base they share, MEASURED with `make
source-count` rather than computed — and the arithmetic cross-checks it to the
row: 57,536 + this branch's 519, less main's 9 `&amp;` duplicates, less **1**
for `JO34_CKANRES_CKAN_PUBLISHING_SERVICE_GOV_UK`, which main removed and this
merge takes. That last row is the merge's one real decision: this branch had
flagged it as "could be made to work and should not be" and left it
registered, while main measured both ends — `ckan.publishing.service.gov.uk`
and `data.gov.uk` answer 59,371 and 59,370 datasets within the same hour, so
the first host IS the CKAN instance behind the second and exposes no drafts it
hides. **Keep the canonical public endpoint, not the backend**: `data.gov.uk`
is the name that survives an internal re-platform, and it is the row that was
re-pointed and measured. A flag is not a decision; main's is.

**This branch's count was 58,055** (2026-10-22): 58,052 plus the three rows
open item B2's long tail ends with, each a publisher's own bulk file standing in
for a search that is gone —
`JP_PMDA_NEW_DRUG_APPROVALS` (the annual drug-approval workbook, 95 of 95 over
four monthly sheets), `FR_AN_QUESTIONS_ECRITES` (19,165 of 19,165 written
questions out of one 50 MB ZIP) and `JP_KANPO_DAILY_CONTENTS` (the official
gazette's daily table of contents, 25 of 25 carrying 43 notice titles). The
three dead pivots they stand in for are KEPT, disclosing: PMDA's search answers
the form page to every POST constructible here, the Assemblée's search path
404s with no form on the homepage, and the gazette's search is behind a paid
member login that no `key_env` opens.

**The registered count was 58,052** (2026-10-22): 58,019 plus **batch 54's 32
rows** and `FR_AN_DEPUTY_STAFF`. Batch 54 exists because of a correction:
**batch 45's staging set was ALREADY REGISTERED** — 5,392 of its 5,643
endpoints are in the tree and 5,392 is batch 49's own row count, so batch 49
*is* that staging set, while the file was still headed "NOT generated, NOT
registered". Believing that header would have created 5,392 duplicate
endpoints; it is a record now, not a to-do list. (Batch 37's file claimed
"still unprobed" for a fortnight after it was probed — same trap, second
time.) Of the 251 genuinely absent, 203 are machinery the sweep rejects by
design, 1 is the Miyawaka casino takeover main already removed, and 48 were
real content rejected by **two over-broad alternatives in one regex, both
found by measuring**: `form` was unanchored so it matched in|form|ation,
silently dropping the whole `information` family including a 1,074-record
member-company register, and `^xo_` (written for a slider plugin) also matched
`xo_event`, an event calendar. Details:
`native/collectors/OSINT_SOURCES_BATCH_54_JPWPLEFTOVERS.md`.

**The registered count was 58,019** (2026-10-21): 58,016 plus three rows from
open item B2's re-pointing pass — the ECB workbook's LSIs sheet (2,175 banks
`HP_XLSX`'s one-sheet read would have dropped at the seam) and two Stortinget
exports the tree was not reading. Five more rows were re-pointed rather than
added. `docs/b2-repoint-emit-2026-10-21.tsv`.

**The registered count was 58,016** (2026-10-21): main's 57,536 merged with
this branch's 57,892 on the 57,415 base they share, MEASURED with `make
source-count` rather than computed. The arithmetic cross-checks it exactly and
is worth writing down, because a merge of two branches that both REMOVED rows
double-subtracts: 57,415 + this branch's 477 + main's 121 = 58,013, **+2** for
the two FAA rows restored below and **+1** because `FAA_NNUMBER_REGISTRY` was
subtracted by both sides. Both readings are kept below, each
labelled with the branch whose reading it is.

**The three `registry.faa.gov` rows: this branch removed them and main revived
two, so main is right and the removal is overturned.** This branch measured
five agents against that host, concluded it refuses any agent carrying a
parenthetical contact route, and removed all three rather than drop our contact
route — explicitly declining to present as a browser, which `httpclient.h` then
forbade. Main measured the SAME host and changed the rule instead, for an
explicit per-host list. Re-measured here on the merge, one request each:
the browser profile answers **200, 54,752 bytes, 52 data rows** carrying
N-number, serial, make/model and owner, where the engine agent answers 403;
the N-number detail page answers 200 with 66 field pairs. So the premise of
the removal ("recovering these rows would mean dropping our contact route")
was true and the CONCLUSION was not, because a third option existed that this
branch had ruled out on policy rather than on measurement. `FAA_REGISTRY` is
restored by reverse-applying its own removal, `US_FAA_AIRCRAFT_REGISTRY` comes
back in main's repaired form, and `FAA_NNUMBER_REGISTRY` stays removed for
main's reason, which is the stronger finding of the two: once the 403 lifted it
stored the page's NAVIGATION LINKS as aircraft. **A row that was failing
honestly can start answering wrongly the moment a fix lets it through — read
what it stores, not that it stores.**

**Main's count was 57,529** (2026-10-22): main's 57,527 (below)
plus this branch's two on the same 57,536 base — the four tuberculosis
windows and the Elia imbalance-price row, less the three OpenDataSoft rows
whose datasets no longer exist (the cycle-advance sweep and the ODS title
sweep, both under "Cadence policy" / `page_resume`, and in the ledger). None
of main's nine `&amp;` duplicates was a row this branch touched.

**Main's count was 57,527** (2026-10-22): 57,536 less nine rows that
duplicated another row once their URL's `&amp;` was decoded. **A literal
`&amp;` in a row URL is not always a bug to fix**: it sends `amp;type=…`,
which the server ignores, and 1,076 rows had one. Decoded and re-measured
(`docs/amp-decode-2026-10-22.tsv`), 714 store as much or more (16 of them
only once decoded), but 206 stored
LESS — a newspaper search whose real filter finds nothing, a Joomla row then
served the Atom it asked for against an RSS declaration, a CMS honouring a
small `displayCount` — and keep the `&amp;` with a marker on the row. Measure
both URLs before changing one. Ledger: `docs/SOURCE_COUNT_LEDGER.md`.

**AND WHAT A DECODE LEAVES BEHIND IS A TWIN NO DUPLICATE CHECK CAN SEE**
(2026-10-25, `docs/removed-sources-2026-10-25-twin-rows.tsv`). A
`?feed=rss2&amp;` decodes to `?feed=rss2&`, whose trailing ampersand still
distinguishes two rows to a STRING comparison — so `check_dup_endpoint`, which
groups on host + path + query textually, could not group them, and neither
could the limit-pair harvest, which requires every other parameter to match.
**19 groups, 38 rows, 19 redundant registrations**, each group inside a single
file: a generator emitting one source twice, which noticed the NAMES collided
and appended `_2` to the second id rather than concluding it had one source.
They were found by fingerprinting the READ rather than comparing names —
normalised URL + array_path + id_keys + title_keys + record_type + detail_url +
detail_key + mode + csv_delim + post_body + next_path — and 31 groups became 19
once two over-broad normalisations were corrected, a `page` that is a CONTENT
selector and a dropped fragment that is a SHEET selector. The corrections only
ever shrink the claim.

Three things the repair measured that the finding had assumed, and each one
mattered:

* **the ampersand does not "parse to nothing on the wire" — it is a 301** to
  the canonical URL on 16 of the 18 hosts, so the duplicate row cost a redirect
  hop as well as a duplicate request. The server canonicalises the character
  rather than ignoring it.
* **one pair was never the same read at all**, which is why each was measured
  instead of the family argued. `ordinefarmacistiviterbo.it` answers the plain
  spelling with 7 items and the `&` spelling with HTTP **200 and a THREE-BYTE
  body** — a UTF-8 BOM, nothing else — with no redirect. So that row fetched 3
  bytes and stored nothing on every run since it shipped: rule 1's invisible
  nothing, hidden behind a sibling reading the same site correctly. A
  per-family argument would have retired it for the wrong reason and written
  down the wrong fact.
* **and the population carrying the artifact was 24 rows across six files, not
  25 across seven.** `hp2_eu_dach`'s
  `data.bka.gv.at/.../Judikatur?Applikation=Justiz&` is the FIRST PART of a
  multi-part C string literal and does not end that URL at all; a grep for the
  bytes `&"` cannot see the concatenation and reports it as a trailing
  ampersand. **Concatenate a multi-part literal before reading its last
  character** — this is the same shape as every other miscount in this file,
  where the tool could not see its real input.

The six genuine singles keep theirs, and that is measured too: each answers
both spellings with the same status, the same byte count and the same record
count, so the artifact is harmless there as a reading rather than as the
finding's assumption. Changing a live row's URL to save nothing is how a
validated row becomes unvalidated.

Which half survives is a per-row judgement and the removal file makes it: the
suffixed `_2` id is the generator's second emission, so the base id is the one
with stored history and other rows' references. For the one group that is not
this artifact — three World Bank country rows, two generated and one curated —
the curated row survives, and nothing is lost because
`api.worldbank.org/v2/country` answers `{"pages":1,"total":296}` at that row's
own `per_page=300`: one page is the whole collection, measured through the
engine at 296 emitted / 296 stored. Rows a removal leaves behind in an OLD
database are what `tools/cleanup_stale_uids.py` is for; it reports by default
and deletes only rows whose replacement is proven to exist.

**Main's count was 57,536** (2026-10-21): main's 57,415 (below) plus
this branch's 121 on the same 56,955 base — batch 51's 136, less 29
takeovers, plus 15 browser-profile feeds, less one duplicate FAA row — merged
with no shared endpoint. On this branch alone the count was **57,076**.


**The registered count was 57,062** (2026-10-21): the 56,955 below, plus
**batch 51's 136** Hong Kong and Macao government feeds and China News Service
(`collectors/sources/cn_feeds_batch51.c`, read off the publishers' own RSS
directories), less **29 rows found taken over or injected by a sweep of every
feed in the tree** (42,593 URLs, 52 flagged, each read by hand —
`docs/removed-sources-2026-10-21-takeovers.tsv`). New tool:
`./bin/japanosint --discover-feeds <sites> [--jobs N]` (`core/feeddisc.c`)
asks each homepage which feeds it declares and verifies them through the
engine's own client, flagging `TAKEOVER_SUSPECT` on item titles.

**Main's count was 57,415** (2026-10-21): 57,409 plus the six batch-49 rows

**This branch's count was 57,892** (2026-10-21), on main's 57,415 merged
with this branch's 57,886, less the rows each side had already counted. The
57,423 was 57,403 plus batch 52's 19 rows and the four jpcorpwp rows that a TLS
chain fault had rejected, less the three `registry.faa.gov` rows — a removal this merge
OVERTURNS, see the browser-profile paragraph below —
and seven CKAN rows come back in this merge that main had removed on a reading
taken before this branch's UA_OVERRIDE entries existed (below). The 57,403 is
the sum of three
batches authored at the same time, two of which had to be renumbered at merge
time. On main's 48,346:

* **main's batch 49, 5,393 measured rows** — batch 45's staging set registered
  (`docs/verified-sources-batch49.tsv`);
* **main's batch 50, 3,247 government and institutional RSS/Atom feeds** for
  Japan, the US, the UK, France and China
  (`collectors/sources/{jp,us,uk,fr,cn,intl}_feeds_batch50.c`,
  `docs/verified-sources-batch50.tsv`, 3,237 OK on the emit sweep, 54,800
  emitted / 54,802 stored), less six `jp_feeds_batch43/44` feeds measured dead
  and 25 rows of batches 44/47/48 whose official domains now publish gambling
  spam (below). It was authored as batch 45 and renumbered TWICE, to 49 and
  then 50;
* **this branch's batch 46 (164 ArcGIS layers) and batch 53 (290)**, batch 53
  being the rest of batch 46's own probed pool
  (`docs/verified-sources-batch53.tsv`). It was generated as 49, renumbered to
  50 when main's PR #55 took 49, and to 51 when main's PR #58 renumbered its
  own 49 to 50 — so BOTH branches moved to the same free number, and the
  add/add on `verified-sources-batch50.tsv` is the collision this rule names,
  hit for the fifth time — and for the SIXTH on this merge, main having
  claimed 51 for `cn_feeds_batch51.c` and its own
  `verified-sources-batch51.tsv`, so the ArcGIS beat was renumbered again,
  to 53, BEFORE merging;
* then a net −5 from a day spent measuring rows rather than adding them: +1 KKJ
  prefecture that had been missing, +5 Setagaya layers recovered from a rename,
  −2 IDESCA San Carlos layers measured as withdrawn, −10 behind a closed
  WordPress REST API with nothing behind it, and −2 from the batch-34/35 emit
  audit: a `data.gov.uk` row that is a PROXY in front of the
  `ckan.publishing.service.gov.uk` row beside it (same 59,370, same `help`
  URL), and an FDSN event row on a node that serves dataselect and not event;
* and +2 for two backfill siblings measured into existence, `PLOS_CORPUS` and
  `ENERGINET_PRODCONSUM_SETTLEMENT_HISTORY`.

**And the UA word filter was found TWICE, independently, on 55 hosts with no
overlap**: this branch bisected six US municipal CKAN portals plus
planelogger.com, main bisected 48 French commune, small US city and Scottish
NHS hosts. Same word in the agent, same remedy, different hosts. Two
independent measurements make it a product people deploy rather than a few
local policies, so bisect the agent before writing off any 403/444/502 row.

**The registered count is 57,415** (2026-10-21): 57,409 plus the six batch-49 rows
the next-day retry pass restored (below). 57,409 was main's 56,955 (below) plus
this branch's 453 on the same base — the 491 rows the User-Agent header
recovered from bot walls, less 19 of them whose feed main's batch 50 reads
(merged first, so its rows stay) and one whose domain the takeover scan below
caught (`bnm.mg`, casino spam under Madagascar's standards bureau — the other
490 were clean), plus `PLOS_CORPUS`, less the 19 rows batches 34 and 35 lost to
their first measurement — then plus one, the ods147 pair that replaced a
misnamed row (the ELIA paragraph under "Cadence policy"). The ledger has both
sessions' days.

**The count was 56,955** (2026-10-21): main's 53,739, less six
`jp_feeds_batch43/44` feeds measured dead, less 25 rows of batches 44/47/48
whose official domains now publish gambling spam (below), plus **batch 50's
3,247 government and institutional RSS/Atom feeds** for Japan, the US, the UK,
France and China (`collectors/sources/{jp,us,uk,fr,cn,intl}_feeds_batch50.c`,
`docs/verified-sources-batch50.tsv`, 3,237 OK on the emit sweep, 54,800 emitted
/ 54,802 stored). Authored as batch 45 and renumbered twice at merge time — to 49, then 50 —
because main's 45, 47, 48 and 49 all landed first. Details: `docs/SOURCE_COUNT_LEDGER.md`.

* **An official domain is not forever, and no record count can see it lapse.**
  Wikidata's `P856` official website — and any register like it — lists the
  domain a body USED to hold. Scanning the ITEM TITLES of 2,729 autodiscovered
  feeds found 27 that serve Vietnamese/Indonesian betting, crypto-casino or
  pharma spam under a city's name (Hudson, Madison, South Houston, Yokohama's
  `kaikou.yokohama`…), and the same scan over main's batches 44/47/48 found 25
  more, including a hacked `pinelandtx.gov` and `ffc.gov.pk`. Every one passed
  probe AND emit with healthy numbers. Page titles are not enough — South
  Houston's homepage title was its own name while its feed was slot spam. Scan
  item titles for gambling vocabulary and for a script the source has no reason
  to publish in, then read the hits: "cialis" is inside "Commercialisti" and
  "casino" inside "Liceo Pascasino". Removals and their evidence:
  `docs/removed-sources-2026-10-20-takeovers.tsv`.
* **Ask the homepage for its feeds.** Probing `/rss/`, `/feed/` on Japanese
  government hosts found 27 feeds; reading each official homepage's own
  `<link rel="alternate" type="application/rss+xml">` found 961 — the batch-42
  lesson again, one level up.

**The registered count was 53,739** (2026-10-20): batch 49's 5,393 measured rows
(`docs/verified-sources-batch49.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_49_JPWPFEDERATIONS.md`) on 48,346 — 5,393
on 2026-10-20 and six restored by the next-day retry pass.
**Batch 49 is batch 45's staging set registered** — the WordPress collections of
the member organisations of Japan's nationwide federations (medical and
pharmacist associations, JA, chambers of commerce, 法人会, co-ops, JC, the 士業
bodies, fire chiefs, welfare councils, public cultural facilities), found by
reading the federations' own link pages for hosts and asking each host for its
REST root; eleven beats named after the federation that listed the host.
5,405 rows were generated and the engine decided: 5,371 OK on the first pass,
28 given a second reading of which 16 recovered, 12 removed
(`docs/rejected-sources-batch49.emit.tsv`); 518,813 emitted / 518,827 stored
over one pass. **The next-day look at the 12 restored six** (749 / 749): three
were transient, two hosts are user-agent bot walls — and `newspromotion.jp`
resets the HTTP/2 stream for **any User-Agent containing the word
`collector`**, which `JO_USER_AGENT` does ("feed collector"), so what the
engine logged as a transport error was a refusal. A transport failure that
reproduces only with one header is a bot wall; `header1=User-Agent: Mozilla/5.0
(compatible; JapanOSINT/1.0)` is the remedy, URL-free because `lint-sources`
reads a URL inside a header as an endpoint. The six that stay out are a
disabled REST API (401 `rest_disabled`), a disabled route (404) and two
collections that hold nothing. Their declared interval is 21600 as data; the cadence that runs
is the system-wide policy (see "Cadence policy" below).

**The registered count was 48,346** (2026-10-19; 48,343 plus the three
embedding maintenance pods of the infrastructure audit): main's 22,408 — its own

**The registered count was 48,346** (2026-10-19; 48,343 plus the three
embedding maintenance pods of the infrastructure audit): main's 22,408 — its own
`jp_feeds_batch41..44.c` beats, PR #48's paging cycle and PR #50's batch 45 of
619 WordPress collections — merged with PR #47's 47,178, less six news
feeds both sessions had added (main merged first, so its rows stay). **Batch numbers 43 and 44 now name two things**: main's
`collectors/sources/jp_feeds_batch4N.c` beats and PR #47's `hp3b4N_*`
tables with `JO4N_` ids — no file or id collides, but read the BEAT, as with
34 and 35. **And PR #47's batches 45 and 46 are 47 and 48**: main merged
its own batch 45 (PR #50, `hp3b45_jpwp*`, `verified-sources-batch45.tsv`) while
that branch waited on CI, a direct file collision, so the public-body beats
(`hp3b47_{itgov,frgov,wdgov}`, `JO47_`) and the press beats
(`hp3b48_{igo,news}`, `JO48_`) were renumbered at merge time — ids, tables,
manifests, TSVs, docs and collector keys — exactly as the batch-41 note below
predicts. The count ledger — every
batch from 37 to 48, main's 45 and batch 46 included, what seeded it, what it measured and what it taught — is
`docs/SOURCE_COUNT_LEDGER.md`; this file keeps the rules and the current number.
Both floors are re-recorded from `make source-count` after every merge, and
the number above is that reading rather than an addition — main reached
57,393/57,394 by its own removals on 2026-10-25 and the 57,536 this line once
stated was two merges stale, which is exactly why it is measured each time.

