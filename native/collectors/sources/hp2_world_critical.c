/* collectors/sources/hp2_world_critical.c — cross-border critical values.
 *
 * The rows here are the ones that do not belong to any single region because
 * the obligation they encode is extraterritorial: a multilateral development
 * bank debarment follows a firm into every member country, an FFI's FATCA
 * registration binds it to US reporting wherever it sits, Japan's foreign
 * end-user list constrains exports from Japan to entities in third countries,
 * and a facility on a supply-chain register is a physical site with a named
 * operator regardless of who owns the brand above it.
 *
 * These are deliberately the checks that cannot be substituted by a national
 * register lookup. */
#include "lib/hpengine.h"

static const hp_source HP2_WORLD_CRITICAL[] = {
  /* ── Multilateral development bank debarment ───────────────────────────── */
  { .id = "ADB_SANCTIONS_LIST", .name = "Asian Development Bank — sanctioned entities",
    .name_ja = "アジア開発銀行 制裁対象", .category = "government",
    .portal = "https://www.adb.org", .record_type = "mdb-debarment",
    .tags = "\"adb\",\"debarment\",\"development\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.adb.org/site/integrity/sanctions?search={q}",
    .base = "https://www.adb.org", .filter_query = 1,
    .description = "Firms and individuals debarred by the ADB for fraud, "
      "corruption, collusion or obstruction — with the sanction type and its "
      "period. Under the cross-debarment agreement an ADB debarment is "
      "enforced by the World Bank, EBRD, IDB and AfDB as well" },

  { .id = "AFDB_DEBARMENT_LIST", .name = "African Development Bank — debarred entities",
    .name_ja = "アフリカ開発銀行 排除対象", .category = "government",
    .portal = "https://www.afdb.org", .record_type = "mdb-debarment",
    .tags = "\"afdb\",\"debarment\",\"development\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.afdb.org/en/search?keys={q}",
    .base = "https://www.afdb.org", .filter_query = 1,
    .description = "AfDB debarment and cross-debarment decisions naming the "
      "sanctioned firm, its affiliates and the ineligibility period — the "
      "African leg of the multilateral cross-debarment regime, and the one "
      "least represented in commercial screening data" },

  { .id = "AIIB_PROJECT_DISCLOSURE", .name = "AIIB — project disclosure & debarment",
    .name_ja = "アジアインフラ投資銀行 案件開示", .category = "government",
    .portal = "https://www.aiib.org", .record_type = "aiib-project",
    .tags = "\"aiib\",\"development\",\"finance\",\"infrastructure\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.aiib.org/en/projects/list/index.html?q={q}",
    .base = "https://www.aiib.org", .filter_query = 1,
    .description = "AIIB-financed infrastructure projects with the borrower, "
      "the implementing agency, the sector and the financing amount, plus the "
      "bank's own list of debarred entities. AIIB lending sits behind a large "
      "share of new Asian energy and transport assets" },

  { .id = "UN_VENDOR_INELIGIBILITY", .name = "United Nations — ineligible vendors",
    .name_ja = "国連 取引不適格ベンダー", .category = "government",
    .portal = "https://www.ungm.org", .record_type = "un-debarment",
    .tags = "\"un\",\"debarment\",\"procurement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.ungm.org/Public/UNGMVendorIneligibility?q={q}",
    .base = "https://www.ungm.org", .filter_query = 1,
    .description = "Vendors suspended or removed from UN procurement, with the "
      "sanctioning UN entity, the basis and the duration. A vendor barred here "
      "is barred across the whole UN system, which is a very large buyer" },

  /* ── AML, tax and financial-integrity regimes ──────────────────────────── */
  { .id = "FATF_MONITORED_JURISDICTIONS", .name = "FATF — high-risk & monitored jurisdictions",
    .name_ja = "FATF 高リスク/監視対象法域", .category = "government",
    .portal = "https://www.fatf-gafi.org", .record_type = "fatf-listing",
    .tags = "\"fatf\",\"aml\",\"jurisdiction-risk\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.fatf-gafi.org/en/search.html?q={q}",
    .base = "https://www.fatf-gafi.org", .filter_query = 1,
    .description = "FATF statements on jurisdictions under increased "
      "monitoring or subject to a call for action, plus the mutual evaluation "
      "and follow-up reports behind them. Jurisdiction listing drives enhanced "
      "due diligence obligations in every FATF member" },

  { .id = "BASEL_AML_INDEX", .name = "Basel AML Index — jurisdiction risk scores",
    .name_ja = "バーゼル AML指数", .category = "research",
    .portal = "https://index.baselgovernance.org", .record_type = "aml-risk-score",
    .tags = "\"aml\",\"jurisdiction-risk\",\"research\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://index.baselgovernance.org/ranking?q={q}",
    .base = "https://index.baselgovernance.org", .filter_query = 1,
    .description = "Independent money-laundering and terrorist-financing risk "
      "scores by country, with the component indicators for AML framework "
      "quality, corruption, financial transparency and rule of law — the "
      "quantitative counterweight to the political FATF listing process" },

  { .id = "IRS_FATCA_FFI_LIST", .name = "IRS — FATCA foreign financial institution list",
    .name_ja = "IRS FATCA 外国金融機関リスト", .category = "government",
    .portal = "https://apps.irs.gov", .record_type = "fatca-ffi",
    .tags = "\"us\",\"tax\",\"financial\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://apps.irs.gov/app/fatcaFfiList/flu.jsf?q={q}",
    .base = "https://apps.irs.gov", .filter_query = 1,
    .description = "Every financial institution worldwide registered under "
      "FATCA — the GIIN, the legal name, the country and the classification. "
      "A global directory of banks, funds and trust companies that includes "
      "entities in jurisdictions with no public financial register at all" },

  { .id = "FINCEN_ENFORCEMENT", .name = "FinCEN — enforcement actions & advisories",
    .name_ja = "FinCEN 執行措置/勧告", .category = "government",
    .portal = "https://www.fincen.gov", .record_type = "us-aml-enforcement",
    .tags = "\"us\",\"aml\",\"enforcement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.fincen.gov/search?search={q}",
    .base = "https://www.fincen.gov", .filter_query = 1,
    .description = "FinCEN civil money penalties, consent orders, section 311 "
      "and 9714 special measures and the advisories naming typologies and "
      "institutions. Section 311 findings cut a named foreign bank out of the "
      "US dollar system, which is the most severe non-sanctions measure there "
      "is" },

  /* ── Export control ────────────────────────────────────────────────────── */
  { .id = "JP_METI_END_USER_LIST", .name = "Japan METI — foreign end user list",
    .name_ja = "経済産業省 外国ユーザーリスト", .category = "government",
    .portal = "https://www.meti.go.jp", .record_type = "jp-export-control",
    .tags = "\"jp\",\"export-control\",\"blacklist\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.meti.go.jp/policy/anpo/law05.html",
    .base = "https://www.meti.go.jp", .filter_query = 1,
    .description = "Japan's foreign end user list — organisations abroad for "
      "which there is a concern of involvement in weapons of mass destruction "
      "development, by country and with the concern category. Exporting to a "
      "listed entity triggers the catch-all licence requirement, so this is "
      "the operative Japanese export-control screen" },

  { .id = "JP_MOF_ASSET_FREEZE", .name = "Japan MOF — economic sanctions & asset freeze list",
    .name_ja = "財務省 経済制裁/資産凍結リスト", .category = "government",
    .portal = "https://www.mof.go.jp", .record_type = "jp-sanction",
    .tags = "\"jp\",\"sanctions\",\"asset-freeze\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.mof.go.jp/policy/international_policy/gaitame_kawase/"
      "gaitame/economic_sanctions/list.html",
    .base = "https://www.mof.go.jp", .filter_query = 1,
    .description = "Japan's asset-freeze designations under the Foreign "
      "Exchange and Foreign Trade Act, by regime. Japanese designations do not "
      "map one-to-one onto OFAC or EU listings, and Japanese financial "
      "institutions screen against this list rather than either of those" },

  /* ── Extractives, health and aviation safety ───────────────────────────── */
  { .id = "EITI_DISCLOSURES", .name = "EITI — extractive industries disclosures",
    .name_ja = "採取産業透明性イニシアティブ 開示", .category = "government",
    .portal = "https://eiti.org", .record_type = "eiti-disclosure",
    .tags = "\"eiti\",\"extractives\",\"transparency\",\"ownership\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://eiti.org/search?search={q}",
    .base = "https://eiti.org", .filter_query = 1,
    .description = "Country reports disclosing which companies hold which oil, "
      "gas and mining licences, what each paid the state, and — under the "
      "beneficial-ownership requirement — who ultimately owns them. For many "
      "resource states this is the only public licence-to-owner mapping" },

  { .id = "WHO_MEDICAL_ALERTS", .name = "WHO — medical product alerts & incidents",
    .name_ja = "WHO 医薬品警告", .category = "safety",
    .portal = "https://www.who.int", .record_type = "who-medical-alert",
    .tags = "\"who\",\"pharma\",\"safety\",\"counterfeit\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.who.int/home/search?query={q}",
    .base = "https://www.who.int", .filter_query = 1,
    .description = "WHO alerts on substandard and falsified medical products — "
      "the product, the stated manufacturer, the batch numbers and the "
      "countries where it was found. Names the manufacturers whose identity "
      "was used on falsified packaging, which no national regulator collates" },

  { .id = "ICAO_SAFETY_AUDIT", .name = "ICAO — safety oversight audit results",
    .name_ja = "ICAO 安全監査結果", .category = "safety",
    .portal = "https://www.icao.int", .record_type = "icao-audit",
    .tags = "\"icao\",\"aviation\",\"safety\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.icao.int/Pages/Search.aspx?k={q}",
    .base = "https://www.icao.int", .filter_query = 1,
    .description = "ICAO universal safety oversight audit findings by state — "
      "effective implementation scores by area and any significant safety "
      "concern raised. A state's audit score is the best available proxy for "
      "whether its aircraft registry and operator certificates mean anything" },

  /* ── Energy, infrastructure and supply chains ──────────────────────────── */
  { .id = "GEM_ENERGY_TRACKERS", .name = "Global Energy Monitor — asset trackers",
    .name_ja = "グローバルエナジーモニター 資産追跡", .category = "research",
    .portal = "https://globalenergymonitor.org", .record_type = "energy-asset",
    .tags = "\"energy\",\"infrastructure\",\"research\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://globalenergymonitor.org/?s={q}",
    .base = "https://globalenergymonitor.org", .filter_query = 1,
    .description = "Unit-level trackers for coal, gas, oil, LNG, steel, "
      "pipelines, wind and solar worldwide — each asset with its owner, "
      "parent, capacity, status and coordinates. The most complete public "
      "owner-to-physical-asset mapping in the energy sector" },

  { .id = "WRI_DATA_CATALOGUE", .name = "WRI — global environment & infrastructure data",
    .name_ja = "世界資源研究所 データカタログ", .category = "research",
    .portal = "https://datasets.wri.org", .record_type = "wri-dataset",
    .tags = "\"environment\",\"infrastructure\",\"opendata\"", .free_tier = 1,
    .url = "https://datasets.wri.org/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,name",
    .id_keys = "id", .date_keys = "metadata_modified",
    .link_keys = "name", .link_tmpl = "https://datasets.wri.org/dataset/{v}",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "The World Resources Institute catalogue — the global power "
      "plant database with plant-level owner and capacity, water risk, forest "
      "concessions and land-use layers — with the downloadable resources "
      "behind each dataset, paged to exhaustion" },

  { .id = "OPEN_SUPPLY_HUB_FACILITIES", .name = "Open Supply Hub — production facilities",
    .name_ja = "オープンサプライハブ 生産施設", .category = "research",
    .portal = "https://opensupplyhub.org", .record_type = "supply-chain-facility",
    .tags = "\"supply-chain\",\"facility\",\"labour\"",
    .key_env = "OPEN_SUPPLY_HUB_TOKEN", .free_tier = 1,
    .url = "https://opensupplyhub.org/api/facilities/?q={q}&pageSize=100",
    .headers = { "Authorization: Token {key}" },
    .array_path = "features", .title_keys = "properties.name,id",
    .id_keys = "id", .lat_key = "geometry.coordinates.1",
    .lon_key = "geometry.coordinates.0",
    .page_param = "page", .page_start = 1,
    .description = "Named production facilities with their address, "
      "coordinates, sector, worker count and — critically — the brands and "
      "buyers that have disclosed sourcing from them. Turns a brand name into "
      "a list of physical factories, which is the starting point for forced- "
      "labour and sanctions-origin work. Needs $OPEN_SUPPLY_HUB_TOKEN; without "
      "it the row reports the missing credential and returns nothing" },

  { .id = "TRASE_SUPPLY_CHAINS", .name = "Trase — commodity supply chain flows",
    .name_ja = "Trase 商品サプライチェーン", .category = "research",
    .portal = "https://trase.earth", .record_type = "commodity-flow",
    .tags = "\"supply-chain\",\"commodity\",\"deforestation\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://trase.earth/search?query={q}",
    .base = "https://trase.earth", .filter_query = 1,
    .description = "Traded volumes of soy, beef, palm oil and timber linked to "
      "named exporters, importers and their sourcing municipalities, with the "
      "deforestation exposure attached to each. Connects a trading company to "
      "specific production regions rather than to a country of origin" },
};

HP_REGISTER_TABLE(HP2_WORLD_CRITICAL)
