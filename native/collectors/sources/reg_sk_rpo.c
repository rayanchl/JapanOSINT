/* collectors/sources/reg_sk_rpo.c
 * OSINT service — SK_RPO. On-demand entity pivot against the Slovak Register
 * of Legal Entities (Register právnických osôb, RPO) OPEN API operated by the
 * Statistical Office of the Slovak Republic (api.statistics.sk/rpo). Keyless
 * and LIVE.
 *
 * For any entity we search /rpo/v1/search?fullName=<entity> and emit one
 * intel_item per matched legal entity: IČO (company id), full name, address
 * and legal/registration status. Only real, parsed API fields are emitted;
 * fetch failure / no matches → honest empty (return 0).
 *
 * The RPO OPEN response nests localized values in arrays of objects carrying a
 * "value" (and validity dates); helpers below pull the current "value" out of
 * either a plain string, an object with "value", or the first element of such
 * an array — tolerant of the documented shape without inventing anything. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* RPO wraps most localized fields as {"value": "...", "validFrom": ...} or as
 * an array of such objects. Return the string "value" of the first usable
 * element, tolerating a plain string or a bare object too. Non-owning. */
static const char *rpo_val(const cJSON *node) {
  if (!node) return NULL;
  if (cJSON_IsString(node))
    return node->valuestring && node->valuestring[0] ? node->valuestring : NULL;
  if (cJSON_IsArray(node)) {
    const cJSON *e;
    cJSON_ArrayForEach(e, node) {
      const char *v = rpo_val(e);
      if (v) return v;
    }
    return NULL;
  }
  if (cJSON_IsObject(node)) {
    const char *v = jo_sv(node, "value");
    return v;
  }
  return NULL;
}

/* Build a single-line address from an RPO address object (street, number,
 * postal code, municipality, country — whichever localized values exist).
 * malloc'd (caller frees) or NULL. */
static char *rpo_address(const cJSON *addr) {
  if (!addr || !cJSON_IsObject(addr)) return NULL;
  const char *parts[6]; int n = 0;
  const char *street = rpo_val(cJSON_GetObjectItem(addr, "street"));
  const char *bnum   = rpo_val(cJSON_GetObjectItem(addr, "buildingNumber"));
  if (!bnum) bnum    = rpo_val(cJSON_GetObjectItem(addr, "regNumber"));
  const char *post   = rpo_val(cJSON_GetObjectItem(addr, "postalCodes"));
  if (!post) post    = rpo_val(cJSON_GetObjectItem(addr, "postalCode"));
  const char *muni   = rpo_val(cJSON_GetObjectItem(addr, "municipality"));
  const char *ctry   = rpo_val(cJSON_GetObjectItem(addr, "country"));
  if (street) parts[n++] = street;
  if (bnum)   parts[n++] = bnum;
  if (post)   parts[n++] = post;
  if (muni)   parts[n++] = muni;
  if (ctry)   parts[n++] = ctry;
  if (!n) return NULL;
  size_t len = 0;
  for (int i = 0; i < n; i++) len += strlen(parts[i]) + 2;
  char *out = (char *)malloc(len + 1);
  if (!out) return NULL;
  out[0] = 0;
  size_t j = 0;
  for (int i = 0; i < n; i++) {
    if (j) { out[j++] = ' '; out[j] = 0; }
    strcpy(out + j, parts[i]); j += strlen(parts[i]);
  }
  return out;
}

/* one legal entity → one intel_item. Returns 1 if emitted. */
static int emit_entity(intel_sink *sink, const cJSON *e) {
  if (!e || !cJSON_IsObject(e)) return 0;

  const char *ico  = rpo_val(cJSON_GetObjectItem(e, "identifiers"));
  if (!ico) ico    = rpo_val(cJSON_GetObjectItem(e, "cin"));  /* company id no. */
  if (!ico) ico    = jo_sv(e, "ico");
  const char *name = rpo_val(cJSON_GetObjectItem(e, "fullNames"));
  if (!name) name  = rpo_val(cJSON_GetObjectItem(e, "fullName"));
  if (!name) name  = jo_sv(e, "name");
  if (!name && !ico) return 0;

  /* status: RPO carries legal/registration status as a localized value too */
  const char *status = rpo_val(cJSON_GetObjectItem(e, "legalStatuses"));
  if (!status) status = rpo_val(cJSON_GetObjectItem(e, "legalStatus"));
  if (!status) status = rpo_val(cJSON_GetObjectItem(e, "statuses"));
  if (!status) status = rpo_val(cJSON_GetObjectItem(e, "status"));

  const char *lform = rpo_val(cJSON_GetObjectItem(e, "legalForms"));
  if (!lform) lform = rpo_val(cJSON_GetObjectItem(e, "legalForm"));
  const char *est   = rpo_val(cJSON_GetObjectItem(e, "establishment"));
  if (!est) est     = jo_sv(e, "establishment");

  char *addr = rpo_address(cJSON_GetObjectItem(e, "address"));
  if (!addr) addr = rpo_address(cJSON_GetObjectItem(e, "addresses"));

  cJSON *data = cJSON_CreateObject();
  if (name)   cJSON_AddStringToObject(data, "name", name);
  if (ico)    cJSON_AddStringToObject(data, "ico", ico);
  if (addr)   cJSON_AddStringToObject(data, "address", addr);
  if (status) cJSON_AddStringToObject(data, "status", status);
  if (lform)  cJSON_AddStringToObject(data, "legal_form", lform);
  if (est)    cJSON_AddStringToObject(data, "established", est);
  cJSON_AddStringToObject(data, "source", "SK RPO");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SK_RPO");
  cJSON_AddStringToObject(props, "source", "SK RPO");
  if (ico)    cJSON_AddStringToObject(props, "ico", ico);
  if (status) cJSON_AddStringToObject(props, "status", status);
  if (addr)   cJSON_AddStringToObject(props, "address", addr);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = ico ? ico : name;
  it.title           = name ? name : ico;
  it.summary         = addr ? addr : status;
  it.body            = bj;
  it.lang            = "sk";
  it.published_at    = est;
  it.record_type     = "sk-rpo-entity";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SK_RPO\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(addr);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  char url[1024];
  snprintf(url, sizeof url,
    "https://api.statistics.sk/rpo/v1/search?fullName=%s", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "sk_rpo");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  /* RPO OPEN returns { "results": [ ... ] } */
  cJSON *results = cJSON_GetObjectItem(root, "results");
  if (!cJSON_IsArray(results)) results = cJSON_GetObjectItem(root, "content");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    cJSON *e;
    cJSON_ArrayForEach(e, results) emitted += emit_entity(sink, e);
  } else if (cJSON_IsArray(root)) {
    cJSON *e;
    cJSON_ArrayForEach(e, root) emitted += emit_entity(sink, e);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[sk_rpo] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def sk_rpo_def = {
  .id = "SK_RPO", .collector = "osint",
  .name = "Slovakia RPO Legal Entity Lookup",
  .name_ja = "スロバキア法人登記(RPO)検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://api.statistics.sk/rpo/v1/search",
  .description = "Slovak Register of Legal Entities (RPO OPEN, keyless): IČO, name, address, status",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sk_rpo_def)
