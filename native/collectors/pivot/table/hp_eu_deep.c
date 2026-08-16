/* collectors/sources/hp_eu_deep.c — EU + continental-European deep records.
 *
 * The existing European coverage is search-shaped: one query, one list of
 * names. These rows go after the documents that make a European entity legible
 * — the statutory-body list behind a Czech company, the beneficial-owner-grade
 * detail behind a Slovak or Dutch registration, the tender awards behind a
 * supplier name, the asset declarations behind a Ukrainian official, the
 * published decisions behind a Greek public body.
 *
 * Rows whose upstream is JS-only or anti-bot are declared HP_HTML and yield
 * real anchors or nothing at all — never a synthesized "found in registry" row. */
#include "lib/hpengine.h"

static const hp_source HP_EU[] = {
  /* ── France ────────────────────────────────────────────────────────────── */
  { .id = "FR_DECP_MARCHES", .name = "France DECP — public procurement awards",
    .name_ja = "フランス 公共調達 落札データ", .category = "government",
    .portal = "https://data.economie.gouv.fr", .record_type = "fr-contract",
    .tags = "\"fr\",\"procurement\"", .free_tier = 1,
    .url = "https://data.economie.gouv.fr/api/records/1.0/search/"
           "?dataset=decp-v3-marches-valides&q={q}&rows=40",
    .array_path = "records", .title_keys = "fields.objet,fields.titulaire_denominationsociale",
    .id_keys = "recordid", .date_keys = "fields.datenotification",
    .description = "French public contracts (DECP): who won which tender, for how "
      "much, from which buyer, on what date — the money trail between a company "
      "and the French state" },

  { .id = "FR_HATVP_DECLARATIONS", .name = "France HATVP — interest & asset declarations",
    .name_ja = "フランス HATVP — 利益・資産申告", .category = "government",
    .portal = "https://www.hatvp.fr", .record_type = "fr-declaration",
    .tags = "\"fr\",\"pep\",\"declarations\"", .free_tier = 1,
    .url = "https://www.hatvp.fr/livraison/merge/declarations.json",
    .filter_query = 1,
    .title_keys = "nom,prenom", .date_keys = "dateDepot",
    .description = "Declarations of interests and assets filed by French public "
      "officials with the HATVP — mandates, shareholdings, outside activities and "
      "the declaring official's role, filtered to the queried name" },

  { .id = "FR_ASSOS_RNA", .name = "France RNA — associations register",
    .name_ja = "フランス 団体登記(RNA)", .category = "government",
    .portal = "https://www.data.gouv.fr", .record_type = "fr-association",
    .tags = "\"fr\",\"nonprofit\"", .free_tier = 1,
    .url = "https://recherche-entreprises.api.gouv.fr/search?q={q}&est_association=true&per_page=25",
    .array_path = "results", .title_keys = "nom_complet", .id_keys = "siren",
    .date_keys = "date_creation",
    .description = "French associations (loi 1901) matched by name via the open "
      "company search — SIREN, RNA number, address, activity and leadership where "
      "declared; the nonprofit half of the French corporate graph" },

  /* ── Germany ───────────────────────────────────────────────────────────── */
  { .id = "DE_OFFENEREGISTER", .name = "OffeneRegister — German company mirror",
    .name_ja = "ドイツ 商業登記ミラー(OffeneRegister)", .category = "government",
    .portal = "https://offeneregister.de", .record_type = "de-company",
    .tags = "\"de\",\"registry\"", .free_tier = 1,
    .url = "https://db.offeneregister.de/openregister.json?sql="
           "select+*+from+company+where+name+like+%27%25{q}%25%27+limit+40&_shape=objects",
    .array_path = "rows", .title_keys = "name", .id_keys = "company_number",
    .date_keys = "registered_office",
    .description = "The open mirror of the German Handelsregister — company name, "
      "register court and number, legal form, registered office and status, "
      "queryable where the official register is not" },

  { .id = "DE_ABGEORDNETENWATCH", .name = "abgeordnetenwatch — German politicians",
    .name_ja = "ドイツ 議員データベース", .category = "government",
    .portal = "https://www.abgeordnetenwatch.de", .record_type = "de-politician",
    .tags = "\"de\",\"pep\"", .free_tier = 1,
    .url = "https://www.abgeordnetenwatch.de/api/v2/politicians?politician%5Blabel%5D%5Bcn%5D={q}&range_end=25",
    .array_path = "data", .title_keys = "label", .id_keys = "id",
    .detail_url = "https://www.abgeordnetenwatch.de/api/v2/candidacies-mandates?politician={v}",
    .detail_key = "id",
    .description = "German federal and state politicians by name, then a second hop "
      "into every candidacy and mandate they have held — party, parliament, "
      "electoral district and term" },

  /* ── Netherlands / Belgium ─────────────────────────────────────────────── */
  { .id = "NL_KVK_BASISPROFIEL", .name = "KVK — Dutch company base profile",
    .name_ja = "オランダ商業会議所 — 企業基本プロファイル", .category = "government",
    .portal = "https://www.kvk.nl", .record_type = "nl-company",
    .tags = "\"nl\",\"registry\"", .want = HP_NUMERIC,
    .key_env = "KVK_API_KEY", .free_tier = 0,
    .url = "https://api.kvk.nl/api/v1/basisprofielen/{qd}",
    .headers = { "apikey: {key}", NULL },
    .title_keys = "naam,handelsnaam", .id_keys = "kvkNummer",
    .date_keys = "formeleRegistratiedatum",
    .description = "Full KVK base profile behind a Dutch KvK number — statutory and "
      "trade names, legal form, SBI activities, employee count, registration and "
      "cessation dates, material relations" },

  { .id = "NL_KVK_VESTIGINGEN", .name = "KVK — Dutch establishments",
    .name_ja = "オランダ商業会議所 — 事業所一覧", .category = "government",
    .portal = "https://www.kvk.nl", .record_type = "nl-establishment",
    .tags = "\"nl\",\"registry\"", .want = HP_NUMERIC,
    .key_env = "KVK_API_KEY", .free_tier = 0,
    .url = "https://api.kvk.nl/api/v1/basisprofielen/{qd}/vestigingen",
    .headers = { "apikey: {key}", NULL },
    .array_path = "vestigingen", .title_keys = "handelsnaam,naam",
    .id_keys = "vestigingsnummer",
    .description = "Every establishment of a Dutch company — branch numbers, trade "
      "names, visiting addresses and per-site activity codes; the physical map "
      "behind one KvK registration" },

  { .id = "NL_PDOK_LOCATIE", .name = "PDOK Locatieserver — Dutch address & parcel lookup",
    .name_ja = "オランダ 住所・地番検索(PDOK)", .category = "geospatial",
    .portal = "https://www.pdok.nl", .record_type = "nl-address",
    .tags = "\"nl\",\"address\"", .free_tier = 1,
    .url = "https://api.pdok.nl/bzk/locatieserver/search/v3_1/free?q={q}&rows=25",
    .array_path = "response.docs", .title_keys = "weergavenaam", .id_keys = "id",
    .description = "Authoritative Dutch address, postcode, parcel and district "
      "resolution — BAG/BRK identifiers and coordinates for an address string" },

  { .id = "NL_RECHTSPRAAK_SEARCH", .name = "Rechtspraak — Dutch court rulings",
    .name_ja = "オランダ 判例検索", .category = "legal", .type = "scraped",
    .portal = "https://uitspraken.rechtspraak.nl", .record_type = "nl-court-ruling",
    .tags = "\"nl\",\"courts\"", .mode = HP_HTML, .free_tier = 1,
    .url = "https://uitspraken.rechtspraak.nl/results?searchMsg={q}",
    .href_must = "/details", .base = "https://uitspraken.rechtspraak.nl",
    .filter_query = 0,
    .description = "Published Dutch court rulings mentioning an entity, by ECLI. "
      "Server-rendered hits only — a JS-only response yields nothing rather than "
      "a fabricated docket" },

  /* ── EU institutions ───────────────────────────────────────────────────── */
  { .id = "EU_TED_NOTICES", .name = "TED — EU tender notices (search API)",
    .name_ja = "EU 公共調達公告 TED", .category = "government",
    .portal = "https://ted.europa.eu", .record_type = "eu-tender",
    .tags = "\"eu\",\"procurement\"", .free_tier = 1,
    .url = "https://api.ted.europa.eu/v3/notices/search",
    .post_body = "{\"query\":\"FT~\\\"{Q}\\\"\",\"limit\":40,\"page\":1,"
                 "\"fields\":[\"publication-number\",\"notice-title\",\"buyer-name\","
                 "\"place-of-performance\",\"publication-date\",\"deadline-receipt-request\"]}",
    .array_path = "notices", .title_keys = "notice-title,publication-number",
    .id_keys = "publication-number", .date_keys = "publication-date",
    .description = "Full-text search of EU tender notices: buyer, subject, place of "
      "performance, deadlines and publication number for every notice naming the "
      "entity — supplier-side and buyer-side both" },

  { .id = "EU_EUROPARL_MEPS", .name = "European Parliament — MEP register",
    .name_ja = "欧州議会 — 議員登録", .category = "government",
    .portal = "https://data.europarl.europa.eu", .record_type = "eu-mep",
    .tags = "\"eu\",\"pep\",\"parliament\"", .free_tier = 1,
    .url = "https://data.europarl.europa.eu/api/v2/meps?parliamentary-term=10&format=application%2Fld%2Bjson&offset=0",
    .array_path = "data", .filter_query = 1, .title_keys = "label", .id_keys = "identifier",
    .page_param = "offset", .page_size = 50, .page_start = 0,
    .description = "Sitting Members of the European Parliament, filtered to the "
      "queried name — official identifier, label and the EP data URI that keys "
      "their declarations and votes" },

  { .id = "EU_EURLEX_SEARCH", .name = "EUR-Lex — EU legal acts mentioning an entity",
    .name_ja = "EUR-Lex — EU法令検索", .category = "legal", .type = "scraped",
    .portal = "https://eur-lex.europa.eu", .record_type = "eu-legal-act",
    .tags = "\"eu\",\"law\"", .mode = HP_HTML, .free_tier = 1,
    .url = "https://eur-lex.europa.eu/search.html?scope=EURLEX&text={q}&lang=en&type=quick",
    .href_must = "legal-content", .base = "https://eur-lex.europa.eu",
    .filter_query = 0,
    .description = "EU legal acts, case law and preparatory documents naming an "
      "entity — the CELEX-keyed hits that show when a company or person is written "
      "into EU law (listings, designations, authorisations)" },

  { .id = "EU_CURIA_PARTIES", .name = "CJEU Curia — cases by party name",
    .name_ja = "EU司法裁判所 — 当事者名検索", .category = "legal", .type = "scraped",
    .portal = "https://curia.europa.eu", .record_type = "eu-court-case",
    .tags = "\"eu\",\"courts\"", .mode = HP_HTML, .free_tier = 1,
    .url = "https://curia.europa.eu/juris/liste.jsf?language=en&td=ALL&parties={q}",
    .href_must = "document", .base = "https://curia.europa.eu",
    .filter_query = 0,
    .description = "Court of Justice and General Court cases where an entity is a "
      "named party — the EU-level litigation history behind a company, member "
      "state or institution, by case document link" },

  /* ── Switzerland / Ireland / Central Europe ────────────────────────────── */
  { .id = "CH_ZEFIX_SEARCH", .name = "Zefix — Swiss central business index",
    .name_ja = "スイス 商業登記(Zefix)", .category = "government",
    .portal = "https://www.zefix.ch", .record_type = "ch-company",
    .tags = "\"ch\",\"registry\"", .free_tier = 1,
    .url = "https://www.zefix.admin.ch/ZefixPublicREST/api/v1/firm/search.json",
    .post_body = "{\"name\":\"{Q}\",\"languageKey\":\"en\",\"maxEntries\":40}",
    .title_keys = "name", .id_keys = "uid", .date_keys = "shabDate",
    .detail_url = "https://www.zefix.admin.ch/ZefixPublicREST/api/v1/company/uid/{v}",
    .detail_key = "uid",
    .description = "Swiss commercial register search, then the full company record "
      "behind each UID — legal seat, purpose, capital, register office and the "
      "SHAB gazette publications that record every change" },

  { .id = "IE_CRO_COMPANIES", .name = "Ireland CRO — company & submission search",
    .name_ja = "アイルランド 会社登記(CRO)", .category = "government",
    .portal = "https://core.cro.ie", .record_type = "ie-company",
    .tags = "\"ie\",\"registry\"", .key_env = "CRO_API_KEY", .free_tier = 1,
    .url = "https://services.cro.ie/cws/companies?company_name={q}&skip=0&max=40&htmlEnc=1",
    .headers = { "Authorization: Basic {keyb64}", NULL },
    .title_keys = "company_name", .id_keys = "company_num",
    .date_keys = "company_reg_date",
    .page_param = "skip", .page_size = 40, .page_start = 0,
    .description = "Irish CRO company records — number, status, type, registered "
      "address, registration and last-return dates; the CRO number then keys "
      "submissions and officer filings" },

  { .id = "CZ_ARES_VR_BODIES", .name = "ARES VR — Czech statutory bodies",
    .name_ja = "チェコ 商業登記 — 役員・支配者", .category = "government",
    .portal = "https://ares.gov.cz", .record_type = "cz-officer",
    .tags = "\"cz\",\"officers\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://ares.gov.cz/ekonomicke-subjekty-v-be/rest/ekonomicke-subjekty-vr/{qd}",
    .title_keys = "obchodniJmeno,zaznamy.0.obchodniJmeno.0.hodnota",
    .id_keys = "ico",
    .description = "The Czech public-register (VR) extract behind an IČO: statutory "
      "bodies and their members, supervisory board, shareholders and beneficial-"
      "owner-relevant entries with validity dates — not just the company header" },

  { .id = "SK_RPO_ENTITY", .name = "Slovakia RPO — legal entity record",
    .name_ja = "スロバキア 法人登記(RPO)", .category = "government",
    .portal = "https://rpo.statistics.sk", .record_type = "sk-company",
    .tags = "\"sk\",\"registry\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://api.statistics.sk/rpo/v1/entity/{qd}",
    .title_keys = "fullNames.0.value,fullName", .id_keys = "identifiers.0.value",
    .date_keys = "establishment",
    .description = "Full Slovak Register of Legal Entities record behind an IČO — "
      "names over time, legal form, statutory bodies, addresses, activity codes "
      "and establishment/termination dates" },

  { .id = "PL_KRS_ODPIS_S", .name = "Poland KRS — associations register extract",
    .name_ja = "ポーランド KRS — 団体登記抄本", .category = "government",
    .portal = "https://wyszukiwarka-krs.ms.gov.pl", .record_type = "pl-entity",
    .tags = "\"pl\",\"registry\",\"nonprofit\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://api-krs.ms.gov.pl/api/krs/OdpisAktualny/{qd}?rejestr=S&format=json",
    .title_keys = "odpis.dane.dzial1.danePodmiotu.nazwa", .id_keys = "odpis.naglowekA.numerKRS",
    .description = "The Polish KRS 'S' register (associations, foundations, unions) "
      "extract for a KRS number — governing bodies, members, statutory purpose and "
      "the entries the commercial 'P' register does not hold" },

  { .id = "GR_DIAVGEIA", .name = "Diavgeia — Greek government decisions",
    .name_ja = "ギリシャ 行政決定公開(Diavgeia)", .category = "government",
    .portal = "https://diavgeia.gov.gr", .record_type = "gr-decision",
    .tags = "\"gr\",\"transparency\"", .free_tier = 1,
    .url = "https://diavgeia.gov.gr/opendata/search?q={q}&size=40",
    .array_path = "decisions", .title_keys = "subject", .id_keys = "ada",
    .date_keys = "issueDate",
    .link_tmpl = "https://diavgeia.gov.gr/decision/view/{v}", .link_keys = "ada",
    .description = "Every Greek public-sector decision naming an entity — Greece "
      "publishes each act with a unique ADA number, so awards, appointments and "
      "payments to a company are individually addressable" },

  /* ── Ukraine ───────────────────────────────────────────────────────────── */
  { .id = "UA_NAZK_DECLARATIONS", .name = "NAZK — Ukrainian officials' asset declarations",
    .name_ja = "ウクライナ 公職者資産申告(NAZK)", .category = "government",
    .portal = "https://public.nazk.gov.ua", .record_type = "ua-declaration",
    .tags = "\"ua\",\"pep\",\"declarations\"", .free_tier = 1,
    .url = "https://public-api.nazk.gov.ua/v2/documents/list?query={q}",
    .array_path = "items", .title_keys = "firstname,lastname,user_declarant_id",
    .id_keys = "id", .date_keys = "created_date",
    .description = "Asset and income declarations filed by Ukrainian public "
      "officials — declarant, position, agency and document id for each filing "
      "year; the primary corruption-investigation dataset for Ukraine" },

  { .id = "UA_PROZORRO_AWARDS", .name = "Prozorro — Ukrainian tender awards",
    .name_ja = "ウクライナ 調達落札(Prozorro)", .category = "government",
    .portal = "https://prozorro.gov.ua", .record_type = "ua-tender",
    .tags = "\"ua\",\"procurement\"", .free_tier = 1,
    .url = "https://prozorro.gov.ua/api/search/tenders?text={q}&order=desc",
    .array_path = "data", .title_keys = "title,tenderID", .id_keys = "tenderID",
    .date_keys = "dateModified",
    .link_tmpl = "https://prozorro.gov.ua/tender/{v}", .link_keys = "tenderID",
    .description = "Ukrainian public tenders naming an entity — procuring agency, "
      "value, status and tender id; Prozorro publishes the whole procurement cycle, "
      "so a supplier's state business is fully traceable" },

  /* ── Iberia / Adriatic ─────────────────────────────────────────────────── */
  { .id = "PT_RCBE_LOOKUP", .name = "Portugal RCBE — beneficial ownership check",
    .name_ja = "ポルトガル 実質的支配者登記(RCBE)", .category = "government",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://rcbe.justica.gov.pt", .record_type = "pt-bo",
    .tags = "\"pt\",\"beneficial-ownership\"", .free_tier = 1,
    .url = "https://rcbe.justica.gov.pt/Home/Pesquisar?termo={q}",
    .base = "https://rcbe.justica.gov.pt", .filter_query = 1,
    .description = "Portuguese central register of beneficial owners — server-"
      "rendered hits for an entity name or NIPC. Anti-bot or JS-only responses "
      "yield nothing rather than an invented ownership claim" },

  { .id = "HR_SUDREG_SUBJECT", .name = "Croatia Sudski registar — company record",
    .name_ja = "クロアチア 商業登記", .category = "government",
    .portal = "https://sudreg.pravosudje.hr", .record_type = "hr-company",
    .tags = "\"hr\",\"registry\"", .want = HP_NUMERIC, .free_tier = 1,
    .url = "https://sudreg-data.gov.hr/api/javni/subjekt_detalji?tip_identifikatora=oib&identifikator={qd}",
    .title_keys = "tvrtka.ime,tvrtka_naziv", .id_keys = "oib",
    .description = "Detailed Croatian court-register record behind an OIB — legal "
      "form, seat, capital, activities and the registered persons authorised to "
      "represent the company" },
};

HP_REGISTER_TABLE(HP_EU)
