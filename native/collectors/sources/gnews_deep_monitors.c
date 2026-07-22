/* Deep Google News OSINT keyword monitors (real search RSS) — breach/leak/dark-web, critical-infrastructure, sanctions-evasion and conflict-hotspot tracking. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

#define RSSX(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, LANG, TAGS, IVAL, DESC)  \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                 \
    int n = rss_collect(c, s, URL, LANG, TAGS); return n < 0 ? -1 : 0; }     \
  static const source_def SYM = {                                           \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,            \
    .update_interval_sec = IVAL, .run = run_##SYM,                           \
    .category = CAT, .type = "web_request", .url = URL,                      \
    .description = DESC, .layer = NULL, .free_tier = 1 };                    \
  REGISTER_SOURCE(SYM)

RSSX(dm_data_leak, "gnews-deep-data-leak", "OSINT Deep Monitor: data leak", "OSINT モニター: data leak", "osint", "news",
  "https://news.google.com/rss/search?q=data%20leak&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"data\"]", 1800,
  "Global deep OSINT monitor tracking 'data leak' — high-penetrancy alerting feed");

RSSX(dm_database_exposed_online, "gnews-deep-database-exposed-online", "OSINT Deep Monitor: database exposed online", "OSINT モニター: database exposed online", "osint", "news",
  "https://news.google.com/rss/search?q=database%20exposed%20online&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"database\"]", 1800,
  "Global deep OSINT monitor tracking 'database exposed online' — high-penetrancy alerting feed");

RSSX(dm_credential_dump, "gnews-deep-credential-dump", "OSINT Deep Monitor: credential dump", "OSINT モニター: credential dump", "osint", "news",
  "https://news.google.com/rss/search?q=credential%20dump&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"credential\"]", 1800,
  "Global deep OSINT monitor tracking 'credential dump' — high-penetrancy alerting feed");

RSSX(dm_dark_web_marketplace, "gnews-deep-dark-web-marketplace", "OSINT Deep Monitor: dark web marketplace", "OSINT モニター: dark web marketplace", "osint", "news",
  "https://news.google.com/rss/search?q=dark%20web%20marketplace&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"dark\"]", 1800,
  "Global deep OSINT monitor tracking 'dark web marketplace' — high-penetrancy alerting feed");

RSSX(dm_ransomware_gang, "gnews-deep-ransomware-gang", "OSINT Deep Monitor: ransomware gang", "OSINT モニター: ransomware gang", "osint", "news",
  "https://news.google.com/rss/search?q=ransomware%20gang&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"ransomware\"]", 1800,
  "Global deep OSINT monitor tracking 'ransomware gang' — high-penetrancy alerting feed");

RSSX(dm_double_extortion_ransomware, "gnews-deep-double-extortion-ransomware", "OSINT Deep Monitor: double extortion ransomware", "OSINT モニター: double extortion ransomware", "osint", "news",
  "https://news.google.com/rss/search?q=double%20extortion%20ransomware&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"double\"]", 1800,
  "Global deep OSINT monitor tracking 'double extortion ransomware' — high-penetrancy alerting feed");

RSSX(dm_initial_access_broker, "gnews-deep-initial-access-broker", "OSINT Deep Monitor: initial access broker", "OSINT モニター: initial access broker", "osint", "news",
  "https://news.google.com/rss/search?q=initial%20access%20broker&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"initial\"]", 1800,
  "Global deep OSINT monitor tracking 'initial access broker' — high-penetrancy alerting feed");

RSSX(dm_zero_day_exploit_sold, "gnews-deep-zero-day-exploit-sold", "OSINT Deep Monitor: zero-day exploit sold", "OSINT モニター: zero-day exploit sold", "osint", "news",
  "https://news.google.com/rss/search?q=zero-day%20exploit%20sold&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"zero-day\"]", 1800,
  "Global deep OSINT monitor tracking 'zero-day exploit sold' — high-penetrancy alerting feed");

RSSX(dm_scada_vulnerability, "gnews-deep-scada-vulnerability", "OSINT Deep Monitor: SCADA vulnerability", "OSINT モニター: SCADA vulnerability", "osint", "news",
  "https://news.google.com/rss/search?q=SCADA%20vulnerability&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"scada\"]", 1800,
  "Global deep OSINT monitor tracking 'SCADA vulnerability' — high-penetrancy alerting feed");

RSSX(dm_ics_cyberattack, "gnews-deep-ics-cyberattack", "OSINT Deep Monitor: ICS cyberattack", "OSINT モニター: ICS cyberattack", "osint", "news",
  "https://news.google.com/rss/search?q=ICS%20cyberattack&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"ics\"]", 1800,
  "Global deep OSINT monitor tracking 'ICS cyberattack' — high-penetrancy alerting feed");

RSSX(dm_ot_security_incident, "gnews-deep-ot-security-incident", "OSINT Deep Monitor: OT security incident", "OSINT モニター: OT security incident", "osint", "news",
  "https://news.google.com/rss/search?q=OT%20security%20incident&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"ot\"]", 1800,
  "Global deep OSINT monitor tracking 'OT security incident' — high-penetrancy alerting feed");

RSSX(dm_critical_infrastructure_breach, "gnews-deep-critical-infrastructure-breach", "OSINT Deep Monitor: critical infrastructure breach", "OSINT モニター: critical infrastructure breach", "osint", "news",
  "https://news.google.com/rss/search?q=critical%20infrastructure%20breach&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"critical\"]", 1800,
  "Global deep OSINT monitor tracking 'critical infrastructure breach' — high-penetrancy alerting feed");

RSSX(dm_software_supply_chain_compromise, "gnews-deep-software-supply-chain-compromise", "OSINT Deep Monitor: software supply chain compromise", "OSINT モニター: software supply chain compromise", "osint", "news",
  "https://news.google.com/rss/search?q=software%20supply%20chain%20compromise&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"software\"]", 1800,
  "Global deep OSINT monitor tracking 'software supply chain compromise' — high-penetrancy alerting feed");

RSSX(dm_malicious_npm_package, "gnews-deep-malicious-npm-package", "OSINT Deep Monitor: malicious npm package", "OSINT モニター: malicious npm package", "osint", "news",
  "https://news.google.com/rss/search?q=malicious%20npm%20package&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"malicious\"]", 1800,
  "Global deep OSINT monitor tracking 'malicious npm package' — high-penetrancy alerting feed");

RSSX(dm_malicious_pypi_package, "gnews-deep-malicious-pypi-package", "OSINT Deep Monitor: malicious PyPI package", "OSINT モニター: malicious PyPI package", "osint", "news",
  "https://news.google.com/rss/search?q=malicious%20PyPI%20package&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"malicious\"]", 1800,
  "Global deep OSINT monitor tracking 'malicious PyPI package' — high-penetrancy alerting feed");

RSSX(dm_state_sponsored_cyberattack, "gnews-deep-state-sponsored-cyberattack", "OSINT Deep Monitor: state-sponsored cyberattack", "OSINT モニター: state-sponsored cyberattack", "osint", "news",
  "https://news.google.com/rss/search?q=state-sponsored%20cyberattack&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"state-sponsored\"]", 1800,
  "Global deep OSINT monitor tracking 'state-sponsored cyberattack' — high-penetrancy alerting feed");

RSSX(dm_apt_campaign, "gnews-deep-apt-campaign", "OSINT Deep Monitor: APT campaign", "OSINT モニター: APT campaign", "osint", "news",
  "https://news.google.com/rss/search?q=APT%20campaign&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"apt\"]", 1800,
  "Global deep OSINT monitor tracking 'APT campaign' — high-penetrancy alerting feed");

RSSX(dm_pegasus_spyware, "gnews-deep-pegasus-spyware", "OSINT Deep Monitor: Pegasus spyware", "OSINT モニター: Pegasus spyware", "osint", "news",
  "https://news.google.com/rss/search?q=Pegasus%20spyware&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"pegasus\"]", 1800,
  "Global deep OSINT monitor tracking 'Pegasus spyware' — high-penetrancy alerting feed");

RSSX(dm_mercenary_spyware, "gnews-deep-mercenary-spyware", "OSINT Deep Monitor: mercenary spyware", "OSINT モニター: mercenary spyware", "osint", "news",
  "https://news.google.com/rss/search?q=mercenary%20spyware&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"mercenary\"]", 1800,
  "Global deep OSINT monitor tracking 'mercenary spyware' — high-penetrancy alerting feed");

RSSX(dm_zero_click_exploit, "gnews-deep-zero-click-exploit", "OSINT Deep Monitor: zero-click exploit", "OSINT モニター: zero-click exploit", "osint", "news",
  "https://news.google.com/rss/search?q=zero-click%20exploit&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"zero-click\"]", 1800,
  "Global deep OSINT monitor tracking 'zero-click exploit' — high-penetrancy alerting feed");

RSSX(dm_sim_swap_attack, "gnews-deep-sim-swap-attack", "OSINT Deep Monitor: SIM swap attack", "OSINT モニター: SIM swap attack", "osint", "news",
  "https://news.google.com/rss/search?q=SIM%20swap%20attack&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"sim\"]", 1800,
  "Global deep OSINT monitor tracking 'SIM swap attack' — high-penetrancy alerting feed");

RSSX(dm_bgp_hijack, "gnews-deep-bgp-hijack", "OSINT Deep Monitor: BGP hijack", "OSINT モニター: BGP hijack", "osint", "news",
  "https://news.google.com/rss/search?q=BGP%20hijack&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"bgp\"]", 1800,
  "Global deep OSINT monitor tracking 'BGP hijack' — high-penetrancy alerting feed");

RSSX(dm_dns_hijacking, "gnews-deep-dns-hijacking", "OSINT Deep Monitor: DNS hijacking", "OSINT モニター: DNS hijacking", "osint", "news",
  "https://news.google.com/rss/search?q=DNS%20hijacking&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"dns\"]", 1800,
  "Global deep OSINT monitor tracking 'DNS hijacking' — high-penetrancy alerting feed");

RSSX(dm_certificate_misissuance, "gnews-deep-certificate-misissuance", "OSINT Deep Monitor: certificate misissuance", "OSINT モニター: certificate misissuance", "osint", "news",
  "https://news.google.com/rss/search?q=certificate%20misissuance&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"certificate\"]", 1800,
  "Global deep OSINT monitor tracking 'certificate misissuance' — high-penetrancy alerting feed");

RSSX(dm_undersea_cable_cut, "gnews-deep-undersea-cable-cut", "OSINT Deep Monitor: undersea cable cut", "OSINT モニター: undersea cable cut", "osint", "news",
  "https://news.google.com/rss/search?q=undersea%20cable%20cut&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"undersea\"]", 1800,
  "Global deep OSINT monitor tracking 'undersea cable cut' — high-penetrancy alerting feed");

RSSX(dm_gps_spoofing, "gnews-deep-gps-spoofing", "OSINT Deep Monitor: GPS spoofing", "OSINT モニター: GPS spoofing", "osint", "news",
  "https://news.google.com/rss/search?q=GPS%20spoofing&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"gps\"]", 1800,
  "Global deep OSINT monitor tracking 'GPS spoofing' — high-penetrancy alerting feed");

RSSX(dm_gps_jamming_aircraft, "gnews-deep-gps-jamming-aircraft", "OSINT Deep Monitor: GPS jamming aircraft", "OSINT モニター: GPS jamming aircraft", "osint", "news",
  "https://news.google.com/rss/search?q=GPS%20jamming%20aircraft&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"gps\"]", 1800,
  "Global deep OSINT monitor tracking 'GPS jamming aircraft' — high-penetrancy alerting feed");

RSSX(dm_drone_incursion_airport, "gnews-deep-drone-incursion-airport", "OSINT Deep Monitor: drone incursion airport", "OSINT モニター: drone incursion airport", "osint", "news",
  "https://news.google.com/rss/search?q=drone%20incursion%20airport&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"drone\"]", 1800,
  "Global deep OSINT monitor tracking 'drone incursion airport' — high-penetrancy alerting feed");

RSSX(dm_airspace_closure, "gnews-deep-airspace-closure", "OSINT Deep Monitor: airspace closure", "OSINT モニター: airspace closure", "osint", "news",
  "https://news.google.com/rss/search?q=airspace%20closure&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"airspace\"]", 1800,
  "Global deep OSINT monitor tracking 'airspace closure' — high-penetrancy alerting feed");

RSSX(dm_military_convoy_sighted, "gnews-deep-military-convoy-sighted", "OSINT Deep Monitor: military convoy sighted", "OSINT モニター: military convoy sighted", "osint", "news",
  "https://news.google.com/rss/search?q=military%20convoy%20sighted&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"military\"]", 1800,
  "Global deep OSINT monitor tracking 'military convoy sighted' — high-penetrancy alerting feed");

RSSX(dm_troop_buildup_satellite, "gnews-deep-troop-buildup-satellite", "OSINT Deep Monitor: troop buildup satellite", "OSINT モニター: troop buildup satellite", "osint", "news",
  "https://news.google.com/rss/search?q=troop%20buildup%20satellite&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"troop\"]", 1800,
  "Global deep OSINT monitor tracking 'troop buildup satellite' — high-penetrancy alerting feed");

RSSX(dm_naval_blockade, "gnews-deep-naval-blockade", "OSINT Deep Monitor: naval blockade", "OSINT モニター: naval blockade", "osint", "news",
  "https://news.google.com/rss/search?q=naval%20blockade&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"naval\"]", 1800,
  "Global deep OSINT monitor tracking 'naval blockade' — high-penetrancy alerting feed");

RSSX(dm_arms_embargo_violation, "gnews-deep-arms-embargo-violation", "OSINT Deep Monitor: arms embargo violation", "OSINT モニター: arms embargo violation", "osint", "news",
  "https://news.google.com/rss/search?q=arms%20embargo%20violation&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"arms\"]", 1800,
  "Global deep OSINT monitor tracking 'arms embargo violation' — high-penetrancy alerting feed");

RSSX(dm_sanctions_evasion_network, "gnews-deep-sanctions-evasion-network", "OSINT Deep Monitor: sanctions evasion network", "OSINT モニター: sanctions evasion network", "osint", "news",
  "https://news.google.com/rss/search?q=sanctions%20evasion%20network&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"sanctions\"]", 1800,
  "Global deep OSINT monitor tracking 'sanctions evasion network' — high-penetrancy alerting feed");

RSSX(dm_shadow_fleet_tanker, "gnews-deep-shadow-fleet-tanker", "OSINT Deep Monitor: shadow fleet tanker", "OSINT モニター: shadow fleet tanker", "osint", "news",
  "https://news.google.com/rss/search?q=shadow%20fleet%20tanker&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"shadow\"]", 1800,
  "Global deep OSINT monitor tracking 'shadow fleet tanker' — high-penetrancy alerting feed");

RSSX(dm_ais_spoofing_vessel, "gnews-deep-ais-spoofing-vessel", "OSINT Deep Monitor: AIS spoofing vessel", "OSINT モニター: AIS spoofing vessel", "osint", "news",
  "https://news.google.com/rss/search?q=AIS%20spoofing%20vessel&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"ais\"]", 1800,
  "Global deep OSINT monitor tracking 'AIS spoofing vessel' — high-penetrancy alerting feed");

RSSX(dm_ship_to_ship_transfer_sanctions, "gnews-deep-ship-to-ship-transfer-sanctions", "OSINT Deep Monitor: ship-to-ship transfer sanctions", "OSINT モニター: ship-to-ship transfer sanctions", "osint", "news",
  "https://news.google.com/rss/search?q=ship-to-ship%20transfer%20sanctions&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"ship-to-ship\"]", 1800,
  "Global deep OSINT monitor tracking 'ship-to-ship transfer sanctions' — high-penetrancy alerting feed");

RSSX(dm_oil_smuggling_network, "gnews-deep-oil-smuggling-network", "OSINT Deep Monitor: oil smuggling network", "OSINT モニター: oil smuggling network", "osint", "news",
  "https://news.google.com/rss/search?q=oil%20smuggling%20network&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"oil\"]", 1800,
  "Global deep OSINT monitor tracking 'oil smuggling network' — high-penetrancy alerting feed");

RSSX(dm_gold_smuggling, "gnews-deep-gold-smuggling", "OSINT Deep Monitor: gold smuggling", "OSINT モニター: gold smuggling", "osint", "news",
  "https://news.google.com/rss/search?q=gold%20smuggling&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"gold\"]", 1800,
  "Global deep OSINT monitor tracking 'gold smuggling' — high-penetrancy alerting feed");

RSSX(dm_wildlife_trafficking_network, "gnews-deep-wildlife-trafficking-network", "OSINT Deep Monitor: wildlife trafficking network", "OSINT モニター: wildlife trafficking network", "osint", "news",
  "https://news.google.com/rss/search?q=wildlife%20trafficking%20network&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"wildlife\"]", 1800,
  "Global deep OSINT monitor tracking 'wildlife trafficking network' — high-penetrancy alerting feed");

RSSX(dm_money_laundering_scheme, "gnews-deep-money-laundering-scheme", "OSINT Deep Monitor: money laundering scheme", "OSINT モニター: money laundering scheme", "osint", "news",
  "https://news.google.com/rss/search?q=money%20laundering%20scheme&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"money\"]", 1800,
  "Global deep OSINT monitor tracking 'money laundering scheme' — high-penetrancy alerting feed");

RSSX(dm_shell_company_network, "gnews-deep-shell-company-network", "OSINT Deep Monitor: shell company network", "OSINT モニター: shell company network", "osint", "news",
  "https://news.google.com/rss/search?q=shell%20company%20network&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"shell\"]", 1800,
  "Global deep OSINT monitor tracking 'shell company network' — high-penetrancy alerting feed");

RSSX(dm_beneficial_ownership_leak, "gnews-deep-beneficial-ownership-leak", "OSINT Deep Monitor: beneficial ownership leak", "OSINT モニター: beneficial ownership leak", "osint", "news",
  "https://news.google.com/rss/search?q=beneficial%20ownership%20leak&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"beneficial\"]", 1800,
  "Global deep OSINT monitor tracking 'beneficial ownership leak' — high-penetrancy alerting feed");

RSSX(dm_offshore_leak, "gnews-deep-offshore-leak", "OSINT Deep Monitor: offshore leak", "OSINT モニター: offshore leak", "osint", "news",
  "https://news.google.com/rss/search?q=offshore%20leak&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"offshore\"]", 1800,
  "Global deep OSINT monitor tracking 'offshore leak' — high-penetrancy alerting feed");

RSSX(dm_corruption_investigation, "gnews-deep-corruption-investigation", "OSINT Deep Monitor: corruption investigation", "OSINT モニター: corruption investigation", "osint", "news",
  "https://news.google.com/rss/search?q=corruption%20investigation&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"corruption\"]", 1800,
  "Global deep OSINT monitor tracking 'corruption investigation' — high-penetrancy alerting feed");

RSSX(dm_bribery_scandal, "gnews-deep-bribery-scandal", "OSINT Deep Monitor: bribery scandal", "OSINT モニター: bribery scandal", "osint", "news",
  "https://news.google.com/rss/search?q=bribery%20scandal&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"bribery\"]", 1800,
  "Global deep OSINT monitor tracking 'bribery scandal' — high-penetrancy alerting feed");

RSSX(dm_interpol_red_notice, "gnews-deep-interpol-red-notice", "OSINT Deep Monitor: Interpol red notice", "OSINT モニター: Interpol red notice", "osint", "news",
  "https://news.google.com/rss/search?q=Interpol%20red%20notice&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"interpol\"]", 1800,
  "Global deep OSINT monitor tracking 'Interpol red notice' — high-penetrancy alerting feed");

RSSX(dm_extradition_request, "gnews-deep-extradition-request", "OSINT Deep Monitor: extradition request", "OSINT モニター: extradition request", "osint", "news",
  "https://news.google.com/rss/search?q=extradition%20request&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"extradition\"]", 1800,
  "Global deep OSINT monitor tracking 'extradition request' — high-penetrancy alerting feed");

RSSX(dm_assassination_plot, "gnews-deep-assassination-plot", "OSINT Deep Monitor: assassination plot", "OSINT モニター: assassination plot", "osint", "news",
  "https://news.google.com/rss/search?q=assassination%20plot&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"assassination\"]", 1800,
  "Global deep OSINT monitor tracking 'assassination plot' — high-penetrancy alerting feed");

RSSX(dm_coup_plot_foiled, "gnews-deep-coup-plot-foiled", "OSINT Deep Monitor: coup plot foiled", "OSINT モニター: coup plot foiled", "osint", "news",
  "https://news.google.com/rss/search?q=coup%20plot%20foiled&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"coup\"]", 1800,
  "Global deep OSINT monitor tracking 'coup plot foiled' — high-penetrancy alerting feed");

RSSX(dm_private_military_company, "gnews-deep-private-military-company", "OSINT Deep Monitor: private military company", "OSINT モニター: private military company", "osint", "news",
  "https://news.google.com/rss/search?q=private%20military%20company&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"private\"]", 1800,
  "Global deep OSINT monitor tracking 'private military company' — high-penetrancy alerting feed");

RSSX(dm_disinformation_campaign_exposed, "gnews-deep-disinformation-campaign-exposed", "OSINT Deep Monitor: disinformation campaign exposed", "OSINT モニター: disinformation campaign exposed", "osint", "news",
  "https://news.google.com/rss/search?q=disinformation%20campaign%20exposed&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"disinformation\"]", 1800,
  "Global deep OSINT monitor tracking 'disinformation campaign exposed' — high-penetrancy alerting feed");

RSSX(dm_influence_operation, "gnews-deep-influence-operation", "OSINT Deep Monitor: influence operation", "OSINT モニター: influence operation", "osint", "news",
  "https://news.google.com/rss/search?q=influence%20operation&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"influence\"]", 1800,
  "Global deep OSINT monitor tracking 'influence operation' — high-penetrancy alerting feed");

RSSX(dm_coordinated_inauthentic_behavior, "gnews-deep-coordinated-inauthentic-behavior", "OSINT Deep Monitor: coordinated inauthentic behavior", "OSINT モニター: coordinated inauthentic behavior", "osint", "news",
  "https://news.google.com/rss/search?q=coordinated%20inauthentic%20behavior&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"coordinated\"]", 1800,
  "Global deep OSINT monitor tracking 'coordinated inauthentic behavior' — high-penetrancy alerting feed");

RSSX(dm_deepfake_video, "gnews-deep-deepfake-video", "OSINT Deep Monitor: deepfake video", "OSINT モニター: deepfake video", "osint", "news",
  "https://news.google.com/rss/search?q=deepfake%20video&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"deepfake\"]", 1800,
  "Global deep OSINT monitor tracking 'deepfake video' — high-penetrancy alerting feed");

RSSX(dm_election_system_breach, "gnews-deep-election-system-breach", "OSINT Deep Monitor: election system breach", "OSINT モニター: election system breach", "osint", "news",
  "https://news.google.com/rss/search?q=election%20system%20breach&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"election\"]", 1800,
  "Global deep OSINT monitor tracking 'election system breach' — high-penetrancy alerting feed");

RSSX(dm_voter_data_exposed, "gnews-deep-voter-data-exposed", "OSINT Deep Monitor: voter data exposed", "OSINT モニター: voter data exposed", "osint", "news",
  "https://news.google.com/rss/search?q=voter%20data%20exposed&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"voter\"]", 1800,
  "Global deep OSINT monitor tracking 'voter data exposed' — high-penetrancy alerting feed");

RSSX(dm_hospital_ransomware_attack, "gnews-deep-hospital-ransomware-attack", "OSINT Deep Monitor: hospital ransomware attack", "OSINT モニター: hospital ransomware attack", "osint", "news",
  "https://news.google.com/rss/search?q=hospital%20ransomware%20attack&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"hospital\"]", 1800,
  "Global deep OSINT monitor tracking 'hospital ransomware attack' — high-penetrancy alerting feed");

RSSX(dm_pipeline_cyberattack, "gnews-deep-pipeline-cyberattack", "OSINT Deep Monitor: pipeline cyberattack", "OSINT モニター: pipeline cyberattack", "osint", "news",
  "https://news.google.com/rss/search?q=pipeline%20cyberattack&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"pipeline\"]", 1800,
  "Global deep OSINT monitor tracking 'pipeline cyberattack' — high-penetrancy alerting feed");

RSSX(dm_water_utility_hacked, "gnews-deep-water-utility-hacked", "OSINT Deep Monitor: water utility hacked", "OSINT モニター: water utility hacked", "osint", "news",
  "https://news.google.com/rss/search?q=water%20utility%20hacked&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"water\"]", 1800,
  "Global deep OSINT monitor tracking 'water utility hacked' — high-penetrancy alerting feed");

RSSX(dm_port_cyberattack, "gnews-deep-port-cyberattack", "OSINT Deep Monitor: port cyberattack", "OSINT モニター: port cyberattack", "osint", "news",
  "https://news.google.com/rss/search?q=port%20cyberattack&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"port\"]", 1800,
  "Global deep OSINT monitor tracking 'port cyberattack' — high-penetrancy alerting feed");

RSSX(dm_airport_systems_disruption, "gnews-deep-airport-systems-disruption", "OSINT Deep Monitor: airport systems disruption", "OSINT モニター: airport systems disruption", "osint", "news",
  "https://news.google.com/rss/search?q=airport%20systems%20disruption&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"airport\"]", 1800,
  "Global deep OSINT monitor tracking 'airport systems disruption' — high-penetrancy alerting feed");

RSSX(dm_rail_signalling_failure, "gnews-deep-rail-signalling-failure", "OSINT Deep Monitor: rail signalling failure", "OSINT モニター: rail signalling failure", "osint", "news",
  "https://news.google.com/rss/search?q=rail%20signalling%20failure&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"rail\"]", 1800,
  "Global deep OSINT monitor tracking 'rail signalling failure' — high-penetrancy alerting feed");

RSSX(dm_telecom_network_outage, "gnews-deep-telecom-network-outage", "OSINT Deep Monitor: telecom network outage", "OSINT モニター: telecom network outage", "osint", "news",
  "https://news.google.com/rss/search?q=telecom%20network%20outage&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"telecom\"]", 1800,
  "Global deep OSINT monitor tracking 'telecom network outage' — high-penetrancy alerting feed");

RSSX(dm_national_internet_shutdown, "gnews-deep-national-internet-shutdown", "OSINT Deep Monitor: national internet shutdown", "OSINT モニター: national internet shutdown", "osint", "news",
  "https://news.google.com/rss/search?q=national%20internet%20shutdown&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"national\"]", 1800,
  "Global deep OSINT monitor tracking 'national internet shutdown' — high-penetrancy alerting feed");

RSSX(dm_chemical_plant_incident, "gnews-deep-chemical-plant-incident", "OSINT Deep Monitor: chemical plant incident", "OSINT モニター: chemical plant incident", "osint", "news",
  "https://news.google.com/rss/search?q=chemical%20plant%20incident&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"chemical\"]", 1800,
  "Global deep OSINT monitor tracking 'chemical plant incident' — high-penetrancy alerting feed");

RSSX(dm_nuclear_facility_incident, "gnews-deep-nuclear-facility-incident", "OSINT Deep Monitor: nuclear facility incident", "OSINT モニター: nuclear facility incident", "osint", "news",
  "https://news.google.com/rss/search?q=nuclear%20facility%20incident&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"nuclear\"]", 1800,
  "Global deep OSINT monitor tracking 'nuclear facility incident' — high-penetrancy alerting feed");

RSSX(dm_uranium_enrichment, "gnews-deep-uranium-enrichment", "OSINT Deep Monitor: uranium enrichment", "OSINT モニター: uranium enrichment", "osint", "news",
  "https://news.google.com/rss/search?q=uranium%20enrichment&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"uranium\"]", 1800,
  "Global deep OSINT monitor tracking 'uranium enrichment' — high-penetrancy alerting feed");

RSSX(dm_hypersonic_missile_test, "gnews-deep-hypersonic-missile-test", "OSINT Deep Monitor: hypersonic missile test", "OSINT モニター: hypersonic missile test", "osint", "news",
  "https://news.google.com/rss/search?q=hypersonic%20missile%20test&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"hypersonic\"]", 1800,
  "Global deep OSINT monitor tracking 'hypersonic missile test' — high-penetrancy alerting feed");

RSSX(dm_anti_satellite_test, "gnews-deep-anti-satellite-test", "OSINT Deep Monitor: anti-satellite test", "OSINT モニター: anti-satellite test", "osint", "news",
  "https://news.google.com/rss/search?q=anti-satellite%20test&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"anti-satellite\"]", 1800,
  "Global deep OSINT monitor tracking 'anti-satellite test' — high-penetrancy alerting feed");

RSSX(dm_cyber_warfare_unit, "gnews-deep-cyber-warfare-unit", "OSINT Deep Monitor: cyber warfare unit", "OSINT モニター: cyber warfare unit", "osint", "news",
  "https://news.google.com/rss/search?q=cyber%20warfare%20unit&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"cyber\"]", 1800,
  "Global deep OSINT monitor tracking 'cyber warfare unit' — high-penetrancy alerting feed");

RSSX(dm_hacktivist_collective, "gnews-deep-hacktivist-collective", "OSINT Deep Monitor: hacktivist collective", "OSINT モニター: hacktivist collective", "osint", "news",
  "https://news.google.com/rss/search?q=hacktivist%20collective&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"hacktivist\"]", 1800,
  "Global deep OSINT monitor tracking 'hacktivist collective' — high-penetrancy alerting feed");

RSSX(dm_ddos_attack_claimed, "gnews-deep-ddos-attack-claimed", "OSINT Deep Monitor: DDoS attack claimed", "OSINT モニター: DDoS attack claimed", "osint", "news",
  "https://news.google.com/rss/search?q=DDoS%20attack%20claimed&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"ddos\"]", 1800,
  "Global deep OSINT monitor tracking 'DDoS attack claimed' — high-penetrancy alerting feed");

RSSX(dm_website_defacement, "gnews-deep-website-defacement", "OSINT Deep Monitor: website defacement", "OSINT モニター: website defacement", "osint", "news",
  "https://news.google.com/rss/search?q=website%20defacement&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"website\"]", 1800,
  "Global deep OSINT monitor tracking 'website defacement' — high-penetrancy alerting feed");

RSSX(dm_insider_threat_arrest, "gnews-deep-insider-threat-arrest", "OSINT Deep Monitor: insider threat arrest", "OSINT モニター: insider threat arrest", "osint", "news",
  "https://news.google.com/rss/search?q=insider%20threat%20arrest&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"insider\"]", 1800,
  "Global deep OSINT monitor tracking 'insider threat arrest' — high-penetrancy alerting feed");

RSSX(dm_espionage_charges, "gnews-deep-espionage-charges", "OSINT Deep Monitor: espionage charges", "OSINT モニター: espionage charges", "osint", "news",
  "https://news.google.com/rss/search?q=espionage%20charges&hl=en-US&gl=US&ceid=US:en", "en", "[\"osint\",\"monitor\",\"deep\",\"espionage\"]", 1800,
  "Global deep OSINT monitor tracking 'espionage charges' — high-penetrancy alerting feed");

RSSX(dm_ua_front_line_advance, "gnews-deep-ua-front-line-advance", "OSINT Deep Monitor (UA): front line advance", "OSINT モニター (UA): front line advance", "osint", "news",
  "https://news.google.com/rss/search?q=front%20line%20advance&hl=uk&gl=UA&ceid=UA:uk", "uk", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"ua\"]", 1800,
  "Regional deep OSINT monitor (UA) tracking 'front line advance'");

RSSX(dm_il_gaza_strikes, "gnews-deep-il-gaza-strikes", "OSINT Deep Monitor (IL): Gaza strikes", "OSINT モニター (IL): Gaza strikes", "osint", "news",
  "https://news.google.com/rss/search?q=Gaza%20strikes&hl=iw&gl=IL&ceid=IL:iw", "iw", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"il\"]", 1800,
  "Regional deep OSINT monitor (IL) tracking 'Gaza strikes'");

RSSX(dm_tw_pla_drills_taiwan, "gnews-deep-tw-pla-drills-taiwan", "OSINT Deep Monitor (TW): PLA drills Taiwan", "OSINT モニター (TW): PLA drills Taiwan", "osint", "news",
  "https://news.google.com/rss/search?q=PLA%20drills%20Taiwan&hl=zh-TW&gl=TW&ceid=TW:zh", "zh", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"tw\"]", 1800,
  "Regional deep OSINT monitor (TW) tracking 'PLA drills Taiwan'");

RSSX(dm_ae_red_sea_drone_attack, "gnews-deep-ae-red-sea-drone-attack", "OSINT Deep Monitor (AE): Red Sea drone attack", "OSINT モニター (AE): Red Sea drone attack", "osint", "news",
  "https://news.google.com/rss/search?q=Red%20Sea%20drone%20attack&hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"ae\"]", 1800,
  "Regional deep OSINT monitor (AE) tracking 'Red Sea drone attack'");

RSSX(dm_fr_sahel_coup, "gnews-deep-fr-sahel-coup", "OSINT Deep Monitor (FR): Sahel coup", "OSINT モニター (FR): Sahel coup", "osint", "news",
  "https://news.google.com/rss/search?q=Sahel%20coup&hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"fr\"]", 1800,
  "Regional deep OSINT monitor (FR) tracking 'Sahel coup'");

RSSX(dm_ru_wagner_africa, "gnews-deep-ru-wagner-africa", "OSINT Deep Monitor (RU): Wagner Africa", "OSINT モニター (RU): Wagner Africa", "osint", "news",
  "https://news.google.com/rss/search?q=Wagner%20Africa&hl=ru&gl=RU&ceid=RU:ru", "ru", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"ru\"]", 1800,
  "Regional deep OSINT monitor (RU) tracking 'Wagner Africa'");

RSSX(dm_mx_cartel_fentanyl, "gnews-deep-mx-cartel-fentanyl", "OSINT Deep Monitor (MX): cartel fentanyl", "OSINT モニター (MX): cartel fentanyl", "osint", "news",
  "https://news.google.com/rss/search?q=cartel%20fentanyl&hl=es-419&gl=MX&ceid=MX:es", "es", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"mx\"]", 1800,
  "Regional deep OSINT monitor (MX) tracking 'cartel fentanyl'");

RSSX(dm_in_cyber_attack, "gnews-deep-in-cyber-attack", "OSINT Deep Monitor (IN): cyber attack", "OSINT モニター (IN): cyber attack", "osint", "news",
  "https://news.google.com/rss/search?q=cyber%20attack&hl=en-IN&gl=IN&ceid=IN:en", "en", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"in\"]", 1800,
  "Regional deep OSINT monitor (IN) tracking 'cyber attack'");

RSSX(dm_kr_north_korea_hackers, "gnews-deep-kr-north-korea-hackers", "OSINT Deep Monitor (KR): North Korea hackers", "OSINT モニター (KR): North Korea hackers", "osint", "news",
  "https://news.google.com/rss/search?q=North%20Korea%20hackers&hl=ko&gl=KR&ceid=KR:ko", "ko", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"kr\"]", 1800,
  "Regional deep OSINT monitor (KR) tracking 'North Korea hackers'");

RSSX(dm_ph_south_china_sea_standoff, "gnews-deep-ph-south-china-sea-standoff", "OSINT Deep Monitor (PH): South China Sea standoff", "OSINT モニター (PH): South China Sea standoff", "osint", "news",
  "https://news.google.com/rss/search?q=South%20China%20Sea%20standoff&hl=en-PH&gl=PH&ceid=PH:en", "en", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"ph\"]", 1800,
  "Regional deep OSINT monitor (PH) tracking 'South China Sea standoff'");

RSSX(dm_lt_baltic_cable_damage, "gnews-deep-lt-baltic-cable-damage", "OSINT Deep Monitor (LT): Baltic cable damage", "OSINT モニター (LT): Baltic cable damage", "osint", "news",
  "https://news.google.com/rss/search?q=Baltic%20cable%20damage&hl=lt&gl=LT&ceid=LT:lt", "lt", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"lt\"]", 1800,
  "Regional deep OSINT monitor (LT) tracking 'Baltic cable damage'");

RSSX(dm_no_arctic_submarine, "gnews-deep-no-arctic-submarine", "OSINT Deep Monitor (NO): Arctic submarine", "OSINT モニター (NO): Arctic submarine", "osint", "news",
  "https://news.google.com/rss/search?q=Arctic%20submarine&hl=no&gl=NO&ceid=NO:no", "no", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"no\"]", 1800,
  "Regional deep OSINT monitor (NO) tracking 'Arctic submarine'");

RSSX(dm_ir_iran_nuclear_site, "gnews-deep-ir-iran-nuclear-site", "OSINT Deep Monitor (IR): Iran nuclear site", "OSINT モニター (IR): Iran nuclear site", "osint", "news",
  "https://news.google.com/rss/search?q=Iran%20nuclear%20site&hl=fa&gl=IR&ceid=IR:fa", "fa", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"ir\"]", 1800,
  "Regional deep OSINT monitor (IR) tracking 'Iran nuclear site'");

RSSX(dm_co_venezuela_border, "gnews-deep-co-venezuela-border", "OSINT Deep Monitor (CO): Venezuela border", "OSINT モニター (CO): Venezuela border", "osint", "news",
  "https://news.google.com/rss/search?q=Venezuela%20border&hl=es-419&gl=CO&ceid=CO:es", "es", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"co\"]", 1800,
  "Regional deep OSINT monitor (CO) tracking 'Venezuela border'");

RSSX(dm_pk_kashmir_clash, "gnews-deep-pk-kashmir-clash", "OSINT Deep Monitor (PK): Kashmir clash", "OSINT モニター (PK): Kashmir clash", "osint", "news",
  "https://news.google.com/rss/search?q=Kashmir%20clash&hl=en-PK&gl=PK&ceid=PK:en", "en", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"pk\"]", 1800,
  "Regional deep OSINT monitor (PK) tracking 'Kashmir clash'");

RSSX(dm_ke_horn_of_africa_piracy, "gnews-deep-ke-horn-of-africa-piracy", "OSINT Deep Monitor (KE): Horn of Africa piracy", "OSINT モニター (KE): Horn of Africa piracy", "osint", "news",
  "https://news.google.com/rss/search?q=Horn%20of%20Africa%20piracy&hl=en-KE&gl=KE&ceid=KE:en", "en", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"ke\"]", 1800,
  "Regional deep OSINT monitor (KE) tracking 'Horn of Africa piracy'");

RSSX(dm_cn_rare_earth_export_ban, "gnews-deep-cn-rare-earth-export-ban", "OSINT Deep Monitor (CN): rare earth export ban", "OSINT モニター (CN): rare earth export ban", "osint", "news",
  "https://news.google.com/rss/search?q=rare%20earth%20export%20ban&hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"cn\"]", 1800,
  "Regional deep OSINT monitor (CN) tracking 'rare earth export ban'");

RSSX(dm_ae_strait_of_hormuz_seizure, "gnews-deep-ae-strait-of-hormuz-seizure", "OSINT Deep Monitor (AE): Strait of Hormuz seizure", "OSINT モニター (AE): Strait of Hormuz seizure", "osint", "news",
  "https://news.google.com/rss/search?q=Strait%20of%20Hormuz%20seizure&hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"ae\"]", 1800,
  "Regional deep OSINT monitor (AE) tracking 'Strait of Hormuz seizure'");

RSSX(dm_pl_belarus_border, "gnews-deep-pl-belarus-border", "OSINT Deep Monitor (PL): Belarus border", "OSINT モニター (PL): Belarus border", "osint", "news",
  "https://news.google.com/rss/search?q=Belarus%20border&hl=pl&gl=PL&ceid=PL:pl", "pl", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"pl\"]", 1800,
  "Regional deep OSINT monitor (PL) tracking 'Belarus border'");

RSSX(dm_rs_kosovo_tensions, "gnews-deep-rs-kosovo-tensions", "OSINT Deep Monitor (RS): Kosovo tensions", "OSINT モニター (RS): Kosovo tensions", "osint", "news",
  "https://news.google.com/rss/search?q=Kosovo%20tensions&hl=sr&gl=RS&ceid=RS:sr", "sr", "[\"osint\",\"monitor\",\"deep\",\"regional\",\"rs\"]", 1800,
  "Regional deep OSINT monitor (RS) tracking 'Kosovo tensions'");
