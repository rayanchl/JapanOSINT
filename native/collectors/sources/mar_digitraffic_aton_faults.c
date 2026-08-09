/* collectors/sources/mar_digitraffic_aton_faults.c
 * Fintraffic Digitraffic — aid-to-navigation faults on Finnish fairways.
 * Unlit/damaged/off-station buoys, beacons, ramarks and lights.
 *
 * Endpoint: https://meri.digitraffic.fi/api/aton/v1/faults
 * Emits (all fetched): id, type (fault type), state, aton_id, aton_name_fi/sv,
 *   aton_type, fairway_number, fairway_name_fi/sv, area_description,
 *   entry_timestamp, fixed_timestamp, and the surveyed aid position from
 *   features[].geometry.coordinates = [lon, lat].
 * Keyless. Licence: Fintraffic Digitraffic, CC BY 4.0.
 *
 * parse_notes — "Coordinate is features[].geometry.coordinates = [lon, lat]
 * and is the surveyed position of the aid itself. fixed_timestamp is null
 * while the fault is open." Both honoured: null fixed_timestamp is simply
 * omitted, and no coordinate is invented when geometry is absent (R2).
 * parse_notes trap — Accept-Encoding: gzip is required; the platform HTTP
 * layer already sends it, so no custom header is added.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define ATON_URL "https://meri.digitraffic.fi/api/aton/v1/faults"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, ATON_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[digitraffic-aton-faults] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    if (!cJSON_IsObject(f)) continue;
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    if (!props) continue;
    const cJSON *idv = cJSON_GetObjectItem(props, "id");
    if (!cJSON_IsNumber(idv)) continue;
    char fid[32];
    snprintf(fid, sizeof fid, "%lld", (long long)idv->valuedouble);

    const char *aton = jo_sv(props, "aton_name_fi");
    if (!aton) aton = jo_sv(props, "aton_name_sv");
    const char *ftype = jo_sv(props, "type");
    const char *state = jo_sv(props, "state");
    const char *entry = jo_sv(props, "entry_timestamp");

    int geo = 0; double lat = 0, lon = 0;
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    if (g && !cJSON_IsNull(g)) {
      cJSON *co = cJSON_GetObjectItem(g, "coordinates");
      cJSON *x = cJSON_GetArrayItem(co, 0), *y = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        geo = 1; lon = x->valuedouble; lat = y->valuedouble;
      }
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "fault_id", fid);
    jo_copy_str(p, props, "type");
    jo_copy_str(p, props, "state");
    jo_copy_str(p, props, "domain");
    jo_copy_str(p, props, "aton_name_fi");
    jo_copy_str(p, props, "aton_name_sv");
    jo_copy_str(p, props, "aton_type");
    jo_copy_str(p, props, "fairway_name_fi");
    jo_copy_str(p, props, "fairway_name_sv");
    jo_copy_str(p, props, "area_description");
    jo_copy_str(p, props, "entry_timestamp");
    jo_copy_str(p, props, "fixed_timestamp");   /* null while the fault is open */
    jo_copy_num(p, props, "aton_id");
    jo_copy_num(p, props, "fairway_number");
    jo_copy_num(p, props, "area_number");
    const cJSON *fx = cJSON_GetObjectItem(props, "fixed");
    if (cJSON_IsBool(fx)) cJSON_AddBoolToObject(p, "fixed", cJSON_IsTrue(fx));
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "surveyed-aid-position");
    }
    cJSON_AddStringToObject(p, "source", "Fintraffic Digitraffic ATON");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[220], summary[240];
    snprintf(title, sizeof title, "AtoN fault: %s%s%s",
             aton ? aton : fid, ftype ? " — " : "", ftype ? ftype : "");
    const char *area = jo_sv(props, "area_description");
    const char *fw = jo_sv(props, "fairway_name_fi");
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             state ? state : "",
             fw ? " · " : "", fw ? fw : "",
             area ? " · " : "", area ? area : "");

    intel_item it = {0};
    it.remote_key      = fid;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = ATON_URL;
    it.published_at    = entry;
    it.record_type     = "aton-fault";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"navigation-hazard\",\"aton\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-aton-faults] emitted %d\n", n);
  return 0;
}

static const source_def mar_digitraffic_aton_faults_def = {
  .id = "digitraffic-aton-faults", .collector = "maritime",
  .name = "Fintraffic Digitraffic — aid-to-navigation faults",
  .update_interval_sec = 3600, .run = run,
  .category = "transport", .type = "api", .url = ATON_URL,
  .description = "Open faults on buoys, beacons, ramarks and lights on Finnish fairways — unlit/damaged/off-station aids with fairway name, sea area and the fault's entry time.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_digitraffic_aton_faults_def)
