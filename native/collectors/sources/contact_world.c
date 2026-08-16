/* collectors/sources/contact_world.c
 * OSINT services — key-gated email/phone enrichment, pivot on ctx->entity.
 * Honest-empty when the key is unset.
 *
 *   EMAILREP_LOOKUP  emailrep.io/<email>                    EMAILREP_KEY
 *   NUMVERIFY_PHONE  apilayer.net/api/validate?number=      NUMVERIFY_KEY */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *CT_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run_emailrep(const source_ctx *ctx, intel_sink *sink, const char *enc, const char *raw) {
  const char *k = jo_env("EMAILREP_KEY");
  if (!k) { fprintf(stderr, "[EMAILREP_LOOKUP] no EMAILREP_KEY\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url, "https://emailrep.io/%s", enc);
  char keyh[160]; snprintf(keyh, sizeof keyh, "Key: %s", k);
  const char *hdrs[] = { "Accept: application/json", CT_UA, keyh, NULL };
  char *body = jo_get(ctx, url, hdrs, "EMAILREP_LOOKUP");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *email = jo_sv(root, "email");
  if (!email) { cJSON_Delete(root); return 0; }
  const char *reputation = jo_sv(root, "reputation");
  const cJSON *susp = cJSON_GetObjectItem(root, "suspicious");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "email", email);
  if (reputation) cJSON_AddStringToObject(d, "reputation", reputation);
  if (cJSON_IsBool(susp)) cJSON_AddBoolToObject(d, "suspicious", cJSON_IsTrue(susp));
  cJSON_AddStringToObject(d, "source", "EmailRep");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "EMAILREP_LOOKUP"); cJSON_AddStringToObject(p, "email", email);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char title[256]; snprintf(title, sizeof title, "%s — reputation %s", email, reputation ? reputation : "unknown");
  intel_item it = {0};
  it.remote_key = email; it.title = title; it.summary = reputation; it.body = bj; it.lang = "en";
  it.record_type = "email-reputation";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"EMAILREP_LOOKUP\"]";
  int rc = sink->emit(sink, &it); free(bj); free(pj); cJSON_Delete(root);
  (void)raw;
  return rc >= 0 ? 1 : 0;
}

static int run_numverify(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("NUMVERIFY_KEY");
  if (!k) { fprintf(stderr, "[NUMVERIFY_PHONE] no NUMVERIFY_KEY\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url, "https://apilayer.net/api/validate?access_key=%s&number=%s", k, enc);
  const char *hdrs[] = { "Accept: application/json", CT_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "NUMVERIFY_PHONE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *valid = cJSON_GetObjectItem(root, "valid");
  if (!cJSON_IsBool(valid)) { cJSON_Delete(root); return 0; }
  const char *number = jo_sv(root, "international_format");
  const char *country = jo_sv(root, "country_name");
  const char *carrier = jo_sv(root, "carrier");
  const char *linetype = jo_sv(root, "line_type");
  const char *loc = jo_sv(root, "location");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddBoolToObject(d, "valid", cJSON_IsTrue(valid));
  if (number) cJSON_AddStringToObject(d, "number", number);
  if (country) cJSON_AddStringToObject(d, "country", country);
  if (carrier) cJSON_AddStringToObject(d, "carrier", carrier);
  if (linetype) cJSON_AddStringToObject(d, "line_type", linetype);
  if (loc) cJSON_AddStringToObject(d, "location", loc);
  cJSON_AddStringToObject(d, "source", "Numverify");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "NUMVERIFY_PHONE"); cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char title[256];
  snprintf(title, sizeof title, "%s — %s%s%s", number ? number : ctx->entity,
           country ? country : "?", carrier ? " · " : "", carrier ? carrier : "");
  intel_item it = {0};
  it.remote_key = number ? number : ctx->entity; it.title = title; it.summary = carrier;
  it.body = bj; it.lang = "en";
  it.record_type = "phone-validation";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"NUMVERIFY_PHONE\"]";
  int rc = sink->emit(sink, &it); free(bj); free(pj); cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "EMAILREP_LOOKUP")) n = run_emailrep(ctx, sink, enc, q);
  else if (!strcmp(sid, "NUMVERIFY_PHONE")) n = run_numverify(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define CT_DEF(sym, ID, NM, CAT, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = CAT, \
    .type = "api", .url = "internal://osint/contact", .description = DESC, \
    .layer = NULL, .free_tier = 0 }; \
  REGISTER_SOURCE(sym)

CT_DEF(ct_emailrep_def, "EMAILREP_LOOKUP", "EmailRep Reputation", "cyber",
  "EmailRep (EMAILREP_KEY): email reputation and suspicious flag.");
CT_DEF(ct_numverify_def, "NUMVERIFY_PHONE", "Numverify Phone Validate", "telecom",
  "Numverify (NUMVERIFY_KEY): phone validation — country, carrier, line type.");
