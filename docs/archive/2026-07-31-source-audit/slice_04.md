# Audit slice 4 — 206 sources across 65 files

`verdict` is from the bulk sweep and is ADVISORY: a blank means the sweep had not reached it, and an EMPTY may just mean the sweep guessed the wrong pivot entity. Re-run anything you report on.

## `native/collectors/sources/anime_pilgrimage.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| anime-pilgrimage | culture | 2592000 | - | DATA | 783 | 783 | 43254 | rt=anime-pilgrimage |

## `native/collectors/sources/bird_flu_outbreaks.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bird-flu-outbreaks | wildlife | 86400 | - | RC_ERROR | 0 | 0 | 855 |  |

## `native/collectors/sources/bus_routes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bus-routes | transport | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/cam_webcamendirect_list.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-webcamendirect_list | cyber | 21600 | - | DATA | 21 | 21 | 1907 | rt=camera |

## `native/collectors/sources/ccs_projects.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ccs-projects | industry | 2592000 | - | DATA | 9 | 9 | 2125 | rt=ccs-projects |

## `native/collectors/sources/cisa_kev_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cisa-kev-jp | cyber | 21600 | - | DATA | 46 | 46 | 257 | rt=cisa-kev-jp |

## `native/collectors/sources/convenience_stores.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| convenience-stores | marketplace | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/credential_leak.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CREDENTIAL_LEAK_SEARCH | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/data_extractor.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DATA_EXTRACTOR | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/docomo_mobaku.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| docomo-mobaku | social | 3600 | - | DATA | 1 | 0 | 946 | rt=(null) |

## `native/collectors/sources/edinet_filings.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| edinet-filings | government | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/estat_employment.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| estat-employment | statistics | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/feodo_tracker_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| feodo-tracker-jp | cyber | 3600 | - | NO_TITLE | 1 | 1 | 57 | rt=feodo-tracker-jp |

## `native/collectors/sources/flightradar_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| flightradar-jp | transport | 60 | - | EMPTY | 0 | 0 | 143 |  |

## `native/collectors/sources/gbizinfo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GBIZINFO | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/github_leaks_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| github-leaks-jp | cyber | 21600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/global_openalex.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| OPENALEX | investigation | 0 | doi=10.1038/nature12373 | DATA | 15 | 0 | 1315 | reads ctx->entity; rt=openalex-work |

## `native/collectors/sources/government_buildings.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| government-buildings | government | 604800 | - | DATA | 4705 | 4705 | 63404 | rt=pension_fund,register_office,government |

## `native/collectors/sources/hatena_bookmark.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| hatena-bookmark | social | 1800 | - | DATA | 40 | 0 | 127 | rt=hatena-bookmark |

## `native/collectors/sources/hospital_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| hospital-map | health | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/internet_exchanges.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| internet-exchanges | telecom | 86400 | - | DATA | 29 | 29 | 1041 | rt=internet-exchanges |

## `native/collectors/sources/japan_post_offices.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| japan-post-offices | infrastructure | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/jftc_mergers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jftc-mergers | government | 86400 | - | DATA | 1 | 0 | 167 | rt=(null) |

## `native/collectors/sources/jma_pollen.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-pollen | environment | 3600 | - | DATA | 1 | 0 | 11 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/jnto_arrivals.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jnto-arrivals | government | 86400 | - | DATA | 1 | 0 | 806 | rt=(null) |

## `native/collectors/sources/jpx_quotes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jpx-quotes | government | 21600 | - | DATA | 1 | 0 | 8568 | rt=(null) |

## `native/collectors/sources/k_net.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| k-net | environment | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/leakix_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| leakix-jp | cyber | 7200 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/manhole_covers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| manhole-covers | infrastructure | 2592000 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/mext_schools.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mext-schools | government | 604800 | - | DATA | 1 | 0 | 8590 | rt=(null) |

## `native/collectors/sources/mlit_dam.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-dam | infrastructure | 600 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/mlit_road_traffic.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-road-traffic | transport | 604800 | - | DATA | 1 | 0 | 1629 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/national_parks.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| national-parks | tourism | 2592000 | - | DATA | 14 | 14 | 8440 | rt=national,protected_area |

## `native/collectors/sources/news_world.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| WORLD_NEWS_RSS | news | 1800 | - | DATA | 614 | 0 | 51869 | reads ctx->entity; rt=world-news-item |

## `native/collectors/sources/nied_mowlas.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nied-mowlas | seismic | 3600 | - | DATA | 1 | 0 | 8565 | rt=(null) |

## `native/collectors/sources/npa_missing_persons.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| npa-missing-persons | safety | 604800 | - | DATA | 6 | 5 | 855 | rt=npa-missing-persons,(null) |

## `native/collectors/sources/odpt_station.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| odpt-station | transport | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/openstreetmap_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| openstreetmap-jp | geospatial | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/overpass_rail_tracks.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| overpass-rail-tracks | investigation | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/pdf_analyzer.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| PDF_ANALYZER | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/pm25_japan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| pm25-japan | environment | 3600 | - | EMPTY | 0 | 0 | 6242 |  |

## `native/collectors/sources/procurement_ocds.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| PROCUREMENT_OCDS | government | 0 | keyword=Tokyo | DATA | 94 | 0 | 71929 | reads ctx->entity; rt=ocds-release |

## `native/collectors/sources/red_light_zones.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| red-light-zones | crime | 2592000 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/reddit_world_geo.c`  (142 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| reddit-afghanistan | social | 1800 | - | RC_ERROR | 0 | 0 | 1102 |  |
| reddit-africa | social | 1800 | - | RC_ERROR | 0 | 0 | 1075 |  |
| reddit-anime-titties | social | 1800 | - | RC_ERROR | 0 | 0 | 1128 |  |
| reddit-argentina | social | 1800 | - | RC_ERROR | 0 | 0 | 9543 |  |
| reddit-armenia | social | 1800 | - | RC_ERROR | 0 | 0 | 9529 |  |
| reddit-asia | social | 1800 | - | RC_ERROR | 0 | 0 | 1070 |  |
| reddit-australia | social | 1800 | - | RC_ERROR | 0 | 0 | 1084 |  |
| reddit-austria | social | 1800 | - | RC_ERROR | 0 | 0 | 17751 |  |
| reddit-aviation | social | 1800 | - | RC_ERROR | 0 | 0 | 1277 |  |
| reddit-azerbaijan | social | 1800 | - | RC_ERROR | 0 | 0 | 9574 |  |
| reddit-bangkok | social | 1800 | - | RC_ERROR | 0 | 0 | 9534 |  |
| reddit-bangladesh | social | 1800 | - | RC_ERROR | 0 | 0 | 17859 |  |
| reddit-beijing | social | 1800 | - | RC_ERROR | 0 | 0 | 1156 |  |
| reddit-belarus | social | 1800 | - | RC_ERROR | 0 | 0 | 9485 |  |
| reddit-belgium | social | 1800 | - | RC_ERROR | 0 | 0 | 9674 |  |
| reddit-berlin | social | 1800 | - | RC_ERROR | 0 | 0 | 1130 |  |
| reddit-blackhat | social | 1800 | - | RC_ERROR | 0 | 0 | 1181 |  |
| reddit-bolivia | social | 1800 | - | RC_ERROR | 0 | 0 | 9551 |  |
| reddit-brasil | social | 1800 | - | RC_ERROR | 0 | 0 | 17917 |  |
| reddit-bulgaria | social | 1800 | - | RC_ERROR | 0 | 0 | 1347 |  |
| reddit-canada | social | 1800 | - | RC_ERROR | 0 | 0 | 1103 |  |
| reddit-chile | social | 1800 | - | RC_ERROR | 0 | 0 | 17958 |  |
| reddit-china | social | 1800 | - | RC_ERROR | 0 | 0 | 9731 |  |
| reddit-china-debate | social | 1800 | - | RC_ERROR | 0 | 0 | 1277 |  |
| reddit-china-irl | social | 1800 | - | RC_ERROR | 0 | 0 | 1137 |  |
| reddit-colombia | social | 1800 | - | RC_ERROR | 0 | 0 | 17957 |  |
| reddit-combatfootage | social | 1800 | - | RC_ERROR | 0 | 0 | 1162 |  |
| reddit-credibledefense | social | 1800 | - | RC_ERROR | 0 | 0 | 1115 |  |
| reddit-croatia | social | 1800 | - | RC_ERROR | 0 | 0 | 1310 |  |
| reddit-cybersecurity | social | 1800 | - | RC_ERROR | 0 | 0 | 1115 |  |
| reddit-czech | social | 1800 | - | RC_ERROR | 0 | 0 | 1107 |  |
| reddit-delhi | social | 1800 | - | DATA | 25 | 0 | 10330 | rt=article |
| reddit-denmark | social | 1800 | - | RC_ERROR | 0 | 0 | 1115 |  |
| reddit-dubai | social | 1800 | - | RC_ERROR | 0 | 0 | 18028 |  |
| reddit-ecuador | social | 1800 | - | RC_ERROR | 0 | 0 | 17927 |  |
| reddit-eesti | social | 1800 | - | RC_ERROR | 0 | 0 | 1113 |  |
| reddit-egypt | social | 1800 | - | RC_ERROR | 0 | 0 | 18015 |  |
| reddit-ethiopia | social | 1800 | - | RC_ERROR | 0 | 0 | 1134 |  |
| reddit-europe | social | 1800 | - | RC_ERROR | 0 | 0 | 1111 |  |
| reddit-finland | social | 1800 | - | RC_ERROR | 0 | 0 | 9697 |  |
| reddit-france | social | 1800 | - | RC_ERROR | 0 | 0 | 1129 |  |
| reddit-geopolitics | social | 1800 | - | DATA | 25 | 0 | 786 | rt=article |
| reddit-geopolitics2 | social | 1800 | - | RC_ERROR | 0 | 0 | 1114 |  |
| reddit-germany | social | 1800 | - | RC_ERROR | 0 | 0 | 1116 |  |
| reddit-ghana | social | 1800 | - | RC_ERROR | 0 | 0 | 9720 |  |
| reddit-greece | social | 1800 | - | RC_ERROR | 0 | 0 | 1110 |  |
| reddit-hackernews | social | 1800 | - | RC_ERROR | 0 | 0 | 1132 |  |
| reddit-hongkong | social | 1800 | - | RC_ERROR | 0 | 0 | 26431 |  |
| reddit-hongkong-2 | social | 1800 | - | RC_ERROR | 0 | 0 | 1141 |  |
| reddit-hungary | social | 1800 | - | RC_ERROR | 0 | 0 | 1250 |  |
| reddit-iceland | social | 1800 | - | RC_ERROR | 0 | 0 | 9792 |  |
| reddit-india | social | 1800 | - | RC_ERROR | 0 | 0 | 1104 |  |
| reddit-indiaspeaks | social | 1800 | - | RC_ERROR | 0 | 0 | 1135 |  |
| reddit-indonesia | social | 1800 | - | RC_ERROR | 0 | 0 | 9649 |  |
| reddit-intelligence | social | 1800 | - | RC_ERROR | 0 | 0 | 1109 |  |
| reddit-iran | social | 1800 | - | RC_ERROR | 0 | 0 | 26454 |  |
| reddit-iraq | social | 1800 | - | RC_ERROR | 0 | 0 | 1114 |  |
| reddit-ireland | social | 1800 | - | RC_ERROR | 0 | 0 | 1115 |  |
| reddit-israel | social | 1800 | - | RC_ERROR | 0 | 0 | 9642 |  |
| reddit-israelpalestine | social | 1800 | - | RC_ERROR | 0 | 0 | 1139 |  |
| reddit-istanbul | social | 1800 | - | RC_ERROR | 0 | 0 | 9544 |  |
| reddit-italy | social | 1800 | - | RC_ERROR | 0 | 0 | 1115 |  |
| reddit-jakarta | social | 1800 | - | RC_ERROR | 0 | 0 | 1137 |  |
| reddit-japan | social | 1800 | - | RC_ERROR | 0 | 0 | 17990 |  |
| reddit-jordan | social | 1800 | - | RC_ERROR | 0 | 0 | 1148 |  |
| reddit-kazakhstan | social | 1800 | - | RC_ERROR | 0 | 0 | 9708 |  |
| reddit-kenya | social | 1800 | - | RC_ERROR | 0 | 0 | 9542 |  |
| reddit-korea | social | 1800 | - | RC_ERROR | 0 | 0 | 26479 |  |
| reddit-kyiv | social | 1800 | - | RC_ERROR | 0 | 0 | 1124 |  |
| reddit-latinamerica | social | 1800 | - | RC_ERROR | 0 | 0 | 1186 |  |
| reddit-latvia | social | 1800 | - | RC_ERROR | 0 | 0 | 9535 |  |
| reddit-lebanon | social | 1800 | - | RC_ERROR | 0 | 0 | 1144 |  |
| reddit-lesscredibledefence | social | 1800 | - | RC_ERROR | 0 | 0 | 1109 |  |
| reddit-lithuania | social | 1800 | - | RC_ERROR | 0 | 0 | 9494 |  |
| reddit-london | social | 1800 | - | RC_ERROR | 0 | 0 | 9547 |  |
| reddit-losangeles | social | 1800 | - | RC_ERROR | 0 | 0 | 9546 |  |
| reddit-malaysia | social | 1800 | - | RC_ERROR | 0 | 0 | 1160 |  |
| reddit-malware | social | 1800 | - | RC_ERROR | 0 | 0 | 1134 |  |
| reddit-melbourne | social | 1800 | - | RC_ERROR | 0 | 0 | 18145 |  |
| reddit-mexico | social | 1800 | - | RC_ERROR | 0 | 0 | 1089 |  |
| reddit-middleeastnews | social | 1800 | - | RC_ERROR | 0 | 0 | 1143 |  |
| reddit-military | social | 1800 | - | RC_ERROR | 0 | 0 | 1117 |  |
| reddit-mongolia | social | 1800 | - | RC_ERROR | 0 | 0 | 9550 |  |
| reddit-morocco | social | 1800 | - | RC_ERROR | 0 | 0 | 9554 |  |
| reddit-moscow | social | 1800 | - | RC_ERROR | 0 | 0 | 17990 |  |
| reddit-mumbai | social | 1800 | - | RC_ERROR | 0 | 0 | 18042 |  |
| reddit-myanmar | social | 1800 | - | RC_ERROR | 0 | 0 | 9371 |  |
| reddit-natsecpol | social | 1800 | - | RC_ERROR | 0 | 0 | 1122 |  |
| reddit-navy | social | 1800 | - | RC_ERROR | 0 | 0 | 1144 |  |
| reddit-nepal | social | 1800 | - | DATA | 25 | 0 | 1019 | rt=article |
| reddit-netherlands | social | 1800 | - | RC_ERROR | 0 | 0 | 1138 |  |
| reddit-netsec | social | 1800 | - | RC_ERROR | 0 | 0 | 1133 |  |
| reddit-neutralnews | social | 1800 | - | DATA | 25 | 0 | 734 | rt=article |
| reddit-news | social | 1800 | - | RC_ERROR | 0 | 0 | 1130 |  |
| reddit-newzealand | social | 1800 | - | RC_ERROR | 0 | 0 | 1099 |  |
| reddit-nigeria | social | 1800 | - | RC_ERROR | 0 | 0 | 9470 |  |
| reddit-norway | social | 1800 | - | RC_ERROR | 0 | 0 | 1112 |  |
| reddit-nyc | social | 1800 | - | RC_ERROR | 0 | 0 | 9556 |  |
| reddit-osint | social | 1800 | - | RC_ERROR | 0 | 0 | 1167 |  |
| reddit-pakistan | social | 1800 | - | RC_ERROR | 0 | 0 | 9519 |  |
| reddit-paris | social | 1800 | - | RC_ERROR | 0 | 0 | 9542 |  |
| reddit-peru | social | 1800 | - | DATA | 25 | 0 | 891 | rt=article |
| reddit-philippines | social | 1800 | - | RC_ERROR | 0 | 0 | 1156 |  |
| reddit-poland | social | 1800 | - | RC_ERROR | 0 | 0 | 9667 |  |
| reddit-portugal | social | 1800 | - | RC_ERROR | 0 | 0 | 1280 |  |
| reddit-qatar | social | 1800 | - | RC_ERROR | 0 | 0 | 18000 |  |
| reddit-qualitynews | social | 1800 | - | RC_ERROR | 0 | 0 | 1098 |  |
| reddit-romania | social | 1800 | - | RC_ERROR | 0 | 0 | 1107 |  |
| reddit-russia | social | 1800 | - | RC_ERROR | 0 | 0 | 1141 |  |
| reddit-sakartvelo | social | 1800 | - | RC_ERROR | 0 | 0 | 9554 |  |
| reddit-saudiarabia | social | 1800 | - | RC_ERROR | 0 | 0 | 9573 |  |
| reddit-seoul | social | 1800 | - | RC_ERROR | 0 | 0 | 9517 |  |
| reddit-serbia | social | 1800 | - | RC_ERROR | 0 | 0 | 9506 |  |
| reddit-shanghai | social | 1800 | - | RC_ERROR | 0 | 0 | 9570 |  |
| reddit-singapore | social | 1800 | - | RC_ERROR | 0 | 0 | 1113 |  |
| reddit-slovakia | social | 1800 | - | RC_ERROR | 0 | 0 | 9496 |  |
| reddit-southafrica | social | 1800 | - | RC_ERROR | 0 | 0 | 9547 |  |
| reddit-southasia | social | 1800 | - | RC_ERROR | 0 | 0 | 1085 |  |
| reddit-spain | social | 1800 | - | RC_ERROR | 0 | 0 | 1121 |  |
| reddit-srilanka | social | 1800 | - | RC_ERROR | 0 | 0 | 9462 |  |
| reddit-sweden | social | 1800 | - | RC_ERROR | 0 | 0 | 9664 |  |
| reddit-switzerland | social | 1800 | - | RC_ERROR | 0 | 0 | 1271 |  |
| reddit-sydney | social | 1800 | - | RC_ERROR | 0 | 0 | 18002 |  |
| reddit-syria | social | 1800 | - | RC_ERROR | 0 | 0 | 9502 |  |
| reddit-syriancivilwar | social | 1800 | - | RC_ERROR | 0 | 0 | 1117 |  |
| reddit-taiwan | social | 1800 | - | RC_ERROR | 0 | 0 | 9459 |  |
| reddit-tankporn | social | 1800 | - | RC_ERROR | 0 | 0 | 1141 |  |
| reddit-thailand | social | 1800 | - | RC_ERROR | 0 | 0 | 18218 |  |
| reddit-tokyo | social | 1800 | - | RC_ERROR | 0 | 0 | 1150 |  |
| reddit-toronto | social | 1800 | - | RC_ERROR | 0 | 0 | 17977 |  |
| reddit-turkey | social | 1800 | - | RC_ERROR | 0 | 0 | 9714 |  |
| reddit-ukpolitics | social | 1800 | - | RC_ERROR | 0 | 0 | 1129 |  |
| reddit-ukraina | social | 1800 | - | RC_ERROR | 0 | 0 | 1095 |  |
| reddit-ukrainewarvideoreport | social | 1800 | - | RC_ERROR | 0 | 0 | 1170 |  |
| reddit-unitedkingdom | social | 1800 | - | RC_ERROR | 0 | 0 | 1116 |  |
| reddit-uruguay | social | 1800 | - | RC_ERROR | 0 | 0 | 1287 |  |
| reddit-uzbekistan | social | 1800 | - | RC_ERROR | 0 | 0 | 9564 |  |
| reddit-vietnam | social | 1800 | - | RC_ERROR | 0 | 0 | 18044 |  |
| reddit-vzla | social | 1800 | - | RC_ERROR | 0 | 0 | 9552 |  |
| reddit-warcollege | social | 1800 | - | RC_ERROR | 0 | 0 | 1128 |  |
| reddit-worldnews | social | 1800 | - | DATA | 25 | 0 | 770 | rt=article |
| reddit-yemenicrisis | social | 1800 | - | RC_ERROR | 0 | 0 | 1110 |  |

## `native/collectors/sources/reg_cz_ares.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CZ_ARES | government | 0 | company=Toyota | DATA | 12 | 0 | 286 | reads ctx->entity; rt=ares-subject |

## `native/collectors/sources/reg_ie_cro.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| IE_CRO | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/reg_ro.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| RO_COMPANIES | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 59 | reads ctx->entity |

## `native/collectors/sources/regional_grid_outages.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| regional-grid-outages | infrastructure | 600 | - | DATA | 9 | 0 | 19381 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/reverse_whois.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| HISTORICAL_WHOIS | cyber | 0 | domain=github.com | EMPTY | 0 | 0 | 1860 | reads ctx->entity |

## `native/collectors/sources/sans_isc.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| sans-isc | cyber | 3600 | - | DATA | 11 | 0 | 438 | rt=article |

## `native/collectors/sources/sentinel_japan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| sentinel-japan | satellite | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/shodan_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SHODAN_SEARCH | cyber | 0 | ip=8.8.8.8 | DATA | 3 | 0 | 71 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/spamhaus_drop.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| spamhaus-drop | cyber | 3600 | - | NO_TITLE | 2086 | 2086 | 2706 | rt=cidr,asn |

## `native/collectors/sources/subdomain_osint.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SUBDOMAIN_FINDER | cyber | 0 | domain=github.com | TIMEOUT | 0 | 0 | None | reads ctx->entity; TIMED OUT |

## `native/collectors/sources/tech_stack.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| TECH_STACK_DETECTION | cyber | 0 | domain=github.com | DATA | 2 | 0 | 205 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/themed_cafes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| themed-cafes | culture | 2592000 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/tor_exit_check.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| TOR_EXIT_CHECK | cyber | 0 | ip=8.8.8.8 | DATA | 1 | 0 | 157 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/twitter_geo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| twitter-geo | social | 600 | - | EMPTY | 0 | 0 | 0 |  |

## `native/collectors/sources/unified_subways.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unified-subways | transport | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/vending_machines.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| vending-machines | culture | 2592000 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/wagyu_ranches.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wagyu-ranches | food | 2592000 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/weather_service.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| WEATHER_SERVICE | environment | 0 | keyword=Tokyo | DATA | 8 | 8 | 636 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/wifi_networks_mls.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wifi-networks-mls | cyber | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/wolfx_eqlist.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wolfx-eqlist | environment | 60 | - | NO_TITLE | 50 | 50 | 1133 | rt=wolfx-eqlist |

## `native/collectors/sources/world_reg_uk.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| UK_REGISTRY | government | 0 | company=Toyota | CRASH | 15 | 0 | None | reads ctx->entity; rt=uk-registry-company,uk-registry-officer,uk-registry-regulator |
