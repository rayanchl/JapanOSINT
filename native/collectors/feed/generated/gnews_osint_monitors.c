/* Google News OSINT keyword monitors — real search-feed RSS tracking high-yield security/conflict/infrastructure/disaster terms globally and per hotspot region. */
#include "source.h"
#include "lib/rss_atom.h"

/* AUDIT NOTE (2026-08-09) — why the interval is ~7200 and not 3600/1800.
 *
 * Every source in gnews_world_top.c (142), gnews_world_topics_1.c (218),
 * gnews_world_topics_2.c (185) and gnews_osint_monitors.c (68) hits ONE host,
 * news.google.com. At the previous intervals (3600 s for the three country
 * files, 1800 s for the monitors) the scheduler wanted 681 requests per hour
 * from one IP — one every 5.3 s.
 *
 * reddit_world_geo.c is the written post-mortem of exactly this failure on a
 * different host: 142 feeds at 1800 s (one per 12.7 s) had 135 of them return
 * rc=-1 with zero rows, because rss_collect() maps any non-2xx — including a
 * transient 429 — to -1, which anomaly_detect turns into status_bad and
 * quarantines. Google News is served through the same RSSX body at ~2.4x that
 * request rate, and throttling is host-correlated, so the whole ~24% of the
 * fleet that lives on news.google.com trips together on one trigger.
 *
 * 7200 s puts the cohort at ~11.7 s average spacing (306 req/h, less than half
 * the old rate). The interval is additionally staggered by one second per
 * source (7200 + cohort index, so 7200..7812): identical intervals preserve
 * whatever burst the boot ramp created forever, whereas a 1 s spread de-phases
 * the 613 sources across a full window inside a day and keeps them spread.
 *
 * As in the reddit case this is a mitigation, not a cure. The cure is a shared
 * per-host rate limiter (core/hostgate.h exists but is not consulted here) and
 * a distinct "throttled" outcome in lib/rss_atom.c so a 429 stops counting as
 * a collector failure. Both live in shared code and are reported, not patched
 * here.
 */

/* SYM, id, name, name_ja, collector, category, url, lang, tags_json, interval, description */
#include "_source_macros.inc"

RSSX(gn_mon_cyberattack, "gnews-mon-cyberattack", "OSINT Monitor: cyberattack", "OSINT モニター: cyberattack", "osint", "news",
  "https://news.google.com/rss/search?q=cyberattack&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"cyberattack\"]", 7745,
  "Global Google News monitor tracking 'cyberattack' — real-time OSINT alerting feed");

RSSX(gn_mon_data_breach, "gnews-mon-data-breach", "OSINT Monitor: data breach", "OSINT モニター: data breach", "osint", "news",
  "https://news.google.com/rss/search?q=data%20breach&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"data\"]", 7746,
  "Global Google News monitor tracking 'data breach' — real-time OSINT alerting feed");

RSSX(gn_mon_ransomware, "gnews-mon-ransomware", "OSINT Monitor: ransomware", "OSINT モニター: ransomware", "osint", "news",
  "https://news.google.com/rss/search?q=ransomware&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"ransomware\"]", 7747,
  "Global Google News monitor tracking 'ransomware' — real-time OSINT alerting feed");

RSSX(gn_mon_zero_day_exploit, "gnews-mon-zero-day-exploit", "OSINT Monitor: zero-day exploit", "OSINT モニター: zero-day exploit", "osint", "news",
  "https://news.google.com/rss/search?q=zero-day%20exploit&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"zero-day\"]", 7748,
  "Global Google News monitor tracking 'zero-day exploit' — real-time OSINT alerting feed");

RSSX(gn_mon_critical_infrastructure_attack, "gnews-mon-critical-infrastructure-attack", "OSINT Monitor: critical infrastructure attack", "OSINT モニター: critical infrastructure attack", "osint", "news",
  "https://news.google.com/rss/search?q=critical%20infrastructure%20attack&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"critical\"]", 7749,
  "Global Google News monitor tracking 'critical infrastructure attack' — real-time OSINT alerting feed");

RSSX(gn_mon_power_grid_failure, "gnews-mon-power-grid-failure", "OSINT Monitor: power grid failure", "OSINT モニター: power grid failure", "osint", "news",
  "https://news.google.com/rss/search?q=power%20grid%20failure&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"power\"]", 7750,
  "Global Google News monitor tracking 'power grid failure' — real-time OSINT alerting feed");

RSSX(gn_mon_undersea_cable, "gnews-mon-undersea-cable", "OSINT Monitor: undersea cable", "OSINT モニター: undersea cable", "osint", "news",
  "https://news.google.com/rss/search?q=undersea%20cable&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"undersea\"]", 7751,
  "Global Google News monitor tracking 'undersea cable' — real-time OSINT alerting feed");

RSSX(gn_mon_gps_jamming, "gnews-mon-gps-jamming", "OSINT Monitor: GPS jamming", "OSINT モニター: GPS jamming", "osint", "news",
  "https://news.google.com/rss/search?q=GPS%20jamming&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"gps\"]", 7752,
  "Global Google News monitor tracking 'GPS jamming' — real-time OSINT alerting feed");

RSSX(gn_mon_electronic_warfare, "gnews-mon-electronic-warfare", "OSINT Monitor: electronic warfare", "OSINT モニター: electronic warfare", "osint", "news",
  "https://news.google.com/rss/search?q=electronic%20warfare&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"electronic\"]", 7753,
  "Global Google News monitor tracking 'electronic warfare' — real-time OSINT alerting feed");

RSSX(gn_mon_military_exercise, "gnews-mon-military-exercise", "OSINT Monitor: military exercise", "OSINT モニター: military exercise", "osint", "news",
  "https://news.google.com/rss/search?q=military%20exercise&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"military\"]", 7754,
  "Global Google News monitor tracking 'military exercise' — real-time OSINT alerting feed");

RSSX(gn_mon_missile_test, "gnews-mon-missile-test", "OSINT Monitor: missile test", "OSINT モニター: missile test", "osint", "news",
  "https://news.google.com/rss/search?q=missile%20test&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"missile\"]", 7755,
  "Global Google News monitor tracking 'missile test' — real-time OSINT alerting feed");

RSSX(gn_mon_nuclear_weapon, "gnews-mon-nuclear-weapon", "OSINT Monitor: nuclear weapon", "OSINT モニター: nuclear weapon", "osint", "news",
  "https://news.google.com/rss/search?q=nuclear%20weapon&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"nuclear\"]", 7756,
  "Global Google News monitor tracking 'nuclear weapon' — real-time OSINT alerting feed");

RSSX(gn_mon_troop_movement, "gnews-mon-troop-movement", "OSINT Monitor: troop movement", "OSINT モニター: troop movement", "osint", "news",
  "https://news.google.com/rss/search?q=troop%20movement&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"troop\"]", 7757,
  "Global Google News monitor tracking 'troop movement' — real-time OSINT alerting feed");

RSSX(gn_mon_border_clash, "gnews-mon-border-clash", "OSINT Monitor: border clash", "OSINT モニター: border clash", "osint", "news",
  "https://news.google.com/rss/search?q=border%20clash&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"border\"]", 7758,
  "Global Google News monitor tracking 'border clash' — real-time OSINT alerting feed");

RSSX(gn_mon_drone_strike, "gnews-mon-drone-strike", "OSINT Monitor: drone strike", "OSINT モニター: drone strike", "osint", "news",
  "https://news.google.com/rss/search?q=drone%20strike&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"drone\"]", 7759,
  "Global Google News monitor tracking 'drone strike' — real-time OSINT alerting feed");

RSSX(gn_mon_airspace_violation, "gnews-mon-airspace-violation", "OSINT Monitor: airspace violation", "OSINT モニター: airspace violation", "osint", "news",
  "https://news.google.com/rss/search?q=airspace%20violation&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"airspace\"]", 7760,
  "Global Google News monitor tracking 'airspace violation' — real-time OSINT alerting feed");

RSSX(gn_mon_naval_deployment, "gnews-mon-naval-deployment", "OSINT Monitor: naval deployment", "OSINT モニター: naval deployment", "osint", "news",
  "https://news.google.com/rss/search?q=naval%20deployment&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"naval\"]", 7761,
  "Global Google News monitor tracking 'naval deployment' — real-time OSINT alerting feed");

RSSX(gn_mon_military_coup, "gnews-mon-military-coup", "OSINT Monitor: military coup", "OSINT モニター: military coup", "osint", "news",
  "https://news.google.com/rss/search?q=military%20coup&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"military\"]", 7762,
  "Global Google News monitor tracking 'military coup' — real-time OSINT alerting feed");

RSSX(gn_mon_civil_unrest, "gnews-mon-civil-unrest", "OSINT Monitor: civil unrest", "OSINT モニター: civil unrest", "osint", "news",
  "https://news.google.com/rss/search?q=civil%20unrest&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"civil\"]", 7763,
  "Global Google News monitor tracking 'civil unrest' — real-time OSINT alerting feed");

RSSX(gn_mon_economic_sanctions, "gnews-mon-economic-sanctions", "OSINT Monitor: economic sanctions", "OSINT モニター: economic sanctions", "osint", "news",
  "https://news.google.com/rss/search?q=economic%20sanctions&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"economic\"]", 7764,
  "Global Google News monitor tracking 'economic sanctions' — real-time OSINT alerting feed");

RSSX(gn_mon_arms_shipment, "gnews-mon-arms-shipment", "OSINT Monitor: arms shipment", "OSINT モニター: arms shipment", "osint", "news",
  "https://news.google.com/rss/search?q=arms%20shipment&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"arms\"]", 7765,
  "Global Google News monitor tracking 'arms shipment' — real-time OSINT alerting feed");

RSSX(gn_mon_weapons_smuggling, "gnews-mon-weapons-smuggling", "OSINT Monitor: weapons smuggling", "OSINT モニター: weapons smuggling", "osint", "news",
  "https://news.google.com/rss/search?q=weapons%20smuggling&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"weapons\"]", 7766,
  "Global Google News monitor tracking 'weapons smuggling' — real-time OSINT alerting feed");

RSSX(gn_mon_money_laundering, "gnews-mon-money-laundering", "OSINT Monitor: money laundering", "OSINT モニター: money laundering", "osint", "news",
  "https://news.google.com/rss/search?q=money%20laundering&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"money\"]", 7767,
  "Global Google News monitor tracking 'money laundering' — real-time OSINT alerting feed");

RSSX(gn_mon_oil_tanker_seizure, "gnews-mon-oil-tanker-seizure", "OSINT Monitor: oil tanker seizure", "OSINT モニター: oil tanker seizure", "osint", "news",
  "https://news.google.com/rss/search?q=oil%20tanker%20seizure&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"oil\"]", 7768,
  "Global Google News monitor tracking 'oil tanker seizure' — real-time OSINT alerting feed");

RSSX(gn_mon_shadow_fleet, "gnews-mon-shadow-fleet", "OSINT Monitor: shadow fleet", "OSINT モニター: shadow fleet", "osint", "news",
  "https://news.google.com/rss/search?q=shadow%20fleet&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"shadow\"]", 7769,
  "Global Google News monitor tracking 'shadow fleet' — real-time OSINT alerting feed");

RSSX(gn_mon_port_disruption, "gnews-mon-port-disruption", "OSINT Monitor: port disruption", "OSINT モニター: port disruption", "osint", "news",
  "https://news.google.com/rss/search?q=port%20disruption&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"port\"]", 7770,
  "Global Google News monitor tracking 'port disruption' — real-time OSINT alerting feed");

RSSX(gn_mon_supply_chain_disruption, "gnews-mon-supply-chain-disruption", "OSINT Monitor: supply chain disruption", "OSINT モニター: supply chain disruption", "osint", "news",
  "https://news.google.com/rss/search?q=supply%20chain%20disruption&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"supply\"]", 7771,
  "Global Google News monitor tracking 'supply chain disruption' — real-time OSINT alerting feed");

RSSX(gn_mon_semiconductor_export_controls, "gnews-mon-semiconductor-export-controls", "OSINT Monitor: semiconductor export controls", "OSINT モニター: semiconductor export controls", "osint", "news",
  "https://news.google.com/rss/search?q=semiconductor%20export%20controls&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"semiconductor\"]", 7772,
  "Global Google News monitor tracking 'semiconductor export controls' — real-time OSINT alerting feed");

RSSX(gn_mon_espionage_arrest, "gnews-mon-espionage-arrest", "OSINT Monitor: espionage arrest", "OSINT モニター: espionage arrest", "osint", "news",
  "https://news.google.com/rss/search?q=espionage%20arrest&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"espionage\"]", 7773,
  "Global Google News monitor tracking 'espionage arrest' — real-time OSINT alerting feed");

RSSX(gn_mon_assassination, "gnews-mon-assassination", "OSINT Monitor: assassination", "OSINT モニター: assassination", "osint", "news",
  "https://news.google.com/rss/search?q=assassination&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"assassination\"]", 7774,
  "Global Google News monitor tracking 'assassination' — real-time OSINT alerting feed");

RSSX(gn_mon_terror_attack, "gnews-mon-terror-attack", "OSINT Monitor: terror attack", "OSINT モニター: terror attack", "osint", "news",
  "https://news.google.com/rss/search?q=terror%20attack&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"terror\"]", 7775,
  "Global Google News monitor tracking 'terror attack' — real-time OSINT alerting feed");

RSSX(gn_mon_hostage_crisis, "gnews-mon-hostage-crisis", "OSINT Monitor: hostage crisis", "OSINT モニター: hostage crisis", "osint", "news",
  "https://news.google.com/rss/search?q=hostage%20crisis&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"hostage\"]", 7776,
  "Global Google News monitor tracking 'hostage crisis' — real-time OSINT alerting feed");

RSSX(gn_mon_drug_cartel, "gnews-mon-drug-cartel", "OSINT Monitor: drug cartel", "OSINT モニター: drug cartel", "osint", "news",
  "https://news.google.com/rss/search?q=drug%20cartel&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"drug\"]", 7777,
  "Global Google News monitor tracking 'drug cartel' — real-time OSINT alerting feed");

RSSX(gn_mon_human_trafficking, "gnews-mon-human-trafficking", "OSINT Monitor: human trafficking", "OSINT モニター: human trafficking", "osint", "news",
  "https://news.google.com/rss/search?q=human%20trafficking&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"human\"]", 7778,
  "Global Google News monitor tracking 'human trafficking' — real-time OSINT alerting feed");

RSSX(gn_mon_wildfire, "gnews-mon-wildfire", "OSINT Monitor: wildfire", "OSINT モニター: wildfire", "osint", "news",
  "https://news.google.com/rss/search?q=wildfire&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"wildfire\"]", 7779,
  "Global Google News monitor tracking 'wildfire' — real-time OSINT alerting feed");

RSSX(gn_mon_major_earthquake, "gnews-mon-major-earthquake", "OSINT Monitor: major earthquake", "OSINT モニター: major earthquake", "osint", "news",
  "https://news.google.com/rss/search?q=major%20earthquake&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"major\"]", 7780,
  "Global Google News monitor tracking 'major earthquake' — real-time OSINT alerting feed");

RSSX(gn_mon_catastrophic_flood, "gnews-mon-catastrophic-flood", "OSINT Monitor: catastrophic flood", "OSINT モニター: catastrophic flood", "osint", "news",
  "https://news.google.com/rss/search?q=catastrophic%20flood&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"catastrophic\"]", 7781,
  "Global Google News monitor tracking 'catastrophic flood' — real-time OSINT alerting feed");

RSSX(gn_mon_volcano_eruption, "gnews-mon-volcano-eruption", "OSINT Monitor: volcano eruption", "OSINT モニター: volcano eruption", "osint", "news",
  "https://news.google.com/rss/search?q=volcano%20eruption&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"volcano\"]", 7782,
  "Global Google News monitor tracking 'volcano eruption' — real-time OSINT alerting feed");

RSSX(gn_mon_chemical_spill, "gnews-mon-chemical-spill", "OSINT Monitor: chemical spill", "OSINT モニター: chemical spill", "osint", "news",
  "https://news.google.com/rss/search?q=chemical%20spill&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"chemical\"]", 7783,
  "Global Google News monitor tracking 'chemical spill' — real-time OSINT alerting feed");

RSSX(gn_mon_nuclear_plant_incident, "gnews-mon-nuclear-plant-incident", "OSINT Monitor: nuclear plant incident", "OSINT モニター: nuclear plant incident", "osint", "news",
  "https://news.google.com/rss/search?q=nuclear%20plant%20incident&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"nuclear\"]", 7784,
  "Global Google News monitor tracking 'nuclear plant incident' — real-time OSINT alerting feed");

RSSX(gn_mon_pipeline_explosion, "gnews-mon-pipeline-explosion", "OSINT Monitor: pipeline explosion", "OSINT モニター: pipeline explosion", "osint", "news",
  "https://news.google.com/rss/search?q=pipeline%20explosion&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"pipeline\"]", 7785,
  "Global Google News monitor tracking 'pipeline explosion' — real-time OSINT alerting feed");

RSSX(gn_mon_aviation_incident, "gnews-mon-aviation-incident", "OSINT Monitor: aviation incident", "OSINT モニター: aviation incident", "osint", "news",
  "https://news.google.com/rss/search?q=aviation%20incident&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"aviation\"]", 7786,
  "Global Google News monitor tracking 'aviation incident' — real-time OSINT alerting feed");

RSSX(gn_mon_cargo_ship_collision, "gnews-mon-cargo-ship-collision", "OSINT Monitor: cargo ship collision", "OSINT モニター: cargo ship collision", "osint", "news",
  "https://news.google.com/rss/search?q=cargo%20ship%20collision&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"cargo\"]", 7787,
  "Global Google News monitor tracking 'cargo ship collision' — real-time OSINT alerting feed");

RSSX(gn_mon_disease_outbreak, "gnews-mon-disease-outbreak", "OSINT Monitor: disease outbreak", "OSINT モニター: disease outbreak", "osint", "news",
  "https://news.google.com/rss/search?q=disease%20outbreak&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"disease\"]", 7788,
  "Global Google News monitor tracking 'disease outbreak' — real-time OSINT alerting feed");

RSSX(gn_mon_election_interference, "gnews-mon-election-interference", "OSINT Monitor: election interference", "OSINT モニター: election interference", "osint", "news",
  "https://news.google.com/rss/search?q=election%20interference&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"election\"]", 7789,
  "Global Google News monitor tracking 'election interference' — real-time OSINT alerting feed");

RSSX(gn_mon_disinformation_campaign, "gnews-mon-disinformation-campaign", "OSINT Monitor: disinformation campaign", "OSINT モニター: disinformation campaign", "osint", "news",
  "https://news.google.com/rss/search?q=disinformation%20campaign&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"disinformation\"]", 7790,
  "Global Google News monitor tracking 'disinformation campaign' — real-time OSINT alerting feed");

RSSX(gn_mon_state_sponsored_hacking, "gnews-mon-state-sponsored-hacking", "OSINT Monitor: state-sponsored hacking", "OSINT モニター: state-sponsored hacking", "osint", "news",
  "https://news.google.com/rss/search?q=state-sponsored%20hacking&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"state-sponsored\"]", 7791,
  "Global Google News monitor tracking 'state-sponsored hacking' — real-time OSINT alerting feed");

RSSX(gn_mon_satellite_launch, "gnews-mon-satellite-launch", "OSINT Monitor: satellite launch", "OSINT モニター: satellite launch", "osint", "news",
  "https://news.google.com/rss/search?q=satellite%20launch&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"satellite\"]", 7792,
  "Global Google News monitor tracking 'satellite launch' — real-time OSINT alerting feed");

RSSX(gn_mon_hypersonic_missile, "gnews-mon-hypersonic-missile", "OSINT Monitor: hypersonic missile", "OSINT モニター: hypersonic missile", "osint", "news",
  "https://news.google.com/rss/search?q=hypersonic%20missile&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"hypersonic\"]", 7793,
  "Global Google News monitor tracking 'hypersonic missile' — real-time OSINT alerting feed");

RSSX(gn_mon_cyber_espionage, "gnews-mon-cyber-espionage", "OSINT Monitor: cyber espionage", "OSINT モニター: cyber espionage", "osint", "news",
  "https://news.google.com/rss/search?q=cyber%20espionage&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"alert\",\"cyber\"]", 7794,
  "Global Google News monitor tracking 'cyber espionage' — real-time OSINT alerting feed");

RSSX(gn_mon_ua_front_line, "gnews-mon-ua-front-line", "OSINT Monitor (UA): front line", "OSINT モニター (UA): front line", "osint", "news",
  "https://news.google.com/rss/search?q=front%20line&hl=uk&gl=UA&ceid=UA:uk", "uk", "[\"osint\",\"monitor\",\"regional\",\"ua\"]", 7795,
  "Regional Google News monitor (UA) tracking 'front line' — hotspot OSINT feed");

RSSX(gn_mon_il_gaza, "gnews-mon-il-gaza", "OSINT Monitor (IL): Gaza", "OSINT モニター (IL): Gaza", "osint", "news",
  "https://news.google.com/rss/search?q=Gaza&hl=iw&gl=IL&ceid=IL:iw", "iw", "[\"osint\",\"monitor\",\"regional\",\"il\"]", 7796,
  "Regional Google News monitor (IL) tracking 'Gaza' — hotspot OSINT feed");

RSSX(gn_mon_tw_pla_incursion, "gnews-mon-tw-pla-incursion", "OSINT Monitor (TW): PLA incursion", "OSINT モニター (TW): PLA incursion", "osint", "news",
  /* AUDIT: the English term "PLA incursion" returns 0 items from the zh-TW
   * locale. 解放軍 擾台 is the term Taiwanese media actually use (100 items). */
  "https://news.google.com/rss/search?q=%E8%A7%A3%E6%94%BE%E8%BB%8D%20%E6%93%BE%E5%8F%B0&hl=zh-TW&gl=TW&ceid=TW:zh", "zh", "[\"osint\",\"monitor\",\"regional\",\"tw\"]", 1800,
  "Regional Google News monitor (TW) tracking 'PLA incursion' — hotspot OSINT feed");

RSSX(gn_mon_ph_south_china_sea, "gnews-mon-ph-south-china-sea", "OSINT Monitor (PH): South China Sea", "OSINT モニター (PH): South China Sea", "osint", "news",
  "https://news.google.com/rss/search?q=South%20China%20Sea&hl=en-PH&gl=PH&ceid=PH:en", "en", "[\"osint\",\"monitor\",\"regional\",\"ph\"]", 7797,
  "Regional Google News monitor (PH) tracking 'South China Sea' — hotspot OSINT feed");

RSSX(gn_mon_fr_sahel_militants, "gnews-mon-fr-sahel-militants", "OSINT Monitor (FR): Sahel militants", "OSINT モニター (FR): Sahel militants", "osint", "news",
  "https://news.google.com/rss/search?q=Sahel%20militants&hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"osint\",\"monitor\",\"regional\",\"fr\"]", 7798,
  "Regional Google News monitor (FR) tracking 'Sahel militants' — hotspot OSINT feed");

RSSX(gn_mon_ae_red_sea_shipping, "gnews-mon-ae-red-sea-shipping", "OSINT Monitor (AE): Red Sea shipping", "OSINT モニター (AE): Red Sea shipping", "osint", "news",
  "https://news.google.com/rss/search?q=Red%20Sea%20shipping&hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"osint\",\"monitor\",\"regional\",\"ae\"]", 7799,
  "Regional Google News monitor (AE) tracking 'Red Sea shipping' — hotspot OSINT feed");

RSSX(gn_mon_kr_north_korea_missile, "gnews-mon-kr-north-korea-missile", "OSINT Monitor (KR): North Korea missile", "OSINT モニター (KR): North Korea missile", "osint", "news",
  "https://news.google.com/rss/search?q=North%20Korea%20missile&hl=ko&gl=KR&ceid=KR:ko", "ko", "[\"osint\",\"monitor\",\"regional\",\"kr\"]", 7800,
  "Regional Google News monitor (KR) tracking 'North Korea missile' — hotspot OSINT feed");

RSSX(gn_mon_us_taiwan_strait, "gnews-mon-us-taiwan-strait", "OSINT Monitor (US): Taiwan Strait", "OSINT モニター (US): Taiwan Strait", "osint", "news",
  "https://news.google.com/rss/search?q=Taiwan%20Strait&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"regional\",\"us\"]", 7801,
  "Regional Google News monitor (US) tracking 'Taiwan Strait' — hotspot OSINT feed");

RSSX(gn_mon_ru_wagner, "gnews-mon-ru-wagner", "OSINT Monitor (RU): Wagner", "OSINT モニター (RU): Wagner", "osint", "news",
  "https://news.google.com/rss/search?q=Wagner&hl=ru&gl=RU&ceid=RU:ru", "ru", "[\"osint\",\"monitor\",\"regional\",\"ru\"]", 7802,
  "Regional Google News monitor (RU) tracking 'Wagner' — hotspot OSINT feed");

RSSX(gn_mon_in_kashmir, "gnews-mon-in-kashmir", "OSINT Monitor (IN): Kashmir", "OSINT モニター (IN): Kashmir", "osint", "news",
  "https://news.google.com/rss/search?q=Kashmir&hl=en-IN&gl=IN&ceid=IN:en", "en", "[\"osint\",\"monitor\",\"regional\",\"in\"]", 7803,
  "Regional Google News monitor (IN) tracking 'Kashmir' — hotspot OSINT feed");

RSSX(gn_mon_mx_cartel_violence, "gnews-mon-mx-cartel-violence", "OSINT Monitor (MX): cartel violence", "OSINT モニター (MX): cartel violence", "osint", "news",
  "https://news.google.com/rss/search?q=cartel%20violence&hl=es-419&gl=MX&ceid=MX:es", "es", "[\"osint\",\"monitor\",\"regional\",\"mx\"]", 7804,
  "Regional Google News monitor (MX) tracking 'cartel violence' — hotspot OSINT feed");

RSSX(gn_mon_br_amazon_deforestation, "gnews-mon-br-amazon-deforestation", "OSINT Monitor (BR): Amazon deforestation", "OSINT モニター (BR): Amazon deforestation", "osint", "news",
  "https://news.google.com/rss/search?q=Amazon%20deforestation&hl=pt-BR&gl=BR&ceid=BR:pt", "pt", "[\"osint\",\"monitor\",\"regional\",\"br\"]", 7805,
  "Regional Google News monitor (BR) tracking 'Amazon deforestation' — hotspot OSINT feed");

RSSX(gn_mon_us_strait_of_hormuz, "gnews-mon-us-strait-of-hormuz", "OSINT Monitor (US): Strait of Hormuz", "OSINT モニター (US): Strait of Hormuz", "osint", "news",
  "https://news.google.com/rss/search?q=Strait%20of%20Hormuz&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"regional\",\"us\"]", 7806,
  "Regional Google News monitor (US) tracking 'Strait of Hormuz' — hotspot OSINT feed");

RSSX(gn_mon_lt_baltic_security, "gnews-mon-lt-baltic-security", "OSINT Monitor (LT): Baltic security", "OSINT モニター (LT): Baltic security", "osint", "news",
  "https://news.google.com/rss/search?q=Baltic%20security&hl=lt&gl=LT&ceid=LT:lt", "lt", "[\"osint\",\"monitor\",\"regional\",\"lt\"]", 7807,
  "Regional Google News monitor (LT) tracking 'Baltic security' — hotspot OSINT feed");

RSSX(gn_mon_no_arctic_militarization, "gnews-mon-no-arctic-militarization", "OSINT Monitor (NO): Arctic militarization", "OSINT モニター (NO): Arctic militarization", "osint", "news",
  /* AUDIT: "Arctic militarization" returns 0 items from the no-NO locale;
   * the Norwegian term "Arktis militær" returns ~80. */
  "https://news.google.com/rss/search?q=Arktis%20milit%C3%A6r&hl=no&gl=NO&ceid=NO:no", "no", "[\"osint\",\"monitor\",\"regional\",\"no\"]", 1800,
  "Regional Google News monitor (NO) tracking 'Arctic militarization' — hotspot OSINT feed");

RSSX(gn_mon_ke_horn_of_africa, "gnews-mon-ke-horn-of-africa", "OSINT Monitor (KE): Horn of Africa", "OSINT モニター (KE): Horn of Africa", "osint", "news",
  "https://news.google.com/rss/search?q=Horn%20of%20Africa&hl=en-KE&gl=KE&ceid=KE:en", "en", "[\"osint\",\"monitor\",\"regional\",\"ke\"]", 7808,
  "Regional Google News monitor (KE) tracking 'Horn of Africa' — hotspot OSINT feed");

RSSX(gn_mon_cn_cross_strait, "gnews-mon-cn-cross-strait", "OSINT Monitor (CN): cross-strait", "OSINT モニター (CN): cross-strait", "osint", "news",
  "https://news.google.com/rss/search?q=cross-strait&hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"osint\",\"monitor\",\"regional\",\"cn\"]", 7809,
  "Regional Google News monitor (CN) tracking 'cross-strait' — hotspot OSINT feed");

RSSX(gn_mon_ir_nuclear_talks, "gnews-mon-ir-nuclear-talks", "OSINT Monitor (IR): nuclear talks", "OSINT モニター (IR): nuclear talks", "osint", "news",
  "https://news.google.com/rss/search?q=nuclear%20talks&hl=fa&gl=IR&ceid=IR:fa", "fa", "[\"osint\",\"monitor\",\"regional\",\"ir\"]", 7810,
  "Regional Google News monitor (IR) tracking 'nuclear talks' — hotspot OSINT feed");

RSSX(gn_mon_fr_coup_attempt, "gnews-mon-fr-coup-attempt", "OSINT Monitor (FR): coup attempt", "OSINT モニター (FR): coup attempt", "osint", "news",
  "https://news.google.com/rss/search?q=coup%20attempt&hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"osint\",\"monitor\",\"regional\",\"fr\"]", 7811,
  "Regional Google News monitor (FR) tracking 'coup attempt' — hotspot OSINT feed");

RSSX(gn_mon_cn_rare_earth, "gnews-mon-cn-rare-earth", "OSINT Monitor (CN): rare earth", "OSINT モニター (CN): rare earth", "osint", "news",
  "https://news.google.com/rss/search?q=rare%20earth&hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"osint\",\"monitor\",\"regional\",\"cn\"]", 7812,
  "Regional Google News monitor (CN) tracking 'rare earth' — hotspot OSINT feed");
