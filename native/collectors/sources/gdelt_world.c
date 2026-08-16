/* collectors/sources/gdelt_world.c
 * OSINT services — GDELT DOC 2.0. On-demand entity pivot against the GDELT
 * Project's global news monitor (api.gdeltproject.org/api/v2/doc/doc). Keyless
 * and LIVE (no API key required).
 *
 * GDELT_DOC — mode=artlist: one intel_item per matched worldwide news article
 *   (url, title, seendate, domain, language, source country).
 * GDELT_GEO  — mode=pointdata (GeoJSON): one geo-tagged intel_item per place
 *   feature GDELT extracted for the query (name, article count, coordinates).
 *
 * Both share one run() switching on ctx->source_id. Only real, parsed API
 * fields are emitted; fetch failure / no matches → honest empty (return 0). */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* GDELT seendate looks like "20240115T063000Z"; leave as-is for published_at. */

/* one DOC article → one intel_item. Returns 1 if emitted. */
static int emit_article(intel_sink *sink, const cJSON *a) {
  if (!a) return 0;
  const char *url   = jo_sv(a, "url");
  const char *title = jo_sv(a, "title");
  if (!url && !title) return 0;

  const char *seen  = jo_sv(a, "seendate");
  const char *dom   = jo_sv(a, "domain");
  const char *lang  = jo_sv(a, "language");
  const char *ctry  = jo_sv(a, "sourcecountry");
  const char *img   = jo_sv(a, "socialimage");

  cJSON *data = cJSON_CreateObject();
  if (title) cJSON_AddStringToObject(data, "title", title);
  if (url)   cJSON_AddStringToObject(data, "url", url);
  if (dom)   cJSON_AddStringToObject(data, "domain", dom);
  if (lang)  cJSON_AddStringToObject(data, "language", lang);
  if (ctry)  cJSON_AddStringToObject(data, "source_country", ctry);
  if (seen)  cJSON_AddStringToObject(data, "seendate", seen);
  if (img)   cJSON_AddStringToObject(data, "image", img);
  cJSON_AddStringToObject(data, "source", "GDELT");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "GDELT_DOC");
  cJSON_AddStringToObject(props, "source", "GDELT");
  if (dom)  cJSON_AddStringToObject(props, "domain", dom);
  if (ctry) cJSON_AddStringToObject(props, "source_country", ctry);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = url ? url : title;
  it.title           = title ? title : url;
  it.summary         = dom;
  it.body            = bj;
  it.lang            = lang ? lang : "en";
  it.published_at    = seen;
  it.link            = url;
  it.record_type     = "gdelt-article";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"GDELT\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* one GeoJSON place feature → one geo intel_item. Returns 1 if emitted. */
static int emit_geo(intel_sink *sink, const cJSON *f) {
  if (!f) return 0;
  const cJSON *geom = cJSON_GetObjectItem(f, "geometry");
  const cJSON *prop = cJSON_GetObjectItem(f, "properties");
  const cJSON *coords = geom ? cJSON_GetObjectItem(geom, "coordinates") : NULL;
  if (!cJSON_IsArray(coords) || cJSON_GetArraySize(coords) < 2) return 0;
  const cJSON *lonj = cJSON_GetArrayItem(coords, 0);
  const cJSON *latj = cJSON_GetArrayItem(coords, 1);
  if (!cJSON_IsNumber(lonj) || !cJSON_IsNumber(latj)) return 0;
  double lon = lonj->valuedouble, lat = latj->valuedouble;

  const char *name = prop ? jo_sv(prop, "name") : NULL;
  const cJSON *cntj = prop ? cJSON_GetObjectItem(prop, "count") : NULL;
  const char *html = prop ? jo_sv(prop, "html") : NULL;
  const char *shdr = prop ? jo_sv(prop, "shareimage") : NULL;
  if (!name) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "name", name);
  cJSON_AddNumberToObject(data, "lat", lat);
  cJSON_AddNumberToObject(data, "lon", lon);
  if (cntj && cJSON_IsNumber(cntj))
    cJSON_AddNumberToObject(data, "article_count", cntj->valuedouble);
  if (html) cJSON_AddStringToObject(data, "html", html);
  if (shdr) cJSON_AddStringToObject(data, "shareimage", shdr);
  cJSON_AddStringToObject(data, "source", "GDELT");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "GDELT_GEO");
  cJSON_AddStringToObject(props, "source", "GDELT");
  cJSON_AddStringToObject(props, "place", name);
  if (cntj && cJSON_IsNumber(cntj))
    cJSON_AddNumberToObject(props, "article_count", cntj->valuedouble);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char geoj[128];
  snprintf(geoj, sizeof geoj,
    "{\"type\":\"Point\",\"coordinates\":[%.6f,%.6f]}", lon, lat);

  intel_item it = {0};
  it.remote_key      = name;
  it.title           = name;
  it.body            = bj;
  it.lang            = "en";
  it.has_geo         = 1;
  it.lat             = lat;
  it.lon             = lon;
  it.geometry_geojson = geoj;
  it.record_type     = "gdelt-geo";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"GDELT\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *sid = ctx->source_id ? ctx->source_id : "GDELT_DOC";
  int is_geo = (strcmp(sid, "GDELT_GEO") == 0);

  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char url[1024];
  int emitted = 0;

  if (is_geo) {
    snprintf(url, sizeof url,
      "https://api.gdeltproject.org/api/v2/geo/geo?query=%s&mode=pointdata&format=geojson",
      enc);
    char *body = jo_get(ctx, url, hdrs, "gdelt-geo");
    if (body) {
      cJSON *root = cJSON_Parse(body);
      free(body);
      if (root) {
        cJSON *feats = cJSON_GetObjectItem(root, "features");
        if (cJSON_IsArray(feats)) {
          cJSON *f;
          cJSON_ArrayForEach(f, feats) {
            emitted += emit_geo(sink, f);
            /* (cap removed: every record of the fetched array is emitted —
             * docs/SOURCE_EXHAUSTIVENESS.md) */
          }
        }
        cJSON_Delete(root);
      }
    }
    fprintf(stderr, "[gdelt-geo] emitted %d\n", emitted);
  } else {
    snprintf(url, sizeof url,
      "https://api.gdeltproject.org/api/v2/doc/doc?query=%s&mode=artlist"
      "&maxrecords=50&sort=datedesc&format=json",
      enc);
    char *body = jo_get(ctx, url, hdrs, "gdelt-doc");
    if (body) {
      cJSON *root = cJSON_Parse(body);
      free(body);
      if (root) {
        cJSON *arts = cJSON_GetObjectItem(root, "articles");
        if (cJSON_IsArray(arts)) {
          cJSON *a; cJSON_ArrayForEach(a, arts) emitted += emit_article(sink, a);
        }
        cJSON_Delete(root);
      }
    }
    fprintf(stderr, "[gdelt-doc] emitted %d\n", emitted);
  }

  free(enc);
  return 0;   /* honest empty is not an error */
}

static const source_def gdelt_doc_def = {
  .id = "GDELT_DOC", .collector = "osint",
  .name = "GDELT Global News Search", .name_ja = "GDELT グローバルニュース検索",
  .update_interval_sec = 0, .run = run,
  .category = "news", .type = "api",
  .url = "https://api.gdeltproject.org/api/v2/doc/doc",
  .description = "GDELT DOC 2.0 — keyless global news article search (url, title, domain, country, language)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(gdelt_doc_def)

static const source_def gdelt_geo_def = {
  .id = "GDELT_GEO", .collector = "osint",
  .name = "GDELT News Geography", .name_ja = "GDELT ニュース地理",
  .update_interval_sec = 0, .run = run,
  .category = "news", .type = "api",
  .url = "https://api.gdeltproject.org/api/v2/geo/geo",
  .description = "GDELT GEO 2.0 — keyless geo-tagged places extracted from worldwide news for a query",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(gdelt_geo_def)
