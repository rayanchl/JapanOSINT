/* collectors/cyber/sources/feodo_tracker_jp.c
 * abuse.ch Feodo C2 IP blocklist JSON → JP-filtered intel rows.
 *
 * AUDIT NOTE (slice a3): the original port placed EVERY row at the Tokyo
 * Station coordinate (139.6917, 35.6895) because the JS it was ported from
 * did. abuse.ch publishes no coordinate for a C2 host, so that pin was an
 * invented location: hundreds of rows stacked on one point that is not where
 * anything is. Geometry is now null — the row still carries ip/asn/country,
 * which is the real geolocation signal, and the map no longer lies.
 * uid is now the stable natural key <ip>:<port> instead of a sha1 over
 * {geometry, properties} that included a positional "idx" (so every row's uid
 * churned whenever abuse.ch inserted or removed an entry). */
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

    cJSON *ipv  = cJSON_GetObjectItem(r, "ip_address");
    if (!ipv || !cJSON_IsString(ipv) || !ipv->valuestring[0]) continue;
    cJSON *portv = cJSON_GetObjectItem(r, "port");
    cJSON *malv  = cJSON_GetObjectItem(r, "malware");
    long long port = (portv && cJSON_IsNumber(portv)) ? (long long)portv->valuedouble : 0;

    cJSON *feat = cJSON_CreateObject();
    cJSON_AddStringToObject(feat, "type", "Feature");
    /* abuse.ch gives no coordinate for a C2 host — emit no geometry rather
     * than an invented one (see AUDIT NOTE above). The key is OMITTED, not set
     * to JSON null: lib/geojson.c:235 serialises whatever it finds under
     * "geometry", so an explicit null persists as the literal string "null". */

    cJSON *p = cJSON_CreateObject();
    char uid[96], title[192];
    if (port) snprintf(uid, sizeof uid, "%s:%lld", ipv->valuestring, port);
    else      snprintf(uid, sizeof uid, "%s", ipv->valuestring);
    snprintf(title, sizeof title, "Feodo C2 %s — %s", uid,
             (malv && cJSON_IsString(malv) && malv->valuestring[0])
               ? malv->valuestring : "unknown family");
    cJSON_AddStringToObject(p, "uid", uid);      /* stable natural key */
    cJSON_AddStringToObject(p, "title", title);
    cJSON_AddStringToObject(p, "record_type", "c2-host");
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
    cJSON_AddStringToObject(p, "link", "https://feodotracker.abuse.ch/browse/");
    cJSON_AddStringToObject(p, "source", "feodo_tracker");
    cJSON_AddItemToObject(feat, "properties", p);
    cJSON_AddItemToArray(features, feat);
    i++;
  }
  cJSON_Delete(arr);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[feodo-tracker-jp] emitted %d\n", n);
  return 0;                       /* an empty JP slice is honest, not an error */
}

static const source_def feodo_tracker_jp_def = {
  .id = "feodo-tracker-jp", .collector = "cyber",
  .name = "abuse.ch Feodo Tracker (JP)", .name_ja = "abuse.ch Feodo Tracker 日本",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(feodo_tracker_jp_def)
