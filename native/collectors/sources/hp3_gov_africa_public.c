/* collectors/sources/hp3_gov_africa_public.c — Sub-Saharan African public record.
 *
 * The received wisdom is that African corporate and government records are not
 * online. That is out of date and, for the records that matter most in a
 * due-diligence context, wrong: South Africa runs a full OCDS procurement API,
 * Kenya, Nigeria, Ghana and Rwanda all publish company registry search, SAFLII
 * and AfricanLII carry decades of judgments across a dozen jurisdictions, and
 * the extractive-industry transparency regimes publish contract-level payment
 * data for exactly the sectors where the risk concentrates.
 *
 * What is genuinely scarce is *indexing*. These portals are rarely crawled and
 * almost never queried programmatically, which is precisely why wiring them is
 * worth more here than another European register. */
#include "../../lib/hpengine.h"

static const hp_source HP3_GOV_AFRICA[] = {
  /* ── South Africa ─────────────────────────────────────────────────────── */
  { .id = "ZA_ETENDERS_OCDS", .name = "South Africa eTenders — OCDS procurement releases",
    .name_ja = "南アフリカ 政府調達OCDS", .category = "government",
    .portal = "https://www.etenders.gov.za", .record_type = "za-tender",
    .tags = "\"za\",\"procurement\",\"ocds\"", .free_tier = 1,
    .url = "https://ocds-api.etenders.gov.za/api/OCDSReleases?PageSize=100",
    .array_path = "releases", .filter_query = 1,
    .title_keys = "tender.title,buyer.name", .id_keys = "ocid", .date_keys = "date",
    .page_param = "PageNumber", .page_size = 100, .page_max = 40,
    .description = "South African national and provincial tenders in Open "
      "Contracting format — the buying department, the tender description, the "
      "briefing session, the closing date and the award where published" },

  { .id = "ZA_CIPC_ENTERPRISE", .name = "South Africa CIPC — company & close corporation search",
    .name_ja = "南アフリカ 企業登記", .category = "government",
    .portal = "https://eservices.cipc.co.za", .record_type = "za-entity",
    .tags = "\"za\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://eservices.cipc.co.za/EnterpriseSearch/Search?enterpriseName={q}",
    .base = "https://eservices.cipc.co.za", .filter_query = 1,
    .description = "The Companies and Intellectual Property Commission's "
      "enterprise search — registration number, entity type, status, and the "
      "directors and members recorded against a South African company" },

  { .id = "ZA_GOVERNMENT_GAZETTE", .name = "South Africa — government gazette notices",
    .name_ja = "南アフリカ 官報", .category = "government",
    .portal = "https://www.gov.za", .record_type = "za-gazette-notice",
    .tags = "\"za\",\"gazette\",\"legal\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.gov.za/documents/government-gazette?search={q}",
    .base = "https://www.gov.za", .filter_query = 1,
    .description = "National and provincial gazettes — liquidations, estate "
      "notices, licence grants, expropriations and the regulatory "
      "determinations that only ever appear in gazette form" },

  { .id = "ZA_MUNICIPAL_MONEY", .name = "South Africa Municipal Money — municipal finances",
    .name_ja = "南アフリカ 自治体財政", .category = "government",
    .portal = "https://municipalmoney.gov.za", .record_type = "za-municipal-finance",
    .tags = "\"za\",\"public-finance\",\"local-government\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://municipalmoney.gov.za/search?q={q}",
    .base = "https://municipalmoney.gov.za", .filter_query = 1,
    .description = "National Treasury's municipal financial data — audit "
      "outcomes, irregular and unauthorised expenditure, cash coverage and "
      "creditor days for every South African municipality. Municipal financial "
      "distress is the leading indicator for service-delivery contracts" },

  { .id = "ZA_PMG_PARLIAMENT", .name = "South Africa PMG — parliamentary committee record",
    .name_ja = "南アフリカ議会 委員会記録", .category = "government",
    .portal = "https://pmg.org.za", .record_type = "za-parliamentary-record",
    .tags = "\"za\",\"parliament\",\"oversight\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://pmg.org.za/search/?q={q}",
    .base = "https://pmg.org.za", .filter_query = 1,
    .description = "The Parliamentary Monitoring Group's minutes of every "
      "committee meeting, questions to ministers, and the tabled reports — the "
      "most complete record of South African state-owned enterprise oversight "
      "that exists in one place" },

  { .id = "AFRICA_SAFLII_JUDGMENTS", .name = "SAFLII — Southern African legal information",
    .name_ja = "南部アフリカ 判例データベース", .category = "legal",
    .portal = "https://www.saflii.org", .record_type = "africa-judgment",
    .tags = "\"za\",\"africa\",\"courts\",\"judgment\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.saflii.org/cgi-bin/sinosrch.cgi?query={q}&method=auto",
    .base = "https://www.saflii.org", .filter_query = 1,
    .description = "Judgments from South Africa, Botswana, Lesotho, Malawi, "
      "Mauritius, Namibia, Eswatini, Tanzania, Zambia and Zimbabwe — parties, "
      "citation and the full text of the decision" },

  { .id = "AFRICA_AFRICANLII", .name = "AfricanLII — pan-African law & judgments",
    .name_ja = "アフリカ 法令判例データベース", .category = "legal",
    .portal = "https://africanlii.org", .record_type = "africa-judgment",
    .tags = "\"africa\",\"courts\",\"law\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://africanlii.org/search/?q={q}",
    .base = "https://africanlii.org", .filter_query = 1,
    .description = "Legislation, gazettes and judgments across the African "
      "Union member states and the regional courts — the African Court on "
      "Human and Peoples' Rights, the ECOWAS Court and the EAC Court" },

  { .id = "AFRICA_OPEN_AFRICA_CKAN", .name = "open.africa — continental dataset catalog",
    .name_ja = "アフリカ オープンデータ目録", .category = "government",
    .portal = "https://open.africa", .record_type = "africa-dataset",
    .tags = "\"africa\",\"open-data\",\"catalog\"", .free_tier = 1,
    .url = "https://open.africa/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,organization.title",
    .id_keys = "id", .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Code for Africa's continental CKAN — budget, health "
      "facility, extractives and election datasets scraped or negotiated out of "
      "governments that publish nothing machine-readable themselves" },

  /* ── West Africa ──────────────────────────────────────────────────────── */
  { .id = "NG_CAC_COMPANY_SEARCH", .name = "Nigeria CAC — company registration search",
    .name_ja = "ナイジェリア 企業登記", .category = "government",
    .portal = "https://search.cac.gov.ng", .record_type = "ng-entity",
    .tags = "\"ng\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://search.cac.gov.ng/home?searchTerm={q}",
    .base = "https://search.cac.gov.ng", .filter_query = 1,
    .description = "Nigeria's Corporate Affairs Commission public search — "
      "RC number, company status, registered address and the business "
      "classification. Nigeria has also begun publishing persons with "
      "significant control against these records" },

  { .id = "NG_BPP_PROCUREMENT", .name = "Nigeria — Bureau of Public Procurement records",
    .name_ja = "ナイジェリア 公共調達庁", .category = "government",
    .portal = "https://www.bpp.gov.ng", .record_type = "ng-procurement",
    .tags = "\"ng\",\"procurement\",\"debarment\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.bpp.gov.ng/?s={q}",
    .base = "https://www.bpp.gov.ng", .filter_query = 1,
    .description = "Certificates of no objection, contract award approvals and "
      "the debarment list of contractors barred from Nigerian federal "
      "procurement, with the ground and the duration" },

  { .id = "NG_NEITI_EXTRACTIVES", .name = "Nigeria NEITI — extractive industry audits",
    .name_ja = "ナイジェリア 採掘産業透明性", .category = "government",
    .portal = "https://neiti.gov.ng", .record_type = "ng-extractive-payment",
    .tags = "\"ng\",\"extractives\",\"transparency\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://neiti.gov.ng/?s={q}",
    .base = "https://neiti.gov.ng", .filter_query = 1,
    .description = "Nigeria Extractive Industries Transparency Initiative audit "
      "reports — company-by-company payments to government, production volumes "
      "by field or lease, and the reconciliation discrepancies that name the "
      "operator responsible" },

  { .id = "GH_RGD_BUSINESS_SEARCH", .name = "Ghana — Registrar General business search",
    .name_ja = "ガーナ 企業登記", .category = "government",
    .portal = "https://rgd.gov.gh", .record_type = "gh-entity",
    .tags = "\"gh\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://rgd.gov.gh/?s={q}",
    .base = "https://rgd.gov.gh", .filter_query = 1,
    .description = "Ghana's Registrar General's Department — company "
      "registration and status, plus the beneficial-ownership regime Ghana "
      "introduced ahead of most of the region" },

  { .id = "GH_PPA_PROCUREMENT", .name = "Ghana — Public Procurement Authority notices",
    .name_ja = "ガーナ 公共調達庁", .category = "government",
    .portal = "https://ppa.gov.gh", .record_type = "gh-procurement",
    .tags = "\"gh\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ppa.gov.gh/?s={q}",
    .base = "https://ppa.gov.gh", .filter_query = 1,
    .description = "Ghanaian tender notices, contract award publications and "
      "the suppliers' register — including the sole-sourcing approvals the "
      "Authority must publish with reasons" },

  { .id = "SN_MARCHES_PUBLICS", .name = "Senegal — public procurement portal",
    .name_ja = "セネガル 公共調達", .category = "government",
    .portal = "https://www.marchespublics.sn", .record_type = "sn-procurement",
    .tags = "\"sn\",\"procurement\",\"francophone\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.marchespublics.sn/index.php?option=com_search&searchword={q}",
    .base = "https://www.marchespublics.sn", .filter_query = 1,
    .description = "Senegal's ARMP procurement portal — tender notices, award "
      "decisions and the dispute rulings of the regulator, which name the "
      "excluded bidders and the reason" },

  { .id = "CI_MARCHES_PUBLICS", .name = "Côte d'Ivoire — public procurement notices",
    .name_ja = "コートジボワール 公共調達", .category = "government",
    .portal = "https://www.marchespublics.ci", .record_type = "ci-procurement",
    .tags = "\"ci\",\"procurement\",\"francophone\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.marchespublics.ci/recherche?q={q}",
    .base = "https://www.marchespublics.ci", .filter_query = 1,
    .description = "Ivorian tender notices and award publications, the "
      "francophone West African procurement model where the regulator "
      "publishes both the award and the losing bids' evaluation" },

  /* ── East Africa ──────────────────────────────────────────────────────── */
  { .id = "KE_PPRA_DEBARMENT", .name = "Kenya PPRA — debarred suppliers & tender portal",
    .name_ja = "ケニア 調達規制庁", .category = "government",
    .portal = "https://ppra.go.ke", .record_type = "ke-debarment",
    .tags = "\"ke\",\"procurement\",\"debarment\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://ppra.go.ke/?s={q}",
    .base = "https://ppra.go.ke", .filter_query = 1,
    .description = "Kenya's Public Procurement Regulatory Authority — the "
      "debarment register naming suppliers barred from public contracts, the "
      "Review Board decisions and the tender portal notices" },

  { .id = "KE_BRS_BUSINESS_SEARCH", .name = "Kenya — business registration service search",
    .name_ja = "ケニア 企業登記", .category = "government",
    .portal = "https://brs.go.ke", .record_type = "ke-entity",
    .tags = "\"ke\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://brs.go.ke/?s={q}",
    .base = "https://brs.go.ke", .filter_query = 1,
    .description = "Kenya's Business Registration Service — company and "
      "business-name registration, directors, and the beneficial-ownership "
      "filings the Companies Act now requires" },

  { .id = "KE_OPEN_DATA", .name = "Kenya Open Data — national dataset portal",
    .name_ja = "ケニア オープンデータ", .category = "government",
    .portal = "https://www.opendata.go.ke", .record_type = "ke-dataset",
    .tags = "\"ke\",\"open-data\",\"catalog\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.opendata.go.ke/search?q={q}",
    .base = "https://www.opendata.go.ke", .filter_query = 1,
    .description = "Kenyan county and national datasets — budget allocations, "
      "health facility registers, school lists and the census tables used to "
      "verify claimed project locations" },

  { .id = "TZ_BRELA_ENTITY", .name = "Tanzania BRELA — business registration",
    .name_ja = "タンザニア 企業登記", .category = "government",
    .portal = "https://ors.brela.go.tz", .record_type = "tz-entity",
    .tags = "\"tz\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ors.brela.go.tz/orsAdmin/searchcompany?name={q}",
    .base = "https://ors.brela.go.tz", .filter_query = 1,
    .description = "Tanzania's Business Registrations and Licensing Agency — "
      "company registration number, status, directors and the industrial "
      "licence attached to a manufacturing entity" },

  { .id = "UG_URSB_REGISTRY", .name = "Uganda URSB — registration services bureau",
    .name_ja = "ウガンダ 企業登記", .category = "government",
    .portal = "https://ursb.go.ug", .record_type = "ug-entity",
    .tags = "\"ug\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ursb.go.ug/?s={q}",
    .base = "https://ursb.go.ug", .filter_query = 1,
    .description = "Uganda Registration Services Bureau — company and business "
      "name registration, insolvency notices and the intellectual property "
      "register, all administered from one body" },

  { .id = "RW_RDB_REGISTRY", .name = "Rwanda RDB — company registration",
    .name_ja = "ルワンダ 企業登記", .category = "government",
    .portal = "https://org.rdb.rw", .record_type = "rw-entity",
    .tags = "\"rw\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://org.rdb.rw/business/search?q={q}",
    .base = "https://org.rdb.rw", .filter_query = 1,
    .description = "Rwanda Development Board's company register — TIN, "
      "registration date, shareholders and directors. Rwanda is one of the few "
      "African registries that publishes shareholding percentages openly" },

  { .id = "ET_ETHIOPIA_TRADE_REGISTRY", .name = "Ethiopia — trade registration & licensing",
    .name_ja = "エチオピア 商業登記", .category = "government",
    .portal = "https://etrade.gov.et", .record_type = "et-entity",
    .tags = "\"et\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://etrade.gov.et/business-license-checker?businessName={q}",
    .base = "https://etrade.gov.et", .filter_query = 1,
    .description = "Ethiopia's trade registration and business licence checker "
      "— the licence number, the permitted business activity, the capital "
      "registered and the licence renewal status" },

  /* ── Southern Africa & regional bodies ────────────────────────────────── */
  { .id = "ZM_PACRA_REGISTRY", .name = "Zambia PACRA — patents & companies registry",
    .name_ja = "ザンビア 企業登記", .category = "government",
    .portal = "https://www.pacra.org.zm", .record_type = "zm-entity",
    .tags = "\"zm\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.pacra.org.zm/#/search?q={q}",
    .base = "https://www.pacra.org.zm", .filter_query = 1,
    .description = "Zambia's Patents and Companies Registration Agency — "
      "company number, directors and shareholders, which matters "
      "disproportionately because Zambian mining assets are held through "
      "chains of locally registered vehicles" },

  { .id = "BW_CIPA_REGISTRY", .name = "Botswana CIPA — companies & IP authority",
    .name_ja = "ボツワナ 企業登記", .category = "government",
    .portal = "https://www.cipa.co.bw", .record_type = "bw-entity",
    .tags = "\"bw\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.cipa.co.bw/?s={q}",
    .base = "https://www.cipa.co.bw", .filter_query = 1,
    .description = "Botswana's Companies and Intellectual Property Authority — "
      "registration status, directors and the annual return record, in a "
      "jurisdiction used as a regional holding domicile" },

  { .id = "NA_BIPA_REGISTRY", .name = "Namibia BIPA — business & IP authority register",
    .name_ja = "ナミビア 企業登記", .category = "government",
    .portal = "https://www.bipa.na", .record_type = "na-entity",
    .tags = "\"na\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.bipa.na/?s={q}",
    .base = "https://www.bipa.na", .filter_query = 1,
    .description = "Namibia's Business and Intellectual Property Authority — "
      "close corporations and companies with their registration number, "
      "status, directors and members, in a jurisdiction that hosts fishing "
      "quota and mining licence holders for foreign parents" },

  { .id = "AFRICA_RESOURCE_PROJECTS", .name = "Resource Projects — extractive payments by project",
    .name_ja = "資源事業 支払開示", .category = "government",
    .portal = "https://resourceprojects.org", .record_type = "extractive-payment",
    .tags = "\"africa\",\"extractives\",\"transparency\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://resourceprojects.org/search?q={q}",
    .base = "https://resourceprojects.org", .filter_query = 1,
    .description = "Payments to governments disclosed under the EU Accounting "
      "Directive, UK, Canadian and Norwegian rules, broken down by project and "
      "payment type — taxes, royalties, bonuses and infrastructure "
      "improvements. Company-to-state money at asset level" },
};

HP_REGISTER_TABLE(HP3_GOV_AFRICA)
