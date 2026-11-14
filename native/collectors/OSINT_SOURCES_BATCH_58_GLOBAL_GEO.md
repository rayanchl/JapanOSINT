# Batch 58 — government geospatial (ArcGIS), US + EU, and the CN/IN/KR ceiling

Added 2,947 emit-verified sources (registry 61,527 → 64,474; both floors
re-recorded per build). Three beats, one method, measured end to end.

## Method (batch 46/53 at wider scale)
Ask each government ArcGIS service root for its own directory (`?f=json`),
walk folders → FeatureServer/MapServer services → layers; for each layer read
its `objectIdField`, `maxRecordCount` and live `returnCountOnly`. Only layers
whose count > 0 become candidate rows. Every candidate is then run through the
real engine (`audit_registry_emit.py`, `JO_PAGE_MAX=2`) and **only rows that
emit AND store** are registered; the rest are kept as data in the per-beat
`rejected-sources` TSVs.

Each row pages on `resultOffset` with the layer's own `objectIdField ASC` and
`page_resume=1`; polygons request a WGS84 centroid (≈1.7% of ring payload),
points/polylines keep geometry; `returnCentroid` is only sent for polygon
layers (it is an HTTP-200 error on others).

| beat | table | rows | source |
|---|---|---|---|
| 1 | hp3b58_arcgis  | 1,584 | US + EU gov (EEA discomap air/water/bio/image; FEMA, NY State, Vermont, USGS, NOAA) |
| 2 | hp3b58_arcgis2 |   713 | US county/state (Miami-Dade FL, King County WA, New Jersey) |
| 3 | hp3b58_arcgis3 |   650 | US state (Oregon DEQ, Washington DNR) |

Region split: 890 EU, 2,057 US.

## What was deliberately NOT registered
- **Region-opaque AGOL org-ids** (services*.arcgis.com/<orgid>): ~740 layers
  found but dropped — a multi-tenant org cannot be attributed to a government
  publisher (Esri's own demographics org P3ePLMYs2RVChkJx among them). Batch-46
  hazard; left out rather than mislabelled.
- **Huge polyline layers that 500 on the full geometry query** (national-map
  contours, hydrography): their count endpoint answers but the data query
  errors; rejected as non-emitting.

## CN / JP / IN / KR — why they contributed ~nothing from this environment
Measured, not assumed:
- **JP**: already saturated (batch 57 = 3,622). 0 new CKAN portals; JP gov
  ArcGIS roots returned 0 services.
- **CN / IN / KR**: blocked by two independent walls.
  1. **Egress** — most gov hosts reset mid-exchange from this container
     (`ws_closed_mid_exchange` / 502 CONNECT), and reachability *flaps*
     (api.odcloud.kr 401 then 000 minutes later). A key cannot fix an
     unreliable tunnel, and a host that intermittently fails cannot be
     emit-verified, so nothing was registered (house rule 1).
  2. **Auth shape** — their data APIs use credentials this session's
     credential channel cannot inject:
     - `data.go.kr` / `apis.data.go.kr` (reachable, HTTP 400 unauth):
       `serviceKey` **query param**.
     - `api.data.gov.in` (egress-reset): `api-key` **query param**.
     - `api.odcloud.kr`: `Authorization: Infuser <key>` (custom prefix).
     - `data.sh.gov.cn` (reachable, HTTP 405): **POST** JSON body.
     Query-param and custom-prefix keys are consumed by the engine's
     `key_env` → `{key}` URL substitution in a real deployment, not by proxy
     header-injection here.

### Handoff for a deployment with normal egress + the operator's own keys
Author these as hpengine rows with `key_env=<ENV>` and a `{key}` token, then
probe/emit-verify there before registering (do NOT register unverified):
- KR data.go.kr: `...&serviceKey={key}&type=json`, env e.g. `JO_DATAGOKR_KEY`.
  Reachable from a normal host; `apis.data.go.kr` answered 400 unauth here.
- IN data.gov.in: `https://api.data.gov.in/resource/<id>?api-key={key}&format=json`,
  env `JO_DATAGOVIN_KEY`. OGD CKAN-style resources, `offset`/`limit` paging.
- KR odcloud: header `Authorization: Infuser {key}`, `page`/`perPage` paging.
- CN provincial (Shanghai data.sh.gov.cn): POST JSON list endpoint
  (`post_body=...`), keyless but needs the dataset id + the portal's own body
  shape measured per dataset.
Each stores the honest `collector-fetch-failure` / gated notice until its key
is set — rule 1's gated-disclosure path, as batch 31's 14 keyed rows do.
