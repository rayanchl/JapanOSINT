/* collectors/sources/hp_apac_deep.c — Asia-Pacific deep records.
 *
 * Korea's DART is the region's EDINET and exposes filing lists plus full
 * company profiles; Taiwan publishes officer and branch data as open JSON;
 * Singapore, Australia and New Zealand expose registry-grade datasets; India
 * and Indonesia are scrape-only but their public search paths are real. Rows
 * that need a credential (DART, data.gov.in, ABR) return an honest empty
 * without it rather than a placeholder record. */
#include "lib/hpengine.h"

static const hp_source HP_APAC[] = {
  /* ── Korea ─────────────────────────────────────────────────────────────── */
  { .id = "KR_DART_FILINGS", .name = "DART — Korean corporate filings",
    .name_ja = "韓国DART — 企業開示一覧", .category = "economy",
    .portal = "https://dart.fss.or.kr", .record_type = "kr-filing",
    .tags = "\"kr\",\"filings\"", .key_env = "DART_API_KEY", .free_tier = 1,
    .url = "https://opendart.fss.or.kr/api/list.json?crtfc_key={key}&corp_code={qd}"
           "&bgn_de=20150101&page_count=50",
    .array_path = "list", .title_keys = "report_nm", .id_keys = "rcept_no",
    .date_keys = "rcept_dt",
    .link_tmpl = "https://dart.fss.or.kr/dsaf001/main.do?rcpNo={v}", .link_keys = "rcept_no",
    .description = "Every disclosure a Korean listed company has filed with the FSS "
      "— report name, receipt number, filer and date. The Korean equivalent of an "
      "EDGAR filing index, keyed by DART corp_code" },

  { .id = "KR_DART_COMPANY", .name = "DART — Korean company profile",
    .name_ja = "韓国DART — 企業概要", .category = "economy",
    .portal = "https://dart.fss.or.kr", .record_type = "kr-company",
    .tags = "\"kr\",\"registry\"", .key_env = "DART_API_KEY", .free_tier = 1,
    .url = "https://opendart.fss.or.kr/api/company.json?crtfc_key={key}&corp_code={qd}",
    .title_keys = "corp_name,corp_name_eng", .id_keys = "corp_code",
    .date_keys = "est_dt",
    .description = "DART company profile — registered and English name, CEO name, "
      "corporate registration number, business number, establishment date, address, "
      "homepage and industry code" },

  { .id = "KR_DART_MAJOR_HOLDERS", .name = "DART — Korean major shareholders",
    .name_ja = "韓国DART — 大株主報告", .category = "economy",
    .portal = "https://dart.fss.or.kr", .record_type = "kr-shareholder",
    .tags = "\"kr\",\"ownership\"", .key_env = "DART_API_KEY", .free_tier = 1,
    .url = "https://opendart.fss.or.kr/api/majorstock.json?crtfc_key={key}&corp_code={qd}",
    .array_path = "list", .title_keys = "repror,corp_name", .date_keys = "rcept_dt",
    .description = "Large-shareholding reports filed for a Korean company — the "
      "reporting holder, their stake before and after, the reason for the change "
      "and the filing date; Korea's beneficial-ownership disclosure stream" },

  /* ── Taiwan ────────────────────────────────────────────────────────────── */
  { .id = "TW_GCIS_COMPANY", .name = "Taiwan GCIS — company registration",
    .name_ja = "台湾 商工登記 — 企業登記", .category = "government",
    .portal = "https://data.gcis.nat.gov.tw", .record_type = "tw-company",
    .tags = "\"tw\",\"registry\"", .free_tier = 1,
    .url = "https://data.gcis.nat.gov.tw/od/data/api/6BBA2268-1367-4B42-9CCA-BC17499EBE8C"
           "?$format=json&$filter=Company_Name%20like%20{q}&$skip=0&$top=40",
    .title_keys = "Company_Name", .id_keys = "Business_Accounting_NO",
    .date_keys = "Company_Setup_Date",
    .description = "Taiwanese company registration — unified business number, "
      "capital, responsible person, registered address and setup/amendment dates, "
      "from the Ministry of Economic Affairs open API" },

  { .id = "TW_GCIS_DIRECTORS", .name = "Taiwan GCIS — directors & supervisors",
    .name_ja = "台湾 商工登記 — 役員一覧", .category = "government",
    .portal = "https://data.gcis.nat.gov.tw", .record_type = "tw-officer",
    .tags = "\"tw\",\"officers\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://data.gcis.nat.gov.tw/od/data/api/4E5F7653-1B91-4DD3-BCFB-8B0B7A275516"
           "?$format=json&$filter=Business_Accounting_NO%20eq%20{qd}",
    .title_keys = "Director_Name,Company_Name",
    .description = "The board of a Taiwanese company — directors, supervisors, their "
      "titles, shareholdings and the corporate representative where a company holds "
      "the seat" },

  /* ── Hong Kong / China ─────────────────────────────────────────────────── */
  { .id = "HK_CR_ICRIS", .name = "Hong Kong Companies Registry — ICRIS search",
    .name_ja = "香港 会社登記(ICRIS)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.icris.cr.gov.hk", .record_type = "hk-company",
    .tags = "\"hk\",\"registry\"", .free_tier = 1,
    .url = "https://www.e-services.cr.gov.hk/ICRIS3EP/system/dispatch.do?searchName={q}",
    .base = "https://www.e-services.cr.gov.hk", .filter_query = 1,
    .description = "Hong Kong company-name search against the Companies Registry "
      "e-services portal. Server-rendered anchors only — HK gates most detail behind "
      "a paid document purchase, and this row never pretends otherwise" },

  { .id = "HK_LICENCES_OPENDATA", .name = "Hong Kong open data — licence & register search",
    .name_ja = "香港 オープンデータ 免許検索", .category = "government",
    .portal = "https://data.gov.hk", .record_type = "hk-licence",
    .tags = "\"hk\",\"licences\"", .free_tier = 1,
    .url = "https://data.gov.hk/en-data/api/3/action/package_search?q={q}&rows=25",
    .array_path = "result.results", .title_keys = "title", .id_keys = "name",
    .date_keys = "metadata_modified",
    .description = "Hong Kong government datasets and registers matching an entity "
      "or sector — licence lists, contractor registers and enforcement datasets, "
      "with their resource URLs for direct download" },

  { .id = "CN_MIIT_ICP", .name = "China ICP filing lookup (beian)",
    .name_ja = "中国 ICP備案照会", .category = "cyber",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://beian.miit.gov.cn", .record_type = "cn-icp",
    .tags = "\"cn\",\"domains\"", .want = HP_DOMAIN, .free_tier = 1,
    .url = "https://www.beianx.cn/search/{qh}",
    .base = "https://www.beianx.cn", .filter_query = 0,
    .description = "Chinese ICP filing behind a domain — the licensed operator name, "
      "filing number and approval date. An ICP record ties a Chinese-hosted site to "
      "a registered legal entity, which WHOIS privacy usually hides" },

  /* ── Singapore / Malaysia / Indonesia / Thailand / Vietnam ─────────────── */
  { .id = "SG_DATA_REGISTRY", .name = "Singapore data.gov.sg — registry datasets",
    .name_ja = "シンガポール 政府データ 検索", .category = "government",
    .portal = "https://data.gov.sg", .record_type = "sg-dataset",
    .tags = "\"sg\",\"opendata\"", .free_tier = 1,
    .url = "https://api-production.data.gov.sg/v2/public/api/search?query={q}",
    .title_keys = "name,title", .id_keys = "datasetId,id",
    .description = "Singapore government datasets matching an entity or sector — "
      "ACRA-derived company lists, licensee registers and enforcement datasets, with "
      "the dataset ids needed to pull the rows themselves" },

  { .id = "MY_SSM_SEARCH", .name = "Malaysia SSM — company & business search",
    .name_ja = "マレーシア 企業登記(SSM)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.ssm-einfo.my", .record_type = "my-company",
    .tags = "\"my\",\"registry\"", .free_tier = 1,
    .url = "https://www.ssm-einfo.my/index.php/search/result?keyword={q}",
    .base = "https://www.ssm-einfo.my", .filter_query = 1,
    .description = "Malaysian SSM e-Info company and business-name search hits — "
      "registration number and status per match; full extracts are pay-per-document, "
      "so only the real listing anchors are emitted" },

  { .id = "ID_AHU_SEARCH", .name = "Indonesia AHU — legal entity search",
    .name_ja = "インドネシア 法人登記(AHU)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://ahu.go.id", .record_type = "id-company",
    .tags = "\"id\",\"registry\"", .free_tier = 1,
    .url = "https://ahu.go.id/pencarian/profil-pt?q={q}",
    .base = "https://ahu.go.id", .filter_query = 1,
    .description = "Indonesian Ministry of Law legal-entity search — PT/CV names, "
      "SK decree numbers and domicile as published on the AHU profile pages" },

  { .id = "TH_DBD_SEARCH", .name = "Thailand DBD — business registration search",
    .name_ja = "タイ 企業登記(DBD)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://datawarehouse.dbd.go.th", .record_type = "th-company",
    .tags = "\"th\",\"registry\"", .free_tier = 1,
    .url = "https://datawarehouse.dbd.go.th/searchJuristicInfo?jpNameTh={q}",
    .base = "https://datawarehouse.dbd.go.th", .filter_query = 1,
    .description = "Thai Department of Business Development registration hits — "
      "juristic person id, name, status and registered capital where the warehouse "
      "serves them without a session" },

  { .id = "VN_BUSINESS_REGISTRY", .name = "Vietnam National Business Registration search",
    .name_ja = "ベトナム 企業登記", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://dangkykinhdoanh.gov.vn", .record_type = "vn-company",
    .tags = "\"vn\",\"registry\"", .free_tier = 1,
    .url = "https://dichvuthongtin.dkkd.gov.vn/inf/default.aspx?keyword={q}",
    .base = "https://dichvuthongtin.dkkd.gov.vn", .filter_query = 1,
    .description = "Vietnamese national business-registration portal hits — company "
      "name, tax code and registered office per match, from the public information "
      "service pages" },

  { .id = "PH_SEC_ICOUNT", .name = "Philippines SEC — company name verification",
    .name_ja = "フィリピン SEC 企業名照会", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://secexpress.ph", .record_type = "ph-company",
    .tags = "\"ph\",\"registry\"", .free_tier = 1,
    .url = "https://secexpress.ph/search?q={q}",
    .base = "https://secexpress.ph", .filter_query = 1,
    .description = "Philippine SEC company-name search results — registration "
      "number and status per matching corporation or partnership" },

  /* ── India ─────────────────────────────────────────────────────────────── */
  { .id = "IN_DATA_RESOURCE", .name = "data.gov.in — registry & filing datasets",
    .name_ja = "インド 政府データ 照会", .category = "government",
    .portal = "https://data.gov.in", .record_type = "in-dataset",
    .tags = "\"in\",\"opendata\"", .key_env = "INDIA_DATA_KEY", .free_tier = 1,
    .url = "https://api.data.gov.in/catalog?api-key={key}&format=json&filters%5Btitle%5D={q}&limit=25",
    .title_keys = "title,desc", .id_keys = "index_name",
    .description = "Indian government data catalogue search — MCA company master "
      "data, GST registrations, licensee lists and court datasets, with the resource "
      "ids required to query the rows" },

  { .id = "IN_ZAUBA_CORP", .name = "ZaubaCorp — Indian company profiles",
    .name_ja = "インド 企業情報(ZaubaCorp)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.zaubacorp.com", .record_type = "in-company",
    .tags = "\"in\",\"registry\",\"officers\"", .free_tier = 1,
    .url = "https://www.zaubacorp.com/companysearchresults/{q}",
    .href_must = "/company/", .base = "https://www.zaubacorp.com",
    .description = "Indian companies matching a name on the MCA-derived ZaubaCorp "
      "mirror — each hit links a company page carrying CIN, authorised capital, "
      "directors and charge history that MCA itself paywalls" },

  { .id = "IN_KANOON_SEARCH", .name = "Indian Kanoon — court judgments",
    .name_ja = "インド 判例検索(Indian Kanoon)", .category = "legal",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://indiankanoon.org", .record_type = "in-judgment",
    .tags = "\"in\",\"courts\"", .free_tier = 1,
    .url = "https://indiankanoon.org/search/?formInput={q}",
    .href_must = "/doc/", .base = "https://indiankanoon.org",
    .description = "Indian Supreme Court, High Court and tribunal judgments naming "
      "an entity — the litigation history that Indian corporate records omit" },

  /* ── Australia / New Zealand ───────────────────────────────────────────── */
  { .id = "AU_ABR_ABN_LOOKUP", .name = "Australian Business Register — ABN record",
    .name_ja = "オーストラリア 事業者登記(ABN)", .category = "government",
    .portal = "https://abr.business.gov.au", .record_type = "au-company",
    .tags = "\"au\",\"registry\"", .key_env = "ABR_GUID", .free_tier = 1,
    .url = "https://abr.business.gov.au/json/MatchingNames.aspx?name={q}&guid={key}&maxResults=40",
    .array_path = "Names", .title_keys = "Name", .id_keys = "Abn",
    .description = "Australian Business Register name matches — ABN, entity type, "
      "GST registration status, state and postcode, plus trading names recorded "
      "against the same ABN" },

  { .id = "AU_ASIC_DATASETS", .name = "Australia data.gov.au — ASIC registers",
    .name_ja = "オーストラリア ASIC 登記データ", .category = "government",
    .portal = "https://data.gov.au", .record_type = "au-dataset",
    .tags = "\"au\",\"registry\"", .free_tier = 1,
    .url = "https://data.gov.au/data/api/3/action/package_search?q={q}&rows=25",
    .array_path = "result.results", .title_keys = "title", .id_keys = "name",
    .date_keys = "metadata_modified",
    .description = "Australian open-data catalogue hits — ASIC company, banned-and-"
      "disqualified-persons and licensee registers, business names and insolvency "
      "notices, with direct resource URLs" },

  { .id = "NZ_COMPANIES_SEARCH", .name = "NZ Companies Office — company & officer search",
    .name_ja = "ニュージーランド 会社登記", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://app.companiesoffice.govt.nz", .record_type = "nz-company",
    .tags = "\"nz\",\"registry\",\"officers\"", .free_tier = 1,
    .url = "https://app.companiesoffice.govt.nz/companies/app/ui/pages/companies/search"
           "?q={q}&entityTypes=ALL&entityStatusGroups=ALL",
    .href_must = "/companies/app/ui/pages/companies/",
    .base = "https://app.companiesoffice.govt.nz",
    .description = "New Zealand Companies Office search hits. NZ publishes director "
      "names, addresses and shareholding structures free of charge, so each hit "
      "leads to a fully public officer and ownership record" },

  { .id = "NZ_NZBN_SEARCH", .name = "NZBN — New Zealand business number register",
    .name_ja = "ニュージーランド 事業者番号(NZBN)", .category = "government",
    .portal = "https://www.nzbn.govt.nz", .record_type = "nz-entity",
    .tags = "\"nz\",\"registry\"", .key_env = "NZBN_ACCESS_TOKEN", .free_tier = 1,
    .url = "https://api.business.govt.nz/services/v4/nzbn/entities?search-term={q}&page-size=40",
    .headers = { "Authorization: Bearer {key}", NULL },
    .array_path = "items", .title_keys = "entityName", .id_keys = "nzbn",
    .date_keys = "registrationDate",
    .description = "New Zealand Business Number register — entity name, NZBN, type, "
      "status, trading names, addresses and the roles held by people in the entity" },
};

HP_REGISTER_TABLE(HP_APAC)
