# Audit slice 6 — 206 sources across 81 files

`verdict` is from the bulk sweep and is ADVISORY: a blank means the sweep had not reached it, and an EMPTY may just mean the sweep guessed the wrong pivot entity. Re-run anything you report on.

## `native/collectors/sources/aed_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| aed-map | health | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/atlas_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| atlas-jp | cyber | 3600 | - | EMPTY | 0 | 0 | 1162 |  |

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
| blitzortung-lightning | environment | 60 | - | DATA | 1 | 0 | 8260 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/bo_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GLEIF_LEI | government | 0 | keyword=Tokyo | DATA | 15 | 0 | 485 | reads ctx->entity; rt=gleif-lei |
| GLEIF_RELATIONS | government | 0 | vessel=9074729 | EMPTY | 0 | 0 | 358 | reads ctx->entity |
| OPENOWNERSHIP | government | 0 | vessel=9074729 | WAF_BLOCKED | 0 | 0 | 321 | reads ctx->entity |

## `native/collectors/sources/cam_camstreamer.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-camstreamer | investigation | 21600 | - | DATA | 1 | 1 | 1012 | rt=camera |

## `native/collectors/sources/cam_webcamtaxi.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-webcamtaxi | investigation | 21600 | - | WAF_BLOCKED | 0 | 0 | 66 |  |

## `native/collectors/sources/censys_japan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| censys-japan | cyber | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/classifieds.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| classifieds | marketplace | 1800 | - | EMPTY | 0 | 0 | 15455 |  |

## `native/collectors/sources/corp_identifiers.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| LEI_SEARCH | investigation | 0 | keyword=Tokyo | RC_ERROR | 5 | 0 | 260 | reads ctx->entity; rt=osint_service_result |
| VAT_VALIDATOR | investigation | 0 | keyword=Tokyo | RC_ERROR | 1 | 0 | 172 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/corp_registry.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CONSTRUCTION_LICENSE | industry | 0 | company=Toyota | EMPTY | 0 | 0 | 1509 | reads ctx->entity |
| INVOICE_REGISTRY | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| MINPAKU_REGISTRY | economy | 0 | company=Toyota | EMPTY | 0 | 0 | 780 | reads ctx->entity |
| NPO_PORTAL | government | 0 | company=Toyota | WAF_BLOCKED | 0 | 0 | 180 | reads ctx->entity |

## `native/collectors/sources/country_gov2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| UK_PARLIAMENT | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1668 | reads ctx->entity |

## `native/collectors/sources/country_opendata.c`  (6 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AU_OPENDATA | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 2098 | reads ctx->entity; rt=open-dataset |
| CA_OPENDATA | government | 0 | keyword=Tokyo | DATA | 1 | 0 | 513 | reads ctx->entity; rt=open-dataset |
| DE_GOVDATA | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 21685 | reads ctx->entity |
| IE_OPENDATA | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 128 | reads ctx->entity |
| IT_OPENDATA | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 307 | reads ctx->entity |
| NL_OPENDATA | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 380 | reads ctx->entity |

## `native/collectors/sources/crypto_tracker.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CRYPTO_TRACKER | economy | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | DATA | 11 | 0 | 238 | reads ctx->entity; rt=osint_service_result |

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
| fire-department | safety | 300 | - | RC_ERROR | 0 | 0 | 3959 |  |

## `native/collectors/sources/food_poisoning.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| food-poisoning | health | 604800 | - | DATA | 1 | 0 | 8583 | NO GEO despite map-ish category; rt=food-poisoning |

## `native/collectors/sources/gdelt_events.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gdelt-events | intelligence | 900 | - | EMPTY | 0 | 0 | 268 |  |

## `native/collectors/sources/gdelt_world.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GDELT_DOC | news | 0 | keyword=Tokyo | DATA | 50 | 0 | 18376 | reads ctx->entity; rt=gdelt-article |
| GDELT_GEO | news | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 8502 | reads ctx->entity |

## `native/collectors/sources/global_adsb.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ADSB_GLOBAL | transport | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 299 | reads ctx->entity |

## `native/collectors/sources/global_ransomlook.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| RANSOMLOOK | cyber | 3600 | - | DATA | 100 | 0 | 467 | reads ctx->entity; rt=ransomware-victim |

## `native/collectors/sources/greynoise_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| greynoise-jp | cyber | 3600 | - | DATA | 5 | 0 | 1082 | rt=(null) |

## `native/collectors/sources/hazard_map_portal.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| hazard-map-portal | safety | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/houmukyoku_commercial.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| houmukyoku-commercial | government | 604800 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/intel_vuln_world.c`  (12 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cvefeed-latest | cyber | 3600 | - | DATA | 25 | 0 | 156 | rt=article |
| cvefeed-newsroom | cyber | 3600 | - | DATA | 25 | 0 | 164 | rt=article |
| exploit-db | cyber | 3600 | - | DATA | 50 | 0 | 111 | rt=article |
| full-disclosure | cyber | 3600 | - | DATA | 15 | 0 | 899 | rt=article |
| oss-security | cyber | 3600 | - | DATA | 15 | 0 | 843 | rt=article |
| packetstorm | cyber | 3600 | - | RC_ERROR | 0 | 0 | 1689 |  |
| talos-disclosures | cyber | 3600 | - | RC_ERROR | 0 | 0 | 332 |  |
| tenable-tra | cyber | 3600 | - | RC_ERROR | 0 | 0 | 2270 |  |
| vuldb-recent | cyber | 3600 | - | DATA | 100 | 0 | 504 | rt=article |
| wpscan-vulndb | cyber | 3600 | - | DATA | 10 | 0 | 267 | rt=article |
| zdi-published | cyber | 3600 | - | DATA | 200 | 0 | 2090 | rt=article |
| zdi-upcoming | cyber | 3600 | - | DATA | 200 | 0 | 1908 | rt=article |

## `native/collectors/sources/intel_worldnews.c`  (22 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| africanews | news | 3600 | - | DATA | 50 | 0 | 193 | rt=article |
| aljazeera | news | 3600 | - | DATA | 25 | 0 | 92 | rt=article |
| ap-topnews | news | 3600 | - | RC_ERROR | 0 | 0 | 68 |  |
| bbc-world | news | 3600 | - | DATA | 39 | 0 | 98 | rt=article |
| buenos-aires-times | news | 3600 | - | DATA | 100 | 0 | 611 | rt=article |
| cnn-world | news | 3600 | - | DATA | 29 | 0 | 325 | rt=article |
| deutsche-welle | news | 3600 | - | DATA | 143 | 0 | 586 | rt=article |
| france24 | news | 3600 | - | DATA | 23 | 0 | 116 | rt=article |
| guardian-world | news | 3600 | - | DATA | 45 | 0 | 207 | rt=article |
| jerusalem-post | news | 3600 | - | DATA | 30 | 0 | 223 | rt=article |
| korea-herald | news | 3600 | - | DATA | 50 | 0 | 2259 | rt=article |
| mail-guardian-africa | news | 3600 | - | RC_ERROR | 0 | 0 | 97 |  |
| mercopress | news | 3600 | - | DATA | 10 | 0 | 529 | rt=article |
| nikkei-asia | news | 3600 | - | DATA | 50 | 0 | 107 | rt=article |
| npr-world | news | 3600 | - | DATA | 10 | 0 | 128 | rt=article |
| nyt-world | news | 3600 | - | DATA | 55 | 0 | 121 | rt=article |
| rio-times | news | 3600 | - | DATA | 10 | 0 | 108 | rt=article |
| scmp-topnews | news | 3600 | - | DATA | 50 | 0 | 1434 | rt=article |
| straits-times | news | 3600 | - | DATA | 50 | 0 | 420 | rt=article |
| the-hindu-intl | news | 3600 | - | DATA | 60 | 0 | 181 | rt=article |
| times-of-india | news | 3600 | - | DATA | 45 | 0 | 198 | rt=article |
| times-of-israel | news | 3600 | - | DATA | 12 | 0 | 83 | rt=article |

## `native/collectors/sources/ioda_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ioda-jp | cyber | 1800 | - | NO_TITLE | 3 | 3 | 1464 | rt=signal |

## `native/collectors/sources/jartic_traffic.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jartic-traffic | transport | 300 | - | EMPTY | 0 | 0 | 1209 |  |

## `native/collectors/sources/jma_forecast_area.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-forecast-area | environment | 3600 | - | NO_TITLE | 10 | 10 | 7591 | rt=jma-forecast-area |

## `native/collectors/sources/jma_tsunami.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-tsunami | environment | 60 | - | DATA | 4 | 4 | 50 | rt=jma-tsunami |

## `native/collectors/sources/jp_corpus_lookup.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| JP_CORPUS_LOOKUP | investigation | 0 | company=Toyota | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/jr_central_delay.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jr-central-delay | transport | 60 | - | DATA | 1 | 0 | 104 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/kakaku_prices.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| kakaku-prices | classifieds | 21600 | - | DATA | 1 | 0 | 8563 | rt=(null) |

## `native/collectors/sources/lifull_homes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| lifull-homes | economy | 86400 | - | RC_ERROR | 0 | 0 | 278 |  |

## `native/collectors/sources/marine_traffic.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| marine-traffic | transport | 300 | - | NO_TITLE | 577 | 577 | 22456 | rt=marine-traffic |

## `native/collectors/sources/maritime_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AISSTREAM | transport | 0 | vessel=9074729 | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| EQUASIS | transport | 0 | vessel=9074729 | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| IMO_GISIS | transport | 0 | vessel=9074729 | EMPTY | 0 | 0 | 205 | reads ctx->entity |

## `native/collectors/sources/mic_broadcast_towers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mic-broadcast-towers | infrastructure | 604800 | - | DATA | 1 | 0 | 830 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/mlit_n02_stations.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-n02-stations | transport | 2592000 | - | EMPTY | 0 | 0 | 1536 |  |

## `native/collectors/sources/mofa_travel_advisory.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mofa-travel-advisory | government | 3600 | - | EMPTY | 0 | 0 | 7061 |  |

## `native/collectors/sources/ndb_open.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ndb-open | health | 604800 | - | DATA | 1 | 0 | 8555 | NO GEO despite map-ish category; rt=ndb-open |

## `native/collectors/sources/nhk_news_rss.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nhk-news-rss | government | 900 | - | DATA | 7 | 0 | 77 | rt=article |

## `native/collectors/sources/nlni_landuse.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nlni-landuse | geospatial | 604800 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/npa_traffic_accidents.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| npa-traffic-accidents | safety | 86400 | - | TIMEOUT | 36725 | 36725 | None | TIMED OUT; rt=npa-traffic-accidents |

## `native/collectors/sources/odpt_transport.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| odpt-transport | investigation | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/osm_transport_buses.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| osm-transport-buses | transport | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/p2pquake_jma.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| p2pquake-jma | environment | 60 | - | NO_TITLE | 50 | 50 | 498 | rt=p2pquake-jma |

## `native/collectors/sources/packages_world.c`  (6 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CRATES_IO | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 427 | reads ctx->entity; rt=rust-crate |
| DOCKERHUB | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 246 | reads ctx->entity; rt=docker-image |
| NPM_REGISTRY | cyber | 0 | company=Toyota | DATA | 20 | 0 | 757 | reads ctx->entity; rt=npm-package |
| PACKAGIST | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 235 | reads ctx->entity; rt=php-package |
| PYPI_PROJECT | cyber | 0 | keyword=Tokyo | DATA | 1 | 0 | 124 | reads ctx->entity; rt=pypi-package |
| RUBYGEMS | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 767 | reads ctx->entity; rt=ruby-gem |

## `native/collectors/sources/people_research.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GOOGLE_PLACES | commercial | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/pogo_raids_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| pogo-raids-jp | social | 3600 | - | DATA | 1 | 0 | 2971 | rt=(null) |

## `native/collectors/sources/psn_xbox_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| psn-xbox-jp | social | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/refineries.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| refineries | industry | 604800 | - | DATA | 8 | 8 | 8055 | rt=refineries |

## `native/collectors/sources/reg_courts.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| COURT_AUSTLII | investigation | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 48 | reads ctx->entity |
| COURT_BAILII | investigation | 0 | keyword=Tokyo | DATA | 14 | 0 | 2110 | reads ctx->entity; rt=court-case |
| COURT_CANLII | investigation | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 449 | reads ctx->entity |
| COURT_CJEU | investigation | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 315 | reads ctx->entity |

## `native/collectors/sources/reg_ee_ariregister.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EE_ARIREGISTER | government | 0 | company=Toyota | DATA | 4 | 0 | 169 | reads ctx->entity; rt=ee-ariregister-company |

## `native/collectors/sources/reg_in_mca.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| IN_MCA | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/reg_sg_data.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SG_ACRA | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 13387 | reads ctx->entity; rt=acra-entity |

## `native/collectors/sources/resas_industry.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| resas-industry | statistics | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/ripestat_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ripestat-jp | telecom | 86400 | - | DATA | 1 | 0 | 1071 | rt=ripestat-jp |

## `native/collectors/sources/sanctions_world.c`  (8 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AU_DFAT | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 811 | reads ctx->entity |
| CA_SANCTIONS | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 41031 | reads ctx->entity |
| CH_SECO | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 171 | reads ctx->entity |
| EU_SANCTIONS | government | 0 | person=Shinzo Abe | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| OFAC_SDN | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 53524 | reads ctx->entity |
| UK_OFSI | government | 0 | person=Shinzo Abe | TIMEOUT | 0 | 0 | None | reads ctx->entity; TIMED OUT |
| UN_SANCTIONS | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 52611 | reads ctx->entity |
| WORLDBANK_DEBARRED | government | 0 | country=Japan | EMPTY | 0 | 0 | 241 | reads ctx->entity |

## `native/collectors/sources/sanctions_world2.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| FBI_WANTED | investigation | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 444 | reads ctx->entity |
| OPENSANCTIONS | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 144 | reads ctx->entity |

## `native/collectors/sources/satellite_imagery.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| satellite-imagery | satellite | 1800 | - | NO_TITLE | 121 | 121 | 3713 | rt=satellite-imagery |

## `native/collectors/sources/shadowserver_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| shadowserver-jp | cyber | 21600 | - | NO_TITLE | 10 | 10 | 2029 | rt=shadowserver-jp |

## `native/collectors/sources/ski_resorts.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ski-resorts | tourism | 2592000 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/sslbl_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| sslbl-jp | cyber | 3600 | - | EMPTY | 0 | 0 | 197 |  |

## `native/collectors/sources/sumo_tournaments.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| sumo-tournaments | social | 86400 | - | DATA | 1 | 0 | 8553 | rt=(null) |

## `native/collectors/sources/telegeography_cables.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| telegeography-cables | infrastructure | 86400 | - | DATA | 142 | 142 | 1262 | rt=telegeography-cables |

## `native/collectors/sources/threat_intel.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| THREAT_INTEL | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 214 | reads ctx->entity |

## `native/collectors/sources/trademark_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| TRADEMARK_SEARCH | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 311 | reads ctx->entity |

## `native/collectors/sources/transparency_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EU_TRANSPARENCY | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 208 | reads ctx->entity |
| OPENSECRETS | government | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| UK_DONATIONS | government | 0 | country=Japan | DATA | 25 | 0 | 514 | reads ctx->entity; rt=uk-political-donation |

## `native/collectors/sources/unified_airports.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unified-airports | transport | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/url_analyzer.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| URL_ANALYZER | cyber | 0 | domain=github.com | DATA | 1 | 0 | 733 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/vessel_tracker.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| VESSEL_TRACKER | transport | 0 | vessel=9074729 | DATA | 1 | 0 | 0 | reads ctx->entity; NO GEO despite map-ish category; rt=osint_service_result |

## `native/collectors/sources/wantedly_bizreach.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wantedly-bizreach | social | 86400 | - | DATA | 2 | 0 | 1690 | rt=(null) |

## `native/collectors/sources/whois_lookup.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DOMAIN_WHOIS | cyber | 0 | domain=github.com | DATA | 1 | 0 | 107 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/wifi_networks_wigle.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wifi-networks-wigle | cyber | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/world_outlets_1.c`  (57 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| abc-au | news | 3600 | - | DATA | 25 | 0 | 319 | rt=article |
| ahram-eg | news | 3600 | - | RC_ERROR | 0 | 0 | 62 |  |
| al-jazeera-ar | news | 3600 | - | DB_ERROR | 25 | 0 | 148 | db:OperationalError: Could not decode to UT; rt=article |
| anadolu | news | 3600 | - | DATA | 28 | 0 | 545 | rt=article |
| ansa-it | news | 3600 | - | DATA | 28 | 0 | 145 | rt=article |
| arab-news | news | 3600 | - | DATA | 10 | 0 | 106 | rt=article |
| bangkok-post | news | 3600 | - | DATA | 10 | 0 | 1410 | rt=article |
| cbc-top | news | 3600 | - | DATA | 20 | 0 | 91 | rt=article |
| cbc-world | news | 3600 | - | DATA | 20 | 0 | 105 | rt=article |
| china-daily | news | 3600 | - | DATA | 41 | 0 | 421 | rt=article |
| clarin-ar | news | 3600 | - | DATA | 10 | 0 | 93 | rt=article |
| cna-sg | news | 3600 | - | DATA | 20 | 0 | 138 | rt=article |
| dawn-pk | news | 3600 | - | DATA | 26 | 0 | 145 | rt=article |
| dhaka-tribune | news | 3600 | - | RC_ERROR | 0 | 0 | 375 |  |
| dw-top | news | 3600 | - | DATA | 21 | 0 | 142 | rt=article |
| elmundo | news | 3600 | - | DATA | 24 | 0 | 144 | rt=article |
| elpais-port | news | 3600 | - | DATA | 149 | 0 | 392 | rt=article |
| eltiempo-co | news | 3600 | - | DATA | 10 | 0 | 212 | rt=article |
| eluniversal-mx | news | 3600 | - | RC_ERROR | 0 | 0 | 115 |  |
| emol-cl | news | 3600 | - | RC_ERROR | 0 | 0 | 4678 |  |
| folha-br | news | 3600 | - | DB_ERROR | 100 | 0 | 1918 | db:OperationalError: Could not decode to UT; rt=article |
| g1-globo | news | 3600 | - | DATA | 100 | 0 | 1752 | rt=article |
| global-times | news | 3600 | - | DATA | 50 | 0 | 395 | rt=article |
| gulf-news | news | 3600 | - | RC_ERROR | 0 | 0 | 270 |  |
| haaretz-2 | news | 3600 | - | RC_ERROR | 0 | 0 | 166 |  |
| hindustan-times | news | 3600 | - | DATA | 100 | 0 | 265 | rt=article |
| hurriyet-en | news | 3600 | - | EMPTY | 0 | 0 | 160 |  |
| infobae | news | 3600 | - | RC_ERROR | 0 | 0 | 95 |  |
| jakarta-post | news | 3600 | - | RC_ERROR | 0 | 0 | 627 |  |
| japan-times | news | 3600 | - | DATA | 30 | 0 | 600 | rt=article |
| korea-times | news | 3600 | - | DATA | 2 | 0 | 1825 | rt=article |
| kyiv-post | news | 3600 | - | DATA | 100 | 0 | 250 | rt=article |
| lefigaro | news | 3600 | - | DATA | 19 | 0 | 112 | rt=article |
| lemonde-une | news | 3600 | - | DATA | 16 | 0 | 96 | rt=article |
| mainichi-en | news | 3600 | - | DATA | 20 | 0 | 439 | rt=article |
| manila-times | news | 3600 | - | RC_ERROR | 0 | 0 | 243 |  |
| milenio | news | 3600 | - | RC_ERROR | 0 | 0 | 65 |  |
| moscow-times-2 | news | 3600 | - | DATA | 50 | 0 | 168 | rt=article |
| nation-ke | news | 3600 | - | RC_ERROR | 0 | 0 | 69 |  |
| ndtv | news | 3600 | - | DATA | 20 | 0 | 206 | rt=article |
| news24-za | news | 3600 | - | RC_ERROR | 0 | 0 | 837 |  |
| pravda-ua | news | 3600 | - | RC_ERROR | 0 | 0 | 52 |  |
| premium-times | news | 3600 | - | DATA | 15 | 0 | 228 | rt=article |
| presstv | news | 3600 | - | RC_ERROR | 0 | 0 | 1527 |  |
| punch-ng | news | 3600 | - | DATA | 30 | 0 | 331 | rt=article |
| repubblica | news | 3600 | - | DATA | 30 | 0 | 121 | rt=article |
| rfi-fr | news | 3600 | - | DATA | 23 | 0 | 126 | rt=article |
| rt-news | news | 3600 | - | RC_ERROR | 0 | 0 | 26405 |  |
| scmp-china2 | news | 3600 | - | DATA | 50 | 0 | 699 | rt=article |
| spiegel-intl | news | 3600 | - | DATA | 20 | 0 | 132 | rt=article |
| straits-top | news | 3600 | - | DATA | 50 | 0 | 217 | rt=article |
| taipei-times | news | 3600 | - | DATA | 46 | 0 | 101 | rt=article |
| tass | news | 3600 | - | DATA | 100 | 0 | 511 | rt=article |
| tehran-times | news | 3600 | - | DATA | 30 | 0 | 519 | rt=article |
| the-hindu-nat | news | 3600 | - | DATA | 60 | 0 | 175 | rt=article |
| the-national-ae | news | 3600 | - | RC_ERROR | 0 | 0 | 89 |  |
| times-israel-2 | news | 3600 | - | DATA | 12 | 0 | 100 | rt=article |

## `native/collectors/sources/world_reg_asia.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ASIA_REGISTRY | government | 0 | company=Toyota | WAF_BLOCKED | 0 | 0 | 41489 | reads ctx->entity |

## `native/collectors/sources/yahoo_auctions.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| yahoo-auctions | classifieds | 21600 | - | DATA | 1 | 0 | 1088 | rt=(null) |
