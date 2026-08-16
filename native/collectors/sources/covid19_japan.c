/* collectors/statistics/sources/covid19_japan.c — port of
 * server/src/collectors/covid19Japan.js. Best-effort key-free fetch of
 * the MHLW open-data CSV (newly_confirmed_cases_daily.csv) that was
 * historically published; if it still resolves, emit the most recent 30
 * national daily rows (column "ALL" if present). The endpoint was likely
 * retired post-2023 reclassification — when it does not resolve we honest
 * empty (return -1), NEVER fabricate counts. uid key mirrors JS
 * intelUid(SOURCE_ID, date) (date is always a non-empty field → JS index
 * fallback is dead). */
#include "source.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CSV_URL "https://covid19.mhlw.go.jp/public/opendata/newly_confirmed_cases_daily.csv"
#define MAX_COLS 96
#define ROWS_CHUNK 4096   /* exhaustive-ok: growth step, not a cap */

static char *trim(char *s) {
  while (*s == ' ' || *s == '\r' || *s == '\t') s++;
  size_t L = strlen(s);
  while (L && (s[L-1] == ' ' || s[L-1] == '\r' || s[L-1] == '\t' || s[L-1] == '\n'))
    s[--L] = 0;
  return s;
}

/* split a CSV line in-place on commas into up to MAX_COLS fields. */
static int split_csv(char *line, char **out, int max) {
  int c = 0;
  char *p = line;
  out[c++] = p;
  for (; *p && c < max; p++) {
    if (*p == ',') { *p = 0; out[c++] = p + 1; }
  }
  return c;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *text = feed_get_text(ctx->http, CSV_URL, 15000);
  if (!text || !*text) {
    if (text) free(text);
    fprintf(stderr, "[covid19-japan] unavailable (MHLW COVID open data likely retired)\n");
    return -1;
  }

  /* index line starts */
  size_t lines_cap = ROWS_CHUNK;
  char **lines = calloc(lines_cap, sizeof *lines);
  if (!lines) { free(text); return -1; }
  int nl = 0;
  char *cur = text;
  /* The daily CSV grows by one row a day; a fixed 4096-row array silently
   * dropped everything past row 4096 (docs/SOURCE_EXHAUSTIVENESS.md). */
  for (char *p = text; *p; p++) {
    if ((size_t)nl + 2 >= lines_cap) {
      size_t nc = lines_cap * 2;
      char **np = realloc(lines, nc * sizeof *np);
      if (!np) break;
      lines = np; lines_cap = nc;
    }
    if (*p == '\n') { *p = 0; lines[nl++] = cur; cur = p + 1; }
  }
  if ((size_t)nl < lines_cap && *cur) lines[nl++] = cur;

  if (nl < 2) {
    free(lines); free(text);
    fprintf(stderr, "[covid19-japan] unavailable (no rows)\n");
    return -1;
  }

  /* header */
  char *hdrcpy = strdup(lines[0]);
  char *hdrs[MAX_COLS];
  int nh = split_csv(hdrcpy, hdrs, MAX_COLS);
  int dateCol = -1, allCol = -1;
  for (int i = 0; i < nh; i++) {
    char *h = trim(hdrs[i]);
    if (dateCol < 0 && (strcmp(h, "Date") == 0 || strcmp(h, "date") == 0))
      dateCol = i;
    if (allCol < 0 && (strcmp(h, "ALL") == 0 || strcmp(h, "All") == 0 ||
                       strcmp(h, "all") == 0))
      allCol = i;
  }

  int total = nl - 1;                 /* data rows */
  int start = total > 30 ? total - 30 : 0;
  int n = 0;
  for (int r = 1 + start; r < nl; r++) {
    char *rowcpy = strdup(lines[r]);
    if (!rowcpy) continue;
    char *fld[MAX_COLS];
    int nf = split_csv(rowcpy, fld, MAX_COLS);
    if (nf == 0) { free(rowcpy); continue; }

    const char *date = NULL;
    if (dateCol >= 0 && dateCol < nf) date = trim(fld[dateCol]);
    else date = trim(fld[0]);
    const char *allv = NULL;
    if (allCol >= 0 && allCol < nf) allv = trim(fld[allCol]);

    char rk[64], title[96], summary[128], body[320];
    snprintf(rk, sizeof rk, "%s", (date && *date) ? date : "null");
    snprintf(title, sizeof title, "COVID-19 Japan - %s",
             (date && *date) ? date : "?");
    if (allv && *allv)
      snprintf(summary, sizeof summary, "Newly confirmed (national): %s", allv);
    else
      snprintf(summary, sizeof summary, "COVID-19 daily row");
    snprintf(body, sizeof body,
             "MHLW newly_confirmed_cases_daily %s: %s",
             (date && *date) ? date : "null", lines[r]);

    cJSON *p = cJSON_CreateObject();
    if (date && *date) cJSON_AddStringToObject(p, "date", date);
    else cJSON_AddNullToObject(p, "date");
    if (allv && *allv) cJSON_AddStringToObject(p, "national_new_cases", allv);
    else cJSON_AddNullToObject(p, "national_new_cases");
    cJSON_AddStringToObject(p, "row", lines[r]);
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("health"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("covid19"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("statistics"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key      = rk;
    it.title           = title;
    it.summary         = summary;
    it.body            = body;
    it.link            = "https://covid19.mhlw.go.jp/";
    it.lang            = "ja";
    it.record_type     = "covid19-japan";
    it.properties_json = pj;
    it.tags_json       = tj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
    free(rowcpy);
  }

  free(hdrcpy);
  free(lines);
  free(text);
  fprintf(stderr, "[covid19-japan] emitted %d\n", n);
  /* run() is a STATUS code, not a row count: fetch/parse failures already
   * returned -1 above, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return 0;
}

static const source_def covid19_japan_def = {
  .id = "covid19-japan", .collector = "statistics",
  .name = "COVID-19 Japan Dashboard", .name_ja = "COVID-19 ダッシュボード",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(covid19_japan_def)
