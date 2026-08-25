# Audit slice 8 — 206 sources across 82 files

`verdict` is from the bulk sweep and is ADVISORY: a blank means the sweep had not reached it, and an EMPTY may just mean the sweep guessed the wrong pivot entity. Re-run anything you report on.

## `native/collectors/sources/academic_world.c`  (9 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ARXIV | government | 0 | doi=10.1038/nature12373 | DATA | 1 | 0 | 364 | reads ctx->entity; rt=arxiv-paper |
| CROSSREF | government | 0 | doi=10.1038/nature12373 | DATA | 15 | 0 | 463 | reads ctx->entity; rt=crossref-work |
| DOAJ_ARTICLES | government | 0 | keyword=Tokyo | DATA | 25 | 0 | 194 | reads ctx->entity; rt=doaj-article |
| OPENCITATIONS | government | 0 | doi=10.1038/nature12373 | DATA | 48 | 0 | 1002 | reads ctx->entity; rt=opencitations-citation |
| OPENLIBRARY | government | 0 | keyword=Tokyo | DATA | 15 | 0 | 3100 | reads ctx->entity; rt=openlibrary-work |
| ORCID_SEARCH | government | 0 | doi=10.1038/nature12373 | DATA | 25 | 0 | 368 | reads ctx->entity; rt=orcid-researcher |
| PUBMED | government | 0 | doi=10.1038/nature12373 | WAF_BLOCKED | 0 | 0 | 1316 | reads ctx->entity |
| ROR_ORGS | government | 0 | company=Toyota | DATA | 20 | 20 | 257 | reads ctx->entity; rt=ror-institution |
| SEMANTIC_SCHOLAR | government | 0 | doi=10.1038/nature12373 | WAF_BLOCKED | 0 | 0 | 762 | reads ctx->entity |

## `native/collectors/sources/airport_infra.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| airport-infra | investigation | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/aviation_world2.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ADSB_LOL | infrastructure | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1008 | reads ctx->entity |
| AIRPLANES_LIVE | infrastructure | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 131 | reads ctx->entity |
| OPENSKY_STATES | infrastructure | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| PLANESPOTTERS | infrastructure | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 171 | reads ctx->entity |

## `native/collectors/sources/bear_encounters.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bear-encounters | wildlife | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/boj_stats.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| boj-stats | government | 86400 | - | DATA | 1 | 0 | 8260 | rt=(null) |

## `native/collectors/sources/cam_insecam_scrape.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-insecam-scrape | cyber | 21600 | - | DATA | 310 | 310 | 53839 | rt=camera |

## `native/collectors/sources/cam_worldcam_eu.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-worldcam_eu | investigation | 21600 | - | DATA | 156 | 156 | 57049 | rt=camera |

## `native/collectors/sources/central_banks_world.c`  (16 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| banxico | economy | 7200 | - | RC_ERROR | 0 | 0 | 1403 |  |
| bcb-brazil | economy | 7200 | - | EMPTY | 0 | 0 | 869 |  |
| bis-press | economy | 7200 | - | EMPTY | 0 | 0 | 170 |  |
| boc-press | economy | 7200 | - | DATA | 10 | 0 | 104 | rt=article |
| boe-news | economy | 7200 | - | DATA | 50 | 0 | 181 | rt=article |
| boj-news | economy | 7200 | - | DATA | 46 | 0 | 437 | rt=article |
| cbr-russia | economy | 7200 | - | DATA | 100 | 0 | 342 | rt=article |
| ecb-blog | economy | 7200 | - | DATA | 15 | 0 | 148 | rt=article |
| ecb-press | economy | 7200 | - | DATA | 15 | 0 | 169 | rt=article |
| fed-press | economy | 7200 | - | DATA | 20 | 0 | 211 | rt=article |
| norgesbank | economy | 7200 | - | RC_ERROR | 0 | 0 | 210 |  |
| rba-media | economy | 7200 | - | RC_ERROR | 0 | 0 | 371 |  |
| rbi-press | economy | 7200 | - | EMPTY | 0 | 0 | 862 |  |
| readme-imf | economy | 7200 | - | RC_ERROR | 0 | 0 | 68 |  |
| riksbank | economy | 7200 | - | RC_ERROR | 0 | 0 | 998 |  |
| snb-press | economy | 7200 | - | DATA | 19 | 0 | 300 | rt=article |

## `native/collectors/sources/certstream_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| certstream-jp | cyber | 60 | - | EMPTY | 0 | 0 | 0 |  |

## `native/collectors/sources/coast_guard_stations.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| coast-guard-stations | defense | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/corp_financials.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BUFFETT_CODE | economy | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| SHIKIHO | economy | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| UFOCATCH | government | 0 | company=Toyota | EMPTY | 0 | 0 | 1680 | reads ctx->entity |

## `native/collectors/sources/court_records.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| COURT_RECORDS | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 847 | reads ctx->entity |

## `native/collectors/sources/crypto_world2.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BLOCKCHAIN_INFO | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | DATA | 1 | 0 | 86 | reads ctx->entity; rt=crypto-address |
| BLOCKCYPHER_ADDR | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | DATA | 1 | 0 | 179 | reads ctx->entity; rt=crypto-address |
| COINGECKO_SEARCH | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | EMPTY | 0 | 0 | 267 | reads ctx->entity |
| DEFILLAMA_TVL | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | EMPTY | 0 | 0 | 365 | reads ctx->entity |

## `native/collectors/sources/crypto_world3.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ETHPLORER_ADDR | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | EMPTY | 0 | 0 | 220 | reads ctx->entity |
| MEMPOOL_ADDR | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | DATA | 1 | 0 | 71 | reads ctx->entity; rt=crypto-address |

## `native/collectors/sources/cycling_ports.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cycling-ports | transport | 3600 | - | DATA | 1879 | 1879 | 4851 | rt=cycling-ports |

## `native/collectors/sources/discord_jp_servers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| discord-jp-servers | cyber | 86400 | - | RC_ERROR | 0 | 0 | 189 |  |

## `native/collectors/sources/domain_history.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DOMAIN_HISTORY | cyber | 0 | domain=github.com | DATA | 1 | 0 | 79 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/embassies.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| embassies | government | 604800 | - | DATA | 164 | 164 | 4148 | rt=embassies |

## `native/collectors/sources/ev_charging.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ev-charging | infrastructure | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/fish_markets.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| fish-markets | food | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/gas_network.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gas-network | infrastructure | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/geodata_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| NOMINATIM_GLOBAL | geospatial | 0 | keyword=Tokyo | DATA | 1 | 1 | 284 | reads ctx->entity; rt=nominatim-place |
| OVERPASS_GLOBAL | geospatial | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 31945 | reads ctx->entity |
| WIKIMAPIA_GLOBAL | geospatial | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/geopremium_world.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| OPENCAGE_GEOCODE | geospatial | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| WHAT3WORDS_CONVERT | geospatial | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/geospatial_jp_ckan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| geospatial-jp-ckan | geospatial | 86400 | - | DATA | 50 | 0 | 6183 | NO GEO despite map-ish category; rt=geospatial-jp-ckan |

## `native/collectors/sources/global_geonames.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GEONAMES | geospatial | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/global_whatsmyname.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| WHATSMYNAME | investigation | 0 | username=torvalds | DATA | 4 | 0 | 24687 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/gov_agencies_world.c`  (44 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| au-dfat | government | 3600 | - | RC_ERROR | 0 | 0 | 868 |  |
| ca-gov-news | government | 3600 | - | EMPTY | 0 | 0 | 497 |  |
| cisa-blog | government | 3600 | - | RC_ERROR | 0 | 0 | 89 |  |
| cn-mofa | government | 3600 | - | EMPTY | 0 | 0 | 1553 |  |
| ec-presscorner | government | 3600 | - | DATA | 10 | 0 | 534 | rt=article |
| eeas-news | government | 3600 | - | RC_ERROR | 0 | 0 | 141 |  |
| ema-news | government | 3600 | - | RC_ERROR | 0 | 0 | 118 |  |
| enisa-news | government | 3600 | - | RC_ERROR | 0 | 0 | 63 |  |
| epa-news | government | 3600 | - | RC_ERROR | 0 | 0 | 55 |  |
| europarl-news | government | 3600 | - | EMPTY | 0 | 0 | 80 |  |
| faa-news | government | 3600 | - | RC_ERROR | 0 | 0 | 422 |  |
| fema-news | government | 3600 | - | RC_ERROR | 0 | 0 | 56 |  |
| frontex-news | government | 3600 | - | RC_ERROR | 0 | 0 | 93 |  |
| iaea-topnews | government | 3600 | - | RC_ERROR | 0 | 0 | 129 |  |
| imf-news | government | 3600 | - | RC_ERROR | 0 | 0 | 63 |  |
| in-pib | government | 3600 | - | DATA | 20 | 0 | 1296 | rt=article |
| interpol-news | government | 3600 | - | RC_ERROR | 0 | 0 | 1037 |  |
| iom-news | government | 3600 | - | RC_ERROR | 0 | 0 | 82 |  |
| itu-news | government | 3600 | - | RC_ERROR | 0 | 0 | 233 |  |
| nato-opinions | government | 3600 | - | RC_ERROR | 0 | 0 | 130 |  |
| noaa-swpc | government | 3600 | - | RC_ERROR | 0 | 0 | 1079 |  |
| ntsb-news | government | 3600 | - | EMPTY | 0 | 0 | 326 |  |
| opcw-news | government | 3600 | - | DATA | 10 | 0 | 299 | rt=article |
| osce-news | government | 3600 | - | RC_ERROR | 0 | 0 | 149 |  |
| ru-mid | government | 3600 | - | EMPTY | 0 | 0 | 288 |  |
| uk-fcdo-news | government | 3600 | - | DATA | 20 | 0 | 262 | rt=article |
| uk-gov-news | government | 3600 | - | DATA | 20 | 0 | 208 | rt=article |
| uk-mod-news | government | 3600 | - | DATA | 20 | 0 | 166 | rt=article |
| un-news-2 | government | 3600 | - | DATA | 30 | 0 | 110 | rt=article |
| unhcr-news | government | 3600 | - | RC_ERROR | 0 | 0 | 82 |  |
| unodc-news | government | 3600 | - | RC_ERROR | 0 | 0 | 222 |  |
| us-dhs-news | government | 3600 | - | RC_ERROR | 0 | 0 | 86 |  |
| us-dod-news | government | 3600 | - | DATA | 25 | 0 | 566 | rt=article |
| us-doj-news | government | 3600 | - | RC_ERROR | 0 | 0 | 201 |  |
| us-ftc-press | government | 3600 | - | DATA | 10 | 0 | 416 | rt=article |
| us-gao-reports | government | 3600 | - | DATA | 25 | 0 | 370 | rt=article |
| us-nhc-atlantic | government | 3600 | - | DATA | 2 | 0 | 389 | rt=article |
| us-nhc-epac | government | 3600 | - | DATA | 7 | 0 | 487 | rt=article |
| us-sec-press | government | 3600 | - | DATA | 25 | 0 | 301 | rt=article |
| us-state-press | government | 3600 | - | EMPTY | 0 | 0 | 727 |  |
| us-treasury-press | government | 3600 | - | RC_ERROR | 0 | 0 | 271 |  |
| wfp-news | government | 3600 | - | RC_ERROR | 0 | 0 | 644 |  |
| who-news | government | 3600 | - | DATA | 25 | 0 | 186 | rt=article |
| worldbank-news | government | 3600 | - | RC_ERROR | 0 | 0 | 640 |  |

## `native/collectors/sources/gov_money.c`  (6 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ASSET_DISCLOSURE | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1162 | reads ctx->entity |
| CUSTOMS_TRADE | economy | 0 | keyword=Tokyo | DB_ERROR | 30 | 0 | 1090 | reads ctx->entity; db:OperationalError: Could not decode to UT; rt=trade-statistics |
| GEPS_PROCUREMENT | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1679 | reads ctx->entity |
| JGRANTS | government | 0 | keyword=Tokyo | DATA | 388 | 0 | 2951 | reads ctx->entity; rt=subsidy-program |
| LOCAL_TENDERS | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 2017 | reads ctx->entity |
| POLITICAL_FUNDS | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 998 | reads ctx->entity |

## `native/collectors/sources/gsi_active_fault.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gsi-active-fault | geospatial | 604800 | - | RC_ERROR | 0 | 0 | 809 |  |

## `native/collectors/sources/hi_net.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| hi-net | environment | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/influenza_surveillance.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| influenza-surveillance | health | 604800 | - | DATA | 1 | 0 | 8670 | NO GEO despite map-ish category; rt=influenza-surveillance |

## `native/collectors/sources/ip_reputation.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| IP_REPUTATION | cyber | 0 | ip=8.8.8.8 | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/jcab_notams.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jcab-notams | transport | 1800 | - | DATA | 1 | 0 | 111 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/jma_ice.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-ice | ocean | 86400 | - | DATA | 1 | 0 | 819 | rt=article |

## `native/collectors/sources/jma_uv.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-uv | environment | 3600 | - | DATA | 1 | 0 | 49 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/jp_world2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| JP_POSTAL | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/jr_west_delay.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jr-west-delay | transport | 60 | - | DATA | 1 | 0 | 1230 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/karaoke_chains.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| karaoke-chains | culture | 2592000 | - | DATA | 1458 | 1458 | 26014 | rt=karaoke-chains |

## `native/collectors/sources/linkedin_jp_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| linkedin-jp-search | social | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/maritime_ais.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| maritime-ais | transport | 300 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/michi_no_eki.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| michi-no-eki | transport | 604800 | - | DATA | 1 | 0 | 1284 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/misc_world.c`  (5 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CODEBERG_SEARCH | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 7876 | reads ctx->entity; rt=codeberg-repo |
| DBLP_SEARCH | investigation | 0 | keyword=Tokyo | DATA | 20 | 0 | 1621 | reads ctx->entity; rt=dblp-publication |
| FRANKFURTER_FX | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 314 | reads ctx->entity |
| GENDERIZE | investigation | 0 | keyword=Tokyo | DATA | 1 | 0 | 520 | reads ctx->entity; rt=name-gender |
| NATIONALIZE | investigation | 0 | country=Japan | DATA | 1 | 0 | 376 | reads ctx->entity; rt=name-nationality |

## `native/collectors/sources/mlit_n07_bus_routes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-n07-bus-routes | transport | 2592000 | - | EMPTY | 0 | 0 | 1514 |  |

## `native/collectors/sources/msil_umishiru.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| msil-umishiru | transport | 900 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/nerv_feed.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nerv-feed | environment | 60 | - | RC_ERROR | 0 | 0 | 2358 |  |

## `native/collectors/sources/nhk_world_rss.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nhk-world-rss | government | 1800 | - | RC_ERROR | 0 | 0 | 405 |  |

## `native/collectors/sources/nonprofit_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CANADA_CHARITIES | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| PROPUBLICA_NONPROFIT | investigation | 0 | keyword=Tokyo | DATA | 25 | 0 | 325 | reads ctx->entity; rt=nonprofit-org |
| UK_CHARITIES | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/note_com_trending.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| note-com-trending | social | 1800 | - | RC_ERROR | 0 | 0 | 543 |  |

## `native/collectors/sources/ntt_fiber.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ntt-fiber | infrastructure | 604800 | - | DATA | 2 | 0 | 9497 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/ooni_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ooni-jp | cyber | 3600 | - | NO_TITLE | 200 | 0 | 1133 | rt=ooni-jp |

## `native/collectors/sources/osm_transport_station_boundaries.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| osm-transport-station-boundaries | investigation | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/parking_facilities.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| parking-facilities | geospatial | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/people_finder.c`  (7 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CINII | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 989 | reads ctx->entity; rt=research-paper |
| COMPANY_SEARCH | investigation | 0 | company=Toyota | EMPTY | 0 | 0 | 135 | reads ctx->entity |
| KAKEN | government | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 1109 | reads ctx->entity |
| MANSION_COMMUNITY | social | 0 | username=torvalds | DATA | 25 | 0 | 2032 | reads ctx->entity; rt=resident-forum |
| PERSON_SEARCH | investigation | 0 | person=Shinzo Abe | RC_ERROR | 50 | 0 | 6295 | reads ctx->entity; rt=github-user,research-paper,resident-forum |
| RESEARCHMAP | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 2152 | reads ctx->entity |
| TDB_TSR | commercial | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/petroleum_stockpile.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| petroleum-stockpile | industry | 2592000 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/police_incidents.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| police-incidents | safety | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/quake360_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| quake360-jp | cyber | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/reg_au_abr.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AU_ABR | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/reg_fi_prh.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| FI_PRH | government | 0 | company=Toyota | DATA | 12 | 0 | 1572 | reads ctx->entity; rt=fi-prh-company |

## `native/collectors/sources/reg_latam2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| LATAM2_REGISTRY | government | 0 | company=Toyota | DATA | 1 | 0 | 38364 | reads ctx->entity; rt=latam-company |

## `native/collectors/sources/reg_ua_prozorro.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| UA_PROZORRO | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 2149 | reads ctx->entity; rt=prozorro-tender |

## `native/collectors/sources/resas_population.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| resas-population | statistics | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/saigai_info.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| saigai-info | safety | 300 | - | DATA | 363 | 0 | 885 | NO GEO despite map-ish category; rt=article |

## `native/collectors/sources/satellite_tracking.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| satellite-tracking | satellite | 60 | - | DATA | 1 | 1 | 1403 | rt=satellite-tracking |

## `native/collectors/sources/shipyards.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| shipyards | industry | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/social_username.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SOCIAL_USERNAME | social | 0 | username=torvalds | RC_ERROR | 91 | 0 | 56184 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/space_world.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CELESTRAK_TLE | satellite | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 3621 | reads ctx->entity |
| N2YO | satellite | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| SATNOGS | satellite | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 705 | reads ctx->entity |
| SPACETRACK | satellite | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/steam_jp_users.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| steam-jp-users | social | 86400 | - | DATA | 1 | 0 | 368 | rt=(null) |

## `native/collectors/sources/suumo_rental_density.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| suumo-rental-density | classifieds | 86400 | - | NO_TITLE | 47 | 47 | 71530 | rt=suumo-rental-density |

## `native/collectors/sources/tellus_satellite.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tellus-satellite | satellite | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/thinktanks_world.c`  (24 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| atlantic-council | news | 10800 | - | DATA | 100 | 0 | 236 | rt=article |
| brookings | news | 10800 | - | EMPTY | 0 | 0 | 107 |  |
| carnegie-china | news | 10800 | - | EMPTY | 0 | 0 | 341 |  |
| carnegie-endow | news | 10800 | - | EMPTY | 0 | 0 | 443 |  |
| cepa | news | 10800 | - | DATA | 10 | 0 | 444 | rt=article |
| chatham-house | news | 10800 | - | RC_ERROR | 0 | 0 | 76 |  |
| cnas | news | 10800 | - | RC_ERROR | 0 | 0 | 733 |  |
| csis | news | 10800 | - | RC_ERROR | 0 | 0 | 74 |  |
| csis-china | news | 10800 | - | DATA | 10 | 0 | 1050 | rt=article |
| ecfr | news | 10800 | - | DATA | 25 | 0 | 959 | rt=article |
| fpri | news | 10800 | - | RC_ERROR | 0 | 0 | 59 |  |
| gmfus | news | 10800 | - | DATA | 10 | 0 | 104 | rt=article |
| heritage | news | 10800 | - | RC_ERROR | 0 | 0 | 523 |  |
| hudson | news | 10800 | - | RC_ERROR | 0 | 0 | 558 |  |
| iiss | news | 10800 | - | RC_ERROR | 0 | 0 | 59 |  |
| jamestown | news | 10800 | - | DATA | 10 | 0 | 250 | rt=article |
| lowy-interpreter | news | 10800 | - | DATA | 50 | 0 | 116 | rt=article |
| mei | news | 10800 | - | RC_ERROR | 0 | 0 | 55 |  |
| merics | news | 10800 | - | RC_ERROR | 0 | 0 | 149 |  |
| rand-pubs | news | 10800 | - | DATA | 20 | 0 | 189 | rt=article |
| rusi | news | 10800 | - | RC_ERROR | 0 | 0 | 690 |  |
| sipri | news | 10800 | - | RC_ERROR | 0 | 0 | 1469 |  |
| stimson | news | 10800 | - | RC_ERROR | 0 | 0 | 61 |  |
| wilson-center | news | 10800 | - | RC_ERROR | 0 | 0 | 160 |  |

## `native/collectors/sources/tiktok_geo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tiktok-geo | social | 600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/transport_cluster_runner.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| transport-cluster-runner | investigation | 3600 | - | EMPTY | 0 | 0 | 0 |  |

## `native/collectors/sources/unified_buses.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unified-buses | transport | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/urlscan_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| urlscan-jp | cyber | 1800 | - | DATA | 100 | 0 | 763 | rt=urlscan-jp |

## `native/collectors/sources/us_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| COURTLISTENER | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 716 | reads ctx->entity; rt=us-court-opinion |
| FEDERAL_REGISTER | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 463 | reads ctx->entity; rt=federal-register-doc |
| US_NPI_PROVIDER | government | 0 | keyword=Tokyo | DATA | 4 | 0 | 421 | reads ctx->entity; rt=us-healthcare-provider |

## `native/collectors/sources/virustotal.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| MALWARE_ANALYSIS | cyber | 0 | hash=44d88612fea8a8f36de82e1278abb02f | EMPTY | 0 | 0 | 166 | reads ctx->entity |

## `native/collectors/sources/water_towers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| water-towers | infrastructure | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/wifi_hotspots_freespot.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wifi-hotspots-freespot | infrastructure | 86400 | - | DATA | 1 | 0 | 9421 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/wind_turbines.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wind-turbines | industry | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/wireless_world2.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| APRS_FI_LOC | telecom | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| OPENCELLID_CELL | telecom | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/world_reg_cis.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CIS_REGISTRY | government | 0 | company=Toyota | DATA | 23 | 0 | 35887 | reads ctx->entity; rt=cis-company,cis-enforcement,cis-person |

## `native/collectors/sources/yahoo_crowd_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| yahoo-crowd-map | social | 600 | - | DATA | 1 | 0 | 1287 | rt=(null) |
