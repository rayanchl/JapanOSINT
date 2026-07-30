/* collectors/osint/sources/email_validator.c
 * OSINT service — EMAIL_VALIDATOR_NEW (osint_dispatcher.c service_registry[]).
 * On-demand (interval 0); ctx->entity = an email. Sources: DNS-over-HTTPS MX
 * (dns.google, no key) + raw-socket SMTP RCPT verification (ports 25/587) +
 * RDAP domain age (rdap.verisign.com, no key) + Hunter.io email-verifier
 * (only when HUNTER_API_KEY is present; absent → that enrichment is simply
 * skipped, no fabricated row). The disposable/free/role lists are CLASSIFIER
 * inputs only (feed the flags / status / score), never emitted as fake data.
 *
 * PER-RECORD EMIT: emits ONE intel row keyed "emailval:<email>"; body = the
 * real computed validation fields (format, MX records, domain age, SMTP
 * status, status/score, risk_factors, hunter_io when keyed). No
 * {success,confidence,data} envelope. Empty input or a parse that yields no
 * domain/local part → emit nothing (return 0). */
#include "../../source.h"
#include "social_fuse.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <regex.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static const char *DISPOSABLE_DOMAINS[] = {
  "tempmail.com","throwaway.email","guerrillamail.com","10minutemail.com",
  "mailinator.com","maildrop.cc","temp-mail.org","fakeinbox.com",
  "dispostable.com","getnada.com","mohmal.com","tempail.com",
  "emailondeck.com","sharklasers.com","spam4.me","trashmail.com",
  "yopmail.com","mailnesia.com","tempr.email","discard.email", NULL
};
static const char *FREE_PROVIDERS[] = {
  "gmail.com","yahoo.com","hotmail.com","outlook.com","aol.com",
  "icloud.com","mail.com","protonmail.com","zoho.com","gmx.com",
  "yandex.com","live.com","msn.com","me.com","fastmail.com", NULL
};
static const char *ROLE_PREFIXES[] = {
  "admin","administrator","info","contact","support","sales",
  "marketing","hr","help","noreply","no-reply","webmaster",
  "postmaster","abuse","hostmaster","billing","team","hello", NULL
};

static int validate_email_format(const char *email) {
  if (!email || strlen(email) < 5 || strlen(email) > 254) return 0;
  regex_t re;
  if (regcomp(&re, "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$",
              REG_EXTENDED) != 0) return 0;
  int r = regexec(&re, email, 0, NULL, 0);
  regfree(&re);
  return r == 0;
}

static int lc_in_list(const char *s, const char *const *list) {
  char low[256];
  strncpy(low, s, sizeof low - 1); low[sizeof low - 1] = 0;
  for (int i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
  for (int i = 0; list[i]; i++) if (strcmp(low, list[i]) == 0) return 1;
  return 0;
}

/* get_mx_records: DNS-over-HTTPS https://dns.google/resolve?name=&type=MX. */
static cJSON *get_mx_records(http_client *http, const char *domain) {
  cJSON *mx = cJSON_CreateArray();
  char url[512];
  snprintf(url, sizeof url,
    "https://dns.google/resolve?name=%s&type=MX", domain);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return mx; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) return mx;
  cJSON *ans = cJSON_GetObjectItem(json, "Answer");
  if (ans && cJSON_IsArray(ans)) {
    int c = cJSON_GetArraySize(ans);
    for (int i = 0; i < c; i++) {
      cJSON *rec = cJSON_GetArrayItem(ans, i);
      cJSON *d = cJSON_GetObjectItem(rec, "data");
      if (d && cJSON_IsString(d)) {
        char *md = strdup(d->valuestring);
        char *sp = strchr(md, ' ');
        if (sp) {
          *sp = '\0';
          int prio = atoi(md);
          char *host = sp + 1;
          size_t hl = strlen(host);
          if (hl > 0 && host[hl - 1] == '.') host[hl - 1] = '\0';
          cJSON *m = cJSON_CreateObject();
          cJSON_AddNumberToObject(m, "priority", prio);
          cJSON_AddStringToObject(m, "hostname", host);
          cJSON_AddItemToArray(mx, m);
        }
        free(md);
      }
    }
  }
  cJSON_Delete(json);
  return mx;
}

/* verify_smtp: raw socket EHLO/MAIL FROM/RCPT TO on 25 then 587.
 * 1=valid, 0=invalid, -1=unable. */
static int verify_smtp(const char *email, const char *mx_host) {
  struct hostent *he = gethostbyname(mx_host);
  if (!he) return -1;
  struct timeval to = { 10, 0 };

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return -1;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof to);
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof to);
  struct sockaddr_in srv;
  memset(&srv, 0, sizeof srv);
  srv.sin_family = AF_INET;
  srv.sin_port = htons(25);
  memcpy(&srv.sin_addr, he->h_addr_list[0], he->h_length);
  if (connect(sock, (struct sockaddr *)&srv, sizeof srv) < 0) {
    close(sock);
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof to);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof to);
    srv.sin_port = htons(587);
    if (connect(sock, (struct sockaddr *)&srv, sizeof srv) < 0) {
      close(sock);
      return -1;
    }
  }
  char buf[1024];
  int result = -1;
  if (recv(sock, buf, sizeof buf - 1, 0) <= 0) { close(sock); return -1; }
  snprintf(buf, sizeof buf, "EHLO osint-validator.local\r\n");
  send(sock, buf, strlen(buf), 0);
  if (recv(sock, buf, sizeof buf - 1, 0) <= 0) { close(sock); return -1; }
  snprintf(buf, sizeof buf, "MAIL FROM:<validator@osint-validator.local>\r\n");
  send(sock, buf, strlen(buf), 0);
  if (recv(sock, buf, sizeof buf - 1, 0) <= 0) { close(sock); return -1; }
  snprintf(buf, sizeof buf, "RCPT TO:<%s>\r\n", email);
  send(sock, buf, strlen(buf), 0);
  int n = recv(sock, buf, sizeof buf - 1, 0);
  if (n > 0) {
    buf[n] = '\0';
    if (buf[0] == '2') result = 1;
    else if (buf[0] == '5') result = 0;
  }
  snprintf(buf, sizeof buf, "QUIT\r\n");
  send(sock, buf, strlen(buf), 0);
  close(sock);
  return result;
}

/* query_hunter_io: key-gated on HUNTER_API_KEY → NULL without it. */
static cJSON *query_hunter_io(http_client *http, const char *email) {
  const char *api_key = getenv("HUNTER_API_KEY");
  if (!api_key || !*api_key) return NULL;
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.hunter.io/v2/email-verifier?email=%s&api_key=%s",
    email, api_key);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) return NULL;
  cJSON *data = cJSON_GetObjectItem(json, "data");
  cJSON *out = data ? cJSON_Duplicate(data, 1) : NULL;
  cJSON_Delete(json);
  return out;
}

/* get_domain_age_days: RDAP rdap.verisign.com/com/v1/domain/<d>. */
static int get_domain_age_days(http_client *http, const char *domain) {
  char url[512];
  snprintf(url, sizeof url,
    "https://rdap.verisign.com/com/v1/domain/%s", domain);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return -1; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) return -1;
  int days = -1;
  cJSON *events = cJSON_GetObjectItem(json, "events");
  if (events && cJSON_IsArray(events)) {
    int c = cJSON_GetArraySize(events);
    for (int i = 0; i < c; i++) {
      cJSON *ev = cJSON_GetArrayItem(events, i);
      cJSON *act = cJSON_GetObjectItem(ev, "eventAction");
      if (act && cJSON_IsString(act) &&
          strcmp(act->valuestring, "registration") == 0) {
        cJSON *date = cJSON_GetObjectItem(ev, "eventDate");
        if (date && cJSON_IsString(date)) {
          int y, m, d;
          if (sscanf(date->valuestring, "%d-%d-%d", &y, &m, &d) == 3) {
            struct tm rt = {0};
            rt.tm_year = y - 1900; rt.tm_mon = m - 1; rt.tm_mday = d;
            time_t rtime = mktime(&rt);
            days = (int)difftime(time(NULL), rtime) / 86400;
          }
        }
        break;
      }
    }
  }
  cJSON_Delete(json);
  return days;
}

static int emit_one(intel_sink *sink, const char *entity,
                    cJSON *data /*owned*/, const char *status) {
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "EMAIL_VALIDATOR_NEW");
  cJSON_AddStringToObject(props, "entity", entity);
  if (status) cJSON_AddStringToObject(props, "status", status);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "emailval:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "%s — validation", entity);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = status ? status : "email validation";
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"EMAIL_VALIDATOR_NEW\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

int jo_email_validator_run(const source_ctx *ctx, intel_sink *sink) {
  const char *email = ctx->entity;
  if (!email || !*email) return 0;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "email", email);

  int format_valid = validate_email_format(email);
  cJSON_AddBoolToObject(root, "format_valid", format_valid);
  if (!format_valid) {
    cJSON_AddStringToObject(root, "status", "invalid");
    cJSON_AddStringToObject(root, "reason", "Invalid email format");
    return emit_one(sink, email, root, "invalid") > 0 ? 0 : 0;
  }

  const char *at = strchr(email, '@');
  char dom[256] = {0}, local[256] = {0};
  if (at) {
    size_t ll = (size_t)(at - email);
    if (ll > sizeof local - 1) ll = sizeof local - 1;
    memcpy(local, email, ll);
    strncpy(dom, at + 1, sizeof dom - 1);
  }
  if (!dom[0] || !local[0]) {
    /* Could not extract domain/local part — genuine failure, emit nothing. */
    cJSON_Delete(root);
    return 0;
  }
  cJSON_AddStringToObject(root, "domain", dom);
  cJSON_AddStringToObject(root, "local_part", local);

  int disposable = lc_in_list(dom, DISPOSABLE_DOMAINS);
  int free_provider = lc_in_list(dom, FREE_PROVIDERS);
  int role_based = lc_in_list(local, ROLE_PREFIXES);
  cJSON_AddBoolToObject(root, "is_disposable", disposable);
  cJSON_AddBoolToObject(root, "is_free_provider", free_provider);
  cJSON_AddBoolToObject(root, "is_role_based", role_based);

  cJSON *mx = get_mx_records(ctx->http, dom);
  int mx_count = cJSON_GetArraySize(mx);
  cJSON_AddItemToObject(root, "mx_records", mx);
  cJSON_AddNumberToObject(root, "mx_count", mx_count);
  cJSON_AddBoolToObject(root, "has_mx", mx_count > 0);

  int domain_age = get_domain_age_days(ctx->http, dom);
  if (domain_age >= 0) cJSON_AddNumberToObject(root, "domain_age_days", domain_age);

  const char *smtp_status = "not_checked";
  int deliverable = 0;
  if (mx_count > 0) {
    cJSON *first = cJSON_GetArrayItem(mx, 0);  /* exhaustive-ok: SMTP probe target; every MX is in the emitted record */
    cJSON *hn = cJSON_GetObjectItem(first, "hostname");
    if (hn && cJSON_IsString(hn)) {
      int sr = verify_smtp(email, hn->valuestring);
      if (sr == 1) { smtp_status = "deliverable"; deliverable = 1; }
      else if (sr == 0) smtp_status = "undeliverable";
      else smtp_status = "unknown";
    }
  } else {
    smtp_status = "no_mx_records";
  }
  cJSON_AddStringToObject(root, "smtp_status", smtp_status);
  cJSON_AddBoolToObject(root, "deliverable", deliverable);

  cJSON *hunter = query_hunter_io(ctx->http, email);
  if (hunter) cJSON_AddItemToObject(root, "hunter_io", hunter);

  const char *status;
  int score = 100;
  if (!format_valid) { status = "invalid"; score = 0; }
  else if (disposable) { status = "risky"; score = 20; }
  else if (mx_count == 0) { status = "invalid"; score = 10; }
  else if (!deliverable && strcmp(smtp_status, "undeliverable") == 0) {
    status = "invalid"; score = 15;
  } else if (deliverable) {
    status = "valid"; score = 95;
    if (domain_age >= 0 && domain_age < 30) score -= 10;
    if (role_based) score -= 5;
  } else { status = "unknown"; score = 50; }
  cJSON_AddStringToObject(root, "status", status);
  cJSON_AddNumberToObject(root, "score", score);

  cJSON *risks = cJSON_CreateArray();
  if (disposable) cJSON_AddItemToArray(risks, cJSON_CreateString("Disposable email domain"));
  if (role_based) cJSON_AddItemToArray(risks, cJSON_CreateString("Role-based email address"));
  if (domain_age >= 0 && domain_age < 30)
    cJSON_AddItemToArray(risks, cJSON_CreateString("Recently registered domain"));
  if (mx_count == 0) cJSON_AddItemToArray(risks, cJSON_CreateString("No MX records"));
  cJSON_AddItemToObject(root, "risk_factors", risks);

  return emit_one(sink, email, root, status) > 0 ? 0 : 0;
}

/* Fused into SOCIAL_EMAIL — exposed via social_fuse.h as jo_email_validator_run. */
