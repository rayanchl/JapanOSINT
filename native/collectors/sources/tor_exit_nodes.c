/* collectors/telecom/sources/tor_exit_nodes.c
 * Port of server/src/collectors/torExitNodes.js.
 * Tor onionoo details?country=jp&flag=Exit → FeatureCollection. Keyless.
 * SEED branch (no live relays) dropped → 0 features when down. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define API_URL "https://onionoo.torproject.org/details?country=jp&flag=Exit"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Accept: application/json", NULL };
  cJSON *data = feed_get_json_h(ctx->http, API_URL, hdrs, 10000);
  cJSON *relays = data ? cJSON_GetObjectItem(data, "relays") : NULL;

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(relays)) {
    cJSON *r;
    cJSON_ArrayForEach(r, relays) {
      /* onionoo no longer publishes per-relay latitude/longitude (every relay
       * in the country=jp&flag=Exit response now has them null), so the old
       * "skip unless it has coordinates" guard silently dropped the WHOLE
       * feed. Emit the relay either way — fingerprint / nickname / AS /
       * bandwidth / flags are the payload; the pin is a bonus. */
      cJSON *latv = cJSON_GetObjectItem(r, "latitude");
      cJSON *lonv = cJSON_GetObjectItem(r, "longitude");
      int has_geo = latv && !cJSON_IsNull(latv) && cJSON_IsNumber(latv) &&
                    lonv && !cJSON_IsNull(lonv) && cJSON_IsNumber(lonv);

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      if (has_geo) {
        cJSON *g = cJSON_CreateObject();
        cJSON_AddStringToObject(g, "type", "Point");
        cJSON *co = cJSON_CreateArray();
        cJSON_AddItemToArray(co, cJSON_Duplicate(lonv, 1));
        cJSON_AddItemToArray(co, cJSON_Duplicate(latv, 1));
        cJSON_AddItemToObject(g, "coordinates", co);
        cJSON_AddItemToObject(f, "geometry", g);
      }
      /* else: no "geometry" key at all — lib/geojson.c turns a cJSON null
       * into the literal string "null" in the geometry column. */

      cJSON *p = cJSON_CreateObject();
      /* relay_id is not in lib/geojson.c's NATIVE_ID_KEYS, so the uid fell
       * back to sha1(geometry+properties) — and properties carries last_seen,
       * which changes hourly, so every run re-inserted every relay as a new
       * row. The fingerprint is the stable identity. */
      {
        cJSON *fp = cJSON_GetObjectItem(r, "fingerprint");
        if (fp && cJSON_IsString(fp) && fp->valuestring[0])
          cJSON_AddStringToObject(p, "uid", fp->valuestring);
      }
      jo_put_or_null(p, "relay_id", r, "fingerprint");
      jo_put_or_null(p, "nickname", r, "nickname");
      /* lib/geojson.c needs title|name|name_ja|label for a readable row. */
      {
        cJSON *nk = cJSON_GetObjectItem(r, "nickname");
        cJSON *an = cJSON_GetObjectItem(r, "as_name");
        char tb[300];
        snprintf(tb, sizeof tb, "Tor exit relay %s%s%s",
                 (nk && cJSON_IsString(nk)) ? nk->valuestring : "(unnamed)",
                 (an && cJSON_IsString(an)) ? " — " : "",
                 (an && cJSON_IsString(an)) ? an->valuestring : "");
        cJSON_AddStringToObject(p, "title", tb);
      }
      jo_put_or_null(p, "published_at", r, "last_seen");
      jo_put_or_null(p, "first_seen", r, "first_seen");
      jo_put_or_null(p, "or_addresses", r, "or_addresses");
      jo_put_or_null(p, "exit_addresses", r, "exit_addresses");
      jo_put_or_null(p, "as", r, "as");
      jo_put_or_null(p, "platform", r, "platform");
      jo_put_or_null(p, "version", r, "version");
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
      jo_put_or_null(p, "city", r, "city_name");
      jo_put_or_null(p, "as_name", r, "as_name");
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
