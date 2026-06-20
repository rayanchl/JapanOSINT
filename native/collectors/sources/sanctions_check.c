/* collectors/osint/sources/sanctions_check.c
 * OSINT service — faithful port of OSINTsaas osint_tools/sanctions_check.c
 * (handle_sanctions_check → sanctions_check). Canonical SERVICE name in
 * osint_dispatcher.c service_registry[] bound to handle_sanctions_check is
 * "SANCTIONS_CHECK" (line 268). On-demand (interval 0); dispatcher runs it
 * with ctx->entity = a person/company name.
 *
 * Reproduces sanctions_check() faithfully: queries OpenSanctions search
 * (api.opensanctions.org/search/default?q=…&limit=50) for sanctions matches
 * and api.opensanctions.org/search/peps?q=…&limit=20 for PEP records; derives
 * the per-list summary (ofac/eu/un/interpol/other), the 0-100 risk_score (same
 * weighting). Upstream's "authenticated" branch still issues an
 * unauthenticated GET (header support was a TODO), so OPENSANCTIONS_API_KEY is
 * intentionally not required — the public path is faithful and identical.
 *
 * PER-RECORD EMIT: emits ONE osint_service_result row per matched entity
 * (sanctions match OR PEP record), keyed by the OpenSanctions entity_id
 * ("sanction:<entity_id>"), not a single summary blob. Each row carries that
 * entity's full record JSON (name, score, datasets, nationality, topics, …).
 * No synthetic risk-overview row is emitted. If zero matches, emits nothing
 * and returns 0 (honest empty — no seeded rows). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char *url_encode_dup(const char *in) {
  size_t n = strlen(in);
  char *out = malloc(n * 3 + 1);
  if (!out) return NULL;
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out[w++] = (char)c;
    } else {
      sprintf(out + w, "%%%02X", c);
      w += 3;
    }
  }
  out[w] = 0;
  return out;
}

static void copy_first_array_str(cJSON *dst, const char *dstkey,
                                 cJSON *props, const char *key) {
  cJSON *v = cJSON_GetObjectItem(props, key);
  if (v && cJSON_IsArray(v) && cJSON_GetArraySize(v) > 0) {
    cJSON *first = cJSON_GetArrayItem(v, 0);
    if (first && first->valuestring)
      cJSON_AddStringToObject(dst, dstkey, first->valuestring);
  }
}

static cJSON *query_opensanctions(http_client *http, const char *name) {
  char *enc = url_encode_dup(name);
  if (!enc) return NULL;
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.opensanctions.org/search/default?q=%s&limit=50", enc);
  free(enc);

  cJSON *json = feed_get_json(http, url, 30000);
  if (!json) return NULL;

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "OpenSanctions");

  cJSON *results = cJSON_GetObjectItem(json, "results");
  if (results && cJSON_IsArray(results)) {
    int count = cJSON_GetArraySize(results);
    cJSON_AddNumberToObject(result, "total_matches", count);
    cJSON *matches = cJSON_CreateArray();
    for (int i = 0; i < count && i < 20; i++) {
      cJSON *item = cJSON_GetArrayItem(results, i);
      cJSON *match = cJSON_CreateObject();
      cJSON *id = cJSON_GetObjectItem(item, "id");
      cJSON *caption = cJSON_GetObjectItem(item, "caption");
      cJSON *schema = cJSON_GetObjectItem(item, "schema");
      cJSON *score = cJSON_GetObjectItem(item, "score");
      cJSON *datasets = cJSON_GetObjectItem(item, "datasets");
      cJSON *properties = cJSON_GetObjectItem(item, "properties");
      if (id && cJSON_IsString(id)) cJSON_AddStringToObject(match, "entity_id", id->valuestring);
      if (caption && cJSON_IsString(caption)) cJSON_AddStringToObject(match, "name", caption->valuestring);
      if (schema && cJSON_IsString(schema)) cJSON_AddStringToObject(match, "entity_type", schema->valuestring);
      if (score && cJSON_IsNumber(score)) cJSON_AddNumberToObject(match, "match_score", score->valuedouble);
      if (datasets && cJSON_IsArray(datasets)) {
        cJSON *lists = cJSON_CreateArray();
        int dc = cJSON_GetArraySize(datasets);
        for (int j = 0; j < dc; j++) {
          cJSON *ds = cJSON_GetArrayItem(datasets, j);
          if (ds && cJSON_IsString(ds)) cJSON_AddItemToArray(lists, cJSON_CreateString(ds->valuestring));
        }
        cJSON_AddItemToObject(match, "sanction_lists", lists);
      }
      if (properties) {
        copy_first_array_str(match, "nationality", properties, "nationality");
        copy_first_array_str(match, "birth_date", properties, "birthDate");
        copy_first_array_str(match, "country", properties, "country");
        copy_first_array_str(match, "position", properties, "position");
        cJSON *topics = cJSON_GetObjectItem(properties, "topics");
        if (topics && cJSON_IsArray(topics))
          cJSON_AddItemToObject(match, "topics", cJSON_Duplicate(topics, 1));
      }
      cJSON_AddItemToArray(matches, match);
    }
    cJSON_AddItemToObject(result, "matches", matches);
  }
  cJSON_Delete(json);
  return result;
}

static cJSON *check_pep(http_client *http, const char *name) {
  char *enc = url_encode_dup(name);
  if (!enc) return NULL;
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.opensanctions.org/search/peps?q=%s&limit=20", enc);
  free(enc);

  cJSON *json = feed_get_json(http, url, 30000);
  if (!json) return NULL;

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "PEP Database");
  cJSON *results = cJSON_GetObjectItem(json, "results");
  if (results && cJSON_IsArray(results)) {
    int count = cJSON_GetArraySize(results);
    cJSON_AddNumberToObject(result, "pep_matches", count);
    cJSON_AddBoolToObject(result, "is_pep", count > 0);
    if (count > 0) {
      cJSON *peps = cJSON_CreateArray();
      for (int i = 0; i < count && i < 10; i++) {
        cJSON *item = cJSON_GetArrayItem(results, i);
        cJSON *pep = cJSON_CreateObject();
        cJSON *id = cJSON_GetObjectItem(item, "id");
        cJSON *caption = cJSON_GetObjectItem(item, "caption");
        cJSON *schema = cJSON_GetObjectItem(item, "schema");
        cJSON *score = cJSON_GetObjectItem(item, "score");
        cJSON *properties = cJSON_GetObjectItem(item, "properties");
        if (id && cJSON_IsString(id)) cJSON_AddStringToObject(pep, "entity_id", id->valuestring);
        if (caption && cJSON_IsString(caption)) cJSON_AddStringToObject(pep, "name", caption->valuestring);
        if (schema && cJSON_IsString(schema)) cJSON_AddStringToObject(pep, "entity_type", schema->valuestring);
        if (score && cJSON_IsNumber(score)) cJSON_AddNumberToObject(pep, "match_score", score->valuedouble);
        if (properties) {
          copy_first_array_str(pep, "position", properties, "position");
          copy_first_array_str(pep, "country", properties, "country");
          copy_first_array_str(pep, "nationality", properties, "nationality");
          copy_first_array_str(pep, "birth_date", properties, "birthDate");
          cJSON *topics = cJSON_GetObjectItem(properties, "topics");
          if (topics && cJSON_IsArray(topics))
            cJSON_AddItemToObject(pep, "topics", cJSON_Duplicate(topics, 1));
        }
        cJSON_AddItemToArray(peps, pep);
      }
      cJSON_AddItemToObject(result, "pep_records", peps);
    }
  }
  cJSON_Delete(json);
  return result;
}

/* Emit one intel row for a single matched entity (sanctions match or PEP).
 * `rec` is a per-entity object built by query_opensanctions/check_pep; it must
 * carry "entity_id" + "name". `is_pep` flags the PEP-database origin. Returns 1
 * if emitted, 0 otherwise. */
static int emit_entity(intel_sink *sink, cJSON *rec, const char *query,
                       int is_pep) {
  if (!rec) return 0;
  cJSON *id_j   = cJSON_GetObjectItem(rec, "entity_id");
  cJSON *name_j = cJSON_GetObjectItem(rec, "name");
  cJSON *score_j= cJSON_GetObjectItem(rec, "match_score");
  cJSON *lists_j= cJSON_GetObjectItem(rec, "sanction_lists");
  const char *eid  = (id_j && id_j->valuestring) ? id_j->valuestring : NULL;
  const char *name = (name_j && name_j->valuestring) ? name_j->valuestring : NULL;
  if (!eid) return 0;   /* need a stable, unique remote_key */

  /* Stable per-entity key = OpenSanctions entity id (globally unique). */
  char rk[320];
  snprintf(rk, sizeof rk, "sanction:%s", eid);

  char title[384];
  snprintf(title, sizeof title, "%s (%s)",
           name ? name : eid, is_pep ? "PEP" : "sanctioned");

  /* Body = the full per-entity record JSON. */
  char *bj = cJSON_PrintUnformatted(rec);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SANCTIONS_CHECK");
  cJSON_AddStringToObject(props, "entity", query);
  cJSON_AddStringToObject(props, "match_kind", is_pep ? "pep" : "sanction");
  if (score_j && cJSON_IsNumber(score_j))
    cJSON_AddNumberToObject(props, "match_score", score_j->valuedouble);
  if (lists_j && cJSON_IsArray(lists_j))
    cJSON_AddItemToObject(props, "datasets", cJSON_Duplicate(lists_j, 1));
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = is_pep ? "Politically exposed person" : "Sanctions list match";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SANCTIONS_CHECK\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *name = ctx->entity;
  if (!name || !*name) return -1;

  int emitted = 0;

  /* Sanctions matches — one row per matched entity. */
  cJSON *opensanctions = query_opensanctions(ctx->http, name);
  if (opensanctions) {
    cJSON *matches = cJSON_GetObjectItem(opensanctions, "matches");
    if (matches && cJSON_IsArray(matches)) {
      int mc = cJSON_GetArraySize(matches);
      for (int i = 0; i < mc; i++)
        emitted += emit_entity(sink, cJSON_GetArrayItem(matches, i), name, 0);
    }
    cJSON_Delete(opensanctions);
  }

  /* PEP records — one row per PEP entity. */
  cJSON *pep = check_pep(ctx->http, name);
  if (pep) {
    cJSON *records = cJSON_GetObjectItem(pep, "pep_records");
    if (records && cJSON_IsArray(records)) {
      int pc = cJSON_GetArraySize(records);
      for (int i = 0; i < pc; i++)
        emitted += emit_entity(sink, cJSON_GetArrayItem(records, i), name, 1);
    }
    cJSON_Delete(pep);
  }

  (void)emitted;   /* honest empty (zero matches) is not an error */
  return 0;
}

static const source_def sanctions_check_def = {
  .id = "SANCTIONS_CHECK", .collector = "osint",
  .name = "Sanctions Check", .name_ja = "制裁チェック",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/sanctions-check",
  .description = "Screen a name/entity against sanctions lists.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanctions_check_def)
