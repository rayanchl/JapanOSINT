/* collectors/sources/hp3_gov_japan_public.c — Japanese government record depth.
 *
 * Japan's public record is unusually good and unusually under-used, because the
 * good parts are APIs with Japanese-only documentation sitting behind an
 * application-ID wall that most tooling never crosses. The corporate number
 * (法人番号) is the join key for all of it: gBizINFO hangs subsidies, government
 * contracts, certifications, patents and financial summaries off that one
 * number, and the NTA publishes the number-to-name mapping and the qualified
 * invoice issuer status against it.
 *
 * That is exactly the shape this engine is for — a list hit that names an
 * identifier, and a detail endpoint behind it that carries the substance. The
 * gBizINFO rows below are five separate second hops off the same corporate
 * number, so a single pivot on a Japanese company name returns its subsidy
 * history, its government contracts, its certifications and its patents rather
 * than just confirming that the company exists. */
#include "../../lib/hpengine.h"

static const hp_source HP3_GOV_JP[] = {
  /* ── The corporate number spine ───────────────────────────────────────── */
  { .id = "JP_NTA_HOUJIN_WEBAPI", .name = "NTA — corporate number (法人番号) Web-API",
    .name_ja = "国税庁 法人番号システムWeb-API", .category = "government",
    .portal = "https://www.houjin-bangou.nta.go.jp", .record_type = "jp-entity",
    .tags = "\"jp\",\"registry\",\"identity\"", .key_env = "NTA_HOUJIN_APP_ID",
    .free_tier = 1, .type = "api", .mode = HP_CSV,
    .url = "https://api.houjin-bangou.nta.go.jp/4/name?id={key}&name={q}"
      "&type=02&mode=2&history=1",
    .filter_query = 1, .title_keys = "col6,col9", .id_keys = "col2",
    .date_keys = "col3",
    .description = "The National Tax Agency's authoritative corporate-number "
      "register, requested with history=1 so name changes, address changes, "
      "mergers and closures are returned rather than only the current state. "
      "The 13-digit number is the join key for every other Japanese "
      "government dataset about a company" },

  { .id = "JP_NTA_INVOICE_ISSUER", .name = "NTA — qualified invoice issuer register",
    .name_ja = "国税庁 適格請求書発行事業者公表", .category = "government",
    .portal = "https://www.invoice-kohyo.nta.go.jp", .record_type = "jp-tax-registration",
    .tags = "\"jp\",\"tax\",\"identity\"", .key_env = "NTA_INVOICE_APP_ID",
    .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://web-api.invoice-kohyo.nta.go.jp/1/num?id={key}&number={qd}&type=21",
    .array_path = "announcement", .title_keys = "name,address",
    .id_keys = "registratedNumber", .date_keys = "registrationDate",
    .description = "Whether a counterparty is a registered qualified invoice "
      "issuer under Japan's 2023 consumption-tax regime — registration number, "
      "the date it took effect, and any disqualification. A supplier missing "
      "from this register cannot pass on deductible tax" },

  { .id = "JP_GBIZINFO_COMPANY", .name = "gBizINFO — government-held company profile",
    .name_ja = "gBizINFO 法人情報", .category = "government",
    .portal = "https://info.gbiz.go.jp", .record_type = "jp-entity",
    .tags = "\"jp\",\"registry\",\"government\"", .key_env = "GBIZINFO_API_TOKEN",
    .free_tier = 1,
    .url = "https://info.gbiz.go.jp/hojin/v1/hojin?name={q}&limit=5000",
    .headers = { "X-hojinInfo-api-token: {key}", NULL },
    .array_path = "hojin-infos", .title_keys = "name,location",
    .id_keys = "corporate_number", .date_keys = "update_date",
    .page_param = "page", .page_size = 5000, .page_max = 20,
    .description = "The Japanese government's consolidated view of a company — "
      "corporate number, registered location, capital, employee count, business "
      "items and the date each field was last confirmed by a ministry" },

  { .id = "JP_GBIZINFO_SUBSIDY", .name = "gBizINFO — subsidies received (補助金)",
    .name_ja = "gBizINFO 補助金情報", .category = "government",
    .portal = "https://info.gbiz.go.jp", .record_type = "jp-subsidy",
    .tags = "\"jp\",\"subsidy\",\"public-money\"", .key_env = "GBIZINFO_API_TOKEN",
    .free_tier = 1,
    .url = "https://info.gbiz.go.jp/hojin/v1/hojin?name={q}&limit=5000",
    .headers = { "X-hojinInfo-api-token: {key}", NULL },
    .array_path = "hojin-infos", .title_keys = "name", .id_keys = "corporate_number",
    .page_param = "page", .page_size = 5000, .page_max = 20,
    .detail_url = "https://info.gbiz.go.jp/hojin/v1/hojin/{v}/subsidy",
    .detail_key = "corporate_number", .detail_path = "hojin-infos",
    .description = "Every government subsidy a named company has received — the "
      "granting ministry, the programme, the amount and the fiscal year. Public "
      "money into a private company, keyed to its corporate number" },

  { .id = "JP_GBIZINFO_PROCUREMENT", .name = "gBizINFO — government contracts won (調達)",
    .name_ja = "gBizINFO 調達情報", .category = "government",
    .portal = "https://info.gbiz.go.jp", .record_type = "jp-public-contract",
    .tags = "\"jp\",\"procurement\",\"public-money\"", .key_env = "GBIZINFO_API_TOKEN",
    .free_tier = 1,
    .url = "https://info.gbiz.go.jp/hojin/v1/hojin?name={q}&limit=5000",
    .headers = { "X-hojinInfo-api-token: {key}", NULL },
    .array_path = "hojin-infos", .title_keys = "name", .id_keys = "corporate_number",
    .page_param = "page", .page_size = 5000, .page_max = 20,
    .detail_url = "https://info.gbiz.go.jp/hojin/v1/hojin/{v}/procurement",
    .detail_key = "corporate_number", .detail_path = "hojin-infos",
    .description = "Central government contracts awarded to a company — the "
      "procuring body, the contract name, the amount, the award date and "
      "whether it was competitively tendered or a single-source award" },

  { .id = "JP_GBIZINFO_CERTIFICATION", .name = "gBizINFO — certifications & commendations (認定)",
    .name_ja = "gBizINFO 表彰/認定情報", .category = "government",
    .portal = "https://info.gbiz.go.jp", .record_type = "jp-certification",
    .tags = "\"jp\",\"certification\",\"government\"", .key_env = "GBIZINFO_API_TOKEN",
    .free_tier = 1,
    .url = "https://info.gbiz.go.jp/hojin/v1/hojin?name={q}&limit=5000",
    .headers = { "X-hojinInfo-api-token: {key}", NULL },
    .array_path = "hojin-infos", .title_keys = "name", .id_keys = "corporate_number",
    .page_param = "page", .page_size = 5000, .page_max = 20,
    .detail_url = "https://info.gbiz.go.jp/hojin/v1/hojin/{v}/certification",
    .detail_key = "corporate_number", .detail_path = "hojin-infos",
    .description = "Ministry certifications and commendations held by a company "
      "— management innovation plans, health-management certification, "
      "regional-revitalisation designations. These reveal which ministry has "
      "an active relationship with the firm" },

  { .id = "JP_GBIZINFO_PATENT", .name = "gBizINFO — patents held (特許)",
    .name_ja = "gBizINFO 特許情報", .category = "research",
    .portal = "https://info.gbiz.go.jp", .record_type = "jp-patent",
    .tags = "\"jp\",\"patent\",\"ip\"", .key_env = "GBIZINFO_API_TOKEN",
    .free_tier = 1,
    .url = "https://info.gbiz.go.jp/hojin/v1/hojin?name={q}&limit=5000",
    .headers = { "X-hojinInfo-api-token: {key}", NULL },
    .array_path = "hojin-infos", .title_keys = "name", .id_keys = "corporate_number",
    .page_param = "page", .page_size = 5000, .page_max = 20,
    .detail_url = "https://info.gbiz.go.jp/hojin/v1/hojin/{v}/patent",
    .detail_key = "corporate_number", .detail_path = "hojin-infos",
    .description = "Patents and applications attributed to the corporate "
      "number — application number, title, classification and status, joined to "
      "the legal entity rather than to a free-text applicant string" },

  { .id = "JP_GBIZINFO_FINANCE", .name = "gBizINFO — financial summary (財務情報)",
    .name_ja = "gBizINFO 財務情報", .category = "finance",
    .portal = "https://info.gbiz.go.jp", .record_type = "jp-financials",
    .tags = "\"jp\",\"financials\"", .key_env = "GBIZINFO_API_TOKEN", .free_tier = 1,
    .url = "https://info.gbiz.go.jp/hojin/v1/hojin?name={q}&limit=5000",
    .headers = { "X-hojinInfo-api-token: {key}", NULL },
    .array_path = "hojin-infos", .title_keys = "name", .id_keys = "corporate_number",
    .page_param = "page", .page_size = 5000, .page_max = 20,
    .detail_url = "https://info.gbiz.go.jp/hojin/v1/hojin/{v}/finance",
    .detail_key = "corporate_number", .detail_path = "hojin-infos",
    .description = "Net sales, operating profit, ordinary profit and net assets "
      "as filed with the government, per fiscal year — available for unlisted "
      "companies that publish nothing on EDINET" },

  /* ── Law, rulemaking and the courts ───────────────────────────────────── */
  { .id = "JP_EGOV_LAW_SEARCH", .name = "e-Gov — Japanese statute & ordinance API",
    .name_ja = "e-Gov 法令検索API", .category = "government",
    .portal = "https://laws.e-gov.go.jp", .record_type = "jp-legislation",
    .tags = "\"jp\",\"law\"", .free_tier = 1,
    .url = "https://laws.e-gov.go.jp/api/2/keyword?keyword={q}&limit=500",
    .array_path = "items", .title_keys = "law_info.law_title,revision_info.law_title",
    .id_keys = "law_info.law_id", .date_keys = "revision_info.amendment_promulgate_date",
    .page_param = "offset", .page_size = 500, .page_start = 0, .page_max = 20,
    .description = "Full-text search across every Japanese act, cabinet order "
      "and ministerial ordinance in force, returning the law ID, the title and "
      "the amendment history — the operative text, not a commentary on it" },

  { .id = "JP_EGOV_PUBLIC_COMMENT", .name = "e-Gov — public comment (パブリックコメント)",
    .name_ja = "e-Gov パブリックコメント", .category = "government",
    .portal = "https://public-comment.e-gov.go.jp", .record_type = "jp-consultation",
    .tags = "\"jp\",\"regulation\",\"consultation\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://public-comment.e-gov.go.jp/pcm/search?searchWord={q}",
    .base = "https://public-comment.e-gov.go.jp", .filter_query = 1,
    .description = "Draft ministerial rules open for comment and the published "
      "response summaries — the earliest public signal that a ministry intends "
      "to change a licensing, safety or reporting requirement" },

  { .id = "JP_COURTS_HANREI", .name = "Courts of Japan — published judgments (裁判例検索)",
    .name_ja = "裁判所 裁判例検索", .category = "legal",
    .portal = "https://www.courts.go.jp", .record_type = "jp-judgment",
    .tags = "\"jp\",\"courts\",\"judgment\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.courts.go.jp/app/hanrei_jp/search1?filter[text]={q}",
    .base = "https://www.courts.go.jp", .filter_query = 1,
    .description = "The Supreme Court's judgment database — case number, court, "
      "date, the provisions applied and the full text of decisions naming a "
      "company or individual" },

  { .id = "JP_KANPO_GAZETTE", .name = "Kanpō — Japanese official gazette search",
    .name_ja = "官報検索", .category = "government",
    .portal = "https://search.npb.go.jp", .record_type = "jp-gazette-notice",
    .tags = "\"jp\",\"gazette\",\"insolvency\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://search.npb.go.jp/kanpou/search.html?q={q}",
    .base = "https://search.npb.go.jp", .filter_query = 1,
    .description = "The official gazette — bankruptcy and civil rehabilitation "
      "commencement notices, company dissolutions, capital reductions, merger "
      "announcements and settlement publications. Japanese corporate distress "
      "appears here before it appears anywhere commercial" },

  { .id = "JP_JFTC_ENFORCEMENT", .name = "JFTC — antitrust cease-and-desist orders",
    .name_ja = "公正取引委員会 排除措置命令", .category = "government",
    .portal = "https://www.jftc.go.jp", .record_type = "jp-antitrust-action",
    .tags = "\"jp\",\"antitrust\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.jftc.go.jp/cgi-bin/search.cgi?q={q}",
    .base = "https://www.jftc.go.jp", .filter_query = 1,
    .description = "Japan Fair Trade Commission cease-and-desist orders and "
      "surcharge payment orders — the cartel or abuse found, the named "
      "participants, the surcharge amount and the leniency applicants" },

  /* ── Licensing and sectoral supervision ───────────────────────────────── */
  { .id = "JP_MLIT_CONSTRUCTION_LICENCE", .name = "MLIT — construction business licence search",
    .name_ja = "国土交通省 建設業者検索", .category = "government",
    .portal = "https://etsuran2.mlit.go.jp", .record_type = "jp-licence",
    .tags = "\"jp\",\"construction\",\"licence\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://etsuran2.mlit.go.jp/TAKKEN/kensetsuKensaku.do?companyName={q}",
    .base = "https://etsuran2.mlit.go.jp", .filter_query = 1,
    .description = "Licensed construction contractors — licence number and "
      "issuing prefecture, the 29 work categories permitted, the technical "
      "staff registered and any suspension. Construction licensing is where "
      "Japanese contractor sanctions become visible" },

  { .id = "JP_MLIT_REALESTATE_LICENCE", .name = "MLIT — real estate agent licence search",
    .name_ja = "国土交通省 宅建業者検索", .category = "government",
    .portal = "https://etsuran2.mlit.go.jp", .record_type = "jp-licence",
    .tags = "\"jp\",\"real-estate\",\"licence\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://etsuran2.mlit.go.jp/TAKKEN/takkenKensaku.do?companyName={q}",
    .base = "https://etsuran2.mlit.go.jp", .filter_query = 1,
    .description = "Licensed real estate brokers — licence number with its "
      "renewal count (which dates the firm), the representative, the branch "
      "offices and any administrative disposition recorded against them" },

  { .id = "JP_MLIT_LAND_TRADE_PRICES", .name = "MLIT — actual land transaction prices",
    .name_ja = "国土交通省 不動産取引価格情報", .category = "government",
    .portal = "https://www.land.mlit.go.jp", .record_type = "jp-land-transaction",
    .tags = "\"jp\",\"property\",\"prices\"", .free_tier = 1,
    .url = "https://www.land.mlit.go.jp/webland/api/TradeListSearch?from=20051&to=20244&area={qd}",
    .array_path = "data", .title_keys = "Municipality,DistrictName",
    .id_keys = "Municipality", .date_keys = "Period",
    .description = "Surveyed actual transaction prices for land and buildings — "
      "price, area, structure, building year, city planning zone and frontage "
      "road, by municipality and quarter. Requests the full history rather than "
      "a single quarter so a district's price series is complete" },

  { .id = "JP_MHLW_LABOUR_VIOLATIONS", .name = "MHLW — labour law violation disclosures",
    .name_ja = "厚生労働省 労働基準関係法令違反公表", .category = "government",
    .portal = "https://www.mhlw.go.jp", .record_type = "jp-labour-violation",
    .tags = "\"jp\",\"labour\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.mhlw.go.jp/stf/seisakunitsuite/bunya/koyou_roudou/"
      "roudoukijun/anzen/hourei/index.html?q={q}",
    .base = "https://www.mhlw.go.jp", .filter_query = 1,
    .description = "The list of employers publicly named for labour-standards "
      "violations — the establishment, the provision breached, the facts and "
      "the date of referral to prosecutors. Japan's closest analogue to a wage "
      "and safety enforcement register" },

  { .id = "JP_PMDA_APPROVALS", .name = "PMDA — drug & device approval database",
    .name_ja = "PMDA 医薬品医療機器情報", .category = "health",
    .portal = "https://www.pmda.go.jp", .record_type = "jp-medical-approval",
    .tags = "\"jp\",\"health\",\"regulatory\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.pmda.go.jp/PmdaSearch/iyakuSearch/Result?keyword={q}",
    .base = "https://www.pmda.go.jp", .filter_query = 1,
    .description = "Japanese approvals for pharmaceuticals and medical devices "
      "— the marketing authorisation holder, approval number and date, the "
      "package insert and the safety measures ordered after approval" },

  { .id = "JP_TDNET_TIMELY_DISCLOSURE", .name = "TDnet — Japanese listed company timely disclosure",
    .name_ja = "TDnet 適時開示情報", .category = "finance",
    .portal = "https://www.release.tdnet.info", .record_type = "jp-listed-disclosure",
    .tags = "\"jp\",\"listed\",\"disclosure\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.release.tdnet.info/inbs/I_main_00.html?q={q}",
    .base = "https://www.release.tdnet.info", .filter_query = 1,
    .description = "The Tokyo Stock Exchange timely disclosure network — "
      "earnings revisions, share buybacks, subsidiary transfers, third-party "
      "allotments, MBO announcements and the internal-control failures a "
      "listed company must report within the day it occurs" },

  /* ── Public money and procurement ─────────────────────────────────────── */
  { .id = "JP_PPORTAL_PROCUREMENT", .name = "Japan — government procurement portal",
    .name_ja = "調達ポータル 政府調達案件", .category = "government",
    .portal = "https://www.p-portal.go.jp", .record_type = "jp-tender",
    .tags = "\"jp\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.p-portal.go.jp/pps-web-biz/UAA01/OAA0101?searchWord={q}",
    .base = "https://www.p-portal.go.jp", .filter_query = 1,
    .description = "Central government tender notices and award results — the "
      "procuring ministry, the specification, the bidders and the successful "
      "price, which together expose single-bid tenders" },

  { .id = "JP_KANKOJU_SME_PROCUREMENT", .name = "Japan — SME public demand portal (官公需)",
    .name_ja = "官公需情報ポータルサイト", .category = "government",
    .portal = "https://www.kkj.go.jp", .record_type = "jp-tender",
    .tags = "\"jp\",\"procurement\",\"sme\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.kkj.go.jp/s/?fromDate=&toDate=&keyword={q}",
    .base = "https://www.kkj.go.jp", .filter_query = 1,
    .description = "Procurement notices from national agencies, independent "
      "administrative institutions and every prefecture and municipality that "
      "publishes to the SME portal — the widest single view of Japanese public "
      "buying below the central-government threshold" },

  { .id = "JP_ESTAT_STATSLIST", .name = "e-Stat — official statistics catalogue API",
    .name_ja = "e-Stat 統計表情報API", .category = "government",
    .portal = "https://www.e-stat.go.jp", .record_type = "jp-statistic",
    .tags = "\"jp\",\"statistics\",\"open-data\"", .key_env = "ESTAT_APP_ID",
    .free_tier = 1,
    .url = "https://api.e-stat.go.jp/rest/3.0/app/json/getStatsList?appId={key}"
      "&searchWord={q}&limit=100000",
    .array_path = "GET_STATS_LIST.DATALIST_INF.TABLE_INF",
    .title_keys = "STATISTICS_NAME,TITLE.$", .id_keys = "@id",
    .date_keys = "UPDATED_DATE",
    .description = "The index of every official Japanese statistical table — "
      "which ministry produces it, the survey behind it, the geographic and "
      "time coverage, and the table ID needed to pull the actual figures" },

  { .id = "JP_TOKYO_OPENDATA_CKAN", .name = "Tokyo Metropolitan Government — open data catalog",
    .name_ja = "東京都オープンデータカタログ", .category = "government",
    .portal = "https://catalog.data.metro.tokyo.lg.jp", .record_type = "jp-dataset",
    .tags = "\"jp\",\"tokyo\",\"open-data\"", .free_tier = 1,
    .url = "https://catalog.data.metro.tokyo.lg.jp/api/3/action/package_search"
      "?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,organization.title",
    .id_keys = "id", .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Tokyo's CKAN catalog — ward-level facility, welfare, "
      "transport and disaster datasets published by the metropolitan government "
      "and its bureaus, most of which never reach the national portal" },

  { .id = "JP_NDL_SEARCH_API", .name = "National Diet Library — search OpenSearch API",
    .name_ja = "国立国会図書館サーチAPI", .category = "research",
    .portal = "https://ndlsearch.ndl.go.jp", .record_type = "jp-bibliographic",
    .tags = "\"jp\",\"library\",\"archive\"", .free_tier = 1,
    .type = "api", .mode = HP_HTML,
    .url = "https://ndlsearch.ndl.go.jp/api/opensearch?any={q}&cnt=200",
    .base = "https://ndlsearch.ndl.go.jp", .filter_query = 1,
    .description = "The National Diet Library's union catalogue — books, "
      "periodicals, official gazettes, government reports and digitised "
      "collections held across Japanese public libraries, with holdings "
      "location for each hit" },

  { .id = "JP_SOUMU_POLITICAL_FUNDS", .name = "MIC — political funds reports (政治資金収支報告書)",
    .name_ja = "総務省 政治資金収支報告書", .category = "government",
    .portal = "https://www.soumu.go.jp", .record_type = "jp-political-finance",
    .tags = "\"jp\",\"political-finance\",\"transparency\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.soumu.go.jp/senkyo/seiji_s/seijishikin/?q={q}",
    .base = "https://www.soumu.go.jp", .filter_query = 1,
    .description = "Income and expenditure reports for political organisations "
      "— donations by company and individual, party subsidy allocations and "
      "the fundraising-party receipts that dominate Japanese political money" },

  { .id = "JP_DATA_GO_JP_SEARCH", .name = "data.go.jp — national open data catalog",
    .name_ja = "データカタログサイト 検索", .category = "government",
    .portal = "https://www.data.go.jp", .record_type = "jp-dataset",
    .tags = "\"jp\",\"open-data\",\"catalog\"", .free_tier = 1,
    .url = "https://www.data.go.jp/data/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,organization.title",
    .id_keys = "id", .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Japan's national CKAN catalog across all ministries — which "
      "body holds a dataset, the resource files behind it and the update "
      "cadence; the directory step before pulling the raw ministry data" },
};

HP_REGISTER_TABLE(HP3_GOV_JP)
