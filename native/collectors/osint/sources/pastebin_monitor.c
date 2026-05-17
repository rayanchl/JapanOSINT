/* collectors/osint/sources/pastebin_monitor.c
 * OSINT service — faithful port of OSINTsaas osint_tools/pastebin_monitor.c
 * (handle_pastebin_monitor). The canonical SERVICE name in osint_dispatcher.c
 * service_registry[] bound to handle_pastebin_monitor is "PASTE_SITE_SEARCH"
 * (line 199; alias DATA_LEAK_MONITOR). On-demand (interval 0); the OSINT
 * dispatcher runs it with ctx->entity = an email/keyword (single entity, so
 * the upstream entity loop runs exactly once).
 *
 * Reproduces handle_pastebin_monitor() faithfully: searches psbdmp.ws
 * (paste-dump IDs), grep.app (code search hits), and web.archive.org CDX
 * (archived breach-related pages); aggregates them into the identical root
 * object: { service:"PASTEBIN_MONITOR", total_leaks_found, potential_exposure,
 * results:[{query, sources:[{name, findings:[…]}]}], note }. The upstream
 * keeps service_name "PASTEBIN_MONITOR" inside the data even though the
 * dispatcher's canonical name is PASTE_SITE_SEARCH — preserved verbatim. No
 * API key. success=true, confidence 85. Emits one osint_service_result row. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* url_encode: OSINTsaas keeps unreserved A-Za-z0-9 -_.~ and %-encodes rest. */
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

/* psbdmp.ws/api/v3/search/<q> → JSON array of paste IDs (limit 10) */
static cJSON *search_psbdmp(http_client *http, const char *query) {
  cJSON *results = cJSON_CreateArray();
  char *enc = url_encode_dup(query);
  if (!enc) return results;
  char url[512];
  snprintf(url, sizeof url, "https://psbdmp.ws/api/v3/search/%s", enc);
  free(enc);

  cJSON *json = feed_get_json(http, url, 30000);
  if (json && cJSON_IsArray(json)) {
    int count = cJSON_GetArraySize(json);
    for (int i = 0; i < count && i < 10; i++) {
      cJSON *item = cJSON_GetArrayItem(json, i);
      if (cJSON_IsString(item)) {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddStringToObject(r, "source", "psbdmp.ws");
        cJSON_AddStringToObject(r, "paste_id", item->valuestring);
        cJSON_AddStringToObject(r, "url", item->valuestring);
        cJSON_AddStringToObject(r, "type", "pastebin_dump");
        cJSON_AddItemToArray(results, r);
      }
    }
  }
  if (json) cJSON_Delete(json);
  return results;
}

/* grep.app/api/search?q=<q>&page=1 → hits.hits[]._source {repo,path} (limit 10) */
static cJSON *search_grep_app(http_client *http, const char *query) {
  cJSON *results = cJSON_CreateArray();
  char *enc = url_encode_dup(query);
  if (!enc) return results;
  char url[512];
  snprintf(url, sizeof url, "https://grep.app/api/search?q=%s&page=1", enc);
  free(enc);

  cJSON *json = feed_get_json(http, url, 30000);
  if (json) {
    cJSON *hits = cJSON_GetObjectItem(json, "hits");
    if (hits && cJSON_IsObject(hits)) {
      cJSON *hits_array = cJSON_GetObjectItem(hits, "hits");
      if (hits_array && cJSON_IsArray(hits_array)) {
        int count = cJSON_GetArraySize(hits_array);
        for (int i = 0; i < count && i < 10; i++) {
          cJSON *hit = cJSON_GetArrayItem(hits_array, i);
          if (!hit) continue;
          cJSON *source = cJSON_GetObjectItem(hit, "_source");
          if (!source) continue;
          cJSON *r = cJSON_CreateObject();
          cJSON_AddStringToObject(r, "source", "grep.app");
          cJSON *repo = cJSON_GetObjectItem(source, "repo");
          cJSON *path = cJSON_GetObjectItem(source, "path");
          if (repo && repo->valuestring)
            cJSON_AddStringToObject(r, "repository", repo->valuestring);
          if (path && path->valuestring)
            cJSON_AddStringToObject(r, "file_path", path->valuestring);
          cJSON_AddStringToObject(r, "type", "code_repository");
          cJSON_AddItemToArray(results, r);
        }
      }
    }
    cJSON_Delete(json);
  }
  return results;
}

/* web.archive.org CDX: rows of [urlkey,timestamp,original,...]; skip header */
static cJSON *search_breach_compilations(http_client *http, const char *query) {
  cJSON *results = cJSON_CreateArray();
  char *enc = url_encode_dup(query);
  if (!enc) return results;
  char url[1024];
  snprintf(url, sizeof url,
    "https://web.archive.org/cdx/search/cdx?url=*%s*&output=json&limit=5", enc);
  free(enc);

  cJSON *json = feed_get_json(http, url, 30000);
  if (json && cJSON_IsArray(json)) {
    int count = cJSON_GetArraySize(json);
    for (int i = 1; i < count && i < 6; i++) {
      cJSON *row = cJSON_GetArrayItem(json, i);
      if (row && cJSON_IsArray(row) && cJSON_GetArraySize(row) >= 3) {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddStringToObject(r, "source", "archive.org");
        cJSON *url_item = cJSON_GetArrayItem(row, 2);
        if (url_item && url_item->valuestring)
          cJSON_AddStringToObject(r, "archived_url", url_item->valuestring);
        cJSON *timestamp = cJSON_GetArrayItem(row, 1);
        if (timestamp && timestamp->valuestring)
          cJSON_AddStringToObject(r, "timestamp", timestamp->valuestring);
        cJSON_AddStringToObject(r, "type", "archived_page");
        cJSON_AddItemToArray(results, r);
      }
    }
  }
  if (json) cJSON_Delete(json);
  return results;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;

  cJSON *root = cJSON_CreateObject();
  cJSON *all_results = cJSON_CreateArray();
  int total_leaks = 0;

  /* dispatcher → single entity; upstream entity loop runs once */
  cJSON *entity_result = cJSON_CreateObject();
  cJSON_AddStringToObject(entity_result, "query", entity);
  cJSON *sources = cJSON_CreateArray();

  cJSON *psbdmp = search_psbdmp(ctx->http, entity);
  if (cJSON_GetArraySize(psbdmp) > 0) {
    total_leaks += cJSON_GetArraySize(psbdmp);
    cJSON *s = cJSON_CreateObject();
    cJSON_AddStringToObject(s, "name", "PasteBin Dumps");
    cJSON_AddItemToObject(s, "findings", psbdmp);
    cJSON_AddItemToArray(sources, s);
  } else {
    cJSON_Delete(psbdmp);
  }

  cJSON *grep = search_grep_app(ctx->http, entity);
  if (cJSON_GetArraySize(grep) > 0) {
    total_leaks += cJSON_GetArraySize(grep);
    cJSON *s = cJSON_CreateObject();
    cJSON_AddStringToObject(s, "name", "Code Repositories");
    cJSON_AddItemToObject(s, "findings", grep);
    cJSON_AddItemToArray(sources, s);
  } else {
    cJSON_Delete(grep);
  }

  cJSON *breach = search_breach_compilations(ctx->http, entity);
  if (cJSON_GetArraySize(breach) > 0) {
    total_leaks += cJSON_GetArraySize(breach);
    cJSON *s = cJSON_CreateObject();
    cJSON_AddStringToObject(s, "name", "Archived Pages");
    cJSON_AddItemToObject(s, "findings", breach);
    cJSON_AddItemToArray(sources, s);
  } else {
    cJSON_Delete(breach);
  }

  cJSON_AddItemToObject(entity_result, "sources", sources);
  cJSON_AddItemToArray(all_results, entity_result);

  cJSON_AddStringToObject(root, "service", "PASTEBIN_MONITOR");
  cJSON_AddNumberToObject(root, "total_leaks_found", total_leaks);
  cJSON_AddBoolToObject(root, "potential_exposure", total_leaks > 0);
  cJSON_AddItemToObject(root, "results", all_results);
  cJSON_AddStringToObject(root, "note",
    "Results indicate potential data exposure. Manual review recommended.");

  char *bj = cJSON_PrintUnformatted(root);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "PASTE_SITE_SEARCH");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", 1);          /* upstream success=true */
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "paste:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "PASTE_SITE_SEARCH — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = total_leaks > 0 ? "potential exposure found" : "no leaks found";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"PASTE_SITE_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  cJSON_Delete(root);
  return rc >= 0 ? 0 : -1;
}

static const source_def pastebin_monitor_def = {
  .id = "PASTE_SITE_SEARCH", .collector = "osint",
  .name = "Paste Site Search", .name_ja = "ペーストサイト検索",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(pastebin_monitor_def)
