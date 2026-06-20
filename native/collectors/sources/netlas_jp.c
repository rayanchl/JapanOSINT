/* collectors/cyber/sources/netlas_jp.c
 * Port of server/src/collectors/netlasJp.js (createThreatIntelCollector).
 * env NETLAS_API_KEY → responses search geo.country:"Japan", size 100,
 * geo.location point or TOKYO fallback. */
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* QUERY default 'geo.country:"Japan"' (NETLAS_QUERY override), SIZE default
 * 100 (NETLAS_LIMIT). url = base + ?q=<enc>&size=<n>; "Japan" enc → %22Japan%22
 * and ':' → %3A, '"' → %22. We build the encoded query for the default. */
#define NETLAS_BASE "https://app.netlas.io/api/responses/"

/* JS d?.a?.b */
static cJSON *opt2(cJSON *o, const char *k1, const char *k2) {
  cJSON *a = o ? cJSON_GetObjectItem(o, k1) : NULL;
  cJSON *b = a ? cJSON_GetObjectItem(a, k2) : NULL;
  return b ? cJSON_Duplicate(b, 1) : cJSON_CreateNull();
}
static cJSON *opt3(cJSON *o, const char *k1, const char *k2, const char *k3) {
  cJSON *a = o ? cJSON_GetObjectItem(o, k1) : NULL;
  cJSON *b = a ? cJSON_GetObjectItem(a, k2) : NULL;
  cJSON *c = b ? cJSON_GetObjectItem(b, k3) : NULL;
  return c ? cJSON_Duplicate(c, 1) : cJSON_CreateNull();
}
static void put1(cJSON *p, const char *k, cJSON *d, const char *ik) {
  cJSON *v = d ? cJSON_GetObjectItem(d, ik) : NULL;
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

/* URL-encode the query value (RFC3986-ish, mirror encodeURIComponent for the
 * chars present in netlas queries: letters/digits/._-~ unescaped). */
static void urlenc(const char *s, char *o, size_t n) {
  static const char hex[] = "0123456789ABCDEF";
  size_t j = 0;
  for (; *s && j + 4 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' || c == '~') {
      o[j++] = (char)c;
    } else {
      o[j++] = '%'; o[j++] = hex[c >> 4]; o[j++] = hex[c & 15];
    }
  }
  o[j] = '\0';
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)ud;
  const char *q = getenv("NETLAS_QUERY");
  if (!q || !*q) q = "geo.country:\"Japan\"";
  const char *szs = getenv("NETLAS_LIMIT");
  long size = (szs && *szs) ? strtol(szs, NULL, 10) : 100;
  char enc[512];
  urlenc(q, enc, sizeof enc);
  char url[768];
  snprintf(url, sizeof url, "%s?q=%s&size=%ld", NETLAS_BASE, enc, size);

  char keyh[256];
  snprintf(keyh, sizeof keyh, "X-API-Key: %s", key);
  const char *hdrs[] = { keyh, "accept: application/json", NULL };
  cJSON *json = feed_get_json_h(ctx->http, url, hdrs, 15000);
  if (!json) return NULL;

  cJSON *items = cJSON_GetObjectItem(json, "items");
  cJSON *features = cJSON_CreateArray();
  int i = 0;
  if (cJSON_IsArray(items)) {
    cJSON *it;
    cJSON_ArrayForEach(it, items) {
      cJSON *d = cJSON_GetObjectItem(it, "data");
      if (!d) d = it;                          /* it?.data || it */
      cJSON *geo = cJSON_GetObjectItem(d, "geo");
      cJSON *loc = geo ? cJSON_GetObjectItem(geo, "location") : NULL;
      cJSON *lonv = loc ? cJSON_GetObjectItem(loc, "lon") : NULL;
      cJSON *latv = loc ? cJSON_GetObjectItem(loc, "lat") : NULL;
      int has = lonv && cJSON_IsNumber(lonv) && latv && cJSON_IsNumber(latv);

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      /* Only emit a Point for records with a real netlas geo.location; the
       * Node TOKYO fallback fabricated a location for geo-less hosts (no-
       * fabrication rule). Geo-less hosts emit geometry:null → has_geo=0,
       * keeping every real fetched field. */
      if (has) {
        cJSON *g = cJSON_CreateObject();
        cJSON_AddStringToObject(g, "type", "Point");
        cJSON *co = cJSON_CreateArray();
        cJSON_AddItemToArray(co, cJSON_CreateNumber(lonv->valuedouble));
        cJSON_AddItemToArray(co, cJSON_CreateNumber(latv->valuedouble));
        cJSON_AddItemToObject(g, "coordinates", co);
        cJSON_AddItemToObject(f, "geometry", g);
      } else {
        cJSON_AddNullToObject(f, "geometry");
      }

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", i);
      put1(pr, "ip", d, "ip");
      put1(pr, "port", d, "port");
      put1(pr, "protocol", d, "protocol");
      put1(pr, "host", d, "host");
      cJSON_AddItemToObject(pr, "country", opt2(d, "geo", "country"));
      cJSON_AddItemToObject(pr, "city", opt2(d, "geo", "city"));
      cJSON_AddItemToObject(pr, "asn", opt2(d, "asn", "number"));
      cJSON_AddItemToObject(pr, "as_name", opt2(d, "asn", "name"));
      put1(pr, "product", d, "product");
      /* d?.http?.title || d?.tls?.subject?.common_name || null */
      cJSON *ht = opt2(d, "http", "title");
      if (!cJSON_IsNull(ht) && !(cJSON_IsString(ht) && !ht->valuestring[0])) {
        cJSON_AddItemToObject(pr, "title", ht);
      } else {
        cJSON_Delete(ht);
        cJSON *cn = opt3(d, "tls", "subject", "common_name");
        if (cJSON_IsString(cn) && !cn->valuestring[0]) {
          cJSON_Delete(cn);
          cJSON_AddItemToObject(pr, "title", cJSON_CreateNull());
        } else {
          cJSON_AddItemToObject(pr, "title", cn);
        }
      }
      put1(pr, "last_updated", d, "last_updated");
      cJSON_AddStringToObject(pr, "source", "netlas");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  cJSON_Delete(json);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, "NETLAS_API_KEY", NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def netlas_jp_def = {
  .id = "netlas-jp", .collector = "cyber",
  .name = "Netlas (JP responses)", .name_ja = "Netlas 日本",
   .update_interval_sec = 7200, .run = run };
REGISTER_SOURCE(netlas_jp_def)
