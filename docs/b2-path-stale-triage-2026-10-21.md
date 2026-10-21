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
