/* Verified-live energy sources (17), part 2.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "_vjson_idkeys.inc"

VJSON(eco_odre_conso_region, "eco-odre-conso-region", "France regional gross consumption", "フランス地域別総消費",
  "energy", "energy-grid",
  "https://odre.opendatasoft.com/api/explore/v2.1/catalog/datasets/consommation-quotidienne-brute-regionale/records?limit=50&order_by=date_heure%20desc",
  "*",
  "fr", "[\"france\",\"consumption\",\"gas\",\"electricity\"]", 1800,
  "Half-hourly gross regional electricity and gas consumption in France.");

/* Keyed on date_heure+code_insee_region. This is the REGIONAL table, so a
 * timestamp is a dimension and not a record id — the national sibling
 * eco-odre-eco2mix-tr below is the row for which date_heure alone is right.
 * Measured live 2026-09-16 over the same 20 offset pages: 2,000 records and
 * 2,000 distinct serialisations, but only 1,856 distinct date_heure, because
 * every quarter-hour appears once per region. date_heure+code_insee_region is
 * distinct on all 2,000; the run had been storing 1,997 of 2,000. `+`
 * composes — a comma here would key on date_heure and never read the region. */
VJSON_IDKEYS(eco_odre_eco2mix_regional, "eco-odre-eco2mix-regional", "RTE eCO2mix regional real time", "RTE eCO2mix 地域別リアルタイム",
  "energy", "energy-grid",
  "https://odre.opendatasoft.com/api/explore/v2.1/catalog/datasets/eco2mix-regional-tr/records?limit=100",
  "results",
  "fr", "[\"france\",\"rte\",\"regional\",\"generation-mix\"]", 900,
  "Quarter-hourly French regional consumption and generation, showing where load stress sits.",
  "date_heure+code_insee_region");

/* Keyed on date_heure. The records carry no id, and the fallback label
 * ("<date> prevision_j1=<MW>") repeats whenever two quarter-hours of one day share
 * a forecast value: 1,000 emitted, 998 stored. Measured 2026-09-15 over the
 * same 20 offset pages: 1,000 records, 1,000 distinct date_heure. limit=100 is
 * the ODS v2.1 maximum and doubles what one walk reads of the 7,488 rows. */
VJSON_IDKEYS(eco_odre_eco2mix_tr, "eco-odre-eco2mix-tr", "RTE eCO2mix national real time", "RTE eCO2mix 全国リアルタイム",
  "energy", "energy-grid",
  "https://odre.opendatasoft.com/api/explore/v2.1/catalog/datasets/eco2mix-national-tr/records?limit=100&order_by=date_heure%20desc",
  "*",
  "fr", "[\"france\",\"rte\",\"generation-mix\",\"realtime\"]", 900,
  "Quarter-hourly French national consumption and generation by source from RTE.",
  "date_heure");

VJSON(eco_odre_parc_eolien, "eco-odre-parc-eolien", "France regional wind and solar fleet", "フランス地域別風力・太陽光設備",
  "energy", "energy-grid",
  "https://odre.opendatasoft.com/api/explore/v2.1/catalog/datasets/parc-regional-annuel-prod-eolien-solaire/exports/json",
  "",
  "fr", "[\"france\",\"wind\",\"solar\",\"capacity\"]", 86400,
  "Annual regional wind and solar capacity and output in France.");

VJSON(eco_odre_prod_region, "eco-odre-prod-region", "France annual regional generation by sector", "フランス地域別年間発電量",
  "energy", "energy-grid",
  "https://odre.opendatasoft.com/api/explore/v2.1/catalog/datasets/prod-region-annuelle-filiere/exports/json",
  "",
  "fr", "[\"france\",\"generation\",\"regional\",\"statistics\"]", 86400,
  "Annual French regional electricity generation by technology.");

/* The aggregated registry carries no record id: codeeicresourceobject is null
 * on every row of the default order and no generic id field exists, so the
 * emitter's fallback key folded different installations across pages (2000
 * emitted, 1996 stored; the same 20 pages hold 2,000 byte-distinct rows, and
 * nominstallation+codeinseecommune alone repeats 57 times). An installation is
 * identified here by its own stable descriptors — name, commune, filière and
 * technology codes, installed capacity, connection and commissioning dates —
 * which is unique on all 2,000 and leaves out the rolling annual-energy
 * columns that change between runs. `+` composes. */
VJSON_IDKEYS(eco_odre_registre_prod, "eco-odre-registre-prod", "France generation and storage registry", "フランス発電・蓄電設備登録簿",
  "energy", "energy-grid",
  "https://odre.opendatasoft.com/api/explore/v2.1/catalog/datasets/registre-national-installation-production-stockage-electricite-agrege/records?limit=100",
  "results",
  "fr", "[\"france\",\"registry\",\"capacity\",\"storage\"]", 86400,
  "Aggregated national registry of French electricity generation and storage installations.",
  "nominstallation+codeinseecommune+codefiliere+codetechnologie+puismaxinstallee+dateraccordement+datemiseenservice");

VRSS(eco_oilprice, "eco-oilprice", "OilPrice.com", "オイルプライス・ドットコム",
  "energy", "commodities",
  "https://oilprice.com/rss/main",
  "en", "[\"oil\",\"gas\",\"markets\",\"geopolitics\"]", 3600,
  "Oil and gas market news including supply disruptions and refinery events.");

VRSS(eco_powermag, "eco-powermag", "POWER Magazine", "パワー・マガジン",
  "energy", "energy-grid",
  "https://www.powermag.com/feed/",
  "en", "[\"power-plants\",\"generation\",\"industry\"]", 3600,
  "Global power generation industry news including plant outages and commissioning.");

VJSON(eco_pse_pl_crb, "eco-pse-pl-crb", "PSE Poland five-minute demand plan", "PSE ポーランド5分需要計画",
  "energy", "energy-grid",
  "https://api.raporty.pse.pl/api/pk5l-wp?%24first=50",
  "*",
  "pl", "[\"poland\",\"tso\",\"demand\",\"forecast\"]", 900,
  "Polish five-minute national demand plan and reserve margin.");

VJSON(eco_pse_pl_generation, "eco-pse-pl-generation", "PSE Poland system generation", "PSE ポーランド系統発電量",
  "energy", "energy-grid",
  "https://api.raporty.pse.pl/api/his-wlk-cal?%24first=50",
  "*",
  "pl", "[\"poland\",\"tso\",\"generation\",\"realtime\"]", 900,
  "Polish transmission operator's quarter-hourly national generation and demand series.");

VRSS(eco_pvmagazine, "eco-pvmagazine", "pv magazine", "pvマガジン",
  "energy", "energy-grid",
  "https://www.pv-magazine.com/feed/",
  "en", "[\"solar\",\"pv\",\"manufacturing\",\"policy\"]", 3600,
  "Global solar PV market, manufacturing and policy coverage.");

VJSON(eco_statnett_flow, "eco-statnett-flow", "Statnett Nordic physical flow map", "スタットネット 北欧潮流マップ",
  "energy", "energy-grid",
  "https://driftsdata.statnett.no/restapi/PhysicalFlowMap/GetFlow",
  "*",
  "no", "[\"norway\",\"nordics\",\"cross-border\",\"flows\"]", 900,
  "Physical power flows between Nordic and neighbouring bidding zones from the Norwegian TSO.");

VJSON(eco_uk_carbon_regional, "eco-uk-carbon-regional", "UK regional carbon intensity", "英国地域別炭素強度",
  "energy", "energy-grid",
  "https://api.carbonintensity.org.uk/regional",
  "*",
  "en", "[\"uk\",\"carbon-intensity\",\"regional\",\"grid\"]", 1800,
  "Half-hourly carbon intensity and generation mix for each GB DNO region.");

VJSON(eco_uk_ckan_datasets, "eco-uk-ckan-datasets", "UK data.gov.uk energy datasets", "英国データポータル エネルギー",
  "energy", "statistics",
  "https://ckan.publishing.service.gov.uk/api/3/action/package_search?q=energy&rows=25&sort=id%20asc",
  "*",
  "en", "[\"uk\",\"opendata\",\"energy\",\"catalogue\"]", 86400,
  "Newly published and updated UK government energy datasets.");

VRSS(eco_uk_desnz_atom, "eco-uk-desnz-atom", "UK Department for Energy Security and Net Zero", "英国エネルギー安全保障・ネットゼロ省",
  "energy", "energy-grid",
  "https://www.gov.uk/government/organisations/department-for-energy-security-and-net-zero.atom",
  "en", "[\"uk\",\"energy\",\"policy\",\"net-zero\"]", 3600,
  "UK energy ministry announcements including supply security and price interventions.");

VRSS(eco_utilitydive, "eco-utilitydive", "Utility Dive", "ユーティリティ・ダイブ",
  "energy", "energy-grid",
  "https://www.utilitydive.com/feeds/news/",
  "en", "[\"usa\",\"utilities\",\"regulation\",\"grid\"]", 3600,
  "US utility sector news covering regulation, grid investment and outages.");

VRSS(eco_worldoil, "eco-worldoil", "World Oil", "ワールド・オイル",
  "energy", "commodities",
  "https://www.worldoil.com/rss?feed=news",
  "en", "[\"oil\",\"upstream\",\"drilling\",\"rigs\"]", 3600,
  "Upstream oil and gas news covering drilling activity and field developments.");
