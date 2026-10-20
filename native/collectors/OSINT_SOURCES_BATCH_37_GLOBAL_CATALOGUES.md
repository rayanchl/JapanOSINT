# Batch 37 — global open-data catalogues (14 rows + 2, probed and measured 2026-10-12..17)

`collectors/pivot/table/hp3b37_{jp,fr,us,eu,row}.c`

This batch is the 80-row staging set in `docs/batch37/STAGING_README.md`, taken
through the gates it had never been able to run. It was authored 2026-10-02 in a
session with **no network egress to any of its hosts**, so every host and path
was inferred from its family's convention, and the README predicted a high pass
rate on the reasoning that a catalogue endpoint "needs only the host to be
correct".

## The headline is the pass rate: 15 of 80

| verdict | rows |
| --- | --- |
| **PASS** | **15** |
| HTTP_ERR | 38 |
| NET_ERR | 12 |
| PATH_UNRESOLVED | 8 |
| UNPARSEABLE | 7 |

19%. The reasoning in the README was sound and the conclusion was still wrong,
which is the point worth keeping: **a uniform API family tells you the shape of
the request, not whether the host serves it.** Convention is a hypothesis.

Every rejected row is kept with the status it answered, per beat, in
`docs/rejected-sources-batch37.<beat>.tsv` — so the next pass starts from 65
measurements instead of re-probing.

What the failures actually were:

* **The entire `cam` beat, 10 of 10.** Official DOT/agency ArcGIS camera
  inventories: every service path was guessed and none exists. The giveaway is
  that ArcGIS answers **HTTP 200** carrying
  `{"error":{"code":400,"message":"Invalid URL"}}`, so a checker that reads the
  status sees success. Only `probe_hp_batch.py`'s body judgement caught it, as
  `PATH_UNRESOLVED` — the same class of trap as rule 4d.
* **8 Socrata domains 404 on `/api/catalog/v1`** while 5 others pass: Nashville,
  Baltimore, Detroit, San Diego, Utah, Hawaii, Fort Worth, Louisville have left
  Socrata. `api.us.socrata.com/api/catalog/v1?domains=…` 404s for them too, so
  no path recovers them — they are not Socrata portals any more.
* **Four national portals that are not CKAN.** data.norge.no and dataportal.se
  serve Next.js applications, data.gov.be serves HTML, catalog.data.gov answers
  `{"message":"Not Found"}`.
* **12 NET_ERR** (502 from the proxy, connection reset) are this environment
  refusing the host, not evidence about the source. Recorded as
  unreachable-from-here, not as dead.

## Three defects in the rows that DID pass

A probe cannot see any of these. Each was found by a measurement after it.

1. **All 75 paged candidates lacked a stable sort**, so the 15 promoted rows
   would have paged an unordered collection by offset. Measured twice elsewhere
   on 2026-10-12 (OpenAerialMap, Vélib): a reordering index serves one record
   twice and **skips another silently**, and the only visible symptom is a
   collision that looks harmless. Each family's parameter was verified accepted
   against a live host before being added — CKAN `sort=id asc`, Socrata
   `order=name`, OpenDataSoft `order_by=dataset_id`.
2. **`EU_ECB_FXREF` keyed 29 daily rates on a field that is not on the record.**
   The file is `Cube > Cube[time] > 29x Cube[currency,rate]`, and repeated XML
   siblings become INDEXED FIELDS (`Cube.Cube.@currency`, `Cube.Cube.1.@currency`
   …) rather than an array, so `array_path=Cube.Cube.Cube` emitted exactly 1.
   One record per publication day holding all 29 rates is what the file is, and
   nothing is discarded — but `id_keys=currency` named a field that does not
   exist, and a first repair guessed `date_field=fx_date`, which both shape
   notices reported as matching 0 of 1 records. The key is `Cube.@time`, read
   off the stored record's `properties` rather than reasoned about.
3. **`JP_CKAN_DATAGOJP` was pointed at a host that 301s and drops the query
   string.** `www.data.go.jp/data/api/...` redirects to
   `data.e-gov.go.jp/data/api/...` and the redirect arrives with no `rows`, no
   `start` and no `sort`, so the API applied its default `rows=10`: the row
   stored ten datasets of the **18,096** the catalogue declares, at `rc=0`,
   looking like a tiny portal rather than a broken request.

   The first repair here was the wrong diagnosis from the right observation —
   ten rows back for `rows=100` reads as a server that caps the page size, so
   the stride was lowered to match. Asked DIRECTLY, `data.e-gov.go.jp` honours
   `rows=100` and `start` (`start=500` opens on a different id). **Follow a 301
   by hand before concluding anything about what a server will serve**: the
   redirect, not the page size, was the whole defect.

   Then, measured complete at 18,826/18,826, the row was REMOVED: `make
   lint-sources` flagged the endpoint as a duplicate, and it is —
   `hp3b23_jpnational.c` reads the same `package_search` per organisation
   (`fq=organization:org_NNNN`, 23 rows). The portal's own `organization_list`
   says 25 organisations hold those 18,826 datasets, so the two missing ones
   are added as rows of that family (`hp3b37_jp.c`), which is what was actually
   uncovered. A full second read would have stored every dataset twice.

## Page ceilings are measured, not defaulted

Nine of the 15 stopped at the engine's default page ceiling on the first audit
and disclosed it. Each now carries a `page_max` sized from the upstream's own
declared total, so the ones that can be complete are complete and the one that
cannot stays bounded and says so: data.europa.eu declares **1,733,660**
datasets (17,337 pages), which nothing wants on a weekly schedule.

## Gates

`manifest.py` strict load, `audit_batch_reachable.py` (0 of 15 can never run),
`audit_batch_pagination.py` (0 undeclared-paged), `batch_exclusions.py`,
`probe_hp_batch.py --check-filter` (no FILTER_IGNORED; these are scheduled
rows, not entity pivots), `audit_registry_emit.py` over all 15, then
`make audit-sources`, `make lint-sources` and both floors.

Emitted-and-stored readings per row: `docs/verified-sources-batch37.tsv`.
