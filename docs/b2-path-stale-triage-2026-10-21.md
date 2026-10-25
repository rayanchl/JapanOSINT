# B2's 44 path-stale rows: the measurement pass

Open item B2 established that 44 of batch 31's twice-refused rows answer 404 on
OUR path and **200 at the host root** — so the publisher is alive and the path
is stale, which is a re-pointing job rather than 44 removals. This is the pass
that turns those 44 unknowns into decisions. Nothing is re-pointed yet; every
line below is a reading taken on 2026-10-21.

## 1. A third reading: the paths really are stale

All 44 re-asked with the engine's own agent, serialised per host.
**43 still HTTP 404**, one `LOCAL_EGRESS` (`opendata.tweedekamer.nl` — this
container's egress, unmeasured from here, not a verdict about the host). Two
readings nine days apart plus a third today: these are not deploys.

## 2. Ask each host what it SERVES, not what path we can guess

Batch 42's and 46's lesson, applied to a search path: each host root was read
for its OWN search form (method, action and the field names a pivot has to
send), for links naming an API or developer portal, and for the sitemaps its
robots.txt declares. `tools/` has no home for this yet; the script is in the
session scratchpad and the output is summarised below.

That found candidates on 24 of the 43. Which is where the pass earns its keep,
because the next step is what separates a candidate from a fix.

## 3. **A 200 IS NOT A FIX.** Rule 4d killed six of eight candidates

Eight candidates answered HTTP 200 with a plausible page of anchors. Asked the
same endpoint about an entity that cannot exist (`zzqqxxjjwwvv7731`) beside a
real one (`Toyota`):

    row                        real    impossible   anchors   verdict
    ZA_GOVERNMENT_GAZETTE     46,284      28,938    82 -> 55  filter works
    US_DATA_GOV_CKAN         144,110     118,234    30 -> 23  filter works
    IE_LOBBYING_REGISTER      48,020      48,040   196    196  FILTER_IGNORED
    IFC_PROJECT_DISCLOSURES   76,527      76,528   195    195  FILTER_IGNORED
    APG_FATF_STYLE_BODIES     40,244      40,284    76     76  FILTER_IGNORED
    MX_INE_FISCALIZACION     124,329     124,499    86     86  FILTER_IGNORED
    TOKYO_MOU_INSPECTIONS          -     167,244     -     158  one reading failed
    ZA_CIPC_ENTERPRISE           404         404     -       -  still 404

**Two of eight.** The other six are moved search pages that now render results
in the browser: the server returns the same shell for any query — one byte
different for IFC, twenty for IE Lobbying — so a row re-pointed there would
emit the site's navigation for every entity, attributed to that entity. That is
rule 4d's confident wrong answer, and it is worse than the 404 it replaces,
because the 404 discloses itself and this would not.

`APG_FATF_STYLE_BODIES` is the one to remember: the host's own search form
declares `GET /documents?keys=`, it answers 200, and it returns 76 anchors —
and it ignores `keys` completely. The form was real; the filter was not.

## 4. What IS there, measured

Three of these rows should never have been HTML scrapes. Their publishers run
documented open-data APIs:

| row | measured replacement | reading |
| --- | --- | --- |
| `FI_EDUSKUNTA_RECORDS` | `avoindata.eduskunta.fi/api/v1/tables/<t>/rows?perPage=&page=` | JSON with `page`/`perPage`/`hasMore` — paging the engine already speaks |
| `NO_STORTINGET_DATA` | `data.stortinget.no/eksport/saker?...&format=json` | JSON export API |
| `AT_PARLAMENT_RECORDS` | `parlament.gv.at/Filter/api/filter/data/101?...&export=true` | JSON |

**And those three APIs were then asked whether they take an ENTITY**, because
these rows are interval-0 pivots and a bulk table API that cannot filter is not
a pivot replacement — it is a different KIND of row:

* **Eduskunta filters, measured.** `?columnName=&columnValue=` returns
  `rowData[1]` unfiltered and `rowData[0]` for a value that matches nothing, so
  the parameter is honoured and this row can stay a pivot. 19 tables, each with
  `page`/`perPage`/`hasMore` paging the engine already speaks.
* **The Austrian filter API is a bulk export, and a large one**:
  `/Filter/api/filter/data/101?...&export=true` answers
  `{"pages":4,"count":391268,"header":[...]}` — 391,268 rows of the parliament's
  document index, machine-readable. That wants to be a SCHEDULED row, not a
  pivot.
* **Stortinget's export is per SESSION** (`?sesjonid=2024-2025&format=json`
  answers `saker_liste`); `?sakid=` alone answers HTTP 500. Also a scheduled
  row rather than a pivot.

So two of the three are a change of KIND as well as of path — interval 0 to a
real interval, which rule 3 and the collector-label rule both have opinions
about. That is a deliberate repair (the openFDA nine went the other way and
this file records what it cost), not a path edit.

And four more have a server-rendered page that works:

| row | measured replacement | reading |
| --- | --- | --- |
| `EU_ECB_SUPERVISED_ENTITIES` | `/banking/list/who/html/index.en.html` | 288 anchors, the actual list of supervised entities |
| `COPERNICUS_EMS_ACTIVATIONS` | `mapping.emergency.copernicus.eu/activations/` | the activations list moved host; 31 anchors |
| `ZA_MUNICIPAL_MONEY` | `municipaldata.treasury.gov.za/api/cubes` | JSON, and the site itself calls it "Municipal Money API" |
| `ZA_GOVERNMENT_GAZETTE` | `/search?search_query=` | filter measured working, 82 → 55 |
| `US_DATA_GOV_CKAN` | `catalog.data.gov/?q=` | filter works; needs `href_must=/dataset/`, the anchors include chrome |

`FR_ASSEMBLEE_NATIONALE` has `data.assemblee-nationale.fr` (274 anchors) and
`CZ_CUZK_CADASTRE` has `api-kn.cuzk.gov.cz` (the host also moved `.cz` →
`.gov.cz`); neither has been resolved to a queryable endpoint yet.

## 5. Four rows where re-pointing is the WRONG answer

* **`WORLDBANK_CONTRACT_AWARDS` — the data left the platform.**
  `finances.worldbank.org` now redirects to `financesone.worldbank.org`, and
  its Socrata catalog is EMPTY: `api.us.socrata.com/api/catalog/v1?domains=
  finances.worldbank.org` answers `resultSetSize: 0` **with no query at all**.
  A re-point to that endpoint would have fetched 200 and emitted nothing for
  ever — rule 1's invisible nothing, shipped. The new site is JS-rendered (0
  anchors), so there is nothing to scrape either.
* **`JP_KANPO_GAZETTE` is now behind a LOGIN.** `search.npb.go.jp/kanpou/`
  redirects to `auth/login/LoginStartUp.form`. That is a credential-gated
  source, not a moved path: it belongs in the "needs credential" disclosure,
  and a key can be added if anyone wants it.
* **`JP_MHLW_LABOUR_VIOLATIONS` and `JP_PMDA_APPROVALS` offer a Google Custom
  Search** (`cof`, `cx`, `ie`, `q`) as their site search. A Google CSE result
  page is Google's data about the ministry, not the ministry's register. Those
  two want their own registers — PMDA's `iyakuSearch` form answers 200 and is
  the real target — and must not be re-pointed at a CSE.
* **`HK_GLD_TENDER_AWARDS`** — the root is 356 bytes and `/eng/` is 404. The
  site is effectively gone; this needs a reading of where GLD publishes tender
  awards now, not a path guess.

## 6. Where this leaves B2

The item said "a re-point each, not a removal", and that is still right — but
the work is **44 decisions, not 44 path edits**, and six of the first eight
candidates were traps that only `--check-filter` could see. The outstanding
work, in order:

1. re-point the seven measured rows in §4, each through
   `probe_hp_batch.py --check-filter` and then `audit_registry_emit.py`;
2. resolve the two with a named-but-unqueried API (`FR_ASSEMBLEE_NATIONALE`,
   `CZ_CUZK_CADASTRE`);
3. find the real register for the four in §5 that have one;
4. **and then a decision that is not mine**: the rows with no
   server-rendered replacement reachable from here. They are honest today —
   one `collector-fetch-failure` record each, naming the 404 — and they are
   useless. Keeping them disclosing and removing them are both defensible;
   the `registry.faa.gov` precedent in this tree is "stays honestly failing
   rather than quietly evading", which argues for keeping them.

What must NOT happen is re-pointing them at a page that answers 200 and
ignores the query. That trades a visible nothing for an invisible wrong answer.

---

# The re-pointing pass, same day

§6's step 1 is done, and it changed five of the seven answers above. Every
reading here is from 2026-10-21, taken with the engine's own agent.

## What shipped

| row | kind | endpoint | the reading that settled it |
| --- | --- | --- | --- |
| `ZA_MUNICIPAL_MONEY` | pivot (unchanged) | `municipaldata.treasury.gov.za/api/cubes/uifwexp/facts?cut=demarcation.label:"{q}"` | 39 of 39 facts for `Cape Town`, **0 of 0** for an impossible label, 10,000 of a declared 10,314 with no cut |
| `ZA_GOVERNMENT_GAZETTE` | pivot (unchanged) | `gov.za/search?search_query={q}` + `href_must=/documents/notices/` | 3 notice anchors for a real entity, **0** for an impossible one |
| `US_DATA_GOV_CKAN` | pivot (unchanged) | `catalog.data.gov/?q={q}` + `href_must=/dataset/`, **no paging** | 8 dataset anchors real, **0** impossible |
| `EU_ECB_SUPERVISED_ENTITIES` | pivot → **weekly** | the monthly workbook via `index_url`/`index_href_must`, sheet `SIs` | 844 rows |
| `EU_ECB_SUPERVISED_ENTITIES_LSI` | **new row** | the same workbook, sheet `LSIs` | 2,175 rows |
| `NO_STORTINGET_DATA` | pivot → **daily** | `data.stortinget.no/eksport/horinger?format=json` | 76 hearings, 83 KB |
| `NO_STORTINGET_INTERPELLATIONS` | **new row** | `/eksport/interpellasjoner?format=json` | 5 |
| `NO_STORTINGET_QUESTION_TIME` | **new row** | `/eksport/sporretimesporsmal?format=json` | 60 |

## Five findings that outlive these rows

* **§4's reading of the ECB row was WRONG, and an anchor count is why.** It
  recorded "288 anchors, the actual list of supervised entities". They are site
  navigation — `/about/working-at-the-ecb` 23, `/about/organisation` 13,
  `/press/other-publications` 10. An anchor COUNT says nothing about what the
  anchors ARE, which is the same error as a record count that cannot see
  whether the fields parsed. The candidate URL was also *itself* already stale
  (`/banking/list/who/html/index.en.html` 301s to
  `/framework/supervised-banks/html/index.en.html`). The list is really
  published as `ssm.listofsupervisedentities<YYYYMM>.en.xlsx`, renamed every
  month — the `index_url`/`index_href_must` case — across **four sheets**
  (SIs 844, LSIs 2,175, Appendix 39, Changes 11). `HP_XLSX` reads one sheet, so
  reading only `SIs` would have dropped 2,175 banks at the seam; hence a
  deliberate pair.
* **Run `make lint-sources` before believing a re-point is a NEW source.**
  §4 named `data.stortinget.no/eksport/saker` for `NO_STORTINGET_DATA`. The
  dup-endpoint check answered immediately: `eur-stortinget-saker`
  (`collectors/feed/generated/vsrc13_europe_gov_2.c`) has read that exact URL
  every six hours all along — 197 records, rc=0, measured today. Registering it
  again would have been the `data.gov.uk` PROXY row over again: two rows, one
  upstream, no new data. Asking which Stortinget exports the tree does NOT read
  found three live ones missing, and all three are now registered.
* **A working filter and chrome are two different problems.** `gov.za`'s search
  filter works, and the impossible entity still returns 3 `/documents/` anchors
  (`/documents/acts`, `/documents/latest`, the constitution) that would be
  emitted as gazette notices for EVERY entity. `href_must` fixes the second,
  `--check-filter` the first, and neither fixes the other.
* **data.gov's CKAN API is GONE, not moved**, and the replacement is a
  downgrade that the row now states. 404 on `/api/3/action/package_search`,
  `/api/action/package_search`, `/api/3/action/status_show` and
  `/api/3/action/package_list` (main's own batch-34 reading of
  `catalog.data.gov` 404 agrees). The HTML search is a RANKED 8, not a complete
  read. **And it must not be paged**: `?page=2` returns the same eight datasets
  as page 1, differing only in a `from_hint` parameter that base64-encodes the
  page number — so the bodies are NOT byte-identical and `pagewalk`'s same-body
  guard would not stop the walk. It would store page 1 under 20 different uids
  and report 160 records, with every gate green.
* **`hp_register()` dropped a row with no `.url` SILENTLY**, and two rows of
  this pass did exactly that: an `index_url` row still needs a `.url` (the
  engine replaces it each run). They compiled, sat in their table, registered
  nothing and said nothing; the only symptom was `make registry-floor` counting
  one fewer source than the day before. It names the table, the index and the
  missing field now.

## Still open, and why

* **`COPERNICUS_EMS_ACTIVATIONS` has no replacement reachable from here.** §4
  claimed "31 anchors"; the same error as the ECB row — all 38 anchors on
  `/activations/` are navigation (`/news/`, `/resources/`, `/accounts/login`).
  The one feed the page declares, `/latest/feed/`, is "CEMS On Demand Mapping
  News" with **2 items** — news articles, not the activation register. The
  activations list is rendered client-side. So this row joins the group in §6
  step 4, below.
* **`FI_EDUSKUNTA_RECORDS` and `AT_PARLAMENT_RECORDS` need an engine
  affordance, not a path.** Both APIs answer with a JSON **table**: an array of
  ARRAYS in `rowData`/`rows`, with the column names in a sibling `columnNames`/
  `header`. The engine's JSON path flattens an array record positionally
  (`.0`, `.1`, …), so a row could ship — and its `title_keys`/`id_keys` would
  be positions whose meaning the API is free to change without us seeing it.
  That is the CSV `csv_no_header` shape in JSON clothing, and two independent
  national parliaments publish it, so it is worth a declared header path rather
  than positional keys in two rows. Eduskunta's filter is honest and measured
  (`columnName=lastname&columnValue=Marin` → 1 row, an impossible value → 0,
  unfiltered → 100 with `hasMore`), so it can stay a pivot once the shape is
  readable. The Austrian one is a bulk export — 391,269 rows over 4 pages, and
  it **ignores `pageSize`**: a request for 3 rows returned 100,000 and 58 MB.
* **§6 step 4's precedent has been overturned.** That paragraph argued for
  keeping the unreplaceable rows disclosing, on the `registry.faa.gov`
  precedent of "stays honestly failing rather than quietly evading". The
  repository owner reversed that for listed hosts on the same day
  (`BROWSER_HOSTS`, `core/httpclient.c`), and two FAA rows are live again. The
  precedent therefore no longer points one way, and the decision is squarely
  the owner's. Note what is NOT in question: a browser profile is a remedy for
  a host that REFUSES us, and every row in this group answers 404 or renders
  client-side, so no agent reaches them.

---

# The long tail, 2026-10-22

## `FR_ASSEMBLEE_NATIONALE` — the pivot path is dead, the publisher is not

The row's path is gone, measured both ways (rule 4d):

    /dyn/recherche?texte=Toyota             404, 64,552 bytes, 148 anchors
    /dyn/recherche?texte=zzqqxxjjwwvv7731   404, 64,562 bytes, 148 anchors

Identical anchor counts because both are the 404 page's own navigation. There
is nothing there to re-point to.

`data.assemblee-nationale.fr` is NOT an open-data platform in the sense the
item assumed — `/api/v2/catalog/datasets` 404s and `/api/explore/v2.1/...`
answers an **Ibexa CMS** error, so it is a CMS, not Opendatasoft. Asking the
host what it SERVES instead (batch 42's lesson) found its real shape: the
portal links **static files** per section, under
`/static/openData/repository/17/amo/…`:

| file | what it is | engine can read it? |
| --- | --- | --- |
| `collaborateurs_csv_opendata/liste_collaborateurs_excel.csv` | every sitting deputy's parliamentary STAFF, by name | **yes** — measured below |
| `AMO50_acteurs_mandats_organes_divises.{csv,json,xml}.zip` | actors, mandates and bodies | no: a CSV or JSON **inside a zip**, and the engine has no mode for that (`lib/zipread.c` exists but only `HP_XLSX` uses it) |
| `AMO10_deputes_actifs_mandats_actifs_organes.json.zip` | sitting deputies with their mandates | no, same reason |

**Measured 2026-10-22** on the one file that is readable: HTTP 200, 109,317
bytes, `text/csv`, **2,077 data rows**, SEMICOLON-delimited (5 fields on
semicolons, 1 on commas), and **not UTF-8** — `windows-1252`, which is why the
header reads `Identifiant du d?put?` to a UTF-8 reader. Five columns: the
deputy's identifier, the deputy's surname and forename, and the
collaborator's surname and forename. One deputy holds many staff, so the
identity is the pair, not the deputy.

That is a real source the tree does not have, and it is registered as its own
row rather than by bending this pivot, whose description is about bills,
amendments and questions — the material that is in the zips.

Two things left behind deliberately, each written down rather than guessed:

* the pivot keeps failing honestly. Its register is the debates and
  amendments, which live in the `.zip` files above.
* **a CSV or JSON inside a ZIP is an engine gap worth one line in the backlog.**
  `lib/zipread.c` already exists and `HP_XLSX` already goes through it, so the
  distance is short; the AMO files are the case that wants it.

## `CZ_CUZK_CADASTRE` — resolved, and it is a credential gate

Found by asking the host what it SERVES rather than guessing a path. The root
of `api-kn.cuzk.gov.cz` is titled "REST API k datům KN" and links a Swagger UI;
the UI's own `index.js` names the spec at `/swagger/v1.0/swagger.json`, and the
spec gives everything the row needed:

    base path     /api/v1              (every path I tried at the root 404s,
                                        identically, which is why guessing failed)
    paths         41
    security      ApiKey, "in": "header"
    searches      /api/v1/{Parcely,Stavby,Jednotky,Rizeni}/Vyhledani

**Measured 2026-10-22, and the gate is clean:**

    no ApiKey header         401, body {"title":"Unauthorized","status":401}
    ApiKey: <bogus value>    401, the same body
    /AplikacniSluzby/Health  200 — the service is UP; it is the DATA that is gated

So the row carries `key_env = "JO_CUZK_API_KEY"` and an `ApiKey: {key}` header,
and stores a "needs credential" notice naming what is missing until a key is
set. That is rule 1's honest degradation, and it is a better state than a 404
that reads like a dead publisher.

Two things about its shape worth knowing before anyone adds the key:

* **its searches take CODES, not names.** `KodKatastralnihoUzemi` is a
  cadastral-area code and `KmenoveCisloParcely` a parcel number, so this is an
  identifier pivot like the openFDA nine — asking it for a company name will
  correctly return nothing.
* **the API documents the honest-empty distinction this tree cares about**: a
  missing object is 404, while a search matching nothing is **200 with an empty
  `data` list and an informational message**. Whoever wires the key should
  check the engine reads the second as an empty rather than a failure.

## B2-bis resolved for Finland, and DELIBERATELY NOT for Austria

Both needed the same thing the engine did not have: a way to key a JSON
**table** — records that are arrays, with the column names in a sibling array.
`row_header_path` is that (hpengine, 2026-10-22; `hptest` 49–49f, six of the
eight fixtures confirmed to fail without it and the two that pass being the
controls that assert the OLD behaviour is preserved).

**`FI_EDUSKUNTA_RECORDS` ships.** Measured through the engine the same day:

    Marin               1 of 1 emitted, 1 stored, 2 requests, 20,312 bytes
    zzqqxxjjwwvv7731    0 records, 289 bytes — an honest empty (rule 4d)

and the stored record proves the affordance did its job, carrying
`personId`, `lastname`, `firstname` and `minister` by NAME rather than `.0`,
`.1`, `.2`. It stays an entity pivot because the API's filter is real, measured
both ways.

**`AT_PARLAMENT_RECORDS` does not ship, and the reason is a measurement.**
Three readings of `/Filter/api/filter/data/101`:

* **`showAll` is a presence, not a value.** `showAll=false` behaves exactly
  like `showAll=true`: 4 pages of 100,000 rows at **58 MB a page**. Omit the
  parameter entirely and the same endpoint pages **20 rows at a time over
  19,565 pages** at 17 KB a request, which is the shape a cycle would want.
* **`pageSize` is ignored** — a request for 3 rows returned 100,000.
* **and the index has NO STABLE ORDERING.** Two identical requests for the
  unpaged first page returned different rows — `5,7,9,314,512` then
  `1,3,4,8,2` — while `page=2` returned the same five rows on two reads. Of ten
  parameter spellings probed, six appeared to "move" the answer and most moved
  it to the same place, which is what an unstable index looks like rather than
  six working parameters.

CLAUDE.md is explicit about this: an offset or page walk with no stable sort
silently SKIPS records, and the only visible symptom is a collision — "the
worst shape in this file", because the visible half is harmless and the
invisible half is lost data. 19,565 pages over an index whose head reshuffles
between two requests would do exactly that, and `page_resume` would carry the
damage across runs.

So the row keeps failing honestly. What it needs is one **measured** sort
parameter, not a guess: the engine affordance is in place, the endpoint and its
391,283 rows are known, and the remaining question is whether that filter API
accepts a sort at all. Written down rather than shipped on an unstable reading.
