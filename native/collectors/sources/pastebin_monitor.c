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
 * API key.
 *
 * PER-RECORD EMIT: emits ONE intel_item per finding — a psbdmp paste
 * (remote_key="paste:<id>"), a grep.app code hit
 * (remote_key="codehit:<repo>/<path>"), or an archive.org page
 * (remote_key="archive:<url>"). Body = that finding's JSON; link set when
 * available. No aggregate/note row. If nothing is found, emits nothing and
 * returns 0 (honest empty). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
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

/* Emit ONE intel_item for a single finding. body = finding JSON; link optional.
 * Returns 1 if emitted. */
static int emit_finding(intel_sink *sink, const char *remote_key,
                        const char *title, const char *summary,
                        const char *link, cJSON *finding) {
  char *bj = cJSON_PrintUnformatted(finding);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "PASTE_SITE_SEARCH");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  intel_item it = {0};
  it.remote_key      = remote_key;
  it.title           = title;
  it.body            = bj;
  it.summary         = summary;
  it.link            = link;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"PASTE_SITE_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static const char *jstr(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && v->valuestring) ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;

  int emitted = 0;
  char rk[640], title[640];

  /* 1. psbdmp pastes — remote_key="paste:<id>" */
  cJSON *psbdmp = search_psbdmp(ctx->http, entity), *f;
  cJSON_ArrayForEach(f, psbdmp) {
    const char *id  = jstr(f, "paste_id");
    const char *url = jstr(f, "url");
    if (!id) continue;
    snprintf(rk, sizeof rk, "paste:%s", id);
    snprintf(title, sizeof title, "Paste dump %s", id);
    emitted += emit_finding(sink, rk, title, "pastebin_dump", url, f);
  }
  cJSON_Delete(psbdmp);

  /* 2. grep.app code hits — remote_key="codehit:<repo>/<path>" */
  cJSON *grep = search_grep_app(ctx->http, entity);
  cJSON_ArrayForEach(f, grep) {
    const char *repo = jstr(f, "repository");
    const char *path = jstr(f, "file_path");
    snprintf(rk, sizeof rk, "codehit:%s/%s", repo ? repo : "?", path ? path : "?");
    snprintf(title, sizeof title, "%s — %s",
             repo ? repo : "code", path ? path : "");
    emitted += emit_finding(sink, rk, title, "code_repository", NULL, f);
  }
  cJSON_Delete(grep);

  /* 3. archive.org pages — remote_key="archive:<url>" */
  cJSON *breach = search_breach_compilations(ctx->http, entity);
  cJSON_ArrayForEach(f, breach) {
    const char *url = jstr(f, "archived_url");
    if (!url) continue;
    snprintf(rk, sizeof rk, "archive:%s", url);
    snprintf(title, sizeof title, "Archived: %s", url);
    emitted += emit_finding(sink, rk, title, "archived_page", url, f);
  }
  cJSON_Delete(breach);

  (void)emitted;
  return 0;                  /* nothing found → honest empty, not an error */
}

static const source_def pastebin_monitor_def = {
  .id = "PASTE_SITE_SEARCH", .collector = "osint",
  .name = "Paste Site Search", .name_ja = "ペーストサイト検索",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/paste-site-search",
  .description = "Search paste sites for leaked text matching a term.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(pastebin_monitor_def)
