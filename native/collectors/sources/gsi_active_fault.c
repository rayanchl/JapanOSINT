/* collectors/geospatial/sources/gsi_active_fault.c
 * Port of server/src/collectors/gsiActiveFault.js — best-effort fetch of
 * an active-fault-trace GeoJSON. Optional override URL via env var
 * GSI_ACTIVE_FAULT_GEOJSON_URL (Node: envFor → native: getenv), else an
 * unverified AIST candidate endpoint. Honest empty when no usable geometry
 * (rule 8 — no fabricated fault geometry). Property key order
 * (fault_id, name, fault_type, activity, source) mirrors JS for parity. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define CANDIDATE_URL "https://gbank.gsj.jp/activefault/cgi-bin/geojson.cgi"

static const char *str_of(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

static int normalize(cJSON *data, cJSON *out) {
  cJSON *src = data ? cJSON_GetObjectItem(data, "features") : NULL;
  if (!src || !cJSON_IsArray(src)) return 0;
  int added = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, src) {
    cJSON *geom = cJSON_GetObjectItem(f, "geometry");
    if (!geom) continue;
    cJSON *gt = cJSON_GetObjectItem(geom, "type");
    cJSON *gc = cJSON_GetObjectItem(geom, "coordinates");
    if (!gt || !gc) continue;
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    if (!p) p = cJSON_CreateObject(); else p = cJSON_Duplicate(p, 1);

    const char *fid = str_of(p, "id");
    if (!fid) fid = str_of(p, "fault_id");
    if (!fid) fid = str_of(p, "name");
    char hk[24]; char fidbuf[64];
    if (!fid) {
      char *gs = cJSON_PrintUnformatted(geom);
      const char *parts[] = { gs ? gs : "" };
      feed_hash_key(hk, parts, 1);
      snprintf(fidbuf, sizeof fidbuf, "AF_%s", hk);
      fid = fidbuf;
      free(gs);
    }
    const char *name = str_of(p, "name");
    if (!name) name = str_of(p, "fault_name");
    if (!name) name = str_of(p, "断層名");
    const char *ftype = str_of(p, "type");
    if (!ftype) ftype = str_of(p, "断層型");
    const char *act = str_of(p, "activity");
    if (!act) act = str_of(p, "活動度");

    cJSON *nf = cJSON_CreateObject();
    cJSON_AddStringToObject(nf, "type", "Feature");
    cJSON_AddItemToObject(nf, "geometry", cJSON_Duplicate(geom, 1));
    cJSON *np = cJSON_CreateObject();             /* EXACT JS key order */
    cJSON_AddStringToObject(np, "fault_id", fid);
    if (name) cJSON_AddStringToObject(np, "name", name); else cJSON_AddNullToObject(np, "name");
    if (ftype) cJSON_AddStringToObject(np, "fault_type", ftype); else cJSON_AddNullToObject(np, "fault_type");
    if (act) cJSON_AddStringToObject(np, "activity", act); else cJSON_AddNullToObject(np, "activity");
    cJSON_AddStringToObject(np, "source", "gsi_active_fault");
    cJSON_AddItemToObject(nf, "properties", np);
    cJSON_AddItemToArray(out, nf);
    cJSON_Delete(p);
    added++;
  }
  return added;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *override = getenv("GSI_ACTIVE_FAULT_GEOJSON_URL");
  const char *urls[2]; int nu = 0;
  if (override && *override) urls[nu++] = override;
  urls[nu++] = CANDIDATE_URL;

  cJSON *features = cJSON_CreateArray();
  for (int i = 0; i < nu; i++) {
    cJSON *data = feed_get_json(ctx->http, urls[i], 20000);
    if (!data) continue;
    int got = normalize(data, features);
    cJSON_Delete(data);
    if (got > 0) break;
    /* reset accumulator for next url if this one yielded nothing */
    cJSON_Delete(features);
    features = cJSON_CreateArray();
  }

  if (cJSON_GetArraySize(features) == 0) {
    cJSON_Delete(features);
    fprintf(stderr, "[gsi-active-fault] unavailable (honest empty)\n");
    return -1;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[gsi-active-fault] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def gsi_active_fault_def = {
  .id = "gsi-active-fault", .collector = "geospatial",
  .name = "GSI Active Fault Map", .name_ja = "国土地理院 活断層図",
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(gsi_active_fault_def)
