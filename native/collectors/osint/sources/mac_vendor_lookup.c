/* collectors/osint/sources/mac_vendor_lookup.c
 * OSINT service — faithful port of OSINTsaas osint_tools/network_utils.c
 * (MAC_VENDOR_LOOKUP handle_mac_lookup; ctx->entity = MAC).
 * On-demand (interval 0). No API keys. Reproduces lookup_mac_vendor via
 * api.maclookup.app. The upstream handler loops over entities[]; the pipeline
 * pivots one entity at a time so it runs over ctx->entity (count==1) — the
 * result envelope keeps the upstream {service,total_*,...,results:[...]} shape
 * with a single result. success=true, confidence 90. Emits ONE
 * osint_service_result row; body = {success,confidence,data} envelope, like
 * ip_geolocation.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* lookup_mac_vendor via api.maclookup.app/v2/macs/<oui>. */
static cJSON *lookup_mac_vendor(http_client *http, const char *mac) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "mac_address", mac);

  char norm[18];
  int j = 0;
  for (int i = 0; mac[i] && j < 17; i++) {
    char c = mac[i];
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F'))
      norm[j++] = c;
  }
  norm[j] = '\0';
  if (strlen(norm) >= 6) norm[6] = '\0';

  char url[256];
  snprintf(url, sizeof url, "https://api.maclookup.app/v2/macs/%s", norm);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    http_response_free(&hr);
    cJSON_AddStringToObject(result, "error", "Lookup failed");
    return result;
  }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (json) {
    cJSON *ok = cJSON_GetObjectItem(json, "success");
    if (ok && cJSON_IsTrue(ok)) {
      cJSON *co = cJSON_GetObjectItem(json, "company");
      cJSON *cn = cJSON_GetObjectItem(json, "country");
      cJSON *ad = cJSON_GetObjectItem(json, "address");
      if (co && co->valuestring) cJSON_AddStringToObject(result, "vendor", co->valuestring);
      if (cn && cn->valuestring) cJSON_AddStringToObject(result, "country", cn->valuestring);
      if (ad && ad->valuestring) cJSON_AddStringToObject(result, "address", ad->valuestring);
      cJSON_AddBoolToObject(result, "found", 1);
    } else {
      cJSON_AddBoolToObject(result, "found", 0);
      cJSON_AddStringToObject(result, "message", "MAC vendor not found");
    }
    cJSON_Delete(json);
  }
  return result;
}

static int emit_one(intel_sink *sink, const char *svc, const char *entity,
                    int confidence, cJSON *data /*owned*/) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);          /* OSINTsaas: always true */
  cJSON_AddNumberToObject(env, "confidence", confidence);
  cJSON_AddItemToObject(env, "data", data);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", svc);
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", 1);
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
  it.summary = "network util result";
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = tags;
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run_mac(const source_ctx *ctx, intel_sink *sink) {
  const char *mac = ctx->entity;
  if (!mac || !*mac) return -1;
  cJSON *root = cJSON_CreateObject();
  cJSON *results = cJSON_CreateArray();
  cJSON *mr = lookup_mac_vendor(ctx->http, mac);
  cJSON *f = cJSON_GetObjectItem(mr, "found");
  int found = (f && cJSON_IsTrue(f)) ? 1 : 0;
  cJSON_AddItemToArray(results, mr);
  cJSON_AddStringToObject(root, "service", "MAC_VENDOR_LOOKUP");
  cJSON_AddNumberToObject(root, "total_queried", 1);
  cJSON_AddNumberToObject(root, "vendors_found", found);
  cJSON_AddItemToObject(root, "results", results);
  return emit_one(sink, "MAC_VENDOR_LOOKUP", mac, 90, root);
}

static const source_def mac_vendor_lookup_def = {
  .id = "MAC_VENDOR_LOOKUP", .collector = "osint",
  .name = "MAC Vendor Lookup", .name_ja = "MACベンダー照会",
  .update_interval_sec = 0, .run = run_mac,
};
REGISTER_SOURCE(mac_vendor_lookup_def)
