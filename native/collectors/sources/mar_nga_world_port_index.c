/* collectors/sources/mar_nga_world_port_index.c
 * NGA World Port Index (Pub 150) — the ~3,700-entry world port reference.
 *
 * Endpoint: https://msi.nga.mil/api/publications/world-port-index?output=json
 * Emits (all fetched): portNumber, portName, countryCode/countryName,
 *   regionName, navArea, harborSize, harborType, shelter, channel/anchorage/
 *   cargo-pier/oil-terminal depths, max vessel length/beam/draft, pilotage,
 *   tugs, medical facilities, drydock flags, chartNumber, publicationNumber,
 *   and the published port reference point parsed from the DMS latitude /
 *   longitude strings.
 * Keyless. Licence: US Government work (NGA), public domain.
 *
 * parse_notes trap — "IMPORTANT: latitude/longitude are DMS STRINGS
 * (\"30°20'00\\\"N\"), not numbers — they must be parsed to decimal degrees
 * (sign from the N/S, E/W suffix) before use; a naive atof() yields 30/48 and
 * silently mislocates the port." dms_to_deg() below does the parse; a string
 * that does not yield a hemisphere and at least one numeric component leaves
 * the row with has_geo=0 rather than a wrong coordinate (R2).
 * parse_notes — the coordinate is the published port reference point, so
 * geo_precision is labelled "area-centroid".
 * parse_notes — "The countryCode query parameter was IGNORED in testing" —
 * no server-side filter is used; the whole index is fetched.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define WPI_URL "https://msi.nga.mil/api/publications/world-port-index?output=json"

/* "30°20'00\"N" / "48°17'00\"E" -> decimal degrees. Scans up to three numeric
 * runs (deg, min, sec) and takes the sign from the N/S/E/W hemisphere letter.
 * Returns 0 when the string has no hemisphere or no digits, so the caller can
 * leave the row un-located instead of inventing one. */
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

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WPI_URL, 60000);
  if (!doc) {
    fprintf(stderr, "[nga-world-port-index] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_GetObjectItem(doc, "ports");
  if (!cJSON_IsArray(arr) && cJSON_IsArray(doc)) arr = doc;
  int n = 0;
  cJSON *po;
  cJSON_ArrayForEach(po, arr) {
    if (!cJSON_IsObject(po)) continue;
    const char *name = jo_sv(po, "portName");
    const cJSON *pnum = cJSON_GetObjectItem(po, "portNumber");
    if (!name || !cJSON_IsNumber(pnum)) continue;
    char key[32];
    snprintf(key, sizeof key, "%lld", (long long)pnum->valuedouble);

    double lat = 0, lon = 0;
    int geo = dms_to_deg(jo_sv(po, "latitude"), &lat) &&
              dms_to_deg(jo_sv(po, "longitude"), &lon);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "portNumber", key);
    jo_copy_str(p, po, "portName");
    jo_copy_str(p, po, "countryCode");
    jo_copy_str(p, po, "countryName");
    jo_copy_str(p, po, "regionName");
    jo_copy_str(p, po, "navArea");
    jo_copy_str(p, po, "harborSize");
    jo_copy_str(p, po, "harborType");
    jo_copy_str(p, po, "shelter");
    jo_copy_str(p, po, "chDepth");
    jo_copy_str(p, po, "anDepth");
    jo_copy_str(p, po, "cpDepth");
    jo_copy_str(p, po, "otDepth");
    jo_copy_str(p, po, "maxVesselLength");
    jo_copy_str(p, po, "maxVesselBeam");
    jo_copy_str(p, po, "maxVesselDraft");
    jo_copy_str(p, po, "ptCompulsory");
    jo_copy_str(p, po, "ptAvailable");
    jo_copy_str(p, po, "tugsAssist");
    jo_copy_str(p, po, "tugsSalvage");
    jo_copy_str(p, po, "medFacilities");
    jo_copy_str(p, po, "srDrydock");
    jo_copy_str(p, po, "chartNumber");
    jo_copy_str(p, po, "publicationNumber");
    jo_copy_str(p, po, "latitude");           /* the upstream DMS string, verbatim */
    jo_copy_str(p, po, "longitude");
    jo_copy_num(p, po, "tide");
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "area-centroid");
      cJSON_AddStringToObject(p, "geo_note",
        "published port reference point, parsed from the upstream DMS strings");
    }
    cJSON_AddStringToObject(p, "source", "NGA World Port Index (Pub 150)");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *cc = jo_sv(po, "countryName");
    char title[220], summary[200];
    snprintf(title, sizeof title, "%s%s%s", name, cc ? ", " : "", cc ? cc : "");
    const char *hsz = jo_sv(po, "harborSize");
    const char *hty = jo_sv(po, "harborType");
    snprintf(summary, sizeof summary, "World Port Index %s%s%s%s%s", key,
             hsz ? " · harbour size " : "", hsz ? hsz : "",
             hty ? " · type " : "", hty ? hty : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://msi.nga.mil/Publications/WPI";
    it.lang            = "en";
    it.record_type     = "port-index-entry";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"port\",\"nga\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[nga-world-port-index] emitted %d\n", n);
  return 0;
}

static const source_def mar_nga_world_port_index_def = {
  .id = "nga-world-port-index", .collector = "maritime",
  .name = "NGA World Port Index (Pub 150)",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset", .url = WPI_URL,
  .description = "The authoritative ~3,700-entry world port reference: location, harbour type and size, channel/anchorage/cargo-pier depths, max vessel dimensions, pilotage, tugs, repair and medical facilities for every major port.",
  .license = "US Government work (NGA), public domain",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_nga_world_port_index_def)
