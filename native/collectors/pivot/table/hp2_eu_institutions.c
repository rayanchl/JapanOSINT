/* collectors/sources/hp2_eu_institutions.c — EU-level critical values.
 *
 * The EU's registers are unusual because they are *supranational obligations*:
 * a firm authorised under MiFID or Solvency II appears in a single European
 * register regardless of which member state licensed it, a product recalled in
 * one country is recalled in the Safety Gate for all of them, and a company
 * excluded under EDES is excluded from every EU-funded contract everywhere.
 * That makes these the highest-leverage European lookups: one query, 27
 * jurisdictions of coverage. */
#include "lib/hpengine.h"

static const hp_source HP2_EU_INSTITUTIONS[] = {
  /* ── Product & food safety ─────────────────────────────────────────────── */
  { .id = "EU_SAFETY_GATE_ALERTS", .name = "EU Safety Gate — dangerous product alerts",
    .name_ja = "EU 危険製品警告(Safety Gate)", .category = "safety",
    .portal = "https://ec.europa.eu/safety-gate-alerts", .record_type = "eu-product-alert",
    .tags = "\"eu\",\"recall\",\"safety\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://ec.europa.eu/safety-gate-alerts/screen/search?searchType=simple&text={q}",
    .base = "https://ec.europa.eu", .filter_query = 1,
    .description = "EU rapid-alert notifications for dangerous non-food "
      "products — the brand, the notifying country, the hazard, the country of "
      "origin and the measures ordered. Names importers and distributors that "
      "no corporate register connects to the product" },

  { .id = "EU_RASFF_WINDOW", .name = "EU RASFF — food & feed rapid alerts",
    .name_ja = "EU 食品飼料 迅速警告(RASFF)", .category = "safety",
    .portal = "https://webgate.ec.europa.eu/rasff-window", .record_type = "eu-food-alert",
    .tags = "\"eu\",\"food\",\"safety\",\"recall\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://webgate.ec.europa.eu/rasff-window/screen/search?searchQueries={q}",
    .base = "https://webgate.ec.europa.eu", .filter_query = 1,
    .description = "Food and feed border rejections and market withdrawals — "
      "the hazard, the origin country, the notifying member state and the "
      "operators involved. A repeated RASFF origin is a supply-chain fact you "
      "cannot get from customs data" },

  { .id = "EU_ECHA_SUBSTANCES", .name = "ECHA — registered substances & registrants",
    .name_ja = "欧州化学品庁 登録物質", .category = "government",
    .portal = "https://echa.europa.eu", .record_type = "eu-chemical-registration",
    .tags = "\"eu\",\"chemicals\",\"licence\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://echa.europa.eu/search-for-chemicals?p_p_id=disssimplesearch_WAR_"
      "disssearchportlet&_disssimplesearch_WAR_disssearchportlet_sskeywordKey={q}",
    .base = "https://echa.europa.eu", .filter_query = 1,
    .description = "REACH registrations, authorisations and restrictions — the "
      "substance, tonnage band and the registrant companies named on the "
      "dossier. ECHA is the only public source that ties a named European "
      "manufacturer or importer to a specific chemical volume" },

  { .id = "EU_EFSA_OUTPUTS", .name = "EFSA — scientific outputs & applicants",
    .name_ja = "欧州食品安全機関 評価文書", .category = "government",
    .portal = "https://www.efsa.europa.eu", .record_type = "eu-efsa-output",
    .tags = "\"eu\",\"food\",\"regulatory\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.efsa.europa.eu/en/search?s={q}",
    .base = "https://www.efsa.europa.eu", .filter_query = 1,
    .description = "European Food Safety Authority opinions and risk "
      "assessments, including the applicant company behind each novel-food, "
      "additive or pesticide dossier and the panel's conclusion" },

  { .id = "EU_EMA_MEDICINES", .name = "EMA — authorised medicines & holders",
    .name_ja = "欧州医薬品庁 承認医薬品", .category = "government",
    .portal = "https://www.ema.europa.eu", .record_type = "eu-medicine",
    .tags = "\"eu\",\"pharma\",\"licence\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.ema.europa.eu/en/medicines?search_api_fulltext={q}",
    .base = "https://www.ema.europa.eu", .filter_query = 1,
    .description = "Centrally authorised medicines with the marketing "
      "authorisation holder, the authorisation status and any referral, "
      "suspension or withdrawal — the regulatory history of a pharma company's "
      "European portfolio" },

  /* ── Financial supervision ─────────────────────────────────────────────── */
  { .id = "EU_ESMA_REGISTERS", .name = "ESMA — consolidated registers search",
    .name_ja = "欧州証券市場監督機構 登録簿", .category = "government",
    .portal = "https://registers.esma.europa.eu", .record_type = "eu-esma-entry",
    .tags = "\"eu\",\"financial\",\"licence\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://registers.esma.europa.eu/publication/searchRegister?core=esma_registers_upreg&q={q}",
    .base = "https://registers.esma.europa.eu", .filter_query = 1,
    .description = "ESMA's registers in one search — investment firms, trading "
      "venues, credit rating agencies, benchmark administrators, securitisation "
      "repositories and the register of firms flagged for operating without "
      "authorisation" },

  { .id = "EU_EBA_CREDIT_REGISTER", .name = "EBA — credit institutions & payment firms register",
    .name_ja = "欧州銀行監督機構 金融機関登録", .category = "government",
    .portal = "https://euclid.eba.europa.eu", .record_type = "eu-credit-institution",
    .tags = "\"eu\",\"financial\",\"licence\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://euclid.eba.europa.eu/register/cir/search?searchTerm={q}",
    .base = "https://euclid.eba.europa.eu", .filter_query = 1,
    .description = "Every authorised credit institution, payment institution "
      "and e-money institution in the EEA — LEI, competent authority, "
      "authorisation and withdrawal dates and the passporting notifications "
      "that show which other member states the firm operates into" },

  { .id = "EU_EIOPA_REGISTER", .name = "EIOPA — insurance & pension undertakings register",
    .name_ja = "欧州保険年金監督機構 登録簿", .category = "government",
    .portal = "https://register.eiopa.europa.eu", .record_type = "eu-insurer",
    .tags = "\"eu\",\"insurance\",\"licence\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://register.eiopa.europa.eu/search?q={q}",
    .base = "https://register.eiopa.europa.eu", .filter_query = 1,
    .description = "Authorised European insurers, reinsurers, intermediaries "
      "and occupational pension funds, with their cross-border activity. "
      "Insurance cover is a sanctions chokepoint, so knowing which EEA insurer "
      "is authorised to write a risk matters" },

  /* ── EU money, exclusions and influence ────────────────────────────────── */
  { .id = "EU_EDES_EXCLUSIONS", .name = "EU EDES — early detection & exclusion database",
    .name_ja = "EU 早期発見/排除データベース", .category = "government",
    .portal = "https://ec.europa.eu", .record_type = "eu-exclusion",
    .tags = "\"eu\",\"debarment\",\"procurement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://ec.europa.eu/info/strategy/eu-budget/how-it-works/annual-lifecycle/"
      "implementation/anti-fraud-measures/edes/database_en?q={q}",
    .base = "https://ec.europa.eu", .filter_query = 1,
    .description = "Persons and entities excluded from receiving EU funds — "
      "the ground for exclusion (fraud, corruption, grave professional "
      "misconduct, serious contract breach), the duration and the scope. The "
      "EU's own debarment list, and it is not mirrored in the US SAM data" },

  { .id = "EU_TRANSPARENCY_REGISTER", .name = "EU Transparency Register — lobbying entities",
    .name_ja = "EU 透明性登録簿(ロビー)", .category = "government",
    .portal = "https://ec.europa.eu/transparencyregister", .record_type = "eu-lobbyist",
    .tags = "\"eu\",\"lobbying\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://ec.europa.eu/transparencyregister/public/consultation/"
      "search.do?locale=en&name={q}",
    .base = "https://ec.europa.eu", .filter_query = 1,
    .description = "Organisations registered to lobby the EU institutions — "
      "declared annual lobbying budget, clients, the legislative files they "
      "work on, the accredited passholders they employ and the logged meetings "
      "with commissioners and directors-general" },

  { .id = "EU_FTS_BENEFICIARIES", .name = "EU Financial Transparency System — fund beneficiaries",
    .name_ja = "EU 資金受益者データベース", .category = "government",
    .portal = "https://ec.europa.eu", .record_type = "eu-fund-beneficiary",
    .tags = "\"eu\",\"grants\",\"funding\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://ec.europa.eu/budget/financial-transparency-system/"
      "analysis.html?searchTerm={q}",
    .base = "https://ec.europa.eu", .filter_query = 1,
    .description = "Beneficiaries of directly managed EU budget commitments — "
      "the recipient, the amount, the budget line, the action and the year. "
      "Traces EU money into named NGOs, consultancies and companies" },

  { .id = "EU_CORDIS_PROJECTS", .name = "CORDIS — EU research projects & participants",
    .name_ja = "EU 研究プロジェクト(CORDIS)", .category = "research",
    .portal = "https://cordis.europa.eu", .record_type = "eu-research-project",
    .tags = "\"eu\",\"research\",\"funding\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://cordis.europa.eu/search?q={q}",
    .base = "https://cordis.europa.eu", .filter_query = 1,
    .description = "EU framework-programme projects with every consortium "
      "participant, its country, role and EU contribution. Research consortia "
      "expose institutional links — including to entities in countries subject "
      "to research-security screening — that appear nowhere else" },

  /* ── Competition, trade defence and IP ─────────────────────────────────── */
  { .id = "EU_COMPETITION_CASES", .name = "European Commission — competition case search",
    .name_ja = "欧州委員会 競争法事件検索", .category = "government",
    .portal = "https://competition-cases.ec.europa.eu", .record_type = "eu-competition-case",
    .tags = "\"eu\",\"antitrust\",\"enforcement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://competition-cases.ec.europa.eu/search?search={q}",
    .base = "https://competition-cases.ec.europa.eu", .filter_query = 1,
    .description = "Antitrust, cartel and merger cases — the undertakings "
      "named, the decision type, the fine imposed and the full text of the "
      "decision. Cartel decisions are the richest public description of how a "
      "market actually operated" },

  { .id = "EU_STATE_AID_CASES", .name = "European Commission — state aid cases",
    .name_ja = "欧州委員会 国家補助事件", .category = "government",
    .portal = "https://competition-cases.ec.europa.eu", .record_type = "eu-state-aid-case",
    .tags = "\"eu\",\"state-aid\",\"subsidy\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://competition-cases.ec.europa.eu/search?caseInstrument=SA&search={q}",
    .base = "https://competition-cases.ec.europa.eu", .filter_query = 1,
    .description = "State aid notifications and decisions — the member state, "
      "the beneficiary undertaking, the aid amount and instrument, and whether "
      "recovery was ordered. The public record of which governments are "
      "propping up which companies, and for how much" },

  { .id = "EU_TRADE_DEFENCE_MEASURES", .name = "EU — anti-dumping & anti-subsidy measures",
    .name_ja = "EU 反ダンピング/相殺措置", .category = "government",
    .portal = "https://tron.trade.ec.europa.eu", .record_type = "eu-trade-measure",
    .tags = "\"eu\",\"trade\",\"customs\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://tron.trade.ec.europa.eu/investigations/search?query={q}",
    .base = "https://tron.trade.ec.europa.eu", .filter_query = 1,
    .description = "EU trade-defence investigations and measures in force — "
      "the product, the origin country, the named exporting producers and "
      "their individual duty rates, plus circumvention and expiry reviews. "
      "Company-specific duty rates identify exporters by name" },

  { .id = "EU_EUIPO_TRADEMARKS", .name = "EUIPO — EU trademarks & designs",
    .name_ja = "EU知的財産庁 商標/意匠", .category = "government",
    .portal = "https://www.tmdn.org", .record_type = "eu-trademark",
    .tags = "\"eu\",\"ip\",\"trademark\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.tmdn.org/tmview/#/tmview/results?page=1&pageSize=100&"
      "criteria=C&basicSearch={q}",
    .base = "https://www.tmdn.org", .filter_query = 1,
    .page_param = "page", .page_start = 1,
    .description = "EU trademark and design filings with the proprietor, "
      "representative, Nice classes, opposition history and every recorded "
      "transfer of ownership. Trademark transfers date corporate acquisitions "
      "months before any register does" },

  /* ── Aviation, energy and the open-data layer ──────────────────────────── */
  { .id = "EU_AIR_SAFETY_LIST", .name = "EU — air safety list (banned carriers)",
    .name_ja = "EU 運航禁止航空会社リスト", .category = "safety",
    .portal = "https://transport.ec.europa.eu", .record_type = "eu-banned-carrier",
    .tags = "\"eu\",\"aviation\",\"safety\",\"blacklist\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://transport.ec.europa.eu/transport-themes/eu-air-safety-list_en?q={q}",
    .base = "https://transport.ec.europa.eu", .filter_query = 1,
    .description = "Air carriers banned or restricted in EU airspace, by "
      "airline and by state of registry. A carrier's presence here is a direct "
      "read on the credibility of its national aviation authority, which "
      "matters for any cargo routed through that state" },

  { .id = "EU_ETS_INSTALLATIONS", .name = "EU ETS — installations & operators (EUTL)",
    .name_ja = "EU 排出量取引 施設/事業者", .category = "government",
    .portal = "https://ec.europa.eu", .record_type = "eu-ets-installation",
    .tags = "\"eu\",\"energy\",\"emissions\",\"infrastructure\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://ec.europa.eu/clima/ets/oha.do?languageCode=en&search={q}",
    .base = "https://ec.europa.eu", .filter_query = 1,
    .description = "Every industrial installation and aircraft operator in the "
      "EU emissions trading system — permit holder, installation address, "
      "activity type, verified emissions and allowance surrenders. An "
      "installation-level map of European heavy industry, by operator name" },

  { .id = "EU_OPEN_DATA_PORTAL", .name = "data.europa.eu — EU open data catalogue",
    .name_ja = "EU オープンデータポータル", .category = "government",
    .portal = "https://data.europa.eu", .record_type = "eu-dataset",
    .tags = "\"eu\",\"opendata\"", .free_tier = 1,
    .url = "https://data.europa.eu/api/hub/search/ckan/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,name",
    .id_keys = "id", .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "The union catalogue over every EU institution and member "
      "state open-data portal — beneficiary lists, inspection registers, "
      "permit databases and vehicle registers, with the downloadable resources "
      "behind each dataset. Paged to exhaustion rather than first-page-only" },

  { .id = "EU_EIB_PROJECTS", .name = "European Investment Bank — financed projects",
    .name_ja = "欧州投資銀行 融資案件", .category = "government",
    .portal = "https://www.eib.org", .record_type = "eu-eib-project",
    .tags = "\"eu\",\"development\",\"finance\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.eib.org/en/projects/all/index.htm?q={q}",
    .base = "https://www.eib.org", .filter_query = 1,
    .description = "EIB-financed projects with the promoter, sector, signature "
      "amount, status and the environmental and social documentation. The EIB "
      "lends more than the World Bank, and its promoter names are the "
      "counterparties behind European infrastructure" },

  { .id = "EU_EIB_EXCLUSION", .name = "European Investment Bank — exclusion list",
    .name_ja = "欧州投資銀行 排除リスト", .category = "government",
    .portal = "https://www.eib.org", .record_type = "eu-eib-exclusion",
    .tags = "\"eu\",\"debarment\",\"finance\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.eib.org/en/about/accountability/anti-fraud/exclusion/index.htm?q={q}",
    .base = "https://www.eib.org", .filter_query = 1,
    .description = "Entities excluded from EIB-financed operations after fraud "
      "or corruption findings, including parties cross-debarred under the "
      "multilateral development banks' mutual enforcement agreement" },

  { .id = "EBRD_PROJECT_SUMMARIES", .name = "EBRD — project summary documents",
    .name_ja = "欧州復興開発銀行 案件概要", .category = "government",
    .portal = "https://www.ebrd.com", .record_type = "ebrd-project",
    .tags = "\"ebrd\",\"development\",\"finance\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.ebrd.com/home/work-with-us/projects/psd.html?q={q}",
    .base = "https://www.ebrd.com", .filter_query = 1,
    .description = "EBRD project summaries across Central Asia, the Caucasus, "
      "the Balkans and Eastern Europe — the client company, sponsor, financing "
      "structure and environmental category. Frequently the only English "
      "description of the ownership behind a regional industrial asset" },
};

HP_REGISTER_TABLE(HP2_EU_INSTITUTIONS)
