# Source count ledger

The registered-source count over time, batch by batch, with what each batch
was seeded from, what it measured and what it taught. Moved out of `CLAUDE.md`
on 2026-10-19 so that file keeps the rules and the current number; the lessons
that a new batch must not relearn are summarised there. Newest first.

**The count is MEASURED ON THE FIFTH MERGE OF MAIN** (2026-10-25, main at
`bd8c9a4f`, 46 commits). 13 conflicts, and the one that mattered was a
deletion: main's HTML reachability sweep removed 23 rows this branch's files
still carried, and **ten of those readings were taken against a URL the row no
longer has**, because the sweep ran in a tree without this branch's B2
re-pointing pass. Resolved per row by comparing each row's registered URL with
the URL main's own TSV records measuring:

| | rows | resolution |
| --- | --- | --- |
| URL differs from what main measured | 13 | KEPT — re-pointed and measured here (ZA gazette, Municipal Money, ČÚZK's keyed API, 4 Stortinget exports, Eduskunta, 2 ECB sheets, 3 bulk-file stand-ins) |
| URL byte-identical to main's reading | 10 | REMOVED — main measured them dead twice, page + bare path + root |

Four of the ten reverse a decision taken on this branch to keep a dead pivot
registered and "disclosing". Both sessions had measured the same FACT — this
branch's own notes say PMDA's search "answers the form page to every POST
constructible here" and the Assemblée's "404s with no form on the homepage" —
so the disagreement was policy, not evidence, and the tree's own precedent
decides it: *"Three sources removed rather than left registered and failing
every run"* (the FAA case). The bulk-file rows that stand in for three of them
survive, so the coverage does not.

Two more conflicts were the same-defect-found-twice shape this ledger keeps
recording. **Numeric keyset cursors**: both sessions fixed `next_path`'s
blindness to a JSON number on the same day, from different APIs — iNaturalist
storing 200 of 385,217,394 here, LegCo storing 1,000 of 542,997 there — with
behaviourally identical code, so the merge keeps main's larger buffer and both
measurements, because two unrelated APIs make it a family rather than a quirk.
**The NRC restructure**: both re-pointed the same three rows to the same URLs
and the same `href_must`, one from the EMITS_NOTHING sweep's byte count and one
from the HTML sweep. Main's note that the two event rows "still store nothing"
is right and now incomplete — this branch's `anchors-unlabelled` notice means
they disclose it instead of failing silently.

`core/collectors.def` conflicted in 23 places and was not hand-resolved: it is
GENERATED, main had split the government collectors into new keys, so it was
regenerated from the binary's own census (`make collectors-def`) — the rule
this file already states, applied rather than re-learned.

**The registered count is 57,395** (2026-10-25, branch `fix6/htmlsweep`): 57,529 less 134 HTML-mode rows measured DEAD by the reachability sweep of every
HP_HTML row (`docs/html-reachability-2026-10.md`): two engine passes each, then
DNS, TCP and the page's bare path checked by hand. 103 answered 404/410 with the
bare path 404 and the root alive, 6 more did once an honest agent got past a
403, 4 are 200 pages titled "404"/"not found", 14 hosts no longer resolve, 2
redirect to a 404 page, 1 to a bare IP that refuses :443, and 4 are 404 past
a certificate chain OpenSSL rejects. 51 come from the unprobed `hp2_*` tables,
16 from `hp_*`, 66 from batch 31 (registered, not proven) and 1 from batch 28. Ledger: `docs/removed-sources-2026-10-25-html-dead.tsv`.
Both floors re-recorded.


**The count stays 58,055 across main's merge of 2026-10-24** (main at
`2c846ac3`, seven commits), and the one conflict is worth recording because of
how it resolved. Main MOVED three San Francisco Socrata rows —
`nam-sf-311-cases`, `nam-sf-building-permits`, `nam-sf-fire-calls` — out of
`collectors/feed/generated/vsrc13_us_local_2.c` onto hpengine
(`collectors/pivot/table/hp3_sfopendata.c`) so each is keyset-paged and read
whole across runs, leaving a tombstone comment where the VJSON row had been.
This branch had, the same week, given those same three VJSON rows measured
`id_keys` off the registry health sweep. Same ids either way, so the count does
not move and `lint-sources` reports no duplicate.

**Main's move wins and this branch's measurement corroborates it**, which is
the useful part: the two sessions reached `service_request_id`, `record_id` and
`rowid` from different directions — main from the datasets' own documented row
ids while re-pointing them onto hpengine, this branch from a collision verdict
measured over two pages (73, 60 and 2 of 10,000 collapsing, then 1,000 of 1,000
distinct). Note what makes that corroboration rather than the trap CLAUDE.md
records for batch 55, where "three keys agreeing on the same wrong answer" was
not evidence: there the three keys were read off the SAME response, here the
two readings are independent methods on independent runs. The tombstones carry
this branch's numbers so the evidence is not lost with the rows.

**The registered count is 58,054** (2026-10-23): 58,047 plus **batch 55's seven
rows** — the Austrian Parliament's whole materials index, 391,357 records read
as seven complete slices (`collectors/pivot/table/hp3b55_atparl.c`,
`docs/verified-sources-batch55.tsv`,
`docs/at-parlament-filter-api-2026-10-23.md`). It closes the last line of open
item B2, and the answer to "one measured sort parameter away" turned out to be
that **there is no sort parameter** — the endpoint ignores its query string
entirely, the filter is a POST body, and 14 of 26 sort spellings "worked"
because the index itself reorders between reads. `frobnicate=7` moving the
answer identically is what settled it.

What ships is a PARTITION rather than a walk: `showAll=true` plus a `GP`
(legislative-period) filter answers `pages: 1` with `rows == count`,
byte-identical across two reads, and the 27 non-empty GP codes' counts sum to
391,354 against an unfiltered 391,354 exactly. Seven rows because each must
stay under the filter API's 100,000-row single-page cap and
`core/httpclient.c`'s 64 MB body ceiling, with no group mixing a closed period
with the open one. Engine readings, `stored == emitted` on all seven and no
notice anywhere: 50,896 / 77,646 / 55,695 / 51,970 / 51,979 / 76,457 / 26,718,
one request each, 222 MB a cycle.

Three lessons, all in CLAUDE.md: **probe a control before believing a
parameter**; **enumerate the dimension you slice on and check the slices sum to
the whole** (the Roman numerals reach 389,139 of 391,354, and a 30-page sample
found `KN` and `PN` but missed `BR`'s two rows); and **three keys agreeing on
the same wrong answer is not corroboration** — `WENTRY_ID`, `HIS_URL` and
`GP_CODE+ITYP+INR` all give 75,485 of 76,457, and all 972 "duplicates" differ,
in `NRBR`, because a material that went through both chambers is served once
per chamber under one web-item id.

**The registered count was 58,047** (2026-10-23): main's 57,529 merged with this
branch's 58,045. Third merge of main in one day, and the one where a conflict
was decided by a measurement only the OTHER side had.

Both branches had rewritten ENA's three rows after finding independently that
`www.ebi.ac.uk` refuses `offset` outright. This branch gave them `window_days`,
a forward cycle that reads the archive; main gave them a 30-day tail and then —
for tuberculosis — **four static era rows**, because main measured something
this branch never saw: one `limit=0` read of all 360,690 MTB runs answers
**HTTP 200 with 331,580 rows, 8% short in every date half**. A silent cut of an
accession-ordered stream, invisible in the response, and the four era blocks
each answer exactly their own count (76,189 / 95,032 / 85,283 / 57,476 /
46,710 = 360,690).

So MTB takes main's five-row set whole, and this is not deference: keeping both
designs would have stored every MTB run TWICE under two source ids, which no
record count shows as wrong because a second copy looks exactly like a second
source. `window_days` stays on SARS-CoV-2, which has no era rows and whose 7.5
million runs a 30-day tail never reaches — its windows are ~15k records, far
under where that cut appears, and each measured equal to the endpoint's own
`/count`. Main's `array_path = "."` is taken for the three JSON rows as well:
ENA answers a bare root array, which must be declared, and these rows had been
working only because their records are flat. It was NOT put on the four TSV
rows, where it means nothing — the scripted edit that added it to all seven was
caught by checking each row's mode.

**The count is still 58,045 after a SECOND merge of main the same day**
(2026-10-22): main merged PR #68 while the first merge was being resolved, and
it adds no rows — it re-measures existing ones. Four conflicts, three of them
the same situation as the first round, and this time the two branches had
reached *opposite* answers on the same endpoints:

* **The three ENA rows.** Both branches measured that `www.ebi.ac.uk` refuses
  `offset` outright. Main then gave them a rolling 30-day `first_public`
  window; this branch gave them `window_days`, a forward cycle from
  2020-01-01 and 2010-01-01. **This branch's rows are kept, and main's own
  comment is the argument**: it records that the 7,556,500 runs already in the
  archive are NOT backfilled by a rolling window, and that date tokens cannot
  express the backfill because they step back on failure rather than iterate.
  A row that reads only the tail drops the archive by design, which rule 2
  does not allow when the engine can iterate. Two of main's findings are
  ported in: `timeout_ms = 120000` on all three (the SARS-CoV-2 query measured
  **38.6 s cold** against the 20 s default, so main's first fixed run timed out
  at `bytes=0` and filed a fetch-failure for a query that works), and
  `first_public` added to the influenza row's FIELD list — a field absent from
  the list is absent from the stored record, so `date_keys` had nothing to
  order on. That row stays unwindowed in both readings, because 1,324 of its
  61,537 samples carry no `first_public` at all.
* **`BC_MINERAL_TENURE`.** Both added `sortBy=TENURE_NUMBER_ID`. **Main's row
  is kept wholesale**: it measured `count=10000` honoured at 15.2 MB / 1.8 s a
  page and `numberMatched` 42,327, so five pages read the cadastre whole where
  this branch's `count=50` cycled 1,000 a run for 42 runs — and main's row also
  carries the `id_keys`, `title_keys` and `date_keys` this branch's lacked.
  This branch's note on NOT applying the sort to the tree's other 17
  sortBy-less WFS rows is appended, because that is the half that generalises.
* **`hpengine_test.c`** — both fixture sets again; main's `40f` ($JO_PAGE_MAX
  bounding a row that declares its own `page_max`) sits beside this branch's
  41–51d.

And main's sweep **refuted an open item of this branch's**: E6 said Slovenia's
`wfs-si-gurs-kn` had been withdrawn, on one 404 to GetCapabilities. Re-measured
here, the same URL answers 200 with 118,420 bytes and both rows store — 2,698
and 580,317 features. Auditing the row that was believed dead then found
`SI_GURS_HOUSE_NUMBERS` keyed on `properties.HS_MID`, **a field that layer does
not have**; the engine's first-non-empty-scalar fallback happened to land on
`FEATUREID`, the right answer by luck, and would have silently re-keyed 580,000
addresses had the layer's field order changed. Named explicitly, with
`sortBy=FEATUREID`. A row cannot be audited while it is believed to be dead.

**This branch's count was 58,045** (2026-10-22): main's 57,527 merged with this
branch's 58,055, on the **57,536** base they share. Measured with `make
source-count` and cross-checked against the binary's own `--list-sources`
(58,045 both ways, 0 duplicates / 0 dropped / 0 never-runs), never added up —
and the arithmetic then agrees to the row: 57,536 + 519 − 9 − 1 = 58,045, the
519 this branch's day of B2 and batch-54 work, the 9 main's `&amp;` duplicates
below, and the 1 `JO34_CKANRES_CKAN_PUBLISHING_SERVICE_GOV_UK`.

Nine conflicts, and four of them were the same situation: **both branches had
found the same defect on the same day and fixed it differently.** Each was
settled by asking which reading the tree's own documents support, not which
diff was larger.

| conflict | both sides | taken |
| --- | --- | --- |
| `hp3b34_fdsnevent.c` | both re-added `JO34_FDSNEVT_EIDA_GEIN_NOA_GR` after measuring that the node needs a time window | main's **30-day** window over this branch's 2-day one. The narrow window is safe — a quiet window answers 404 with FDSN's own "no matching events found", which `hp_status_is_declared_empty` reads as an honest empty — but it is one day of slack against a daily interval, so one missed run loses events permanently |
| `audit_source_exhaustiveness.py` `_WALKER_CALL` | both found that `pw_walk_max` / `od_walk_rows_max` / `od_ckan_walk` / `od_ods_catalog_walk` never reached the walker list — 49 findings each | this branch's `\w*` suffixes over main's exact `(?:_max)?`. The lesson of the bug is that a new walker arrives without reaching the tool; a pattern that already matches the next suffix is the fix that survives it |
| the `source_def.url` false positive | both excused it; **this branch outright, main only when the `.url` restates a URL handed to a walker** | this branch's categorical skip. `source.h` calls the field "canonical/base or internal:// sentinel", rule 3 says nothing at that layer can see an entity token, and no code fetches it for records (its one reader, `core/pagination_probe.c`, probes it for a page parameter). Main's narrower rule still flagged a `.url` whose collector pages in a `while` loop of its own — `od_udata_collect` is exactly that shape |
| `htmlparse.c` `int cap = 0` | both marked it `exhaustive-ok` | this branch's wording, which names `hardcap` as the real ceiling |
| `hp3b34_ckanbulk.c` | this branch added 7 rows; main measured 17 rows' `result.count` and raised their `page_max`, and removed the gov.uk backend | **both**: every row main measured carries main's ceiling, the 7 new rows keep `page_max = 20` because nobody has measured them, and the backend row is gone |
| `httpclient.c` | this branch added 7 UA_OVERRIDE hosts; main **replaced the whole mechanism** with `core/host_agents.def` | main's refactor, with this branch's 7 hosts and their bisections ported into the def. One table that the engine and the probe both read beats two tables and 471 per-row headers, and the 7 entries are worth more inside it |
| `CLAUDE.md`, the ledger, both floors | each side prepended its own day | both days, each labelled with the branch that measured it; the floors re-recorded from the merged binary |

**This branch's count was 58,055** (2026-10-22): 58,052 plus the three rows
that close open item B2's long tail, each a publisher's own bulk file standing
in for a search that is gone, and two of them needing an engine affordance
before they could ship at all:

| row | measured |
| --- | --- |
| `JP_PMDA_NEW_DRUG_APPROVALS` | 95 / 95 in 2 requests — the annual drug-approval workbook, whose sheets are MONTHS and grow by one every month, read whole on the new `xlsx_sheet_all`. The sum of the four sheets' data rows (11 + 29 + 23 + 32) exactly |
| `FR_AN_QUESTIONS_ECRITES` | 19,165 / 19,165 in ONE request, 50,197,713 bytes, 126 s — every written question of the 17th legislature, published as 19,165 JSON documents inside one ZIP, read on the new `zip_all_entries`. 0 notices, 3 records undated because the Journal officiel has not published them yet |
| `JP_KANPO_DAILY_CONTENTS` | 25 / 25 in 1 request — the official gazette's daily table of contents, free for 90 days where the SEARCH went behind a paid member login. The day's 43 anchors become 25 records because several notices share one gazette page; 9 of the 25 carry the others as `other_labels` |

Three existing rows stay registered and failing honestly, which is the standing
decision rather than an oversight: PMDA's search answers the FORM PAGE to every
POST constructible from here (session cookie, referer, the image button's x/y,
the column checkboxes), the Assemblée's `/dyn/recherche` 404s for a real name
and an impossible one alike with no search form on its homepage, and the
gazette's search redirects to `/kanpou/auth/login/LoginStartUp.form` — a member
account, not an API key, so no `key_env` opens it. Each discloses itself once
per run and the source pages read the reason back.

Also measured and NOT re-pointed, with the reason recorded rather than a guess
shipped: `JP_MHLW_LABOUR_VIOLATIONS` (the ministry's own site search is a
Google Custom Search — Google's index of the ministry, not its register — and
the labour-standards section of its own sitemap lists no disclosure page) and
`WORLDBANK_CONTRACT_AWARDS` (its Socrata API answers 404 outright now, the
portal has moved to financesone.worldbank.org, and the Bank's data-catalogue
API answered 429 to both attempts from here — unmeasured, not dead).

**The registered count was 58,052** (2026-10-22): 58,019 plus batch 54's 32
rows and `FR_AN_DEPUTY_STAFF` (2,077 of 2,077 French parliamentary staff,
measured). Batch 54 is the part of batch 45's staging set that batch 49 left
behind, recovered by fixing two over-broad alternatives in
`tools/wp_routes_sweep.py`'s MACHINERY regex — and the headline finding is that
**the staging set was already registered**, 5,392 of 5,643 endpoints, which is
batch 49's own count. The note is
`native/collectors/OSINT_SOURCES_BATCH_54_JPWPLEFTOVERS.md`; the readings are
`docs/probe-sources-batch54.tsv`, `docs/emit-sources-batch54-2026-10-22.tsv`
(35 of 35 OK, 4,454 records) and `docs/rejected-sources-batch54.tsv`.

**The registered count was 58,019** (2026-10-21): 58,016 plus **3 rows from open
item B2's re-pointing pass** — `EU_ECB_SUPERVISED_ENTITIES_LSI` (the LSIs sheet
of the ECB's monthly workbook, 2,175 banks that `HP_XLSX`'s one-sheet read
would otherwise have dropped at the seam) and two Stortinget exports the tree
was not reading, `NO_STORTINGET_INTERPELLATIONS` and
`NO_STORTINGET_QUESTION_TIME`. Five existing rows were re-pointed rather than
added, three of them keeping their kind and two becoming scheduled rows;
measurements in `docs/b2-repoint-emit-2026-10-21.tsv`, corrections in
`docs/b2-path-stale-triage-2026-10-21.md`.

The three new rows came from asking the INVERSE question. The triage had named
`/eksport/saker` as the replacement for a dead Stortinget pivot; `make
lint-sources` answered `dup-endpoint`, because a generated feed row has read
that exact URL every six hours all along. Asking instead which of a
publisher's exports the tree does NOT read found three live Stortinget
endpoints missing — hearings, interpellations and question time — and all
three are registered now.

**The registered count was 58,016** (2026-10-21): main's 57,536 merged with
this branch's 57,886, on the 57,415 base they share. Measured with `make
source-count`, not computed, and then cross-checked: 57,415 + 477 + 121 =
58,013, **+2** for the two FAA rows restored and **+1** because both sides had
subtracted `FAA_NNUMBER_REGISTRY`, so the sum had removed it twice. A merge of
two branches that both removed rows double-subtracts every row they both took. Both days' entries are kept below under the branch that
measured them.

Renumbering, for the sixth time: main claimed 51 for `cn_feeds_batch51.c` and
its own `docs/verified-sources-batch51.tsv`, an add/add on that file, so this
branch's ArcGIS beat moved 51 → **53** (290 ids, the table symbol, the
collector key, the manifest, the TSV, the batch note and both page-two
measurement records) BEFORE the merge rather than after — which is again what
kept the merge to one round.

**Main's count was 57,529** (2026-10-22): main's 57,527 (its
`&amp;` pass, below) plus this branch's +2 on the shared 57,536 base: +4
tuberculosis windows, +1 Elia imbalance-price row, −3 ODS rows whose datasets
no longer exist. No id is in both sides' changes; both floors re-recorded.

**This branch's count was 57,538** (2026-10-22, later): 57,540 − 3 + 1. The
ods147 lesson applied to every OpenDataSoft row
(`docs/measured-ods-titles-2026-10-22.tsv`): the metadata endpoint of all
237 datasets the tree's 245 ODS rows read was asked for its title; 118 rows
got one, 124 never got an answer (the platform resets this environment's
path on nearly every try — a reading of the path), 3 datasets no longer
exist (`NotFoundResource`; the three `vsrc13_europe_data` rows are gone),
one row was named for a different measure than its dataset holds (Qatar's
"paramedic workforce" is paramedics' PATIENTS by gender; renamed), and
`ELIA_SYSTEM_IMBALANCE` — re-pointed only two days earlier — had been
pointed at the imbalance-PRICE dataset: the one-minute successor of ods045
is ods126 by Elia's own titles (3,622 of 3,622 in its window through the
engine), and ods134 is its own row now, `ELIA_IMBALANCE_PRICES_QH` (241 of
241). Every other low-overlap pair was a language or wording difference,
read by hand.

**This branch's count was 57,540** (2026-10-22): 57,536 (below) plus
four. `tools/audit_cycle_advance.py` ran all 669 `page_resume` rows under
`JO_PAGE_MAX=2` and read their notices back
(`docs/measured-cycle-advance-2026-10-22.tsv`): 600 advance, 20 complete
inside two pages, 43 were refused page 1 from this environment (39 of them
twice), 2 answered 429 to page 2 (Energinet, Tronscan — per-host gaps in
`core/hostgate.c`, whose live-entry count is sizeof arithmetic now rather
than the `7` that had been hiding the two new gaps), and **4 could never
advance**: `BC_MINERAL_TENURE` (GeoServer wants `sortBy`; added) and the
three ENA Portal API rows, whose `offset` the API refuses outright. ENA's
complete read is `limit=0` in one request; influenza samples (61,625) and a
30-day SARS-CoV-2 tail fit in one, and the tuberculosis runs became FIVE
single-request windows on `first_public` because one `limit=0` read of the
whole 360,690 came back 8% short in every date half (a silent cut), while
each window answers exactly its count. +4 rows; both floors re-recorded.

**Main's count was 57,527** (2026-10-22): 57,536 less nine rows that
became exact duplicates when the `&amp;` in 1,076 row URLs was decoded
(`docs/removed-sources-2026-10-22-amp-duplicates.tsv`): four batch-43 municipal
feeds batch 25 already reads, four batch-44 US feeds batch 50 reads, one CNRS
job feed. Each of the 1,076 was measured twice through the engine, URL as
registered and URL decoded (`docs/amp-decode-2026-10-22.tsv`): 698 store
as much or more decoded (7,009 records as registered, 9,153 decoded), 16 store
only once decoded, 147 failed both ways, and 206 were worse decoded and keep their `&amp;` with a marker
saying why: batch 48's newspaper search feeds, where the decoded filter
applies and finds nothing (the mangled URL fell back to the site's headlines),
Joomla `type=atom` rows that then really serve Atom against a row declared RSS,
and municipal CMS feeds that then honour a smaller `displayCount`. The 147 that
failed both ways keep the decoded URL. Also on this day:
`jp-b43-tokyogirlsupdate-com` moved onto hpengine (a 2 MB feed at 23 KB/s,
84 s against the RSS path's 25 s: 0 → 150 stored) and `JO48_IGO_Q1371744`
was reclassified from IGO to Japanese web media (Banpresto Spain's figure
news). Same count; same ids.

**Main's count was 57,536** (2026-10-21): main's 57,415 + this branch's
121 (+136 batch 51, −29 takeovers, +15 browser-profile feeds, −1 duplicate FAA
row), both on 56,955. The merge kept main's `adaptive_interval` on every row
and this branch's removal of the 29 taken-over rows, whose ids main had
edited; no removed id is back in any table or manifest.

**This branch's count was 57,076** (2026-10-21): 57,062 + 15 − 1.

* **+15**: the batch-50 feeds that refused every honest agent, restored with
  the per-host browser profile (`BROWSER_HOSTS`, `core/httpclient.c`): five
  NOAA offices, BLS Latest Numbers and CPI, NASA JPL news (100 items), State's
  Office of Fine Arts, San Diego County Water Authority, the UK Government
  Cyber Coordination Centre, the Intelligence and Security Committee, NHS 24,
  METI Hokkaido's Atom feed and the Japan Writers' Association. All 15 OK
  through the binary.
* **−1**: `FAA_NNUMBER_REGISTRY` (`hp_americas_deep.c`), a duplicate of
  `FAA_REGISTRY` that read the N-number page as anchors.
* Two existing pivots recovered by the same host entry: `FAA_REGISTRY` (one
  31-field record per N-number) and `US_FAA_AIRCRAFT_REGISTRY` (re-pointed at
  the GET form of the owner search; 1,729 emitted / 1,729 stored for one
  operator). `sec-reliefweb-headlines` re-pointed at its final https URL: 20.

**The registered count was 57,062** (2026-10-21): 56,955 + 136 (batch 51) − 29 (takeovers).

* **+136 — batch 51**, Hong Kong and Macao government feeds plus China News
  Service (`collectors/sources/cn_feeds_batch51.c`): news.gov.hk by category,
  HKMA (20), the Centre for Health Protection (12), RTHK, Census and Statistics,
  OFCA, the Marine Department, EPD and the AQHI, Macao's transport, labour,
  statistics, meteorological and cultural bureaux, and chinanews.com.cn (23).
  Read off the publishers' own feed directories — GovHK's RSS list, the HKMA's,
  China News Service's `/rss/`, and the page each of 306 HK/Macao government
  homepages (Wikidata P856) links as "RSS". Mainland ministries publish on
  WeChat rather than RSS, which is why the mainland share is one news agency.
  Emit: 135 OK + 1 collision that is the upstream's own 16 byte-identical
  repeats (Macao's events feed lists one event under several dates); 5,051
  emitted / 5,037 stored. LegCo's 39 feeds refuse the connection from here.
* **−29 — a sweep of every feed in the tree for domain takeovers**
  (2026-10-21): the item titles of 42,593 distinct feed and WordPress-collection
  URLs were read and 52 flagged; each flag was read by hand. 29 were taken over
  or injected and are removed with their evidence in
  `docs/removed-sources-2026-10-21-takeovers.tsv` — Ghana's national CERT
  publishing casino posts, Visit The Gambia, 11 French commune and Mission
  Locale sites (three of them COMMENT feeds full of pharma spam), five Italian
  public bodies, a Miyawaka chamber of commerce in main's batch 49, the Belize
  Reporter now a Vietnamese lottery site, `japanesecreations` from batch 42,
  and two of batch 50's own rows found after shipping (`pcsa.jp`, City of
  Hazard). 23 were kept: Vietnamese papers reporting lottery results, Indonesian
  news on gambling raids, the Horserace Betting Levy Board, Korean and Japanese
  bodies publishing in Vietnamese for migrant residents, Serbian and Bosnian
  `đ`, and four real outlets carrying a sponsored post (The Coast, INFO.FR,
  Asian Journal, The Village Voice) — a sponsored post is the publisher's own,
  a takeover is not.
* Batch 50's retry pass: 15 of the 16 hosts that refused every honest agent
  still do; Camden County, NJ refuses only the contact URL and is back with the
  PIB-style agent; Edinburg ND is another "collector" block; Ballwin's feed is
  still empty and left (`EMPTY_RESULTSET`). Batch 50 now reads 3,243 OK + 2
  byte-identical collisions.

**The registered count was 56,955** (2026-10-21): main's 53,739 (its batch 49, PR #55) − 6 − 25 + 3,247 (batch 50).

**Main's count was 57,409** (2026-10-21, later the same day): 57,408

**This branch's count was 57,886** (2026-10-21, after merging main's
57,409): this branch's 57,423 — 57,403 + 19 (batch 52) + 4 (jpcorpwp recovered)
− 3 (registry.faa.gov) — composed with main's day, plus the seven CKAN rows the
merge restored (below).

* **+19 — batch 52**, and **+4 into batch 41's `jpcorpwp` table**, all 23 of
  them rows that nine servers had made unreachable by serving a leaf without
  the public intermediate that signed it. The chain fault, the bundle that
  fixes it and the proof it cannot widen trust are
  `docs/tls-intermediates-2026-10-21.md`; the rows are
  `native/collectors/OSINT_SOURCES_BATCH_52_TLSRECOVERED.md`. 24 candidates
  re-probed 24 of 24 PASS; **2 were still not registered** — a live WordPress
  whose entire collection is the installer's "Hello world!" post, and a
  keyword search superseded by the collection it is a subset of
  (`docs/rejected-sources-batch52.tsv`). One REGISTERED row,
  `NI_MARENA_PUBLICACIONES`, came back for free on an intermediate the bundle
  already carried.
* **−3 — `registry.faa.gov`, OVERTURNED by the next merge.**
  `US_FAA_AIRCRAFT_REGISTRY`, `FAA_NNUMBER_REGISTRY` and `FAA_REGISTRY` were
  removed because the host answers 403 to any agent carrying a parenthetical
  contact route and 200 to a bare name or none at all, so recovering them
  looked like it meant dropping our contact route. Main measured the same host
  and added an explicit per-host browser profile instead (`BROWSER_HOSTS`),
  which this container confirms: 200 and 52 data rows where the engine agent
  gets 403. Two of the three are back (net −1, main's own reading); the third
  stays out because once the 403 lifted it stored navigation links as
  aircraft.

* **+7 restored in the merge with main.** Main removed 14 batch-34 CKAN rows
  that had failed twice, and SIX of them are the very hosts this branch
  bisected for the User-Agent word filter. Measured 2026-10-21, package_search
  on each: HTTP 502 to the engine's own agent and **200 to the UA_OVERRIDE
  agent**, with `result.count` 32,090 (data.virginia.gov), 963
  (opendata.hawaii.gov), 397 (data.ok.gov), 186 (data.milwaukee.gov), 166
  (data.sanantonio.gov) and 96 (data.houstontx.gov). Main measured them before
  that table entry existed, so the removals are superseded — the fix and the
  rows ship together. `dati.comune.matera.it` is the seventh and a different
  case: 200 to BOTH agents with real CKAN JSON (`count` 249), so it was
  checked separately rather than assumed. Main's other seven removals STAND,
  each re-measured: catalog.data.gov 404, data.go.th 403,
  datosabiertos.gob.pe 418, three transport failures, and
  catalogue.data.govt.nz answering 200 with Imperva's "Pardon Our
  Interruption" — the bot wall no agent in the table reaches.
* and the NOA FDSN event row main removed is **restored with its measured
  window**: that node requires a time range, and with `starttime` two days
  back it answers 200 with 232 events in 36 KB (re-measured 2026-10-21).

**The registered count is 57,409** (2026-10-21, later the same day): 57,408
plus one. `ELIA_RENEWABLE_INTRADAY_FORECAST` (batch 19, ods147) was a guess at
its dataset's content: ods147's own metadata — the first answer
opendata.elia.be gave in two days — titles it "System imbalance forecast next
quarter hour (near real-time)", one record a minute since 2023-10-06,
1,576,655 of them, time field `predictiontimeutc`. The row is replaced by the
deliberate pair `ELIA_SI_FORECAST_NEXT_QH` (last six hours, newest first,
every 15 minutes) and `ELIA_SI_FORECAST_NEXT_QH_HISTORY` (the whole series on
a keyset cursor, `next_path=results.$last.predictiontimeutc`, 100 pages an
hour over a `page_resume` cycle, because the records endpoint caps `offset`
at 10,000). Both floors re-recorded at 57,409.

**The count was 57,408** (2026-10-21): main's 56,955 (its batch 50, the
six dead feeds and the 25 takeovers, below) plus this branch's 453 — the 491
rows recovered from bot walls, less 19 whose feed batch 50 already reads and
one the same takeover scan caught (`bnm.mg`, casino spam; the other 490 were
clean), plus `PLOS_CORPUS`, less 19 rows batches 34 and 35 lost to their
first measurement — composed in one merge; the cadence policy and the adaptive
interval compose in `sched_row_interval()`.

**The count was 56,955 on main** (2026-10-21): main's 53,739 (its batch 49, PR #55) − 6 − 25 + 3,247 (batch 50).

* **−6**: `jp_feeds_batch43/44` feeds measured dead on the 2026-10-20 emit
  sweep (game-i.daa.jp serves XHTML, peco-japan.com an S3 AccessDenied,
  buddhanet.net HTML, tecture.jp an empty body, kotaku.com 403, kinmaweb.jp an
  HTTP/2 framing error on every attempt); suigenkyo moved from its comments
  feed to its blog feed. The merge with main had reintroduced all six.
* **−25**: batch 44/47/48 rows whose feeds now carry gambling spam under an
  official or press name — lapsed domains re-registered by betting sites, and
  official WordPress sites with injected casino posts.
  `docs/removed-sources-2026-10-20-takeovers.tsv` keeps the item titles that
  convicted each one; manifests are trimmed to match.
* **+3,247 — batch 50**, government and institutional RSS/Atom: Japan 883,
  US 915, UK 1,111, France 261, China/HK 39, international 38. Two seeds.
  Research agents proposed 1,744 feeds, 1,371 of them live (gov.uk's
  per-organisation `.atom` is most of the UK). Then **autodiscovery**: ~20,000
  official homepages — Wikidata `P856` for Japanese municipalities and
  agencies, US states/cities/agencies/universities, UK public bodies, French
  regions/départements/communes over 15,000 people and Chinese government
  bodies, plus every gov/academic host already in the tree — were fetched and
  their own `<link rel="alternate">` declarations read: 3,641 declared feeds,
  2,729 live. 802 were already registered after merging main (its batch 44 and
  47 seed from the same kind of list) and are not duplicated. Rejected rows,
  each with a reason, are `docs/rejected-sources-batch50.tsv`: 25 domain
  takeovers (27 found; the other two were already main's rows and are in the
  removal file), 16 hosts that refuse every honest agent, 5 comment feeds, 4
  misattributions, 1 ccTLD contradiction. Emit sweep: 3,237 OK, 2 collisions
  that are byte-identical upstream repeats, 8 transient or currently empty.
  Closed gov.uk bodies (`govuk_status=closed` in gov.uk's organisations API)
  poll weekly, not hourly.
* Two engine fixes came out of it: `lib/rss_atom.c` waits 25 s instead of 8
  (ipu-japan.ac.jp's 4.6 MB feed went from 0 to 500 records), and 48 more hosts
  get the no-"collector" agent in `core/httpclient.c` (86 rows recovered).
* Batch number 50, not 45: main merged its own 45, 47, 48 and then 49 (PR #55)
  while this one was open — it was renumbered at two merges. 46 is absent from main and was left alone in case an open branch holds
  it. Batches 43 and 44 now also name main's `JO43_` municipal and `JO44_` US
  government tables; the `jp_feeds` measurements are
  `verified-sources-batch4{3,4}.jp_feeds.tsv`.

**The registered count was 48,343** (2026-10-19): the 47,178 below merged with

## This branch, merged 2026-10-21 (batches 46 and 51, and a day of measuring)

**The registered count is 48,794** (2026-10-20): the 48,507 below, plus batch
49's **290 ArcGIS feature layers** — the remainder of batch 46's own probed
pool, through the same filters and the same live probe
(`native/collectors/OSINT_SOURCES_BATCH_53_ARCGISLAYERS.md`,
`docs/verified-sources-batch53.tsv`) — and then a net −3 from a day of
measuring rows rather than adding them:

| change | rows | why |
| --- | --- | --- |
| batch 53 | +290 | batch 46's pool finished; its 164 were a batch-size cap, not a quality one |
| KKJ 三重県 | +1 | the set covered 46 of Japan's 47 prefectures and nobody had counted them |
| Setagaya street fire extinguishers | +5 | one dead row, looking for the name we expected, became five ward layers (4,951 records) plus the ward's earthquake-well register (1,148) |
| IDESCA San Carlos | −2 | `patentes` and `licores` withdrawn — 38 feature types and 0 catalogue hits for `patente`, `licores` AND `comercial`, so absent rather than renamed |
| jic-bestork.co.jp | −10 | the whole WordPress REST API behind `rest_login_required`, and its `/feed/` holds one default "Hello world!" item, so there is nothing to re-point at |

The day's larger result is not in the count at all: **654,412 KKJ procurement
notices became reachable** where 46,000 were being re-read forever, 47 rows
moved onto a measured date-window cycle after twelve probes established that
the portal honours no offset parameter under any name. Plus CV INE 50 → 2,425,
MET Malaysia 100 → 3,080 and 100 → 902, and EU Parliament 3,717 → 3,972.

Three tools were also found measuring their own limits and reporting them as
findings — the page-2 sweep's saturated egress tunnel (569 false dead
endpoints), Python's default TLS context (20 live rows called dead), and a
rebuild during an emit audit (229 of 290 rows as `NO_RUN_LINE`). All three are
now impossible rather than remembered; see CLAUDE.md.

**The registered count is 48,507** (2026-10-19): the 48,343 below plus **batch
46's 164 measured ArcGIS feature layers**
(`docs/verified-sources-batch46.tsv`, `docs/emit-sources-batch46.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_46_ARCGISLAYERS.md`).

**Batch 46 is batch 42's method on a THIRD family.** The tree already reached
195 ArcGIS service roots, each by ONE row naming ONE layer of ONE service — the
path somebody happened to know. A root publishes its whole directory at
`?f=json`: its folders, its services, and for each FeatureServer or MapServer
the layers and tables it holds. 195 roots → 899 candidate layers (a
self-imposed 5-per-root cap, not a limit the hosts set; one root alone published
899 services) → 819 probed PASS → 456 after dedupe of the `arcgis`/`ArcGIS` and
MapServer/FeatureServer spellings, a 50-record floor and dropping cartography →
164 selected at 2 per root. Measured: probe 164/164 PASS,
`audit_registry_emit.py` 163 OK / 1 SLOW, **1,832,156 records emitted and not
one row storing fewer than it emits**.

**It was authored as batch 45 and renumbered to 46 at merge time**, the third
session in this ledger to be renumbered by the same rule: 44 was the highest
number when it generated, and by merge time main had taken 45 (PR #50) and
renumbered its own 45/46 to 47/48, leaving only 46 free.
`docs/verified-sources-batch45.tsv` was a direct collision. Renumbering BEFORE
merging main kept it to one pass — the merge then had no both-added file.

Also in that branch, outside the batch: **`window_days`**, a date-window cycle
for a collection whose only slicing axis is a date
(`ENA_PATHOGEN_SARSCOV2_RUNS` read 1,000 of 7,556,500 for ever because
ebi.ac.uk honours no `offset` at all); **a sweep of all 530 cycling rows**
(`docs/cycle-health-2026-10-19.tsv`, `tools/audit_cycle_health.py`) which found
exactly one row stuck for a reason that was a property of its endpoint
(`BC_MINERAL_TENURE`, 50 of 42,234, GeoServer refusing `startIndex` without a
sort); and **`CR_SANCARLOS_PREDIOS` recovered** from a month documented dead —
its cadastre is published under `geonode:Catastro_sc`, which no `predio*` search
could match, and it now stores 20,000 records a run of 99,222.

**The registered count is 48,343** (2026-10-19): the 47,178 below merged with
**The count was 54,212 on this branch** (2026-10-21, before main's batch 50 landed): two sessions' 2026-10-20 work
merged. Main's 53,739 — batch 49's 5,393 federation-member WordPress
collections (`docs/verified-sources-batch49.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_49_JPWPFEDERATIONS.md`; 518,813 emitted
/ 518,827 stored, 12 rows removed on two readings) on 48,346, with the
system-wide cadence policy (PR #52) and the osint collectors split by category
(PR #53) — plus this branch's 473 below on the same base. The merge composed
the cadence policy with the adaptive interval (`sched_row_interval()`, see
CLAUDE.md's "Cadence policy") and found one defect in doing so: the adaptive
measurement could give an entity pivot a schedule; it cannot now.

**The count was 48,819** (2026-10-20): 48,346 (the 48,343 below plus
the infrastructure audit's three embedding maintenance pods, PR #42) **plus 491
rows recovered from bot walls, plus `PLOS_CORPUS`, less 19 rows batches 34 and
35 lost to their first measurement.** No new batch: a day of measuring what
was already registered.

* **491 of the rows batches 44–48 had dropped as 403s are back** — 86 UK, 328
  French, 58 Italian, 8 Wikidata, 4 IGO, 7 press. The 523 rows that had
  answered 403 to the engine's client were re-probed with a declared
  `header1=User-Agent: Mozilla/5.0 (compatible\; JapanOSINT/1.0\; +repo)`:
  495 answered the probe and 491 store their feed in the engine (11,159
  emitted, 11,159 stored); four Italian school sites refused twice. The walls
  key on the default User-Agent, not on the reader; the header is on each
  row, so the condition a row was proven under is the one it runs under.
* **Batches 34 and 35, authored unfetched on 2026-10-04, measured**: 139 → 122
  and 100 → 98. Zero `FILTER_IGNORED` across their 122 pivots; three FDSN event
  rows on nodes without that service and 14 CKAN portals refusing twice left;
  two WEKO repositories answering 5xx twice left. The 81 CKAN bulk rows moved
  from `current_package_list_with_resources` (no sort — three collisions that
  were skips) to `package_search?sort=id+asc` with a cycle, re-measured.
* **`PLOS_CORPUS`**: the ascending twin `PLOS_RECENT`'s own note asked for,
  418,125 articles read over a daily cycle (2,000 a run, resumes at
  `start=2000`, `start=4000` — measured).
* Not a count change, but the day's other measurements: the four feed batches
  (25,920 rows) re-measure their own cadence after every run
  (`adaptive_interval`); `count`/`start` joined `lib/jsonlist.c`'s page-size
  pairs, so six GOV.UK search rows that read one page of three a run page and
  cycle (2,000 a run) and MAIB/RAIB read their whole registers (1,106 / 604);
  the probe paces per host; ELIA's grid rows are windowed on their measured
  publication lag (`ods031`: 41 hours).

**The count was 48,343** (2026-10-19): the 47,178 below merged with
main's 22,408 (21,789 plus PR #50's batch 45) (its own `jp_feeds_batch41..44.c` beats of Japanese RSS sources,
PR #48's paging cycle of 533 rows, PR #49's collector labels), less six news
feeds both sessions had added — `make lint-sources` named them at merge time
and main's rows stay. Batch numbers 43 and 44 are shared between main's
`jp_feeds_batch4N.c` beats and this branch's `hp3b4N_*` tables; nothing
collides, read the beat. Batches 47 and 48 below were authored as 45 and 46 and
renumbered at merge time when main merged its own batch 45 (PR #50) first.

**On main, merged into this branch at 48,343** — the numbering collision this
branch's batches 47/48 (authored as 45/46) came from:

**main's count was 22,408** (2026-10-19), measured — batch 45's 619
measured rows (`docs/verified-sources-batch45.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_45_JPWPREGISTRY.md`) on 21,789, which
was 21,237 plus batches 43/44's 400 Japanese RSS feeds and the rows that came
with them.

**Batch 45 is batch 42's question asked of the registry itself.** Every `.jp`
host a registered row already fetches (an RSS feed, an HTML register, a CSV) was
asked whether it also runs WordPress REST — the homepage's
`Link: rel="https://api.w.org/"` names the root, which found 232 of the 245
hosts that have one; a bare `/wp-json` found 13. 691 collections on 148 hosts
answered with records; 619 survived machinery filtering, probe and the emit
sweep (580 OK, 38 SLOW with thousands of rows stored at the 200 s line). The
tables are split by HOST type (media, company, association, culture, cyber,
science, hazard, government), not by collection type, so each beat files
under one collector group. The same sweep over **5,123 federation-member hosts**
(chambers of commerce, JA, medical associations, fire chiefs, welfare councils,
co-ops, professional bodies) found **5,678 live collections on 1,372 hosts** —
NOT registered, because ~5,700 scheduled rows on a 6-hour cadence is a
scheduler-load decision; kept with counts in
`docs/staging-sources-batch45.federations.tsv`.

**main's count was 21,237** (2026-10-12), measured — batch 42's 1,916
measured rows (`docs/verified-sources-batch42.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_42_JPWPROUTES.md`) on 19,321.

**The count was 47,178** (2026-10-19): 47,115 plus the 63 rows a
retry pass recovered. Every row batches 44–46 had dropped as dead on two
readings minutes apart — 2,724 of them — was rebuilt from git history and
re-probed a day later: 74 passed the probe, 63 of those stored their feed in
the engine and are back in their beats (4 US, 1 UK, 25 Italian, 4 French, 1
Wikidata public body, 2 international organisations, 26 news outlets), with
their readings appended to the batch's verified TSV and their `engine` lines
removed from its rejects. The other 2,650 are still dead: the UK and Italian
403 bot walls, and hosts that were down and stayed down. The same day the
interval of every public-body feed row was set from the newest-item date its
description records (hourly within 3 days, 6-hourly within 30, daily within a
year, else weekly), the World Bank sanctions row moved to hpengine so its
apikey could leave the source file, and the 20 French collisions that had been
unverifiable were re-measured: every one the feed's own byte-identical copies.

**Batch 46 is the Wikidata seed on the two classes an analyst asks for first:
the press and the international bodies.** Newspapers, daily and online
newspapers, news websites and news agencies per country (8,482 hosts → 3,980
rows), and international organisations, intergovernmental organisations and
think tanks in one country-less query each (2,141 → 492): 155,994 emitted /
155,790 stored. Two things generalise. **A generator's defaults are claims
too**: `build_feed_manifest.py` had hard-coded `category=government` and
`record_type=government-notice` since batch 44, so the first generation of the
news beat labelled newspapers as government notices — it takes those as
arguments now. And **Wikidata's class is a claim, not a fact** (it calls Capcom
a think tank), so the class lives in the description as Wikidata's, never in
the row name.

**The count was 42,643** (2026-10-18): batch 45's 16,520 measured
public-body feed rows (`docs/verified-sources-batch47.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_47_WORLDGOVFEEDS.md`) on 26,123. Both
floors were re-recorded at 42,643 and `make source-count` agreed.

**Batch 45 is the same method on three more registers, and it hit the registry
cap.** Italy's IPA (22,455 official sites → 5,640 rows), France's DILA annuaire
(28,885 → 9,319) and Wikidata's public bodies in 196 countries, harvested one
cheap (country, class) SPARQL query at a time because a world-wide query times
out (11,245 → 1,561 rows from 157 countries): 277,289 emitted / 276,945 stored.
Four things generalise. **`MAX_SOURCES` (`registry.c`) was 32,768 and the tree
reached 43,453**: the build was clean and 2,392 rows did not exist at runtime,
found only by comparing a beat's registration count with its table — the cap
is 131,072 now, and `make registry-floor` fails on any OVERFLOW line, so run it
before believing an audit. **A URL changed after validation is unvalidated**:
fixing an HTML-escaped `&amp;` in 534 French URLs made Joomla finally receive
`type=atom` and serve Atom where the rows declared `channel.item`; every changed
row was re-fetched, re-classified and re-measured. **The engine's measurement
outranks the probe**: 392 rows the probe refused (502s and 403s from shared
municipal hosting at twelve concurrent requests) store their feed at six, and
ship with both readings. And **rebuilding the binary while an emit audit runs
corrupts the audit** (NO_RUN_LINE, UNREGISTERED): finish or stop the audit
first. Feed titles are unescaped and whitespace-normalised by the builder now;
batches 43–45 were regenerated with the names fixed.

**The count was 26,123** (2026-10-17): batch 44's 3,715 measured
US and UK public-body feed rows (`docs/verified-sources-batch44.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_44_GOVFEEDS.md`) on 22,408. Both floors
were re-recorded at 26,123 and `make source-count` agreed.

**Batch 44 is batch 43's method on two public registers of official domains**
— CISA's 16,906 `.gov` domains and the Cabinet Office's 7,927 `.gov.uk` names —
each root fetched once for the feeds it declares or links: 2,075 US and 1,640 UK
rows, 45,705 emitted / 45,704 stored. What generalises: **a domain
registration is not a website** (4,372 `.gov` and 1,564 `.gov.uk` names have no
server at the root; 1,639 and 992 answer 403 to a non-browser client);
**WordPress declares a COMMENTS feed beside every posts feed**, so drop
`/comments/feed/` before it ships as thousands of rows of nothing; and
**the engine measurement outranks the probe in both directions** — 91 UK rows
answered 403 to the probe's client and 200 to curl, so the table was generated
with all of them and the emit audit decided: 98 of those 403s refuse the engine
too (a bot wall, removed), and 4 rows the probe refused store their whole feed
(kept, both readings in the TSV). `gen_hp_batch.py` now escapes `.name_ja` as it
did `.name`: a feed title with a double quote broke the build.

**The count was 22,408** (2026-10-17): batch 43's 1,156 measured
Japanese municipal feed rows (`docs/verified-sources-batch43.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_43_JPRSS.md`) on 21,252. Both floors were
re-recorded at 22,408 and `make source-count` agreed.

**Batch 43 is what the municipalities themselves publish, and how it was found
matters more than the number.** The Digital Agency's list of 1,648 open-data
municipalities was followed to each site root; the catalogue axis yielded zero
(776 hosts swept for CKAN/ODS/Hub/dataeye paths, body-judged:
`docs/discovery-jp-catalogue-hosts-2026-10-17.tsv`), the feed axis 1,156 rows on
546 municipalities — 195 declared in `<link rel=alternate>`, the rest found by
reading the root page's anchors. Three things from the gates generalise. **A
substring count of `<item` is not a record count**: RSS 1.0 wraps its sequence in
an `<items>` container, so 60 EMPTY feeds carried "1 items" until every row was
re-measured at the element start. **A well-formed feed with zero items is
`EMPTY_RESULTSET`, not UNPARSEABLE** — `probe_hp_batch.py` says so now; the 60 are
almost all 緊急 (emergency) feeds that are empty on every day there is no
emergency, and they are NOT registered, because a source never seen to emit is
not verified however plausible its purpose. And **one reading is one reading**:
18 probe transport failures became 11 PASS on a second probe, and the one
`FETCH_FAILED` of the emit sweep stored 10 of 10 minutes later. 45,217 emitted,
45,216 stored; the one collision is an upstream feed carrying the same notice
three times, two copies byte-identical.

**The count was 21,252** (2026-10-17): main's 21,237 (below) plus batch 37's 14
measured rows and the two data.e-gov.go.jp organisation rows it surfaced, less the
one duplicate the url-limit pass removed (`geo-stac-swisstopo-swissimage`).

**Batch 42 is batch 41 asked properly.** Batch 41 requested ONE collection per
Japanese WordPress host, `wp/v2/posts`, because that is the path everyone knows.
Asking each host for its own route table at `/wp-json` instead found 6,726
advertised collections on 433 hosts — 1,067 distinct custom post types, the
registers where a company publishes its IR releases and a university its staff —
of which 2,567 held records and 1,916 survived probe and the emit sweep. **The
generalisable part is the method: ask the endpoint what it serves, rather than
probing the paths you can name.** A convention sweep found 465 live endpoints on
6,720 hosts; the route tables of those same hosts held four times as many.

**The count was 21,237** (2026-10-12), measured — batch 42's 1,916 measured rows
(`docs/verified-sources-batch42.tsv`, `native/collectors/OSINT_SOURCES_BATCH_42_JPWPROUTES.md`)
on 19,321. **19,321** was batch 41's 437
measured Japanese WordPress-REST rows (`docs/verified-sources-batch41.tsv`,
`native/collectors/OSINT_SOURCES_BATCH_41_JPWP.md`) on main's 18,884, which
made 19,321, LESS the one duplicate the url-limit pass removed:
`geo-stac-swisstopo-swissimage` read the same swisstopo endpoint, interval and
collection as the hand-written `swisstopo-stac`, and `make lint-sources` caught
it (dup-endpoint) the moment that row's page size changed to match. Both floors
are re-recorded at 19,320 and `make source-count` agrees.

**Batch 37 is the staging set finally probed (2026-10-12..17), and the headline is
15 of 80.** It was authored 2026-10-02 with no egress to any of its hosts, so every
host and path was inferred from its family's convention, and its README predicted a
high pass rate because a catalogue endpoint "needs only the host to be correct".
Sound reasoning, wrong conclusion, 19%: **a uniform API family tells you the shape of
the request, not whether the host serves it.** The whole `cam` beat is gone (every
ArcGIS path guessed; the server answers HTTP 200 carrying
`{"error":{"code":400,"Invalid URL"}}`, which only a body judgement can see),
eight cities have left Socrata, four national portals are not CKAN at all. The 65
are kept with their measured verdicts in `docs/rejected-sources-batch37.<beat>.tsv`;
the 15 are `hp3b37_{jp,fr,us,eu,row}.c`, every one measured emitted-AND-stored
(`docs/verified-sources-batch37.tsv`): Alberta 37,515, Greece 22,780, BODIK
18,535, opendata.swiss 16,521 — each equal to its own declared count.
Three defects were in the rows that PASSED, none visible to a probe: all 75 paged
rows lacked a stable sort (and Socrata's `order=name` is NOT unique — Colombia
stored 9,934 of 10,000 on it and 10,000 on `order=dataset_id`); `EU_ECB_FXREF`
keyed 29 daily rates on a field not on the record (repeated XML siblings are
INDEXED fields, not an array — read one record's `properties` before choosing a
key); and `JP_CKAN_DATAGOJP` pointed at `www.data.go.jp`, which **301s to
`data.e-gov.go.jp` and drops the query string**, so it stored ten of 18,826 at
`rc=0`. The first repair there lowered the stride to match "a server that serves
10" — the right observation and the wrong diagnosis. **Follow a 301 by hand before
concluding anything about what a server will serve.** And once it was pointed at
the real host it measured 18,826/18,826 — and `make lint-sources` removed it as a
duplicate: `hp3b23_jpnational.c` already reads that endpoint per organisation,
23 rows of `fq=organization:`. The portal has 25; the two it lacked are added as
rows of that family rather than as a second full read of the catalogue.

**Batch 41 is numbered 41 because 38, 39 and 40 were all taken while it was
open.** It was generated as 38, and by the time it came to merge, another
session's batch 38 (africa/china/france/uk/usa) had landed along with 39 and 40
— including a *direct* collision on `docs/verified-sources-batch38.tsv`, the
exact file the note below predicts. Renumbering touched the ids (`JO38_`→`JO41_`),
the four manifests, the four tables and their array symbols, the rejected and
verified TSVs, the batch doc and its filename. Re-check at merge time, not just
at generation time — this branch had to do it twice, because main moved again
between the renumber and the merge.

**The count was 18,884** (2026-10-12: 18,883 plus the `log-retention`
maintenance pod from the infrastructure audit, see the last section). 18,883
(2026-10-09) was main's 18,730: 18,645 —
batches 33–36 (batch 33's 43 measured supply-chain rows,
`docs/verified-sources-batch33.tsv`; batch 34's NDJSON row; batch 35's four
string-array rows; the spec-families beats and batch 36's 188) and the audit
follow-up's two SEC listings, on top of the 18,170 that included batch 32's 340
Japanese rows, less batch 31's `EU_VIES_VAT_VALIDATION` — plus batch 38's 150
(PR #37) less 65 batch-31 rows removed for rule 4d (PR #38). Then the
JP/CN integration's 168: batch 40's 100 Japanese and Chinese rows
(`docs/verified-sources-batch40.md`; authored as "batch 38" and renumbered when
main's batch 38 merged first) and batch 39's 72 rows unlocked by date tokens /
JSONP / body paging / HTML tables less the frozen VCSV row one of them replaced
(`docs/verified-sources-batch39.md`) — less batch 36's `CN_HK_LANDS_ADDR`, a
fuzzy geocoder that answers any text with addresses (`FILTER_IGNORED`,
`docs/rejected-sources-batch36.china.tsv`), and less 14 batch-36 Socrata
portal inventories that re-read a catalogue `hp3_portals.c`/`hp3_society.c`
already reads (`docs/rejected-sources-batch36.usa.tsv`). `make lint-sources`
prints it, counting hp_source table rows as well as `REGISTER_SOURCE`, and
`./bin/japanosint --list-sources` agrees.

