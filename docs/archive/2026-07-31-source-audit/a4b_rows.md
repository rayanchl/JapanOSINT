| admin-boundaries | ENV_BLOCKED | 0 | 0 | gated by env | Overpass; all 4 endpoints IP-banned from this host |
| OPENAQ_GLOBAL | KEY_GATED | 0 | 0 | gated | no OPENAQ_KEY; v2 keyless fallback now 410 |
| OPENMETEO_AQ | DATA | 1 | 1 | real+complete | PM2.5/PM10/O3/NO2/SO2/CO + AQI, geo-pinned |
| WAQI_GLOBAL | KEY_GATED | 0 | 0 | gated | no AQICN_TOKEN |
| ASN_LOOKUP | DATA | 1 | 0 | real+complete | BGP/ASN + prefix; no geo (an ASN is not a place) |
| GBIF_OCCURRENCE | DATA | 30 | 30 | real+complete | 30 occurrences, all geo-pinned |
| INATURALIST | DATA | 30 | 30 | real+complete | 30 observations, all geo-pinned |
| OBIS_MARINE | DATA | 30 | 30 | real+complete | sweep used a vessel IMO as pivot; species name gives 30 geo rows |
| bird-makeup-jp | DEAD_UPSTREAM | 0 | 0 | dead upstream | 6 handles fetch 200 OK but every outbox is empty; rc fixed to 0 |
| BREACH_INDEX_EMAIL | DATA | 1 | 0 | real+complete | local 1,018-breach corpus, hit/count only |
| BREACH_INDEX_PASSWORD | DATA | 1 | 0 | real+complete | local corpus |
| BREACH_INDEX_PHONE | DATA | 1 | 0 | real+complete | local corpus |
| BREACH_INDEX_USERNAME | DATA | 1 | 0 | real+complete | local corpus |
| cam-camscape | DATA | 32 | 32 | real+thin (no link) | core/camera_store.c never sets it.link; camera page URL sits unused in props.url |
| cam-webcamera24 | DATA | 160 | 160 | real+thin (no link) | same core/camera_store.c bug; 160 cams |
| cell-towers | ENV_BLOCKED | 0 | 0 | gated by env | Overpass IP-ban |
| city-halls | ENV_BLOCKED | 0 | 0 | gated by env | Overpass IP-ban; rc=-1 quarantines |
| EMAILREP_LOOKUP | KEY_GATED | 0 | 0 | gated | no EMAILREP_KEY |
| NUMVERIFY_PHONE | KEY_GATED | 0 | 0 | gated | no NUMVERIFY_KEY |
| OPENCORPORATES | KEY_GATED | 0 | 0 | gated | no OPENCORPORATES_API_KEY |
| CH_OPENDATA | DATA | 20 | 0 | real+complete | 20 datasets with a local-language pivot |
| FI_OPENDATA | DATA | 20 | 0 | real+complete | 20 datasets with a local-language pivot |
| UK_DATAGOV | DATA | 10 | 0 | real+complete | 10 datasets |
| crtsh-historical | TIMEOUT | 0 | 0 | unverifiable | crt.sh never returned inside 280 s |
| data-go-jp-ckan | DATA | 10 | 0 | real+complete | 10 CKAN datasets, no geo (datasets) |
| docomo-population | KEY_GATED | 0 | 0 | gated | no DOCOMO_POPULATION_API_KEY |
| egov-laws | DATA | 200 | 0 | real+complete | 200 laws, title/link/date all present |
| estat-household | KEY_GATED | 0 | 0 | gated | no ESTAT_APP_ID |
| ferry-routes | ENV_BLOCKED | 0 | 0 | gated by env | Overpass IP-ban |
| bitcoin-mag | DATA | 10 | 0 | real+complete |  |
| cnbc-finance | DATA | 30 | 0 | real+complete |  |
| cnbc-top | DATA | 30 | 0 | real+complete |  |
| cnbc-world | DATA | 30 | 0 | real+complete |  |
| coindesk | DATA | 25 | 0 | real+complete |  |
| cointelegraph | DATA | 30 | 0 | real+complete |  |
| decrypt | DATA | 38 | 0 | real+complete |  |
| ft-home | DATA | 10 | 0 | real+complete |  |
| investing-news | DATA | 10 | 0 | real+complete |  |
| kitco-news | DEAD_UPSTREAM | 0 | 0 | dead upstream | every published Kitco RSS path 404s; left visible, not papered over |
| marketwatch-top | DATA | 10 | 0 | real+complete |  |
| oilprice-2 | DATA | 15 | 0 | real+complete |  |
| seeking-alpha | DATA | 7 | 0 | real+complete |  |
| the-block | DATA | 20 | 0 | real+complete |  |
| trading-econ | DATA | 100 | 0 | real+complete | rewritten onto ws/stream.ashx: 100 items with country/category/importance |
| zerohedge | DATA | 25 | 0 | real+complete |  |
| ALPHAVANTAGE_SEARCH | KEY_GATED | 0 | 0 | gated | no ALPHAVANTAGE_KEY |
| FINNHUB_SEARCH | KEY_GATED | 0 | 0 | gated | no FINNHUB_KEY |
| fofa-jp | KEY_GATED | 0 | 0 | gated | no FOFA_API_KEY |
| gcom-w | DATA | 50 | 0 | real+thin (no pin) | granule catalogue not measurements; newest granule 2025-08-31 (~11 mo stale) |
| bom-au-warnings | DEAD_UPSTREAM | 0 | 0 | dead upstream | bom.gov.au/rss/1.xml 404; only replacement is a licence-restricted private API |
| copernicus-ems | DATA | 10 | 0 | real+complete | URL repointed at mapping.emergency.copernicus.eu/latest/feed/ |
| emsc-quakes | DATA | 300 | 300 | real+complete | rewritten onto FDSN: 300 quakes, all with hypocentre |
| iceland-quakes | DATA | 300 | 300 | real+complete | rewritten onto FDSN Iceland AOI: 300 quakes with hypocentre |
| metoffice-warnings | EMPTY | 0 | 0 | real+complete | feed 200 OK with an empty channel — no active UK warnings |
| nhc-cpac | DATA | 2 | 0 | real+thin (no geo) | 2 items; NHC georss not parsed by lib/rss_atom.c |
| pdc-disasters | WAF_BLOCKED | 0 | 0 | WAF | pdc.org Cloudflare-challenges every path |
| volcanodiscovery | DATA | 20 | 0 | real+complete | URL repointed at /volcano-news.rss |
| gitlab-bitbucket-leaks | DATA | 2 | 0 | real+thin | 2 rows; GitLab/Bitbucket search is heavily rate-limited anonymously |
| RADIO_BROWSER | DATA | 11 | 0 | real+thin (no geo) | radio-browser publishes geo_lat/geo_long; not carried |
| gnews-mon-ae-red-sea-shipping | DATA | 66 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-airspace-violation | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-arms-shipment | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-assassination | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-aviation-incident | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-border-clash | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-br-amazon-deforestation | DATA | 67 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-cargo-ship-collision | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-catastrophic-flood | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-chemical-spill | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-civil-unrest | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-cn-cross-strait | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-cn-rare-earth | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-critical-infrastructure-attack | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-cyber-espionage | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-cyberattack | DATA | 102 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-data-breach | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-disease-outbreak | DATA | 99 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-disinformation-campaign | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-drone-strike | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-drug-cartel | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-economic-sanctions | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-election-interference | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-electronic-warfare | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-espionage-arrest | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-fr-coup-attempt | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-fr-sahel-militants | DATA | 62 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-gps-jamming | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-hostage-crisis | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-human-trafficking | DATA | 102 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-hypersonic-missile | DATA | 92 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-il-gaza | DATA | 101 | 0 | real+complete | DB_ERROR class: lib/rss_atom.c:172 cuts summary at 240 BYTES |
| gnews-mon-in-kashmir | DATA | 103 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-ir-nuclear-talks | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-ke-horn-of-africa | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-kr-north-korea-missile | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-lt-baltic-security | DATA | 45 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-major-earthquake | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-military-coup | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-military-exercise | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-missile-test | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-money-laundering | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-mx-cartel-violence | DATA | 73 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-naval-deployment | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-no-arctic-militarization | DATA | 81 | 0 | real+complete | query retranslated to Arktis militær — 0 -> 81 items |
| gnews-mon-nuclear-plant-incident | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-nuclear-weapon | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-oil-tanker-seizure | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-ph-south-china-sea | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-pipeline-explosion | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-port-disruption | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-power-grid-failure | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-ransomware | DATA | 102 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-ru-wagner | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-satellite-launch | DATA | 102 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-semiconductor-export-controls | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-shadow-fleet | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-state-sponsored-hacking | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-supply-chain-disruption | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-terror-attack | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-troop-movement | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-tw-pla-incursion | DATA | 100 | 0 | real+complete | query retranslated to 解放軍 擾台 — 0 -> 100 items |
| gnews-mon-ua-front-line | DATA | 84 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-undersea-cable | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-us-strait-of-hormuz | DATA | 107 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-us-taiwan-strait | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-volcano-eruption | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-weapons-smuggling | DATA | 100 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-wildfire | DATA | 110 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| gnews-mon-zero-day-exploit | DATA | 102 | 0 | real+complete | Google News RSS, article rows; no geo (news) |
| ADVISORY_COUNCILS | DEAD_UPSTREAM | 0 | 0 | real+complete |  |
| BUSINESS_LICENSES | WAF_BLOCKED | 0 | 0 | real+complete |  |
| EGOV_PUBLIC_COMMENT | WAF_BLOCKED | 0 | 0 | real+complete |  |
| WASTE_HAULERS | WAF_BLOCKED | 0 | 0 | real+complete |  |
| grayhat-buckets | KEY_GATED | 0 | 0 | gated | no GRAYHAT_API_KEY |
| hatena-bookmark-extended | DATA | 200 | 0 | real+complete | 199 bookmarks |
| houjin-bangou | KEY_GATED | 0 | 0 | gated | no HOUJIN_BANGOU_KEY |
| IOC_LOOKUP | DATA | 1 | 0 | real+complete | rc bug fixed: a HIT used to return rc=1 and quarantine the source |
| japan-reit | DATA | 57 | 0 | real+complete | /meigara/ retired; scrape moved to /list/rimawari/, 57 issues (was 3) |
| jma-earthquake | DATA | 446 | 439 | real+complete | per-eid dedupe + depth/max-intensity/link; geo 398 -> 439 |
| jma-tide | DATA | 166 | 166 | real+complete | link + measurement summary added; 166/166 pinned |
| job-boards | ENV_BLOCKED | 0 | 0 | gated by env | Overpass IP-ban; NO_TITLE + order-dependent uid already fixed in code |
| jr-boarding-stats | DATA | 2 | 0 | labels-only | reachability probe, not boarding numbers; reachable=false |
| kafun-pollen | DATA | 1 | 0 | labels-only | reachability probe, no pollen concentrations |
| BANKRUPTCY_SEARCH | DATA | 15 | 0 | real+complete | CourtListener; rc fixed from 15 to 0 |
| CRIMINAL_RECORDS | DATA | 15 | 0 | real+complete | CourtListener; rc fixed |
| LEGAL_SEARCH | DATA | 15 | 0 | real+complete | CourtListener; rc fixed |
| LICENSE_PLATE_LOOKUP | KEY_GATED | 0 | 0 | gated | no lawful public API; now logs its gate |
| mapfan-api | KEY_GATED | 0 | 0 | gated | no MAPFAN_API_KEY |
| mhlw-health | DATA | 1 | 0 | labels-only | portal row; its own body admits there is no machine API |
| mlit-landprice | KEY_GATED | 0 | 0 | dead upstream | land.mlit.go.jp/webland/api refuses connections; reinfolib needs a key |
| mlit-transaction | KEY_GATED | 0 | 0 | dead upstream | same retired WebLand API |
| navitime-api | KEY_GATED | 0 | 0 | gated | no NAVITIME_API_KEY |
| nexco-roadwork | DATA | 3 | 0 | labels-only | 3 reachability rows, no roadwork records |
| nitter-mirrors | DATA | 3 | 0 | labels-only | 3 mirror-reachability rows, no posts |
| npa-special-fraud | DATA | 26 | 25 | real+thin (pin is NPA HQ) | 25 monthly national totals all pinned at NPA headquarters |
| odpt-train | KEY_GATED | 0 | 0 | gated | no ODPT token |
| ARCHIVE_ORG_SEARCH | DATA | 20 | 0 | real+complete | 20 items |
| GITHUB_COMMITS_BY_EMAIL | DATA | 20 | 0 | real+complete | 20 commits |
| MUSICBRAINZ | DATA | 20 | 0 | real+complete | 20 artists |
| PHOTON_GEOCODE | DATA | 9 | 9 | real+complete | 9 results, all geo |
| TOR_ONIONOO | DATA | 1 | 0 | real+thin | 1 relay |
| BIGDATACLOUD_REVERSE | DATA | 1 | 1 | real+complete | needs a "lat,lon" pivot; sweep used a keyword |
| HN_USER | DATA | 1 | 0 | real+complete | needs an HN username pivot |
| OPENVERSE | DATA | 20 | 0 | real+complete | 20 images |
| SEC_FULLTEXT | DATA | 20 | 0 | real+complete | 20 filings |
| STACKEXCHANGE_SEARCH | DATA | 20 | 0 | real+complete | 20 questions |
| WIKIDATA_ENTITY_SEARCH | DATA | 20 | 0 | real+complete | 20 entities |
| osm-changesets-jp | DATA | 100 | 100 | real+complete | uid was the OSM USER id (100 emitted -> 28 stored); title added; Tokyo fallback removed |
| overpass-subway-tracks | ENV_BLOCKED | 0 | 0 | gated by env | Overpass IP-ban |
| peeringdb-jp | DATA | 27 | 0 | real+complete | ~1.1k rows; every IXP and ASN used to be pinned at Tokyo station — removed |
| PEP_CHECK | KEY_GATED | 0 | 0 | gated | no OPENSANCTIONS_API_KEY; rc bug fixed, gate now logged |
| WATCHLIST_CHECK_NEW | KEY_GATED | 0 | 0 | gated | no OPENSANCTIONS_API_KEY; rc bug fixed, gate now logged |
| CARRIER_LOOKUP | DATA | 1 | 0 | real+thin | libphonenumber-style metadata only |
| PHONE_LOOKUP | DATA | 1 | 0 | real+thin | rc fixed from 1 to 0 |
| PHONE_REPUTATION | DATA | 1 | 0 | real+thin | rc fixed from 1 to 0 |
| poc-in-github | DATA | 94 | 0 | real+complete | 94 PoC repos; 94 fake Tokyo pins removed, link/date/uid added |
| psbdmp-pastes | DATA | 1 | 0 | labels-only | reachability probe, no pastes |
| reddit-jp-subs | WAF_BLOCKED | 0 | 0 | WAF | reddit.com/*.json returns 403 to this host |
| DK_CVR | DATA | 1 | 0 | real+thin | 1 company |
| HR_SUDREG | DEAD_UPSTREAM | 0 | 0 | dead upstream | endpoint 404 |
| HU_COMPANIES | EMPTY | 0 | 0 | honest empty | fetch OK, no match for the pivot |
| LT_REGISTRAI | DEAD_UPSTREAM | 0 | 0 | dead upstream | endpoint 404 |
| LU_LBR | WAF_BLOCKED | 0 | 0 | WAF | HTTP 429 |
| NL_KVK | KEY_GATED | 0 | 0 | gated | no KVK_API_KEY |
| PT_RCBE | DEAD_UPSTREAM | 0 | 0 | dead upstream | endpoint 404 |
| RS_APR | WAF_BLOCKED | 0 | 0 | WAF | connection refused / status 0 |
| IL_COMPANIES | DATA | 2 | 0 | real+complete | 2 companies |
| SEA2_REGISTRY | WAF_BLOCKED | 0 | 0 | WAF | 6 of 6 SEA registries 404/refused, 73 s wall clock |
| reinfolib | KEY_GATED | 0 | 0 | gated | no subscription key |
| rice-paddies | ENV_BLOCKED | 0 | 0 | gated by env | Overpass IP-ban |
| satellite-ground-stations | ENV_BLOCKED | 0 | 0 | gated by env | Overpass IP-ban |
| sento-public-baths | ENV_BLOCKED | 0 | 0 | gated by env | Overpass IP-ban |
| shrine-temple | ENV_BLOCKED | 0 | 0 | gated by env | Overpass IP-ban |
| SSL_ANALYZER | DATA | 101 | 0 | real+complete | 101 rows (cert chain + SANs); 59 s -> duration_outlier |
| submarine-cables | ENV_BLOCKED | 0 | 0 | gated by env | Overpass IP-ban |
| teikoku-failures | DATA | 1 | 0 | labels-only | reachability probe, no bankruptcy records |
| THREAT_FEED_LOOKUP | EMPTY | 0 | 0 | honest empty | 8.8.8.8 is genuinely clean in OTX/URLhaus/ThreatFox |
| tor-exit-nodes | DATA | 5 | 0 | real+complete | geo guard was dropping the whole feed; onionoo no longer publishes lat/lon |
| unesco-heritage | WAF_BLOCKED | 0 | 0 | WAF | whc.unesco.org/en/list/xml/ Cloudflare-challenged; the RSS that works has no coords |
| unified-trains | ENV_BLOCKED | 0 | 0 | gated by env | Overpass sub-collector + MLIT N02 both empty |
| vessel-finder | KEY_GATED | 0 | 0 | gated | no VESSELFINDER_API_KEY; rc fixed from -1 to 0 |
| wanted-persons | DEAD_UPSTREAM | 0 | 0 | dead upstream | NPA shimeitehai index 404s; extraction method is unsound anyway |
| WHALE_ALERT | DATA | 1 | 0 | real+thin | 1 row |
| wifi-networks-shodan | KEY_GATED | 0 | 0 | gated | no SHODAN_API_KEY |
| AFRICA_REGISTRY | DATA | 1 | 0 | real+thin | 1 company, 15.8 s |
| xrain-radar | DATA | 1 | 0 | labels-only | reachability probe, no rainfall values |
