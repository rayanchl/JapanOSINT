/* collectors/osint/sources/email_reputation.c
 * OSINT service — EMAIL_REPUTATION (osint_dispatcher.c service_registry[]).
 * On-demand (interval 0); ctx->entity = an email. Sources: EmailRep.io
 * (https://emailrep.io/<email>, no key) + RFC5322-ish regex + getaddrinfo
 * MX-ish resolve. The disposable-domain list is a CLASSIFIER input only
 * (feeds the disposable flag / fallback score), never emitted as fabricated
 * data.
 *
 * PER-RECORD EMIT: emits ONE intel row keyed "emailrep:<email>"; body = the
 * real fetched/computed reputation fields (EmailRep.io reputation/suspicious/
 * deliverable/details + format check + MX-exists + score). No
 * {success,confidence,data} envelope. If the input is empty, emit nothing
 * (return 0); EmailRep failure simply omits those fields (format/MX still
 * computed locally). */
#include "source.h"
#include "social_fuse.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
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

static int emit_one(intel_sink *sink, const char *entity,
                    cJSON *data /*owned*/, const char *status) {
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "EMAIL_REPUTATION");
  cJSON_AddStringToObject(props, "entity", entity);
  if (status) cJSON_AddStringToObject(props, "reputation_status", status);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "emailrep:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "%s — reputation", entity);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = status ? status : "email reputation";
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"EMAIL_REPUTATION\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

int jo_email_reputation_run(const source_ctx *ctx, intel_sink *sink) {
  const char *email = ctx->entity;
  if (!email || !*email) return 0;

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
  int have_deliverable = 0;                /* upstream actually said so */
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
      if (deliv && cJSON_IsBool(deliv)) { deliverable = cJSON_IsTrue(deliv);
                                          have_deliverable = 1; }
      if (disp && cJSON_IsBool(disp)) disposable = cJSON_IsTrue(disp);
      if (det) details = cJSON_Duplicate(det, 1);
      api_success = 1;
      cJSON_Delete(j);
    }
  }
  http_response_free(&hr);

  /* There is no fallback score any more. This used to start from a bare 50 —
   * a number nothing measured — subtract penalties from it, and publish the
   * result as `reputation_score` alongside a "high"/"medium"/"low" status,
   * indistinguishable in the emitted row from a real emailrep.io reputation.
   * When the API gave us nothing, the honest answer is that the reputation is
   * unknown; the locally computed facts (format, MX, disposable) still ship. */
  int have_score = (reputation_score != 0);
  if (!have_score) snprintf(reputation_status, sizeof reputation_status,
                            "unknown");

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "email", email);
  cJSON_AddStringToObject(data, "username", user);
  cJSON_AddStringToObject(data, "domain", dom);
  cJSON_AddBoolToObject(data, "valid_format", valid_format);
  cJSON_AddBoolToObject(data, "disposable", disposable);
  cJSON_AddBoolToObject(data, "mx_exists", mx_exists);
  /* deliverable / reputation_score are upstream facts or nothing — a false and
   * a 0 read as measurements, and "not deliverable" is a damaging claim to
   * invent about an address. */
  cJSON_AddItemToObject(data, "deliverable",
    have_deliverable ? cJSON_CreateBool(deliverable) : cJSON_CreateNull());
  cJSON_AddItemToObject(data, "reputation_score",
    have_score ? cJSON_CreateNumber(reputation_score) : cJSON_CreateNull());
  cJSON_AddStringToObject(data, "reputation_status", reputation_status);
  cJSON_AddBoolToObject(data, "suspicious", suspicious);
  cJSON_AddBoolToObject(data, "api_lookup", api_success);
  if (details) cJSON_AddItemToObject(data, "details", details);

  return emit_one(sink, email, data, reputation_status) > 0 ? 0 : 0;
}

/* Fused into SOCIAL_EMAIL — exposed via social_fuse.h as jo_email_reputation_run. */
