# Audit slice 8 — 206 sources across 82 files

`verdict` is from the bulk sweep and is ADVISORY: a blank means the sweep had not reached it, and an EMPTY may just mean the sweep guessed the wrong pivot entity. Re-run anything you report on.

## `native/collectors/sources/academic_world.c`  (9 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ARXIV | government | 0 | doi=10.1038/nature12373 | DATA | 1 | 0 | 1422 | reads ctx->entity; rt=arxiv-paper |
| CROSSREF | government | 0 | doi=10.1038/nature12373 | DATA | 15 | 0 | 487 | reads ctx->entity; rt=crossref-work |
| DOAJ_ARTICLES | government | 0 | keyword=Tokyo | DATA | 25 | 0 | 238 | reads ctx->entity; rt=doaj-article |
| OPENCITATIONS | government | 0 | doi=10.1038/nature12373 | DATA | 48 | 0 | 3757 | reads ctx->entity; rt=opencitations-citation |
| OPENLIBRARY | government | 0 | keyword=Tokyo | DATA | 15 | 0 | 925 | reads ctx->entity; rt=openlibrary-work |
| ORCID_SEARCH | government | 0 | doi=10.1038/nature12373 | DATA | 25 | 0 | 654 | reads ctx->entity; rt=orcid-researcher |
| PUBMED | government | 0 | doi=10.1038/nature12373 | DATA | 1 | 0 | 842 | reads ctx->entity; rt=pubmed-article |
| ROR_ORGS | government | 0 | company=Toyota | DATA | 20 | 20 | 253 | reads ctx->entity; rt=ror-institution |
| SEMANTIC_SCHOLAR | government | 0 | doi=10.1038/nature12373 | WAF_BLOCKED | 0 | 0 | 643 | reads ctx->entity |

## `native/collectors/sources/airport_infra.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| airport-infra | investigation | 86400 | - | DATA | 700 | 700 | 111795 | rt=airport-infra |

## `native/collectors/sources/aviation_world2.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ADSB_LOL | infrastructure | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 240 | reads ctx->entity |
| AIRPLANES_LIVE | infrastructure | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 140 | reads ctx->entity |
| OPENSKY_STATES | infrastructure | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| PLANESPOTTERS | infrastructure | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 235 | reads ctx->entity |

## `native/collectors/sources/bear_encounters.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bear-encounters | wildlife | 86400 | - | RC_ERROR | 0 | 0 | 0 |  |

## `native/collectors/sources/boj_stats.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| boj-stats | government | 86400 | - | DATA | 1 | 0 | 8003 | rt=(null) |

## `native/collectors/sources/cam_insecam_scrape.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-insecam-scrape | cyber | 21600 | - | DATA | 315 | 315 | 24607 | rt=camera |

## `native/collectors/sources/cam_worldcam_eu.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-worldcam_eu | investigation | 21600 | - | DATA | 156 | 156 | 40184 | rt=camera |

## `native/collectors/sources/central_banks_world.c`  (16 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| banxico | economy | 7200 | - | RC_ERROR | 0 | 0 | 985 |  |
| bcb-brazil | economy | 7200 | - | EMPTY | 0 | 0 | 84 |  |
| bis-press | economy | 7200 | - | EMPTY | 0 | 0 | 242 |  |
| boc-press | economy | 7200 | - | DATA | 10 | 0 | 128 | rt=article |
| boe-news | economy | 7200 | - | DATA | 50 | 0 | 245 | rt=article |
| boj-news | economy | 7200 | - | DATA | 41 | 0 | 386 | rt=article |
| cbr-russia | economy | 7200 | - | DATA | 100 | 0 | 473 | rt=article |
| ecb-blog | economy | 7200 | - | DATA | 15 | 0 | 188 | rt=article |
| ecb-press | economy | 7200 | - | DATA | 15 | 0 | 209 | rt=article |
| fed-press | economy | 7200 | - | DATA | 20 | 0 | 166 | rt=article |
| norgesbank | economy | 7200 | - | RC_ERROR | 0 | 0 | 153 |  |
| rba-media | economy | 7200 | - | RC_ERROR | 0 | 0 | 365 |  |
| rbi-press | economy | 7200 | - | EMPTY | 0 | 0 | 1140 |  |
| readme-imf | economy | 7200 | - | RC_ERROR | 0 | 0 | 115 |  |
| riksbank | economy | 7200 | - | RC_ERROR | 0 | 0 | 826 |  |
| snb-press | economy | 7200 | - | DATA | 17 | 0 | 361 | rt=article |

## `native/collectors/sources/certstream_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| certstream-jp | cyber | 60 | - | EMPTY | 0 | 0 | 0 |  |

## `native/collectors/sources/coast_guard_stations.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| coast-guard-stations | defense | 604800 | - | RC_ERROR | 0 | 0 | 88868 |  |

## `native/collectors/sources/corp_financials.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BUFFETT_CODE | economy | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| SHIKIHO | economy | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| UFOCATCH | government | 0 | company=Toyota | EMPTY | 0 | 0 | 1548 | reads ctx->entity |

## `native/collectors/sources/court_records.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| COURT_RECORDS | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1170 | reads ctx->entity |

## `native/collectors/sources/crypto_world2.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BLOCKCHAIN_INFO | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | DATA | 1 | 0 | 70 | reads ctx->entity; rt=crypto-address |
| BLOCKCYPHER_ADDR | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | DATA | 1 | 0 | 149 | reads ctx->entity; rt=crypto-address |
| COINGECKO_SEARCH | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | EMPTY | 0 | 0 | 288 | reads ctx->entity |
| DEFILLAMA_TVL | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | EMPTY | 0 | 0 | 288 | reads ctx->entity |

## `native/collectors/sources/crypto_world3.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ETHPLORER_ADDR | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | EMPTY | 0 | 0 | 219 | reads ctx->entity |
| MEMPOOL_ADDR | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | DATA | 1 | 0 | 62 | reads ctx->entity; rt=crypto-address |

## `native/collectors/sources/cycling_ports.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cycling-ports | transport | 3600 | - | DATA | 1879 | 1879 | 6117 | rt=cycling-ports |

## `native/collectors/sources/discord_jp_servers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| discord-jp-servers | cyber | 86400 | - | RC_ERROR | 0 | 0 | 133 |  |

## `native/collectors/sources/domain_history.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DOMAIN_HISTORY | cyber | 0 | domain=github.com | DATA | 1 | 0 | 94 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/embassies.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| embassies | government | 604800 | - | DATA | 162 | 162 | 4460 | rt=embassies |

## `native/collectors/sources/ev_charging.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ev-charging | infrastructure | 86400 | - | DATA | 1016 | 1016 | 32988 | rt=ev-charging |

## `native/collectors/sources/fish_markets.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| fish-markets | food | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/gas_network.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gas-network | infrastructure | 86400 | - | RC_ERROR | 0 | 0 | 72843 |  |

## `native/collectors/sources/geodata_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| NOMINATIM_GLOBAL | geospatial | 0 | keyword=Tokyo | DATA | 1 | 1 | 188 | reads ctx->entity; rt=nominatim-place |
| OVERPASS_GLOBAL | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 2466 | reads ctx->entity |
| WIKIMAPIA_GLOBAL | geospatial | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/geopremium_world.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| OPENCAGE_GEOCODE | geospatial | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| WHAT3WORDS_CONVERT | geospatial | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/geospatial_jp_ckan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| geospatial-jp-ckan | geospatial | 86400 | - | DB_ERROR | 50 | 0 | 7802 | db:OperationalError: Could not decode to UT; NO GEO despite map-ish category; rt=geospatial-jp-ckan |

## `native/collectors/sources/global_geonames.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GEONAMES | geospatial | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/global_whatsmyname.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| WHATSMYNAME | investigation | 0 | username=torvalds | DATA | 4 | 0 | 11962 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/gov_agencies_world.c`  (44 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| au-dfat | government | 3600 | - | RC_ERROR | 0 | 0 | 878 |  |
| ca-gov-news | government | 3600 | - | EMPTY | 0 | 0 | 510 |  |
| cisa-blog | government | 3600 | - | RC_ERROR | 0 | 0 | 80 |  |
| cn-mofa | government | 3600 | - | EMPTY | 0 | 0 | 1531 |  |
| ec-presscorner | government | 3600 | - | DATA | 10 | 0 | 637 | rt=article |
| eeas-news | government | 3600 | - | RC_ERROR | 0 | 0 | 167 |  |
| ema-news | government | 3600 | - | RC_ERROR | 0 | 0 | 114 |  |
| enisa-news | government | 3600 | - | RC_ERROR | 0 | 0 | 121 |  |
| epa-news | government | 3600 | - | RC_ERROR | 0 | 0 | 139 |  |
| europarl-news | government | 3600 | - | EMPTY | 0 | 0 | 83 |  |
| faa-news | government | 3600 | - | RC_ERROR | 0 | 0 | 803 |  |
| fema-news | government | 3600 | - | RC_ERROR | 0 | 0 | 119 |  |
| frontex-news | government | 3600 | - | RC_ERROR | 0 | 0 | 187 |  |
| iaea-topnews | government | 3600 | - | RC_ERROR | 0 | 0 | 116 |  |
| imf-news | government | 3600 | - | RC_ERROR | 0 | 0 | 69 |  |
| in-pib | government | 3600 | - | DATA | 20 | 0 | 1720 | rt=article |
| interpol-news | government | 3600 | - | RC_ERROR | 0 | 0 | 1398 |  |
| iom-news | government | 3600 | - | RC_ERROR | 0 | 0 | 84 |  |
| itu-news | government | 3600 | - | RC_ERROR | 0 | 0 | 217 |  |
| nato-opinions | government | 3600 | - | RC_ERROR | 0 | 0 | 114 |  |
| noaa-swpc | government | 3600 | - | RC_ERROR | 0 | 0 | 1556 |  |
| ntsb-news | government | 3600 | - | EMPTY | 0 | 0 | 337 |  |
| opcw-news | government | 3600 | - | DATA | 10 | 0 | 287 | rt=article |
| osce-news | government | 3600 | - | RC_ERROR | 0 | 0 | 157 |  |
| ru-mid | government | 3600 | - | EMPTY | 0 | 0 | 302 |  |
| uk-fcdo-news | government | 3600 | - | DATA | 20 | 0 | 289 | rt=article |
| uk-gov-news | government | 3600 | - | DATA | 20 | 0 | 316 | rt=article |
| uk-mod-news | government | 3600 | - | DATA | 20 | 0 | 298 | rt=article |
| un-news-2 | government | 3600 | - | DATA | 30 | 0 | 211 | rt=article |
| unhcr-news | government | 3600 | - | RC_ERROR | 0 | 0 | 80 |  |
| unodc-news | government | 3600 | - | RC_ERROR | 0 | 0 | 226 |  |
| us-dhs-news | government | 3600 | - | RC_ERROR | 0 | 0 | 123 |  |
| us-dod-news | government | 3600 | - | DATA | 25 | 0 | 615 | rt=article |
| us-doj-news | government | 3600 | - | RC_ERROR | 0 | 0 | 280 |  |
| us-ftc-press | government | 3600 | - | DATA | 10 | 0 | 261 | rt=article |
| us-gao-reports | government | 3600 | - | DATA | 25 | 0 | 448 | rt=article |
| us-nhc-atlantic | government | 3600 | - | EMPTY | 0 | 0 | 69 |  |
| us-nhc-epac | government | 3600 | - | EMPTY | 0 | 0 | 82 |  |
| us-sec-press | government | 3600 | - | DATA | 25 | 0 | 286 | rt=article |
| us-state-press | government | 3600 | - | EMPTY | 0 | 0 | 736 |  |
| us-treasury-press | government | 3600 | - | RC_ERROR | 0 | 0 | 225 |  |
| wfp-news | government | 3600 | - | RC_ERROR | 0 | 0 | 553 |  |
| who-news | government | 3600 | - | DATA | 25 | 0 | 170 | rt=article |
| worldbank-news | government | 3600 | - | RC_ERROR | 0 | 0 | 584 |  |

## `native/collectors/sources/gov_money.c`  (6 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ASSET_DISCLOSURE | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1246 | reads ctx->entity |
| CUSTOMS_TRADE | economy | 0 | keyword=Tokyo | DB_ERROR | 30 | 0 | 1038 | reads ctx->entity; db:OperationalError: Could not decode to UT; rt=trade-statistics |
| GEPS_PROCUREMENT | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1688 | reads ctx->entity |
| JGRANTS | government | 0 | keyword=Tokyo | DATA | 386 | 0 | 3073 | reads ctx->entity; rt=subsidy-program |
| LOCAL_TENDERS | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1178 | reads ctx->entity |
| POLITICAL_FUNDS | government | 0 | person=Shinzo Abe | EMPTY | 0 | 0 | 845 | reads ctx->entity |

## `native/collectors/sources/gsi_active_fault.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gsi-active-fault | geospatial | 604800 | - | RC_ERROR | 0 | 0 | 969 |  |

## `native/collectors/sources/hi_net.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| hi-net | environment | 86400 | - | RC_ERROR | 0 | 0 | 75574 |  |

## `native/collectors/sources/influenza_surveillance.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| influenza-surveillance | health | 604800 | - | DATA | 1 | 0 | 7957 | NO GEO despite map-ish category; rt=influenza-surveillance |

## `native/collectors/sources/ip_reputation.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| IP_REPUTATION | cyber | 0 | ip=8.8.8.8 | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/jcab_notams.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jcab-notams | transport | 1800 | - | DATA | 1 | 0 | 101 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/jma_ice.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-ice | ocean | 86400 | - | DATA | 1 | 0 | 720 | rt=article |

## `native/collectors/sources/jma_uv.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-uv | environment | 3600 | - | DATA | 1 | 0 | 44 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/jp_world2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| JP_POSTAL | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/jr_west_delay.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jr-west-delay | transport | 60 | - | DATA | 1 | 0 | 2753 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/karaoke_chains.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| karaoke-chains | culture | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/linkedin_jp_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| linkedin-jp-search | social | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/maritime_ais.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| maritime-ais | transport | 300 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/michi_no_eki.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| michi-no-eki | transport | 604800 | - | DATA | 1 | 0 | 1346 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/misc_world.c`  (5 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CODEBERG_SEARCH | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 6584 | reads ctx->entity; rt=codeberg-repo |
| DBLP_SEARCH | investigation | 0 | keyword=Tokyo | DATA | 20 | 0 | 300 | reads ctx->entity; rt=dblp-publication |
| FRANKFURTER_FX | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 144 | reads ctx->entity |
| GENDERIZE | investigation | 0 | keyword=Tokyo | DATA | 1 | 0 | 381 | reads ctx->entity; rt=name-gender |
| NATIONALIZE | investigation | 0 | country=Japan | DATA | 1 | 0 | 508 | reads ctx->entity; rt=name-nationality |

## `native/collectors/sources/mlit_n07_bus_routes.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-n07-bus-routes | transport | 2592000 | - | EMPTY | 0 | 0 | 1013 |  |

## `native/collectors/sources/msil_umishiru.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| msil-umishiru | transport | 900 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/nerv_feed.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nerv-feed | environment | 60 | - | RC_ERROR | 0 | 0 | 1272 |  |

## `native/collectors/sources/nhk_world_rss.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nhk-world-rss | government | 1800 | - | RC_ERROR | 0 | 0 | 456 |  |

## `native/collectors/sources/nonprofit_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CANADA_CHARITIES | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| PROPUBLICA_NONPROFIT | investigation | 0 | keyword=Tokyo | DATA | 25 | 0 | 291 | reads ctx->entity; rt=nonprofit-org |
| UK_CHARITIES | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/note_com_trending.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| note-com-trending | social | 1800 | - | RC_ERROR | 0 | 0 | 797 |  |

## `native/collectors/sources/ntt_fiber.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ntt-fiber | infrastructure | 604800 | - | DATA | 2 | 0 | 8916 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/ooni_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ooni-jp | cyber | 3600 | - | NO_TITLE | 200 | 0 | 481 | rt=ooni-jp |

## `native/collectors/sources/osm_transport_station_boundaries.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| osm-transport-station-boundaries | investigation | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/parking_facilities.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| parking-facilities | geospatial | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/people_finder.c`  (7 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CINII | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 999 | reads ctx->entity; rt=research-paper |
| COMPANY_SEARCH | investigation | 0 | company=Toyota | EMPTY | 0 | 0 | 77 | reads ctx->entity |
| KAKEN | government | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 1014 | reads ctx->entity |
| MANSION_COMMUNITY | social | 0 | username=torvalds | DATA | 25 | 0 | 1132 | reads ctx->entity; rt=resident-forum |
| PERSON_SEARCH | investigation | 0 | person=Shinzo Abe | RC_ERROR | 50 | 0 | 7201 | reads ctx->entity; rt=github-user,research-paper,resident-forum |
| RESEARCHMAP | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1771 | reads ctx->entity |
| TDB_TSR | commercial | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/petroleum_stockpile.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| petroleum-stockpile | industry | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/police_incidents.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| police-incidents | safety | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/quake360_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| quake360-jp | cyber | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/reg_au_abr.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AU_ABR | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/reg_fi_prh.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| FI_PRH | government | 0 | company=Toyota | DATA | 12 | 0 | 1578 | reads ctx->entity; rt=fi-prh-company |

## `native/collectors/sources/reg_latam2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| LATAM2_REGISTRY | government | 0 | company=Toyota | DATA | 1 | 0 | 49516 | reads ctx->entity; rt=latam-company |

## `native/collectors/sources/reg_ua_prozorro.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| UA_PROZORRO | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 320 | reads ctx->entity; rt=prozorro-tender |

## `native/collectors/sources/resas_population.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| resas-population | statistics | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/saigai_info.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| saigai-info | safety | 300 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/satellite_tracking.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| satellite-tracking | satellite | 60 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/shipyards.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| shipyards | industry | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/social_username.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SOCIAL_USERNAME | social | 0 | username=torvalds | RC_ERROR | 90 | 0 | 75301 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/space_world.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CELESTRAK_TLE | satellite | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 4972 | reads ctx->entity |
| N2YO | satellite | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| SATNOGS | satellite | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 425 | reads ctx->entity |
| SPACETRACK | satellite | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/steam_jp_users.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| steam-jp-users | social | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/suumo_rental_density.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| suumo-rental-density | classifieds | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/tellus_satellite.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tellus-satellite | satellite | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/thinktanks_world.c`  (24 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| atlantic-council | news | 10800 | - | (not swept) | - | - | - |  |
| brookings | news | 10800 | - | (not swept) | - | - | - |  |
| carnegie-china | news | 10800 | - | (not swept) | - | - | - |  |
| carnegie-endow | news | 10800 | - | (not swept) | - | - | - |  |
| cepa | news | 10800 | - | (not swept) | - | - | - |  |
| chatham-house | news | 10800 | - | (not swept) | - | - | - |  |
| cnas | news | 10800 | - | (not swept) | - | - | - |  |
| csis | news | 10800 | - | (not swept) | - | - | - |  |
| csis-china | news | 10800 | - | (not swept) | - | - | - |  |
| ecfr | news | 10800 | - | (not swept) | - | - | - |  |
| fpri | news | 10800 | - | (not swept) | - | - | - |  |
| gmfus | news | 10800 | - | (not swept) | - | - | - |  |
| heritage | news | 10800 | - | (not swept) | - | - | - |  |
| hudson | news | 10800 | - | (not swept) | - | - | - |  |
| iiss | news | 10800 | - | (not swept) | - | - | - |  |
| jamestown | news | 10800 | - | (not swept) | - | - | - |  |
| lowy-interpreter | news | 10800 | - | (not swept) | - | - | - |  |
| mei | news | 10800 | - | (not swept) | - | - | - |  |
| merics | news | 10800 | - | (not swept) | - | - | - |  |
| rand-pubs | news | 10800 | - | (not swept) | - | - | - |  |
| rusi | news | 10800 | - | (not swept) | - | - | - |  |
| sipri | news | 10800 | - | (not swept) | - | - | - |  |
| stimson | news | 10800 | - | (not swept) | - | - | - |  |
| wilson-center | news | 10800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/tiktok_geo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tiktok-geo | social | 600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/transport_cluster_runner.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| transport-cluster-runner | investigation | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/unified_buses.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unified-buses | transport | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/urlscan_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| urlscan-jp | cyber | 1800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/us_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| COURTLISTENER | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 1560 | reads ctx->entity; rt=us-court-opinion |
| FEDERAL_REGISTER | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 570 | reads ctx->entity; rt=federal-register-doc |
| US_NPI_PROVIDER | government | 0 | keyword=Tokyo | DATA | 4 | 0 | 331 | reads ctx->entity; rt=us-healthcare-provider |

## `native/collectors/sources/virustotal.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| MALWARE_ANALYSIS | cyber | 0 | hash=44d88612fea8a8f36de82e1278abb02f | EMPTY | 0 | 0 | 207 | reads ctx->entity |

## `native/collectors/sources/water_towers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| water-towers | infrastructure | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/wifi_hotspots_freespot.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wifi-hotspots-freespot | infrastructure | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/wind_turbines.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wind-turbines | industry | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/wireless_world2.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| APRS_FI_LOC | telecom | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| OPENCELLID_CELL | telecom | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/world_reg_cis.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CIS_REGISTRY | government | 0 | company=Toyota | DATA | 23 | 0 | 31802 | reads ctx->entity; rt=cis-company,cis-enforcement,cis-person |

## `native/collectors/sources/yahoo_crowd_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| yahoo-crowd-map | social | 600 | - | (not swept) | - | - | - |  |
