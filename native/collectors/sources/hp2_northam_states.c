/* collectors/sources/hp2_northam_states.c — US state-level critical values.
 *
 * Federal registers answer "is this entity sanctioned or debarred". State
 * registers answer the questions that decide a real due-diligence outcome:
 * who incorporated it and where, which licence lets it trade, which regulator
 * has already fined it, and which unpaid wage judgment or environmental order
 * is sitting against it. US corporate existence is a state fact, not a federal
 * one, so a US entity that is invisible federally is usually fully documented
 * in exactly one state. */
#include "../../lib/hpengine.h"

static const hp_source HP2_NORTHAM_STATES[] = {
  /* ── Secretary-of-State entity registers ───────────────────────────────── */
  { .id = "US_CA_SOS_BUSINESS", .name = "California SOS — business entity search",
    .name_ja = "カリフォルニア州 法人登記", .category = "government",
    .portal = "https://bizfileonline.sos.ca.gov", .record_type = "us-ca-entity",
    .tags = "\"us\",\"california\",\"registry\"", .free_tier = 1,
    .url = "https://bizfileonline.sos.ca.gov/api/Records/businessSearch",
    .post_body = "{\"SEARCH_VALUE\":\"{Q}\",\"SEARCH_FILTER_TYPE_ID\":\"0\","
      "\"SEARCH_TYPE_ID\":\"1\",\"FILING_TYPE_ID\":\"\",\"STATUS_ID\":\"\"}",
    .content_type = "application/json",
    .array_path = "rows", .title_keys = "TITLE,title",
    .id_keys = "ID,FILING_NUMBER", .date_keys = "FILING_DATE,INITIAL_FILING_DATE",
    .description = "California entity records — filing number, entity type, "
      "status, jurisdiction of formation and the registered agent. California "
      "is where most US operating subsidiaries actually trade, so this is the "
      "register that shows whether a foreign parent has a live US presence" },

  { .id = "US_NY_SOS_CORP", .name = "New York — active corporations register",
    .name_ja = "ニューヨーク州 法人登記", .category = "government",
    .portal = "https://data.ny.gov", .record_type = "us-ny-entity",
    .tags = "\"us\",\"newyork\",\"registry\"", .free_tier = 1,
    .url = "https://data.ny.gov/resource/n9v6-gdp6.json?$q={q}&$limit=1000",
    .title_keys = "current_entity_name,initial_dos_filing_date",
    .id_keys = "dos_id", .date_keys = "initial_dos_filing_date",
    .page_param = "$offset", .page_size = 1000, .page_start = 0,
    .description = "Every active New York corporation since 1800 — DOS ID, "
      "county, jurisdiction, entity type, registered agent and the full service "
      "address. Walks the whole paged dataset rather than the first page" },

  { .id = "US_TX_COMPTROLLER_ENTITY", .name = "Texas Comptroller — taxable entity search",
    .name_ja = "テキサス州 課税事業者検索", .category = "government",
    .portal = "https://mycpa.cpa.state.tx.us", .record_type = "us-tx-entity",
    .tags = "\"us\",\"texas\",\"registry\",\"tax\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://mycpa.cpa.state.tx.us/coa/search.do?userSearchType=Name&entityName={q}",
    .base = "https://mycpa.cpa.state.tx.us", .filter_query = 1,
    .description = "Texas franchise-tax standing for an entity — taxpayer "
      "number, right-to-transact status and registered agent. A forfeited "
      "franchise-tax status is the earliest public signal that a Texas company "
      "has stopped filing" },

  { .id = "US_FL_SUNBIZ_ENTITY", .name = "Florida Sunbiz — corporate register",
    .name_ja = "フロリダ州 法人登記", .category = "government",
    .portal = "https://search.sunbiz.org", .record_type = "us-fl-entity",
    .tags = "\"us\",\"florida\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://search.sunbiz.org/Inquiry/CorporationSearch/SearchResults?"
      "inquiryType=EntityName&searchNameOrder={qn}&searchTerm={q}",
    .base = "https://search.sunbiz.org", .href_must = "/Inquiry/",
    .detail_url = "https://search.sunbiz.org{v}", .detail_key = "link",
    .description = "Florida entity records with the officer/director list and "
      "annual-report history behind each hit. Florida names every officer in "
      "the public filing, which makes it one of the most useful US registers "
      "for person-to-company pivots" },

  { .id = "US_DE_ENTITY_SEARCH", .name = "Delaware — division of corporations search",
    .name_ja = "デラウェア州 法人検索", .category = "government",
    .portal = "https://icis.corp.delaware.gov", .record_type = "us-de-entity",
    .tags = "\"us\",\"delaware\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://icis.corp.delaware.gov/eCorp/EntitySearch/NameSearch.aspx?entityName={q}",
    .base = "https://icis.corp.delaware.gov", .filter_query = 1,
    .description = "Delaware entity file numbers and incorporation dates. "
      "Delaware discloses almost nothing beyond existence — which is exactly "
      "why confirming the file number and formation date matters before "
      "treating a Delaware shell as a real counterparty" },

  { .id = "US_IL_SOS_CORP", .name = "Illinois SOS — corporation & LLC search",
    .name_ja = "イリノイ州 法人検索", .category = "government",
    .portal = "https://apps.ilsos.gov", .record_type = "us-il-entity",
    .tags = "\"us\",\"illinois\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://apps.ilsos.gov/corporatellc/CorporateLlcController?command=search&name={q}",
    .base = "https://apps.ilsos.gov", .filter_query = 1,
    .description = "Illinois corporate and LLC records — file number, status, "
      "agent and the annual-report filing history" },

  { .id = "US_OH_SOS_BUSINESS", .name = "Ohio SOS — business name search",
    .name_ja = "オハイオ州 事業者検索", .category = "government",
    .portal = "https://businesssearch.ohiosos.gov", .record_type = "us-oh-entity",
    .tags = "\"us\",\"ohio\",\"registry\"", .free_tier = 1,
    .url = "https://businesssearch.ohiosos.gov/?=businessDetails/{q}",
    .type = "scraped", .mode = HP_HTML,
    .base = "https://businesssearch.ohiosos.gov", .filter_query = 1,
    .description = "Ohio business filings — charter number, status, agent and "
      "the full filing history including mergers and dissolutions" },

  { .id = "US_CO_SOS_ENTITIES", .name = "Colorado — business entities open data",
    .name_ja = "コロラド州 事業者オープンデータ", .category = "government",
    .portal = "https://data.colorado.gov", .record_type = "us-co-entity",
    .tags = "\"us\",\"colorado\",\"registry\"", .free_tier = 1,
    .url = "https://data.colorado.gov/resource/4ykn-tg5h.json?$q={q}&$limit=1000",
    .title_keys = "entityname,entityid", .id_keys = "entityid",
    .date_keys = "entityformdate",
    .page_param = "$offset", .page_size = 1000, .page_start = 0,
    .description = "Colorado's full business-entity dataset as structured "
      "records — entity ID, status, formation date, jurisdiction and the "
      "principal and agent addresses, paged to exhaustion" },

  { .id = "US_WA_SOS_CORPS", .name = "Washington SOS — corporations search",
    .name_ja = "ワシントン州 法人検索", .category = "government",
    .portal = "https://ccfs.sos.wa.gov", .record_type = "us-wa-entity",
    .tags = "\"us\",\"washington\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://ccfs.sos.wa.gov/#/BusinessSearch/BusinessInformation?name={q}",
    .base = "https://ccfs.sos.wa.gov", .filter_query = 1,
    .description = "Washington corporate records — UBI number, governors "
      "(the state's term for directors/managers), registered agent and status" },

  { .id = "US_NV_SILVERFLUME", .name = "Nevada SilverFlume — business entity search",
    .name_ja = "ネバダ州 法人検索", .category = "government",
    .portal = "https://esos.nv.gov", .record_type = "us-nv-entity",
    .tags = "\"us\",\"nevada\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://esos.nv.gov/EntitySearch/OnlineEntitySearch?entityName={q}",
    .base = "https://esos.nv.gov", .filter_query = 1,
    .description = "Nevada entity records including the officer list. Nevada "
      "is the other US shell jurisdiction of choice, and unlike Delaware it "
      "publishes officers — so the same beneficial owner often surfaces here" },

  { .id = "US_NJ_BUSINESS_SEARCH", .name = "New Jersey — business records search",
    .name_ja = "ニュージャージー州 事業者記録", .category = "government",
    .portal = "https://www.njportal.com", .record_type = "us-nj-entity",
    .tags = "\"us\",\"newjersey\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.njportal.com/DOR/BusinessNameSearch/Search/BusinessName?businessName={q}",
    .base = "https://www.njportal.com", .filter_query = 1,
    .description = "New Jersey business records — entity ID, formation date, "
      "status and registered agent" },

  /* ── Occupational & trade licences ─────────────────────────────────────── */
  { .id = "US_CA_CSLB_CONTRACTOR", .name = "California CSLB — contractor licence & discipline",
    .name_ja = "カリフォルニア州 建設業免許/処分", .category = "government",
    .portal = "https://www.cslb.ca.gov", .record_type = "us-ca-contractor-licence",
    .tags = "\"us\",\"california\",\"licence\",\"enforcement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.cslb.ca.gov/OnlineServices/CheckLicenseII/NameSearch.aspx?BusinessName={q}",
    .base = "https://www.cslb.ca.gov", .filter_query = 1,
    .description = "California contractor licences with the bond, workers' "
      "compensation coverage and — the part that matters — any citation, "
      "revocation or licence suspension recorded against the licensee" },

  { .id = "US_CA_MEDICAL_BOARD", .name = "California Medical Board — licence & discipline",
    .name_ja = "カリフォルニア州 医師免許/処分", .category = "government",
    .portal = "https://search.dca.ca.gov", .record_type = "us-ca-professional-licence",
    .tags = "\"us\",\"california\",\"licence\",\"enforcement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://search.dca.ca.gov/results?boardCode=0&termType=1&term={q}",
    .base = "https://search.dca.ca.gov", .filter_query = 1,
    .description = "California Department of Consumer Affairs licence lookup "
      "across every board it runs — licence status plus public accusations, "
      "probation terms and disciplinary orders attached to the licensee" },

  { .id = "US_MI_LARA_LICENCE", .name = "Michigan LARA — professional licence verification",
    .name_ja = "ミシガン州 資格免許確認", .category = "government",
    .portal = "https://aca-prod.accela.com", .record_type = "us-mi-licence",
    .tags = "\"us\",\"michigan\",\"licence\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://aca-prod.accela.com/MILARA/GeneralProperty/PropertyLookUp.aspx?q={q}",
    .base = "https://aca-prod.accela.com", .filter_query = 1,
    .description = "Michigan licensing and regulatory affairs verification — "
      "licence type, status, issue and expiry dates and any disciplinary "
      "action on the record" },

  { .id = "US_FL_DBPR_LICENCE", .name = "Florida DBPR — licensee search",
    .name_ja = "フロリダ州 事業免許検索", .category = "government",
    .portal = "https://www.myfloridalicense.com", .record_type = "us-fl-licence",
    .tags = "\"us\",\"florida\",\"licence\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.myfloridalicense.com/wl11.asp?mode=2&search=Name&SID=&brd=&typ=N&name={q}",
    .base = "https://www.myfloridalicense.com", .filter_query = 1,
    .description = "Florida business and professional licences — construction, "
      "real estate, hospitality and the rest — with licence status and the "
      "discipline history behind each licensee" },

  /* ── State regulators & enforcement ────────────────────────────────────── */
  { .id = "US_NY_DFS_ENFORCEMENT", .name = "New York DFS — enforcement actions",
    .name_ja = "ニューヨーク州金融サービス局 処分", .category = "government",
    .portal = "https://www.dfs.ny.gov", .record_type = "us-ny-enforcement",
    .tags = "\"us\",\"newyork\",\"enforcement\",\"financial\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.dfs.ny.gov/search?search_api_fulltext={q}",
    .base = "https://www.dfs.ny.gov", .filter_query = 1,
    .description = "New York Department of Financial Services consent orders "
      "and penalties. DFS is the regulator that fines global banks over "
      "sanctions and AML failures, so its orders name the correspondent "
      "relationships nobody else publishes" },

  { .id = "US_CA_DFPI_ACTIONS", .name = "California DFPI — enforcement & licence actions",
    .name_ja = "カリフォルニア州金融保護局 処分", .category = "government",
    .portal = "https://dfpi.ca.gov", .record_type = "us-ca-enforcement",
    .tags = "\"us\",\"california\",\"enforcement\",\"financial\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://dfpi.ca.gov/?s={q}",
    .base = "https://dfpi.ca.gov", .filter_query = 1,
    .description = "California Department of Financial Protection and "
      "Innovation actions — desist-and-refrain orders, crypto and lending "
      "enforcement, and the consumer-complaint driven investigations" },

  { .id = "US_TX_TCEQ_ENFORCEMENT", .name = "Texas TCEQ — environmental enforcement",
    .name_ja = "テキサス州環境局 処分", .category = "government",
    .portal = "https://www.tceq.texas.gov", .record_type = "us-tx-enforcement",
    .tags = "\"us\",\"texas\",\"enforcement\",\"environment\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.tceq.texas.gov/search?q={q}",
    .base = "https://www.tceq.texas.gov", .filter_query = 1,
    .description = "Texas environmental enforcement orders and agreed penalties "
      "against permitted facilities — the state layer under EPA ECHO, and "
      "usually the one that names the operating subsidiary rather than the "
      "corporate parent" },

  { .id = "US_CA_DIR_JUDGMENTS", .name = "California DIR — wage judgments & debarment",
    .name_ja = "カリフォルニア州労働局 判決/指名停止", .category = "government",
    .portal = "https://www.dir.ca.gov", .record_type = "us-ca-labour-judgment",
    .tags = "\"us\",\"california\",\"labour\",\"debarment\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.dir.ca.gov/dlse/Judgment-Search.html?q={q}",
    .base = "https://www.dir.ca.gov", .filter_query = 1,
    .description = "Unsatisfied wage judgments and public-works debarments "
      "issued by California's labour commissioner — a direct read on whether a "
      "contractor is barred from state work and whether it pays its workers" },

  { .id = "US_NY_UNIFIED_COURTS", .name = "New York — unified court system case search",
    .name_ja = "ニューヨーク州 裁判所 事件検索", .category = "government",
    .portal = "https://iapps.courts.state.ny.us", .record_type = "us-ny-case",
    .tags = "\"us\",\"newyork\",\"court\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://iapps.courts.state.ny.us/nyscef/CaseSearch?party={q}",
    .base = "https://iapps.courts.state.ny.us", .filter_query = 1,
    .description = "New York state court filings by party name through NYSCEF "
      "— the commercial division docket where most US-facing international "
      "commercial disputes are actually litigated" },

  /* ── Money in politics & lobbying ──────────────────────────────────────── */
  { .id = "US_CA_CALACCESS_FILINGS", .name = "California Cal-Access — campaign & lobbying filings",
    .name_ja = "カリフォルニア州 政治資金/ロビー届出", .category = "government",
    .portal = "https://cal-access.sos.ca.gov", .record_type = "us-ca-political-filing",
    .tags = "\"us\",\"california\",\"lobbying\",\"campaign-finance\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://cal-access.sos.ca.gov/Campaign/Committees/list.aspx?name={q}",
    .base = "https://cal-access.sos.ca.gov", .filter_query = 1,
    .description = "California campaign committees, lobbying employers and "
      "their registered lobbyists, with contribution and payment detail — the "
      "state analogue of the federal lobbying disclosure register" },

  { .id = "US_NY_LOBBYING_REGISTRY", .name = "New York — lobbying registration & reports",
    .name_ja = "ニューヨーク州 ロビー登録", .category = "government",
    .portal = "https://reports.ethics.ny.gov", .record_type = "us-ny-lobbying",
    .tags = "\"us\",\"newyork\",\"lobbying\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://reports.ethics.ny.gov/PublicQuery?q={q}",
    .base = "https://reports.ethics.ny.gov", .filter_query = 1,
    .description = "New York lobbying registrations and bi-monthly reports — "
      "client, lobbyist, compensation and the subjects lobbied, plus the "
      "financial-disclosure statements filed by state officials" },

  { .id = "US_TX_ETHICS_FILINGS", .name = "Texas Ethics Commission — filings search",
    .name_ja = "テキサス州倫理委員会 届出", .category = "government",
    .portal = "https://www.ethics.state.tx.us", .record_type = "us-tx-political-filing",
    .tags = "\"us\",\"texas\",\"lobbying\",\"campaign-finance\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.ethics.state.tx.us/search/cf/SearchByName.php?name={q}",
    .base = "https://www.ethics.state.tx.us", .filter_query = 1,
    .description = "Texas campaign-finance and lobbyist registrations with "
      "itemised contributions and expenditures — who paid whom, at what point "
      "in which legislative session" },

  { .id = "US_STATE_UCC_LIENS", .name = "US states — UCC secured-transaction liens",
    .name_ja = "米国州 UCC 担保権登記", .category = "government",
    .portal = "https://bizfileonline.sos.ca.gov", .record_type = "us-ucc-lien",
    .tags = "\"us\",\"lien\",\"finance\"", .free_tier = 1,
    .url = "https://bizfileonline.sos.ca.gov/api/Records/uccsearch",
    .post_body = "{\"SEARCH_VALUE\":\"{Q}\",\"SEARCH_TYPE_ID\":\"1\","
      "\"FILING_TYPE_ID\":\"\",\"STATUS_ID\":\"\"}",
    .content_type = "application/json",
    .array_path = "rows", .title_keys = "TITLE,title",
    .id_keys = "ID,FILING_NUMBER", .date_keys = "FILING_DATE",
    .description = "UCC-1 financing statements filed against a debtor — the "
      "secured lenders, the collateral described and the filing dates. Liens "
      "expose the lending relationships and asset pledges that never appear in "
      "a corporate register" },
};

HP_REGISTER_TABLE(HP2_NORTHAM_STATES)
