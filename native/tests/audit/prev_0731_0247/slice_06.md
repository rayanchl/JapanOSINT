# Audit slice 6 — 206 sources across 81 files

`verdict` is from the bulk sweep and is ADVISORY: a blank means the sweep had not reached it, and an EMPTY may just mean the sweep guessed the wrong pivot entity. Re-run anything you report on.

## `native/collectors/sources/aed_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| aed-map | health | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/atlas_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| atlas-jp | cyber | 3600 | - | EMPTY | 0 | 0 | 594 |  |

## `native/collectors/sources/attacksurface_world.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BINARYEDGE_IP | cyber | 0 | ip=8.8.8.8 | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| CRIMINALIP_IP | cyber | 0 | ip=8.8.8.8 | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| FULLHUNT_DOMAIN | cyber | 0 | domain=github.com | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| ONYPHE_SUMMARY | cyber | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/blitzortung_lightning.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| blitzortung-lightning | environment | 60 | - | DATA | 1 | 0 | 8003 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/bo_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GLEIF_LEI | government | 0 | keyword=Tokyo | DATA | 15 | 0 | 364 | reads ctx->entity; rt=gleif-lei |
| GLEIF_RELATIONS | government | 0 | vessel=9074729 | EMPTY | 0 | 0 | 202 | reads ctx->entity |
| OPENOWNERSHIP | government | 0 | vessel=9074729 | WAF_BLOCKED | 0 | 0 | 112 | reads ctx->entity |

## `native/collectors/sources/cam_camstreamer.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-camstreamer | investigation | 21600 | - | DATA | 1 | 1 | 820 | rt=camera |

## `native/collectors/sources/cam_webcamtaxi.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-webcamtaxi | investigation | 21600 | - | WAF_BLOCKED | 0 | 0 | 41 |  |

## `native/collectors/sources/censys_japan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| censys-japan | cyber | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/classifieds.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| classifieds | marketplace | 1800 | - | EMPTY | 0 | 0 | 16466 |  |

## `native/collectors/sources/corp_identifiers.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| LEI_SEARCH | investigation | 0 | keyword=Tokyo | RC_ERROR | 5 | 0 | 546 | reads ctx->entity; rt=osint_service_result |
| VAT_VALIDATOR | investigation | 0 | keyword=Tokyo | RC_ERROR | 1 | 0 | 378 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/corp_registry.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CONSTRUCTION_LICENSE | industry | 0 | company=Toyota | EMPTY | 0 | 0 | 1269 | reads ctx->entity |
| INVOICE_REGISTRY | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| MINPAKU_REGISTRY | economy | 0 | company=Toyota | EMPTY | 0 | 0 | 1355 | reads ctx->entity |
| NPO_PORTAL | government | 0 | company=Toyota | WAF_BLOCKED | 0 | 0 | 64 | reads ctx->entity |

## `native/collectors/sources/country_gov2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| UK_PARLIAMENT | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 537 | reads ctx->entity |

## `native/collectors/sources/country_opendata.c`  (6 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AU_OPENDATA | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 2021 | reads ctx->entity; rt=open-dataset |
| CA_OPENDATA | government | 0 | keyword=Tokyo | DATA | 1 | 0 | 429 | reads ctx->entity; rt=open-dataset |
| DE_GOVDATA | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 20255 | reads ctx->entity |
| IE_OPENDATA | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 115 | reads ctx->entity |
| IT_OPENDATA | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 202 | reads ctx->entity |
| NL_OPENDATA | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 145 | reads ctx->entity |

## `native/collectors/sources/crypto_tracker.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CRYPTO_TRACKER | economy | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | DATA | 11 | 0 | 147 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/dehashed_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DEHASHED_SEARCH | cyber | 0 | hash=44d88612fea8a8f36de82e1278abb02f | EMPTY | 0 | 0 | 225 | reads ctx->entity |

## `native/collectors/sources/document_analyzer.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DOCUMENT_ANALYZER | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/domain_world2.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| HOST_IO_FULL | cyber | 0 | domain=github.com | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| IPINFO_LOOKUP | cyber | 0 | domain=github.com | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| WHOISFREAKS | cyber | 0 | domain=github.com | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/eight_sansan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| eight-sansan | social | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/estat_industry.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| estat-industry | statistics | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/fire_department.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| fire-department | safety | 300 | - | RC_ERROR | 0 | 0 | 3218 |  |

## `native/collectors/sources/food_poisoning.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| food-poisoning | health | 604800 | - | DATA | 1 | 0 | 8004 | NO GEO despite map-ish category; rt=food-poisoning |

## `native/collectors/sources/gdelt_events.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gdelt-events | intelligence | 900 | - | DATA | 7 | 7 | 219 | rt=gdelt-events |

## `native/collectors/sources/gdelt_world.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GDELT_DOC | news | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 22109 | reads ctx->entity |
| GDELT_GEO | news | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 10478 | reads ctx->entity |

## `native/collectors/sources/global_adsb.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ADSB_GLOBAL | transport | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 602 | reads ctx->entity |

## `native/collectors/sources/global_ransomlook.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| RANSOMLOOK | cyber | 3600 | - | DATA | 100 | 0 | 347 | reads ctx->entity; rt=ransomware-victim |

## `native/collectors/sources/greynoise_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| greynoise-jp | cyber | 3600 | - | DATA | 5 | 0 | 983 | rt=(null) |

## `native/collectors/sources/hazard_map_portal.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| hazard-map-portal | safety | 86400 | - | DATA | 189 | 189 | 29972 | rt=hazard-map-portal |

## `native/collectors/sources/houmukyoku_commercial.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| houmukyoku-commercial | government | 604800 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/intel_vuln_world.c`  (12 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cvefeed-latest | cyber | 3600 | - | DATA | 25 | 0 | 150 | rt=article |
| cvefeed-newsroom | cyber | 3600 | - | DATA | 25 | 0 | 142 | rt=article |
| exploit-db | cyber | 3600 | - | DATA | 50 | 0 | 134 | rt=article |
| full-disclosure | cyber | 3600 | - | RC_ERROR | 0 | 0 | 24758 |  |
| oss-security | cyber | 3600 | - | RC_ERROR | 0 | 0 | 24759 |  |
| packetstorm | cyber | 3600 | - | RC_ERROR | 0 | 0 | 1752 |  |
| talos-disclosures | cyber | 3600 | - | RC_ERROR | 0 | 0 | 339 |  |
| tenable-tra | cyber | 3600 | - | RC_ERROR | 0 | 0 | 442 |  |
| vuldb-recent | cyber | 3600 | - | DATA | 100 | 0 | 458 | rt=article |
| wpscan-vulndb | cyber | 3600 | - | DATA | 10 | 0 | 212 | rt=article |
| zdi-published | cyber | 3600 | - | DATA | 200 | 0 | 2073 | rt=article |
| zdi-upcoming | cyber | 3600 | - | DATA | 200 | 0 | 1869 | rt=article |

## `native/collectors/sources/intel_worldnews.c`  (22 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| africanews | news | 3600 | - | DATA | 50 | 0 | 200 | rt=article |
| aljazeera | news | 3600 | - | DATA | 25 | 0 | 126 | rt=article |
| ap-topnews | news | 3600 | - | RC_ERROR | 0 | 0 | 62 |  |
| bbc-world | news | 3600 | - | DATA | 31 | 0 | 134 | rt=article |
| buenos-aires-times | news | 3600 | - | DATA | 100 | 0 | 606 | rt=article |
| cnn-world | news | 3600 | - | DATA | 29 | 0 | 315 | rt=article |
| deutsche-welle | news | 3600 | - | DATA | 143 | 0 | 552 | rt=article |
| france24 | news | 3600 | - | DATA | 23 | 0 | 118 | rt=article |
| guardian-world | news | 3600 | - | DATA | 45 | 0 | 222 | rt=article |
| jerusalem-post | news | 3600 | - | DATA | 30 | 0 | 245 | rt=article |
| korea-herald | news | 3600 | - | DATA | 50 | 0 | 2124 | rt=article |
| mail-guardian-africa | news | 3600 | - | RC_ERROR | 0 | 0 | 94 |  |
| mercopress | news | 3600 | - | DATA | 10 | 0 | 456 | rt=article |
| nikkei-asia | news | 3600 | - | DATA | 50 | 0 | 133 | rt=article |
| npr-world | news | 3600 | - | DATA | 10 | 0 | 103 | rt=article |
| nyt-world | news | 3600 | - | DATA | 54 | 0 | 152 | rt=article |
| rio-times | news | 3600 | - | DATA | 10 | 0 | 140 | rt=article |
| scmp-topnews | news | 3600 | - | DATA | 50 | 0 | 598 | rt=article |
| straits-times | news | 3600 | - | DATA | 50 | 0 | 194 | rt=article |
| the-hindu-intl | news | 3600 | - | DATA | 60 | 0 | 187 | rt=article |
| times-of-india | news | 3600 | - | DATA | 47 | 0 | 167 | rt=article |
| times-of-israel | news | 3600 | - | DATA | 15 | 0 | 137 | rt=article |

## `native/collectors/sources/ioda_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ioda-jp | cyber | 1800 | - | NO_TITLE | 3 | 3 | 1402 | rt=signal |

## `native/collectors/sources/jartic_traffic.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jartic-traffic | transport | 300 | - | EMPTY | 0 | 0 | 1019 |  |

## `native/collectors/sources/jma_forecast_area.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-forecast-area | environment | 3600 | - | NO_TITLE | 10 | 10 | 7045 | rt=jma-forecast-area |

## `native/collectors/sources/jma_tsunami.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-tsunami | environment | 60 | - | DATA | 4 | 4 | 53 | rt=jma-tsunami |

## `native/collectors/sources/jp_corpus_lookup.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| JP_CORPUS_LOOKUP | investigation | 0 | company=Toyota | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/jr_central_delay.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jr-central-delay | transport | 60 | - | DATA | 1 | 0 | 996 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/kakaku_prices.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| kakaku-prices | classifieds | 21600 | - | DATA | 1 | 0 | 8004 | rt=(null) |

## `native/collectors/sources/lifull_homes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| lifull-homes | economy | 86400 | - | RC_ERROR | 0 | 0 | 319 |  |

## `native/collectors/sources/marine_traffic.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| marine-traffic | transport | 300 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/maritime_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AISSTREAM | transport | 0 | vessel=9074729 | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| EQUASIS | transport | 0 | vessel=9074729 | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| IMO_GISIS | transport | 0 | vessel=9074729 | EMPTY | 0 | 0 | 227 | reads ctx->entity |

## `native/collectors/sources/mic_broadcast_towers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mic-broadcast-towers | infrastructure | 604800 | - | DATA | 1 | 0 | 737 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/mlit_n02_stations.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-n02-stations | transport | 2592000 | - | EMPTY | 0 | 0 | 2100 |  |

## `native/collectors/sources/mofa_travel_advisory.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mofa-travel-advisory | government | 3600 | - | EMPTY | 0 | 0 | 6930 |  |

## `native/collectors/sources/ndb_open.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ndb-open | health | 604800 | - | DATA | 1 | 0 | 8005 | NO GEO despite map-ish category; rt=ndb-open |

## `native/collectors/sources/nhk_news_rss.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nhk-news-rss | government | 900 | - | DATA | 7 | 0 | 518 | rt=article |

## `native/collectors/sources/nlni_landuse.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nlni-landuse | geospatial | 604800 | - | RC_ERROR | 0 | 0 | 0 |  |

## `native/collectors/sources/npa_traffic_accidents.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| npa-traffic-accidents | safety | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/odpt_transport.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| odpt-transport | investigation | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/osm_transport_buses.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| osm-transport-buses | transport | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/p2pquake_jma.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| p2pquake-jma | environment | 60 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/packages_world.c`  (6 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CRATES_IO | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 414 | reads ctx->entity; rt=rust-crate |
| DOCKERHUB | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 238 | reads ctx->entity; rt=docker-image |
| NPM_REGISTRY | cyber | 0 | company=Toyota | DATA | 20 | 0 | 308 | reads ctx->entity; rt=npm-package |
| PACKAGIST | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 240 | reads ctx->entity; rt=php-package |
| PYPI_PROJECT | cyber | 0 | keyword=Tokyo | DATA | 1 | 0 | 151 | reads ctx->entity; rt=pypi-package |
| RUBYGEMS | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 721 | reads ctx->entity; rt=ruby-gem |

## `native/collectors/sources/people_research.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GOOGLE_PLACES | commercial | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/pogo_raids_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| pogo-raids-jp | social | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/psn_xbox_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| psn-xbox-jp | social | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/refineries.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| refineries | industry | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/reg_courts.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| COURT_AUSTLII | investigation | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 45 | reads ctx->entity |
| COURT_BAILII | investigation | 0 | keyword=Tokyo | DATA | 14 | 0 | 2690 | reads ctx->entity; rt=court-case |
| COURT_CANLII | investigation | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 405 | reads ctx->entity |
| COURT_CJEU | investigation | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 300 | reads ctx->entity |

## `native/collectors/sources/reg_ee_ariregister.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EE_ARIREGISTER | government | 0 | company=Toyota | DATA | 4 | 0 | 167 | reads ctx->entity; rt=ee-ariregister-company |

## `native/collectors/sources/reg_in_mca.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| IN_MCA | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/reg_sg_data.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SG_ACRA | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 9748 | reads ctx->entity; rt=acra-entity |

## `native/collectors/sources/resas_industry.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| resas-industry | statistics | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/ripestat_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ripestat-jp | telecom | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/sanctions_world.c`  (8 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AU_DFAT | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 963 | reads ctx->entity |
| CA_SANCTIONS | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 28188 | reads ctx->entity |
| CH_SECO | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 201 | reads ctx->entity |
| EU_SANCTIONS | government | 0 | person=Shinzo Abe | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| OFAC_SDN | government | 0 | person=Shinzo Abe | WAF_BLOCKED | 0 | 0 | 958 | reads ctx->entity |
| UK_OFSI | government | 0 | person=Shinzo Abe | TIMEOUT | 0 | 0 | None | reads ctx->entity; TIMED OUT |
| UN_SANCTIONS | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 41252 | reads ctx->entity |
| WORLDBANK_DEBARRED | government | 0 | country=Japan | EMPTY | 0 | 0 | 477 | reads ctx->entity |

## `native/collectors/sources/sanctions_world2.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| FBI_WANTED | investigation | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 410 | reads ctx->entity |
| OPENSANCTIONS | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 59 | reads ctx->entity |

## `native/collectors/sources/satellite_imagery.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| satellite-imagery | satellite | 1800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/shadowserver_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| shadowserver-jp | cyber | 21600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/ski_resorts.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ski-resorts | tourism | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/sslbl_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| sslbl-jp | cyber | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/sumo_tournaments.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| sumo-tournaments | social | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/telegeography_cables.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| telegeography-cables | infrastructure | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/threat_intel.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| THREAT_INTEL | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 204 | reads ctx->entity |

## `native/collectors/sources/trademark_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| TRADEMARK_SEARCH | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 359 | reads ctx->entity |

## `native/collectors/sources/transparency_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EU_TRANSPARENCY | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 171 | reads ctx->entity |
| OPENSECRETS | government | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| UK_DONATIONS | government | 0 | country=Japan | DATA | 25 | 0 | 380 | reads ctx->entity; rt=uk-political-donation |

## `native/collectors/sources/unified_airports.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unified-airports | transport | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/url_analyzer.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| URL_ANALYZER | cyber | 0 | domain=github.com | DATA | 1 | 0 | 827 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/vessel_tracker.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| VESSEL_TRACKER | transport | 0 | vessel=9074729 | DATA | 1 | 0 | 0 | reads ctx->entity; NO GEO despite map-ish category; rt=osint_service_result |

## `native/collectors/sources/wantedly_bizreach.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wantedly-bizreach | social | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/whois_lookup.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DOMAIN_WHOIS | cyber | 0 | domain=github.com | DATA | 1 | 0 | 153 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/wifi_networks_wigle.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wifi-networks-wigle | cyber | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/world_outlets_1.c`  (57 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| abc-au | news | 3600 | - | (not swept) | - | - | - |  |
| ahram-eg | news | 3600 | - | (not swept) | - | - | - |  |
| al-jazeera-ar | news | 3600 | - | (not swept) | - | - | - |  |
| anadolu | news | 3600 | - | (not swept) | - | - | - |  |
| ansa-it | news | 3600 | - | (not swept) | - | - | - |  |
| arab-news | news | 3600 | - | (not swept) | - | - | - |  |
| bangkok-post | news | 3600 | - | (not swept) | - | - | - |  |
| cbc-top | news | 3600 | - | (not swept) | - | - | - |  |
| cbc-world | news | 3600 | - | (not swept) | - | - | - |  |
| china-daily | news | 3600 | - | (not swept) | - | - | - |  |
| clarin-ar | news | 3600 | - | (not swept) | - | - | - |  |
| cna-sg | news | 3600 | - | (not swept) | - | - | - |  |
| dawn-pk | news | 3600 | - | (not swept) | - | - | - |  |
| dhaka-tribune | news | 3600 | - | (not swept) | - | - | - |  |
| dw-top | news | 3600 | - | (not swept) | - | - | - |  |
| elmundo | news | 3600 | - | (not swept) | - | - | - |  |
| elpais-port | news | 3600 | - | (not swept) | - | - | - |  |
| eltiempo-co | news | 3600 | - | (not swept) | - | - | - |  |
| eluniversal-mx | news | 3600 | - | (not swept) | - | - | - |  |
| emol-cl | news | 3600 | - | (not swept) | - | - | - |  |
| folha-br | news | 3600 | - | (not swept) | - | - | - |  |
| g1-globo | news | 3600 | - | (not swept) | - | - | - |  |
| global-times | news | 3600 | - | (not swept) | - | - | - |  |
| gulf-news | news | 3600 | - | (not swept) | - | - | - |  |
| haaretz-2 | news | 3600 | - | (not swept) | - | - | - |  |
| hindustan-times | news | 3600 | - | (not swept) | - | - | - |  |
| hurriyet-en | news | 3600 | - | (not swept) | - | - | - |  |
| infobae | news | 3600 | - | (not swept) | - | - | - |  |
| jakarta-post | news | 3600 | - | (not swept) | - | - | - |  |
| japan-times | news | 3600 | - | (not swept) | - | - | - |  |
| korea-times | news | 3600 | - | (not swept) | - | - | - |  |
| kyiv-post | news | 3600 | - | (not swept) | - | - | - |  |
| lefigaro | news | 3600 | - | (not swept) | - | - | - |  |
| lemonde-une | news | 3600 | - | (not swept) | - | - | - |  |
| mainichi-en | news | 3600 | - | (not swept) | - | - | - |  |
| manila-times | news | 3600 | - | (not swept) | - | - | - |  |
| milenio | news | 3600 | - | (not swept) | - | - | - |  |
| moscow-times-2 | news | 3600 | - | (not swept) | - | - | - |  |
| nation-ke | news | 3600 | - | (not swept) | - | - | - |  |
| ndtv | news | 3600 | - | (not swept) | - | - | - |  |
| news24-za | news | 3600 | - | (not swept) | - | - | - |  |
| pravda-ua | news | 3600 | - | (not swept) | - | - | - |  |
| premium-times | news | 3600 | - | (not swept) | - | - | - |  |
| presstv | news | 3600 | - | (not swept) | - | - | - |  |
| punch-ng | news | 3600 | - | (not swept) | - | - | - |  |
| repubblica | news | 3600 | - | (not swept) | - | - | - |  |
| rfi-fr | news | 3600 | - | (not swept) | - | - | - |  |
| rt-news | news | 3600 | - | (not swept) | - | - | - |  |
| scmp-china2 | news | 3600 | - | (not swept) | - | - | - |  |
| spiegel-intl | news | 3600 | - | (not swept) | - | - | - |  |
| straits-top | news | 3600 | - | (not swept) | - | - | - |  |
| taipei-times | news | 3600 | - | (not swept) | - | - | - |  |
| tass | news | 3600 | - | (not swept) | - | - | - |  |
| tehran-times | news | 3600 | - | (not swept) | - | - | - |  |
| the-hindu-nat | news | 3600 | - | (not swept) | - | - | - |  |
| the-national-ae | news | 3600 | - | (not swept) | - | - | - |  |
| times-israel-2 | news | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/world_reg_asia.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ASIA_REGISTRY | government | 0 | company=Toyota | WAF_BLOCKED | 0 | 0 | 43751 | reads ctx->entity |

## `native/collectors/sources/yahoo_auctions.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| yahoo-auctions | classifieds | 21600 | - | (not swept) | - | - | - |  |
