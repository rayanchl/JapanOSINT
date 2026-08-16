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
 * branch sets it. PER-RECORD EMIT: emits ONE osint_service_result row per
 * platform where the account was confirmed to exist (remote_key=
 * "account:<platform>:<email>", body={platform,email,account_exists,confidence,
 * http_status}). Platforms where the account does not exist (or detection was
 * inconclusive) are not emitted; none confirmed → emits nothing (return 0). */
#include "../../source.h"
#include "social_fuse.h"
#include "../../lib/jocore.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
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
  /* Indicators must match a VALUE, not a key: "taken" alone is present in every
   * response this endpoint gives, including its "empty email" refusal, so it
   * asserted an account for any input. The live contract is
   * {"valid":…,"msg":…,"taken":true|false}. */
  {"Twitter","https://api.twitter.com/i/users/email_available.json","GET",
   "email={}","\"taken\":true","\"taken\":false",1},
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
    /* The GET rows carry their parameter in data_template ("email={}"), NOT in
     * url — url holds no "{}" at all. This used to format s->url, so
     * format_email_data fell through to its verbatim-copy branch and the
     * address was never transmitted: every GET row queried the bare endpoint
     * and then matched an indicator against a response about nothing. Twitter
     * answered 200 with {"valid":false,"msg":"Vous ne pouvez pas avoir une
     * adresse email vide.","taken":false} and the substring test below found
     * "taken" — the KEY — so every email on earth was reported as having a
     * confirmed Twitter account at confidence 90. Build the query string from
     * data_template, and %-encode the address so a '+' or '&' in it cannot
     * reshape the request. */
    char url[1024], enc[512], qs[768];
    jo_urlencode_buf(email, enc, sizeof enc);
    format_email_data(qs, sizeof qs, s->data_template, enc);
    snprintf(url, sizeof url, "%s%s%s", s->url,
             strchr(s->url, '?') ? "&" : "?", qs);
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

/* Emit ONE intel row for a single confirmed account (platform where the email
 * is registered). body = {platform,email,account_exists,confidence,http_status}. */
static int emit_account(intel_sink *sink, const char *svc, const char *platform,
                        const char *email, int confidence, long http_status) {
  cJSON *body = cJSON_CreateObject();
  cJSON_AddStringToObject(body, "platform", platform);
  cJSON_AddStringToObject(body, "email", email);
  cJSON_AddBoolToObject(body, "account_exists", 1);
  cJSON_AddNumberToObject(body, "confidence", confidence);
  cJSON_AddNumberToObject(body, "http_status", http_status);
  char *bj = cJSON_PrintUnformatted(body);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", svc);
  cJSON_AddStringToObject(props, "entity", email);
  cJSON_AddStringToObject(props, "platform", platform);
  cJSON_AddBoolToObject(props, "account_exists", 1);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  cJSON_AddStringToObject(props, "detection_method", "direct");
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320];
  snprintf(rk, sizeof rk, "account:%s:%s", platform, email);
  char title[360];
  snprintf(title, sizeof title, "%s: account exists", platform);
  char summary[200];
  snprintf(summary, sizeof summary, "%s has an account for %s", platform, email);
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
  cJSON_Delete(body); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* email_validator_service: holehe over holehe_sites[] → ONE row per platform
 * where the account was confirmed to exist. None confirmed → emit nothing. */
static int holehe_run(const source_ctx *ctx, intel_sink *sink,
                      const char *svc, const char *email) {
  int ns = (int)(sizeof holehe_sites / sizeof holehe_sites[0]);
  for (int i = 0; i < ns; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    int exists = 0, conf = CONFIDENCE_MEDIUM;
    char info[256];
    if (!check_email_account(ctx->http, email, &holehe_sites[i],
                             &exists, &conf, info, sizeof info))
      continue;
    if (!exists) continue;   /* only emit confirmed-existing accounts */
    long http_status = 0;
    sscanf(info, "HTTP: %ld", &http_status);
    emit_account(sink, svc, holehe_sites[i].name, email, conf, http_status);
  }
  return 0;   /* honest empty is not an error */
}

/* SOCIAL_EMAIL — email pivot fused over every email source: Holehe account
 * existence + EmailRep reputation + MX/syntax validation + HIBP breach. For
 * source parity with SOCIAL_USERNAME it ALSO runs the full username stack on
 * the email's local-part, so an email surfaces the same site coverage a
 * username would. A bare username (no '@') just runs the username stack. */
static int run_email(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return -1;
  const char *at = strchr(e, '@');
  if (!at) {
    int u = 0;
    u += jo_social_intel_run(ctx, sink);
    u += jo_social_platforms_run(ctx, sink);
    u += jo_sherlock_run(ctx, sink);
    u += jo_maigret_run(ctx, sink);
    (void)u;
    return 0;
  }
  int t = 0;
  t += holehe_run(ctx, sink, "SOCIAL_EMAIL", e);
  t += jo_email_reputation_run(ctx, sink);
  t += jo_email_validator_run(ctx, sink);
  t += jo_breach_checker_run(ctx, sink);
  /* parity: username presence on the email's local-part */
  char user[160]; size_t n = (size_t)(at - e);
  if (n >= sizeof user) n = sizeof user - 1;
  memcpy(user, e, n); user[n] = 0;
  if (*user) {
    source_ctx c2 = *ctx; c2.entity = user;
    t += jo_social_intel_run(&c2, sink);
    t += jo_social_platforms_run(&c2, sink);
    t += jo_sherlock_run(&c2, sink);
    t += jo_maigret_run(&c2, sink);
  }
  /* run() is a STATUS code, not a row count: core/scheduler.c does
   * `status = rc == 0 ? "ok" : "error"` and feeds it to anomaly_detect(), so
   * returning the emitted count marked every successful lookup as an errored
   * run and quarantined the source precisely because it worked. */
  (void)t;
  return 0;
}

static const source_def social_email_def = {
  .id = "SOCIAL_EMAIL", .collector = "osint",
  .name = "Email OSINT", .name_ja = "メールOSINT",
  .update_interval_sec = 0, .run = run_email,
  .category = "social", .type = "api",
  .url = "internal://osint/social-email",
  .description = "Email pivot: Holehe account existence, EmailRep reputation, MX/syntax validation, HIBP breach exposure, plus username-presence enumeration on the local-part.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(social_email_def)
