/* collectors/osint/sources/code_search.c
 * OSINT service — faithful port of OSINTsaas osint_tools/code_search.c
 * (code_search → handle_code_search). Canonical SERVICE id in
 * osint_dispatcher.c: {SERVICE_GITHUB_USER_LOOKUP, handle_code_search,
 * "GITHUB_CODE_SEARCH", true}. (The CREDENTIAL_LEAK_SEARCH alias maps to a
 * *different* handler handle_credential_leak_search → not this file.)
 * Entity = keyword. GITHUB_TOKEN / GITHUB_API_TOKEN is optional (upstream
 * never sends it as a header — it only affects the rate-limit message), so
 * NOT a hard gate. Builds {query,github_code,github_repos,gitlab,
 * credential_scan} exactly. success=true, conf 85. Emits one
 * osint_service_result row (body = {success,confidence,data}). */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void uri_encode(const char *in, char *out, size_t cap) {
  static const char *keep = "-_.!~*'()";
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && w + 4 < cap; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(keep, c)) out[w++] = (char)c;
    else { snprintf(out + w, cap - w, "%%%02X", c); w += 3; }
  }
  out[w] = 0;
}

/* GitHub requires a User-Agent. Upstream uses bare http_get (libcurl default
 * UA); we set a UA so the API doesn't 403 the request outright. */
static const char *GH_HDR[2] = { "User-Agent: OSINT-SaaS-Platform/1.0", NULL };

static int http_get(http_client *h, const char *url, long *code, cJSON **out) {
  http_response hr = {0};
  int hc = http_request(h, "GET", url, GH_HDR, NULL, 0, 20000, 1, &hr);
  *code = hr.status;
  *out = (hc == 0 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  return hc;
}

static cJSON *github_code(http_client *h, const char *q, int have_tok) {
  char enc[1024]; uri_encode(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url,
    "https://api.github.com/search/code?q=%s&per_page=30", enc);
  long code = 0; cJSON *j = NULL;
  int hc = http_get(h, url, &code, &j);
  if (hc == 0 && (code == 403 || code == 429)) {
    if (j) cJSON_Delete(j);
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "error", "Rate limited");
    cJSON_AddStringToObject(r, "note",
      have_tok ? "Rate limit exceeded" : "Set GITHUB_TOKEN for higher limits");
    return r;
  }
  if (hc != 0 || code != 200 || !j) { if (j) cJSON_Delete(j); return NULL; }
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "GitHub Code");
  cJSON *tot = cJSON_GetObjectItem(j, "total_count");
  if (tot && cJSON_IsNumber(tot))
    cJSON_AddNumberToObject(r, "total_results", tot->valueint);
  cJSON *items = cJSON_GetObjectItem(j, "items");
  if (items && cJSON_IsArray(items)) {
    cJSON *ms = cJSON_CreateArray();
    int n = cJSON_GetArraySize(items);
    for (int i = 0; i < n && i < 20; i++) {
      cJSON *it = cJSON_GetArrayItem(items, i);
      cJSON *m = cJSON_CreateObject();
      cJSON *nm = cJSON_GetObjectItem(it, "name");
      cJSON *pa = cJSON_GetObjectItem(it, "path");
      cJSON *hu = cJSON_GetObjectItem(it, "html_url");
      cJSON *rp = cJSON_GetObjectItem(it, "repository");
      if (nm && cJSON_IsString(nm)) cJSON_AddStringToObject(m, "filename", nm->valuestring);
      if (pa && cJSON_IsString(pa)) cJSON_AddStringToObject(m, "path", pa->valuestring);
      if (hu && cJSON_IsString(hu)) cJSON_AddStringToObject(m, "url", hu->valuestring);
      if (rp) {
        cJSON *rn = cJSON_GetObjectItem(rp, "full_name");
        cJSON *ru = cJSON_GetObjectItem(rp, "html_url");
        cJSON *st = cJSON_GetObjectItem(rp, "stargazers_count");
        if (rn && cJSON_IsString(rn)) cJSON_AddStringToObject(m, "repository", rn->valuestring);
        if (ru && cJSON_IsString(ru)) cJSON_AddStringToObject(m, "repo_url", ru->valuestring);
        if (st && cJSON_IsNumber(st)) cJSON_AddNumberToObject(m, "stars", st->valueint);
      }
      cJSON_AddItemToArray(ms, m);
    }
    cJSON_AddItemToObject(r, "matches", ms);
  }
  cJSON_Delete(j);
  return r;
}

static cJSON *github_repos(http_client *h, const char *q) {
  char enc[1024]; uri_encode(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url,
    "https://api.github.com/search/repositories?q=%s&per_page=30", enc);
  long code = 0; cJSON *j = NULL;
  int hc = http_get(h, url, &code, &j);
  if (hc != 0 || code != 200 || !j) { if (j) cJSON_Delete(j); return NULL; }
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "GitHub Repositories");
  cJSON *tot = cJSON_GetObjectItem(j, "total_count");
  if (tot && cJSON_IsNumber(tot))
    cJSON_AddNumberToObject(r, "total_results", tot->valueint);
  cJSON *items = cJSON_GetObjectItem(j, "items");
  if (items && cJSON_IsArray(items)) {
    cJSON *rs = cJSON_CreateArray();
    int n = cJSON_GetArraySize(items);
    for (int i = 0; i < n && i < 15; i++) {
      cJSON *it = cJSON_GetArrayItem(items, i);
      cJSON *ro = cJSON_CreateObject();
      cJSON *fn = cJSON_GetObjectItem(it, "full_name");
      cJSON *de = cJSON_GetObjectItem(it, "description");
      cJSON *hu = cJSON_GetObjectItem(it, "html_url");
      cJSON *la = cJSON_GetObjectItem(it, "language");
      cJSON *st = cJSON_GetObjectItem(it, "stargazers_count");
      cJSON *fk = cJSON_GetObjectItem(it, "forks_count");
      cJSON *up = cJSON_GetObjectItem(it, "updated_at");
      cJSON *ow = cJSON_GetObjectItem(it, "owner");
      if (fn && cJSON_IsString(fn)) cJSON_AddStringToObject(ro, "name", fn->valuestring);
      if (de && cJSON_IsString(de)) cJSON_AddStringToObject(ro, "description", de->valuestring);
      if (hu && cJSON_IsString(hu)) cJSON_AddStringToObject(ro, "url", hu->valuestring);
      if (la && cJSON_IsString(la)) cJSON_AddStringToObject(ro, "language", la->valuestring);
      if (st && cJSON_IsNumber(st)) cJSON_AddNumberToObject(ro, "stars", st->valueint);
      if (fk && cJSON_IsNumber(fk)) cJSON_AddNumberToObject(ro, "forks", fk->valueint);
      if (up && cJSON_IsString(up)) cJSON_AddStringToObject(ro, "last_updated", up->valuestring);
      if (ow) {
        cJSON *lo = cJSON_GetObjectItem(ow, "login");
        cJSON *av = cJSON_GetObjectItem(ow, "avatar_url");
        if (lo && cJSON_IsString(lo)) cJSON_AddStringToObject(ro, "owner", lo->valuestring);
        if (av && cJSON_IsString(av)) cJSON_AddStringToObject(ro, "owner_avatar", av->valuestring);
      }
      cJSON_AddItemToArray(rs, ro);
    }
    cJSON_AddItemToObject(r, "repositories", rs);
  }
  cJSON_Delete(j);
  return r;
}

static cJSON *gitlab(http_client *h, const char *q) {
  char enc[1024]; uri_encode(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url,
    "https://gitlab.com/api/v4/search?scope=projects&search=%s", enc);
  long code = 0; cJSON *j = NULL;
  int hc = http_get(h, url, &code, &j);
  if (hc != 0 || code != 200 || !j || !cJSON_IsArray(j)) {
    if (j) cJSON_Delete(j); return NULL;
  }
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "GitLab");
  int n = cJSON_GetArraySize(j);
  cJSON_AddNumberToObject(r, "total_results", n);
  cJSON *ps = cJSON_CreateArray();
  for (int i = 0; i < n && i < 15; i++) {
    cJSON *it = cJSON_GetArrayItem(j, i);
    cJSON *p = cJSON_CreateObject();
    cJSON *nm = cJSON_GetObjectItem(it, "path_with_namespace");
    cJSON *de = cJSON_GetObjectItem(it, "description");
    cJSON *wu = cJSON_GetObjectItem(it, "web_url");
    cJSON *st = cJSON_GetObjectItem(it, "star_count");
    cJSON *fk = cJSON_GetObjectItem(it, "forks_count");
    cJSON *up = cJSON_GetObjectItem(it, "last_activity_at");
    if (nm && cJSON_IsString(nm)) cJSON_AddStringToObject(p, "name", nm->valuestring);
    if (de && cJSON_IsString(de)) cJSON_AddStringToObject(p, "description", de->valuestring);
    if (wu && cJSON_IsString(wu)) cJSON_AddStringToObject(p, "url", wu->valuestring);
    if (st && cJSON_IsNumber(st)) cJSON_AddNumberToObject(p, "stars", st->valueint);
    if (fk && cJSON_IsNumber(fk)) cJSON_AddNumberToObject(p, "forks", fk->valueint);
    if (up && cJSON_IsString(up)) cJSON_AddStringToObject(p, "last_updated", up->valuestring);
    cJSON_AddItemToArray(ps, p);
  }
  cJSON_AddItemToObject(r, "projects", ps);
  cJSON_Delete(j);
  return r;
}

/* Credential-leak scan: GitHub code search w/ leak query patterns. */
static cJSON *credential_scan(http_client *h, const char *q) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "search_type", "credential_leak");
  const char *lq[] = { "password filename:.env", "api_key filename:config",
                       "secret_key filename:.json", NULL };
  cJSON *lf = cJSON_CreateArray();
  int total = 0;
  for (int i = 0; lq[i] && i < 3; i++) {
    char sq[512];
    snprintf(sq, sizeof sq, "%s %s", q, lq[i]);
    char enc[1024]; uri_encode(sq, enc, sizeof enc);
    char url[1100];
    snprintf(url, sizeof url,
      "https://api.github.com/search/code?q=%s&per_page=30", enc);
    long code = 0; cJSON *j = NULL;
    int hc = http_get(h, url, &code, &j);
    if (hc != 0 || code != 200 || !j) { if (j) cJSON_Delete(j); continue; }
    cJSON *items = cJSON_GetObjectItem(j, "items");
    cJSON *tot = cJSON_GetObjectItem(j, "total_count");
    if (tot && cJSON_IsNumber(tot) && tot->valueint > 0) {
      total += tot->valueint;
      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "pattern", lq[i]);
      cJSON_AddNumberToObject(f, "matches", tot->valueint);
      if (items && cJSON_IsArray(items) && cJSON_GetArraySize(items) > 0) {
        cJSON *samp = cJSON_CreateArray();
        int sn = cJSON_GetArraySize(items);
        for (int k = 0; k < sn && k < 3; k++) {
          cJSON *it = cJSON_GetArrayItem(items, k);
          cJSON *s = cJSON_CreateObject();
          cJSON *pa = cJSON_GetObjectItem(it, "path");
          cJSON *hu = cJSON_GetObjectItem(it, "html_url");
          cJSON *rp = cJSON_GetObjectItem(it, "repository");
          if (pa && cJSON_IsString(pa)) cJSON_AddStringToObject(s, "file", pa->valuestring);
          if (hu && cJSON_IsString(hu)) cJSON_AddStringToObject(s, "url", hu->valuestring);
          if (rp) {
            cJSON *rn = cJSON_GetObjectItem(rp, "full_name");
            if (rn && cJSON_IsString(rn)) cJSON_AddStringToObject(s, "repo", rn->valuestring);
          }
          cJSON_AddItemToArray(samp, s);
        }
        cJSON_AddItemToObject(f, "samples", samp);
      }
      cJSON_AddItemToArray(lf, f);
    }
    cJSON_Delete(j);
  }
  cJSON_AddItemToObject(r, "findings", lf);
  cJSON_AddNumberToObject(r, "total_potential_leaks", total);
  if (total > 0) {
    cJSON_AddStringToObject(r, "risk_level", total > 10 ? "HIGH" : "MEDIUM");
    cJSON_AddStringToObject(r, "recommendation",
      "Review findings and rotate any exposed credentials immediately");
  } else {
    cJSON_AddStringToObject(r, "risk_level", "LOW");
  }
  return r;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  const char *tok = getenv("GITHUB_TOKEN");
  if (!tok) tok = getenv("GITHUB_API_TOKEN");
  int have_tok = tok && *tok;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", q);
  cJSON *gc = github_code(ctx->http, q, have_tok);
  if (gc) cJSON_AddItemToObject(root, "github_code", gc);
  cJSON *gr = github_repos(ctx->http, q);
  if (gr) cJSON_AddItemToObject(root, "github_repos", gr);
  cJSON *gl = gitlab(ctx->http, q);
  if (gl) cJSON_AddItemToObject(root, "gitlab", gl);
  cJSON *cs = credential_scan(ctx->http, q);
  if (cs) cJSON_AddItemToObject(root, "credential_scan", cs);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 85);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "GITHUB_CODE_SEARCH");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300], title[320];
  snprintf(rk, sizeof rk, "codesearch:%s", q);
  snprintf(title, sizeof title, "GITHUB_CODE_SEARCH — %s", q);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "code search complete";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"GITHUB_CODE_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def code_search_def = {
  .id = "GITHUB_CODE_SEARCH", .collector = "osint",
  .name = "Code Search", .name_ja = "コード検索",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(code_search_def)
