# Batch 29 (agent A) — verified sources: webcams + forgotten

Measured 2026-09-14/15 in a private WSL build tree synced to the repo's current `native/` (engine included) with only the two `hp3b29a_*` tables added. The per-row table is ONE run of the final binary: `probe_hp_batch.py --check-filter`, `audit_registry_emit.py` (emitted, stored and DB rows read independently against a fresh warm-template DB per source), and `--run <id> <entity>` for the three entity pivots, which a sweep cannot measure. Nothing is estimated.

## Gates and repairs

### How the 198 rows were arrived at

- **Authored by this agent: 164 candidates**, every count read live by the manifest builder at authoring time.
  - `webcams`: 112 candidates. 77 are government ArcGIS camera layers generated from layer metadata: `returnCountOnly`, object-id field, `maxRecordCount`, and pagination support. 35 are hand-written non-ArcGIS camera inventories: IBI 511 map-icon and list feeds, DriveBC, DelDOT, NSW Live Traffic, Hong Kong TD, DGT DATEX II, Madrid Informo, Vegagerðin, Taiwan TDX and NOAA NDBC.
  - `forgotten`: 52 candidates, all unregistered open-data catalogues (14 CKAN incl. 3 honoured-filter search pivots, 8 OpenDataSoft, 25 Socrata-by-domain, 2 DKAN 2).
- **Adopted from the stray sub-agent run: 73 rows.** They came from `candidate-sources-batch29.webcam.txt` and were renamed JO29_ → JO29A_. Rows were matched on their layer or endpoint:
  - 20 were the same endpoint as a row authored here, so the stray copy was dropped.
  - 2 duplicated a camera set already covered here (City of Gainesville, DriveBC), so the stray copy was dropped.
  - 51 were adopted.
  - 8 rows authored here were dropped instead, because an adopted row reads the same cameras from the agency's primary feed: MiDrive, TripCheck, Plymouth, Austin, Baton Rouge traffic, Vancouver, ALERTCalifornia, and 511NY, where the adopted `getcameras` rows carry names and URLs.
  - After adoption, the stray run changed its own manifest once more, at 23:42. That later state was diffed and its three real fixes are carried here: `page_zero_based=1` on the ArcGIS rows, the Honolulu key, and TripCheck's `Accept` header.
- **Dropped after probing, own rows:**
  - 8 Taiwan TDX CCTV rows: HTTP 401 "Valid API Key Required" to the engine and to `JapanOSINT/1.0`. Only a browser User-Agent was served, and impersonating one to get past a key requirement is not done.
  - 1 Buenos Aires Province CKAN row: HTTP 200 with the JSON body cut at 97,306 and 80,922 of 761,461 bytes on two probes.
- **Result: 198 rows**, 147 `webcams` and 51 `forgotten`. Every drop is recorded with its measured reason in `docs/rejected-sources-batch29.webcams.tsv`.

### First-pass gates

- `probe_hp_batch.py --check-filter` on the 164 own candidates gave 149 PASS, 14 HTTP_ERR and 1 UNPARSEABLE.
  - Six US CKAN portals hosted on one platform (data.ca.gov, San Antonio, Houston, Milwaukee, Virginia, Oklahoma) answer HTTP 502 to the engine's full User-Agent string and 200 to `JapanOSINT/1.0`. Those rows declare `header1=User-Agent: JapanOSINT/1.0`, which is honest self-identification with no browser impersonation, and they re-probed PASS.
  - The 51 adopted rows probed 51/51 PASS.
  - No pivot row was FILTER_IGNORED. Measured filters: `trafik` 42 vs impossible 0, `agua` 69 vs 0, `liikenne` 132 vs 1.
- `batch_exclusions.py --bin`: 0 collisions against the registry. The 2 near-misses are geospatial.jp CSV resources, read by hand: `geospatial_jp_ckan.c` reads only `package_search` metadata, never a resource file.
- `audit_batch_pagination.py` and `audit_batch_reachable.py`: 0 findings at every stage.
- `audit_page_param.py` on the two generated tables: 49 of the 141 paged rows also carry their page key in the URL, as `start=0` on CKAN and `offset=0` on OpenDataSoft and the Socrata catalogue; the whole table directory has 152 such rows. This is the watch-lint CLAUDE.md describes, not a defect. The engine replaces the bound value rather than appending a second one (`hp_url_set_param`, pinned by `hptest` 9f-bis), and these exact rows walk to completion. `JO29A_FGT_VIRGINIA_CKAN` binds `start=0` and reaches 32,458 of 32,458 distinct ids over 33 pages, which is only possible if every page is requested at its own offset.
- Cross-agent check: the registry exclusion cannot see agent B's batch-29 rows, which were not yet in the repo. So the 198 endpoints were normalised and compared directly against agent B's 153 rows (`deepweb` and `govosint` manifests), with query-order and paging parameters ignored. Result: 0 exact endpoint overlaps. The only shared host is data.winnipeg.ca, and the two rows read different Socrata resources: `JO29A_CAM_CA_MB_WINNIPEG` the traffic camera table, `JO29B_WINNIPEG_FIPPA_RESPONSES` the access-to-information responses.

### Defects found by running the engine, not the prober, and how each was repaired

None of these showed in the probe. Each was found by comparing `emitted`, `stored` and the upstream's own total.

1. **CKAN default order is not stable across pages.** `JO29A_FGT_VIRGINIA_CKAN` emitted 32,458 of 32,458 and stored 28,943 (`UID-COLLISION: 3515`). The id key was right. A direct walk of 33 pages of 1,000 in the default order (score desc, metadata_modified desc) returned 32,458 rows but only 29,157 distinct ids: 2,992 datasets repeated and 3,301 never returned. The same walk with `sort=id asc` returned all 32,458 distinct ids. All 16 paged CKAN rows now declare `sort=id%20asc`.
2. **Socrata Discovery catalogue pages unordered.** `JO29A_FGT_BAYAREA_METRO_SOCRATA` emitted 3,417 and stored 3,414. A direct walk returned 3,417 rows with 3,415 distinct ids, and with `order=dataset_id` all 3,417 were distinct. All 25 Socrata rows now declare `order=dataset_id`, and the 8 OpenDataSoft catalogue rows declare `order_by=dataset_id`.
3. **`resultOffset` start coercion.** The engine this tree was first built from matched the offset parameter with a case-sensitive `strstr(…,"offset")`, so `resultOffset` walked 0, 1001, 2001 and lost one record per page boundary. Measured on `JO29A_CAM_US_DC_CCTV_STREET`: 1,059 of 1,060. The repo's `lib/hpengine.c` gained a case-insensitive fix during this session, and the final build uses it. Every `resultOffset` row also declares `page_zero_based=1`, which is correct on both engine versions.
4. **Unordered offset paging on the adopted ArcGIS and OpenDataSoft rows.** 16 adopted ArcGIS rows now carry `orderByFields=<object id>`. 4 adopted OpenDataSoft rows declare `order_by` on a field measured unique across all records (Vancouver `mapid`, QLDTraffic `id`, Orléans `id_webcam`, FGC `id`). Geelong has no unique sortable field, so it now reads `exports/json` on its source domain, www.geelongdataexchange.com.au: all 115 records in one response, probe PASS. The federated data.opendatasoft.com export URL answered 404 to the probe's encoded request.
5. **TripCheck refuses the engine's default Accept header.** `cctvinventory.js` answers HTTP 406 to `Accept: application/json` and 200 to `*/*`. The row declares `header1=Accept: */*`, and a hand run gave 1,155 of 1,155 stored.
6. **Honolulu identity.** `description+location` gave 248 distinct values over 249 distinct upstream records, so two different cameras merged. The key is now `description+cameraimageurl+location.latitude+location.longitude`, which gives 249 distinct values. The upstream's other 4 rows (253 − 249) are byte-identical repeats and correctly store once. Colombia's municipal CCTV table is the same situation: 50 rows, 48 distinct.
7. **PEMA HIVIS short page.** During the first sweep the layer once answered a 2,000-record request with 500 records and no `exceededTransferLimit`, which a 2,000 page size reads as the last page (500 of 1,321 emitted). A hand run minutes later got all 1,321. The row now pages at 500, so a short answer cannot end the walk; a direct 500-per-page walk returned all 1,321 distinct OBJECTIDs in 3 pages.

### Final binary

The build tree was re-synced from the repo's current `native/` (engine included; `lib/hpengine.c` byte-identical to the repo, which contains the case-insensitive offset fix), `obj/` purged, both tables regenerated with `gen_hp_batch.py --prefix hp3b29a --batch 29 --force` (only these two files), and every gate re-run against that one binary:

- `make -j12` (clean): rc=0, **0 warnings**, **198 JO29A_ ids registered**, 0 unsuffixed JO29_ ids (nothing registered twice).
- Gate 1 `probe_hp_batch.py --check-filter`, all rows: **198 / 198 PASS**, 0 FILTER_IGNORED.
- Gate 2 `batch_exclusions.py --bin --skip-prefix hp3b29a`: the 198 reported collisions are all DUP-ID self-matches (the binary registers these ids); **0 name a non-JO29A id**, **0 endpoint collisions** outside the two tables; the 2 geospatial.jp near-misses are cleared as above.
- Gate 3: `audit_batch_pagination.py` 0 of 198, `audit_batch_reachable.py` 0 of 198.
- Gate 5 `audit_registry_emit.py`, all 198 ids, fresh warm-template DB per source, `--timeout 1500` so the 33-page Virginia walk completes: **OK 193, COLLISION 2, EMITS_NOTHING 3**; **137,871 records emitted, 137,865 rows stored**.
  - The 2 COLLISION rows are upstream byte-identical repeats, measured record by record, not key defects: `JO29A_CAM_US_HI_HONOLULU` 253 emitted / 249 stored = 249 distinct records upstream; `JO29A_CAM_CO_META_MUNICIPAL_CCTV` 50 emitted / 48 stored = 48 distinct records upstream (two posts listed twice, byte-identical). Every distinct record is stored.
  - The 3 EMITS_NOTHING rows are the entity pivots, which a sweep cannot measure because it supplies no entity; they were run by hand with real entities (below).
- Entity pivots, `--run <id> <entity>` on the final binary against a fresh DB (emitted read off the engine's run line, stored counted in `intel_items`):
  - `JO29A_FGT_OPENDATA_DK_SEARCH` `trafik`: emitted 42 of 42 across 2 pages, stored 42.
  - `JO29A_FGT_GVA_DADESOBERTES_SEARCH` `agua`: emitted 69 of 69 across 2 pages, stored 69.
  - `JO29A_FGT_HELSINKI_HRI_SEARCH` `liikenne`: emitted 132 of 132 across 3 pages, stored 132.
- Gate 6: `make lint-sources` OK.
- Gate 6: `make audit-sources` 0 findings (strict set `collectors/pivot/table/hp*_*.c`: 0); `make source-floor` OK (16565 >= 16271).

## Beat `webcams` — 147 rows

| id | mode | probe | probe items | emitted / available | stored | verdict |
| --- | --- | --- | --- | --- | --- | --- |
| `JO29A_CAM_BALTIMORE_CITIWATCH_CCTV` | json | PASS | 861 | 861 / 861 | 861 | OK |
| `JO29A_CAM_BANGKOK_BMA_CCTV` | json | PASS | 591 | 591 / 591 | 591 | OK |
| `JO29A_CAM_MASSDOT_CCTV` | json | PASS | 845 | 845 / 845 | 845 | OK |
| `JO29A_CAM_PENNDOT_TSAMS_CAMERAS` | json | PASS | 1749 | 1749 / 1749 | 1749 | OK |
| `JO29A_CAM_BEDFORD_BOROUGH_CCTV` | json | PASS | 230 | 230 / 230 | 230 | OK |
| `JO29A_CAM_BATONROUGE_PTZ_CAMERAS` | json | PASS | 88 | 88 / 88 | 88 | OK |
| `JO29A_CAM_YORK_UK_CCTV` | json | PASS | 222 | 222 / 222 | 222 | OK |
| `JO29A_CAM_BRISTOL_COUNCIL_CCTV` | json | PASS | 1532 | 1532 / 1532 | 1532 | OK |
| `JO29A_CAM_DC_AUTOMATED_SAFETY_CAMERAS_43` | json | PASS | 327 | 327 / 327 | 327 | OK |
| `JO29A_CAM_DC_AUTOMATED_SAFETY_CAMERAS_47` | json | PASS | 562 | 562 / 562 | 562 | OK |
| `JO29A_CAM_DC_TRAFFIC_CAMERAS` | json | PASS | 235 | 235 / 235 | 235 | OK |
| `JO29A_CAM_DC_DDOT_CCTV_FEEDS` | json | PASS | 139 | 139 / 139 | 139 | OK |
| `JO29A_CAM_HK_HYD_TRAFFIC_ENFORCEMENT_CAMERAS` | json | PASS | 155 | 155 / 155 | 155 | OK |
| `JO29A_CAM_IOWA_DOT_TRAFFIC_CAMERAS` | json | PASS | 1251 | 1251 / 1251 | 1251 | OK |
| `JO29A_CAM_IOWA_DOT_RWIS_CAMERAS` | json | PASS | 307 | 307 / 307 | 307 | OK |
| `JO29A_CAM_CALOES_CALIFORNIA_WEBCAMS` | json | PASS | 2000 | 4346 / 4346 | 4346 | OK |
| `JO29A_CAM_CALOES_COASTAL_WEBCAMS` | json | PASS | 157 | 157 / 157 | 157 | OK |
| `JO29A_CAM_NZTA_WAKAKOTAHI_CAMERAS` | json | PASS | 224 | 224 / 224 | 224 | OK |
| `JO29A_CAM_OTTAWA_SPEED_ENFORCEMENT_CAMERAS` | json | PASS | 60 | 60 / 60 | 60 | OK |
| `JO29A_CAM_OTTAWA_REDLIGHT_CAMERAS` | json | PASS | 88 | 88 / 88 | 88 | OK |
| `JO29A_CAM_NCDOT_TIMS_CAMERAS` | json | PASS | 1112 | 1112 / 1112 | 1112 | OK |
| `JO29A_CAM_HOUSTON_TRANSTAR_CAMERAS` | json | PASS | 453 | 453 / 453 | 453 | OK |
| `JO29A_CAM_NOLA_RTCC_CAMERAS` | json | PASS | 528 | 528 / 528 | 528 | OK |
| `JO29A_CAM_NOLA_TRAFFIC_SAFETY_CAMERAS` | json | PASS | 139 | 139 / 139 | 139 | OK |
| `JO29A_CAM_SEATTLE_TRAFFIC_SAFETY_CAMERAS` | json | PASS | 114 | 114 / 114 | 114 | OK |
| `JO29A_CAM_SEATTLE_SPD_CCTV_PILOT` | json | PASS | 86 | 86 / 86 | 86 | OK |
| `JO29A_CAM_SEATTLE_TRAFFIC_CAMERAS` | json | PASS | 660 | 660 / 660 | 660 | OK |
| `JO29A_CAM_EPA_REGIONAL_HAZE_WEBCAMS` | json | PASS | 63 | 63 / 63 | 63 | OK |
| `JO29A_CAM_BROOKHAVEN_FLOCK_CAMERAS` | json | PASS | 124 | 124 / 124 | 124 | OK |
| `JO29A_CAM_LAPALMA_CABILDO_WEBCAMS` | json | PASS | 60 | 60 / 60 | 60 | OK |
| `JO29A_CAM_OREGON_ALERTWEST_CAMERAS` | json | PASS | 69 | 69 / 69 | 69 | OK |
| `JO29A_CAM_RALEIGH_TRAFFIC_CAMERAS` | json | PASS | 382 | 382 / 382 | 382 | OK |
| `JO29A_CAM_GEMA_GDOT_511_CAMERAS` | json | PASS | 1000 | 7083 / 7083 | 7083 | OK |
| `JO29A_CAM_OKI_ARTIMIS_CAMERAS` | json | PASS | 109 | 109 / 109 | 109 | OK |
| `JO29A_CAM_IDAHO_STATE_WEBCAMS` | json | PASS | 237 | 237 / 237 | 237 | OK |
| `JO29A_CAM_ADOT_EXISTING_CCTV` | json | PASS | 526 | 526 / 526 | 526 | OK |
| `JO29A_CAM_LOUISIANA_DOTD_CAMERAS` | json | PASS | 435 | 435 / 435 | 435 | OK |
| `JO29A_CAM_OAKLAND_PARK_CAMERAS` | json | PASS | 135 | 135 / 135 | 135 | OK |
| `JO29A_CAM_NJSEA_TRAFFIC_CAMERAS` | json | PASS | 67 | 67 / 67 | 67 | OK |
| `JO29A_CAM_MRMPO_CCTV` | json | PASS | 404 | 404 / 404 | 404 | OK |
| `JO29A_CAM_SANBERNARDINO_SECURITY_CAMERAS` | json | PASS | 316 | 316 / 316 | 316 | OK |
| `JO29A_CAM_KYTC_TRAFFIC_CAMERAS` | json | PASS | 254 | 254 / 254 | 254 | OK |
| `JO29A_CAM_GAINESVILLE_TRAFFIC_CAMERAS` | json | PASS | 118 | 118 / 118 | 118 | OK |
| `JO29A_CAM_IDOT_TRAFFIC_CAMERAS` | json | PASS | 1000 | 3675 / 3675 | 3675 | OK |
| `JO29A_CAM_BRISBANE_COUNCIL_CAMERAS` | json | PASS | 44 | 44 / 44 | 44 | OK |
| `JO29A_CAM_KIRKLAND_TRAFFIC_CAMERAS` | json | PASS | 57 | 57 / 57 | 57 | OK |
| `JO29A_CAM_PEMA_HIVIS_CAMERAS` | json | PASS | 500 | 1321 / 1321 | 1321 | OK |
| `JO29A_CAM_CORONA_CITY_CAMERAS` | json | PASS | 465 | 465 / 465 | 465 | OK |
| `JO29A_CAM_CALEDON_CCTV` | json | PASS | 42 | 42 / 42 | 42 | OK |
| `JO29A_CAM_TORONTO_TRAFFIC_CAMERAS` | json | PASS | 336 | 336 / 336 | 336 | OK |
| `JO29A_CAM_SOUTHFULTON_POLICE_CAMERAS` | json | PASS | 44 | 44 / 44 | 44 | OK |
| `JO29A_CAM_TEMECULA_FLOCK_CAMERAS` | json | PASS | 50 | 50 / 50 | 50 | OK |
| `JO29A_CAM_SURREY_TRAFFIC_CAMERAS` | json | PASS | 703 | 703 / 703 | 703 | OK |
| `JO29A_CAM_LOGAN_SAFETY_CAMERAS` | json | PASS | 311 | 311 / 311 | 311 | OK |
| `JO29A_CAM_EAST_DUNBARTONSHIRE_CCTV` | json | PASS | 46 | 46 / 46 | 46 | OK |
| `JO29A_CAM_CHICAGO_REDLIGHT_CAMERAS` | json | PASS | 300 | 300 / 300 | 300 | OK |
| `JO29A_CAM_CHICAGO_SPEED_CAMERAS` | json | PASS | 209 | 209 / 209 | 209 | OK |
| `JO29A_CAM_COLLEGEPARK_CAMERAS` | json | PASS | 48 | 48 / 48 | 48 | OK |
| `JO29A_CAM_PERTH_SECURITY_CAMERAS` | json | PASS | 742 | 742 / 742 | 742 | OK |
| `JO29A_CAM_ROCHESTER_PD_CAMERAS` | json | PASS | 177 | 177 / 177 | 177 | OK |
| `JO29A_CAM_NL_HIGHWAY_CAMERAS` | json | PASS | 48 | 48 / 48 | 48 | OK |
| `JO29A_CAM_AUBURN_CITY_CAMERAS` | json | PASS | 267 | 267 / 267 | 267 | OK |
| `JO29A_CAM_WATERLOO_FORMER_SPEED_CAMERAS` | json | PASS | 47 | 47 / 47 | 47 | OK |
| `JO29A_CAM_UPPER_AUSTRIA_ASFINAG_WEBCAMS` | json | PASS | 160 | 160 / 160 | 160 | OK |
| `JO29A_CAM_UPPER_AUSTRIA_WEBCAMS` | json | PASS | 133 | 133 / 133 | 133 | OK |
| `JO29A_CAM_YORK_REGION_TRAFFIC_CAMERAS` | json | PASS | 394 | 394 / 394 | 394 | OK |
| `JO29A_CAM_YORK_REGION_REDLIGHT_CAMERAS` | json | PASS | 55 | 55 / 55 | 55 | OK |
| `JO29A_CAM_NORTH_AYRSHIRE_CCTV` | json | PASS | 108 | 108 / 108 | 108 | OK |
| `JO29A_CAM_RECIFE_GCMR_CAMERAS` | json | PASS | 86 | 86 / 86 | 86 | OK |
| `JO29A_CAM_GBUAPCD_CAMERA_NETWORK` | json | PASS | 57 | 57 / 57 | 57 | OK |
| `JO29A_CAM_511GA_MAPICONS` | json | PASS | 4331 | 4331 / 4331 | 4331 | OK |
| `JO29A_CAM_FL511_MAPICONS` | json | PASS | 4955 | 4955 / 4955 | 4955 | OK |
| `JO29A_CAM_511PA_MAPICONS` | json | PASS | 1543 | 1543 / 1543 | 1543 | OK |
| `JO29A_CAM_511ON_MAPICONS` | json | PASS | 944 | 944 / 944 | 944 | OK |
| `JO29A_CAM_AZ511_MAPICONS` | json | PASS | 644 | 644 / 644 | 644 | OK |
| `JO29A_CAM_511AB_MAPICONS` | json | PASS | 354 | 354 / 354 | 354 | OK |
| `JO29A_CAM_NEWENGLAND511_MAPICONS` | json | PASS | 406 | 406 / 406 | 406 | OK |
| `JO29A_CAM_511LA_MAPICONS` | json | PASS | 336 | 336 / 336 | 336 | OK |
| `JO29A_CAM_NVROADS_MAPICONS` | json | PASS | 640 | 640 / 640 | 640 | OK |
| `JO29A_CAM_CTROADS_MAPICONS` | json | PASS | 347 | 347 / 347 | 347 | OK |
| `JO29A_CAM_511IDAHO_MAPICONS` | json | PASS | 457 | 457 / 457 | 457 | OK |
| `JO29A_CAM_511ALASKA_MAPICONS` | json | PASS | 129 | 129 / 129 | 129 | OK |
| `JO29A_CAM_511NB_LIST` | json | PASS | 57 | 57 / 57 | 57 | OK |
| `JO29A_CAM_511NS_LIST` | json | PASS | 57 | 57 / 57 | 57 | OK |
| `JO29A_CAM_SK_HIGHWAY_HOTLINE_LIST` | json | PASS | 56 | 56 / 56 | 56 | OK |
| `JO29A_CAM_MANITOBA511_LIST` | json | PASS | 49 | 49 / 49 | 49 | OK |
| `JO29A_CAM_511YUKON_LIST` | json | PASS | 15 | 15 / 15 | 15 | OK |
| `JO29A_CAM_511NL_LIST` | json | PASS | 48 | 48 / 48 | 48 | OK |
| `JO29A_CAM_DRIVEBC_WEBCAMS` | json | PASS | 1077 | 1077 / 1077 | 1077 | OK |
| `JO29A_CAM_DELDOT_VIDEO_CAMERAS` | json | PASS | 361 | 361 / 361 | 361 | OK |
| `JO29A_CAM_NSW_LIVETRAFFIC_ALLFEEDS` | json | PASS | 3221 | 3224 / 3224 | 3224 | OK |
| `JO29A_CAM_HK_TD_TRAFFIC_SNAPSHOTS` | xml | PASS | 1013 | 1013 / 1013 | 1013 | OK |
| `JO29A_CAM_DGT_DATEX2_CAMERAS` | xml | PASS | 1948 | 1948 / 1948 | 1948 | OK |
| `JO29A_CAM_MADRID_INFORMO_CCTV` | xml | PASS | 357 | 357 / 357 | 357 | OK |
| `JO29A_CAM_ICELAND_VEGAGERDIN_WEBCAMS` | json | PASS | 500 | 500 / 500 | 500 | OK |
| `JO29A_CAM_NOAA_NDBC_BUOYCAMS` | json | PASS | 90 | 90 / 90 | 90 | OK |
| `JO29A_CAM_US_MI_MIDRIVE` | json | PASS | 802 | 802 / 802 | 802 | OK |
| `JO29A_CAM_US_VA_VDOT_CAMS` | json | PASS | 1668 | 1669 / 1669 | 1669 | OK |
| `JO29A_CAM_US_NY_511NY_CAMERAS` | json | PASS | 2933 | 2933 / 2933 | 2933 | OK |
| `JO29A_CAM_US_DC_CCTV_STREET` | json | PASS | 1000 | 1060 / 1060 | 1060 | OK |
| `JO29A_CAM_US_WA_KINGCOUNTY` | json | PASS | 125 | 125 / 125 | 125 | OK |
| `JO29A_CAM_US_OR_TRIPCHECK` | json | PASS | 1155 | 1155 / 1155 | 1155 | OK |
| `JO29A_CAM_US_ND_NDDOT` | json | PASS | 189 | 189 / 189 | 189 | OK |
| `JO29A_CAM_US_TX_MONTGOMERY_COUNTY` | json | PASS | 141 | 141 / 141 | 141 | OK |
| `JO29A_CAM_US_KY_LEXINGTON` | json | PASS | 108 | 108 / 108 | 108 | OK |
| `JO29A_CAM_US_TX_AUSTIN` | json | PASS | 1000 | 1005 / 1005 | 1005 | OK |
| `JO29A_CAM_US_HI_HONOLULU` | json | PASS | 253 | 253 / 253 | 249 | COLLISION |
| `JO29A_CAM_US_MD_CHART` | json | PASS | 451 | 451 / 451 | 451 | OK |
| `JO29A_CAM_US_LA_BATON_ROUGE` | json | PASS | 118 | 118 / 118 | 118 | OK |
| `JO29A_CAM_US_AK_DOT_RWIS` | json | PASS | 178 | 178 / 178 | 178 | OK |
| `JO29A_CAM_US_CA_ALERTCALIFORNIA` | json | PASS | 2229 | 2229 / 2229 | 2229 | OK |
| `JO29A_CAM_US_GCOOS_GULF_WEBCAMS` | json | PASS | 101 | 101 / 101 | 101 | OK |
| `JO29A_CAM_US_ID_ITD_CCTV` | json | PASS | 82 | 82 / 82 | 82 | OK |
| `JO29A_CAM_NZ_HORIZONS_WEBCAMS` | json | PASS | 54 | 54 / 54 | 54 | OK |
| `JO29A_CAM_NL_ROTTERDAM_PORT_WEBCAMS` | json | PASS | 13 | 13 / 13 | 13 | OK |
| `JO29A_CAM_UK_DURHAM_TRAFFIC_WEBCAMS` | json | PASS | 33 | 33 / 33 | 33 | OK |
| `JO29A_CAM_UK_DURHAM_ROAD_WEATHER` | json | PASS | 9 | 9 / 9 | 9 | OK |
| `JO29A_CAM_UK_SHEFFIELD_CCTV` | json | PASS | 212 | 212 / 212 | 212 | OK |
| `JO29A_CAM_AU_BRISBANE_RESILIENCE` | json | PASS | 13 | 13 / 13 | 13 | OK |
| `JO29A_CAM_AU_SYDNEY_CCTV` | json | PASS | 106 | 106 / 106 | 106 | OK |
| `JO29A_CAM_AU_MORETON_BAY_CCTV` | json | PASS | 825 | 825 / 825 | 825 | OK |
| `JO29A_CAM_NZ_QLDC_CCTV` | json | PASS | 240 | 240 / 240 | 240 | OK |
| `JO29A_CAM_CA_QC_QUEBEC511` | json | PASS | 678 | 678 / 678 | 678 | OK |
| `JO29A_CAM_CA_ON_OTTAWA` | json | PASS | 428 | 428 / 428 | 428 | OK |
| `JO29A_CAM_CA_AB_CALGARY` | json | PASS | 215 | 215 / 215 | 215 | OK |
| `JO29A_CAM_CA_BC_VANCOUVER` | json | PASS | 100 | 218 / 218 | 218 | OK |
| `JO29A_CAM_CA_MB_WINNIPEG` | json | PASS | 161 | 161 / 161 | 161 | OK |
| `JO29A_CAM_AU_NSW_LIVETRAFFIC` | json | PASS | 217 | 217 / 217 | 217 | OK |
| `JO29A_CAM_AU_QLDTRAFFIC_WEBCAMS` | json | PASS | 100 | 136 / 136 | 136 | OK |
| `JO29A_CAM_LT_EISMOINFO_CAMERAS` | json | PASS | 296 | 296 / 296 | 296 | OK |
| `JO29A_CAM_FR_ORLEANS_METROPOLE` | json | PASS | 16 | 16 / 16 | 16 | OK |
| `JO29A_CAM_FR_ANGLET_BEACH_WEBCAMS` | json | PASS | 7 | 7 / 7 | 7 | OK |
| `JO29A_CAM_ES_FGC_SKI_WEBCAMS` | json | PASS | 30 | 30 / 30 | 30 | OK |
| `JO29A_CAM_BE_BRUSSELS_WEBCAMS` | json | PASS | 3 | 3 / 3 | 3 | OK |
| `JO29A_CAM_AU_GEELONG_CCTV` | json | PASS | 115 | 115 / 115 | 115 | OK |
| `JO29A_CAM_UK_PLYMOUTH_TRAFFIC` | csv | PASS | 43 | 43 / 43 | 43 | OK |
| `JO29A_CAM_UK_PLYMOUTH_CCTV` | csv | PASS | 136 | 136 / 136 | 136 | OK |
| `JO29A_CAM_CO_META_MUNICIPAL_CCTV` | json | PASS | 50 | 50 / 50 | 48 | COLLISION |
| `JO29A_CAM_JP_TOKUSHIMA_LIVECAMERA` | csv | PASS | 83 | 82 / 82 | 82 | OK |
| `JO29A_CAM_JP_TOKYO_PORT_SEA_CAMERAS` | csv | PASS | 21 | 20 / 20 | 20 | OK |
| `JO29A_CAM_JP_TOKYO_IZU_OGASAWARA_PORTS` | csv | PASS | 17 | 17 / 17 | 17 | OK |
| `JO29A_CAM_JP_TOKYO_RIVER_MONITORING` | csv | PASS | 110 | 106 / 106 | 106 | OK |
| `JO29A_CAM_JP_CHINO_RIVER_CAMERAS` | csv | PASS | 3 | 3 / 3 | 3 | OK |
| `JO29A_CAM_JP_KYOTANGO_LIVECAMERA` | csv | PASS | 8 | 8 / 8 | 8 | OK |
| `JO29A_CAM_JP_MINAMIASO_LIVECAMERA` | csv | PASS | 1 | 1 / 1 | 1 | OK |
| `JO29A_CAM_JP_YAMATO_LIVECAMERA` | csv | PASS | 1 | 1 / 1 | 1 | OK |
| `JO29A_CAM_JP_ICHIKAWA_STREET_CCTV` | csv | PASS | 240 | 240 / 240 | 240 | OK |

Verdicts: COLLISION 2, OK 145

## Beat `forgotten` — 51 rows

| id | mode | probe | probe items | emitted / available | stored | verdict |
| --- | --- | --- | --- | --- | --- | --- |
| `JO29A_FGT_CA_DATA_CKAN` | json | PASS | 1000 | 4581 / 4581 | 4581 | OK |
| `JO29A_FGT_SANANTONIO_CKAN` | json | PASS | 100 | 164 / 164 | 164 | OK |
| `JO29A_FGT_HOUSTON_CKAN` | json | PASS | 94 | 94 / 94 | 94 | OK |
| `JO29A_FGT_MILWAUKEE_CKAN` | json | PASS | 100 | 186 / 186 | 186 | OK |
| `JO29A_FGT_VIRGINIA_CKAN` | json | PASS | 1000 | 32458 / 32458 | 32458 | OK |
| `JO29A_FGT_OPENDATA_DK_CKAN` | json | PASS | 100 | 631 / 631 | 631 | OK |
| `JO29A_FGT_GVA_DADESOBERTES_CKAN` | json | PASS | 100 | 300 / 300 | 300 | OK |
| `JO29A_FGT_BELOHORIZONTE_CKAN` | json | PASS | 100 | 606 / 606 | 606 | OK |
| `JO29A_FGT_ALAGOAS_CKAN` | json | PASS | 100 | 394 / 394 | 394 | OK |
| `JO29A_FGT_OKLAHOMA_CKAN` | json | PASS | 100 | 395 / 395 | 395 | OK |
| `JO29A_FGT_KYIV_CITY_CKAN` | json | PASS | 93 | 93 / 93 | 93 | OK |
| `JO29A_FGT_OPENCOM_NO_CKAN` | json | PASS | 100 | 360 / 360 | 360 | OK |
| `JO29A_FGT_HELSINKI_HRI_CKAN` | json | PASS | 100 | 593 / 593 | 593 | OK |
| `JO29A_FGT_BRISBANE_ODS` | json | PASS | 100 | 446 / 446 | 446 | OK |
| `JO29A_FGT_ORLEANS_METROPOLE_ODS` | json | PASS | 100 | 213 / 213 | 213 | OK |
| `JO29A_FGT_HAUTS_DE_SEINE_ODS` | json | PASS | 100 | 253 / 253 | 253 | OK |
| `JO29A_FGT_BRUXELLES_VILLE_ODS` | json | PASS | 100 | 208 / 208 | 208 | OK |
| `JO29A_FGT_WALLONIA_ODWB_ODS` | json | PASS | 100 | 1290 / 1290 | 1290 | OK |
| `JO29A_FGT_POTSDAM_ODS` | json | PASS | 100 | 114 / 114 | 114 | OK |
| `JO29A_FGT_CAISSE_DEPOTS_ODS` | json | PASS | 87 | 87 / 87 | 87 | OK |
| `JO29A_FGT_MULHOUSE_ODS` | json | PASS | 100 | 146 / 146 | 146 | OK |
| `JO29A_FGT_SOMERVILLE_SOCRATA` | json | PASS | 53 | 53 / 53 | 53 | OK |
| `JO29A_FGT_ACT_GOV_SOCRATA` | json | PASS | 100 | 784 / 784 | 784 | OK |
| `JO29A_FGT_VERMONT_SOCRATA` | json | PASS | 100 | 270 / 270 | 270 | OK |
| `JO29A_FGT_BUFFALO_SOCRATA` | json | PASS | 100 | 106 / 106 | 106 | OK |
| `JO29A_FGT_NORFOLK_SOCRATA` | json | PASS | 100 | 221 / 221 | 221 | OK |
| `JO29A_FGT_GAINESVILLE_SOCRATA` | json | PASS | 100 | 209 / 209 | 209 | OK |
| `JO29A_FGT_PRINCE_GEORGES_SOCRATA` | json | PASS | 100 | 248 / 248 | 248 | OK |
| `JO29A_FGT_EDMONTON_SOCRATA` | json | PASS | 100 | 2088 / 2088 | 2088 | OK |
| `JO29A_FGT_CALGARY_SOCRATA` | json | PASS | 100 | 937 / 937 | 937 | OK |
| `JO29A_FGT_WINNIPEG_SOCRATA` | json | PASS | 100 | 521 / 521 | 521 | OK |
| `JO29A_FGT_NEW_ORLEANS_SOCRATA` | json | PASS | 100 | 298 / 298 | 298 | OK |
| `JO29A_FGT_CINCINNATI_SOCRATA` | json | PASS | 100 | 163 / 163 | 163 | OK |
| `JO29A_FGT_FORTWORTH_SOCRATA` | json | PASS | 14 | 14 / 14 | 14 | OK |
| `JO29A_FGT_HONOLULU_SOCRATA` | json | PASS | 100 | 451 / 451 | 451 | OK |
| `JO29A_FGT_PROVIDENCE_SOCRATA` | json | PASS | 100 | 297 / 297 | 297 | OK |
| `JO29A_FGT_ROSEVILLE_SOCRATA` | json | PASS | 100 | 206 / 206 | 206 | OK |
| `JO29A_FGT_BAYAREA_METRO_SOCRATA` | json | PASS | 100 | 3417 / 3417 | 3417 | OK |
| `JO29A_FGT_BATONROUGE_SOCRATA` | json | PASS | 100 | 498 / 498 | 498 | OK |
| `JO29A_FGT_MESA_SOCRATA` | json | PASS | 41 | 41 / 41 | 41 | OK |
| `JO29A_FGT_BLOOMINGTON_SOCRATA` | json | PASS | 100 | 358 / 358 | 358 | OK |
| `JO29A_FGT_EVERETT_SOCRATA` | json | PASS | 100 | 332 / 332 | 332 | OK |
| `JO29A_FGT_SANTACLARA_COUNTY_SOCRATA` | json | PASS | 100 | 1758 / 1758 | 1758 | OK |
| `JO29A_FGT_SONOMA_COUNTY_SOCRATA` | json | PASS | 100 | 139 / 139 | 139 | OK |
| `JO29A_FGT_RIVERSIDE_COUNTY_SOCRATA` | json | PASS | 45 | 45 / 45 | 45 | OK |
| `JO29A_FGT_NOVASCOTIA_SOCRATA` | json | PASS | 100 | 1277 / 1277 | 1277 | OK |
| `JO29A_FGT_MEDICAID_DKAN` | json | PASS | 554 | 554 / 554 | 554 | OK |
| `JO29A_FGT_HEALTHCARE_GOV_DKAN` | json | PASS | 337 | 337 / 337 | 337 | OK |
| `JO29A_FGT_OPENDATA_DK_SEARCH` | json | PASS | 42 | 42 / 42 | 42 | OK (hand run, entity `trafik`) |
| `JO29A_FGT_GVA_DADESOBERTES_SEARCH` | json | PASS | 69 | 69 / 69 | 69 | OK (hand run, entity `agua`) |
| `JO29A_FGT_HELSINKI_HRI_SEARCH` | json | PASS | 100 | 132 / 132 | 132 | OK (hand run, entity `liikenne`) |

Verdicts: OK 51

Across both beats on the final binary: 138114 records emitted, 138108 rows stored.

## Rejected / dropped (98 lines)

Full list with the measured reason per endpoint: `docs/rejected-sources-batch29.webcams.tsv` (it carries the rejects of both this agent and the adopted stray webcam run). Reason heads:

- DUPLICATE_ENDPOINT: 20
- IBI: 19
- HTTP: 13
- DUPLICATE_OF_ADOPTED: 8
- NEEDS_CREDENTIAL: 8
- timed: 2
- DUPLICATE: 2
- byte-identical: 1
- endpoint: 1
- TLS: 1
- answers: 1
- works: 1
- real: 1
- array: 1
- Autobahn: 1
- Kure: 1
- Suginami: 1
- 7,977: 1
- Houston: 1
- KC: 1
- older: 1
- Oregon: 1
- overlaps: 1
- third-party: 1
- Esri: 1
- test: 1
- 8: 1
- Darwin: 1
- 5: 1
- school-zone: 1
- traffic: 1
- catalogue: 1
- UPSTREAM_TRUNCATES: 1

