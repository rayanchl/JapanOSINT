# Audit slice 9 — 206 sources across 83 files

`verdict` is from the bulk sweep and is ADVISORY: a blank means the sweep had not reached it, and an EMPTY may just mean the sweep guessed the wrong pivot entity. Re-run anything you report on.

## `native/collectors/sources/(unattributed)`  (10 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| chubu-power | infrastructure | 300 | - | EMPTY | 0 | 0 | 816 |  |
| chugoku-power | infrastructure | 300 | - | EMPTY | 0 | 0 | 2020 |  |
| hokkaido-power | infrastructure | 300 | - | EMPTY | 0 | 0 | 1395 |  |
| hokuriku-power | infrastructure | 300 | - | EMPTY | 0 | 0 | 655 |  |
| kepco-power | infrastructure | 300 | - | NO_TITLE | 1 | 1 | 1119 | rt=kepco-power |
| kyushu-power | infrastructure | 300 | - | EMPTY | 0 | 0 | 1595 |  |
| okinawa-power | infrastructure | 300 | - | EMPTY | 0 | 0 | 1797 |  |
| shikoku-power | infrastructure | 300 | - | EMPTY | 0 | 0 | 1634 |  |
| tepco-power | infrastructure | 300 | - | EMPTY | 0 | 0 | 632 |  |
| tohoku-power | infrastructure | 300 | - | EMPTY | 0 | 0 | 1524 |  |

## `native/collectors/sources/5g_coverage.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| 5g-coverage | telecom | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/academic_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ACADEMIC_SEARCH | news | 0 | doi=10.1038/nature12373 | DATA | 12 | 0 | 3828 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/alienvault_otx_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| alienvault-otx-jp | cyber | 7200 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/arxiv_feeds.c`  (40 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| arxiv-astro-ph-ep | news | 21600 | - | DATA | 19 | 0 | 178 | rt=article |
| arxiv-astro-ph-im | news | 21600 | - | DATA | 33 | 0 | 161 | rt=article |
| arxiv-cond-mat-mtrl-sci | news | 21600 | - | DATA | 61 | 0 | 235 | rt=article |
| arxiv-cs-ai | news | 21600 | - | DATA | 282 | 0 | 1193 | rt=article |
| arxiv-cs-ar | news | 21600 | - | DATA | 10 | 0 | 248 | rt=article |
| arxiv-cs-cl | news | 21600 | - | DATA | 139 | 0 | 571 | rt=article |
| arxiv-cs-cr | news | 21600 | - | DATA | 54 | 0 | 239 | rt=article |
| arxiv-cs-cv | news | 21600 | - | DATA | 201 | 0 | 765 | rt=article |
| arxiv-cs-cy | news | 21600 | - | DATA | 22 | 0 | 153 | rt=article |
| arxiv-cs-db | news | 21600 | - | DATA | 3 | 0 | 56 | rt=article |
| arxiv-cs-dc | news | 21600 | - | DATA | 17 | 0 | 168 | rt=article |
| arxiv-cs-ds | news | 21600 | - | DATA | 21 | 0 | 206 | rt=article |
| arxiv-cs-et | news | 21600 | - | DATA | 9 | 0 | 145 | rt=article |
| arxiv-cs-gt | news | 21600 | - | DATA | 21 | 0 | 196 | rt=article |
| arxiv-cs-hc | news | 21600 | - | DATA | 35 | 0 | 200 | rt=article |
| arxiv-cs-ir | news | 21600 | - | DATA | 38 | 0 | 176 | rt=article |
| arxiv-cs-lg | news | 21600 | - | DATA | 290 | 0 | 1271 | rt=article |
| arxiv-cs-ma | news | 21600 | - | DATA | 28 | 0 | 204 | rt=article |
| arxiv-cs-ne | news | 21600 | - | DATA | 2 | 0 | 172 | rt=article |
| arxiv-cs-ni | news | 21600 | - | DATA | 13 | 0 | 174 | rt=article |
| arxiv-cs-os | news | 21600 | - | EMPTY | 0 | 0 | 43 |  |
| arxiv-cs-pl | news | 21600 | - | DATA | 5 | 0 | 129 | rt=article |
| arxiv-cs-ro | news | 21600 | - | DATA | 58 | 0 | 232 | rt=article |
| arxiv-cs-se | news | 21600 | - | DATA | 44 | 0 | 236 | rt=article |
| arxiv-cs-si | news | 21600 | - | DATA | 11 | 0 | 303 | rt=article |
| arxiv-econ-gn | news | 21600 | - | DATA | 20 | 0 | 137 | rt=article |
| arxiv-eess-sp | news | 21600 | - | DATA | 23 | 0 | 188 | rt=article |
| arxiv-eess-sy | news | 21600 | - | DATA | 40 | 0 | 255 | rt=article |
| arxiv-math-oc | news | 21600 | - | DATA | 51 | 0 | 254 | rt=article |
| arxiv-nucl-ex | news | 21600 | - | DATA | 13 | 0 | 105 | rt=article |
| arxiv-physics-ao-ph | news | 21600 | - | DATA | 8 | 0 | 246 | rt=article |
| arxiv-physics-geo-ph | news | 21600 | - | DATA | 7 | 0 | 51 | rt=article |
| arxiv-physics-med-ph | news | 21600 | - | DATA | 7 | 0 | 138 | rt=article |
| arxiv-physics-optics | news | 21600 | - | DATA | 26 | 0 | 189 | rt=article |
| arxiv-physics-soc-ph | news | 21600 | - | DATA | 10 | 0 | 143 | rt=article |
| arxiv-physics-space-ph | news | 21600 | - | DATA | 5 | 0 | 50 | rt=article |
| arxiv-q-bio-pe | news | 21600 | - | DATA | 6 | 0 | 144 | rt=article |
| arxiv-q-fin-gn | news | 21600 | - | DATA | 3 | 0 | 138 | rt=article |
| arxiv-quant-ph | news | 21600 | - | DATA | 145 | 0 | 532 | rt=article |
| arxiv-stat-ml | news | 21600 | - | DATA | 40 | 0 | 245 | rt=article |

## `native/collectors/sources/bgp_lookup.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BGP_LOOKUP | investigation | 0 | ip=8.8.8.8 | RC_ERROR | 1 | 0 | 200 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/bosai_shelter.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bosai-shelter | safety | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/cam_scs_com_ua.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-scs_com_ua | cyber | 21600 | - | DATA | 238 | 238 | 8451 | rt=camera |

## `native/collectors/sources/cam_worldcams.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-worldcams | investigation | 21600 | - | DATA | 31 | 31 | 325 | rt=camera |

## `native/collectors/sources/chan_5ch.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| chan-5ch | social | 1800 | - | EMPTY | 0 | 0 | 924 |  |

## `native/collectors/sources/code_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GITHUB_CODE_SEARCH | cyber | 0 | keyword=Tokyo | DATA | 15 | 0 | 1958 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/corp_reviews.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EN_LIGHTHOUSE | commercial | 0 | keyword=Tokyo | DATA | 2 | 0 | 1705 | reads ctx->entity; rt=company-review |
| OPENWORK | commercial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1130 | reads ctx->entity |
| TENSHOKU_KAIGI | commercial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 166 | reads ctx->entity |

## `native/collectors/sources/courts_prisons.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| courts-prisons | government | 604800 | - | DATA | 1139 | 1139 | 54509 | rt=courts-prisons |

## `native/collectors/sources/crypto_world4.c`  (7 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ARBISCAN_BALANCE | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| BASE_BALANCE | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| BSCSCAN_BALANCE | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| ETHERSCAN_BALANCE | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| GNOSIS_BALANCE | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| OPTIMISM_BALANCE | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| POLYGONSCAN_BALANCE | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/dam_water_level.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| dam-water-level | infrastructure | 600 | - | EMPTY | 0 | 0 | 837 |  |

## `native/collectors/sources/dns_records.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DNS_RECORDS | cyber | 0 | domain=github.com | DATA | 1 | 0 | 33 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/dns_world.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DOH_RESOLVE | cyber | 0 | domain=github.com | DATA | 32 | 0 | 374 | reads ctx->entity; rt=dns-record |
| RDAP_IP | cyber | 0 | ip=8.8.8.8 | DATA | 1 | 0 | 1611 | reads ctx->entity; rt=ip-whois |

## `native/collectors/sources/domains_world.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CERTSPOTTER | cyber | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 387 | reads ctx->entity |
| CIRCL_PDNS | cyber | 0 | domain=github.com | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| HACKERTARGET | cyber | 0 | keyword=Tokyo | DATA | 50 | 0 | 614 | reads ctx->entity; rt=hackertarget-host |
| URLSCAN_SEARCH | cyber | 0 | domain=github.com | DATA | 25 | 0 | 968 | reads ctx->entity; rt=urlscan-result |

## `native/collectors/sources/drone_nofly.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| drone-nofly | safety | 86400 | - | EMPTY | 0 | 0 | 1200 |  |

## `native/collectors/sources/estat_census.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| estat-census | statistics | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/exif_extractor.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EXIF_EXTRACTOR | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/flickr_geo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| flickr-geo | social | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/gas_outages.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gas-outages | infrastructure | 1800 | - | DATA | 4 | 0 | 10489 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/geo_property.c`  (6 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CADASTRAL_PARCELS | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1412 | reads ctx->entity |
| CHIKAMAP | economy | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1599 | reads ctx->entity |
| GSI_HISTORICAL | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 872 | reads ctx->entity |
| REALPROPERTY_REGISTRY | economy | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| ROSENKA | economy | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1112 | reads ctx->entity |
| ZENRIN_JUTAKU | geospatial | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/geothermal_projects.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| geothermal-projects | industry | 2592000 | - | DATA | 8 | 8 | 3071 | rt=geothermal-projects |

## `native/collectors/sources/global_icij.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ICIJ_OFFSHORE | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 52 | reads ctx->entity |

## `native/collectors/sources/global_wikidata.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| WIKIDATA | investigation | 0 | keyword=Tokyo | DATA | 10 | 0 | 308 | reads ctx->entity; rt=wikidata-entity |

## `native/collectors/sources/grants_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| NIH_REPORTER | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 1094 | reads ctx->entity; rt=nih-reporter-project |
| NSF_AWARDS | government | 0 | keyword=Tokyo | DATA | 25 | 0 | 2152 | reads ctx->entity; rt=nsf-award |
| UKRI_GRANTS | government | 0 | keyword=Tokyo | DATA | 25 | 0 | 331 | reads ctx->entity; rt=ukri-gtr-project |

## `native/collectors/sources/gsi_geocode.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gsi-geocode | geospatial | 86400 | - | DATA | 1 | 1 | 876 | rt=gsi-geocode |

## `native/collectors/sources/highway_traffic.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| highway-traffic | transport | 600 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/infra_sensing.c`  (5 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DRONE_REGISTRY | safety | 0 | company=Toyota | EMPTY | 0 | 0 | 3168 | reads ctx->entity |
| HAZMAT_FACILITIES | infrastructure | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1282 | reads ctx->entity |
| HUNTER_IO | cyber | 0 | email=test@example.com | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| RADIO_STATIONS | telecom | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 940 | reads ctx->entity |
| ZOOMEYE | cyber | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/instagram_geo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| instagram-geo | social | 600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/intel_gov_disaster.c`  (18 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cdc-newsroom | health | 3600 | - | EMPTY | 0 | 0 | 160 |  |
| ecdc-threats | health | 3600 | - | DATA | 10 | 0 | 133 | NO GEO despite map-ish category; rt=article |
| europol-news | government | 3600 | - | DATA | 10 | 0 | 175 | rt=article |
| fbi-press | government | 3600 | - | DATA | 300 | 0 | 880 | rt=article |
| gdacs | environment | 900 | - | DATA | 348 | 0 | 1404 | NO GEO despite map-ish category; rt=article |
| gvp-volcano-activity | environment | 21600 | - | DATA | 22 | 0 | 381 | NO GEO despite map-ish category; rt=article |
| nasa-firms-blog | environment | 3600 | - | DATA | 10 | 0 | 152 | NO GEO despite map-ish category; rt=article |
| nato-news | government | 3600 | - | RC_ERROR | 0 | 0 | 148 |  |
| nws-alerts-us | environment | 900 | - | RC_ERROR | 0 | 0 | 804 |  |
| ofac-recent-actions | government | 3600 | - | RC_ERROR | 0 | 0 | 240 |  |
| reliefweb | environment | 3600 | - | RC_ERROR | 0 | 0 | 317 |  |
| uk-fcdo-travel | government | 3600 | - | DATA | 20 | 0 | 183 | rt=article |
| un-news | government | 3600 | - | DATA | 30 | 0 | 79 | rt=article |
| us-state-travel | government | 3600 | - | DATA | 219 | 0 | 1077 | rt=article |
| usgs-quake-m25-day | environment | 600 | - | DATA | 31 | 0 | 96 | NO GEO despite map-ish category; rt=article |
| usgs-quake-m45-week | environment | 1800 | - | DATA | 134 | 0 | 627 | NO GEO despite map-ish category; rt=article |
| usgs-quake-significant | environment | 900 | - | DATA | 3 | 0 | 348 | NO GEO despite map-ish category; rt=article |
| who-outbreak-news | health | 3600 | - | RC_ERROR | 0 | 0 | 72 |  |

## `native/collectors/sources/ipa_alerts.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ipa-alerts | investigation | 3600 | - | DATA | 29 | 0 | 3254 | rt=article |

## `native/collectors/sources/jcg_msi.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jcg-msi | transport | 1800 | - | DATA | 1 | 0 | 8363 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/jma_intensity.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-intensity | environment | 60 | - | DATA | 93 | 93 | 157 | rt=jma-intensity |

## `native/collectors/sources/jma_volcano.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-volcano | environment | 3600 | - | DATA | 37 | 0 | 880 | NO GEO despite map-ish category; rt=article |

## `native/collectors/sources/jpcert_alerts_rss.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jpcert-alerts-rss | cyber | 3600 | - | DATA | 26 | 0 | 1577 | rt=article |

## `native/collectors/sources/jsdf_bases.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jsdf-bases | defense | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/koban_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| koban-map | safety | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/luup_private.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| luup-private | transport | 600 | - | DATA | 1 | 0 | 69 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/malware_world.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| HYBRIDANALYSIS_HASH | cyber | 0 | hash=44d88612fea8a8f36de82e1278abb02f | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| VIRUSTOTAL_LOOKUP | cyber | 0 | hash=44d88612fea8a8f36de82e1278abb02f | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/maritime_world2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DATALASTIC_VESSEL | infrastructure | 0 | vessel=9074729 | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/misskey_timeline.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| misskey-timeline | social | 600 | - | DATA | 30 | 0 | 1667 | rt=misskey-timeline |

## `native/collectors/sources/mlit_p02_airports.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-p02-airports | transport | 2592000 | - | EMPTY | 0 | 0 | 2370 |  |

## `native/collectors/sources/museums.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| museums | tourism | 604800 | - | DATA | 3812 | 3812 | 43451 | rt=general,art,photography |

## `native/collectors/sources/netlas_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| netlas-jp | cyber | 7200 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/niconico_ranking.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| niconico-ranking | social | 1800 | - | EMPTY | 0 | 0 | 0 |  |

## `native/collectors/sources/nowphas_wave.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nowphas-wave | environment | 1800 | - | EMPTY | 0 | 0 | 1014 |  |

## `native/collectors/sources/nuclear_facilities.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nuclear-facilities | infrastructure | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/opendata_ckan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| OPENDATA_CKAN | government | 0 | keyword=Tokyo | DATA | 4 | 0 | 65470 | reads ctx->entity; rt=ckan-dataset |

## `native/collectors/sources/osint_extras3.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SECURITY_SE_SEARCH | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 262 | reads ctx->entity |
| SERVERFAULT_SEARCH | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 307 | reads ctx->entity; rt=stackexchange-question |
| SUPERUSER_SEARCH | cyber | 0 | keyword=Tokyo | DATA | 18 | 0 | 347 | reads ctx->entity; rt=stackexchange-question |

## `native/collectors/sources/osm_transport_subways.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| osm-transport-subways | transport | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/password_checker.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| PASSWORD_CHECKER | cyber | 0 | keyword=Tokyo | DATA | 1 | 0 | 81 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/pharmacy_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| pharmacy-map | health | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/port_infra.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| port-infra | investigation | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/racetracks.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| racetracks | tourism | 604800 | - | DATA | 240 | 240 | 23025 | rt=racetracks |

## `native/collectors/sources/reg_be_kbo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BE_KBO | government | 0 | company=Toyota | EMPTY | 0 | 0 | 215 | reads ctx->entity |

## `native/collectors/sources/reg_fr_recherche.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| FR_ENTREPRISES | government | 0 | keyword=Tokyo | DATA | 15 | 0 | 218 | reads ctx->entity; rt=fr-entreprise |

## `native/collectors/sources/reg_no_brreg.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| NO_BRREG | government | 0 | company=Toyota | DATA | 15 | 0 | 373 | reads ctx->entity; rt=brreg-company |

## `native/collectors/sources/reg_uk_companies.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| UK_COMPANIES | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/regional_outlets_extra.c`  (24 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| allafrica | news | 3600 | - | DATA | 30 | 0 | 406 | rt=article |
| amwaj-media | news | 3600 | - | RC_ERROR | 0 | 0 | 871 |  |
| balkan-insight-2 | news | 3600 | - | RC_ERROR | 0 | 0 | 301 |  |
| benar-news | news | 3600 | - | RC_ERROR | 0 | 0 | 648 |  |
| bne-intellinews | news | 3600 | - | DATA | 15 | 0 | 138 | rt=article |
| caribbean-news | news | 3600 | - | EMPTY | 0 | 0 | 750 |  |
| dialogo-americas | news | 3600 | - | DATA | 10 | 0 | 808 | rt=article |
| emerging-europe | news | 3600 | - | DATA | 10 | 0 | 346 | rt=article |
| frontier-myanmar | news | 3600 | - | RC_ERROR | 0 | 0 | 91 |  |
| hong-kong-fp | news | 3600 | - | DATA | 30 | 0 | 106 | rt=article |
| insight-crime-2 | news | 3600 | - | EMPTY | 0 | 0 | 87 |  |
| islands-business | news | 3600 | - | DATA | 12 | 0 | 2235 | rt=article |
| iss-africa | news | 3600 | - | EMPTY | 0 | 0 | 97 |  |
| kyiv-indep-3 | news | 3600 | - | RC_ERROR | 0 | 0 | 115 |  |
| meduza-2 | news | 3600 | - | DATA | 30 | 0 | 145 | rt=article |
| novaya-europe | news | 3600 | - | DB_ERROR | 100 | 0 | 534 | db:OperationalError: Could not decode to UT; rt=article |
| oc-media | news | 3600 | - | DATA | 16 | 0 | 289 | rt=article |
| rnz-pacific | news | 3600 | - | DATA | 17 | 0 | 352 | rt=article |
| scroll-in | news | 3600 | - | RC_ERROR | 0 | 0 | 289 |  |
| semafor-africa-2 | news | 3600 | - | RC_ERROR | 0 | 0 | 240 |  |
| the-continent | news | 3600 | - | RC_ERROR | 0 | 0 | 59 |  |
| the-new-arab | news | 3600 | - | RC_ERROR | 0 | 0 | 63 |  |
| the-print-india | news | 3600 | - | RC_ERROR | 0 | 0 | 638 |  |
| the-wire-india | news | 3600 | - | EMPTY | 0 | 0 | 532 |  |

## `native/collectors/sources/resas_tourism.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| resas-tourism | statistics | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/sake_breweries.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| sake-breweries | food | 2592000 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/sec_edgar.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SEC_EDGAR_SEARCH | government | 0 | company=Toyota | DATA | 44 | 0 | 1723 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/shodan_cameras_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| shodan-cameras-jp | cyber | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/social_world2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| YOUTUBE_SEARCH | social | 0 | username=torvalds | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/steel_mills.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| steel-mills | industry | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/tabelog_restaurants.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tabelog-restaurants | marketplace | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/tenki_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tenki-jp | environment | 1800 | - | DATA | 1 | 0 | 1696 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/tiktok_jp_discover.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tiktok-jp-discover | social | 1800 | - | DATA | 1 | 0 | 388 | rt=(null) |

## `native/collectors/sources/trickest_cve.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| trickest-cve | cyber | 21600 | - | NO_TITLE | 16 | 16 | 11461 | rt=trickest-cve |

## `native/collectors/sources/unified_flights.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unified-flights | transport | 60 | - | NO_TITLE | 93 | 93 | 1559 | rt=unified-flights |

## `native/collectors/sources/usfj_bases.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| usfj-bases | defense | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/vehicles_world.c`  (5 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| FAA_REGISTRY | transport | 0 | company=Toyota | DATA | 27 | 0 | 561 | reads ctx->entity; NO GEO despite map-ish category; rt=aircraft-registration |
| NHTSA_VIN | transport | 0 | keyword=Tokyo | DATA | 1 | 0 | 358 | reads ctx->entity; NO GEO despite map-ish category; rt=nhtsa-vin |
| PARIS_MOU_PSC | transport | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 2320 | reads ctx->entity |
| TOKYO_MOU_PSC | transport | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 2912 | reads ctx->entity |
| USCG_PSIX | transport | 0 | vessel=9074729 | EMPTY | 0 | 0 | 5328 | reads ctx->entity |

## `native/collectors/sources/virustotal_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| virustotal-jp | cyber | 21600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/vuln_world.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CISA_KEV_GLOBAL | cyber | 0 | cve=CVE-2021-44228 | DATA | 2 | 0 | 136 | reads ctx->entity; rt=cisa-kev |
| EPSS_SCORES | cyber | 0 | cve=CVE-2021-44228 | DATA | 1 | 0 | 65 | reads ctx->entity; rt=epss-score |
| EXPLOITDB | cyber | 0 | cve=CVE-2021-44228 | EMPTY | 0 | 0 | 2265 | reads ctx->entity |
| NVD_CVE | cyber | 0 | cve=CVE-2021-44228 | DATA | 1 | 0 | 535 | reads ctx->entity; rt=nvd-cve |

## `native/collectors/sources/wayback_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wayback-jp | cyber | 86400 | - | DATA | 194 | 0 | 31911 | rt=wayback-jp |

## `native/collectors/sources/weather_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AVIATION_METAR | environment | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 184 | reads ctx->entity |
| NWS_US | environment | 0 | keyword=Tokyo | DATA | 14 | 14 | 768 | reads ctx->entity; rt=weather-forecast-period |
| OPENMETEO_GLOBAL | environment | 0 | keyword=Tokyo | DATA | 7 | 7 | 261 | reads ctx->entity; rt=weather-forecast-daily |

## `native/collectors/sources/wifi_hotspots_jcfw.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wifi-hotspots-jcfw | infrastructure | 86400 | - | EMPTY | 0 | 0 | 3159 |  |

## `native/collectors/sources/windy_japan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| windy-japan | environment | 1800 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/world_reg_europe.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EUROPE_REGISTRY | government | 0 | company=Toyota | WAF_BLOCKED | 0 | 0 | 12486 | reads ctx->entity |

## `native/collectors/sources/yahoo_map_api.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| yahoo-map-api | commercial | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |
