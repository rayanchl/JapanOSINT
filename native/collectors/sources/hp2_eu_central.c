/* collectors/sources/hp2_eu_central.c — Central & South-East Europe, Ukraine.
 *
 * This is the part of Europe where the state registers are genuinely open and
 * almost nothing is wired: Slovenia publishes every payment made by every
 * public body, Poland publishes the VAT status and settlement account of every
 * taxpayer, the Czech insolvency register is fully public, and Ukraine's open
 * data portal carries the corporate register itself. The region also carries
 * the highest concentration of sanctions-circumvention routing in Europe, so
 * the procurement and tax-status rows here answer questions no western register
 * can. */
#include "lib/hpengine.h"

static const hp_source HP2_EU_CENTRAL[] = {
  /* ── Poland ────────────────────────────────────────────────────────────── */
  { .id = "PL_CEIDG_SOLE_TRADERS", .name = "Poland CEIDG — sole traders register",
    .name_ja = "ポーランド 個人事業者登録", .category = "government",
    .portal = "https://aplikacja.ceidg.gov.pl", .record_type = "pl-sole-trader",
    .tags = "\"pl\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://aplikacja.ceidg.gov.pl/ceidg/ceidg.public.ui/Search.aspx?q={q}",
    .base = "https://aplikacja.ceidg.gov.pl", .filter_query = 1,
    .description = "Polish sole traders and their businesses — NIP, REGON, "
      "business address, PKD activity codes, start and suspension dates and "
      "the licences held. KRS covers companies; this covers the far larger "
      "population of individual entrepreneurs" },

  /* The podatki.gov.pl search page — a different endpoint from
   * reg2_taxid_pivots.c's PL_VAT_WHITELIST, which calls the MF API at
   * wl-api.mf.gov.pl. Two real sources that had collided on one id (R5). */
  { .id = "PL_VAT_WHITELIST_SEARCH", .name = "Poland — VAT taxpayer whitelist",
    .name_ja = "ポーランド VAT納税者ホワイトリスト", .category = "government",
    .portal = "https://www.podatki.gov.pl", .record_type = "pl-vat-status",
    .tags = "\"pl\",\"tax\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.podatki.gov.pl/wykaz-podatnikow-vat-wyszukiwarka/?q={q}",
    .base = "https://www.podatki.gov.pl", .filter_query = 1,
    .description = "Poland's VAT whitelist — registration status (active, "
      "exempt, refused, struck off, restored), the reason for any refusal or "
      "removal, and the bank accounts the taxpayer declared. Paying a Polish "
      "supplier into an account not on this list breaks the deduction, which "
      "makes the account data unusually reliable" },

  { .id = "PL_EZAMOWIENIA_TENDERS", .name = "Poland — public procurement notices",
    .name_ja = "ポーランド 公共調達公告", .category = "government",
    .portal = "https://ezamowienia.gov.pl", .record_type = "pl-tender",
    .tags = "\"pl\",\"procurement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://ezamowienia.gov.pl/mp-client/search/list?searchPhrase={q}",
    .base = "https://ezamowienia.gov.pl", .filter_query = 1,
    .description = "Polish contract notices and award results — contracting "
      "authority, procedure, the bidders and the winning offer with its price. "
      "Poland publishes the losing bids too, which reveals the competitive set "
      "around a supplier" },

  { .id = "PL_KNF_WARNINGS", .name = "Poland KNF — public warning list",
    .name_ja = "ポーランド 金融監督庁 警告リスト", .category = "government",
    .portal = "https://www.knf.gov.pl", .record_type = "pl-regulator-warning",
    .tags = "\"pl\",\"financial\",\"warning\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.knf.gov.pl/szukaj?query={q}",
    .base = "https://www.knf.gov.pl", .filter_query = 1,
    .description = "Polish financial supervision authority public warnings — "
      "entities reported to prosecutors for operating without a licence, with "
      "the entity name, the notification date and the case reference" },

  /* ── Czechia & Slovakia ────────────────────────────────────────────────── */
  { .id = "CZ_ISIR_INSOLVENCY", .name = "Czechia ISIR — insolvency register",
    .name_ja = "チェコ 倒産登録", .category = "government",
    .portal = "https://isir.justice.cz", .record_type = "cz-insolvency",
    .tags = "\"cz\",\"insolvency\",\"court\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://isir.justice.cz/isir/ueu/evidence_upadcu_result.do?nazevOrg={q}",
    .base = "https://isir.justice.cz", .filter_query = 1,
    .description = "The Czech insolvency register — debtor, IČO, the "
      "insolvency court and file number, the administrator appointed and every "
      "document in the file. Fully public, including the creditor register" },

  { .id = "CZ_JUSTICE_SBIRKA", .name = "Czechia — commercial register & document collection",
    .name_ja = "チェコ 商業登記/文書集", .category = "government",
    .portal = "https://or.justice.cz", .record_type = "cz-entity",
    .tags = "\"cz\",\"registry\",\"filings\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://or.justice.cz/ias/ui/rejstrik-$firma?nazev={q}",
    .base = "https://or.justice.cz", .filter_query = 1,
    .description = "Czech commercial register entries and, behind each, the "
      "sbírka listin — the actual filed documents: articles, annual accounts, "
      "shareholder resolutions and merger projects" },

  { .id = "CZ_CNB_REGISTER", .name = "Czechia CNB — supervised entities register",
    .name_ja = "チェコ中央銀行 監督対象登録", .category = "government",
    .portal = "https://apl.cnb.cz", .record_type = "cz-financial-entity",
    .tags = "\"cz\",\"financial\",\"licence\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://apl.cnb.cz/apljerrsdad/JERRS.WEB07.INTRO_PAGE?p_lang=en&p_name={q}",
    .base = "https://apl.cnb.cz", .filter_query = 1,
    .description = "Entities supervised by the Czech National Bank — banks, "
      "investment intermediaries, insurance agents and payment institutions — "
      "with licence scope, effective dates and the sanction decisions on file" },

  { .id = "SK_ORSR_EXTRACT", .name = "Slovakia ORSR — commercial register extract",
    .name_ja = "スロバキア 商業登記抄本", .category = "government",
    .portal = "https://www.orsr.sk", .record_type = "sk-entity",
    .tags = "\"sk\",\"registry\",\"ownership\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.orsr.sk/hladaj_subjekt.asp?OBMENO={q}&PF=0&SID=0&S=on",
    .base = "https://www.orsr.sk", .filter_query = 1,
    .description = "Slovak commercial register extracts — IČO, legal form, "
      "the executive body with each member's address and effective dates, the "
      "shareholders and their contributions, and the full history of changes" },

  { .id = "SK_UVO_TENDERS", .name = "Slovakia ÚVO — public procurement office",
    .name_ja = "スロバキア 公共調達庁", .category = "government",
    .portal = "https://www.uvo.gov.sk", .record_type = "sk-tender",
    .tags = "\"sk\",\"procurement\",\"debarment\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.uvo.gov.sk/vyhladavanie?q={q}",
    .base = "https://www.uvo.gov.sk", .filter_query = 1,
    .description = "Slovak procurement notices and awards, the register of "
      "public-sector partners (Slovakia's beneficial-ownership register for "
      "anyone taking public money) and the register of economic operators "
      "banned from participating in tenders" },

  /* ── Hungary & Slovenia ────────────────────────────────────────────────── */
  { .id = "HU_CEGJEGYZEK", .name = "Hungary — company register search",
    .name_ja = "ハンガリー 会社登記", .category = "government",
    .portal = "https://www.e-cegjegyzek.hu", .record_type = "hu-entity",
    .tags = "\"hu\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.e-cegjegyzek.hu/?cegkereses&nev={q}",
    .base = "https://www.e-cegjegyzek.hu", .filter_query = 1,
    .description = "Hungarian company register — registration number, seat, "
      "activity, representatives and members, plus the status flags for "
      "liquidation, forced deletion and tax-number suspension" },

  { .id = "HU_EKR_TENDERS", .name = "Hungary EKR — electronic procurement system",
    .name_ja = "ハンガリー 電子調達", .category = "government",
    .portal = "https://ekr.gov.hu", .record_type = "hu-tender",
    .tags = "\"hu\",\"procurement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://ekr.gov.hu/portal/kozbeszerzes/eljarasok?q={q}",
    .base = "https://ekr.gov.hu", .filter_query = 1,
    .description = "Hungarian procurement procedures and awards, including "
      "EU-funded contracts — contracting authority, procedure type, bidders "
      "and the winning price. The concentration pattern in these awards is the "
      "documented basis of the EU's conditionality proceedings" },

  { .id = "SI_AJPES_REGISTRY", .name = "Slovenia AJPES — business register",
    .name_ja = "スロベニア 企業登録", .category = "government",
    .portal = "https://www.ajpes.si", .record_type = "si-entity",
    .tags = "\"si\",\"registry\",\"accounts\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.ajpes.si/prs/PrsSearch.aspx?q={q}",
    .base = "https://www.ajpes.si", .filter_query = 1,
    .description = "Slovenian business register — registration number, tax "
      "number, founders and their holdings, representatives, and the annual "
      "reports AJPES publishes for every registered entity" },

  { .id = "SI_ERAR_PAYMENTS", .name = "Slovenia Erar — public sector payments",
    .name_ja = "スロベニア 公的支出データベース", .category = "government",
    .portal = "https://erar.si", .record_type = "si-public-payment",
    .tags = "\"si\",\"procurement\",\"payments\",\"transparency\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://erar.si/iskanje/?q={q}",
    .base = "https://erar.si", .filter_query = 1,
    .description = "Every payment made by every Slovenian public body to every "
      "supplier, by month and counterparty. Not tenders — actual transfers. "
      "Very few countries publish this, and it makes the whole "
      "state-to-private money flow queryable by company name" },

  /* ── Croatia, Romania, Bulgaria, Serbia ────────────────────────────────── */
  { .id = "HR_EOJN_TENDERS", .name = "Croatia EOJN — public procurement bulletin",
    .name_ja = "クロアチア 公共調達公告", .category = "government",
    .portal = "https://eojn.nn.hr", .record_type = "hr-tender",
    .tags = "\"hr\",\"procurement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://eojn.nn.hr/Oglasnik/?q={q}",
    .base = "https://eojn.nn.hr", .filter_query = 1,
    .description = "Croatian tender notices and award decisions with the "
      "contracting authority, the bidders and the awarded value, plus the "
      "register of economic operators excluded from procurement" },

  { .id = "RO_ONRC_RECOM", .name = "Romania ONRC — trade register",
    .name_ja = "ルーマニア 商業登記", .category = "government",
    .portal = "https://portal.onrc.ro", .record_type = "ro-entity",
    .tags = "\"ro\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://portal.onrc.ro/ONRCPortalWeb/ONRCPortal.portal?q={q}",
    .base = "https://portal.onrc.ro", .filter_query = 1,
    .description = "Romanian trade register — CUI, registration number, seat, "
      "administrators, associates and share capital, plus the insolvency and "
      "dissolution flags recorded against the entity" },

  { .id = "RO_ANAF_TAXPAYERS", .name = "Romania ANAF — taxpayer & VAT status",
    .name_ja = "ルーマニア 納税者/VAT状況", .category = "government",
    .portal = "https://mfinante.gov.ro", .record_type = "ro-tax-status",
    .tags = "\"ro\",\"tax\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://mfinante.gov.ro/ro/web/site/info-contribuabili?cod={q}",
    .base = "https://mfinante.gov.ro", .filter_query = 1,
    .description = "Romanian taxpayer records — VAT registration and any "
      "cancellation, inactive-taxpayer status, split-VAT enrolment and the "
      "published balance-sheet indicators. Inactive status invalidates a "
      "counterparty's invoices under Romanian law" },

  { .id = "RO_SICAP_TENDERS", .name = "Romania SEAP/SICAP — electronic procurement",
    .name_ja = "ルーマニア 電子調達", .category = "government",
    .portal = "https://e-licitatie.ro", .record_type = "ro-tender",
    .tags = "\"ro\",\"procurement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://e-licitatie.ro/pub/notices/contract-notices/list/0/0?q={q}",
    .base = "https://e-licitatie.ro", .filter_query = 1,
    .description = "Romanian contract and award notices — authority, CPV, "
      "estimated and awarded value and the winning economic operator, "
      "including the direct acquisitions that dominate by count" },

  { .id = "BG_TRADE_REGISTER", .name = "Bulgaria — commercial register (BULSTAT/TR)",
    .name_ja = "ブルガリア 商業登記", .category = "government",
    .portal = "https://portal.registryagency.bg", .record_type = "bg-entity",
    .tags = "\"bg\",\"registry\",\"ownership\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://portal.registryagency.bg/CR/en/Reports/VerificationPersonOrg?name={q}",
    .base = "https://portal.registryagency.bg", .filter_query = 1,
    .description = "The Bulgarian commercial register — UIC, managers, "
      "partners and their shares, capital, and the scanned filed documents. "
      "Bulgaria publishes the actual company file free of charge, including "
      "the beneficial-ownership declarations under the AML act" },

  { .id = "BG_EOP_TENDERS", .name = "Bulgaria — centralised procurement platform",
    .name_ja = "ブルガリア 公共調達", .category = "government",
    .portal = "https://app.eop.bg", .record_type = "bg-tender",
    .tags = "\"bg\",\"procurement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://app.eop.bg/today?q={q}",
    .base = "https://app.eop.bg", .filter_query = 1,
    .description = "Bulgarian procurement procedures, decisions and contracts "
      "— the contracting authority, participants, the award decision and the "
      "appeals lodged against it" },

  { .id = "RS_APR_REGISTRY", .name = "Serbia APR — business registers agency",
    .name_ja = "セルビア 企業登録庁", .category = "government",
    .portal = "https://pretraga2.apr.gov.rs", .record_type = "rs-entity",
    .tags = "\"rs\",\"registry\",\"ownership\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://pretraga2.apr.gov.rs/unifiedentitysearch/Search?q={q}",
    .base = "https://pretraga2.apr.gov.rs", .filter_query = 1,
    .description = "Serbian companies, entrepreneurs, associations and their "
      "financial statements in one unified search — registration number, PIB, "
      "founders, directors and the beneficial owners recorded in the separate "
      "central register" },

  /* ── Ukraine ───────────────────────────────────────────────────────────── */
  { .id = "UA_OPENDATA_CKAN", .name = "Ukraine data.gov.ua — open data catalogue",
    .name_ja = "ウクライナ オープンデータ", .category = "government",
    .portal = "https://data.gov.ua", .record_type = "ua-dataset",
    .tags = "\"ua\",\"opendata\",\"registry\"", .free_tier = 1,
    .url = "https://data.gov.ua/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,name",
    .id_keys = "id", .date_keys = "metadata_modified",
    .link_keys = "name", .link_tmpl = "https://data.gov.ua/dataset/{v}",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "Ukraine's open data portal, which carries the unified "
      "state register of legal entities itself along with the property, "
      "vehicle, licence and sanctions datasets — with the downloadable "
      "resources behind every matching dataset, paged to exhaustion" },
};

HP_REGISTER_TABLE(HP2_EU_CENTRAL)
