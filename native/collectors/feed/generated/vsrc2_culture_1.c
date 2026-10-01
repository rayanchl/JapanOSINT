/* Verified-live culture sources (12), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* jpx-jpsearch-*, 2026-09-15: the walk the note below asks for, done in these
 * rows. Two measured facts decide it. (1) Japan Search's relevance order is not
 * stable: the same size=50 query fetched twice shares only 48 of 50 ids, and
 * page=2 is IGNORED (48 of its 50 ids are page 1's) — jinja stored 54 of 150.
 * `sort=common.id:asc` is deterministic (50/50 on a repeat; other spellings
 * such as `id:asc` or `title:asc` return hit 0). (2) The offset is `from`
 * (from=50 overlaps from=0 on 2 ids under relevance order, 0 once sorted),
 * which neither page walker advances. jps_walk pages by `from` under that sort
 * to JO_PAGE_MAX pages and discloses the remainder against the upstream's `hit`.
 *
 * Earlier triage, kept for the record: */
#include "lib/jocore.h"
static int jps_walk(const source_ctx *c, intel_sink *s, const char *id,
                    const char *base, const char *tags) {
  int page_max = 20;   /* exhaustive-ok: JO_PAGE_MAX runaway ceiling (pagewalk's default), remainder disclosed as a truncation notice */
  const char *pm = getenv("JO_PAGE_MAX");
  if (pm && atoi(pm) > 0) page_max = atoi(pm);
  long size = jsonlist_query_int(base, "size");
  if (size <= 0) size = 20;
  size_t cap = strlen(base) + 64;
  char *url = malloc(cap);
  if (!url) return -1;
  long hit = -1, from = 0;
  int pages = 0, n = 0, done = 0, failed = 0;
  while (!done && pages < page_max) {   /* exhaustive-ok: JO_PAGE_MAX ceiling, remainder disclosed below */
    snprintf(url, cap, "%s&sort=common.id:asc&from=%ld", base, from);
    cJSON *doc = feed_get_json(c->http, url, 25000);
    cJSON *list = doc ? cJSON_GetObjectItemCaseSensitive(doc, "list") : NULL;
    if (!cJSON_IsArray(list)) {
      cJSON_Delete(doc);
      if (pages == 0) { free(url); fprintf(stderr, "[%s] fetch failed\n", id); return -1; }
      failed = 1;
      break;
    }
    cJSON *h = cJSON_GetObjectItemCaseSensitive(doc, "hit");
    if (cJSON_IsNumber(h)) hit = (long)h->valuedouble;
    int got = cJSON_GetArraySize(list);
    pages++;
    int e = jsonlist_emit_ex(s, id, doc, "list", "heritage", "ja", tags, NULL);
    if (e > 0) n += e;
    cJSON_Delete(doc);
    from += got;
    if (got < size || (hit >= 0 && from >= hit)) done = 1;
  }
  free(url);
  if (!done)
    jo_truncation_notice_ex(s, id, NULL, n, hit,
      failed ? "a later page failed; the walk stopped there"
             : "the JO_PAGE_MAX ceiling stopped the walk while the upstream still had records",
      "raise $JO_PAGE_MAX — see docs/SOURCE_EXHAUSTIVENESS.md", NULL);
  fprintf(stderr, "[%s] emitted %d across %d page(s) of hit=%ld\n", id, n, pages, hit);
  return n;
}

#define VJPS(SYM, ID, NAME, NAMEJA, URL, TAGS, DESC)                          \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                  \
    return jps_walk(c, s, ID, URL, TAGS) < 0 ? -1 : 0; }                      \
  static const source_def SYM = {                                            \
    .id = ID, .collector = "culture", .name = NAME, .name_ja = NAMEJA,        \
    .update_interval_sec = 86400, .run = run_##SYM,                           \
    .category = "heritage", .type = "api", .url = URL,                        \
    .description = DESC, .layer = NULL, .free_tier = 1 };                     \
  REGISTER_SOURCE(SYM)

/* 2026-09-08 COLLISION triage of jpx-jpsearch-cross, jpx-jpsearch-jinja and
 * jpx-jpsearch-kofun (each 100 emitted / 51 stored). NOT a keying defect, and
 * nothing here is changed — an id_keys-style patch would be treating a symptom.
 *
 * Fetched live 2026-09-08 (jps-cross, keyword=防災, size=50): the response is
 * {"facets":…,"from":0,"hit":61084,"list":[…50 items…]} and every item carries
 * a top-level `id` that is globally unique and human-meaningful
 * ("saitama_digi_lib-1498302095", "sagamihara_archives-16327"). All 50 ids in a
 * page are distinct, all 50 serialisations are distinct, and page 2 (&from=50)
 * is a completely different set. So the records themselves do not collide and
 * jsonlist's own id precedence already picks the right field.
 *
 * What the numbers look like instead is page 1 being fetched twice: 100 emitted
 * = 50 records counted two times, 51 stored = those 50 records plus the one
 * collector-truncation-notice row. Japan Search's offset parameter is spelled
 * `from`, and lib/pagewalk.c PW_OFF_PARAMS knows only offset / $skip / skip /
 * resultOffset / startIndex / start — so the walk has a declared size (`size`
 * IS in PW_SIZE_PARAMS) and no cursor it recognises.
 *
 * Not fixed here because the remedy is in lib/pagewalk.c, not in these rows:
 * `from` is a real offset spelling and adding it would let all three walk
 * (hit=61,084 against the 50 currently taken). Writing `&from=0` into these
 * URLs alone would change nothing, since pagewalk still would not know to
 * advance it. Confirming the double-fetch needs a run of the binary, which is
 * not available from this host, so it is stated as the reading that fits the
 * measurements rather than as established fact. */
VRSS(jpx_jnto_news_rss_xml, "jpx-jnto-news-rss-xml", "Japan National Tourism Organization — Press releases", "報道発表・お知らせ | 日本政府観光局（JNTO）",
  "culture", "heritage",
  "https://www.jnto.go.jp/news/rss.xml",
  "ja", "[\"japan\",\"culture\",\"heritage\"]", 3600,
  "Official press releases, the primary announcement channel of the body — Japan National Tourism Organization.");

VJPS(jpx_jpsearch_bunkazai, "jpx-jpsearch-bunkazai", "Japan Search — cultural property records", "ジャパンサーチ 横断検索（文化財）",
  "https://jpsearch.go.jp/api/item/search/jps-cross?keyword=%E6%96%87%E5%8C%96%E8%B2%A1&size=50",
  "[\"japan\",\"heritage\",\"museum\",\"archives\"]",
  "Cross-institution search over national heritage catalogues for designated cultural properties.");

VJPS(jpx_jpsearch_cross, "jpx-jpsearch-cross", "Japan Search — disaster records", "ジャパンサーチ 横断検索（防災）",
  "https://jpsearch.go.jp/api/item/search/jps-cross?keyword=%E9%98%B2%E7%81%BD&size=50",
  "[\"japan\",\"heritage\",\"disaster-archive\"]",
  "Cross-institution archival records on disaster and disaster prevention.");

VJPS(jpx_jpsearch_jinja, "jpx-jpsearch-jinja", "Japan Search — shrine records", "ジャパンサーチ 横断検索（神社）",
  "https://jpsearch.go.jp/api/item/search/jps-cross?keyword=%E7%A5%9E%E7%A4%BE&size=50",
  "[\"japan\",\"heritage\",\"shrine\"]",
  "Cross-institution catalogue records relating to Shinto shrines.");

VJPS(jpx_jpsearch_kofun, "jpx-jpsearch-kofun", "Japan Search — kofun burial mound records", "ジャパンサーチ 横断検索（古墳）",
  "https://jpsearch.go.jp/api/item/search/jps-cross?keyword=%E5%8F%A4%E5%A2%B3&size=50",
  "[\"japan\",\"heritage\",\"archaeology\"]",
  "Cross-institution archaeological records for kofun burial mounds.");

VRSS(jpx_ndl_opensearch_bosai, "jpx-ndl-opensearch-bosai", "NDL Search — disaster prevention holdings", "国立国会図書館サーチ 防災",
  "culture", "heritage",
  "https://ndlsearch.ndl.go.jp/api/opensearch?any=%E9%98%B2%E7%81%BD&cnt=50",
  "ja", "[\"japan\",\"library\",\"disaster\"]", 86400,
  "National Diet Library holdings on disaster prevention and response.");

VRSS(jpx_ndl_opensearch_kyodo, "jpx-ndl-opensearch-kyodo", "NDL Search — local history holdings", "国立国会図書館サーチ 郷土史",
  "culture", "heritage",
  "https://ndlsearch.ndl.go.jp/api/opensearch?any=%E9%83%B7%E5%9C%9F%E5%8F%B2&cnt=50",
  "ja", "[\"japan\",\"library\",\"local-history\"]", 86400,
  "National Diet Library holdings on local and regional history, useful for place research.");

VRSS(jpx_ndl_opensearch_sensai, "jpx-ndl-opensearch-sensai", "NDL Search — war damage records", "国立国会図書館サーチ 戦災",
  "culture", "heritage",
  "https://ndlsearch.ndl.go.jp/api/opensearch?any=%E6%88%A6%E7%81%BD&cnt=50",
  "ja", "[\"japan\",\"library\",\"war-records\"]", 86400,
  "National Diet Library holdings on wartime damage, a common OSINT background source.");

VCSV(jpx_tokyo_t000021_bunkazai, "jpx-tokyo-t000021-bunkazai", "東京都教育庁 — designated cultural properties", "東京都教育庁 文化財一覧",
  "culture", "heritage",
  "https://www.opendata.metro.tokyo.lg.jp/suisyoudataset/130001_cultural_property.csv",
  "ja", "[\"japan\",\"tokyo\",\"opendata\",\"geodata\",\"heritage\"]", 86400,
  "Point layer of designated cultural properties published by 東京都教育庁 with per-record coordinates.");

VCSV(jpx_tokyo_t132039_bunkazai, "jpx-tokyo-t132039-bunkazai", "武蔵野市 — designated cultural properties", "武蔵野市 文化財一覧",
  "culture", "heritage",
  "https://www.opendata.metro.tokyo.lg.jp/musashino/132039_cultural_property.csv",
  "ja", "[\"japan\",\"tokyo\",\"opendata\",\"geodata\",\"heritage\"]", 86400,
  "Point layer of designated cultural properties published by 武蔵野市 with per-record coordinates.");

VCSV(jpx_tokyo_t132152_bunkazai, "jpx-tokyo-t132152-bunkazai", "国立市 — designated cultural properties", "国立市 文化財一覧",
  "culture", "heritage",
  "https://www.opendata.metro.tokyo.lg.jp/kunitachi/132152_cultural_property.csv",
  "ja", "[\"japan\",\"tokyo\",\"opendata\",\"geodata\",\"heritage\"]", 86400,
  "Point layer of designated cultural properties published by 国立市 with per-record coordinates.");

VCSV(jpx_tokyo_t133621_bunkazai, "jpx-tokyo-t133621-bunkazai", "利島村 — designated cultural properties", "利島村 文化財一覧",
  "culture", "heritage",
  "https://www.opendata.metro.tokyo.lg.jp/toshimamura/133621_cultural_property.csv",
  "ja", "[\"japan\",\"tokyo\",\"opendata\",\"geodata\",\"heritage\"]", 86400,
  "Point layer of designated cultural properties published by 利島村 with per-record coordinates.");
