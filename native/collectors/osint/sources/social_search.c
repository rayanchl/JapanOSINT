/* collectors/osint/sources/social_search.c
 * OSINT service — faithful port of the Holehe half of OSINTsaas
 * osint_tools/email_osint.c (the unified Holehe + theHarvester engine).
 * Canonical service (osint_dispatcher.c service_registry[]):
 *   SOCIAL_SEARCH           handle_social_search: '@' → holehe_check_email,
 *                           else → sherlock_search_username (deferred)
 * On-demand (interval 0). Pure-C HTTP (no key).
 *
 * Holehe path: the exact email_osint.c holehe_sites[] checked with
 * check_email_account's DIRECT detection (success/failure indicator on a 200) —
 * the conclusive primary path. OSINTsaas adds a LLaMA page-analysis FALLBACK
 * only when direct detection is inconclusive (ambiguous 200 / 403 / 429); that
 * fallback depends on the engine's page-analysis prompt+grammar which is not
 * exposed to a source_def, so only the faithful direct-detection result is
 * produced (no fabricated LLaMA verdicts) — confidence stays HIGH(90) on a
 * conclusive direct hit, MEDIUM(70) when inconclusive, exactly as the direct
 * branch sets it. Output == email_validator_service result_builder JSON shape
 * (reproduced inline): {query,query_type:"email",results:[{source,found,data:
 * {platform,account_exists,info},confidence,detection_method:"direct"}],
 * summary:{...}}; envelope confidence found?90:70. Emits ONE
 * osint_service_result row; body = {success,confidence,data,error?} envelope,
 * like ip_geolocation.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

/* email_osint.c holehe_sites[] (verbatim). */
typedef struct {
  const char *name;
  const char *url;
  const char *method;
  const char *data_template;
  const char *success_indicator;
  const char *failure_indicator;
  int use_json;
} holehe_site_t;

static const holehe_site_t holehe_sites[] = {
  {"Instagram","https://www.instagram.com/accounts/web_create_ajax/attempt/","POST",
   "email={}","email_is_taken","not_taken",1},
  {"Twitter","https://api.twitter.com/i/users/email_available.json","GET",
   "email={}","taken","available",1},
  {"GitHub","https://github.com/signup_check/email","GET",
   "value={}","\"taken\":true","\"taken\":false",1},
  {"Discord","https://discord.com/api/v9/auth/register","POST",
   "{\"email\":\"{}\",\"username\":\"test\",\"password\":\"test123\"}",
   "EMAIL_ALREADY_REGISTERED","errors",1},
  {"Spotify","https://spclient.wg.spotify.com/signup/public/v1/account","POST",
   "{\"account_details\":{\"birthdate\":\"1990-01-01\",\"consent_flags\":{\"eula_agreed\":false,\"send_email\":false,\"third_party_email\":false},\"display_name\":\"test\",\"email_and_password_identifier\":{\"email\":\"{}\",\"password\":\"test123\"},\"gender\":1}}",
   "email-already-registered","success",1},
  {"Adobe","https://auth.services.adobe.com/signin/v2/session/check","POST",
   "{\"username\":\"{}\"}","account_type","unknown_user",1},
  {"Pinterest","https://www.pinterest.com/_ngjs/resource/EmailExistsResource/get/","GET",
   "source_url=/&data={\"options\":{\"email\":\"{}\"},\"context\":{}}",
   "\"email_exists\":true","\"email_exists\":false",1},
  {"Tumblr","https://www.tumblr.com/svc/account/register","POST",
   "{\"email\":\"{}\",\"password\":\"test123\"}","email_in_use","success",1},
  {"Reddit","https://www.reddit.com/api/check_email.json","POST",
   "email={}","EMAIL_TAKEN","success",0},
  {"Snapchat","https://accounts.snapchat.com/accounts/get_username_suggestions","POST",
   "{\"email\":\"{}\",\"requested_username\":\"test\"}","ALREADY_REGISTERED","suggestions",1},
};

#define CONFIDENCE_HIGH   90
#define CONFIDENCE_MEDIUM 70

static void format_email_data(char *dst, size_t cap, const char *tpl,
                              const char *email) {
  const char *pos = strstr(tpl, "{}");
  if (pos) {
    size_t pre = (size_t)(pos - tpl);
    if (pre >= cap) pre = cap - 1;
    memcpy(dst, tpl, pre);
    dst[pre] = '\0';
    strncat(dst, email, cap - strlen(dst) - 1);
    strncat(dst, pos + 2, cap - strlen(dst) - 1);
  } else {
    strncpy(dst, tpl, cap - 1);
    dst[cap - 1] = '\0';
  }
}

/* check_email_account direct detection: on a 200, success_indicator →
 * exists+HIGH, failure_indicator → !exists+HIGH; otherwise inconclusive
 * (exists=false, MEDIUM). Returns whether a response was obtained. */
static int check_email_account(http_client *http, const char *email,
                               const holehe_site_t *s, int *exists,
                               int *confidence, char *info, size_t infocap) {
  *exists = 0;
  *confidence = CONFIDENCE_MEDIUM;
  info[0] = '\0';

  http_response hr = {0};
  int hc;
  if (strcmp(s->method, "GET") == 0) {
    char url[1024];
    format_email_data(url, sizeof url, s->url, email);
    hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 0, &hr);
  } else {
    char body[1024];
    format_email_data(body, sizeof body, s->data_template, email);
    const char *ct = s->use_json
      ? "Content-Type: application/json"
      : "Content-Type: application/x-www-form-urlencoded";
    const char *hdrs[] = { ct, NULL };
    hc = http_request(http, "POST", s->url, hdrs, body, strlen(body),
                      15000, 0, &hr);
  }

  int got = (hc == 0 && hr.body);
  if (got && hr.status == 200) {
    if (s->success_indicator[0] && strstr(hr.body, s->success_indicator)) {
      *exists = 1;
      *confidence = CONFIDENCE_HIGH;
    } else if (s->failure_indicator[0] &&
               strstr(hr.body, s->failure_indicator)) {
      *exists = 0;
      *confidence = CONFIDENCE_HIGH;
    }
    snprintf(info, infocap, "HTTP: %ld", hr.status);
  } else if (got) {
    snprintf(info, infocap, "HTTP: %ld", hr.status);
  }
  http_response_free(&hr);
  return got;
}

static int emit_env(intel_sink *sink, const char *svc, const char *entity,
                    int success, int confidence, cJSON *data /*owned*/,
                    const char *summary) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  if (data) cJSON_AddItemToObject(env, "data", data);
  else cJSON_AddNullToObject(env, "data");
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", svc);
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320];
  snprintf(rk, sizeof rk, "%s:%s", svc, entity);
  char title[360];
  snprintf(title, sizeof title, "%s — %s", svc, entity);
  char tags[96];
  snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", svc);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = summary;
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = tags;
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

/* email_validator_service: holehe over holehe_sites[] → result_builder JSON. */
static int holehe_run(const source_ctx *ctx, intel_sink *sink,
                      const char *svc, const char *email) {
  cJSON *results = cJSON_CreateArray();
  int total = 0, found = 0;
  int ns = (int)(sizeof holehe_sites / sizeof holehe_sites[0]);
  for (int i = 0; i < ns; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    int exists = 0, conf = CONFIDENCE_MEDIUM;
    char info[256];
    if (!check_email_account(ctx->http, email, &holehe_sites[i],
                             &exists, &conf, info, sizeof info))
      continue;
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "source", holehe_sites[i].name);
    cJSON_AddBoolToObject(item, "found", exists);
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "platform", holehe_sites[i].name);
    cJSON_AddBoolToObject(d, "account_exists", exists);
    cJSON_AddStringToObject(d, "info", info);
    cJSON_AddItemToObject(item, "data", d);
    cJSON_AddNumberToObject(item, "confidence", conf);
    cJSON_AddStringToObject(item, "detection_method", "direct");
    cJSON_AddItemToArray(results, item);
    total++;
    if (exists) found++;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", email);
  cJSON_AddStringToObject(root, "query_type", "email");
  cJSON_AddItemToObject(root, "results", results);
  cJSON *summary = cJSON_CreateObject();
  cJSON_AddNumberToObject(summary, "total_sources", total);
  cJSON_AddNumberToObject(summary, "found_count", found);
  cJSON *bd = cJSON_CreateObject();
  cJSON_AddNumberToObject(bd, "direct", total);
  cJSON_AddNumberToObject(bd, "llama", 0);
  cJSON_AddItemToObject(summary, "detection_breakdown", bd);
  cJSON_AddItemToObject(root, "summary", summary);

  /* email_validator_service: success=true, confidence found?90:70. */
  return emit_env(sink, svc, email, 1, found > 0 ? 90 : 70, root,
                  found > 0 ? "accounts found" : "no accounts found");
}

/* handle_social_search: '@' → holehe; else sherlock_search_username
 * (username_osint.c, a different port batch) → defer (no row). */
static int run_social(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return -1;
  if (strchr(e, '@')) return holehe_run(ctx, sink, "SOCIAL_SEARCH", e);
  return 0;
}

static const source_def social_search_def = {
  .id = "SOCIAL_SEARCH", .collector = "osint",
  .name = "Social Search", .name_ja = "ソーシャル検索",
  .update_interval_sec = 0, .run = run_social,
};
REGISTER_SOURCE(social_search_def)
