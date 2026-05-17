/* collectors/osint/sources/username_osint.c
 * OSINT service — port of OSINTsaas osint_tools/username_osint.c
 * (username_checker_service → handle_sherlock_search). Canonical SERVICE =
 * SHERLOCK_SEARCH — the FIRST registry name bound to handle_sherlock_search
 * (osint_dispatcher.c:149 {SERVICE_SHERLOCK_SEARCH, handle_sherlock_search,
 * "SHERLOCK_SEARCH", true}; WHATSMYNAME_SEARCH / USERNAME_CHECKER alias the
 * same handler/fn, so SHERLOCK_SEARCH is the dedup id). On-demand (interval 0);
 * ctx->entity = a username. No key. Faithfully reproduces username_osint_search
 * over the verbatim 511-entry platform table: replace_username_placeholder
 * (first "{}" only), check_username_on_platform direct detection
 * (status_code → http==expected; response_text/message_check → !strstr(body,
 * needle); regex_check → POSIX-extended match), result_builder add of every
 * FOUND platform {platform,category,profile_url,http_status,response_time_ms}.
 * Faithful-port note: the C tool's optional LLaMA page-analysis fallback
 * (llama_analyze_page_for_query, OSINTsaas-only infra invoked only when direct
 * detection is inconclusive — HTTP 200-but-not-found / 403 / 429) is omitted;
 * the unified JS pipeline also runs username search direct-only, and the
 * direct-detection result shape is preserved unchanged. Sequential (one entity
 * per OSINT dispatch — no pthread batch needed; matches the dispatcher calling
 * convention). confidence 95 if any profile found else 65. Emits ONE
 * osint_service_result row like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <time.h>

typedef struct {
  const char *name;
  const char *url_template;
  const char *check_type;        /* status_code|response_text|message_check|regex_check */
  int   expected_status;         /* status_code */
  const char *needle;            /* response_text/message_check NOT-FOUND text or regex */
  const char *category;
  int   enabled;
} platform_t;

static const platform_t PLATFORMS[] = {
#include "username_osint_platforms.inc"
};

static void fill_url(char *dst, size_t cap, const char *tmpl, const char *user) {
  const char *ph = strstr(tmpl, "{}");
  if (ph) {
    size_t pre = (size_t)(ph - tmpl);
    if (pre >= cap) pre = cap - 1;
    memcpy(dst, tmpl, pre);
    dst[pre] = 0;
    strncat(dst, user, cap - strlen(dst) - 1);
    strncat(dst, ph + 2, cap - strlen(dst) - 1);
  } else {
    strncpy(dst, tmpl, cap - 1);
    dst[cap - 1] = 0;
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *user = ctx->entity;
  if (!user || !*user) return -1;

  int total = (int)(sizeof PLATFORMS / sizeof PLATFORMS[0]);
  cJSON *items = cJSON_CreateArray();
  int found_count = 0, checked = 0;

  for (int i = 0; i < total; i++) {
    const platform_t *p = &PLATFORMS[i];
    if (!p->enabled) continue;
    if (ctx->cancel && *ctx->cancel) break;

    char url[1024];
    fill_url(url, sizeof url, p->url_template, user);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    http_response hr = {0};
    int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 12000, 0, &hr);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (t1.tv_sec - t0.tv_sec) * 1000.0 +
                (t1.tv_nsec - t0.tv_nsec) / 1e6;

    if (hc != 0 || !hr.body) { http_response_free(&hr); checked++; continue; }
    checked++;

    int found = 0;
    if (strcmp(p->check_type, "status_code") == 0) {
      found = (hr.status == p->expected_status);
    } else if (strcmp(p->check_type, "response_text") == 0 ||
               strcmp(p->check_type, "message_check") == 0) {
      found = (p->needle && *p->needle) ? (strstr(hr.body, p->needle) == NULL) : 0;
    } else if (strcmp(p->check_type, "regex_check") == 0) {
      regex_t re;
      if (p->needle && regcomp(&re, p->needle, REG_EXTENDED) == 0) {
        found = (regexec(&re, hr.body, 0, NULL, 0) == 0);
        regfree(&re);
      }
    }
    long status = hr.status;
    http_response_free(&hr);

    if (found) {
      found_count++;
      cJSON *it = cJSON_CreateObject();
      cJSON_AddStringToObject(it, "platform", p->name);
      cJSON_AddStringToObject(it, "category", p->category);
      cJSON_AddStringToObject(it, "profile_url", url);
      cJSON_AddNumberToObject(it, "http_status", (double)status);
      cJSON_AddNumberToObject(it, "response_time_ms", ms);
      cJSON_AddItemToArray(items, it);
    }
  }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "username", user);
  cJSON_AddStringToObject(data, "type", "username");
  cJSON_AddNumberToObject(data, "platforms_checked", checked);
  cJSON_AddNumberToObject(data, "profiles_found", found_count);
  cJSON_AddItemToObject(data, "items", items);

  int conf = found_count > 0 ? 95 : 65;
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", conf);
  cJSON_AddItemToObject(env, "data", data);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SHERLOCK_SEARCH");
  cJSON_AddStringToObject(props, "entity", user);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", conf);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300]; snprintf(rk, sizeof rk, "username:%s", user);
  char title[320]; snprintf(title, sizeof title, "SHERLOCK_SEARCH — %s", user);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = found_count > 0 ? "profiles found" : "no profiles found";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SHERLOCK_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def username_osint_def = {
  .id = "SHERLOCK_SEARCH", .collector = "osint",
  .name = "Username OSINT", .name_ja = "ユーザー名OSINT",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(username_osint_def)
