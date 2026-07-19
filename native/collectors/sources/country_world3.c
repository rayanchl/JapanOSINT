/* collectors/sources/country_world3.c
 * OSINT service — Denmark address search (keyless DAWA), pivot on ctx->entity.
 * Real fetch or honest-empty. No API key.
 *   DK_ADDRESS  api.dataforsyningen.dk/adresser?q= */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *CW3_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  if (strcmp(ctx->source_id ? ctx->source_id : "", "DK_ADDRESS") != 0) return 0;
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
    "https://api.dataforsyningen.dk/adresser?q=%s&per_side=10&struktur=mini&srid=4326", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json", CW3_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "DK_ADDRESS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!cJSON_IsArray(root)) { if (root) cJSON_Delete(root); return 0; }
  int n = 0;
  const cJSON *a;
  cJSON_ArrayForEach(a, root) {
    const char *bet = jo_sv(a, "betegnelse");
    const char *id = jo_sv(a, "id");
    const cJSON *x = cJSON_GetObjectItem(a, "x");   /* lon */
    const cJSON *y = cJSON_GetObjectItem(a, "y");   /* lat */
    const char *postnr = jo_sv(a, "postnr");
    const char *postnrnavn = jo_sv(a, "postnrnavn");
    if (!bet) continue;
    int hg = (cJSON_IsNumber(x) && cJSON_IsNumber(y));
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "address", bet);
    if (postnr) cJSON_AddStringToObject(d, "postal_code", postnr);
    if (postnrnavn) cJSON_AddStringToObject(d, "post_town", postnrnavn);
    cJSON_AddStringToObject(d, "source", "DAWA (Denmark)");
    char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "service", "DK_ADDRESS"); cJSON_AddBoolToObject(p, "success", 1);
    char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
    intel_item it = {0};
    it.remote_key = id ? id : bet; it.title = bet; it.summary = postnrnavn; it.body = bj; it.lang = "da";
    it.has_geo = hg; it.lat = hg ? y->valuedouble : 0; it.lon = hg ? x->valuedouble : 0;
    it.record_type = "dk-address"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"DK_ADDRESS\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj); free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[DK_ADDRESS] emitted %d\n", n);
  return 0;
}

static const source_def dk_address_def = {
  .id = "DK_ADDRESS", .collector = "osint",
  .name = "Denmark Address (DAWA)", .name_ja = "Denmark Address (DAWA)",
  .update_interval_sec = 0, .run = run,
  .category = "geospatial", .type = "api",
  .url = "https://api.dataforsyningen.dk",
  .description = "DAWA keyless Danish address search with coordinates (postal code, town).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(dk_address_def)
