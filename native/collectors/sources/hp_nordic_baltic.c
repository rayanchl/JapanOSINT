/* collectors/sources/hp_nordic_baltic.c — Nordic + Baltic deep records.
 *
 * The Nordics publish the best open company data in the world: Norway and
 * Denmark expose roles, ownership and sub-units as plain JSON with no key at
 * all, and Finland's PRH does the same. Existing coverage stopped at "does
 * this name exist"; these rows take the organisation number and pull the
 * roles, the sub-units, the annual accounts, the register events and the
 * bankruptcy record behind it — which is the entire point of an org number. */
#include "../../lib/hpengine.h"

static const hp_source HP_NORDIC[] = {
  /* ── Norway (Brønnøysund, keyless) ─────────────────────────────────────── */
  { .id = "NO_BRREG_ENHET", .name = "Brønnøysund — Norwegian entity record",
    .name_ja = "ノルウェー 企業登記 — 法人記録", .category = "government",
    .portal = "https://data.brreg.no", .record_type = "no-company",
    .tags = "\"no\",\"registry\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://data.brreg.no/enhetsregisteret/api/enheter/{qd}",
    .title_keys = "navn", .id_keys = "organisasjonsnummer",
    .date_keys = "registreringsdatoEnhetsregisteret",
    .description = "Full Norwegian entity record behind an org number — legal form, "
      "NACE activity, employee count, VAT registration, business and postal "
      "addresses, bankruptcy/liquidation flags and parent organisation" },

  { .id = "NO_BRREG_ROLLER", .name = "Brønnøysund — roles & officers",
    .name_ja = "ノルウェー 企業登記 — 役職者", .category = "government",
    .portal = "https://data.brreg.no", .record_type = "no-officer",
    .tags = "\"no\",\"officers\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://data.brreg.no/enhetsregisteret/api/enheter/{qd}/roller",
    .title_keys = "person.navn.fornavn,enhet.navn.0,type.beskrivelse", .max_items = 50,
    .description = "Every registered role in a Norwegian entity — chair, board "
      "members, CEO, auditor, accountant, contact person — with the natural person "
      "or company holding each role. Keyless, complete, and named" },

  { .id = "NO_BRREG_UNDERENHETER", .name = "Brønnøysund — sub-units",
    .name_ja = "ノルウェー 企業登記 — 事業所", .category = "government",
    .portal = "https://data.brreg.no", .record_type = "no-establishment",
    .tags = "\"no\",\"branches\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://data.brreg.no/enhetsregisteret/api/underenheter?overordnetEnhet={qd}&size=50",
    .array_path = "_embedded.underenheter", .title_keys = "navn",
    .id_keys = "organisasjonsnummer", .max_items = 50,
    .description = "The operating sites of a Norwegian company — each sub-unit with "
      "its own org number, address, activity and employee count; where the parent "
      "record shows one entity, this shows the real footprint" },

  { .id = "NO_BRREG_SEARCH", .name = "Brønnøysund — entity name search",
    .name_ja = "ノルウェー 企業登記 — 名称検索", .category = "government",
    .portal = "https://data.brreg.no", .record_type = "no-company",
    .tags = "\"no\",\"registry\"", .free_tier = 1,
    .url = "https://data.brreg.no/enhetsregisteret/api/enheter?navn={q}&size=40",
    .array_path = "_embedded.enheter", .title_keys = "navn",
    .id_keys = "organisasjonsnummer", .max_items = 40,
    .detail_url = "https://data.brreg.no/enhetsregisteret/api/enheter/{v}/roller",
    .detail_key = "organisasjonsnummer", .detail_max = 3,
    .description = "Norwegian entities by name, each deepened with its role list — "
      "one query goes from a company name to the people on its board" },

  /* ── Denmark ───────────────────────────────────────────────────────────── */
  { .id = "DK_CVRAPI_ENTITY", .name = "CVR — Danish company record",
    .name_ja = "デンマーク 企業登記(CVR)", .category = "government",
    .portal = "https://datacvr.virk.dk", .record_type = "dk-company",
    .tags = "\"dk\",\"registry\"", .free_tier = 1,
    .url = "https://cvrapi.dk/api?search={q}&country=dk",
    .title_keys = "name", .id_keys = "vat", .date_keys = "startdate",
    .description = "Danish CVR record for a company name or CVR number — address, "
      "industry code, employee band, phone/email as registered, protected-address "
      "flag, and the owners/management array Denmark publishes openly" },

  { .id = "DK_VIRK_PARTICIPANTS", .name = "Virk — Danish owners & management",
    .name_ja = "デンマーク 企業登記 — 所有者・経営陣", .category = "government",
    .portal = "https://datacvr.virk.dk", .record_type = "dk-officer",
    .tags = "\"dk\",\"officers\"", .free_tier = 1,
    .url = "https://cvrapi.dk/api?search={q}&country=dk",
    .array_path = "owners", .title_keys = "name", .max_items = 40,
    .description = "The owner and management rows of a Danish company as published "
      "in CVR — real personal names attached to a registered company, which is what "
      "makes Denmark the most penetrable registry in Europe" },

  { .id = "DK_DAWA_ADDRESS", .name = "DAWA — Danish address resolution",
    .name_ja = "デンマーク 住所解決(DAWA)", .category = "geospatial",
    .portal = "https://dawadocs.dataforsyningen.dk", .record_type = "dk-address",
    .tags = "\"dk\",\"address\"", .free_tier = 1,
    .url = "https://api.dataforsyningen.dk/adresser?q={q}&per_side=25",
    .title_keys = "adressebetegnelse", .id_keys = "id",
    .lat_key = "adgangsadresse.adgangspunkt.koordinater.1",
    .lon_key = "adgangsadresse.adgangspunkt.koordinater.0", .max_items = 25,
    .description = "Authoritative Danish address lookup — official address id, "
      "municipality/parish codes and access-point coordinates for an address "
      "string; the key that joins CVR registrations to a physical place" },

  /* ── Sweden ────────────────────────────────────────────────────────────── */
  { .id = "SE_ALLABOLAG_SEARCH", .name = "allabolag — Swedish company search",
    .name_ja = "スウェーデン 企業検索(allabolag)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.allabolag.se", .record_type = "se-company",
    .tags = "\"se\",\"registry\"", .free_tier = 1,
    .url = "https://www.allabolag.se/what/{q}",
    .href_must = "/foretag/", .base = "https://www.allabolag.se",
    .filter_query = 0, .max_items = 25,
    .description = "Swedish companies matching a name, as served by the public "
      "allabolag listing — organisation links only, from real page anchors. "
      "Bolagsverket itself has no open API, so this is the honest substitute" },

  { .id = "SE_BOLAGSVERKET_NOTICES", .name = "Post- och Inrikes Tidningar — Swedish official notices",
    .name_ja = "スウェーデン 官報公告", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://poit.bolagsverket.se", .record_type = "se-notice",
    .tags = "\"se\",\"gazette\"", .free_tier = 1,
    .url = "https://poit.bolagsverket.se/poit-app/sok?fritext={q}",
    .base = "https://poit.bolagsverket.se", .filter_query = 1, .max_items = 25,
    .description = "Swedish statutory gazette notices (bankruptcies, mergers, "
      "creditor calls, company changes) naming an entity — the legally required "
      "publication that Bolagsverket's closed API does not expose" },

  /* ── Finland ───────────────────────────────────────────────────────────── */
  { .id = "FI_PRH_COMPANY", .name = "PRH — Finnish company record",
    .name_ja = "フィンランド 企業登記(PRH)", .category = "government",
    .portal = "https://avoindata.prh.fi", .record_type = "fi-company",
    .tags = "\"fi\",\"registry\"", .free_tier = 1,
    .url = "https://avoindata.prh.fi/opendata-ytj-api/v3/companies?name={q}",
    .array_path = "companies", .title_keys = "names.0.name", .id_keys = "businessId.value",
    .date_keys = "registrationDate", .max_items = 40,
    .description = "Finnish Business Information System records — business id, all "
      "registered and auxiliary names with validity dates, company form, main line "
      "of business, addresses and registered-entry history" },

  { .id = "FI_PRH_BY_BUSINESSID", .name = "PRH — Finnish company by business id",
    .name_ja = "フィンランド 企業登記 — 事業者IDで照会", .category = "government",
    .portal = "https://avoindata.prh.fi", .record_type = "fi-company",
    .tags = "\"fi\",\"registry\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://avoindata.prh.fi/opendata-ytj-api/v3/companies?businessId={qn}",
    .array_path = "companies", .title_keys = "names.0.name", .id_keys = "businessId.value",
    .description = "The same Finnish record keyed by business id (Y-tunnus) — the "
      "identifier that appears on Finnish invoices, tenders and court filings" },

  /* ── Iceland / Estonia / Latvia / Lithuania ────────────────────────────── */
  { .id = "IS_FYRIRTAEKJASKRA", .name = "Iceland — company register search",
    .name_ja = "アイスランド 企業登記", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.skatturinn.is", .record_type = "is-company",
    .tags = "\"is\",\"registry\"", .free_tier = 1,
    .url = "https://www.skatturinn.is/fyrirtaekjaskra/leit/?nafn={q}",
    .base = "https://www.skatturinn.is", .filter_query = 1, .max_items = 20,
    .description = "Icelandic company register hits for a name or kennitala — real "
      "anchors from the tax authority's public search; nothing when the page is "
      "served as JS only" },

  { .id = "EE_ARIREGISTER_JSON", .name = "Estonia e-Business Register — entity data",
    .name_ja = "エストニア 商業登記", .category = "government",
    .portal = "https://ariregister.rik.ee", .record_type = "ee-company",
    .tags = "\"ee\",\"registry\"", .free_tier = 1,
    .url = "https://ariregister.rik.ee/est/api/autocomplete?q={q}&results=40",
    .title_keys = "name,text", .id_keys = "reg_code,code", .max_items = 40,
    .link_tmpl = "https://ariregister.rik.ee/eng/company/{v}", .link_keys = "reg_code",
    .description = "Estonian business-register entities matching a name or registry "
      "code — Estonia's register is fully public, so the code returned here opens "
      "the annual reports and board history for that company" },

  { .id = "LV_UR_REGISTER", .name = "Latvia Uzņēmumu reģistrs — entity search",
    .name_ja = "ラトビア 企業登記", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.ur.gov.lv", .record_type = "lv-company",
    .tags = "\"lv\",\"registry\"", .free_tier = 1,
    .url = "https://info.ur.gov.lv/?#/search?q={q}",
    .base = "https://info.ur.gov.lv", .filter_query = 1, .max_items = 20,
    .description = "Latvian Register of Enterprises hits for a company name or "
      "registration number. The portal is largely client-rendered, so this row is "
      "expected to be thin — it never invents a record to compensate" },

  { .id = "LT_REKVIZITAI_SEARCH", .name = "Lithuania rekvizitai — company directory",
    .name_ja = "リトアニア 企業ディレクトリ", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://rekvizitai.vz.lt", .record_type = "lt-company",
    .tags = "\"lt\",\"registry\"", .free_tier = 1,
    .url = "https://rekvizitai.vz.lt/en/search/?q={q}",
    .href_must = "/en/company/", .base = "https://rekvizitai.vz.lt", .max_items = 25,
    .description = "Lithuanian companies by name — the public directory that mirrors "
      "Registrų centras data (code, VAT number, address, manager) with per-company "
      "pages behind each hit" },

  /* ── Cross-Nordic financial / sanction-adjacent ────────────────────────── */
  { .id = "NO_KONKURS_REGISTER", .name = "Brønnøysund — Norwegian bankruptcy flags",
    .name_ja = "ノルウェー 破産情報", .category = "economy",
    .portal = "https://data.brreg.no", .record_type = "no-insolvency",
    .tags = "\"no\",\"insolvency\"", .free_tier = 1,
    .url = "https://data.brreg.no/enhetsregisteret/api/enheter?navn={q}&konkurs=true&size=40",
    .array_path = "_embedded.enheter", .title_keys = "navn",
    .id_keys = "organisasjonsnummer", .max_items = 40,
    .description = "Norwegian entities matching a name that are flagged bankrupt, "
      "under compulsory liquidation or winding up — a direct solvency check that "
      "needs no credit-bureau subscription" },

  { .id = "DK_CVR_BRANCH_UNITS", .name = "CVR — Danish production units",
    .name_ja = "デンマーク 事業所(P-enhed)", .category = "government",
    .portal = "https://datacvr.virk.dk", .record_type = "dk-establishment",
    .tags = "\"dk\",\"branches\"", .free_tier = 1,
    .url = "https://cvrapi.dk/api?search={q}&country=dk",
    .array_path = "productionunits", .title_keys = "name", .id_keys = "pno",
    .max_items = 40,
    .description = "The production units (P-numbers) under a Danish CVR number — "
      "each physical site with its own identifier, address, industry code and "
      "employee band" },

  { .id = "FI_TENDERS_HILMA", .name = "HILMA — Finnish public procurement notices",
    .name_ja = "フィンランド 公共調達公告(HILMA)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://www.hankintailmoitukset.fi", .record_type = "fi-tender",
    .tags = "\"fi\",\"procurement\"", .free_tier = 1,
    .url = "https://www.hankintailmoitukset.fi/en/public/procurements?search={q}",
    .href_must = "/procurement/", .base = "https://www.hankintailmoitukset.fi",
    .max_items = 25,
    .description = "Finnish public procurement notices naming an entity — the "
      "contracting authority, subject and award links for each notice" },
};

HP_REGISTER_TABLE(HP_NORDIC)
