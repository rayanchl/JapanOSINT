/* collectors/telecom/sources/tor_exit_nodes.c
 * Port of server/src/collectors/torExitNodes.js.
 * Tor onionoo details?country=jp&flag=Exit → FeatureCollection. Keyless.
 * SEED branch (no live relays) dropped → 0 features when down. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define API_URL "https://onionoo.torproject.org/details?country=jp&flag=Exit"

static void passthru(cJSON *p, const char *k, cJSON *r, const char *ik) {
  cJSON *v = cJSON_GetObjectItem(r, ik);
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Accept: application/json", NULL };
  cJSON *data = feed_get_json_h(ctx->http, API_URL, hdrs, 10000);
  cJSON *relays = data ? cJSON_GetObjectItem(data, "relays") : NULL;

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(relays)) {
    cJSON *r;
    cJSON_ArrayForEach(r, relays) {
      cJSON *latv = cJSON_GetObjectItem(r, "latitude");
      cJSON *lonv = cJSON_GetObjectItem(r, "longitude");
      if (!latv || cJSON_IsNull(latv) || !lonv || cJSON_IsNull(lonv)) continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_Duplicate(lonv, 1));
      cJSON_AddItemToArray(co, cJSON_Duplicate(latv, 1));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
      passthru(p, "relay_id", r, "fingerprint");
      passthru(p, "nickname", r, "nickname");
      /* r.country?.toUpperCase() || 'JP' */
      cJSON *cc = cJSON_GetObjectItem(r, "country");
      if (cc && cJSON_IsString(cc) && cc->valuestring[0]) {
        char up[64];
        size_t j = 0;
        for (const char *s = cc->valuestring; *s && j + 1 < sizeof up; s++)
          up[j++] = (char)toupper((unsigned char)*s);
        up[j] = '\0';
        cJSON_AddStringToObject(p, "country", up);
      } else {
        cJSON_AddStringToObject(p, "country", "JP");
      }
      passthru(p, "city", r, "city_name");
      passthru(p, "as_name", r, "as_name");
      /* Math.round((r.observed_bandwidth || 0) / 1000) */
      cJSON *obw = cJSON_GetObjectItem(r, "observed_bandwidth");
      double bw = (obw && cJSON_IsNumber(obw)) ? obw->valuedouble : 0;
      cJSON_AddItemToObject(p, "bandwidth_kbs",
        cJSON_CreateNumber(floor(bw / 1000.0 + 0.5)));
      cJSON *run = cJSON_GetObjectItem(r, "running");
      cJSON_AddItemToObject(p, "running",
        cJSON_CreateBool(cJSON_IsTrue(run)));
      /* (r.flags || []).join(',') */
      cJSON *fl = cJSON_GetObjectItem(r, "flags");
      char flags[512];
      flags[0] = '\0';
      size_t fp = 0;
      if (cJSON_IsArray(fl)) {
        cJSON *e;
        int first = 1;
        cJSON_ArrayForEach(e, fl) {
          const char *es = cJSON_IsString(e) ? e->valuestring : "";
          int wn = snprintf(flags + fp, sizeof flags - fp, "%s%s",
                            first ? "" : ",", es);
          if (wn > 0) fp += (size_t)wn;
          if (fp >= sizeof flags) { fp = sizeof flags - 1; break; }
          first = 0;
        }
      }
      cJSON_AddStringToObject(p, "flags", flags);
      cJSON_AddStringToObject(p, "source", "onionoo");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[tor-exit-nodes] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def tor_exit_nodes_def = {
  .id = "tor-exit-nodes", .collector = "telecom",
  .name = "Tor Exit Nodes", .name_ja = "Tor 出口ノード",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(tor_exit_nodes_def)
