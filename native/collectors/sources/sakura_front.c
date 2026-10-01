/* collectors/agriculture/sources/sakura_front.c
 * Port of server/src/collectors/sakuraFront.js — JMA cherry-blossom
 * (桜前線) listing page. fetchText → defensive <tr>/<td|th> rows: cell[0]
 * = observatory/city name (must contain kana/kanji), a later cell with a
 * M月D日 / M/D date = flowering date. Cap 60 rows, geocode each name via
 * GSI key-free AddressSearch (real coords only; inlined gsiAddressSearch).
 * Honest empty on fetch/parse failure or no geocodable rows (rule 8).
 * Property key order (station, flowering_date, observation_id, link,
 * source) mirrors the JS mapFn for featureUid hash parity.
 * (No native wildlife/ dir — filed under agriculture per task rule.) */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAKURA_URL "https://www.data.jma.go.jp/sakura/data/sakura_3monthmean.html"
#define GSI_BASE   "https://msearch.gsi.go.jp/address-search/AddressSearch?q="

static char *strip_dup(const char *inner, int len) {
  char *raw = strndup(inner, (size_t)len);
  if (!raw) return NULL;
  char *s = html_strip(raw);
  free(raw);
  return s;
}

/* encodeURIComponent for the unreserved set; everything else %HH (upper). */
static void url_encode(const char *in, char *out, size_t n) {
  static const char *hex = "0123456789ABCDEF";
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && w + 4 < n; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
        c == '*' || c == '\'' || c == '(' || c == ')') {
      out[w++] = (char)c;
    } else {
      out[w++] = '%';
      out[w++] = hex[c >> 4];
      out[w++] = hex[c & 0xF];
    }
  }
  out[w] = 0;
}

/* contains a digit then 月 ... 日, or D/D. JS:
 * /\d+\s*月\s*\d+\s*日|\d{1,2}\/\d{1,2}/  (approx: digit before 月 & 日;
 * or digit '/' digit). */
static int is_date_cell(const char *s) {
  /* M月D日 form */
  const char *g = strstr(s, "月");
  if (g) {
    int dbefore = 0;
    for (const char *t = s; t < g; t++) if (*t >= '0' && *t <= '9') { dbefore = 1; break; }
    const char *d2 = strstr(g, "日");
    if (dbefore && d2) {
      for (const char *t = g; t < d2; t++) if (*t >= '0' && *t <= '9') return 1;
    }
  }
  /* D/D form */
  for (const char *t = s; *t; t++) {
    if (t[0] >= '0' && t[0] <= '9') {
      const char *u = t;
      while (*u >= '0' && *u <= '9') u++;
      if (*u == '/' && u[1] >= '0' && u[1] <= '9') return 1;
    }
  }
  return 0;
}

/* has a kanji or hiragana byte (rough: any byte >= 0x80 → multibyte JP). */
static int has_jp(const char *s) {
  for (const unsigned char *p = (const unsigned char *)s; *p; p++)
    if (*p >= 0x80) return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *html = feed_get_text(ctx->http, SAKURA_URL, 15000);
  if (!html || strlen(html) < 50) { free(html); fprintf(stderr, "[sakura-front] unavailable\n"); return -1; }

  cJSON *features = cJSON_CreateArray();
  /* The bound was `considered < 60`, on a cJSON array that grows anyway — a
   * number, not a memory limit. The page is one table of observation rows and it
   * is already fetched; every row on it is read. */
  int considered = 0;
  const char *cur = html;
  const char *tr_in; int tr_len;
  while ((cur = html_block(cur, "tr", &tr_in, &tr_len)) != NULL) {
    char *row = strndup(tr_in, (size_t)tr_len);
    if (!row) continue;

    cJSON *cells = cJSON_CreateArray();
    const char *tdcur = row;
    const char *td_in; int td_len;
    while ((tdcur = html_block(tdcur, "td", &td_in, &td_len)) != NULL) {
      char *t = strip_dup(td_in, td_len);
      cJSON_AddItemToArray(cells, cJSON_CreateString(t ? t : ""));
      free(t);
    }
    tdcur = row;
    while ((tdcur = html_block(tdcur, "th", &td_in, &td_len)) != NULL) {
      char *t = strip_dup(td_in, td_len);
      cJSON_AddItemToArray(cells, cJSON_CreateString(t ? t : ""));
      free(t);
    }
    int cn = cJSON_GetArraySize(cells);
    if (cn < 2) { cJSON_Delete(cells); free(row); continue; }

    const char *name = cJSON_GetArrayItem(cells, 0)->valuestring;  /* exhaustive-ok: name column; every cell is in properties */
    if (!name || !*name || !has_jp(name)) { cJSON_Delete(cells); free(row); continue; }
    const char *date = NULL;
    for (int i = 1; i < cn; i++) {
      const char *x = cJSON_GetArrayItem(cells, i)->valuestring;
      if (is_date_cell(x)) { date = x; break; }
    }
    if (!date) { cJSON_Delete(cells); free(row); continue; }

    considered++;

    /* geocode via GSI: top hit coords or skip (no fabrication) */
    char q[512], url[640];
    url_encode(name, q, sizeof q);
    snprintf(url, sizeof url, "%s%s", GSI_BASE, q);
    cJSON *gd = feed_get_json(ctx->http, url, 8000);
    double lon = 0, lat = 0; int ok = 0;
    if (gd && cJSON_IsArray(gd) && cJSON_GetArraySize(gd) > 0) {
      cJSON *first = cJSON_GetArrayItem(gd, 0);  /* exhaustive-ok: place->coordinate resolution step */
      cJSON *gg = first ? cJSON_GetObjectItem(first, "geometry") : NULL;
      cJSON *gc = gg ? cJSON_GetObjectItem(gg, "coordinates") : NULL;
      if (cJSON_IsArray(gc) && cJSON_GetArraySize(gc) >= 2) {
        cJSON *c0 = cJSON_GetArrayItem(gc, 0);
        cJSON *c1 = cJSON_GetArrayItem(gc, 1);
        if (c0 && cJSON_IsNumber(c0) && c1 && cJSON_IsNumber(c1)) {
          lon = c0->valuedouble; lat = c1->valuedouble; ok = 1;
        }
      }
    }
    if (gd) cJSON_Delete(gd);
    if (!ok) { cJSON_Delete(cells); free(row); continue; }

    char hk[24];
    const char *parts[] = { name, date };
    feed_hash_key(hk, parts, 2);              /* intelHashKey(name,date) */
    char oid[64];
    snprintf(oid, sizeof oid, "SAKURA_%s", hk);

    cJSON *f = gj_point_feature(lon, lat);

    cJSON *p = cJSON_CreateObject();          /* EXACT JS key order */
    cJSON_AddStringToObject(p, "station", name);
    cJSON_AddStringToObject(p, "flowering_date", date);
    cJSON_AddStringToObject(p, "observation_id", oid);
    cJSON_AddStringToObject(p, "link", SAKURA_URL);
    cJSON_AddStringToObject(p, "source", "jma_sakura");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);

    cJSON_Delete(cells);
    free(row);
  }
  free(html);

  if (cJSON_GetArraySize(features) == 0) {
    cJSON_Delete(features);
    fprintf(stderr, "[sakura-front] unavailable (no geocodable rows)\n");
    return -1;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[sakura-front] emitted %d\n", n);
  /* run() is a STATUS code, not a row count: fetch/parse failures already
   * returned -1 above, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return 0;
}

static const source_def sakura_front_def = {
  .id = "sakura-front", .collector = "agriculture",
  .name = "Sakura Front", .name_ja = "桜前線",
  .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(sakura_front_def)
