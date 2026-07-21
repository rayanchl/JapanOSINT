/* collectors/cyber/sources/feodo_tracker_jp.c
 * Port of server/src/collectors/feodoTrackerJp.js. abuse.ch Feodo C2 IP
 * blocklist JSON → JP-filtered Features at TOKYO. No natural id → uid is the
 * sha1 hash fallback over JSON.stringify({g,p}); properties are built in the
 * EXACT JS key order so the cJSON serialization (hence the hash) matches
 * Node byte-for-byte (geojson featureUid parity proven earlier). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../core/intel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define URL_JSON "https://feodotracker.abuse.ch/downloads/ipblocklist.json"

/* copy r[key] preserving original JSON type/value (== JS passthrough); add a
 * JSON null when absent (JS `undefined`→ omitted by JSON.stringify; abuse.ch
 * always provides these fields, so duplicate-or-null is faithful). */
static void copy_field(cJSON *dst, const char *outk, cJSON *r, const char *ink) {
  cJSON *v = cJSON_GetObjectItem(r, ink);
  cJSON_AddItemToObject(dst, outk, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *arr = feed_get_json(ctx->http, URL_JSON, 20000);
  if (!arr || !cJSON_IsArray(arr)) { if (arr) cJSON_Delete(arr); return -1; }

  cJSON *features = cJSON_CreateArray();
  int i = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    cJSON *cc = cJSON_GetObjectItem(r, "country");
    if (!cc || !cJSON_IsString(cc)) continue;
    /* String(country).toUpperCase()==='JP' */
    if (!((cc->valuestring[0]=='J'||cc->valuestring[0]=='j') &&
          (cc->valuestring[1]=='P'||cc->valuestring[1]=='p') &&
          cc->valuestring[2]=='\0')) continue;

    cJSON *feat = cJSON_CreateObject();
    cJSON_AddStringToObject(feat, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(139.6917)); /* TOKYO */
    cJSON_AddItemToArray(co, cJSON_CreateNumber(35.6895));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(feat, "geometry", g);

    cJSON *p = cJSON_CreateObject();          /* EXACT JS key order */
    cJSON_AddNumberToObject(p, "idx", i);
    copy_field(p, "ip", r, "ip_address");
    copy_field(p, "port", r, "port");
    copy_field(p, "malware", r, "malware");
    copy_field(p, "country", r, "country");
    copy_field(p, "asn", r, "as_number");
    copy_field(p, "asn_name", r, "as_name");
    copy_field(p, "hostname", r, "hostname");
    copy_field(p, "first_seen", r, "first_seen");
    copy_field(p, "last_online", r, "last_online");
    copy_field(p, "status", r, "status");
    cJSON_AddStringToObject(p, "source", "feodo_tracker");
    cJSON_AddItemToObject(feat, "properties", p);
    cJSON_AddItemToArray(features, feat);
    i++;
  }
  cJSON_Delete(arr);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[feodo-tracker-jp] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def feodo_tracker_jp_def = {
  .id = "feodo-tracker-jp", .collector = "cyber",
  .name = "abuse.ch Feodo Tracker (JP)", .name_ja = "abuse.ch Feodo Tracker 日本",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(feodo_tracker_jp_def)
