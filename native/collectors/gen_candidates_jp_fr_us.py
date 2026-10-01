#!/usr/bin/env python3
"""Build the JP/FR/US high-penetrancy candidate manifest (batch 14).

This emits CANDIDATES, not verified sources. The distinction matters and is
the reason this script exists separately from gen_verified_sources.py:

  gen_verified_sources.py renders rows that verify_feeds.py already fetched
  and parsed. Its file headers assert "returned 2xx ... at generation time"
  and that assertion has to stay true.

  This script renders rows that have NOT been fetched, because the build
  environment they were authored in has no egress (the agent proxy denies
  CONNECT to every host outside github/npm/pypi). Every row is therefore
  marked verified=no with a recorded reason, and the collectors generated
  from it carry an UNVERIFIED header.

Endpoints here are not invented hostnames. Each generator below is one of:

  (a) a documented platform API contract (CKAN action API, Socrata discovery
      and metadata APIs, OpenDataSoft Explore v2.1, JMA bosai JSON) applied
      to a host that this repository has ALREADY proven live -- the proven
      base paths were mined out of docs/verified-sources-manifest.tsv; or
  (b) a documented national API whose contract is stable and public
      (geo.api.gouv.fr, data.gouv.fr, recherche-entreprises.api.gouv.fr,
      Caltrans CWWP district feeds).

That keeps the dead-endpoint rate low, but it does not make them verified.
Run `make verify-candidates` once egress is available to promote the
survivors into the verified manifest.

Usage:
  python3 gen_candidates_jp_fr_us.py --out ../../docs/candidate-sources-batch14.tsv \
      --exclude-urls <file of urls already in the tree> \
      --exclude-ids  <file of source ids already registered>
"""
import argparse
import os
import sys

# Rows accumulate here as dicts matching the manifest column order.
ROWS = []

COLUMNS = ["id", "url", "kind", "items", "name", "name_ja", "collector",
           "category", "lang", "tags", "interval", "description",
           "verified", "verified_note"]

# Why every row in this batch says verified=no. Recorded per row rather than
# only in this docstring so the TSV is self-describing when it is read back
# by the promotion tool.
UNVERIFIED_NOTE = "egress-blocked-at-authoring"


def add(sid, url, kind, name, name_ja, collector, category, lang, tags,
        interval, description):
    ROWS.append(dict(
        id=sid, url=url, kind=kind, items="", name=name, name_ja=name_ja,
        collector=collector, category=category, lang=lang,
        tags=",".join(tags), interval=str(interval), description=description,
        verified="no", verified_note=UNVERIFIED_NOTE))


# --------------------------------------------------------------- Japan / JMA

# JMA publishes one JSON document per forecast-office code under /bosai/.
# These are the real office codes (the same list the agency's own site uses);
# the tree already consumes /forecast/data/forecast/<code>.json for all of
# them, so the generators below take the sibling documents that are not yet
# consumed. Anything that collides is dropped by the URL exclusion pass.
JMA_OFFICES = [
    ("011000", "宗谷地方", "Souya, Hokkaido"),
    ("012000", "上川・留萌地方", "Kamikawa/Rumoi, Hokkaido"),
    ("013000", "網走・北見・紋別地方", "Abashiri/Kitami/Monbetsu, Hokkaido"),
    ("014100", "釧路・根室・十勝地方", "Kushiro/Nemuro/Tokachi, Hokkaido"),
    ("015000", "十勝地方", "Tokachi, Hokkaido"),
    ("016000", "石狩・空知・後志地方", "Ishikari/Sorachi/Shiribeshi, Hokkaido"),
    ("017000", "胆振・日高地方", "Iburi/Hidaka, Hokkaido"),
    ("020000", "青森県", "Aomori Prefecture"),
    ("030000", "岩手県", "Iwate Prefecture"),
    ("040000", "宮城県", "Miyagi Prefecture"),
    ("050000", "秋田県", "Akita Prefecture"),
    ("060000", "山形県", "Yamagata Prefecture"),
    ("070000", "福島県", "Fukushima Prefecture"),
    ("080000", "茨城県", "Ibaraki Prefecture"),
    ("090000", "栃木県", "Tochigi Prefecture"),
    ("100000", "群馬県", "Gunma Prefecture"),
    ("110000", "埼玉県", "Saitama Prefecture"),
    ("120000", "千葉県", "Chiba Prefecture"),
    ("130000", "東京都", "Tokyo Metropolis"),
    ("140000", "神奈川県", "Kanagawa Prefecture"),
    ("150000", "新潟県", "Niigata Prefecture"),
    ("160000", "富山県", "Toyama Prefecture"),
    ("170000", "石川県", "Ishikawa Prefecture"),
    ("180000", "福井県", "Fukui Prefecture"),
    ("190000", "山梨県", "Yamanashi Prefecture"),
    ("200000", "長野県", "Nagano Prefecture"),
    ("210000", "岐阜県", "Gifu Prefecture"),
    ("220000", "静岡県", "Shizuoka Prefecture"),
    ("230000", "愛知県", "Aichi Prefecture"),
    ("240000", "三重県", "Mie Prefecture"),
    ("250000", "滋賀県", "Shiga Prefecture"),
    ("260000", "京都府", "Kyoto Prefecture"),
    ("270000", "大阪府", "Osaka Prefecture"),
    ("280000", "兵庫県", "Hyogo Prefecture"),
    ("290000", "奈良県", "Nara Prefecture"),
    ("300000", "和歌山県", "Wakayama Prefecture"),
    ("310000", "鳥取県", "Tottori Prefecture"),
    ("320000", "島根県", "Shimane Prefecture"),
    ("330000", "岡山県", "Okayama Prefecture"),
    ("340000", "広島県", "Hiroshima Prefecture"),
    ("350000", "山口県", "Yamaguchi Prefecture"),
    ("360000", "徳島県", "Tokushima Prefecture"),
    ("370000", "香川県", "Kagawa Prefecture"),
    ("380000", "愛媛県", "Ehime Prefecture"),
    ("390000", "高知県", "Kochi Prefecture"),
    ("400000", "福岡県", "Fukuoka Prefecture"),
    ("410000", "佐賀県", "Saga Prefecture"),
    ("420000", "長崎県", "Nagasaki Prefecture"),
    ("430000", "熊本県", "Kumamoto Prefecture"),
    ("440000", "大分県", "Oita Prefecture"),
    ("450000", "宮崎県", "Miyazaki Prefecture"),
    ("460100", "鹿児島県", "Kagoshima Prefecture"),
    ("471000", "沖縄本島地方", "Okinawa Main Island"),
    ("472000", "大東島地方", "Daito Islands, Okinawa"),
    ("473000", "宮古島地方", "Miyakojima, Okinawa"),
    ("474000", "八重山地方", "Yaeyama, Okinawa"),
]


def gen_jma():
    for code, ja, en in JMA_OFFICES:
        add(f"jp-jma-warning-{code}",
            f"https://www.jma.go.jp/bosai/warning/data/warning/{code}.json",
            "json-object",
            f"JMA Warnings and Advisories — {en}",
            f"気象庁 警報・注意報 {ja}",
            "jp_prefectural", "disaster", "ja",
            ["japan", "prefecture", "jma", "warning", "disaster", code],
            900,
            f"Live warning/advisory state for every municipality in {en} "
            f"(JMA office {code}): warning codes, issue times and the "
            f"per-area breakdown as published by the Japan Meteorological Agency.")

        add(f"jp-jma-overview-week-{code}",
            f"https://www.jma.go.jp/bosai/forecast/data/overview_week/{code}.json",
            "json-object",
            f"JMA Weekly Outlook Narrative — {en}",
            f"気象庁 週間天気概況 {ja}",
            "jp_prefectural", "weather", "ja",
            ["japan", "prefecture", "jma", "forecast", "weekly", code],
            10800,
            f"Weekly forecast narrative issued by the JMA office for {en} "
            f"({code}): headline text, publishing office and report timestamp.")


# ------------------------------------------------------- Japan / open data

# Proven-live CKAN bases, mined from docs/verified-sources-manifest.tsv. The
# action names below are core CKAN API, present on every CKAN deployment.
JP_CKAN = [
    ("tokyo", "https://catalog.data.metro.tokyo.lg.jp/api/3/action/",
     "Tokyo Metropolitan Government Open Data", "東京都オープンデータカタログ"),
    ("yokohama", "https://data.city.yokohama.lg.jp/api/3/action/",
     "City of Yokohama Open Data", "横浜市オープンデータ"),
    ("geospatial", "https://www.geospatial.jp/ckan/api/3/action/",
     "G-Spatial Information Center (Japan)", "G空間情報センター"),
    # BODIK is the shared CKAN that a large number of Japanese prefectures and
    # municipalities publish through, so one host covers many local bodies.
    ("bodik", "https://data.bodik.jp/api/3/action/",
     "BODIK Local Government Open Data (Japan)", "BODIK 自治体オープンデータ"),
]

# Registry endpoints: the catalogue's own shape. These are the highest-
# penetrancy calls on a CKAN -- one request enumerates every publisher,
# group, tag or dataset in the portal.
CKAN_REGISTRY = [
    ("organization-list", "organization_list?all_fields=true&limit=1000",
     "json:result", "Publisher Registry", "提供組織一覧",
     "Every publishing organisation in the portal with title, description and dataset count."),
    ("group-list", "group_list?all_fields=true",
     "json:result", "Group Registry", "グループ一覧",
     "Every thematic group defined in the portal, with dataset counts."),
    ("tag-list", "tag_list?all_fields=false",
     "json:result", "Tag Vocabulary", "タグ語彙",
     "The portal's full free-tag vocabulary — a cheap map of what the publisher covers."),
    ("license-list", "license_list",
     "json:result", "Licence Registry", "ライセンス一覧",
     "Licences in use across the portal's datasets."),
    ("recent-changes", "recently_changed_packages_activity_list?limit=100",
     "json:result", "Recent Dataset Activity", "最近の更新",
     "The most recent create/update activity across the whole catalogue — a change feed for the portal."),
]

JP_CKAN_TOPICS = [
    ("bousai", "防災", "disaster preparedness", "disaster"),
    ("jinko", "人口", "population", "statistics"),
    ("koutsuu", "交通", "transport", "transport"),
    ("iryou", "医療", "healthcare", "health"),
    ("kyouiku", "教育", "education", "education"),
    ("gakkou", "学校", "schools", "education"),
    ("shisetsu", "施設", "public facilities", "infrastructure"),
    ("kankou", "観光", "tourism", "tourism"),
    ("kankyou", "環境", "environment", "environment"),
    ("toukei", "統計", "statistics", "statistics"),
    ("zaisei", "財政", "public finance", "economy"),
    ("nyuusatsu", "入札", "procurement and tenders", "procurement"),
    ("hinanjo", "避難所", "evacuation shelters", "disaster"),
    ("suidou", "水道", "water supply", "infrastructure"),
    ("gomi", "ごみ", "waste collection", "environment"),
    ("chiiki", "地域", "regional planning", "government"),
    ("anzen", "安全", "public safety", "security"),
    ("kasen", "河川", "rivers and flood control", "disaster"),
    ("doro", "道路", "roads", "infrastructure"),
    ("hoikuen", "保育園", "childcare facilities", "education"),
    ("byouin", "病院", "hospitals", "health"),
    ("keisatsu", "警察", "policing", "security"),
    ("shoubou", "消防", "fire and rescue", "disaster"),
    ("jishin", "地震", "earthquakes", "disaster"),
    ("tsunami", "津波", "tsunami", "disaster"),
    ("dosha", "土砂災害", "landslide hazard", "disaster"),
    ("kouzui", "洪水", "flooding", "disaster"),
    ("kion", "気温", "temperature", "weather"),
    ("denryoku", "電力", "electricity", "energy"),
    ("tsuushin", "通信", "telecommunications", "telecom"),
    ("joho", "情報システム", "information systems", "technology"),
    ("dejitaru", "デジタル", "digital government", "technology"),
    ("kigyou", "企業", "enterprises", "corporate"),
    ("sangyou", "産業", "industry", "industry"),
    ("koujou", "工場", "factories", "industry"),
    ("kouwan", "港湾", "ports", "maritime"),
    ("kuukou", "空港", "airports", "aviation"),
    ("tetsudou", "鉄道", "railways", "transport"),
    ("basu", "バス", "bus services", "transport"),
    ("chuusha", "駐車場", "parking", "transport"),
]


def gen_jp_ckan():
    for slug, base, portal_en, portal_ja in JP_CKAN:
        for key, action, kind, label_en, label_ja, desc in CKAN_REGISTRY:
            add(f"jp-ckan-{slug}-{key}", base + action, kind,
                f"{portal_en} — {label_en}",
                f"{portal_ja} {label_ja}",
                "jp_prefectural", "opendata", "ja",
                ["japan", "opendata", "ckan", "registry", slug],
                86400,
                f"{desc} Source: {portal_en}.")

        for tslug, term_ja, term_en, category in JP_CKAN_TOPICS:
            add(f"jp-ckan-{slug}-q-{tslug}",
                base + f"package_search?rows=100&q={term_ja}",
                "json:result.results",
                f"{portal_en} — {term_en.title()} Datasets",
                f"{portal_ja} {term_ja}データセット",
                "jp_prefectural", category, "ja",
                ["japan", "opendata", "ckan", tslug, slug],
                86400,
                f"Every dataset in {portal_en} matching '{term_ja}' "
                f"({term_en}), with publisher, licence, resources and "
                f"update time as held in the catalogue.")


# --------------------------------------------------------------- France

# Metropolitan departments plus the four DROM that geo.api.gouv.fr serves.
FR_DEPARTMENTS = (
    [f"{i:02d}" for i in range(1, 20)] + ["2A", "2B"] +
    [f"{i:02d}" for i in range(21, 96)] + ["971", "972", "973", "974", "976"]
)

# geo.api.gouv.fr is the French state's official geographic reference API and
# needs no key. One call per department enumerates every commune in it with
# INSEE code, postal codes, SIREN, centroid, population and EPCI linkage --
# the backbone for resolving any French local-government entity.
GEO_FIELDS = ("nom,code,codesPostaux,siren,centre,surface,population,"
              "codeEpci,codeDepartement,codeRegion")


def gen_fr_communes():
    for dep in FR_DEPARTMENTS:
        add(f"fr-geoapi-communes-{dep.lower()}",
            f"https://geo.api.gouv.fr/communes?codeDepartement={dep}"
            f"&fields={GEO_FIELDS}&format=json",
            "json-array",
            f"France — Communes of Department {dep}",
            f"フランス {dep}県 全市町村",
            "fr_prefectural", "government", "fr",
            ["france", "commune", "prefecture", "registry", "geoapi", dep],
            604800,
            f"Every commune in French department {dep} with INSEE code, "
            f"postal codes, SIREN identifier, centroid, area, population and "
            f"EPCI/region linkage — the official state geographic reference.")


FR_DATAGOUV_TOPICS = [
    ("securite", "sécurité", "security", "security"),
    ("delinquance", "délinquance", "crime statistics", "crime"),
    ("prefecture", "préfecture", "prefecture", "government"),
    ("education", "éducation", "education", "education"),
    ("ecole", "école", "schools", "education"),
    ("universite", "université", "universities", "education"),
    ("entreprise", "entreprise", "enterprises", "corporate"),
    ("emploi", "emploi", "employment", "economy"),
    ("marches-publics", "marchés publics", "public procurement", "procurement"),
    ("subvention", "subvention", "public subsidies", "transparency"),
    ("budget", "budget", "public budgets", "economy"),
    ("elus", "élus", "elected officials", "government"),
    ("sante", "santé", "health", "health"),
    ("hopital", "hôpital", "hospitals", "health"),
    ("transport", "transport", "transport", "transport"),
    ("mobilite", "mobilité", "mobility", "transport"),
    ("energie", "énergie", "energy", "energy"),
    ("environnement", "environnement", "environment", "environment"),
    ("risques", "risques naturels", "natural hazards", "disaster"),
    ("inondation", "inondation", "flooding", "disaster"),
    ("logement", "logement", "housing", "geospatial"),
    ("urbanisme", "urbanisme", "urban planning", "geospatial"),
    ("cadastre", "cadastre", "land registry", "geospatial"),
    ("population", "population", "population", "statistics"),
    ("commune", "commune", "communes", "government"),
    ("cybersecurite", "cybersécurité", "cyber security", "cyber"),
    ("numerique", "numérique", "digital services", "technology"),
    ("telecom", "télécommunications", "telecommunications", "telecom"),
    ("reseau", "réseau", "networks", "infrastructure"),
    ("eau", "eau", "water", "environment"),
    ("dechets", "déchets", "waste", "environment"),
    ("agriculture", "agriculture", "agriculture", "agriculture"),
    ("tourisme", "tourisme", "tourism", "tourism"),
    ("culture", "culture", "culture", "culture"),
    ("police", "police", "policing", "security"),
    ("gendarmerie", "gendarmerie", "gendarmerie", "security"),
    ("pompiers", "pompiers", "fire and rescue", "disaster"),
    ("defense", "défense", "defence", "security"),
    ("justice", "justice", "justice", "legal"),
    ("tribunal", "tribunal", "courts", "legal"),
    ("douane", "douane", "customs", "government"),
    ("aeroport", "aéroport", "airports", "aviation"),
    ("port", "port", "ports", "maritime"),
    ("ferroviaire", "ferroviaire", "rail", "transport"),
    ("velo", "vélo", "cycling", "transport"),
    ("stationnement", "stationnement", "parking", "transport"),
    ("camera", "vidéoprotection", "video surveillance", "cameras"),
    ("capteur", "capteur", "sensors", "infrastructure"),
    ("qualite-air", "qualité de l'air", "air quality", "environment"),
    ("bruit", "bruit", "noise", "environment"),
    ("radon", "radon", "radon", "environment"),
    ("nucleaire", "nucléaire", "nuclear", "energy"),
    ("electricite", "électricité", "electricity", "energy"),
    ("gaz", "gaz", "gas", "energy"),
    ("industrie", "industrie", "industry", "industry"),
    ("icpe", "installation classée", "classified industrial sites", "industry"),
    ("foncier", "foncier", "land ownership", "geospatial"),
    ("immobilier", "immobilier", "real estate", "geospatial"),
    ("association", "association", "non-profits", "civil-society"),
    ("statistiques", "statistiques", "statistics", "statistics"),
    ("collectivite", "collectivité territoriale", "local authorities", "government"),
    ("departement", "département", "departments", "government"),
    ("region", "région", "regions", "government"),
    ("intercommunalite", "intercommunalité", "intercommunal bodies", "government"),
    ("election", "élection", "elections", "government"),
    ("deliberation", "délibération", "council deliberations", "government"),
    ("recensement", "recensement", "census", "statistics"),
    ("siren", "siren", "company identifiers", "corporate"),
    ("etablissement", "établissement", "establishments", "corporate"),
    ("industrie-defense", "industrie de défense", "defence industry", "security"),
    ("aeronautique", "aéronautique", "aeronautics", "aviation"),
    ("spatial", "spatial", "space", "space"),
    ("recherche", "recherche", "research", "education"),
    ("laboratoire", "laboratoire", "laboratories", "education"),
    ("etudiant", "étudiant", "students", "education"),
    ("college", "collège", "middle schools", "education"),
    ("lycee", "lycée", "high schools", "education"),
    ("formation", "formation professionnelle", "vocational training", "education"),
    ("hebergement", "hébergement", "accommodation", "geospatial"),
    ("antenne", "antenne relais", "mobile masts", "telecom"),
]


def gen_fr_datagouv():
    for slug, term, term_en, category in FR_DATAGOUV_TOPICS:
        add(f"fr-datagouv-q-{slug}",
            "https://www.data.gouv.fr/api/1/datasets/?page_size=100"
            f"&q={term.replace(' ', '%20')}",
            "json:data",
            f"data.gouv.fr — {term_en.title()} Datasets",
            f"data.gouv.fr {term_en}データセット",
            "fr_government", category, "fr",
            ["france", "opendata", "datagouv", slug],
            86400,
            f"Datasets published on the French national open-data portal "
            f"matching '{term}' ({term_en}) — with publishing organisation, "
            f"resources, licence, temporal coverage and quality score.")

    # The portal's own registries. Each one call, whole-of-France scope.
    registries = [
        ("organizations", "organizations/?page_size=100",
         "Organisation Registry", "組織レジストリ",
         "Public bodies publishing on data.gouv.fr, with SIRET, badges and dataset counts."),
        ("dataservices", "dataservices/?page_size=100",
         "API Registry", "API レジストリ",
         "Machine-readable APIs registered on data.gouv.fr, with base URLs and access conditions."),
        ("reuses", "reuses/?page_size=100",
         "Reuse Registry", "再利用事例",
         "Applications and analyses built on French public data, linked to their source datasets."),
        ("topics", "topics/?page_size=100",
         "Topic Registry", "トピック一覧",
         "Curated thematic collections on data.gouv.fr."),
    ]
    for slug, path, label_en, label_ja, desc in registries:
        add(f"fr-datagouv-{slug}",
            f"https://www.data.gouv.fr/api/1/{path}",
            "json:data",
            f"data.gouv.fr — {label_en}", f"data.gouv.fr {label_ja}",
            "fr_government", "registry", "fr",
            ["france", "opendata", "datagouv", "registry", slug],
            86400, desc)


# recherche-entreprises.api.gouv.fr is the state's keyless company-search API
# over the SIRENE register: legal unit, SIREN/SIRET, NAF activity code,
# headcount band, officers and establishment addresses.
FR_ENTERPRISE_QUERIES = [
    ("defense", "défense", "defence contractors"),
    ("armement", "armement", "armaments"),
    ("aeronautique", "aéronautique", "aeronautics"),
    ("spatial", "spatial", "space industry"),
    ("nucleaire", "nucléaire", "nuclear industry"),
    ("cybersecurite", "cybersécurité", "cyber security"),
    ("informatique", "informatique", "IT services"),
    ("logiciel", "logiciel", "software"),
    ("datacenter", "datacenter", "data centres"),
    ("telecom", "télécommunications", "telecommunications"),
    ("semiconducteur", "semiconducteur", "semiconductors"),
    ("biotechnologie", "biotechnologie", "biotechnology"),
    ("pharmaceutique", "pharmaceutique", "pharmaceuticals"),
    ("chimie", "chimie", "chemicals"),
    ("energie", "énergie", "energy"),
    ("petrole", "pétrole", "petroleum"),
    ("transport-maritime", "transport maritime", "maritime shipping"),
    ("logistique", "logistique", "logistics"),
    ("banque", "banque", "banking"),
    ("assurance", "assurance", "insurance"),
    ("cryptomonnaie", "cryptomonnaie", "crypto assets"),
    ("securite-privee", "sécurité privée", "private security"),
    ("intelligence-economique", "intelligence économique", "competitive intelligence"),
    ("drone", "drone", "unmanned systems"),
    ("robotique", "robotique", "robotics"),
    ("intelligence-artificielle", "intelligence artificielle", "artificial intelligence"),
    ("laboratoire", "laboratoire", "laboratories"),
    ("ingenierie", "ingénierie", "engineering"),
    ("construction-navale", "construction navale", "shipbuilding"),
    ("ferroviaire", "ferroviaire", "rail industry"),
]


def gen_fr_enterprises():
    for slug, term, term_en in FR_ENTERPRISE_QUERIES:
        add(f"fr-entreprises-{slug}",
            "https://recherche-entreprises.api.gouv.fr/search?q="
            f"{term.replace(' ', '%20')}&per_page=25",
            "json:results",
            f"France Company Register — {term_en.title()}",
            f"フランス企業登記 {term_en}",
            "fr_corporate", "corporate", "fr",
            ["france", "corporate", "sirene", "registry", slug],
            86400,
            f"French legal units matching '{term}' ({term_en}) from the state "
            f"SIRENE-backed company search: SIREN, SIRET, NAF activity code, "
            f"legal form, headcount band, officers and establishment addresses.")


# OpenDataSoft Explore v2.1 is one documented contract; these are the French
# public portals that run it. /catalog/datasets enumerates the whole portal.
FR_ODS_PORTALS = [
    ("education", "data.education.gouv.fr", "French Ministry of Education Open Data", "フランス教育省オープンデータ", "education"),
    ("enseignementsup", "data.enseignementsup-recherche.gouv.fr", "French Higher Education and Research Open Data", "フランス高等教育・研究省", "education"),
    ("paris", "opendata.paris.fr", "City of Paris Open Data", "パリ市オープンデータ", "government"),
    ("iledefrance", "data.iledefrance.fr", "Île-de-France Region Open Data", "イル・ド・フランス地域圏", "government"),
    ("idfmobilites", "data.iledefrance-mobilites.fr", "Île-de-France Mobilités Open Data", "イル・ド・フランス交通", "transport"),
    ("toulouse", "data.toulouse-metropole.fr", "Toulouse Métropole Open Data", "トゥールーズ都市圏", "government"),
    ("rennes", "data.rennesmetropole.fr", "Rennes Métropole Open Data", "レンヌ都市圏", "government"),
    ("nantes", "data.nantesmetropole.fr", "Nantes Métropole Open Data", "ナント都市圏", "government"),
    ("montpellier", "data.montpellier3m.fr", "Montpellier Méditerranée Métropole Open Data", "モンペリエ都市圏", "government"),
    ("bordeaux", "opendata.bordeaux-metropole.fr", "Bordeaux Métropole Open Data", "ボルドー都市圏", "government"),
    ("angers", "data.angers.fr", "Angers Loire Métropole Open Data", "アンジェ都市圏", "government"),
    ("issy", "data.issy.com", "Issy-les-Moulineaux Open Data", "イシー・レ・ムリノー市", "government"),
    ("nouvelleaquitaine", "opendata.nouvelle-aquitaine.fr", "Nouvelle-Aquitaine Region Open Data", "ヌーヴェル＝アキテーヌ地域圏", "government"),
    ("regionsud", "data.regionsud.fr", "Provence-Alpes-Côte d'Azur Region Open Data", "プロヴァンス地域圏", "government"),
    ("centrevaldeloire", "data.centrevaldeloire.fr", "Centre-Val de Loire Region Open Data", "サントル地域圏", "government"),
    ("normandie", "data.normandie.fr", "Normandy Region Open Data", "ノルマンディー地域圏", "government"),
    ("occitanie", "data.laregion.fr", "Occitanie Region Open Data", "オクシタニー地域圏", "government"),
    ("bretagne", "data.bretagne.bzh", "Brittany Region Open Data", "ブルターニュ地域圏", "government"),
    ("grandest", "data.grandest.fr", "Grand Est Region Open Data", "グラン・テスト地域圏", "government"),
    ("ademe", "data.ademe.fr", "ADEME Ecological Transition Agency Open Data", "ADEME 環境エネルギー庁", "environment"),
    ("odre", "odre.opendatasoft.com", "Réseaux Énergies Open Data (RTE/GRTgaz)", "フランス送電網オープンデータ", "energy"),
    ("public", "public.opendatasoft.com", "OpenDataSoft Public Catalogue", "OpenDataSoft 公開カタログ", "opendata"),
    ("boamp", "boamp-datadila.opendatasoft.com", "BOAMP Public Procurement Notices", "BOAMP 公共調達公告", "procurement"),
    ("bodacc", "bodacc-datadila.opendatasoft.com", "BODACC Civil and Commercial Announcements", "BODACC 商業公告", "corporate"),
    ("economie", "data.economie.gouv.fr", "French Ministry of the Economy Open Data", "フランス経済省オープンデータ", "economy"),
    ("agriculture", "data.agriculture.gouv.fr", "French Ministry of Agriculture Open Data", "フランス農業省", "agriculture"),
    ("culture", "data.culture.gouv.fr", "French Ministry of Culture Open Data", "フランス文化省", "culture"),
    ("interieur", "www.data.gouv.fr", "data.gouv.fr OpenDataSoft Mirror", "data.gouv.fr ODSミラー", "government"),
    ("sncf", "ressources.data.sncf.com", "SNCF Open Data", "SNCF オープンデータ", "transport"),
    ("ratp", "data.ratp.fr", "RATP Open Data", "RATP オープンデータ", "transport"),
    ("enedis", "data.enedis.fr", "Enedis Electricity Distribution Open Data", "Enedis 配電データ", "energy"),
    ("cerema", "datalab.cerema.fr", "Cerema Territorial Data", "Cerema 国土データ", "infrastructure"),
]

# The higher-education ministry's establishment reference, sliced by region.
FR_ESR_DATASET = "fr-esr-principaux-etablissements-enseignement-superieur"


def gen_fr_highered():
    for code, name in FR_GEO_REGIONS:
        add(f"fr-highered-etablissements-{code}",
            "https://data.enseignementsup-recherche.gouv.fr/api/explore/v2.1/"
            f"catalog/datasets/{FR_ESR_DATASET}/records?limit=100"
            f"&refine=code_region%3A%22{code}%22",
            "json:results",
            f"France Higher Education Establishments — {name}",
            f"フランス高等教育機関 {name}",
            "fr_education", "education", "fr",
            ["france", "university", "education", "research", code],
            604800,
            f"Universities, grandes écoles and research establishments in the "
            f"{name} region: UAI identifier, official name, type, parent "
            f"institution, address, coordinates and sector.")


def gen_fr_ods():
    for slug, host, name_en, name_ja, category in FR_ODS_PORTALS:
        add(f"fr-ods-{slug}-catalog",
            f"https://{host}/api/explore/v2.1/catalog/datasets?limit=100",
            "json:results",
            f"{name_en} — Dataset Catalogue",
            f"{name_ja} データセット目録",
            "fr_opendata", "registry", "fr",
            ["france", "opendata", "opendatasoft", "registry", slug],
            86400,
            f"Every dataset published by {name_en}, with field schemas, "
            f"record counts, themes, licences and modification times.")

        add(f"fr-ods-{slug}-facets",
            f"https://{host}/api/explore/v2.1/catalog/facets",
            "json:facets",
            f"{name_en} — Catalogue Facets",
            f"{name_ja} ファセット",
            "fr_opendata", category, "fr",
            ["france", "opendata", "opendatasoft", "facets", slug],
            86400,
            f"Theme, keyword, publisher and licence facet counts across "
            f"{name_en} — a one-request map of the portal's subject coverage.")


# The Ministry of Education's national school directory, sliced by department.
# One dataset, but the per-department slice is what makes it usable: the full
# table is ~68k establishments.
FR_SCHOOL_DEPARTMENTS = FR_DEPARTMENTS


def gen_fr_schools():
    for dep in FR_SCHOOL_DEPARTMENTS:
        add(f"fr-schools-annuaire-{dep.lower()}",
            "https://data.education.gouv.fr/api/explore/v2.1/catalog/datasets/"
            "fr-en-annuaire-education/records?limit=100"
            f"&refine=code_departement%3A%22{dep}%22",
            "json:results",
            f"France School Directory — Department {dep}",
            f"フランス学校名簿 {dep}県",
            "fr_education", "education", "fr",
            ["france", "school", "education", "directory", dep],
            604800,
            f"Every state and private school registered in French department "
            f"{dep}: UAI identifier, name, type, level, address, coordinates, "
            f"contact details and education authority.")


def gen_fr_cyber():
    feeds = [
        ("avis", "https://www.cert.ssi.gouv.fr/avis/feed/", "CERT-FR Security Advisories",
         "CERT-FR セキュリティ勧告",
         "Vulnerability advisories issued by the French national CERT (ANSSI)."),
        ("alerte", "https://www.cert.ssi.gouv.fr/alerte/feed/", "CERT-FR Active Alerts",
         "CERT-FR 警報",
         "Active exploitation alerts issued by the French national CERT (ANSSI)."),
        ("actualite", "https://www.cert.ssi.gouv.fr/actualite/feed/", "CERT-FR Bulletins",
         "CERT-FR 情報",
         "Situational bulletins from the French national CERT (ANSSI)."),
        ("ioc", "https://www.cert.ssi.gouv.fr/ioc/feed/", "CERT-FR Indicators of Compromise",
         "CERT-FR 侵害指標",
         "Indicator-of-compromise publications from the French national CERT."),
        ("cti", "https://www.cert.ssi.gouv.fr/cti/feed/", "CERT-FR Threat Intelligence Reports",
         "CERT-FR 脅威情報",
         "Threat-actor and campaign reporting published by the French national CERT."),
    ]
    for slug, url, name_en, name_ja, desc in feeds:
        add(f"fr-certfr-{slug}", url, "rss", name_en, name_ja,
            "fr_cyber", "cyber", "fr",
            ["france", "cyber", "cert", "anssi", "advisory", slug],
            3600, desc)


# ------------------------------------------------------------------- US

US_DATAGOV_TOPICS = [
    ("cybersecurity", "cybersecurity", "cyber"),
    ("critical-infrastructure", "critical infrastructure", "infrastructure"),
    ("telecommunications", "telecommunications", "telecom"),
    ("broadband", "broadband", "telecom"),
    ("spectrum", "radio spectrum", "telecom"),
    ("satellite", "satellite", "space"),
    ("surveillance", "surveillance", "security"),
    ("law-enforcement", "law enforcement", "security"),
    ("crime", "crime statistics", "crime"),
    ("corrections", "corrections", "legal"),
    ("courts", "courts", "legal"),
    ("immigration", "immigration", "government"),
    ("border", "border security", "security"),
    ("defense", "defense", "security"),
    ("veterans", "veterans", "government"),
    ("procurement", "federal procurement", "procurement"),
    ("contracts", "government contracts", "procurement"),
    ("grants", "federal grants", "procurement"),
    ("lobbying", "lobbying", "transparency"),
    ("campaign-finance", "campaign finance", "transparency"),
    ("education", "education", "education"),
    ("schools", "public schools", "education"),
    ("universities", "universities", "education"),
    ("student-loans", "student loans", "education"),
    ("workforce", "workforce", "economy"),
    ("employment", "employment", "economy"),
    ("census", "census", "statistics"),
    ("housing", "housing", "geospatial"),
    ("transportation", "transportation", "transport"),
    ("aviation", "aviation safety", "aviation"),
    ("railroad", "railroad", "transport"),
    ("maritime", "maritime", "maritime"),
    ("pipeline", "pipeline safety", "infrastructure"),
    ("energy", "energy", "energy"),
    ("electric-grid", "electric grid", "energy"),
    ("nuclear", "nuclear", "energy"),
    ("oil-gas", "oil and gas", "energy"),
    ("emissions", "greenhouse gas emissions", "environment"),
    ("water-quality", "water quality", "environment"),
    ("air-quality", "air quality", "environment"),
    ("toxic-release", "toxic release", "environment"),
    ("superfund", "superfund", "environment"),
    ("wildfire", "wildfire", "disaster"),
    ("flood", "flood", "disaster"),
    ("earthquake", "earthquake", "disaster"),
    ("hurricane", "hurricane", "disaster"),
    ("emergency-management", "emergency management", "disaster"),
    ("public-health", "public health", "health"),
    ("hospitals", "hospitals", "health"),
    ("opioid", "opioid", "health"),
    ("food-safety", "food safety", "health"),
    ("agriculture", "agriculture", "agriculture"),
    ("wildlife", "wildlife", "environment"),
    ("land-use", "land use", "geospatial"),
    ("parcels", "parcels", "geospatial"),
    ("elevation", "elevation", "geospatial"),
    ("imagery", "aerial imagery", "geospatial"),
    ("weather", "weather", "weather"),
    ("climate", "climate", "environment"),
    ("banking", "banking", "finance"),
    ("securities", "securities", "finance"),
    ("consumer-complaints", "consumer complaints", "transparency"),
    ("sanctions", "sanctions", "sanctions"),
    ("trade", "international trade", "economy"),
    ("tariffs", "tariffs", "economy"),
    ("supply-chain", "supply chain", "economy"),
    ("semiconductors", "semiconductors", "industry"),
    ("manufacturing", "manufacturing", "industry"),
    ("mining", "mining", "industry"),
    ("chemicals", "chemical facilities", "industry"),
    ("ports", "seaports", "maritime"),
    ("shipping", "shipping", "maritime"),
    ("airports", "airports", "aviation"),
    ("air-traffic", "air traffic", "aviation"),
    ("drones", "unmanned aircraft", "aviation"),
    ("space", "space launch", "space"),
    ("gps", "global positioning", "space"),
    ("radio", "radio licenses", "telecom"),
    ("wireless", "wireless facilities", "telecom"),
    ("fiber", "fiber optic", "telecom"),
    ("data-centers", "data centers", "technology"),
    ("cloud", "cloud services", "technology"),
    ("artificial-intelligence", "artificial intelligence", "technology"),
    ("software", "software inventory", "technology"),
    ("it-spending", "information technology spending", "technology"),
    ("privacy", "privacy impact assessment", "transparency"),
    ("foia", "freedom of information", "transparency"),
    ("inspector-general", "inspector general", "transparency"),
    ("audit", "audit reports", "transparency"),
    ("enforcement", "enforcement actions", "legal"),
    ("debarment", "debarment", "sanctions"),
    ("export-control", "export control", "sanctions"),
    ("money-laundering", "money laundering", "finance"),
    ("fraud", "fraud", "crime"),
    ("human-trafficking", "human trafficking", "crime"),
    ("firearms", "firearms", "security"),
    ("hazmat", "hazardous materials", "disaster"),
    ("radiation", "radiation monitoring", "environment"),
    ("seismic", "seismic monitoring", "disaster"),
    ("tsunami", "tsunami", "disaster"),
    ("drought", "drought", "environment"),
    ("groundwater", "groundwater", "environment"),
    ("coastal", "coastal zone", "maritime"),
    ("fisheries", "fisheries", "maritime"),
]


def gen_us_datagov():
    base = "https://catalog.data.gov/api/3/action/"
    for slug, term, category in US_DATAGOV_TOPICS:
        add(f"us-datagov-q-{slug}",
            base + f"package_search?rows=100&q={term.replace(' ', '%20')}",
            "json:result.results",
            f"data.gov — {term.title()} Datasets",
            f"data.gov {term} データセット",
            "us_government", category, "en",
            ["usa", "opendata", "ckan", "datagov", slug],
            86400,
            f"Federal, state and local datasets on the US national catalogue "
            f"matching '{term}', with publishing agency, distribution "
            f"resources, licence and update cadence.")

    for key, action, kind, label_en, label_ja, desc in CKAN_REGISTRY:
        add(f"us-datagov-{key}", base + action, kind,
            f"data.gov — {label_en}", f"data.gov {label_ja}",
            "us_government", "registry", "en",
            ["usa", "opendata", "ckan", "registry", key],
            86400, f"{desc} Source: the US national open-data catalogue.")


# Socrata domains. Two documented contracts per domain: the federated
# discovery API (api.us.socrata.com, which answers for any domain) and the
# portal's own metadata endpoint.
US_SOCRATA = [
    ("nyc", "data.cityofnewyork.us", "New York City Open Data", "ニューヨーク市"),
    ("nystate", "data.ny.gov", "New York State Open Data", "ニューヨーク州"),
    ("nyhealth", "health.data.ny.gov", "New York State Health Data", "ニューヨーク州保健"),
    ("chicago", "data.cityofchicago.org", "City of Chicago Open Data", "シカゴ市"),
    ("cook", "datacatalog.cookcountyil.gov", "Cook County Open Data", "クック郡"),
    ("illinois", "data.illinois.gov", "State of Illinois Open Data", "イリノイ州"),
    ("austin", "data.austintexas.gov", "City of Austin Open Data", "オースティン市"),
    ("texas", "data.texas.gov", "State of Texas Open Data", "テキサス州"),
    ("dallas", "www.dallasopendata.com", "City of Dallas Open Data", "ダラス市"),
    ("seattle", "data.seattle.gov", "City of Seattle Open Data", "シアトル市"),
    ("kingcounty", "data.kingcounty.gov", "King County Open Data", "キング郡"),
    ("washington", "data.wa.gov", "Washington State Open Data", "ワシントン州"),
    ("oregon", "data.oregon.gov", "State of Oregon Open Data", "オレゴン州"),
    ("california", "data.ca.gov", "State of California Open Data", "カリフォルニア州"),
    ("losangeles", "data.lacity.org", "City of Los Angeles Open Data", "ロサンゼルス市"),
    ("sandiego", "data.sandiego.gov", "City of San Diego Open Data", "サンディエゴ市"),
    ("sfgov", "data.sfgov.org", "City of San Francisco Open Data", "サンフランシスコ市"),
    ("oakland", "data.oaklandca.gov", "City of Oakland Open Data", "オークランド市"),
    ("colorado", "data.colorado.gov", "State of Colorado Open Data", "コロラド州"),
    ("utah", "opendata.utah.gov", "State of Utah Open Data", "ユタ州"),
    ("hawaii", "data.hawaii.gov", "State of Hawaii Open Data", "ハワイ州"),
    ("maryland", "opendata.maryland.gov", "State of Maryland Open Data", "メリーランド州"),
    ("baltimore", "data.baltimorecity.gov", "City of Baltimore Open Data", "ボルチモア市"),
    ("montgomery", "data.montgomerycountymd.gov", "Montgomery County Open Data", "モンゴメリー郡"),
    ("virginia", "data.virginia.gov", "Commonwealth of Virginia Open Data", "バージニア州"),
    ("connecticut", "data.ct.gov", "State of Connecticut Open Data", "コネチカット州"),
    ("delaware", "data.delaware.gov", "State of Delaware Open Data", "デラウェア州"),
    ("newjersey", "data.nj.gov", "State of New Jersey Open Data", "ニュージャージー州"),
    ("pennsylvania", "data.pa.gov", "Commonwealth of Pennsylvania Open Data", "ペンシルベニア州"),
    ("michigan", "data.michigan.gov", "State of Michigan Open Data", "ミシガン州"),
    ("iowa", "data.iowa.gov", "State of Iowa Open Data", "アイオワ州"),
    ("missouri", "data.mo.gov", "State of Missouri Open Data", "ミズーリ州"),
    ("kansascity", "data.kcmo.org", "Kansas City Open Data", "カンザスシティ"),
    ("nashville", "data.nashville.gov", "Metro Nashville Open Data", "ナッシュビル市"),
    ("louisville", "data.louisvilleky.gov", "Louisville Metro Open Data", "ルイビル市"),
    ("cambridge", "data.cambridgema.gov", "City of Cambridge Open Data", "ケンブリッジ市"),
    ("boston", "data.boston.gov", "City of Boston Open Data", "ボストン市"),
    ("cdc", "data.cdc.gov", "US CDC Open Data", "米国CDC"),
    ("chronicdata", "chronicdata.cdc.gov", "US CDC Chronic Disease Data", "米国CDC 慢性疾患"),
    ("healthdata", "healthdata.gov", "US HealthData.gov", "米国保健データ"),
    ("medicare", "data.cms.gov", "US Centers for Medicare and Medicaid Data", "米国CMS"),
    ("fcc", "opendata.fcc.gov", "US FCC Open Data", "米国FCC"),
    ("dot", "data.transportation.gov", "US DOT Open Data", "米国運輸省"),
    ("bts", "data.bts.gov", "US Bureau of Transportation Statistics", "米国運輸統計局"),
    ("energy", "data.openei.org", "US Open Energy Information", "米国エネルギー情報"),
    ("ojp", "data.ojp.usdoj.gov", "US Office of Justice Programs Data", "米国司法プログラム局"),
    ("hrsa", "data.hrsa.gov", "US Health Resources and Services Data", "米国HRSA"),
    ("performance", "performance.gov", "US Federal Performance Data", "米国政府実績データ"),
]


def gen_us_socrata():
    for slug, host, name_en, name_ja in US_SOCRATA:
        add(f"us-socrata-discovery-{slug}",
            "https://api.us.socrata.com/api/catalog/v1?limit=100"
            f"&domains={host}",
            "json:results",
            f"{name_en} — Asset Discovery",
            f"{name_ja} データ資産一覧",
            "us_state", "registry", "en",
            ["usa", "opendata", "socrata", "discovery", slug],
            86400,
            f"Every dataset, chart, map, filtered view and file published on "
            f"{host}, via the Socrata federated discovery API: identifier, "
            f"owner, category, column schema and update time.")

        add(f"us-socrata-metadata-{slug}",
            f"https://{host}/api/views/metadata/v1?limit=100",
            "json-array",
            f"{name_en} — Asset Metadata",
            f"{name_ja} メタデータ",
            "us_state", "opendata", "en",
            ["usa", "opendata", "socrata", "metadata", slug],
            86400,
            f"Portal-side metadata for the assets hosted on {host}: name, "
            f"description, category, tags, attribution, licence, owner and "
            f"provenance for each view.")


# Caltrans publishes district-level roadway device state as plain JSON with no
# key. Cameras carry the stream/still URL and the device's coordinates, which
# is exactly the camera layer this batch is asked for.
CALTRANS_DISTRICTS = [
    ("01", "Eureka / North Coast"), ("02", "Redding / Northeast"),
    ("03", "Marysville / Sacramento Valley"), ("04", "Bay Area / Oakland"),
    ("05", "San Luis Obispo / Central Coast"), ("06", "Fresno / South Valley"),
    ("07", "Los Angeles / Ventura"), ("08", "San Bernardino / Riverside"),
    ("09", "Bishop / Eastern Sierra"), ("10", "Stockton / Central Valley"),
    ("11", "San Diego / Imperial"), ("12", "Orange County"),
]


def gen_us_cameras():
    for num, region in CALTRANS_DISTRICTS:
        d = str(int(num))
        add(f"us-caltrans-cctv-d{num}",
            f"https://cwwp2.dot.ca.gov/data/d{d}/cctv/cctvStatusD{num}.json",
            "json:data",
            f"Caltrans CCTV Cameras — District {num} ({region})",
            f"カリフォルニア州道路カメラ 第{num}管区",
            "us_cameras", "cameras", "en",
            ["usa", "california", "camera", "cctv", "roadway", f"d{num}"],
            1800,
            f"Every Caltrans traffic camera in District {num} ({region}): "
            f"location name, route and postmile, latitude/longitude, "
            f"in-service state and the current still-image and stream URLs.")

        add(f"us-caltrans-cms-d{num}",
            f"https://cwwp2.dot.ca.gov/data/d{d}/cms/cmsStatusD{num}.json",
            "json:data",
            f"Caltrans Changeable Message Signs — District {num} ({region})",
            f"カリフォルニア州可変情報板 第{num}管区",
            "us_cameras", "transport", "en",
            ["usa", "california", "roadway", "signage", f"d{num}"],
            1800,
            f"Live text displayed on every changeable message sign in "
            f"Caltrans District {num} ({region}), with sign location and "
            f"coordinates — a running feed of incidents and closures.")

        add(f"us-caltrans-lcs-d{num}",
            f"https://cwwp2.dot.ca.gov/data/d{d}/lcs/lcsStatusD{num}.json",
            "json:data",
            f"Caltrans Lane Closures — District {num} ({region})",
            f"カリフォルニア州車線規制 第{num}管区",
            "us_cameras", "transport", "en",
            ["usa", "california", "roadway", "closure", f"d{num}"],
            1800,
            f"Active and planned lane closures in Caltrans District {num} "
            f"({region}): route, postmile range, work type, schedule and the "
            f"closure's coordinates.")


# geo.api.gouv.fr also serves the tiers above the commune. These four calls
# enumerate the whole of French local government at each administrative level.
FR_GEO_REGIONS = [
    ("11", "Île-de-France"), ("24", "Centre-Val de Loire"),
    ("27", "Bourgogne-Franche-Comté"), ("28", "Normandie"),
    ("32", "Hauts-de-France"), ("44", "Grand Est"), ("52", "Pays de la Loire"),
    ("53", "Bretagne"), ("75", "Nouvelle-Aquitaine"), ("76", "Occitanie"),
    ("84", "Auvergne-Rhône-Alpes"), ("93", "Provence-Alpes-Côte d'Azur"),
    ("94", "Corse"), ("01", "Guadeloupe"), ("02", "Martinique"),
    ("03", "Guyane"), ("04", "La Réunion"), ("06", "Mayotte"),
]


def gen_fr_geo_extra():
    tiers = [
        ("departements", "departements?fields=nom,code,codeRegion,chefLieu",
         "Departments", "県一覧",
         "Every French department with its INSEE code, prefecture seat and parent region."),
        ("regions", "regions?fields=nom,code",
         "Regions", "地域圏一覧",
         "Every French region with its INSEE code."),
        ("epcis", "epcis?fields=nom,code,type,financement,population",
         "Intercommunal Bodies (EPCI)", "広域行政組織",
         "Every French EPCI — metropoles, urban and commune communities — with type, funding regime and population."),
    ]
    for slug, path, label_en, label_ja, desc in tiers:
        add(f"fr-geoapi-{slug}", f"https://geo.api.gouv.fr/{path}",
            "json-array",
            f"France — {label_en}", f"フランス {label_ja}",
            "fr_prefectural", "government", "fr",
            ["france", "registry", "geoapi", slug], 604800, desc)

    for code, name in FR_GEO_REGIONS:
        add(f"fr-geoapi-communes-region-{code}",
            f"https://geo.api.gouv.fr/communes?codeRegion={code}"
            f"&fields={GEO_FIELDS}&format=json",
            "json-array",
            f"France — Communes of {name}",
            f"フランス {name} 地域圏 全市町村",
            "fr_prefectural", "government", "fr",
            ["france", "commune", "region", "geoapi", code],
            604800,
            f"Every commune in the {name} region with INSEE code, postal "
            f"codes, SIREN, centroid, population and department linkage.")


# ArcGIS Hub exposes an OGC API Records search across every public ArcGIS Hub
# site — the platform most US state and municipal GIS offices publish through.
US_HUB_TOPICS = [
    ("police-stations", "police station", "security"),
    ("fire-stations", "fire station", "disaster"),
    ("hospitals", "hospital", "health"),
    ("schools", "school", "education"),
    ("colleges", "college campus", "education"),
    ("libraries", "library", "government"),
    ("government-buildings", "government building", "government"),
    ("military", "military installation", "security"),
    ("airports", "airport", "aviation"),
    ("ports", "port facility", "maritime"),
    ("railroads", "railroad", "transport"),
    ("bridges", "bridge", "infrastructure"),
    ("dams", "dam", "infrastructure"),
    ("power-plants", "power plant", "energy"),
    ("substations", "electric substation", "energy"),
    ("pipelines", "pipeline", "infrastructure"),
    ("water-treatment", "water treatment plant", "infrastructure"),
    ("cell-towers", "cell tower", "telecom"),
    ("broadband", "broadband service", "telecom"),
    ("traffic-cameras", "traffic camera", "cameras"),
    ("surveillance-cameras", "surveillance camera", "cameras"),
    ("traffic-counts", "traffic count", "transport"),
    ("parcels", "parcel", "geospatial"),
    ("zoning", "zoning", "geospatial"),
    ("building-footprints", "building footprint", "geospatial"),
    ("addresses", "address point", "geospatial"),
    ("floodplain", "floodplain", "disaster"),
    ("wildfire-risk", "wildfire risk", "disaster"),
    ("evacuation-routes", "evacuation route", "disaster"),
    ("crime-incidents", "crime incident", "crime"),
    ("code-enforcement", "code enforcement", "government"),
    ("voting-precincts", "voting precinct", "government"),
]


def gen_us_arcgis_hub():
    for slug, term, category in US_HUB_TOPICS:
        add(f"us-arcgishub-{slug}",
            "https://hub.arcgis.com/api/search/v1/collections/dataset/items"
            f"?limit=100&q={term.replace(' ', '%20')}",
            "json:features",
            f"ArcGIS Hub — {term.title()} Layers",
            f"ArcGIS Hub {term} レイヤー",
            "us_state", category, "en",
            ["usa", "gis", "arcgis", "hub", "catalog", slug],
            86400,
            f"Public ArcGIS Hub datasets matching '{term}', across US state, "
            f"county and municipal GIS sites: title, owner organisation, "
            f"extent, licence and the service URL each layer is served from.")


# The Socrata discovery API also answers unscoped by domain, which makes it a
# single cross-portal search over every Socrata deployment in the US.
US_SOCRATA_QUERIES = [
    ("body-worn-camera", "body worn camera", "cameras"),
    ("surveillance-technology", "surveillance technology", "security"),
    ("license-plate-reader", "license plate reader", "security"),
    ("police-stops", "police stops", "crime"),
    ("use-of-force", "use of force", "crime"),
    ("arrests", "arrests", "crime"),
    ("calls-for-service", "calls for service", "crime"),
    ("311-requests", "311 service requests", "government"),
    ("building-permits", "building permits", "geospatial"),
    ("business-licenses", "business licenses", "corporate"),
    ("contractor-registry", "contractor registry", "corporate"),
    ("lobbyist-registry", "lobbyist registration", "transparency"),
    ("campaign-contributions", "campaign contributions", "transparency"),
    ("public-salaries", "employee salaries", "transparency"),
    ("budget-expenditures", "budget expenditures", "economy"),
    ("vendor-payments", "vendor payments", "procurement"),
    ("procurement-contracts", "procurement contracts", "procurement"),
    ("school-performance", "school performance", "education"),
    ("school-enrollment", "school enrollment", "education"),
    ("student-demographics", "student demographics", "education"),
    ("hospital-quality", "hospital quality", "health"),
    ("inspections", "restaurant inspections", "health"),
    ("environmental-violations", "environmental violations", "environment"),
    ("air-monitoring", "air monitoring", "environment"),
    ("water-testing", "water testing", "environment"),
    ("traffic-collisions", "traffic collisions", "transport"),
    ("transit-ridership", "transit ridership", "transport"),
    ("street-closures", "street closures", "transport"),
    ("evictions", "evictions", "geospatial"),
    ("property-assessments", "property assessments", "geospatial"),
    ("vacant-properties", "vacant properties", "geospatial"),
    ("broadband-access", "broadband access", "telecom"),
    ("cybersecurity-incidents", "cybersecurity incidents", "cyber"),
    ("emergency-shelters", "emergency shelters", "disaster"),
    ("power-outages", "power outages", "energy"),
]


def gen_us_socrata_search():
    for slug, term, category in US_SOCRATA_QUERIES:
        add(f"us-socrata-search-{slug}",
            "https://api.us.socrata.com/api/catalog/v1?limit=100&q="
            f"{term.replace(' ', '%20')}",
            "json:results",
            f"Socrata Cross-Portal Search — {term.title()}",
            f"Socrata 横断検索 {term}",
            "us_state", category, "en",
            ["usa", "opendata", "socrata", "search", slug],
            86400,
            f"Every Socrata-hosted asset across all US government portals "
            f"matching '{term}': owning domain, dataset identifier, category, "
            f"column schema and last update — a cross-jurisdiction sweep in "
            f"one request.")


GENERATORS = [
    gen_jma, gen_jp_ckan,
    gen_fr_geo_extra, gen_us_arcgis_hub, gen_us_socrata_search,
    gen_fr_communes, gen_fr_datagouv, gen_fr_enterprises, gen_fr_ods,
    gen_fr_schools, gen_fr_cyber, gen_fr_highered,
    gen_us_datagov, gen_us_socrata, gen_us_cameras,
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--exclude-urls")
    ap.add_argument("--exclude-ids")
    args = ap.parse_args()

    for g in GENERATORS:
        before = len(ROWS)
        g()
        print(f"{g.__name__}: {len(ROWS) - before}", file=sys.stderr)

    def load(path):
        if not path or not os.path.exists(path):
            return set()
        return {l.strip() for l in open(path, encoding="utf-8") if l.strip()}

    bad_urls, bad_ids = load(args.exclude_urls), load(args.exclude_ids)
    seen_urls, seen_ids, kept, dropped = set(), set(), [], 0
    for r in ROWS:
        if r["url"] in bad_urls or r["url"] in seen_urls:
            dropped += 1
            continue
        if r["id"] in bad_ids or r["id"] in seen_ids:
            dropped += 1
            continue
        seen_urls.add(r["url"])
        seen_ids.add(r["id"])
        kept.append(r)

    with open(args.out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\t".join(COLUMNS) + "\n")
        for r in kept:
            fh.write("\t".join(r[c].replace("\t", " ") for c in COLUMNS) + "\n")

    print(f"--- wrote {len(kept)} candidates ({dropped} deduped) -> {args.out}",
          file=sys.stderr)


if __name__ == "__main__":
    main()
