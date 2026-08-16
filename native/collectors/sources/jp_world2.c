/* collectors/sources/jp_world2.c
 * OSINT service — net-new keyless Japan postal-code lookup, pivot on ctx->entity
 * (a 7-digit postal code). Real fetch or honest-empty. No API key.
 *   JP_POSTAL  zipcloud.ibsnet.co.jp/api/search?zipcode=<7digits> */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *JP_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  if (strcmp(ctx->source_id ? ctx->source_id : "", "JP_POSTAL") != 0) return 0;
  /* keep only digits (accept "150-0001" or "1500001") */
  char zip[16]; int zi = 0;
  for (const char *p = q; *p && zi < 15; p++) if (isdigit((unsigned char)*p)) zip[zi++] = *p;
  zip[zi] = 0;
  if (zi != 7) { fprintf(stderr, "[JP_POSTAL] entity must be a 7-digit postal code\n"); return 0; }
  char url[256];
  snprintf(url, sizeof url, "https://zipcloud.ibsnet.co.jp/api/search?zipcode=%s", zip);
  const char *hdrs[] = { "Accept: application/json", JP_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "JP_POSTAL");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  int n = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *a1 = jo_sv(r, "address1");   /* prefecture */
      const char *a2 = jo_sv(r, "address2");   /* city/ward */
      const char *a3 = jo_sv(r, "address3");   /* town */
      const char *k1 = jo_sv(r, "kana1");
      if (!a1 && !a2 && !a3) continue;
      char full[256];
      snprintf(full, sizeof full, "%s%s%s", a1 ? a1 : "", a2 ? a2 : "", a3 ? a3 : "");
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "zipcode", zip);
      if (a1) cJSON_AddStringToObject(d, "prefecture", a1);
      if (a2) cJSON_AddStringToObject(d, "city", a2);
      if (a3) cJSON_AddStringToObject(d, "town", a3);
      if (k1) cJSON_AddStringToObject(d, "prefecture_kana", k1);
      cJSON_AddStringToObject(d, "source", "zipcloud");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "JP_POSTAL"); cJSON_AddStringToObject(p, "zipcode", zip);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      intel_item it = {0};
      it.remote_key = zip; it.title = full; it.summary = a1; it.body = bj; it.lang = "ja";
      it.record_type = "jp-postal-address"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"JP_POSTAL\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[JP_POSTAL] emitted %d\n", n);
  return 0;
}

static const source_def jp_postal_def = {
  .id = "JP_POSTAL", .collector = "osint",
  .name = "Japan Postal Code", .name_ja = "郵便番号検索",
  .update_interval_sec = 0, .run = run,
  .category = "geospatial", .type = "api",
  .url = "https://zipcloud.ibsnet.co.jp",
  .description = "zipcloud keyless: Japan 7-digit postal code → prefecture/city/town.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(jp_postal_def)
