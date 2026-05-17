/* collectors/osint/sources/wifi_lookup.c
 * OSINT service — port of OSINTsaas osint_tools/wifi_lookup.c
 * (wifi_lookup → handle_wifi_lookup). Canonical SERVICE = WIFI_LOOKUP
 * (dispatcher row {SERVICE_WIFI_MAPPER, handle_wifi_lookup, "WIFI_LOOKUP",
 * true}; BSSID_LOOKUP aliases the same handler — WIFI_LOOKUP is the dedup id).
 * On-demand (interval 0); ctx->entity = a BSSID. mylnikov.org geolocation is
 * key-free; WiGLE needs WIGLE_API_NAME/WIGLE_API_TOKEN — the C original
 * degrades gracefully without them (skips WiGLE, adds wigle_status), so we do
 * the same rather than return 0. Faithfully reproduces normalize_bssid,
 * get_oui, local_vendor_lookup (built-in OUI table), query_mac_vendor
 * (api.macvendors.com), query_mylnikov (api.mylnikov.org → lat/lon/range),
 * query_wigle (gated), the location aggregation (mylnikov first, then WiGLE),
 * map_url and resources. Invalid BSSID → success=false, confidence 0 (matches
 * the C early-return). confidence 85 if a location was found else 50. Emits ONE
 * osint_service_result row like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct { const char *oui, *vendor; } oui_t;
static const oui_t VENDORS[] = {
  {"00:1A:2B","Cisco"},{"00:50:56","VMware"},{"AC:DE:48","Amazon"},
  {"F4:F5:D8","Google"},{"3C:5A:B4","Apple"},{"DC:A6:32","Apple"},
  {"B8:27:EB","Raspberry Pi"},{"00:0C:29","VMware"},{"00:15:5D","Microsoft Hyper-V"},
  {"08:00:27","VirtualBox"},{"00:1E:67","Intel"},{"00:1F:3B","Intel"},
  {"00:21:5C","Intel"},{"00:24:D7","Intel"},{"00:26:C7","Intel"},
  {"00:E0:4C","Realtek"},{"00:14:22","Dell"},{"00:1A:A0","Dell"},
  {"00:21:70","Dell"},{"00:23:AE","Dell"},{"18:A9:05","Hewlett-Packard"},
  {"3C:D9:2B","Hewlett-Packard"},{"00:0D:56","NETGEAR"},{"00:14:6C","NETGEAR"},
  {"00:1E:2A","NETGEAR"},{"00:22:3F","NETGEAR"},{"00:1D:7E","Linksys"},
  {"00:21:29","Linksys"},{"00:25:9C","Linksys"},{"C0:3F:0E","NETGEAR"},
  {"00:18:39","Cisco-Linksys"},{"00:1C:10","Cisco-Linksys"},{"00:1A:70","Cisco-Linksys"},
  {"00:25:00","Apple"},{"1C:1A:C0","Apple"},{"28:E1:4C","Apple"},
  {"A8:86:DD","Apple"},{"F0:DB:E2","Apple"},{NULL,NULL}
};

static char *normalize_bssid(const char *b) {
  if (!b) return NULL;
  char clean[18]; int j = 0;
  for (size_t i = 0; b[i] && j < 12; i++)
    if (isxdigit((unsigned char)b[i])) clean[j++] = (char)toupper((unsigned char)b[i]);
  clean[j] = 0;
  if (j != 12) return NULL;
  char *r = calloc(18, 1);
  if (!r) return NULL;
  snprintf(r, 18, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
    clean[0],clean[1],clean[2],clean[3],clean[4],clean[5],
    clean[6],clean[7],clean[8],clean[9],clean[10],clean[11]);
  return r;
}

static char *get_oui(const char *b) {
  char *n = normalize_bssid(b);
  if (!n) return NULL;
  char *o = calloc(9, 1);
  if (o) { strncpy(o, n, 8); o[8] = 0; }
  free(n);
  return o;
}

static const char *local_vendor(const char *oui) {
  if (!oui) return NULL;
  for (int i = 0; VENDORS[i].oui; i++)
    if (strcasecmp(VENDORS[i].oui, oui) == 0) return VENDORS[i].vendor;
  return NULL;
}

static void enc_colon(const char *in, char *out, size_t cap) {
  size_t j = 0;
  for (size_t i = 0; in[i] && j + 4 < cap; i++) {
    if (in[i] == ':') { out[j++] = '%'; out[j++] = '3'; out[j++] = 'A'; }
    else out[j++] = in[i];
  }
  out[j] = 0;
}

static char *query_mac_vendor(http_client *http, const char *oui) {
  char enc[24], url[256];
  enc_colon(oui, enc, sizeof enc);
  snprintf(url, sizeof url, "https://api.macvendors.com/%s", enc);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 12000, 1, &hr);
  char *v = (hc == 0 && hr.status == 200 && hr.body) ? strdup(hr.body) : NULL;
  http_response_free(&hr);
  if (v) { size_t l = strlen(v);
           while (l > 0 && (v[l-1] == '\n' || v[l-1] == '\r')) v[--l] = 0; }
  return v;
}

static cJSON *query_mylnikov(http_client *http, const char *bssid) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "mylnikov.org");
  char *n = normalize_bssid(bssid);
  if (!n) { cJSON_AddStringToObject(r, "error", "Invalid BSSID format"); return r; }
  char url[256];
  snprintf(url, sizeof url,
    "https://api.mylnikov.org/geolocation/wifi?v=1.1&bssid=%s", n);
  free(n);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  if (hc != 0 || hr.status != 200) {
    http_response_free(&hr);
    if (j) cJSON_Delete(j);
    cJSON_AddStringToObject(r, "error", "API request failed");
    return r;
  }
  http_response_free(&hr);
  if (!j) { cJSON_AddStringToObject(r, "error", "Invalid JSON response"); return r; }
  const cJSON *rc = cJSON_GetObjectItem(j, "result");
  if (!rc || !cJSON_IsNumber(rc) || rc->valuedouble != 200) {
    cJSON_Delete(j);
    cJSON_AddStringToObject(r, "status", "not_found");
    return r;
  }
  const cJSON *d = cJSON_GetObjectItem(j, "data");
  if (d) {
    cJSON_AddStringToObject(r, "status", "found");
    const cJSON *la = cJSON_GetObjectItem(d, "lat");
    const cJSON *lo = cJSON_GetObjectItem(d, "lon");
    const cJSON *rg = cJSON_GetObjectItem(d, "range");
    if (la && cJSON_IsNumber(la)) cJSON_AddNumberToObject(r, "latitude", la->valuedouble);
    if (lo && cJSON_IsNumber(lo)) cJSON_AddNumberToObject(r, "longitude", lo->valuedouble);
    if (rg && cJSON_IsNumber(rg)) cJSON_AddNumberToObject(r, "accuracy_meters", rg->valuedouble);
  }
  cJSON_Delete(j);
  return r;
}

static cJSON *query_wigle(http_client *http, const char *bssid) {
  const char *nm = getenv("WIGLE_API_NAME");
  const char *tk = getenv("WIGLE_API_TOKEN");
  if (!nm || !tk || !*nm || !*tk) return NULL;   /* graceful: caller adds note */
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "WiGLE");
  char *n = normalize_bssid(bssid);
  if (!n) { cJSON_AddStringToObject(r, "error", "Invalid BSSID format"); return r; }
  char enc[24], url[512];
  enc_colon(n, enc, sizeof enc);
  free(n);
  snprintf(url, sizeof url,
    "https://api.wigle.net/api/v2/network/search?netid=%s", enc);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  if (hc != 0 || hr.status != 200) {
    http_response_free(&hr);
    if (j) cJSON_Delete(j);
    cJSON_AddStringToObject(r, "error", "API request failed (auth required)");
    return r;
  }
  http_response_free(&hr);
  if (!j) { cJSON_AddStringToObject(r, "error", "Invalid JSON response"); return r; }
  const cJSON *ok = cJSON_GetObjectItem(j, "success");
  if (!ok || !cJSON_IsTrue(ok)) { cJSON_Delete(j);
    cJSON_AddStringToObject(r, "error", "API returned failure"); return r; }
  const cJSON *res = cJSON_GetObjectItem(j, "results");
  if (res && cJSON_IsArray(res) && cJSON_GetArraySize(res) > 0) {
    const cJSON *f = cJSON_GetArrayItem(res, 0);
    if (f) {
      cJSON_AddStringToObject(r, "status", "found");
      const cJSON *ss = cJSON_GetObjectItem(f, "ssid");
      const cJSON *la = cJSON_GetObjectItem(f, "trilat");
      const cJSON *lo = cJSON_GetObjectItem(f, "trilong");
      const cJSON *ch = cJSON_GetObjectItem(f, "channel");
      const cJSON *en = cJSON_GetObjectItem(f, "encryption");
      const cJSON *lu = cJSON_GetObjectItem(f, "lastupdt");
      const cJSON *co = cJSON_GetObjectItem(f, "country");
      const cJSON *ci = cJSON_GetObjectItem(f, "city");
      if (ss && cJSON_IsString(ss)) cJSON_AddStringToObject(r, "ssid", ss->valuestring);
      if (la && cJSON_IsNumber(la)) cJSON_AddNumberToObject(r, "latitude", la->valuedouble);
      if (lo && cJSON_IsNumber(lo)) cJSON_AddNumberToObject(r, "longitude", lo->valuedouble);
      if (ch && cJSON_IsNumber(ch)) cJSON_AddNumberToObject(r, "channel", ch->valuedouble);
      if (en && cJSON_IsString(en)) cJSON_AddStringToObject(r, "encryption", en->valuestring);
      if (lu && cJSON_IsString(lu)) cJSON_AddStringToObject(r, "last_seen", lu->valuestring);
      if (co && cJSON_IsString(co)) cJSON_AddStringToObject(r, "country", co->valuestring);
      if (ci && cJSON_IsString(ci)) cJSON_AddStringToObject(r, "city", ci->valuestring);
    }
  } else cJSON_AddStringToObject(r, "status", "not_found");
  cJSON_Delete(j);
  return r;
}

static int emit_result(intel_sink *sink, const char *entity, int success,
                        int confidence, cJSON *data /*owned*/) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  cJSON_AddItemToObject(env, "data", data);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WIFI_LOOKUP");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300]; snprintf(rk, sizeof rk, "wifi:%s", entity);
  char title[320]; snprintf(title, sizeof title, "WIFI_LOOKUP — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = success ? "BSSID lookup" : "invalid BSSID";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WIFI_LOOKUP\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *bssid = ctx->entity;
  if (!bssid || !*bssid) return -1;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", bssid);

  char *norm = normalize_bssid(bssid);
  if (!norm) {
    cJSON_AddStringToObject(root, "error", "Invalid BSSID format");
    cJSON_AddStringToObject(root, "hint",
      "BSSID should be 12 hex characters (e.g., AA:BB:CC:DD:EE:FF or AABBCCDDEEFF)");
    return emit_result(sink, bssid, 0, 0, root);
  }
  cJSON_AddStringToObject(root, "bssid", norm);

  char *oui = get_oui(bssid);
  if (oui) {
    cJSON_AddStringToObject(root, "oui", oui);
    const char *lv = local_vendor(oui);
    if (lv) {
      cJSON_AddStringToObject(root, "vendor", lv);
      cJSON_AddStringToObject(root, "vendor_source", "local_database");
    } else {
      char *av = query_mac_vendor(ctx->http, oui);
      if (av) {
        cJSON_AddStringToObject(root, "vendor", av);
        cJSON_AddStringToObject(root, "vendor_source", "macvendors.com");
        free(av);
      }
    }
    free(oui);
  }
  free(norm);

  cJSON *myl = query_mylnikov(ctx->http, bssid);
  cJSON_AddItemToObject(root, "mylnikov", myl);
  cJSON *wig = query_wigle(ctx->http, bssid);
  if (wig) cJSON_AddItemToObject(root, "wigle", wig);
  else cJSON_AddStringToObject(root, "wigle_status",
    "API credentials not configured (set WIGLE_API_NAME and WIGLE_API_TOKEN)");

  cJSON *loc = cJSON_CreateObject();
  int found = 0;
  {
    const cJSON *st = cJSON_GetObjectItem(myl, "status");
    if (st && cJSON_IsString(st) && strcmp(st->valuestring, "found") == 0) {
      const cJSON *la = cJSON_GetObjectItem(myl, "latitude");
      const cJSON *lo = cJSON_GetObjectItem(myl, "longitude");
      if (la && lo) {
        cJSON_AddNumberToObject(loc, "latitude", la->valuedouble);
        cJSON_AddNumberToObject(loc, "longitude", lo->valuedouble);
        cJSON_AddStringToObject(loc, "source", "mylnikov.org");
        found = 1;
      }
    }
  }
  if (!found && wig) {
    const cJSON *st = cJSON_GetObjectItem(wig, "status");
    if (st && cJSON_IsString(st) && strcmp(st->valuestring, "found") == 0) {
      const cJSON *la = cJSON_GetObjectItem(wig, "latitude");
      const cJSON *lo = cJSON_GetObjectItem(wig, "longitude");
      if (la && lo) {
        cJSON_AddNumberToObject(loc, "latitude", la->valuedouble);
        cJSON_AddNumberToObject(loc, "longitude", lo->valuedouble);
        cJSON_AddStringToObject(loc, "source", "WiGLE");
        found = 1;
      }
    }
  }
  if (found) {
    double la = cJSON_GetObjectItem(loc, "latitude")->valuedouble;
    double lo = cJSON_GetObjectItem(loc, "longitude")->valuedouble;
    cJSON_AddItemToObject(root, "location", loc);
    char mu[256];
    snprintf(mu, sizeof mu, "https://www.google.com/maps?q=%f,%f", la, lo);
    cJSON_AddStringToObject(root, "map_url", mu);
  } else {
    cJSON_Delete(loc);
    cJSON_AddStringToObject(root, "location_status", "not_found");
  }

  cJSON *res = cJSON_CreateArray();
  cJSON_AddItemToArray(res, cJSON_CreateString("https://wigle.net/"));
  cJSON_AddItemToArray(res, cJSON_CreateString("https://macvendors.com/"));
  cJSON_AddItemToObject(root, "resources", res);

  return emit_result(sink, bssid, 1, found ? 85 : 50, root);
}

static const source_def wifi_lookup_def = {
  .id = "WIFI_LOOKUP", .collector = "osint",
  .name = "WiFi Lookup", .name_ja = "WiFi照会",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(wifi_lookup_def)
