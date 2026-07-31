/* collectors/sources/hp3_gov_us_states.c — US state and municipal public record.
 *
 * Company law in the United States is state law, so the authoritative record of
 * an American entity is never federal: it is a Secretary of State register in
 * one of fifty jurisdictions, and the operating reality — the licences, the
 * permits, the payroll, the inspections — sits in a city open-data portal
 * underneath it. `hp2_northam_states.c` wired eleven of the large states; this
 * file continues to the rest and then goes down a level, into the municipal
 * record datasets that say what a business at an address is actually doing.
 *
 * The Socrata rows all declare `$offset` paging: those portals hard-cap a
 * single response at 1000 rows, so a row that did not page would silently stop
 * at the cap on any common query. */
#include "../../lib/hpengine.h"

static const hp_source HP3_GOV_US_STATES[] = {
  /* ── Secretary of State registers, continued ──────────────────────────── */
  { .id = "US_PA_BUSINESS_SEARCH", .name = "Pennsylvania — business entity search",
    .name_ja = "米ペンシルベニア州 法人登記", .category = "government",
    .portal = "https://file.dos.pa.gov", .record_type = "us-entity",
    .tags = "\"us\",\"pa\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://file.dos.pa.gov/search/business?searchTerm={q}",
    .base = "https://file.dos.pa.gov", .filter_query = 1,
    .description = "Pennsylvania corporations, LLCs and fictitious names — "
      "entity number, status, registered office and the filing history "
      "including officers named in the annual registration" },

  { .id = "US_MA_CORP_SEARCH", .name = "Massachusetts — corporate database",
    .name_ja = "米マサチューセッツ州 法人登記", .category = "government",
    .portal = "https://corp.sec.state.ma.us", .record_type = "us-entity",
    .tags = "\"us\",\"ma\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://corp.sec.state.ma.us/CorpWeb/CorpSearch/CorpSearchResults.aspx"
      "?SEARCH_TYPE=1&sysvalue={q}",
    .base = "https://corp.sec.state.ma.us", .filter_query = 1,
    .description = "Massachusetts entities — one of the few state registers that "
      "publishes officers and directors with their residential addresses on the "
      "annual report, which makes it a person-to-company pivot as well" },

  { .id = "US_GA_SOS_BUSINESS", .name = "Georgia — corporations division search",
    .name_ja = "米ジョージア州 法人登記", .category = "government",
    .portal = "https://ecorp.sos.ga.gov", .record_type = "us-entity",
    .tags = "\"us\",\"ga\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ecorp.sos.ga.gov/BusinessSearch/BusinessSearchResults"
      "?businessName={q}",
    .base = "https://ecorp.sos.ga.gov", .filter_query = 1,
    .description = "Georgia entities with control number, principal office, "
      "registered agent and the officer block from the annual registration" },

  { .id = "US_NC_SOS_BUSINESS", .name = "North Carolina — business registration",
    .name_ja = "米ノースカロライナ州 法人登記", .category = "government",
    .portal = "https://www.sosnc.gov", .record_type = "us-entity",
    .tags = "\"us\",\"nc\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.sosnc.gov/online_services/search/by_title/_Business_Registration"
      "?searchStr={q}",
    .base = "https://www.sosnc.gov", .filter_query = 1,
    .description = "North Carolina corporations and LLCs — SOSID, status, "
      "registered agent and the scanned filings behind each record" },

  { .id = "US_VA_SCC_BUSINESS", .name = "Virginia SCC — business entity search",
    .name_ja = "米バージニア州 法人登記", .category = "government",
    .portal = "https://cis.scc.virginia.gov", .record_type = "us-entity",
    .tags = "\"us\",\"va\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://cis.scc.virginia.gov/EntitySearch/Index?searchTerm={q}",
    .base = "https://cis.scc.virginia.gov", .filter_query = 1,
    .description = "Virginia's Clerk's Information System — entity ID, status, "
      "registered agent and, for stock corporations, the authorised share "
      "structure that the annual assessment fee is calculated from" },

  { .id = "US_AZ_CORP_COMMISSION", .name = "Arizona Corporation Commission — entity search",
    .name_ja = "米アリゾナ州 法人委員会", .category = "government",
    .portal = "https://ecorp.azcc.gov", .record_type = "us-entity",
    .tags = "\"us\",\"az\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ecorp.azcc.gov/EntitySearch/Index?searchTerm={q}",
    .base = "https://ecorp.azcc.gov", .filter_query = 1,
    .description = "Arizona entities — file number, statutory agent, principal "
      "address and the members or managers disclosed for an LLC, which Arizona "
      "requires and most states do not" },

  { .id = "US_WI_DFI_CORP", .name = "Wisconsin DFI — corporate records",
    .name_ja = "米ウィスコンシン州 法人記録", .category = "government",
    .portal = "https://www.wdfi.org", .record_type = "us-entity",
    .tags = "\"us\",\"wi\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.wdfi.org/apps/CorpSearch/Results.aspx?type=Simple&q={q}",
    .base = "https://www.wdfi.org", .filter_query = 1,
    .description = "Wisconsin corporate records — entity ID, registered agent, "
      "principal office and the full index of filed documents" },

  { .id = "US_MN_SOS_BUSINESS", .name = "Minnesota — business filings search",
    .name_ja = "米ミネソタ州 事業登記", .category = "government",
    .portal = "https://mblsportal.sos.state.mn.us", .record_type = "us-entity",
    .tags = "\"us\",\"mn\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://mblsportal.sos.state.mn.us/Business/Search?searchValue={q}",
    .base = "https://mblsportal.sos.state.mn.us", .filter_query = 1,
    .description = "Minnesota business filings — file number, filing type, "
      "registered office and the assumed names operating under the entity" },

  { .id = "US_TN_SOS_BUSINESS", .name = "Tennessee — business information search",
    .name_ja = "米テネシー州 事業登記", .category = "government",
    .portal = "https://tnbear.tn.gov", .record_type = "us-entity",
    .tags = "\"us\",\"tn\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://tnbear.tn.gov/Ecommerce/FilingSearch.aspx?CN={q}",
    .base = "https://tnbear.tn.gov", .filter_query = 1,
    .description = "Tennessee entities — control number, status, registered "
      "agent and the annual report history that shows when an entity stopped "
      "being maintained" },

  { .id = "US_MO_SOS_BUSINESS", .name = "Missouri — business entity search",
    .name_ja = "米ミズーリ州 事業登記", .category = "government",
    .portal = "https://bsd.sos.mo.gov", .record_type = "us-entity",
    .tags = "\"us\",\"mo\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://bsd.sos.mo.gov/search/business/businesssearch.aspx?searchType"
      "=BusinessName&searchValue={q}",
    .base = "https://bsd.sos.mo.gov", .filter_query = 1,
    .description = "Missouri corporations and LLCs — charter number, registered "
      "agent, good-standing status and the officer list from the annual report" },

  { .id = "US_IN_SOS_BUSINESS", .name = "Indiana — business entity search",
    .name_ja = "米インディアナ州 事業登記", .category = "government",
    .portal = "https://bsd.sos.in.gov", .record_type = "us-entity",
    .tags = "\"us\",\"in\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://bsd.sos.in.gov/publicbusinesssearch?searchType=Business&name={q}",
    .base = "https://bsd.sos.in.gov", .filter_query = 1,
    .description = "Indiana entities with business ID, principal office, "
      "registered agent and the governing persons Indiana requires on the "
      "biennial report" },

  { .id = "US_UT_BUSINESS_SEARCH", .name = "Utah — business entity search",
    .name_ja = "米ユタ州 事業登記", .category = "government",
    .portal = "https://secure.utah.gov", .record_type = "us-entity",
    .tags = "\"us\",\"ut\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://secure.utah.gov/bes/action/details?entity={q}",
    .base = "https://secure.utah.gov", .filter_query = 1,
    .description = "Utah's business entity search — entity number, status, "
      "registered agent and the principals list. Utah is a common domicile for "
      "multi-level marketing and nutraceutical structures" },

  { .id = "US_OK_SOS_BUSINESS", .name = "Oklahoma — business entity filings",
    .name_ja = "米オクラホマ州 事業登記", .category = "government",
    .portal = "https://www.sos.ok.gov", .record_type = "us-entity",
    .tags = "\"us\",\"ok\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.sos.ok.gov/corp/corpInquiryFind.aspx?name={q}",
    .base = "https://www.sos.ok.gov", .filter_query = 1,
    .description = "Oklahoma entity filings — filing number, registered agent "
      "and the document index, including the mergers that move an operating "
      "business between shells" },

  { .id = "US_LA_SOS_BUSINESS", .name = "Louisiana — commercial database",
    .name_ja = "米ルイジアナ州 商業登記", .category = "government",
    .portal = "https://coraweb.sos.la.gov", .record_type = "us-entity",
    .tags = "\"us\",\"la\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://coraweb.sos.la.gov/CommercialSearch/CommercialSearchDetails.aspx"
      "?SearchName={q}",
    .base = "https://coraweb.sos.la.gov", .filter_query = 1,
    .description = "Louisiana's commercial register — charter number, agent, "
      "officers and the amendment history; Louisiana publishes officer names "
      "and addresses more completely than most Gulf states" },

  { .id = "US_MD_BUSINESS_EXPRESS", .name = "Maryland — business entity search",
    .name_ja = "米メリーランド州 事業登記", .category = "government",
    .portal = "https://egov.maryland.gov", .record_type = "us-entity",
    .tags = "\"us\",\"md\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://egov.maryland.gov/BusinessExpress/EntitySearch/Search?query={q}",
    .base = "https://egov.maryland.gov", .filter_query = 1,
    .description = "Maryland entities with department ID, status, resident agent "
      "and the personal property return that discloses business assets by "
      "county — an unusual disclosure among state registers" },

  { .id = "US_OR_BUSINESS_REGISTRY", .name = "Oregon — business registry search",
    .name_ja = "米オレゴン州 事業登記", .category = "government",
    .portal = "https://sos.oregon.gov", .record_type = "us-entity",
    .tags = "\"us\",\"or\",\"registry\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://sos.oregon.gov/business/pages/find.aspx?q={q}",
    .base = "https://sos.oregon.gov", .filter_query = 1,
    .description = "Oregon's registry — registry number, registered agent, "
      "principal place of business and the president and secretary Oregon "
      "requires to be named on every annual report" },

  { .id = "US_AK_CORP_DATABASE", .name = "Alaska — corporations database",
    .name_ja = "米アラスカ州 法人データベース", .category = "government",
    .portal = "https://www.commerce.alaska.gov", .record_type = "us-entity",
    .tags = "\"us\",\"ak\",\"registry\",\"ownership\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.commerce.alaska.gov/cbp/main/search/entities?entityName={q}",
    .base = "https://www.commerce.alaska.gov", .filter_query = 1,
    .description = "Alaska's corporations database, which is the outlier worth "
      "knowing: it publishes the officials AND their percentage ownership for "
      "every entity, making it one of the only US registers with beneficial "
      "ownership on the public face" },

  /* ── Municipal and state record datasets (Socrata, paged) ─────────────── */
  { .id = "US_NYC_DCA_LICENSES", .name = "New York City — legally operating businesses",
    .name_ja = "ニューヨーク市 営業許可事業者", .category = "government",
    .portal = "https://data.cityofnewyork.us", .record_type = "us-city-licence",
    .tags = "\"us\",\"nyc\",\"licence\"", .free_tier = 1,
    .url = "https://data.cityofnewyork.us/resource/w7w3-xahh.json?$q={q}&$limit=1000",
    .title_keys = "business_name,business_name_2", .id_keys = "license_nbr",
    .date_keys = "license_creation_date",
    .lat_key = "latitude", .lon_key = "longitude",
    .page_param = "$offset", .page_size = 1000, .page_start = 0, .page_max = 25,
    .description = "Every business licensed by NYC Consumer and Worker "
      "Protection — licence type and status, the trading and legal names, the "
      "address, and the contact person named on the licence" },

  { .id = "US_NYC_RESTAURANT_INSPECTIONS", .name = "New York City — restaurant inspection results",
    .name_ja = "ニューヨーク市 飲食店検査", .category = "government",
    .portal = "https://data.cityofnewyork.us", .record_type = "us-inspection",
    .tags = "\"us\",\"nyc\",\"inspection\",\"health\"", .free_tier = 1,
    .url = "https://data.cityofnewyork.us/resource/43nn-pn8j.json?$q={q}&$limit=1000",
    .title_keys = "dba,street", .id_keys = "camis", .date_keys = "inspection_date",
    .lat_key = "latitude", .lon_key = "longitude",
    .page_param = "$offset", .page_size = 1000, .page_start = 0, .page_max = 25,
    .description = "NYC food-service inspections — the violation code and "
      "description, score, grade and the CAMIS identifier that joins every "
      "inspection of the same establishment across ownership changes" },

  { .id = "US_NYC_DOB_PERMITS", .name = "New York City — DOB permit issuance",
    .name_ja = "ニューヨーク市 建築許可", .category = "government",
    .portal = "https://data.cityofnewyork.us", .record_type = "us-building-permit",
    .tags = "\"us\",\"nyc\",\"construction\",\"permit\"", .free_tier = 1,
    .url = "https://data.cityofnewyork.us/resource/ipu4-2q9a.json?$q={q}&$limit=1000",
    .title_keys = "owner_s_business_name,job_type", .id_keys = "job__",
    .date_keys = "issuance_date",
    .page_param = "$offset", .page_size = 1000, .page_start = 0, .page_max = 25,
    .description = "Construction permits with the owner's business name, the "
      "permittee firm and its licence number, and the filing representative — "
      "the chain of who is building what, at which block and lot" },

  { .id = "US_NYC_PAYROLL", .name = "New York City — citywide payroll",
    .name_ja = "ニューヨーク市 職員給与", .category = "government",
    .portal = "https://data.cityofnewyork.us", .record_type = "us-public-payroll",
    .tags = "\"us\",\"nyc\",\"payroll\",\"transparency\"", .free_tier = 1,
    .url = "https://data.cityofnewyork.us/resource/k397-673e.json?$q={q}&$limit=1000",
    .title_keys = "last_name,agency_name", .id_keys = "payroll_number",
    .page_param = "$offset", .page_size = 1000, .page_start = 0, .page_max = 25,
    .description = "Named municipal employees with agency, title, base salary "
      "and overtime paid. Public payroll is the cleanest way to place a named "
      "person inside a specific government unit at a specific time" },

  { .id = "US_CHICAGO_BUSINESS_LICENSES", .name = "Chicago — business licences",
    .name_ja = "シカゴ市 営業許可", .category = "government",
    .portal = "https://data.cityofchicago.org", .record_type = "us-city-licence",
    .tags = "\"us\",\"chicago\",\"licence\"", .free_tier = 1,
    .url = "https://data.cityofchicago.org/resource/r5kz-chrr.json?$q={q}&$limit=1000",
    .title_keys = "legal_name,doing_business_as_name",
    .id_keys = "id", .date_keys = "license_start_date",
    .lat_key = "latitude", .lon_key = "longitude",
    .page_param = "$offset", .page_size = 1000, .page_start = 0, .page_max = 25,
    .description = "Chicago business licences — the legal entity behind the "
      "trading name, the licence code, the site address and the conditional "
      "approval flags that mark liquor, cannabis and late-hour operations" },

  { .id = "US_CHICAGO_SALARIES", .name = "Chicago — employee names, positions & salaries",
    .name_ja = "シカゴ市 職員給与", .category = "government",
    .portal = "https://data.cityofchicago.org", .record_type = "us-public-payroll",
    .tags = "\"us\",\"chicago\",\"payroll\",\"transparency\"", .free_tier = 1,
    .url = "https://data.cityofchicago.org/resource/xzkq-xp2w.json?$q={q}&$limit=1000",
    .title_keys = "name,job_titles", .id_keys = "name",
    .page_param = "$offset", .page_size = 1000, .page_start = 0, .page_max = 25,
    .description = "Every named City of Chicago employee — department, job "
      "title, salaried or hourly, and the annual salary or hourly rate" },

  { .id = "US_CHICAGO_FOOD_INSPECTIONS", .name = "Chicago — food establishment inspections",
    .name_ja = "シカゴ市 食品衛生検査", .category = "government",
    .portal = "https://data.cityofchicago.org", .record_type = "us-inspection",
    .tags = "\"us\",\"chicago\",\"inspection\",\"health\"", .free_tier = 1,
    .url = "https://data.cityofchicago.org/resource/4ijn-s7e5.json?$q={q}&$limit=1000",
    .title_keys = "dba_name,aka_name", .id_keys = "inspection_id",
    .date_keys = "inspection_date",
    .lat_key = "latitude", .lon_key = "longitude",
    .page_param = "$offset", .page_size = 1000, .page_start = 0, .page_max = 25,
    .description = "Chicago health inspections with the licence number, the "
      "result, and the full violation narrative — the inspector's own text, "
      "which is far more informative than the pass/fail flag" },

  { .id = "US_SF_REGISTERED_BUSINESSES", .name = "San Francisco — registered business locations",
    .name_ja = "サンフランシスコ市 事業所登録", .category = "government",
    .portal = "https://data.sfgov.org", .record_type = "us-city-registration",
    .tags = "\"us\",\"sf\",\"registry\"", .free_tier = 1,
    .url = "https://data.sfgov.org/resource/g8m3-pdis.json?$q={q}&$limit=1000",
    .title_keys = "dba_name,ownership_name", .id_keys = "ttxid",
    .date_keys = "dba_start_date",
    .page_param = "$offset", .page_size = 1000, .page_start = 0, .page_max = 25,
    .description = "San Francisco's tax registry of business locations — the "
      "ownership name behind the trading name, the NAICS code, the start and "
      "end dates and the mailing address, which is often the real controller" },

  { .id = "US_LA_ACTIVE_BUSINESSES", .name = "Los Angeles — active business tax registrants",
    .name_ja = "ロサンゼルス市 事業税登録", .category = "government",
    .portal = "https://data.lacity.org", .record_type = "us-city-registration",
    .tags = "\"us\",\"la\",\"registry\"", .free_tier = 1,
    .url = "https://data.lacity.org/resource/6rrh-rzua.json?$q={q}&$limit=1000",
    .title_keys = "business_name,dba_name", .id_keys = "location_account",
    .date_keys = "location_start_date",
    .page_param = "$offset", .page_size = 1000, .page_start = 0, .page_max = 25,
    .description = "Every business paying the Los Angeles business tax — legal "
      "name, DBA, mailing address, primary NAICS and the council district; the "
      "closest thing LA has to a municipal company register" },

  { .id = "US_SEATTLE_BUSINESS_LICENSES", .name = "Seattle — business licence tax certificates",
    .name_ja = "シアトル市 事業許可", .category = "government",
    .portal = "https://data.seattle.gov", .record_type = "us-city-licence",
    .tags = "\"us\",\"seattle\",\"licence\"", .free_tier = 1,
    .url = "https://data.seattle.gov/resource/wnbq-64tb.json?$q={q}&$limit=1000",
    .title_keys = "trade_name,ownership_type", .id_keys = "customer_number",
    .page_param = "$offset", .page_size = 1000, .page_start = 0, .page_max = 25,
    .description = "Seattle business licence tax certificates — the legal owner, "
      "the trade name, the NAICS description and the street address of each "
      "licensed location" },
};

HP_REGISTER_TABLE(HP3_GOV_US_STATES)
