# HTML-mode reachability, October 2026

**What this is.** The October replay of the HTML engine (docs/audit-2026-10-02.md,
open item 6) compared 1,127 HTML rows and could not compare 88 of them: their
fetch failed at the transport level or with an HTTP error, before and after.
That list was not saved. This rebuilds it from scratch and classifies every
HTML row that fails or stores nothing today.

**How it was measured (2026-10-24/25).**

- **Rows:** every registered hpengine row with `mode = HP_HTML` on main at
  905eb9e2, read from the registry itself (`hp_spec_get()` over
  `registry_all()`), not from the table text: **1,079 rows**, 600 scheduled
  and 479 entity pivots. (The replay's 1,127 has shrunk through earlier
  removals.)
- **Runs:** each row once through the real binary, each run on a fresh copy
  of a warm scratch DB, with `tools/audit_registry_emit.py --ids-file …
  --entities … --jobs 4 --timeout 120`. `--entities` was added for this
  (923f6740): the 188 pivots with a manifest were asked the entity their
  manifest's probe URL carries; the 291 without one (the `hp2_*` and `hp_*`
  tables) a plain common term in their language — `Siemens`, `банк`, `東京`,
  `Petrobras`, `bank` and so on, listed per row in the run logs.
- **Second pass:** every row that failed or stored nothing (456) was run
  again the same way. **None recovered.** One row changed reading
  (`CA_ON_BUSINESS_REGISTRY`, 404 then a 200 bot-manager captcha).
- **Then by hand, per failure class:** DNS against 1.1.1.1 and 8.8.8.8, a TCP
  connect and TLS handshake, the page's bare path and the site root, the exact
  libcurl error from the engine's own TLS stack (Homebrew curl + OpenSSL 3)
  and from the macOS stack (`/usr/bin/curl`), and for every 403-class answer
  the honest agent variants the per-host table documents (engine agent,
  minus "collector", contact-only, bare `JapanOSINT/1.0`, the compat prefix,
  the RCorp feed agent). No browser agent was tried.

**The whole set.**

| reading | rows |
| --- | --- |
| OK (stored records) | 621 |
| SLOW but working (`JP25_JPNATGOV_JTSB_SHIP` 630 rows, `PYPI_SIMPLE_INDEX` 253,284 rows stored at the 120 s kill) | 2 |
| fetch failed, both passes — **the rebuilt "unreachable" list** | 296 |
| fetched a 200 and stored nothing, both passes | 160 |
| **total** | **1,079** |

The rebuilt unreachable list is 296 rows, not 88: a different day and a
different network. 268 of the 296 are entity pivots in the `hp2_*`/`hp_*`
tables or batch 31 (registered, not proven, per CLAUDE.md). Nothing below is
inferred from the replay.

## Classes

| class | what it means here | fetch failed | 200, stored nothing | total |
| --- | --- | --- | --- | --- |
| DEAD | host gone from DNS, or the page (and its bare path) 404/410 while the site answers | 130 | 4 | **134** |
| MOVED | redirect or obvious successor on the same site; re-pointed | — | 2 | **2** |
| SHAPE | answers 200 but not with anchors the row can use (client-rendered search, POST-only form, refused parameters, session-bound redirects, wrong mode) | 6 | 129 | **135** |
| BLOCKED | 403 or a challenge page for every honest agent, or a 200 bot wall | 93 | 8 | **101** |
| GEO/NETWORK | DNS resolves, TCP times out or is refused/reset from this network | 28 | — | **28** |
| TLS | the engine's OpenSSL refuses the server: incomplete chain, hostname mismatch, expired certificate, unclean close, legacy renegotiation, HTTP/2 stream reset | 25 | — | **25** |
| SERVER-ERROR | 5xx (or a 200 error page) to every agent on both passes | 6 | 2 | **8** |
| TRANSIENT | the site answers a maintenance page | 8 | 6 | **14** |
| EMPTY-OK | a working search with nothing for the probe entity; the same row stores records for another | — | 9 | **9** |
| | | **296** | **160** | **456** |

By table family: `hp2_*` 205 of these rows (51 DEAD), batch 31 168 (66 DEAD),
`hp_*` 48 (16 DEAD), and 35 across batches 19–30 (1 DEAD).

## What was done

**Fixed — re-pointed and measured through the engine** (2ab3e2ad; each run on
a fresh scratch DB, `stored == emitted`):

| row | before | after | change |
| --- | --- | --- | --- |
| `NRC_PART21_REPORTS_2026` | 0 | **41** | the old path 301s to `/regulations-legislation/…/part-21-reports/2026`, which links each report to its ADAMS PDF; `href_must=/docs/ML` |
| `JO28_JCG_TOPICS_ADVISORY_ARCHIVE` | 0 | **110** | posts are linked relatively now; `base` is the listing's directory, not the host root |
| `JP25_JPPOLICE_IBARAKI_KENKEI_OSHIRASE` | 0 | **6** | the force rebuilt its site; the notice list is `/kenkei/contents/information.html` (PDFs under `/kenkei/documents/`) |
| `NRC_EVENT_REPORTS_2026`, `_2025` | 0 | 0 | re-pointed to the page the old path 301s to, with the new day-link `href_must`; still 0 because the calendar labels each day `1`..`31` (see engine findings) |

**Not re-pointed although the site has a new search:** `US_NY_DFS_ENFORCEMENT`
(`/search/site?search=` stores 34 for "bank" — bank-holiday pages, FAQs and
pagination links, not enforcement actions) and `ICAO_SAFETY_AUDIT` /
`ICAO_SAFETY_AUDITS` (`/search?search_api_fulltext=` stores the language and
pagination links that echo the query: 6 records for an impossible entity).
A re-point that stores links echoing the question is rule 4d's confident wrong
answer, so these count as DEAD. `MD_COMPANY_REGISTRY`'s catalogue moved to
dataset.gov.md, which `MD_DATASET_GOV_CATALOGUE` already searches through the
CKAN API, so it is removed rather than duplicated.

**Removed — 134 DEAD rows** (abe2ccfb: 130; 4208150e: 4), registry 57,529 →
**57,395**, both floors re-recorded by `make source-floor-record
registry-floor-record` and the id diff against the sweep's binary is exactly
those 134. Evidence per row: `docs/removed-sources-2026-10-25-html-dead.tsv`;
batch 31's 66 also in the new `docs/rejected-sources-batch31.tsv`, batch 28's
one appended to `docs/rejected-sources-batch28.jpforgotten.tsv`.

| why dead | rows |
| --- | --- |
| page and bare path 404/410, site root alive | 103 |
| 403 to the default agent; 404 page and bare path once an honest agent is let in | 6 |
| 404 page and bare path past a certificate chain OpenSSL rejects (seen with the macOS stack) | 4 |
| a 200 page titled "404" / "not found" (AJPES, simap, WTO, courts.ie) | 4 |
| host no longer resolves (1.1.1.1 and 8.8.8.8), or redirects to one that does not | 14 |
| redirects to the site's own 404 page (thru.de ×2) | 2 |
| 308 to a bare IP that refuses :443 (the retired CIPO trademarks app) | 1 |

**Left registered**, as the brief says: GEO/NETWORK (28), TLS (25), BLOCKED
(101), SHAPE (134 that still store nothing, the two NRC calendars among
them), SERVER-ERROR (8), TRANSIENT (14), EMPTY-OK (9).
Every fetch failure among them already files a `collector-fetch-failure`
notice per run.

## BLOCKED hosts an honest agent would let in (not edited here)

Measured with the engine's own TLS stack (Homebrew curl + OpenSSL 3); the
host table `core/host_agents.def` is being edited in another session, so
nothing was added. Statuses per agent: engine / no-"collector" / contact /
bare `JapanOSINT/1.0` / compat / RCorp feed.

| host | row | statuses | worth an entry? |
| --- | --- | --- | --- |
| `www.kansai-airport.or.jp` | `JPC_KANSAI_AIRPORT_FLIGHTS_HTML` (scheduled) | 403 / 403 / **200** / **200** / 403 / 403 — 31 KB, 257 anchors | yes: `HA_CONTACT` |
| `aplikacja.ceidg.gov.pl` | `PL_CEIDG_SOLE_TRADERS` | 403 / 403 / **200** / **200** / 403 / 403 — 12 KB, 33 anchors | yes: `HA_CONTACT` |
| `www.allabolag.se` | `SE_ALLABOLAG_SEARCH` | 403 / 403 / 403 / 403 / **200** / 403 — 74 KB, 257 anchors | yes: `HA_COMPAT` only |
| `www.fcc.gov` | `US_FCC_ENFORCEMENT` | 403 / 403 / **200** / **200** / 403 / 403 — 12 KB, 181 anchors | the agent passes, but the row searches after `#q=` (client-side), so expect few hits |
| `divulgacandcontas.tse.jus.br` | `BR_TSE_CANDIDATES`, `BR_TSE_DONATIONS` | 403 / 403 / **200** / **200** / 403 / 403 — 5 KB, 0 anchors | no: an Angular app behind `#/`; still SHAPE |
| `www.planelogger.com` | `OFFSHORE_AIRCRAFT_REGISTRIES` | 403 / **200** / **200** / **200** / **200** / 403 — redirected to `/Account/Login` | no: the search needs an account |
| `sti.gov.kg` | `KG_TAX_REGISTRY` | timeout / timeout / timeout / **200** / **200** / 403 — 456 B, 0 anchors | no: an empty shell |

Two related observations. `www.adb.org` (`ADB_SANCTIONS_LIST`) answers the
macOS stack 200 for every agent and the engine's stack 403 for every agent:
Cloudflare is scoring the TLS client, not the User-Agent, so no agent entry
can help. And `www.phmsa.dot.gov` answers 403 without `Accept-Encoding` and
404 with it.

## Engine findings (for the engine's owner; `lib/hpengine.{c,h}` untouched)

1. **An unclean TLS close discards a delivered page.** Five rows
   (`SK_ORSR_EXTRACT` with 62 anchors, the EU Safety Gate, RASFF and FTS
   pages, `AU_LOBBYIST_REGISTER`) receive HTTP 200 and the whole body, then
   the server closes without `close_notify`; OpenSSL 3 reports "unexpected eof
   while reading" (curl 56) and the engine treats the transfer as a transport
   failure. `SSL_OP_IGNORE_UNEXPECTED_EOF` is the usual remedy, with the
   truncation risk it implies.
2. **Incomplete certificate chains: 16 rows' hosts** send only their leaf
   certificate. The macOS stack completes the chain and gets an answer;
   OpenSSL does not (curl 60). Four were 404 behind it and are removed; the
   rest stay as TLS.
3. **Anchors labelled under three characters are skipped** when nothing beside
   them names them, so NRC's event-report calendar (links labelled `1`..`31`)
   yields nothing to two correctly pointed rows.
4. **`filter_query` folds case in ASCII only.** `UZ_OPENINFO_COMPANY` stores
   nothing for `Банк` and 6 for `BANK`; any Cyrillic, Greek or accented
   entity typed in a different case than the page misses.
5. **No cookie jar.** Four session-bound searches (Bundesanzeiger, ISED
   spectrum, Missouri SOS, Vietnam's business registry) redirect in a loop
   past the 5-redirect limit.

## Per-row table

Every row that failed or stored nothing on both passes, by class. "engine:"
is the two passes' reading; the rest is the hand check. Actions: *remove*
(done, in the two commits above), *re-pointed* (2ab3e2ad), *leave*.

| row id | table (kind) | class | evidence | action |
| --- | --- | --- | --- | --- |
| `NRC_PART21_REPORTS_2026` | hp3b19_defence (scheduled) | MOVED | engine: 200, 0 stored, both passes. 0 -> 41 stored: old path 301s to /regulations-legislation/.../part-21-reports/2026, reports link to ADAMS PDFs; href_must=/docs/ML | re-pointed (2ab3e2ad) |
| `JP25_JPPOLICE_IBARAKI_KENKEI_OSHIRASE` | hp3b25_jppolice (scheduled) | MOVED | engine: 200, 0 stored, both passes. 0 -> 6 stored: oshirase.html redirects to the rebuilt /kenkei/ site; notices are PDFs on contents/information.html | re-pointed (2ab3e2ad) |
| `AM_COMPANY_REGISTRY` | hp2_asia_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 33577 B, 47 anchors; 'bank' is in the page text but in no link and a second entity (Ameria, LLC) stored nothing either: the results are not links carrying the term | leave |
| `GE_COMPANY_EXTRACT` | hp2_asia_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200 with an empty body | leave |
| `GE_PROCUREMENT_SPA` | hp2_asia_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 67368 B, 1 anchors; 'bank' is in the page text but in no link: the results are not links carrying the term | leave |
| `UZ_XARID_TENDERS` | hp2_asia_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 1952 B, 0 anchors, none naming 'банк', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CN_WENSHU_JUDGMENTS` | hp2_asia_east (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 13773 B, 10 anchors, none naming '华为', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `HK_HKEX_FILINGS` | hp2_asia_east (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 12684 B, 3 anchors, none naming 'HSBC', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `HK_SFC_LICENSEES` | hp2_asia_east (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 22869 B, 7 anchors, none naming 'HSBC', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `KR_COURT_REGISTRY` | hp2_asia_east (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 229 B, 0 anchors, none naming '삼성', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `LK_COMPANY_REGISTRY` | hp2_asia_south_sea (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 1997 B, 0 anchors, none naming 'Lanka', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `PH_PHILGEPS_TENDERS` | hp2_asia_south_sea (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 18935 B, 4 anchors, none naming 'Manila', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `TH_EGP_TENDERS` | hp2_asia_south_sea (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200 with an empty body | leave |
| `VN_MUASAMCONG_TENDERS` | hp2_asia_south_sea (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 254530 B, 117 anchors, none naming 'Viettel', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `BG_EOP_TENDERS` | hp2_eu_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 7493 B, 0 anchors, none naming 'Sofia', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `BG_TRADE_REGISTER` | hp2_eu_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 18304 B, 64 anchors, none naming 'Sofia', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `HR_EOJN_TENDERS` | hp2_eu_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 53259 B, 28 anchors, none naming 'Zagreb', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `HU_CEGJEGYZEK` | hp2_eu_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 2507 B, 0 anchors, none naming 'MOL', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `HU_EKR_TENDERS` | hp2_eu_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 216471 B, 0 anchors, none naming 'MOL', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `PL_KNF_WARNINGS` | hp2_eu_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 59998 B, 8 anchors, none naming 'Orlen', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `PL_VAT_WHITELIST_SEARCH` | hp2_eu_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 1513 B, 0 anchors, none naming 'Orlen', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `RO_SICAP_TENDERS` | hp2_eu_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 23940 B, 0 anchors, none naming 'Petrom', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `SI_ERAR_PAYMENTS` | hp2_eu_central (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 2021 B, 0 anchors, none naming 'Krka', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `AT_EDIKTSDATEI` | hp2_eu_dach (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 23544 B, 32 anchors; 'Siemens' is in the page text but in no link and a second entity (GmbH, Wien) stored nothing either: the results are not links carrying the term | leave |
| `AT_FIRMENBUCH_LOOKUP` | hp2_eu_dach (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 4385 B, 1 anchors, none naming 'Siemens', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `AT_FMA_REGISTER` | hp2_eu_dach (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 138668 B, 330 anchors, none naming 'Siemens', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CH_FINMA_AUTHORISED` | hp2_eu_dach (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 81882 B, 181 anchors, none naming 'Nestle', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CH_SECO_SANCTIONS` | hp2_eu_dach (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 7785 B, 0 anchors, none naming 'Nestle', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `DE_BUNDESANZEIGER_FILINGS` | hp2_eu_dach (pivot) | SHAPE | engine: fetch failed (transport), both passes. suchergebnis redirects to /pub/error in a loop; with a cookie jar it lands on /pub/error (200): the search needs a Wicket session the engine does not hold | leave |
| `DE_DPMA_REGISTER` | hp2_eu_dach (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 787745 B, 63 anchors, none naming 'Siemens', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `DE_RECHTSPRECHUNG` | hp2_eu_dach (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 15110 B, 16 anchors, none naming 'Siemens', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_COMPETITION_CASES` | hp2_eu_institutions (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 56823 B, 0 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_EBA_CREDIT_REGISTER` | hp2_eu_institutions (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 685 B, 0 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_EDES_EXCLUSIONS` | hp2_eu_institutions (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 152104 B, 182 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_ESMA_REGISTERS` | hp2_eu_institutions (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 174555 B, 10 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_ETS_INSTALLATIONS` | hp2_eu_institutions (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 26348 B, 0 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_STATE_AID_CASES` | hp2_eu_institutions (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 56823 B, 0 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_TRADE_DEFENCE_MEASURES` | hp2_eu_institutions (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 56579 B, 0 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `ES_BDNS_SUBSIDIES` | hp2_eu_south (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 1167 B, 0 anchors, none naming 'Telefonica', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `ES_BOE_GAZETTE` | hp2_eu_south (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 15781 B, 59 anchors, none naming 'Telefonica', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `ES_CNMV_ENTITIES` | hp2_eu_south (pivot) | SHAPE | engine: fetch failed 400, both passes. 400 to every agent: the server refuses the row's query parameters (an error page) | leave |
| `ES_PLACSP_CONTRACTS` | hp2_eu_south (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 95519 B, 52 anchors, none naming 'Telefonica', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `GR_PROMITHEUS_TENDERS` | hp2_eu_south (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 1902 B, 0 anchors, none naming 'Athens', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `PT_DRE_GAZETTE` | hp2_eu_south (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 2346 B, 0 anchors, none naming 'Galp', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `BE_MONITEUR_BELGE` | hp2_eu_west (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 13097 B, 12 anchors, none naming 'Solvay', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `BE_NBB_ACCOUNTS` | hp2_eu_west (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 5344 B, 0 anchors, none naming 'Solvay', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `FR_JUDILIBRE_DECISIONS` | hp2_eu_west (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 267 B, 0 anchors, none naming 'Total', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `FR_TRESOR_GELS_AVOIRS` | hp2_eu_west (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 153701 B, 5 anchors, none naming 'Total', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `IE_ETENDERS_NOTICES` | hp2_eu_west (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200 with an empty body | leave |
| `NL_AFM_REGISTERS` | hp2_eu_west (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 47603 B, 65 anchors, none naming 'Philips', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `NL_STAATSCOURANT` | hp2_eu_west (pivot) | SHAPE | engine: fetch failed 400, both passes. 400 to every agent: the server refuses the row's query parameters (an error page) | leave |
| `NL_TENDERNED_NOTICES` | hp2_eu_west (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 72303 B, 0 anchors, none naming 'Philips', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CA_SIMA_TRADE_MEASURES` | hp2_northam_ca_mx (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 25578 B, 85 anchors, none naming 'Bombardier', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `MX_PJF_SENTENCIAS` | hp2_northam_ca_mx (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 847 B, 0 anchors, none naming 'Pemex', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `MX_SAT_INCUMPLIDOS` | hp2_northam_ca_mx (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 4976 B, 0 anchors, none naming 'Pemex', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `MX_SIGER_RPC` | hp2_northam_ca_mx (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 14941 B, 12 anchors, none naming 'Pemex', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_CA_CALACCESS_FILINGS` | hp2_northam_states (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 1073 B, 0 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_CA_CSLB_CONTRACTOR` | hp2_northam_states (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 37195 B, 43 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_CA_MEDICAL_BOARD` | hp2_northam_states (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 47800 B, 17 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_DE_ENTITY_SEARCH` | hp2_northam_states (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 46593 B, 53 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_FL_DBPR_LICENCE` | hp2_northam_states (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 31474 B, 21 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_MI_LARA_LICENCE` | hp2_northam_states (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 174288 B, 14 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_NJ_BUSINESS_SEARCH` | hp2_northam_states (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 11335 B, 8 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_NV_SILVERFLUME` | hp2_northam_states (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 1048 B, 0 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_NY_LOBBYING_REGISTRY` | hp2_northam_states (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 36567 B, 19 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_TX_COMPTROLLER_ENTITY` | hp2_northam_states (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 87925 B, 255 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_WA_SOS_CORPS` | hp2_northam_states (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 6684 B, 0 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_NTSB_ACCIDENTS` | hp2_northam_us_reg (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 52604 B, 84 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `AR_COMPRAR_TENDERS` | hp2_southam_andes (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 159772 B, 24 anchors, none naming 'YPF', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `BO_SICOES_TENDERS` | hp2_southam_andes (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 32582 B, 40 anchors, none naming 'YPFB', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CL_CMF_ENTITIES` | hp2_southam_andes (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 122226 B, 190 anchors, none naming 'Codelco', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CL_PJUD_CASES` | hp2_southam_andes (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 44707 B, 31 anchors, none naming 'Codelco', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CO_RUES_COMPANY` | hp2_southam_andes (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 4304 B, 0 anchors, none naming 'Ecopetrol', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `BR_ANVISA_REGISTRATIONS` | hp2_southam_brazil (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 20081 B, 8 anchors, none naming 'Petrobras', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `BR_CADE_ANTITRUST` | hp2_southam_brazil (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 32808 B, 4 anchors, none naming 'Petrobras', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `BASEL_AML_INDEX` | hp2_world_critical (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 219125 B, 33 anchors, none naming 'Iran', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `IRS_FATCA_FFI_LIST` | hp2_world_critical (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 52419 B, 18 anchors; 'bank' is in the page text but in no link and a second entity (BANK, Bank) stored nothing either: the results are not links carrying the term | leave |
| `TRASE_SUPPLY_CHAINS` | hp2_world_critical (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 2852268 B, 20 anchors, none naming 'soy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `WHO_MEDICAL_ALERTS` | hp2_world_critical (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 77040 B, 152 anchors, none naming 'falsified', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `NRC_EVENT_REPORTS_2025` | hp3b19_defence (scheduled) | SHAPE | engine: 200, 0 stored, both passes. 200, 137 KB: same calendar page and 2-char day labels as 2026 | re-pointed (2ab3e2ad); still 0 — engine label rule |
| `NRC_EVENT_REPORTS_2026` | hp3b19_defence (scheduled) | SHAPE | engine: 200, 0 stored, both passes. 200, 130 KB: old path 301s to /event-notification-reports/2026; its day links are labelled '1'..'31' and the engine skips anchors labelled under 3 chars | re-pointed (2ab3e2ad); still 0 — engine label rule |
| `JO28_JCG_TOPICS_ADVISORY_ARCHIVE` | hp3b28_jppolice (scheduled) | SHAPE | engine: 200, 0 stored, both passes. 0 -> 110 stored: posts now linked relatively (post-991.html); base was the host root | base = listing directory (2ab3e2ad) |
| `ID_SATU_DATA_CATALOG` | hp3b31_apacgov (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 305398 B, 30 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `KR_ALIO_PUBLIC_INSTITUTIONS` | hp3b31_apacgov (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 18531 B, 3 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `PH_OPEN_DATA_CATALOG` | hp3b31_apacgov (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 7364 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CA_ISED_SPECTRUM_LICENCES` | hp3b31_assetreg (pivot) | SHAPE | engine: fetch failed (transport), both passes. sms-sgs.ic.gc.ca loops on ?execution=e1s1 (Spring Web Flow session) past 5 redirects | leave |
| `EE_LAND_BOARD_CADASTRE` | hp3b31_assetreg (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 1194 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_FISHING_FLEET_REGISTER` | hp3b31_assetreg (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 1147 B, 0 anchors, none naming 'Toyota', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `GLOBAL_FOREST_CONCESSIONS` | hp3b31_assetreg (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 36438 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CZ_NEN_PROCUREMENT` | hp3b31_eunat (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 301889 B, 138 anchors, none naming 'Toyota', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `DK_UDBUD_TENDERS` | hp3b31_eunat (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 908 B, 0 anchors, none naming 'Toyota', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `NO_DOFFIN_TENDERS` | hp3b31_eunat (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 1871 B, 0 anchors, none naming 'Toyota', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `PT_DIARIO_REPUBLICA` | hp3b31_eunat (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 2346 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_COMITOLOGY_REGISTER` | hp3b31_eutrans (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 1368 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_DOCUMENTS_REGISTER` | hp3b31_eutrans (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 55040 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_EFTA_SURVEILLANCE_CASES` | hp3b31_eutrans (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 16605 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_EUDAMED_DEVICES` | hp3b31_eutrans (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 48314 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_EXPERT_GROUPS_REGISTER` | hp3b31_eutrans (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 2036 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_FUNDING_TENDERS_PORTAL` | hp3b31_eutrans (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 59960 B, 1 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_HAVE_YOUR_SAY_FEEDBACK` | hp3b31_eutrans (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 3966 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_KOHESIO_PROJECTS` | hp3b31_eutrans (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 2651 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `IATI_AID_ACTIVITIES` | hp3b31_intlbody (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 3347 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `JP_NDL_SEARCH_API` | hp3b31_jpgov (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 362 KB of RSS 2.0 (OpenSearch) declared HP_HTML: no <a> anchors; the same endpoint is read as XML by hp3b23_jpregistry's any={q} row | leave (duplicate of a working XML row) |
| `JP_TDNET_TIMELY_DISCLOSURE` | hp3b31_jpgov (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 6836 B, 0 anchors, none naming '環境', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `BR_TCU_AUDIT_DECISIONS` | hp3b31_latamgov (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 22032 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CR_SICOP_PROCUREMENT` | hp3b31_latamgov (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 7266 B, 0 anchors, none naming 'Toyota', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `PA_PANAMACOMPRA` | hp3b31_latamgov (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 34772 B, 0 anchors, none naming 'Toyota', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `SV_COMPRASAL` | hp3b31_latamgov (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 35816 B, 0 anchors, none naming 'Toyota', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `RU_FEDRESURS_FACTS` | hp3b31_menacis (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 22106 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `SA_ETIMAD_TENDERS` | hp3b31_menacis (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 44592 B, 0 anchors, none naming 'Toyota', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `TR_EKAP_PROCUREMENT` | hp3b31_menacis (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 2820 B, 1 anchors, none naming 'Toyota', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CAM_DRIVEBC_BC` | hp3b31_survcam (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 4111 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CAM_EE_TARKTEE` | hp3b31_survcam (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 66252 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CAM_ES_DGT_TRAFFIC` | hp3b31_survcam (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 5042 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CAM_IS_VEGAGERDIN` | hp3b31_survcam (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 268188 B, 15 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `CAM_NO_VEGVESEN` | hp3b31_survcam (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 4595 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EEA_AIR_QUALITY_STATIONS` | hp3b31_survsensor (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 3628 B, 0 anchors, none naming 'energy', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_FPDS_CONTRACT_ACTIONS` | hp3b31_usfed (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 43308 B, 0 anchors, none naming 'Toyota', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_LA_SOS_BUSINESS` | hp3b31_usstate (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 18165 B, 2 anchors, none naming 'Toyota', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_MA_CORP_SEARCH` | hp3b31_usstate (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 923 B, 0 anchors, none naming 'Toyota', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `US_MO_SOS_BUSINESS` | hp3b31_usstate (pivot) | SHAPE | engine: fetch failed (transport), both passes. bsd.sos.mo.gov loops to /Common/SWCommonErrorPage.aspx (session-bound search) | leave |
| `HK_CR_ICRIS` | hp_apac_deep (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 30123 B, 0 anchors, none naming 'HSBC', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `MY_SSM_SEARCH` | hp_apac_deep (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 338442 B, 102 anchors, none naming 'Petronas', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `VN_BUSINESS_REGISTRY` | hp_apac_deep (pivot) | SHAPE | engine: fetch failed (transport), both passes. dichvuthongtin.dkkd.gov.vn 307-loops on itself past 5 redirects (cookie check) | leave |
| `EU_CURIA_PARTIES` | hp_eu_deep (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 130226 B, anchors=0, href_must document:0 matched: the listing is client-rendered | leave |
| `NL_RECHTSPRAAK_SEARCH` | hp_eu_deep (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 3892 B, anchors=0, href_must /details:0 matched: the listing is client-rendered | leave |
| `PARIS_MOU_INSPECTIONS` | hp_maritime_air (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 54631 B, 62 anchors, none naming 'Ever Given', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `USCG_PSIX_VESSEL` | hp_maritime_air (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 55489 B, 17 anchors, none naming 'Liberty', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `GE_OPENDATA_COMPANY` | hp_mena_africa_cis (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 18510 B, 4 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `RU_ARBITR_CASES` | hp_mena_africa_cis (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 3594 B, 3 anchors, none naming 'Газпром', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `FI_TENDERS_HILMA` | hp_nordic_baltic (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 9660 B, anchors=0, href_must /procurement/:0 matched: the listing is client-rendered | leave |
| `IS_FYRIRTAEKJASKRA` | hp_nordic_baltic (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 131899 B, 23 anchors; 'Icelandair' is in the page text but in no link and a second entity (ehf, Reykjavik) stored nothing either: the results are not links carrying the term | leave |
| `LV_UR_REGISTER` | hp_nordic_baltic (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 1450 B, 0 anchors, none naming 'Riga', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `SE_BOLAGSVERKET_NOTICES` | hp_nordic_baltic (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 49878 B, 0 anchors, none naming 'Volvo', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `EU_KOHESIO_BENEFICIARIES` | hp_procurement_aid (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 2651 B, 0 anchors, none naming 'bank', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `UNGM_NOTICES` | hp_procurement_aid (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 143220 B, anchors=218, href_must /Public/Notice/:0 matched: the listing is client-rendered | leave |
| `EUROPOL_MOST_WANTED` | hp_sanctions_deep (pivot) | SHAPE | engine: 200, 0 stored, both passes. 200, 14334 B, 6 anchors, none naming 'murder', and the term appears nowhere in the page text: the query is not applied server-side (search form or client-rendered app) | leave |
| `MD_COMPANY_REGISTRY` | hp2_asia_central (pivot) | DEAD | engine: fetch failed 404, both passes. date.gov.md/ckan/en/dataset 404 (bare path too, root 200); the catalogue moved to dataset.gov.md, which MD_DATASET_GOV_CATALOGUE already searches by entity through the CKAN API | remove |
| `CN_MOFCOM_SANCTIONS` | hp2_asia_east (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `JP_FSA_ACTIONS` | hp2_asia_east (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `KR_KIPRIS_PATENTS` | hp2_asia_east (pivot) | DEAD | engine: fetch failed (transport), both passes. engdipl.kipris.or.kr: 1.1.1.1 answers 127.0.0.1, 8.8.8.8 no A | remove |
| `TW_IMPORT_EXPORT` | hp2_asia_east (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `ID_OJK_LICENSEES` | hp2_asia_south_sea (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `IN_SEBI_INTERMEDIARIES` | hp2_asia_south_sea (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `RO_ANAF_TAXPAYERS` | hp2_eu_central (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `RO_ONRC_RECOM` | hp2_eu_central (pivot) | DEAD | engine: fetch failed (transport), both passes. portal.onrc.ro NXDOMAIN (1.1.1.1, 8.8.8.8) | remove |
| `SI_AJPES_REGISTRY` | hp2_eu_central (pivot) | DEAD | engine: 200, 0 stored, both passes. 200 titled '404 - AJPES' (bare path too) | remove |
| `AT_LOBBYREGISTER` | hp2_eu_dach (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `CH_SIMAP_TENDERS` | hp2_eu_dach (pivot) | DEAD | engine: 200, 0 stored, both passes. redirects to /en/oldSimapUrl 'Page not found' | remove |
| `DE_BAFIN_COMPANY_DB` | hp2_eu_dach (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `DE_PRTR_EMISSIONS` | hp2_eu_dach (pivot) | DEAD | engine: fetch failed (transport), both passes. thru.de/thrude/suche/ redirects to thru.de/404-2 (loop past 5 redirects), every agent | remove |
| `DE_UNTERNEHMENSREGISTER` | hp2_eu_dach (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `LI_HANDELSREGISTER` | hp2_eu_dach (pivot) | DEAD | engine: fetch failed (transport), both passes. www.oera.li SERVFAIL (1.1.1.1, 8.8.8.8); oera.li no A | remove |
| `EU_EIOPA_REGISTER` | hp2_eu_institutions (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `EU_TRANSPARENCY_REGISTER` | hp2_eu_institutions (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `CY_CYSEC_ENTITIES` | hp2_eu_south (pivot) | DEAD | engine: fetch failed 410, both passes. 410 in both passes; page 410; bare path 410; root 200 | remove |
| `ES_BORME_MERCANTIL` | hp2_eu_south (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `GR_GEMI_REGISTRY` | hp2_eu_south (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `IT_ANAC_CASELLARIO` | hp2_eu_south (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `IT_BANCA_ITALIA_ALBI` | hp2_eu_south (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `IT_REGISTRO_IMPRESE` | hp2_eu_south (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `PT_CMVM_ENTITIES` | hp2_eu_south (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `FR_AMF_DECISIONS` | hp2_eu_west (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `FR_REGAFI_INSTITUTIONS` | hp2_eu_west (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `LU_CSSF_ENTITIES` | hp2_eu_west (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `LU_RCS_ENTITIES` | hp2_eu_west (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `CA_AIRCRAFT_REGISTER` | hp2_northam_ca_mx (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `CA_CIPO_TRADEMARKS` | hp2_northam_ca_mx (pivot) | DEAD | engine: fetch failed (transport), both passes. www.ic.gc.ca/app/opic-cipo/trdmrks/srch/resultList.html 308s to https://88.166.116.130/ (a bare IP refusing :443); the old CIPO app is retired | remove |
| `CA_INELIGIBILITY_SUSPENSION` | hp2_northam_ca_mx (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `MX_CNBV_SUPERVISED` | hp2_northam_ca_mx (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `MX_COMPRANET_CONTRACTS` | hp2_northam_ca_mx (pivot) | DEAD | engine: fetch failed (transport), both passes. compranetinfo.hacienda.gob.mx NXDOMAIN | remove |
| `MX_RUG_LIENS` | hp2_northam_ca_mx (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `MX_SFP_SANCIONADOS` | hp2_northam_ca_mx (pivot) | DEAD | engine: fetch failed (transport), both passes. directoriosancionados.funcionpublica.gob.mx NXDOMAIN | remove |
| `US_CA_DIR_JUDGMENTS` | hp2_northam_states (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `US_NY_DFS_ENFORCEMENT` | hp2_northam_states (pivot) | DEAD | engine: fetch failed 404, both passes. /search?search_api_fulltext= 404 (bare path too, root 200). The site's new /search/site?search= stores 34 for 'bank' but they are generic site pages (bank holidays, FAQ) and pagination links, not enforcement actions: not re-pointed | remove |
| `US_TX_ETHICS_FILINGS` | hp2_northam_states (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `US_CFTC_ACTIONS` | hp2_northam_us_reg (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `US_FTC_CASES` | hp2_northam_us_reg (pivot) | DEAD | engine: fetch failed 404, both passes. www.ftc.gov is HA_CONTACT in the table: under that agent /search?search= is 404 in both passes (the default agent gets 403) | remove |
| `US_PHMSA_PIPELINE` | hp2_northam_us_reg (pivot) | DEAD | engine: fetch failed 403, both passes. 403 to the default agent; under contact the page and its bare path are 404, root 200 | remove |
| `CO_PROCURADURIA_ANTECEDENTES` | hp2_southam_andes (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `EC_SUPERCIAS_COMPANY` | hp2_southam_andes (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `UY_DGI_RUT` | hp2_southam_andes (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `BR_ANP_FUEL_LICENCES` | hp2_southam_brazil (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `BR_JUCESP_COMPANY` | hp2_southam_brazil (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `BR_SLAVE_LABOUR_LIST` | hp2_southam_brazil (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `FINCEN_ENFORCEMENT` | hp2_world_critical (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `ICAO_SAFETY_AUDIT` | hp2_world_critical (pivot) | DEAD | engine: fetch failed 404, both passes. /Pages/Search.aspx 404 (bare path too, root 200). The site's new /search?search_api_fulltext= answers, but measured through the engine it stores language and pagination links that echo the query (6 for an impossible entity), not audit records: not re-pointed | remove |
| `UN_VENDOR_INELIGIBILITY` | hp2_world_critical (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `JO28_KOKUGAKUIN_DMUSEUM_INDEX` | hp3b28_jpforgotten (scheduled) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path same; root 200 | remove |
| `AFRICA_RESOURCE_PROJECTS` | hp3b31_afrgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `KE_OPEN_DATA` | hp3b31_afrgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 404 | remove |
| `RW_RDB_REGISTRY` | hp3b31_afrgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `TZ_BRELA_ENTITY` | hp3b31_afrgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `ZA_CIPC_ENTERPRISE` | hp3b31_afrgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `ZA_GOVERNMENT_GAZETTE` | hp3b31_afrgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `ZA_MUNICIPAL_MONEY` | hp3b31_afrgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `AU_AUSTENDER_CONTRACTS` | hp3b31_apacgov (pivot) | DEAD | engine: fetch failed 403, both passes. 403 to the default agent; under compat the page and its bare path are 404, root 200 | remove |
| `HK_DATA_GOV_CATALOG` | hp3b31_apacgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `HK_GLD_TENDER_AWARDS` | hp3b31_apacgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `NZ_PARLIAMENT_PECUNIARY` | hp3b31_apacgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `AU_ACMA_SPECTRUM_LICENCES` | hp3b31_assetreg (pivot) | DEAD | engine: fetch failed (transport), both passes. web.acma.gov.au NOERROR with no A record | remove |
| `AU_MINING_TENEMENTS` | hp3b31_assetreg (pivot) | DEAD | engine: fetch failed (transport), both passes. minedexext.dmirs.wa.gov.au NXDOMAIN | remove |
| `CA_VESSEL_REGISTRY` | hp3b31_assetreg (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `CZ_CUZK_CADASTRE` | hp3b31_assetreg (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `ES_CATASTRO_PARCELS` | hp3b31_assetreg (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `EU_INDUSTRIAL_EMISSIONS_PORTAL` | hp3b31_assetreg (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `INDIAN_OCEAN_MOU_PSC` | hp3b31_assetreg (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `ITU_SPACE_NETWORKS` | hp3b31_assetreg (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `TOKYO_MOU_INSPECTIONS` | hp3b31_assetreg (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `UK_CAA_GINFO_REGISTER` | hp3b31_assetreg (pivot) | DEAD | engine: fetch failed (transport), both passes. siteapps.caa.co.uk NXDOMAIN (G-INFO now a client-side search at www.caa.co.uk) | remove |
| `AT_PARLAMENT_RECORDS` | hp3b31_eunat (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `DE_BUNDESKARTELLAMT_DECISIONS` | hp3b31_eunat (pivot) | DEAD | engine: fetch failed 403, both passes. 403 to the default agent; under nocollector the page and its bare path are 404, root 200 | remove |
| `DE_PRTR_FACILITIES` | hp3b31_eunat (pivot) | DEAD | engine: fetch failed (transport), both passes. same thru.de/thrude/suche/ path as DE_PRTR_EMISSIONS: redirects to thru.de/404-2 | remove |
| `ES_PORTAL_TRANSPARENCIA` | hp3b31_eunat (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `FI_EDUSKUNTA_RECORDS` | hp3b31_eunat (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `FR_ASSEMBLEE_NATIONALE` | hp3b31_eunat (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `IT_OPENCOESIONE_PROJECTS` | hp3b31_eunat (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `NL_TWEEDE_KAMER_OPENDATA` | hp3b31_eunat (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `NO_STORTINGET_DATA` | hp3b31_eunat (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `EU_CHAFEA_HEALTH_PROGRAMME` | hp3b31_eutrans (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `EU_COMMISSION_MEETINGS` | hp3b31_eutrans (pivot) | DEAD | engine: fetch failed 404, both passes. transparencyinitiative/… redirects to transparency-initiative/meetings/meeting.do, 404 with the query and 400 bare | remove |
| `EU_ECB_SUPERVISED_ENTITIES` | hp3b31_eutrans (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `EU_EIOPA_PENSION_REGISTER` | hp3b31_eutrans (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `EU_INTEGRITY_WATCH` | hp3b31_eutrans (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `EU_OLAF_INVESTIGATION_REPORTS` | hp3b31_eutrans (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `APG_FATF_STYLE_BODIES` | hp3b31_intlbody (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `ICAO_SAFETY_AUDITS` | hp3b31_intlbody (pivot) | DEAD | engine: fetch failed 404, both passes. /search/pages/results.aspx 404 (bare path too, root 200); same successor problem as ICAO_SAFETY_AUDIT | remove |
| `IFC_PROJECT_DISCLOSURES` | hp3b31_intlbody (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `OPCW_DECLARATIONS` | hp3b31_intlbody (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `PCA_ARBITRATION_CASES` | hp3b31_intlbody (pivot) | DEAD | engine: fetch failed 404, both passes. /en/cases/?s= redirects to /cases/?s= which is 404 (bare path 404, root 200); /en/cases/ is a landing page, the case database is on another host | remove |
| `WTO_DISPUTE_SETTLEMENT` | hp3b31_intlbody (pivot) | DEAD | engine: 200, 0 stored, both passes. redirects to /error/error_404.htm | remove |
| `JP_EGOV_PUBLIC_COMMENT` | hp3b31_jpgov (pivot) | DEAD | engine: fetch failed 403, both passes. 403 to the default agent; under contact the page and its bare path are 404, root 200 | remove |
| `JP_JFTC_ENFORCEMENT` | hp3b31_jpgov (pivot) | DEAD | engine: fetch failed 403, both passes. 403 to the default agent; under contact the page and its bare path are 404, root 200 | remove |
| `JP_KANPO_GAZETTE` | hp3b31_jpgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `JP_MHLW_LABOUR_VIOLATIONS` | hp3b31_jpgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `JP_MLIT_CONSTRUCTION_LICENCE` | hp3b31_jpgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `JP_PMDA_APPROVALS` | hp3b31_jpgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `BR_CGU_LENIENCY_AGREEMENTS` | hp3b31_latamgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `CL_INFOPROBIDAD_DECLARATIONS` | hp3b31_latamgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `CL_TRANSPARENCIA_ACTIVA` | hp3b31_latamgov (pivot) | DEAD | engine: fetch failed 403, both passes. 403 to the default agent; under compat the page and its bare path are 404, root 200 | remove |
| `MX_DECLARANET_ASSETS` | hp3b31_latamgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `MX_INE_FISCALIZACION` | hp3b31_latamgov (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `AM_ARLIS_LEGAL` | hp3b31_menacis (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `KW_KUWAIT_TENDERS` | hp3b31_menacis (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `QA_MONAQASAT_TENDERS` | hp3b31_menacis (pivot) | DEAD | engine: fetch failed (transport), both passes. incomplete certificate chain under OpenSSL; past it (macOS TLS stack) the page and its bare path are 404, root 200 | remove |
| `TN_OPEN_DATA_TENDERS` | hp3b31_menacis (pivot) | DEAD | engine: fetch failed (transport), both passes. incomplete certificate chain under OpenSSL; past it (macOS TLS stack) the page and its bare path are 404, root 200 | remove |
| `UA_OPENDATABOT_ENTITY` | hp3b31_menacis (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `CAM_CZ_RSD_KAMERY` | hp3b31_survcam (pivot) | DEAD | engine: fetch failed (transport), both passes. kamery.rsd.cz NXDOMAIN | remove |
| `COPERNICUS_EMS_ACTIVATIONS` | hp3b31_survsensor (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `JP_NRA_RADIATION_MONITORING` | hp3b31_survsensor (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `CH_OPENTRANSPORTDATA` | hp3b31_survtransport (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `EU_RAIL_RINF_INFRASTRUCTURE` | hp3b31_survtransport (pivot) | DEAD | engine: fetch failed (transport), both passes. rinf.era.europa.eu 301s to data-interop.era.europa.eu, which does not resolve | remove |
| `IE_LOBBYING_REGISTER` | hp3b31_ukie (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `US_AZ_CORP_COMMISSION` | hp3b31_usstate (pivot) | DEAD | engine: fetch failed (transport), both passes. ecorp.azcc.gov is a CNAME whose target ends in a space ('…azurefd.us\032.'), NXDOMAIN at 1.1.1.1/8.8.8.8 and to the system resolver; the redirect target arizonabusinesscenter.azcc.gov is a client-side app (47 KB, 0 anchors) | remove |
| `US_WI_DFI_CORP` | hp3b31_usstate (pivot) | DEAD | engine: fetch failed (transport), both passes. www.wdfi.org NXDOMAIN | remove |
| `CN_MIIT_ICP` | hp_apac_deep (pivot) | DEAD | engine: fetch failed 410, both passes. 410 in both passes; page 410; bare path same; root 410 | remove |
| `PH_SEC_ICOUNT` | hp_apac_deep (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `TH_DBD_SEARCH` | hp_apac_deep (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `IE_COURTS_SEARCH` | hp_courts_legal (pivot) | DEAD | engine: 200, 0 stored, both passes. redirects to /hubs/not-found | remove |
| `WIPO_UDRP_DECISIONS` | hp_courts_legal (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `PT_RCBE_LOOKUP` | hp_eu_deep (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `TC_CCARCS_REGISTRY` | hp_maritime_air (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `UK_GINFO_REGISTRY` | hp_maritime_air (pivot) | DEAD | engine: fetch failed (transport), both passes. siteapps.caa.co.uk NXDOMAIN (as UK_CAA_GINFO_REGISTER) | remove |
| `AE_ADGM_REGISTER` | hp_mena_africa_cis (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `KZ_ADATA_SEARCH` | hp_mena_africa_cis (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `RU_RUSPROFILE_SEARCH` | hp_mena_africa_cis (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `RW_DOMESTIC_REGISTRY` | hp_mena_africa_cis (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 200 | remove |
| `TR_MERSIS_GAZETTE` | hp_mena_africa_cis (pivot) | DEAD | engine: fetch failed (transport), both passes. incomplete certificate chain under OpenSSL; past it (macOS TLS stack) the page and its bare path are '404 Not Found', root 200 | remove |
| `ZA_CIPC_DISCLOSURE` | hp_mena_africa_cis (pivot) | DEAD | engine: fetch failed (transport), both passes. incomplete certificate chain under OpenSSL; past it (macOS TLS stack) the page and its bare path are 404, root 200 | remove |
| `LT_REKVIZITAI_SEARCH` | hp_nordic_baltic (pivot) | DEAD | engine: fetch failed 404, both passes. 404 in both passes; page 404; bare path 404; root 403 | remove |
| `UA_NAZK_SANCTIONS` | hp_sanctions_deep (pivot) | DEAD | engine: fetch failed (transport), both passes. sanctions.nazk.gov.ua NXDOMAIN | remove |
| `KG_TAX_REGISTRY` | hp2_asia_central (pivot) | BLOCKED | engine: fetch failed 403, both passes. www.sti.gov.kg 301 -> sti.gov.kg: engine/nocollector/contact time out or 403, rcorp 403; bare and compat 200 but 456 B, 0 anchors: would still be SHAPE | leave; honest agent listed for the host table (not edited here) |
| `CN_CREDIT_CHINA` | hp2_asia_east (pivot) | BLOCKED | engine: fetch failed 412, both passes. 412 to every honest agent (a JavaScript challenge page) | leave |
| `HK_LAND_REGISTRY` | hp2_asia_east (pivot) | BLOCKED | engine: 200, 0 stored, both passes. 200 but the body is a bot wall: 'Request Rejected' | leave |
| `ID_LPSE_TENDERS` | hp2_asia_south_sea (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `KH_LA_MM_REGISTRY` | hp2_asia_south_sea (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `MY_BURSA_ANNOUNCEMENTS` | hp2_asia_south_sea (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `MY_EPEROLEHAN_TENDERS` | hp2_asia_south_sea (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `TH_SEC_LICENSEES` | hp2_asia_south_sea (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `PL_CEIDG_SOLE_TRADERS` | hp2_eu_central (pivot) | BLOCKED | engine: fetch failed 403, both passes. aplikacja.ceidg.gov.pl: engine/nocollector/compat/rcorp 403; contact and bare JapanOSINT/1.0 200, 12 KB, 33 anchors | leave; honest agent listed for the host table (not edited here) |
| `PL_EZAMOWIENIA_TENDERS` | hp2_eu_central (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `DE_BAFIN_WARNINGS` | hp2_eu_dach (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `DE_INSOLVENZ_ANNOUNCEMENTS` | hp2_eu_dach (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `EU_ECHA_SUBSTANCES` | hp2_eu_institutions (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `EU_EFSA_OUTPUTS` | hp2_eu_institutions (pivot) | BLOCKED | engine: fetch failed 401, both passes. 401 'Security verification required' to every honest agent | leave |
| `IT_GAZZETTA_UFFICIALE` | hp2_eu_south (pivot) | BLOCKED | engine: 200, 0 stored, both passes. 200 but the body is a bot wall: 'Request Rejected' | leave |
| `MT_MBR_COMPANIES` | hp2_eu_south (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `CA_LOBBYING_REGISTRY` | hp2_northam_ca_mx (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `CA_ON_BUSINESS_REGISTRY` | hp2_northam_ca_mx (pivot) | BLOCKED | engine: pass 1 fetch failed 404; pass 2 200, 0 stored. pass 1: 404; pass 2 and curl: 200 'Radware Bot Manager Captcha' | leave |
| `CA_QC_REGISTRAIRE` | hp2_northam_ca_mx (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `US_CA_DFPI_ACTIONS` | hp2_northam_states (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `US_FL_SUNBIZ_ENTITY` | hp2_northam_states (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `US_IL_SOS_CORP` | hp2_northam_states (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `US_NY_UNIFIED_COURTS` | hp2_northam_states (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `US_OH_SOS_BUSINESS` | hp2_northam_states (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `US_FCC_ENFORCEMENT` | hp2_northam_us_reg (pivot) | BLOCKED | engine: fetch failed 403, both passes. www.fcc.gov: engine/nocollector/compat/rcorp 403; contact and bare 200, 12 KB, 181 anchors (the row's #q= search is client-side, so expect few hits) | leave; honest agent listed for the host table (not edited here) |
| `LATAM_MERCOSUR_MEASURES` | hp2_southam_andes (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `PE_SEACE_TENDERS` | hp2_southam_andes (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `UY_COMPRAS_ESTATALES` | hp2_southam_andes (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403,503 | leave |
| `VE_REGISTRY_SEARCH` | hp2_southam_andes (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `BR_IBAMA_CTF` | hp2_southam_brazil (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `BR_IBAMA_EMBARGOES` | hp2_southam_brazil (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `BR_JUSBRASIL_CASES` | hp2_southam_brazil (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `BR_SISCOMEX_TRADE` | hp2_southam_brazil (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `BR_TSE_CANDIDATES` | hp2_southam_brazil (pivot) | BLOCKED | engine: fetch failed 403, both passes. divulgacandcontas.tse.jus.br: engine/nocollector/compat/rcorp 403; contact and bare 200 but 5 KB, 0 anchors (an Angular app behind '#/'): would still be SHAPE | leave; honest agent listed for the host table (not edited here) |
| `BR_TSE_DONATIONS` | hp2_southam_brazil (pivot) | BLOCKED | engine: fetch failed 403, both passes. divulgacandcontas.tse.jus.br: as BR_TSE_CANDIDATES — contact/bare 200, 0 anchors: would still be SHAPE | leave; honest agent listed for the host table (not edited here) |
| `ADB_SANCTIONS_LIST` | hp2_world_critical (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `AFDB_DEBARMENT_LIST` | hp2_world_critical (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `FATF_MONITORED_JURISDICTIONS` | hp2_world_critical (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP_METI_END_USER_LIST` | hp2_world_critical (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JPC_KANSAI_AIRPORT_FLIGHTS_HTML` | hp3b23_jpcyber (scheduled) | BLOCKED | engine: fetch failed 403, both passes. www.kansai-airport.or.jp: engine/nocollector/compat/rcorp 403; contact and bare 200, 31 KB, 257 anchors | leave; honest agent listed for the host table (not edited here) |
| `JP25_JPBIZ_KANPOUAI_TOSAN` | hp3b25_jpbizreg (scheduled) | BLOCKED | engine: fetch failed 202, both passes. 202 with an empty body to every honest agent (a WAF challenge) | leave |
| `JP25_JPPOLICE_M47_MAIL_AICHI` | hp3b25_jppolice (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP25_JPPOLICE_M47_MAIL_CHIBA` | hp3b25_jppolice (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP25_JPPOLICE_M47_MAIL_FUKUOKA` | hp3b25_jppolice (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP25_JPPOLICE_M47_MAIL_HOKKAIDO` | hp3b25_jppolice (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP25_JPPOLICE_M47_MAIL_HYOGO` | hp3b25_jppolice (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP25_JPPOLICE_M47_MAIL_KANAGAWA` | hp3b25_jppolice (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP25_JPPOLICE_M47_MAIL_KYOTO` | hp3b25_jppolice (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP25_JPPOLICE_M47_MAIL_OKINAWA` | hp3b25_jppolice (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP25_JPPOLICE_M47_MAIL_OSAKA` | hp3b25_jppolice (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP25_JPPOLICE_M47_MAIL_SAITAMA` | hp3b25_jppolice (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP25_JPPOLICE_M47_MAIL_SHIZUOKA` | hp3b25_jppolice (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP25_JPPOLICE_M47_MAIL_TOKYO` | hp3b25_jppolice (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `JP25_JPREC_KANPOUAI_TOSAN_PAGED` | hp3b25_jprecovered (scheduled) | BLOCKED | engine: fetch failed 202, both passes. 202 with an empty body to every honest agent (a WAF challenge) | leave |
| `JO28_YOKOHAMA_VISITORS_EVENTS` | hp3b28_jpindustry (scheduled) | BLOCKED | engine: fetch failed 307, both passes. 307 'You are being redirected… Javascript is required' to every honest agent | leave |
| `JO30_CAM_YODOGAWA_LIVE` | hp3b30_jpcam (scheduled) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `AFRICA_SAFLII_JUDGMENTS` | hp3b31_afrgov (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `NG_CAC_COMPANY_SEARCH` | hp3b31_afrgov (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `AU_PARLINFO_HANSARD` | hp3b31_apacgov (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `ID_LKPP_BLACKLIST` | hp3b31_apacgov (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `ID_SUPREME_COURT_JUDGMENTS` | hp3b31_apacgov (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `IN_CPGRAMS_ENFORCEMENT` | hp3b31_apacgov (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `PACIFIC_ISLAND_REGISTRIES` | hp3b31_apacgov (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `GLOBAL_RESOURCE_CONTRACTS` | hp3b31_assetreg (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `OFFSHORE_AIRCRAFT_REGISTRIES` | hp3b31_assetreg (pivot) | BLOCKED | engine: fetch failed 403, both passes. www.planelogger.com: engine and rcorp 403 (the word 'collector'); nocollector/contact/bare/compat are let in and redirected to /Account/Login: the search needs an account, so an agent entry would not give records | leave; honest agent listed for the host table (not edited here) |
| `DK_FOLKETINGET_ODA` | hp3b31_eunat (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `IT_ANAC_CONTRACTS` | hp3b31_eunat (pivot) | BLOCKED | engine: 200, 0 stored, both passes. 200 but the body is a bot wall: 'Request Rejected' | leave |
| `EU_ACER_ENERGY_REMIT` | hp3b31_eutrans (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `EU_COUNCIL_PUBLIC_REGISTER` | hp3b31_eutrans (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `COE_GRECO_MONEYVAL` | hp3b31_intlbody (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `IAEA_SAFEGUARDS_RECORD` | hp3b31_intlbody (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `ISDB_PROJECT_DISCLOSURE` | hp3b31_intlbody (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `OECD_ANTIBRIBERY_REPORTS` | hp3b31_intlbody (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `UN_DIGITAL_LIBRARY` | hp3b31_intlbody (pivot) | BLOCKED | engine: fetch failed 202, both passes. 202 with an empty body to every honest agent (a WAF challenge) | leave |
| `BR_SENADO_ABERTO` | hp3b31_latamgov (pivot) | BLOCKED | engine: 200, 0 stored, both passes. 200 but the body is a bot wall: 'Verificação de segurança — Senado Federal' | leave |
| `CARIBBEAN_COMPANY_REGISTRIES` | hp3b31_latamgov (pivot) | BLOCKED | engine: 200, 0 stored, both passes. 200 but the body is a bot wall: 'Checking your browser - reCAPTCHA' | leave |
| `GT_GUATECOMPRAS` | hp3b31_latamgov (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `MX_PLATAFORMA_TRANSPARENCIA` | hp3b31_latamgov (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `AE_DIFC_PUBLIC_REGISTER` | hp3b31_menacis (pivot) | BLOCKED | engine: fetch failed 429, both passes. 429 'Vercel Security Checkpoint' to every honest agent | leave |
| `GE_MATSNE_GAZETTE` | hp3b31_menacis (pivot) | BLOCKED | engine: 200, 0 stored, both passes. 200 but the body is a bot wall: 'Access Denied' | leave |
| `OM_ISTITHMAAR_TENDERS` | hp3b31_menacis (pivot) | BLOCKED | engine: 200, 0 stored, both passes. 200 but the body is a bot wall: 'Request Rejected' | leave |
| `SMITHSONIAN_VOLCANOES` | hp3b31_survsensor (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `IE_CHARITIES_REGULATOR` | hp3b31_ukie (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `UK_WDTK_FOI_REQUESTS` | hp3b31_ukie (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `US_AK_CORP_DATABASE` | hp3b31_usstate (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `US_GA_SOS_BUSINESS` | hp3b31_usstate (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `US_NC_SOS_BUSINESS` | hp3b31_usstate (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `US_PA_BUSINESS_SEARCH` | hp3b31_usstate (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `US_UT_BUSINESS_SEARCH` | hp3b31_usstate (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `IN_KANOON_SEARCH` | hp_apac_deep (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `IN_ZAUBA_CORP` | hp_apac_deep (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `AUSTLII_SEARCH` | hp_courts_legal (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `CANLII_SEARCH` | hp_courts_legal (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `NZ_LEGAL_SEARCH` | hp_courts_legal (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `EU_EURLEX_SEARCH` | hp_eu_deep (pivot) | BLOCKED | engine: fetch failed 202, both passes. 202 with an empty body to every honest agent (a WAF challenge) | leave |
| `AE_DED_LICENCE` | hp_mena_africa_cis (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `AFRICA_OPENOWNERSHIP` | hp_mena_africa_cis (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `NG_CAC_SEARCH` | hp_mena_africa_cis (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `UA_CLARITY_PROJECT` | hp_mena_africa_cis (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `ZA_SAFLII_JUDGMENTS` | hp_mena_africa_cis (pivot) | BLOCKED | engine: fetch failed 403, both passes. 403 in both passes; every honest agent (engine, nocollector, contact, bare, compat, rcorp) 403 | leave |
| `SE_ALLABOLAG_SEARCH` | hp_nordic_baltic (pivot) | BLOCKED | engine: fetch failed 403, both passes. www.allabolag.se: every JapanOSINT agent 403; compat (Mozilla/5.0 (compatible; JapanOSINT/1.0; …)) 200, 74 KB, 257 anchors | leave; honest agent listed for the host table (not edited here) |
| `AZ_TAX_ENTITIES` | hp2_asia_central (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. www.e-taxes.gov.az resolves (94.20.63.38); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `BY_UNP_REGISTRY` | hp2_asia_central (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. egr.gov.by resolves (134.17.213.114); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `CIS_INTERPOL_WANTED` | hp2_asia_central (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. DNS 195.50.7.35, TCP+TLS ok, then no byte in 30 s (both TLS stacks) | leave |
| `JP_NITE_RECALLS` | hp2_asia_east (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. www.jiko.nite.go.jp resolves (202.241.100.30); TCP connect fails from here ([Errno 51] Network is unreachable) | leave |
| `TW_JUDICIAL_JUDGMENTS` | hp2_asia_east (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. judgment.judicial.gov.tw resolves (210.69.124.167); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `BD_RJSC_ENTITY` | hp2_asia_south_sea (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. app.roc.gov.bd resolves (123.49.32.37); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `IN_ECOURTS_CASES` | hp2_asia_south_sea (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. services.ecourts.gov.in resolves (103.195.217.34); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `IN_EPFO_ESTABLISHMENT` | hp2_asia_south_sea (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. unifiedportal-epfo.epfindia.gov.in resolves (103.194.45.3); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `IN_IP_INDIA_PATENTS` | hp2_asia_south_sea (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. ipindiaservices.gov.in resolves (103.208.60.22); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `NP_OCR_COMPANY` | hp2_asia_south_sea (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. www.ocr.gov.np resolves (110.34.25.94); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `PK_SECP_COMPANY` | hp2_asia_south_sea (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. eservices.secp.gov.pk resolves (110.38.234.12); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `CZ_CNB_REGISTER` | hp2_eu_central (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. apl.cnb.cz resolves (193.84.159.153); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `US_NLRB_CASES` | hp2_northam_us_reg (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. www.nlrb.gov resolves (20.41.44.121); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `AR_BORA_GAZETTE` | hp2_southam_andes (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. www.boletinoficial.gob.ar resolves (200.108.150.10); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `BR_INPI_TRADEMARKS` | hp2_southam_brazil (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. busca.inpi.gov.br resolves (168.90.175.139); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `BR_MINING_CADASTRE` | hp2_southam_brazil (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. sistemas.anm.gov.br resolves (200.198.193.234); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `JP25_JPFIN_DAINICHIGINKYO_STATS` | hp3b25_jpfinance (scheduled) | GEO/NETWORK | engine: fetch failed (transport), both passes. www.dainichiginkyo.or.jp resolves (157.205.209.37); TCP connect fails from here ([Errno 61] Connection refused) | leave |
| `JO28_CHIBA_VMA_HOSPITAL_CHIBA` | hp3b28_jpforgotten (scheduled) | GEO/NETWORK | engine: fetch failed (transport), both passes. www.cpvma.com resolves (161.34.25.159); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `SN_MARCHES_PUBLICS` | hp3b31_afrgov (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. www.marchespublics.sn resolves (213.213.213.213); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `PE_INFOBRAS_PUBLIC_WORKS` | hp3b31_latamgov (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. apps.contraloria.gob.pe resolves (161.132.147.156); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `MA_MARCHES_PUBLICS` | hp3b31_menacis (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. www.marchespublics.gov.ma resolves (196.12.235.214); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `RU_ZAKUPKI_PROCUREMENT` | hp3b31_menacis (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. zakupki.gov.ru resolves (95.167.245.92); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `UA_EDR_MINJUST` | hp3b31_menacis (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. usr.minjust.gov.ua resolves (193.111.173.248); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `CAM_TW_FREEWAY_CCTV` | hp3b31_survcam (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. tisvcloud.freeway.gov.tw resolves (210.241.139.76); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `US_NCUA_CREDIT_UNIONS` | hp3b31_usfed (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. mapping.ncua.gov resolves (216.69.120.115); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `US_MN_SOS_BUSINESS` | hp3b31_usstate (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. DNS 156.98.17.58, TCP+TLS ok, then 'Connection reset by peer' under both TLS stacks | leave |
| `US_TN_SOS_BUSINESS` | hp3b31_usstate (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. tnbear.tn.gov resolves (170.141.168.172); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `ID_AHU_SEARCH` | hp_apac_deep (pivot) | GEO/NETWORK | engine: fetch failed (transport), both passes. ahu.go.id resolves (103.200.129.129); TCP connect to the port times out from here (python socket 8 s, curl 30 s, both TLS stacks) | leave |
| `TJ_COMPANY_REGISTRY` | hp2_asia_central (pivot) | TLS | engine: fetch failed (transport), both passes. andoz.tj: incomplete certificate chain under OpenSSL (the engine's stack); the macOS TLS stack gets HTTP 200 | leave |
| `RS_APR_REGISTRY` | hp2_eu_central (pivot) | TLS | engine: fetch failed (transport), both passes. pretraga2.apr.gov.rs: incomplete certificate chain under OpenSSL; past it the macOS TLS stack gets HTTP 500 'The URL you requested has been rejected' (a WAF; BLOCKED behind it) | leave |
| `SK_ORSR_EXTRACT` | hp2_eu_central (pivot) | TLS | engine: fetch failed (transport), both passes. www.orsr.sk: 200 and a body, then 'unexpected eof while reading' — the server closes TLS without close_notify, which OpenSSL 3 treats as a failed transfer | leave |
| `EU_FTS_BENEFICIARIES` | hp2_eu_institutions (pivot) | TLS | engine: fetch failed (transport), both passes. ec.europa.eu: 200 and a body, then 'unexpected eof while reading' — the server closes TLS without close_notify, which OpenSSL 3 treats as a failed transfer | leave |
| `EU_RASFF_WINDOW` | hp2_eu_institutions (pivot) | TLS | engine: fetch failed (transport), both passes. webgate.ec.europa.eu: 200 and a body, then 'unexpected eof while reading' — the server closes TLS without close_notify, which OpenSSL 3 treats as a failed transfer | leave |
| `EU_SAFETY_GATE_ALERTS` | hp2_eu_institutions (pivot) | TLS | engine: fetch failed (transport), both passes. ec.europa.eu: 200 and a body, then 'unexpected eof while reading' — the server closes TLS without close_notify, which OpenSSL 3 treats as a failed transfer | leave |
| `PT_BASE_CONTRACTS` | hp2_eu_south (pivot) | TLS | engine: fetch failed (transport), both passes. www.base.gov.pt: incomplete certificate chain under OpenSSL; past it the macOS TLS stack gets HTTP 999 from a WebKnight firewall on every path (BLOCKED behind it) | leave |
| `MX_DOF_GAZETTE` | hp2_northam_ca_mx (pivot) | TLS | engine: fetch failed (transport), both passes. www.dof.gob.mx: hostname mismatch under OpenSSL (the engine's stack); the macOS TLS stack fails too ((60) SSL: no alternative certificate subject name ) | leave |
| `LATAM_CAF_PROJECTS` | hp2_southam_andes (pivot) | TLS | engine: fetch failed (transport), both passes. www.caf.com: incomplete certificate chain under OpenSSL (the engine's stack); the macOS TLS stack gets HTTP 200 | leave |
| `PE_OSCE_DEBARRED` | hp2_southam_andes (pivot) | TLS | engine: fetch failed (transport), both passes. apps.osce.gob.pe: incomplete certificate chain under OpenSSL; past it the macOS TLS stack gets 403 on page, path and root (BLOCKED behind it) | leave |
| `ET_ETHIOPIA_TRADE_REGISTRY` | hp3b31_afrgov (pivot) | TLS | engine: fetch failed (transport), both passes. etrade.gov.et: incomplete certificate chain under OpenSSL (the engine's stack); the macOS TLS stack gets HTTP 200 | leave |
| `NG_NEITI_EXTRACTIVES` | hp3b31_afrgov (pivot) | TLS | engine: fetch failed (transport), both passes. neiti.gov.ng: incomplete certificate chain under OpenSSL (the engine's stack); the macOS TLS stack gets HTTP 200 | leave |
| `AU_ACNC_CHARITY_REGISTER` | hp3b31_apacgov (pivot) | TLS | engine: fetch failed (transport), both passes. DNS ok, TLS 1.3 ok; HTTP/2 stream reset INTERNAL_ERROR under both TLS stacks | leave |
| `AU_LOBBYIST_REGISTER` | hp3b31_apacgov (pivot) | TLS | engine: fetch failed (transport), both passes. lobbyists.ag.gov.au: 200 and a body, then 'unexpected eof while reading' — the server closes TLS without close_notify, which OpenSSL 3 treats as a failed transfer | leave |
| `TW_LEGISLATIVE_YUAN_DATA` | hp3b31_apacgov (pivot) | TLS | engine: fetch failed (transport), both passes. OpenSSL 3 refuses the server's unsafe legacy renegotiation; the macOS TLS stack connects and gets HTTP 500 | leave |
| `AT_LOBBYING_REGISTER` | hp3b31_eunat (pivot) | TLS | engine: fetch failed (transport), both passes. www.lobbyreg.justiz.gv.at: hostname mismatch under OpenSSL (the engine's stack); the macOS TLS stack fails too ((60) SSL: no alternative certificate subject name ) | leave |
| `BR_DOU_GAZETTE` | hp3b31_latamgov (pivot) | TLS | engine: fetch failed (transport), both passes. DNS ok, TLS 1.3 ok; HTTP/2 stream reset PROTOCOL_ERROR under both stacks; HTTP/1.1 also returns nothing | leave |
| `CO_SIGEP_PUBLIC_SERVANTS` | hp3b31_latamgov (pivot) | TLS | engine: fetch failed (transport), both passes. www.funcionpublica.gov.co: incomplete certificate chain under OpenSSL (the engine's stack); the macOS TLS stack gets HTTP 200 | leave |
| `BY_BELARUS_TENDERS` | hp3b31_menacis (pivot) | TLS | engine: fetch failed (transport), both passes. icetrade.by: incomplete certificate chain under OpenSSL; past it the macOS TLS stack gets 403 'Access denied' (BLOCKED behind it) | leave |
| `EG_EGYPT_GOV_SEARCH` | hp3b31_menacis (pivot) | TLS | engine: fetch failed (transport), both passes. www.egypt.gov.eg: expired certificate under OpenSSL (the engine's stack); the macOS TLS stack fails too ((60) SSL certificate problem: certificate has expi) | leave |
| `RU_ARBITR_BANKRUPTCY_GAZETTE` | hp3b31_menacis (pivot) | TLS | engine: fetch failed (transport), both passes. bankruptcy.kommersant.ru: incomplete certificate chain under OpenSSL (the engine's stack); the macOS TLS stack gets HTTP 200 | leave |
| `DK_OPEN_AIS_ARCHIVE` | hp3b31_survtransport (pivot) | TLS | engine: fetch failed (transport), both passes. web.ais.dk: expired certificate under OpenSSL (the engine's stack); the macOS TLS stack fails too ((60) SSL certificate problem: certificate has expi) | leave |
| `MX_DOF_NOTICES` | hp_americas_deep (pivot) | TLS | engine: fetch failed (transport), both passes. www.dof.gob.mx: hostname mismatch under OpenSSL (the engine's stack); the macOS TLS stack fails too ((60) SSL: no alternative certificate subject name ) | leave |
| `GH_RGD_SEARCH` | hp_mena_africa_cis (pivot) | TLS | engine: fetch failed (transport), both passes. incomplete certificate chain (OpenSSL: unable to get local issuer); the macOS stack gets an HTTP/2 PROTOCOL_ERROR | leave |
| `SA_MC_COMMERCIAL` | hp_mena_africa_cis (pivot) | TLS | engine: fetch failed (transport), both passes. mc.gov.sa: incomplete certificate chain under OpenSSL; the macOS TLS stack gets HTTP 200 'E-Services' | leave |
| `CZ_ISIR_INSOLVENCY` | hp2_eu_central (pivot) | SERVER-ERROR | engine: fetch failed 500, both passes. 500 to every agent, both passes | leave |
| `DE_TRANSPARENZREGISTER` | hp2_eu_dach (pivot) | SERVER-ERROR | engine: fetch failed 500, both passes. 500 to every agent, both passes | leave |
| `IE_CBI_REGISTERS` | hp2_eu_west (pivot) | SERVER-ERROR | engine: 200, 0 stored, both passes. 200 error page: 'Error Page' | leave |
| `MX_IMPI_MARCAS` | hp2_northam_ca_mx (pivot) | SERVER-ERROR | engine: fetch failed 500, both passes. 500 to every agent, both passes | leave |
| `JP25_JPBIZ_TOKYO_TAKKEN` | hp3b25_jpbizreg (pivot) | SERVER-ERROR | engine: fetch failed 502, both passes. 502 to every agent, both passes | leave |
| `BH_SIJILAT_REGISTRY` | hp3b31_menacis (pivot) | SERVER-ERROR | engine: 200, 0 stored, both passes. 200 error page: 'Internal Server Error' | leave |
| `CAM_NZ_NZTA_CAMERAS` | hp3b31_survcam (pivot) | SERVER-ERROR | engine: fetch failed 500, both passes. 500 to every agent, both passes | leave |
| `US_OK_SOS_BUSINESS` | hp3b31_usstate (pivot) | SERVER-ERROR | engine: fetch failed (transport), both passes. TCP+TLS ok; engine transport failure, curl now 520 (Cloudflare: origin error) | leave |
| `SK_UVO_TENDERS` | hp2_eu_central (pivot) | TRANSIENT | engine: 200, 0 stored, both passes. 200 maintenance page: 'Plánovaná technická údržba IS UVO' | leave |
| `JP25_JPDEEP_JSSEC_NEWS_HTML` | hp3b25_jpdeep (scheduled) | TRANSIENT | engine: fetch failed 503, both passes. 503 maintenance page to every agent, both passes | leave |
| `JP25_JPFIN_FINMAC_NEWS` | hp3b25_jpfinance (scheduled) | TRANSIENT | engine: 200, 0 stored, both passes. 200 maintenance page: 'メンテナンス中｜FINMAC' | leave |
| `JO28_JFSA_MONTHLY_SURVEY` | hp3b28_jpbank (scheduled) | TRANSIENT | engine: fetch failed 503, both passes. 503 maintenance page to every agent, both passes | leave |
| `JO28_JFSA_WHITE_PAPER` | hp3b28_jpbank (scheduled) | TRANSIENT | engine: fetch failed 503, both passes. 503 maintenance page to every agent, both passes | leave |
| `JO28_SHINYOKUMIAI_MONTHLY_STATS` | hp3b28_jpbank (scheduled) | TRANSIENT | engine: fetch failed 503, both passes. 503 maintenance page to every agent, both passes | leave |
| `JO28_ADR_FINMAC` | hp3b28_jpcourt (scheduled) | TRANSIENT | engine: 200, 0 stored, both passes. 200 maintenance page: 'メンテナンス中｜FINMAC' | leave |
| `JO28_BAR_CHIBA` | hp3b28_jpcourt (scheduled) | TRANSIENT | engine: fetch failed 503, both passes. 503 maintenance page to every agent, both passes | leave |
| `JO28_GOVBOOK_SHOP_DIRECTORY` | hp3b28_jpforgotten (scheduled) | TRANSIENT | engine: fetch failed 503, both passes. 503 maintenance page to every agent, both passes | leave |
| `JO28_TOHOKU_KANKO_ATTRACTIONS` | hp3b28_jpindustry (scheduled) | TRANSIENT | engine: fetch failed 503, both passes. 503 maintenance page to every agent, both passes | leave |
| `AU_ASIC_BANNED_DISQUALIFIED` | hp3b31_apacgov (pivot) | TRANSIENT | engine: 200, 0 stored, both passes. 200 maintenance page: 'Service availability / ASIC' | leave |
| `EU_EASA_AIRWORTHINESS_DIRECTIVES` | hp3b31_eutrans (pivot) | TRANSIENT | engine: 200, 0 stored, both passes. 200 maintenance page: 'EASA - Maintenance Page' | leave |
| `IL_KNESSET_ODATA` | hp3b31_menacis (pivot) | TRANSIENT | engine: 200, 0 stored, both passes. 200 maintenance page: 'Unavailable' | leave |
| `US_IN_SOS_BUSINESS` | hp3b31_usstate (pivot) | TRANSIENT | engine: fetch failed 503, both passes. 503 maintenance page to every agent, both passes | leave |
| `EAEU_CUSTOMS_DECISIONS` | hp2_asia_central (pivot) | EMPTY-OK | engine: 200, 0 stored, both passes. 200; nothing for 'таможня', but the same row stores 2 for 'Таможен' | leave |
| `KZ_STAT_ENTITIES` | hp2_asia_central (pivot) | EMPTY-OK | engine: 200, 0 stored, both passes. 200; nothing for 'банк', but the same row stores 1 for 'статистика' | leave |
| `UZ_OPENINFO_COMPANY` | hp2_asia_central (pivot) | EMPTY-OK | engine: 200, 0 stored, both passes. 200; nothing for 'банк', but the same row stores 6 for 'BANK' | leave |
| `JP_MLIT_NEGATIVE_LIST` | hp2_asia_east (pivot) | EMPTY-OK | engine: 200, 0 stored, both passes. 200; nothing for '東京', but the same row stores 3 for '建設' | leave |
| `MN_COMPANY_REGISTER` | hp2_asia_east (pivot) | EMPTY-OK | engine: 200, 0 stored, both passes. 200; nothing for 'Монгол', but the same row stores 1 for 'ХХК' | leave |
| `BR_CVM_FILINGS` | hp2_southam_brazil (pivot) | EMPTY-OK | engine: 200, 0 stored, both passes. 200; nothing for 'Petrobras', but the same row stores 29 for 'fundos' | leave |
| `JP_MOF_ASSET_FREEZE` | hp2_world_critical (pivot) | EMPTY-OK | engine: 200, 0 stored, both passes. 200; nothing for '東京', but the same row stores 5 for '北朝鮮' | leave |
| `BW_CIPA_REGISTRY` | hp3b31_afrgov (pivot) | EMPTY-OK | engine: 200, 0 stored, both passes. 200; nothing for 'Toyota', but the same row stores 19 for 'company' | leave |
| `NG_BPP_PROCUREMENT` | hp3b31_afrgov (pivot) | EMPTY-OK | engine: 200, 0 stored, both passes. 200; nothing for 'Toyota', but the same row stores 16 for 'procurement' | leave |
