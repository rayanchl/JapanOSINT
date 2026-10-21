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
| `EU_ECB_SUPERVISED_ENTITIES` | → weekly | the monthly workbook, sheet `SIs` (844 rows) |
| `EU_ECB_SUPERVISED_ENTITIES_LSI` | NEW | the same workbook, sheet `LSIs` (2,175) |

Five of those answers are not what the triage predicted, and the corrections
are the part worth keeping — all four generalise and are in CLAUDE.md:
`filter_query` on a server-filtered row discarding the upstream's own matches
(1 of 8 kept); `hp_register()` dropping a url-less row in SILENCE; the
dup-endpoint lint catching a "re-point" onto a URL the tree already read every
six hours; and an anchor COUNT being mistaken for the data on two separate
rows.

Still outstanding:

* **`FI_EDUSKUNTA_RECORDS` and `AT_PARLAMENT_RECORDS` need an engine
  affordance, not a path** — see item B2-bis below.
* `FR_ASSEMBLEE_NATIONALE` and `CZ_CUZK_CADASTRE` still name an API nobody has
  queried.
* the real register for the four that must not be re-pointed.
* **and then a decision that is the operator's**, for the rows with no
  server-rendered replacement reachable from here — now including
  `COPERNICUS_EMS_ACTIVATIONS`, whose "31 anchors" were chrome and whose only
  declared feed is two news items: keep them disclosing, or remove them. Both
  are defensible. **The faa.gov precedent that argued for keeping has since
  been overturned by its own author** (`BROWSER_HOSTS`, and two FAA rows are
  live again), so it no longer points one way. What is NOT in question: a
  browser profile is a remedy for a host that REFUSES us, and every row in
  this group answers 404 or renders client-side, so no agent reaches them.

### B2-bis. Two national parliaments publish a JSON TABLE the engine cannot key

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

### C1. KKJ's two entity pivots read 1,000 of 62,255

`JP25_JPBIZ_KKJ_QUERY` and `_ORG` are bounded at the portal's hard `Count` cap.
That is legitimate for a pivot and is now disclosed, and the order is
newest-first so the bound is a window rather than an arbitrary slice. But the
47 prefecture rows now read the same corpus exhaustively by date, so the pivots
could slice per entity the same way instead of bounding.

### C2. Toyama's dump days are unreachable by any means currently available

Single days of **1,449** and **1,161** notices against a per-read cap of 1,000,
measured day by day. No `window_days` fixes it — the cap is per READ. Reaching
them needs a different route (a bulk export, or a filter that subdivides within
a day). ~460 records.

### C3. Tokyo's KKJ backfill takes ~132 days

4-day windows, 530 of them, at a 6-hour cadence. Deliberate and disclosed, and
the notice says where it is. On the list so nobody reads it later as a stall.

### C4. Eight cycling rows still carry a declared descending sort

CLAUDE.md: a descending sort and a cycle do not compose, and the fix is an
ascending sort **verified per host**. 22 rows were done that way on 2026-10-18;
these 8 were deliberately left rather than changed on reasoning. Each needs its
own host checked.

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

### E1. Every embedding threshold is a guess from the literature

CLAUDE.md says it plainly: entity 0.80, story 0.85, alert 0.82, relevance, the
resolver margin — "**none has been measured on the production model**". They
were chosen from typical multilingual-e5 scores and tested against a stub.
Each one decides whether two entities merge, whether two items are one story,
and whether an alert fires.

### E2. One sortBy-less WFS row is unaccounted for

Of 16 other WFS `startIndex` rows without `sortBy`, CLAUDE.md records that "12
of the 13 reachable ones serve page 2 fine". The 13th is not named, and neither
are the 3 unreachable. Four rows with no recorded verdict.

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
