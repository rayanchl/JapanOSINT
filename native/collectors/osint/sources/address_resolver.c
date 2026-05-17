/* collectors/osint/sources/address_resolver.c
 * OSINT service — faithful port of OSINTsaas osint_tools/geo_osint.c
 * (handle_geo_lookup → geo_analyze_location). Canonical service
 * ADDRESS_RESOLVER (osint_dispatcher.c service_registry[]: → handle_geo_lookup
 * → geo_analyze_location). On-demand (interval 0); ctx->entity = a location
 * query. geo_analyze_location: geo_geocode_address (OSM Nominatim
 * /search?q=...&format=json&addressdetails=1&limit=5) → on success wrap as
 * {location_query,geocode_data:<{query,results:[{latitude,longitude,
 * display_name,type,importance,address_details}]}>} confidence 85 success=true;
 * on geocode failure → success=false "Failed to analyze location - geocoding
 * failed". Reproduced verbatim. Emits ONE osint_service_result row; body =
 * {success,confidence,data,error?} envelope, like ip_geolocation.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char *url_encode(const char *s) {
  static const char *unres = "-_.~";
  size_t n = strlen(s);
  char *out = malloc(n * 3 + 1);
  if (!out) return NULL;
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(unres, c))
      out[w++] = (char)c;
    else { sprintf(out + w, "%%%02X", c); w += 3; }
  }
  out[w] = 0;
  return out;
}

/* geo_geocode_address: returns the {query,results:[...]} object, or NULL on
 * any failure (encode/http/parse/empty) — matching the OSINTsaas
 * success=false branches of geo_geocode_address that propagate up to
 * geo_analyze_location's "geocoding failed". */
static cJSON *geo_geocode_address(http_client *http, const char *address) {
  char *enc = url_encode(address);
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

  int n = cJSON_GetArraySize(json);
  if (n == 0) { cJSON_Delete(json); return NULL; }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", address);
  cJSON *results = cJSON_CreateArray();

  for (int i = 0; i < n && i < 5; i++) {
    cJSON *loc = cJSON_GetArrayItem(json, i);
    cJSON *lat = cJSON_GetObjectItem(loc, "lat");
    cJSON *lon = cJSON_GetObjectItem(loc, "lon");
    cJSON *dn = cJSON_GetObjectItem(loc, "display_name");
    cJSON *type = cJSON_GetObjectItem(loc, "type");
    cJSON *imp = cJSON_GetObjectItem(loc, "importance");
    if (lat && lon && cJSON_IsString(lat) && cJSON_IsString(lon)) {
      cJSON *r = cJSON_CreateObject();
      /* upstream emits latitude/longitude as raw JSON numbers from the
       * string values; cJSON_CreateNumber(atof) preserves the value. */
      cJSON_AddNumberToObject(r, "latitude", atof(lat->valuestring));
      cJSON_AddNumberToObject(r, "longitude", atof(lon->valuestring));
      if (dn && cJSON_IsString(dn)) {
        char buf[256];
        snprintf(buf, sizeof buf, "%.200s", dn->valuestring);
        cJSON_AddStringToObject(r, "display_name", buf);
      }
      if (type && cJSON_IsString(type))
        cJSON_AddStringToObject(r, "type", type->valuestring);
      if (imp && cJSON_IsNumber(imp))
        cJSON_AddNumberToObject(r, "importance", imp->valuedouble);
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
        cJSON_AddItemToObject(r, "address_details", ad);
      }
      cJSON_AddItemToArray(results, r);
    }
  }
  cJSON_AddItemToObject(root, "results", results);
  cJSON_Delete(json);
  return root;
}

static int emit_one(intel_sink *sink, const char *svc, const char *entity,
                    int success, int confidence, cJSON *data /*owned*/,
                    const char *error) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  if (data) cJSON_AddItemToObject(env, "data", data);
  else cJSON_AddNullToObject(env, "data");
  if (error) cJSON_AddStringToObject(env, "error", error);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", svc);
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320];
  snprintf(rk, sizeof rk, "%s:%s", svc, entity);
  char title[360];
  snprintf(title, sizeof title, "%s — %s", svc, entity);
  char tags[96];
  snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", svc);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = success ? "location analyzed"
                       : (error ? error : "lookup failed");
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = tags;
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run_svc(const source_ctx *ctx, intel_sink *sink, const char *svc) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  cJSON *geo = geo_geocode_address(ctx->http, q);
  if (!geo) {
    /* geo_analyze_location: geocode failed → success=false. */
    return emit_one(sink, svc, q, 0, 0, NULL,
                    "Failed to analyze location - geocoding failed");
  }
  /* geo_analyze_location wraps: {location_query, geocode_data:<geo>}. */
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "location_query", q);
  cJSON_AddItemToObject(data, "geocode_data", geo);
  return emit_one(sink, svc, q, 1, 85, data, NULL);
}

static int run_addr(const source_ctx *ctx, intel_sink *sink) {
  return run_svc(ctx, sink, "ADDRESS_RESOLVER");
}

static const source_def address_resolver_def = {
  .id = "ADDRESS_RESOLVER", .collector = "osint",
  .name = "Address Resolver", .name_ja = "住所解決",
  .update_interval_sec = 0, .run = run_addr,
};
REGISTER_SOURCE(address_resolver_def)
