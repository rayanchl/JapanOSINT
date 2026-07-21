/* collectors/sources/bo_world.c
 * OSINT services — global beneficial-ownership & legal-entity identifiers.
 * On-demand entity pivot (ctx->entity = a company / legal-entity name) against
 * the real, keyless public registers:
 *
 *   GLEIF_LEI       GLEIF — the authoritative Legal Entity Identifier (LEI)
 *                   index. api.gleif.org/api/v1/lei-records filtered by
 *                   entity.legalName (L1 reference data: legal name, LEI,
 *                   jurisdiction, registration status, address).
 *   GLEIF_RELATIONS GLEIF Level-2 relationship data — the direct/ultimate
 *                   parent(s) reported for the matched LEI(s)
 *                   (…/lei-records/<lei>/direct-parent + ultimate-parent).
 *   OPENOWNERSHIP   OpenOwnership Register — the open beneficial-ownership data
 *                   store (register.openownership.org search + entity API),
 *                   returning statements that link entities to the people who
 *                   ultimately own or control them.
 *
 * All three are keyless LIVE. Each run() REAL-fetches the JSON:API / register
 * responses and emits ONLY parsed real fields, one intel_item per record, or
 * honest-empty (return 0) on fetch failure / no match. Nothing is fabricated.
 *
 * One run() dispatches on ctx->source_id. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* GLEIF JSON:API attribute helpers -------------------------------------- */

/* nested attribute: obj->attributes->...->key (single level under attributes) */
static const char *bo_attr(const cJSON *rec, const char *key) {
  const cJSON *at = cJSON_GetObjectItem(rec, "attributes");
  return at ? jo_sv(at, key) : NULL;
}

/* ---- GLEIF LEI records (L1 reference data) ----------------------------- *
 * GET api.gleif.org/api/v1/lei-records?filter[entity.legalName]=<name>
 * → { data: [ { id:"<LEI>", attributes:{ lei, entity:{ legalName:{name},
 *     legalAddress:{...}, jurisdiction, status }, registration:{status} } } ] }
 */
static int bo_emit_lei(intel_sink *sink, const cJSON *rec, const char *query) {
  if (!rec) return 0;
  const char *lei = jo_sv(rec, "id");
  const cJSON *at = cJSON_GetObjectItem(rec, "attributes");
  if (!at) return 0;
  if (!lei) lei = jo_sv(at, "lei");

  const cJSON *ent  = cJSON_GetObjectItem(at, "entity");
  const cJSON *ln   = ent ? cJSON_GetObjectItem(ent, "legalName") : NULL;
  const char *name  = ln ? jo_sv(ln, "name") : NULL;
  const char *juris = ent ? jo_sv(ent, "jurisdiction") : NULL;
  const char *estat = ent ? jo_sv(ent, "status") : NULL;
  const cJSON *addr = ent ? cJSON_GetObjectItem(ent, "legalAddress") : NULL;
  const char *city  = addr ? jo_sv(addr, "city") : NULL;
  const char *ctry  = addr ? jo_sv(addr, "country") : NULL;
  const cJSON *reg  = cJSON_GetObjectItem(at, "registration");
  const char *rstat = reg ? jo_sv(reg, "status") : NULL;
  const char *rdate = reg ? jo_sv(reg, "initialRegistrationDate") : NULL;

  if (!name && !lei) return 0;

  char loc[256] = {0};
  if (city || ctry)
    snprintf(loc, sizeof loc, "%s%s%s", city ? city : "",
             (city && ctry) ? ", " : "", ctry ? ctry : "");

  cJSON *data = cJSON_CreateObject();
  if (name)  cJSON_AddStringToObject(data, "legal_name", name);
  if (lei)   cJSON_AddStringToObject(data, "lei", lei);
  if (juris) cJSON_AddStringToObject(data, "jurisdiction", juris);
  if (estat) cJSON_AddStringToObject(data, "entity_status", estat);
  if (rstat) cJSON_AddStringToObject(data, "registration_status", rstat);
  if (loc[0]) cJSON_AddStringToObject(data, "legal_address", loc);
  cJSON_AddStringToObject(data, "source", "GLEIF");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "GLEIF_LEI");
  cJSON_AddStringToObject(props, "source", "GLEIF");
  if (lei)   cJSON_AddStringToObject(props, "lei", lei);
  if (juris) cJSON_AddStringToObject(props, "jurisdiction", juris);
  if (query) cJSON_AddStringToObject(props, "query", query);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[128] = {0};
  if (lei) snprintf(link, sizeof link, "https://search.gleif.org/#/record/%s", lei);

  intel_item it = {0};
  it.remote_key      = lei ? lei : name;
  it.title           = name ? name : lei;
  it.summary         = loc[0] ? loc : juris;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = rdate;
  it.link            = link[0] ? link : NULL;
  it.record_type     = "gleif-lei";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"GLEIF_LEI\",\"legal-entity\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int bo_gleif_lei(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.gleif.org/api/v1/lei-records?filter[entity.legalName]=%s&page[size]=15",
    enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/vnd.api+json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "GLEIF_LEI");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *arr = cJSON_GetObjectItem(root, "data");
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *r;
    cJSON_ArrayForEach(r, arr) emitted += bo_emit_lei(sink, r, q);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[GLEIF_LEI] emitted %d\n", emitted);
  return emitted;
}

/* ---- GLEIF Level-2 relationships (parents) ----------------------------- *
 * First resolve the LEI(s) for the entity name (same legalName filter), then
 * for each LEI fetch its direct-parent and ultimate-parent relationship
 * records and emit one item per reported parent. Parents that are not reported
 * yield GLEIF 404 with a reason → honest empty for that LEI. */
static int bo_emit_relation(intel_sink *sink, const cJSON *root,
                            const char *child_lei, const char *reltype) {
  /* the …/direct-parent (or ultimate-parent) endpoint returns a single
   * lei-record for the parent under data{}. */
  cJSON *rec = cJSON_GetObjectItem(root, "data");
  if (!cJSON_IsObject(rec)) return 0;
  const char *plei = jo_sv(rec, "id");
  const cJSON *at  = cJSON_GetObjectItem(rec, "attributes");
  if (!plei && at) plei = jo_sv(at, "lei");
  const cJSON *ent = at ? cJSON_GetObjectItem(at, "entity") : NULL;
  const cJSON *ln  = ent ? cJSON_GetObjectItem(ent, "legalName") : NULL;
  const char *pname = ln ? jo_sv(ln, "name") : NULL;
  const char *juris = ent ? jo_sv(ent, "jurisdiction") : NULL;
  if (!plei && !pname) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "relationship", reltype);
  if (child_lei) cJSON_AddStringToObject(data, "child_lei", child_lei);
  if (pname) cJSON_AddStringToObject(data, "parent_name", pname);
  if (plei)  cJSON_AddStringToObject(data, "parent_lei", plei);
  if (juris) cJSON_AddStringToObject(data, "parent_jurisdiction", juris);
  cJSON_AddStringToObject(data, "source", "GLEIF");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "GLEIF_RELATIONS");
  cJSON_AddStringToObject(props, "source", "GLEIF");
  cJSON_AddStringToObject(props, "relationship", reltype);
  if (child_lei) cJSON_AddStringToObject(props, "child_lei", child_lei);
  if (plei) cJSON_AddStringToObject(props, "parent_lei", plei);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char title[512];
  snprintf(title, sizeof title, "%s → %s",
           reltype, pname ? pname : (plei ? plei : "?"));
  char key[256];
  snprintf(key, sizeof key, "%s|%s|%s", child_lei ? child_lei : "",
           reltype, plei ? plei : (pname ? pname : ""));
  char link[128] = {0};
  if (plei) snprintf(link, sizeof link, "https://search.gleif.org/#/record/%s", plei);

  intel_item it = {0};
  it.remote_key      = key;
  it.title           = title;
  it.summary         = juris;
  it.body            = bj;
  it.lang            = "en";
  it.link            = link[0] ? link : NULL;
  it.record_type     = "gleif-relationship";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"GLEIF_RELATIONS\",\"ownership\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int bo_gleif_relations(const source_ctx *ctx, intel_sink *sink,
                              const char *q) {
  /* step 1: resolve LEIs for the name (cap a few to bound requests) */
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.gleif.org/api/v1/lei-records?filter[entity.legalName]=%s&page[size]=5",
    enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/vnd.api+json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "GLEIF_RELATIONS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  char leis[5][32]; int nlei = 0;
  cJSON *arr = cJSON_GetObjectItem(root, "data");
  if (cJSON_IsArray(arr)) {
    cJSON *r;
    cJSON_ArrayForEach(r, arr) {
      const char *lei = jo_sv(r, "id");
      if (lei && nlei < 5) { snprintf(leis[nlei], sizeof leis[0], "%.31s", lei); nlei++; }
    }
  }
  cJSON_Delete(root);

  int emitted = 0;
  const char *kinds[2] = { "direct-parent", "ultimate-parent" };
  for (int i = 0; i < nlei; i++) {
    for (int k = 0; k < 2; k++) {
      char rurl[256];
      snprintf(rurl, sizeof rurl,
        "https://api.gleif.org/api/v1/lei-records/%s/%s", leis[i], kinds[k]);
      char *rb = jo_get(ctx, rurl, hdrs, "GLEIF_RELATIONS");
      if (!rb) continue;                 /* 404 = no reported parent → skip */
      cJSON *rr = cJSON_Parse(rb);
      free(rb);
      if (!rr) continue;
      emitted += bo_emit_relation(sink, rr, leis[i], kinds[k]);
      cJSON_Delete(rr);
    }
  }
  fprintf(stderr, "[GLEIF_RELATIONS] emitted %d\n", emitted);
  return emitted;
}

/* ---- OpenOwnership Register -------------------------------------------- *
 * The open BO register exposes a JSON search:
 *   register.openownership.org/search.json?q=<name>
 * → { results: [ { self:"/entities/<id>", name, ... } ] } (register app JSON).
 * We emit one item per matched entity, linking to its register page. Only
 * parsed real fields are surfaced. */
static int bo_emit_oo(intel_sink *sink, const cJSON *rec, const char *query) {
  if (!rec) return 0;
  const char *name = jo_sv(rec, "name");
  if (!name) name = jo_sv(rec, "company_number");
  const char *self = jo_sv(rec, "self");
  if (!self) self = jo_sv(rec, "url");
  const char *id = jo_sv(rec, "id");
  if (!name && !self && !id) return 0;

  const char *ctry = jo_sv(rec, "country");
  const char *ctype = jo_sv(rec, "type");           /* entity / person */
  const char *cnum = jo_sv(rec, "company_number");

  cJSON *data = cJSON_CreateObject();
  if (name)  cJSON_AddStringToObject(data, "name", name);
  if (ctype) cJSON_AddStringToObject(data, "type", ctype);
  if (cnum)  cJSON_AddStringToObject(data, "company_number", cnum);
  if (ctry)  cJSON_AddStringToObject(data, "country", ctry);
  if (id)    cJSON_AddStringToObject(data, "oo_id", id);
  cJSON_AddStringToObject(data, "source", "OpenOwnership");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  char link[512] = {0};
  if (self && strncmp(self, "http", 4) == 0)
    snprintf(link, sizeof link, "%s", self);
  else if (self)
    snprintf(link, sizeof link, "https://register.openownership.org%s", self);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "OPENOWNERSHIP");
  cJSON_AddStringToObject(props, "source", "OpenOwnership");
  if (ctype) cJSON_AddStringToObject(props, "type", ctype);
  if (ctry)  cJSON_AddStringToObject(props, "country", ctry);
  if (link[0]) cJSON_AddStringToObject(props, "href", link);
  if (query) cJSON_AddStringToObject(props, "query", query);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = id ? id : (link[0] ? link : name);
  it.title           = name ? name : id;
  it.summary         = ctry;
  it.body            = bj;
  it.lang            = "en";
  it.link            = link[0] ? link : NULL;
  it.record_type     = "openownership-entity";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"OPENOWNERSHIP\",\"beneficial-ownership\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int bo_openownership(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://register.openownership.org/search.json?q=%s", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "OPENOWNERSHIP");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  /* tolerate {results:[...]} or a bare [...] */
  cJSON *arr = cJSON_GetObjectItem(root, "results");
  if (!cJSON_IsArray(arr) && cJSON_IsArray(root)) arr = root;
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *r;
    cJSON_ArrayForEach(r, arr) emitted += bo_emit_oo(sink, r, q);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[OPENOWNERSHIP] emitted %d\n", emitted);
  return emitted;
}

/* ---- dispatch ---------------------------------------------------------- */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!q) return -1;
  if (strlen(q) < 3) {
    fprintf(stderr, "[bo_world] entity too short, skipping\n");
    return 0;
  }
  const char *id = ctx->source_id;
  if (strcmp(id, "GLEIF_LEI") == 0)        bo_gleif_lei(ctx, sink, q);
  else if (strcmp(id, "GLEIF_RELATIONS") == 0) bo_gleif_relations(ctx, sink, q);
  else if (strcmp(id, "OPENOWNERSHIP") == 0)   bo_openownership(ctx, sink, q);
  else return -1;
  return 0;   /* honest empty is not an error */
}

#define BO_DEF(SYM, ID, NAME, NAMEJA, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "government", .type = "api", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

BO_DEF(bo_gleif_lei_def, "GLEIF_LEI", "GLEIF LEI Records", "GLEIF LEI レコード",
  "https://api.gleif.org/api/v1/lei-records",
  "GLEIF Legal Entity Identifier index — screen a legal name for LEI reference data (keyless)");
BO_DEF(bo_gleif_rel_def, "GLEIF_RELATIONS", "GLEIF Ownership Relations", "GLEIF 所有関係",
  "https://api.gleif.org/api/v1/lei-records",
  "GLEIF Level-2 relationship data — direct & ultimate parents reported for an entity's LEI (keyless)");
BO_DEF(bo_oo_def, "OPENOWNERSHIP", "OpenOwnership Register", "OpenOwnership 登録簿",
  "https://register.openownership.org",
  "OpenOwnership open beneficial-ownership register — entities linked to their ultimate owners (keyless)");
