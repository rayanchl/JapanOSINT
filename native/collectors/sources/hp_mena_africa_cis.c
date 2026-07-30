/* collectors/sources/hp_mena_africa_cis.c — MENA, Africa and CIS deep records.
 *
 * These are the thinnest regions in the existing set, and the ones where a
 * single real endpoint changes an investigation: Rwanda and Kenya run open
 * company APIs, South Africa publishes court judgments in full text, the Gulf
 * free zones publish licence registers, Israel and Georgia publish company data
 * as open datasets, and Russian/Ukrainian mirrors expose ownership that the
 * primary registries charge for. Every scrape row emits real anchors or
 * nothing — several of these portals are behind CAPTCHAs and will legitimately
 * return zero from a datacenter IP. */
#include "../../lib/hpengine.h"

static const hp_source HP_MENA_AF_CIS[] = {
  /* ── Gulf & wider Middle East ──────────────────────────────────────────── */
  { .id = "AE_DED_LICENCE", .name = "Dubai DED — trade licence lookup",
    .name_ja = "ドバイ 商業許可照会", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.dubaided.gov.ae", .record_type = "ae-licence",
    .tags = "\"ae\",\"licences\"", .free_tier = 1,
    .url = "https://eservices.dubaided.gov.ae/Pages/BusinessSearch.aspx?q={q}",
    .base = "https://eservices.dubaided.gov.ae", .filter_query = 1,
    .description = "Dubai Department of Economy trade-licence search — licence "
      "number, legal form and status for a business name. Dubai licences name the "
      "local partner/sponsor, which is the point of the lookup" },

  { .id = "AE_ADGM_REGISTER", .name = "ADGM — Abu Dhabi Global Market register",
    .name_ja = "アブダビ国際市場 登記", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.adgm.com", .record_type = "ae-company",
    .tags = "\"ae\",\"free-zone\",\"registry\"", .free_tier = 1,
    .url = "https://www.adgm.com/public-registers/registration-authority?search={q}",
    .base = "https://www.adgm.com", .filter_query = 1,
    .description = "ADGM public register hits — free-zone entities, their licence "
      "categories and status. Gulf free zones are the standard vehicle for regional "
      "holding structures, and their registers are the only public view of them" },

  { .id = "SA_MC_COMMERCIAL", .name = "Saudi Ministry of Commerce — CR lookup",
    .name_ja = "サウジ 商業登記照会", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://mc.gov.sa", .record_type = "sa-company",
    .tags = "\"sa\",\"registry\"", .free_tier = 1,
    .url = "https://mc.gov.sa/en/eservices/Pages/ServiceDetails.aspx?q={q}",
    .base = "https://mc.gov.sa", .filter_query = 1,
    .description = "Saudi commercial-registration lookup surface for a company name "
      "or CR number. Heavily gated; emits only what the public page actually "
      "renders" },

  { .id = "QA_MOCI_REGISTER", .name = "Qatar MOCI — commercial register search",
    .name_ja = "カタール 商業登記", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.moci.gov.qa", .record_type = "qa-company",
    .tags = "\"qa\",\"registry\"", .free_tier = 1,
    .url = "https://www.moci.gov.qa/en/search/?q={q}",
    .base = "https://www.moci.gov.qa", .filter_query = 1,
    .description = "Qatari Ministry of Commerce and Industry register and circular "
      "search — commercial registrations, trade names and published notices naming "
      "an entity" },

  { .id = "IL_COMPANIES_DATA", .name = "Israel — companies register open dataset",
    .name_ja = "イスラエル 企業登記データ", .category = "government",
    .portal = "https://data.gov.il", .record_type = "il-company",
    .tags = "\"il\",\"registry\"", .free_tier = 1,
    .url = "https://data.gov.il/api/3/action/datastore_search?resource_id="
           "f004176c-b85f-4542-8901-7b3176f9a054&q={q}&limit=40",
    .array_path = "result.records", .title_keys = "שם חברה,CompanyName",
    .id_keys = "מספר חברה,CompanyNumber",
    .description = "Israeli Registrar of Companies open dataset — company number, "
      "Hebrew and English names, status, type, address and whether the company is a "
      "government or public company" },

  { .id = "TR_MERSIS_GAZETTE", .name = "Turkey — trade registry gazette search",
    .name_ja = "トルコ 商業登記官報", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.ticaretsicil.gov.tr", .record_type = "tr-gazette",
    .tags = "\"tr\",\"gazette\",\"registry\"", .free_tier = 1,
    .url = "https://www.ticaretsicil.gov.tr/view/hizmetlerimiz/ilanlar.php?ara={q}",
    .base = "https://www.ticaretsicil.gov.tr", .filter_query = 1,
    .description = "Turkish Trade Registry Gazette announcements naming an entity — "
      "incorporations, capital increases, board changes and liquidations, which is "
      "where Turkish corporate change is legally published" },

  /* ── Africa ────────────────────────────────────────────────────────────── */
  { .id = "ZA_CIPC_DISCLOSURE", .name = "South Africa CIPC — enterprise enquiry",
    .name_ja = "南アフリカ 企業登記(CIPC)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.cipc.co.za", .record_type = "za-company",
    .tags = "\"za\",\"registry\"", .free_tier = 1,
    .url = "https://www.bizportal.gov.za/CompanySearch?q={q}",
    .base = "https://www.bizportal.gov.za", .filter_query = 1,
    .description = "South African company and close-corporation search via BizPortal "
      "— registration number, status and type; CIPC's own API is credential-gated, "
      "so this is the honest public surface" },

  { .id = "ZA_SAFLII_JUDGMENTS", .name = "SAFLII — Southern African judgments",
    .name_ja = "南部アフリカ 判例(SAFLII)", .category = "legal",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.saflii.org", .record_type = "za-judgment",
    .tags = "\"za\",\"courts\"", .free_tier = 1,
    .url = "https://www.saflii.org/cgi-bin/sinosrch.cgi?query={q}&method=auto&meta=%2Fsaflii",
    .href_must = "/cases/", .base = "https://www.saflii.org",
    .description = "Full-text judgments from South African and regional courts "
      "naming an entity — the litigation, liquidation and business-rescue record "
      "that CIPC does not hold" },

  { .id = "NG_CAC_SEARCH", .name = "Nigeria CAC — company public search",
    .name_ja = "ナイジェリア 企業登記(CAC)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://search.cac.gov.ng", .record_type = "ng-company",
    .tags = "\"ng\",\"registry\"", .free_tier = 1,
    .url = "https://search.cac.gov.ng/home?q={q}",
    .base = "https://search.cac.gov.ng", .filter_query = 1,
    .description = "Nigerian Corporate Affairs Commission public search — RC number, "
      "company name and status for matches; the entry point to Nigeria's company "
      "and business-name registers" },

  { .id = "KE_BRS_ODATA", .name = "Kenya open data — business register & tenders",
    .name_ja = "ケニア 企業・調達データ", .category = "government",
    .portal = "https://www.opendata.go.ke", .record_type = "ke-record",
    .tags = "\"ke\",\"registry\",\"procurement\"", .free_tier = 1,
    .url = "https://africaopendata.org/api/3/action/package_search?q={q}&rows=25",
    .array_path = "result.results", .title_keys = "title", .id_keys = "name",
    .date_keys = "metadata_modified",
    .description = "Kenyan and pan-African open datasets matching an entity or "
      "sector — company registers, procurement awards and licence lists published "
      "through the Africa Open Data CKAN network" },

  { .id = "RW_DOMESTIC_REGISTRY", .name = "Rwanda RDB — business register",
    .name_ja = "ルワンダ 企業登記(RDB)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://org.rdb.rw", .record_type = "rw-company",
    .tags = "\"rw\",\"registry\"", .free_tier = 1,
    .url = "https://org.rdb.rw/busregonline/Home/SearchCompany?companyName={q}",
    .base = "https://org.rdb.rw", .filter_query = 1,
    .description = "Rwanda Development Board business register — one of the few "
      "African registries that publishes company code, status, shareholders and "
      "directors without payment" },

  { .id = "GH_RGD_SEARCH", .name = "Ghana Registrar-General — entity search",
    .name_ja = "ガーナ 企業登記", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://rgd.gov.gh", .record_type = "gh-company",
    .tags = "\"gh\",\"registry\"", .free_tier = 1,
    .url = "https://rgdeservices.com/search?q={q}",
    .base = "https://rgdeservices.com", .filter_query = 1,
    .description = "Ghanaian Registrar-General company and business-name search — "
      "registration number, type and status per match" },

  { .id = "AFRICA_OPENOWNERSHIP", .name = "OpenOwnership — beneficial ownership register",
    .name_ja = "実質的支配者登記(OpenOwnership)", .category = "government",
    .portal = "https://register.openownership.org", .record_type = "beneficial-owner",
    .tags = "\"global\",\"beneficial-ownership\"", .free_tier = 1,
    .url = "https://register.openownership.org/search?q={q}",
    .mode = HP_HTML, .type = "scraped",
    .href_must = "/entities/", .base = "https://register.openownership.org",
    .description = "The OpenOwnership beneficial-ownership register — cross-border "
      "ownership statements pooled from UK PSC, Denmark, Slovakia, Nigeria, Ghana, "
      "Armenia and others, entity by entity" },

  /* ── Russia / CIS ──────────────────────────────────────────────────────── */
  { .id = "RU_RUSPROFILE_SEARCH", .name = "Rusprofile — Russian company mirror",
    .name_ja = "ロシア 企業情報(Rusprofile)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.rusprofile.ru", .record_type = "ru-company",
    .tags = "\"ru\",\"registry\",\"ownership\"", .free_tier = 1,
    .url = "https://www.rusprofile.ru/search?query={q}",
    .href_must = "/id/", .base = "https://www.rusprofile.ru",
    .description = "Russian legal entities and sole traders matching a name, INN or "
      "OGRN on the EGRUL mirror — each hit leads to founders, director history, "
      "capital, affiliations and arbitration cases" },

  { .id = "RU_LIST_ORG", .name = "List-Org — Russian entity directory",
    .name_ja = "ロシア 企業ディレクトリ(List-Org)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.list-org.com", .record_type = "ru-company",
    .tags = "\"ru\",\"registry\"", .free_tier = 1,
    .url = "https://www.list-org.com/search?type=all&val={q}",
    .href_must = "/company/", .base = "https://www.list-org.com",
    .description = "Second independent Russian EGRUL mirror — INN/OGRN, registered "
      "address, activity codes, director and reported revenue. Cross-reading two "
      "mirrors is how you tell a real record from a stale one" },

  { .id = "RU_ARBITR_CASES", .name = "kad.arbitr — Russian arbitration cases",
    .name_ja = "ロシア 商事裁判事件", .category = "legal",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://kad.arbitr.ru", .record_type = "ru-court-case",
    .tags = "\"ru\",\"courts\"", .free_tier = 1,
    .url = "https://kad.arbitr.ru/Card?q={q}",
    .base = "https://kad.arbitr.ru", .filter_query = 1,
    .description = "Russian commercial-court (arbitrazh) case search for a party — "
      "case number, court, parties and stage. Aggressively bot-protected, so a zero "
      "result here is expected rather than evidence of no litigation" },

  { .id = "UA_CLARITY_PROJECT", .name = "Clarity Project — Ukrainian entity & tender graph",
    .name_ja = "ウクライナ 企業・調達(Clarity)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://clarity-project.info", .record_type = "ua-company",
    .tags = "\"ua\",\"registry\",\"procurement\"", .free_tier = 1,
    .url = "https://clarity-project.info/search?q={q}",
    .base = "https://clarity-project.info", .filter_query = 1,
    .description = "Ukrainian companies, beneficial owners, tenders and court cases "
      "linked in one graph — the analyst-facing mirror of EDR, Prozorro and the "
      "court registry" },

  { .id = "KZ_ADATA_SEARCH", .name = "adata.kz — Kazakh company data",
    .name_ja = "カザフスタン 企業情報", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://adata.kz", .record_type = "kz-company",
    .tags = "\"kz\",\"registry\"", .free_tier = 1,
    .url = "https://adata.kz/search?query={q}",
    .base = "https://adata.kz", .filter_query = 1,
    .description = "Kazakh legal entities by name or BIN — registration data, "
      "director, activity, tax and state-procurement participation as republished "
      "from official sources" },

  { .id = "GE_OPENDATA_COMPANY", .name = "Georgia — public registry data search",
    .name_ja = "ジョージア 企業登記データ", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://enreg.reestri.gov.ge", .record_type = "ge-company",
    .tags = "\"ge\",\"registry\"", .free_tier = 1,
    .url = "https://enreg.reestri.gov.ge/main.php?m=new_index&s={q}",
    .base = "https://enreg.reestri.gov.ge", .filter_query = 1,
    .description = "Georgian National Agency of Public Registry search — entity "
      "identification code, status and the extract links that carry directors and "
      "share ownership" },

  { .id = "AM_ARMEPS_TENDERS", .name = "Armenia ARMEPS — procurement notices",
    .name_ja = "アルメニア 公共調達", .category = "government",
    .portal = "https://armeps.am", .record_type = "am-tender",
    .tags = "\"am\",\"procurement\"", .free_tier = 1,
    .url = "https://armeps-ocds.gov.am/api/v1/records?q={q}",
    .array_path = "records", .title_keys = "compiledRelease.tender.title,ocid",
    .id_keys = "ocid", .date_keys = "compiledRelease.date",
    .description = "Armenian procurement records in OCDS form — buyer, tender title, "
      "value, awards and suppliers, keyed by OCID so a supplier's whole public-"
      "contract history is traceable" },
};

HP_REGISTER_TABLE(HP_MENA_AF_CIS)
