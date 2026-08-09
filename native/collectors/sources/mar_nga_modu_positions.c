/* collectors/sources/mar_nga_modu_positions.c
 * NGA MSI — Mobile Offshore Drilling Unit reported positions (drillships,
 * semisubmersibles, jack-ups) worldwide.
 *
 * Endpoint: https://msi.nga.mil/api/publications/modu?output=json
 * Emits (all fetched): name, date of the report, rigStatus, specialStatus,
 *   navArea, region, subregion, the upstream DMS 'position' string, and the
 *   last reported position from the numeric latitude/longitude fields.
 * Keyless. Licence: US Government work (NGA), public domain.
 *
 * parse_notes — "latitude/longitude here ARE numeric decimal degrees (unlike
 * the World Port Index), and are the rig's last REPORTED position as of the
 * 'date' field — not a live feed, so carry 'date' through and let the analyst
 * judge staleness." date becomes published_at and report_date.
 * parse_notes trap — "Rows where latitude/longitude are null (seen on some
 * reports) must be emitted with has_geo=0." Handled (R2).
 * NOTE: a MODU is a fixed-for-now installation report, NOT a vessel AIS
 * position; record_type says so.
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

#define MODU_URL "https://msi.nga.mil/api/publications/modu?output=json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, MODU_URL, 40000);
  if (!doc) {
    fprintf(stderr, "[nga-modu-positions] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_GetObjectItem(doc, "modu");
  if (!cJSON_IsArray(arr) && cJSON_IsArray(doc)) arr = doc;
  int n = 0;
  cJSON *m;
  cJSON_ArrayForEach(m, arr) {
    if (!cJSON_IsObject(m)) continue;
    const char *name = jo_sv(m, "name");
    if (!name) continue;
    const char *date = jo_sv(m, "date");

    char key[160];
    snprintf(key, sizeof key, "%s|%s", name, date ? date : "");

    const cJSON *latj = cJSON_GetObjectItem(m, "latitude");
    const cJSON *lonj = cJSON_GetObjectItem(m, "longitude");
    int geo = cJSON_IsNumber(latj) && cJSON_IsNumber(lonj);
    double lat = geo ? latj->valuedouble : 0;
    double lon = geo ? lonj->valuedouble : 0;
    if (geo && (lat < -90 || lat > 90 || lon < -180 || lon > 180)) geo = 0;

    cJSON *p = cJSON_CreateObject();
    jo_copy_str(p, m, "name");
    jo_copy_str(p, m, "date");
    jo_copy_str(p, m, "rigStatus");
    jo_copy_str(p, m, "specialStatus");
    jo_copy_str(p, m, "navArea");
    jo_copy_str(p, m, "position");     /* upstream DMS duplicate of the position */
    jo_copy_num(p, m, "region");
    jo_copy_num(p, m, "subregion");
    jo_copy_num(p, m, "distance");
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "reported-position");
      cJSON_AddStringToObject(p, "geo_note",
        "last position REPORTED as of 'date' — not a live position feed");
    }
    cJSON_AddStringToObject(p, "source", "NGA Maritime Safety Information");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *rig = jo_sv(m, "rigStatus");
    const char *spc = jo_sv(m, "specialStatus");
    char title[200], summary[220];
    snprintf(title, sizeof title, "MODU %s", name);
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             rig ? rig : "",
             spc ? " · " : "", spc ? spc : "",
             date ? " · reported " : "", date ? date : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = "https://msi.nga.mil/Publications/MODU";
    it.lang            = "en";
    it.published_at    = date;
    it.record_type     = "offshore-drilling-unit-report";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"offshore\",\"drilling-rig\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[nga-modu-positions] emitted %d\n", n);
  return 0;
}

static const source_def mar_nga_modu_positions_def = {
  .id = "nga-modu-positions", .collector = "maritime",
  .name = "NGA MSI — Mobile Offshore Drilling Unit positions",
  .update_interval_sec = 43200, .run = run,
  .category = "industry", .type = "api", .url = MODU_URL,
  .description = "Reported positions and status of mobile offshore drilling units worldwide (drillships, semisubmersibles, jack-ups) — named rig, active/inactive status, wide-berth requests and last reported position with its report date.",
  .license = "US Government work (NGA), public domain",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_nga_modu_positions_def)
