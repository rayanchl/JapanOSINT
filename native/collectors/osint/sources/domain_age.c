/* collectors/osint/sources/domain_age.c
 * OSINT service — faithful port of OSINTsaas osint_tools/domain_age.c
 * (handle_domain_age → domain_age_lookup). Canonical service DOMAIN_AGE
 * (osint_dispatcher.c service_registry[]: → handle_domain_age →
 * domain_age_lookup). On-demand
 * (interval 0); the pipeline runs it with ctx->entity = a domain. Source:
 * RDAP via rdap.org (no key). Reproduces extract_domain → query_rdap (events:
 * registration/expiration/last-changed, status[], registrar via vcard fn,
 * nameservers ldhName) → age/trust_score/trust_category/red_flags exactly.
 * result->success = (root has "dates"); confidence 85/30. Emits ONE
 * osint_service_result row; body = {success,confidence,data,error?} envelope
 * (data == the OSINTsaas service_result_t.data JSON), like ip_geolocation.c. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

/* domain_age.c parse_date: ISO-8601 (YYYY-MM-DDThh:mm:ss) or YYYY-MM-DD. */
static time_t parse_date(const char *date_str) {
  if (!date_str) return 0;
  struct tm tm = {0};
  if (sscanf(date_str, "%d-%d-%dT%d:%d:%d",
             &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
             &tm.tm_hour, &tm.tm_min, &tm.tm_sec) >= 3) {
    tm.tm_year -= 1900; tm.tm_mon -= 1;
    return mktime(&tm);
  }
  if (sscanf(date_str, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) {
    tm.tm_year -= 1900; tm.tm_mon -= 1;
    return mktime(&tm);
  }
  return 0;
}

static int calculate_age_days(time_t created) {
  if (created == 0) return -1;
  double diff = difftime(time(NULL), created);
  return (int)(diff / 86400);
}

/* domain_age.c calculate_trust_score (verbatim). */
static int calculate_trust_score(int age_days, const char *status) {
  int score = 50;
  if (age_days > 3650) score += 40;
  else if (age_days > 1825) score += 30;
  else if (age_days > 730) score += 20;
  else if (age_days > 365) score += 10;
  else if (age_days > 30) score += 0;
  else score -= 20;
  if (status) {
    if (strstr(status, "clientTransferProhibited")) score += 5;
    if (strstr(status, "serverDeleteProhibited")) score += 5;
    if (strstr(status, "pendingDelete")) score -= 30;
    if (strstr(status, "redemptionPeriod")) score -= 20;
  }
  if (score < 0) score = 0;
  if (score > 100) score = 100;
  return score;
}

/* domain_age.c extract_domain: strip scheme, www., path, port; lowercase. */
static char *extract_domain(const char *input) {
  char *domain = strdup(input);
  if (!domain) return NULL;
  char *ptr = domain;
  if (strncmp(ptr, "http://", 7) == 0) memmove(ptr, ptr + 7, strlen(ptr + 7) + 1);
  else if (strncmp(ptr, "https://", 8) == 0) memmove(ptr, ptr + 8, strlen(ptr + 8) + 1);
  if (strncmp(ptr, "www.", 4) == 0) memmove(ptr, ptr + 4, strlen(ptr + 4) + 1);
  char *slash = strchr(ptr, '/'); if (slash) *slash = '\0';
  char *colon = strchr(ptr, ':'); if (colon) *colon = '\0';
  for (char *c = ptr; *c; c++) *c = (char)tolower((unsigned char)*c);
  return domain;
}

/* domain_age.c query_rdap: GET https://rdap.org/domain/<domain>, extract
 * created/expires/updated, status[], registrar (vcard fn), nameservers. */
static cJSON *query_rdap(http_client *http, const char *domain) {
  char url[512];
  snprintf(url, sizeof url, "https://rdap.org/domain/%s", domain);
  cJSON *json = feed_get_json(http, url, 30000);
  if (!json) return NULL;

  cJSON *result = cJSON_CreateObject();

  cJSON *events = cJSON_GetObjectItem(json, "events");
  if (events && cJSON_IsArray(events)) {
    int n = cJSON_GetArraySize(events);
    for (int i = 0; i < n; i++) {
      cJSON *ev = cJSON_GetArrayItem(events, i);
      cJSON *action = cJSON_GetObjectItem(ev, "eventAction");
      cJSON *date = cJSON_GetObjectItem(ev, "eventDate");
      if (action && date && cJSON_IsString(action) && cJSON_IsString(date)) {
        if (strcmp(action->valuestring, "registration") == 0)
          cJSON_AddStringToObject(result, "created", date->valuestring);
        else if (strcmp(action->valuestring, "expiration") == 0)
          cJSON_AddStringToObject(result, "expires", date->valuestring);
        else if (strcmp(action->valuestring, "last changed") == 0 ||
                 strcmp(action->valuestring, "last update of RDAP database") == 0)
          cJSON_AddStringToObject(result, "updated", date->valuestring);
      }
    }
  }

  cJSON *status = cJSON_GetObjectItem(json, "status");
  if (status && cJSON_IsArray(status)) {
    cJSON *sa = cJSON_CreateArray();
    int n = cJSON_GetArraySize(status);
    for (int i = 0; i < n; i++) {
      cJSON *s = cJSON_GetArrayItem(status, i);
      if (cJSON_IsString(s)) cJSON_AddItemToArray(sa, cJSON_CreateString(s->valuestring));
    }
    cJSON_AddItemToObject(result, "status", sa);
  }

  cJSON *entities = cJSON_GetObjectItem(json, "entities");
  if (entities && cJSON_IsArray(entities)) {
    int en = cJSON_GetArraySize(entities);
    for (int i = 0; i < en; i++) {
      cJSON *entity = cJSON_GetArrayItem(entities, i);
      cJSON *roles = cJSON_GetObjectItem(entity, "roles");
      if (roles && cJSON_IsArray(roles)) {
        int rn = cJSON_GetArraySize(roles);
        for (int j = 0; j < rn; j++) {
          cJSON *role = cJSON_GetArrayItem(roles, j);
          if (cJSON_IsString(role) && strcmp(role->valuestring, "registrar") == 0) {
            cJSON *vcard = cJSON_GetObjectItem(entity, "vcardArray");
            if (vcard && cJSON_IsArray(vcard) && cJSON_GetArraySize(vcard) > 1) {
              cJSON *props = cJSON_GetArrayItem(vcard, 1);
              if (props && cJSON_IsArray(props)) {
                int pn = cJSON_GetArraySize(props);
                for (int k = 0; k < pn; k++) {
                  cJSON *prop = cJSON_GetArrayItem(props, k);
                  if (cJSON_IsArray(prop) && cJSON_GetArraySize(prop) > 0) {
                    cJSON *type = cJSON_GetArrayItem(prop, 0);
                    if (cJSON_IsString(type) && strcmp(type->valuestring, "fn") == 0) {
                      cJSON *value = cJSON_GetArrayItem(prop, 3);
                      if (cJSON_IsString(value))
                        cJSON_AddStringToObject(result, "registrar", value->valuestring);
                    }
                  }
                }
              }
            }
            cJSON *handle = cJSON_GetObjectItem(entity, "handle");
            if (handle && cJSON_IsString(handle))
              cJSON_AddStringToObject(result, "registrar_id", handle->valuestring);
            break;
          }
        }
      }
    }
  }

  cJSON *nameservers = cJSON_GetObjectItem(json, "nameservers");
  if (nameservers && cJSON_IsArray(nameservers)) {
    cJSON *nsa = cJSON_CreateArray();
    int n = cJSON_GetArraySize(nameservers);
    for (int i = 0; i < n; i++) {
      cJSON *ns = cJSON_GetArrayItem(nameservers, i);
      cJSON *name = cJSON_GetObjectItem(ns, "ldhName");
      if (name && cJSON_IsString(name))
        cJSON_AddItemToArray(nsa, cJSON_CreateString(name->valuestring));
    }
    cJSON_AddItemToObject(result, "nameservers", nsa);
  }

  cJSON_Delete(json);
  return result;
}

/* domain_age.c domain_age_lookup body → builds the result->data JSON object. */
static cJSON *domain_age_build(http_client *http, const char *domain,
                               int *out_success, int *out_conf) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "domain", domain);
  cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));

  cJSON *rdap = query_rdap(http, domain);
  if (rdap) {
    cJSON_AddStringToObject(root, "source", "RDAP");
    cJSON *created = cJSON_GetObjectItem(rdap, "created");
    cJSON *expires = cJSON_GetObjectItem(rdap, "expires");
    cJSON *updated = cJSON_GetObjectItem(rdap, "updated");

    cJSON *dates = cJSON_CreateObject();
    time_t created_time = 0;
    if (created && cJSON_IsString(created)) {
      cJSON_AddStringToObject(dates, "created", created->valuestring);
      created_time = parse_date(created->valuestring);
    }
    if (expires && cJSON_IsString(expires)) {
      cJSON_AddStringToObject(dates, "expires", expires->valuestring);
      time_t expires_time = parse_date(expires->valuestring);
      int days_to_expiry = (int)(difftime(expires_time, time(NULL)) / 86400);
      if (days_to_expiry < 30 && days_to_expiry > 0)
        cJSON_AddStringToObject(root, "expiration_warning",
                                "Domain expires in less than 30 days");
      else if (days_to_expiry <= 0)
        cJSON_AddStringToObject(root, "expiration_warning", "Domain may have expired");
      cJSON_AddNumberToObject(dates, "days_to_expiry", days_to_expiry);
    }
    if (updated && cJSON_IsString(updated))
      cJSON_AddStringToObject(dates, "last_updated", updated->valuestring);
    cJSON_AddItemToObject(root, "dates", dates);

    if (created_time > 0) {
      int age_days = calculate_age_days(created_time);
      cJSON *age = cJSON_CreateObject();
      cJSON_AddNumberToObject(age, "days", age_days);
      cJSON_AddNumberToObject(age, "years", age_days / 365);
      cJSON_AddNumberToObject(age, "months", age_days / 30);
      char age_str[64];
      if (age_days > 365)
        snprintf(age_str, sizeof age_str, "%d years, %d months",
                 age_days / 365, (age_days % 365) / 30);
      else if (age_days > 30)
        snprintf(age_str, sizeof age_str, "%d months, %d days",
                 age_days / 30, age_days % 30);
      else
        snprintf(age_str, sizeof age_str, "%d days", age_days);
      cJSON_AddStringToObject(age, "human_readable", age_str);
      cJSON_AddItemToObject(root, "age", age);
    }

    cJSON *registrar = cJSON_GetObjectItem(rdap, "registrar");
    if (registrar && cJSON_IsString(registrar))
      cJSON_AddStringToObject(root, "registrar", registrar->valuestring);

    cJSON *status = cJSON_GetObjectItem(rdap, "status");
    if (status) {
      cJSON_AddItemToObject(root, "status", cJSON_Duplicate(status, 1));
      char status_str[512] = "";
      int sc = cJSON_GetArraySize(status);
      for (int i = 0; i < sc; i++) {
        cJSON *s = cJSON_GetArrayItem(status, i);
        if (cJSON_IsString(s)) {
          if (strlen(status_str) > 0) strncat(status_str, " ",
              sizeof status_str - strlen(status_str) - 1);
          strncat(status_str, s->valuestring,
                  sizeof status_str - strlen(status_str) - 1);
        }
      }
      int age_days = created_time > 0 ? calculate_age_days(created_time) : -1;
      int trust = calculate_trust_score(age_days, status_str);
      cJSON_AddNumberToObject(root, "trust_score", trust);
      if (trust >= 80) cJSON_AddStringToObject(root, "trust_category", "highly_trusted");
      else if (trust >= 60) cJSON_AddStringToObject(root, "trust_category", "trusted");
      else if (trust >= 40) cJSON_AddStringToObject(root, "trust_category", "neutral");
      else if (trust >= 20) cJSON_AddStringToObject(root, "trust_category", "suspicious");
      else cJSON_AddStringToObject(root, "trust_category", "untrusted");
    }

    cJSON *nameservers = cJSON_GetObjectItem(rdap, "nameservers");
    if (nameservers)
      cJSON_AddItemToObject(root, "nameservers", cJSON_Duplicate(nameservers, 1));

    cJSON_Delete(rdap);
  } else {
    cJSON_AddStringToObject(root, "error", "Failed to retrieve RDAP data");
  }

  cJSON *analysis = cJSON_CreateObject();
  cJSON *red_flags = cJSON_CreateArray();
  cJSON *age = cJSON_GetObjectItem(root, "age");
  if (age) {
    cJSON *days = cJSON_GetObjectItem(age, "days");
    if (days && cJSON_IsNumber(days) && days->valueint < 30)
      cJSON_AddItemToArray(red_flags,
        cJSON_CreateString("Domain is less than 30 days old - newly registered"));
  }
  cJSON *trust = cJSON_GetObjectItem(root, "trust_score");
  if (trust && cJSON_IsNumber(trust) && trust->valueint < 40)
    cJSON_AddItemToArray(red_flags,
      cJSON_CreateString("Low trust score - exercise caution"));
  if (cJSON_GetArraySize(red_flags) > 0)
    cJSON_AddItemToObject(analysis, "red_flags", red_flags);
  else cJSON_Delete(red_flags);
  cJSON_AddItemToObject(root, "analysis", analysis);

  int success = (cJSON_GetObjectItem(root, "dates") != NULL);
  *out_success = success;
  *out_conf = success ? 85 : 30;
  return root;
}

static int emit_one(intel_sink *sink, const char *svc, const char *entity,
                    int success, int confidence, cJSON *data /*owned*/,
                    const char *error) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  if (data) cJSON_AddItemToObject(env, "data", data);
  else cJSON_AddNullToObject(env, "data");
  if (error) cJSON_AddStringToObject(env, "error", error);
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
  it.summary = success ? "domain age resolved" : (error ? error : "lookup failed");
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = tags;
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run_svc(const source_ctx *ctx, intel_sink *sink, const char *svc) {
  const char *input = ctx->entity;
  if (!input || !*input) return -1;
  char *domain = extract_domain(input);
  if (!domain) return emit_one(sink, svc, input, 0, 0, NULL, "encode failed");

  int success = 0, conf = 0;
  cJSON *data = domain_age_build(ctx->http, domain, &success, &conf);
  int rc = emit_one(sink, svc, domain, success, conf, data, NULL);
  free(domain);
  return rc;
}

static int run_age(const source_ctx *ctx, intel_sink *sink) {
  return run_svc(ctx, sink, "DOMAIN_AGE");
}

static const source_def domain_age_def = {
  .id = "DOMAIN_AGE", .collector = "osint",
  .name = "Domain Age", .name_ja = "ドメイン経過年数",
  .update_interval_sec = 0, .run = run_age,
};
REGISTER_SOURCE(domain_age_def)
