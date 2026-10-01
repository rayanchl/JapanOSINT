/* collectors/osint/sources/reverse_geocoding.c
 * OSINT service — faithful port of OSINTsaas osint_tools/geo_osint.c
 * (handle_geo_lookup → geo_analyze_location). Canonical service
 * REVERSE_GEOCODING (osint_dispatcher.c service_registry[]: → handle_geo_lookup
 * → geo_analyze_location). On-demand (interval 0); ctx->entity = a location
 * query. Fetch: OSM Nominatim /search?q=...&format=json&addressdetails=1
 * &limit=5.
 *
 * PER-RECORD EMIT: emits ONE osint_service_result row per geocode result
 * (keyed by Nominatim place_id, else osm_type+osm_id, else lat:lon), each with
 * real geo (lat/lon) so individual matches surface as distinct search/map
 * items. If the query yields zero results (or HTTP fails), emits nothing and
 * returns 0 (honest empty — no seeded rows). */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Fetch the raw Nominatim results array, or NULL on any failure
 * (encode/http/parse/non-array). Caller owns the returned cJSON. */
static cJSON *geo_geocode_fetch(http_client *http, const char *address) {
  char *enc = jo_urlencode(address);
  if (!enc) return NULL;
  char url[2560];
  snprintf(url, sizeof url,
    "https://nominatim.openstreetmap.org/search?q=%s&format=json&addressdetails=1&limit=5",
    enc);
  free(enc);

  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json || !cJSON_IsArray(json)) { if (json) cJSON_Delete(json); return NULL; }
  return json;
}

/* Emit one intel row for a single Nominatim result. Returns 1 if emitted. */
static int emit_result(intel_sink *sink, const char *svc, const char *entity,
                       cJSON *loc) {
  if (!loc) return 0;
  cJSON *lat = cJSON_GetObjectItem(loc, "lat");
  cJSON *lon = cJSON_GetObjectItem(loc, "lon");
  if (!lat || !lon || !cJSON_IsString(lat) || !cJSON_IsString(lon)) return 0;

  cJSON *dn   = cJSON_GetObjectItem(loc, "display_name");
  cJSON *type = cJSON_GetObjectItem(loc, "type");
  cJSON *imp  = cJSON_GetObjectItem(loc, "importance");

  double dlat = atof(lat->valuestring);
  double dlon = atof(lon->valuestring);

  /* Per-record body: this single geocode result. */
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "latitude", dlat);
  cJSON_AddNumberToObject(data, "longitude", dlon);
  if (dn && cJSON_IsString(dn)) {
    char buf[256];
    snprintf(buf, sizeof buf, "%.200s", dn->valuestring);
    cJSON_AddStringToObject(data, "display_name", buf);
  }
  if (type && cJSON_IsString(type))
    cJSON_AddStringToObject(data, "type", type->valuestring);
  if (imp && cJSON_IsNumber(imp))
    cJSON_AddNumberToObject(data, "importance", imp->valuedouble);
  cJSON *addr = cJSON_GetObjectItem(loc, "address");
  if (addr) {
    cJSON *ad = cJSON_CreateObject();
    cJSON *city = cJSON_GetObjectItem(addr, "city");
    cJSON *state = cJSON_GetObjectItem(addr, "state");
    cJSON *country = cJSON_GetObjectItem(addr, "country");
    cJSON *postcode = cJSON_GetObjectItem(addr, "postcode");
    if (city && cJSON_IsString(city))
      cJSON_AddStringToObject(ad, "city", city->valuestring);
    if (state && cJSON_IsString(state))
      cJSON_AddStringToObject(ad, "state", state->valuestring);
    if (country && cJSON_IsString(country))
      cJSON_AddStringToObject(ad, "country", country->valuestring);
    if (postcode && cJSON_IsString(postcode))
      cJSON_AddStringToObject(ad, "postcode", postcode->valuestring);
    cJSON_AddItemToObject(data, "address_details", ad);
  }
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", svc);
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  /* Stable per-result key: Nominatim place_id, else osm_type:osm_id, else
   * lat:lon. place_id may be a number or string depending on response. */
  char rk[160];
  cJSON *place_id = cJSON_GetObjectItem(loc, "place_id");
  cJSON *osm_type = cJSON_GetObjectItem(loc, "osm_type");
  cJSON *osm_id   = cJSON_GetObjectItem(loc, "osm_id");
  if (place_id && cJSON_IsNumber(place_id))
    snprintf(rk, sizeof rk, "geo:%.0f", place_id->valuedouble);
  else if (place_id && cJSON_IsString(place_id))
    snprintf(rk, sizeof rk, "geo:%s", place_id->valuestring);
  else if (osm_type && cJSON_IsString(osm_type) && osm_id && cJSON_IsNumber(osm_id))
    snprintf(rk, sizeof rk, "geo:%s%.0f", osm_type->valuestring, osm_id->valuedouble);
  else
    snprintf(rk, sizeof rk, "geo:%.6f:%.6f", dlat, dlon);

  char title[360];
  snprintf(title, sizeof title, "%.300s",
           (dn && cJSON_IsString(dn)) ? dn->valuestring : entity);
  char summary[128];
  snprintf(summary, sizeof summary, "%.6f, %.6f", dlat, dlon);
  char tags[96];
  snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", svc);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = summary;
  it.has_geo = 1;
  it.lat = dlat;
  it.lon = dlon;
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = tags;
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run_svc(const source_ctx *ctx, intel_sink *sink, const char *svc) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;

  cJSON *json = geo_geocode_fetch(ctx->http, q);
  if (!json) return 0;   /* HTTP/parse failure → honest empty */

  int emitted = 0;
  int n = cJSON_GetArraySize(json);
  for (int i = 0; i < n && i < 5; i++)
    emitted += emit_result(sink, svc, q, cJSON_GetArrayItem(json, i));
  cJSON_Delete(json);
  (void)emitted;          /* zero results → emitted nothing, still success */
  return 0;
}

static int run_revgeo(const source_ctx *ctx, intel_sink *sink) {
  return run_svc(ctx, sink, "REVERSE_GEOCODING");
}

static const source_def reverse_geocoding_def = {
  .id = "REVERSE_GEOCODING", .collector = "osint",
  .name = "Reverse Geocoding", .name_ja = "逆ジオコーディング",
  .update_interval_sec = 0, .run = run_revgeo,
  .category = "geospatial", .type = "api",
  .url = "internal://osint/reverse-geocoding",
  .description = "Resolve coordinates to a place or address.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reverse_geocoding_def)
