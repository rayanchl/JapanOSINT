/* collectors/sources/global_aleph.c
 * OSINT service — OCCRP Aleph (aleph.occrp.org). On-demand entity pivot
 * (person / company / any name) against Aleph's cross-border investigative
 * dataset of leaks, company registries, court records and sanctions lists.
 *
 * GET https://aleph.occrp.org/api/2/entities?q=<entity>&limit=15
 * Public collections are searchable keyless; when ALEPH_API_KEY is set it is
 * sent as "Authorization: ApiKey <key>" to widen access to private collections.
 * One intel_item per matched entity: name, schema, collection, countries.
 * Non-200 / no matches → honest empty (return 0). Never synthesizes records. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Aleph nests entity attributes under .properties.<key> as arrays of strings.
 * Return the first string value of properties[key], or NULL. */
static const char *aleph_prop1(const cJSON *props, const char *key) {
  if (!props) return NULL;
  const cJSON *arr = cJSON_GetObjectItem(props, key);
  if (cJSON_IsArray(arr)) {
    const cJSON *v = cJSON_GetArrayItem(arr, 0);
    if (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
      return v->valuestring;
  } else if (cJSON_IsString(arr) && arr->valuestring && arr->valuestring[0]) {
    return arr->valuestring;
  }
  return NULL;
}

/* Join all string values of properties[key] with ", " into buf. */
static void aleph_propjoin(const cJSON *props, const char *key,
                           char *buf, size_t cap) {
  buf[0] = 0;
  if (!props) return;
  const cJSON *arr = cJSON_GetObjectItem(props, key);
  if (!cJSON_IsArray(arr)) return;
  size_t j = 0; const cJSON *v;
  cJSON_ArrayForEach(v, arr) {
    if (!cJSON_IsString(v) || !v->valuestring || !v->valuestring[0]) continue;
    int n = snprintf(buf + j, cap - j, "%s%s", j ? ", " : "", v->valuestring);
    if (n < 0 || (size_t)n >= cap - j) { buf[cap - 1] = 0; break; }
    j += (size_t)n;
  }
}

static int emit_entity(intel_sink *sink, const cJSON *e) {
  if (!e) return 0;
  const char *id     = jo_sv(e, "id");
  const char *name   = jo_sv(e, "name");
  const char *schema = jo_sv(e, "schema");
  const cJSON *props = cJSON_GetObjectItem(e, "properties");
  if (!name) name = aleph_prop1(props, "name");
  if (!name && !id) return 0;

  /* collection label */
  const char *coll = NULL;
  const cJSON *cobj = cJSON_GetObjectItem(e, "collection");
  if (cJSON_IsObject(cobj)) coll = jo_sv(cobj, "label");
  if (!coll) coll = jo_sv(e, "collection_id");

  char countries[256]; aleph_propjoin(props, "country", countries, sizeof countries);
  if (!countries[0]) aleph_propjoin(props, "jurisdiction", countries, sizeof countries);

  const char *addr = aleph_prop1(props, "address");
  const char *reg  = aleph_prop1(props, "registrationNumber");

  cJSON *data = cJSON_CreateObject();
  if (name)         cJSON_AddStringToObject(data, "name", name);
  if (schema)       cJSON_AddStringToObject(data, "schema", schema);
  if (coll)         cJSON_AddStringToObject(data, "collection", coll);
  if (countries[0]) cJSON_AddStringToObject(data, "countries", countries);
  if (addr)         cJSON_AddStringToObject(data, "address", addr);
  if (reg)          cJSON_AddStringToObject(data, "registration_number", reg);
  cJSON_AddStringToObject(data, "source", "OCCRP Aleph");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "ALEPH_SEARCH");
  cJSON_AddStringToObject(p, "source", "OCCRP Aleph");
  if (schema)       cJSON_AddStringToObject(p, "schema", schema);
  if (coll)         cJSON_AddStringToObject(p, "collection", coll);
  if (countries[0]) cJSON_AddStringToObject(p, "countries", countries);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char link[512] = {0};
  if (id) snprintf(link, sizeof link, "https://aleph.occrp.org/entities/%s", id);

  intel_item it = {0};
  it.remote_key      = id ? id : name;
  it.title           = name ? name : id;
  it.summary         = schema;
  it.body            = bj;
  it.link            = link[0] ? link : NULL;
  it.record_type     = "aleph-entity";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"ALEPH_SEARCH\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[1024];
  snprintf(url, sizeof url,
           "https://aleph.occrp.org/api/2/entities?q=%s&limit=15", enc);
  free(enc);

  const char *key = jo_env("ALEPH_API_KEY");
  char authhdr[256];
  const char *hdrs_key[] = { authhdr, "Accept: application/json", NULL };
  const char *hdrs_pub[] = { "Accept: application/json", NULL };
  const char *const *hdrs;
  if (key) {
    snprintf(authhdr, sizeof authhdr, "Authorization: ApiKey %s", key);
    hdrs = hdrs_key;
  } else {
    /* Aleph closed anonymous API access: /api/2/entities now answers
     * 401 {"message":"You are not authorized to do this."} without a key, so
     * this is a gated source, not an empty one. Say so — otherwise it reads as
     * "searched and found nothing". */
    fprintf(stderr, "[aleph] gated (no ALEPH_API_KEY)\n");
    hdrs = hdrs_pub;
  }

  char *body = jo_get(ctx, url, hdrs, "aleph");
  if (!body) return 0;                 /* non-200 → honest empty */
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    cJSON *e; cJSON_ArrayForEach(e, results) emitted += emit_entity(sink, e);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[aleph] emitted %d\n", emitted);
  return 0;
}

static const source_def global_aleph_def = {
  .id = "ALEPH_SEARCH", .collector = "osint",
  .name = "OCCRP Aleph Search", .name_ja = "OCCRP Aleph 検索",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "https://aleph.occrp.org/api/2/entities",
  .description = "OCCRP Aleph — cross-border leaks, registries, court & sanctions entities (optional ALEPH_API_KEY)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(global_aleph_def)
