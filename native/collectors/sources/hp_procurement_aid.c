/* collectors/sources/hp_procurement_aid.c — public money: contracts, grants, aid.
 *
 * Procurement and aid data is where an organisation's relationships with states
 * are written down in full: the buyer, the amount, the dates, the beneficiary
 * and usually the losing bidders too. These rows go to the award-level records
 * rather than tender headlines, and cover the multilateral side (World Bank,
 * IATI, OCHA FTS, Global Fund, USAID) that no existing collector touched. */
#include "../../lib/hpengine.h"

static const hp_source HP_PROCUREMENT[] = {
  { .id = "UK_CONTRACTS_FINDER", .name = "UK Contracts Finder — OCDS notices",
    .name_ja = "英国 公共調達(Contracts Finder)", .category = "government",
    .portal = "https://www.contractsfinder.service.gov.uk", .record_type = "uk-contract",
    .tags = "\"uk\",\"procurement\",\"ocds\"", .free_tier = 1,
    .url = "https://www.contractsfinder.service.gov.uk/Published/Notices/OCDS/Search"
           "?keyword={q}&stages=award&size=40",
    .array_path = "results", .title_keys = "tender.title,awards.0.suppliers.0.name",
    .id_keys = "ocid", .date_keys = "date", .max_items = 40,
    .description = "UK central and local government contract awards in OCDS form — "
      "buyer, supplier, value, contract period and CPV classification for every "
      "notice naming an entity" },

  { .id = "USASPENDING_SUBAWARDS", .name = "USAspending — federal subawards",
    .name_ja = "米国連邦支出 — 下請け契約", .category = "government",
    .portal = "https://www.usaspending.gov", .record_type = "us-federal-subaward",
    .tags = "\"us\",\"procurement\",\"subcontracts\"", .free_tier = 1,
    .url = "https://api.usaspending.gov/api/v2/search/spending_by_award/",
    .post_body = "{\"filters\":{\"keywords\":[\"{Q}\"],\"award_type_codes\":"
                 "[\"A\",\"B\",\"C\",\"D\"]},\"subawards\":true,\"fields\":"
                 "[\"Sub-Award ID\",\"Sub-Awardee Name\",\"Sub-Award Amount\","
                 "\"Sub-Award Date\",\"Awarding Agency\",\"Prime Recipient Name\"],"
                 "\"page\":1,\"limit\":40,\"sort\":\"Sub-Award Amount\",\"order\":\"desc\"}",
    .array_path = "results", .title_keys = "Sub-Awardee Name,Prime Recipient Name",
    .id_keys = "Sub-Award ID", .date_keys = "Sub-Award Date", .max_items = 40,
    .description = "The second tier of US federal contracting — who the prime "
      "contractor actually passes the work and money to. Subaward records expose "
      "supply-chain relationships that never appear in the prime award data" },

  { .id = "WORLDBANK_PROC_NOTICES", .name = "World Bank — procurement notices & awards",
    .name_ja = "世界銀行 — 調達公告/落札", .category = "government",
    .portal = "https://projects.worldbank.org", .record_type = "wb-procurement",
    .tags = "\"worldbank\",\"procurement\"", .free_tier = 1,
    .url = "https://search.worldbank.org/api/v2/procnotices?format=json&qterm={q}&rows=40",
    .array_path = "procnotices", .title_keys = "bid_description,project_name",
    .id_keys = "id", .date_keys = "submission_date", .max_items = 40,
    .description = "World Bank-financed procurement notices and contract awards — "
      "borrower country, project, notice type, bid description and the awarded "
      "supplier where published" },

  { .id = "WORLDBANK_DOCUMENTS", .name = "World Bank — documents & reports",
    .name_ja = "世界銀行 — 文書/報告書", .category = "government",
    .portal = "https://documents.worldbank.org", .record_type = "wb-document",
    .tags = "\"worldbank\",\"documents\"", .free_tier = 1,
    .url = "https://search.worldbank.org/api/v2/wds?format=json&qterm={q}&rows=40",
    .array_path = "documents", .title_keys = "docna,display_title", .id_keys = "id",
    .date_keys = "docdt", .max_items = 40,
    .description = "The World Bank documents archive — appraisal documents, audits, "
      "investigation reports and implementation reviews naming an entity, with direct "
      "PDF/TXT links" },

  { .id = "IATI_ACTIVITIES", .name = "IATI — development aid activities",
    .name_ja = "IATI — 開発援助活動", .category = "government",
    .portal = "https://iatistandard.org", .record_type = "aid-activity",
    .tags = "\"aid\",\"iati\"", .key_env = "IATI_API_KEY", .free_tier = 1,
    .url = "https://api.iatistandard.org/datastore/activity/select?q={q}&rows=40&wt=json",
    .headers = { "Ocp-Apim-Subscription-Key: {key}", NULL },
    .array_path = "response.docs", .title_keys = "title_narrative,iati_identifier",
    .id_keys = "iati_identifier", .date_keys = "activity_date_iso_date", .max_items = 40,
    .description = "Aid activities published to the IATI standard by donors, NGOs and "
      "implementing partners — budgets, transactions, participating organisations and "
      "recipient countries for anything naming the entity" },

  { .id = "OCHA_FTS_ORGS", .name = "OCHA FTS — humanitarian funding organisations",
    .name_ja = "OCHA FTS — 人道支援資金の組織", .category = "government",
    .portal = "https://fts.unocha.org", .record_type = "aid-organisation",
    .tags = "\"aid\",\"un\",\"funding\"", .free_tier = 1,
    .url = "https://api.hpc.tools/v1/public/organization",
    .array_path = "data", .filter_query = 1, .title_keys = "name", .id_keys = "id",
    .max_items = 20,
    .description = "Organisations recognised in the UN humanitarian financial tracking "
      "system, filtered to the query — canonical name, abbreviation, type and country, "
      "which is the id needed to pull that organisation's funding flows" },

  { .id = "SAM_OPPORTUNITIES", .name = "SAM.gov — federal contract opportunities",
    .name_ja = "SAM.gov — 連邦調達機会", .category = "government",
    .portal = "https://sam.gov", .record_type = "us-opportunity",
    .tags = "\"us\",\"procurement\"", .key_env = "SAM_API_KEY", .free_tier = 1,
    .url = "https://api.sam.gov/prod/opportunities/v2/search?api_key={key}"
           "&title={q}&limit=40&postedFrom=01/01/2020&postedTo=12/31/2026",
    .array_path = "opportunitiesData", .title_keys = "title", .id_keys = "noticeId",
    .date_keys = "postedDate", .max_items = 40,
    .description = "US federal solicitations and awards matching a term — issuing "
      "office, NAICS/PSC classification, set-aside type, response deadline and the "
      "awardee once published" },

  { .id = "USAID_ASSISTANCE", .name = "USAID — foreign assistance transactions",
    .name_ja = "USAID — 対外援助支出", .category = "government",
    .portal = "https://foreignassistance.gov", .record_type = "aid-transaction",
    .tags = "\"us\",\"aid\"", .free_tier = 1,
    .url = "https://data.usaid.gov/resource/azij-hu6e.json?$q={q}&$limit=40",
    .title_keys = "activity_name,implementing_partner_name",
    .id_keys = "activity_id", .date_keys = "fiscal_year", .max_items = 40,
    .description = "US foreign-assistance transactions — implementing partner, "
      "country, sector, funding agency and obligated/disbursed amounts by fiscal "
      "year; how a contractor's aid income is actually structured" },

  { .id = "GLOBALFUND_GRANTS", .name = "The Global Fund — grants & disbursements",
    .name_ja = "世界基金 — 助成/支出", .category = "government",
    .portal = "https://data.theglobalfund.org", .record_type = "aid-grant",
    .tags = "\"aid\",\"health\",\"grants\"", .free_tier = 1,
    .url = "https://data-service.theglobalfund.org/v3.3/odata/VGrantAgreements?"
           "%24filter=contains%28principalRecipientName%2C%27{Q}%27%29&%24top=40",
    .array_path = "value", .title_keys = "grantAgreementNumber,principalRecipientName",
    .id_keys = "grantAgreementNumber", .date_keys = "grantAgreementStartDate",
    .max_items = 40,
    .description = "Global Fund grant agreements by principal recipient — country, "
      "component (HIV/TB/malaria), signed amount, disbursements and grant period. "
      "Recipients are often national agencies and large NGOs" },

  { .id = "CANADA_OPEN_CONTRACTS", .name = "Canada open data — contracts & registries",
    .name_ja = "カナダ 政府データ 契約検索", .category = "government",
    .portal = "https://open.canada.ca", .record_type = "ca-dataset",
    .tags = "\"ca\",\"procurement\",\"opendata\"", .free_tier = 1,
    .url = "https://open.canada.ca/data/api/3/action/package_search?q={q}&rows=25",
    .array_path = "result.results", .title_keys = "title", .id_keys = "name",
    .date_keys = "metadata_modified", .max_items = 25,
    .description = "Canadian federal datasets matching an entity or programme — "
      "proactive-disclosure contract lists, grants and contributions, lobbying "
      "registrations and departmental registries, with resource download URLs" },

  { .id = "UNGM_NOTICES", .name = "UNGM — UN procurement notices",
    .name_ja = "UNGM — 国連調達公告", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.ungm.org", .record_type = "un-tender",
    .tags = "\"un\",\"procurement\"", .free_tier = 1,
    .url = "https://www.ungm.org/Public/Notice?keywords={q}",
    .href_must = "/Public/Notice/", .base = "https://www.ungm.org", .max_items = 25,
    .description = "United Nations Global Marketplace tender notices and awards "
      "matching a term — the procurement surface for UNDP, UNICEF, WFP, WHO and the "
      "agencies that publish only here" },

  { .id = "EU_KOHESIO_BENEFICIARIES", .name = "Kohesio — EU cohesion fund beneficiaries",
    .name_ja = "EU 結束基金 受益者検索", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://kohesio.ec.europa.eu", .record_type = "eu-beneficiary",
    .tags = "\"eu\",\"funding\",\"beneficiaries\"", .free_tier = 1,
    .url = "https://kohesio.ec.europa.eu/en/search?keywords={q}",
    .base = "https://kohesio.ec.europa.eu", .filter_query = 1, .max_items = 25,
    .description = "Beneficiaries of EU cohesion and regional-development funding — "
      "the project, the money received and the region, for entities that received "
      "structural funds" },
};

HP_REGISTER_TABLE(HP_PROCUREMENT)
