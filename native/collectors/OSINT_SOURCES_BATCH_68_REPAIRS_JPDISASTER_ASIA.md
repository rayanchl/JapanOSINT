# Batch 68 — repairs to batches 63–67, Japan disaster layers, webcams, Asia

Registry 105,229 → **109,801** (`make source-count`, both floors re-recorded):
+5,058 new rows, −486 removed. Every new row was run through the engine
(`audit_registry_emit.py`, `JO_PAGE_MAX=2`); readings in
`docs/verified-sources-batch68.tsv`, rejects in `docs/rejected-sources-batch68.tsv`,
removals with reasons in `docs/removed-sources-2026-11-23-batch68.tsv`.

## New rows

| table | rows | what |
|---|---|---|
| `hp3b68_jpdisaster.c` | 4,199 | Token-free feature layers of ISUT (Cabinet Office), NIED, Tokyo, MLIT and five municipal crisis offices on ArcGIS Online. 4,199 of 4,199 OK, 1,290,815 emitted / 1,291,322 stored in a two-page pass. Each layer on its own objectIdField and maxRecordCount. |
| `hp3b68_webcams.c` | 86 | Camera lists published by their operators. 49 are river.go.jp per-prefecture lists (~11,300 river cameras) with a detail hop for coordinates, owner and image URL. 86 of 86 OK (Houston needed `Accept: */*`: 406 → 1,123). |
| `hp3b68_asia.c` | 773 | Taiwan 451 (TWSE/TPEx OpenAPI, gov.tw notices, feeds), Korea 189, China 82, India 58. 790 probed PASS; 17 dropped by the engine (10 store nothing, 2 answer 307, 5 with a non-unique key). 3 kept failing on this path (502, 503, refused handshake). |

Not registered, on purpose: 8,730 ISUT hourly heat-index / daily heatstroke-alert
snapshot services (one static service per hour, no "latest"); 371 Asian homepage
rows whose anchor shape matched navigation rather than notices
(`docs/staging-sources-batch68.asia-heldback.txt`); ~220 third-party re-uploads
of camera lists and four layers exposing internal device fields.
The `.gov.cn` re-probe reached 110 of 2,870 hosts (3.8%); the rest is unmeasured
from this container, not dead.

## Repairs to batches 63–67

* **CKAN datastore, stable sort** (`docs/measured-ckan-sort-2026-11-16.tsv`):
  6,753 of 7,419 rows now carry `sort=_id asc`, one measured request per row.
  Montreal IGNORES it (desc returns the same rows; 229 unchanged), NHSBSA
  refuses it (BigQuery backend, no `_id`; 85), 352 unmeasured. 5,382 rows get
  title keys from the resource's own `fields`.
* **OpenDataSoft, stable order** (`docs/measured-ods-order-2026-11-16.tsv`):
  7,048 of 7,419 rows carry an `order_by` verified accepted and honoured —
  5,794 TOTAL (tuple measured unique by `group_by` max count 1, or the whole
  dataset in the sample), 1,254 PARTIAL. `count(distinct)` is approximate on
  ODS (312,521 for 300,001 unique rows) and was not used as evidence.
* **A title pass changes identity on a row with no `id_keys`.** hpengine keys
  such a record on its title, so giving 4,862 ODS rows real titles collapsed
  records sharing one (Bondoufle's register: 109 of 200). Found by the engine
  re-run of a per-host sample, not by the probe. 4,501 TOTAL rows now key on
  their measured-unique order tuple (`a+b+c`), 1,282 on a single unique field;
  the four worst re-read 200 of 200. PARTIAL rows have no unique key to give.
* **Retries.** `data.chhs.ca.gov` is the "collector" token filter again
  (`HA_NOCOLLECTOR`; 146 of 151 rows recover). 95 WFS rows rewritten: NRW needs
  `application/json; subtype=geojson`; 35 geopf/PDOK "network" failures were the
  64 MB body ceiling at `count=1000` and are sized per row; 51 Polish MapServer
  layers publish GML only and are `HP_XML` now (51 of 51 OK); ICNF's feature
  `id` is generated per request, so it keys on `properties.id`.
* **Removed, 486:** 213 ODS rows whose records are restricted to domain members
  (metadata 200, `/records` 403 under every agent), 176 CKAN datastore rows on
  two hosts that 403 the datastore path while the catalogue answers, 47 on
  Fort Collins' retired Socrata portal, 43 CKAN resources that answer JSON 404,
  6 dead or empty, 1 duplicate of `ods-public-geonames`. None was an agent wall:
  every 403 host was bisected across five honest agents.

Still failing and kept: 154 batch-67 rows on seven hosts the egress gateway
cannot connect to (unmeasured), 3 rows over the body ceiling.
