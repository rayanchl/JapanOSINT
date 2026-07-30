/* collectors/sources/hp_uk_deep.c — UK deep-record collectors (hp engine).
 *
 * The repo already searched Companies House by name (reg_uk_companies.c). A
 * name hit is where a UK investigation starts, not where it ends: the officers,
 * the persons with significant control behind those officers, the charges the
 * company has granted, its filing history and any insolvency case are separate
 * endpoints of the same free API — and they are what actually answers "who is
 * behind this". Same idea for the Gazette (insolvency/appointment notices),
 * the Charity Commission (trustees + financial history), Parliament (members'
 * registered financial interests) and the Electoral Commission (donations).
 *
 * Companies House rows authenticate with HTTP Basic where the API key is the
 * username and the password is empty — {keyb64} builds exactly that header.
 * Without COMPANIES_HOUSE_API_KEY every CH row returns an honest empty. */
#include "../../lib/hpengine.h"

#define CH_AUTH { "Authorization: Basic {keyb64}", NULL }
#define CH_KEY  "COMPANIES_HOUSE_API_KEY"
#define CH_BASE "https://api.company-information.service.gov.uk"
#define CH_WEB  "https://find-and-update.company-information.service.gov.uk"

static const hp_source HP_UK[] = {
  /* ── Companies House: the record behind the search hit ─────────────────── */
  { .id = "UK_CH_COMPANY_PROFILE", .name = "UK Companies House — company profile",
    .name_ja = "英国 会社登記 — 会社プロファイル", .category = "government",
    .portal = CH_WEB, .record_type = "uk-company", .tags = "\"uk\",\"registry\"",
    .want = HP_NUMERIC, .url = CH_BASE "/company/{qn}", .headers = CH_AUTH,
    .key_env = CH_KEY, .free_tier = 1,
    .title_keys = "company_name", .id_keys = "company_number",
    .date_keys = "date_of_creation", .link_tmpl = CH_WEB "/company/{qn}",
    .description = "Full Companies House record for a company number: status, "
      "type, SIC codes, incorporation/cessation dates, accounts and confirmation-"
      "statement due dates, registered office, previous names" },

  { .id = "UK_CH_OFFICERS", .name = "UK Companies House — officers",
    .name_ja = "英国 会社登記 — 役員一覧", .category = "government",
    .portal = CH_WEB, .record_type = "uk-officer", .tags = "\"uk\",\"officers\"",
    .want = HP_NUMERIC, .url = CH_BASE "/company/{qn}/officers?items_per_page=50",
    .headers = CH_AUTH, .key_env = CH_KEY, .free_tier = 1, .array_path = "items",
    .title_keys = "name", .id_keys = "links.officer.appointments",
    .date_keys = "appointed_on",
    .link_tmpl = CH_WEB "/company/{qn}/officers",
    .page_param = "start_index", .page_size = 50,
    .description = "Every appointed and resigned officer of a UK company — name, "
      "role, nationality, occupation, month/year of birth, country of residence, "
      "service address, appointment and resignation dates" },

  { .id = "UK_CH_PSC", .name = "UK Companies House — persons with significant control",
    .name_ja = "英国 会社登記 — 重要支配者(PSC)", .category = "government",
    .portal = CH_WEB, .record_type = "uk-psc", .tags = "\"uk\",\"beneficial-ownership\"",
    .want = HP_NUMERIC,
    .url = CH_BASE "/company/{qn}/persons-with-significant-control?items_per_page=50",
    .headers = CH_AUTH, .key_env = CH_KEY, .free_tier = 1, .array_path = "items",
    .title_keys = "name", .date_keys = "notified_on",
    .link_tmpl = CH_WEB "/company/{qn}/persons-with-significant-control",
    .page_param = "start_index", .page_size = 50,
    .description = "UK beneficial ownership: the declared PSCs of a company with "
      "their natures of control (ownership/voting bands), nationality, DOB "
      "month-year, address and any corporate/legal-person PSC entries" },

  { .id = "UK_CH_FILING_HISTORY", .name = "UK Companies House — filing history",
    .name_ja = "英国 会社登記 — 提出書類履歴", .category = "government",
    .portal = CH_WEB, .record_type = "uk-filing", .tags = "\"uk\",\"filings\"",
    .want = HP_NUMERIC, .url = CH_BASE "/company/{qn}/filing-history?items_per_page=50",
    .headers = CH_AUTH, .key_env = CH_KEY, .free_tier = 1, .array_path = "items",
    .title_keys = "description,type", .id_keys = "transaction_id",
    .date_keys = "date",
    .link_tmpl = CH_WEB "/company/{qn}/filing-history",
    .page_param = "start_index", .page_size = 50,
    .description = "Every document a UK company has filed — accounts, confirmation "
      "statements, officer appointments/terminations, mortgages, capital changes — "
      "with dates, categories and document metadata" },

  { .id = "UK_CH_CHARGES", .name = "UK Companies House — charges & mortgages",
    .name_ja = "英国 会社登記 — 担保設定", .category = "government",
    .portal = CH_WEB, .record_type = "uk-charge", .tags = "\"uk\",\"security-interest\"",
    .want = HP_NUMERIC, .url = CH_BASE "/company/{qn}/charges?items_per_page=50",
    .headers = CH_AUTH, .key_env = CH_KEY, .free_tier = 1, .array_path = "items",
    .title_keys = "classification.description,particulars.description",
    .id_keys = "charge_number", .date_keys = "created_on",
    .link_tmpl = CH_WEB "/company/{qn}/charges",
    .page_param = "start_index", .page_size = 50,
    .description = "Registered charges over a UK company's assets — who lent, what "
      "was secured, when created/satisfied. The lender list is often the clearest "
      "map of a company's real financial counterparties" },

  { .id = "UK_CH_INSOLVENCY", .name = "UK Companies House — insolvency cases",
    .name_ja = "英国 会社登記 — 倒産手続", .category = "government",
    .portal = CH_WEB, .record_type = "uk-insolvency", .tags = "\"uk\",\"insolvency\"",
    .want = HP_NUMERIC, .url = CH_BASE "/company/{qn}/insolvency",
    .headers = CH_AUTH, .key_env = CH_KEY, .free_tier = 1, .array_path = "cases",
    .title_keys = "type,number", .id_keys = "number",
    .link_tmpl = CH_WEB "/company/{qn}/insolvency",
    .description = "Insolvency cases against a UK company: case type (CVL, "
      "administration, receivership…), practitioners appointed, their firms and "
      "addresses, and the case dates" },

  { .id = "UK_CH_ESTABLISHMENTS", .name = "UK Companies House — UK establishments",
    .name_ja = "英国 会社登記 — 英国内拠点", .category = "government",
    .portal = CH_WEB, .record_type = "uk-establishment", .tags = "\"uk\",\"branches\"",
    .want = HP_NUMERIC, .url = CH_BASE "/company/{qn}/uk-establishments",
    .headers = CH_AUTH, .key_env = CH_KEY, .free_tier = 1, .array_path = "items",
    .title_keys = "company_name", .id_keys = "company_number",
    .page_param = "start_index", .page_size = 50,
    .description = "UK branches/establishments registered by an overseas company — "
      "the local footprint of a foreign parent, each with its own company number" },

  { .id = "UK_CH_OFFICER_APPOINTMENTS", .name = "UK Companies House — officer appointments",
    .name_ja = "英国 会社登記 — 役員の兼任一覧", .category = "government",
    .portal = CH_WEB, .record_type = "uk-appointment", .tags = "\"uk\",\"officers\"",
    .want = HP_ANY, .url = CH_BASE "/officers/{qn}/appointments?items_per_page=50",
    .headers = CH_AUTH, .key_env = CH_KEY, .free_tier = 1, .array_path = "items",
    .title_keys = "appointed_to.company_name,name", .date_keys = "appointed_on",
    .page_param = "start_index", .page_size = 50,
    .description = "Every company a given Companies House officer id is appointed "
      "to — the directorship network of one person, which is the pivot that turns "
      "a single company into a group" },

  { .id = "UK_CH_OFFICER_SEARCH", .name = "UK Companies House — officer search",
    .name_ja = "英国 会社登記 — 役員検索", .category = "government",
    .portal = CH_WEB, .record_type = "uk-officer", .tags = "\"uk\",\"officers\"",
    .url = CH_BASE "/search/officers?q={q}&items_per_page=40",
    .headers = CH_AUTH, .key_env = CH_KEY, .free_tier = 1, .array_path = "items",
    .title_keys = "title,description", .date_keys = "date_of_birth.year",
    .page_param = "start_index", .page_size = 40,
    .description = "Search UK company officers by person name — returns each "
      "matching officer with their appointment count, DOB month/year and address "
      "snippet, plus the officer id needed for the appointments pivot" },

  { .id = "UK_CH_DISQUALIFIED", .name = "UK Companies House — disqualified directors",
    .name_ja = "英国 会社登記 — 資格剥奪役員", .category = "government",
    .portal = CH_WEB, .record_type = "uk-disqualified", .tags = "\"uk\",\"enforcement\"",
    .url = CH_BASE "/search/disqualified-officers?q={q}&items_per_page=40",
    .headers = CH_AUTH, .key_env = CH_KEY, .free_tier = 1, .array_path = "items",
    .title_keys = "title,description",
    .page_param = "start_index", .page_size = 40,
    .description = "The UK disqualified-directors register — people banned from "
      "acting as a company director, with the disqualification period and the "
      "conduct that triggered it" },

  { .id = "UK_CH_ADVANCED_SEARCH", .name = "UK Companies House — advanced company search",
    .name_ja = "英国 会社登記 — 詳細検索", .category = "government",
    .portal = CH_WEB, .record_type = "uk-company", .tags = "\"uk\",\"registry\"",
    .url = CH_BASE "/advanced-search/companies?company_name_includes={q}&size=40",
    .headers = CH_AUTH, .key_env = CH_KEY, .free_tier = 1, .array_path = "items",
    .title_keys = "company_name", .id_keys = "company_number",
    .date_keys = "date_of_creation",
    .description = "Advanced Companies House search — name-contains matching with "
      "SIC code, status, type and incorporation-date facets returned per hit" },

  { .id = "UK_CH_DISSOLVED", .name = "UK Companies House — dissolved companies",
    .name_ja = "英国 会社登記 — 解散会社検索", .category = "government",
    .portal = CH_WEB, .record_type = "uk-company", .tags = "\"uk\",\"dissolved\"",
    .url = CH_BASE "/dissolved-search/companies?q={q}&items_per_page=40",
    .headers = CH_AUTH, .key_env = CH_KEY, .free_tier = 1, .array_path = "items",
    .title_keys = "company_name", .id_keys = "company_number",
    .date_keys = "date_of_cessation",
    .page_param = "start_index", .page_size = 40,
    .description = "Dissolved UK companies — the history that name search hides. "
      "Previous names, incorporation and cessation dates, last registered office" },

  /* ── The Gazette: statutory notices ────────────────────────────────────── */
  { .id = "UK_GAZETTE_INSOLVENCY", .name = "The Gazette — insolvency notices",
    .name_ja = "英国官報 — 倒産公告", .category = "government",
    .portal = "https://www.thegazette.co.uk", .record_type = "uk-gazette-notice",
    .tags = "\"uk\",\"insolvency\",\"gazette\"", .free_tier = 1,
    .url = "https://www.thegazette.co.uk/insolvency/notice/data.json?text={q}&results-page-size=40",
    .array_path = "entry", .title_keys = "title", .date_keys = "updated",
    .description = "Statutory UK insolvency notices (winding-up petitions, "
      "administrations, creditor meetings, liquidator appointments) matching an "
      "entity — the legally required publication, searchable as data" },

  { .id = "UK_GAZETTE_ALL", .name = "The Gazette — all statutory notices",
    .name_ja = "英国官報 — 全公告検索", .category = "government",
    .portal = "https://www.thegazette.co.uk", .record_type = "uk-gazette-notice",
    .tags = "\"uk\",\"gazette\"", .free_tier = 1,
    .url = "https://www.thegazette.co.uk/all-notices/notice/data.json?text={q}&results-page-size=40",
    .array_path = "entry", .title_keys = "title", .date_keys = "updated",
    .description = "All London/Edinburgh/Belfast Gazette notices mentioning an "
      "entity — company, personal-insolvency, deceased-estate, planning, honours "
      "and state notices in one query" },

  /* ── Charity Commission (England & Wales) ──────────────────────────────── */
  { .id = "UK_CHARITY_DETAILS", .name = "UK Charity Commission — charity record",
    .name_ja = "英国慈善団体委員会 — 団体記録", .category = "government",
    .portal = "https://register-of-charities.charitycommission.gov.uk",
    .record_type = "uk-charity", .tags = "\"uk\",\"charity\"",
    .want = HP_NUMERIC, .key_env = "UK_CHARITY_KEY", .free_tier = 1,
    .url = "https://api.charitycommission.gov.uk/register/api/allcharitydetailsV2/{qd}/0",
    .headers = { "Ocp-Apim-Subscription-Key: {key}", NULL },
    .title_keys = "charity_name", .id_keys = "reg_charity_number",
    .date_keys = "date_of_registration",
    .description = "Full registered-charity record: objects, activities, area of "
      "operation, governing document, contact and registration/removal history" },

  { .id = "UK_CHARITY_TRUSTEES", .name = "UK Charity Commission — trustees",
    .name_ja = "英国慈善団体委員会 — 理事", .category = "government",
    .portal = "https://register-of-charities.charitycommission.gov.uk",
    .record_type = "uk-charity-trustee", .tags = "\"uk\",\"charity\",\"officers\"",
    .want = HP_NUMERIC, .key_env = "UK_CHARITY_KEY", .free_tier = 1,
    .url = "https://api.charitycommission.gov.uk/register/api/charitytrustee/{qd}/0",
    .headers = { "Ocp-Apim-Subscription-Key: {key}", NULL },
    .title_keys = "trustee_name", .id_keys = "trustee_id",
    .page_param = "page", .page_start = 1,
    .description = "The trustee board of a registered charity — names and trustee "
      "ids, which link the same individual across every charity they sit on" },

  { .id = "UK_CHARITY_FINANCIALS", .name = "UK Charity Commission — financial history",
    .name_ja = "英国慈善団体委員会 — 財務履歴", .category = "economy",
    .portal = "https://register-of-charities.charitycommission.gov.uk",
    .record_type = "uk-charity-financials", .tags = "\"uk\",\"charity\",\"finance\"",
    .want = HP_NUMERIC, .key_env = "UK_CHARITY_KEY", .free_tier = 1,
    .url = "https://api.charitycommission.gov.uk/register/api/charityfinancialhistory/{qd}/0",
    .headers = { "Ocp-Apim-Subscription-Key: {key}", NULL },
    .title_keys = "financial_period_end_date", .date_keys = "financial_period_end_date",
    .description = "Year-by-year income and expenditure of a registered charity — "
      "the series that shows when money started or stopped moving" },

  /* ── Parliament, elections, land, health ───────────────────────────────── */
  { .id = "UK_MP_INTERESTS", .name = "UK Parliament — members' registered interests",
    .name_ja = "英国議会 — 議員と利害関係登録", .category = "government",
    .portal = "https://members.parliament.uk", .record_type = "uk-mp",
    .tags = "\"uk\",\"pep\",\"parliament\"", .free_tier = 1,
    .url = "https://members-api.parliament.uk/api/Members/Search?Name={q}&take=20",
    .array_path = "items", .title_keys = "nameDisplayAs,nameFullTitle",
    .id_keys = "id",
    .detail_url = "https://members-api.parliament.uk/api/Members/{v}/RegisteredInterests",
    .detail_key = "id",
    .description = "UK MPs and Lords by name, then a second hop into each member's "
      "Register of Financial Interests — donations, directorships, shareholdings "
      "and overseas visits declared by that member" },

  { .id = "UK_HANSARD_SEARCH", .name = "UK Hansard — parliamentary record search",
    .name_ja = "英国議事録 — 発言検索", .category = "government",
    .portal = "https://hansard.parliament.uk", .record_type = "uk-hansard",
    .tags = "\"uk\",\"parliament\"", .free_tier = 1,
    .url = "https://hansard-api.parliament.uk/search.json?queryParameters.searchTerm={q}&queryParameters.take=25",
    .title_keys = "Title,MemberName", .date_keys = "SittingDate",
    .description = "Every mention of an entity in the UK parliamentary record — "
      "who said it, in which debate, on which sitting day" },

  { .id = "UK_CH_REGISTERS", .name = "UK Companies House — company registers",
    .name_ja = "英国 会社登記 — 登記簿の保管状況", .category = "government",
    .portal = CH_WEB, .record_type = "uk-register", .tags = "\"uk\",\"registry\"",
    .want = HP_NUMERIC, .url = CH_BASE "/company/{qn}/registers",
    .headers = CH_AUTH, .key_env = CH_KEY, .free_tier = 1,
    .title_keys = "company_number", .id_keys = "company_number",
    .link_tmpl = CH_WEB "/company/{qn}",
    .description = "Where a UK company keeps its statutory registers (directors, "
      "secretaries, PSCs, members) and whether it has elected to keep them at "
      "Companies House — which decides whether the member list is public at all" },

  { .id = "UK_LAND_REGISTRY_PPD", .name = "HM Land Registry — price paid records",
    .name_ja = "英国土地登記 — 取引価格記録", .category = "economy",
    .portal = "https://landregistry.data.gov.uk", .record_type = "uk-property-sale",
    .tags = "\"uk\",\"property\"", .free_tier = 1,
    .url = "https://landregistry.data.gov.uk/data/ppi/transaction-record.json"
           "?_pageSize=40&propertyAddress.postcode={qU}",
    .title_keys = "propertyAddress.paon,propertyAddress.street",
    .date_keys = "transactionDate",
    .description = "Every recorded sale price at a UK postcode — address, price, "
      "date, property type, new-build and freehold/leasehold flags" },

  { .id = "UK_NHS_ODS_ORG", .name = "NHS Organisation Data Service — organisations",
    .name_ja = "NHS 組織データ — 機関検索", .category = "health",
    .portal = "https://directory.spineservices.nhs.uk", .record_type = "uk-nhs-org",
    .tags = "\"uk\",\"health\"", .free_tier = 1,
    .url = "https://directory.spineservices.nhs.uk/ORD/2-0-0/organisations?Name={q}&Limit=40",
    .array_path = "Organisations", .title_keys = "Name", .id_keys = "OrgId",
    .detail_url = "https://directory.spineservices.nhs.uk/ORD/2-0-0/organisations/{v}",
    .detail_key = "OrgId",
    .description = "NHS ODS organisation codes — trusts, GP practices, pharmacies "
      "and suppliers — with a second hop into the full record: roles, addresses, "
      "operational dates and successor relationships" },
};

HP_REGISTER_TABLE(HP_UK)
