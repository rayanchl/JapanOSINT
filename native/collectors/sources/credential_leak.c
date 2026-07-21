/* collectors/osint/sources/credential_leak.c — CREDENTIAL_LEAK_SEARCH, ported
 * from OSINTsaas code_search.c (handle_credential_leak_search) onto the
 * JapanOSINT source ABI.
 *
 * Pivots on a domain/email/keyword and searches public GitHub code for that
 * term in credential-ish contexts. GitHub's code-search API requires auth, so
 * this gates on GITHUB_TOKEN / GITHUB_API_TOKEN and returns honest-empty
 * without one. Emits one item per code hit (repo + path + url) — real data
 * only, never fabricated. */
#include "../../source.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void urlenc(const char *s, char *out, size_t cap) {
  size_t o = 0;
  for (const char *p = s; *p && o + 4 < cap; p++) {
    unsigned char c = (unsigned char)*p;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out[o++] = (char)c;
    else o += (size_t)snprintf(out + o, cap - o, "%%%02X", c);
  }
  out[o] = 0;
}

static int emit_hit(intel_sink *sink, const char *entity, cJSON *item) {
  cJSON *path = cJSON_GetObjectItem(item, "path");
  cJSON *html = cJSON_GetObjectItem(item, "html_url");
  cJSON *repo = cJSON_GetObjectItem(item, "repository");
  cJSON *full = repo ? cJSON_GetObjectItem(repo, "full_name") : NULL;
  if (!html || !cJSON_IsString(html)) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "source", "api.github.com");
  if (full && cJSON_IsString(full)) cJSON_AddStringToObject(data, "repo", full->valuestring);
  if (path && cJSON_IsString(path)) cJSON_AddStringToObject(data, "path", path->valuestring);
  cJSON_AddStringToObject(data, "url", html->valuestring);
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CREDENTIAL_LEAK_SEARCH");
  cJSON_AddStringToObject(props, "entity", entity);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[400], title[320];
  snprintf(rk, sizeof rk, "credleak:%s", html->valuestring);
  snprintf(title, sizeof title, "Possible exposure: %s",
           (full && cJSON_IsString(full)) ? full->valuestring : html->valuestring);

  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj;
  it.summary = "github code match"; it.link = html->valuestring;
  it.record_type = "osint_service_result"; it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"CREDENTIAL_LEAK_SEARCH\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int cred_run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return 0;
  const char *token = getenv("GITHUB_TOKEN");
  if (!token || !*token) token = getenv("GITHUB_API_TOKEN");
  if (!token || !*token) return 0;             /* code search needs auth */

  char q[512];
  snprintf(q, sizeof q, "\"%s\" (password OR api_key OR secret OR token)", entity);
  char enc[640]; urlenc(q, enc, sizeof enc);
  char url[768];
  snprintf(url, sizeof url, "https://api.github.com/search/code?q=%s&per_page=30", enc);

  char auth[256]; snprintf(auth, sizeof auth, "Authorization: Bearer %s", token);
  const char *headers[] = { auth, "Accept: application/vnd.github+json", NULL };

  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, headers, NULL, 0, 15000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  if (!j) return 0;
  cJSON *items = cJSON_GetObjectItem(j, "items");
  int emitted = 0, n = (items && cJSON_IsArray(items)) ? cJSON_GetArraySize(items) : 0;
  for (int i = 0; i < n; i++) emitted += emit_hit(sink, entity, cJSON_GetArrayItem(items, i));
  cJSON_Delete(j);
  return emitted;
}

static const source_def credential_leak_def = {
  .id = "CREDENTIAL_LEAK_SEARCH", .collector = "osint",
  .name = "Credential Leak Search", .run = cred_run,
  .category = "investigation", .type = "api", .url = "https://api.github.com/",
  .description = "Searches public GitHub code for an entity in credential contexts (requires GITHUB_TOKEN).",
  .free_tier = 1,
};
REGISTER_SOURCE(credential_leak_def)
