# Batches 34 and 35 measured — 239 rows, rule 4 and 4b

Open item **B1** closed. These two batches were authored 2026-10-04 in a session
whose network policy reached only package registries and GitHub, so no row had
ever been run against its upstream. 239 rows: 139 `JO34_*` and 100 `JO35_*`.

Readings: `docs/emit-sources-batch34-35-2026-10-20.tsv` (the registry sweep) and
`docs/emit-sources-batch34-35-pivots-2026-10-20.tsv` (the manifest audit).

## The two halves need two tools, and that is not a detail

The registry sweep runs every row with no entity, so it reported **124 of the
239 as `NEEDS_ENTITY`** — the 24 Socrata Discovery pivots and all 100 WEKO3
repository pivots. That is the verdict working as designed, not a gap:
CLAUDE.md says to use `audit_batch_emit.py` with the MANIFEST for pivot rows,
and the sweep has `NEEDS_ENTITY` precisely so an on-demand pivot asked nothing
cannot read as a source that emits nothing.

| | scheduled rows (115) | pivot rows (124) |
| --- | --- | --- |
| tool | `audit_registry_emit.py --match '^JO3[45]_'` | `audit_batch_emit.py <manifests>` |
| healthy | **92** OK | **73** OK |
| answered, held nothing for the entity | — | **40** `EMPTY_UPSTREAM` |
| collision (a question, not a verdict) | 3 | — |
| upstream refused | 20 | 11 (9×500, 1×502, 1 transport) |

33,108 records emitted and 33,108 stored across the scheduled rows where both
numbers are known — **0 lost to uid collision**, which is the `id_keys`
question rule 4b asks, answered for the beat as a whole.

## Three rows were the sweep's own concurrency, and six were not

At three parallel jobs the sweep reported 23 `FETCH_FAILED`. Re-run **serially**,
three of them store records: `DATOSABIERTOS_MALAGA_EU` 1,377,
`FDSNSTA_SEISDATA_EPOS_FRANCE_FR` 6,594 stations, `FDSNSTA_SERVICE_SCEDC_CALTECH_EDU`
6,897. So the re-run earned its keep.

What matters more is what it ruled OUT. Six rows — data.ok.gov,
data.virginia.gov, opendata.hawaii.gov, data.houstontx.gov, data.milwaukee.gov,
data.sanantonio.gov — answered **502 serially as well**, while the same URLs
answer 200 in half a second by hand. That left the CLIENT as the only
difference, which is the `au-asic` question CLAUDE.md names and leaves open.
Bisected, six for six:

    host                   engine UA   without "collector"   bare
    data.ok.gov              502            200               200
    data.virginia.gov        502            200               200
    opendata.hawaii.gov      502            200               200
    data.houstontx.gov       502            200               200
    data.milwaukee.gov       502            200               200
    data.sanantonio.gov      502            200               200

One word in the User-Agent, the same word and the same status as the
`data.boston.gov` and `data.sanjoseca.gov` entries already in
`core/httpclient.c`'s UA_OVERRIDE table. Six host entries later: 0 → 96, 186,
397, 166, 963 and 1,823 records.

**And the tree already knew, one row at a time.** Each of those six hosts
carries a second registered row from an earlier batch — five `JO29A_FGT_*` and
`SOCRATA_HAWAII` — and every one of them carries a per-ROW
`User-Agent: JapanOSINT/1.0` header. An earlier session met this same filter on
these same hosts and fixed it in the row instead of in the table, so batch 34
could not inherit it. A host entry fixes every row on the host, including the
next one somebody adds.

## data.virginia.gov: a collision that is really a skip

Virginia came back `COLLISION`, 2,000 emitted and 1,823 stored, and a collision
is a question. Measured against itself:

| | fetched | distinct | duplicates |
| --- | --- | --- | --- |
| `current_package_list_with_resources`, offset-paged | 2,000 | 1,800 | **200** |
| `package_search`, `sort=id asc` | 2,000 | 2,000 | **0** |

and **1,884 of the sorted ids never appeared in the bulk walk at all**. The
duplicates are the visible half; the invisible half is records skipped
permanently, with no symptom but the collision — the worst shape in CLAUDE.md.
Its own count is **32,089** datasets, which no 20-page ceiling reaches, so the
row is now sorted, keyed and **cycling** (`page_resume = 1`, which an ascending
sort is the precondition for).

**It is NOT a beat-wide change, and that was checked rather than assumed.** A
12-host sample walked both ways shows the bulk endpoint returning **0 duplicates
on 9 of the 10 reachable hosts** (Kanazawa: 1 of 319). Re-pointing all 95 rows
on Virginia's evidence would be the blanket fix CLAUDE.md records as a mistake
for the 16 `sortBy`-less WFS rows. What the sample DID establish is a separate,
real finding: `ckan.govdata.de` holds 169,255 datasets, `ckan.opendata.swiss`
16,117 and Tokyo Metro 9,709, against 2,000 a run and no cycle — a page-ceiling
question for `audit_page_ceiling.py`, recorded rather than guessed at.

## Four more rows re-pointed, one removed as a duplicate

Three portals refuse the bulk action and serve `package_search` on the same
CKAN, whose results are whole package dicts carrying `resources` — verified
key by key, since that inline resource list is the only reason these rows exist:

    data.gov.il                        bulk 404 (HTML)  → search 200, 1,203 datasets
    admin.opendata.az                  bulk 403         → search 200, 1,970
    ckan.publishing.service.gov.uk     bulk 403         → search 200, 59,370 (cycles)

The fourth is a removal. `JO34_CKANRES_DATA_GOV_UK` asked data.gov.uk for the
same action, and **data.gov.uk/api is a proxy in front of
ckan.publishing.service.gov.uk** — its own response names that host in the
`help` URL and both report the identical 59,370. Two ids, one collection, one
upstream; the proxy row is gone and the canonical one kept.

## The FDSN event beat: one honest empty, one dead service, told apart

Three of the five event rows failed, and the three failures are three different
things. The status could not tell two of them apart — both HTTP 404 — and the
bodies could:

* **`ws.icgc.cat` is LIVE AND EMPTY.** `/fdsnws/event/1/version` answers
  `1.2.4`, and the catalogue holds no events for any window asked of it,
  including a whole year. The proof it is empty rather than broken comes from
  the same endpoint: drop `nodata` and the identical query answers **HTTP 204
  with no body**, FDSN's documented empty. The row is correct and is kept.
* **`eida.koeri.boun.edu.tr` does not run the service.**
  `/fdsnws/event/1/version` 404, `/fdsnws/dataselect/1/version` 200 `1.1.1`.
  That is batch 34's own lesson — "the FDSN spec defines services; a node runs
  only some of them", which dropped 11 rows before any probe — hit a twelfth
  time by a row that survived it. **Removed.**
* **`eida.gein.noa.gr` requires a time range.** No window → HTTP 400 on every
  run since it shipped. With `starttime` two days back → 200, 249 events, 39 KB,
  one request. A per-row window, because SCEDC on the same beat serves 20,000
  with no window at all, so this is a property of the endpoint and not of the
  specification.

ICGC is why the engine changed. Its 404 was being filed as a
`collector-fetch-failure` reading "the upstream refused the request", remedy
"the endpoint has moved or retired" — a false disclosure pointing the wrong way,
the same defect as the Archive-It rate-limit 200. `hp_status_is_declared_empty()`
now reads an upstream's own no-data answer as the honest empty rule 1 asks for,
on the BODY's words and never on the status or on the row having asked for
`nodata=404` — because keying on either of those would have filed KOERI's dead
service as a healthy empty, which is the dangerous direction. `hptest` 46, 46b,
46c and 46d pin all four halves, and were checked against the old behaviour.

## What is left, with its reason

| rows | reading | what it is |
| --- | --- | --- |
| `catalog.data.gov` | 404 on `/api/3/action/*`, including `package_search` | data.gov has moved off the CKAN Action API. Not a filter and not a path typo — there is no CKAN there to re-point to. Needs a route for data.gov's current stack, which is a piece of work, not a line |
| `catalogue.data.govt.nz` | HTTP **200** carrying Imperva's `Pardon Our Interruption` | a bot wall wearing a success status. The engine now names it rather than suggesting a move; whether any honest agent clears it is unmeasured |
| `www.datosabiertos.gob.pe` | 418, and 404 on every CKAN path | a wall and no reachable API |
| `data.go.th` | 403 on every CKAN path | a wall, or a key |
| `data.gov.ro` | connection reset | the `LOCAL_EGRESS` shape — unmeasured from here, not a verdict |
| `opendata.city.niigata.lg.jp`, `data.city.tendo.yamagata.jp` | `SSL: UNEXPECTED_EOF_WHILE_READING`, for the engine AND by hand | the item-D1 TLS family: a handshake neither client completes |
| 11 pivots | 9×500, 1×502, 1 transport | one reading each; CLAUDE.md's rule is to re-run a FETCH_FAILED once before removing it |
| 3 collisions | govdata.de 1 of 2,000, Zurich, NOA | measure the upstream against itself before touching a key — Virginia above is why, in both directions |
