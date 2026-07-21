/* collectors/transport/sources/marinetraffic_jp.c
 * Port of server/src/collectors/marinetrafficJp.js (distinct from
 * marine_traffic.c / marineTraffic.js — that one has an OSM fallback; THIS
 * one has none). MarineTraffic Exportvessels REST API for the Japan bbox,
 * gated on MARINETRAFFIC_API_KEY. Honest empty without the key or on
 * failure — never fabricated vessels. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* r.X != null → number (or numeric string). */
static int num_field(cJSON *r, const char *k, double *out) {
  cJSON *v = cJSON_GetObjectItem(r, k);
  if (!v || cJSON_IsNull(v)) return 0;
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring[0]) {
    *out = strtod(v->valuestring, NULL); return 1;
  }
  return 0;
}

/* r.A || r.B || null  (string, "" falsy). */
static void s2_or_null(cJSON *p, const char *outk, cJSON *r,
                       const char *a, const char *b) {
  cJSON *va = cJSON_GetObjectItem(r, a);
  if (va && cJSON_IsString(va) && va->valuestring[0]) {
    cJSON_AddStringToObject(p, outk, va->valuestring); return;
  }
  if (b) {
    cJSON *vb = cJSON_GetObjectItem(r, b);
    if (vb && cJSON_IsString(vb) && vb->valuestring[0]) {
      cJSON_AddStringToObject(p, outk, vb->valuestring); return;
    }
    if (vb && cJSON_IsNumber(vb)) {
      cJSON_AddItemToObject(p, outk, cJSON_Duplicate(vb, 1)); return;
    }
  }
  cJSON_AddItemToObject(p, outk, cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("MARINETRAFFIC_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[marinetraffic-jp] gated (no MARINETRAFFIC_API_KEY)\n");
    return 0;                       /* honest empty */
  }
  char url[512];
  snprintf(url, sizeof url,
    "https://services.marinetraffic.com/api/exportvessels/v:8/%s"
    "/MINLAT:24/MAXLAT:46/MINLON:122/MAXLON:146/protocol:jsono", key);
  cJSON *data = feed_get_json(ctx->http, url, 20000);
  if (!data || !cJSON_IsArray(data)) {
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[marinetraffic-jp] unavailable\n");
    return 0;                       /* honest empty */
  }

  cJSON *features = cJSON_CreateArray();
  cJSON *r;
  cJSON_ArrayForEach(r, data) {
    double lon, lat;
    if (!num_field(r, "LON", &lon) || !num_field(r, "LAT", &lat)) continue;

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();               /* EXACT JS key order */
    cJSON *mmsi = cJSON_GetObjectItem(r, "MMSI");
    cJSON_AddItemToObject(p, "mmsi",
      mmsi ? cJSON_Duplicate(mmsi, 1) : cJSON_CreateNull());
    cJSON *imo = cJSON_GetObjectItem(r, "IMO");
    if (imo && !cJSON_IsNull(imo) &&
        ((cJSON_IsString(imo) && imo->valuestring[0]) || cJSON_IsNumber(imo)))
      cJSON_AddItemToObject(p, "imo", cJSON_Duplicate(imo, 1));
    else
      cJSON_AddItemToObject(p, "imo", cJSON_CreateNull());
    s2_or_null(p, "vessel_name", r, "SHIPNAME", NULL);
    s2_or_null(p, "ship_type", r, "TYPE_NAME", "SHIPTYPE");

    double sp;
    if (num_field(r, "SPEED", &sp)) cJSON_AddNumberToObject(p, "speed", sp/10);
    else cJSON_AddItemToObject(p, "speed", cJSON_CreateNull());
    double hd;
    if (num_field(r, "HEADING", &hd)) cJSON_AddNumberToObject(p, "heading", hd);
    else cJSON_AddItemToObject(p, "heading", cJSON_CreateNull());
    s2_or_null(p, "destination", r, "DESTINATION", NULL);
    cJSON_AddStringToObject(p, "source", "marinetraffic_api");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[marinetraffic-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def marinetraffic_jp_def = {
  .id = "marinetraffic-jp", .collector = "transport",
  .name = "MarineTraffic Japan", .name_ja = "MarineTraffic 日本",
   .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(marinetraffic_jp_def)
