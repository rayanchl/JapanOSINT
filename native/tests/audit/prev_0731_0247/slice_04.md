# Audit slice 4 — 206 sources across 65 files

`verdict` is from the bulk sweep and is ADVISORY: a blank means the sweep had not reached it, and an EMPTY may just mean the sweep guessed the wrong pivot entity. Re-run anything you report on.

## `native/collectors/sources/anime_pilgrimage.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| anime-pilgrimage | culture | 2592000 | - | RC_ERROR | 0 | 0 | 103664 |  |

## `native/collectors/sources/bird_flu_outbreaks.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bird-flu-outbreaks | wildlife | 86400 | - | RC_ERROR | 0 | 0 | 802 |  |

## `native/collectors/sources/bus_routes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bus-routes | transport | 86400 | - | DATA | 692 | 692 | 77200 | rt=bus-routes |

## `native/collectors/sources/cam_webcamendirect_list.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-webcamendirect_list | cyber | 21600 | - | DATA | 21 | 21 | 1913 | rt=camera |

## `native/collectors/sources/ccs_projects.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ccs-projects | industry | 2592000 | - | DATA | 9 | 9 | 2058 | rt=ccs-projects |

## `native/collectors/sources/cisa_kev_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cisa-kev-jp | cyber | 21600 | - | DATA | 46 | 46 | 418 | rt=cisa-kev-jp |

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
| docomo-mobaku | social | 3600 | - | DATA | 1 | 0 | 844 | rt=(null) |

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
| feodo-tracker-jp | cyber | 3600 | - | NO_TITLE | 1 | 1 | 61 | rt=feodo-tracker-jp |

## `native/collectors/sources/flightradar_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| flightradar-jp | transport | 60 | - | EMPTY | 0 | 0 | 90 |  |

## `native/collectors/sources/gbizinfo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GBIZINFO | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/github_leaks_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| github-leaks-jp | cyber | 21600 | - | RC_ERROR | 0 | 0 | 0 |  |

## `native/collectors/sources/global_openalex.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| OPENALEX | investigation | 0 | doi=10.1038/nature12373 | DATA | 15 | 0 | 1059 | reads ctx->entity; rt=openalex-work |

## `native/collectors/sources/government_buildings.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| government-buildings | government | 604800 | - | RC_ERROR | 0 | 0 | 75075 |  |

## `native/collectors/sources/hatena_bookmark.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| hatena-bookmark | social | 1800 | - | DATA | 40 | 0 | 743 | rt=hatena-bookmark |

## `native/collectors/sources/hospital_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| hospital-map | health | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/internet_exchanges.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| internet-exchanges | telecom | 86400 | - | DATA | 29 | 29 | 979 | rt=internet-exchanges |

## `native/collectors/sources/japan_post_offices.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| japan-post-offices | infrastructure | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/jftc_mergers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jftc-mergers | government | 86400 | - | DATA | 1 | 0 | 76 | rt=(null) |

## `native/collectors/sources/jma_pollen.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-pollen | environment | 3600 | - | DATA | 1 | 0 | 11 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/jnto_arrivals.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jnto-arrivals | government | 86400 | - | DATA | 1 | 0 | 780 | rt=(null) |

## `native/collectors/sources/jpx_quotes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jpx-quotes | government | 21600 | - | DATA | 1 | 0 | 8006 | rt=(null) |

## `native/collectors/sources/k_net.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| k-net | environment | 86400 | - | RC_ERROR | 0 | 0 | 52412 |  |

## `native/collectors/sources/leakix_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| leakix-jp | cyber | 7200 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/manhole_covers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| manhole-covers | infrastructure | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/mext_schools.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mext-schools | government | 604800 | - | DATA | 1 | 0 | 8002 | rt=(null) |

## `native/collectors/sources/mlit_dam.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-dam | infrastructure | 600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/mlit_road_traffic.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-road-traffic | transport | 604800 | - | DATA | 1 | 0 | 966 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/national_parks.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| national-parks | tourism | 2592000 | - | DATA | 14 | 14 | 9003 | rt=national,protected_area |

## `native/collectors/sources/news_world.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| WORLD_NEWS_RSS | news | 1800 | - | (not swept) | - | - | - | reads ctx->entity |

## `native/collectors/sources/nied_mowlas.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nied-mowlas | seismic | 3600 | - | DATA | 1 | 0 | 8001 | rt=(null) |

## `native/collectors/sources/npa_missing_persons.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| npa-missing-persons | safety | 604800 | - | DATA | 6 | 5 | 776 | rt=npa-missing-persons,(null) |

## `native/collectors/sources/odpt_station.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| odpt-station | transport | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/openstreetmap_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| openstreetmap-jp | geospatial | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/overpass_rail_tracks.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| overpass-rail-tracks | investigation | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/pdf_analyzer.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| PDF_ANALYZER | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/pm25_japan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| pm25-japan | environment | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/procurement_ocds.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| PROCUREMENT_OCDS | government | 0 | keyword=Tokyo | DATA | 94 | 0 | 43914 | reads ctx->entity; rt=ocds-release |

## `native/collectors/sources/red_light_zones.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| red-light-zones | crime | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/reddit_world_geo.c`  (142 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| reddit-afghanistan | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-africa | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-anime-titties | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-argentina | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-armenia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-asia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-australia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-austria | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-aviation | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-azerbaijan | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-bangkok | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-bangladesh | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-beijing | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-belarus | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-belgium | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-berlin | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-blackhat | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-bolivia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-brasil | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-bulgaria | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-canada | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-chile | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-china | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-china-debate | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-china-irl | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-colombia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-combatfootage | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-credibledefense | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-croatia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-cybersecurity | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-czech | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-delhi | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-denmark | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-dubai | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-ecuador | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-eesti | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-egypt | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-ethiopia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-europe | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-finland | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-france | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-geopolitics | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-geopolitics2 | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-germany | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-ghana | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-greece | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-hackernews | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-hongkong | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-hongkong-2 | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-hungary | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-iceland | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-india | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-indiaspeaks | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-indonesia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-intelligence | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-iran | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-iraq | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-ireland | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-israel | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-israelpalestine | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-istanbul | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-italy | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-jakarta | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-japan | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-jordan | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-kazakhstan | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-kenya | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-korea | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-kyiv | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-latinamerica | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-latvia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-lebanon | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-lesscredibledefence | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-lithuania | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-london | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-losangeles | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-malaysia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-malware | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-melbourne | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-mexico | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-middleeastnews | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-military | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-mongolia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-morocco | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-moscow | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-mumbai | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-myanmar | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-natsecpol | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-navy | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-nepal | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-netherlands | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-netsec | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-neutralnews | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-news | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-newzealand | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-nigeria | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-norway | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-nyc | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-osint | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-pakistan | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-paris | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-peru | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-philippines | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-poland | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-portugal | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-qatar | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-qualitynews | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-romania | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-russia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-sakartvelo | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-saudiarabia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-seoul | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-serbia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-shanghai | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-singapore | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-slovakia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-southafrica | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-southasia | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-spain | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-srilanka | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-sweden | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-switzerland | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-sydney | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-syria | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-syriancivilwar | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-taiwan | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-tankporn | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-thailand | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-tokyo | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-toronto | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-turkey | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-ukpolitics | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-ukraina | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-ukrainewarvideoreport | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-unitedkingdom | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-uruguay | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-uzbekistan | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-vietnam | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-vzla | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-warcollege | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-worldnews | social | 1800 | - | (not swept) | - | - | - |  |
| reddit-yemenicrisis | social | 1800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/reg_cz_ares.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CZ_ARES | government | 0 | company=Toyota | DATA | 12 | 0 | 324 | reads ctx->entity; rt=ares-subject |

## `native/collectors/sources/reg_ie_cro.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| IE_CRO | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/reg_ro.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| RO_COMPANIES | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 102 | reads ctx->entity |

## `native/collectors/sources/regional_grid_outages.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| regional-grid-outages | infrastructure | 600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/reverse_whois.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| HISTORICAL_WHOIS | cyber | 0 | domain=github.com | EMPTY | 0 | 0 | 1224 | reads ctx->entity |

## `native/collectors/sources/sans_isc.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| sans-isc | cyber | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/sentinel_japan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| sentinel-japan | satellite | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/shodan_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SHODAN_SEARCH | cyber | 0 | ip=8.8.8.8 | DATA | 3 | 0 | 62 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/spamhaus_drop.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| spamhaus-drop | cyber | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/subdomain_osint.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SUBDOMAIN_FINDER | cyber | 0 | domain=github.com | TIMEOUT | 0 | 0 | None | reads ctx->entity; TIMED OUT |

## `native/collectors/sources/tech_stack.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| TECH_STACK_DETECTION | cyber | 0 | domain=github.com | DATA | 2 | 0 | 186 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/themed_cafes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| themed-cafes | culture | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/tor_exit_check.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| TOR_EXIT_CHECK | cyber | 0 | ip=8.8.8.8 | DATA | 1 | 0 | 136 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/twitter_geo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| twitter-geo | social | 600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/unified_subways.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unified-subways | transport | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/vending_machines.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| vending-machines | culture | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/wagyu_ranches.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wagyu-ranches | food | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/weather_service.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| WEATHER_SERVICE | environment | 0 | keyword=Tokyo | DATA | 8 | 8 | 663 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/wifi_networks_mls.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wifi-networks-mls | cyber | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/wolfx_eqlist.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wolfx-eqlist | environment | 60 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/world_reg_uk.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| UK_REGISTRY | government | 0 | company=Toyota | CRASH | 15 | 0 | None | reads ctx->entity; rt=uk-registry-company,uk-registry-officer,uk-registry-regulator |
