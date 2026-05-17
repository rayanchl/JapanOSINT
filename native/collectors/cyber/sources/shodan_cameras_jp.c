/* collectors/cyber/sources/shodan_cameras_jp.c
 * Port of server/src/collectors/shodanCamerasJp.js.
 * Shodan host/search camera fingerprints in country:JP → FeatureCollection.
 * Gated on SHODAN_API_KEY. No seed. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* QUERY (JS verbatim), percent-encoded by encodeURIComponent. */
#define QUERY \
  "country:JP (webcamXP OR \"Server: yawcam\" OR product:\"Hipcam RealServer\" " \
  "OR title:\"Network Camera\" OR product:\"Hikvision\" OR product:\"Dahua\")"

/* encodeURIComponent: keep A-Za-z0-9 - _ . ! ~ * ' ( ) */
static void uric(const char *s, char *out, size_t n) {
  size_t o = 0;
  for (; *s && o + 4 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')') {
      out[o++] = (char)c;
    } else {
      snprintf(out + o, n - o, "%%%02X", c);
      o += 3;
    }
  }
  out[o] = '\0';
}

static int num_of(cJSON *v, double *out) {
  if (!v) return 0;
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *e; double d = strtod(v->valuestring, &e);
    if (e != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}

static cJSON *product_of(cJSON *m) {
  cJSON *v = cJSON_GetObjectItem(m, "product");
  if (v && cJSON_IsString(v) && v->valuestring[0])
    return cJSON_CreateString(v->valuestring);
  cJSON *sh = cJSON_GetObjectItem(m, "_shodan");
  if (sh && cJSON_IsObject(sh)) {
    cJSON *mod = cJSON_GetObjectItem(sh, "module");
    if (mod && cJSON_IsString(mod) && mod->valuestring[0])
      return cJSON_CreateString(mod->valuestring);
  }
  return cJSON_CreateNull();
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *k = getenv("SHODAN_API_KEY");
  if (!k || !*k) {
    fprintf(stderr, "[shodan-cameras-jp] gated (no SHODAN_API_KEY)\n");
    return 0;
  }
  char qenc[1024];
  uric(QUERY, qenc, sizeof qenc);
  char url[1280];
  snprintf(url, sizeof url,
    "https://api.shodan.io/shodan/host/search?key=%s&query=%s", k, qenc);
  cJSON *data = feed_get_json(ctx->http, url, 20000);
  cJSON *matches = data ? cJSON_GetObjectItem(data, "matches") : NULL;

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(matches)) {
    cJSON *m;
    cJSON_ArrayForEach(m, matches) {
      cJSON *loc = cJSON_GetObjectItem(m, "location");
      double lon, lat;
      if (!(loc &&
            num_of(cJSON_GetObjectItem(loc, "longitude"), &lon) &&
            num_of(cJSON_GetObjectItem(loc, "latitude"), &lat)))
        continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
      cJSON *ipv = cJSON_GetObjectItem(m, "ip_str");
      cJSON *prtv = cJSON_GetObjectItem(m, "port");
      const char *ipstr = (ipv && cJSON_IsString(ipv)) ? ipv->valuestring : "";
      char idb[96];
      if (prtv && cJSON_IsNumber(prtv))
        snprintf(idb, sizeof idb, "%s:%g", ipstr, prtv->valuedouble);
      else if (prtv && cJSON_IsString(prtv))
        snprintf(idb, sizeof idb, "%s:%s", ipstr, prtv->valuestring);
      else
        snprintf(idb, sizeof idb, "%s:", ipstr);
      cJSON_AddStringToObject(p, "camera_uid", idb);
      cJSON_AddItemToObject(p, "ip",
        ipv ? cJSON_Duplicate(ipv, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "port",
        prtv ? cJSON_Duplicate(prtv, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "vendor", product_of(m));
      cJSON *cityv = loc ? cJSON_GetObjectItem(loc, "city") : NULL;
      cJSON_AddItemToObject(p, "city",
        (cityv && cJSON_IsString(cityv) && cityv->valuestring[0])
          ? cJSON_CreateString(cityv->valuestring) : cJSON_CreateNull());
      cJSON *orgv = cJSON_GetObjectItem(m, "org");
      cJSON_AddItemToObject(p, "org",
        (orgv && cJSON_IsString(orgv) && orgv->valuestring[0])
          ? cJSON_CreateString(orgv->valuestring) : cJSON_CreateNull());
      char streamurl[128];
      if (prtv && cJSON_IsNumber(prtv))
        snprintf(streamurl, sizeof streamurl, "http://%s:%g/", ipstr,
                 prtv->valuedouble);
      else if (prtv && cJSON_IsString(prtv))
        snprintf(streamurl, sizeof streamurl, "http://%s:%s/", ipstr,
                 prtv->valuestring);
      else
        snprintf(streamurl, sizeof streamurl, "http://%s:undefined/", ipstr);
      cJSON_AddStringToObject(p, "stream_url", streamurl);
      cJSON_AddStringToObject(p, "source", "shodan_api");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[shodan-cameras-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def shodan_cameras_jp_def = {
  .id = "shodan-cameras-jp", .collector = "cyber",
  .name = "Shodan Cameras (JP)",
  .name_ja = "Shodan \xE3\x82\xAB\xE3\x83\xA1\xE3\x83\xA9 \xE6\x97\xA5\xE6\x9C\xAC",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(shodan_cameras_jp_def)
