/* collectors/osint/sources/email_reputation.c
 * OSINT service — faithful port of OSINTsaas osint_tools/email_reputation.c
 * (handle_email_reputation → email_reputation_check → analyze_email_reputation).
 * Canonical service EMAIL_REPUTATION (osint_dispatcher.c service_registry[]).
 * On-demand (interval 0); ctx->entity = an email. Sources: EmailRep.io
 * (https://emailrep.io/<email>, no key) + RFC5322-ish regex + disposable list
 * + getaddrinfo MX-ish resolve. Reproduces analyze_email_reputation exactly:
 * format/disposable/mx/emailrep reputation→score map, fallback scoring,
 * data fields, confidence 90 (api ok) / 70. Emits ONE osint_service_result
 * row; body = {success,confidence,data,error?} envelope, like ip_geolocation.c.
 * Note: OSINTsaas always sets success=true here. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <netdb.h>
#include <sys/socket.h>

static const char *DISPOSABLE_DOMAINS[] = {
  "tempmail.com", "guerrillamail.com", "10minutemail.com", "mailinator.com",
  "throwaway.email", "temp-mail.org", "getnada.com", "maildrop.cc",
  "yopmail.com", "fakeinbox.com", "trashmail.com", "dispostable.com",
  "mintemail.com", "getairmail.com", "spamgourmet.com", NULL
};

static int validate_email_format(const char *email) {
  regex_t re;
  const char *pat = "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$";
  if (regcomp(&re, pat, REG_EXTENDED | REG_ICASE) != 0) return 0;
  int r = regexec(&re, email, 0, NULL, 0);
  regfree(&re);
  return r == 0;
}

static int is_disposable_email(const char *domain) {
  for (int i = 0; DISPOSABLE_DOMAINS[i]; i++)
    if (strcasecmp(domain, DISPOSABLE_DOMAINS[i]) == 0) return 1;
  return 0;
}

/* email_reputation.c check_mx_records: getaddrinfo(domain) == 0. */
static int check_mx_records(const char *domain) {
  struct addrinfo hints, *res = NULL;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  int rc = getaddrinfo(domain, NULL, &hints, &res);
  if (res) freeaddrinfo(res);
  return rc == 0;
}

static int emit_one(intel_sink *sink, const char *entity, int success,
                    int confidence, cJSON *data /*owned*/, const char *error) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  if (data) cJSON_AddItemToObject(env, "data", data);
  else cJSON_AddNullToObject(env, "data");
  if (error) cJSON_AddStringToObject(env, "error", error);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "EMAIL_REPUTATION");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "emailrep:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "EMAIL_REPUTATION — %s", entity);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = success ? "email reputation checked"
                       : (error ? error : "lookup failed");
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"EMAIL_REPUTATION\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *email = ctx->entity;
  if (!email || !*email) return -1;

  char dom[256] = {0}, user[256] = {0};
  const char *at = strchr(email, '@');
  if (at) {
    size_t ul = (size_t)(at - email);
    if (ul > sizeof user - 1) ul = sizeof user - 1;
    memcpy(user, email, ul);
    strncpy(dom, at + 1, sizeof dom - 1);
  }

  int valid_format = validate_email_format(email);
  int disposable = (dom[0]) ? is_disposable_email(dom) : 0;
  int mx_exists = (dom[0]) ? check_mx_records(dom) : 0;
  int deliverable = 0, suspicious = 0;
  int reputation_score = 0;
  char reputation_status[32] = {0};
  cJSON *details = NULL;
  int api_success = 0;

  /* query_emailrep: GET https://emailrep.io/<email> ; 200 → parse. */
  char url[512];
  snprintf(url, sizeof url, "https://emailrep.io/%s", email);
  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc == 0 && hr.status == 200 && hr.body) {
    cJSON *j = cJSON_Parse(hr.body);
    if (j) {
      cJSON *rep = cJSON_GetObjectItem(j, "reputation");
      cJSON *susp = cJSON_GetObjectItem(j, "suspicious");
      cJSON *deliv = cJSON_GetObjectItem(j, "deliverable");
      cJSON *disp = cJSON_GetObjectItem(j, "disposable");
      cJSON *det = cJSON_GetObjectItem(j, "details");
      if (rep && cJSON_IsString(rep)) {
        strncpy(reputation_status, rep->valuestring, sizeof reputation_status - 1);
        if (strcmp(rep->valuestring, "high") == 0) reputation_score = 90;
        else if (strcmp(rep->valuestring, "medium") == 0) reputation_score = 60;
        else if (strcmp(rep->valuestring, "low") == 0) reputation_score = 30;
        else reputation_score = 10;
      }
      if (susp && cJSON_IsBool(susp)) suspicious = cJSON_IsTrue(susp);
      if (deliv && cJSON_IsBool(deliv)) deliverable = cJSON_IsTrue(deliv);
      if (disp && cJSON_IsBool(disp)) disposable = cJSON_IsTrue(disp);
      if (det) details = cJSON_Duplicate(det, 1);
      api_success = 1;
      cJSON_Delete(j);
    }
  }
  http_response_free(&hr);

  /* Fallback scoring when API gave no reputation. */
  if (reputation_score == 0) {
    reputation_score = 50;
    if (!valid_format) reputation_score -= 40;
    if (disposable) reputation_score -= 30;
    if (!mx_exists) reputation_score -= 20;
    if (suspicious) reputation_score -= 25;
    if (reputation_score < 0) reputation_score = 0;
    if (reputation_score >= 70) strcpy(reputation_status, "high");
    else if (reputation_score >= 40) strcpy(reputation_status, "medium");
    else strcpy(reputation_status, "low");
  }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "email", email);
  cJSON_AddStringToObject(data, "username", user);
  cJSON_AddStringToObject(data, "domain", dom);
  cJSON_AddBoolToObject(data, "valid_format", valid_format);
  cJSON_AddBoolToObject(data, "disposable", disposable);
  cJSON_AddBoolToObject(data, "mx_exists", mx_exists);
  cJSON_AddBoolToObject(data, "deliverable", deliverable);
  cJSON_AddNumberToObject(data, "reputation_score", reputation_score);
  cJSON_AddStringToObject(data, "reputation_status", reputation_status);
  cJSON_AddBoolToObject(data, "suspicious", suspicious);
  cJSON_AddBoolToObject(data, "api_lookup", api_success);
  if (details) cJSON_AddItemToObject(data, "details", details);

  /* OSINTsaas: result->success = true; confidence 90 if api else 70. */
  return emit_one(sink, email, 1, api_success ? 90 : 70, data, NULL);
}

static const source_def email_reputation_def = {
  .id = "EMAIL_REPUTATION", .collector = "osint",
  .name = "Email Reputation", .name_ja = "メール評価",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(email_reputation_def)
