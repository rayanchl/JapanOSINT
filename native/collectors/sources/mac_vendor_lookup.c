/* collectors/osint/sources/mac_vendor_lookup.c
 * OSINT service — faithful port of OSINTsaas osint_tools/network_utils.c
 * (MAC_VENDOR_LOOKUP handle_mac_lookup; ctx->entity = MAC). On-demand
 * (interval 0). No API keys. Reproduces lookup_mac_vendor via
 * api.maclookup.app/v2/macs/<oui>.
 *
 * PER-RECORD EMIT: single-entity lookup yielding ONE real record. Emits ONE
 * clean intel_item carrying the real vendor fields directly (vendor, country,
 * address) — NOT a {success,confidence,data,results[]} envelope. title =
 * "<vendor>", remote_key = "mac:<oui>". On HTTP failure / vendor not found →
 * emit NOTHING and return 0 (honest empty). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int run_mac(const source_ctx *ctx, intel_sink *sink) {
  const char *mac = ctx->entity;
  if (!mac || !*mac) return 0;

  /* Normalize to the 6-hex-digit OUI (== upstream lookup_mac_vendor). */
  char oui[18];
  int j = 0;
  for (int i = 0; mac[i] && j < 17; i++) {
    char c = mac[i];
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F'))
      oui[j++] = c;
  }
  oui[j] = '\0';
  if (strlen(oui) >= 6) oui[6] = '\0';

  char url[256];
  snprintf(url, sizeof url, "https://api.maclookup.app/v2/macs/%s", oui);
  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  cJSON *json = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);

  /* HTTP failure → honest empty. */
  if (!json) return 0;

  /* Vendor not found → honest empty. */
  cJSON *ok = cJSON_GetObjectItem(json, "success");
  if (!ok || !cJSON_IsTrue(ok)) { cJSON_Delete(json); return 0; }

  cJSON *co = cJSON_GetObjectItem(json, "company");
  cJSON *cn = cJSON_GetObjectItem(json, "country");
  cJSON *ad = cJSON_GetObjectItem(json, "address");
  const char *vendor = (co && co->valuestring) ? co->valuestring : NULL;
  if (!vendor || !*vendor) { cJSON_Delete(json); return 0; }

  /* Real fetched vendor fields, emitted directly as the body. */
  cJSON *body = cJSON_CreateObject();
  cJSON_AddStringToObject(body, "mac_address", mac);
  cJSON_AddStringToObject(body, "vendor", vendor);
  if (cn && cn->valuestring) cJSON_AddStringToObject(body, "country", cn->valuestring);
  if (ad && ad->valuestring) cJSON_AddStringToObject(body, "address", ad->valuestring);
  char *bj = cJSON_PrintUnformatted(body);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "MAC_VENDOR_LOOKUP");
  cJSON_AddStringToObject(props, "entity", mac);
  cJSON_AddStringToObject(props, "vendor", vendor);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[64];
  snprintf(rk, sizeof rk, "mac:%s", oui);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = vendor;
  it.body            = bj;
  it.summary         = (cn && cn->valuestring) ? cn->valuestring : vendor;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"MAC_VENDOR_LOOKUP\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(body);
  cJSON_Delete(json);
  return rc >= 0 ? 0 : -1;
}

static const source_def mac_vendor_lookup_def = {
  .id = "MAC_VENDOR_LOOKUP", .collector = "osint",
  .name = "MAC Vendor Lookup", .name_ja = "MACベンダー照会",
  .update_interval_sec = 0, .run = run_mac,
  .category = "cyber", .type = "api",
  .url = "internal://osint/mac-vendor-lookup",
  .description = "Resolve a MAC address OUI to a vendor via maclookup.app.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mac_vendor_lookup_def)
