/* collectors/sources/mar_emodnet_offshore_platforms.c
 * EMODnet Human Activities — European offshore oil and gas platforms.
 *
 * Endpoint: https://ows.emodnet-humanactivities.eu/wfs?service=WFS
 *   &version=2.0.0&request=GetFeature&outputFormat=application/json
 *   &typeName=emodnet:platforms
 * Emits (all fetched): platformid, name, country, operator, current_status,
 *   category (structure type), function, primary_production, water_depth,
 *   production_start, valid_from/valid_to, location_blocks, coast_dist,
 *   remarks and the installation's surveyed position.
 * Keyless WFS. Licence: EMODnet Human Activities (EU), free reuse with
 *   attribution.
 *
 * parse_notes — "current_status distinguishes Operational / Removed / Under
 * construction; keep it so decommissioned structures are not shown as live."
 * The status is carried in properties and surfaced in the summary.
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

#define WFS_URL "https://ows.emodnet-humanactivities.eu/wfs?service=WFS" \
                "&version=2.0.0&request=GetFeature" \
                "&outputFormat=application/json&typeName=emodnet:platforms"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WFS_URL, 90000);
  if (!doc) {
    fprintf(stderr, "[emodnet-offshore-platforms] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *pid = jo_sv(pr, "platformid");
    const char *name = jo_sv(pr, "name");
    if (!pid && !name) continue;
    const char *key = pid ? pid : name;

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
    if (pid) cJSON_AddStringToObject(p, "platformid", pid);
    jo_copy_str(p, pr, "name");
    jo_copy_str(p, pr, "country");
    jo_copy_str(p, pr, "operator");
    jo_copy_str(p, pr, "current_status");
    jo_copy_str(p, pr, "category");
    jo_copy_str(p, pr, "function");
    jo_copy_str(p, pr, "primary_production");
    jo_copy_str(p, pr, "production_start");
    jo_copy_str(p, pr, "valid_from");
    jo_copy_str(p, pr, "valid_to");
    jo_copy_str(p, pr, "location_blocks");
    jo_copy_str(p, pr, "remarks");
    jo_copy_num(p, pr, "water_depth");
    jo_copy_num(p, pr, "weight_sub");
    jo_copy_num(p, pr, "weight_top");
    jo_copy_num(p, pr, "coast_dist");
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "surveyed-installation-position");
    }
    cJSON_AddStringToObject(p, "source", "EMODnet Human Activities");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *op = jo_sv(pr, "operator");
    const char *st = jo_sv(pr, "current_status");
    const char *prod = jo_sv(pr, "primary_production");
    char title[220], summary[240];
    if (pid && name) snprintf(title, sizeof title, "Platform %s (%s)", name, pid);
    else             snprintf(title, sizeof title, "Platform %s", key);
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             st ? st : "", op ? " · " : "", op ? op : "",
             prod ? " · " : "", prod ? prod : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = "https://emodnet.ec.europa.eu/en/human-activities";
    it.record_type     = "offshore-platform";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"offshore\",\"energy\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[emodnet-offshore-platforms] emitted %d\n", n);
  return 0;
}

static const source_def mar_emodnet_offshore_platforms_def = {
  .id = "emodnet-offshore-platforms", .collector = "maritime",
  .name = "EMODnet Human Activities — offshore oil and gas platforms",
  .update_interval_sec = 2592000, .run = run,
  .category = "industry", .type = "dataset", .url = WFS_URL,
  .description = "European offshore hydrocarbon platforms with operator, structure type, function, product, water depth, licence block and operational status — fixed maritime infrastructure and a cross-reference for MODU reporting.",
  .license = "EMODnet Human Activities (EU), free reuse with attribution",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_emodnet_offshore_platforms_def)
