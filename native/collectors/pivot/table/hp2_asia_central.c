/* collectors/sources/hp2_asia_central.c — Central Asia & Caucasus critical values.
 *
 * Kazakhstan publishes its entire state-procurement system as an open API, and
 * several neighbours publish company and tax registers that are far more open
 * than their reputation suggests. For sanctions-adjacent work this region is
 * where re-export routes and shell intermediaries are registered, so the
 * procurement and company records here carry disproportionate weight. */
#include "lib/hpengine.h"

static const hp_source HP2_ASIA_CENTRAL[] = {
  /* ── Kazakhstan ────────────────────────────────────────────────────────── */
  { .id = "KZ_GOSZAKUP_SUPPLIERS", .name = "Kazakhstan goszakup — registered suppliers",
    .name_ja = "カザフスタン 公共調達 供給者", .category = "government",
    .portal = "https://goszakup.gov.kz", .record_type = "kz-supplier",
    .tags = "\"kz\",\"procurement\"", .free_tier = 1,
    .url = "https://ows.goszakup.gov.kz/v3/subject/all?name={q}&limit=100",
    .array_path = "items", .title_keys = "name_ru,name_kz", .id_keys = "pid",
    .date_keys = "crdate", .page_param = "offset", .page_size = 100, .page_start = 0,
    .description = "Suppliers registered in Kazakhstan's state procurement system "
      "— BIN/IIN, legal name, address, whether the supplier is a small business "
      "and whether it appears in the unreliable-suppliers register" },

  { .id = "KZ_GOSZAKUP_CONTRACTS", .name = "Kazakhstan goszakup — awarded contracts",
    .name_ja = "カザフスタン 公共調達 契約", .category = "government",
    .portal = "https://goszakup.gov.kz", .record_type = "kz-contract",
    .tags = "\"kz\",\"procurement\",\"contracts\"", .free_tier = 1,
    .url = "https://ows.goszakup.gov.kz/v3/contract/all?supplier_biin={qd}&limit=100",
    .array_path = "items", .title_keys = "contract_number_sys,descr",
    .id_keys = "id", .date_keys = "sign_date",
    .page_param = "offset", .page_size = 100, .page_start = 0,
    .description = "Every state contract awarded to a Kazakh supplier BIN — "
      "customer, sum, signing date and status. The full public-money history of "
      "a company, in one query" },

  { .id = "KZ_GOSZAKUP_UNRELIABLE", .name = "Kazakhstan — unreliable suppliers register",
    .name_ja = "カザフスタン 不誠実供給者登録", .category = "government",
    .portal = "https://goszakup.gov.kz", .record_type = "kz-debarment",
    .tags = "\"kz\",\"debarment\",\"blacklist\"", .free_tier = 1,
    .url = "https://ows.goszakup.gov.kz/v3/rnu?limit=100",
    .array_path = "items", .filter_query = 1, .title_keys = "supplier_name_ru,supplier_biin",
    .id_keys = "supplier_biin", .date_keys = "start_date",
    .page_param = "offset", .page_size = 100, .page_start = 0,
    .description = "Kazakhstan's register of unreliable suppliers, filtered to "
      "the query — the debarment list that bars a company from state contracts, "
      "with the court decision and the period" },

  { .id = "KZ_STAT_ENTITIES", .name = "Kazakhstan — statistical business register",
    .name_ja = "カザフスタン 統計企業登録", .category = "government",
    .portal = "https://stat.gov.kz", .record_type = "kz-company",
    .tags = "\"kz\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://stat.gov.kz/ru/search/?q={q}",
    .base = "https://stat.gov.kz", .filter_query = 1,
    .description = "Kazakh statistical register entries — BIN, OKED activity "
      "code, size band and registration date; the state's own view of what a "
      "company actually does" },

  /* ── Uzbekistan / Kyrgyzstan / Tajikistan / Turkmenistan ───────────────── */
  { .id = "UZ_OPENINFO_COMPANY", .name = "Uzbekistan — open business register",
    .name_ja = "ウズベキスタン 企業登録", .category = "government",
    .portal = "https://openinfo.uz", .record_type = "uz-company",
    .tags = "\"uz\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://openinfo.uz/ru/search/?q={q}",
    .base = "https://openinfo.uz", .filter_query = 1,
    .description = "Uzbek open business information — INN, registered address, "
      "founders and activity; Uzbekistan opened this register as part of its "
      "investment reforms and it is unusually complete" },

  { .id = "UZ_XARID_TENDERS", .name = "Uzbekistan xarid — public procurement",
    .name_ja = "ウズベキスタン 公共調達", .category = "government",
    .portal = "https://xarid.uzex.uz", .record_type = "uz-tender",
    .tags = "\"uz\",\"procurement\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://xarid.uzex.uz/search?query={q}",
    .base = "https://xarid.uzex.uz", .filter_query = 1,
    .description = "Uzbek state procurement lots and results through the "
      "commodity exchange — customer, lot value and winner; the main channel "
      "for state purchases in the country" },

  { .id = "KG_TAX_REGISTRY", .name = "Kyrgyzstan — taxpayer & entity register",
    .name_ja = "キルギス 納税者登録", .category = "government",
    .portal = "https://www.sti.gov.kg", .record_type = "kg-company",
    .tags = "\"kg\",\"registry\",\"tax\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.sti.gov.kg/search?q={q}",
    .base = "https://www.sti.gov.kg", .filter_query = 1,
    .description = "Kyrgyz taxpayer register lookups — INN, registered name and "
      "status. Kyrgyzstan is a common re-export node, so the registration date "
      "of a trading company matters as much as its name" },

  { .id = "TJ_COMPANY_REGISTRY", .name = "Tajikistan — unified state register",
    .name_ja = "タジキスタン 企業登録", .category = "government",
    .portal = "https://andoz.tj", .record_type = "tj-company",
    .tags = "\"tj\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://andoz.tj/search?q={q}",
    .base = "https://andoz.tj", .filter_query = 1,
    .description = "Tajik tax committee entity lookups — INN and registration "
      "status for legal entities and individual entrepreneurs" },

  /* ── Caucasus ──────────────────────────────────────────────────────────── */
  { .id = "AZ_TAX_ENTITIES", .name = "Azerbaijan — commercial legal entity register",
    .name_ja = "アゼルバイジャン 法人登記", .category = "government",
    .portal = "https://www.e-taxes.gov.az", .record_type = "az-company",
    .tags = "\"az\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.e-taxes.gov.az/ebyn/commercialSearch.jsp?name={q}",
    .base = "https://www.e-taxes.gov.az", .filter_query = 1,
    .description = "Azerbaijani commercial register — VÖEN tax number, legal "
      "form, registration date and legal representative; the register behind "
      "Caspian energy and logistics counterparties" },

  { .id = "AM_COMPANY_REGISTRY", .name = "Armenia — state register of legal entities",
    .name_ja = "アルメニア 法人登記", .category = "government",
    .portal = "https://www.e-register.am", .record_type = "am-company",
    .tags = "\"am\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.e-register.am/en/search?name={q}",
    .base = "https://www.e-register.am", .filter_query = 1,
    .description = "Armenian state register — company, founders, director and "
      "share distribution. Armenia publishes beneficial-ownership declarations "
      "for extractive and media companies through this register" },

  { .id = "GE_PROCUREMENT_SPA", .name = "Georgia — state procurement (SPA)",
    .name_ja = "ジョージア 公共調達", .category = "government",
    .portal = "https://tenders.procurement.gov.ge", .record_type = "ge-tender",
    .tags = "\"ge\",\"procurement\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://tenders.procurement.gov.ge/public/?lang=en&search={q}",
    .base = "https://tenders.procurement.gov.ge", .filter_query = 1,
    .description = "Georgian state procurement announcements and awards, plus "
      "the blacklist of suppliers barred from bidding — Georgia runs one of the "
      "most transparent procurement systems in the region" },

  { .id = "GE_COMPANY_EXTRACT", .name = "Georgia NAPR — company extract",
    .name_ja = "ジョージア 企業抄本", .category = "government",
    .portal = "https://enreg.reestri.gov.ge", .record_type = "ge-company-extract",
    .tags = "\"ge\",\"registry\",\"ownership\"", .want = HP_NUMERIC,
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://enreg.reestri.gov.ge/main.php?c=mortgage&m=find_legal_persons&s_legal_person_idnumber={qd}",
    .base = "https://enreg.reestri.gov.ge", .filter_query = 0,
    .description = "The full Georgian registry extract behind an identification "
      "number — directors, partners and their shares, plus any pledge over the "
      "company's assets" },

  /* ── Regional cross-cutting ────────────────────────────────────────────── */
  { .id = "EAEU_CUSTOMS_DECISIONS", .name = "Eurasian Economic Union — decisions & measures",
    .name_ja = "ユーラシア経済連合 決定/措置", .category = "government",
    .portal = "https://eec.eaeunion.org", .record_type = "eaeu-measure",
    .tags = "\"eaeu\",\"trade\",\"customs\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://eec.eaeunion.org/search/?q={q}",
    .base = "https://eec.eaeunion.org", .filter_query = 1,
    .description = "Eurasian Economic Commission decisions — tariff measures, "
      "anti-dumping duties and technical-regulation certificates naming firms; "
      "the customs-union layer above Russian and Kazakh national rules" },

  { .id = "CIS_INTERPOL_WANTED", .name = "CIS — interstate wanted persons list",
    .name_ja = "CIS 指名手配リスト", .category = "safety",
    .portal = "https://mvd.gov.by", .record_type = "cis-wanted",
    .tags = "\"cis\",\"wanted\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://mvd.gov.by/ru/search?q={q}",
    .base = "https://mvd.gov.by", .filter_query = 1,
    .description = "CIS interstate wanted-persons notices — often the only place "
      "a regional criminal proceeding surfaces publicly, and a cross-check "
      "against politically motivated listings" },

  { .id = "BY_UNP_REGISTRY", .name = "Belarus — unified state register (UNP)",
    .name_ja = "ベラルーシ 企業登記(UNP)", .category = "government",
    .portal = "https://egr.gov.by", .record_type = "by-company",
    .tags = "\"by\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://egr.gov.by/egrn/index.jsp?content=Find&nm={q}",
    .base = "https://egr.gov.by", .filter_query = 1,
    .description = "Belarusian unified state register — UNP number, legal form, "
      "registration date and liquidation status; sanctioned-entity work on "
      "Belarus starts here" },

  { .id = "MD_COMPANY_REGISTRY", .name = "Moldova — state register of legal entities",
    .name_ja = "モルドバ 法人登記", .category = "government",
    .portal = "https://date.gov.md", .record_type = "md-company",
    .tags = "\"md\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://date.gov.md/ckan/en/dataset?q={q}",
    .base = "https://date.gov.md", .filter_query = 1,
    .description = "Moldovan open-data register entries — company identifiers, "
      "founders and state-contract participation; the jurisdiction at the centre "
      "of the Laundromat cases" },
};

HP_REGISTER_TABLE(HP2_ASIA_CENTRAL)
