# Batch 38: 150 rows — 75 free and measured, 75 key-gated and honest about it

Second regional sweep, authored 2026-10-06 with outbound HTTPS. Registry
**18,643 → 18,793**, +150. With batch 36's 188 the two sweeps add **338**
sources for France, the UK, the USA, Africa and Hong Kong.

| Family | Rows | Kind | What a record is |
|---|---:|---|---|
| OpenAlex institutions by country | 58 | gated | a research institution — ROR id, type, geo, parent/child, works and citation counts |
| GLEIF LEI by country | 46 | **free** | a legal entity — LEI, legal name, registration authority and status, legal and HQ address |
| US Treasury fiscal data | 17 | **free** | one line of a federal fiscal statement — MTS, DTS, MSPD and securities tables |
| GBIF occurrences by country | 11 | **free** | a georeferenced species observation — taxon, coordinates, date, institution, dataset |
| congress.gov | 7 | gated | bills, members, committees, nominations, treaties, hearings, amendments |
| EIA energy | 4 | gated | an energy or emissions observation |
| FRED | 3 | gated | statistical releases, sources and tags |
| OpenFEC / regulations.gov / NASA / UK FSA | 4 | 3 gated, 1 free | committees, rulemaking dockets, near-Earth objects, food-business types |
| **total** | **150** | 75 free, 75 gated | |

By beat: africa 102, usa 36, china 5, uk 4, france 3.

## Gated rows are declared, not pretended

Every gated row carries `key_env`, so `lib/hpengine.c` routes it through
`_credential_notice.inc` and stores a **needs-credential notice** until the
variable is set. It is honest about being unrun rather than silently empty, and
the moment a key exists the row works without further editing.

Their ROUTE was probed, and the status codes are evidence rather than noise:

* **401** — the route exists and wants a key (Companies House, before those rows
  turned out to be duplicates).
* **429** — the route exists and the *quota* refused, not the path. A shared
  `DEMO_KEY` is exhausted across everyone using it.
* **400** — FRED validates the key *format*, so the literal `DEMO_KEY` is
  rejected by a route that is plainly there.
* **200** — eight rows answered a `DEMO_KEY` for real, so their record shape was
  read rather than assumed. Those say so.

What is NOT claimed for a gated row: a record count. Each description states
that explicitly.

## OpenAlex is gated because its free tier is an IP-shared budget

This is the finding worth carrying forward. `api.openalex.org` answered:

```
HTTP/2 429   retry-after: 52568
{"error":"Rate limit exceeded","message":"Insufficient budget. This request has
no API key, so it counts against the free daily budget shared by everyone on
your network's IP address, and that budget is used up ($0 remaining; resets at
midnight UTC). Use your own key instead..."}
```

Shared by everyone on the network's IP, and resets only at midnight UTC — 14.6
hours away at probe time. Serial requests with the documented `mailto=` polite
parameter did not help, because the budget is already spent, not rate-limited
per second.

**That has a consequence for rows already in the tree.** 54 OpenAlex endpoints
are registered keyless with `free_tier = 1`. Run today,
`OPENALEX_WORKS_SDG16_LATEST` — a *scheduled* row, 870 ms, so it really did
fetch — stored **0**. It is not possible to tell from the run line that a budget
refused it rather than the world being empty. This batch does not change those
rows, but it is why its own 58 are gated: a keyless row against an IP-shared
budget is unreliable, not free. Worth a follow-up pass over the existing 54.

Their shape and keys are copied verbatim from `OPENALEX_INSTITUTIONS` in
`hp_research_ip.c`, which passed the emit audit — the batch-34 precedent for a
row that cannot be measured in the authoring session.

## The auto key-picker repeated batch 36's mistake in four new disguises

Batch 36's lesson was that choosing `id_keys` from a precedence list of field
*names* keys a station register on its longitude. So this batch built key
selection on **measured uniqueness** instead — and the measurement still had to
be checked, because "unique over one page" is not "unique over the series":

| row | auto-picked | what it is | corrected to |
|---|---|---|---|
| `US_TREAS_DEBT_SUBJECT_TO_LIMIT` | `close_today_bal+record_date` | a **balance** | `record_date+src_line_nbr` |
| `US_TREAS_SECURITIES_SALES` | `record_calendar_month+securities_sold_cnt` | a **count** | `record_date+src_line_nbr` |
| `US_TREAS_MTS_TABLE_5` | `line_code_nbr` | a bare dimension | `record_date+classification_id` |
| `US_EIA_CO2` | `fuel-name+sector-name+state-name` | a series with **no time** in its key | `period+fuel-name+sector-name+state-name` |

The forbid-list caught `lat`/`long`/`value`/`count` and still missed `_bal` and
`_cnt`. The fix that actually works is not a longer list of forbidden names: it
is **verifying the composite over more records than one page holds.** Over 2,000
records, `record_date+src_line_nbr` and `record_date+classification_id` are both
1:1 on every Treasury table tested, and `classification_id` alone happens to be
too — but it is a surrogate whose uniqueness is not promised, so the date is
kept in the key.

### A fifth disguise, and the one the sweep caught: a date PART is not a date

Four Treasury rows shipped with `record_calendar_day` in their key.
**`record_calendar_day` is the day-of-MONTH number, 1 to 31** — so two records
from different months that share a day and a line number are the same key. The
sweep found it on one row (`deposits_withdrawals_operating_cash`: 1,000 emitted,
992 stored) and the other three were latent, waiting for their series to span
enough months.

This one is instructive because it passed *two* checks that should have caught
it: it is not a measurement, so the forbid-list let it through, and it was
measurably 1:1 over the page the auto-picker read, because that page was one
month. Only the registry sweep, which pages, could see it.

Re-measured over 2,000 records per table: `record_date+src_line_nbr` is 1:1 on
every `dts`, `od` and `mspd` table and `record_date+classification_id` on every
`mts` table. After the fix, the four rows store 1,000/1,000 and 500/500 with
nothing lost. Titles were corrected at the same time — `title_keys` had been
`record_calendar_day`, which titles every row of a fiscal statement with a number
between 1 and 31.

GBIF's auto-picked title was
`classifications.<uuid>.acceptedUsage.name` — a readable field reached through a
**checklist UUID** in the path. Replaced by hand with `scientificName`, because
a title keyed on a UUID breaks the moment GBIF reissues that checklist id.

## GLEIF by country is not the GLEIF already in the tree

`batch_exclusions.py` reported 46 NEAR-ENDPOINT near-misses against
`collectors/sources/bo_world.c`, which composes `api.gleif.org/api/v1/lei-records`
URLs at runtime. Read by hand: `bo_world.c` filters
`filter[entity.legalName]=<name>` — *"find the LEI for a company called X"*.
These rows filter `filter[entity.legalAddress.country]=<C>` — *"list every legal
entity registered in country C"*. Different parameter, different question, and
the second cannot be derived from the first. All 46 kept.

Eight rows were dropped as genuine duplicates: three Companies House searches
and three FSA endpoints (`Establishments?name=` is `uk_world.c:112` with a
different `pageSize`), `US_FEC_FILINGS` and `US_REGULATIONS_COMMENTS`.

## Two smaller findings

**A header value with a semicolon splits the opts field.** The OpenAlex rows
carry `User-Agent: JapanOSINT (research; contact@japanosint.local)` copied from
`OA_UA`. Written straight into `opts`, that `;` ends the token and the rest
becomes a value-less fragment — the defect `tools/manifest.py` exists to report.
Escaped as `\;`, which the parser passes through as a literal. The generator
asserts on it now, splitting the way `manifest.py` does.

**FSA needs its own header and the first probe called it dead.**
`api.ratings.food.gov.uk` answers 403 without `x-api-version: 2`. All four FSA
rows were written off as dead until the header went on; three then turned out to
be duplicates, so the header work bought one row — and the knowledge that a
403 from that host is a missing header, not a dead route.

**GBIF's path from this container is flaky.** Measured 2026-10-06:
`api.gbif.org` answered **7 of 10** consecutive attempts, the other three failing
with a connection reset through the egress proxy rather than any HTTP status.
That is a property of this path, not the endpoint, and a failed fetch is an
honest empty in the engine — a tick that resets stores nothing rather than
anything invented. Recorded in each GBIF row's description.

## Measurements

`probe_hp_batch.py --check-filter`: **73 of 75 free rows PASS**, the two
exceptions being GBIF transport resets that pass on retry (`items=300`). 8 of the
75 gated rows also PASS, via `DEMO_KEY`. No `FILTER_IGNORED`.

`batch_exclusions.py`: **0 collisions**, 46 near-misses read by hand (above).
`audit_batch_reachable.py`: **0 of 150 can never run.**
`audit_batch_pagination.py`: **0 undeclared**, 25 carrying a measured
`pagination_ok` note.

Paging measured, not assumed — page 2 fetched and compared against page 1:
GLEIF `page[number]`, Treasury `page[number]`, FSA `pageNumber` and GBIF
`offset` (offset=300 returned first key 5938082799 against 5937748555) all
produce distinct bodies. Gated rows declare their published paging and say it is
unmeasured.

### The whole-batch sweep

`audit_registry_emit.py` over all 150 ids, reading `stored` off the run line and
the row count back out of a fresh database:

**146 OK, 3 EMITS_NOTHING, 1 COLLISION** — and after the fixes above, **150 OK**.

* **75 of 75 gated rows: OK.** Each stores exactly one row, the needs-credential
  notice. `OPENALEX_INST_CF` emitted 0 and stored 1. That is the whole point: a
  gated row is legible as unrun, not indistinguishable from an empty world.
* The 1 COLLISION was the `record_calendar_day` defect above. Fixed, re-measured
  at 1,000 emitted / 1,000 stored.
* The 3 EMITS_NOTHING were `GBIF_OCC_GB`, `_HK` and `_US` — the 7-in-10 transport
  path, not the rows. Re-run: **3,000 emitted and 3,001 stored each**, so they
  page ten deep and lose nothing when the fetch lands.

Across the seven rows re-measured after the fixes: 11,500 emitted, 11,503 stored,
**0 records lost to uid collision**.

Per-row evidence: `docs/verified-sources-batch38.tsv`.
