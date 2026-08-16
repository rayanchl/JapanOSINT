/* collectors/sources/wireless_world2.c
 * OSINT services — key-gated wireless / RF geolocation, pivot on ctx->entity.
 * Honest-empty when the key is unset.
 *
 *   APRS_FI_LOC     api.aprs.fi/api/get?name=<call>&what=loc   APRS_FI_KEY
 *   OPENCELLID_CELL opencellid.org/cell/get?...  OPENCELLID_KEY
 *                   (entity = "mcc,mnc,lac,cellid") */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *WL_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

/* APRS.fi position lookup by station name/callsign. */
static int run_aprs(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("APRS_FI_KEY");
  if (!k) { fprintf(stderr, "[APRS_FI_LOC] no APRS_FI_KEY\n"); return 0; }
  char url[640];
  snprintf(url, sizeof url,
    "https://api.aprs.fi/api/get?name=%s&what=loc&apikey=%s&format=json", enc, k);
  const char *hdrs[] = { "Accept: application/json", WL_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "APRS_FI_LOC");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *result = jo_sv(root, "result");
  if (!result || strcmp(result, "ok") != 0) { cJSON_Delete(root); return 0; }
  const cJSON *entries = cJSON_GetObjectItem(root, "entries");
  int emitted = 0;
  if (cJSON_IsArray(entries)) {
    const cJSON *e;
    cJSON_ArrayForEach(e, entries) {
      const char *name = jo_sv(e, "name");
      const char *lat = jo_sv(e, "lat");
      const char *lng = jo_sv(e, "lng");
      const char *comment = jo_sv(e, "comment");
      const char *time = jo_sv(e, "lasttime");
      if (!name) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "callsign", name);
      if (lat) cJSON_AddStringToObject(d, "lat", lat);
      if (lng) cJSON_AddStringToObject(d, "lng", lng);
      if (comment) cJSON_AddStringToObject(d, "comment", comment);
      cJSON_AddStringToObject(d, "source", "APRS.fi");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "APRS_FI_LOC");
      cJSON_AddStringToObject(p, "callsign", name);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      int has_geo = (lat && lng);
      char link[256]; snprintf(link, sizeof link, "https://aprs.fi/#!call=%s", name);
      intel_item it = {0};
      it.remote_key = name; it.title = name; it.summary = comment;
      it.body = bj; it.link = link; it.lang = "en";
      it.has_geo = has_geo; it.lat = has_geo ? atof(lat) : 0; it.lon = has_geo ? atof(lng) : 0;
      it.record_type = "aprs-station";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"APRS_FI_LOC\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
      (void)time;
    }
  }
  cJSON_Delete(root);
  return emitted;
}

/* OpenCelliD cell tower geolocation. entity = "mcc,mnc,lac,cellid". */
static int run_opencellid(const source_ctx *ctx, intel_sink *sink, const char *raw) {
  const char *k = jo_env("OPENCELLID_KEY");
  if (!k) { fprintf(stderr, "[OPENCELLID_CELL] no OPENCELLID_KEY\n"); return 0; }
  int mcc = 0, mnc = 0, lac = 0; long cellid = 0;
  if (sscanf(raw, "%d,%d,%d,%ld", &mcc, &mnc, &lac, &cellid) != 4) {
    fprintf(stderr, "[OPENCELLID_CELL] entity must be 'mcc,mnc,lac,cellid'\n");
    return 0;
  }
  char url[640];
  snprintf(url, sizeof url,
    "https://opencellid.org/cell/get?key=%s&mcc=%d&mnc=%d&lac=%d&cellid=%ld&format=json",
    k, mcc, mnc, lac, cellid);
  const char *hdrs[] = { "Accept: application/json", WL_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "OPENCELLID_CELL");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *lat = cJSON_GetObjectItem(root, "lat");
  const cJSON *lon = cJSON_GetObjectItem(root, "lon");
  if (!cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) { cJSON_Delete(root); return 0; }
  const cJSON *range = cJSON_GetObjectItem(root, "range");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddNumberToObject(d, "mcc", mcc); cJSON_AddNumberToObject(d, "mnc", mnc);
  cJSON_AddNumberToObject(d, "lac", lac); cJSON_AddNumberToObject(d, "cellid", (double)cellid);
  cJSON_AddNumberToObject(d, "lat", lat->valuedouble);
  cJSON_AddNumberToObject(d, "lon", lon->valuedouble);
  if (range && cJSON_IsNumber(range)) cJSON_AddNumberToObject(d, "range_m", range->valuedouble);
  cJSON_AddStringToObject(d, "source", "OpenCelliD");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "OPENCELLID_CELL");
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char rk[128], title[160];
  snprintf(rk, sizeof rk, "cell:%d-%d-%d-%ld", mcc, mnc, lac, cellid);
  snprintf(title, sizeof title, "Cell %d-%d-%d-%ld", mcc, mnc, lac, cellid);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.lang = "en";
  it.has_geo = 1; it.lat = lat->valuedouble; it.lon = lon->valuedouble;
  it.record_type = "cell-tower";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"OPENCELLID_CELL\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  int n = 0;
  if (!strcmp(sid, "APRS_FI_LOC")) {
    char *enc = jo_urlencode(q); if (!enc) return 0;
    n = run_aprs(ctx, sink, enc); free(enc);
  } else if (!strcmp(sid, "OPENCELLID_CELL")) {
    n = run_opencellid(ctx, sink, q);
  }
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define WL_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "telecom", \
    .type = "api", .url = "internal://osint/wireless", .description = DESC, \
    .layer = NULL, .free_tier = 0 }; \
  REGISTER_SOURCE(sym)

WL_DEF(wl_aprs_def, "APRS_FI_LOC", "APRS.fi Station Location",
  "APRS.fi (APRS_FI_KEY): amateur-radio station position by callsign.");
WL_DEF(wl_opencellid_def, "OPENCELLID_CELL", "OpenCelliD Cell Locate",
  "OpenCelliD (OPENCELLID_KEY): cell-tower geolocation from mcc,mnc,lac,cellid.");
