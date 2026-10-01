# Audit slice 7 — 206 sources across 81 files

`verdict` is from the bulk sweep and is ADVISORY: a blank means the sweep had not reached it, and an EMPTY may just mean the sweep guessed the wrong pivot entity. Re-run anything you report on.

## `native/collectors/sources/agoop_flow.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| agoop-flow | commercial | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/auto_plants.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| auto-plants | industry | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/bluesky_jetstream_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bluesky-jetstream-jp | social | 600 | - | EMPTY | 0 | 0 | 0 |  |

## `native/collectors/sources/cam_geocam.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-geocam | investigation | 21600 | - | DATA | 3 | 3 | 677 | rt=camera |

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
| GITHUB_GISTS | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 288 | reads ctx->entity |
| GITHUB_REPO_SEARCH | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 896 | reads ctx->entity; rt=github-repo |
| GITLAB_PROJECT_SEARCH | cyber | 0 | keyword=Tokyo | DATA | 20 | 0 | 1025 | reads ctx->entity; rt=gitlab-project |

## `native/collectors/sources/country_geo2.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AT_POSTAL | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| CH_ADDRESS | geospatial | 0 | keyword=Tokyo | DATA | 10 | 10 | 160 | reads ctx->entity; rt=ch-location |
| CH_POSTAL | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| NO_ADDRESS | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 230 | reads ctx->entity |

## `native/collectors/sources/country_world3.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DK_ADDRESS | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 192 | reads ctx->entity |

## `native/collectors/sources/crypto_onchain.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DEFI_TRACKER | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| EXCHANGE_FLOW | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/crypto_world.c`  (7 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BLOCKCHAIR | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| BTC_MEMPOOL | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 94 | reads ctx->entity |
| ENS_RESOLVE | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 193 | reads ctx->entity |
| ETH_BLOCKSCOUT | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | EMPTY | 0 | 0 | 152 | reads ctx->entity |
| OFAC_CRYPTO | cyber | 0 | btc=1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa | EMPTY | 0 | 0 | 721 | reads ctx->entity |
| SOLANA_RPC | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 362 | reads ctx->entity |
| TRON_SCAN | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 300 | reads ctx->entity |

## `native/collectors/sources/ct_logs.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CERTIFICATE_TRANSPARENCY | cyber | 0 | domain=github.com | DATA | 84 | 0 | 15454 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/diet_records.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| diet-records | government | 86400 | - | DATA | 114 | 0 | 2328 | rt=diet-speech |

## `native/collectors/sources/domain_age.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DOMAIN_AGE | cyber | 0 | domain=github.com | DATA | 1 | 0 | 99 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/econ_world.c`  (6 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EUROSTAT | economy | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 82 | reads ctx->entity |
| FRED_SERIES | economy | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| IMF_DATA | economy | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 520 | reads ctx->entity |
| STOOQ_QUOTES | economy | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 135 | reads ctx->entity |
| UN_COMTRADE | economy | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| WORLDBANK_INDICATORS | economy | 0 | country=Japan | EMPTY | 0 | 0 | 84 | reads ctx->entity |

## `native/collectors/sources/electrical_grid.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| electrical-grid | infrastructure | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/estat_population.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| estat-population | statistics | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/fire_station_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| fire-station-map | safety | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/fr_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| FR_ADDRESS_BAN | geospatial | 0 | keyword=Tokyo | DATA | 6 | 6 | 262 | reads ctx->entity; rt=fr-address |
| FR_BODACC | government | 0 | keyword=Tokyo | DATA | 20 | 0 | 312 | reads ctx->entity; rt=fr-bodacc-annonce |
| FR_DATAGOUV | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 69 | reads ctx->entity |

## `native/collectors/sources/fsa_crypto_exchanges.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| fsa-crypto-exchanges | government | 604800 | - | DATA | 1 | 0 | 104 | rt=(null) |

## `native/collectors/sources/geo_world2.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ELEVATION_LOOKUP | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| SUN_TIMES | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/geocoding.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GEOCODING | geospatial | 0 | keyword=Tokyo | DATA | 1 | 1 | 88 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/global_aleph.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ALEPH_SEARCH | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 184 | reads ctx->entity |

## `native/collectors/sources/global_rdap.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| RDAP_LOOKUP | cyber | 0 | domain=github.com | DATA | 1 | 0 | 96 | reads ctx->entity; rt=rdap-domain |

## `native/collectors/sources/grid_usage_realtime.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| grid-usage-realtime | infrastructure | 300 | - | DATA | 10 | 0 | 8546 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/hazard_world.c`  (5 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EMSC_QUAKES | environment | 300 | - | DATA | 50 | 50 | 107 | reads ctx->entity; rt=emsc-earthquake |
| FIRMS_GLOBAL | environment | 3600 | - | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| GDACS_DISASTERS | environment | 1800 | - | DATA | 60 | 60 | 842 | reads ctx->entity; rt=gdacs-disaster |
| GVP_VOLCANOES | environment | 86400 | - | DATA | 1 | 0 | 216 | reads ctx->entity; NO GEO despite map-ish category; rt=gvp-volcano |
| USGS_QUAKES | environment | 300 | - | DATA | 50 | 50 | 557 | reads ctx->entity; rt=usgs-earthquake |

## `native/collectors/sources/here_japan.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| here-japan | commercial | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/hudson_rock_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| hudson-rock-jp | cyber | 86400 | - | NO_TITLE | 34 | 34 | 73508 | rt=hudson-rock-jp |

## `native/collectors/sources/intel_geoint_conflict.c`  (20 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| 38north | news | 3600 | - | RC_ERROR | 0 | 0 | 62 |  |
| bellingcat | news | 3600 | - | DATA | 10 | 0 | 125 | rt=article |
| breaking-defense | news | 3600 | - | DATA | 15 | 0 | 140 | rt=article |
| cfr | news | 3600 | - | RC_ERROR | 0 | 0 | 230 |  |
| cipher-brief | news | 3600 | - | DATA | 30 | 0 | 256 | rt=article |
| defense-news | news | 3600 | - | DATA | 25 | 0 | 149 | rt=article |
| icij | news | 3600 | - | DATA | 11 | 0 | 113 | rt=article |
| isw | news | 3600 | - | RC_ERROR | 0 | 0 | 59 |  |
| kyiv-independent | news | 3600 | - | RC_ERROR | 0 | 0 | 158 |  |
| long-war-journal | news | 3600 | - | DATA | 30 | 0 | 237 | rt=article |
| maritime-executive | news | 3600 | - | RC_ERROR | 0 | 0 | 1054 |  |
| meduza-en | news | 3600 | - | DATA | 30 | 0 | 129 | rt=article |
| moscow-times | news | 3600 | - | DATA | 50 | 0 | 227 | rt=article |
| naval-news | news | 3600 | - | DATA | 10 | 0 | 476 | rt=article |
| occrp | news | 3600 | - | DATA | 60 | 0 | 5080 | rt=article |
| rferl | news | 3600 | - | EMPTY | 0 | 0 | 92 |  |
| scmp-china | news | 3600 | - | DATA | 50 | 0 | 1094 | rt=article |
| the-diplomat | news | 3600 | - | DATA | 96 | 0 | 244 | rt=article |
| the-war-zone | news | 3600 | - | DATA | 38 | 0 | 166 | rt=article |
| war-on-the-rocks | news | 3600 | - | DATA | 100 | 0 | 695 | rt=article |

## `native/collectors/sources/ip_geolocation.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| IP_GEOLOCATION | cyber | 0 | ip=8.8.8.8 | DATA | 1 | 1 | 42 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/jaxa_earth.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jaxa-earth | satellite | 86400 | - | RC_ERROR | 0 | 0 | 3033 |  |

## `native/collectors/sources/jma_himawari.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-himawari | environment | 600 | - | DATA | 1 | 0 | 811 | NO GEO despite map-ish category; rt=article |

## `native/collectors/sources/jma_typhoon_json.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-typhoon-json | environment | 900 | - | EMPTY | 0 | 0 | 50 |  |

## `native/collectors/sources/jp_news_rss.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jp-news-rss | social | 1800 | - | DATA | 87 | 0 | 4505 | rt=article |

## `native/collectors/sources/jr_east_delay.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jr-east-delay | transport | 1800 | - | DATA | 1 | 0 | 1110 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/kanpo_gazette.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| kanpo-gazette | government | 86400 | - | DATA | 173 | 0 | 3660 | rt=kanpo-notice |

## `native/collectors/sources/lighthouse_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| lighthouse-map | transport | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/marinetraffic_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| marinetraffic-jp | transport | 300 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/mic_elections.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mic-elections | government | 604800 | - | DATA | 1 | 0 | 8533 | rt=(null) |

## `native/collectors/sources/mlit_n05_rail_history.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-n05-rail-history | transport | 2592000 | - | EMPTY | 0 | 0 | 1398 |  |

## `native/collectors/sources/moj_crime_whitepaper.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| moj-crime-whitepaper | crime | 604800 | - | DATA | 3 | 1 | 11240 | rt=moj-crime-whitepaper,(null) |

## `native/collectors/sources/ndl_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ndl-search | government | 86400 | - | DATA | 1 | 0 | 8888 | rt=(null) |

## `native/collectors/sources/news_world2.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GNEWS_SEARCH | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| MEDIASTACK_SEARCH | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| NEWSAPI_SEARCH | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/nhk_relay_towers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nhk-relay-towers | infrastructure | 604800 | - | DATA | 1 | 0 | 423 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/note_com_profiles.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| note-com-profiles | social | 3600 | - | RC_ERROR | 0 | 0 | 1328 |  |

## `native/collectors/sources/nra_radiation.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nra-radiation | environment | 600 | - | RC_ERROR | 0 | 0 | 12082 |  |

## `native/collectors/sources/onsen_map.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| onsen-map | culture | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/osm_transport_ports.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| osm-transport-ports | transport | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/pachinko_density.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| pachinko-density | crime | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/people_world.c`  (5 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GITHUB_USER | investigation | 0 | keyword=Tokyo | DATA | 10 | 0 | 587 | reads ctx->entity; rt=github-user |
| GITLAB_USER | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 288 | reads ctx->entity |
| GRAVATAR_GLOBAL | investigation | 0 | username=torvalds | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| KEYBASE | investigation | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 319 | reads ctx->entity |
| MASTODON_SEARCH | investigation | 0 | username=torvalds | DATA | 20 | 0 | 456 | reads ctx->entity; rt=mastodon-account |

## `native/collectors/sources/petrochemical.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| petrochemical | industry | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/police_crime.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| police-crime | investigation | 86400 | - | NO_TITLE | 9903 | 9903 | 44383 | rt=police-crime |

## `native/collectors/sources/public_cameras.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| public-cameras | investigation | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/reg_africa2.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| AFRICA2_REGISTRY | government | 0 | company=Toyota | DATA | 1 | 0 | 13531 | reads ctx->entity; rt=africa2-registry-company |

## `native/collectors/sources/reg_es_borme.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ES_BORME | government | 0 | company=Toyota | EMPTY | 0 | 0 | 164 | reads ctx->entity |

## `native/collectors/sources/reg_kr_dart.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| KR_DART | government | 0 | company=Toyota | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/reg_sk_rpo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SK_RPO | government | 0 | company=Toyota | DATA | 11 | 0 | 4687 | reads ctx->entity; rt=sk-rpo-entity |

## `native/collectors/sources/resas_municipality.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| resas-municipality | statistics | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/research_world2.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CORE_SEARCH | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| HACKERNEWS_SEARCH | investigation | 0 | keyword=Tokyo | DATA | 20 | 0 | 508 | reads ctx->entity; rt=hackernews-item |
| REDDIT_PULLPUSH | investigation | 0 | keyword=Tokyo | DATA | 20 | 0 | 5852 | reads ctx->entity; rt=reddit-submission |
| WIKIPEDIA_SEARCH | investigation | 0 | keyword=Tokyo | DATA | 20 | 0 | 507 | reads ctx->entity; rt=wikipedia-article |

## `native/collectors/sources/saibansyo_rulings.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| saibansyo-rulings | government | 86400 | - | DATA | 1 | 0 | 1068 | rt=(null) |

## `native/collectors/sources/satellite_tracker.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SATELLITE_TRACKER | transport | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/science_space_tech.c`  (15 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ars-technica | news | 7200 | - | DATA | 20 | 0 | 107 | rt=article |
| defense-one | news | 7200 | - | DATA | 22 | 0 | 641 | rt=article |
| esa-topnews | news | 7200 | - | DATA | 9 | 0 | 77 | rt=article |
| hn-frontpage | news | 7200 | - | DATA | 20 | 0 | 511 | rt=article |
| ieee-spectrum | news | 7200 | - | DATA | 30 | 0 | 405 | rt=article |
| mit-news | news | 7200 | - | DATA | 50 | 0 | 174 | rt=article |
| mit-tech-review | news | 7200 | - | DATA | 10 | 0 | 82 | rt=article |
| nature-news | news | 7200 | - | DATA | 75 | 0 | 127 | rt=article |
| phys-org | news | 7200 | - | DATA | 30 | 0 | 353 | rt=article |
| quanta | news | 7200 | - | DATA | 5 | 0 | 66 | rt=article |
| science-aaas | news | 7200 | - | DATA | 10 | 0 | 81 | rt=article |
| sciencedaily | news | 7200 | - | DATA | 60 | 0 | 772 | rt=article |
| space-com | news | 7200 | - | DATA | 50 | 0 | 247 | rt=article |
| the-register | news | 7200 | - | DATA | 50 | 0 | 375 | rt=article |
| wired | news | 7200 | - | DATA | 50 | 0 | 122 | rt=article |

## `native/collectors/sources/search_world.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BING_WEB_SEARCH | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| SERPAPI_SEARCH | investigation | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/shinkansen_status.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| shinkansen-status | transport | 60 | - | DATA | 5 | 0 | 4949 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/social_search.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| SOCIAL_EMAIL | social | 0 | email=test@example.com | RC_ERROR | 122 | 0 | 68072 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/stadiums.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| stadiums | tourism | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/suumo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| suumo | economy | 86400 | - | DATA | 7 | 0 | 73030 | rt=suumo |

## `native/collectors/sources/telegram_jp_channels.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| telegram-jp-channels | cyber | 1800 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/threatfox_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| threatfox-jp | cyber | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/transmission_towers.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| transmission-towers | infrastructure | 604800 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/uk_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| UK_FSA_RATINGS | government | 0 | keyword=Tokyo | DATA | 5 | 0 | 296 | reads ctx->entity; rt=uk-food-rating |
| UK_POLICE_CRIME | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| UK_POSTCODES | geospatial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 73 | reads ctx->entity |

## `native/collectors/sources/unified_ais_ships.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unified-ais-ships | transport | 300 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/urlhaus_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| urlhaus-jp | cyber | 3600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/vics_traffic.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| vics-traffic | transport | 300 | - | DATA | 1 | 0 | 8369 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/water_infra.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| water-infra | infrastructure | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/whoisxml_reverse.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| whoisxml-reverse | cyber | 86400 | - | DATA | 1 | 0 | 7919 | rt=(null) |

## `native/collectors/sources/wikipedia_ja_recent.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wikipedia-ja-recent | social | 600 | - | DATA | 200 | 200 | 570 | rt=wikipedia-ja-recent |

## `native/collectors/sources/world_outlets_2.c`  (57 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| addis-standard | news | 3600 | - | RC_ERROR | 0 | 0 | 87 |  |
| africa-report | news | 3600 | - | DATA | 10 | 0 | 180 | rt=article |
| al-monitor | news | 3600 | - | DATA | 14 | 0 | 821 | rt=article |
| americas-quarterly | news | 3600 | - | DATA | 10 | 0 | 283 | rt=article |
| astana-times | news | 3600 | - | DATA | 10 | 0 | 383 | rt=article |
| balkan-insight | news | 3600 | - | DATA | 90 | 0 | 455 | rt=article |
| baltic-times | news | 3600 | - | DATA | 50 | 0 | 616 | rt=article |
| belta-by | news | 3600 | - | DATA | 100 | 0 | 643 | rt=article |
| brussels-times | news | 3600 | - | EMPTY | 0 | 0 | 339 |  |
| buenosaires-h | news | 3600 | - | DATA | 10 | 0 | 1205 | rt=article |
| caixin | news | 3600 | - | RC_ERROR | 0 | 0 | 1196 |  |
| caracas-chronicles | news | 3600 | - | DATA | 3 | 0 | 568 | rt=article |
| caucasus-watch | news | 3600 | - | RC_ERROR | 0 | 0 | 588 |  |
| daily-mirror-lk | news | 3600 | - | RC_ERROR | 0 | 0 | 234 |  |
| daily-sabah | news | 3600 | - | DATA | 50 | 0 | 406 | rt=article |
| dawn-world | news | 3600 | - | DATA | 30 | 0 | 126 | rt=article |
| dutch-news | news | 3600 | - | DATA | 10 | 0 | 322 | rt=article |
| euractiv | news | 3600 | - | RC_ERROR | 0 | 0 | 51 |  |
| eurasianet | news | 3600 | - | RC_ERROR | 0 | 0 | 64 |  |
| fiji-times | news | 3600 | - | EMPTY | 0 | 0 | 3156 |  |
| greek-reporter | news | 3600 | - | RC_ERROR | 0 | 0 | 91 |  |
| guardian-au | news | 3600 | - | DATA | 23 | 0 | 162 | rt=article |
| ice-news | news | 3600 | - | RC_ERROR | 0 | 0 | 78 |  |
| indian-express | news | 3600 | - | RC_ERROR | 0 | 0 | 47 |  |
| interfax | news | 3600 | - | DATA | 25 | 0 | 461 | rt=article |
| irish-times | news | 3600 | - | RC_ERROR | 0 | 0 | 238 |  |
| irrawaddy | news | 3600 | - | RC_ERROR | 0 | 0 | 62 |  |
| jamaica-gleaner | news | 3600 | - | DATA | 10 | 0 | 559 | rt=article |
| kathmandu-post | news | 3600 | - | DATA | 40 | 0 | 792 | rt=article |
| khmer-times | news | 3600 | - | DATA | 20 | 0 | 1634 | rt=article |
| kommersant | news | 3600 | - | DATA | 588 | 0 | 2181 | rt=article |
| kyiv-independent-2 | news | 3600 | - | RC_ERROR | 0 | 0 | 189 |  |
| local-se | news | 3600 | - | DATA | 20 | 0 | 771 | rt=article |
| mail-guardian-2 | news | 3600 | - | RC_ERROR | 0 | 0 | 129 |  |
| mees | news | 3600 | - | RC_ERROR | 0 | 0 | 56 |  |
| mercopress-2 | news | 3600 | - | DATA | 10 | 0 | 519 | rt=article |
| mg-africa-2 | news | 3600 | - | DATA | 50 | 0 | 273 | rt=article |
| middle-east-eye | news | 3600 | - | DATA | 20 | 0 | 121 | rt=article |
| myanmar-now | news | 3600 | - | DATA | 10 | 0 | 795 | rt=article |
| nation-thailand | news | 3600 | - | EMPTY | 0 | 0 | 75 |  |
| nikkei-asia-2 | news | 3600 | - | DATA | 50 | 0 | 167 | rt=article |
| notes-poland | news | 3600 | - | DATA | 12 | 0 | 104 | rt=article |
| nzherald | news | 3600 | - | EMPTY | 0 | 0 | 1145 |  |
| pina-pacific | news | 3600 | - | DATA | 17 | 0 | 133 | rt=article |
| politico-eu | news | 3600 | - | RC_ERROR | 0 | 0 | 47 |  |
| rappler-ph | news | 3600 | - | RC_ERROR | 0 | 0 | 490 |  |
| scmp-china3 | news | 3600 | - | DATA | 50 | 0 | 1078 | rt=article |
| semafor-africa | news | 3600 | - | DATA | 272 | 0 | 795 | rt=article |
| smh-au | news | 3600 | - | DATA | 20 | 0 | 123 | rt=article |
| swissinfo | news | 3600 | - | RC_ERROR | 0 | 0 | 1024 |  |
| the-diplomat-2 | news | 3600 | - | DATA | 96 | 0 | 285 | rt=article |
| the-east-african | news | 3600 | - | RC_ERROR | 0 | 0 | 83 |  |
| thecable-ng | news | 3600 | - | RC_ERROR | 0 | 0 | 52 |  |
| thelocal-eu | news | 3600 | - | DATA | 20 | 0 | 588 | rt=article |
| tico-times | news | 3600 | - | DATA | 10 | 0 | 134 | rt=article |
| tuoitre | news | 3600 | - | RC_ERROR | 0 | 0 | 2279 |  |
| vnexpress | news | 3600 | - | DATA | 58 | 0 | 952 | rt=article |

## `native/collectors/sources/world_reg_china.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CHINA_REGISTRY | government | 0 | company=Toyota | DATA | 12 | 0 | 85416 | reads ctx->entity; rt=icp-beian,court-judgement,gazette |

## `native/collectors/sources/yahoo_chiebukuro.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| yahoo-chiebukuro | social | 3600 | - | DATA | 1 | 0 | 8561 | rt=(null) |
