# Audit slice 5 — 206 sources across 81 files

`verdict` is from the bulk sweep and is ADVISORY: a blank means the sweep had not reached it, and an EMPTY may just mean the sweep guessed the wrong pivot entity. Re-run anything you report on.

## `native/collectors/sources/admin_boundaries.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| admin-boundaries | geospatial | 604800 | - | DATA | 1787 | 1787 | 77027 | rt=admin-boundaries |

## `native/collectors/sources/airquality_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| OPENAQ_GLOBAL | environment | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 50 | reads ctx->entity |
| OPENMETEO_AQ | environment | 0 | keyword=Tokyo | DATA | 1 | 1 | 89 | reads ctx->entity; rt=air-quality-measurement |
| WAQI_GLOBAL | environment | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/asn_lookup.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ASN_LOOKUP | cyber | 0 | ip=8.8.8.8 | DATA | 1 | 0 | 38 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/biodiversity_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GBIF_OCCURRENCE | environment | 0 | keyword=Tokyo | DATA | 30 | 30 | 466 | reads ctx->entity; rt=gbif-occurrence |
| INATURALIST | environment | 0 | keyword=Tokyo | DATA | 30 | 30 | 1530 | reads ctx->entity; rt=inaturalist-observation |
| OBIS_MARINE | environment | 0 | vessel=9074729 | EMPTY | 0 | 0 | 101 | reads ctx->entity |

## `native/collectors/sources/bird_makeup_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bird-makeup-jp | social | 900 | - | RC_ERROR | 0 | 0 | 808 |  |

## `native/collectors/sources/breach_index_svc.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BREACH_INDEX_EMAIL | cyber | 0 | email=test@example.com | DATA | 1 | 0 | 1 | reads ctx->entity; rt=osint_service_result |
| BREACH_INDEX_PASSWORD | cyber | 0 | email=test@example.com | DATA | 1 | 0 | 0 | reads ctx->entity; rt=osint_service_result |
| BREACH_INDEX_PHONE | cyber | 0 | phone=+81312345678 | DATA | 1 | 0 | 8 | reads ctx->entity; rt=osint_service_result |
| BREACH_INDEX_USERNAME | cyber | 0 | email=test@example.com | DATA | 1 | 0 | 1 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/cam_camscape.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-camscape | cyber | 21600 | - | DATA | 32 | 32 | 1372 | rt=camera |

## `native/collectors/sources/cam_webcamera24.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-webcamera24 | investigation | 21600 | - | DATA | 160 | 160 | 1606 | rt=camera |

## `native/collectors/sources/cell_towers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cell-towers | infrastructure | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/city_halls.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| city-halls | government | 604800 | - | DATA | 4825 | 4825 | 66419 | rt=townhall,prefecture,municipal |

## `native/collectors/sources/contact_world.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EMAILREP_LOOKUP | cyber | 0 | email=test@example.com | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| NUMVERIFY_PHONE | telecom | 0 | phone=+81312345678 | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/corporate_world2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| OPENCORPORATES | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/country_opendata2.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CH_OPENDATA | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 192 | reads ctx->entity |
| FI_OPENDATA | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 414 | reads ctx->entity |
| UK_DATAGOV | government | 0 | keyword=Tokyo | DATA | 10 | 0 | 428 | reads ctx->entity; rt=open-dataset |

## `native/collectors/sources/crtsh_historical.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| crtsh-historical | cyber | 21600 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/data_go_jp_ckan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| data-go-jp-ckan | government | 86400 | - | DATA | 10 | 0 | 1330 | rt=data-go-jp-ckan |

## `native/collectors/sources/docomo_population.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| docomo-population | commercial | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/egov_laws.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| egov-laws | government | 86400 | - | DATA | 200 | 0 | 3602 | rt=(null) |

## `native/collectors/sources/estat_household.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| estat-household | statistics | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/ferry_routes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ferry-routes | transport | 86400 | - | DATA | 1045 | 1045 | 38198 | rt=ferry-routes |

## `native/collectors/sources/finance_markets_crypto.c`  (16 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bitcoin-mag | economy | 3600 | - | DATA | 10 | 0 | 245 | rt=article |
| cnbc-finance | economy | 3600 | - | DATA | 30 | 0 | 215 | rt=article |
| cnbc-top | economy | 3600 | - | DATA | 30 | 0 | 245 | rt=article |
| cnbc-world | economy | 3600 | - | DATA | 30 | 0 | 213 | rt=article |
| coindesk | economy | 3600 | - | DATA | 25 | 0 | 129 | rt=article |
| cointelegraph | economy | 3600 | - | DATA | 30 | 0 | 110 | rt=article |
| decrypt | economy | 3600 | - | DATA | 38 | 0 | 120 | rt=article |
| ft-home | economy | 3600 | - | DATA | 10 | 0 | 386 | rt=article |
| investing-news | economy | 3600 | - | DATA | 10 | 0 | 128 | rt=article |
| kitco-news | economy | 3600 | - | RC_ERROR | 0 | 0 | 131 |  |
| marketwatch-top | economy | 3600 | - | DATA | 10 | 0 | 263 | rt=article |
| oilprice-2 | economy | 3600 | - | DATA | 15 | 0 | 672 | rt=article |
| seeking-alpha | economy | 3600 | - | DATA | 7 | 0 | 41 | rt=article |
| the-block | economy | 3600 | - | DATA | 19 | 0 | 95 | rt=article |
| trading-econ | economy | 3600 | - | RC_ERROR | 0 | 0 | 62 |  |
| zerohedge | economy | 3600 | - | DATA | 25 | 0 | 317 | rt=article |

## `native/collectors/sources/finance_world2.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ALPHAVANTAGE_SEARCH | government | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| FINNHUB_SEARCH | government | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/fofa_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| fofa-jp | cyber | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/gcom_w.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gcom-w | satellite | 86400 | - | RC_ERROR | 0 | 0 | 203 |  |

## `native/collectors/sources/geohazard_extra.c`  (8 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bom-au-warnings | environment | 900 | - | RC_ERROR | 0 | 0 | 362 |  |
| copernicus-ems | environment | 900 | - | RC_ERROR | 0 | 0 | 227 |  |
| emsc-quakes | environment | 900 | - | RC_ERROR | 0 | 0 | 79 |  |
| iceland-quakes | environment | 900 | - | RC_ERROR | 0 | 0 | 314 |  |
| metoffice-warnings | environment | 900 | - | EMPTY | 0 | 0 | 103 |  |
| nhc-cpac | environment | 900 | - | EMPTY | 0 | 0 | 80 |  |
| pdc-disasters | environment | 900 | - | RC_ERROR | 0 | 0 | 91 |  |
| volcanodiscovery | environment | 900 | - | RC_ERROR | 0 | 0 | 95 |  |

## `native/collectors/sources/gitlab_bitbucket_leaks.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gitlab-bitbucket-leaks | cyber | 21600 | - | DATA | 2 | 0 | 4590 | rt=(null) |

## `native/collectors/sources/global_radiobrowser.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| RADIO_BROWSER | investigation | 0 | keyword=Tokyo | DATA | 11 | 0 | 339 | reads ctx->entity; rt=radio-station |

## `native/collectors/sources/gnews_osint_monitors.c`  (70 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gnews-mon-ae-red-sea-shipping | news | 1800 | - | DATA | 64 | 0 | 827 | rt=article |
| gnews-mon-airspace-violation | news | 1800 | - | DATA | 100 | 0 | 922 | rt=article |
| gnews-mon-arms-shipment | news | 1800 | - | DATA | 100 | 0 | 1047 | rt=article |
| gnews-mon-assassination | news | 1800 | - | DATA | 100 | 0 | 1261 | rt=article |
| gnews-mon-aviation-incident | news | 1800 | - | DATA | 100 | 0 | 1100 | rt=article |
| gnews-mon-border-clash | news | 1800 | - | DATA | 100 | 0 | 855 | rt=article |
| gnews-mon-br-amazon-deforestation | news | 1800 | - | DATA | 66 | 0 | 957 | rt=article |
| gnews-mon-cargo-ship-collision | news | 1800 | - | DATA | 100 | 0 | 1070 | rt=article |
| gnews-mon-catastrophic-flood | news | 1800 | - | DATA | 100 | 0 | 667 | rt=article |
| gnews-mon-chemical-spill | news | 1800 | - | DATA | 100 | 0 | 1054 | rt=article |
| gnews-mon-civil-unrest | news | 1800 | - | DATA | 100 | 0 | 945 | rt=article |
| gnews-mon-cn-cross-strait | news | 1800 | - | DATA | 100 | 0 | 1165 | rt=article |
| gnews-mon-cn-rare-earth | news | 1800 | - | DATA | 100 | 0 | 1311 | rt=article |
| gnews-mon-critical-infrastructure-attack | news | 1800 | - | DATA | 100 | 0 | 997 | rt=article |
| gnews-mon-cyber-espionage | news | 1800 | - | DATA | 100 | 0 | 838 | rt=article |
| gnews-mon-cyberattack | news | 1800 | - | DATA | 100 | 0 | 342 | rt=article |
| gnews-mon-data-breach | news | 1800 | - | DATA | 102 | 0 | 738 | rt=article |
| gnews-mon-disease-outbreak | news | 1800 | - | DATA | 98 | 0 | 1207 | rt=article |
| gnews-mon-disinformation-campaign | news | 1800 | - | DATA | 100 | 0 | 1153 | rt=article |
| gnews-mon-drone-strike | news | 1800 | - | DATA | 97 | 0 | 1014 | rt=article |
| gnews-mon-drug-cartel | news | 1800 | - | DATA | 100 | 0 | 1219 | rt=article |
| gnews-mon-economic-sanctions | news | 1800 | - | DATA | 101 | 0 | 1040 | rt=article |
| gnews-mon-election-interference | news | 1800 | - | DATA | 100 | 0 | 1071 | rt=article |
| gnews-mon-electronic-warfare | news | 1800 | - | DATA | 100 | 0 | 903 | rt=article |
| gnews-mon-espionage-arrest | news | 1800 | - | DATA | 100 | 0 | 1098 | rt=article |
| gnews-mon-fr-coup-attempt | news | 1800 | - | DATA | 100 | 0 | 1351 | rt=article |
| gnews-mon-fr-sahel-militants | news | 1800 | - | DATA | 61 | 0 | 674 | rt=article |
| gnews-mon-gps-jamming | news | 1800 | - | DATA | 100 | 0 | 1082 | rt=article |
| gnews-mon-hostage-crisis | news | 1800 | - | DATA | 100 | 0 | 1224 | rt=article |
| gnews-mon-human-trafficking | news | 1800 | - | DATA | 105 | 0 | 1385 | rt=article |
| gnews-mon-hypersonic-missile | news | 1800 | - | DATA | 100 | 0 | 979 | rt=article |
| gnews-mon-il-gaza | news | 1800 | - | DATA | 100 | 0 | 1190 | rt=article |
| gnews-mon-in-kashmir | news | 1800 | - | DATA | 102 | 0 | 1229 | rt=article |
| gnews-mon-ir-nuclear-talks | news | 1800 | - | DATA | 100 | 0 | 1334 | rt=article |
| gnews-mon-ke-horn-of-africa | news | 1800 | - | DATA | 100 | 0 | 1388 | rt=article |
| gnews-mon-kr-north-korea-missile | news | 1800 | - | DATA | 100 | 0 | 1127 | rt=article |
| gnews-mon-lt-baltic-security | news | 1800 | - | DATA | 46 | 0 | 724 | rt=article |
| gnews-mon-major-earthquake | news | 1800 | - | DATA | 92 | 0 | 1000 | rt=article |
| gnews-mon-military-coup | news | 1800 | - | DATA | 100 | 0 | 1110 | rt=article |
| gnews-mon-military-exercise | news | 1800 | - | DATA | 100 | 0 | 906 | rt=article |
| gnews-mon-missile-test | news | 1800 | - | DATA | 100 | 0 | 896 | rt=article |
| gnews-mon-money-laundering | news | 1800 | - | DATA | 100 | 0 | 889 | rt=article |
| gnews-mon-mx-cartel-violence | news | 1800 | - | DATA | 69 | 0 | 988 | rt=article |
| gnews-mon-naval-deployment | news | 1800 | - | DATA | 100 | 0 | 984 | rt=article |
| gnews-mon-no-arctic-militarization | news | 1800 | - | EMPTY | 0 | 0 | 206 |  |
| gnews-mon-nuclear-plant-incident | news | 1800 | - | DATA | 100 | 0 | 1027 | rt=article |
| gnews-mon-nuclear-weapon | news | 1800 | - | DATA | 100 | 0 | 539 | rt=article |
| gnews-mon-oil-tanker-seizure | news | 1800 | - | DATA | 100 | 0 | 791 | rt=article |
| gnews-mon-ph-south-china-sea | news | 1800 | - | DATA | 102 | 0 | 1282 | rt=article |
| gnews-mon-pipeline-explosion | news | 1800 | - | DATA | 100 | 0 | 1115 | rt=article |
| gnews-mon-port-disruption | news | 1800 | - | DATA | 100 | 0 | 1248 | rt=article |
| gnews-mon-power-grid-failure | news | 1800 | - | DATA | 100 | 0 | 964 | rt=article |
| gnews-mon-ransomware | news | 1800 | - | DATA | 100 | 0 | 833 | rt=article |
| gnews-mon-ru-wagner | news | 1800 | - | DATA | 100 | 0 | 1237 | rt=article |
| gnews-mon-satellite-launch | news | 1800 | - | DATA | 102 | 0 | 1011 | rt=article |
| gnews-mon-semiconductor-export-controls | news | 1800 | - | DATA | 100 | 0 | 1011 | rt=article |
| gnews-mon-shadow-fleet | news | 1800 | - | DATA | 100 | 0 | 970 | rt=article |
| gnews-mon-state-sponsored-hacking | news | 1800 | - | DATA | 100 | 0 | 1134 | rt=article |
| gnews-mon-supply-chain-disruption | news | 1800 | - | DATA | 100 | 0 | 1238 | rt=article |
| gnews-mon-terror-attack | news | 1800 | - | DATA | 100 | 0 | 1230 | rt=article |
| gnews-mon-troop-movement | news | 1800 | - | DATA | 100 | 0 | 728 | rt=article |
| gnews-mon-tw-pla-incursion | news | 1800 | - | EMPTY | 0 | 0 | 323 |  |
| gnews-mon-ua-front-line | news | 1800 | - | DATA | 83 | 0 | 976 | rt=article |
| gnews-mon-undersea-cable | news | 1800 | - | DATA | 100 | 0 | 1056 | rt=article |
| gnews-mon-us-strait-of-hormuz | news | 1800 | - | DATA | 109 | 0 | 1242 | rt=article |
| gnews-mon-us-taiwan-strait | news | 1800 | - | DATA | 100 | 0 | 1151 | rt=article |
| gnews-mon-volcano-eruption | news | 1800 | - | DATA | 92 | 0 | 875 | rt=article |
| gnews-mon-weapons-smuggling | news | 1800 | - | DATA | 100 | 0 | 1001 | rt=article |
| gnews-mon-wildfire | news | 1800 | - | DATA | 106 | 0 | 1276 | rt=article |
| gnews-mon-zero-day-exploit | news | 1800 | - | DATA | 100 | 0 | 1074 | rt=article |

## `native/collectors/sources/gov_admin.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ADVISORY_COUNCILS | government | 0 | cve=CVE-2021-44228 | EMPTY | 0 | 0 | 2452 | reads ctx->entity |
| BUSINESS_LICENSES | government | 0 | company=Toyota | EMPTY | 0 | 0 | 274 | reads ctx->entity |
| EGOV_PUBLIC_COMMENT | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 2036 | reads ctx->entity |
| WASTE_HAULERS | government | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 770 | reads ctx->entity |

## `native/collectors/sources/grayhat_buckets.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| grayhat-buckets | cyber | 21600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/hatena_bookmark_extended.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| hatena-bookmark-extended | social | 1800 | - | DATA | 192 | 0 | 6186 | rt=hatena-bookmark-extended |

## `native/collectors/sources/houjin_bangou.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| houjin-bangou | government | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/ioc_lookup.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| IOC_LOOKUP | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 77 | reads ctx->entity |

## `native/collectors/sources/japan_reit.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| japan-reit | economy | 86400 | - | RC_ERROR | 0 | 0 | 1385 |  |

## `native/collectors/sources/jma_earthquake.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-earthquake | environment | 60 | - | DATA | 426 | 378 | 497 | rt=jma-earthquake |

## `native/collectors/sources/jma_tide.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-tide | environment | 600 | - | DATA | 166 | 166 | 2604 | rt=jma-tide |

## `native/collectors/sources/job_boards.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| job-boards | marketplace | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/jr_boarding_stats.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jr-boarding-stats | transport | 604800 | - | DATA | 2 | 0 | 484 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/kafun_pollen.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| kafun-pollen | environment | 3600 | - | DATA | 1 | 0 | 13 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/legal_records.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BANKRUPTCY_SEARCH | investigation | 0 | keyword=Tokyo | RC_ERROR | 15 | 0 | 2532 | reads ctx->entity; rt=osint_service_result |
| CRIMINAL_RECORDS | investigation | 0 | keyword=Tokyo | RC_ERROR | 15 | 0 | 659 | reads ctx->entity; rt=osint_service_result |
| LEGAL_SEARCH | investigation | 0 | keyword=Tokyo | RC_ERROR | 15 | 0 | 741 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/license_plate.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| LICENSE_PLATE_LOOKUP | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/mapfan_api.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mapfan-api | commercial | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/mhlw_health.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mhlw-health | statistics | 604800 | - | DATA | 1 | 0 | 8015 | rt=mhlw-health |

## `native/collectors/sources/mlit_landprice.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-landprice | economy | 86400 | - | EMPTY | 0 | 0 | 38108 |  |

## `native/collectors/sources/mlit_transaction.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-transaction | economy | 86400 | - | RC_ERROR | 0 | 0 | 860 |  |

## `native/collectors/sources/navitime_api.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| navitime-api | commercial | 300 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/nexco_roadwork.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nexco-roadwork | transport | 21600 | - | DATA | 3 | 0 | 11261 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/nitter_mirrors.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nitter-mirrors | social | 1800 | - | DATA | 3 | 0 | 8178 | rt=(null) |

## `native/collectors/sources/npa_special_fraud.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| npa-special-fraud | crime | 86400 | - | DATA | 25 | 24 | 324 | rt=npa-special-fraud,(null) |

## `native/collectors/sources/odpt_train.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| odpt-train | transport | 30 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/osint_extras.c`  (5 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ARCHIVE_ORG_SEARCH | investigation | 0 | keyword=Tokyo | DATA | 20 | 0 | 633 | reads ctx->entity; rt=archive-item |
| GITHUB_COMMITS_BY_EMAIL | cyber | 0 | email=test@example.com | DATA | 20 | 0 | 538 | reads ctx->entity; rt=github-commit |
| MUSICBRAINZ | investigation | 0 | keyword=Tokyo | DATA | 20 | 0 | 178 | reads ctx->entity; rt=musicbrainz-artist |
| PHOTON_GEOCODE | geospatial | 0 | keyword=Tokyo | DATA | 9 | 9 | 188 | reads ctx->entity; rt=geocode-result |
| TOR_ONIONOO | cyber | 0 | keyword=Tokyo | DATA | 1 | 0 | 483 | reads ctx->entity; rt=tor-relay |

## `native/collectors/sources/osint_extras2.c`  (6 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BIGDATACLOUD_REVERSE | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| HN_USER | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 837 | reads ctx->entity |
| OPENVERSE | investigation | 0 | keyword=Tokyo | DATA | 20 | 0 | 899 | reads ctx->entity; rt=openverse-image |
| SEC_FULLTEXT | government | 0 | company=Toyota | DATA | 20 | 0 | 604 | reads ctx->entity; rt=sec-filing |
| STACKEXCHANGE_SEARCH | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 273 | reads ctx->entity; rt=stackexchange-question |
| WIKIDATA_ENTITY_SEARCH | investigation | 0 | keyword=Tokyo | DATA | 20 | 0 | 272 | reads ctx->entity; rt=wikidata-entity |

## `native/collectors/sources/osm_changesets_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| osm-changesets-jp | social | 1800 | - | NO_TITLE | 43 | 43 | 344 | rt=osm-changesets-jp |

## `native/collectors/sources/overpass_subway_tracks.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| overpass-subway-tracks | investigation | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/peeringdb_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| peeringdb-jp | telecom | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/pep_watchlist.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| PEP_CHECK | investigation | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| WATCHLIST_CHECK_NEW | investigation | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/phone_intel.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CARRIER_LOOKUP | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| PHONE_LOOKUP | investigation | 0 | phone=+81312345678 | RC_ERROR | 1 | 0 | 0 | reads ctx->entity; rt=osint_service_result |
| PHONE_REPUTATION | investigation | 0 | phone=+81312345678 | RC_ERROR | 1 | 0 | 0 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/poc_in_github.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| poc-in-github | cyber | 21600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/psbdmp_pastes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| psbdmp-pastes | cyber | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/reddit_jp_subs.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| reddit-jp-subs | social | 1800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/reg_dk_cvr.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DK_CVR | government | 0 | company=Toyota | DATA | 1 | 0 | 281 | reads ctx->entity; rt=cvr-company |

## `native/collectors/sources/reg_eu_more.c`  (7 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| HR_SUDREG | government | 0 | company=Toyota | EMPTY | 0 | 0 | 275 | reads ctx->entity |
| HU_COMPANIES | government | 0 | company=Toyota | EMPTY | 0 | 0 | 1205 | reads ctx->entity |
| LT_REGISTRAI | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 92 | reads ctx->entity |
| LU_LBR | government | 0 | company=Toyota | WAF_BLOCKED | 0 | 0 | 439 | reads ctx->entity |
| NL_KVK | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| PT_RCBE | government | 0 | company=Toyota | EMPTY | 0 | 0 | 388 | reads ctx->entity |
| RS_APR | government | 0 | company=Toyota | EMPTY | 0 | 0 | 1979 | reads ctx->entity |

## `native/collectors/sources/reg_il_data.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| IL_COMPANIES | government | 0 | company=Toyota | DATA | 2 | 0 | 417 | reads ctx->entity; rt=il-company |

## `native/collectors/sources/reg_sea2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SEA2_REGISTRY | government | 0 | company=Toyota | WAF_BLOCKED | 0 | 0 | 68473 | reads ctx->entity |

## `native/collectors/sources/reinfolib.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| reinfolib | economy | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/rice_paddies.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| rice-paddies | agriculture | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/satellite_ground_stations.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| satellite-ground-stations | telecom | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/sento_public_baths.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| sento-public-baths | culture | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/shrine_temple.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| shrine-temple | culture | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/ssl_analyzer.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SSL_ANALYZER | cyber | 0 | domain=github.com | DATA | 1 | 0 | 47442 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/submarine_cables.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| submarine-cables | telecom | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/teikoku_failures.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| teikoku-failures | government | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/threat_feed.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| THREAT_FEED_LOOKUP | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1390 | reads ctx->entity |

## `native/collectors/sources/tor_exit_nodes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tor-exit-nodes | telecom | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/unesco_heritage.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unesco-heritage | tourism | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/unified_trains.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unified-trains | transport | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/vessel_finder.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| vessel-finder | transport | 300 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/wanted_persons.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wanted-persons | crime | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/whale_monitor.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| WHALE_ALERT | economy | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | DATA | 1 | 0 | 98 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/wifi_networks_shodan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wifi-networks-shodan | cyber | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/world_reg_africa.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AFRICA_REGISTRY | government | 0 | company=Toyota | DATA | 1 | 0 | 12386 | reads ctx->entity; rt=africa-registry-company |

## `native/collectors/sources/xrain_radar.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| xrain-radar | infrastructure | 60 | - | (not swept) | - | - | - |  |
