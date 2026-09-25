/* Verified-live economy sources (32), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VRSS(eco_afdb_news, "eco-afdb-news", "African Development Bank news", "アフリカ開発銀行 ニュース",
  "economy", "statistics",
  "https://www.afdb.org/en/rss.xml",
  "en", "[\"africa\",\"development\",\"finance\",\"projects\"]", 3600,
  "African Development Bank project approvals and economic outlook releases.");

VJSON(eco_ar_series_cpi, "eco-ar-series-cpi", "Argentina national CPI series", "アルゼンチン 全国消費者物価指数",
  "economy", "statistics",
  "https://apis.datos.gob.ar/series/api/series/?ids=148.3_INIVELNAL_DICI_M_26&format=json&limit=100",
  "*",
  "es", "[\"argentina\",\"inflation\",\"cpi\",\"indec\"]", 86400,
  "Argentine national CPI time series from the government open-data series API.");

/* eco-cbs-nl-*: CBS OData TypedDataSet is JSON ({"odata.metadata", "value":[{ID,
 * …}]}), but both rows were registered on the RSS macro, which parsed it as a
 * feed and emitted 0 on every run (live 2026-09-15). Now JSON rows on `value`,
 * keyed on the table's own ID. The tables are 23,095 rows (83625NED) and
 * 159,120 (83131ENG) per TypedDataSet/$count, and the /ODataApi/ path cannot
 * page at all: `$skip` answers HTTP 500 "The 'Skip' query option is not
 * supported on the ODataApi. This functionality is available only on the
 * ODataFeed endpoint." So the rows read the same table from CBS's /ODataFeed/
 * path, which serves 10,000 rows per page and honours $top/$skip (skip=10000
 * returned IDs from 10000 on) — the URL declares both for the page walk. */
#include "_vjson_shapes.inc"

VJSON_PREP(eco_cbs_nl_catalog, "eco-cbs-nl-catalog", "Statistics Netherlands producer confidence", "オランダ統計局 生産者景況感",
  "economy", "statistics",
  "https://opendata.cbs.nl/ODataFeed/odata/83625NED/TypedDataSet?$format=json&$top=10000&$skip=0",
  "value",
  "nl", "[\"netherlands\",\"cbs\",\"business\",\"sentiment\"]", 86400,
  "Dutch business and producer confidence indicators.",
  "ID", NULL);

VJSON_PREP(eco_cbs_nl_cpi, "eco-cbs-nl-cpi", "Statistics Netherlands CPI", "オランダ統計局 消費者物価指数",
  "economy", "statistics",
  "https://opendata.cbs.nl/ODataFeed/odata/83131ENG/TypedDataSet?$format=json&$top=10000&$skip=0",
  "value",
  "en", "[\"netherlands\",\"cbs\",\"inflation\",\"cpi\"]", 86400,
  "Dutch consumer price index series from Statistics Netherlands.",
  "ID", NULL);

VJSON(eco_dk_statbank_tables, "eco-dk-statbank-tables", "Statistics Denmark labour tables", "デンマーク統計局 労働統計表",
  "economy", "statistics",
  "https://api.statbank.dk/v1/tables?format=JSON&subjects=2",
  "*",
  "en", "[\"denmark\",\"statistics\",\"labour\",\"tables\"]", 86400,
  "Statistics Denmark labour and income table registry with last-update timestamps.");

VJSON(eco_es_ine_ipc, "eco-es-ine-ipc", "Spain INE consumer price index", "スペイン統計局 消費者物価指数",
  "economy", "statistics",
  "https://servicios.ine.es/wstempus/js/EN/DATOS_TABLA/50902?nult=6",
  "*",
  "en", "[\"spain\",\"ine\",\"inflation\",\"cpi\"]", 86400,
  "Spanish national statistics institute CPI series, latest six periods.");

VRSS(eco_eurostat_catalogue_rss, "eco-eurostat-catalogue-rss", "Eurostat statistics updates", "ユーロスタット 統計更新",
  "economy", "statistics",
  "https://ec.europa.eu/eurostat/api/dissemination/catalogue/rss/en/statistics-update.rss",
  "en", "[\"eu\",\"eurostat\",\"statistics\",\"release\"]", 86400,
  "Eurostat's feed of every dataset updated, the fastest notice of new EU statistics.");

VJSON(eco_eurostat_gdp, "eco-eurostat-gdp", "Eurostat quarterly GDP", "ユーロスタット 四半期GDP",
  "economy", "statistics",
  "https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data/namq_10_gdp?format=JSON&lang=EN&lastTimePeriod=1",
  "*",
  "en", "[\"eu\",\"eurostat\",\"gdp\",\"national-accounts\"]", 86400,
  "Latest quarterly GDP and national accounts aggregates for the EU.");

VJSON(eco_eurostat_hicp, "eco-eurostat-hicp", "Eurostat HICP annual inflation", "ユーロスタット HICP年間インフレ率",
  "economy", "statistics",
  "https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data/prc_hicp_manr?format=JSON&lang=EN&lastTimePeriod=1",
  "*",
  "en", "[\"eu\",\"eurostat\",\"inflation\",\"hicp\"]", 86400,
  "Latest harmonised consumer price inflation for every EU member state.");

/* eco-eurostat-*: the dissemination API answers JSON-stat 2.0 — `value` is a
 * sparse map keyed by the linear index into the `id` / `size` dimension cube,
 * and every dimension's category index and label sit beside it. The VJSON rows
 * pointed "*" at that document, so the auto-detect emitted the only arrays of
 * objects it could find — extension annotations and positions-with-no-data
 * lists (19 and 17 rows, several colliding) — and not one observation (live
 * 2026-09-15). od_jsonstat_collect (od_shared.inc, the SCB / StatBank / CSO
 * reader) decodes every non-null observation against the cube with its
 * dimension labels. A guard far above the flow size is disclosed if reached. */
#include "od_shared.inc"
#include "lib/jocore.h"
#define EUROSTAT_JS_MAX_ROWS 1000000 /* exhaustive-ok: runaway-document guard far above either flow (54 / 12,597 obs); reaching it files a truncation notice */
static int eurostat_jsonstat_run(const source_ctx *c, intel_sink *s, const char *id,
                                 const char *url, const char *tags) {
  int n = od_jsonstat_collect(c, s, url, "statistics", tags, url, EUROSTAT_JS_MAX_ROWS);
  if (n >= EUROSTAT_JS_MAX_ROWS)
    jo_truncation_notice_ex(s, id, NULL, n, -1,
      "the JSON-stat reader stopped at its per-run observation guard",
      "raise EUROSTAT_JS_MAX_ROWS in collectors/feed/generated/vsrc_economy_1.c", NULL);
  return od_rc(id, n);
}

#define EUROSTAT_INPR_URL \
  "https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data/sts_inpr_m?format=JSON&lang=EN&lastTimePeriod=1"
static int run_eco_eurostat_industrial(const source_ctx *c, intel_sink *s) {
  return eurostat_jsonstat_run(c, s, "eco-eurostat-industrial", EUROSTAT_INPR_URL,
                               "[\"eu\",\"eurostat\",\"industry\",\"production\"]");
}
static const source_def eco_eurostat_industrial = {
  .id = "eco-eurostat-industrial", .collector = "economy",
  .name = "Eurostat industrial production", .name_ja = "ユーロスタット 鉱工業生産",
  .update_interval_sec = 86400, .run = run_eco_eurostat_industrial,
  .category = "statistics", .type = "api", .url = EUROSTAT_INPR_URL,
  .description = "Latest monthly EU industrial production index by country.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(eco_eurostat_industrial)

VJSON(eco_eurostat_trade, "eco-eurostat-trade", "Eurostat external trade by SITC", "ユーロスタット SITC別対外貿易",
  "economy", "trade",
  "https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data/ext_st_eu27_2020sitc?format=JSON&lang=EN&lastTimePeriod=1",
  "*",
  "en", "[\"eu\",\"eurostat\",\"trade\",\"exports\"]", 86400,
  "Latest EU external trade values by SITC product group.");

#define EUROSTAT_UNE_URL \
  "https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data/une_rt_m?format=JSON&lang=EN&lastTimePeriod=1"
static int run_eco_eurostat_unemployment(const source_ctx *c, intel_sink *s) {
  return eurostat_jsonstat_run(c, s, "eco-eurostat-unemployment", EUROSTAT_UNE_URL,
                               "[\"eu\",\"eurostat\",\"unemployment\",\"labour\"]");
}
static const source_def eco_eurostat_unemployment = {
  .id = "eco-eurostat-unemployment", .collector = "economy",
  .name = "Eurostat monthly unemployment", .name_ja = "ユーロスタット 月次失業率",
  .update_interval_sec = 86400, .run = run_eco_eurostat_unemployment,
  .category = "statistics", .type = "api", .url = EUROSTAT_UNE_URL,
  .description = "Latest monthly unemployment rates across the EU.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(eco_eurostat_unemployment)

VJSON(eco_gleif_lei, "eco-gleif-lei", "GLEIF legal entity identifiers", "GLEIF 取引主体識別子",
  "economy", "disclosure",
  "https://api.gleif.org/api/v1/lei-records?page%5Bsize%5D=50",
  "*",
  "en", "[\"corporate\",\"lei\",\"registry\",\"ownership\"]", 86400,
  "Global LEI registry records with legal name, jurisdiction and parent relationships.");

VJSON(eco_hdx_food_prices, "eco-hdx-food-prices", "HDX food price datasets", "HDX 食料価格データセット",
  "economy", "commodities",
  "https://data.humdata.org/api/3/action/package_search?q=food+prices&rows=50&sort=id%20asc",
  "*",
  "en", "[\"hdx\",\"food-prices\",\"wfp\",\"humanitarian\"]", 86400,
  "Humanitarian Data Exchange food price datasets, largely WFP market monitoring by country.");

VJSON(eco_ibge_noticias, "eco-ibge-noticias", "Brazil IBGE statistical releases", "ブラジル地理統計院 統計公表",
  "economy", "statistics",
  "https://servicodados.ibge.gov.br/api/v3/noticias/?qtd=30",
  "*",
  "pt", "[\"brazil\",\"ibge\",\"statistics\",\"inflation\"]", 86400,
  "Brazilian statistics institute release announcements covering IPCA, GDP and employment.");

VJSON(eco_imf_datamapper_ngdpd, "eco-imf-datamapper-ngdpd", "IMF GDP current prices", "IMF 名目GDP",
  "economy", "statistics",
  "https://www.imf.org/external/datamapper/api/v1/NGDPD",
  "*",
  "en", "[\"imf\",\"gdp\",\"weo\",\"macro\"]", 86400,
  "IMF World Economic Outlook nominal GDP in USD for every country and forecast year.");

VJSON(eco_imf_datamapper_pcpipch, "eco-imf-datamapper-pcpipch", "IMF inflation rate", "IMF インフレ率",
  "economy", "statistics",
  "https://www.imf.org/external/datamapper/api/v1/PCPIPCH",
  "*",
  "en", "[\"imf\",\"inflation\",\"cpi\",\"weo\"]", 86400,
  "IMF World Economic Outlook consumer price inflation by country.");

VJSON(eco_imf_dm_bca, "eco-imf-dm-bca", "IMF current account balance", "IMF 経常収支",
  "economy", "statistics",
  "https://www.imf.org/external/datamapper/api/v1/BCA_NGDPD",
  "*",
  "en", "[\"imf\",\"current-account\",\"external\",\"weo\"]", 86400,
  "IMF current account balance as a share of GDP by country.");

VJSON(eco_imf_dm_ggxwdg, "eco-imf-dm-ggxwdg", "IMF general government debt", "IMF 一般政府債務",
  "economy", "statistics",
  "https://www.imf.org/external/datamapper/api/v1/GGXWDG_NGDP",
  "*",
  "en", "[\"imf\",\"debt\",\"fiscal\",\"weo\"]", 86400,
  "IMF general government gross debt as a share of GDP by country.");

VJSON(eco_imf_dm_lp, "eco-imf-dm-lp", "IMF population", "IMF 人口",
  "economy", "statistics",
  "https://www.imf.org/external/datamapper/api/v1/LP",
  "*",
  "en", "[\"imf\",\"population\",\"weo\",\"demographics\"]", 86400,
  "IMF World Economic Outlook population series by country.");

VJSON(eco_imf_dm_lur, "eco-imf-dm-lur", "IMF unemployment rate", "IMF 失業率",
  "economy", "statistics",
  "https://www.imf.org/external/datamapper/api/v1/LUR",
  "*",
  "en", "[\"imf\",\"unemployment\",\"labour\",\"weo\"]", 86400,
  "IMF World Economic Outlook unemployment rate by country.");

VJSON(eco_my_cpi, "eco-my-cpi", "Malaysia headline CPI", "マレーシア 消費者物価指数",
  "economy", "statistics",
  "https://api.data.gov.my/data-catalogue?id=cpi_headline&limit=20",
  "*",
  "en", "[\"malaysia\",\"inflation\",\"cpi\",\"opendosm\"]", 86400,
  "Malaysian headline consumer price index from the official open data API.");

VRSS(eco_uk_companies_house_news, "eco-uk-companies-house-news", "UK Companies House announcements", "英国会社登記所 発表",
  "economy", "disclosure",
  "https://www.gov.uk/government/organisations/companies-house.atom",
  "en", "[\"uk\",\"companies\",\"registry\",\"filings\"]", 3600,
  "Companies House service and policy announcements affecting UK company filings.");

VRSS(eco_uk_dbt_atom, "eco-uk-dbt-atom", "UK Department for Business and Trade", "英国ビジネス・貿易省",
  "economy", "trade",
  "https://www.gov.uk/government/organisations/department-for-business-and-trade.atom",
  "en", "[\"uk\",\"trade\",\"business\",\"policy\"]", 3600,
  "UK trade and business ministry announcements including sanctions and trade deals.");

VRSS(eco_uk_defra_atom, "eco-uk-defra-atom", "UK Defra announcements", "英国環境・食料・農村地域省",
  "economy", "agriculture",
  "https://www.gov.uk/government/organisations/department-for-environment-food-rural-affairs.atom",
  "en", "[\"uk\",\"defra\",\"agriculture\",\"animal-health\"]", 3600,
  "Defra announcements covering agriculture, animal disease and food supply.");

VRSS(eco_uk_hmrc_atom, "eco-uk-hmrc-atom", "UK HM Revenue and Customs", "英国歳入関税庁",
  "economy", "trade",
  "https://www.gov.uk/government/organisations/hm-revenue-customs.atom",
  "en", "[\"uk\",\"customs\",\"tax\",\"trade-statistics\"]", 3600,
  "HMRC announcements including customs procedure and trade statistics changes.");

VRSS(eco_uk_insolvency_news, "eco-uk-insolvency-news", "UK Insolvency Service", "英国倒産庁",
  "economy", "disclosure",
  "https://www.gov.uk/government/organisations/insolvency-service.atom",
  "en", "[\"uk\",\"insolvency\",\"bankruptcy\",\"enforcement\"]", 3600,
  "UK Insolvency Service enforcement actions, disqualifications and statistics.");

VRSS(eco_uk_ons_atom, "eco-uk-ons-atom", "UK Office for National Statistics", "英国国家統計局",
  "economy", "statistics",
  "https://www.gov.uk/government/organisations/office-for-national-statistics.atom",
  "en", "[\"uk\",\"ons\",\"statistics\",\"inflation\"]", 3600,
  "ONS statistical release announcements and methodology notes.");

VRSS(eco_uk_statistics_atom, "eco-uk-statistics-atom", "UK government research and statistics", "英国政府 調査・統計",
  "economy", "statistics",
  "https://www.gov.uk/search/research-and-statistics.atom",
  "en", "[\"uk\",\"statistics\",\"release\",\"government\"]", 3600,
  "All UK government statistical releases as they publish.");

VRSS(eco_us_dol_news, "eco-us-dol-news", "US Department of Labor releases", "米国労働省 発表",
  "economy", "statistics",
  "https://www.dol.gov/rss/releases.xml",
  "en", "[\"usa\",\"labour\",\"employment\",\"regulation\"]", 3600,
  "US Labor Department news releases including enforcement and labour market actions.");

/* World Bank projects, walked on `os`.
 *
 * /api/v3/projects answers {rows, os, page, total, projects:{"P518248":{id,
 * proj_id, countryshortname, boardapprovaldate, totalamt, …}, …}} — `projects`
 * is a MAP keyed by project id, not an array, so the "*" auto-detect found no
 * array of objects and 0 were emitted (live 2026-09-15). Each member becomes a
 * record (it already carries its own `id`). Like the procnotices API on the
 * same host, it pages by `os` and ignores `start` (start=1000 re-served os=0
 * exactly); rows=1000 is honoured and os=0 / os=1000 share 0 of 1,000 ids. The
 * walk stops at the shared page ceiling and discloses the remainder of `total`
 * (28,113). */
#include "_vjson_shapes.inc"
#include "lib/jocore.h"
static int run_eco_worldbank_projects(const source_ctx *c, intel_sink *s) {
  static const char *const ID = "eco-worldbank-projects";
  const int rows = 1000;
  int page_max = 20; /* exhaustive-ok: page-walk ceiling shared with lib/jsonlist.c; a stop is disclosed below and $JO_JSONLIST_PAGE_MAX raises it */
  const char *env = getenv("JO_JSONLIST_PAGE_MAX");
  if (env && *env && atoi(env) > 0) page_max = atoi(env);

  long total = 0, available = -1;
  int pages = 0, more = 0;
  for (long os = 0;; os += rows) {
    if (pages >= page_max) { more = 1; break; }
    char url[160];
    snprintf(url, sizeof url,
             "https://search.worldbank.org/api/v3/projects?format=json&rows=%d&os=%ld",
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
    const cJSON *pm = cJSON_GetObjectItemCaseSensitive(doc, "projects");
    int got = cJSON_IsObject(pm) ? cJSON_GetArraySize(pm) : 0;
    vshape_map(doc, "projects", "id");
    total += jsonlist_emit(s, ID, doc, "records", "statistics", "en",
                           "[\"worldbank\",\"projects\",\"lending\",\"development\"]");
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
      "while the upstream still declared more projects",
      "raise $JO_JSONLIST_PAGE_MAX — see docs/SOURCE_EXHAUSTIVENESS.md", extra);
  }
  fprintf(stderr, "[%s] emitted %ld across %d page(s) of %ld declared\n", ID,
          total, pages, available);
  return 0;
}
static const source_def eco_worldbank_projects = {
  .id = "eco-worldbank-projects", .collector = "economy",
  .name = "World Bank project database", .name_ja = "世界銀行 プロジェクトデータベース",
  .update_interval_sec = 86400, .run = run_eco_worldbank_projects,
  .category = "statistics", .type = "api",
  .url = "https://search.worldbank.org/api/v3/projects?format=json&rows=1000&os=0",
  .description = "World Bank lending projects with borrower, sector, amount and approval date.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(eco_worldbank_projects)

VRSS(eco_wto_news_rss, "eco-wto-news-rss", "WTO latest news", "WTO 最新ニュース",
  "economy", "trade",
  "https://www.wto.org/library/rss/latest_news_e.xml",
  "en", "[\"wto\",\"trade\",\"disputes\",\"tariffs\"]", 3600,
  "World Trade Organization news on disputes, tariffs and trade policy reviews.");
