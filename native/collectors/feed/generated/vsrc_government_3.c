/* Verified-live government sources (45), part 3.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(gov_opensanctions_eu_fsf, "gov-opensanctions-eu-fsf", "OpenSanctions — EU Consolidated Sanctions", "OpenSanctions EU統合制裁リスト",
  "government", "sanctions",
  "https://data.opensanctions.org/datasets/latest/eu_fsf/index.json",
  "resources",
  "en", "[\"sanctions\",\"eu\"]", 86400,
  "OpenSanctions build of the EU Financial Sanctions Files consolidated list.");

/* Re-pointed 2026-09-15. datasets/latest/gb_hmt_sanctions/index.json now 307s
 * to a frozen 2026-03-07 artifact that answers 403, and OpenSanctions' own
 * gb_hmt_sanctions dataset page is 404: the HMT/OFSI consolidated list was
 * retired when the UK moved its sanctions register to the FCDO. The same
 * register is published as gb_fcdo_sanctions ("UK FCDO Sanctions List —
 * official register of individuals, entities and vessels subject to UK
 * sanctions under SAMLA 2018", 18,543 entities, updated 2026-09-15). No other
 * registered source reads that dataset. The id is kept so history stays joined. */
VJSON(gov_opensanctions_gb_hmt, "gov-opensanctions-gb-hmt", "OpenSanctions — UK FCDO (formerly HMT) Sanctions", "OpenSanctions 英国制裁リスト（FCDO、旧財務省）",
  "government", "sanctions",
  "https://data.opensanctions.org/datasets/latest/gb_fcdo_sanctions/index.json",
  "resources",
  "en", "[\"sanctions\",\"uk\",\"ofsi\",\"fcdo\"]", 86400,
  "OpenSanctions build of the UK sanctions register — the FCDO UK Sanctions List that replaced the HM Treasury / OFSI consolidated list.");

VJSON(gov_opensanctions_index, "gov-opensanctions-index", "OpenSanctions — Dataset Catalogue", "OpenSanctions データセット目録",
  "government", "sanctions",
  "https://data.opensanctions.org/datasets/latest/index.json",
  "datasets",
  "en", "[\"sanctions\",\"catalogue\",\"global\"]", 86400,
  "Master index of all 460+ OpenSanctions source datasets with coverage, entity counts and refresh times.");

/* peps / sanctions: the collection index's "children" (and "datasets") are
 * arrays of dataset NAME STRINGS (live 2026-09-06: ["fr_hatvp_declarations",
 * "fj_parliament", ...]), which jsonlist cannot turn into records — hence the
 * sweep's EMITS_NOTHING on both rows. "resources" is the index's only array
 * of objects (name, title, url, checksum, size, mime_type, updated) and is
 * what the gb_hmt row above already reads; the collection rows now read it
 * too. The endpoint 307-redirects to the dated artifact; the client follows. */
VJSON(gov_opensanctions_peps, "gov-opensanctions-peps", "OpenSanctions — PEP Collection", "OpenSanctions 重要公人コレクション",
  "government", "transparency",
  "https://data.opensanctions.org/datasets/latest/peps/index.json",
  "resources",
  "en", "[\"pep\",\"politicians\",\"global\"]", 86400,
  "Index of politically exposed person datasets covering 190 national sources.");

VJSON(gov_opensanctions_sanctions, "gov-opensanctions-sanctions", "OpenSanctions — Sanctions Collection", "OpenSanctions 制裁コレクション",
  "government", "sanctions",
  "https://data.opensanctions.org/datasets/latest/sanctions/index.json",
  "resources",
  "en", "[\"sanctions\",\"global\",\"watchlist\"]", 86400,
  "Index of every sanctions regime OpenSanctions aggregates, across 93 national and supranational lists.");

VJSON(gov_opensanctions_un_sc, "gov-opensanctions-un-sc", "OpenSanctions — UN Security Council List", "OpenSanctions 国連安保理制裁リスト",
  "government", "sanctions",
  "https://data.opensanctions.org/datasets/latest/un_sc_sanctions/index.json",
  "resources",
  "en", "[\"sanctions\",\"un\",\"securitycouncil\"]", 86400,
  "OpenSanctions build of the UN Security Council consolidated sanctions list.");

VJSON(gov_opensanctions_us_ofac, "gov-opensanctions-us-ofac", "OpenSanctions — OFAC SDN Mirror", "OpenSanctions OFAC SDN 版",
  "government", "sanctions",
  "https://data.opensanctions.org/datasets/latest/us_ofac_sdn/index.json",
  "resources",
  "en", "[\"sanctions\",\"ofac\",\"usa\"]", 86400,
  "OpenSanctions build of the US OFAC SDN list, with normalised entities and download resources.");

VJSON(gov_opensanctions_wb_debarred, "gov-opensanctions-wb-debarred", "OpenSanctions — World Bank Debarred Firms", "OpenSanctions 世銀入札排除企業",
  "government", "procurement",
  "https://data.opensanctions.org/datasets/latest/worldbank_debarred/index.json",
  "resources",
  "en", "[\"debarment\",\"worldbank\",\"procurement\"]", 86400,
  "World Bank debarred and cross-debarred firms and individuals, normalised by OpenSanctions.");

VJSON(gov_opensanctions_wd_peps, "gov-opensanctions-wd-peps", "OpenSanctions — Wikidata PEPs", "OpenSanctions Wikidata由来PEP",
  "government", "transparency",
  "https://data.opensanctions.org/datasets/latest/wd_peps/index.json",
  "resources",
  "en", "[\"pep\",\"wikidata\",\"global\"]", 86400,
  "Politically exposed persons derived from Wikidata office-holder statements worldwide.");

VRSS(gov_osha_news, "gov-osha-news", "OSHA — News Releases", "米国OSHA 報道発表",
  "government", "regulator",
  "https://www.osha.gov/news/newsreleases.xml",
  "en", "[\"usa\",\"osha\",\"workplace\",\"safety\"]", 3600,
  "Occupational Safety and Health Administration citations, penalties and safety announcements.");

VJSON(gov_pa_datos_ckan, "gov-pa-datos-ckan", "Datos Abiertos Panamá", "パナマ公開データ",
  "government", "transparency",
  "https://www.datosabiertos.gob.pa/api/3/action/package_search?rows=50&sort=id%20asc",
  "result.results",
  "es", "[\"panama\",\"opendata\",\"catalogue\",\"latam\"]", 86400,
  "Newly published datasets on the Panamanian national open data portal.");

VJSON(gov_pt_dados_datasets, "gov-pt-dados-datasets", "dados.gov.pt — Portuguese Open Data", "dados.gov.pt ポルトガル公開データ",
  "government", "transparency",
  "https://dados.gov.pt/api/1/datasets/?page_size=50",
  "data",
  "pt", "[\"portugal\",\"opendata\",\"catalogue\"]", 86400,
  "Newly published datasets on the Portuguese national open data portal.");

VJSON(gov_ro_data, "gov-ro-data", "data.gov.ro — Romanian Open Data", "data.gov.ro ルーマニア公開データ",
  "government", "transparency",
  "https://data.gov.ro/api/3/action/package_search?rows=50&sort=id%20asc",
  "result.results",
  "ro", "[\"romania\",\"opendata\",\"catalogue\"]", 86400,
  "Newly published datasets on the Romanian national open data portal.");

VJSON(gov_rs_data, "gov-rs-data", "data.gov.rs — Serbian Open Data", "data.gov.rs セルビア公開データ",
  "government", "transparency",
  "https://data.gov.rs/api/1/datasets/?page_size=50",
  "data",
  "sr", "[\"serbia\",\"opendata\",\"catalogue\",\"balkans\"]", 86400,
  "Newly published datasets on the Serbian national open data portal.");

VJSON(gov_sanctionsmap_eu, "gov-sanctionsmap-eu", "EU Sanctions Map — Regime Data", "EU制裁マップ データ",
  "government", "sanctions",
  "https://www.sanctionsmap.eu/api/v1/data",
  "data.searchtypes",
  "en", "[\"sanctions\",\"eu\",\"regimes\"]", 86400,
  "Machine-readable EU Sanctions Map: every EU restrictive-measure regime, legal act and target country.");

VJSON(gov_sg_datasets, "gov-sg-datasets", "data.gov.sg — Singapore Datasets", "data.gov.sg シンガポール データセット",
  "government", "transparency",
  "https://api-production.data.gov.sg/v2/public/api/datasets?page=1",
  "data.datasets",
  "en", "[\"singapore\",\"opendata\",\"catalogue\",\"asia\"]", 86400,
  "Datasets published on the Singapore government open data platform.");

VJSON(gov_si_data, "gov-si-data", "podatki.gov.si — Slovenian Open Data", "podatki.gov.si スロベニア公開データ",
  "government", "transparency",
  "https://podatki.gov.si/api/3/action/package_search?rows=50&sort=id%20asc",
  "result.results",
  "sl", "[\"slovenia\",\"opendata\",\"catalogue\"]", 86400,
  "Newly published datasets on the Slovenian national open data portal.");

VJSON(gov_socrata_data_ny, "gov-socrata-data-ny", "Open Data NY — Dataset Catalogue", "ニューヨーク州 公開データ目録",
  "government", "transparency",
  "https://api.us.socrata.com/api/catalog/v1?domains=data.ny.gov&limit=50",
  "results",
  "en", "[\"usa\",\"newyork\",\"opendata\",\"catalogue\"]", 86400,
  "Catalogue of New York State open datasets including lobbying, licensing and enforcement data.");

VJSON(gov_socrata_data_tx, "gov-socrata-data-tx", "Open Data Texas — Dataset Catalogue", "テキサス州 公開データ目録",
  "government", "transparency",
  "https://api.us.socrata.com/api/catalog/v1?domains=data.texas.gov&limit=50",
  "results",
  "en", "[\"usa\",\"texas\",\"opendata\",\"catalogue\"]", 86400,
  "Catalogue of Texas state open datasets across agencies and regulators.");

VJSON(gov_socrata_data_wa, "gov-socrata-data-wa", "Open Data Washington — Dataset Catalogue", "ワシントン州 公開データ目録",
  "government", "transparency",
  "https://api.us.socrata.com/api/catalog/v1?domains=data.wa.gov&limit=50",
  "results",
  "en", "[\"usa\",\"washington\",\"opendata\",\"catalogue\"]", 86400,
  "Catalogue of Washington State open datasets across agencies and regulators.");

VJSON(gov_ua_data_ckan, "gov-ua-data-ckan", "data.gov.ua — Ukrainian Open Data", "data.gov.ua ウクライナ公開データ",
  "government", "transparency",
  "https://data.gov.ua/api/3/action/package_search?rows=50&sort=id%20asc",
  "result.results",
  "uk", "[\"ukraine\",\"opendata\",\"catalogue\"]", 86400,
  "Newly published datasets on the Ukrainian national open data portal.");

VJSON(gov_ua_prozorro, "gov-ua-prozorro", "ProZorro — Ukrainian Public Procurement", "ProZorro ウクライナ公共調達",
  "government", "procurement",
  "https://public.api.openprocurement.org/api/2.5/tenders",
  "data",
  "uk", "[\"ukraine\",\"procurement\",\"tenders\",\"ocds\"]", 1800,
  "Live feed of every tender published on Ukraine's ProZorro e-procurement system.");

VRSS(gov_uk_hse, "gov-uk-hse", "Health and Safety Executive — Press", "英国安全衛生庁 報道発表",
  "government", "regulator",
  "https://press.hse.gov.uk/feed/",
  "en", "[\"uk\",\"hse\",\"workplace\",\"prosecutions\"]", 3600,
  "UK Health and Safety Executive prosecutions, fines and safety alerts.");

VJSON(gov_un_comtrade_reporters, "gov-un-comtrade-reporters", "UN Comtrade — Reporter Registry", "国連コムトレード 報告国一覧",
  "government", "statistics",
  "https://comtradeapi.un.org/files/v1/app/reference/Reporters.json",
  "results",
  "en", "[\"un\",\"trade\",\"comtrade\",\"reference\"]", 604800,
  "Reference list of all reporting countries and territories in UN Comtrade trade statistics.");

VRSS(gov_un_news_ar, "gov-un-news-ar", "UN News (Arabic)", "国連ニュース（アラビア語）",
  "government", "transparency",
  "https://news.un.org/feed/subscribe/ar/news/all/rss.xml",
  "ar", "[\"un\",\"news\",\"multilateral\"]", 3600,
  "United Nations news service in Arabic across all UN bodies and missions.");

VRSS(gov_un_news_es, "gov-un-news-es", "UN News (Spanish)", "国連ニュース（西語）",
  "government", "transparency",
  "https://news.un.org/feed/subscribe/es/news/all/rss.xml",
  "es", "[\"un\",\"news\",\"multilateral\"]", 3600,
  "United Nations news service in Spanish across all UN bodies and missions.");

VRSS(gov_un_news_fr, "gov-un-news-fr", "UN News (French)", "国連ニュース（仏語）",
  "government", "transparency",
  "https://news.un.org/feed/subscribe/fr/news/all/rss.xml",
  "fr", "[\"un\",\"news\",\"multilateral\"]", 3600,
  "United Nations news service in French across all UN bodies and missions.");

VRSS(gov_un_news_ru, "gov-un-news-ru", "UN News (Russian)", "国連ニュース（露語）",
  "government", "transparency",
  "https://news.un.org/feed/subscribe/ru/news/all/rss.xml",
  "ru", "[\"un\",\"news\",\"multilateral\"]", 3600,
  "United Nations news service in Russian across all UN bodies and missions.");

VRSS(gov_un_news_zh, "gov-un-news-zh", "UN News (Chinese)", "国連ニュース（中国語）",
  "government", "transparency",
  "https://news.un.org/feed/subscribe/zh/news/all/rss.xml",
  "zh", "[\"un\",\"news\",\"multilateral\"]", 3600,
  "United Nations news service in Chinese across all UN bodies and missions.");

VRSS(gov_unep_news, "gov-unep-news", "UN Environment Programme — News", "国連環境計画 ニュース",
  "government", "regulator",
  "https://www.unep.org/rss.xml",
  "en", "[\"un\",\"environment\",\"unep\"]", 3600,
  "UN Environment Programme announcements, treaty negotiations and assessments.");

VJSON(gov_unesco_uis_indicators, "gov-unesco-uis-indicators", "UNESCO UIS — Indicator Definitions", "ユネスコ統計研究所 指標定義",
  "government", "statistics",
  "https://api.uis.unesco.org/api/public/definitions/indicators",
  "",
  "en", "[\"un\",\"unesco\",\"education\",\"statistics\"]", 604800,
  "Over 5,000 UNESCO Institute for Statistics indicator definitions covering education, science and culture.");

VJSON(gov_unhcr_population, "gov-unhcr-population", "UNHCR — Refugee Population Statistics", "UNHCR 難民人口統計",
  "government", "statistics",
  "https://api.unhcr.org/population/v1/population/?limit=50&per_page=50&page=1&yearFrom=2023",
  "items",
  "en", "[\"un\",\"unhcr\",\"refugees\",\"statistics\"]", 86400,
  "Refugee, asylum-seeker and IDP population figures by country of origin and asylum.");

VJSON(gov_unsd_sdg_indicators, "gov-unsd-sdg-indicators", "UN SDG — Indicator Registry", "国連SDG 指標一覧",
  "government", "statistics",
  "https://unstats.un.org/SDGAPI/v1/sdg/Indicator/List",
  "",
  "en", "[\"un\",\"sdg\",\"indicators\",\"statistics\"]", 604800,
  "Full registry of the 251 UN Sustainable Development Goal indicators with series and targets.");

VJSON(gov_usaspending_agencies, "gov-usaspending-agencies", "USAspending — Federal Agency Budgets", "USAspending 連邦機関予算",
  "government", "transparency",
  "https://api.usaspending.gov/api/v2/references/toptier_agencies/",
  "results",
  "en", "[\"usa\",\"spending\",\"budget\",\"transparency\"]", 86400,
  "Budgetary resources and obligations for every top-tier US federal agency.");

VJSON(gov_usaspending_budgetfunctions, "gov-usaspending-budgetfunctions", "USAspending — Budget Functions", "USAspending 予算機能分類",
  "government", "transparency",
  "https://api.usaspending.gov/api/v2/budget_functions/list_budget_functions/",
  "results",
  "en", "[\"usa\",\"spending\",\"budget\"]", 86400,
  "Budget function and sub-function taxonomy used to classify all US federal spending.");

VJSON(gov_usaspending_defcodes, "gov-usaspending-defcodes", "USAspending — Disaster and Emergency Codes", "USAspending 災害・緊急支出コード",
  "government", "transparency",
  "https://api.usaspending.gov/api/v2/references/def_codes/",
  "codes",
  "en", "[\"usa\",\"spending\",\"disaster\",\"transparency\"]", 86400,
  "Disaster Emergency Fund codes used to tag US emergency and supplemental spending.");

VJSON(gov_uy_datos_ckan, "gov-uy-datos-ckan", "Catálogo de Datos Uruguay", "ウルグアイ公開データ目録",
  "government", "transparency",
  "https://catalogodatos.gub.uy/api/3/action/package_search?rows=50&sort=id%20asc",
  "result.results",
  "es", "[\"uruguay\",\"opendata\",\"catalogue\",\"latam\"]", 86400,
  "Newly published datasets on the Uruguayan national open data catalogue.");

VRSS(gov_wipo_news, "gov-wipo-news", "WIPO — Press Room", "世界知的所有権機関 報道発表",
  "government", "regulator",
  "https://www.wipo.int/pressroom/en/rss.xml",
  "en", "[\"wipo\",\"ip\",\"patents\",\"trademarks\"]", 86400,
  "World Intellectual Property Organization press releases and treaty developments.");

VJSON(gov_worldbank_indicators, "gov-worldbank-indicators", "World Bank — Indicator Catalogue", "世界銀行 指標カタログ",
  "government", "statistics",
  "https://api.worldbank.org/v2/indicator?format=json&per_page=100",
  "",
  "en", "[\"worldbank\",\"indicators\",\"statistics\"]", 604800,
  "Catalogue of World Bank development indicators with source, topic and definition.");

/* World Bank procurement notices, walked on `os`.
 *
 * search.worldbank.org pages with `os` (a record offset) and IGNORES `start`,
 * which is the only cursor the generic walk moves beside `rows`. Measured
 * 2026-09-15: `rows=50&start=50` re-served page 1 byte-for-byte, the walk stopped
 * on the repeat, and 50 of 418,781 notices were stored on every run with no
 * disclosure (the repeat guard files none). `os` advances: rows=500 at
 * os=0/500/1000 gave 1,500 records and 1,500 distinct ids, identical with and
 * without an explicit sort, and rows=1000 is honoured. The walk stops at the
 * same page ceiling as the generic engine and discloses the remainder. */
#include "lib/jocore.h"
static int run_gov_worldbank_procnotices(const source_ctx *c, intel_sink *s) {
  static const char *const ID = "gov-worldbank-procnotices";
  const int rows = 1000;
  int page_max = 20; /* exhaustive-ok: page-walk ceiling shared with lib/jsonlist.c; a stop is disclosed below and $JO_JSONLIST_PAGE_MAX raises it */
  const char *env = getenv("JO_JSONLIST_PAGE_MAX");
  if (env && *env && atoi(env) > 0) page_max = atoi(env);

  long total = 0, available = -1;
  int pages = 0, more = 0;
  for (long os = 0;; os += rows) {
    if (pages >= page_max) { more = 1; break; }
    char url[192];
    snprintf(url, sizeof url,
             "https://search.worldbank.org/api/v2/procnotices?format=json&rows=%d&os=%ld",
             rows, os);
    cJSON *doc = feed_get_json(c->http, url, 90000);
    if (!doc) {
      if (pages == 0) { fprintf(stderr, "[%s] fetch failed\n", ID); return -1; }
      more = 1;
      break;
    }
    pages++;
    const cJSON *t = cJSON_GetObjectItemCaseSensitive(doc, "total");
    if (cJSON_IsNumber(t)) available = (long)t->valuedouble;
    else if (cJSON_IsString(t) && t->valuestring) available = atol(t->valuestring);
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(doc, "procnotices");
    int got = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
    total += jsonlist_emit(s, ID, doc, "procnotices", "procurement", "en",
                           "[\"worldbank\",\"procurement\",\"tenders\",\"global\"]");
    cJSON_Delete(doc);
    if (got < rows) break;
  }
  if (more || (available >= 0 && available > total)) {
    cJSON *extra = cJSON_CreateObject();
    if (extra) {
      cJSON_AddNumberToObject(extra, "pages_read", pages);
      cJSON_AddNumberToObject(extra, "page_ceiling", page_max);
      cJSON_AddNumberToObject(extra, "page_size", rows);
    }
    jo_truncation_notice_ex(s, ID, NULL, total, available,
      "the os-offset walk stopped at the page ceiling (or a later page failed) "
      "while the upstream still declared more notices",
      "raise $JO_JSONLIST_PAGE_MAX — see docs/SOURCE_EXHAUSTIVENESS.md", extra);
  }
  fprintf(stderr, "[%s] emitted %ld across %d page(s) of %ld declared\n", ID,
          total, pages, available);
  return 0;
}
static const source_def gov_worldbank_procnotices = {
  .id = "gov-worldbank-procnotices", .collector = "government",
  .name = "World Bank — Procurement Notices", .name_ja = "世界銀行 調達公告",
  .update_interval_sec = 21600, .run = run_gov_worldbank_procnotices,
  .category = "procurement", .type = "api",
  .url = "https://search.worldbank.org/api/v2/procnotices?format=json&rows=1000&os=0",
  .description = "Live World Bank procurement notices: bids, expressions of interest and contract awards worldwide.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(gov_worldbank_procnotices)

VJSON(gov_worldbank_projects, "gov-worldbank-projects", "World Bank — Project Operations", "世界銀行 プロジェクト一覧",
  "government", "transparency",
  "https://search.worldbank.org/api/v2/projects?format=json&rows=50",
  ".",
  "en", "[\"worldbank\",\"projects\",\"development\",\"finance\"]", 86400,
  "World Bank lending operations with country, sector, commitment amount and approval status.");

VJSON(gov_worldbank_sources, "gov-worldbank-sources", "World Bank — Data Source Registry", "世界銀行 データソース一覧",
  "government", "statistics",
  "https://api.worldbank.org/v2/sources?format=json&per_page=100",
  "",
  "en", "[\"worldbank\",\"statistics\",\"reference\"]", 604800,
  "Registry of World Bank data sources and databanks feeding the open data API.");

VJSON(gov_worldbank_wds, "gov-worldbank-wds", "World Bank — Documents and Reports", "世界銀行 文書・報告書",
  "government", "transparency",
  "https://search.worldbank.org/api/v2/wds?format=json&rows=50",
  ".",
  "en", "[\"worldbank\",\"documents\",\"development\"]", 86400,
  "World Bank Documents & Reports archive: country reports, appraisals and evaluations.");

VRSS(gov_wto_news, "gov-wto-news", "WTO — Latest News", "世界貿易機関 最新ニュース",
  "government", "regulator",
  "https://www.wto.org/library/rss/latest_news_e.xml",
  "en", "[\"wto\",\"trade\",\"disputes\"]", 3600,
  "World Trade Organization news: disputes, accessions, notifications and council meetings.");

VRSS(gov_za_gov_news, "gov-za-gov-news", "South African Government — News", "南アフリカ政府 ニュース",
  "government", "transparency",
  "https://www.gov.za/rss.xml",
  "en", "[\"southafrica\",\"africa\",\"government\",\"press\"]", 3600,
  "Official South African government statements, cabinet decisions and departmental news.");
