/* collectors/infrastructure/sources/grid_usage_realtime.c — port of
 * server/src/collectors/gridUsageRealtime.js.
 * 10 regional grids; each juyo CSV's last data line's last numeric column is
 * the latest load_mw. JS uses plain fetchText (no SJIS decode) so we do too.
 * Emits ONE intel item per grid (always, reachable or not) — uid =
 * intelUid(SOURCE_ID, g.id) == "grid-usage-realtime|<gid>" (remote_key=g.id).
 * properties order: grid, lat, lon, load_mw, reachable. tags: power,grid,
 * demand,<gid>. title/summary/link/lang/published_at mirrored. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/probe.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TIMEOUT_MS 10000

typedef struct { const char *id, *url; double lat, lon; } grid_t;

static const grid_t GRIDS[] = {
  {"tepco",   "https://www.tepco.co.jp/forecast/html/images/juyo-d-j.csv",                35.6762, 139.6503},
  {"kepco",   "https://www.kansai-td.co.jp/yamasou/juyo1_kansai.csv",                     34.6937, 135.5023},
  {"chuden",  "https://denki-yoho.chuden.jp/denki_yoho_content_data/juyo_cepco003.csv",   35.1815, 136.9066},
  {"tohoku",  "https://setsuden.nw.tohoku-epco.co.jp/common/demand/juyo_05_TOHOKU.csv",   38.2682, 140.8694},
  {"kyuden",  "https://www.kyuden.co.jp/td_power_usages/csv/juyo-hourly-Kyushu.csv",      33.5902, 130.4017},
  {"hepco",   "https://denkiyoho.hepco.co.jp/area/data/juyo_01_hokkaido.csv",             43.0642, 141.3469},
  {"yonden",  "https://www.yonden.co.jp/nw/denkiyoho/csv/juyo_shikoku.csv",               34.3401, 134.0434},
  {"rikuden", "https://www.rikuden.co.jp/nw/denki-yoho/csv/juyo_05_rikuden.csv",          36.6953, 137.2113},
  {"chugoku", "https://www.energia.co.jp/nw/jukyuu/sys/juyo-d1-j.csv",                    34.3966, 132.4596},
  {"okiden",  "https://www.okiden.co.jp/denki2/juyo_10_okinawa.csv",                      26.2124, 127.6792},
};
#define NGRIDS ((int)(sizeof(GRIDS)/sizeof(GRIDS[0])))

/* parseLastLoad: walk lines bottom-up; first line with >=2 comma fields whose
 * last field parses to a finite >0 number → {load_mw, raw=line}. */
static int parse_last_load(const char *csv, double *load_mw, char *raw, size_t rawn) {
  if (!csv || !*csv) return 0;
  /* collect trimmed non-empty lines */
  size_t cap = 256, n = 0;
  char **lines = malloc(cap * sizeof(char *));
  char *dup = strdup(csv);
  for (char *p = dup, *nl; p && *p; p = nl ? nl + 1 : NULL) {
    nl = strchr(p, '\n');
    if (nl) *nl = 0;
    char *s = p;
    size_t len = strlen(s);
    while (*s == ' ' || *s == '\t' || *s == '\r') { s++; len--; }
    while (len && (s[len-1]==' '||s[len-1]=='\t'||s[len-1]=='\r')) s[--len] = 0;
    if (!len) continue;
    if (n == cap) { cap *= 2; lines = realloc(lines, cap * sizeof(char *)); }
    lines[n++] = s;
  }
  int found = 0;
  for (long i = (long)n - 1; i >= 0; i--) {
    char *ln = lines[i];
    /* cols = split(','); need >=2 */
    int commas = 0;
    char *lastcol = ln;
    for (char *q = ln; *q; q++) if (*q == ',') { commas++; lastcol = q + 1; }
    if (commas < 1) continue;
    char *e;
    double v = strtod(lastcol, &e);
    if (e == lastcol) continue;       /* !Number.isFinite */
    if (!(v > 0)) continue;
    *load_mw = v;
    snprintf(raw, rawn, "%s", ln);
    found = 1;
    break;
  }
  free(lines);
  free(dup);
  return found;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char now[32];
  probe_iso_now(now, sizeof now);
  int emitted = 0;

  for (int gi = 0; gi < NGRIDS; gi++) {
    const grid_t *g = &GRIDS[gi];
    char *csv = feed_get_text(ctx->http, g->url, TIMEOUT_MS);
    double load_mw = 0;
    char raw[1024] = {0};
    int parsed = parse_last_load(csv, &load_mw, raw, sizeof raw);
    free(csv);

    char up[16];
    snprintf(up, sizeof up, "%s", g->id);
    for (char *c = up; *c; c++) *c = (char)toupper((unsigned char)*c);

    char title[64];
    snprintf(title, sizeof title, "%s 5-min supply/demand", up);
    char summary[1200];
    if (parsed) {
      snprintf(summary, sizeof summary, "latest load ≈ %g (raw: %s)", load_mw, raw);
    } else {
      snprintf(summary, sizeof summary, "unreachable");
    }

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("power"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("grid"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("demand"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(g->id));
    char *tj = cJSON_PrintUnformatted(tags);

    cJSON *p = cJSON_CreateObject();          /* EXACT JS key order */
    cJSON_AddStringToObject(p, "grid", g->id);
    cJSON_AddNumberToObject(p, "lat", g->lat);
    cJSON_AddNumberToObject(p, "lon", g->lon);
    cJSON_AddItemToObject(p, "load_mw",
      parsed ? cJSON_CreateNumber(load_mw) : cJSON_CreateNull());
    cJSON_AddBoolToObject(p, "reachable", parsed ? 1 : 0);
    char *pj = cJSON_PrintUnformatted(p);

    intel_item it = {0};
    it.remote_key      = g->id;             /* → uid grid-usage-realtime|<gid> */
    it.title           = title;
    it.summary         = summary;
    it.link            = g->url;
    it.lang            = "ja";
    it.published_at    = now;
    it.tags_json       = tj;
    it.properties_json = pj;
    if (sink->emit(sink, &it) >= 0) emitted++;

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(p);
  }
  fprintf(stderr, "[grid-usage-realtime] emitted %d\n", emitted);
  /* run() is a STATUS code, not a row count: fetch/parse failures already
   * returned -1 above, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return 0;
}

static const source_def grid_usage_realtime_def = {
  .id = "grid-usage-realtime", .collector = "infrastructure",
  .name = "10-grid 5-min supply/demand CSVs", .name_ja = "10エリア 5分電力CSV",
   .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(grid_usage_realtime_def)
