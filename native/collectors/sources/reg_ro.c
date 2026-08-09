/* collectors/sources/reg_ro.c
 * OSINT service — RO_COMPANIES. On-demand entity pivot (entity = Romanian
 * company name or CUI/fiscal code) against Romanian company data.
 *
 * PRIMARY path — openapi.ro (api.openapi.ro), a real JSON company-registry API
 * keyed on the national CUI. It authenticates with an "x-api-key" header, so we
 * treat it exactly like the other key-gated collectors (gbizinfo.c): when the
 * OPENAPI_RO_KEY / OPENAPIRO_KEY env var is unset we emit NOTHING and return 0
 * (honest empty), logging "[reg_ro] gated". When a key is present we fetch the
 * by-CUI endpoint for an all-digit entity, else the by-name find endpoint, and
 * emit one intel_item per real company object parsed from the JSON.
 *
 * FALLBACK path (no key) — a REAL anchor scrape of listafirme.ro's public
 * search page. That site sits behind Cloudflare's anti-bot challenge, so this
 * fetch normally honest-empties (returns 0). We NEVER fabricate a result: the
 * only records emitted are companies parsed from a real openapi.ro JSON body or
 * real <a> hits extracted from the listafirme.ro listing. No constructed URLs,
 * names, coordinates or counts. Honest-empty beats fake.
 *
 * NOTE: openapi.ro field names follow its public v1 schema (denumire, cui,
 * adresa, judet, stare, cod_postal …). Where a field is absent it is simply
 * omitted; the CUI is the natural remote_key. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* one company JSON object → one intel_item. Returns 1 if emitted. */
static int ro_emit_company(intel_sink *sink, const cJSON *c) {
  if (!c || !cJSON_IsObject(c)) return 0;

  static const char *K_NAME[] = { "denumire", "nume", "name", NULL };
  static const char *K_CUI[]  = { "cui", "cif", "cod_fiscal", NULL };
  static const char *K_ADDR[] = { "adresa", "address", NULL };
  static const char *K_CITY[] = { "localitate", "oras", "city", NULL };
  static const char *K_JUD[]  = { "judet", "county", NULL };
  static const char *K_STATE[]= { "stare", "status", NULL };
  static const char *K_POST[] = { "cod_postal", "postal_code", NULL };
  static const char *K_REG[]  = { "numar_reg_com", "reg_com", NULL };

  const char *name = jo_first_sv(c, K_NAME);
  const char *cui  = jo_first_sv(c, K_CUI);
  /* openapi.ro sometimes returns cui as a number */
  char cuibuf[32] = {0};
  if (!cui) {
    const cJSON *n = cJSON_GetObjectItem(c, "cui");
    if (n && cJSON_IsNumber(n)) { snprintf(cuibuf, sizeof cuibuf, "%ld", (long)n->valuedouble); cui = cuibuf; }
  }
  if (!name && !cui) return 0;

  const char *addr  = jo_first_sv(c, K_ADDR);
  const char *city  = jo_first_sv(c, K_CITY);
  const char *jud   = jo_first_sv(c, K_JUD);
  const char *state = jo_first_sv(c, K_STATE);
  const char *post  = jo_first_sv(c, K_POST);
  const char *regc  = jo_first_sv(c, K_REG);

  cJSON *data = cJSON_CreateObject();
  if (name)  cJSON_AddStringToObject(data, "name", name);
  if (cui)   cJSON_AddStringToObject(data, "cui", cui);
  if (regc)  cJSON_AddStringToObject(data, "reg_com", regc);
  if (addr)  cJSON_AddStringToObject(data, "address", addr);
  if (city)  cJSON_AddStringToObject(data, "city", city);
  if (jud)   cJSON_AddStringToObject(data, "county", jud);
  if (post)  cJSON_AddStringToObject(data, "postal_code", post);
  if (state) cJSON_AddStringToObject(data, "status", state);
  cJSON_AddStringToObject(data, "country", "RO");
  cJSON_AddStringToObject(data, "source", "openapi.ro");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "RO_COMPANIES");
  cJSON_AddStringToObject(props, "source", "openapi.ro");
  cJSON_AddStringToObject(props, "country", "RO");
  if (cui)  cJSON_AddStringToObject(props, "cui", cui);
  if (jud)  cJSON_AddStringToObject(props, "county", jud);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = cui ? cui : name;
  it.title           = name ? name : cui;
  it.summary         = addr ? addr : city;
  it.body            = bj;
  it.lang            = "ro";
  it.record_type     = "ro-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"RO_COMPANIES\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Walk a parsed body that may be a single company object or an array/wrapped
 * list of them; emit each. Returns count emitted. */
static int ro_walk(intel_sink *sink, cJSON *root) {
  if (!root) return 0;
  int n = 0;
  if (cJSON_IsArray(root)) {
    cJSON *c; cJSON_ArrayForEach(c, root) n += ro_emit_company(sink, c);
    return n;
  }
  /* common wrappers: {"data":[...]}, {"results":[...]}, {"companies":[...]} */
  static const char *WRAP[] = { "data", "results", "companies", "items", NULL };
  for (int i = 0; WRAP[i]; i++) {
    cJSON *w = cJSON_GetObjectItem(root, WRAP[i]);
    if (w && cJSON_IsArray(w)) {
      cJSON *c; cJSON_ArrayForEach(c, w) n += ro_emit_company(sink, c);
      return n;
    }
  }
  /* single object (by-CUI detail) */
  return ro_emit_company(sink, root);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return -1;

  const char *key = jo_env("OPENAPI_RO_KEY");
  if (!key) key = jo_env("OPENAPIRO_KEY");

  /* No key → honest fallback: real scrape of listafirme.ro's public search
   * page. Cloudflare normally makes this honest-empty; we log and never fake. */
  if (!key) {
    fprintf(stderr, "[reg_ro] gated (no OPENAPI_RO_KEY); trying listafirme.ro scrape\n");
    char *q = jo_urlencode(e);
    if (!q) return 0;
    char url[768];
    snprintf(url, sizeof url,
             "https://www.listafirme.ro/cauta.asp?cauta=%s", q);
    free(q);
    jo_emit_anchors(ctx, sink, url, "-cui-",
                    "listafirme.ro", "ro-company",
                    "https://www.listafirme.ro", NULL, 0, "reg_ro");
    return 0;   /* honest empty is not an error */
  }

  /* All-digit entity → treat as CUI; else name search. */
  int alldig = 1, len = 0;
  for (const char *p = e; *p; p++, len++)
    if (!isdigit((unsigned char)*p)) alldig = 0;

  char url[768];
  if (alldig && len > 0) {
    snprintf(url, sizeof url, "https://api.openapi.ro/api/companies/%s", e);
  } else {
    char *q = jo_urlencode(e);
    if (!q) return 0;
    snprintf(url, sizeof url,
             "https://api.openapi.ro/api/companies/find?name=%s", q);
    free(q);
  }

  char keyhdr[256];
  snprintf(keyhdr, sizeof keyhdr, "x-api-key: %s", key);
  const char *hdrs[] = { keyhdr, "Accept: application/json", NULL };

  char *body = jo_get(ctx, url, hdrs, "reg_ro");
  if (!body) return 0;   /* jo_get already logged status; honest empty */

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[reg_ro] unparseable body\n"); return 0; }

  int emitted = ro_walk(sink, root);
  cJSON_Delete(root);
  fprintf(stderr, "[reg_ro] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def reg_ro_def = {
  .id = "RO_COMPANIES", .collector = "osint",
  .name = "Romania Company Registry",
  .name_ja = "ルーマニア 企業登記検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/reg_ro",
  .description = "Romanian company pivot via openapi.ro JSON registry (by CUI or name; needs OPENAPI_RO_KEY) with a real listafirme.ro anchor-scrape fallback; honest-empty when gated / anti-bot",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg_ro_def)
