# Audit slice 3 — 207 sources across 66 files

`verdict` is from the bulk sweep and is ADVISORY: a blank means the sweep had not reached it, and an EMPTY may just mean the sweep guessed the wrong pivot entity. Re-run anything you report on.

## `native/collectors/sources/amateur_radio_repeaters.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| amateur-radio-repeaters | telecom | 2592000 | - | RC_ERROR | 0 | 0 | 212 |  |

## `native/collectors/sources/bike_share_gbfs.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bike-share-gbfs | transport | 600 | - | DATA | 1000 | 0 | 5808 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/bridge_tunnel_infra.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bridge-tunnel-infra | investigation | 86400 | - | RC_ERROR | 0 | 0 | 110285 |  |

## `native/collectors/sources/cam_tabi_cam.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-tabi_cam | cyber | 21600 | - | DATA | 27 | 27 | 405 | rt=camera |

## `native/collectors/sources/castles.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| castles | tourism | 2592000 | - | DATA | 528 | 528 | 50825 | rt=castles |

## `native/collectors/sources/chiriin_place.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| chiriin-place | geospatial | 86400 | - | DATA | 71761 | 71761 | 83351 | rt=chiriin-place |

## `native/collectors/sources/company_lookup.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| COMPANY_LOOKUP | government | 0 | company=Toyota | EMPTY | 0 | 0 | 96 | reads ctx->entity |

## `native/collectors/sources/covid19_japan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| covid19-japan | statistics | 3600 | - | DATA | 30 | 0 | 1486 | rt=covid19-japan |

## `native/collectors/sources/data_centers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| data-centers | telecom | 604800 | - | RC_ERROR | 0 | 0 | 72733 |  |

## `native/collectors/sources/docomo_insight.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| docomo-insight | commercial | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/earthquake_network_citizen.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| earthquake-network-citizen | environment | 60 | - | DATA | 1 | 0 | 73 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/estat_education.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| estat-education | statistics | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/famous_places.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| famous-places | tourism | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/flight_tracker.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| FLIGHT_TRACKER | transport | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/gazette_world.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GAZETTE_WORLD | government | 0 | country=Japan | WAF_BLOCKED | 0 | 0 | 16739 | reads ctx->entity |

## `native/collectors/sources/ghsa_advisories.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ghsa-advisories | cyber | 3600 | - | DATA | 100 | 100 | 1188 | rt=ghsa-advisories |

## `native/collectors/sources/global_mediacloud.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| MEDIACLOUD | news | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/gnews_world_top.c`  (142 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gnews-top-ae | news | 3600 | - | DATA | 33 | 0 | 862 | rt=article |
| gnews-top-af | news | 3600 | - | DATA | 38 | 0 | 552 | rt=article |
| gnews-top-al | news | 3600 | - | DATA | 38 | 0 | 387 | rt=article |
| gnews-top-am | news | 3600 | - | DATA | 34 | 0 | 322 | rt=article |
| gnews-top-ao | news | 3600 | - | DATA | 34 | 0 | 229 | rt=article |
| gnews-top-ar | news | 3600 | - | DATA | 34 | 0 | 292 | rt=article |
| gnews-top-at | news | 3600 | - | DATA | 34 | 0 | 382 | rt=article |
| gnews-top-au | news | 3600 | - | DATA | 38 | 0 | 276 | rt=article |
| gnews-top-az | news | 3600 | - | DATA | 34 | 0 | 335 | rt=article |
| gnews-top-ba | news | 3600 | - | DATA | 38 | 0 | 364 | rt=article |
| gnews-top-bd | news | 3600 | - | DATA | 26 | 0 | 637 | rt=article |
| gnews-top-be | news | 3600 | - | DATA | 34 | 0 | 320 | rt=article |
| gnews-top-bg | news | 3600 | - | DATA | 26 | 0 | 401 | rt=article |
| gnews-top-bh | news | 3600 | - | DATA | 31 | 0 | 557 | rt=article |
| gnews-top-bn | news | 3600 | - | DATA | 38 | 0 | 469 | rt=article |
| gnews-top-bo | news | 3600 | - | DATA | 32 | 0 | 613 | rt=article |
| gnews-top-br | news | 3600 | - | DATA | 34 | 0 | 408 | rt=article |
| gnews-top-bs | news | 3600 | - | DATA | 38 | 0 | 404 | rt=article |
| gnews-top-bt | news | 3600 | - | DATA | 38 | 0 | 473 | rt=article |
| gnews-top-bw | news | 3600 | - | DATA | 38 | 0 | 690 | rt=article |
| gnews-top-by | news | 3600 | - | DATA | 34 | 0 | 259 | rt=article |
| gnews-top-ca | news | 3600 | - | DATA | 38 | 0 | 511 | rt=article |
| gnews-top-cd | news | 3600 | - | DATA | 34 | 0 | 264 | rt=article |
| gnews-top-ch | news | 3600 | - | DATA | 34 | 0 | 590 | rt=article |
| gnews-top-ci | news | 3600 | - | DATA | 34 | 0 | 284 | rt=article |
| gnews-top-cl | news | 3600 | - | DATA | 34 | 0 | 473 | rt=article |
| gnews-top-cm | news | 3600 | - | DATA | 34 | 0 | 316 | rt=article |
| gnews-top-cn | news | 3600 | - | DB_ERROR | 26 | 0 | 888 | db:OperationalError: Could not decode to UT; rt=article |
| gnews-top-co | news | 3600 | - | DATA | 34 | 0 | 731 | rt=article |
| gnews-top-cr | news | 3600 | - | DATA | 32 | 0 | 525 | rt=article |
| gnews-top-cu | news | 3600 | - | DATA | 30 | 0 | 615 | rt=article |
| gnews-top-cy | news | 3600 | - | DATA | 30 | 0 | 374 | rt=article |
| gnews-top-cz | news | 3600 | - | DATA | 30 | 0 | 220 | rt=article |
| gnews-top-de | news | 3600 | - | DATA | 34 | 0 | 444 | rt=article |
| gnews-top-dk | news | 3600 | - | DATA | 30 | 0 | 265 | rt=article |
| gnews-top-do | news | 3600 | - | DATA | 32 | 0 | 444 | rt=article |
| gnews-top-dz | news | 3600 | - | DATA | 31 | 0 | 386 | rt=article |
| gnews-top-ec | news | 3600 | - | DATA | 32 | 0 | 731 | rt=article |
| gnews-top-ee | news | 3600 | - | DATA | 27 | 0 | 561 | rt=article |
| gnews-top-eg | news | 3600 | - | DATA | 31 | 0 | 549 | rt=article |
| gnews-top-es | news | 3600 | - | DATA | 34 | 0 | 881 | rt=article |
| gnews-top-et | news | 3600 | - | DATA | 38 | 0 | 758 | rt=article |
| gnews-top-fi | news | 3600 | - | DATA | 30 | 0 | 600 | rt=article |
| gnews-top-fj | news | 3600 | - | DATA | 38 | 0 | 286 | rt=article |
| gnews-top-fr | news | 3600 | - | DATA | 34 | 0 | 448 | rt=article |
| gnews-top-gb | news | 3600 | - | DATA | 38 | 0 | 486 | rt=article |
| gnews-top-ge | news | 3600 | - | DATA | 38 | 0 | 234 | rt=article |
| gnews-top-gh | news | 3600 | - | DATA | 38 | 0 | 775 | rt=article |
| gnews-top-gr | news | 3600 | - | DATA | 30 | 0 | 222 | rt=article |
| gnews-top-gt | news | 3600 | - | DATA | 32 | 0 | 462 | rt=article |
| gnews-top-hk | news | 3600 | - | DATA | 30 | 0 | 440 | rt=article |
| gnews-top-hn | news | 3600 | - | DATA | 32 | 0 | 176 | rt=article |
| gnews-top-hr | news | 3600 | - | DATA | 38 | 0 | 216 | rt=article |
| gnews-top-hu | news | 3600 | - | DATA | 30 | 0 | 220 | rt=article |
| gnews-top-id | news | 3600 | - | DATA | 38 | 0 | 531 | rt=article |
| gnews-top-ie | news | 3600 | - | DATA | 38 | 0 | 225 | rt=article |
| gnews-top-il | news | 3600 | - | DATA | 34 | 0 | 586 | rt=article |
| gnews-top-in | news | 3600 | - | DATA | 38 | 0 | 420 | rt=article |
| gnews-top-iq | news | 3600 | - | DATA | 31 | 0 | 404 | rt=article |
| gnews-top-ir | news | 3600 | - | DATA | 38 | 0 | 297 | rt=article |
| gnews-top-is | news | 3600 | - | DATA | 38 | 0 | 325 | rt=article |
| gnews-top-it | news | 3600 | - | DATA | 34 | 0 | 514 | rt=article |
| gnews-top-jm | news | 3600 | - | DATA | 38 | 0 | 399 | rt=article |
| gnews-top-jo | news | 3600 | - | DATA | 31 | 0 | 406 | rt=article |
| gnews-top-jp | news | 3600 | - | DATA | 30 | 0 | 302 | rt=article |
| gnews-top-ke | news | 3600 | - | DATA | 37 | 0 | 774 | rt=article |
| gnews-top-kg | news | 3600 | - | DATA | 34 | 0 | 517 | rt=article |
| gnews-top-kh | news | 3600 | - | DATA | 38 | 0 | 263 | rt=article |
| gnews-top-kr | news | 3600 | - | DATA | 34 | 0 | 848 | rt=article |
| gnews-top-kw | news | 3600 | - | DATA | 31 | 0 | 320 | rt=article |
| gnews-top-kz | news | 3600 | - | DATA | 34 | 0 | 326 | rt=article |
| gnews-top-la | news | 3600 | - | DATA | 38 | 0 | 459 | rt=article |
| gnews-top-lb | news | 3600 | - | DATA | 34 | 0 | 617 | rt=article |
| gnews-top-lk | news | 3600 | - | DATA | 38 | 0 | 289 | rt=article |
| gnews-top-lt | news | 3600 | - | DATA | 26 | 0 | 448 | rt=article |
| gnews-top-lu | news | 3600 | - | DATA | 34 | 0 | 370 | rt=article |
| gnews-top-lv | news | 3600 | - | DATA | 26 | 0 | 425 | rt=article |
| gnews-top-ly | news | 3600 | - | DATA | 31 | 0 | 375 | rt=article |
| gnews-top-ma | news | 3600 | - | DATA | 31 | 0 | 412 | rt=article |
| gnews-top-md | news | 3600 | - | DATA | 35 | 0 | 243 | rt=article |
| gnews-top-me | news | 3600 | - | DATA | 38 | 0 | 396 | rt=article |
| gnews-top-mk | news | 3600 | - | DATA | 38 | 0 | 365 | rt=article |
| gnews-top-mm | news | 3600 | - | DATA | 38 | 0 | 277 | rt=article |
| gnews-top-mn | news | 3600 | - | DATA | 38 | 0 | 249 | rt=article |
| gnews-top-mo | news | 3600 | - | DATA | 38 | 0 | 474 | rt=article |
| gnews-top-mt | news | 3600 | - | DATA | 38 | 0 | 398 | rt=article |
| gnews-top-mv | news | 3600 | - | DATA | 38 | 0 | 458 | rt=article |
| gnews-top-mw | news | 3600 | - | DATA | 38 | 0 | 307 | rt=article |
| gnews-top-mx | news | 3600 | - | DATA | 34 | 0 | 481 | rt=article |
| gnews-top-my | news | 3600 | - | DATA | 38 | 0 | 649 | rt=article |
| gnews-top-mz | news | 3600 | - | DATA | 34 | 0 | 225 | rt=article |
| gnews-top-na | news | 3600 | - | DATA | 38 | 0 | 722 | rt=article |
| gnews-top-ng | news | 3600 | - | DATA | 38 | 0 | 554 | rt=article |
| gnews-top-ni | news | 3600 | - | DATA | 32 | 0 | 391 | rt=article |
| gnews-top-nl | news | 3600 | - | DATA | 34 | 0 | 611 | rt=article |
| gnews-top-no | news | 3600 | - | DATA | 30 | 0 | 208 | rt=article |
| gnews-top-np | news | 3600 | - | DATA | 38 | 0 | 271 | rt=article |
| gnews-top-nz | news | 3600 | - | DATA | 38 | 0 | 475 | rt=article |
| gnews-top-om | news | 3600 | - | DATA | 31 | 0 | 409 | rt=article |
| gnews-top-pa | news | 3600 | - | DATA | 32 | 0 | 479 | rt=article |
| gnews-top-pe | news | 3600 | - | DATA | 34 | 0 | 426 | rt=article |
| gnews-top-pg | news | 3600 | - | DATA | 38 | 0 | 205 | rt=article |
| gnews-top-ph | news | 3600 | - | DATA | 38 | 0 | 882 | rt=article |
| gnews-top-pk | news | 3600 | - | DATA | 38 | 0 | 342 | rt=article |
| gnews-top-pl | news | 3600 | - | DATA | 30 | 0 | 254 | rt=article |
| gnews-top-pt | news | 3600 | - | DATA | 34 | 0 | 201 | rt=article |
| gnews-top-py | news | 3600 | - | DATA | 32 | 0 | 619 | rt=article |
| gnews-top-qa | news | 3600 | - | DATA | 31 | 0 | 400 | rt=article |
| gnews-top-ro | news | 3600 | - | DATA | 35 | 0 | 237 | rt=article |
| gnews-top-rs | news | 3600 | - | DATA | 34 | 0 | 619 | rt=article |
| gnews-top-ru | news | 3600 | - | DATA | 34 | 0 | 410 | rt=article |
| gnews-top-rw | news | 3600 | - | DATA | 38 | 0 | 350 | rt=article |
| gnews-top-sa | news | 3600 | - | DATA | 34 | 0 | 555 | rt=article |
| gnews-top-sd | news | 3600 | - | DATA | 31 | 0 | 409 | rt=article |
| gnews-top-se | news | 3600 | - | DATA | 34 | 0 | 331 | rt=article |
| gnews-top-sg | news | 3600 | - | DATA | 38 | 0 | 698 | rt=article |
| gnews-top-si | news | 3600 | - | DATA | 34 | 0 | 574 | rt=article |
| gnews-top-sk | news | 3600 | - | DATA | 26 | 0 | 493 | rt=article |
| gnews-top-sn | news | 3600 | - | DATA | 34 | 0 | 279 | rt=article |
| gnews-top-so | news | 3600 | - | DATA | 38 | 0 | 377 | rt=article |
| gnews-top-sv | news | 3600 | - | DATA | 32 | 0 | 402 | rt=article |
| gnews-top-sy | news | 3600 | - | DATA | 31 | 0 | 554 | rt=article |
| gnews-top-th | news | 3600 | - | DB_ERROR | 35 | 0 | 549 | db:OperationalError: Could not decode to UT; rt=article |
| gnews-top-tj | news | 3600 | - | DATA | 34 | 0 | 528 | rt=article |
| gnews-top-tm | news | 3600 | - | DATA | 34 | 0 | 489 | rt=article |
| gnews-top-tn | news | 3600 | - | DATA | 31 | 0 | 365 | rt=article |
| gnews-top-tr | news | 3600 | - | DATA | 34 | 0 | 579 | rt=article |
| gnews-top-tt | news | 3600 | - | DATA | 38 | 0 | 391 | rt=article |
| gnews-top-tw | news | 3600 | - | DATA | 34 | 0 | 427 | rt=article |
| gnews-top-tz | news | 3600 | - | DATA | 38 | 0 | 786 | rt=article |
| gnews-top-ua | news | 3600 | - | DATA | 29 | 0 | 650 | rt=article |
| gnews-top-ug | news | 3600 | - | DATA | 38 | 0 | 868 | rt=article |
| gnews-top-us | news | 3600 | - | DATA | 38 | 0 | 550 | rt=article |
| gnews-top-uy | news | 3600 | - | DATA | 32 | 0 | 628 | rt=article |
| gnews-top-uz | news | 3600 | - | DATA | 34 | 0 | 321 | rt=article |
| gnews-top-ve | news | 3600 | - | DATA | 32 | 0 | 513 | rt=article |
| gnews-top-vn | news | 3600 | - | DATA | 26 | 0 | 398 | rt=article |
| gnews-top-xk | news | 3600 | - | DATA | 38 | 0 | 228 | rt=article |
| gnews-top-ye | news | 3600 | - | DATA | 31 | 0 | 354 | rt=article |
| gnews-top-za | news | 3600 | - | DATA | 38 | 0 | 194 | rt=article |
| gnews-top-zm | news | 3600 | - | DATA | 38 | 0 | 291 | rt=article |
| gnews-top-zw | news | 3600 | - | DATA | 38 | 0 | 871 | rt=article |

## `native/collectors/sources/google_my_maps.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| google-my-maps | tourism | 3600 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/hash_lookup.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| HASH_LOOKUP | cyber | 0 | hash=44d88612fea8a8f36de82e1278abb02f | EMPTY | 0 | 0 | 92 | reads ctx->entity |

## `native/collectors/sources/homes_co.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| homes-co | economy | 86400 | - | RC_ERROR | 0 | 0 | 493 |  |

## `native/collectors/sources/intelx_leaks.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| intelx-leaks | cyber | 21600 | - | DATA | 1 | 0 | 121 | rt=(null) |

## `native/collectors/sources/japan_api_prefectures.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| japan-api-prefectures | geospatial | 604800 | - | RC_ERROR | 0 | 0 | 791 |  |

## `native/collectors/sources/jcg_patrol.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jcg-patrol | safety | 604800 | - | EMPTY | 0 | 0 | 2381 |  |

## `native/collectors/sources/jma_ocean_wave.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-ocean-wave | environment | 3600 | - | DATA | 6 | 6 | 6197 | rt=jma-ocean-wave |

## `native/collectors/sources/jma_weather.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-weather | environment | 3600 | - | NO_TITLE | 1 | 1 | 738 | rt=jma-weather |

## `native/collectors/sources/jpo_trademarks.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jpo-trademarks | government | 86400 | - | DATA | 1 | 0 | 8002 | rt=(null) |

## `native/collectors/sources/jstat_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jstat-map | statistics | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/landsat_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| landsat-jp | satellite | 604800 | - | DATA | 50 | 0 | 683 | NO GEO despite map-ish category; rt=landsat-jp |

## `native/collectors/sources/manga_net_cafes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| manga-net-cafes | culture | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/mercari_trending.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mercari-trending | classifieds | 86400 | - | RC_ERROR | 0 | 0 | 505 |  |

## `native/collectors/sources/mlit_c02_ports.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-c02-ports | transport | 2592000 | - | EMPTY | 0 | 0 | 1791 |  |

## `native/collectors/sources/mlit_river.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-river | infrastructure | 600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/nasa_firms_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nasa-firms-jp | environment | 1800 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/news_archive.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| NEWS_ARCHIVE | news | 0 | keyword=Tokyo | DATA | 25 | 0 | 24724 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/nicter_stats.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nicter-stats | cyber | 86400 | - | RC_ERROR | 0 | 0 | 1306 |  |

## `native/collectors/sources/npa_important_wanted.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| npa-important-wanted | crime | 86400 | - | DATA | 10 | 10 | 696 | rt=npa-important-wanted |

## `native/collectors/sources/odpt_flight.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| odpt-flight | transport | 300 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/openmeteo_jma.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| openmeteo-jma | environment | 3600 | - | NO_TITLE | 9 | 9 | 346 | rt=openmeteo-jma |

## `native/collectors/sources/osv_dev.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| osv-dev | cyber | 21600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/patent_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| PATENT_SEARCH | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/phone_scam_hotspots.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| phone-scam-hotspots | crime | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/pref_police_crime.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| pref-police-crime | crime | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/real_estate.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| real-estate | marketplace | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/reg_ca_ised.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CA_CORPORATIONS | government | 0 | company=Toyota | EMPTY | 0 | 0 | 702 | reads ctx->entity |

## `native/collectors/sources/reg_gulf.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GULF_REGISTRY | government | 0 | company=Toyota | DATA | 6 | 0 | 9568 | reads ctx->entity; rt=gulf-registry-result |

## `native/collectors/sources/reg_pl_krs.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| PL_KRS | government | 0 | company=Toyota | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/reg_za_cipc.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ZA_CIPC | government | 0 | company=Toyota | WAF_BLOCKED | 0 | 0 | 1252 | reads ctx->entity |

## `native/collectors/sources/reverse_geocoding.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| REVERSE_GEOCODING | geospatial | 0 | coords=35.6762,139.6503 | DATA | 1 | 1 | 189 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/sanctions_check.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SANCTIONS_CHECK | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 133 | reads ctx->entity |

## `native/collectors/sources/semiconductor_fabs.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| semiconductor-fabs | industry | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/shodan_japan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| shodan-japan | cyber | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/soramame.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| soramame | environment | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/strava_segments_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| strava-segments-jp | cyber | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/tea_zones.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tea-zones | agriculture | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/theharvester.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EMAIL_HARVESTER | cyber | 0 | email=test@example.com | EMPTY | 0 | 0 | 32540 | reads ctx->entity |

## `native/collectors/sources/tochi_info.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tochi-info | economy | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/twitch_jp_streams.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| twitch-jp-streams | social | 600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/unified_port_infra.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unified-port-infra | transport | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/vehicle_lookup.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| VEHICLE_LOOKUP | transport | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/vrchat_active_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| vrchat-active-jp | social | 1800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/wdcgg_co2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wdcgg-co2 | environment | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/wifi_networks.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wifi-networks | investigation | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/wolfx_eew.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wolfx-eew | environment | 10 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/world_reg_mena.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| MENA_REGISTRY | government | 0 | company=Toyota | (not swept) | - | - | - | reads ctx->entity |

## `native/collectors/sources/yahoo_realtime.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| yahoo-realtime | social | 600 | - | (not swept) | - | - | - |  |
