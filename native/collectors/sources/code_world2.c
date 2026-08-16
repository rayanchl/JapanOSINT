/* collectors/sources/code_world2.c
 * OSINT services — net-new keyless code-hosting search pivoting on ctx->entity
 * (a keyword, org, or username). Shared run() switches on ctx->source_id. Real
 * fetch or honest-empty. Keyless (GitHub unauth ~60 req/hr; GITHUB_TOKEN used if
 * present to raise the limit).
 *
 *   GITHUB_REPO_SEARCH   api.github.com/search/repositories?q=<e>   (JSON)
 *   GITLAB_PROJECT_SEARCH gitlab.com/api/v4/projects?search=<e>     (JSON)
 *   GITHUB_GISTS         api.github.com/users/<e>/gists             (JSON) */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *CD_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

/* Build the optional GitHub token header into buf; returns buf or NULL. */
static const char *cd_gh_auth(char *buf, size_t cap) {
  const char *tok = jo_env("GITHUB_TOKEN");
  if (!tok) return NULL;
  snprintf(buf, cap, "Authorization: Bearer %s", tok);
  return buf;
}

static int run_github_repos(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url,
    "https://api.github.com/search/repositories?q=%s&per_page=20&sort=stars", enc);
  char authbuf[160]; const char *auth = cd_gh_auth(authbuf, sizeof authbuf);
  const char *hdrs[] = { "Accept: application/vnd.github+json", CD_UA, auth, NULL };
  char *body = jo_get(ctx, url, hdrs, "GITHUB_REPO_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *items = cJSON_GetObjectItem(root, "items");
  int emitted = 0;
  if (cJSON_IsArray(items)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, items) {
      const char *full = jo_sv(r, "full_name");
      const char *desc = jo_sv(r, "description");
      const char *html = jo_sv(r, "html_url");
      const char *lang = jo_sv(r, "language");
      const cJSON *stars = cJSON_GetObjectItem(r, "stargazers_count");
      if (!full) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "full_name", full);
      if (desc) cJSON_AddStringToObject(d, "description", desc);
      if (lang) cJSON_AddStringToObject(d, "language", lang);
      if (stars && cJSON_IsNumber(stars)) cJSON_AddNumberToObject(d, "stars", stars->valuedouble);
      cJSON_AddStringToObject(d, "source", "GitHub");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "GITHUB_REPO_SEARCH");
      cJSON_AddStringToObject(p, "repo", full);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      intel_item it = {0};
      it.remote_key = full; it.title = full; it.summary = desc;
      it.body = bj; it.link = html; it.lang = "en";
      it.record_type = "github-repo";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"GITHUB_REPO_SEARCH\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_gitlab_projects(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url,
    "https://gitlab.com/api/v4/projects?search=%s&per_page=20&order_by=star_count&sort=desc", enc);
  const char *hdrs[] = { "Accept: application/json", CD_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "GITLAB_PROJECT_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!cJSON_IsArray(root)) { if (root) cJSON_Delete(root); return 0; }
  int emitted = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, root) {
    const char *path = jo_sv(r, "path_with_namespace");
    const char *desc = jo_sv(r, "description");
    const char *web = jo_sv(r, "web_url");
    const cJSON *stars = cJSON_GetObjectItem(r, "star_count");
    if (!path) continue;
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "path", path);
    if (desc) cJSON_AddStringToObject(d, "description", desc);
    if (stars && cJSON_IsNumber(stars)) cJSON_AddNumberToObject(d, "stars", stars->valuedouble);
    cJSON_AddStringToObject(d, "source", "GitLab");
    char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "service", "GITLAB_PROJECT_SEARCH");
    cJSON_AddStringToObject(p, "project", path);
    cJSON_AddBoolToObject(p, "success", 1);
    char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
    intel_item it = {0};
    it.remote_key = path; it.title = path; it.summary = desc;
    it.body = bj; it.link = web; it.lang = "en";
    it.record_type = "gitlab-project";
    it.properties_json = pj; it.tags_json = "[\"osint-search\",\"GITLAB_PROJECT_SEARCH\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj);
    /* (cap removed: every record of the fetched array is emitted —
     * docs/SOURCE_EXHAUSTIVENESS.md) */
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_github_gists(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://api.github.com/users/%s/gists?per_page=20", enc);
  char authbuf[160]; const char *auth = cd_gh_auth(authbuf, sizeof authbuf);
  const char *hdrs[] = { "Accept: application/vnd.github+json", CD_UA, auth, NULL };
  char *body = jo_get(ctx, url, hdrs, "GITHUB_GISTS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!cJSON_IsArray(root)) { if (root) cJSON_Delete(root); return 0; }
  int emitted = 0;
  const cJSON *g;
  cJSON_ArrayForEach(g, root) {
    const char *id = jo_sv(g, "id");
    const char *desc = jo_sv(g, "description");
    const char *html = jo_sv(g, "html_url");
    const char *created = jo_sv(g, "created_at");
    if (!id) continue;
    /* first filename */
    const char *fname = NULL;
    const cJSON *files = cJSON_GetObjectItem(g, "files");
    if (files && files->child) fname = files->child->string;
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "id", id);
    if (desc) cJSON_AddStringToObject(d, "description", desc);
    if (fname) cJSON_AddStringToObject(d, "first_file", fname);
    cJSON_AddStringToObject(d, "source", "GitHub Gist");
    char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "service", "GITHUB_GISTS");
    cJSON_AddStringToObject(p, "gist_id", id);
    cJSON_AddBoolToObject(p, "success", 1);
    char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
    char title[256];
    snprintf(title, sizeof title, "gist %s%s%s", id, (desc && desc[0]) ? " · " : "", (desc && desc[0]) ? desc : (fname ? fname : ""));
    intel_item it = {0};
    it.remote_key = id; it.title = title; it.summary = fname;
    it.body = bj; it.link = html; it.lang = "en"; it.published_at = created;
    it.record_type = "github-gist";
    it.properties_json = pj; it.tags_json = "[\"osint-search\",\"GITHUB_GISTS\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj);
  }
  cJSON_Delete(root);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "GITHUB_REPO_SEARCH"))    n = run_github_repos(ctx, sink, enc);
  else if (!strcmp(sid, "GITLAB_PROJECT_SEARCH")) n = run_gitlab_projects(ctx, sink, enc);
  else if (!strcmp(sid, "GITHUB_GISTS"))          n = run_github_gists(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define CD_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "cyber", \
    .type = "api", .url = "internal://osint/code", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

CD_DEF(cd_ghrepo_def, "GITHUB_REPO_SEARCH", "GitHub Repository Search",
  "GitHub keyless repo search by keyword/org (stars, language, description).");
CD_DEF(cd_gitlab_def, "GITLAB_PROJECT_SEARCH", "GitLab Project Search",
  "GitLab keyless project search by keyword (stars, description, url).");
CD_DEF(cd_ghgists_def, "GITHUB_GISTS", "GitHub User Gists",
  "GitHub keyless public gists for a username (description, first file, created).");
