#!/usr/bin/env python3
"""Generate core/collectors.def — every collector key with a label and a PARENT group.

    python3 tools/gen_collectors_def.py docs/source-pages-revamp/collector-census-2026-10-12.tsv \
        > core/collectors.def

Input: the per-collector census (key, source count, top categories as
"cat:n;cat:n;…") — docs/source-pages-revamp/collector-census-<date>.tsv, written
from GET /api/collectors. Output: one COLLECTOR(...) line per GROUP (keys
`grp_*`, parent NULL) and one per collector key (parent = a group key), so every
collector appears under a natural-language heading on the Collectors pages.

How a key is assigned, in order:
  1. an explicit override by key (OVERRIDES);
  2. the BEAT word of a generated table key (hp3b25_jpmedia -> "jpmedia") or the
     SUFFIX of a country-prefixed key (il_company-registry -> "company-registry")
     matched against KEYWORDS, longest keyword first;
  3. the majority CATEGORY of the collector's sources (CATEGORY_GROUP);
  4. otherwise grp_unassigned — emitted, visible, and the script exits non-zero
     naming the keys, because an unassigned collector is a hole in the map.

Labels: a hand label where one is known (LABELS), else the key humanised
("hp3b25_jpmedia" -> "jpmedia (batch 25)", "il_company-registry" ->
"Israel — company registry"). A label describes the beat; the per-collector
tallies beside it are the measurement.
"""
import sys

GROUPS = [
    # key, label, label_ja, description
    ("grp_government",   "Government & public administration", "政府・行政",
     "National, regional and municipal administrations, portals, gazettes and public registers."),
    ("grp_legal",        "Law, courts, legislation & IP", "法務・司法・立法",
     "Courts, legislatures, legal gazettes, licensing and intellectual-property registers."),
    ("grp_corporate",    "Companies, finance & markets", "企業・金融・市場",
     "Company registries, financial regulators, exchanges, filings and economic indicators."),
    ("grp_procurement",  "Procurement, aid & public spending", "調達・援助・公的支出",
     "Tenders, contracts, development aid, payrolls, public finance and transparency portals."),
    ("grp_opendata",     "Open data portals & statistics", "オープンデータ・統計",
     "CKAN/Socrata/ODS portals, statistical offices and microdata catalogues."),
    ("grp_science",      "Science, research & academia", "科学・研究・学術",
     "Journals, repositories, preprints, scholarly indexes and research institutions."),
    ("grp_health",       "Health, food & agriculture", "保健・食品・農業",
     "Clinical registries, outbreak surveillance, drug and device registers, food safety, agriculture."),
    ("grp_environment",  "Environment, climate & hazards", "環境・気候・災害",
     "Weather, air, water, oceans, seismic and volcanic activity, wildfire, flood and disaster feeds."),
    ("grp_space",        "Space & satellites", "宇宙・衛星",
     "Spaceflight, orbital catalogues, astronomy and space weather."),
    ("grp_cyber",        "Cyber threat intelligence & hacking", "サイバー脅威情報",
     "Advisories, vulnerabilities, malware, leaks and breaches, paste sites, onion and deep-web indexes, software supply chain."),
    ("grp_telecom",      "Telecom, networks & signals", "通信・ネットワーク・信号",
     "Internet infrastructure, routing, DNS, spectrum, SIGINT-adjacent public feeds."),
    ("grp_infrastructure","Infrastructure, energy & utilities", "インフラ・エネルギー",
     "Power grids, outages, utilities, industrial control, built infrastructure."),
    ("grp_transport",    "Transport, aviation & maritime", "交通・航空・海事",
     "Rail, road, transit, vehicle registers, airspace and aviation, shipping and ports."),
    ("grp_media",        "News & media", "ニュース・メディア",
     "Wire services, newspapers, broadcasters, regional and trade press."),
    ("grp_social",       "Social, web & communities", "ソーシャル・ウェブ",
     "Social platforms, forums and boards, classifieds, marketplaces, people and profile lookups."),
    ("grp_geo",          "Geospatial & mapping", "地理空間・地図",
     "Geocoders, OSM and cadastral data, GIS servers, boundaries and place names."),
    ("grp_safety",       "Crime, policing, defense & conflict", "犯罪・警察・防衛・紛争",
     "Police, crime statistics, public safety, defense, geopolitics and human-rights monitoring."),
    ("grp_sanctions",    "Sanctions & compliance", "制裁・コンプライアンス",
     "Sanctions lists, designated persons, debarment and watchlists."),
    ("grp_culture",      "Culture, heritage, libraries & archives", "文化・遺産・図書館・アーカイブ",
     "Libraries, archives, museums, heritage registers, tourism and education."),
    ("grp_surveillance", "Cameras, sensors & exposure", "カメラ・センサー・露出",
     "Public webcams, telemetry, seismic and environmental sensor networks, internet-exposure scanners."),
    ("grp_osint",        "Investigation pivots & reference", "調査ピボット・参照",
     "Hand-written entity pivots, identifier bridges, web archives, standards and reference lookups."),
    ("grp_internal",     "Internal pods & tools", "内部ポッド・ツール",
     "Maintenance, enrichment, probe and test pods — not data sources."),
]
GROUP_KEYS = {g[0] for g in GROUPS}

# Explicit key -> group.
OVERRIDES = {
    "osint": "grp_osint", "intelligence": "grp_osint",
    # Round 3 (2026-10-18): the 951 SCHEDULED feeds that were labelled "osint"
    # — Google News editions, outlets, arXiv, think tanks, threat feeds — now
    # carry the collector of what they are. Pivots keep "osint".
    "gnews": "grp_media", "gnews_monitors": "grp_media", "world_outlets": "grp_media",
    "thinktanks": "grp_media", "trade_press": "grp_media", "ngo_rights": "grp_safety", "conflict_press": "grp_safety",
    "arxiv": "grp_science", "science_press": "grp_science", "finance_press": "grp_corporate",
    "osm": "grp_geo", "hp3b28_jpdb": "grp_science", "reddit": "grp_social",
    "jp_webmedia": "grp_media",   # batches 43/44: 400 Japanese web-media/blog RSS feeds, authored under "social"
    # batch 45: WordPress collections of hosts the registry already held, split by host type
    "hp3b45_jpwpmedia": "grp_media", "hp3b45_jpwpcorp": "grp_corporate", "hp3b45_jpwporg": "grp_legal",
    "hp3b45_jpwpculture": "grp_culture", "hp3b45_jpwpcyber": "grp_cyber", "hp3b45_jpwpscience": "grp_science",
    "hp3b45_jpwpenv": "grp_environment", "hp3b45_jpwpgov": "grp_government",
    # batch 49: federation-member sites (the staging set of batch 45), by federation
    "hp3b49_jpwpmedical": "grp_health", "hp3b49_jpwpagri": "grp_health", "hp3b49_jpwpcci": "grp_corporate",
    "hp3b49_jpwpcommerce": "grp_corporate", "hp3b49_jpwpcoop": "grp_corporate", "hp3b49_jpwpjc": "grp_social",
    "hp3b49_jpwpprof": "grp_legal", "hp3b49_jpwpfire": "grp_safety", "hp3b49_jpwpwelfare": "grp_health",
    "hp3b49_jpwpculture": "grp_culture", "hp3b49_jpwpedu": "grp_science",
    "_maint": "grp_internal", "_enrich": "grp_internal", "_test": "grp_internal", "_probe": "grp_internal",
    "_breach": "grp_cyber",
    "camera-discovery": "grp_surveillance", "us_cameras": "grp_surveillance",
    "hp3b29a_webcams": "grp_surveillance", "hp3b30_jpcam": "grp_surveillance",
    "hp3b31_survnet": "grp_surveillance", "hp3b31_survcam": "grp_surveillance",
    "hp3b31_survsensor": "grp_surveillance", "hp3b31_survtransport": "grp_transport",
    "hp3b34_fdsnstation": "grp_environment", "hp3b34_fdsnevent": "grp_environment",
    "hp3b32_jptelemetry": "grp_infrastructure",
    "hp_exposure_geo": "grp_surveillance",
    "geopolitics": "grp_safety", "hp3b19_humanrights": "grp_safety", "hp3b19_defence": "grp_safety",
    "sanctions": "grp_sanctions", "hp_sanctions_deep": "grp_sanctions",
    "transparency": "grp_procurement", "pa_payroll": "grp_procurement", "hp_procurement_aid": "grp_procurement",
    "finreg": "grp_corporate", "hp3b19_crypto": "grp_corporate", "hp_chain_finance": "grp_corporate",
    "hp3b19_landreg": "grp_corporate", "hp3b19_idbridge": "grp_osint",
    "hp3b22_deeparchive": "grp_osint", "hp3b20_standards": "grp_osint", "hp3b19_reference": "grp_osint",
    "hp3b24_jpbulk": "grp_osint", "hp_people_ident": "grp_social",
    "hp3b22_pastechat": "grp_cyber", "hp3b22_extortion": "grp_cyber", "hp3b22_onion": "grp_cyber",
    "hp3b22_breachcat": "grp_cyber", "hp3b23_jpcyber": "grp_cyber", "hp3b19_cyber": "grp_cyber",
    "hp3b25_jpdeep": "grp_cyber", "hp3b28_jpdeepweb": "grp_cyber", "hp3b33_supplychain": "grp_cyber",
    "hp3b35_strings": "grp_cyber", "hp3b34_ndjson": "grp_cyber", "hp_netintel_deep": "grp_cyber",
    "hp3_netintel": "grp_telecom", "hp3_netintel2": "grp_telecom", "hp3b19_iana": "grp_telecom",
    "hp3b19_sigint": "grp_telecom", "hp3_sigint": "grp_telecom", "hp3b19_telecom": "grp_telecom",
    "net_infra": "grp_telecom", "telecom": "grp_telecom",
    "hp3b29b_deepweb": "grp_legal",          # US state professional licensing registers
    "hp3b29b_govosint": "grp_government", "hp3b29a_forgotten": "grp_government",
    "hp3b28_jpforgotten": "grp_culture", "hp3b30_jplibrary": "grp_culture",
    "hp3b25_jparchive": "grp_culture", "hp3b32_jpsearch2": "grp_culture",
    "hp3b25_jpkeizai": "grp_media", "hp3b25_jpmedia": "grp_media", "hp3b25_jpmedia2": "grp_media",
    "hp3b19_media": "grp_media", "hp3b32_jpfeeds": "grp_media",
    "hp3b23_jpsocial": "grp_social", "hp3b24_jpbbs": "grp_social",
    "hp3b30_jpscada": "grp_infrastructure", "hp3b25_jpinfra": "grp_infrastructure",
    "hp3b19_energyenv": "grp_infrastructure", "hp3_energy": "grp_infrastructure", "hp3_infra": "grp_infrastructure",
    "energy": "grp_infrastructure",
    "hp3b25_jppolice": "grp_safety", "hp3b28_jppolice": "grp_safety", "hp30tokyo_tokyocrime": "grp_safety",
    "hp3b28_jpindustry": "grp_corporate", "hp3b25_jpbizreg": "grp_corporate", "hp3b25_jpbiz2": "grp_corporate",
    "hp3b30_jpbank2": "grp_corporate", "hp3b28_jpbank": "grp_corporate", "hp3b25_jpfinance": "grp_corporate",
    "hp3b32_jpmarket": "grp_corporate", "hp3b40_cnmkt": "grp_corporate", "hp3b40_cnsar": "grp_corporate",
    "hp3b32_jpregister": "grp_corporate", "hp3b19_finance": "grp_corporate", "hp3_corp": "grp_corporate",
    "hp3b20_tradestat": "grp_corporate", "hp3b39_cn39": "grp_corporate", "hp3b39_tbl39": "grp_corporate",
    "hp3b28_jpcourt": "grp_legal", "hp3b19_legalip": "grp_legal", "hp_courts_legal": "grp_legal",
    "hp3b21_austrialaw": "grp_legal", "hp3b21_delaw": "grp_legal", "hp3b20_parliament": "grp_legal",
    "hp3b25_jpgeospatial": "grp_geo", "hp3b19_osm": "grp_geo", "hp3b32_jparcgis": "grp_geo",
    "hp3b40_jpgeo3": "grp_geo", "hp3_geo": "grp_environment", "hp3b21_envreg": "grp_environment",
    "hp3b19_fisheries": "grp_environment", "ocean_cryo": "grp_environment", "climate_air": "grp_environment",
    "hp3b21_vehiclereg": "grp_transport", "hp3b20_airspace": "grp_transport", "hp_maritime_air": "grp_transport",
    "hp3_maritime": "grp_transport", "hp3b19_transport": "grp_transport", "aviation": "grp_transport",
    "hp3b25_jpkaigo": "grp_health", "hp3b21_healthpay": "grp_health", "hp3b21_medreg": "grp_health",
    "hp3b21_providerreg": "grp_health", "hp3b21_foodhygiene": "grp_health", "hp3b19_health": "grp_health",
    "hp3b35_jpweko": "grp_science", "hp3b32_jprepo2": "grp_science", "hp3b25_jpacademic": "grp_science",
    "hp3b20_scholarly": "grp_science", "hp_research_ip": "grp_science", "hp3b19_science": "grp_science",
    "hp3_research": "grp_science", "space_sci": "grp_space",
    "hp3b31_intlbody": "grp_government", "hp3b19_intlorg": "grp_government", "hp3b31_assetreg": "grp_government",
    "hp3b23_jpregistry": "grp_government", "hp3b25_jpnatgov": "grp_government",
    "hp3b31_eutrans": "grp_government", "hp3b31_menacis": "grp_government",
    "hp3b40_cngov": "grp_government", "hp3b40_jpnat3": "grp_government", "hp3b32_jpmuni3": "grp_government",
    "hp3b36_usa": "grp_government", "hp3b36_uk": "grp_government", "hp3b36_france": "grp_government",
    "hp3b36_china": "grp_government", "hp3b36_africa": "grp_corporate", "hp3b38_africa": "grp_science",
    "hp3b38_usa": "grp_corporate", "hp3b38_uk": "grp_government", "hp3b38_france": "grp_government",
    "hp3b38_china": "grp_government", "hp3b39_jp39": "grp_infrastructure",
    "hp3b25_jprecovered": "grp_cyber", "hp3_threat": "grp_cyber", "threat-intel": "grp_cyber",
    "cyber_advisory": "grp_cyber", "security": "grp_cyber", "cyber": "grp_cyber",
    "hp3_portals": "grp_government", "hp3_society": "grp_government", "hp3_gov": "grp_government",
    "hp3b19_eurasia": "grp_government", "hp3b19_latam": "grp_government", "hp3b19_usstate": "grp_government",
    "hp3b19_mena": "grp_government", "hp3b19_apac": "grp_government", "hp3b19_africa": "grp_government",
    "hp3b19_northam": "grp_government", "hp30tokyo_tokyo": "grp_government",
    "us_civilian": "grp_geo", "de_civilian": "grp_geo", "gb_civilian": "grp_geo", "fr_civilian": "grp_geo",
    "jp_civilian": "grp_geo",             # OSM/Nominatim and relationship graphs
    "culture": "grp_culture", "tourism": "grp_culture", "fr_education": "grp_culture",
    "marketplace": "grp_social", "classifieds": "grp_social", "social": "grp_social", "commercial": "grp_corporate",
    "agriculture": "grp_health", "food": "grp_health",
    "disaster": "grp_environment", "jp_river": "grp_environment", "earthobs": "grp_environment",
    "europe_env": "grp_environment",   # an environment beat whose rows are geo-tagged
    # batches 41/42 (2026-10-18): Japanese WordPress REST beats, by the kind of host
    "hp3b41_jpcorpwp": "grp_corporate", "hp3b42_jpcptcorp": "grp_corporate", "hp3b42_jppages": "grp_corporate",
    "hp3b42_jpusers": "grp_corporate", "hp3b41_jpbankwp": "grp_corporate", "hp3b42_jpcptbank": "grp_corporate",
    "hp3b41_jpuniwp": "grp_science", "hp3b42_jpcptuni": "grp_science",
    "hp3b41_jpmuniwp": "grp_government", "hp3b42_jpcptmuni": "grp_government",
    "satellite": "grp_space", "space": "grp_space",
    "crime": "grp_safety", "safety": "grp_safety", "defense": "grp_safety",
    "statistics": "grp_opendata", "economy": "grp_corporate", "industry": "grp_corporate", "finance": "grp_corporate",
    "legal": "grp_legal", "news": "grp_media", "maritime": "grp_transport", "transport": "grp_transport",
    "infrastructure": "grp_infrastructure", "geospatial": "grp_geo", "health": "grp_health",
    "academic": "grp_science", "science": "grp_science",
}

# Beat / suffix keyword -> group (longest match wins).
KEYWORDS = [
    ("company-registry", "grp_corporate"), ("professional-register", "grp_legal"), ("ip-registry", "grp_legal"),
    ("health-regulator", "grp_health"), ("public-finance", "grp_procurement"), ("travel-expenses", "grp_procurement"),
    ("anti-corruption", "grp_procurement"), ("gov-publications", "grp_government"), ("law-enforcement", "grp_safety"),
    ("foreign-affairs", "grp_government"), ("public-works", "grp_infrastructure"),
    ("procurement", "grp_procurement"), ("payroll", "grp_procurement"), ("payments", "grp_procurement"),
    ("customs", "grp_procurement"), ("tax", "grp_procurement"), ("aid", "grp_procurement"),
    ("opendata", "grp_opendata"), ("statistics", "grp_opendata"), ("microdata", "grp_opendata"), ("demographics", "grp_opendata"),
    ("research", "grp_science"), ("education", "grp_culture"),
    ("health", "grp_health"),
    ("environment", "grp_environment"), ("hazard", "grp_environment"), ("disaster", "grp_environment"), ("hydrology", "grp_environment"),
    ("geodata", "grp_geo"), ("geography", "grp_geo"), ("geospatial", "grp_geo"),
    ("corporate", "grp_corporate"), ("finance", "grp_corporate"), ("markets", "grp_corporate"), ("business", "grp_corporate"),
    ("trade", "grp_corporate"), ("registry", "grp_corporate"), ("insolvency", "grp_corporate"), ("economy", "grp_corporate"),
    ("ip", "grp_legal"), ("legal", "grp_legal"), ("courts", "grp_legal"), ("legislative", "grp_legal"), ("parliament", "grp_legal"),
    ("sanctions", "grp_sanctions"), ("enforcement", "grp_safety"),
    ("transport", "grp_transport"), ("logistics", "grp_transport"),
    ("energy", "grp_infrastructure"), ("infrastructure", "grp_infrastructure"),
    ("news", "grp_media"), ("media", "grp_media"),
    ("government", "grp_government"), ("gov", "grp_government"), ("elections", "grp_government"), ("policy", "grp_government"),
    ("regulator", "grp_government"), ("regulatory", "grp_government"), ("permits", "grp_government"), ("property", "grp_government"),
    ("organizations", "grp_government"), ("projects", "grp_government"), ("labour", "grp_government"), ("gazette", "grp_government"),
    ("reference", "grp_osint"), ("people", "grp_social"),
    ("prefectural", "grp_government"), ("local", "grp_government"), ("state", "grp_government"),
]

# Majority category -> group (fallback).
CATEGORY_GROUP = {
    "government": "grp_government", "local-gov": "grp_government", "gov": "grp_government", "politics": "grp_government",
    "labour": "grp_government", "labor": "grp_government", "policy": "grp_government", "regulator": "grp_government",
    "regulatory": "grp_government", "gazette": "grp_government", "permits": "grp_government", "spending": "grp_procurement",
    "transparency": "grp_procurement", "procurement": "grp_procurement", "aid": "grp_procurement", "payroll": "grp_procurement",
    "public-finance": "grp_procurement",
    "legal": "grp_legal", "courts": "grp_legal", "parliament": "grp_legal", "legislative": "grp_legal", "licensing": "grp_legal",
    "disciplinary": "grp_legal", "legal-record": "grp_legal", "dispute-resolution": "grp_legal",
    "corporate": "grp_corporate", "economy": "grp_corporate", "finance": "grp_corporate", "business": "grp_corporate",
    "company-registry": "grp_corporate", "commerce": "grp_corporate", "commercial": "grp_corporate", "industry": "grp_corporate",
    "disclosure": "grp_corporate", "markets": "grp_corporate", "trade": "grp_corporate", "registry": "grp_corporate",
    "opendata": "grp_opendata", "statistics": "grp_opendata", "microdata": "grp_opendata", "demographics": "grp_opendata",
    "research": "grp_science", "literature": "grp_science", "genomics": "grp_science", "education": "grp_culture",
    "health": "grp_health", "clinical": "grp_health", "outbreak": "grp_health", "agriculture": "grp_health", "food": "grp_health",
    "environment": "grp_environment", "weather": "grp_environment", "wildfire": "grp_environment", "seismic": "grp_environment",
    "ocean": "grp_environment", "hazard": "grp_environment", "disaster": "grp_environment", "hydrology": "grp_environment",
    "earthobs": "grp_environment", "wildlife": "grp_environment", "geo": "grp_geo", "geodata": "grp_geo",
    "geography": "grp_geo", "geospatial": "grp_geo",
    "space": "grp_space", "spaceweather": "grp_space", "astronomy": "grp_space", "spaceflight": "grp_space",
    "physics": "grp_space", "satellite": "grp_space",
    "cyber": "grp_cyber", "threat": "grp_cyber", "advisory": "grp_cyber", "vulnerability": "grp_cyber", "security": "grp_cyber",
    "malware": "grp_cyber", "ransomware": "grp_cyber", "breach": "grp_cyber", "internet": "grp_cyber",
    "telecom": "grp_telecom", "infrastructure": "grp_infrastructure", "outage": "grp_infrastructure",
    "utility": "grp_infrastructure", "energy": "grp_infrastructure", "energy-grid": "grp_infrastructure",
    "transport": "grp_transport", "transit": "grp_transport", "logistics": "grp_transport", "maritime": "grp_transport",
    "aviation": "grp_transport", "airspace": "grp_transport", "navwarning": "grp_transport", "shipping": "grp_transport",
    "news": "grp_media", "wire": "grp_media", "media": "grp_media",
    "social": "grp_social", "classifieds": "grp_social", "marketplace": "grp_social",
    "police": "grp_safety", "crime": "grp_safety", "safety": "grp_safety", "defense": "grp_safety", "defence": "grp_safety",
    "conflict": "grp_safety", "public-safety": "grp_safety", "law-enforcement": "grp_safety", "enforcement": "grp_safety",
    "sanctions": "grp_sanctions",
    "culture": "grp_culture", "heritage": "grp_culture", "library": "grp_culture", "archive": "grp_culture",
    "tourism": "grp_culture", "directory": "grp_culture",
    "surveillance": "grp_surveillance", "webcam": "grp_surveillance", "cameras": "grp_surveillance",
    "investigation": "grp_osint", "reference": "grp_osint", "osint": "grp_osint", "intelligence": "grp_osint",
    "maintenance": "grp_internal", "enrichment": "grp_internal", "test": "grp_internal",
}

LABELS = {
    "osint": ("Hand-written OSINT pivots", "手書きOSINTピボット",
              "Entity-pivot collectors written by hand in collectors/sources/ — asked about an entity, not scheduled."),
    "gnews": ("Google News editions", "Google ニュース 国別・トピック別",
              "Google News RSS by country and topic (gnews-top-*, gnews-<cc>-<topic>)."),
    "gnews_monitors": ("Google News keyword monitors", "Google ニュース キーワード監視",
                       "Google News searches for OSINT keywords (gnews-mon-*)."),
    "world_outlets": ("News outlets worldwide", "世界の報道機関", "National and regional newspapers, broadcasters and wires."),
    "thinktanks": ("Think tanks & policy analysis", "シンクタンク・政策分析", None),
    "trade_press": ("Specialist & trade press", "専門誌", "Maritime, aviation, space, energy, health and conflict-analysis publications."),
    "ngo_rights": ("NGOs, rights & press freedom", "NGO・人権・報道の自由", None),
    "conflict_press": ("Conflict, defence & investigative press", "紛争・防衛・調査報道", None),
    "arxiv": ("arXiv preprint feeds", "arXiv プレプリント", "One feed per arXiv category."),
    "science_press": ("Science & technology press", "科学・技術メディア", None),
    "finance_press": ("Markets & crypto press", "金融・暗号資産メディア", None),
    "osm": ("OpenStreetMap — taginfo & changesets", "OpenStreetMap タグ・変更セット", None),
    "reddit": ("Reddit regional & thematic multireddits", "Reddit 地域・テーマ別", "One feed per multireddit (reddit-group-*)."),
    "hp3b28_jpdb": ("Japan — CiNii Research (batch 28)", "日本 CiNii Research", None),
    # hand-written family labels that had no human name
    "geopolitics": ("Geopolitics, defence & conflict", "地政学・防衛・紛争", None),
    "aviation": ("Aviation & airspace", "航空・空域", None),
    "energy": ("Energy & commodities", "エネルギー・資源", None),
    "economy": ("Economy & economic statistics", "経済・経済統計", None),
    "transparency": ("Transparency & political finance", "透明性・政治資金", None),
    "finance": ("Financial regulators & registers", "金融規制・登録", None),
    "finreg": ("Financial regulation", "金融規制", None),
    "industry": ("Industry & manufacturing", "産業・製造", None),
    "commercial": ("Commercial & press releases", "商業・プレスリリース", None),
    "geospatial": ("Geospatial data", "地理空間データ", None),
    "culture": ("Culture & heritage", "文化・遺産", None),
    "tourism": ("Tourism", "観光", None),
    "agriculture": ("Agriculture & wildlife", "農業・野生生物", None),
    "food": ("Food safety", "食品安全", None),
    "safety": ("Public safety", "公共安全", None),
    "crime": ("Crime", "犯罪", None),
    "defense": ("Defense", "防衛", None),
    "marketplace": ("Marketplaces", "マーケットプレイス", None),
    "classifieds": ("Classifieds", "クラシファイド広告", None),
    "intelligence": ("Intelligence & event data", "インテリジェンス・イベントデータ", None),
    "government": ("Government feeds", "政府・自治体フィード", "Scheduled feeds from national and municipal government sites."),
    "academic": ("Academic and research feeds", "学術・研究フィード", None),
    "science": ("Science feeds", "科学フィード", None),
    "europe_data": ("European open data", "欧州オープンデータ", None),
    "europe_news": ("European news", "欧州ニュース", None),
    "europe_env": ("European environment", "欧州環境", None),
    "europe_gov": ("European government", "欧州政府", None),
    "fr_opendata": ("France — open data", "フランス オープンデータ", None),
    "fr_prefectural": ("France — prefectures", "フランス 県", None),
    "de_opendata": ("Germany — open data", "ドイツ オープンデータ", None),
    "ch_opendata": ("Switzerland — open data", "スイス オープンデータ", None),
    "us_opendata": ("United States — open data", "米国 オープンデータ", None),
    "us_state": ("United States — states", "米国 州", None),
    "latam_data": ("Latin America — open data", "中南米 オープンデータ", None),
    "africa_gov": ("Africa — government", "アフリカ 政府", None),
    "oceania_data": ("Oceania — open data", "オセアニア オープンデータ", None),
    "jp_prefectural": ("Japan — prefectural portals", "日本 都道府県ポータル", None),
    "jp_local": ("Japan — municipalities", "日本 市区町村", None),
    "jp_river": ("Japan — rivers and flood", "日本 河川・洪水", None),
    "disaster": ("Disaster and hazard", "災害・ハザード", None),
    "environment": ("Environment", "環境", None),
    "earthobs": ("Earth observation", "地球観測", None),
    "space": ("Space", "宇宙", None), "satellite": ("Satellites", "衛星", None),
    "cyber": ("Cyber threat intelligence", "サイバー脅威情報", None),
    "security": ("Security advisories", "セキュリティ勧告", None),
    "infrastructure": ("Infrastructure", "インフラ", None), "transport": ("Transport", "交通", None),
    "maritime": ("Maritime", "海事", None), "telecom": ("Telecom and networks", "通信・ネットワーク", None),
    "statistics": ("Statistics", "統計", None), "health": ("Health", "保健・医療", None),
    "legal": ("Legal", "法務", None), "news": ("News", "ニュース", None),
    "sanctions": ("Sanctions lists", "制裁リスト", None), "social": ("Social platforms", "ソーシャル", None),
    "camera-discovery": ("Camera discovery", "カメラ探索", "The per-channel camera discovery fan-out (one source per channel)."),
    "_maint": ("Maintenance pods", "保守ポッド", "Internal pipeline pods — not data sources."),
    "_enrich": ("Enrichment pods", "エンリッチメントポッド", "Internal enrichment pods — not data sources."),
    "_test": ("Test fixtures", "テスト用", "Fixtures for the test suite — not data sources."),
    "_probe": ("Probe tools", "プローブ", "Operator probe tools — not data sources."),
    "_breach": ("Breach corpus", "漏洩コーパス", None),
    "hp3b25_jpmedia": ("Japan — media (batch 25)", "日本 メディア", None),
    "hp3b25_jpkeizai": ("Japan — business press (batch 25)", "日本 経済メディア", None),
    "hp3b30_jpscada": ("Japan — SCADA and infrastructure telemetry (batch 30)", "日本 SCADA・インフラ", None),
    "hp3b25_jppolice": ("Japan — prefectural police (batch 25)", "日本 都道府県警察", None),
    "hp3b28_jppolice": ("Japan — police notices (batch 28)", "日本 警察情報", None),
    "hp30tokyo_tokyocrime": ("Tokyo — crime and police", "東京 犯罪・警察", None),
    "hp3b30_jpbank2": ("Japan — banks (batch 30)", "日本 銀行", None),
    "hp3b23_jpmuni": ("Japan — municipalities (batch 23)", "日本 市区町村", None),
    "hp3b25_jpmuni2": ("Japan — municipalities (batch 25)", "日本 市区町村", None),
    "hp3b29a_webcams": ("Public webcams (batch 29)", "公開ウェブカメラ", None),
    "hp3b30_jpcam": ("Japan — webcams (batch 30)", "日本 ウェブカメラ", None),
    "hp3b33_supplychain": ("Software supply chain and package registries (batch 33)", "ソフトウェア・サプライチェーン", None),
    "hp3b22_pastechat": ("Paste sites and chat leaks (batch 22)", "ペーストサイト・チャット", None),
    "hp3b22_extortion": ("Extortion and ransomware leak sites (batch 22)", "恐喝・ランサムウェア", None),
    "hp3b22_deeparchive": ("Web archives (batch 22)", "ウェブアーカイブ", None),
    "hp3b35_jpweko": ("Japan — JAIRO Cloud repositories (batch 35)", "日本 機関リポジトリ", None),
    "hp3b30_jplibrary": ("Japan — libraries (batch 30)", "日本 図書館", None),
    # batches 41/42: WordPress REST feeds read from the hosts' own route tables
    "hp3b41_jpcorpwp": ("Japan — company websites, WordPress feeds (batch 41)", "日本 企業サイト WordPress", None),
    "hp3b41_jpuniwp": ("Japan — university websites, WordPress feeds (batch 41)", "日本 大学サイト WordPress", None),
    "hp3b41_jpbankwp": ("Japan — bank websites, WordPress feeds (batch 41)", "日本 銀行サイト WordPress", None),
    "hp3b41_jpmuniwp": ("Japan — municipal websites, WordPress feeds (batch 41)", "日本 自治体サイト WordPress", None),
    "hp3b42_jpcptcorp": ("Japan — company sites, WordPress custom post types (batch 42)", "日本 企業サイト カスタム投稿", None),
    "hp3b42_jppages": ("Japan — company sites, WordPress pages (batch 42)", "日本 企業サイト 固定ページ", None),
    "hp3b42_jpusers": ("Japan — company sites, WordPress authors (batch 42)", "日本 企業サイト 投稿者", None),
    "hp3b42_jpcptuni": ("Japan — university sites, WordPress custom post types (batch 42)", "日本 大学サイト カスタム投稿", None),
    "hp3b42_jpcptmuni": ("Japan — municipal sites, WordPress custom post types (batch 42)", "日本 自治体サイト カスタム投稿", None),
    "hp3b42_jpcptbank": ("Japan — bank sites, WordPress custom post types (batch 42)", "日本 銀行サイト カスタム投稿", None),
    "jp_webmedia": ("Japan — web media & blogs, RSS (batches 43/44)", "日本 ウェブメディア・ブログ RSS", "400 Japanese online magazines, tech and lifestyle blogs, tourism and business sites (jp-b43-*, jp-b44-*)."),
    # batch 45: hosts already live in the registry under another endpoint, asked for their WordPress REST root
    "hp3b45_jpwpmedia": ("Japan — online media & magazines, WordPress collections (batch 45)", "日本 ウェブメディア WordPress", None),
    "hp3b45_jpwpcorp": ("Japan — company sites, WordPress collections (batch 45)", "日本 企業サイト WordPress（45）", None),
    "hp3b45_jpwporg": ("Japan — associations & professional bodies, WordPress collections (batch 45)", "日本 団体・士業 WordPress", None),
    "hp3b45_jpwpculture": ("Japan — libraries & culture, WordPress collections (batch 45)", "日本 図書館・文化 WordPress", None),
    "hp3b45_jpwpcyber": ("Japan — security organisations, WordPress collections (batch 45)", "日本 セキュリティ団体 WordPress", None),
    "hp3b45_jpwpscience": ("Japan — research institutions, WordPress collections (batch 45)", "日本 研究機関 WordPress", None),
    "hp3b45_jpwpenv": ("Japan — hazard & geospatial sites, WordPress collections (batch 45)", "日本 防災・地理空間 WordPress", None),
    "hp3b45_jpwpgov": ("Japan — government sites, WordPress collections (batch 45)", "日本 政府サイト WordPress", None),
    # batch 49: member organisations of nationwide federations, asked for their WordPress REST root
    "hp3b49_jpwpmedical": ("Japan — medical, pharmacist, nursing & veterinary associations (batch 49)", "日本 医師会・薬剤師会・看護協会・獣医師会", None),
    "hp3b49_jpwpagri": ("Japan — JA cooperatives, NOSAI & agricultural committees (batch 49)", "日本 JA・NOSAI・農業委員会", None),
    "hp3b49_jpwpcci": ("Japan — chambers of commerce & industry (batch 49)", "日本 商工会議所", None),
    "hp3b49_jpwpcommerce": ("Japan — corporate-tax associations 法人会 (batch 49)", "日本 法人会", None),
    "hp3b49_jpwpcoop": ("Japan — consumer, fishery & labour co-operatives (batch 49)", "日本 生協・漁協・労金", None),
    "hp3b49_jpwpjc": ("Japan — Junior Chamber local organisations (batch 49)", "日本 青年会議所", None),
    "hp3b49_jpwpprof": ("Japan — professional bodies: tax, legal, labour, architects, realtors (batch 49)", "日本 士業団体", None),
    "hp3b49_jpwpfire": ("Japan — fire departments & fire-chief associations (batch 49)", "日本 消防本部・消防長会", None),
    "hp3b49_jpwpwelfare": ("Japan — social welfare councils (batch 49)", "日本 社会福祉協議会", None),
    "hp3b49_jpwpculture": ("Japan — public cultural facilities, zoos & aquariums (batch 49)", "日本 公立文化施設・動物園・水族館", None),
    "hp3b49_jpwpedu": ("Japan — junior-college & private-university associations (batch 49)", "日本 短大協会・私大協", None),
}

COUNTRY = {
    "ae": "UAE", "ar": "Argentina", "au": "Australia", "be": "Belgium", "bf": "Burkina Faso", "bh": "Bahrain",
    "bj": "Benin", "bn": "Brunei", "bo": "Bolivia", "br": "Brazil", "bt": "Bhutan", "bw": "Botswana",
    "cd": "DR Congo", "ch": "Switzerland", "ci": "Côte d'Ivoire", "cm": "Cameroon", "co": "Colombia",
    "de": "Germany", "dk": "Denmark", "dz": "Algeria", "ee": "Estonia", "eg": "Egypt", "es": "Spain",
    "et": "Ethiopia", "eu": "EU", "fi": "Finland", "fr": "France", "gb": "UK", "gh": "Ghana", "gr": "Greece",
    "hr": "Croatia", "hu": "Hungary", "id": "Indonesia", "ie": "Ireland", "il": "Israel", "in": "India",
    "int": "International", "it": "Italy", "jp": "Japan", "ke": "Kenya", "kh": "Cambodia", "kr": "Korea",
    "lk": "Sri Lanka", "lv": "Latvia", "ma": "Morocco", "mg": "Madagascar", "ml": "Mali", "mm": "Myanmar",
    "mu": "Mauritius", "mv": "Maldives", "mx": "Mexico", "my": "Malaysia", "na": "Namibia", "ng": "Nigeria",
    "nl": "Netherlands", "no": "Norway", "np": "Nepal", "pa": "Panama", "ph": "Philippines", "pk": "Pakistan",
    "pl": "Poland", "qa": "Qatar", "ro": "Romania", "rw": "Rwanda", "sg": "Singapore", "si": "Slovenia",
    "sk": "Slovakia", "sl": "Sierra Leone", "sn": "Senegal", "th": "Thailand", "tn": "Tunisia", "tr": "Turkey",
    "tw": "Taiwan", "tz": "Tanzania", "ua": "Ukraine", "ug": "Uganda", "uk": "UK", "us": "United States",
    "vn": "Vietnam", "za": "South Africa", "zm": "Zambia", "mena": "Middle East & North Africa",
    "latam": "Latin America", "seasia": "South-East Asia", "sasia": "South Asia", "eastasia": "East Asia",
    "africa": "Africa", "europe": "Europe", "oceania": "Oceania", "namerica": "North America", "korea": "Korea",
    "taiwan": "Taiwan", "canada": "Canada",
}


def beat_of(key):
    """hp3b25_jpmedia -> 'jpmedia'; hp_uk_deep -> 'uk_deep'; il_company-registry -> 'company-registry'."""
    if key.startswith("hp"):
        parts = key.split("_", 1)
        return parts[1] if len(parts) > 1 else key
    parts = key.split("_", 1)
    return parts[1] if len(parts) > 1 else key


def assign(key, cats):
    if key in OVERRIDES:
        return OVERRIDES[key]
    # osint_<category>: the hand-written entity pivots, split by their category
    # (round 5, 2026-10-20). The category decides the topical group; a category
    # the map does not know stays under the pivots group rather than unassigned.
    if key.startswith("osint_"):
        return CATEGORY_GROUP.get(key[len("osint_"):], "grp_osint")
    beat = beat_of(key)
    for kw, grp in sorted(KEYWORDS, key=lambda kv: -len(kv[0])):
        if kw == beat or beat.endswith("_" + kw) or beat.endswith("-" + kw) or beat.startswith(kw + "_"):
            return grp
    for cat, _n in cats:
        if cat in CATEGORY_GROUP:
            return CATEGORY_GROUP[cat]
    return "grp_unassigned"


def humanise(key):
    if key in LABELS:
        return LABELS[key][0]
    if key.startswith("osint_"):
        return f"OSINT pivots — {key[len('osint_'):].replace('-', ' ').replace('_', ' ')}"
    if key.startswith("hp") and "_" in key:
        pfx, beat = key.split("_", 1)
        batch = pfx.replace("hp3b", "").replace("hp3", "").replace("hp2", "").replace("hp", "")
        batch = "".join(ch for ch in batch if ch.isdigit())
        return f"{beat} (batch {batch})" if batch else f"{beat} ({pfx} table)"
    if "_" in key:
        cc, rest = key.split("_", 1)
        if cc in COUNTRY:
            return f"{COUNTRY[cc]} — {rest.replace('-', ' ').replace('_', ' ')}"
    return key.replace("_", " ").replace("-", " ")


def cstr(s):
    if s is None:
        return "NULL"
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    rows = []
    for line in open(sys.argv[1], encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#") or line.startswith("key\t"):
            continue
        key, n, cats = (line.split("\t") + ["", ""])[:3]
        parsed = []
        for part in cats.split(";"):
            if ":" in part:
                c, cnt = part.rsplit(":", 1)
                parsed.append((c, int(cnt)))
        rows.append((key, int(n or 0), parsed))

    out = []
    out.append("/* core/collectors.def — display metadata and GROUPING for collector keys.")
    out.append(" *")
    out.append(" * GENERATED by tools/gen_collectors_def.py from the collector census")
    out.append(" * (docs/source-pages-revamp/collector-census-*.tsv); edit the rules there, or")
    out.append(" * this file directly for a one-off label — the generator refuses nothing and")
    out.append(" * a hand edit survives until the next regeneration, so prefer the rules.")
    out.append(" *")
    out.append(" * `source_def.collector` is a MACHINE KEY: the beat a source was authored and")
    out.append(" * probed in (hpengine rows default to their table's file name). Every key the")
    out.append(" * registry carries is listed here with a PARENT: one of the `grp_*` GROUP")
    out.append(" * entries below (parent NULL). A group is a natural-language heading the")
    out.append(" * Collectors pages and the iOS dashboard nest collectors under; a collector")
    out.append(" * the server meets that is NOT listed here is shown under `grp_unassigned`,")
    out.append(" * visibly, and tests/unit/test_sourceapi.c fails while any exists.")
    out.append(" *")
    out.append(" *   COLLECTOR(key, label, label_ja, description, parent)")
    out.append(" */")
    out.append("")
    out.append("/* ── groups ──────────────────────────────────────────────────────────── */")
    for k, l, lj, d in GROUPS:
        out.append(f"COLLECTOR({cstr(k)}, {cstr(l)}, {cstr(lj)},\n          {cstr(d)}, NULL)")
    out.append('COLLECTOR("grp_unassigned", "Unassigned — add to core/collectors.def", "未分類",\n'
               '          "Collectors the registry carries that collectors.def does not list yet. Should be empty.", NULL)')
    out.append("")
    out.append("/* ── collectors (parent = group) ─────────────────────────────────────── */")
    unassigned = []
    by_group = {}
    for key, n, cats in sorted(rows, key=lambda r: (-r[1], r[0])):
        grp = assign(key, cats)
        if grp == "grp_unassigned":
            unassigned.append(key)
        by_group.setdefault(grp, []).append((key, n, cats))
    for grp in [g[0] for g in GROUPS] + ["grp_unassigned"]:
        members = by_group.get(grp, [])
        if not members:
            continue
        label = next((g[1] for g in GROUPS if g[0] == grp), "Unassigned")
        out.append(f"/* {label}: {len(members)} collector(s), {sum(m[1] for m in members)} sources */")
        for key, n, cats in members:
            lab, lj, desc = LABELS.get(key, (humanise(key), None, None))
            if key.startswith("osint_") and key not in LABELS:
                lj = f"OSINTピボット — {key[len('osint_'):]}"
                desc = f"Hand-written entity pivots whose category is {key[len('osint_'):]} (collectors/sources/, DEFR/DEF rows)."
            top = ", ".join(f"{c} {cnt}" for c, cnt in cats[:3])
            comment = f"  /* {n} src; {top} */" if top else f"  /* {n} src */"
            out.append(f"COLLECTOR({cstr(key)}, {cstr(lab)}, {cstr(lj)}, {cstr(desc)}, {cstr(grp)}){comment}")
        out.append("")
    sys.stdout.write("\n".join(out).rstrip("\n") + "\n")
    print(f"# {len(rows)} collectors, {len(GROUPS)} groups, {len(unassigned)} unassigned", file=sys.stderr)
    if unassigned:
        print("UNASSIGNED: " + " ".join(unassigned), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
