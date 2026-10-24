# What still needs fixing — 2026-10-20

Every item below was checked against the tree today, not remembered. Where a
number differs from CLAUDE.md, the number here is the one `make` prints.

Ordered by how much it costs to leave alone.

---

## A. The gates can no longer be read, which blocks everything else

> **A1 and the audit-sources half of A2 are FIXED** (same day). The scanner now
> knows the four walkers and the `source_def.url` rule, and `make audit-sources`
> reports 0 findings across the whole tree again, strict set 0/342. Fixtures 5
> and 6 pin both, and were checked against the old behaviour. The rest of A2
> and sections B–E stand.
>
> **A2 is FIXED too**: every drifted number corrected, including the batch-37
> line, which had claimed for a fortnight that a probed-and-generated set was
> "still not generated into C, still unprobed".

### A1. `make audit-sources` reports 50 findings where CLAUDE.md claims 0

```
strict set (the gated glob): 342 files, 0 findings      <- still clean
the rest of the tree:         27 files, 50 findings     <- CLAUDE.md says 0
    single-page  49
    record-cap    1
```

CLAUDE.md's whole argument for holding the non-strict scope at zero is that it
"makes any NEW finding a regression rather than a number in a backlog". That
property is currently gone: nobody can tell which of the 50 is new.

**They are false positives, verified on one.** `od_ckan_ch_zurich.c` pages
through the shared `od_ckan_collect()` → `pw_walk`, and carries an
`exhaustive-ok` marker on its `CKAN_PAGE_MAX` line — but its two `.url` lines
have no marker, and the scanner matches per line and cannot see into the shared
helper. The 26 files are the CKAN / ODS / ArcGIS portal callers fixed on
2026-10-12, plus `eni_bc_mineral_tenure.c`.

**Fix:** either put a one-line `exhaustive-ok` on each flagged `.url` line
(cheap, and exactly what CLAUDE.md prescribes for a scanner false positive), or
teach the scanner that a row whose `run()` calls `od_ckan_collect` /
`od_ods_catalog_collect` / `pw_walk` is paged. The second is better — 26 files
of markers is 26 chances to mark something that is genuinely broken.

**Do not just re-record the number.** Confirm the `record-cap` finding is also
benign first; it was not identified today.

### A2. CLAUDE.md's measured numbers have drifted

| claim in CLAUDE.md | what the tree says now |
| --- | --- |
| strict set "0 findings across 308 files" | 342 files |
| "the rest of the tree — 0 findings (1,678 files)" | 27 files, 50 findings (A1) |
| gated set "290 files as of 2026-10-05" | 342 |
| "554 keys" (collectors) | 593 |
| batch 37 "still not generated into C, still unprobed" | **generated and probed** — commit `5d5e1f99`, "15 of 80 pass, 16 rows measured", 70 rejected into `docs/rejected-sources-batch37.*.tsv` |
| `audit_url_limits.py` "89" | 70 |

The batch-37 line is the one that matters: a future session reading CLAUDE.md
would re-probe a set that has already been probed and rejected.

---

## B. Rule-4 debt — registered rows with no emit reading

### B1. ~~Batches 34 and 35: 239 rows never measured~~ — DONE

**Measured, same day: `docs/BATCH_34_35_EMIT_AUDIT_2026-10-20.md`.** 115
scheduled rows through the registry sweep (92 OK, 20 refused, 3 collisions) and
124 pivots through the manifest audit (73 OK, 40 EMPTY_UPSTREAM, 11 upstream
errors) — the two tools the two halves need, since the sweep's own
`NEEDS_ENTITY` covers all 124 pivots by design. 33,108 emitted, 33,108 stored,
**0 lost to uid collision**.

Eleven rows repaired or removed, each on a measurement: six hosts whose filter
rejects one WORD of the engine's User-Agent (0 → 96/186/397/166/963/1,823
records, and the tree already knew host by host in per-ROW headers), four rows
re-pointed from a refused bulk action to `package_search` on the same CKAN, and
two removed — a `data.gov.uk` row that is a proxy in front of the row beside it,
and an FDSN event row on a node that does not run the event service. Registry
48,794 → 48,792.

The original item read:

### B1 (as written). Batches 34 and 35: 239 rows never measured

139 `JO34_*` and 100 `JO35_*` rows. `docs/verified-sources-batch3{4,5}.tsv`
exist but describe only the small measured beats (`ndjson`, `strings`) — the
hazard CLAUDE.md flags as "read the BEAT, not the batch".

They were unverifiable when authored because that session's network policy
reached only package registries. **That is no longer the constraint**, and two
things fixed today remove the rest of the excuse: the shared TLS context (a
strict default context was calling live hosts dead) and the pinned binary (a
rebuild mid-audit silently voided 229 of 290 verdicts).

```sh
python3 native/tools/audit_registry_emit.py --bin ./bin/japanosint --match '^JO34_' --jobs 4 --timeout 220
python3 native/tools/audit_registry_emit.py --bin ./bin/japanosint --match '^JO35_' --jobs 4 --timeout 220
```

### B2. ~~Batch 31's 90 refused rows have never been re-run~~ — RE-RUN, and the rule would have been wrong 44 times

Second reading taken: **46 HTTP_404, 42 HTTP_403, 2 transport**, against
2026-10-11's 48/42. Essentially identical nine days apart, so "a row that fails
twice leaves" applies to 88 rows by the letter.

**It would have discarded 44 live publishers.** Each row was then asked three
questions instead — its probe URL with the engine's agent, the same URL with the
agent that clears the known word filter, and the HOST ROOT — and the root is
what separates "the publisher is gone" from "our path is stale"
(`docs/batch31-refused-triage-2026-10-20.tsv`):

| rows | reading | what it means |
| --- | --- | --- |
| **44** | path 404, root **200** | host alive and serving; our path is stale. A re-point each, not a removal |
| **36** | path 403, root **403** | the host refuses this client everywhere; no agent in UA_OVERRIDE changes it. A wall |
| 5 | path 403, root 200 | refused at the endpoint while the site serves — a key, a session, or an endpoint wall |
| 1 | path 403, **alt agent 200** | `OFFSHORE_AIRCRAFT_REGISTRIES`. The word filter again — planelogger.com added to UA_OVERRIDE, row recovered, verified twice each way |
| 1 | path **200** | `JP_EGOV_LAW_SEARCH` answers today, so its two 404s were about the entity asked or the day |
| 1 | path 404, root 404 | `KE_OPEN_DATA` — the host itself is gone. The one row the rule fits exactly |
| 1 | path 404, root 403 | `TZ_BRELA_ENTITY` |
| 1 | timeout, root 200 | `CA_VESSEL_REGISTRY` — unmeasured, not dead |

So the removal list is **one row long**, not 88. What remains is a re-pointing
job (44) and a decision already taken once in this tree for the walls (36): the
`registry.faa.gov` precedent is that a source "stays honestly failing rather
than quietly evading", and they do disclose, one `collector-fetch-failure` row
each.

**The 44 have now had their measurement pass** (2026-10-21,
`docs/b2-path-stale-triage-2026-10-21.md`), and it changes the shape of the
remaining work from "44 path edits" to "44 decisions".

* **A third reading** confirms the paths: 43 still 404,
  1 `LOCAL_EGRESS` (unmeasured from here). Not a deploy.
* **Each host was asked what it serves** rather than probed for paths we can
  name — its own search form with the field names a pivot must send, its API
  and developer links, its declared sitemaps
  (`tools/discover_search_endpoints.py`,
  `docs/b2-path-stale-discovery-2026-10-21.txt`). Candidates on 24 of 43.
* **And a 200 IS NOT A FIX: rule 4d killed six of the first eight candidates.**
  Asked about an impossible entity beside a real one, `IE_LOBBYING_REGISTER`
  answered within 20 bytes, `IFC_PROJECT_DISCLOSURES` within ONE, and
  `APG_FATF_STYLE_BODIES` — whose own search form declares
  `GET /documents?keys=` and which returns 76 anchors — ignores `keys`
  completely. Those pages render results in the browser; the server hands back
  the same shell for any query. Re-pointing there trades a 404 that discloses
  itself for navigation chrome attributed to the entity asked about.
* **Seven rows have a measured replacement**, three of them documented
  open-data APIs that are a straight upgrade on the HTML scrape they replace
  (Eduskunta's `avoindata` API, Stortinget's JSON export, the Austrian
  Parliament's filter API), plus the ECB's server-rendered list of supervised
  entities, Copernicus EMS's moved activations host, Municipal Money's own API
  and two search paths whose filter measurably works.
* **Four rows must NOT be re-pointed**, and each is a different lesson:
  `WORLDBANK_CONTRACT_AWARDS`'s Socrata domain is EMPTY with no query at all
  (a re-point there fetches 200 and emits nothing for ever);
  `JP_KANPO_GAZETTE` is behind a login now, so it is credential-gated rather
  than moved; and `JP_MHLW_LABOUR_VIOLATIONS` and `JP_PMDA_APPROVALS` offer a
  Google Custom Search as their site search, which is Google's data about the
  ministry and not the ministry's register.

**The re-pointing pass is DONE** (2026-10-21, same day — the readings are in
`docs/b2-path-stale-triage-2026-10-21.md` and
`docs/b2-repoint-emit-2026-10-21.tsv`), and it corrected five of the seven
answers above. Six rows ship, three of them NEW, and the registry goes
58,016 → 58,019:

| row | kind | measured |
| --- | --- | --- |
| `ZA_MUNICIPAL_MONEY` | pivot | Treasury's cubes API, `cut=demarcation.label:"{q}"` — 39 of a declared 39, impossible label 0 of 0 |
| `ZA_GOVERNMENT_GAZETTE` | pivot | `/search?search_query=` + `href_must=/documents/notices/`, 3 real / 0 impossible |
| `US_DATA_GOV_CKAN` | pivot | the HTML search, 8 records over 7 datasets |
| `NO_STORTINGET_DATA` | → daily | `/eksport/horinger`, 76 of 76 |
| `NO_STORTINGET_INTERPELLATIONS` | NEW | `/eksport/interpellasjoner`, 5 of 5 |
| `NO_STORTINGET_QUESTION_TIME` | NEW | `/eksport/sporretimesporsmal`, 60 of 60 |
| `EU_ECB_SUPERVISED_ENTITIES` | → weekly | the monthly workbook, sheet `SIs` — 831 emitted / 830 stored, the one collision being ECB's own duplicate LEI |
| `EU_ECB_SUPERVISED_ENTITIES_LSI` | NEW | the same workbook, sheet `LSIs` — 2,166 of 2,166 |

Five of those answers are not what the triage predicted, and the corrections
are the part worth keeping — all five generalise and are in CLAUDE.md:
`filter_query` on a server-filtered row discarding the upstream's own matches
(1 of 8 kept); `hp_register()` dropping a url-less row in SILENCE; the
dup-endpoint lint catching a "re-point" onto a URL the tree already read every
six hours; an anchor COUNT being mistaken for the data on two separate rows;
and a spreadsheet laid out for HUMANS needing `csv_no_header = 1`, which took
four readings because the ECB workbook's first run reported 831 records and
rc=0 while storing its own title block as supervised banks — and because the
two sheets of one workbook turned out to have different column geometry.

**The long tail is now closed too** (2026-10-22). Every row named below was
asked what its publisher actually serves, and the answer was an engine
affordance twice over rather than a path:

| row | outcome | measured |
| --- | --- | --- |
| `FI_EDUSKUNTA_RECORDS` | re-pointed | the `avoindata` API, on the new `row_header_path` — `Marin` 1 of 1, an impossible value 0 |
| `AT_PARLAMENT_RECORDS` | SHIPPED 2026-10-23, as batch 55 | there is no sort parameter — and none was needed. See below |
| `CZ_CUZK_CADASTRE` | re-pointed | `/api/v1/Parcely/Vyhledani`, `ApiKey` header, `JO_CUZK_API_KEY` — a credential disclosure until a key exists |
| `FR_ASSEMBLEE_NATIONALE` | kept failing + NEW row | /dyn/recherche 404s for a real name and an impossible one alike, and the homepage has no search form. `FR_AN_QUESTIONS_ECRITES` reads the open data instead: **19,165 of 19,165 written questions stored** in one request, 50 MB, on the new `zip_all_entries` |
| `JP_PMDA_APPROVALS` | kept failing + NEW row | the search is `POST /PmdaSearch/iyakuSearch` and answers the FORM PAGE for every POST constructible here (session cookie, referer, the image button's x/y, the column checkboxes). `JP_PMDA_NEW_DRUG_APPROVALS` reads the annual workbook: **95 of 95** across four monthly sheets, on the new `xlsx_sheet_all` |
| `JP_KANPO_GAZETTE` | kept failing + NEW row | the search is behind a paid MEMBER LOGIN (`/kanpou/auth/login/LoginStartUp.form`), which no `key_env` opens. But the gazette itself is free: `JP_KANPO_DAILY_CONTENTS` reads each day's table of contents, **25 of 25** carrying 43 notice titles |
| `JP_MHLW_LABOUR_VIOLATIONS` | kept failing; the register was FOUND and is still not shippable | see below |
| `WORLDBANK_CONTRACT_AWARDS` | kept failing; the move is now measured, the successor is not | see below |

### The three that were still open on 2026-10-22, settled 2026-10-23

**`AT_PARLAMENT_RECORDS` — the answer was "no sort parameter exists", and the
row ships anyway.** Full readings in
`docs/at-parlament-filter-api-2026-10-23.md`; the short version is three
measurements and a control.

* **The query string is ignored.** `?GP=XXVII` answers the unfiltered 391,354
  where the POST body `{"GP":["XXVII"]}` answers 76,457. Every sort spelling
  the 2026-10-21 triage probed was being discarded before the server looked at
  it, which is why 14 of 26 appeared to "work".
* **The control settles it.** `frobnicate=7` and `sortColumn=99999` reorder the
  first page exactly as `sort=28` does, and no reading is monotonic in the
  column it names. Nine spellings inside the POST body behave the same; an
  unknown body key there is silently ignored. *Probe a control before believing
  a parameter works.*
* **`pageNumber` is not the page parameter — `page` is, and it drifts.** Page 1
  and page 2 of ONE filtered query overlap by **8 of 20 rows**, and two reads of
  `page=1` return disjoint sets. No cycle can sit on that.
* **But `showAll=true` plus a filter is a complete, deterministic, single-page
  read**: `pages: 1`, `rows == count`, byte-identical across two reads, on
  every slice. No ordering is involved because nothing is paged.

So the row is a PARTITION, not a walk. `GP` (legislative period) has 27
non-empty codes whose counts sum to **391,354 — the unfiltered count exactly**,
and finding that needed two steps that generalise: enumerating I..XXVIII reaches
only 389,139, and sampling 30 pages for the missing codes finds `KN` and `PN`
but **misses `BR` (2 rows)**, which only a direct count found. *Enumerate the
dimension, then check the slices sum to the whole; a sample finds the common
gaps and not the rare ones.*

Seven rows ship (`native/collectors/pivot/table/hp3b55_atparl.c`), grouped under
the 100,000-row page cap and the 64 MB body ceiling, no group mixing a closed
period with the open one. Engine readings, each `stored == emitted` with no
notice: 50,896 / 77,646 / 55,695 / 51,970 / 51,979 / 76,457 / 26,718 — one
request each. The record key was measured and the obvious one is wrong:
`WENTRY_ID`, `HIS_URL` and `GP_CODE+ITYP+INR` all agree on 75,485 of XXVII's
76,457, which invites writing the 972 off as the upstream's duplicates — and all
972 **differ**, in exactly one column, `NRBR`: a material that went through both
chambers is served once per chamber under one web-item id.
`GP_CODE+ITYP+INR+NRBR` is 76,457 of 76,457.

**`JP_MHLW_LABOUR_VIOLATIONS` — the register WAS found, and that is a better
reason for not shipping than "no replacement".** It is not on
`www.mhlw.go.jp` at all, which is why a search of the ministry's site and
sitemap could not see it: each **prefectural labour bureau** publishes its own
on `jsite.mhlw.go.jp` (Tokyo's 労働基準関係法令違反に係る公表事案 at
`/tokyo-roudoukyoku/jirei_toukei/souken_jirei.html`). Two things stop it. The
page's only content is a link to a **PDF re-published under a new numeric id
each edition**, and hpengine has no PDF mode — so an `HP_HTML` row here would
store the page's NAVIGATION as enforcement cases, which is what
`FAA_NNUMBER_REGISTRY` was removed for. And the path is Tokyo's own: osaka,
aichi, fukuoka, kanagawa and hokkaido all 404 on it, so this is 47
differently-shaped bureau sites and a discovery beat of its own. What would
make it shippable is a PDF text mode in the engine.

**`WORLDBANK_CONTRACT_AWARDS` — the move is measured; the successor is not
measurable from here.** `finances.worldbank.org` **302s to
`financesone.worldbank.org` preserving the path**, and that host answers HTTP
200 with its single-page-app HTML for every path — `/api/catalog/v1` and
`/resource/<id>.json` alike. So the Socrata platform is retired and the row's
fetch-failure note reads "a 200 that did not parse", the Archive-It shape: the
move really happened, unlike the three cases CLAUDE.md records where that
wording sent a reader hunting one. The successor is the Data Catalog API
(`datacatalogapi.worldbank.org/ddhxext/`), which is where the new site's own
dataset ids live (the awards page canonicalises to `DS00047`). It is **not**
re-pointed there: four attempts across two sessions, one after **150 s of
complete idleness**, all HTTP 429 from Cloudflare with a fresh `retry-after` of
~45 s, so the quota is not ours to drain and the endpoint cannot be verified
from this container. A candidate is not a fix.

**And the operator's decision has been taken: keep every one of them,
disclosing.** The rows with no reachable replacement — the 36 walls, the 404s
that render client-side, `COPERNICUS_EMS_ACTIVATIONS` whose "31 anchors" were
chrome — stay registered and file one `collector-fetch-failure` each, which the
source pages now read back as a reason and a remedy beside the status. Nothing
is removed for failing honestly. (The faa.gov precedent that used to argue for
keeping had been overturned by its own author, so this rests on the decision
rather than on that precedent.)

### B2-bis. ~~Two national parliaments publish a JSON TABLE the engine cannot key~~ — `row_header_path` SHIPPED

`hp_source.row_header_path` names the sibling array holding the column names,
and the engine keys each array record by NAME rather than by position; where the
header runs out, the surplus cells keep their positional names rather than being
dropped. `hptest` 49–49f pin it, six of the eight confirmed to fail with the
path disabled and the other two being the controls that pin the old behaviour.
`FI_EDUSKUNTA_RECORDS` ships on it, measured through the engine. The Austrian
row still does not, for the reason below: its index has no stable ordering.

### B2-ter. A workbook's other sheets and an archive's other members — both shipped

The same shape twice, found while closing B2's long tail (2026-10-22): one fetch
buys a container, and the engine read one thing out of it.

* **`xlsx_sheet_all`** reads every worksheet, each with its own header. PMDA
  publishes its annual drug approvals as one workbook with ONE SHEET PER MONTH
  and a new sheet every month, so a row pinned to a name goes stale in weeks and
  a row pinned to index 0 reads May for ever — neither visible in a record
  count. `JP_PMDA_NEW_DRUG_APPROVALS`: 95 of 95, the sum of the four sheets'
  data rows exactly.
* **`zip_all_entries`** reads every archive member, each parsed on its own.
  The Assemblée nationale publishes its written questions as 19,165 JSON
  documents inside one 50 MB ZIP, so the first-entry read was 19,164 records
  dropped — and the engine's own `zip-extra-entries` notice had been saying so
  for as long as the row existed, with remedies ("a named-entry read, or one row
  per member") that do not reach 19,165 members. `FR_AN_QUESTIONS_ECRITES`:
  19,165 of 19,165 in one request.

Both disclose a container member they could not read, by name, instead of
leaving a silent shortfall; `hptest` 50–50e and 51–51c pin them, each confirmed
to fail with the new path disabled.

**And finding them found a real bug in the OLD path.** HP_XLSX's single-sheet
failure set `out = -1` under a comment calling it an honest failure, and
`out = st.emitted` three lines below overwrites every mode driver's return —
deliberately, so a later page's failure cannot erase the earlier pages' count.
So a workbook that would not convert AT ALL reported `rc=0 records=0`, the
invisible nothing rule 1 exists to prevent. The run's own error flag is what
survives; `hptest` 50e fails under the old line.

The original item read:

### B2-bis (as written). Two national parliaments publish a JSON TABLE the engine cannot key

Measured 2026-10-21. Eduskunta's `avoindata` API and the Austrian Parliament's
filter API both answer with an array of ARRAYS — `rowData` / `rows` — and put
the column names in a sibling `columnNames` / `header`. The engine's JSON path
flattens an array record POSITIONALLY (`.0`, `.1`, …), so a row could ship
today and its `title_keys`/`id_keys` would be positions whose meaning the
upstream is free to change without anything here noticing. That is the CSV
`csv_no_header` shape in JSON clothing, and two unrelated publishers serve it,
which is the argument for a declared header path rather than positional keys
hand-written into two rows.

Both are otherwise ready. Eduskunta's filter is honest and measured —
`columnName=lastname&columnValue=Marin` → 1 row, an impossible value → 0,
unfiltered → 100 with `hasMore` — so it can stay an entity pivot. The Austrian
one is a bulk export and wants an interval: 391,269 rows over 4 pages, and it
**ignores `pageSize`** (a request for 3 rows returned 100,000 rows and 58 MB),
so it also wants a per-row timeout and a hard look at cost before it ships.

The original item read:

### B2 (as written). Batch 31's 90 refused rows have never been re-run

`docs/emit-sources-batch31.tsv` records 48 answering 404 and 42 answering 403.
They were deliberately kept, correctly, because "one probe is one reading" —
but CLAUDE.md's own rule is "re-run each FETCH_FAILED once; a row that fails
twice leaves", and the second reading was never taken. A retry pass on batches
44–46 recovered 63 of 2,724, so the expected yield is real but small.

---

## C. Rows with a measured shortfall and a known next step

### C1. ~~KKJ's two entity pivots read 1,000 of 62,255~~ — MEASURED, declined, and it found a corpus hole

`JP25_JPBIZ_KKJ_QUERY` and `_ORG` are bounded at the portal's hard `Count` cap.
The open question was whether they could slice per entity by date the way the 47
prefecture rows do. Measured 2026-10-22, in both directions:

**The slice is possible.** `CFT_Issue_Date` composes with the pivots' own
parameters — 清掃 answers 62,308 hits in all and 13,406 for 2026; 東京都 answers
3,312 and 946.

**It is declined, on the arithmetic.** A busy keyword holds ~13,400 notices a
year against a 1,000-record cap, so fitting a window under the cap means windows
near a week — several hundred requests to answer one question a person is
waiting on. The pivot's bound is a view bound over the NEWEST records, which is
what rule 2 permits so long as the view says how much of how much it is showing,
and it does.

**And the premise that the prefecture rows cover the corpus is 97.7% true.**
清掃 answers 62,308 hits; the same query summed over `LG_Code` 01..47 answers
**60,902**. So about one notice in forty-four carries no prefecture code, and
nothing below reads it. Reading 1,000 records of that query, 9 of them carry no
`LgCode`, and they are national bodies: 日本中央競馬会, 林野庁, 国立研究開発法人
医薬基盤・健康・栄養研究所, 科学技術振興機構, 国立研究開発法人森林研究・整備機構.
See E5 — this is now its own item, because no filter was found for them.

### C2. Toyama's dump days are unreachable by any means currently available

Single days of **1,449** and **1,161** notices against a per-read cap of 1,000,
measured day by day. No `window_days` fixes it — the cap is per READ. Reaching
them needs a different route (a bulk export, or a filter that subdivides within
a day). ~460 records.

### C3. Tokyo's KKJ backfill takes ~132 days

4-day windows, 530 of them, at a 6-hour cadence. Deliberate and disclosed, and
the notice says where it is. On the list so nobody reads it later as a stall.

### C4. ~~Eight cycling rows still carry a declared descending sort~~ — all measured per host

CLAUDE.md: a descending sort and a cycle do not compose, and the fix is an
ascending sort **verified per host**. 22 rows were done that way on 2026-10-18;
the rest were deliberately left rather than changed on reasoning.

The eight were never named here, so the set was rebuilt from the tree on
2026-10-22 rather than guessed at: every row with a declared descending sort
whose ceiling the 2026-10-12 audit could not establish, less the ones whose
identity IS recency (the 15 `DATAVERSE_*_LATEST_DATASETS`, the five
`NB_NO_LATEST_*`, `DIGITAL_COMMONWEALTH_LATEST`), plus the three rows that carry
`page_resume` AND a descending sort — which is the non-composition itself.
Each host was then asked. What came back:

**Changed, each measured.**

* **The two US Treasury rows** both cycle on an append-only series, so every new
  day landed at the HEAD of the walk. `api.fiscaldata.treasury.gov` honours
  `sort=record_date` and declares `meta.total-count`:
  `US_FISCAL_CASH_BALANCE` 16,710 records over 168 pages — ascending now, cycle
  verified over two runs against one database (2,000 a run, 2,001 → 4,001
  distinct, no overlap, position page 21 → 41, dates walking forward
  2005-10-03 → 2009-04-13 → 2011-06-06, about 2 days per cycle at six-hourly);
  and `US_FISCAL_MTS` 3,145 records over 32 pages, which does not need a cycle
  at all — `page_max = 40`, `page_resume` dropped, measured **3,145 / 3,145
  stored in 33 requests**, complete every day instead of two thirds of it a day
  stale.
* **The NADA microdata catalogues**, eleven rows, all authored
  `sort_by=created&sort_order=desc`. Six honour `sort_by=idno` and are ascending
  now (UNHCR 1,108, FAO 1,783, Zambia 25, Cambodia 45, ERF MENA 211, Somalia
  11); **two ignore the parameters entirely** — DataFirst South Africa and
  Armenia return identical first rows for asc, for desc and for the original
  sort, so the parameters are dropped rather than left claiming an order the
  server does not give; three are unreachable from here (Nepal, Palestine,
  Costa Rica, each `rc=-1` with zero bytes through the engine's own client the
  same day) and are left exactly as authored. Re-measured after: FAO 1,783 /
  1,783, UNHCR 1,108 / 1,108, ERF 211 / 211, DataFirst 605 / 605, each equal to
  its own `found`.

**Deliberately kept descending, with the reason measured rather than asserted.**

* `UK_EC_DONATIONS` — ascending leads with records whose `AcceptedDate` is null,
  so that sort is not a stable ordering. Already documented.
* `TAGINFO_TAGS_POPULAR` — "popular" is the question; the sort IS the ranking,
  and the endpoint returns all 14,546 tags in one response anyway.
* `ENERGINET_PRODCONSUM_SETTLEMENT` — the live-tail half of a deliberate pair
  with `ENERGINET_PRODCONSUM_SETTLEMENT_HISTORY`, the same shape as
  `PLOS_RECENT`/`PLOS_CORPUS`.
* `UK_FSA_FOOD_ALERTS` — COMPLETE inside the ceiling, so newest-first costs
  nothing. The FSA publishes no total, so the register's size was measured by
  bisecting `_offset`: **1,370 alerts**, 14 pages of 100, the oldest
  2018-01-09. `_sort=created` is honoured and is the fix if it ever outgrows
  the ceiling.
* The 21 rows whose id says `LATEST` — reading the newest page is the question
  they exist to ask.

### C5. ~~`PLOS_RECENT` wants a sibling~~ — DONE

417,719 records against a 20-page ceiling. It deliberately does not cycle —
recency is its purpose — so the whole corpus needs a second row with an
ascending sort.

**`PLOS_CORPUS` ships.** `publication_date asc,id asc`, `page_resume = 1`, and
`page_max = 50` because a 50-page burst was MEASURED on api.plos.org (50× 200 in
16.0 s, no rate-limit headers) rather than extrapolated from the default 20.
Deep paging reaches `start=400000` in 1.1 s, so there is no cap to work around
and no hostgate gap was invented. Verified over two runs against one database:
5,000 records a run, 10,001 distinct after the second, no overlap, dates
2003-08-18 → 2008-11-10 — the oldest end of the corpus, walking forward. Full
cycle ≈ 21 days. 418,142 by PLOS's own `numFound` today.

### C6. ~~`JO32_MUNI_YAMAGUCHI_CKAN` became reachable today~~ — REFUTED, and it found a bigger gap

It was rejected as a TLS failure. With the shared context it now reaches the
host and answers **HTTP 409**, not a TLS alert — a CKAN `datastore_search`
missing its `resource_id`. That read as "a URL defect, which is fixable: one
recoverable source". **Measured, it is not a source at all**, and the thing it
pointed at is already read exhaustively:

* the candidate was a HOST-level placeholder from batch 32's generation, with no
  `resource_id` because no particular resource was ever chosen. There is nothing
  to re-point;
* `yamaguchi-opendata.jp` publishes **20 organisations and 788 datasets**
  (`organization_list` and `package_search?rows=0` agree), and the tree holds
  **all 20** organisation rows plus the org list and the search pivot — an exact
  set match, measured id by id;
* run serially through the emit audit, every one of those 20 rows stores
  **exactly** the `package_count` the portal declares for it (102/102 for the
  prefecture, 76/76 Ube, 47/47 Shimonoseki …): **808 emitted, 808 stored, 0 lost
  to collision**. Dataset-level coverage of that host is complete.

**What the 409 did surface is a layer the tree does not read anywhere**: the
portal's datastore holds **799 tabular resources**
(`datastore_search?resource_id=_table_metadata` → `total: 799`), i.e. the ROWS
inside the datasets, where every CKAN row in this tree reads only the dataset
METADATA. That is batch-shaped work rather than a repair — one row per resource,
each named from `package_search`, with counts measured per row — and it
generalises to every CKAN with the datastore extension enabled, which is most of
the ones already registered. `_table_metadata` itself is NOT a source: its
records carry a UUID and an oid and no title.

Filed as an opportunity, not a defect. The item as written was a false positive
of my own: it inferred a repairable row from an error code without asking what
the row was meant to be.

---

## D. Unmeasurable from this container — record, do not guess

### D1. ~~31 rows fail certificate verification, for the engine too~~ — FIXED for 24, and the premise was wrong

This item read one MESSAGE as one cause. `unable to get local issuer
certificate` does not mean the certificate is untrustworthy; it means no client
here could find the certificate that SIGNED it, which on these hosts is because
the server sends its leaf WITHOUT the public intermediate that signed it.

Measured 2026-10-21, nine hosts, each leaf's own AIA `CA Issuers` URL fetched
and the chain verified against the SYSTEM root store: nine of nine OK, seven
distinct intermediates, every one chaining to a root already trusted. They are
in `native/data/tls/extra-intermediates.pem`, loaded by the engine
(`core/catrust.c`) and the tools (`tools/jo_tls.py`); verification stays on
throughout, and `tests/unit/test_catrust.c` fails the build if any entry is
self-signed, expired or unchainable. Through `http_request()` itself: all nine
go from `rc=1 status=0` to HTTP 200.

**23 sources registered as a result** — batch 52's 19 (`hp3b52_{ansd,hklegco,
statsgov}.c`) plus four `JO41_WP_CORP_*` rows spliced back into
`hp3b41_jpcorpwp.c`. Two of the 24 recovered candidates were still not
registered, for reasons no record count can see
(`docs/rejected-sources-batch52.tsv`). Full account:
`docs/tls-intermediates-2026-10-21.md`.

What is LEFT of the original thirty: **2 genuinely unreachable** — an SNI
mismatch at `dataverse.openscholar.org` and `nesstar.statssa.gov.za`, where
only not checking the hostname would get in, which is not on offer — 1 that was
this container's own egress, 1 already-refuted Yamaguchi 409, and 2 that
re-point rather than need a certificate (`www.ip.kyusan-u.ac.jp` →
`www.kyusan-u.ac.jp`; `matrix.kde.org` delegates to `kde.modular.im`, which its
`*.modular.im` certificate covers). The six rows this item cited by name —
IODA ×3, Dataverse UCLA, NADA Nepal, NI MARENA — are registered rows on hosts
whose chains are a DIFFERENT fault and were not re-measured here; they are
D1-ter below.

### D1-bis. The engine cannot tell four TLS faults apart

A chain fault arrives as `rc=-1` and one `collector-fetch-failure` naming a
transport error. "No local issuer", "expired", "hostname mismatch" and
"self-signed in chain" want four different remedies — fetch the intermediate,
wait for the operator, re-point the row, refuse — and only the last is about a
certificate we should not trust. Writing the libcurl/OpenSSL verify result into
that record would have made D1 self-diagnosing instead of costing 24 live
publishers a near-miss with deletion.

### D1-ter. ~~The six registered rows D1 cited were not re-measured~~ — MEASURED, three distinct faults

IODA ×3, Dataverse UCLA, NADA Nepal, NI MARENA — REGISTERED rows, not
candidates, whose 2026-10-20 reading was `rc=-1`. All four hosts were asked the
same question on 2026-10-21, and they are **three different faults**, which is
the whole point of D1-bis:

* **`www.marena.gob.ni` — RECOVERED, for free.** Its leaf is signed by the same
  `Sectigo Public Server Authentication CA DV R36` that `www.ons.dz` omits, so
  the bundle already carried it. Measured through `http_request()`:
  `rc=1 status=0` → **HTTP 200, 203,652 bytes**. `NI_MARENA_PUBLICACIONES`
  works again with no change of its own.
* **`microdata.nsonepal.gov.np` — the leaf EXPIRED on 2026-10-16**, five days
  before this reading (`error 10 at 0 depth: certificate has expired`). Nothing
  here fixes that and nothing should; it resolves when Nepal's statistics
  office renews. Leave it disclosing.
* **IODA (`api.ioda.inetintel.cc.gatech.edu`) and `dataverse.ucla.edu` — the
  ROOT is not trusted in this container.** Both serve an InCommon intermediate
  whose own issuer is `emSign Root TLS CA - G3` / `G1` (eMudhra), and
  `/etc/ssl/certs/ca-certificates.crt` holds **zero** emSign certificates, so
  the failure is `error 20 at 1 depth` — the INTERMEDIATE's issuer is missing,
  not the leaf's. Fetching the intermediate therefore changes nothing, and
  making it work would mean adding a new TRUST ANCHOR, which
  `native/data/tls/extra-intermediates.pem` forbids by construction and
  `tests/unit/test_catrust.c` fails the build over. **This is an OS trust-store
  question for the container image, not a source question** — record it, do not
  work around it.

One method note, because it cost two findings before it was caught: an AIA
extension lists its OCSP responder FIRST and `CA Issuers` second, so a read of
the line after the extension header finds only the responder. Both IODA and
UCLA first came back `NO_AIA` from exactly that, and so did
`www.kanamic.net` among the nine. Read the whole block.

### D2. A set of hosts answer HTTP 000 through this container's proxy

Measured one at a time, with no concurrency: `odre.opendatasoft.com`,
`opendata.bordeaux-metropole.fr`, `bodacc-datadila.opendatasoft.com`,
Dataverse KU Leuven and others. The proxy names the cause itself —
`ws_closed_mid_exchange` — and the hosts are not denied by policy. So these
rows are unmeasured **from here** rather than broken, and a sweep run from
elsewhere would settle them. The final page-2 sweep is quantifying how many.

### D3. Archive-It's rate-limit budget cannot be measured from behind the limit

`partner.archive-it.org` answers `<title>Rate limit reached</title>` with HTTP
**200**, an hour apart, to single requests. The engine now names that shape and
points at `core/hostgate.c` — but deliberately no gap was added, because a gap
chosen without measuring the budget is a guess.

### D4-bis. The "it's the client" question is ANSWERED for one family

D4 below says the remaining difference between a host that serves a hand request
and refuses the engine is **the client, since the engine sends its own headers**,
and that nobody had measured it. Measured now, on a different family: six US
municipal CKAN portals answer **502 to the engine's User-Agent and 200 to the
same agent with the word "collector" removed**, six for six, reproducibly, and
serially — so concurrency was ruled out first. Entries added to
`core/httpclient.c`'s UA_OVERRIDE table; 0 → 3,631 records.

That does not settle `data.gov.au` below, which is a different host and may be a
different filter. It does say the bisection is cheap, it works, and it belongs
before any 4xx/5xx row is written off.

### D4. The `au-asic` 403s are not concurrency, and the next step is named

Five rows share `data.gov.au`; host-paced, two cycle, two read FETCH_FAILED and
one STUCK. By hand the host served 8 rapid serial then 12 concurrent requests,
all 200. CLAUDE.md already names the remaining difference: **the client**,
since the engine sends its own headers. Nobody has measured that.

### D5. `trickest_cve` — already recorded as unmeasurable; do not re-litigate.

---

## E. Correctness nobody has measured

### E1. CLOSED (2026-10-23): the eleven thresholds are MEASURED on the production model

A real embedding server was stood up in this container for it — llama.cpp built
from source and `multilingual-e5-large-q8_0.gguf`, the file this tree's notes
name as the production model, serving `/v1/embeddings` with 1024-d unit vectors
and without the e5 `query:`/`passage:` prefixes (which `core/service_vec.c`
measured worse here and defaults off). Full readings and how to reproduce:
**`docs/measured-thresholds-2026-10-23.md`**, raw output
`docs/measured-thresholds-2026-10-23.tsv`.

| | result |
| --- | --- |
| `story_tau` 0.85 | **MEASURED and correct.** 200 positives (same simhash `cluster_id`) vs 200 negatives (different cluster and source) separate completely — positives 0.9395 at p10, negatives max 0.8372. Best F1 0.8372 at F1 = 1.000; the value in force gives precision 1.000 and recall 1.000 |
| `relevance_sim_min` 0.40, `suggest_min_cos` 0.50, `route_min_cos` 0.55, `resolve_emb_min_cos` 0.60 | **admit 100% of unrelated pairs** — not filters on this model |
| `repair_sem_cos_floor` 0.70 | admits 99.3% |
| `corpus_vec_max_distance` √2 (L2) | admits 100%; the observed unrelated L2 range is 0.5572–0.7906, so the ceiling is twice the maximum |
| `entity_vec_cos_floor` 0.80 | admits 14% — a real but loose filter |
| `alert_sem_threshold` 0.82 | admits 2% — a real filter |
| `resolve_emb_margin`, `camera_dup_cosine_min` | not measurable this way, and the tool says so: a margin needs a triple, and the camera index is a different model with `camera_stills` empty here |

The unrelated band on this model is **0.6874 to 0.8448** over 300 question ↔
unrelated-record pairs across 30 sources (median 0.7641). `route_min_cos` was
re-measured in its own space — a question against a source CARD — and the
MINIMUM of 300 such pairs is 0.7261 against a cut-off of 0.55, so as shipped a
question with no entity routes to every scheduled source.

**Nothing was changed.** The measurement says what a cut-off cannot do, not what
it should be: any cosine cut-off here has to be above ~0.85 before it rejects
anything, and picking a value inside that window needs positives — pairs a human
has marked "these SHOULD match" — which exist nowhere in this tree.
`docs/threshold-labels.jsonl` is the next step and it is human work, not a
measurement. That is the remaining item, and it is a much smaller one than
"nothing has been measured".

Three defects in the harness were found by running it for real, and they are
written up in the .md. The worst: **its negative set came back EMPTY on a real
database** — `pairs_story` read a prefix `order by cluster_id` and then required
two different sources, and on a 412,521-row database holding 31 sources every
row in that prefix came from one source, so `n_neg = 0` after a complete
embedding run. A sample taken off the head of a collection is not a sample of
the collection; it samples per source now. The self-test gained two cases, so a
twelfth threshold whose default cannot be read fails `make unit`.

#### E1 as it stood (the harness, 2026-10-22)

`native/tools/measure_thresholds.py` (2026-10-22) is the tool that measures
them. It changes nothing: it reads the eleven keys out of `core/thresholds.c`
rather than keeping a second copy of the table, asks a real embedding server
for vectors on the same `/v1/embeddings` contract `core/llm.c` uses, and
reports per key the positive and negative score distributions, the cut-off
with the best F1, and the precision and recall THE VALUE IN FORCE achieves —
which is the number that says whether today's guess is wrong and in which
direction.

**It refuses to run without a real server** (`--embed-url`, else
`$JO_EMBED_URL`) and says why: a stub is what produced the numbers it exists
to replace, so measuring against one would report a stub's agreement with
itself. This container has no embedding server, so **nothing has been measured
yet** — the harness is ready and the reading is still owed.

It records the model `/v1/models` names in every output row, because a
threshold is only meaningful for one model.

**The hard part is labels, and three keys already have one that is not
circular:**

| key | label | why it is not circular |
| --- | --- | --- |
| `story_tau` | `intel_items.cluster_id` | simhash clustering, computed without embeddings. `story_id` is NOT usable: it was assigned using `story_tau`, so measuring against it would confirm whatever it already does |
| `entity_vec_cos_floor` | `entities.norm_key` and each row's aliases | string normalisation, not a vector |
| `camera_dup_cosine_min` | `camera_stills.sha256` | byte identity |

The other eight judge a QUERY against a record or a source and no human
verdict on such a pair is stored anywhere, so they read **UNLABELLED** and
carry a distribution only. A human has to author the label file; its format is
in the tool's docstring (`{"key", "a", "b", "same", "note"}`, one JSON object
per line). **No label file is shipped on purpose** — a file of invented
verdicts fed to this tool would turn the guess it is meant to replace into a
measurement, which is rule 1 at the level of the gate.

One key cannot be measured from pairs at all and says so rather than
approximating: `resolve_emb_margin` is the GAP between the best match and the
runner-up, so it needs a triple.

`--self-test` exercises the tool against a stub server in this process (the
`cleanup_stale_uids.py` precedent) and runs from `make unit`: 16 checks,
including that an L2 distance is swept the other way round from a cosine and
that reading a distance as a similarity cannot reach F1 1.0 — the one mistake
that would invert a reading silently.

### E1 (as written). Every embedding threshold is a guess from the literature

CLAUDE.md says it plainly: entity 0.80, story 0.85, alert 0.82, relevance, the
resolver margin — "**none has been measured on the production model**". They
were chosen from typical multilingual-e5 scores and tested against a stub.
Each one decides whether two entities merge, whether two items are one story,
and whether an alert fires.

### E2. ~~One sortBy-less WFS row is unaccounted for~~ — all thirteen named, and one was losing a parcel every run

Of 16 other WFS `startIndex` rows without `sortBy`, CLAUDE.md records that "12
of the 13 reachable ones serve page 2 fine". The 13th was not named and neither
were the 3 unreachable.

The set was rebuilt from the tree on 2026-10-22 — 20 rows page on
`page_param = "startIndex"`, **13 without `sortBy`** — and every one was asked
for page 2 directly. The verdicts in full:

**Eight are COMPLETE on page 1**, with `numberMatched`/`totalFeatures` at or
under the page size and page 2 honestly empty, so the sort question is moot for
them: `AR_IGN_AEROPUERTOS` 60, `AR_IGN_COMPLEJOS_FRONTERIZOS` 144,
`AR_IGN_ENERGIA` 445, `CR_SANCARLOS_TORRES` 48, `PY_MADES_ESTACIONES` 34,
`SI_GURS_MUNICIPALITIES` 212, `UY_MIDES_HOSPITALES` 25,
`UY_MTOP_AEROPUERTOS` 25.

**Four page — and two of them were skipping a feature.** The four Lechner
Hungary INSPIRE layers serve a different page 2, but
`HU_INSPIRE_CADASTRAL_PARCELS` returned `CP.CadastralParcels.107` on BOTH page 1
and page 2, the same feature on every one of four readings, and the engine
reported **1,774 emitted / 1,773 stored** — one parcel served twice and one
never served at all. `LC.LandCoverSurfaces.190` did the same.

The cause is not instability but a seam: **`GetFeature` without `startIndex` is
not the same page as `startIndex=0`** on this GeoServer, so page 1 of the walk
sat on a different ordering from pages 2..N. Walking the whole 18-page cadastre
by hand: as shipped 1,774 emitted / **1,773** distinct; with an explicit
`startIndex=0` 1,774 / 1,774; with a declared `sortBy` and no `startIndex=0`
1,774 / 1,774. Both are declared now, because two of these rows cycle and a
cycle needs the order to hold between runs as well as within one walk. The
engine agrees after: **1,774 / 1,774**.

The sort key had to be measured per layer. `sortBy=id` is HTTP 400 from the
backend — the host is a PHP proxy in front of a local GeoServer and reports that
400 inside an HTTP **200** as a PHP warning page naming `localhost:8085`, which
is rule 1's refusal-wearing-a-success-status in a new place. `id_localid` and
`objectid` are unique identifiers; land cover exposes five attributes of which
`kod` is a 20-value class code, so it sorts on `kod,area,perimeter` — 5,000
distinct triples over 5,000 consecutive features.

Two of the four also carried a comment that the code did not back: "read over a
cycle, 100 pages a run" with no `page_max`, so they read the 20-page default —
2,000 of 90,463 names and 2,000 of 170,058 surfaces a week, 45 and 85 weekly
runs per cycle. The host serves `count=1000` (800 KB and 2.9 s against 94 KB and
2.2 s for 100), so both are at 1,000 × 100 now. Measured:
**`HU_INSPIRE_PLACE_NAMES` 90,463 / 90,463 in one run**, 92 requests, position
cleared — the whole Hungarian gazetteer, where it had been reading 2,000 of it.

**One is DEAD, and its sibling with it.** `SI_GURS_HOUSE_NUMBERS` answers 404 —
and so does `GetCapabilities` for the whole `wfs-si-gurs-kn` service, while
`wfs-si-gurs-rpe` on the same host answers 200 with 112 KB of capabilities. So
Slovenia's cadastre WFS has gone from that path and its register of place names
has not. `SI_GURS_CADASTRAL_MUN` reads the same dead service. No replacement was
found from here: the host's root redirects to an auth page with no service
directory, and probing for the name we expected (`wfs-si-gurs-kat`, `-ku`,
`-kn-slo`, `/ows`) is what CLAUDE.md warns against — six guesses, six 404s.
Both rows are KEPT and disclose themselves through the health projection; see
E6.

### E7. The registry-wide health sweep is running, and its first 2,400 rows already found 14,060 records a run

`docs/registry-health-sweep-2026-10-22.tsv`, `audit_registry_emit.py
--scheduled --jobs 6 --timeout 90 --resume`. **It covers a fraction of the
55,699 scheduled rows and must be read as that** — at roughly 1,000 rows an
hour a complete pass is a two-day job, and the file is committed as it
accumulates rather than held back.

**Where it stands at 2026-10-24: 24,462 of 55,699 rows** (43.9%), resumed
across several invocations — each runs until the harness's two-hour background
limit and `--resume` picks up where it stopped. The last pass added 9,135 rows,
the fastest yet, so the remainder is roughly three more passes:

| verdict | n | at 15,327 rows | at 12,584 rows |
| --- | --- | --- | --- |
| OK | 20,635 | 12,850 | 10,603 |
| FETCH_FAILED | 2,674 | 1,473 | 1,306 |
| SLOW | 404 | 356 | 222 |
| COLLISION | 398 | 385 | 298 |
| EMITS_NOTHING | 239 | 151 | 132 |
| GATED | 93 | 93 | 4 |
| POD | 19 | 19 | 19 |

**FETCH_FAILED nearly doubled over this stretch and that is a property of the
REGION the sweep is walking, not of the fleet.** Rows 15,327..24,462 are the
`JO41_WP_CORP_*` and `JO47_FR_*` blocks — Japanese corporate WordPress
installs and French commune portals, thousands of small self-hosted sites,
where a dead host is the base rate rather than the exception. Each of those
rows DISCLOSED its refusal, which is the sweep working: `FETCH_FAILED` is a row
that filed a `collector-fetch-failure` record naming what happened, not a row
that failed in silence. The verdict to read as a defect is `EMITS_NOTHING`.

**SLOW has stopped growing with the machine**: 356 → 404 over 9,135 rows, about
0.5%, against the 5% and 8% rates of the two earlier stretches that shared the
container with a build and a unit suite. Those earlier 134 rows still need
re-running on a quiet machine before any is read as a finding; this stretch
does not.

The `GATED` jump from 4 to 93 is not a change in the fleet: that stretch of the
registry is where the key-gated rows sit, and `GATED` is the verdict
`audit_registry_emit.py` gained on 2026-10-23 for a row that stores one
`collector-status-notice` naming the env var it needs — a source behaving
exactly as designed, which this tool used to call `SINK_MISMATCH`.

#### The 87 `csv_delim = "comma"` rows, re-measured after the fix (2026-10-24)

`docs/comma-delim-refix-2026-10-24.tsv`. **The main sweep's rows for these 87
sources predate the fix and are superseded by that file** — they were swept
while the engine was splitting those files on the letter `c`.

Re-run through the same tool, all 87: **86 OK, 1 FETCH_FAILED, zero
COLLISION**, against 24 COLLISION / 62 OK / 1 FETCH_FAILED before. **1,483
records a run recovered**, 1,383 of them Japan Coast Guard vessel accidents
(19,370 of 19,370 where it had stored 17,987).

The count is the smaller half of it. Every one of the 87 now stores **20 to 24
NAMED FIELDS a record** where it stored one cell holding the whole line, and no
gate could see that: the record count was right throughout, which is why a
careful hand triage of the same rows reasoned about "thirteen columns" in
September while the engine was reading one.

Read those four non-OK columns with the corrections below rather than as
counts of broken sources: `FETCH_FAILED` is a row that DISCLOSED its refusal
(which is the sweep working, not a defect), `SLOW` means unmeasured at this
tool's kill line, `COLLISION` is a question and not a diagnosis, and
`EMITS_NOTHING` was measured 95% wrong as a verdict before the scheduler
disclosure landed.

#### The 80 `EMITS_NOTHING` rows, re-measured twice (2026-10-23)

All 80 were re-run against the fixed binary, then the 43 survivors re-run again
after two further fixes the first pass itself provoked
(`docs/empties-recheck-2026-10-23.tsv`, `docs/empties-recheck2-2026-10-23.tsv`):

| | n | |
| --- | --- | --- |
| **FETCH_FAILED** | **39** | were failing in SILENCE and now disclose |
| OK | 3 | transient — they store records |
| **EMITS_NOTHING** | **38** | fetch and store nothing |

So **half the column was a disclosure gap rather than an empty source**, and
closing it took two fixes that generalise:

* **32 of the 80 returned rc=-1 with every host answering**, which
  `run_status()` reads as "ok" ("fetched fine, feed was empty") — so nothing
  was disclosed. `il-knesset-dates` fetches **131,618 bytes** of the OData
  service's HTML maintenance page and returns rc=-1;
  `geo-defra-ukair-stations` and `eco-511ny-events` do the same with 2,038 and
  9,364 bytes. `core/scheduler.c` now has a second disclosure trigger — the
  collector returned non-zero and left no record and no notice — which names
  BOTH readings (a refusal served as a success, or a collector with a sloppy
  return code) rather than picking, and points the reader at the body and not
  the status. `run_status()` is untouched, so no dashboard changes colour.
* **Four national press feeds were serving their HTML homepage** and reporting
  `rc=0 records=0`: `fiji-times` (128,800 bytes), `nation-thailand` (107,639),
  `daily-mirror-lk` (70,103) and `cyb-cybereason` (106,845). `lib/rss_atom.c`
  already argued that a zero-byte 2xx is not an empty feed; a 2xx that is not a
  FEED is not one either. `make rsstest` pins the rule and, more importantly,
  its controls: it fires only when the parse found no items, and a body
  carrying `<item`/`<entry` is a feed whatever its root is called.
  `news24-za` is the discriminating case and stays silent — **424 bytes of
  valid RSS with today's `lastBuildDate` and zero items**, a feed being built
  and serving nothing, where the finding is the upstream's.

**One of the four recovered a live source.** Every feed path on
`cybereason.com` 301s to `levelblue.com` — the publisher was absorbed — and
`--discover-feeds` found that host's own declaration,
`/blogs/levelblue-blog/rss.xml`, PASS with 10 items. It ships as a NEW row
(`cyb-levelblue`) rather than a re-point, because a row named "Cybereason Blog"
reading LevelBlue's feed would be a false claim about whose research it is.
The other three have no replacement: all four homepages declare zero feeds, and
fijitimes redirects every feed path to `/`.

**And the 38 that remain are mostly SUPPOSED to be empty.** About a third are
warning and alert feeds with nothing active — `geo-meteoalarm-{cyprus,czechia,
poland}`, `geo-imgw-warnings-meteo`, `geo-hkobservatory-warnings`,
`eas-hk-hko-special-tips`, `geo-eccc-thunderstorm-outlook`,
`jpx-p2pquake-tsunami`, `geo-firms-{modis,viirs}-alaska-24h` (no fires in
Alaska in late October), `sas-arc-tas-natenv` — plus `news24-za` above and
`ENA_PATHOGEN_SARSCOV2_RUNS`, whose 30-day window holds 0 runs and whose row
says so. That is the honest empty the whole disclosure machinery exists to
distinguish, and after two passes it is what the column mostly contains.

The point of the sweep is to FILL the health disclosure, not to gate anything:
every scheduled run files a `collector-fetch-failure` record of its own, so a
source's page acquires its reason and remedy as the scheduler reaches it. The
sweep just front-loads that.

**That sentence was FALSE when it was written, and the sweep is what proved
it.** It said "every scheduled run already files" — true only of the 44,017
hpengine rows, because `lib/hpengine.c` is where that record is written. The
~7,350 VJSON/VGEO rows and ~1,250 bespoke collectors filed nothing, so a
refused fetch stored `records=0 stored=0` and was indistinguishable from an
honest empty: rule 1's invisible nothing, for about a sixth of the fleet,
underneath an open item that assumed the opposite.

The sweep found it as an arithmetic oddity rather than as a failure. Of its 310
`EMITS_NOTHING` verdicts, **288 carry `sched_rc=-1`** — a run that could not
fetch, not a run that fetched and stored none — and the tool had no way to tell
them apart, because its `FETCH_FAILED` test is corroborated against `[hp:…]`
stderr and these rows produce none. 18 of 18 sampled resolve to `vsrc*` files;
all 34 that did disclose are `hp16_*`. So the verdict distribution was reading
"301 sources that fetch and store nothing" where most of it was "288 sources
that cannot fetch and never said so".

Fixed in `core/scheduler.c`, which is the one place that sees every collector's
run — see rule 1 in `CLAUDE.md` for the four deliberate limits on what that
record may claim, and why an honest empty must stay silent. Re-measured on
three of the 288 afterwards: two now read `FETCH_FAILED` with a disclosure in
the database, and the third stores 10 of 10 — it had been a transient failure,
which is "one probe is one reading" arriving from the useful direction.

**So the remaining `EMITS_NOTHING` count is not a to-do list of 301 broken
rows** — and rather than estimate how many survive, all 310 were re-run under
the fixed tool. **Fourteen of the 310 are real**, which makes that column 95%
wrong as it stood:

| re-read as | n | |
| --- | --- | --- |
| `FETCH_FAILED` | **192** | could not fetch, and until this change had no way to say so |
| `OK` | **93** | works; the earlier reading was a one-off failure, which is "one probe is one reading" from the useful side |
| `EMITS_NOTHING` | **14** | genuinely fetches and stores nothing — the defect the verdict names |
| `POD` | 8 | maintenance pods, from before the tool learned to skip them |
| `COLLISION` / `SLOW` | 2 / 1 | |

The 14: `gnews-bg-health`, `gnews-ee-science`, `gnews-ee-technology`,
`gnews-fi-science`, `gnews-fi-technology`, `us-usaspending-agency-overview`,
`ansd-nada-microdata`, `sci-aq-ni-stations`, `sci-aq-ukair-phenomena`,
`sci-aq-ukair-timeseries`, `sci-stac-inpe-modisa-ocsmart-poc-daily-1`,
`eas-hk-hko-special-tips`, `lat-ec-expreso`, `sas-arc-tas-natenv`. Five are
Google News editions in small languages, which may hold nothing honestly — a
distinction the disclosure can now draw and the old verdict could not.
`sci-ogcf-idee-existinglanduseobject` is SLOW at 120 s with 2,400 rows already
stored, so it is unmeasured rather than failed.

**A RESUME FILE CARRIES THE VERDICTS OF WHICHEVER VERSION OF THE TOOL WROTE
EACH ROW**, and that is the lesson worth more than the re-count. `--resume`
skips ids already present in `--out`, by design and correctly — but the tool
was improved twice while this sweep was running (the `POD` verdict, then the
`[sched]` fetch-failure match), so the file became a mixture of three tools'
opinions with nothing in it saying which row came from which. The eight pods
sat there reading `EMITS_NOTHING` under a tool that would now skip them, and
resuming would never have revisited one of them. When a sweep's tool changes
mid-run, the rows measured before the change are stale data wearing the same
column headings as fresh data — re-run them deliberately, or the file quietly
reports the old behaviour for ever.

**And the COLLISION column was stale the same way, for the same reason** — so
all 99 were re-run too, and 33 of them are now `OK`. The arithmetic is the
point: those 99 rows read **55,921 records lost per run** as the file stood,
and **2,930** when re-measured. Nineteen times too high, because the rows
Catalonia's atlases, Colombia's portal and the nineteen Socrata city datasets
contributed were measured hours BEFORE their keys were repaired on the same
day. The ~53,000-record difference is the recovery those repairs made, which
independently cross-checks the 53,281 figure recorded for them under rule 4b.

What is left is a small and largely DECIDED tail: 66 rows losing 2,930 records
a run between them, the largest single row 645, and several of the biggest
entries are rows where the content-hash fallback was MEASURED to beat any
available key and deliberately kept — `lat-co-dispositivos-med` (386, whose
best reachable key is no better than the loss), `nam-norfolk-streetlights-and-outages-reports`
(155, reverted for exactly that reason) and a Providence tree row. So the
collision tail is not 99 rows of unexamined loss; it is 2,930 records a run, a
fraction of it already settled on the evidence.

Sweep totals after both re-measures and a further run, over **9,151 of 55,699**
scheduled rows (16.4%): **OK 7,841, FETCH_FAILED 891, COLLISION 213, SLOW 109,
EMITS_NOTHING 76, POD 19, GATED 2.**

#### The collision tail, worked from the top down (2026-10-23)

At 9,854 rows the COLLISION column is **206 rows losing 40,366 records a run**,
and the headline is that **86% of that number is correct dedupe somebody has
already measured and written down.** Read from the largest loss downwards:

| row | lost/run | what it is |
| --- | --- | --- |
| `gr-diavgeia-positions` | 25,054 | **correct dedupe, documented 2026-09-11.** 233 official post titles in a 1.6 MB file that repeats them 25,188 times, byte for byte; its own comment says "do not re-key this row" |
| `sci-exoarchive-stellarhosts` | 3,015 | **correct dedupe, documented.** `select *` over all 136 columns gives 47,903 rows and 44,929 distinct, so the residual is the upstream's own duplicate rows |
| `AP19_VN_VNDIRECT_STOCKS` | 2,737 | **REAL, and fixed** — see below |
| `ifrc-go-appeal-document` | 1,896 | **correct dedupe, documented.** `ordering=id` repeats a document once per related row, byte-identical |
| `AP19_TW_TWSE_AP11` | 675 | documented: 70 rows sharing a triple but differing are kept by the in-page guard; the rest are TWSE's verbatim repeats |
| `nam-providenceri-providence-tree-inventory` | 644 | **correct dedupe, measured today** — a composite was tried and REVERTED, see below |
| `TPEX_DIRECTOR_HOLDINGS` | 428 | documented: ~460 of 17,517 are TPEx repeating a row verbatim |
| `eur-soc-cat-b5rw-yz3c` | 386 | **correct dedupe, measured today** — a composite was tried and REVERTED, see below |
| `lat-co-dispositivos-med` | 385 | documented decision: its best reachable key is no better than the loss |
| `geo-jrc-inform-scores` | 382 | **correct dedupe, measured today**: 46,461 rows, **46,079 distinct whole records — exactly what it stores**. And `Iso3+IndicatorId` would be WORSE (45,932), so the content-hash fallback stays |
| `nam-norfolk-streetlights-and-outages-reports` | 154 | documented decision, reverted for the same reason |

That is 34,770 of the 40,366 accounted for, and the residual ~5,600 is spread
over ~195 rows averaging 29 records each.

**One of the three candidates was real. The other two were SAMPLE ARTEFACTS,
and that is the finding worth more than either fix.**

* **`AP19_VN_VNDIRECT_STOCKS` — real, fixed, +203 records a run.** Page 1 has
  100 distinct codes of 100, which is why a one-page check reads clean; across
  the table **a large share of rows carry no `code` field at all** — OTC
  instruments with no ticker, which sort together under `sort=code` and so
  cluster on the later pages, the "two pages can be too few" shape with the
  breaking dimension at the END of the collection rather than on page four.
  With `code` alone, the engine fell back to the record's first non-empty
  scalar, which for those rows is `type: "STOCK"` — identical for every one of
  them. Measured over the WHOLE 145-page table (14,433 rows): **11,606 distinct
  whole records, and `code` else `companyId` is 11,606 exactly** — the
  attainable maximum, with every one of the 2,827 repeated-key groups
  byte-identical. So `id_keys = "code,companyId"` (`,` CHOOSES; a 3-letter
  ticker and a decimal companyId cannot collide), and the engine now stores
  **11,566 of 14,433** against 11,363 before. The remaining gap is the API
  serving the same record on more than one page, which must collapse.
  Its `page_max` went 141 → 160 in the same change: the endpoint's own
  `totalElements` has moved 14,061 → 14,433 since 2026-10-12, so 141 pages had
  quietly become a ceiling that cuts.
* **`nam-providenceri-providence-tree-inventory` — a composite was shipped and
  REVERTED the same hour.** An 800-feature sample said
  `property_address_address+species+diameter_in_inches` hit the attainable
  maximum exactly (765 of 765). The engine disagreed: **7,324 stored against
  the content-hash fallback's 7,356**, and the full 8,000-feature read says why
  — 7,355 distinct whole features, 7,323 on that key. The fallback was already
  within one record of the maximum. Reverted, with the arithmetic in the row.
* **`eur-soc-cat-b5rw-yz3c` — same, 51 records a run.** A 2,000-row sample gave
  `numero_ens+espai+detall_espai` 1,978 of an attainable 1,983; over the full
  20,000 it gives 19,561 against the fallback's 19,614, with 19,616 distinct
  whole records. Even an eight-column key reaches only 19,604. Reverted.

So **a sample can say a key is perfect when it loses records, and the error
runs in the direction that gets shipped** — the sample is drawn from the head
of the collection, where the dimension that breaks the key has not appeared
yet. The engine's own `stored` count against the row's CURRENT loss is the only
reading that settles it, and it costs one `--run`. CLAUDE.md already says both
halves ("two pages can be too few"; "compare the proposed key against the row's
current loss and keep the fallback when it wins"); this is them being
rediscovered from the other side, twice in one hour.

**`FETCH_FAILED` is ~10% of the fleet now against 1% in the file's first
3,612 rows, and that is the disclosure working rather than the fleet
degrading.** Those failures were always there; until the scheduler filed a
record for collectors that file none, they stored `records=0 stored=0 rc=0` and
were indistinguishable from an honest empty.

**A third verdict column was wrong, and this one was a FALSE ACCUSATION** —
the worst thing a gate can produce. Two rows read `SINK_MISMATCH`, whose note
says "a collector emitting under another id, or a bug": `OPENEI_UTILITY_RATES`
and `worldbank-debarred-firms`, both behaving exactly as designed. They are
key-gated, so each stores one `collector-status-notice` naming the env var it
needs — rule 1's gated note — and the tool's `ENGINE_NOTICES` list named only
three notice kinds and not that one. So the run line counted the row as a
NOTICE and the database read counted it as a RECORD, the two disagreed, and the
verdict blamed the sink. Two rows in 9,151, which is exactly why nobody would
have questioned it.

Both halves are fixed. The database read now mirrors the engine's own rule
(`collector-%-notice` plus `collector-fetch-failure`) instead of copying a list
of kinds, so a notice kind added tomorrow is covered — the same rule-4c lesson
as `tools/manifest.py`: two things that must agree should not be two copies.
And a waiting-for-a-credential row gets its own verdict, **`GATED`**, naming
the variable that would open it (`JO_OPENEI_KEY`,
`WORLDBANK_SANCTIONS_APIKEY`), because a source doing the right thing does not
belong in a defect bucket — the same reasoning as `NEEDS_ENTITY` and `POD`.
Re-running all 93 `EMITS_NOTHING`/`SINK_MISMATCH` rows against the fix found no
further gated rows and 15 more transients, leaving 76.

**The rule for whoever resumes this sweep:** re-run the rows that carry a
finding before believing one. Three of this file's six verdict columns were
wrong — `EMITS_NOTHING` 95% wrong, `COLLISION` overstated 19-fold, `POD`
absent for eight rows — every time because the tool or the tree changed after
the row was measured and `--resume` never revisits a row it already has. The
verdicts that need no re-reading are the ones that claim nothing: `OK` and
`SLOW`.

What the first 2,390 rows say: **1,502 OK, 161 EMITS_NOTHING, 88 SLOW (hit the
90 s kill line — unmeasured, not failed), 34 FETCH_FAILED, 16 COLLISION.**

**Two of the collisions were a 70% loss and are fixed.** `eur-soc-cat-tqdx-tz5i`
read 15,152 emitted / 4,615 stored and `eur-soc-cat-9ehv-pmf5` 3,788 / 1,265 —
Catalonia's climate atlases, where the record is a MUNICIPALITY × VARIABLE and
VJSON's id precedence list finds no field at all (`codi_municipi`,
`nom_municipi`, `cap_municipi`, `variable`, `unitat`, `valor`), so it fell back
and collapsed each municipality's whole series. Measured against the upstream
before touching the key, as a collision verdict requires: 947 municipalities,
which is Catalonia's own count, × 16 and × 4 variables = 15,152 and 3,788 rows
exactly, and `codi_municipi+variable` is distinct on 5,000 of 5,000 and 3,788 of
3,788. `VJSON_IDKEYS` composes with `+`, so it is a one-line change each. Both
re-measured after: **15,152 / 15,152 and 3,788 / 3,788** — 14,060 records a run
that were being discarded.

Both still file a label-keys notice (every record is titled from its first
scalar, a numeric municipality code). That is disclosed and is not a loss;
fixing it needs a title override VJSON has no macro for.

**And then the sweep reached Colombia: eleven more of the same shape, and ten
are fixed.** `datos.gov.co` Socrata rows, the same cause — VJSON's id
precedence list finds no field in the record, so it falls back and keys on the
first scalar, which is a dimension. Every key below was measured over two pages
(1,000 records) BEFORE it was changed, and the row re-measured through the
engine after:

| row | key, measured | before | after |
| --- | --- | --- | --- |
| `lat-co-runt-actores` | `id_empresa+a_o+mes_nombre+tipo_actor` | 10,000 / 5,111 | **10,000 / 10,000** |
| `lat-co-medicamentos` | `expediente+expedientecum+consecutivocum+principioactivo+nombrerol` | 10,000 / 6,245 | 10,000 / 9,937 |
| `lat-co-secop2-contratos` | `id_contrato` | 10,000 / 6,789 | **10,000 / 10,000** |
| `lat-co-ideam-precip` | `codigoestacion+codigosensor+fechaobservacion` | 10,000 / 7,591 | **10,000 / 10,000** |
| `lat-co-ideam-presion` | same | 10,000 / 9,075 | **10,000 / 10,000** |
| `lat-co-procuraduria` | `correo_electronico+cargo` | 4,986 / 4,166 | **4,986 / 4,986** |
| `lat-co-ideam-temp` | same as precip | 10,000 / 9,653 | **10,000 / 10,000** |
| `lat-co-secop1-multas` | `ruta_de_proceso` | 1,716 / 1,619 | **1,716 / 1,716** |
| `lat-co-trm` | `vigenciadesde` | 8,372 / 8,288 | **8,372 / 8,372** |
| `lat-co-ideam-estaciones` | `codigo` | 10,000 / 9,917 | 10,000 / 9,986 |

**16,545 records a run** recovered there, **30,605 with Catalonia's two.**

Three measurements in that table are worth keeping for the next person:

* **`fechaobservacion` alone is unique over 1,000 IDEAM observations and is the
  WRONG key.** A station network records many stations at one instant; the
  sample only looks unique because it is ordered by `:id`. The minimal key a
  search finds is not the same thing as the record's identity — this is the
  generator mistake that keyed batch 36 on each record's first field, arrived at
  by a different route.
* **INVIMA repeats a medicine's CUM row per ACTIVE INGREDIENT and per role.**
  The natural three-part key (`expediente+expedientecum+consecutivocum`)
  reaches only 983 of 1,000; the 15 remaining groups differ on
  `principioactivo`, `cantidad`, `nombrerol` and `tiporol`, so those belong in
  the identity.
* **A longer key is not automatically a better one.**
  `lat-co-dispositivos-med` is the one row left unchanged: its best reachable
  key (`expediente+presentacion_comercial+nombre_rol+marca+rol`) is 961 of
  1,000, no better than the 3.85% the row already loses, and reaching 1,000 of
  1,000 needs nine fields of which **539 of 1,000 rows are missing at least
  one** — a key that degrades for half the collection. The duplicates differ
  only in `pais_rol` / `ciudad_rol`, often blank. INVIMA's device register does
  not expose its own record granularity, and that is the finding.

The residuals are sub-1% (63 of 10,000 and 14 of 10,000) and are past what a
4,000-row sample can see — `codigo` is distinct on all 4,000 sampled station
rows. A byte-identical repeat upserting onto itself is correct dedupe, not
loss; whether these are that is unmeasured.

**And the third large collision the sweep found is one where the KEY IS
RIGHT.** `sci-ocean-ioc-sealevel-tab` reads 1,921 emitted / 1,340 stored, and
`sensorid` was already measured as 1,336 of 1,336 distinct on 2026-09-07 — the
feed has simply grown and now repeats rows. Measured on one body 2026-10-22:
1,921 rows hold **1,635 distinct WHOLE records**, so 286 are the upstream's own
byte-identical duplicates, which the sink's content hash collapses correctly;
and of the 259 `sensorid` groups with more than one row, **167 differ only in
`observations` / `first` / `last` / `timestampdata` / `updatedata`** — running
counters and timestamps, the same sensor reported twice in one response with
different totals. `sensorid+observations` would reach 1,602 of 1,921 and store
one sensor several times, inflating a station INVENTORY. The longer key is the
worse one again. The row now says so, so nobody re-litigates the verdict.

So of the three large collisions this sweep has surfaced, two were a wrong
identity worth 30,605 records a run and one is the upstream repeating itself.
A collision verdict is a question; it took a measurement each time to say which.

The other 14 collisions are 1–4 records each out of hundreds or thousands,
which is the shape of an upstream's own duplicate rather than a wrong key — but
each is a question until measured, and they are not measured yet.

**Then a THIRD portal family, and this one taught the opposite lesson.**
32 `nam-*` rows — North American Socrata city portals — read COLLISION for
24,536 records a run between them. Same cause as Catalonia and Colombia, and
the same two-page measurement on each. **19 ship for 22,676 records a run**,
each keyed on the dataset's own documented row id and re-measured through the
engine after:

| row | key | before | after |
| --- | --- | --- | --- |
| `nam-cityofchicago-chicago-energy-benchmarking` | `id+data_year` | 8,000 / 3,915 | **8,000 / 8,000** |
| `nam-la-crime-recent` | `dr_no` | 10,000 / 7,179 | **10,000 / 10,000** |
| `nam-austin-311` | `sr_number` | 10,000 / 7,384 | **10,000 / 10,000** |
| `nam-nyc-dob-permits` | `permit_si_no` | 10,000 / 7,470 | **10,000 / 10,000** |
| `nam-nyc-collisions-recent` | `collision_id` | 10,000 / 7,422 | 10,000 / 9,867 |
| `nam-chicago-food-inspections` | `inspection_id` | 10,000 / 7,778 | **10,000 / 10,000** |
| `nam-chicago-traffic-crashes` | `crash_record_id` | 10,000 / 8,106 | 10,000 / 9,999 |
| `nam-nyc-restaurant-inspections` | `camis+inspection_date+violation_code` | 10,000 / 8,113 | 10,000 / 9,996 |
| `nam-chicago-311-requests` | `sr_number` | 10,000 / 8,511 | **10,000 / 10,000** |
| `nam-sonomacounty-…-animal-shelter` | `impound_number` | 8,000 / 7,680 | **8,000 / 8,000** |
| `nam-seattle-building-permits` | `permitnum` | 10,000 / 9,782 | **10,000 / 10,000** |
| `nam-sf-311-cases`, `nam-sf-building-permits`, `nam-sf-fire-calls` | `service_request_id`, `record_id`, `rowid` | 73 / 60 / 2 lost | **0 lost** |
| `nam-seattle-police-reports` | `offense_id` | 10,000 / 9,984 | **10,000 / 10,000** |
| `nam-nyc-311-open`, `nam-nyc-noise-complaints` | `unique_key` | 7 / 1 lost | 1 / 0 lost |
| `nam-delaware-storm-water-notices-of-intent` | `permitnumber` | 2 lost | 2 lost (unchanged count, the honest key) |
| `nam-cityofnewyork-street-closures-…` | `uniqueid` | 4,343 / 4,152 | 4,343 / 4,152 — identical, and 4,152 is the sample's whole-record distinctness, so the remainder is the upstream's own duplicate. Kept because a closure that changes should UPDATE its row rather than appear twice |

**53,281 records a run across the three families.**

Three findings from this pass, and two of them cut against the other two
families:

* **TWO PAGES CAN BE TOO FEW.** `id` is distinct on all 800 sampled Chicago
  energy-benchmarking records and collapses 4,500 of 8,000 in the engine,
  because the first 800 records are all `data_year` 2019 and the dimension that
  breaks the key does not appear until page four. `id+data_year` stores 8,000 of
  8,000. A sample that does not reach the breaking dimension reads exactly like
  a unique key.
* **THE CONTENT-HASH FALLBACK CAN BEAT A REAL ID, and four rows prove it.**
  Measured and then REVERTED: `nam-chicago-crimes-recent` (`case_number`: 6
  collisions against the fallback's 2 — CPD files several offences under one
  case), `nam-chicago-towed-vehicles` (`inventory_number` 140 against 78),
  `nam-norfolk-streetlights-and-outages-reports` (`grid_address+fixture_style`
  168 against 155) and `nam-providenceri-providence-tree-dataset` (a four-part
  address composite 52 against 4). Where a record has no stable identity,
  VJSON's fallback hash of (title, link, date) separates rows that a weak "real
  id" merges. So compare the proposed key against the row's CURRENT loss and
  keep the fallback when it wins — the opposite of what Catalonia and Colombia
  needed, from the same starting symptom. All four reverts were re-measured and
  restore the original counts exactly.
* **And the minimal-key search offered `shape_len` (a geometry LENGTH),
  `offense_date` (a timestamp) and `link` (a URL)** as "unique" keys on three
  of these datasets. Each is the rule this file already states, in a new place:
  the minimal key a search finds is not the record's identity.

Nine of the 32 keep their fallback deliberately, each with the arithmetic:
Delaware's school directory (a "real" key reaches 55% loss against the
fallback's 9%), Delaware's septic permits (4% against 2%), Baton Rouge's
occupancy register (20% against 0.9%), Norfolk's railroad and storm registers,
Providence's tree inventory, Gainesville's reclaimed water, Nova Scotia's road
geometry (no id field at all) and Oakland's parking zones — whose 742 rows hold
**17 distinct whole records**, one per polygon part, so 17 is the correct count
and not a loss.

**The sweep was killed by a container worker restart at 3,343 rows** (2026-10-22
10:32) and resumes with `--resume`, which skips what is already in the TSV. That
is worth writing down as the shape of this job rather than as a result: a
two-day sweep in a container that restarts will be interrupted, so the file is
committed as it accumulates and the coverage is stated every time. Nothing
depends on it finishing — every scheduled run files its own
`collector-fetch-failure`, so a source's page gains its reason and remedy as the
scheduler reaches it, and the sweep only front-loads that.

**And eight of the 161 EMITS_NOTHING rows were the TOOL's own confusion.**
`_camera_refframe`, `_image_embed`, `camera-stills`, `media-analyze`,
`_entity_vec`, `embedding-backfill`, `translate`, `_svec_index` are maintenance
pods: they write their own tables and nothing to `intel_items`, by design, so
asking them whether they emit records is exactly the mistake the EMITS_NOTHING
verdict exists to expose — a verdict about a source that was never asked the
question. `audit_registry_emit.py` now reports a collector whose name begins
with `_` (the convention `source.h` documents) as **POD** and does not run it:
19 of them in the registry, listed rather than dropped so nothing registered
hides, and never counted as a finding. **The committed TSV was produced by the
tool BEFORE that fix**, so its `_`-collector rows still read EMITS_NOTHING;
read them as POD.

### E5. 2.3% of the KKJ procurement corpus carries no prefecture code, and nothing reads it

Found while settling C1 (2026-10-22). The 47 `JP25_JPBIZ_KKJ_PREF_*` rows slice
the portal by `LG_Code` 01..47 and were taken to read the corpus exhaustively.
They read 97.7% of it: 清掃 answers **62,308** hits, the same query summed over
all 47 codes answers **60,902**. The missing 1,406 are national bodies —
日本中央競馬会, 林野庁, the national research institutes, 科学技術振興機構 — whose
records carry an empty `LgCode`.

No filter for them was found. `LG_Code` = 00, 0, 000000, 48 and 99 each answer 0
hits; `Category` (工事/物品/役務) is refused unless another parameter is present;
`CFT_Issue_Date` alone is refused. `Organization_Name` does substring-match
(`省` → 211,575 hits, `庁` → 71,889), so a handful of broad rows could in
principle enumerate the national space — but a single agency already overflows
the cap (`林野庁` + 2026 = 4,771 against 1,000), and choosing the strings would
be a guess about coverage that nothing here can verify. The API guide
(`kkj.go.jp/doc/ja/api_guide.pdf`, 334 KB) is CID-encoded, so its parameter list
could not be read with what this container has; reading it is the next step and
it is cheap.

### E6. CLOSED — Slovenia's cadastre WFS never moved; the 404 was one reading

**This item was wrong, and the way it was wrong is the finding.** It recorded
that `ipi.eprostor.gov.si/wfs-si-gurs-kn` answers 404 to `GetCapabilities`
while `wfs-si-gurs-rpe` on the same host answers 200, concluded the service had
been withdrawn, and set about looking for its new path. Re-asked on 2026-10-22
— the same day — that URL answers **HTTP 200 with 118,420 bytes** of
capabilities listing 20 feature types, and both rows store:

| row | measured 2026-10-22 |
| --- | --- |
| `SI_GURS_CADASTRAL_MUN` | 200, 12,913,325 bytes, 1,000 features, `numberMatched` **2,698** |
| `SI_GURS_HOUSE_NUMBERS` | 200, 496,316 bytes, 1,000 features, `numberMatched` **580,317** (580,213 on 2026-10-12, so it grows) |

Main reached the same conclusion independently, from the other end: its own
cycle-stall sweep listed this row's 404 as transient and re-read 2,698 of
2,698. Two sessions, two readings, one answer — and the item that said
"withdrawn" rested on exactly one request. **A 404 looks exactly like a
withdrawn service, and this tree already knows that from the FDSN side**, where
a node that does not run `event/1` and a node with an empty catalogue answer the
identical status and are told apart only by the body. The rule applies in this
direction too: a transient 404 looks exactly like a retirement, and the remedy
is the same one the batch-49 retry pass and the TLS re-probe both used — ask
again a day later before writing anything down.

Looking properly then found a defect the "dead service" framing had hidden.
`SI_GURS_HOUSE_NUMBERS` declared `id_keys = "properties.HS_MID"` and **this
layer has no `HS_MID`**: its properties are `FEATUREID`,
`EID_HISNA_STEVILKA`, `HS_STEVILKA`, `HS_DODATEK`, `EID_STAVBA`,
`EID_NASELJE`, `EID_ULICA`, `TIP_TABLICE`, `EID_POSTNI_OKOLIS`, `DATUM_SYS`,
`ST_HS`. The declared key matched nothing on all 580,317 records, so the engine
fell back to the first non-empty scalar — which happens to be `FEATUREID`, the
right answer, reached by luck. The fallback follows field ORDER, so a layer
re-ordered upstream would have silently re-keyed the whole address register.
Named explicitly now, with `sortBy=FEATUREID` added for the stable-order rule:
three consecutive pages of 1,000 are 3,000 of 3,000 distinct both sorted and
unsorted, so no skip is demonstrable here over three pages, but this row cycles
581 pages across many runs and the sibling layer on this same service measured
473 re-served and 473 lost unsorted. **A row cannot be audited while it is
believed to be dead** — which is the argument for the standing decision to keep
and disclose rather than remove, made from the other side.

---

## The re-run finished — `docs/page-two-final-2026-10-20.tsv`

**654 of the 818 (80%) are healthy** — 350 OK, 304 PAGE1_IS_ALL — where the
first run had written every one of them off. That is the measure of what the
saturated egress and the strict TLS context were doing.

Findings acted on: `LUCHTMEETNET_LKI` 100 -> **604** (its bare path 302s and the
redirect drops the page parameter; its `id_keys` was wrong too, and invisible
while it read one page) and `JO34_CKANRES_DATENREGISTER_BERLIN_DE` 2,000 ->
**2,627**. `UKPARL_LDA_SESSIONS` was benign but running it surfaced a DEAD
`id_keys` matching 0 of 12 records, now keyed on `_about`. Three more were
complete as they stood.

**73 rows remain LOCAL_EGRESS** — unmeasured from here, which is item D2, not a
defect in those rows.
