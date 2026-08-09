/* collectors/sources/mar_nga_list_of_lights.c
 * NGA List of Lights, Radio Aids and Fog Signals — lighthouses, lighted buoys,
 * RACONs and fog signals for one publication volume.
 *
 * Endpoint: https://msi.nga.mil/api/publications/ngalol/lights-buoys
 *           ?output=json&volume=110&includeRemovals=false
 * Emits (all fetched): volumeNumber, aidType, geopoliticalHeading,
 *   regionHeading, featureNumber, name, characteristic, heightFeetMeters,
 *   range, structure, remarks, noticeNumber/noticeWeek/noticeYear, and the
 *   aid position parsed from the DMS 'position' string.
 * Keyless. Licence: US Government work (NGA), public domain.
 *
 * parse_notes trap — "The only coordinate is the 'position' field, a DMS
 * string with an embedded newline between lat and lon
 * (\"65°35'32.1\\\"N \\n37°34'08.9\\\"W\") — split on the newline and parse
 * each half to decimal degrees; there is no numeric lat/lon." lol_position()
 * splits on the hemisphere letter that closes the latitude half (which also
 * handles the newline) and parses both halves with dms_to_deg().
 * parse_notes trap — "Rows with position null exist (headings/notes) and must
 * be skipped rather than given a region coordinate": those rows are emitted
 * with has_geo=0, never given the region's coordinate (R2).
 * parse_notes — "'volume' is a required query parameter (110-116 etc.);
 * requests without it return 400." Volume 110 is pinned in the URL.
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

#define LOL_URL "https://msi.nga.mil/api/publications/ngalol/lights-buoys" \
                "?output=json&volume=110&includeRemovals=false"

/* One DMS half ("65°35'32.1\"N") -> decimal degrees; 0 if unparseable. */
static int dms_to_deg(const char *s, double *out) {
  if (!s || !*s) return 0;
  double part[3] = { 0, 0, 0 };
  int np = 0;
  char hemi = 0;
  const char *p = s;
  while (*p) {
    unsigned char c = (unsigned char)*p;
    if (isdigit(c)) {
      char *end = NULL;
      double v = strtod(p, &end);
      if (end == p) { p++; continue; }
      if (np < 3) part[np++] = v;
      p = end;
      continue;
    }
    if (c == 'N' || c == 'S' || c == 'E' || c == 'W') hemi = (char)c;
    p++;
  }
  if (np == 0 || !hemi) return 0;
  double d = part[0] + part[1] / 60.0 + part[2] / 3600.0;
  if (hemi == 'S' || hemi == 'W') d = -d;
  if (hemi == 'N' || hemi == 'S') { if (d < -90.0  || d > 90.0)  return 0; }
  else                            { if (d < -180.0 || d > 180.0) return 0; }
  *out = d;
  return 1;
}

/* Split the combined "<lat DMS>N \n<lon DMS>W" position and parse both. */
static int lol_position(const char *pos, double *lat, double *lon) {
  if (!pos) return 0;
  const char *p = pos;
  while (*p && *p != 'N' && *p != 'S') p++;
  if (!*p) return 0;
  size_t latlen = (size_t)(p - pos) + 1;
  if (latlen >= 96) return 0;
  char half[96];
  memcpy(half, pos, latlen);
  half[latlen] = 0;
  if (!dms_to_deg(half, lat)) return 0;
  return dms_to_deg(p + 1, lon);
}

/* Collapse the embedded newlines NGA uses inside multi-line text fields. */
static void flatten(char *s) {
  for (; *s; s++) if (*s == '\n' || *s == '\r' || *s == '\t') *s = ' ';
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, LOL_URL, 60000);
  if (!doc) {
    fprintf(stderr, "[nga-list-of-lights] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_GetObjectItem(doc, "ngalol");
  if (!cJSON_IsArray(arr) && cJSON_IsArray(doc)) arr = doc;
  int n = 0;
  cJSON *a;
  cJSON_ArrayForEach(a, arr) {
    if (!cJSON_IsObject(a)) continue;
    const char *name = jo_sv(a, "name");
    const char *feat = jo_sv(a, "featureNumber");
    if (!name && !feat) continue;

    double lat = 0, lon = 0;
    int geo = lol_position(jo_sv(a, "position"), &lat, &lon);

    char key[192];
    snprintf(key, sizeof key, "%s|%s",
             jo_sv(a, "volumeNumber") ? jo_sv(a, "volumeNumber") : "PUB",
             feat ? feat : (name ? name : "?"));
    flatten(key);

    cJSON *p = cJSON_CreateObject();
    jo_copy_str(p, a, "volumeNumber");
    jo_copy_str(p, a, "aidType");
    jo_copy_str(p, a, "geopoliticalHeading");
    jo_copy_str(p, a, "regionHeading");
    jo_copy_str(p, a, "subregionHeading");
    jo_copy_str(p, a, "featureNumber");
    jo_copy_str(p, a, "name");
    jo_copy_str(p, a, "position");
    jo_copy_str(p, a, "characteristic");
    jo_copy_str(p, a, "heightFeetMeters");
    jo_copy_str(p, a, "range");
    jo_copy_str(p, a, "structure");
    jo_copy_str(p, a, "remarks");
    jo_copy_str(p, a, "noticeWeek");
    jo_copy_str(p, a, "noticeYear");
    jo_copy_num(p, a, "noticeNumber");
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "charted-aid-position");
      cJSON_AddStringToObject(p, "geo_note",
        "parsed from the upstream DMS 'position' string; no numeric lat/lon exists upstream");
    }
    cJSON_AddStringToObject(p, "source", "NGA List of Lights");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[256], summary[256];
    const char *geoh = jo_sv(a, "geopoliticalHeading");
    snprintf(title, sizeof title, "%s%s%s", name ? name : feat,
             geoh ? " — " : "", geoh ? geoh : "");
    flatten(title);
    const char *ch = jo_sv(a, "characteristic");
    const char *aid = jo_sv(a, "aidType");
    snprintf(summary, sizeof summary, "%s%s%s", aid ? aid : "",
             ch ? " · " : "", ch ? ch : "");
    flatten(summary);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = "https://msi.nga.mil/Publications/LOL";
    it.lang            = "en";
    it.record_type     = "light-list-entry";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"navigation-aid\",\"nga\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[nga-list-of-lights] emitted %d\n", n);
  return 0;
}

static const source_def mar_nga_list_of_lights_def = {
  .id = "nga-list-of-lights", .collector = "maritime",
  .name = "NGA List of Lights, Radio Aids and Fog Signals",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset", .url = LOL_URL,
  .description = "Official light list for PUB 110: lighthouses, lighted buoys, RACONs and fog signals with characteristic, height, nominal range, structure description and the Notice to Mariners week that last changed them.",
  .license = "US Government work (NGA), public domain",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_nga_list_of_lights_def)
