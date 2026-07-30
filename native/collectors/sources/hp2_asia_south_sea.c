/* collectors/sources/hp2_asia_south_sea.c — South & South-East Asia critical values.
 *
 * The records that decide things in this region: India's insolvency and
 * provident-fund registers (which say whether a company actually pays its
 * staff), GST registration status, the ASEAN procurement portals, the
 * financial-regulator licensee lists, and the tax-ID checks that separate a
 * real trading company from a letterhead. */
#include "../../lib/hpengine.h"

static const hp_source HP2_ASIA_SS[] = {
  /* ── India ─────────────────────────────────────────────────────────────── */
  { .id = "IN_GST_TAXPAYER", .name = "India GST — taxpayer registration status",
    .name_ja = "インド GST 納税者照会", .category = "economy",
    .portal = "https://services.gst.gov.in", .record_type = "in-gst-registration",
    .tags = "\"in\",\"tax\",\"status\"", .want = HP_ANY, .free_tier = 1,
    .url = "https://commonapi.mastersindia.co/commonapis/searchgstin?gstin={qn}",
    .title_keys = "data.lgnm,data.tradeNam", .id_keys = "data.gstin",
    .date_keys = "data.rgdt",
    .description = "Indian GST registration behind a GSTIN — legal and trade "
      "name, registration date, constitution, jurisdiction and whether the "
      "registration is active or cancelled" },

  { .id = "IN_IBBI_INSOLVENCY", .name = "India IBBI — insolvency proceedings",
    .name_ja = "インド 倒産手続(IBBI)", .category = "legal",
    .portal = "https://ibbi.gov.in", .record_type = "in-insolvency",
    .tags = "\"in\",\"insolvency\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://ibbi.gov.in/en/search?keyword={q}",
    .base = "https://ibbi.gov.in", .filter_query = 1,
    .description = "Indian corporate insolvency resolution processes — the "
      "corporate debtor, the resolution professional appointed, the NCLT bench "
      "and the stage. India's public bankruptcy record" },

  { .id = "IN_EPFO_ESTABLISHMENT", .name = "India EPFO — employer establishment record",
    .name_ja = "インド EPFO 事業所情報", .category = "economy",
    .portal = "https://unifiedportal-emp.epfindia.gov.in", .record_type = "in-employer",
    .tags = "\"in\",\"employment\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://unifiedportal-epfo.epfindia.gov.in/publicPortal/no-auth/misReport/home/loadEstSearchHome?estName={q}",
    .base = "https://unifiedportal-epfo.epfindia.gov.in", .filter_query = 1,
    .description = "Provident-fund establishment records — employee count, "
      "contribution history and whether remittances stopped. Missed PF payments "
      "are the earliest public signal that an Indian employer is failing" },

  { .id = "IN_ECOURTS_CASES", .name = "India eCourts — case status by party",
    .name_ja = "インド 裁判所 事件検索", .category = "legal",
    .portal = "https://services.ecourts.gov.in", .record_type = "in-court-case",
    .tags = "\"in\",\"courts\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://services.ecourts.gov.in/ecourtindia_v6/?p=casestatus/index&partyname={q}",
    .base = "https://services.ecourts.gov.in", .filter_query = 1,
    .description = "District and subordinate court case status by party name "
      "across Indian states — the litigation layer beneath the High Court "
      "judgments that Indian Kanoon indexes" },

  { .id = "IN_SEBI_INTERMEDIARIES", .name = "India SEBI — registered intermediaries & orders",
    .name_ja = "インド SEBI 登録業者/命令", .category = "economy",
    .portal = "https://www.sebi.gov.in", .record_type = "in-sebi-record",
    .tags = "\"in\",\"finance\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.sebi.gov.in/sebiweb/home/search.jsp?searchText={q}",
    .base = "https://www.sebi.gov.in", .filter_query = 1,
    .description = "SEBI registered intermediaries plus its enforcement orders, "
      "debarments and settlement records — whether an Indian market participant "
      "is licensed, and whether it has been barred" },

  { .id = "IN_IP_INDIA_PATENTS", .name = "India IP — patent & trademark applications",
    .name_ja = "インド 特許/商標出願", .category = "research",
    .portal = "https://ipindiaservices.gov.in", .record_type = "in-patent",
    .tags = "\"in\",\"patents\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://ipindiaservices.gov.in/PublicSearch/PublicationSearch/Search?q={q}",
    .base = "https://ipindiaservices.gov.in", .filter_query = 1,
    .description = "Indian patent and trademark filings by applicant — the IP "
      "portfolio and, through the applicant address, the operating location of "
      "an Indian technology firm" },

  /* ── Pakistan / Bangladesh / Sri Lanka / Nepal ─────────────────────────── */
  { .id = "PK_SECP_COMPANY", .name = "Pakistan SECP — company register",
    .name_ja = "パキスタン 企業登記(SECP)", .category = "government",
    .portal = "https://www.secp.gov.pk", .record_type = "pk-company",
    .tags = "\"pk\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://eservices.secp.gov.pk/eServices/NameSearch.jsp?companyName={q}",
    .base = "https://eservices.secp.gov.pk", .filter_query = 1,
    .description = "Pakistani company name search — incorporation number, status "
      "and company kind; the register that also carries SECP's investor alerts "
      "about unlicensed operators" },

  { .id = "PK_PPRA_TENDERS", .name = "Pakistan PPRA — public tenders",
    .name_ja = "パキスタン 公共調達(PPRA)", .category = "government",
    .portal = "https://www.ppra.org.pk", .record_type = "pk-tender",
    .tags = "\"pk\",\"procurement\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.ppra.org.pk/search.asp?keyword={q}",
    .base = "https://www.ppra.org.pk", .filter_query = 1,
    .description = "Pakistani federal tender notices and evaluation reports — "
      "the procuring agency, the bidders and the award; PPRA also publishes "
      "blacklisted-contractor notices" },

  { .id = "BD_RJSC_ENTITY", .name = "Bangladesh RJSC — company & society register",
    .name_ja = "バングラデシュ 企業登記(RJSC)", .category = "government",
    .portal = "https://www.roc.gov.bd", .record_type = "bd-company",
    .tags = "\"bd\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://app.roc.gov.bd:7781/psp/NameSearch?entityName={q}",
    .base = "https://app.roc.gov.bd:7781", .filter_query = 1,
    .description = "Bangladeshi Registrar of Joint Stock Companies search — "
      "registration number, entity type and status for companies, partnerships "
      "and societies, including the garment exporters behind supply chains" },

  { .id = "LK_COMPANY_REGISTRY", .name = "Sri Lanka — Registrar of Companies",
    .name_ja = "スリランカ 企業登記", .category = "government",
    .portal = "https://eroc.drc.gov.lk", .record_type = "lk-company",
    .tags = "\"lk\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://eroc.drc.gov.lk/eROC/companySearch?name={q}",
    .base = "https://eroc.drc.gov.lk", .filter_query = 1,
    .description = "Sri Lankan company search — registration number, type and "
      "status; the register behind port, tea and apparel sector counterparties" },

  { .id = "NP_OCR_COMPANY", .name = "Nepal — Office of Company Registrar",
    .name_ja = "ネパール 企業登記", .category = "government",
    .portal = "https://www.ocr.gov.np", .record_type = "np-company",
    .tags = "\"np\",\"registry\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.ocr.gov.np/index.php/en/search?q={q}",
    .base = "https://www.ocr.gov.np", .filter_query = 1,
    .description = "Nepali company registrar search — registration number, "
      "capital and status for private and public companies" },

  /* ── Singapore / Malaysia ──────────────────────────────────────────────── */
  { .id = "SG_MAS_FI_DIRECTORY", .name = "Singapore MAS — financial institutions directory",
    .name_ja = "シンガポール MAS 金融機関名簿", .category = "economy",
    .portal = "https://eservices.mas.gov.sg", .record_type = "sg-licensee",
    .tags = "\"sg\",\"finance\",\"licences\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://eservices.mas.gov.sg/fid/institution?name={q}",
    .base = "https://eservices.mas.gov.sg", .filter_query = 1,
    .description = "Every institution licensed by the Monetary Authority of "
      "Singapore, plus its Investor Alert List of unlicensed entities — the "
      "licence check for a Singapore-domiciled fund, broker or payment firm" },

  { .id = "SG_SGX_ANNOUNCEMENTS", .name = "SGX — listed company announcements",
    .name_ja = "シンガポール取引所 開示", .category = "economy",
    .portal = "https://www.sgx.com", .record_type = "sg-filing",
    .tags = "\"sg\",\"filings\"", .free_tier = 1,
    .url = "https://api.sgx.com/announcements/v1.0/searchbytitle?title={q}&pagesize=100&pagestart=0",
    .array_path = "data", .title_keys = "title,issuer_name", .id_keys = "id",
    .date_keys = "broadcast_date_time", .page_param = "pagestart", .page_size = 100,
    .description = "Announcements by SGX-listed issuers — results, substantial "
      "shareholder changes, related-party transactions and queries from the "
      "exchange; the disclosure stream for Singapore-listed groups" },

  { .id = "MY_EPEROLEHAN_TENDERS", .name = "Malaysia — federal tender notices",
    .name_ja = "マレーシア 公共調達", .category = "government",
    .portal = "https://www.eperolehan.gov.my", .record_type = "my-tender",
    .tags = "\"my\",\"procurement\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.eperolehan.gov.my/en/search?q={q}",
    .base = "https://www.eperolehan.gov.my", .filter_query = 1,
    .description = "Malaysian federal procurement notices and awards — ministry, "
      "tender reference, value and the successful bidder where published" },

  { .id = "MY_BURSA_ANNOUNCEMENTS", .name = "Bursa Malaysia — listed company announcements",
    .name_ja = "マレーシア取引所 開示", .category = "economy",
    .portal = "https://www.bursamalaysia.com", .record_type = "my-filing",
    .tags = "\"my\",\"filings\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.bursamalaysia.com/market_information/announcements/company_announcement?keyword={q}",
    .base = "https://www.bursamalaysia.com", .filter_query = 1,
    .description = "Bursa Malaysia disclosures — director dealings, material "
      "contracts, related-party transactions and the PN17 financial-distress "
      "classifications that flag a failing issuer" },

  /* ── Indonesia / Thailand / Vietnam / Philippines ──────────────────────── */
  { .id = "ID_LPSE_TENDERS", .name = "Indonesia LPSE/LKPP — procurement",
    .name_ja = "インドネシア 公共調達(LPSE)", .category = "government",
    .portal = "https://lpse.lkpp.go.id", .record_type = "id-tender",
    .tags = "\"id\",\"procurement\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://inaproc.id/cari?q={q}",
    .base = "https://inaproc.id", .filter_query = 1,
    .description = "Indonesian public procurement notices and winners across the "
      "LPSE network, plus LKPP's blacklist of barred providers — the sanction "
      "that matters most to an Indonesian contractor" },

  { .id = "ID_OJK_LICENSEES", .name = "Indonesia OJK — licensed financial institutions",
    .name_ja = "インドネシア OJK 免許金融機関", .category = "economy",
    .portal = "https://www.ojk.go.id", .record_type = "id-licensee",
    .tags = "\"id\",\"finance\",\"licences\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.ojk.go.id/id/Search.aspx?k={q}",
    .base = "https://www.ojk.go.id", .filter_query = 1,
    .description = "Indonesian financial-services authority registers and its "
      "illegal-investment alert list — the difference between a licensed "
      "multifinance company and a scheme operating without one" },

  { .id = "TH_SEC_LICENSEES", .name = "Thailand SEC — licensed intermediaries & enforcement",
    .name_ja = "タイ SEC 免許業者/処分", .category = "economy",
    .portal = "https://www.sec.or.th", .record_type = "th-licensee",
    .tags = "\"th\",\"finance\",\"enforcement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.sec.or.th/EN/Pages/Search.aspx?k={q}",
    .base = "https://www.sec.or.th", .filter_query = 1,
    .description = "Thai SEC licensee registers, investor alerts and enforcement "
      "actions — settlements, criminal complaints and the named individuals "
      "behind them" },

  { .id = "TH_EGP_TENDERS", .name = "Thailand e-GP — government procurement",
    .name_ja = "タイ 政府調達(e-GP)", .category = "government",
    .portal = "https://process3.gprocurement.go.th", .record_type = "th-tender",
    .tags = "\"th\",\"procurement\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://process3.gprocurement.go.th/egp2procmainWeb/jsp/procsearch.sch?keyword={q}",
    .base = "https://process3.gprocurement.go.th", .filter_query = 1,
    .description = "Thai government e-procurement announcements and awards — "
      "agency, method, budget and winning supplier; Thailand publishes the "
      "award price, which makes over-pricing visible" },

  { .id = "VN_TAX_CODE_LOOKUP", .name = "Vietnam — tax code (MST) lookup",
    .name_ja = "ベトナム 税コード照会", .category = "economy",
    .portal = "https://tracuunnt.gdt.gov.vn", .record_type = "vn-taxpayer",
    .tags = "\"vn\",\"tax\",\"status\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://masothue.com/Search/?q={q}&type=auto",
    .base = "https://masothue.com", .filter_query = 1,
    .description = "Vietnamese tax code lookup — registered name, address, legal "
      "representative and operating status. In Vietnam the tax code, not the "
      "business licence, is the identifier that appears on contracts" },

  { .id = "VN_MUASAMCONG_TENDERS", .name = "Vietnam — national procurement network",
    .name_ja = "ベトナム 公共調達", .category = "government",
    .portal = "https://muasamcong.mpi.gov.vn", .record_type = "vn-tender",
    .tags = "\"vn\",\"procurement\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://muasamcong.mpi.gov.vn/web/guest/contractor-selection?keyword={q}",
    .base = "https://muasamcong.mpi.gov.vn", .filter_query = 1,
    .description = "Vietnamese contractor-selection notices and results — the "
      "investor, the package value and the selected contractor, plus the list of "
      "contractors barred from bidding" },

  { .id = "PH_PHILGEPS_TENDERS", .name = "Philippines PhilGEPS — procurement",
    .name_ja = "フィリピン 公共調達(PhilGEPS)", .category = "government",
    .portal = "https://www.philgeps.gov.ph", .record_type = "ph-tender",
    .tags = "\"ph\",\"procurement\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://notices.philgeps.gov.ph/GEPSNONPILOT/Tender/SplashBidNoticeAbstractUI.aspx?keyword={q}",
    .base = "https://notices.philgeps.gov.ph", .filter_query = 1,
    .description = "Philippine bid notices and award abstracts — procuring "
      "entity, approved budget and winning bidder, plus the blacklisting orders "
      "the GPPB publishes against suppliers" },

  { .id = "KH_LA_MM_REGISTRY", .name = "Cambodia/Laos/Myanmar — business registers",
    .name_ja = "カンボジア/ラオス/ミャンマー 企業登記", .category = "government",
    .portal = "https://www.businessregistration.moc.gov.kh",
    .record_type = "mekong-company", .tags = "\"kh\",\"la\",\"mm\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.businessregistration.moc.gov.kh/cambodia-master/viewInstance/view.html?keyword={q}",
    .base = "https://www.businessregistration.moc.gov.kh", .filter_query = 1,
    .description = "Cambodian business registration search — the least-covered "
      "corner of ASEAN, and the domicile of choice for regional gaming, timber "
      "and casino structures" },
};

HP_REGISTER_TABLE(HP2_ASIA_SS)
