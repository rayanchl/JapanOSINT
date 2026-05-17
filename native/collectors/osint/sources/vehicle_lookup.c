/* collectors/osint/sources/vehicle_lookup.c
 * OSINT service — port of OSINTsaas osint_tools/vehicle_lookup.c
 * (vehicle_lookup → handle_vehicle_lookup). Canonical SERVICE = VEHICLE_LOOKUP
 * (dispatcher row {SERVICE_VEHICLE_LOOKUP, handle_vehicle_lookup,
 * "VEHICLE_LOOKUP", true}; LICENSE_PLATE_LOOKUP is a separate handler/source).
 * On-demand (interval 0); ctx->entity = a 17-char VIN or a license plate. No
 * key (NHTSA vPIC/recalls public). 17 chars → VIN branch: validate_vin
 * (transliteration + position-9 check digit, WMI/VDS/VIS split, model-year
 * code) + decode_vin_nhtsa (vpic.nhtsa.dot.gov decodevinvalues) + get_recalls
 * (api.nhtsa.gov recallsByVehicle, only when Make/Model/ModelYear decoded) +
 * the static VIN-source descriptors. Else → license-plate branch:
 * analyze_license_plate (letter/digit counts, US/UK/DE/FR format heuristics) +
 * static plate-source descriptors. Both add the India/Australia/Canada/Japan
 * static source descriptors (faithful URL builders, no HTTP — exactly as the C
 * search_* helpers). root {query,timestamp,query_type,...,sources,
 * sources_checked,sources_with_data}; confidence 85 if >3 sources_with_data
 * else 70. Emits ONE osint_service_result row like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

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

static cJSON *validate_vin(const char *vin) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "vin", vin);
  size_t len = strlen(vin);
  if (len != 17) {
    cJSON_AddBoolToObject(r, "valid_format", 0);
    cJSON_AddStringToObject(r, "error",
      len < 17 ? "VIN too short (must be 17 characters)"
               : "VIN too long (must be 17 characters)");
    return r;
  }
  for (int i = 0; i < 17; i++) {
    char c = (char)toupper((unsigned char)vin[i]);
    if (c == 'I' || c == 'O' || c == 'Q') {
      cJSON_AddBoolToObject(r, "valid_format", 0);
      char e[64]; snprintf(e, sizeof e, "Invalid character '%c' at position %d", c, i + 1);
      cJSON_AddStringToObject(r, "error", e);
      return r;
    }
    if (!isalnum((unsigned char)c)) {
      cJSON_AddBoolToObject(r, "valid_format", 0);
      cJSON_AddStringToObject(r, "error", "VIN must contain only letters and numbers");
      return r;
    }
  }
  cJSON_AddBoolToObject(r, "valid_format", 1);
  int sum = 0;
  for (int i = 0; i < 17; i++) {
    int v = vin_val(vin[i]);
    if (v < 0) { cJSON_AddStringToObject(r, "check_digit_status", "calculation_error"); return r; }
    sum += v * VINW[i];
  }
  int chk = sum % 11;
  char exp = (chk == 10) ? 'X' : (char)('0' + chk);
  char act = (char)toupper((unsigned char)vin[8]);
  if (act == exp) cJSON_AddBoolToObject(r, "check_digit_valid", 1);
  else {
    cJSON_AddBoolToObject(r, "check_digit_valid", 0);
    char m[64]; snprintf(m, sizeof m, "Expected '%c', got '%c'", exp, act);
    cJSON_AddStringToObject(r, "check_digit_error", m);
  }
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
  return r;
}

static cJSON *decode_vin_nhtsa(http_client *http, const char *vin) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "NHTSA vPIC");
  char url[256];
  snprintf(url, sizeof url,
    "https://vpic.nhtsa.dot.gov/api/vehicles/decodevinvalues/%s?format=json", vin);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (!j) { cJSON_AddStringToObject(r, "status", "api_error");
            cJSON_AddStringToObject(r, "note", "NHTSA API unavailable"); return r; }
  const cJSON *res = cJSON_GetObjectItem(j, "Results");
  if (res && cJSON_IsArray(res) && cJSON_GetArraySize(res) > 0) {
    const cJSON *v = cJSON_GetArrayItem(res, 0);
    if (v) {
      static const char *F[] = {
        "Make","Model","ModelYear","VehicleType","BodyClass","DriveType",
        "FuelTypePrimary","EngineConfiguration","DisplacementL","EngineCylinders",
        "EngineHP","Manufacturer","PlantCity","PlantCountry","Series","Trim",
        "GVWR","CurbWeightLB","TransmissionStyle","ABS","ESC","ErrorCode",
        "ErrorText",NULL };
      cJSON *vi = cJSON_CreateObject();
      for (int i = 0; F[i]; i++) {
        const cJSON *fl = cJSON_GetObjectItem(v, F[i]);
        if (fl && cJSON_IsString(fl) && strlen(fl->valuestring) > 0)
          cJSON_AddStringToObject(vi, F[i], fl->valuestring);
      }
      cJSON_AddItemToObject(r, "vehicle_info", vi);
      const cJSON *ec = cJSON_GetObjectItem(v, "ErrorCode");
      if (ec && cJSON_IsString(ec) && strcmp(ec->valuestring, "0") != 0) {
        const cJSON *et = cJSON_GetObjectItem(v, "ErrorText");
        if (et && cJSON_IsString(et)) cJSON_AddStringToObject(r, "decode_warning", et->valuestring);
      }
    }
  }
  cJSON_Delete(j);
  cJSON_AddStringToObject(r, "status", "decoded");
  return r;
}

static cJSON *get_recalls(http_client *http, const char *mk, const char *md, const char *yr) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "NHTSA Recalls");
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
  if (!j) { cJSON_AddStringToObject(r, "status", "api_error"); return r; }
  const cJSON *res = cJSON_GetObjectItem(j, "results");
  if (res && cJSON_IsArray(res)) {
    int n = cJSON_GetArraySize(res);
    cJSON_AddNumberToObject(r, "recall_count", n);
    if (n > 0) {
      cJSON *rc = cJSON_CreateArray();
      for (int i = 0; i < n && i < 10; i++) {
        const cJSON *rec = cJSON_GetArrayItem(res, i);
        if (!rec) continue;
        cJSON *e = cJSON_CreateObject();
        const cJSON *cn = cJSON_GetObjectItem(rec, "NHTSACampaignNumber");
        const cJSON *co = cJSON_GetObjectItem(rec, "Component");
        const cJSON *su = cJSON_GetObjectItem(rec, "Summary");
        const cJSON *cq = cJSON_GetObjectItem(rec, "Conequence");
        const cJSON *re = cJSON_GetObjectItem(rec, "Remedy");
        if (cn && cJSON_IsString(cn)) cJSON_AddStringToObject(e, "campaign_number", cn->valuestring);
        if (co && cJSON_IsString(co)) cJSON_AddStringToObject(e, "component", co->valuestring);
        if (su && cJSON_IsString(su)) cJSON_AddStringToObject(e, "summary", su->valuestring);
        if (cq && cJSON_IsString(cq)) cJSON_AddStringToObject(e, "consequence", cq->valuestring);
        if (re && cJSON_IsString(re)) cJSON_AddStringToObject(e, "remedy", re->valuestring);
        cJSON_AddItemToArray(rc, e);
      }
      cJSON_AddItemToObject(r, "recalls", rc);
    }
  }
  cJSON_Delete(j);
  return r;
}

/* Static source descriptor — faithful to the C search_* URL builders. */
static cJSON *src_obj(const char *source) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", source);
  cJSON_AddBoolToObject(r, "real_data", 1);
  return r;
}

static cJSON *analyze_license_plate(const char *plate) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "plate", plate);
  size_t len = strlen(plate);
  if (len < 2 || len > 10) { cJSON_AddStringToObject(r, "analysis", "Invalid plate length"); return r; }
  char up[16];
  for (size_t i = 0; i < len && i < 15; i++) up[i] = (char)toupper((unsigned char)plate[i]);
  up[len < 15 ? len : 15] = 0;
  int letters = 0, digits = 0;
  for (size_t i = 0; i < len; i++) {
    if (isalpha((unsigned char)plate[i])) letters++;
    else if (isdigit((unsigned char)plate[i])) digits++;
  }
  cJSON_AddNumberToObject(r, "letter_count", letters);
  cJSON_AddNumberToObject(r, "digit_count", digits);
  const char *fmt = "unknown", *cty = "Unknown";
  if (len == 7 && letters == 3 && digits == 4) { fmt = "AAA-0000 or similar"; cty = "United States (likely)"; }
  else if (len == 6 && letters == 3 && digits == 3) { fmt = "AAA-000"; cty = "United States (likely)"; }
  else if (len == 7 && isalpha((unsigned char)up[0]) && isalpha((unsigned char)up[1]) &&
           isdigit((unsigned char)up[2]) && isdigit((unsigned char)up[3]) &&
           isalpha((unsigned char)up[4]) && isalpha((unsigned char)up[5]) &&
           isalpha((unsigned char)up[6])) { fmt = "AA00 AAA (UK current)"; cty = "United Kingdom"; }
  else if (len >= 5 && len <= 8 && isalpha((unsigned char)up[0])) {
    if (strchr(plate, '-') || strchr(plate, ' ')) { fmt = "X-XX-0000 (German style)"; cty = "Germany (possible)"; }
  }
  else if (len == 7 && isalpha((unsigned char)up[0]) && isalpha((unsigned char)up[1]) &&
           isdigit((unsigned char)up[2]) && isdigit((unsigned char)up[3]) && isdigit((unsigned char)up[4]) &&
           isalpha((unsigned char)up[5]) && isalpha((unsigned char)up[6])) { fmt = "AA-000-AA"; cty = "France"; }
  cJSON_AddStringToObject(r, "format", fmt);
  cJSON_AddStringToObject(r, "country_hint", cty);
  cJSON *res = cJSON_CreateArray();
  cJSON_AddItemToArray(res, cJSON_CreateString("US: https://www.vehiclehistory.com/"));
  cJSON_AddItemToArray(res, cJSON_CreateString("UK: https://www.gov.uk/get-vehicle-information-from-dvla"));
  cJSON_AddItemToArray(res, cJSON_CreateString("EU: National traffic authority databases"));
  cJSON_AddItemToObject(r, "lookup_resources", res);
  cJSON_AddStringToObject(r, "note",
    "License plate lookup requires access to national vehicle databases. "
    "Use official government services for accurate information.");
  return r;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", q);
  cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));

  size_t len = strlen(q);
  cJSON *sources = cJSON_CreateArray();
  int swd = 0;
  char enc[256];
  uri_encode(q, enc, sizeof enc);

  if (len == 17) {
    cJSON_AddStringToObject(root, "query_type", "VIN");
    cJSON_AddItemToObject(root, "vin_validation", validate_vin(q));
    cJSON *nhtsa = decode_vin_nhtsa(ctx->http, q);
    cJSON_AddItemToObject(root, "nhtsa_decode", nhtsa);
    swd++;
    const cJSON *vi = cJSON_GetObjectItem(nhtsa, "vehicle_info");
    if (vi) {
      const cJSON *mk = cJSON_GetObjectItem(vi, "Make");
      const cJSON *md = cJSON_GetObjectItem(vi, "Model");
      const cJSON *yr = cJSON_GetObjectItem(vi, "ModelYear");
      if (mk && md && yr && cJSON_IsString(mk) && cJSON_IsString(md) && cJSON_IsString(yr))
        cJSON_AddItemToObject(root, "recalls",
          get_recalls(ctx->http, mk->valuestring, md->valuestring, yr->valuestring));
    }
    { cJSON *o = src_obj("CheckThatVin");
      char u[256]; snprintf(u, sizeof u, "https://www.checkthatvin.com/vin/%s", q);
      cJSON_AddStringToObject(o, "lookup_url", u);
      cJSON_AddStringToObject(o, "service_type", "VIN Verification");
      cJSON_AddBoolToObject(o, "theft_check", 1);
      cJSON_AddBoolToObject(o, "salvage_check", 1);
      cJSON_AddItemToArray(sources, o); swd++; }
    { cJSON *o = src_obj("VINDecoderz");
      char u[256]; snprintf(u, sizeof u, "https://www.vindecoderz.com/EN/check-lookup/%s", q);
      cJSON_AddStringToObject(o, "lookup_url", u);
      cJSON_AddStringToObject(o, "service_type", "Free VIN Decoder");
      cJSON_AddBoolToObject(o, "free_service", 1);
      cJSON_AddItemToArray(sources, o); swd++; }
    { cJSON *o = src_obj("NICB_VINCheck");
      cJSON_AddStringToObject(o, "lookup_url", "https://www.nicb.org/vincheck");
      cJSON_AddStringToObject(o, "service_type", "Theft/Salvage Database");
      cJSON_AddStringToObject(o, "note", "Free service - checks if vehicle was reported stolen or salvaged");
      cJSON_AddBoolToObject(o, "manual_entry_required", 1);
      cJSON_AddBoolToObject(o, "free_service", 1);
      cJSON_AddNumberToObject(o, "daily_limit", 5);
      cJSON_AddItemToArray(sources, o); swd++; }
    { cJSON *o = src_obj("FaxVin");
      char u[256]; snprintf(u, sizeof u, "https://www.faxvin.com/vin-decoder/result?vin=%s", enc);
      cJSON_AddStringToObject(o, "lookup_url", u);
      cJSON_AddStringToObject(o, "service_type", "Vehicle History Report");
      cJSON_AddStringToObject(o, "coverage", "United States");
      cJSON_AddBoolToObject(o, "free_basic", 1);
      cJSON_AddBoolToObject(o, "paid_full_report", 1);
      cJSON_AddItemToArray(sources, o); swd++; }
    { cJSON *o = src_obj("VehicleHistory");
      char u[256]; snprintf(u, sizeof u, "https://www.vehiclehistory.com/vin-report/%s", enc);
      cJSON_AddStringToObject(o, "lookup_url", u);
      cJSON_AddStringToObject(o, "service_type", "Vehicle History");
      cJSON_AddStringToObject(o, "coverage", "United States");
      cJSON_AddBoolToObject(o, "free_basic", 1);
      cJSON_AddItemToArray(sources, o); swd++; }
    { cJSON *o = src_obj("Carvana");
      char u[256]; snprintf(u, sizeof u, "https://www.carvana.com/vehicle/%s", q);
      cJSON_AddStringToObject(o, "lookup_url", u);
      cJSON_AddStringToObject(o, "note", "Check if vehicle is listed for sale");
      cJSON_AddStringToObject(o, "service_type", "Used Car Marketplace");
      cJSON_AddStringToObject(o, "coverage", "United States");
      cJSON_AddItemToArray(sources, o); swd++; }
  } else {
    cJSON_AddStringToObject(root, "query_type", "license_plate");
    cJSON_AddItemToObject(root, "plate_analysis", analyze_license_plate(q));
    { cJSON *o = src_obj("FindByPlate");
      char u[256]; snprintf(u, sizeof u, "https://www.findbyplate.com/US/%s/", enc);
      cJSON_AddStringToObject(o, "lookup_url", u);
      cJSON_AddStringToObject(o, "service_type", "License Plate Lookup");
      cJSON_AddStringToObject(o, "coverage", "United States");
      cJSON_AddBoolToObject(o, "free_basic", 1);
      cJSON_AddItemToArray(sources, o); swd++; }
    { cJSON *o = src_obj("FaxVin");
      char u[256]; snprintf(u, sizeof u, "https://www.faxvin.com/license-plate-lookup/result?plate=%s", enc);
      cJSON_AddStringToObject(o, "lookup_url", u);
      cJSON_AddStringToObject(o, "service_type", "Vehicle History Report");
      cJSON_AddStringToObject(o, "coverage", "United States");
      cJSON_AddBoolToObject(o, "free_basic", 1);
      cJSON_AddBoolToObject(o, "paid_full_report", 1);
      cJSON_AddItemToArray(sources, o); swd++; }
    { cJSON *o = src_obj("VehicleHistory");
      char u[256]; snprintf(u, sizeof u, "https://www.vehiclehistory.com/license-plate-search?plate=%s", enc);
      cJSON_AddStringToObject(o, "lookup_url", u);
      cJSON_AddStringToObject(o, "service_type", "Vehicle History");
      cJSON_AddStringToObject(o, "coverage", "United States");
      cJSON_AddBoolToObject(o, "free_basic", 1);
      cJSON_AddItemToArray(sources, o); swd++; }
    { cJSON *o = src_obj("InfoTracer");
      cJSON_AddStringToObject(o, "lookup_url", "https://infotracer.com/plate-lookup/");
      cJSON_AddStringToObject(o, "service_type", "License Plate + Owner Lookup");
      cJSON_AddStringToObject(o, "coverage", "United States");
      cJSON_AddBoolToObject(o, "requires_signup", 1);
      cJSON_AddItemToArray(sources, o); swd++; }
    { cJSON *o = src_obj("ThatsThem");
      char u[256]; snprintf(u, sizeof u, "https://thatsthem.com/license-plate/%s", enc);
      cJSON_AddStringToObject(o, "lookup_url", u);
      cJSON_AddStringToObject(o, "service_type", "License Plate + Owner Lookup");
      cJSON_AddBoolToObject(o, "free_basic", 1);
      cJSON_AddItemToArray(sources, o); swd++; }
    { cJSON *o = src_obj("Facebook");
      char m[512]; snprintf(m, sizeof m, "https://www.facebook.com/marketplace/search/?query=%s", enc);
      cJSON_AddStringToObject(o, "marketplace_url", m);
      char p[512]; snprintf(p, sizeof p, "https://www.facebook.com/search/posts/?q=%s", enc);
      cJSON_AddStringToObject(o, "posts_search_url", p);
      char ph[512]; snprintf(ph, sizeof ph, "https://www.facebook.com/search/photos/?q=%s", enc);
      cJSON_AddStringToObject(o, "photos_search_url", ph);
      cJSON_AddStringToObject(o, "service_type", "Social Media Search");
      cJSON_AddStringToObject(o, "note", "Search Facebook posts and marketplace for vehicle mentions");
      cJSON_AddItemToArray(sources, o); swd++; }
  }

  /* International static sources (both branches) */
  { cJSON *o = src_obj("VahanInfos");
    char u[256]; snprintf(u, sizeof u, "https://www.vahaninfos.com/vehicle-registration-details/%s", enc);
    cJSON_AddStringToObject(o, "lookup_url", u);
    cJSON_AddStringToObject(o, "service_type", "Vehicle Registration Lookup");
    cJSON_AddStringToObject(o, "country", "India");
    cJSON_AddStringToObject(o, "coverage", "RC details, owner info, registration validity, insurance status");
    cJSON_AddBoolToObject(o, "free_basic", 1);
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("Cars24");
    char u[256]; snprintf(u, sizeof u, "https://www.cars24.com/rc-transfer/?registration=%s", enc);
    cJSON_AddStringToObject(o, "lookup_url", u);
    cJSON_AddStringToObject(o, "service_type", "Vehicle Registration Check");
    cJSON_AddStringToObject(o, "country", "India");
    cJSON_AddStringToObject(o, "coverage", "Registration details, ownership transfer info");
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("DroomHistory");
    char u[256]; snprintf(u, sizeof u, "https://history.droom.in/?registration=%s", enc);
    cJSON_AddStringToObject(o, "lookup_url", u);
    cJSON_AddStringToObject(o, "service_type", "Vehicle History Report");
    cJSON_AddStringToObject(o, "country", "India");
    cJSON_AddStringToObject(o, "coverage", "Full vehicle history, blacklist check, challans, insurance");
    cJSON_AddBoolToObject(o, "paid_full_report", 1);
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("Parivahan (Govt of India)");
    cJSON_AddStringToObject(o, "lookup_url", "https://parivahan.gov.in/parivahan/");
    cJSON_AddStringToObject(o, "vahan_url", "https://vahan.parivahan.gov.in/");
    cJSON_AddStringToObject(o, "service_type", "Official Government Portal");
    cJSON_AddStringToObject(o, "country", "India");
    cJSON_AddStringToObject(o, "coverage", "Official RC details, driving license verification, e-Challan check");
    cJSON_AddStringToObject(o, "note", "Official Ministry of Road Transport portal - most authoritative");
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("CarDekho");
    char u[256]; snprintf(u, sizeof u, "https://www.cardekho.com/rto/%s", enc);
    cJSON_AddStringToObject(o, "lookup_url", u);
    cJSON_AddStringToObject(o, "service_type", "Vehicle Information");
    cJSON_AddStringToObject(o, "country", "India");
    cJSON_AddStringToObject(o, "coverage", "RTO information, vehicle specifications");
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("eChallan (Govt of India)");
    cJSON_AddStringToObject(o, "lookup_url", "https://echallan.parivahan.gov.in/");
    cJSON_AddStringToObject(o, "service_type", "Traffic Violations Check");
    cJSON_AddStringToObject(o, "country", "India");
    cJSON_AddStringToObject(o, "coverage", "Traffic challans, pending fines, violation history");
    cJSON_AddStringToObject(o, "note", "Official government portal for traffic violations");
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("PPSR Australia");
    cJSON_AddStringToObject(o, "lookup_url", "https://www.ppsr.gov.au/searching");
    cJSON_AddStringToObject(o, "service_type", "Personal Property Securities Register");
    cJSON_AddStringToObject(o, "country", "Australia");
    cJSON_AddStringToObject(o, "coverage", "Vehicle encumbrances, finance owing, write-off status");
    cJSON_AddStringToObject(o, "note", "Official government register - check if vehicle has money owing");
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("NEVDIS Australia");
    cJSON_AddStringToObject(o, "lookup_url", "https://www.nevdis.com.au/");
    cJSON_AddStringToObject(o, "service_type", "Vehicle Information Exchange");
    cJSON_AddStringToObject(o, "country", "Australia");
    cJSON_AddStringToObject(o, "coverage", "Stolen vehicle check, written-off status, registration details");
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("CARFAX Canada");
    char u[256]; snprintf(u, sizeof u, "https://www.carfax.ca/vehicle-history-report/%s", enc);
    cJSON_AddStringToObject(o, "lookup_url", u);
    cJSON_AddStringToObject(o, "service_type", "Vehicle History Report");
    cJSON_AddStringToObject(o, "country", "Canada");
    cJSON_AddStringToObject(o, "coverage", "Accident history, service records, odometer, lien check");
    cJSON_AddBoolToObject(o, "paid_service", 1);
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("ICBC (British Columbia)");
    cJSON_AddStringToObject(o, "lookup_url", "https://www.icbc.com/vehicle-registration/Pages/default.aspx");
    cJSON_AddStringToObject(o, "service_type", "Vehicle Registration");
    cJSON_AddStringToObject(o, "country", "Canada");
    cJSON_AddStringToObject(o, "province", "British Columbia");
    cJSON_AddStringToObject(o, "coverage", "Registration, insurance, claims history");
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("Goo-net Japan");
    char u[512]; snprintf(u, sizeof u,
      "https://www.goo-net.com/usedcar/spread/goo/18/700050011030101001001%s.html", enc);
    cJSON_AddStringToObject(o, "lookup_url", u);
    cJSON_AddStringToObject(o, "service_type", "Used Car Database");
    cJSON_AddStringToObject(o, "country", "Japan");
    cJSON_AddStringToObject(o, "coverage", "Used car listings, vehicle history, specifications");
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("Car Sensor Japan");
    char u[512]; snprintf(u, sizeof u,
      "https://www.carsensor.net/usedcar/search.php?STID=CS210610&FREE_WORD=%s", enc);
    cJSON_AddStringToObject(o, "lookup_url", u);
    cJSON_AddStringToObject(o, "service_type", "Used Car Portal");
    cJSON_AddStringToObject(o, "country", "Japan");
    cJSON_AddStringToObject(o, "coverage", "Used cars, dealerships, market prices");
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("JAMA (Japan Auto Mfrs)");
    cJSON_AddStringToObject(o, "lookup_url", "https://www.jama.or.jp/");
    cJSON_AddStringToObject(o, "statistics_url", "https://www.jama-english.jp/statistics/");
    cJSON_AddStringToObject(o, "service_type", "Industry Association");
    cJSON_AddStringToObject(o, "country", "Japan");
    cJSON_AddStringToObject(o, "coverage", "Vehicle statistics, industry data, manufacturer info");
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("AIRIA Japan");
    cJSON_AddStringToObject(o, "lookup_url", "https://www.airia.or.jp/");
    cJSON_AddStringToObject(o, "service_type", "Vehicle Registration Data");
    cJSON_AddStringToObject(o, "country", "Japan");
    cJSON_AddStringToObject(o, "coverage", "Vehicle registration statistics, inspection data");
    cJSON_AddStringToObject(o, "note", "Automobile Inspection & Registration Information Association");
    cJSON_AddItemToArray(sources, o); swd++; }
  { cJSON *o = src_obj("Yahoo Japan Autos");
    char u[512]; snprintf(u, sizeof u, "https://carview.yahoo.co.jp/ncar/search/?q=%s", enc);
    cJSON_AddStringToObject(o, "lookup_url", u);
    cJSON_AddStringToObject(o, "service_type", "Car Portal");
    cJSON_AddStringToObject(o, "country", "Japan");
    cJSON_AddStringToObject(o, "coverage", "New cars, used cars, reviews, specifications");
    cJSON_AddItemToArray(sources, o); swd++; }

  cJSON_AddItemToObject(root, "sources", sources);
  cJSON_AddNumberToObject(root, "sources_checked", cJSON_GetArraySize(sources));
  cJSON_AddNumberToObject(root, "sources_with_data", swd);

  int conf = swd > 3 ? 85 : 70;
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", conf);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "VEHICLE_LOOKUP");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", conf);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300]; snprintf(rk, sizeof rk, "vehicle:%s", q);
  char title[320]; snprintf(title, sizeof title, "VEHICLE_LOOKUP — %s", q);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = (len == 17) ? "VIN lookup" : "license plate lookup";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"VEHICLE_LOOKUP\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def vehicle_lookup_def = {
  .id = "VEHICLE_LOOKUP", .collector = "osint",
  .name = "Vehicle Lookup", .name_ja = "車両照会",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(vehicle_lookup_def)
