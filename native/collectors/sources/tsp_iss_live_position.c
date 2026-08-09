/* collectors/sources/tsp_iss_live_position.c
 * ISS live sub-satellite point — a live space object on the map without an
 * SGP4 implementation in this process.
 * Endpoint: https://api.wheretheiss.at/v1/satellites/25544  (keyless)
 * Emits: name, NORAD id, latitude, longitude, altitude (km), velocity (km/h),
 *   illumination state, radio footprint radius (km), timestamp, day number and
 *   the sub-solar point.
 * Geo (R2): "latitude/longitude are real propagated numbers ... this is a
 *   computed sub-point, not an address" — genuine upstream geometry, and it is
 *   labelled in properties as a derived SGP4 sub-satellite point so nobody
 *   mistakes it for an authoritative registry position.
 * parse_notes honoured: "Single JSON object, not an array"; "units defaults to
 *   kilometers"; "Path parameter is the NORAD catalogue number".
 * Licence: wheretheiss.at public API — keyless, documented ~1 request/second
 *   limit, free use. Derived product (propagation of public TLEs) — cite
 *   CelesTrak / Space-Track for authoritative elements.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISS_NORAD 25544
#define ISS_URL "https://api.wheretheiss.at/v1/satellites/25544"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, ISS_URL, 20000);
  if (!doc) {
    fprintf(stderr, "[iss-live-position] fetch failed\n");
    return -1;
  }
  if (!cJSON_IsObject(doc)) {
    fprintf(stderr, "[iss-live-position] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  double lat, lon;
  if (!jo_num(doc, "latitude", &lat) || !jo_num(doc, "longitude", &lon) ||
      lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
    /* no usable position in the reply: honest empty rather than a fake point */
    fprintf(stderr, "[iss-live-position] no position in reply; emitted 0\n");
    cJSON_Delete(doc);
    return 0;
  }

  double alt = 0, vel = 0, foot = 0, ts = 0, solar_lat = 0, solar_lon = 0, dnum = 0;
  int has_alt  = jo_num(doc, "altitude", &alt);
  int has_vel  = jo_num(doc, "velocity", &vel);
  int has_foot = jo_num(doc, "footprint", &foot);
  int has_ts   = jo_num(doc, "timestamp", &ts);
  const char *vis = jo_sv(doc, "visibility");
  const char *name = jo_sv(doc, "name");
  const char *units = jo_sv(doc, "units");

  cJSON *pr = cJSON_CreateObject();
  cJSON_AddStringToObject(pr, "object", name ? name : "iss");
  cJSON_AddNumberToObject(pr, "norad_cat_id", ISS_NORAD);
  cJSON_AddNumberToObject(pr, "latitude", lat);
  cJSON_AddNumberToObject(pr, "longitude", lon);
  if (has_alt)  cJSON_AddNumberToObject(pr, "altitude_km", alt);
  if (has_vel)  cJSON_AddNumberToObject(pr, "velocity_km_h", vel);
  if (has_foot) cJSON_AddNumberToObject(pr, "footprint_km", foot);
  if (has_ts)   cJSON_AddNumberToObject(pr, "timestamp_epoch", ts);
  if (vis)      cJSON_AddStringToObject(pr, "visibility", vis);
  if (units)    cJSON_AddStringToObject(pr, "units", units);
  if (jo_num(doc, "daynum", &dnum))       cJSON_AddNumberToObject(pr, "daynum", dnum);
  if (jo_num(doc, "solar_lat", &solar_lat)) cJSON_AddNumberToObject(pr, "solar_lat", solar_lat);
  if (jo_num(doc, "solar_lon", &solar_lon)) cJSON_AddNumberToObject(pr, "solar_lon", solar_lon);
  cJSON_AddStringToObject(pr, "geo_subject", "sub-satellite point");
  cJSON_AddStringToObject(pr, "geo_provenance",
    "SGP4 propagation of public TLEs by wheretheiss.at — derived, not a registry position");
  cJSON_AddStringToObject(pr, "source", "wheretheiss.at");
  char *pj = cJSON_PrintUnformatted(pr);
  cJSON_Delete(pr);

  char title[192], summary[224], key[64];
  snprintf(title, sizeof title, "ISS (NORAD 25544) at %.4f, %.4f", lat, lon);
  if (has_ts) snprintf(key, sizeof key, "25544|%.0f", ts);
  else        snprintf(key, sizeof key, "25544");
  snprintf(summary, sizeof summary, "%s%.1f km · %s%.0f km/h · %s",
           has_alt ? "altitude " : "altitude n/a ", has_alt ? alt : 0.0,
           has_vel ? "velocity " : "velocity n/a ", has_vel ? vel : 0.0,
           vis ? vis : "illumination n/a");

  intel_item it = {0};
  it.remote_key      = key;
  it.title           = title;
  it.summary         = summary;
  it.link            = "https://wheretheiss.at/";
  it.lang            = "en";
  it.record_type     = "satellite-subpoint";
  it.has_geo         = 1;
  it.lat             = lat;
  it.lon             = lon;
  it.properties_json = pj;
  it.tags_json       = "[\"space\",\"satellite\",\"iss\"]";
  int emitted = (sink->emit(sink, &it) >= 0) ? 1 : 0;

  free(pj);
  cJSON_Delete(doc);
  fprintf(stderr, "[iss-live-position] emitted %d\n", emitted);
  return 0;
}

static const source_def tsp_iss_live_position_def = {
  .id = "iss-live-position", .collector = "satellite",
  .name = "ISS live sub-satellite point",
  .update_interval_sec = 300, .run = run,
  .category = "satellite", .type = "api", .url = ISS_URL,
  .description = "Current sub-satellite point, altitude, velocity, illumination state and radio footprint radius for the ISS.",
  .license = "wheretheiss.at public API — keyless, free use, ~1 request/second. Derived from public TLEs; cite CelesTrak/Space-Track for authoritative elements.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_iss_live_position_def)
