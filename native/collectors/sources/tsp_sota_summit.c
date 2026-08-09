/* collectors/sources/tsp_sota_summit.c
 * OSINT service — SOTA_SUMMIT. On-demand entity pivot: given a SOTA summit
 * reference (entity_type "summit-ref", e.g. "W1/CR-001"), resolve it to the
 * surveyed summit. This is what makes sota-spots mappable.
 * Endpoint: https://api-db2.sota.org.uk/api/summits/{associationCode}/{regionCode}-{number}
 *   — i.e. the summit reference appended verbatim (keyless).
 * Emits: association and region names/codes, summit code and name, activation
 *   points, altitude in metres and feet, Maidenhead locator, validity window,
 *   valid flag, any access restrictions, and the summit coordinates.
 *
 * parse_notes honoured, quoted:
 *  - "URL form is /api/summits/{associationCode}/{regionCode}-{number}, i.e.
 *    the summitCode with the association stripped ('W1/CR-001' -> /W1/CR-001)".
 *  - "latitude/longitude are surveyed summit coordinates (gridRef1 = longitude,
 *    gridRef2 = latitude — reversed from the named fields, use
 *    latitude/longitude)": the named fields are used and gridRef1/2 are
 *    deliberately ignored, which is the trap here (R2).
 *  - "An invalid reference returns an empty/near-empty object, which must be
 *    treated as an honest miss (return 0) not an error."
 * A NULL, empty or wrong-shaped entity returns 0 without touching the network.
 * Licence: SOTA database public API — keyless, community use with attribution.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Accept "AA/BB-123" style references: association / region '-' number. */
static int normalise_ref(const char *in, char *out, size_t cap) {
  if (!in || !*in) return 0;
  size_t n = strlen(in);
  if (n < 5 || n + 1 > cap) return 0;
  size_t j = 0;
  int slashes = 0, dashes = 0, digits_after_dash = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)in[i];
    if (c == ' ') continue;
    if (c == '/') { slashes++; out[j++] = '/'; continue; }
    if (c == '-') { dashes++; out[j++] = '-'; continue; }
    if (!isalnum(c)) return 0;
    if (dashes && isdigit(c)) digits_after_dash++;
    out[j++] = (char)toupper(c);
  }
  out[j] = '\0';
  if (slashes != 1 || dashes != 1 || digits_after_dash < 1) return 0;
  const char *slash = strchr(out, '/');
  const char *dash  = strchr(out, '-');
  if (!slash || !dash || dash < slash) return 0;
  if (slash == out || dash == slash + 1 || !dash[1]) return 0;
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char ref[64];
  if (!normalise_ref(ctx->entity, ref, sizeof ref)) return 0;  /* honest miss */

  char url[192];
  snprintf(url, sizeof url, "https://api-db2.sota.org.uk/api/summits/%s", ref);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (sota-summit)", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 20000, 1, &hr);
  if (rc != 0) {                               /* transport failure (R3) */
    fprintf(stderr, "[SOTA_SUMMIT] transport error for %s\n", ref);
    http_response_free(&hr);
    return -1;
  }
  if (hr.status != 200 || !hr.body) {          /* unknown reference: empty */
    fprintf(stderr, "[SOTA_SUMMIT] %s http status=%ld\n", ref, hr.status);
    http_response_free(&hr);
    return 0;
  }
  char *body = hr.body;
  hr.body = NULL;
  http_response_free(&hr);

  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!cJSON_IsObject(doc)) {
    if (doc) cJSON_Delete(doc);
    return 0;
  }

  const char *code = jo_sv(doc, "summitCode");
  const char *name = jo_sv(doc, "name");
  if (!code && !name) {           /* near-empty object = invalid reference */
    cJSON_Delete(doc);
    fprintf(stderr, "[SOTA_SUMMIT] %s not found\n", ref);
    return 0;
  }

  /* R2: use the NAMED latitude/longitude fields. gridRef1/gridRef2 are
   * reversed (gridRef1 = longitude) and are not used for geometry. */
  double lat = 0, lon = 0;
  int has_geo = 0;
  if (jo_num(doc, "latitude", &lat) && jo_num(doc, "longitude", &lon) &&
      lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
      !(lat == 0.0 && lon == 0.0))
    has_geo = 1;

  cJSON *pr = cJSON_CreateObject();
  cJSON_AddStringToObject(pr, "service", "SOTA_SUMMIT");
  cJSON_AddStringToObject(pr, "query", ref);
  jo_add_str(pr, "summit_code", code);
  jo_add_str(pr, "name", name);
  jo_add_str(pr, "association_code", jo_sv(doc, "associationCode"));
  jo_add_str(pr, "association_name", jo_sv(doc, "associationName"));
  jo_add_str(pr, "region_code", jo_sv(doc, "regionCode"));
  jo_add_str(pr, "region_name", jo_sv(doc, "regionName"));
  jo_add_str(pr, "locator", jo_sv(doc, "locator"));
  jo_add_str(pr, "valid_from", jo_sv(doc, "validFrom"));
  jo_add_str(pr, "valid_to", jo_sv(doc, "validTo"));
  jo_add_str(pr, "restriction_list", jo_sv(doc, "restrictionList"));
  double d;
  if (jo_num(doc, "points", &d)) cJSON_AddNumberToObject(pr, "activation_points", d);
  if (jo_num(doc, "altM", &d))   cJSON_AddNumberToObject(pr, "altitude_m", d);
  if (jo_num(doc, "altFt", &d))  cJSON_AddNumberToObject(pr, "altitude_ft", d);
  const cJSON *valid = cJSON_GetObjectItem(doc, "valid");
  if (cJSON_IsBool(valid)) cJSON_AddBoolToObject(pr, "valid", cJSON_IsTrue(valid));
  if (has_geo) {
    cJSON_AddNumberToObject(pr, "latitude", lat);
    cJSON_AddNumberToObject(pr, "longitude", lon);
    cJSON_AddStringToObject(pr, "geo_subject", "surveyed summit position");
  }
  cJSON_AddStringToObject(pr, "source", "SOTA database summit API");
  cJSON_AddBoolToObject(pr, "success", 1);
  char *pj = cJSON_PrintUnformatted(pr);
  cJSON_Delete(pr);

  double alt = 0, pts = 0;
  int has_alt = jo_num(doc, "altM", &alt), has_pts = jo_num(doc, "points", &pts);
  char title[256], summary[256];
  snprintf(title, sizeof title, "%s — %s", code ? code : ref,
           name ? name : "SOTA summit");
  char atxt[48] = "", ptxt[48] = "";
  if (has_alt) snprintf(atxt, sizeof atxt, "%.0f m", alt);
  if (has_pts) snprintf(ptxt, sizeof ptxt, " · %.0f points", pts);
  snprintf(summary, sizeof summary, "%s%s", has_alt ? atxt : "altitude n/a", ptxt);

  intel_item it = {0};
  it.remote_key      = code ? code : ref;
  it.title           = title;
  it.summary         = summary;
  it.link            = "https://www.sotadata.org.uk/en/summits";
  it.lang            = "en";
  it.record_type     = "sota-summit";
  it.has_geo         = has_geo;
  it.lat             = lat;
  it.lon             = lon;
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"amateur-radio\",\"sota\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(doc);
  fprintf(stderr, "[SOTA_SUMMIT] emitted 1 (%s)\n", code ? code : ref);
  return 0;
}

static const source_def tsp_sota_summit_def = {
  .id = "SOTA_SUMMIT", .collector = "osint",
  .name = "SOTA summit lookup (association/summit reference pivot)",
  .update_interval_sec = 0, .run = run,
  .category = "telecom", .type = "api",
  .url = "https://api-db2.sota.org.uk/api/summits/",
  .description = "Turns a SOTA summit reference (e.g. W1/CR-001) into a surveyed summit: name, association and region, altitude, activation points, Maidenhead locator, validity window, access restrictions and real coordinates.",
  .license = "SOTA database public API — keyless, community use with attribution.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(tsp_sota_summit_def)
