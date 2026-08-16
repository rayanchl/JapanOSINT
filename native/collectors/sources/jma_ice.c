/* collectors/environment/sources/jma_ice.c   (category: ocean -> environment)
 * INTEL source — port of server/src/collectors/jmaIce.js.
 * Tab-separated "SEASON<TAB>MAX" longterm Okhotsk sea-ice file; emit ONE
 * intel item for the latest season's maximum extent (+ recent seasons).
 * uid = jma-ice|<latest season>. Honest empty on fetch/parse failure. */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define PAGE "https://www.data.jma.go.jp/kaiyou/shindan/a_1/series_okhotsk/series_okhotsk.html"
#define DATA "https://www.data.jma.go.jp/kaiyou/data/shindan/a_1/series_okhotsk/longterm_okhotsk_latest.txt"

struct row { char season[16]; double val; };

/* /^\d{4}-\d{2}$/ */
static int season_ok(const char *s) {
  if (strlen(s) != 7) return 0;
  for (int i = 0; i < 4; i++) if (!isdigit((unsigned char)s[i])) return 0;
  if (s[4] != '-') return 0;
  if (!isdigit((unsigned char)s[5]) || !isdigit((unsigned char)s[6])) return 0;
  return 1;
}

static void trim(char *s) {
  size_t l = strlen(s);
  while (l && isspace((unsigned char)s[l-1])) s[--l] = 0;
  size_t i = 0; while (s[i] && isspace((unsigned char)s[i])) i++;
  if (i) memmove(s, s + i, strlen(s + i) + 1);
}

static void iso_now(char *out, size_t n) {
  time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
  strftime(out, n, "%Y-%m-%dT%H:%M:%S.000Z", &g);
}

/* trim trailing zeros like JS Number->string (e.g. 12.30 -> "12.3"). */
static void numstr(double v, char *out, size_t n) {
  snprintf(out, n, "%.6f", v);
  char *dot = strchr(out, '.');
  if (!dot) return;
  char *e = out + strlen(out) - 1;
  while (e > dot && *e == '0') *e-- = 0;
  if (e == dot) *e = 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *txt = feed_get_text(ctx->http, DATA, 10000);
  if (!txt) {
    fprintf(stderr, "[jma-ice] upstream unavailable\n");
    return -1;
  }
  struct row *rows = NULL; int rn = 0, rcap = 0;
  char *save = NULL;
  for (char *line = strtok_r(txt, "\n", &save); line;
       line = strtok_r(NULL, "\n", &save)) {
    char *cr = strchr(line, '\r'); if (cr) *cr = 0;
    char *tab = strchr(line, '\t');
    if (!tab) continue;
    *tab = 0;
    char season[64]; snprintf(season, sizeof season, "%s", line);
    trim(season);
    char *endp; double val = strtod(tab + 1, &endp);
    if (endp == tab + 1) continue;
    if (!season_ok(season)) continue;
    if (rn == rcap) { rcap = rcap ? rcap * 2 : 64;
      rows = realloc(rows, (size_t)rcap * sizeof *rows); }
    snprintf(rows[rn].season, sizeof rows[rn].season, "%s", season);
    rows[rn].val = val;
    rn++;
  }
  free(txt);

  if (rn == 0) {
    free(rows);
    fprintf(stderr, "[jma-ice] no rows parsed\n");
    return -1;
  }

  struct row *latest = &rows[rn - 1];
  int rstart = rn > 10 ? rn - 10 : 0;
  char now[40]; iso_now(now, sizeof now);
  char latv[32]; numstr(latest->val, latv, sizeof latv);

  /* body recent list "season:val, ..." */
  char recent_str[1024] = {0};
  for (int i = rstart; i < rn; i++) {
    char vb[32]; numstr(rows[i].val, vb, sizeof vb);
    char piece[64];
    snprintf(piece, sizeof piece, "%s%s:%s",
             i > rstart ? ", " : "", rows[i].season, vb);
    strncat(recent_str, piece, sizeof recent_str - strlen(recent_str) - 1);
  }

  char title[256], summ[256], body[1536];
  snprintf(title, sizeof title,
    "Sea of Okhotsk max sea-ice extent %s: %s \xC3\x97\x31\x30\xE2\x81\xB4 km\xC2\xB2",
    latest->season, latv);
  snprintf(summ, sizeof summ,
    "JMA: maximum sea-ice extent for the %s winter season was %s \xC3\x97\x31\x30\xE2\x81\xB4 km\xC2\xB2.",
    latest->season, latv);
  snprintf(body, sizeof body,
    "Annual maximum Sea of Okhotsk sea-ice extent (\xC3\x97\x31\x30\xE2\x81\xB4 km\xC2\xB2). "
    "Latest: %s = %s. Recent seasons: %s.",
    latest->season, latv, recent_str);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "latest_season", latest->season);
  cJSON_AddNumberToObject(p, "latest_max_extent_1e4_km2", latest->val);
  cJSON *rec = cJSON_CreateArray();
  for (int i = rstart; i < rn; i++) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "season", rows[i].season);
    cJSON_AddNumberToObject(o, "max_extent_1e4_km2", rows[i].val);
    cJSON_AddItemToArray(rec, o);
  }
  cJSON_AddItemToObject(p, "recent_seasons", rec);
  cJSON_AddNumberToObject(p, "total_seasons", rn);
  char *pj = cJSON_PrintUnformatted(p);

  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("sea-ice"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("okhotsk"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("jma"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("climate"));
  char *tj = cJSON_PrintUnformatted(tags);

  intel_item it = {0};
  it.remote_key = latest->season;
  it.title = title;
  it.summary = summ;
  it.body = body;
  it.link = PAGE;
  it.author = "\xE6\xB0\x97\xE8\xB1\xA1\xE5\xBA\x81 Japan Meteorological Agency";
  it.lang = "ja";
  it.published_at = now;
  it.record_type = "article";
  it.properties_json = pj;
  it.tags_json = tj;
  int rc = sink->emit(sink, &it);

  free(pj); free(tj); free(rows);
  cJSON_Delete(p); cJSON_Delete(tags);
  fprintf(stderr, "[jma-ice] emitted %d (latest=%s)\n",
          rc >= 0 ? 1 : 0, latest->season);
  return rc >= 0 ? 0 : -1;
}

static const source_def jma_ice_def = {
  .id = "jma-ice", .collector = "environment",
  .name = "JMA Sea Ice Okhotsk", .name_ja = "気象庁 オホーツク海氷",
  .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(jma_ice_def)
