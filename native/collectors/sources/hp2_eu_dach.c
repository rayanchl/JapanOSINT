/* collectors/sources/hp2_eu_dach.c — Germany, Austria, Switzerland, Liechtenstein.
 *
 * The German-speaking jurisdictions bury their most valuable records behind
 * portals that look closed but are not: insolvency announcements, the
 * beneficial-ownership transparency register, published annual accounts, the
 * financial regulators' unauthorised-business warnings, and — in Austria — a
 * genuine open-data judgment API. Liechtenstein and Switzerland are attached
 * here rather than to "Europe" because the DACH corporate chain routinely runs
 * through Vaduz and Zug regardless of where the operating company sits. */
#include "../../lib/hpengine.h"

static const hp_source HP2_EU_DACH[] = {
  /* ── Germany ───────────────────────────────────────────────────────────── */
  { .id = "DE_UNTERNEHMENSREGISTER", .name = "Germany — Unternehmensregister company search",
    .name_ja = "ドイツ 企業登録簿", .category = "government",
    .portal = "https://www.unternehmensregister.de", .record_type = "de-entity",
    .tags = "\"de\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.unternehmensregister.de/ureg/search1.html?"
      "submitaction=microsearch&research.name={q}",
    .base = "https://www.unternehmensregister.de", .filter_query = 1,
    .description = "The German company register portal — HRB number, register "
      "court, legal form and the index of every document the company has "
      "filed, including shareholder lists and articles of association" },

  { .id = "DE_BUNDESANZEIGER_FILINGS", .name = "Germany Bundesanzeiger — published accounts & notices",
    .name_ja = "ドイツ 連邦官報 決算公告", .category = "government",
    .portal = "https://www.bundesanzeiger.de", .record_type = "de-filing",
    .tags = "\"de\",\"filings\",\"accounts\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.bundesanzeiger.de/pub/en/suchergebnis?fulltext={q}",
    .base = "https://www.bundesanzeiger.de", .filter_query = 1,
    .description = "Annual accounts, capital measures, merger notices and "
      "liquidation announcements published by German companies. Every German "
      "company of size must file here, so it is the closest thing to a "
      "national financial-statement archive" },

  { .id = "DE_TRANSPARENZREGISTER", .name = "Germany — Transparenzregister (beneficial owners)",
    .name_ja = "ドイツ 実質的支配者登録", .category = "government",
    .portal = "https://www.transparenzregister.de", .record_type = "de-beneficial-owner",
    .tags = "\"de\",\"ownership\",\"beneficial-owner\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.transparenzregister.de/treg/en/suche?suchtext={q}",
    .base = "https://www.transparenzregister.de", .filter_query = 1,
    .description = "Germany's beneficial-ownership register. Access has been "
      "restricted since the CJEU ruling, so this row is honest about what it "
      "gets: where the register answers, the beneficial owner and the nature "
      "of control; where it demands a legitimate-interest login, nothing is "
      "invented — the lookup returns empty" },

  { .id = "DE_BAFIN_COMPANY_DB", .name = "Germany BaFin — supervised institutions database",
    .name_ja = "ドイツ BaFin 監督対象機関", .category = "government",
    .portal = "https://portal.mvp.bafin.de", .record_type = "de-financial-institution",
    .tags = "\"de\",\"financial\",\"licence\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://portal.mvp.bafin.de/database/InstInfo/suche.do?"
      "locale=en_GB&meldung=Suche&searchString={q}",
    .base = "https://portal.mvp.bafin.de", .filter_query = 1,
    .description = "Institutions authorised by BaFin — banks, insurers, "
      "payment and crypto-custody firms — with the BaFin identification "
      "number, the licences held and the date each was granted or withdrawn" },

  { .id = "DE_BAFIN_WARNINGS", .name = "Germany BaFin — unauthorised business & investor warnings",
    .name_ja = "ドイツ BaFin 無登録業者警告", .category = "government",
    .portal = "https://www.bafin.de", .record_type = "de-regulator-warning",
    .tags = "\"de\",\"financial\",\"enforcement\",\"warning\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.bafin.de/SiteGlobals/Forms/Suche/EN/Servicesuche_Formular.html?input_={q}",
    .base = "https://www.bafin.de", .filter_query = 1,
    .description = "BaFin warnings against firms operating without "
      "authorisation, cease-and-desist orders and identity-abuse notices. The "
      "German regulator names cloned-firm and boiler-room operations here "
      "months before they reach any sanctions list" },

  { .id = "DE_INSOLVENZ_ANNOUNCEMENTS", .name = "Germany — insolvency announcements",
    .name_ja = "ドイツ 倒産公告", .category = "government",
    .portal = "https://www.insolvenzbekanntmachungen.de", .record_type = "de-insolvency",
    .tags = "\"de\",\"insolvency\",\"court\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.insolvenzbekanntmachungen.de/cgi-bin/bl_suche.pl?"
      "Suchfunktion=uneingeschraenkt&Name={q}",
    .base = "https://www.insolvenzbekanntmachungen.de", .filter_query = 1,
    .description = "Court-published German insolvency proceedings — the "
      "debtor, the insolvency court and file number, the administrator "
      "appointed and each procedural step from filing to termination. The "
      "earliest reliable signal that a German counterparty has failed" },

  { .id = "DE_LOBBYREGISTER", .name = "Germany — Bundestag lobby register",
    .name_ja = "ドイツ 連邦議会 ロビー登録", .category = "government",
    .portal = "https://www.lobbyregister.bundestag.de", .record_type = "de-lobbyist",
    .tags = "\"de\",\"lobbying\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.lobbyregister.bundestag.de/suche?q={q}",
    .base = "https://www.lobbyregister.bundestag.de", .filter_query = 1,
    .description = "Organisations registered to lobby the Bundestag and "
      "federal government — declared expenditure band, clients, the interests "
      "represented, the staff involved and the funding sources they had to "
      "disclose" },

  { .id = "DE_DPMA_REGISTER", .name = "Germany DPMAregister — patents, trademarks & designs",
    .name_ja = "ドイツ特許商標庁 登録", .category = "government",
    .portal = "https://register.dpma.de", .record_type = "de-ip-right",
    .tags = "\"de\",\"ip\",\"patent\",\"trademark\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://register.dpma.de/DPMAregister/marke/einsteiger?queryString={q}",
    .base = "https://register.dpma.de", .filter_query = 1,
    .description = "German industrial property rights with the proprietor, "
      "representative, legal status and the register of transfers and licences "
      "— including the security interests that companies grant over IP" },

  { .id = "DE_PRTR_EMISSIONS", .name = "Germany PRTR — facility emissions & operators",
    .name_ja = "ドイツ 汚染物質排出移動登録", .category = "government",
    .portal = "https://www.thru.de", .record_type = "de-facility",
    .tags = "\"de\",\"environment\",\"infrastructure\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.thru.de/thrude/suche/?tx_solr%5Bq%5D={q}",
    .base = "https://www.thru.de", .filter_query = 1,
    .description = "German pollutant release and transfer register — the "
      "facility, its operator, coordinates, industrial activity and reported "
      "releases by substance and year. An operator-named map of German "
      "industrial sites" },

  { .id = "DE_RECHTSPRECHUNG", .name = "Germany — federal case law database",
    .name_ja = "ドイツ 連邦判例データベース", .category = "government",
    .portal = "https://www.rechtsprechung-im-internet.de", .record_type = "de-judgment",
    .tags = "\"de\",\"court\",\"judgment\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.rechtsprechung-im-internet.de/jportal/portal/page/"
      "bsjrsprod.psml?query={q}",
    .base = "https://www.rechtsprechung-im-internet.de", .filter_query = 1,
    .description = "Decisions of the German federal courts in full text — "
      "the parties as anonymised by the court, the facts found and the "
      "reasoning. Federal judgments are where corporate liability, competition "
      "and tax disputes finally resolve" },

  /* ── Austria ───────────────────────────────────────────────────────────── */
  { .id = "AT_FIRMENBUCH_LOOKUP", .name = "Austria — Firmenbuch company lookup",
    .name_ja = "オーストリア 商業登記", .category = "government",
    .portal = "https://justizonline.gv.at", .record_type = "at-entity",
    .tags = "\"at\",\"registry\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://justizonline.gv.at/jop/web/firmenbuchabfrage?suchbegriff={q}",
    .base = "https://justizonline.gv.at", .filter_query = 1,
    .description = "Austrian commercial register — FN number, legal form, "
      "registered seat, managing directors and their signing authority, and "
      "the shareholders recorded on the register" },

  { .id = "AT_EDIKTSDATEI", .name = "Austria — Ediktsdatei (insolvency & judicial notices)",
    .name_ja = "オーストリア 司法公告(倒産/競売)", .category = "government",
    .portal = "https://edikte.justiz.gv.at", .record_type = "at-judicial-notice",
    .tags = "\"at\",\"insolvency\",\"court\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://edikte.justiz.gv.at/edikte/id/idedi8.nsf/suchedi?SearchView&query={q}",
    .base = "https://edikte.justiz.gv.at", .filter_query = 1,
    .description = "Austrian judicial edicts — insolvency openings and "
      "terminations, compulsory auctions of real property, missing-person and "
      "guardianship notices. The insolvency and forced-sale record for every "
      "Austrian debtor, published by the courts themselves" },

  { .id = "AT_FMA_REGISTER", .name = "Austria FMA — licensed entities & warnings",
    .name_ja = "オーストリア FMA 免許先/警告", .category = "government",
    .portal = "https://www.fma.gv.at", .record_type = "at-financial-entity",
    .tags = "\"at\",\"financial\",\"licence\",\"warning\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.fma.gv.at/en/search/?tx_solr%5Bq%5D={q}",
    .base = "https://www.fma.gv.at", .filter_query = 1,
    .description = "Austrian financial market authority company database and, "
      "just as importantly, its investor warnings against firms providing "
      "regulated services without a licence" },

  { .id = "AT_RIS_JUDGMENTS", .name = "Austria RIS — judgments open data API",
    .name_ja = "オーストリア 法情報システム 判例", .category = "government",
    .portal = "https://www.ris.bka.gv.at", .record_type = "at-judgment",
    .tags = "\"at\",\"court\",\"judgment\"", .free_tier = 1,
    .url = "https://data.bka.gv.at/ris/api/v2.6/Judikatur?Applikation=Justiz&"
      "Suchworte={q}&DokumenteProSeite=One_Hundred&Seitennummer=1",
    .title_keys = "Data.Metadaten.Judikatur.Justiz.Geschaeftszahl,Data.Metadaten.Technisch.ID",
    .id_keys = "Data.Metadaten.Technisch.ID",
    .date_keys = "Data.Metadaten.Judikatur.Justiz.Entscheidungsdatum",
    .page_param = "Seitennummer", .page_start = 1,
    .description = "Austrian court decisions as structured records from the "
      "legal information system's open-data API — case number, deciding court, "
      "decision date, norms applied and the document URLs. A real API, walked "
      "page by page rather than sampled" },

  { .id = "AT_LOBBYREGISTER", .name = "Austria — lobbying & interest representation register",
    .name_ja = "オーストリア ロビー登録", .category = "government",
    .portal = "https://lobbyreg.justiz.gv.at", .record_type = "at-lobbyist",
    .tags = "\"at\",\"lobbying\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://lobbyreg.justiz.gv.at/edikte/ir/iredi18.nsf/suchel?SearchView&query={q}",
    .base = "https://lobbyreg.justiz.gv.at", .filter_query = 1,
    .description = "Austrian lobbying register entries — lobbying firms, their "
      "clients, in-house lobbyists and self-governing bodies, with the "
      "declared turnover from lobbying activity" },

  { .id = "AT_DATA_GV_AT", .name = "Austria data.gv.at — open data catalogue",
    .name_ja = "オーストリア オープンデータ", .category = "government",
    .portal = "https://www.data.gv.at", .record_type = "at-dataset",
    .tags = "\"at\",\"opendata\"", .free_tier = 1,
    .url = "https://www.data.gv.at/katalog/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,name",
    .id_keys = "id", .date_keys = "metadata_modified",
    .link_keys = "name", .link_tmpl = "https://www.data.gv.at/katalog/dataset/{v}",
    .page_param = "start", .page_size = 100, .page_start = 0,
    .description = "Austria's federal and regional open-data catalogue — "
      "subsidy recipient lists, permit registers, cadastral and infrastructure "
      "layers — with every matching dataset and its downloadable resources" },

  /* ── Switzerland & Liechtenstein ───────────────────────────────────────── */
  { .id = "CH_SECO_SANCTIONS", .name = "Switzerland SECO — sanctions search (SESAM)",
    .name_ja = "スイス SECO 制裁検索", .category = "government",
    .portal = "https://www.sesam.search.admin.ch", .record_type = "ch-sanction",
    .tags = "\"ch\",\"sanctions\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.sesam.search.admin.ch/sesam-search-web/pages/search.xhtml?q={q}",
    .base = "https://www.sesam.search.admin.ch", .filter_query = 1,
    .description = "Switzerland's sanctions listings with the ordinance that "
      "imposed each measure. Swiss listings diverge from the EU's in timing "
      "and scope, and Swiss financial intermediaries are bound by this list "
      "rather than the EU one" },

  { .id = "CH_FINMA_AUTHORISED", .name = "Switzerland FINMA — authorised institutions & warning list",
    .name_ja = "スイス FINMA 認可機関/警告", .category = "government",
    .portal = "https://www.finma.ch", .record_type = "ch-financial-entity",
    .tags = "\"ch\",\"financial\",\"licence\",\"warning\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.finma.ch/en/search/?q={q}",
    .base = "https://www.finma.ch", .filter_query = 1,
    .description = "FINMA-authorised banks, securities firms, fund managers, "
      "insurers and their approved products — plus the warning list of "
      "entities acting without authorisation and the enforcement reports "
      "naming the institutions FINMA has proceeded against" },

  { .id = "CH_SHAB_GAZETTE", .name = "Switzerland SHAB — official commercial gazette",
    .name_ja = "スイス 商業公報", .category = "government",
    .portal = "https://www.shab.ch", .record_type = "ch-gazette",
    .tags = "\"ch\",\"gazette\",\"registry\"", .free_tier = 1,
    .url = "https://www.shab.ch/api/v1/publications?query={q}&pageRequest.size=100"
      "&pageRequest.page=0",
    .array_path = "content", .title_keys = "meta.title.de,meta.title.fr,meta.title.en",
    .id_keys = "meta.id", .date_keys = "meta.publicationDate",
    .page_param = "pageRequest.page", .page_size = 100, .page_start = 0,
    .description = "Every Swiss commercial-register mutation, debt-collection "
      "notice, bankruptcy schedule and legal announcement as structured "
      "records — the daily change stream behind Zefix, including the "
      "director changes and capital movements Zefix only shows as a snapshot" },

  { .id = "CH_SIMAP_TENDERS", .name = "Switzerland simap — public procurement",
    .name_ja = "スイス 公共調達", .category = "government",
    .portal = "https://www.simap.ch", .record_type = "ch-tender",
    .tags = "\"ch\",\"procurement\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.simap.ch/shabforms/COMMON/search/searchForm.jsf?q={q}",
    .base = "https://www.simap.ch", .filter_query = 1,
    .description = "Swiss federal, cantonal and municipal tender notices and "
      "awards — contracting authority, procedure, the winning bidder and the "
      "award value, including the direct awards that bypass competition" },

  { .id = "LI_HANDELSREGISTER", .name = "Liechtenstein — commercial register",
    .name_ja = "リヒテンシュタイン 商業登記", .category = "government",
    .portal = "https://www.oera.li", .record_type = "li-entity",
    .tags = "\"li\",\"registry\",\"ownership\"",
    .type = "scraped", .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.oera.li/hrweb/ger/firmensuche_afj.htm?name={q}",
    .base = "https://www.oera.li", .filter_query = 1,
    .description = "Liechtenstein's commercial register — the Anstalt, "
      "Stiftung and Treuunternehmen structures that hold assets for owners who "
      "never appear elsewhere, with their registered representatives and the "
      "trustee firms administering them" },
};

HP_REGISTER_TABLE(HP2_EU_DACH)
