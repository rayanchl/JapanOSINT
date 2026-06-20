/* collectors/osint/sources/license_plate.c — LICENSE_PLATE_LOOKUP, ported from
 * OSINTsaas vehicle_lookup.c onto the JapanOSINT source ABI.
 *
 * Plate → owner/vehicle lookup is a regulated DMV/parivahan function with no
 * lawful public API (the OSINTsaas tool only built links to gated government
 * portals and paid services). Per the no-fabrication rule this collector emits
 * nothing unless a private plate-lookup endpoint is wired via env
 * (PLATE_LOOKUP_URL, a "%s" template + optional PLATE_LOOKUP_API_KEY bearer).
 * Registered for OSINTsaas parity; honest-empty by default. */
#include "../../source.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void urlenc(const char *s, char *out, size_t cap) {
  size_t o = 0;
  for (const char *p = s; *p && o + 4 < cap; p++) {
    unsigned char c = (unsigned char)*p;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out[o++] = (char)c;
    else o += (size_t)snprintf(out + o, cap - o, "%%%02X", c);
  }
  out[o] = 0;
}

static int plate_run(const source_ctx *ctx, intel_sink *sink) {
  const char *plate = ctx->entity;
  if (!plate || !*plate) return 0;
  const char *tmpl = getenv("PLATE_LOOKUP_URL");   /* "%s" = url-encoded plate */
  if (!tmpl || !strstr(tmpl, "%s")) return 0;       /* honest empty — no lawful public source */

  char enc[128]; urlenc(plate, enc, sizeof enc);
  char url[600]; snprintf(url, sizeof url, tmpl, enc);
  const char *key = getenv("PLATE_LOOKUP_API_KEY");
  char auth[256] = {0}; const char *headers[2] = { NULL, NULL };
  if (key && *key) { snprintf(auth, sizeof auth, "Authorization: Bearer %s", key); headers[0] = auth; }

  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, headers[0] ? headers : NULL, NULL, 0, 12000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body);
  /* pass the provider payload through verbatim (or raw string if not JSON) */
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "plate", plate);
  if (j) cJSON_AddItemToObject(data, "result", j);
  else cJSON_AddStringToObject(data, "result", hr.body);
  http_response_free(&hr);
  char *bj = cJSON_PrintUnformatted(data);
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "LICENSE_PLATE_LOOKUP");
  cJSON_AddStringToObject(props, "entity", plate);
  char *pj = cJSON_PrintUnformatted(props);
  char rk[128], title[160];
  snprintf(rk, sizeof rk, "plate:%s", plate);
  snprintf(title, sizeof title, "Plate lookup — %s", plate);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.summary = "plate lookup";
  it.record_type = "osint_service_result"; it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"LICENSE_PLATE_LOOKUP\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static const source_def plate_def = {
  .id = "LICENSE_PLATE_LOOKUP", .collector = "osint", .name = "License Plate Lookup", .run = plate_run,
  .category = "investigation", .type = "api", .url = "internal://plate",
  .description = "Plate → vehicle/owner via a configured DMV endpoint (PLATE_LOOKUP_URL); no public source.",
  .free_tier = 0,
};
REGISTER_SOURCE(plate_def)
