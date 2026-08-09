# Audit slice 10 — 206 sources across 83 files

`verdict` is from the bulk sweep and is ADVISORY: a blank means the sweep had not reached it, and an EMPTY may just mean the sweep guessed the wrong pivot entity. Re-run anything you report on.

## `native/collectors/sources/abuseipdb_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| abuseipdb-jp | cyber | 21600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/address_resolver.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ADDRESS_RESOLVER | geospatial | 0 | keyword=Tokyo | DATA | 1 | 1 | 234 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/aid_world.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| IATI_REGISTRY | government | 0 | company=Toyota | EMPTY | 0 | 0 | 79 | reads ctx->entity |
| OCHA_FTS | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 397 | reads ctx->entity |
| RELIEFWEB | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 320 | reads ctx->entity |
| WORLDBANK_PROJECTS | government | 0 | country=Japan | DATA | 25 | 0 | 463 | reads ctx->entity; rt=aid-project |

## `native/collectors/sources/alos_palsar.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| alos-palsar | satellite | 604800 | - | DATA | 25 | 0 | 1398 | NO GEO despite map-ish category; rt=alos-palsar |

## `native/collectors/sources/aviation_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ADSB_FI | transport | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 609 | reads ctx->entity |
| HEXDB_AIRCRAFT | transport | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 97 | reads ctx->entity |
| OURAIRPORTS | transport | 0 | company=Toyota | DATA | 15 | 15 | 423 | reads ctx->entity; rt=airport |

## `native/collectors/sources/bgp_tools_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bgp-tools-jp | telecom | 86400 | - | EMPTY | 0 | 0 | 0 |  |

## `native/collectors/sources/bosai_volcano_cam.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bosai-volcano-cam | seismic | 300 | - | DATA | 1 | 0 | 60 | rt=(null) |

## `native/collectors/sources/cam_skylinewebcams.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-skylinewebcams | investigation | 21600 | - | DATA | 36 | 36 | 194 | rt=camera |

## `native/collectors/sources/cam_youtube_live.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| cam-youtube_live | investigation | 21600 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/chaos_bugbounty_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| chaos-bugbounty-jp | cyber | 86400 | - | NO_TITLE | 8 | 8 | 1108 | rt=program |

## `native/collectors/sources/comiket_events.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| comiket-events | social | 86400 | - | DATA | 1 | 0 | 842 | rt=(null) |

## `native/collectors/sources/corp_markets_media.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| FSA_FINBIZ_REGISTRY | government | 0 | company=Toyota | WAF_BLOCKED | 0 | 0 | 734 | reads ctx->entity |
| ITOWNPAGE | commercial | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 40750 | reads ctx->entity |
| JPX_SHORT_SELLING | economy | 0 | keyword=Tokyo | DATA | 25 | 0 | 482 | reads ctx->entity; rt=short-selling-report |
| PRTIMES | commercial | 1800 | - | DATA | 30 | 0 | 875 | reads ctx->entity; rt=press-release |

## `native/collectors/sources/country_extras.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CA_PARLIAMENT | government | 0 | country=Japan | DATA | 20 | 0 | 519 | reads ctx->entity; rt=ca-politician |
| DE_POSTAL | geospatial | 0 | country=Japan | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| NL_ADDRESS | geospatial | 0 | country=Japan | EMPTY | 0 | 0 | 82 | reads ctx->entity |

## `native/collectors/sources/courts_world.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| COURT_WORLD | government | 0 | keyword=Tokyo | DATA | 25 | 0 | 5886 | reads ctx->entity; rt=court-case |

## `native/collectors/sources/dark_web_monitor.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DARK_WEB_MONITOR | cyber | 0 | keyword=Tokyo | DATA | 1 | 0 | 279 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/dnstwist_jp_targets.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| dnstwist-jp-targets | cyber | 86400 | - | DATA | 50 | 0 | 35758 | rt=dnstwist-jp-targets |

## `native/collectors/sources/earthquake_monitor.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EARTHQUAKE_MONITOR | environment | 0 | keyword=Tokyo | DATA | 20 | 20 | 273 | rt=osint_service_result |

## `native/collectors/sources/estat_crime.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| estat-crime | crime | 604800 | - | DATA | 1 | 0 | 0 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/facebook_geo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| facebook-geo | social | 1800 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/flight_adsb.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| flight-adsb | transport | 60 | - | NO_TITLE | 367 | 367 | 1592 | rt=flight-adsb |

## `native/collectors/sources/gas_stations.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gas-stations | infrastructure | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/geothermal_springs.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| geothermal-springs | industry | 86400 | - | DATA | 6867 | 6867 | 21363 | rt=geothermal-springs |

## `native/collectors/sources/global_littlesis.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| LITTLESIS | investigation | 0 | keyword=Tokyo | DATA | 10 | 0 | 446 | reads ctx->entity; rt=littlesis-entity |

## `native/collectors/sources/google_dorking.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| google-dorking | cyber | 7200 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/gov_enforcement.c`  (6 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| CAA_ENFORCEMENT | government | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 1769 | reads ctx->entity |
| FSA_ENFORCEMENT | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 94 | reads ctx->entity |
| JFTC_ENFORCEMENT | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 2286 | reads ctx->entity |
| MHLW_LABOR_VIOLATIONS | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 905 | reads ctx->entity |
| MLIT_NEGATIVE_INFO | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 843 | reads ctx->entity |
| PMDA_APPROVALS | health | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 1938 | reads ctx->entity |

## `native/collectors/sources/gtfs_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| gtfs-jp | transport | 86400 | - | TIMEOUT | 0 | 0 | None | TIMED OUT |

## `native/collectors/sources/himawari_realtime.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| himawari-realtime | satellite | 600 | - | DATA | 1 | 0 | 1241 | NO GEO despite map-ish category; rt=himawari-realtime |

## `native/collectors/sources/infra_world.c`  (5 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BGPVIEW | cyber | 0 | ip=8.8.8.8 | EMPTY | 0 | 0 | 300 | reads ctx->entity |
| IPAPI_GLOBAL | cyber | 0 | ip=8.8.8.8 | DATA | 1 | 1 | 67 | reads ctx->entity; rt=infra-ip-geo |
| PEERINGDB_GLOBAL | cyber | 0 | ip=8.8.8.8 | EMPTY | 0 | 0 | 436 | reads ctx->entity |
| RIPESTAT_GLOBAL | cyber | 0 | ip=8.8.8.8 | DATA | 1 | 0 | 81 | reads ctx->entity; rt=infra-prefix |
| SHODAN_INTERNETDB | cyber | 0 | ip=8.8.8.8 | DATA | 1 | 0 | 61 | reads ctx->entity; rt=infra-host |

## `native/collectors/sources/instagram_locations.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| instagram-locations | social | 86400 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/intel_threat_world.c`  (28 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| bleepingcomputer | cyber | 3600 | - | DATA | 15 | 0 | 122 | rt=article |
| cert-eu | cyber | 3600 | - | DATA | 10 | 0 | 247 | rt=article |
| checkpoint-research | cyber | 3600 | - | DATA | 15 | 0 | 156 | rt=article |
| cisa-advisories | cyber | 3600 | - | RC_ERROR | 0 | 0 | 76 |  |
| cisa-news | cyber | 3600 | - | RC_ERROR | 0 | 0 | 79 |  |
| cisco-talos | cyber | 3600 | - | DATA | 15 | 0 | 166 | rt=article |
| cybersecurity-dive | cyber | 3600 | - | DATA | 10 | 0 | 414 | rt=article |
| dark-reading | cyber | 3600 | - | DATA | 50 | 0 | 459 | rt=article |
| dfir-report | cyber | 3600 | - | DATA | 10 | 0 | 150 | rt=article |
| google-security-blog | cyber | 3600 | - | DATA | 25 | 0 | 1473 | rt=article |
| graham-cluley | cyber | 3600 | - | DATA | 20 | 0 | 334 | rt=article |
| hacker-news-sec | cyber | 3600 | - | DATA | 50 | 0 | 308 | rt=article |
| intel471 | cyber | 3600 | - | DATA | 567 | 0 | 1054 | rt=article |
| krebs-on-security | cyber | 3600 | - | DATA | 10 | 0 | 119 | rt=article |
| malwarebytes-labs | cyber | 3600 | - | DATA | 20 | 0 | 142 | rt=article |
| msrc-blog | cyber | 3600 | - | EMPTY | 0 | 0 | 369 |  |
| ncsc-uk | cyber | 3600 | - | DATA | 20 | 0 | 145 | rt=article |
| project-zero | cyber | 3600 | - | DATA | 9 | 0 | 1677 | rt=article |
| rapid7-blog | cyber | 3600 | - | DATA | 20 | 0 | 375 | rt=article |
| recorded-future | cyber | 3600 | - | DATA | 50 | 0 | 228 | rt=article |
| schneier | cyber | 3600 | - | DATA | 10 | 0 | 111 | rt=article |
| securelist | cyber | 3600 | - | DATA | 10 | 0 | 596 | rt=article |
| securityweek | cyber | 3600 | - | DATA | 10 | 0 | 568 | rt=article |
| sophos-news | cyber | 3600 | - | RC_ERROR | 0 | 0 | 982 |  |
| the-record | cyber | 3600 | - | DATA | 5 | 0 | 303 | rt=article |
| troy-hunt | cyber | 3600 | - | DATA | 15 | 0 | 256 | rt=article |
| unit42 | cyber | 3600 | - | DATA | 15 | 0 | 127 | rt=article |
| welivesecurity | cyber | 3600 | - | DATA | 100 | 0 | 251 | rt=article |

## `native/collectors/sources/ipthreat_world2.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| ABUSEIPDB_CHECK | cyber | 0 | ip=8.8.8.8 | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| IPQUALITYSCORE_IP | cyber | 0 | ip=8.8.8.8 | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| PULSEDIVE_INDICATOR | cyber | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/jamstec_argo.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jamstec-argo | ocean | 86400 | - | NO_TITLE | 85 | 85 | 34130 | rt=jamstec-argo |

## `native/collectors/sources/jcg_navarea.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jcg-navarea | transport | 3600 | - | DATA | 1 | 0 | 8001 | NO GEO despite map-ish category; rt=(null) |

## `native/collectors/sources/jma_ocean_temp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-ocean-temp | environment | 86400 | - | EMPTY | 0 | 0 | 1013 |  |

## `native/collectors/sources/jma_warnings.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jma-warnings | safety | 300 | - | NO_TITLE | 16 | 16 | 30985 | rt=jma-warnings |

## `native/collectors/sources/jpo_jplatpat.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jpo-jplatpat | government | 86400 | - | DATA | 1 | 0 | 8004 | rt=(null) |

## `native/collectors/sources/jshis_seismic.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| jshis-seismic | safety | 86400 | - | RC_ERROR | 0 | 0 | 2107 |  |

## `native/collectors/sources/kyodo_rss.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| kyodo-rss | government | 1800 | - | RC_ERROR | 0 | 0 | 1189 |  |

## `native/collectors/sources/mac_vendor_lookup.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| MAC_VENDOR_LOOKUP | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 104 | reads ctx->entity |

## `native/collectors/sources/mastodon_jp_instances.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mastodon-jp-instances | social | 600 | - | NO_TITLE | 51 | 51 | 2445 | rt=mastodon-jp-instances,instance_error |

## `native/collectors/sources/media_world.c`  (4 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| COMMONCRAWL_CDX | news | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |
| GDELT_TV | news | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 11087 | reads ctx->entity |
| GOOGLE_BOOKS | news | 0 | keyword=Tokyo | WAF_BLOCKED | 0 | 0 | 1382 | reads ctx->entity |
| WIKINEWS | news | 0 | keyword=Tokyo | DATA | 17 | 0 | 316 | reads ctx->entity; rt=wikinews-article |

## `native/collectors/sources/mlit_bridge.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-bridge | infrastructure | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/mlit_p11_bus_stops.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| mlit-p11-bus-stops | transport | 2592000 | - | EMPTY | 0 | 0 | 1603 |  |

## `native/collectors/sources/my_jvn.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| my-jvn | cyber | 3600 | - | DB_ERROR | 50 | 0 | 3579 | db:OperationalError: Could not decode to UT; rt=article |

## `native/collectors/sources/netintel_world2.c`  (2 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GREYNOISE_COMMUNITY | cyber | 0 | ip=8.8.8.8 | EMPTY | 0 | 0 | 450 | reads ctx->entity |
| REDHAT_CVE | cyber | 0 | cve=CVE-2021-44228 | DATA | 1 | 0 | 358 | reads ctx->entity; rt=cve |

## `native/collectors/sources/news_aggregator.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| NEWS_AGGREGATOR | news | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 20256 | reads ctx->entity |

## `native/collectors/sources/ngo_rights_extra.c`  (13 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| access-now | news | 10800 | - | DATA | 15 | 0 | 1108 | rt=article |
| article19 | news | 10800 | - | DATA | 9 | 0 | 671 | rt=article |
| bellingcat-3 | news | 10800 | - | DATA | 10 | 0 | 228 | rt=article |
| citizen-lab | news | 10800 | - | DATA | 10 | 0 | 302 | rt=article |
| cpj | news | 10800 | - | DATA | 10 | 0 | 107 | rt=article |
| eff-deeplinks | news | 10800 | - | DATA | 50 | 0 | 224 | rt=article |
| forbidden-stories | news | 10800 | - | DATA | 10 | 0 | 1923 | rt=article |
| freedom-house | news | 10800 | - | DATA | 10 | 0 | 112 | rt=article |
| global-witness | news | 10800 | - | RC_ERROR | 0 | 0 | 262 |  |
| insight-crime | news | 10800 | - | DATA | 11 | 0 | 132 | rt=article |
| privacy-intl | news | 10800 | - | DATA | 10 | 0 | 221 | rt=article |
| rsf | news | 10800 | - | RC_ERROR | 0 | 0 | 118 |  |
| transparency-intl | news | 10800 | - | RC_ERROR | 0 | 0 | 94 |  |

## `native/collectors/sources/nict_atlas.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| nict-atlas | cyber | 3600 | - | DATA | 1 | 0 | 8005 | rt=(null) |

## `native/collectors/sources/npa_cyber_threat_obs.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| npa-cyber-threat-obs | cyber | 3600 | - | DATA | 2 | 1 | 610 | rt=npa-cyber-threat-obs,(null) |

## `native/collectors/sources/odpt_bus.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| odpt-bus | transport | 30 | - | KEY_GATED | 0 | 0 | 0 |  |

## `native/collectors/sources/opendata_socrata.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| OPENDATA_SOCRATA | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 3146 | reads ctx->entity |

## `native/collectors/sources/osint_thematic_global.c`  (20 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| acled-blog | news | 3600 | - | RC_ERROR | 0 | 0 | 824 |  |
| amnesty | news | 3600 | - | DATA | 12 | 0 | 230 | rt=article |
| argus-energy | news | 3600 | - | EMPTY | 0 | 0 | 1429 |  |
| avherald | news | 3600 | - | RC_ERROR | 0 | 0 | 175 |  |
| bellingcat-2 | news | 3600 | - | DATA | 10 | 0 | 123 | rt=article |
| cidrap | news | 3600 | - | DATA | 10 | 0 | 106 | rt=article |
| crisis-group | news | 3600 | - | DATA | 10 | 0 | 141 | rt=article |
| flightglobal | news | 3600 | - | DATA | 10 | 0 | 311 | rt=article |
| gcaptain | news | 3600 | - | DATA | 12 | 0 | 95 | rt=article |
| hrw-news | news | 3600 | - | DATA | 20 | 0 | 112 | rt=article |
| intelnews | news | 3600 | - | DATA | 30 | 0 | 177 | rt=article |
| lawfare | news | 3600 | - | RC_ERROR | 0 | 0 | 70 |  |
| maritime-bulletin | news | 3600 | - | RC_ERROR | 0 | 0 | 57 |  |
| nasaspaceflight | news | 3600 | - | DATA | 10 | 0 | 828 | rt=article |
| oilprice | news | 3600 | - | DATA | 15 | 0 | 874 | rt=article |
| rigzone | news | 3600 | - | DATA | 20 | 0 | 454 | rt=article |
| small-wars | news | 3600 | - | RC_ERROR | 0 | 0 | 479 |  |
| spacenews | news | 3600 | - | DATA | 24 | 0 | 100 | rt=article |
| splash247 | news | 3600 | - | DATA | 10 | 0 | 456 | rt=article |
| statnews | news | 3600 | - | DATA | 20 | 0 | 210 | rt=article |

## `native/collectors/sources/osm_transport_trains.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| osm-transport-trains | transport | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/pastebin_monitor.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| PASTE_SITE_SEARCH | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 8863 | reads ctx->entity |

## `native/collectors/sources/patents_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| EPO_OPS | government | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| LENS_PATENTS | government | 0 | keyword=Tokyo | KEY_GATED | 0 | 0 | 0 | reads ctx->entity |
| PATENTSVIEW | government | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 267 | reads ctx->entity |

## `native/collectors/sources/phishing_feeds_jp.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| phishing-feeds-jp | cyber | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/port_scanner.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| PORT_SCANNER | cyber | 0 | ip=8.8.8.8 | DATA | 2 | 0 | 20037 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/radar_sites.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| radar-sites | defense | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/reg_br_cnpj.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| BR_CNPJ | government | 0 | company=Toyota | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/reg_gr_gemi.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| GR_GEMI | government | 0 | company=Toyota | EMPTY | 0 | 0 | 101 | reads ctx->entity |

## `native/collectors/sources/reg_nz_companies.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| NZ_COMPANIES | government | 0 | company=Toyota | EMPTY | 0 | 0 | 2727 | reads ctx->entity |

## `native/collectors/sources/reg_us_states.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| US_STATES_SOS | government | 0 | company=Toyota | DATA | 3 | 0 | 35786 | reads ctx->entity; rt=us-company |

## `native/collectors/sources/reverse_dns.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| REVERSE_DNS | cyber | 0 | ip=8.8.8.8 | DATA | 1 | 0 | 8 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/sakura_front.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| sakura-front | wildlife | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/securitytrails_history.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| securitytrails-history | cyber | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/shodan_iot.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| shodan-iot | cyber | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/softbank_crowd.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| softbank-crowd | commercial | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/strava_heatmap_bases.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| strava-heatmap-bases | cyber | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/tdnet_disclosure.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tdnet-disclosure | government | 1800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/tepco_outage.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tepco-outage | infrastructure | 600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/threatfeeds_world.c`  (8 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| FEODO_GLOBAL | cyber | 3600 | - | (not swept) | - | - | - | reads ctx->entity |
| MALWAREBAZAAR | cyber | 3600 | - | (not swept) | - | - | - | reads ctx->entity |
| OPENPHISH | cyber | 3600 | - | (not swept) | - | - | - | reads ctx->entity |
| SPAMHAUS_DROP_GLOBAL | cyber | 3600 | - | (not swept) | - | - | - | reads ctx->entity |
| SSLBL_GLOBAL | cyber | 3600 | - | (not swept) | - | - | - | reads ctx->entity |
| THREATFOX_GLOBAL | cyber | 3600 | - | (not swept) | - | - | - | reads ctx->entity |
| TOR_EXITS_GLOBAL | cyber | 3600 | - | (not swept) | - | - | - | reads ctx->entity |
| URLHAUS_GLOBAL | cyber | 3600 | - | (not swept) | - | - | - | reads ctx->entity |

## `native/collectors/sources/tmp_protests.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tmp-protests | government | 21600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/tsr_closures.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| tsr-closures | government | 86400 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/unified_highway.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| unified-highway | transport | 600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/utility_poles.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| utility-poles | infrastructure | 604800 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/vnet.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| vnet | seismic | 600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/wayback_machine.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| WAYBACK_MACHINE | news | 0 | domain=github.com | DATA | 39 | 0 | 29688 | reads ctx->entity; rt=osint_service_result |

## `native/collectors/sources/wifi_lookup.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| WIFI_LOOKUP | cyber | 0 | keyword=Tokyo | EMPTY | 0 | 0 | 0 | reads ctx->entity |

## `native/collectors/sources/wikimedia_world.c`  (3 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| DBPEDIA | investigation | 0 | keyword=Tokyo | DATA | 15 | 0 | 324 | reads ctx->entity; rt=dbpedia-resource |
| WIKIDATA_SPARQL | investigation | 0 | vessel=9074729 | EMPTY | 0 | 0 | 265 | reads ctx->entity |
| WIKIMEDIA_COMMONS | investigation | 0 | keyword=Tokyo | DATA | 25 | 0 | 789 | reads ctx->entity; rt=commons-media |

## `native/collectors/sources/wineries_craftbeer.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| wineries-craftbeer | food | 2592000 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/world_cert_advisories.c`  (30 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| acsc-au | cyber | 3600 | - | (not swept) | - | - | - |  |
| cccs-ca | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-at | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-be | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-br | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-dk | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-ee | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-ee2 | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-es | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-fi | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-fr | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-gov-uk | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-in | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-it | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-lt | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-no | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-nz | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-pl | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-pt | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-se | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-si | cyber | 3600 | - | (not swept) | - | - | - |  |
| cert-ua | cyber | 3600 | - | (not swept) | - | - | - |  |
| circl-lu | cyber | 3600 | - | (not swept) | - | - | - |  |
| cisa-ics | cyber | 3600 | - | (not swept) | - | - | - |  |
| csa-sg | cyber | 3600 | - | (not swept) | - | - | - |  |
| jpcert-en | cyber | 3600 | - | (not swept) | - | - | - |  |
| krcert | cyber | 3600 | - | (not swept) | - | - | - |  |
| ncsc-ch | cyber | 3600 | - | (not swept) | - | - | - |  |
| ncsc-ie | cyber | 3600 | - | (not swept) | - | - | - |  |
| ncsc-nl | cyber | 3600 | - | (not swept) | - | - | - |  |

## `native/collectors/sources/world_reg_latam.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| LATAM_REGISTRY | government | 0 | company=Toyota | DATA | 1 | 0 | 57117 | reads ctx->entity; rt=latam-company |

## `native/collectors/sources/yahoo_news_jp_rss.c`  (1 sources)

| id | cat | iv | pivot | sweep | rows | geo | ms | note |
|---|---|---|---|---|---|---|---|---|
| yahoo-news-jp-rss | social | 1800 | - | (not swept) | - | - | - |  |
