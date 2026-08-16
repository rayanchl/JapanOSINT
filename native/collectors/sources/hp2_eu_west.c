/* collectors/sources/hp2_eu_west.c — France, Benelux and Ireland.
 *
 * Western Europe is where the corporate paperwork is thickest and the OSINT
 * coverage thinnest, because the good records are in national gazettes rather
 * than company registers: BODACC carries every French insolvency, sale of
 * business and management change as structured open data; the Moniteur belge
 * and the Staatscourant do the same for Belgium and the Netherlands. The
 * financial regulators in this cluster — AMF, FSMA, AFM, CSSF, the Central
 * Bank of Ireland — all publish warning lists that name unauthorised firms
 * long before any sanctions body does. */
#include "lib/hpengine.h"

static const hp_source HP2_EU_WEST[] = {
  /* ── France ────────────────────────────────────────────────────────────── */
  { .id = "FR_BODACC_ANNONCES", .name = "France BODACC — commercial announcements",
    .name_ja = "フランス 商業公告(BODACC)", .category = "government",
    .portal = "https://bodacc-datadila.opendatasoft.com", .record_type = "fr-gazette-notice",
    .tags = "\"fr\",\"gazette\",\"insolvency\",\"registry\"", .free_tier = 1,
    .url = "https://bodacc-datadila.opendatasoft.com/api/records/1.0/search/?"
      "dataset=annonces-commerciales&q={q}&rows=100",
    .array_path = "records",
    .title_keys = "fields.commercant,fields.numeroannonce",
    .id_keys = "recordid,fields.id",
    .date_keys = "fields.dateparution",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "Every French commercial announcement as structured data — "
      "incorporations, management changes, sales of business, and the whole "
      "insolvency ladder from sauvegarde to liquidation judiciaire, with the "
      "SIREN, the court and the judgment date. Paged to exhaustion; this is "
      "the single richest French corporate-event source" },

  { .id = "FR_INPI_RNE", .name = "France INPI — registre national des entreprises",
    .name_ja = "フランス 国家企業登録(INPI)", .category = "government",
    .portal = "https://data.inpi.fr", .record_type = "fr-entity",
    .tags = "\"fr\",\"registry\",\"ownership\"",
    .key_env = "INPI_TOKEN", .free_tier = 0,
    .url = "https://registre-national-entreprises.inpi.fr/api/companies?companyName={q}&pageSize=100",
    .headers = { "Authorization: Bearer {key}" },
    .title_keys = "formality.content.personneMorale.identite.entreprise.denomination,siren",
    .id_keys = "siren", .date_keys = "updatedAt",
    .page_param = "page", .page_start = 1,
    .description = "The French national business register — legal form, "
      "capital, the full list of directors and, for companies that filed one, "
      "the beneficial-ownership declaration. Requires an INPI token; without "
      "$INPI_TOKEN the row reports the missing credential rather than "
      "returning anything" },

  { .id = "FR_SIRENE_ETABLISSEMENTS", .name = "France INSEE Sirene — establishments",
    .name_ja = "フランス Sirene 事業所", .category = "government",
    .portal = "https://api.insee.fr", .record_type = "fr-establishment",
    .tags = "\"fr\",\"registry\",\"establishment\"",
    .key_env = "INSEE_SIRENE_TOKEN", .free_tier = 1,
    .url = "https://api.insee.fr/entreprises/sirene/V3.11/siret?"
      "q=denominationUniteLegale:\"{q}\"&nombre=1000",
    .headers = { "X-INSEE-Api-Key-Integration: {key}" },
    .array_path = "etablissements",
    .title_keys = "uniteLegale.denominationUniteLegale,siret",
    .id_keys = "siret", .date_keys = "dateCreationEtablissement",
    .page_param = "debut", .page_size = 1000, .page_start = 0,
    .description = "Every physical establishment of a French company — SIRET, "
      "address, NAF activity code, headcount band, opening and closing dates. "
      "Establishment-level data shows where a group actually operates, not "
      "just where it is registered" },

  { .id = "FR_TRESOR_GELS_AVOIRS", .name = "France — registre national des gels d'avoirs",
    .name_ja = "フランス 資産凍結登録", .category = "government",
    .portal = "https://gels-avoirs.dgtresor.gouv.fr", .record_type = "fr-asset-freeze",
    .tags = "\"fr\",\"sanctions\",\"asset-freeze\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://gels-avoirs.dgtresor.gouv.fr/List?query={q}",
    .base = "https://gels-avoirs.dgtresor.gouv.fr", .filter_query = 1,
    .description = "France's national asset-freeze register — the persons and "
      "entities whose funds are frozen, the legal basis, and the national "
      "designations France applies on top of the EU list, notably in "
      "counter-terrorism cases" },

  { .id = "FR_AMF_DECISIONS", .name = "France AMF — sanctions & enforcement decisions",
    .name_ja = "フランス AMF 制裁決定", .category = "government",
    .portal = "https://www.amf-france.org", .record_type = "fr-enforcement",
    .tags = "\"fr\",\"financial\",\"enforcement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.amf-france.org/fr/recherche-globale?search={q}",
    .base = "https://www.amf-france.org", .filter_query = 1,
    .description = "French market authority sanction decisions, injunctions "
      "and the blacklist of unauthorised investment and crypto websites — with "
      "the firm, the breach found and the financial penalty" },

  { .id = "FR_REGAFI_INSTITUTIONS", .name = "France REGAFI — authorised financial institutions",
    .name_ja = "フランス 金融機関登録(REGAFI)", .category = "government",
    .portal = "https://www.regafi.fr", .record_type = "fr-financial-institution",
    .tags = "\"fr\",\"financial\",\"licence\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.regafi.fr/spip.php?page=results&type=advanced&denomination={q}",
    .base = "https://www.regafi.fr", .filter_query = 1,
    .description = "Institutions authorised by the ACPR — banks, payment and "
      "e-money institutions, investment firms — with their CIB code, licence "
      "categories, effective dates and any withdrawal of authorisation" },

  { .id = "FR_JUDILIBRE_DECISIONS", .name = "France — Cour de cassation & appeal decisions",
    .name_ja = "フランス 破毀院/控訴院 判決", .category = "government",
    .portal = "https://www.courdecassation.fr", .record_type = "fr-judgment",
    .tags = "\"fr\",\"court\",\"judgment\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.courdecassation.fr/recherche-judilibre?search_api_fulltext={q}",
    .base = "https://www.courdecassation.fr", .filter_query = 1,
    .description = "French supreme court and appeal decisions in full text — "
      "commercial disputes, corporate criminal liability and the "
      "money-laundering convictions that name the companies involved" },

  { .id = "FR_GEORISQUES_ICPE", .name = "France Géorisques — classified industrial installations",
    .name_ja = "フランス 分類施設(ICPE)", .category = "government",
    .portal = "https://www.georisques.gouv.fr", .record_type = "fr-industrial-site",
    .tags = "\"fr\",\"environment\",\"infrastructure\"", .free_tier = 1,
    .url = "https://www.georisques.gouv.fr/api/v1/installations_classees?"
      "page=1&page_size=100&nom={q}",
    .array_path = "data", .title_keys = "raisonSociale,nomEts",
    .id_keys = "codeAIOT", .date_keys = "dateMaj",
    .lat_key = "latitude", .lon_key = "longitude",
    .page_param = "page", .page_size = 100, .page_start = 1,
    .description = "French installations classified for environmental "
      "protection — operator, site coordinates, regime (declaration, "
      "registration, authorisation, Seveso), activities and the inspection and "
      "sanction history. Structured records, geolocated, paged to exhaustion" },

  /* ── Belgium ───────────────────────────────────────────────────────────── */
  { .id = "BE_KBO_ENTERPRISE", .name = "Belgium KBO/BCE — crossroads bank for enterprises",
    .name_ja = "ベルギー 企業登録(KBO)", .category = "government",
    .portal = "https://kbopub.economie.fgov.be", .record_type = "be-entity",
    .tags = "\"be\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://kbopub.economie.fgov.be/kbopub/zoeknaamfonetischform.html?searchWord={q}",
    .base = "https://kbopub.economie.fgov.be", .filter_query = 1,
    .description = "Belgian enterprise numbers with legal form, status, every "
      "registered establishment unit, the NACE activity codes and the "
      "authorisations the entity holds" },

  { .id = "BE_MONITEUR_BELGE", .name = "Belgium — Moniteur belge / Belgisch Staatsblad",
    .name_ja = "ベルギー 官報", .category = "government",
    .portal = "https://www.ejustice.just.fgov.be", .record_type = "be-gazette",
    .tags = "\"be\",\"gazette\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.ejustice.just.fgov.be/cgi_tsv/rech.pl?language=fr&nm={q}",
    .base = "https://www.ejustice.just.fgov.be", .filter_query = 1,
    .description = "The Belgian official gazette — articles of association, "
      "director appointments and resignations, capital changes, mergers and "
      "bankruptcy judgments, all published as scanned annexes tied to the "
      "enterprise number" },

  { .id = "BE_FSMA_REGISTER", .name = "Belgium FSMA — registers & warning list",
    .name_ja = "ベルギー FSMA 登録/警告", .category = "government",
    .portal = "https://www.fsma.be", .record_type = "be-financial-entity",
    .tags = "\"be\",\"financial\",\"licence\",\"warning\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.fsma.be/en/search?search_api_fulltext={q}",
    .base = "https://www.fsma.be", .filter_query = 1,
    .description = "Belgian financial regulator registers — intermediaries, "
      "crowdfunding and crypto service providers — plus the warning list of "
      "firms offering financial services in Belgium without authorisation" },

  { .id = "BE_NBB_ACCOUNTS", .name = "Belgium NBB — central balance sheet filings",
    .name_ja = "ベルギー 中央銀行 決算開示", .category = "government",
    .portal = "https://consult.cbso.nbb.be", .record_type = "be-accounts",
    .tags = "\"be\",\"accounts\",\"filings\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://consult.cbso.nbb.be/consult-enterprise?q={q}",
    .base = "https://consult.cbso.nbb.be", .filter_query = 1,
    .description = "Annual accounts filed by Belgian companies with the "
      "National Bank — balance sheet, profit and loss, staff numbers and the "
      "social balance sheet, for every filing year. Belgium requires even "
      "small companies to file, so coverage is close to total" },

  /* ── Netherlands ───────────────────────────────────────────────────────── */
  { .id = "NL_AFM_REGISTERS", .name = "Netherlands AFM — registers & warnings",
    .name_ja = "オランダ AFM 登録/警告", .category = "government",
    .portal = "https://www.afm.nl", .record_type = "nl-financial-entity",
    .tags = "\"nl\",\"financial\",\"licence\",\"warning\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.afm.nl/en/sector/registers?query={q}",
    .base = "https://www.afm.nl", .filter_query = 1,
    .description = "Dutch market authority registers of licensed advisers, "
      "fund managers and crypto firms, together with the public warnings and "
      "penalty decisions the AFM has issued against named parties" },

  { .id = "NL_DNB_REGISTER", .name = "Netherlands DNB — public supervision register",
    .name_ja = "オランダ 中央銀行 監督登録", .category = "government",
    .portal = "https://www.dnb.nl", .record_type = "nl-supervised-entity",
    .tags = "\"nl\",\"financial\",\"licence\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.dnb.nl/en/public-register/?search={q}",
    .base = "https://www.dnb.nl", .filter_query = 1,
    .description = "Institutions supervised by the Dutch central bank — banks, "
      "insurers, payment institutions, trust offices and crypto providers — "
      "with licence type and dates. Dutch trust offices administer a very "
      "large share of Europe's holding structures, and this is the register "
      "that says which of them are still licensed" },

  { .id = "NL_STAATSCOURANT", .name = "Netherlands — official publications search",
    .name_ja = "オランダ 官報/公示", .category = "government",
    .portal = "https://zoek.officielebekendmakingen.nl", .record_type = "nl-gazette",
    .tags = "\"nl\",\"gazette\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://zoek.officielebekendmakingen.nl/resultaten?q={q}",
    .base = "https://zoek.officielebekendmakingen.nl", .filter_query = 1,
    .description = "Dutch official publications — Staatscourant notices, "
      "permits and environmental decisions, parliamentary papers and the "
      "municipal announcements that name applicants and site operators" },

  { .id = "NL_TENDERNED_NOTICES", .name = "Netherlands TenderNed — procurement notices",
    .name_ja = "オランダ 公共調達公告", .category = "government",
    .portal = "https://www.tenderned.nl", .record_type = "nl-tender",
    .tags = "\"nl\",\"procurement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.tenderned.nl/aankondigingen/overzicht?zoekwoorden={q}",
    .base = "https://www.tenderned.nl", .filter_query = 1,
    .description = "Dutch tender notices and award decisions — contracting "
      "authority, CPV codes, estimated value and the awarded supplier, "
      "including the framework agreements that determine who supplies Dutch "
      "government for years at a time" },

  /* ── Luxembourg & Ireland ──────────────────────────────────────────────── */
  { .id = "LU_RCS_ENTITIES", .name = "Luxembourg — trade & companies register (RCS)",
    .name_ja = "ルクセンブルク 商業登記", .category = "government",
    .portal = "https://www.lbr.lu", .record_type = "lu-entity",
    .tags = "\"lu\",\"registry\",\"ownership\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.lbr.lu/mjrcs/jsp/IndexActionNotSecured.action?"
      "time=0&currentMenuLabel=menuItemRcsl&FROM_MENU=true&q={q}",
    .base = "https://www.lbr.lu", .filter_query = 1,
    .description = "Luxembourg registered entities — RCS number, legal form, "
      "managers, and the filed documents including articles and accounts. "
      "Luxembourg holding and fund vehicles sit in the middle of most European "
      "ownership chains, which makes this register a routing table" },

  { .id = "LU_CSSF_ENTITIES", .name = "Luxembourg CSSF — supervised entities & warnings",
    .name_ja = "ルクセンブルク CSSF 監督対象", .category = "government",
    .portal = "https://www.cssf.lu", .record_type = "lu-financial-entity",
    .tags = "\"lu\",\"financial\",\"licence\",\"warning\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.cssf.lu/en/entities-search/?searchterm={q}",
    .base = "https://www.cssf.lu", .filter_query = 1,
    .description = "Luxembourg supervised banks, investment firms, funds and "
      "their management companies, with authorisation status — plus the CSSF "
      "warnings naming entities that falsely claim Luxembourg supervision" },

  { .id = "IE_CBI_REGISTERS", .name = "Ireland — Central Bank registers",
    .name_ja = "アイルランド 中央銀行 登録簿", .category = "government",
    .portal = "https://registers.centralbank.ie", .record_type = "ie-financial-entity",
    .tags = "\"ie\",\"financial\",\"licence\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://registers.centralbank.ie/SearchPage.aspx?searchEntity={q}",
    .base = "https://registers.centralbank.ie", .filter_query = 1,
    .description = "Irish authorised funds, fund service providers, insurers, "
      "payment firms and retail credit firms, with authorisation and "
      "revocation dates — the register behind the largest fund-domicile in "
      "Europe after Luxembourg" },

  { .id = "IE_ETENDERS_NOTICES", .name = "Ireland eTenders — public procurement",
    .name_ja = "アイルランド 公共調達", .category = "government",
    .portal = "https://www.etenders.gov.ie", .record_type = "ie-tender",
    .tags = "\"ie\",\"procurement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.etenders.gov.ie/epps/quickSearchAction.do?searchSelect={q}",
    .base = "https://www.etenders.gov.ie", .filter_query = 1,
    .description = "Irish tender notices and contract award notices — the "
      "contracting authority, value, procedure and the successful supplier, "
      "including awards made under national frameworks" },
};

HP_REGISTER_TABLE(HP2_EU_WEST)
