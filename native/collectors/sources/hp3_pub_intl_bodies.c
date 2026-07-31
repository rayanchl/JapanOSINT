/* collectors/sources/hp3_pub_intl_bodies.c — multilateral & treaty-body record.
 *
 * International organisations are the most under-queried public record there
 * is. They publish at record level — a World Bank project has a document set,
 * a contract award list and a supplier; an ICSID case names the investor, the
 * respondent state and the counsel; an IMO GISIS entry names the ship's
 * registered owner and its port state control detentions; a UN Panel of Experts
 * report names the companies moving the cargo — and almost none of it is
 * indexed against the company names it contains.
 *
 * These bodies also carry the debarment lists that bind across institutions:
 * a World Bank sanction is enforced by the AfDB, ADB, EBRD and IDB under the
 * cross-debarment agreement, so one lookup here disqualifies a counterparty
 * from five development banks at once. */
#include "../../lib/hpengine.h"

static const hp_source HP3_PUB_INTL[] = {
  /* ── United Nations ───────────────────────────────────────────────────── */
  { .id = "UN_DIGITAL_LIBRARY", .name = "UN Digital Library — documents & resolutions",
    .name_ja = "国連デジタルライブラリ", .category = "government",
    .portal = "https://digitallibrary.un.org", .record_type = "un-document",
    .tags = "\"un\",\"documents\",\"multilateral\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://digitallibrary.un.org/search?p={q}&rg=100",
    .base = "https://digitallibrary.un.org", .filter_query = 1,
    .description = "Security Council and General Assembly resolutions, Panel of "
      "Experts reports, Secretary-General reports and voting records. Panel of "
      "Experts reports in particular name the companies, vessels and "
      "individuals behind sanctions evasion, in detail, with evidence" },

  { .id = "UN_UNGM_PROCUREMENT", .name = "UNGM — UN system procurement notices & awards",
    .name_ja = "国連調達 公告/落札", .category = "government",
    .portal = "https://www.ungm.org", .record_type = "un-procurement",
    .tags = "\"un\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.ungm.org/Public/Notice?keyword={q}",
    .base = "https://www.ungm.org", .filter_query = 1,
    .description = "The United Nations Global Marketplace — tender notices and "
      "contract awards across every UN agency, with the awarded supplier, the "
      "country and the value. UN supply chains run through a small number of "
      "repeat vendors and this is where they are named" },

  { .id = "UN_OHCHR_TREATY_BODIES", .name = "OHCHR — treaty body & special procedures record",
    .name_ja = "国連人権 条約機関文書", .category = "government",
    .portal = "https://tbinternet.ohchr.org", .record_type = "un-human-rights-record",
    .tags = "\"un\",\"human-rights\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://tbinternet.ohchr.org/_layouts/15/TreatyBodyExternal/"
      "TBSearch.aspx?Lang=en&TreatyID=&DocTypeID=&Keyword={q}",
    .base = "https://tbinternet.ohchr.org", .filter_query = 1,
    .description = "State reports, concluding observations, individual "
      "communications and the special rapporteurs' communications to "
      "governments and companies — which name specific corporate actors and "
      "the allegations put to them, together with any reply" },

  { .id = "UN_COMTRADE_TRADE_FLOWS", .name = "UN Comtrade — official bilateral trade statistics",
    .name_ja = "国連 貿易統計", .category = "finance",
    .portal = "https://comtradeplus.un.org", .record_type = "un-trade-flow",
    .tags = "\"un\",\"trade\",\"statistics\"", .key_env = "COMTRADE_API_KEY",
    .free_tier = 1,
    .url = "https://comtradeapi.un.org/data/v1/get/C/A/HS?reporterCode={qd}"
      "&subscription-key={key}",
    .array_path = "data", .title_keys = "cmdDesc,partnerDesc",
    .id_keys = "period", .date_keys = "period",
    .description = "Reported imports and exports by commodity and partner. "
      "Mirror-statistics gaps — where A reports exporting far more to B than B "
      "reports importing — are the standard method for locating "
      "trade-based value transfer and sanctions circumvention" },

  { .id = "IMO_GISIS_SHIPS", .name = "IMO GISIS — global integrated shipping information",
    .name_ja = "IMO 船舶情報システム", .category = "maritime",
    .portal = "https://gisis.imo.org", .record_type = "imo-vessel",
    .tags = "\"un\",\"maritime\",\"vessel\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://gisis.imo.org/Public/SHIPS/Search.aspx?keyword={q}",
    .base = "https://gisis.imo.org", .filter_query = 1,
    .description = "The IMO's own ship database — IMO number, flag history, "
      "registered owner and operator, company IMO numbers, port state control "
      "detentions, casualties and reported acts of piracy against the vessel" },

  { .id = "ICAO_SAFETY_AUDITS", .name = "ICAO — state safety audit & aviation registry data",
    .name_ja = "ICAO 航空安全監査", .category = "transport",
    .portal = "https://www.icao.int", .record_type = "icao-safety-record",
    .tags = "\"un\",\"aviation\",\"safety\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.icao.int/search/pages/results.aspx?k={q}",
    .base = "https://www.icao.int", .filter_query = 1,
    .description = "ICAO's universal safety oversight audit results by state, "
      "the significant safety concerns raised, aircraft registration prefixes "
      "and the state letters that record regulatory failures" },

  /* ── Development banks: projects, contracts, debarment ────────────────── */
  { .id = "WORLDBANK_PROJECTS_API", .name = "World Bank — projects & operations API",
    .name_ja = "世界銀行 事業API", .category = "government",
    .portal = "https://projects.worldbank.org", .record_type = "mdb-project",
    .tags = "\"mdb\",\"funding\",\"development\"", .free_tier = 1,
    .url = "https://search.worldbank.org/api/v2/projects?qterm={q}&rows=100"
      "&format=json&fl=id,project_name,countryname,totalamt,boardapprovaldate,"
      "impagency,sector,theme,status,projectfinancialtype,url",
    .array_path = "projects", .title_keys = "project_name,countryname",
    .id_keys = "id", .date_keys = "boardapprovaldate",
    .page_param = "os", .page_size = 100, .page_start = 0, .page_max = 40,
    .description = "World Bank lending operations — the implementing agency, "
      "the amount, the sectors and the status, requested with an explicit field "
      "list so the full project record is returned rather than the four-field "
      "summary the API defaults to" },

  { .id = "WORLDBANK_DEBARRED_FIRMS", .name = "World Bank — debarred & cross-debarred firms",
    .name_ja = "世界銀行 制裁企業リスト", .category = "government",
    .portal = "https://www.worldbank.org", .record_type = "mdb-debarment",
    .tags = "\"mdb\",\"debarment\",\"sanctions\"", .free_tier = 1,
    .url = "https://apigwext.worldbank.org/dvsvc/v1.0/json/APPLICATION/"
      "ADOBE_EXPERIENCE_MANAGER/FIRM/SANCTIONED_FIRM",
    .array_path = "response.ZPROCSUPP", .filter_query = 1,
    .title_keys = "SUPP_NAME,COUNTRY_NAME", .id_keys = "SUPP_ID",
    .date_keys = "DEBAR_FROM_DATE",
    .description = "Firms and individuals barred from World Bank-financed "
      "contracts — the ineligibility period, the grounds, the country and the "
      "affiliates covered. Cross-debarment makes this list binding at the "
      "AfDB, ADB, EBRD and IDB simultaneously" },

  { .id = "WORLDBANK_CONTRACT_AWARDS", .name = "World Bank — major contract awards",
    .name_ja = "世界銀行 主要契約落札", .category = "government",
    .portal = "https://finances.worldbank.org", .record_type = "mdb-contract",
    .tags = "\"mdb\",\"procurement\"", .free_tier = 1,
    .url = "https://finances.worldbank.org/api/catalog/v1?q={q}&limit=100",
    .array_path = "results", .title_keys = "resource.name,resource.description",
    .id_keys = "resource.id", .date_keys = "resource.updatedAt",
    .link_keys = "permalink",
    .page_param = "offset", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "The Bank's finances catalog, which carries the major "
      "contract award dataset — the supplier, its country, the borrower "
      "country, the procurement method and the contract amount for every award "
      "above the disclosure threshold" },

  { .id = "IFC_PROJECT_DISCLOSURES", .name = "IFC — private sector investment disclosures",
    .name_ja = "IFC 投融資案件開示", .category = "finance",
    .portal = "https://disclosures.ifc.org", .record_type = "mdb-investment",
    .tags = "\"mdb\",\"investment\",\"private-sector\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://disclosures.ifc.org/results?q={q}",
    .base = "https://disclosures.ifc.org", .filter_query = 1,
    .description = "International Finance Corporation investments — the client "
      "company by name, the sponsor, the investment amount, the environmental "
      "and social category and the summary of investment information. A direct "
      "link between a private company and development finance" },

  { .id = "ADB_PROJECTS_TENDERS", .name = "Asian Development Bank — projects & procurement",
    .name_ja = "アジア開発銀行 事業/調達", .category = "government",
    .portal = "https://www.adb.org", .record_type = "mdb-project",
    .tags = "\"mdb\",\"asia\",\"funding\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.adb.org/search?keywords={q}",
    .base = "https://www.adb.org", .filter_query = 1,
    .description = "ADB sovereign and non-sovereign operations — the executing "
      "agency, loan amount, procurement plans, contract awards and the "
      "anticorruption sanctions list of debarred entities" },

  { .id = "ISDB_PROJECT_DISCLOSURE", .name = "Islamic Development Bank — projects & procurement",
    .name_ja = "イスラム開発銀行 事業/調達", .category = "government",
    .portal = "https://www.isdb.org", .record_type = "mdb-project",
    .tags = "\"mdb\",\"islamic-finance\",\"funding\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.isdb.org/projects?search={q}",
    .base = "https://www.isdb.org", .filter_query = 1,
    .description = "IsDB financing across its 57 member countries — the "
      "executing agency, the financing mode (istisna'a, murabaha, leasing), "
      "the amount and the procurement notices. Development finance for a "
      "membership that overlaps only partly with the other banks' portfolios" },

  { .id = "IATI_AID_ACTIVITIES", .name = "IATI / d-portal — aid activity & transaction data",
    .name_ja = "国際援助透明性 事業データ", .category = "government",
    .portal = "https://d-portal.org", .record_type = "aid-activity",
    .tags = "\"aid\",\"development\",\"transparency\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://d-portal.org/ctrack.html?search={q}",
    .base = "https://d-portal.org", .filter_query = 1,
    .description = "The International Aid Transparency Initiative's activity "
      "and transaction records — donor, implementing organisation, budget and "
      "each disbursement. Following the implementing partner chain is how aid "
      "money is traced to a local contractor" },

  /* ── Arbitration and international courts ─────────────────────────────── */
  { .id = "ICSID_CASE_DATABASE", .name = "ICSID — investor-state arbitration cases",
    .name_ja = "ICSID 投資家対国家仲裁", .category = "legal",
    .portal = "https://icsid.worldbank.org", .record_type = "arbitration-case",
    .tags = "\"arbitration\",\"investment\",\"disputes\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://icsid.worldbank.org/cases/case-database?keyword={q}",
    .base = "https://icsid.worldbank.org", .filter_query = 1,
    .description = "Investor-state cases — the claimant company and its "
      "nationality, the respondent state, the economic sector, the treaty "
      "invoked, the tribunal members and the outcome. An ICSID claim is a "
      "public statement that an investment has gone wrong and how much is "
      "being claimed" },

  { .id = "PCA_ARBITRATION_CASES", .name = "Permanent Court of Arbitration — cases",
    .name_ja = "常設仲裁裁判所 事件", .category = "legal",
    .portal = "https://pca-cpa.org", .record_type = "arbitration-case",
    .tags = "\"arbitration\",\"disputes\",\"treaty\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://pca-cpa.org/en/cases/?s={q}",
    .base = "https://pca-cpa.org", .filter_query = 1,
    .description = "PCA-administered inter-state, investor-state and "
      "contract-based arbitrations — the parties, the applicable rules, the "
      "tribunal and the procedural orders and awards released publicly" },

  { .id = "UNCITRAL_TRANSPARENCY_REGISTRY", .name = "UNCITRAL — treaty arbitration transparency registry",
    .name_ja = "UNCITRAL 仲裁透明性登録", .category = "legal",
    .portal = "https://www.uncitral.org", .record_type = "arbitration-case",
    .tags = "\"arbitration\",\"transparency\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.uncitral.org/transparency-registry/registry/index.jspx?q={q}",
    .base = "https://www.uncitral.org", .filter_query = 1,
    .description = "Treaty-based investor-state arbitrations conducted under "
      "the UNCITRAL transparency rules — the notice of arbitration, the "
      "submissions and the awards, published in full where the rules apply" },

  { .id = "WTO_DISPUTE_SETTLEMENT", .name = "WTO — dispute settlement & trade measures",
    .name_ja = "WTO 紛争解決/貿易措置", .category = "government",
    .portal = "https://www.wto.org", .record_type = "wto-dispute",
    .tags = "\"trade\",\"disputes\",\"multilateral\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.wto.org/english/res_e/search_e.htm?q={q}",
    .base = "https://www.wto.org", .filter_query = 1,
    .description = "WTO disputes with the complainant, respondent, measure "
      "challenged and panel findings, plus the notified anti-dumping, "
      "countervailing and safeguard measures that name the exporters affected" },

  /* ── Standards, compliance and mutual evaluation ──────────────────────── */
  { .id = "APG_FATF_STYLE_BODIES", .name = "FATF-style regional bodies — regional AML evaluations",
    .name_ja = "FATF地域体 相互審査", .category = "government",
    .portal = "https://www.apgml.org", .record_type = "aml-evaluation",
    .tags = "\"aml\",\"compliance\",\"asia-pacific\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.apgml.org/search/default.aspx?q={q}",
    .base = "https://www.apgml.org", .filter_query = 1,
    .description = "The Asia/Pacific Group on Money Laundering and its sister "
      "regional bodies assess the countries the FATF plenary never reaches "
      "directly. Their mutual evaluation and follow-up reports carry the "
      "jurisdiction-level detail — beneficial ownership access, supervision "
      "coverage, prosecution counts — that the FATF summary omits" },

  { .id = "COE_GRECO_MONEYVAL", .name = "Council of Europe — GRECO & MONEYVAL evaluations",
    .name_ja = "欧州評議会 汚職/資金洗浄審査", .category = "government",
    .portal = "https://www.coe.int", .record_type = "compliance-evaluation",
    .tags = "\"europe\",\"anti-corruption\",\"aml\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.coe.int/en/web/portal/search?p_p_id=search&q={q}",
    .base = "https://www.coe.int", .filter_query = 1,
    .description = "GRECO's anti-corruption evaluations of judiciaries, "
      "parliaments and prosecution services, and MONEYVAL's AML assessments — "
      "the institution-level findings behind a country's corruption ranking" },

  { .id = "OECD_ANTIBRIBERY_REPORTS", .name = "OECD — anti-bribery & integrity monitoring",
    .name_ja = "OECD 贈賄防止監視", .category = "government",
    .portal = "https://www.oecd.org", .record_type = "compliance-evaluation",
    .tags = "\"oecd\",\"anti-corruption\",\"enforcement\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.oecd.org/en/search.html?q={q}",
    .base = "https://www.oecd.org", .filter_query = 1,
    .description = "Working Group on Bribery phase reports, which enumerate "
      "every foreign-bribery enforcement action each signatory has taken and "
      "the cases it has closed without charge — plus the national contact point "
      "complaints against named multinationals" },

  { .id = "OSCE_ODIHR_REPORTS", .name = "OSCE ODIHR — election & human dimension reports",
    .name_ja = "OSCE 選挙監視報告", .category = "government",
    .portal = "https://www.osce.org", .record_type = "osce-report",
    .tags = "\"osce\",\"elections\",\"europe\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.osce.org/search?keys={q}",
    .base = "https://www.osce.org", .filter_query = 1,
    .description = "Election observation mission reports with the detailed "
      "findings on media ownership, campaign finance and administrative "
      "resources, plus the human dimension reporting on named states" },

  { .id = "IAEA_SAFEGUARDS_RECORD", .name = "IAEA — safeguards, incidents & facility reporting",
    .name_ja = "IAEA 保障措置/事象報告", .category = "government",
    .portal = "https://www.iaea.org", .record_type = "nuclear-record",
    .tags = "\"nuclear\",\"safeguards\",\"multilateral\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.iaea.org/search?search_api_fulltext={q}",
    .base = "https://www.iaea.org", .filter_query = 1,
    .description = "Safeguards implementation reports, board resolutions, "
      "incident and trafficking database summaries and the power reactor "
      "information system records for each named facility and operator" },

  { .id = "OPCW_DECLARATIONS", .name = "OPCW — chemical weapons convention record",
    .name_ja = "OPCW 化学兵器条約 記録", .category = "government",
    .portal = "https://www.opcw.org", .record_type = "chemical-record",
    .tags = "\"chemical\",\"nonproliferation\",\"multilateral\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.opcw.org/search?search_api_fulltext={q}",
    .base = "https://www.opcw.org", .filter_query = 1,
    .description = "Declared chemical industry facilities subject to "
      "inspection, verification reports, investigation of alleged use reports "
      "and the identification of attribution findings naming units and "
      "entities" },
};

HP_REGISTER_TABLE(HP3_PUB_INTL)
