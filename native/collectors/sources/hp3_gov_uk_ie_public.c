/* collectors/sources/hp3_gov_uk_ie_public.c — UK & Ireland public-sector depth.
 *
 * `hp_uk_deep.c` goes down the corporate chain — Companies House officers,
 * PSCs, charges, insolvency. This file goes down the *state* chain instead:
 * what the public sector bought, from whom, who lobbied whom to get it, which
 * regulator inspected the result, and what the courts said afterwards. Both
 * jurisdictions publish these as proper APIs — the UK Parliament's member,
 * bill and question APIs, the Contracts Finder OCDS feed, the FSA hygiene
 * ratings API and, in Ireland, the Oireachtas API — so almost none of this
 * needs scraping. */
#include "../../lib/hpengine.h"

static const hp_source HP3_GOV_UK_IE[] = {
  /* ── Public money ─────────────────────────────────────────────────────── */
  { .id = "UK_PUBLIC_CONTRACTS_SCOTLAND", .name = "Public Contracts Scotland — devolved procurement",
    .name_ja = "スコットランド 公共調達", .category = "government",
    .portal = "https://www.publiccontractsscotland.gov.uk",
    .record_type = "uk-contract-notice",
    .tags = "\"uk\",\"scotland\",\"procurement\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.publiccontractsscotland.gov.uk/search/Search_MainPage.aspx"
      "?searchTerm={q}",
    .base = "https://www.publiccontractsscotland.gov.uk", .filter_query = 1,
    .description = "Scottish public bodies procure separately from Whitehall "
      "and publish here — councils, NHS boards, universities and the Scottish "
      "Government, with the notice, the award and the supplier. None of it "
      "appears in the UK-wide Contracts Finder" },

  { .id = "UK_FIND_A_TENDER", .name = "UK Find a Tender — high-value procurement",
    .name_ja = "英国 高額調達公告", .category = "government",
    .portal = "https://www.find-tender.service.gov.uk",
    .record_type = "uk-tender-notice",
    .tags = "\"uk\",\"procurement\",\"ocds\"", .free_tier = 1,
    .url = "https://www.find-tender.service.gov.uk/api/1.0/ocdsReleasePackages"
      "?limit=100&stages=award",
    .array_path = "releases", .filter_query = 1,
    .title_keys = "tender.title,buyer.name", .id_keys = "ocid", .date_keys = "date",
    .next_path = "links.next", .page_max = 40,
    .description = "The post-Brexit successor to OJEU for above-threshold UK "
      "procurement — full OCDS releases including the award, the supplier "
      "identifier, the value and every amendment to the contract" },

  { .id = "UK_GOVUK_SEARCH_API", .name = "GOV.UK — whole-estate content search",
    .name_ja = "英国政府 全文検索API", .category = "government",
    .portal = "https://www.gov.uk", .record_type = "uk-gov-publication",
    .tags = "\"uk\",\"government\",\"publication\"", .free_tier = 1,
    .url = "https://www.gov.uk/api/search.json?q={q}&count=100"
      "&fields=title,link,description,public_timestamp,organisations,"
      "content_store_document_type,format",
    .array_path = "results", .title_keys = "title", .id_keys = "link",
    .date_keys = "public_timestamp", .link_keys = "link",
    .link_tmpl = "https://www.gov.uk{v}",
    .page_param = "start", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "The search API behind GOV.UK, covering every department's "
      "consultations, transparency releases, spend-over-£25k publications, "
      "guidance and statistics. The explicit field list asks for the whole "
      "record rather than accepting the default headline-only response" },

  { .id = "UK_LEGISLATION_FEED", .name = "legislation.gov.uk — statute & SI feed",
    .name_ja = "英国法令データベース", .category = "government",
    .portal = "https://www.legislation.gov.uk", .record_type = "uk-legislation",
    .tags = "\"uk\",\"law\"", .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.legislation.gov.uk/all?title={q}",
    .base = "https://www.legislation.gov.uk", .filter_query = 1,
    .description = "Acts, statutory instruments and devolved legislation by "
      "title — including the sanctions, export-control and licensing SIs that "
      "change what a named company or sector may do" },

  { .id = "UK_CASELAW_ARCHIVE", .name = "Find Case Law — National Archives judgments",
    .name_ja = "英国 判例アーカイブ", .category = "legal",
    .portal = "https://caselaw.nationalarchives.gov.uk", .record_type = "uk-judgment",
    .tags = "\"uk\",\"courts\",\"judgment\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://caselaw.nationalarchives.gov.uk/search?query={q}&per_page=50",
    .base = "https://caselaw.nationalarchives.gov.uk", .filter_query = 1,
    .description = "The official archive of judgments from the Supreme Court, "
      "Court of Appeal, High Court and the Upper Tribunals — party names, "
      "neutral citation and the full text of the decision" },

  /* ── Parliament ───────────────────────────────────────────────────────── */
  { .id = "UK_COMMONS_DIVISIONS", .name = "UK Parliament — Commons division (vote) records",
    .name_ja = "英国下院 採決記録", .category = "government",
    .portal = "https://commonsvotes-api.parliament.uk", .record_type = "uk-division",
    .tags = "\"uk\",\"parliament\",\"votes\"", .free_tier = 1,
    .url = "https://commonsvotes-api.parliament.uk/data/divisions.json/search"
      "?queryParameters.searchTerm={q}&queryParameters.take=100",
    .title_keys = "Title,DivisionId", .id_keys = "DivisionId",
    .date_keys = "Date",
    .page_param = "queryParameters.skip", .page_size = 100, .page_start = 0,
    .page_max = 30,
    .detail_url = "https://commonsvotes-api.parliament.uk/data/division/{v}.json",
    .detail_key = "DivisionId",
    .description = "Commons divisions with the detail hop for the full aye and "
      "no lists — every MP named on each side of a vote, plus the tellers. How "
      "a legislator actually voted, not how their party described it" },

  { .id = "UK_PARLIAMENT_WRITTEN_QUESTIONS", .name = "UK Parliament — written questions & answers",
    .name_ja = "英国議会 書面質問", .category = "government",
    .portal = "https://questions-statements.parliament.uk",
    .record_type = "uk-parliamentary-question",
    .tags = "\"uk\",\"parliament\",\"oversight\"", .free_tier = 1,
    .url = "https://questions-statements.parliament.uk/api/writtenquestions/questions"
      "?searchTerm={q}&take=100&expandMember=true",
    .array_path = "results", .title_keys = "value.questionText,value.askingMember.name",
    .id_keys = "value.id", .date_keys = "value.dateTabled",
    .page_param = "skip", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Written parliamentary questions naming a company, contract "
      "or country, and the minister's answer. A question is often the first "
      "public confirmation that a government holds a particular record" },

  { .id = "UK_PARLIAMENT_BILLS_API", .name = "UK Parliament — bills & amendments",
    .name_ja = "英国議会 法案API", .category = "government",
    .portal = "https://bills-api.parliament.uk", .record_type = "uk-bill",
    .tags = "\"uk\",\"parliament\",\"legislation\"", .free_tier = 1,
    .url = "https://bills-api.parliament.uk/api/v1/Bills?SearchTerm={q}&take=100",
    .array_path = "items", .title_keys = "shortTitle,currentHouse",
    .id_keys = "billId", .date_keys = "lastUpdate",
    .page_param = "skip", .page_size = 100, .page_start = 0, .page_max = 30,
    .detail_url = "https://bills-api.parliament.uk/api/v1/Bills/{v}",
    .detail_key = "billId",
    .description = "Bills before Parliament with the detail hop for sponsors, "
      "stage history and the promoter of a private bill — which is how "
      "infrastructure and utility companies legislate for themselves" },

  { .id = "UK_PARLIAMENT_COMMITTEES", .name = "UK Parliament — committees & evidence",
    .name_ja = "英国議会 委員会/証拠提出", .category = "government",
    .portal = "https://committees-api.parliament.uk", .record_type = "uk-committee",
    .tags = "\"uk\",\"parliament\",\"oversight\"", .free_tier = 1,
    .url = "https://committees-api.parliament.uk/api/Committees?SearchTerm={q}&take=50",
    .array_path = "items", .title_keys = "name,category",
    .id_keys = "id", .page_param = "skip", .page_size = 50, .page_start = 0,
    .page_max = 30,
    .detail_url = "https://committees-api.parliament.uk/api/Committees/{v}",
    .detail_key = "id",
    .description = "Select committees, their current membership and their "
      "inquiries. Written evidence submitted to an inquiry is a company "
      "stating its own position on the record, under its own name" },

  { .id = "UK_LORDS_REGISTER_INTERESTS", .name = "UK Parliament — registered financial interests",
    .name_ja = "英国議会 利益登録簿", .category = "government",
    .portal = "https://interests-api.parliament.uk", .record_type = "uk-member-interest",
    .tags = "\"uk\",\"parliament\",\"conflict-of-interest\"", .free_tier = 1,
    .url = "https://interests-api.parliament.uk/api/v1/Interests/?Take=100",
    .array_path = "items", .filter_query = 1,
    .title_keys = "value.summary,value.member.nameDisplayAs",
    .id_keys = "value.id", .date_keys = "value.registrationDate",
    .page_param = "Skip", .page_size = 100, .page_start = 0, .page_max = 40,
    .description = "The machine-readable register of members' financial "
      "interests — outside employment, shareholdings, gifts, overseas visits "
      "and who paid for them. The formal record of who is paying a legislator" },

  /* ── Regulators and inspectorates ─────────────────────────────────────── */
  { .id = "UK_FCA_FIRM_REGISTER", .name = "UK FCA — financial services register",
    .name_ja = "英国FCA 金融業者登録", .category = "finance",
    .portal = "https://register.fca.org.uk", .record_type = "uk-authorised-firm",
    .tags = "\"uk\",\"financial\",\"licence\"", .key_env = "FCA_API_KEY",
    .free_tier = 1,
    .url = "https://register.fca.org.uk/services/V0.1/Search?q={q}&type=firm",
    .headers = { "X-Auth-Email: {key}", "X-Auth-Key: {key}", NULL },
    .array_path = "Data", .title_keys = "Name,Type of business or Individual",
    .id_keys = "Reference Number",
    .description = "Firms and individuals authorised by the Financial Conduct "
      "Authority — reference number, permissions held, status and the "
      "appointed-representative relationships that let an unauthorised firm "
      "trade under someone else's licence" },

  { .id = "UK_FSA_HYGIENE_RATINGS", .name = "UK Food Standards Agency — hygiene ratings",
    .name_ja = "英国食品基準庁 衛生評価", .category = "government",
    .portal = "https://ratings.food.gov.uk", .record_type = "uk-inspection",
    .tags = "\"uk\",\"inspection\",\"health\"", .free_tier = 1,
    .url = "https://api.ratings.food.gov.uk/Establishments?name={q}&pageSize=100",
    .headers = { "x-api-version: 2", NULL },
    .array_path = "establishments", .title_keys = "BusinessName,AddressLine1",
    .id_keys = "FHRSID", .date_keys = "RatingDate",
    .lat_key = "geocode.latitude", .lon_key = "geocode.longitude",
    .page_param = "pageNumber", .page_size = 100, .page_max = 30,
    .description = "Every food business inspected in the UK — the rating, the "
      "component scores for hygiene, structure and confidence in management, "
      "the local authority and the exact premises address" },

  { .id = "UK_CQC_PROVIDERS", .name = "UK Care Quality Commission — providers & locations",
    .name_ja = "英国CQC 医療介護事業者", .category = "health",
    .portal = "https://api.service.cqc.org.uk", .record_type = "uk-care-provider",
    .tags = "\"uk\",\"health\",\"inspection\"", .key_env = "CQC_API_KEY",
    .free_tier = 1,
    .url = "https://api.service.cqc.org.uk/public/v1/providers?perPage=1000",
    .headers = { "Ocp-Apim-Subscription-Key: {key}", NULL },
    .array_path = "providers", .filter_query = 1,
    .title_keys = "providerName", .id_keys = "providerId",
    .page_param = "page", .page_size = 1000, .page_max = 30,
    .detail_url = "https://api.service.cqc.org.uk/public/v1/providers/{v}",
    .detail_key = "providerId",
    .description = "Registered health and social care providers in England, "
      "with the detail hop for inspection ratings, enforcement action, "
      "registered manager and every location the provider operates" },

  { .id = "UK_OFSTED_REPORTS", .name = "UK Ofsted — inspection reports",
    .name_ja = "英国Ofsted 学校監査報告", .category = "government",
    .portal = "https://reports.ofsted.gov.uk", .record_type = "uk-inspection",
    .tags = "\"uk\",\"education\",\"inspection\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://reports.ofsted.gov.uk/search?q={q}",
    .base = "https://reports.ofsted.gov.uk", .filter_query = 1,
    .description = "Inspection reports for schools, colleges, nurseries and "
      "children's homes — the URN, the responsible body or trust, the judgement "
      "and the full inspector narrative" },

  { .id = "UK_ICO_FEE_PAYERS", .name = "UK ICO — data protection fee register",
    .name_ja = "英国ICO データ保護登録", .category = "government",
    .portal = "https://ico.org.uk", .record_type = "uk-data-controller",
    .tags = "\"uk\",\"privacy\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ico.org.uk/ESDWebPages/Search?SearchOption=Contains&Name={q}",
    .base = "https://ico.org.uk", .filter_query = 1,
    .description = "Organisations registered as data controllers with the "
      "Information Commissioner — registration number, the security and "
      "trading names, the address and the data-protection officer contact" },

  { .id = "UK_INSOLVENCY_INDIVIDUAL", .name = "UK Insolvency Service — individual insolvency register",
    .name_ja = "英国 個人破産登録簿", .category = "legal",
    .portal = "https://www.insolvencydirect.bis.gov.uk",
    .record_type = "uk-individual-insolvency",
    .tags = "\"uk\",\"insolvency\",\"distress\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.insolvencydirect.bis.gov.uk/eiir/IIRCaseNameSearchResult.asp"
      "?txtSurname={q}",
    .base = "https://www.insolvencydirect.bis.gov.uk", .filter_query = 1,
    .description = "Bankruptcies, debt relief orders and individual voluntary "
      "arrangements against a named person — court, case number, date and the "
      "insolvency practitioner appointed. The personal counterpart to a "
      "company's insolvency record" },

  { .id = "UK_LAND_OVERSEAS_OWNERSHIP", .name = "HM Land Registry — overseas company ownership",
    .name_ja = "英国土地登記 海外法人所有", .category = "government",
    .portal = "https://use-land-property-data.service.gov.uk",
    .record_type = "uk-property-ownership",
    .tags = "\"uk\",\"property\",\"ownership\",\"offshore\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://use-land-property-data.service.gov.uk/datasets/ocod?q={q}",
    .base = "https://use-land-property-data.service.gov.uk", .filter_query = 1,
    .description = "Overseas Companies Ownership Data — freehold and leasehold "
      "titles in England and Wales held by companies incorporated outside the "
      "UK, with the proprietor name, its country of incorporation and the price "
      "paid. The single most direct offshore-to-asset link the UK publishes" },

  { .id = "UK_SCOTLAND_CHARITY_OSCR", .name = "Scotland OSCR — charity register",
    .name_ja = "スコットランド 慈善団体登録", .category = "government",
    .portal = "https://www.oscr.org.uk", .record_type = "uk-charity",
    .tags = "\"uk\",\"scotland\",\"charity\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.oscr.org.uk/about-charities/search-the-register/charity-details"
      "?searchTerm={q}",
    .base = "https://www.oscr.org.uk", .filter_query = 1,
    .description = "Scottish charities, which are outside the Charity "
      "Commission for England and Wales entirely — SC number, trustees, "
      "objects, accounts and any inquiry OSCR has opened" },

  { .id = "UK_NI_CHARITY_COMMISSION", .name = "Northern Ireland — charity register",
    .name_ja = "北アイルランド 慈善団体登録", .category = "government",
    .portal = "https://www.charitycommissionni.org.uk", .record_type = "uk-charity",
    .tags = "\"uk\",\"northern-ireland\",\"charity\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.charitycommissionni.org.uk/charity-search/?q={q}",
    .base = "https://www.charitycommissionni.org.uk", .filter_query = 1,
    .description = "The third UK charity jurisdiction — NIC number, trustees, "
      "income, and the statutory inquiries the Commission has opened, which in "
      "Northern Ireland often touch cross-border funding flows" },

  { .id = "UK_WDTK_FOI_REQUESTS", .name = "WhatDoTheyKnow — FOI request archive",
    .name_ja = "英国 情報公開請求アーカイブ", .category = "government",
    .portal = "https://www.whatdotheyknow.com", .record_type = "uk-foi-request",
    .tags = "\"uk\",\"foi\",\"transparency\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.whatdotheyknow.com/search/{q}/requests",
    .base = "https://www.whatdotheyknow.com", .filter_query = 1,
    .description = "Every Freedom of Information request made through mySociety "
      "and the authority's answer, in full. Where a government record is not "
      "published, this is often where a copy of it already exists" },

  { .id = "UK_ELECTORAL_COMMISSION_ENTITIES", .name = "UK Electoral Commission — regulated entities",
    .name_ja = "英国選挙委員会 規制対象団体", .category = "government",
    .portal = "https://search.electoralcommission.org.uk",
    .record_type = "uk-political-entity",
    .tags = "\"uk\",\"elections\",\"political-finance\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://search.electoralcommission.org.uk/English/Registrations?"
      "searchText={q}",
    .base = "https://search.electoralcommission.org.uk", .filter_query = 1,
    .description = "Registered political parties, third-party campaigners and "
      "permitted participants — the registered officers, the accounting units "
      "and the entity's registration status" },

  /* ── Ireland ──────────────────────────────────────────────────────────── */
  { .id = "IE_OIREACHTAS_MEMBERS", .name = "Oireachtas — members of the Irish parliament",
    .name_ja = "アイルランド議会 議員API", .category = "government",
    .portal = "https://api.oireachtas.ie", .record_type = "ie-parliamentarian",
    .tags = "\"ie\",\"parliament\",\"politics\"", .free_tier = 1,
    .url = "https://api.oireachtas.ie/v1/members?limit=500",
    .array_path = "results", .filter_query = 1,
    .title_keys = "member.fullName,member.party.showAs",
    .id_keys = "member.pId", .date_keys = "member.dateOfDeath",
    .page_param = "skip", .page_size = 500, .page_start = 0, .page_max = 20,
    .description = "TDs, senators and MEPs with every parliamentary house they "
      "have sat in, party membership over time, constituency and offices held" },

  { .id = "IE_OIREACHTAS_LEGISLATION", .name = "Oireachtas — bills and acts",
    .name_ja = "アイルランド議会 法案/法律", .category = "government",
    .portal = "https://api.oireachtas.ie", .record_type = "ie-legislation",
    .tags = "\"ie\",\"parliament\",\"legislation\"", .free_tier = 1,
    .url = "https://api.oireachtas.ie/v1/legislation?bill_source=Government&limit=500",
    .array_path = "results", .filter_query = 1,
    .title_keys = "bill.shortTitleEn,bill.sponsors",
    .id_keys = "bill.uri", .date_keys = "bill.lastUpdated",
    .page_param = "skip", .page_size = 500, .page_start = 0, .page_max = 20,
    .description = "Irish bills with sponsors, current stage, amendments and "
      "the debates attached to each stage — the whole legislative record as "
      "structured data rather than PDFs" },

  { .id = "IE_LOBBYING_REGISTER", .name = "Ireland — register of lobbying",
    .name_ja = "アイルランド ロビー活動登録", .category = "government",
    .portal = "https://www.lobbying.ie", .record_type = "ie-lobbying-return",
    .tags = "\"ie\",\"lobbying\",\"influence\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.lobbying.ie/app/search?query={q}",
    .base = "https://www.lobbying.ie", .filter_query = 1,
    .description = "Statutory lobbying returns — who lobbied which Designated "
      "Public Official, about what, on whose behalf, and whether the lobbyist "
      "is a former public official inside the cooling-off period" },

  { .id = "IE_CHARITIES_REGULATOR", .name = "Ireland — register of charities",
    .name_ja = "アイルランド 慈善団体登録", .category = "government",
    .portal = "https://www.charitiesregulator.ie", .record_type = "ie-charity",
    .tags = "\"ie\",\"charity\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.charitiesregulator.ie/en/information-for-the-public/"
      "search-the-register-of-charities?searchTerm={q}",
    .base = "https://www.charitiesregulator.ie", .filter_query = 1,
    .description = "Irish registered charities — registered charity number, "
      "the trustees, the charitable purpose, annual reports and the CRO number "
      "where the charity is also an incorporated company" },

  { .id = "IE_GOV_OPEN_DATA", .name = "data.gov.ie — Irish public dataset catalog",
    .name_ja = "アイルランド オープンデータ目録", .category = "government",
    .portal = "https://data.gov.ie", .record_type = "ie-dataset",
    .tags = "\"ie\",\"open-data\",\"catalog\"", .free_tier = 1,
    .url = "https://data.gov.ie/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,organization.title",
    .id_keys = "id", .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Ireland's CKAN catalog — which department or agency holds "
      "a dataset on a subject, the resources behind it and the licence, "
      "including the planning, procurement and health datasets that are not "
      "linked from any department page" },
};

HP_REGISTER_TABLE(HP3_GOV_UK_IE)
