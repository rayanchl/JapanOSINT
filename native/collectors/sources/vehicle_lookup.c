/* collectors/osint/sources/vehicle_lookup.c
 * OSINT service — VEHICLE_LOOKUP. Canonical SERVICE = VEHICLE_LOOKUP
 * (dispatcher row {SERVICE_VEHICLE_LOOKUP, handle_vehicle_lookup,
 * "VEHICLE_LOOKUP", true}; LICENSE_PLATE_LOOKUP is a separate handler/source).
 * On-demand (interval 0); ctx->entity = a 17-char VIN or a license plate. No
 * key (NHTSA vPIC/recalls public).
 *
 * PER-RECORD EMIT (VIN branch only): validates the VIN locally
 * (transliteration + position-9 check digit, WMI/VDS/VIS split, model-year
 * code), decodes via NHTSA vPIC (vpic.nhtsa.dot.gov decodevinvalues) and emits
 * ONE intel_item for the decoded VIN (body = real Make/Model/Year/etc). When
 * Make/Model/ModelYear decode, queries NHTSA recalls (api.nhtsa.gov
 * recallsByVehicle) and emits ONE intel_item per recall campaign. VIN decode
 * failure → emits nothing. License-plate branch: no real fetch is available →
 * emits nothing (honest empty — no static source-link descriptors). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static const int VINW[] = {8,7,6,5,4,3,2,10,0,9,8,7,6,5,4,3,2};

static int vin_val(char c) {
  c = (char)toupper((unsigned char)c);
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'H') return c - 'A' + 1;
  if (c == 'J') return 1;
  if (c == 'K') return 2;
  if (c >= 'L' && c <= 'N') return c - 'L' + 3;
  if (c == 'P') return 7;
  if (c == 'R') return 9;
  if (c >= 'S' && c <= 'Z') return c - 'S' + 2;
  return -1;
}

static void uri_encode(const char *in, char *out, size_t cap) {
  static const char *keep = "-_.!~*'()";
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && w + 4 < cap; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(keep, c)) out[w++] = (char)c;
    else { snprintf(out + w, cap - w, "%%%02X", c); w += 3; }
  }
  out[w] = 0;
}

/* Local VIN validation → object added into the decoded-VIN body. NULL if the
 * VIN format is invalid (no character set check failures returned to caller via
 * *ok). */
static cJSON *validate_vin(const char *vin, int *ok) {
  *ok = 0;
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "vin", vin);
  size_t len = strlen(vin);
  if (len != 17) { cJSON_Delete(r); return NULL; }
  for (int i = 0; i < 17; i++) {
    char c = (char)toupper((unsigned char)vin[i]);
    if (c == 'I' || c == 'O' || c == 'Q' || !isalnum((unsigned char)c)) {
      cJSON_Delete(r); return NULL;
    }
  }
  cJSON_AddBoolToObject(r, "valid_format", 1);
  int sum = 0;
  for (int i = 0; i < 17; i++) {
    int v = vin_val(vin[i]);
    if (v < 0) { cJSON_Delete(r); return NULL; }
    sum += v * VINW[i];
  }
  int chk = sum % 11;
  char exp = (chk == 10) ? 'X' : (char)('0' + chk);
  char act = (char)toupper((unsigned char)vin[8]);
  cJSON_AddBoolToObject(r, "check_digit_valid", act == exp);
  char wmi[4]; strncpy(wmi, vin, 3); wmi[3] = 0; cJSON_AddStringToObject(r, "wmi", wmi);
  char vds[7]; strncpy(vds, vin + 3, 6); vds[6] = 0; cJSON_AddStringToObject(r, "vds", vds);
  char vis[9]; strncpy(vis, vin + 9, 8); vis[8] = 0; cJSON_AddStringToObject(r, "vis", vis);
  char yc = (char)toupper((unsigned char)vin[9]);
  int my = 0;
  if (yc >= 'A' && yc <= 'H') my = 2010 + (yc - 'A');
  else if (yc >= 'J' && yc <= 'N') my = 2018 + (yc - 'J');
  else if (yc == 'P') my = 2023;
  else if (yc == 'R') my = 2024;
  else if (yc >= 'S' && yc <= 'Y') my = 1995 + (yc - 'S');
  else if (yc >= '1' && yc <= '9') my = 2001 + (yc - '1');
  if (my > 0) cJSON_AddNumberToObject(r, "estimated_model_year", my);
  *ok = 1;
  return r;
}

/* Decode VIN via NHTSA vPIC. Returns the vehicle_info object (owned) or NULL on
 * failure. *out_make/model/year point into the returned object when present. */
static cJSON *decode_vin_nhtsa(http_client *http, const char *vin) {
  char url[256];
  snprintf(url, sizeof url,
    "https://vpic.nhtsa.dot.gov/api/vehicles/decodevinvalues/%s?format=json", vin);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (!j) return NULL;

  cJSON *vi = NULL;
  const cJSON *res = cJSON_GetObjectItem(j, "Results");
  if (res && cJSON_IsArray(res) && cJSON_GetArraySize(res) > 0) {
    const cJSON *v = cJSON_GetArrayItem(res, 0);  /* exhaustive-ok: vPIC returns one decode per VIN */
    if (v) {
      static const char *F[] = {
        "Make","Model","ModelYear","VehicleType","BodyClass","DriveType",
        "FuelTypePrimary","EngineConfiguration","DisplacementL","EngineCylinders",
        "EngineHP","Manufacturer","PlantCity","PlantCountry","Series","Trim",
        "GVWR","CurbWeightLB","TransmissionStyle","ABS","ESC",NULL };
      vi = cJSON_CreateObject();
      for (int i = 0; F[i]; i++) {
        const cJSON *fl = cJSON_GetObjectItem(v, F[i]);
        if (fl && cJSON_IsString(fl) && strlen(fl->valuestring) > 0)
          cJSON_AddStringToObject(vi, F[i], fl->valuestring);
      }
      if (cJSON_GetArraySize(vi) == 0) { cJSON_Delete(vi); vi = NULL; }
    }
  }
  cJSON_Delete(j);
  return vi;
}

/* Emit ONE intel row per NHTSA recall campaign. Returns number emitted. */
static int emit_recalls(intel_sink *sink, http_client *http, const char *vin,
                        const char *mk, const char *md, const char *yr) {
  char em[256], emd[256], url[640];
  uri_encode(mk, em, sizeof em);
  uri_encode(md, emd, sizeof emd);
  snprintf(url, sizeof url,
    "https://api.nhtsa.gov/recalls/recallsByVehicle?make=%s&model=%s&modelYear=%s",
    em, emd, yr);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (!j) return 0;

  int emitted = 0;
  const cJSON *res = cJSON_GetObjectItem(j, "results");
  if (res && cJSON_IsArray(res)) {
    int n = cJSON_GetArraySize(res);
    for (int i = 0; i < n; i++) {
      const cJSON *rec = cJSON_GetArrayItem(res, i);
      if (!rec) continue;
      const cJSON *cn = cJSON_GetObjectItem(rec, "NHTSACampaignNumber");
      const cJSON *co = cJSON_GetObjectItem(rec, "Component");
      const cJSON *su = cJSON_GetObjectItem(rec, "Summary");
      const cJSON *cq = cJSON_GetObjectItem(rec, "Conequence");
      const cJSON *re = cJSON_GetObjectItem(rec, "Remedy");
      const char *cid = (cn && cJSON_IsString(cn)) ? cn->valuestring : NULL;

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "source", "NHTSA Recalls");
      if (cid) cJSON_AddStringToObject(data, "campaign_number", cid);
      if (co && cJSON_IsString(co)) cJSON_AddStringToObject(data, "component", co->valuestring);
      if (su && cJSON_IsString(su)) cJSON_AddStringToObject(data, "summary", su->valuestring);
      if (cq && cJSON_IsString(cq)) cJSON_AddStringToObject(data, "consequence", cq->valuestring);
      if (re && cJSON_IsString(re)) cJSON_AddStringToObject(data, "remedy", re->valuestring);
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "VEHICLE_LOOKUP");
      cJSON_AddStringToObject(props, "record", "recall");
      if (cid) cJSON_AddStringToObject(props, "campaign_number", cid);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);

      char rk[160];
      snprintf(rk, sizeof rk, "recall:%s", cid ? cid : vin);
      char title[320];
      snprintf(title, sizeof title, "Recall %s — %s",
               cid ? cid : "?", (co && cJSON_IsString(co)) ? co->valuestring : "vehicle");

      intel_item it = {0};
      it.remote_key      = rk;
      it.title           = title;
      it.body            = bj;
      it.summary         = (co && cJSON_IsString(co)) ? co->valuestring : "recall";
      it.record_type     = "osint_service_result";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"VEHICLE_LOOKUP\"]";
      int rc = sink->emit(sink, &it);
      free(bj); free(pj);
      if (rc >= 0) emitted++;
    }
  }
  cJSON_Delete(j);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  /* Only the VIN branch has a real upstream fetch. License plates have no real
   * data source → emit nothing. */
  if (strlen(q) != 17) return 0;

  int ok = 0;
  cJSON *validation = validate_vin(q, &ok);
  if (!ok) { if (validation) cJSON_Delete(validation); return 0; }

  cJSON *vi = decode_vin_nhtsa(ctx->http, q);
  if (!vi) { cJSON_Delete(validation); return 0; }

  /* Emit ONE item for the decoded VIN. */
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "vin", q);
  cJSON_AddStringToObject(data, "source", "NHTSA vPIC");
  cJSON_AddItemToObject(data, "vehicle_info", cJSON_Duplicate(vi, 1));
  cJSON_AddItemToObject(data, "vin_validation", validation); /* adopted */
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  const cJSON *mk = cJSON_GetObjectItem(vi, "Make");
  const cJSON *md = cJSON_GetObjectItem(vi, "Model");
  const cJSON *yr = cJSON_GetObjectItem(vi, "ModelYear");
  char ttl[320];
  snprintf(ttl, sizeof ttl, "%s %s %s — %s",
           (yr && cJSON_IsString(yr)) ? yr->valuestring : "",
           (mk && cJSON_IsString(mk)) ? mk->valuestring : "",
           (md && cJSON_IsString(md)) ? md->valuestring : "",
           q);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "VEHICLE_LOOKUP");
  cJSON_AddStringToObject(props, "record", "vin");
  cJSON_AddStringToObject(props, "vin", q);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[64]; snprintf(rk, sizeof rk, "vin:%s", q);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = ttl;
  it.body            = bj;
  it.summary         = "VIN decode";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"VEHICLE_LOOKUP\"]";
  sink->emit(sink, &it);
  free(bj); free(pj);

  /* Emit one item per recall when make/model/year decoded. */
  if (mk && md && yr && cJSON_IsString(mk) && cJSON_IsString(md) && cJSON_IsString(yr))
    emit_recalls(sink, ctx->http, q, mk->valuestring, md->valuestring, yr->valuestring);

  cJSON_Delete(vi);
  return 0;
}

static const source_def vehicle_lookup_def = {
  .id = "VEHICLE_LOOKUP", .collector = "osint",
  .name = "Vehicle Lookup", .name_ja = "車両照会",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "api",
  .url = "internal://osint/vehicle-lookup",
  .description = "Decode a VIN and list NHTSA recalls (vPIC + recalls API).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(vehicle_lookup_def)
