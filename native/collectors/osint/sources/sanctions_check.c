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
 * weighting), risk_level + recommendation. Builds the identical root:
 * { query, check_timestamp, opensanctions{…}, list_summary{…}, pep_check{…},
 *   risk_score, risk_level, recommendation }. Upstream's "authenticated"
 * branch still issues an unauthenticated GET (header support was a TODO), so
 * OPENSANCTIONS_API_KEY is intentionally not required — the public path is
 * faithful and identical. success=true, confidence 85. Emits one
 * osint_service_result row. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
        cJSON *caption = cJSON_GetObjectItem(item, "caption");
        cJSON *properties = cJSON_GetObjectItem(item, "properties");
        if (caption && cJSON_IsString(caption)) cJSON_AddStringToObject(pep, "name", caption->valuestring);
        if (properties) {
          copy_first_array_str(pep, "position", properties, "position");
          copy_first_array_str(pep, "country", properties, "country");
        }
        cJSON_AddItemToArray(peps, pep);
      }
      cJSON_AddItemToObject(result, "pep_records", peps);
    }
  }
  cJSON_Delete(json);
  return result;
}

/* OpenSanctions matches → ofac/eu/un/interpol/other summary (upstream logic) */
static cJSON *build_list_summary(cJSON *opensanctions) {
  cJSON *matches = cJSON_GetObjectItem(opensanctions, "matches");
  if (!matches || !cJSON_IsArray(matches)) return NULL;
  int ofac = 0, eu = 0, un = 0, interpol = 0, other = 0;
  int mc = cJSON_GetArraySize(matches);
  for (int i = 0; i < mc; i++) {
    cJSON *match = cJSON_GetArrayItem(matches, i);
    cJSON *lists = cJSON_GetObjectItem(match, "sanction_lists");
    if (lists && cJSON_IsArray(lists)) {
      int lc = cJSON_GetArraySize(lists);
      for (int j = 0; j < lc; j++) {
        cJSON *l = cJSON_GetArrayItem(lists, j);
        if (l && cJSON_IsString(l)) {
          const char *n = l->valuestring;
          if (strstr(n, "ofac") || strstr(n, "us_")) ofac++;
          else if (strstr(n, "eu_")) eu++;
          else if (strstr(n, "un_")) un++;
          else if (strstr(n, "interpol")) interpol++;
          else other++;
        }
      }
    }
  }
  cJSON *summary = cJSON_CreateObject();
  cJSON_AddNumberToObject(summary, "ofac_matches", ofac);
  cJSON_AddNumberToObject(summary, "eu_matches", eu);
  cJSON_AddNumberToObject(summary, "un_matches", un);
  cJSON_AddNumberToObject(summary, "interpol_matches", interpol);
  cJSON_AddNumberToObject(summary, "other_matches", other);
  return summary;
}

static int calculate_risk_score(cJSON *root) {
  int score = 0;
  cJSON *os = cJSON_GetObjectItem(root, "opensanctions");
  if (os) {
    cJSON *total = cJSON_GetObjectItem(os, "total_matches");
    if (total && cJSON_IsNumber(total)) {
      if (total->valueint > 0) score += 50;
      if (total->valueint > 5) score += 25;
    }
    cJSON *matches = cJSON_GetObjectItem(os, "matches");
    if (matches && cJSON_IsArray(matches)) {
      int count = cJSON_GetArraySize(matches);
      for (int i = 0; i < count; i++) {
        cJSON *m = cJSON_GetArrayItem(matches, i);
        cJSON *ms = cJSON_GetObjectItem(m, "match_score");
        if (ms && cJSON_IsNumber(ms)) {
          if (ms->valuedouble > 0.9) score += 15;
          else if (ms->valuedouble > 0.8) score += 10;
        }
      }
    }
  }
  cJSON *pep = cJSON_GetObjectItem(root, "pep_check");
  if (pep) {
    cJSON *is_pep = cJSON_GetObjectItem(pep, "is_pep");
    if (is_pep && cJSON_IsTrue(is_pep)) score += 30;
  }
  if (score > 100) score = 100;
  return score;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *name = ctx->entity;
  if (!name || !*name) return -1;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", name);
  time_t ts = time(NULL);
  cJSON_AddNumberToObject(root, "check_timestamp", (double)ts);

  cJSON *opensanctions = query_opensanctions(ctx->http, name);
  if (opensanctions) {
    cJSON *summary = build_list_summary(opensanctions);
    cJSON_AddItemToObject(root, "opensanctions", opensanctions);
    if (summary) cJSON_AddItemToObject(root, "list_summary", summary);
  }

  cJSON *pep = check_pep(ctx->http, name);
  if (pep) cJSON_AddItemToObject(root, "pep_check", pep);

  int risk_score = calculate_risk_score(root);
  cJSON_AddNumberToObject(root, "risk_score", risk_score);

  const char *risk_level;
  if (risk_score >= 80) risk_level = "CRITICAL";
  else if (risk_score >= 60) risk_level = "HIGH";
  else if (risk_score >= 40) risk_level = "MEDIUM";
  else if (risk_score >= 20) risk_level = "LOW";
  else risk_level = "CLEAR";
  cJSON_AddStringToObject(root, "risk_level", risk_level);

  const char *recommendation;
  if (risk_score >= 60)
    recommendation = "Further due diligence required. Manual review recommended.";
  else if (risk_score >= 20)
    recommendation = "Some matches found. Review results and verify identity.";
  else
    recommendation = "No significant matches found. Standard procedures apply.";
  cJSON_AddStringToObject(root, "recommendation", recommendation);

  char *bj = cJSON_PrintUnformatted(root);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SANCTIONS_CHECK");
  cJSON_AddStringToObject(props, "entity", name);
  cJSON_AddBoolToObject(props, "success", 1);          /* upstream success=true */
  cJSON_AddNumberToObject(props, "confidence", 85);
  cJSON_AddStringToObject(props, "risk_level", risk_level);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "sanctions:%s", name);
  char title[320];
  snprintf(title, sizeof title, "SANCTIONS_CHECK — %s", name);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = risk_level;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SANCTIONS_CHECK\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  cJSON_Delete(root);
  return rc >= 0 ? 0 : -1;
}

static const source_def sanctions_check_def = {
  .id = "SANCTIONS_CHECK", .collector = "osint",
  .name = "Sanctions Check", .name_ja = "制裁チェック",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(sanctions_check_def)
