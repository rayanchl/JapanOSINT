# Audit slice 7 — 206 sources across 81 files

`verdict` is from the bulk sweep and is ADVISORY: a blank means the sweep had not reached it, and an EMPTY may just mean the sweep guessed the wrong pivot entity. Re-run anything you report on.

## `native/collectors/sources/agoop_flow.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| agoop-flow | commercial | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/auto_plants.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| auto-plants | industry | 604800 | - | DATA | 4 | 4 | 63957 | rt=auto-plants |

## `native/collectors/sources/bluesky_jetstream_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bluesky-jetstream-jp | social | 600 | - | EMPTY | 0 | 0 | 0 |  |

## `native/collectors/sources/cam_geocam.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-geocam | investigation | 21600 | - | DATA | 3 | 3 | 596 | rt=camera |

## `native/collectors/sources/cam_windy_api.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-windy_api | cyber | 21600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/censys_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CENSYS_SEARCH | cyber | 0 | ip=8.8.8.8 | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/cloudflare_radar_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cloudflare-radar-jp | cyber | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/code_world2.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GITHUB_GISTS | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 238 | reads ctx->entity |
| GITHUB_REPO_SEARCH | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 691 | reads ctx->entity; rt=github-repo |
| GITLAB_PROJECT_SEARCH | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 1545 | reads ctx->entity; rt=gitlab-project |

## `native/collectors/sources/country_geo2.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AT_POSTAL | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| CH_ADDRESS | geospatial | 0 | keyword=Tokyo | DATA | 10 | 10 | 123 | reads ctx->entity; rt=ch-location |
| CH_POSTAL | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| NO_ADDRESS | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 155 | reads ctx->entity |

## `native/collectors/sources/country_world3.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DK_ADDRESS | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 178 | reads ctx->entity |

## `native/collectors/sources/crypto_onchain.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DEFI_TRACKER | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| EXCHANGE_FLOW | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/crypto_world.c`  (7 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BLOCKCHAIR | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| BTC_MEMPOOL | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 61 | reads ctx->entity |
| ENS_RESOLVE | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 572 | reads ctx->entity |
| ETH_BLOCKSCOUT | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | EMPTY | 0 | 0 | 100 | reads ctx->entity |
| OFAC_CRYPTO | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | EMPTY | 0 | 0 | 464 | reads ctx->entity |
| SOLANA_RPC | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 313 | reads ctx->entity |
| TRON_SCAN | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 315 | reads ctx->entity |

## `native/collectors/sources/ct_logs.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CERTIFICATE_TRANSPARENCY | cyber | 0 | domain=github.com | DATA | 84 | 0 | 1567 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/diet_records.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| diet-records | government | 86400 | - | DATA | 114 | 0 | 3187 | rt=diet-speech |

## `native/collectors/sources/domain_age.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DOMAIN_AGE | cyber | 0 | domain=github.com | DATA | 1 | 0 | 110 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/econ_world.c`  (6 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EUROSTAT | economy | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 110 | reads ctx->entity |
| FRED_SERIES | economy | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| IMF_DATA | economy | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 207 | reads ctx->entity |
| STOOQ_QUOTES | economy | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 121 | reads ctx->entity |
| UN_COMTRADE | economy | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| WORLDBANK_INDICATORS | economy | 0 | country=Japan | EMPTY | 0 | 0 | 885 | reads ctx->entity |

## `native/collectors/sources/electrical_grid.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| electrical-grid | infrastructure | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/estat_population.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| estat-population | statistics | 86400 | - | NO_TITLE | 2377 | 2377 | 51179 | rt=estat-population |

## `native/collectors/sources/fire_station_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| fire-station-map | safety | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/fr_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| FR_ADDRESS_BAN | geospatial | 0 | keyword=Tokyo | DATA | 6 | 6 | 115 | reads ctx->entity; rt=fr-address |
| FR_BODACC | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 369 | reads ctx->entity; rt=fr-bodacc-annonce |
| FR_DATAGOUV | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 91 | reads ctx->entity |

## `native/collectors/sources/fsa_crypto_exchanges.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| fsa-crypto-exchanges | government | 604800 | - | DATA | 1 | 0 | 91 | rt=(null) |

## `native/collectors/sources/geo_world2.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ELEVATION_LOOKUP | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| SUN_TIMES | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/geocoding.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GEOCODING | geospatial | 0 | keyword=Tokyo | DATA | 1 | 1 | 38 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/global_aleph.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ALEPH_SEARCH | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 165 | reads ctx->entity |

## `native/collectors/sources/global_rdap.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| RDAP_LOOKUP | cyber | 0 | domain=github.com | DATA | 1 | 0 | 106 | reads ctx->entity; rt=rdap-domain |

## `native/collectors/sources/grid_usage_realtime.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| grid-usage-realtime | infrastructure | 300 | - | DATA | 10 | 0 | 10489 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/hazard_world.c`  (5 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EMSC_QUAKES | environment | 300 | - | DATA | 50 | 50 | 131 | reads ctx->entity; rt=emsc-earthquake |
| FIRMS_GLOBAL | environment | 3600 | - | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| GDACS_DISASTERS | environment | 1800 | - | DATA | 60 | 60 | 736 | reads ctx->entity; rt=gdacs-disaster |
| GVP_VOLCANOES | environment | 86400 | - | DATA | 1 | 0 | 391 | reads ctx->entity; NO GEO despite map-ish category; rt=gvp-volcano |
| USGS_QUAKES | environment | 300 | - | DATA | 50 | 50 | 711 | reads ctx->entity; rt=usgs-earthquake |

## `native/collectors/sources/here_japan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| here-japan | commercial | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/hudson_rock_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| hudson-rock-jp | cyber | 86400 | - | NO_TITLE | 34 | 34 | 69739 | rt=hudson-rock-jp |

## `native/collectors/sources/intel_geoint_conflict.c`  (20 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| 38north | news | 3600 | - | RC_ERROR | 0 | 0 | 67 |  |
| bellingcat | news | 3600 | - | DATA | 10 | 0 | 119 | rt=article |
| breaking-defense | news | 3600 | - | DATA | 15 | 0 | 116 | rt=article |
| cfr | news | 3600 | - | RC_ERROR | 0 | 0 | 220 |  |
| cipher-brief | news | 3600 | - | DATA | 30 | 0 | 279 | rt=article |
| defense-news | news | 3600 | - | DATA | 25 | 0 | 148 | rt=article |
| icij | news | 3600 | - | DATA | 11 | 0 | 120 | rt=article |
| isw | news | 3600 | - | RC_ERROR | 0 | 0 | 55 |  |
| kyiv-independent | news | 3600 | - | RC_ERROR | 0 | 0 | 185 |  |
| long-war-journal | news | 3600 | - | DATA | 30 | 0 | 239 | rt=article |
| maritime-executive | news | 3600 | - | RC_ERROR | 0 | 0 | 800 |  |
| meduza-en | news | 3600 | - | DATA | 30 | 0 | 154 | rt=article |
| moscow-times | news | 3600 | - | DATA | 50 | 0 | 173 | rt=article |
| naval-news | news | 3600 | - | DATA | 10 | 0 | 482 | rt=article |
| occrp | news | 3600 | - | DATA | 60 | 0 | 390 | rt=article |
| rferl | news | 3600 | - | EMPTY | 0 | 0 | 74 |  |
| scmp-china | news | 3600 | - | DATA | 50 | 0 | 739 | rt=article |
| the-diplomat | news | 3600 | - | DATA | 96 | 0 | 272 | rt=article |
| the-war-zone | news | 3600 | - | DATA | 39 | 0 | 231 | rt=article |
| war-on-the-rocks | news | 3600 | - | DATA | 100 | 0 | 691 | rt=article |

## `native/collectors/sources/ip_geolocation.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| IP_GEOLOCATION | cyber | 0 | ip=8.8.8.8 | DATA | 1 | 1 | 74 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/jaxa_earth.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jaxa-earth | satellite | 86400 | - | RC_ERROR | 0 | 0 | 3288 |  |

## `native/collectors/sources/jma_himawari.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-himawari | environment | 600 | - | DATA | 1 | 0 | 747 | NO GEO despite map-ish category; rt=article |

## `native/collectors/sources/jma_typhoon_json.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-typhoon-json | environment | 900 | - | EMPTY | 0 | 0 | 38 |  |

## `native/collectors/sources/jp_news_rss.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jp-news-rss | social | 1800 | - | DATA | 87 | 0 | 4344 | rt=article |

## `native/collectors/sources/jr_east_delay.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jr-east-delay | transport | 1800 | - | DATA | 1 | 0 | 1031 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/kanpo_gazette.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| kanpo-gazette | government | 86400 | - | DATA | 173 | 0 | 3172 | rt=kanpo-notice |

## `native/collectors/sources/lighthouse_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| lighthouse-map | transport | 604800 | - | DATA | 1499 | 1499 | 32261 | rt=lighthouse-map |

## `native/collectors/sources/marinetraffic_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| marinetraffic-jp | transport | 300 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/mic_elections.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mic-elections | government | 604800 | - | DATA | 1 | 0 | 8003 | rt=(null) |

## `native/collectors/sources/mlit_n05_rail_history.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-n05-rail-history | transport | 2592000 | - | EMPTY | 0 | 0 | 1264 |  |

## `native/collectors/sources/moj_crime_whitepaper.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| moj-crime-whitepaper | crime | 604800 | - | DATA | 3 | 1 | 10459 | rt=moj-crime-whitepaper,(null) |

## `native/collectors/sources/ndl_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ndl-search | government | 86400 | - | DATA | 1 | 0 | 4084 | rt=(null) |

## `native/collectors/sources/news_world2.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GNEWS_SEARCH | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| MEDIASTACK_SEARCH | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| NEWSAPI_SEARCH | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/nhk_relay_towers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nhk-relay-towers | infrastructure | 604800 | - | DATA | 1 | 0 | 403 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/note_com_profiles.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| note-com-profiles | social | 3600 | - | RC_ERROR | 0 | 0 | 2437 |  |

## `native/collectors/sources/nra_radiation.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nra-radiation | environment | 600 | - | RC_ERROR | 0 | 0 | 11295 |  |

## `native/collectors/sources/onsen_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| onsen-map | culture | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/osm_transport_ports.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| osm-transport-ports | transport | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/pachinko_density.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| pachinko-density | crime | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/people_world.c`  (5 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GITHUB_USER | investigation | 0 | keyword=Tokyo | DATA | 10 | 0 | 585 | reads ctx->entity; rt=github-user |
| GITLAB_USER | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 211 | reads ctx->entity |
| GRAVATAR_GLOBAL | investigation | 0 | username=torvalds | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| KEYBASE | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 303 | reads ctx->entity |
| MASTODON_SEARCH | investigation | 0 | username=torvalds | DATA | 20 | 0 | 495 | reads ctx->entity; rt=mastodon-account |

## `native/collectors/sources/petrochemical.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| petrochemical | industry | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/police_crime.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| police-crime | investigation | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/public_cameras.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| public-cameras | investigation | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/reg_africa2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AFRICA2_REGISTRY | government | 0 | company=Toyota | DATA | 1 | 0 | 9541 | reads ctx->entity; rt=africa2-registry-company |

## `native/collectors/sources/reg_es_borme.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ES_BORME | government | 0 | company=Toyota | EMPTY | 0 | 0 | 132 | reads ctx->entity |

## `native/collectors/sources/reg_kr_dart.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| KR_DART | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/reg_sk_rpo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SK_RPO | government | 0 | company=Toyota | DATA | 11 | 0 | 3463 | reads ctx->entity; rt=sk-rpo-entity |

## `native/collectors/sources/resas_municipality.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| resas-municipality | statistics | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/research_world2.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CORE_SEARCH | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| HACKERNEWS_SEARCH | investigation | 0 | keyword=Tokyo | DATA | 20 | 0 | 392 | reads ctx->entity; rt=hackernews-item |
| REDDIT_PULLPUSH | investigation | 0 | keyword=Tokyo | DATA | 20 | 0 | 2534 | reads ctx->entity; rt=reddit-submission |
| WIKIPEDIA_SEARCH | investigation | 0 | keyword=Tokyo | DATA | 20 | 0 | 393 | reads ctx->entity; rt=wikipedia-article |

## `native/collectors/sources/saibansyo_rulings.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| saibansyo-rulings | government | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/satellite_tracker.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SATELLITE_TRACKER | transport | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/science_space_tech.c`  (15 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ars-technica | news | 7200 | - | (not swept) | - | - | - |  |
| defense-one | news | 7200 | - | (not swept) | - | - | - |  |
| esa-topnews | news | 7200 | - | (not swept) | - | - | - |  |
| hn-frontpage | news | 7200 | - | (not swept) | - | - | - |  |
| ieee-spectrum | news | 7200 | - | (not swept) | - | - | - |  |
| mit-news | news | 7200 | - | (not swept) | - | - | - |  |
| mit-tech-review | news | 7200 | - | (not swept) | - | - | - |  |
| nature-news | news | 7200 | - | (not swept) | - | - | - |  |
| phys-org | news | 7200 | - | (not swept) | - | - | - |  |
| quanta | news | 7200 | - | (not swept) | - | - | - |  |
| science-aaas | news | 7200 | - | (not swept) | - | - | - |  |
| sciencedaily | news | 7200 | - | (not swept) | - | - | - |  |
| space-com | news | 7200 | - | (not swept) | - | - | - |  |
| the-register | news | 7200 | - | (not swept) | - | - | - |  |
| wired | news | 7200 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/search_world.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BING_WEB_SEARCH | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| SERPAPI_SEARCH | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/shinkansen_status.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| shinkansen-status | transport | 60 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/social_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SOCIAL_EMAIL | social | 0 | email=test@example.com | RC_ERROR | 119 | 0 | 83548 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/stadiums.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| stadiums | tourism | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/suumo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| suumo | economy | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/telegram_jp_channels.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| telegram-jp-channels | cyber | 1800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/threatfox_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| threatfox-jp | cyber | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/transmission_towers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| transmission-towers | infrastructure | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/uk_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| UK_FSA_RATINGS | government | 0 | keyword=Tokyo | DATA | 5 | 0 | 234 | reads ctx->entity; rt=uk-food-rating |
| UK_POLICE_CRIME | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| UK_POSTCODES | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 131 | reads ctx->entity |

## `native/collectors/sources/unified_ais_ships.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unified-ais-ships | transport | 300 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/urlhaus_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| urlhaus-jp | cyber | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/vics_traffic.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| vics-traffic | transport | 300 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/water_infra.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| water-infra | infrastructure | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/whoisxml_reverse.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| whoisxml-reverse | cyber | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/wikipedia_ja_recent.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wikipedia-ja-recent | social | 600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/world_outlets_2.c`  (57 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| addis-standard | news | 3600 | - | (not swept) | - | - | - |  |
| africa-report | news | 3600 | - | (not swept) | - | - | - |  |
| al-monitor | news | 3600 | - | (not swept) | - | - | - |  |
| americas-quarterly | news | 3600 | - | (not swept) | - | - | - |  |
| astana-times | news | 3600 | - | (not swept) | - | - | - |  |
| balkan-insight | news | 3600 | - | (not swept) | - | - | - |  |
| baltic-times | news | 3600 | - | (not swept) | - | - | - |  |
| belta-by | news | 3600 | - | (not swept) | - | - | - |  |
| brussels-times | news | 3600 | - | (not swept) | - | - | - |  |
| buenosaires-h | news | 3600 | - | (not swept) | - | - | - |  |
| caixin | news | 3600 | - | (not swept) | - | - | - |  |
| caracas-chronicles | news | 3600 | - | (not swept) | - | - | - |  |
| caucasus-watch | news | 3600 | - | (not swept) | - | - | - |  |
| daily-mirror-lk | news | 3600 | - | (not swept) | - | - | - |  |
| daily-sabah | news | 3600 | - | (not swept) | - | - | - |  |
| dawn-world | news | 3600 | - | (not swept) | - | - | - |  |
| dutch-news | news | 3600 | - | (not swept) | - | - | - |  |
| euractiv | news | 3600 | - | (not swept) | - | - | - |  |
| eurasianet | news | 3600 | - | (not swept) | - | - | - |  |
| fiji-times | news | 3600 | - | (not swept) | - | - | - |  |
| greek-reporter | news | 3600 | - | (not swept) | - | - | - |  |
| guardian-au | news | 3600 | - | (not swept) | - | - | - |  |
| ice-news | news | 3600 | - | (not swept) | - | - | - |  |
| indian-express | news | 3600 | - | (not swept) | - | - | - |  |
| interfax | news | 3600 | - | (not swept) | - | - | - |  |
| irish-times | news | 3600 | - | (not swept) | - | - | - |  |
| irrawaddy | news | 3600 | - | (not swept) | - | - | - |  |
| jamaica-gleaner | news | 3600 | - | (not swept) | - | - | - |  |
| kathmandu-post | news | 3600 | - | (not swept) | - | - | - |  |
| khmer-times | news | 3600 | - | (not swept) | - | - | - |  |
| kommersant | news | 3600 | - | (not swept) | - | - | - |  |
| kyiv-independent-2 | news | 3600 | - | (not swept) | - | - | - |  |
| local-se | news | 3600 | - | (not swept) | - | - | - |  |
| mail-guardian-2 | news | 3600 | - | (not swept) | - | - | - |  |
| mees | news | 3600 | - | (not swept) | - | - | - |  |
| mercopress-2 | news | 3600 | - | (not swept) | - | - | - |  |
| mg-africa-2 | news | 3600 | - | (not swept) | - | - | - |  |
| middle-east-eye | news | 3600 | - | (not swept) | - | - | - |  |
| myanmar-now | news | 3600 | - | (not swept) | - | - | - |  |
| nation-thailand | news | 3600 | - | (not swept) | - | - | - |  |
| nikkei-asia-2 | news | 3600 | - | (not swept) | - | - | - |  |
| notes-poland | news | 3600 | - | (not swept) | - | - | - |  |
| nzherald | news | 3600 | - | (not swept) | - | - | - |  |
| pina-pacific | news | 3600 | - | (not swept) | - | - | - |  |
| politico-eu | news | 3600 | - | (not swept) | - | - | - |  |
| rappler-ph | news | 3600 | - | (not swept) | - | - | - |  |
| scmp-china3 | news | 3600 | - | (not swept) | - | - | - |  |
| semafor-africa | news | 3600 | - | (not swept) | - | - | - |  |
| smh-au | news | 3600 | - | (not swept) | - | - | - |  |
| swissinfo | news | 3600 | - | (not swept) | - | - | - |  |
| the-diplomat-2 | news | 3600 | - | (not swept) | - | - | - |  |
| the-east-african | news | 3600 | - | (not swept) | - | - | - |  |
| thecable-ng | news | 3600 | - | (not swept) | - | - | - |  |
| thelocal-eu | news | 3600 | - | (not swept) | - | - | - |  |
| tico-times | news | 3600 | - | (not swept) | - | - | - |  |
| tuoitre | news | 3600 | - | (not swept) | - | - | - |  |
| vnexpress | news | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/world_reg_china.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CHINA_REGISTRY | government | 0 | company=Toyota | DATA | 12 | 0 | 50487 | reads ctx->entity; rt=icp-beian,court-judgement,gazette |

## `native/collectors/sources/yahoo_chiebukuro.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| yahoo-chiebukuro | social | 3600 | - | (not swept) | - | - | - |  |
