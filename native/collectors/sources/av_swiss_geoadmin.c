/* collectors/sources/av_swiss_geoadmin.c
 * Swiss Federal Office of Civil Aviation (BAZL/FOCA) via the geo.admin.ch
 * MapServer `identify` API — two keyless COORDINATE PIVOTS
 * (ctx->entity = "lat,lon"; anything else is a no-op).
 *
 *   SWISS_AIR_OBSTACLE  layers=all:ch.bazl.luftfahrthindernis
 *       Registered aviation obstacles near a point. Emits per obstacle:
 *       registrationnumber, obstacletype (BRIDGE, CRANE, MAST, CABLEWAY, wind
 *       turbine …), maxheightagl (m), topelevationamsl (m), radius,
 *       effectivedate, marking, lighting, group, airport, uuid.
 *   SWISS_DRONE_ZONE    layers=all:ch.bazl.einschraenkungen-drohnen
 *       Official Swiss UAS geographical zones covering a point. Emits:
 *       zone_name_en/de/fr/it, zone_message_*, zone_restriction_id (emitted
 *       VERBATIM, e.g. REQ_AUTHORISATION.MTOM_FROM — never translated),
 *       air_vol_lower/upper_limit and vref, period/time validity, and the
 *       authority contact block.
 *
 * GEOMETRY (R2): each results[] entry is already a GeoJSON Feature whose
 * `geometry` (Point / LineString with a third Z ordinate in metres AMSL /
 * MultiPolygon zone boundary) comes straight from the register. The QUERY
 * POINT is never plotted: quoting the work list, an empty `results` "is an
 * honest empty (return 0), never a reason to plot the query point", and for
 * the drone layer "absolutely do not fall back to plotting the query
 * coordinate as a 'zone'".
 *
 * TRAP (quoted): "`tolerance` is in screen pixels relative to the supplied
 * mapExtent/imageDisplay, so keep those consistent or the search radius
 * silently changes." We therefore derive mapExtent from the pivot point as a
 * fixed ±SWX_HALF° box against a fixed 500x500 display, which pins the ground
 * scale at SWX_HALF*2/500 degrees per pixel; SWX_TOL pixels is then a stable
 * ~11 km search radius rather than an accidental nationwide sweep.
 * The response caps at ~200 features — logged, not hidden.
 *
 * R3: no obstacle / no zone at the point → return 0. Fetch failure → 0 as
 * well, since an on-demand pivot must not quarantine itself.
 *
 * Licence: swisstopo / opendata.swiss "Open use" terms — free reuse including
 * commercial, source must be cited: "Federal Office of Civil Aviation FOCA /
 * geo.admin.ch". Keyless.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_common.inc"

#define SWX_LICENSE \
  "swisstopo / opendata.swiss 'Open use' — free reuse including commercial, " \
  "with citation: Federal Office of Civil Aviation FOCA / geo.admin.ch."
#define SWX_HALF 0.25     /* mapExtent half-width in degrees               */
#define SWX_TOL  100      /* pixels → ~0.1 deg (~11 km) at the scale above */

static cJSON *swx_identify(const source_ctx *ctx, const char *layer,
                           double lat, double lon, const char *tag) {
  char url[640];
  snprintf(url, sizeof url,
           "https://api3.geo.admin.ch/rest/services/api/MapServer/identify"
           "?geometry=%.6f,%.6f&geometryType=esriGeometryPoint&layers=all:%s"
           "&tolerance=%d&mapExtent=%.6f,%.6f,%.6f,%.6f"
           "&imageDisplay=500,500,96&geometryFormat=geojson&sr=4326",
           lon, lat, layer, SWX_TOL,
           lon - SWX_HALF, lat - SWX_HALF, lon + SWX_HALF, lat + SWX_HALF);
  const char *hdrs[] = { "Accept: application/json", AV_UA_HDR, NULL };
  char *body = jo_get(ctx, url, hdrs, tag);
  if (!body) return NULL;
  cJSON *doc = cJSON_Parse(body);
  free(body);
  return doc;
}

/* Copy the string-valued properties we want onto the Feature, add a title and
 * a uid, and hand the whole results[] array to geojson_emit_features. */
static int swx_run(const source_ctx *ctx, intel_sink *sink, const char *layer,
                   const char *record_type, const char *tag,
                   void (*decor)(cJSON *props)) {
  double lat = 0, lon = 0;
  if (!av_latlon(ctx->entity, &lat, &lon, NULL)) return 0;  /* wrong shape */
  cJSON *doc = swx_identify(ctx, layer, lat, lon, tag);
  if (!doc) return 0;                       /* on-demand pivot: honest empty */
  cJSON *res = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(res)) { cJSON_Delete(doc); return 0; }

  cJSON *f;
  cJSON_ArrayForEach(f, res) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    if (!cJSON_IsObject(p)) continue;
    decor(p);
    if (cJSON_GetObjectItem(p, "record_type"))
      cJSON_DeleteItemFromObject(p, "record_type");
    cJSON_AddStringToObject(p, "record_type", record_type);
    cJSON_AddStringToObject(p, "source",
                            "Federal Office of Civil Aviation FOCA / geo.admin.ch");
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("osint-search"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(tag));
    cJSON_AddItemToObject(p, "tags", tags);
  }
  int n = geojson_emit_features(sink, ctx->source_id, res);
  int total = cJSON_GetArraySize(res);
  cJSON_Delete(doc);
  fprintf(stderr, "[%s] emitted %d of %d returned%s\n", tag, n, total,
          total >= 200 ? " (response capped at ~200 by the API)" : "");
  return 0;
}

static void swx_set(cJSON *p, const char *k, const char *v) {
  if (!v || !*v) return;
  if (cJSON_GetObjectItem(p, k)) cJSON_DeleteItemFromObject(p, k);
  cJSON_AddStringToObject(p, k, v);
}

static void dec_obstacle(cJSON *p) {
  const char *reg  = jo_sv(p, "registrationnumber");
  const char *type = jo_sv(p, "obstacletype");
  const char *uuid = jo_sv(p, "uuid");
  double agl = 0, amsl = 0;
  av_dbl(p, "maxheightagl", &agl);
  av_dbl(p, "topelevationamsl", &amsl);
  char t[288];
  snprintf(t, sizeof t, "%s%s%s · %.1f m AGL / %.1f m AMSL",
           type ? type : "Obstacle", reg ? " " : "", reg ? reg : "", agl, amsl);
  swx_set(p, "title", t);
  swx_set(p, "uid", uuid ? uuid : reg);
}

static void dec_zone(cJSON *p) {
  const char *en = jo_sv(p, "zone_name_en");
  const char *de = jo_sv(p, "zone_name_de");
  const char *rid = jo_sv(p, "zone_restriction_id");
  const char *lbl = jo_sv(p, "label");
  const char *name = en ? en : (de ? de : lbl);
  char t[288];
  snprintf(t, sizeof t, "Drone zone: %s%s%s", name ? name : "restricted zone",
           rid ? " · " : "", rid ? rid : "");
  swx_set(p, "title", t);
  /* zone_restriction_id is an enumerated code — emitted verbatim, untranslated. */
  const cJSON *id = cJSON_GetObjectItem(p, "id");
  if (id && cJSON_IsNumber(id)) {
    char u[48];
    snprintf(u, sizeof u, "%.0f", id->valuedouble);
    swx_set(p, "uid", u);
  }
}

static int run_obstacle(const source_ctx *ctx, intel_sink *sink) {
  return swx_run(ctx, sink, "ch.bazl.luftfahrthindernis", "aviation-obstacle",
                 "SWISS_AIR_OBSTACLE", dec_obstacle);
}

static int run_zone(const source_ctx *ctx, intel_sink *sink) {
  return swx_run(ctx, sink, "ch.bazl.einschraenkungen-drohnen",
                 "drone-restriction-zone", "SWISS_DRONE_ZONE", dec_zone);
}

static const source_def av_swiss_obstacle_def = {
  .id = "SWISS_AIR_OBSTACLE", .collector = "osint",
  .name = "Swiss BAZL Aviation Obstacles (coordinate pivot)",
  .update_interval_sec = 0, .run = run_obstacle,
  .category = "transport", .type = "api",
  .url = "https://api3.geo.admin.ch/rest/services/api/MapServer/identify",
  .description = "Coordinate pivot (entity = \"lat,lon\") into the Swiss Federal Office of Civil Aviation obstacle register: registered masts, cranes, cableways, bridges and wind turbines near a point, with AGL height, AMSL top, marking and lighting status.",
  .license = SWX_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_swiss_obstacle_def)

static const source_def av_swiss_zone_def = {
  .id = "SWISS_DRONE_ZONE", .collector = "osint",
  .name = "Swiss BAZL Drone Restriction Zones (coordinate pivot)",
  .update_interval_sec = 0, .run = run_zone,
  .category = "transport", .type = "api",
  .url = "https://api3.geo.admin.ch/rest/services/api/MapServer/identify",
  .description = "Coordinate pivot (entity = \"lat,lon\") into Switzerland's official UAS geographical zones — where drone flight is prohibited, restricted or requires authorisation, with the machine-readable restriction code and multilingual zone name.",
  .license = SWX_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_swiss_zone_def)
