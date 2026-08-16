/* collectors/osint/sources/phone_intel.c — phone-number OSINT, ported from
 * OSINTsaas osint_tools/phone_intel.c onto the JapanOSINT source ABI.
 *
 * Registers three entity-pivot services on a phone number:
 *   PHONE_LOOKUP     — E.164 validation + country/region + NumVerify enrichment
 *   CARRIER_LOOKUP   — carrier / line-type focus
 *   PHONE_REPUTATION — validity + line-type risk signal
 *
 * Real data only: deterministic E.164 analysis of the input (like
 * email_validator's format checks) plus live NumVerify enrichment when
 * NUMVERIFY_API_KEY is set. No key → validation-only (still real). Never
 * fabricates carrier/owner data. */
#include "source.h"
#include "core/httpclient.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct { const char *code, *country, *region; int min_len, max_len; } dialcode;

/* ITU-T E.164 dial codes (subset, Japan-first focus retained). */
static const dialcode DIAL_CODES[] = {
  {"1","United States/Canada","North America",10,10},
  {"44","United Kingdom","Europe",10,10},{"49","Germany","Europe",10,11},
  {"33","France","Europe",9,9},{"39","Italy","Europe",9,11},{"34","Spain","Europe",9,9},
  {"31","Netherlands","Europe",9,9},{"32","Belgium","Europe",8,9},{"41","Switzerland","Europe",9,9},
  {"43","Austria","Europe",10,13},{"45","Denmark","Europe",8,8},{"46","Sweden","Europe",9,9},
  {"47","Norway","Europe",8,8},{"48","Poland","Europe",9,9},{"351","Portugal","Europe",9,9},
  {"353","Ireland","Europe",9,9},{"358","Finland","Europe",9,10},{"380","Ukraine","Europe",9,9},
  {"7","Russia","Europe/Asia",10,10},
  {"81","Japan","Asia",10,10},{"82","South Korea","Asia",9,10},{"86","China","Asia",11,11},
  {"91","India","Asia",10,10},{"852","Hong Kong","Asia",8,8},{"65","Singapore","Asia",8,8},
  {"60","Malaysia","Asia",9,10},{"66","Thailand","Asia",9,9},{"63","Philippines","Asia",10,10},
  {"84","Vietnam","Asia",9,10},{"62","Indonesia","Asia",10,12},{"61","Australia","Oceania",9,9},
  {"64","New Zealand","Oceania",9,10},
  {"971","UAE","Middle East",9,9},{"966","Saudi Arabia","Middle East",9,9},
  {"972","Israel","Middle East",9,9},{"90","Turkey","Middle East",10,10},{"98","Iran","Middle East",10,10},
  {"27","South Africa","Africa",9,9},{"20","Egypt","Africa",10,10},{"234","Nigeria","Africa",10,10},
  {"254","Kenya","Africa",9,9},{"212","Morocco","Africa",9,9},
  {"55","Brazil","South America",10,11},{"54","Argentina","South America",10,10},
  {"52","Mexico","North America",10,10},{"57","Colombia","South America",10,10},
  {"56","Chile","South America",9,9},{"51","Peru","South America",9,9},
  {NULL,NULL,NULL,0,0}
};

typedef struct { const char *cc, *prefixes; } mobile_prefix;
static const mobile_prefix MOBILE_PREFIXES[] = {
  {"44","7"},{"49","15,16,17"},{"33","6,7"},{"1",""},{"91","7,8,9"},
  {"86","13,14,15,17,18,19"},{"81","70,80,90"},{"61","4"},{NULL,NULL}
};

/* Digits-only (keep leading +) into out (>=32). */
static void normalize_phone(const char *phone, char *out, size_t cap) {
  size_t j = 0; int plus = phone && phone[0] == '+';
  if (plus && j < cap - 1) out[j++] = '+';
  for (size_t i = plus ? 1 : 0; phone && phone[i] && j < cap - 1; i++)
    if (isdigit((unsigned char)phone[i])) out[j++] = phone[i];
  out[j] = 0;
}

static const dialcode *identify_country(const char *digits) {
  if (!digits || strlen(digits) < 2) return NULL;
  for (int width = 3; width >= 1; width--)
    for (int i = 0; DIAL_CODES[i].code; i++)
      if ((int)strlen(DIAL_CODES[i].code) == width &&
          strncmp(digits, DIAL_CODES[i].code, (size_t)width) == 0)
        return &DIAL_CODES[i];
  return NULL;
}

static int is_likely_mobile(const char *digits, const char *cc) {
  for (int i = 0; MOBILE_PREFIXES[i].cc; i++) {
    if (strcmp(MOBILE_PREFIXES[i].cc, cc) != 0) continue;
    if (!*MOBILE_PREFIXES[i].prefixes) return 1;  /* prefix not indicative */
    char buf[64]; snprintf(buf, sizeof buf, "%s", MOBILE_PREFIXES[i].prefixes);
    const char *national = digits + strlen(cc);
    char *save = NULL;            /* strtok_r: concurrent workers, see jsonlist.c */
    for (char *p = strtok_r(buf, ",", &save); p; p = strtok_r(NULL, ",", &save))
      if (strncmp(national, p, strlen(p)) == 0) return 1;
    return 0;
  }
  return 1;  /* default: mobile is most common */
}

/* Deterministic E.164 validation of the input (real analysis, no network). */
static cJSON *validate_phone(const char *norm) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "normalized", norm);
  int plus = norm[0] == '+';
  size_t digit_count = strlen(norm) - (plus ? 1 : 0);
  if (digit_count < 7 || digit_count > 15) {
    cJSON_AddBoolToObject(r, "valid", 0);
    cJSON_AddStringToObject(r, "error", "Invalid length (E.164 requires 7-15 digits)");
    return r;
  }
  const char *digits = plus ? norm + 1 : norm;
  const dialcode *c = identify_country(digits);
  if (c) {
    cJSON_AddStringToObject(r, "country_code", c->code);
    cJSON_AddStringToObject(r, "country", c->country);
    cJSON_AddStringToObject(r, "region", c->region);
    size_t national = digit_count - strlen(c->code);
    int ok = national >= (size_t)c->min_len && national <= (size_t)c->max_len;
    cJSON_AddBoolToObject(r, "valid", ok);
    cJSON_AddStringToObject(r, "likely_type",
                            is_likely_mobile(digits, c->code) ? "mobile" : "landline");
  } else {
    cJSON_AddBoolToObject(r, "valid", 1);
    cJSON_AddStringToObject(r, "country", "Unknown");
  }
  return r;
}

/* NumVerify enrichment (real). NULL when no key or no usable response. */
static cJSON *query_numverify(http_client *http, const char *norm) {
  const char *key = getenv("NUMVERIFY_API_KEY");
  if (!key || !*key) return NULL;
  const char *num = norm[0] == '+' ? norm + 1 : norm;
  char url[512];
  snprintf(url, sizeof url,
           "http://apilayer.net/api/validate?access_key=%s&number=%s&format=1", key, num);
  http_response hr = {0};
  if (http_request(http, "GET", url, NULL, NULL, 0, 10000, 2, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  if (!j) return NULL;
  cJSON *succ = cJSON_GetObjectItem(j, "success");
  if (succ && cJSON_IsBool(succ) && !cJSON_IsTrue(succ)) { cJSON_Delete(j); return NULL; }
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "numverify.com");
  struct { const char *src, *dst; } map[] = {
    {"valid", "valid"}, {"local_format", "local_format"},
    {"international_format", "international_format"}, {"country_name", "country"},
    {"location", "location"}, {"carrier", "carrier"}, {"line_type", "line_type"}, {NULL, NULL}
  };
  for (int i = 0; map[i].src; i++) {
    cJSON *v = cJSON_GetObjectItem(j, map[i].src);
    if (!v) continue;
    if (cJSON_IsBool(v)) cJSON_AddBoolToObject(r, map[i].dst, cJSON_IsTrue(v));
    else if (cJSON_IsString(v) && v->valuestring && *v->valuestring)
      cJSON_AddStringToObject(r, map[i].dst, v->valuestring);
  }
  cJSON_Delete(j);
  return r;
}

/* mode: 0=lookup, 1=carrier, 2=reputation. */
static int phone_run(const source_ctx *ctx, intel_sink *sink,
                     const char *service, const char *tags, int mode) {
  const char *phone = ctx->entity;
  if (!phone || !*phone) return 0;
  char norm[32]; normalize_phone(phone, norm, sizeof norm);
  size_t digit_count = strlen(norm) - (norm[0] == '+' ? 1 : 0);
  if (digit_count < 7 || digit_count > 15) return 0;  /* not a phone → honest empty */

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "service", service);
  cJSON_AddStringToObject(data, "entity", phone);
  cJSON *val = validate_phone(norm);
  cJSON *nv = query_numverify(ctx->http, norm);

  if (mode == 1) {                       /* CARRIER_LOOKUP */
    cJSON *lt = nv ? cJSON_GetObjectItem(nv, "line_type") : NULL;
    cJSON *car = nv ? cJSON_GetObjectItem(nv, "carrier") : NULL;
    if (car && cJSON_IsString(car)) cJSON_AddStringToObject(data, "carrier", car->valuestring);
    cJSON_AddStringToObject(data, "line_type",
      (lt && cJSON_IsString(lt)) ? lt->valuestring
      : (cJSON_IsString(cJSON_GetObjectItem(val, "likely_type"))
         ? cJSON_GetObjectItem(val, "likely_type")->valuestring : "unknown"));
  } else if (mode == 2) {                /* PHONE_REPUTATION */
    cJSON *vv = cJSON_GetObjectItem(nv ? nv : val, "valid");
    int valid = vv && cJSON_IsBool(vv) ? cJSON_IsTrue(vv) : 1;
    cJSON *lt = nv ? cJSON_GetObjectItem(nv, "line_type") : NULL;
    const char *line = (lt && cJSON_IsString(lt)) ? lt->valuestring : NULL;
    /* deterministic risk note from real signals only */
    const char *risk = !valid ? "high (invalid/unallocated number)"
                     : (line && (strstr(line, "voip") || strstr(line, "VoIP")))
                       ? "elevated (VoIP — disposable/spoofable)" : "baseline";
    cJSON_AddStringToObject(data, "risk", risk);
    cJSON_AddBoolToObject(data, "valid", valid);
  }

  cJSON_AddItemToObject(data, "validation", val);   /* val ownership → data */
  if (nv) cJSON_AddItemToObject(data, "numverify", nv);

  char *bj = cJSON_PrintUnformatted(data);
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", service);
  cJSON_AddStringToObject(props, "entity", phone);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[160], title[200];
  snprintf(rk, sizeof rk, "phone:%s:%s", service, norm);
  snprintf(title, sizeof title, "%s — %s", service, phone);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = service;
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = tags;
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  /* run() is a STATUS code, not a row count: core/scheduler.c does
   * `status = rc == 0 ? "ok" : "error"`. Returning 1 here marked every
   * successful lookup as an errored run. */
  return rc >= 0 ? 0 : -1;
}

static int run_lookup(const source_ctx *ctx, intel_sink *sink) {
  return phone_run(ctx, sink, "PHONE_LOOKUP",
                   "[\"osint-search\",\"PHONE_LOOKUP\"]", 0);
}
static int run_carrier(const source_ctx *ctx, intel_sink *sink) {
  return phone_run(ctx, sink, "CARRIER_LOOKUP",
                   "[\"osint-search\",\"CARRIER_LOOKUP\"]", 1);
}
static int run_reputation(const source_ctx *ctx, intel_sink *sink) {
  return phone_run(ctx, sink, "PHONE_REPUTATION",
                   "[\"osint-search\",\"PHONE_REPUTATION\"]", 2);
}

static const source_def phone_lookup_def = {
  .id = "PHONE_LOOKUP", .collector = "osint",
  .name = "Phone Lookup", .run = run_lookup,
  .category = "investigation", .type = "api", .url = "http://apilayer.net/",
  .description = "E.164 phone validation, country/region, carrier & line type (NumVerify).",
  .free_tier = 1,
};
REGISTER_SOURCE(phone_lookup_def)

static const source_def carrier_lookup_def = {
  .id = "CARRIER_LOOKUP", .collector = "osint",
  .name = "Carrier Lookup", .run = run_carrier,
  .category = "investigation", .type = "api", .url = "http://apilayer.net/",
  .description = "Phone carrier and line-type identification (NumVerify).",
  .free_tier = 1,
};
REGISTER_SOURCE(carrier_lookup_def)

static const source_def phone_reputation_def = {
  .id = "PHONE_REPUTATION", .collector = "osint",
  .name = "Phone Reputation", .run = run_reputation,
  .category = "investigation", .type = "api", .url = "http://apilayer.net/",
  .description = "Phone validity + line-type risk signal (VoIP/invalid).",
  .free_tier = 1,
};
REGISTER_SOURCE(phone_reputation_def)
