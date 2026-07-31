/* collectors/sources/hp3_gov_mena_cis_public.c — MENA & post-Soviet public record.
 *
 * Two regions with the same structural feature: the corporate register is thin
 * or paywalled, but the *state's transactional record* is wide open. Saudi
 * Arabia publishes every government tender through Etimad. Russia publishes
 * every state purchase through zakupki.gov.ru and every corporate legal fact —
 * bankruptcy intent, pledge, auditor change, lease — through Fedresurs.
 * Ukraine publishes the full unified state register plus Prozorro. Israel runs
 * a CKAN portal and the Knesset runs an OData service.
 *
 * For counterparties in these regions the procurement and legal-fact feeds are
 * usually a better identification path than the company register, because they
 * carry the tax identifier alongside the name. */
#include "../../lib/hpengine.h"

static const hp_source HP3_GOV_MENA_CIS[] = {
  /* ── Gulf ─────────────────────────────────────────────────────────────── */
  { .id = "SA_ETIMAD_TENDERS", .name = "Saudi Arabia Etimad — government tenders & awards",
    .name_ja = "サウジアラビア 政府調達", .category = "government",
    .portal = "https://tenders.etimad.sa", .record_type = "sa-tender",
    .tags = "\"sa\",\"procurement\",\"gulf\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://tenders.etimad.sa/Tender/AllTendersForVisitor?"
      "MultipleSearch={q}",
    .base = "https://tenders.etimad.sa", .filter_query = 1,
    .description = "Every Saudi government tender routed through the Etimad "
      "platform — the buying agency, the tender number and value, the bid "
      "opening date and the award. The single best window into Saudi public "
      "spending and the contractors who receive it" },

  { .id = "AE_BAYANAT_OPEN_DATA", .name = "UAE Bayanat — federal open data portal",
    .name_ja = "UAE オープンデータ", .category = "government",
    .portal = "https://bayanat.ae", .record_type = "ae-dataset",
    .tags = "\"ae\",\"open-data\",\"gulf\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://bayanat.ae/en/DataSet?q={q}",
    .base = "https://bayanat.ae", .filter_query = 1,
    .description = "The UAE federal data portal — licensing counts, trade "
      "volumes, sectoral registers and the emirate-level datasets that carry "
      "establishment and permit records" },

  { .id = "AE_DIFC_PUBLIC_REGISTER", .name = "DIFC — Dubai International Financial Centre register",
    .name_ja = "DIFC 公開登録簿", .category = "finance",
    .portal = "https://www.difc.ae", .record_type = "ae-entity",
    .tags = "\"ae\",\"registry\",\"financial-centre\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://www.difc.ae/public-register?search={q}",
    .base = "https://www.difc.ae", .filter_query = 1,
    .description = "Entities registered in the DIFC free zone — legal form, "
      "registration number, registered address and the DFSA licence category "
      "where the entity is regulated. A common holding layer between Gulf "
      "operating companies and offshore owners" },

  { .id = "BH_SIJILAT_REGISTRY", .name = "Bahrain Sijilat — commercial registration",
    .name_ja = "バーレーン 商業登記", .category = "government",
    .portal = "https://www.sijilat.bh", .record_type = "bh-entity",
    .tags = "\"bh\",\"registry\",\"gulf\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.sijilat.bh/Pages/CRSearch.aspx?name={q}",
    .base = "https://www.sijilat.bh", .filter_query = 1,
    .description = "Bahrain's commercial registration system — CR number, "
      "legal form, licensed activities and the branch structure, in the Gulf's "
      "most openly searchable company register" },

  { .id = "QA_MONAQASAT_TENDERS", .name = "Qatar — government tenders portal",
    .name_ja = "カタール 政府調達", .category = "government",
    .portal = "https://monaqasat.mof.gov.qa", .record_type = "qa-tender",
    .tags = "\"qa\",\"procurement\",\"gulf\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://monaqasat.mof.gov.qa/Tenders?search={q}",
    .base = "https://monaqasat.mof.gov.qa", .filter_query = 1,
    .description = "Qatari ministry and public-entity tenders — the tender "
      "reference, the scope, the bond required and the award, covering the "
      "infrastructure programme that dominates Qatari state spending" },

  { .id = "KW_KUWAIT_TENDERS", .name = "Kuwait CAPT — central agency for public tenders",
    .name_ja = "クウェート 中央入札庁", .category = "government",
    .portal = "https://www.capt.gov.kw", .record_type = "kw-tender",
    .tags = "\"kw\",\"procurement\",\"gulf\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.capt.gov.kw/search?q={q}",
    .base = "https://www.capt.gov.kw", .filter_query = 1,
    .description = "Kuwait's central tendering agency — tender announcements, "
      "the classified-supplier register and the award decisions published in "
      "the agency's bulletin" },

  { .id = "OM_ISTITHMAAR_TENDERS", .name = "Oman — tender board & government procurement",
    .name_ja = "オマーン 政府調達", .category = "government",
    .portal = "https://etendering.tenderboard.gov.om", .record_type = "om-tender",
    .tags = "\"om\",\"procurement\",\"gulf\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://etendering.tenderboard.gov.om/product/publicDashboard?search={q}",
    .base = "https://etendering.tenderboard.gov.om", .filter_query = 1,
    .description = "Oman's tender board portal — public tender notices, the "
      "registered supplier grades and the award announcements for ministry and "
      "state-owned enterprise contracts" },

  /* ── Levant, Turkey, North Africa ─────────────────────────────────────── */
  { .id = "IL_DATA_GOV_CKAN", .name = "Israel data.gov.il — national data catalog",
    .name_ja = "イスラエル オープンデータ", .category = "government",
    .portal = "https://data.gov.il", .record_type = "il-dataset",
    .tags = "\"il\",\"open-data\",\"catalog\"", .free_tier = 1,
    .url = "https://data.gov.il/api/3/action/package_search?q={q}&rows=100",
    .array_path = "result.results", .title_keys = "title,organization.title",
    .id_keys = "id", .date_keys = "metadata_modified",
    .page_param = "start", .page_size = 100, .page_start = 0, .page_max = 30,
    .description = "Israel's CKAN portal — the companies register extract, "
      "non-profit register, licensed dealers, building permits and the "
      "government contracts dataset, each with a datastore API behind it" },

  { .id = "IL_KNESSET_ODATA", .name = "Knesset — parliamentary OData service",
    .name_ja = "イスラエル国会 データサービス", .category = "government",
    .portal = "https://knesset.gov.il", .record_type = "il-parliamentary-record",
    .tags = "\"il\",\"parliament\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://main.knesset.gov.il/EN/Search/Pages/Results.aspx?k={q}",
    .base = "https://main.knesset.gov.il", .filter_query = 1,
    .description = "Knesset members, bills, committee sessions, votes and the "
      "lobbyist register, exposed as structured data rather than transcripts" },

  { .id = "TR_EKAP_PROCUREMENT", .name = "Turkey EKAP — public procurement platform",
    .name_ja = "トルコ 公共調達", .category = "government",
    .portal = "https://ekap.kik.gov.tr", .record_type = "tr-tender",
    .tags = "\"tr\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://ekap.kik.gov.tr/EKAP/Ortak/IhaleArama/index.html?keyword={q}",
    .base = "https://ekap.kik.gov.tr", .filter_query = 1,
    .description = "Turkey's electronic procurement platform — tender notices "
      "and results, plus the Public Procurement Authority's ban list of firms "
      "excluded from bidding and the reason for each" },

  { .id = "TR_RESMI_GAZETE", .name = "Turkey — Resmî Gazete official gazette",
    .name_ja = "トルコ 官報", .category = "government",
    .portal = "https://www.resmigazete.gov.tr", .record_type = "tr-gazette-notice",
    .tags = "\"tr\",\"gazette\",\"law\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.resmigazete.gov.tr/arama?q={q}",
    .base = "https://www.resmigazete.gov.tr", .filter_query = 1,
    .description = "The Turkish official gazette — presidential decrees, asset "
      "freezes, trustee appointments over companies, licence grants and the "
      "regulatory decisions that take effect on publication" },

  { .id = "TR_KAP_DISCLOSURES", .name = "Turkey KAP — public disclosure platform",
    .name_ja = "トルコ 上場企業開示", .category = "finance",
    .portal = "https://www.kap.org.tr", .record_type = "tr-listed-disclosure",
    .tags = "\"tr\",\"listed\",\"disclosure\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.kap.org.tr/en/bildirim-sorgu?query={q}",
    .base = "https://www.kap.org.tr", .filter_query = 1,
    .description = "Material event disclosures by Turkish listed companies, "
      "investment funds and their subsidiaries — shareholding changes, "
      "management changes, related-party transactions and audit reports" },

  { .id = "MA_MARCHES_PUBLICS", .name = "Morocco — public procurement portal",
    .name_ja = "モロッコ 公共調達", .category = "government",
    .portal = "https://www.marchespublics.gov.ma", .record_type = "ma-tender",
    .tags = "\"ma\",\"procurement\",\"maghreb\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.marchespublics.gov.ma/index.php?page=entreprise."
      "EntrepriseAdvancedSearch&keyword={q}",
    .base = "https://www.marchespublics.gov.ma", .filter_query = 1,
    .description = "Moroccan state and local authority tenders — the notice, "
      "the estimate, the bidders admitted and the award, plus the excluded "
      "contractors list maintained by the Treasury" },

  { .id = "TN_OPEN_DATA_TENDERS", .name = "Tunisia — open data & public procurement",
    .name_ja = "チュニジア オープンデータ/調達", .category = "government",
    .portal = "http://www.data.gov.tn", .record_type = "tn-dataset",
    .tags = "\"tn\",\"open-data\",\"procurement\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "http://www.data.gov.tn/fr/dataset?q={q}",
    .base = "http://www.data.gov.tn", .filter_query = 1,
    .description = "Tunisia's open data portal, which carries the TUNEPS "
      "procurement extracts, the public enterprise list and the budget "
      "execution data published after the transparency reforms" },

  { .id = "EG_EGYPT_GOV_SEARCH", .name = "Egypt — government services & gazette search",
    .name_ja = "エジプト 政府情報検索", .category = "government",
    .portal = "https://www.egypt.gov.eg", .record_type = "eg-government-record",
    .tags = "\"eg\",\"government\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.egypt.gov.eg/english/search.aspx?q={q}",
    .base = "https://www.egypt.gov.eg", .filter_query = 1,
    .description = "The Egyptian government portal — ministerial decrees, "
      "investment authority announcements, tender notices and the licensing "
      "decisions published centrally rather than by each ministry" },

  { .id = "JO_JORDAN_TENDERS", .name = "Jordan — government tenders directorate",
    .name_ja = "ヨルダン 政府調達", .category = "government",
    .portal = "https://gtd.gov.jo", .record_type = "jo-tender",
    .tags = "\"jo\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://gtd.gov.jo/en/search?q={q}",
    .base = "https://gtd.gov.jo", .filter_query = 1,
    .description = "Jordan's General Tenders Directorate — tender "
      "announcements, the classified contractor register with grades, and the "
      "award decisions for ministry works and supply contracts" },

  /* ── Russia and the post-Soviet space ─────────────────────────────────── */
  { .id = "RU_ZAKUPKI_PROCUREMENT", .name = "Russia zakupki.gov.ru — unified procurement register",
    .name_ja = "ロシア 統一調達登録", .category = "government",
    .portal = "https://zakupki.gov.ru", .record_type = "ru-procurement",
    .tags = "\"ru\",\"procurement\",\"state-contract\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://zakupki.gov.ru/epz/order/extendedsearch/results.html"
      "?searchString={q}&morphology=on",
    .base = "https://zakupki.gov.ru", .filter_query = 1,
    .description = "Every Russian state and state-company purchase — customer, "
      "supplier with its INN, contract value, subject and execution status. "
      "The most detailed public record of who supplies the Russian state, "
      "including entities that appear nowhere else" },

  { .id = "RU_FEDRESURS_FACTS", .name = "Russia Fedresurs — register of corporate legal facts",
    .name_ja = "ロシア 法人事実登録簿", .category = "government",
    .portal = "https://fedresurs.ru", .record_type = "ru-corporate-fact",
    .tags = "\"ru\",\"insolvency\",\"corporate-events\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://fedresurs.ru/search/entity?query={q}",
    .base = "https://fedresurs.ru", .filter_query = 1,
    .description = "The mandatory register of corporate legal facts — intent "
      "to file for bankruptcy, pledges over assets, leasing contracts, auditor "
      "opinions, net-asset reductions and reorganisations. Russian distress "
      "appears here months before it appears in a court file" },

  { .id = "RU_EGRUL_FNS_EXTRACT", .name = "Russia EGRUL — tax service company extract",
    .name_ja = "ロシア 法人登記抄本", .category = "government",
    .portal = "https://egrul.nalog.ru", .record_type = "ru-entity",
    .tags = "\"ru\",\"registry\",\"tax\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://egrul.nalog.ru/index.html?query={q}",
    .base = "https://egrul.nalog.ru", .filter_query = 1,
    .description = "The Federal Tax Service's own extract service — INN, OGRN, "
      "registered address, director, founders with their shares, and the "
      "unreliability flags the tax service stamps on an address or a director" },

  { .id = "RU_ARBITR_BANKRUPTCY_GAZETTE", .name = "Russia Kommersant — bankruptcy notices",
    .name_ja = "ロシア 破産公告", .category = "legal",
    .portal = "https://bankruptcy.kommersant.ru", .record_type = "ru-insolvency-notice",
    .tags = "\"ru\",\"insolvency\",\"gazette\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://bankruptcy.kommersant.ru/search?query={q}",
    .base = "https://bankruptcy.kommersant.ru", .filter_query = 1,
    .description = "The statutory bankruptcy gazette — commencement of "
      "proceedings, the appointed manager, creditor meetings and the asset "
      "auctions, which is where a distressed Russian company's assets are "
      "traceable as they leave" },

  { .id = "UA_EDR_MINJUST", .name = "Ukraine — unified state register (EDR) search",
    .name_ja = "ウクライナ 統一国家登記", .category = "government",
    .portal = "https://usr.minjust.gov.ua", .record_type = "ua-entity",
    .tags = "\"ua\",\"registry\",\"ownership\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://usr.minjust.gov.ua/content/free-search?query={q}",
    .base = "https://usr.minjust.gov.ua", .filter_query = 1,
    .description = "Ukraine's unified state register of legal entities — EDRPOU "
      "code, address, director, founders and, since the transparency reforms, "
      "the ultimate beneficial owner declared for each entity" },

  { .id = "UA_OPENDATABOT_ENTITY", .name = "Ukraine Opendatabot — registry & court monitoring",
    .name_ja = "ウクライナ 登記/裁判モニタ", .category = "government",
    .portal = "https://opendatabot.ua", .record_type = "ua-entity",
    .tags = "\"ua\",\"registry\",\"courts\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://opendatabot.ua/search?query={q}",
    .base = "https://opendatabot.ua", .filter_query = 1,
    .description = "The consolidated Ukrainian view — registry changes, court "
      "cases, tax debts, enforcement proceedings, licence grants and property "
      "registrations against one entity or person" },

  { .id = "BY_BELARUS_TENDERS", .name = "Belarus — state procurement portal",
    .name_ja = "ベラルーシ 政府調達", .category = "government",
    .portal = "https://icetrade.by", .record_type = "by-procurement",
    .tags = "\"by\",\"procurement\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://icetrade.by/search/auctions?search={q}",
    .base = "https://icetrade.by", .filter_query = 1,
    .description = "Belarusian state purchasing — customer, supplier with UNP, "
      "lot value and outcome. In a jurisdiction with heavy sanctions exposure, "
      "the state contract feed is often the only current evidence a company is "
      "still operating and who it trades with" },

  { .id = "AM_ARLIS_LEGAL", .name = "Armenia ARLIS — legal information system",
    .name_ja = "アルメニア 法令情報", .category = "legal",
    .portal = "https://www.arlis.am", .record_type = "am-legislation",
    .tags = "\"am\",\"law\",\"caucasus\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://www.arlis.am/DocumentView.aspx?search={q}",
    .base = "https://www.arlis.am", .filter_query = 1,
    .description = "Armenia's legal database — laws, government decisions, "
      "licence grants and the individual administrative acts naming companies "
      "that receive concessions or exemptions" },

  { .id = "GE_MATSNE_GAZETTE", .name = "Georgia Matsne — legislative herald",
    .name_ja = "ジョージア 官報", .category = "government",
    .portal = "https://matsne.gov.ge", .record_type = "ge-gazette-notice",
    .tags = "\"ge\",\"gazette\",\"law\",\"caucasus\"", .type = "scraped",
    .mode = HP_HTML, .free_tier = 1,
    .url = "https://matsne.gov.ge/ka/search?q={q}",
    .base = "https://matsne.gov.ge", .filter_query = 1,
    .description = "Georgia's legislative herald — laws, government "
      "resolutions, ministerial orders and the individual acts granting "
      "licences, concessions and citizenship, all published in full" },

  { .id = "UZ_LEX_LEGISLATION", .name = "Uzbekistan Lex.uz — national legislation database",
    .name_ja = "ウズベキスタン 法令データベース", .category = "legal",
    .portal = "https://lex.uz", .record_type = "uz-legislation",
    .tags = "\"uz\",\"law\",\"central-asia\"", .type = "scraped", .mode = HP_HTML,
    .free_tier = 1,
    .url = "https://lex.uz/search/nat?query={q}",
    .base = "https://lex.uz", .filter_query = 1,
    .description = "Uzbek laws, presidential decrees and cabinet resolutions — "
      "including the named-company decrees that grant investment privileges, "
      "land allocations and tax exemptions, a hallmark of Central Asian "
      "state-business relations" },
};

HP_REGISTER_TABLE(HP3_GOV_MENA_CIS)
