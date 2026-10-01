/* soc_forges.c — code-forge activity streams and search.
 *
 *  github-public-events   (scheduled) GET https://api.github.com/events?per_page=30
 *      emits: event id, type, actor.login/id, repo.name, created_at plus the
 *             payload fields that exist FOR THAT TYPE (push head/ref/size,
 *             release tag_name, issue number/title/action, create ref_type,
 *             fork forkee.full_name, PR number/title/action)
 *      keyless at 60 req/hr per IP; GITHUB_TOKEN is read with getenv and used
 *      when present, never required (R6).
 *      NOTE: the ETag/If-None-Match round trip the API documents cannot be
 *      done here — the shared http client does not surface response headers —
 *      so the collector simply respects the 15-minute interval.
 *      licence: documented public REST endpoint.
 *  github-public-gists    (scheduled) GET https://api.github.com/gists/public
 *      emits: gist id, html_url, description, created_at, updated_at,
 *             owner.login, comments, and per-file filename/language/raw_url
 *             (files is an OBJECT keyed by filename, not an array). Metadata
 *             only — raw bodies are not downloaded.
 *      licence: documented public REST endpoint, keyless; only gists their
 *               author explicitly published as public are served.
 *  gitlab-recent-projects (scheduled) GET https://gitlab.com/api/v4/projects
 *      emits: id, name, path_with_namespace, web_url, description, created_at,
 *             last_activity_at, star_count, forks_count, topics, default_branch
 *      licence: GitLab REST API v4; unauthenticated listing of public projects.
 *  GITEA_REPO_SEARCH      (entity pivot: keyword)
 *      GET https://gitea.com/api/v1/repos/search?q=&limit=20  → {ok, data[]}
 *      emits: id, full_name, html_url, description, owner.login/full_name,
 *             created_at, updated_at, stars_count, forks_count, language
 *      licence: Gitea's Swagger-documented public API; unauthenticated read.
 *  SOURCEGRAPH_CODE_SEARCH (entity pivot: keyword)
 *      GET https://sourcegraph.com/.api/search/stream?q=context:global+count:N+<kw>
 *      TRAP: this is server-sent events, not plain JSON — read `event: matches`
 *      frames and parse the following `data:` line as a JSON array; ignore the
 *      filters/progress/done frames. `count:` MUST be in the query or the first
 *      progress frame reports matchCount 0.
 *      emits: repository, repoStars, path, commit, branches, language and the
 *             matched line text + line number
 *      licence: sourcegraph.com public code search over open-source repos; the
 *               streaming endpoint answers unauthenticated (rate limited).
 *
 * No upstream here returns coordinates → has_geo stays 0 (R2).
 */
#include "lib/osintemit.h"
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOC_UA_LINE \
  "User-Agent: JapanOSINT-native/1.0 (OSINT research collector; +https://github.com/)"

static const char *const SOC_UA[] = { SOC_UA_LINE, "Accept: application/json", NULL };

/* ---- github-public-events ---------------------------------------------- */

static int run_gh_events(const source_ctx *ctx, intel_sink *sink) {
  const char *tok = getenv("GITHUB_TOKEN");
  char auth[256];
  const char *hdrs[5];
  int hi = 0;
  hdrs[hi++] = SOC_UA_LINE;
  hdrs[hi++] = "Accept: application/vnd.github+json";
  if (tok && *tok) {                       /* optional: raises the rate limit */
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", tok);
    hdrs[hi++] = auth;
  } else {
    fprintf(stderr, "[github-public-events] GITHUB_TOKEN unset — using the "
                    "keyless 60 req/hr tier\n");
  }
  hdrs[hi] = NULL;

  cJSON *doc = feed_get_json_h(ctx->http, "https://api.github.com/events?per_page=30",
                               hdrs, 25000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[github-public-events] fetch/parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *e;
  cJSON_ArrayForEach(e, doc) {
    const char *id   = jo_sv(e, "id");
    const char *type = jo_sv(e, "type");
    if (!id || !type) continue;
    const cJSON *actor = cJSON_GetObjectItem(e, "actor");
    const cJSON *repo  = cJSON_GetObjectItem(e, "repo");
    const cJSON *pl    = cJSON_GetObjectItem(e, "payload");
    const char *login  = cJSON_IsObject(actor) ? jo_sv(actor, "login") : NULL;
    const char *rname  = cJSON_IsObject(repo)  ? jo_sv(repo, "name")  : NULL;
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "api.github.com");
    jcopy(p, "id", e, "id");
    jcopy(p, "type", e, "type");
    jcopy(p, "created_at", e, "created_at");
    jcopy(p, "public", e, "public");
    if (cJSON_IsObject(actor)) {
      jcopy(p, "actor", actor, "login");
      jcopy(p, "actor_id", actor, "id");
    }
    if (cJSON_IsObject(repo)) {
      jcopy(p, "repo", repo, "name");
      jcopy(p, "repo_url", repo, "url");
    }
    /* payload shape varies by type — only read what exists for this type */
    char detail[300]; detail[0] = 0;
    if (cJSON_IsObject(pl)) {
      if (!strcmp(type, "PushEvent")) {
        jcopy(p, "ref", pl, "ref");
        jcopy(p, "head", pl, "head");
        jcopy(p, "commits_pushed", pl, "size");
        const cJSON *sz = cJSON_GetObjectItem(pl, "size");
        snprintf(detail, sizeof detail, "%ld commit(s) to %s",
                 cJSON_IsNumber(sz) ? (long)sz->valuedouble : 0L,
                 jo_sv(pl, "ref") ? jo_sv(pl, "ref") : "?");
      } else if (!strcmp(type, "CreateEvent") || !strcmp(type, "DeleteEvent")) {
        jcopy(p, "ref", pl, "ref");
        jcopy(p, "ref_type", pl, "ref_type");
        snprintf(detail, sizeof detail, "%s %s",
                 jo_sv(pl, "ref_type") ? jo_sv(pl, "ref_type") : "ref",
                 jo_sv(pl, "ref") ? jo_sv(pl, "ref") : "");
      } else if (!strcmp(type, "ReleaseEvent")) {
        const cJSON *rel = cJSON_GetObjectItem(pl, "release");
        if (cJSON_IsObject(rel)) {
          jcopy(p, "release_tag", rel, "tag_name");
          jcopy(p, "release_name", rel, "name");
          jcopy(p, "release_url", rel, "html_url");
          snprintf(detail, sizeof detail, "release %s",
                   jo_sv(rel, "tag_name") ? jo_sv(rel, "tag_name") : "");
        }
      } else if (!strcmp(type, "IssuesEvent") || !strcmp(type, "IssueCommentEvent")) {
        const cJSON *iss = cJSON_GetObjectItem(pl, "issue");
        jcopy(p, "action", pl, "action");
        if (cJSON_IsObject(iss)) {
          jcopy(p, "issue_number", iss, "number");
          jcopy(p, "issue_title", iss, "title");
          jcopy(p, "issue_url", iss, "html_url");
          snprintf(detail, sizeof detail, "issue: %.200s",
                   jo_sv(iss, "title") ? jo_sv(iss, "title") : "");
        }
      } else if (!strcmp(type, "PullRequestEvent")) {
        const cJSON *pr = cJSON_GetObjectItem(pl, "pull_request");
        jcopy(p, "action", pl, "action");
        if (cJSON_IsObject(pr)) {
          jcopy(p, "pr_number", pr, "number");
          jcopy(p, "pr_title", pr, "title");
          jcopy(p, "pr_url", pr, "html_url");
          snprintf(detail, sizeof detail, "PR: %.200s",
                   jo_sv(pr, "title") ? jo_sv(pr, "title") : "");
        }
      } else if (!strcmp(type, "ForkEvent")) {
        const cJSON *fk = cJSON_GetObjectItem(pl, "forkee");
        if (cJSON_IsObject(fk)) {
          jcopy(p, "forkee", fk, "full_name");
          snprintf(detail, sizeof detail, "forked to %s",
                   jo_sv(fk, "full_name") ? jo_sv(fk, "full_name") : "");
        }
      } else {
        jcopy(p, "action", pl, "action");
      }
    }
    char title[420], link[300];
    snprintf(title, sizeof title, "%s %s %s", login ? login : "?", type,
             rname ? rname : "");
    if (rname) snprintf(link, sizeof link, "https://github.com/%s", rname);
    else       snprintf(link, sizeof link, "https://github.com/");
    n += soc_emit(sink, p, "forge-event",
                  "[\"github\",\"forge\",\"activity\"]",
                  id, title, detail[0] ? detail : NULL, link,
                  jo_sv(e, "created_at"), NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[github-public-events] emitted %d\n", n);
  return 0;
}

static const source_def soc_gh_events_def = {
  .id = "github-public-events", .collector = "social",
  .name = "GitHub Public Events Firehose",
  .update_interval_sec = 900, .run = run_gh_events,
  .category = "social", .type = "api",
  .url = "https://api.github.com/events",
  .description = "The global GitHub activity firehose — public pushes, forks, releases, issues and repo creations in real time, with the payload fields that exist for each event type.",
  .license = "Documented public REST endpoint; keyless at 60 req/hr per IP, GITHUB_TOKEN optional.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_gh_events_def)

/* ---- github-public-gists ------------------------------------------------ */

static int run_gh_gists(const source_ctx *ctx, intel_sink *sink) {
  const char *tok = getenv("GITHUB_TOKEN");
  char auth[256];
  const char *hdrs[5];
  int hi = 0;
  hdrs[hi++] = SOC_UA_LINE;
  hdrs[hi++] = "Accept: application/vnd.github+json";
  if (tok && *tok) {
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", tok);
    hdrs[hi++] = auth;
  }
  hdrs[hi] = NULL;

  cJSON *doc = feed_get_json_h(ctx->http,
      "https://api.github.com/gists/public?per_page=30", hdrs, 25000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[github-public-gists] fetch/parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *g;
  cJSON_ArrayForEach(g, doc) {
    const char *id  = jo_sv(g, "id");
    const char *hu  = jo_sv(g, "html_url");
    if (!id || !hu) continue;
    const cJSON *owner = cJSON_GetObjectItem(g, "owner");
    const char *login = cJSON_IsObject(owner) ? jo_sv(owner, "login") : NULL;
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "api.github.com");
    jcopy(p, "id", g, "id");
    jcopy(p, "html_url", g, "html_url");
    jcopy(p, "description", g, "description");
    jcopy(p, "created_at", g, "created_at");
    jcopy(p, "updated_at", g, "updated_at");
    jcopy(p, "comments", g, "comments");
    jcopy(p, "public", g, "public");
    if (login) cJSON_AddStringToObject(p, "owner", login);
    /* files is an OBJECT keyed by filename — iterate the children. */
    cJSON *farr = cJSON_CreateArray();
    char firstlang[64]; firstlang[0] = 0;
    const cJSON *files = cJSON_GetObjectItem(g, "files");
    int nfiles = 0;
    if (farr && cJSON_IsObject(files)) {
      for (const cJSON *f = files->child; f; f = f->next) {
        cJSON *fo = cJSON_CreateObject();
        if (!fo) continue;
        if (f->string) cJSON_AddStringToObject(fo, "filename", f->string);
        jcopy(fo, "language", f, "language");
        jcopy(fo, "type", f, "type");
        jcopy(fo, "size", f, "size");
        jcopy(fo, "raw_url", f, "raw_url");
        cJSON_AddItemToArray(farr, fo);
        nfiles++;
        if (!firstlang[0] && jo_sv(f, "language"))
          snprintf(firstlang, sizeof firstlang, "%s", jo_sv(f, "language"));
      }
    }
    if (farr) cJSON_AddItemToObject(p, "files", farr);
    const char *desc = jo_sv(g, "description");
    char title[420], summary[220];
    snprintf(title, sizeof title, "gist by %s: %.250s", login ? login : "?",
             desc ? desc : (files && files->child && files->child->string
                              ? files->child->string : id));
    snprintf(summary, sizeof summary, "%d file(s)%s%s", nfiles,
             firstlang[0] ? " · " : "", firstlang[0] ? firstlang : "");
    n += soc_emit(sink, p, "gist",
                  "[\"github\",\"gist\",\"paste\"]",
                  id, title, summary, hu, jo_sv(g, "created_at"), NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[github-public-gists] emitted %d\n", n);
  return 0;
}

static const source_def soc_gh_gists_def = {
  .id = "github-public-gists", .collector = "social",
  .name = "GitHub Public Gists Stream",
  .update_interval_sec = 1800, .run = run_gh_gists,
  .category = "social", .type = "api",
  .url = "https://api.github.com/gists/public",
  .description = "Chronological stream of newly published public gists — filenames, language, owner, timestamps and raw URLs. The largest genuinely open paste index still operating.",
  .license = "Documented public REST endpoint, keyless; only author-published public gists are served.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_gh_gists_def)

/* ---- gitlab-recent-projects --------------------------------------------- */

static int run_gitlab_recent(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://gitlab.com/api/v4/projects?visibility=public"
      "&order_by=last_activity_at&per_page=20", SOC_UA, 25000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[gitlab-recent-projects] fetch/parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    const char *pwn = jo_sv(r, "path_with_namespace");
    const char *web = jo_sv(r, "web_url");
    if (!pwn || !web) continue;
    const cJSON *st = cJSON_GetObjectItem(r, "star_count");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "gitlab.com");
    jcopy(p, "id", r, "id");
    jcopy(p, "name", r, "name");
    jcopy(p, "path_with_namespace", r, "path_with_namespace");
    jcopy(p, "web_url", r, "web_url");
    jcopy(p, "description", r, "description");
    jcopy(p, "created_at", r, "created_at");
    jcopy(p, "last_activity_at", r, "last_activity_at");
    jcopy(p, "star_count", r, "star_count");
    jcopy(p, "forks_count", r, "forks_count");
    jcopy(p, "topics", r, "topics");
    jcopy(p, "default_branch", r, "default_branch");
    char summary[300];
    snprintf(summary, sizeof summary, "%ld stars · active %s · created %s",
             cJSON_IsNumber(st) ? (long)st->valuedouble : 0L,
             jo_sv(r, "last_activity_at") ? jo_sv(r, "last_activity_at") : "?",
             jo_sv(r, "created_at") ? jo_sv(r, "created_at") : "?");
    n += soc_emit(sink, p, "forge-repo",
                  "[\"gitlab\",\"forge\",\"repository\"]",
                  pwn, pwn, summary, web, jo_sv(r, "last_activity_at"), NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[gitlab-recent-projects] emitted %d\n", n);
  return 0;
}

static const source_def soc_gitlab_recent_def = {
  .id = "gitlab-recent-projects", .collector = "social",
  .name = "GitLab.com Recently Active Public Projects",
  .update_interval_sec = 1800, .run = run_gitlab_recent,
  .category = "social", .type = "api",
  .url = "https://gitlab.com/api/v4/projects?visibility=public&order_by=last_activity_at",
  .description = "Activity-ordered stream of public GitLab.com projects — surfaces a repo that was just made public or just pushed to, which keyword search only indexes later.",
  .license = "GitLab REST API v4; unauthenticated listing of public projects, rate limited per IP.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_gitlab_recent_def)

/* ---- GITEA_REPO_SEARCH (entity pivot: keyword) -------------------------- */

static int run_gitea_search(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
           "https://gitea.com/api/v1/repos/search?q=%s&limit=20", enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!doc) {
    fprintf(stderr, "[GITEA_REPO_SEARCH] no response for %s\n", q);
    return 0;
  }
  const cJSON *arr = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(arr)) arr = cJSON_IsArray(doc) ? doc : NULL;
  int n = 0;
  const cJSON *r;
  if (arr) cJSON_ArrayForEach(r, arr) {
    const char *full = jo_sv(r, "full_name");
    const char *hu   = jo_sv(r, "html_url");
    if (!full || !hu) continue;
    const cJSON *owner = cJSON_GetObjectItem(r, "owner");
    const cJSON *st = cJSON_GetObjectItem(r, "stars_count");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "GITEA_REPO_SEARCH");
    cJSON_AddStringToObject(p, "source", "gitea.com");
    cJSON_AddStringToObject(p, "entity", q);
    jcopy(p, "id", r, "id");
    jcopy(p, "full_name", r, "full_name");
    jcopy(p, "html_url", r, "html_url");
    jcopy(p, "description", r, "description");
    jcopy(p, "created_at", r, "created_at");
    jcopy(p, "updated_at", r, "updated_at");
    jcopy(p, "stars_count", r, "stars_count");
    jcopy(p, "forks_count", r, "forks_count");
    jcopy(p, "language", r, "language");
    if (cJSON_IsObject(owner)) {
      jcopy(p, "owner_login", owner, "login");
      jcopy(p, "owner_full_name", owner, "full_name");
    }
    cJSON_AddBoolToObject(p, "success", 1);
    char summary[300];
    snprintf(summary, sizeof summary, "%ld stars · %s · %.150s",
             cJSON_IsNumber(st) ? (long)st->valuedouble : 0L,
             jo_sv(r, "language") ? jo_sv(r, "language") : "?",
             jo_sv(r, "description") ? jo_sv(r, "description") : "");
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"GITEA_REPO_SEARCH\",\"forge\"]",
                  full, full, summary, hu, jo_sv(r, "updated_at"), NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[GITEA_REPO_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_gitea_search_def = {
  .id = "GITEA_REPO_SEARCH", .collector = "osint",
  .name = "Gitea.com Repository Search",
  .update_interval_sec = 0, .run = run_gitea_search,
  .category = "social", .type = "api",
  .url = "https://gitea.com/api/v1/repos/search",
  .description = "Keyword search over the flagship Gitea host — full name, owner, description, language, stars and timestamps. The same path works on every self-hosted Gitea/Forgejo install.",
  .license = "Gitea public Swagger-documented API; unauthenticated read of public repos.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_gitea_search_def)

/* ---- SOURCEGRAPH_CODE_SEARCH (entity pivot: keyword) -------------------- */

static int sg_emit_matches(intel_sink *sink, const cJSON *arr, const char *q) {
  int n = 0;
  const cJSON *m;
  cJSON_ArrayForEach(m, arr) {
    const char *repo = jo_sv(m, "repository");
    const char *path = jo_sv(m, "path");
    if (!repo) continue;
    /* one row per matched line (lineMatches) or chunk (chunkMatches) */
    const cJSON *lm = cJSON_GetObjectItem(m, "lineMatches");
    const cJSON *cm = cJSON_GetObjectItem(m, "chunkMatches");
    const cJSON *l;
    int emitted_here = 0;
    if (cJSON_IsArray(lm)) cJSON_ArrayForEach(l, lm) {
      const char *line = jo_sv(l, "line");
      const cJSON *ln = cJSON_GetObjectItem(l, "lineNumber");
      if (!line) continue;
      cJSON *p = cJSON_CreateObject();
      if (!p) continue;
      cJSON_AddStringToObject(p, "service", "SOURCEGRAPH_CODE_SEARCH");
      cJSON_AddStringToObject(p, "source", "sourcegraph.com");
      cJSON_AddStringToObject(p, "entity", q);
      jcopy(p, "repository", m, "repository");
      jcopy(p, "repository_id", m, "repositoryID");
      jcopy(p, "repo_stars", m, "repoStars");
      jcopy(p, "path", m, "path");
      jcopy(p, "commit", m, "commit");
      jcopy(p, "branches", m, "branches");
      jcopy(p, "language", m, "language");
      cJSON_AddStringToObject(p, "line", line);
      if (cJSON_IsNumber(ln)) cJSON_AddNumberToObject(p, "line_number", ln->valuedouble);
      cJSON_AddBoolToObject(p, "success", 1);
      char title[500], link[600], rk[400], summary[400];
      snprintf(title, sizeof title, "%s/%s:%ld", repo, path ? path : "",
               cJSON_IsNumber(ln) ? (long)ln->valuedouble + 1 : 0L);
      snprintf(link, sizeof link, "https://sourcegraph.com/%s/-/blob/%s",
               repo, path ? path : "");
      snprintf(rk, sizeof rk, "sg:%s:%s:%ld", repo, path ? path : "",
               cJSON_IsNumber(ln) ? (long)ln->valuedouble : 0L);
      snprintf(summary, sizeof summary, "%.300s", line);
      n += soc_emit(sink, p, "osint_service_result",
                    "[\"osint-search\",\"SOURCEGRAPH_CODE_SEARCH\",\"code\"]",
                    rk, title, summary, link, NULL, NULL);
      emitted_here++;
    }
    if (!emitted_here && cJSON_IsArray(cm)) cJSON_ArrayForEach(l, cm) {
      const char *content = jo_sv(l, "content");
      const cJSON *cs = cJSON_GetObjectItem(l, "contentStart");
      const cJSON *ln = cJSON_IsObject(cs) ? cJSON_GetObjectItem(cs, "line") : NULL;
      if (!content) continue;
      cJSON *p = cJSON_CreateObject();
      if (!p) continue;
      cJSON_AddStringToObject(p, "service", "SOURCEGRAPH_CODE_SEARCH");
      cJSON_AddStringToObject(p, "source", "sourcegraph.com");
      cJSON_AddStringToObject(p, "entity", q);
      jcopy(p, "repository", m, "repository");
      jcopy(p, "repo_stars", m, "repoStars");
      jcopy(p, "path", m, "path");
      jcopy(p, "commit", m, "commit");
      jcopy(p, "language", m, "language");
      cJSON_AddStringToObject(p, "line", content);
      if (cJSON_IsNumber(ln)) cJSON_AddNumberToObject(p, "line_number", ln->valuedouble);
      cJSON_AddBoolToObject(p, "success", 1);
      char title[500], link[600], rk[400], summary[400];
      snprintf(title, sizeof title, "%s/%s:%ld", repo, path ? path : "",
               cJSON_IsNumber(ln) ? (long)ln->valuedouble + 1 : 0L);
      snprintf(link, sizeof link, "https://sourcegraph.com/%s/-/blob/%s",
               repo, path ? path : "");
      snprintf(rk, sizeof rk, "sg:%s:%s:%ld", repo, path ? path : "",
               cJSON_IsNumber(ln) ? (long)ln->valuedouble : 0L);
      snprintf(summary, sizeof summary, "%.300s", content);
      n += soc_emit(sink, p, "osint_service_result",
                    "[\"osint-search\",\"SOURCEGRAPH_CODE_SEARCH\",\"code\"]",
                    rk, title, summary, link, NULL, NULL);
    }
  }
  return n;
}

static int run_sourcegraph(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[600];
  /* count: MUST be present or the first progress frame reports matchCount 0 */
  snprintf(url, sizeof url,
           "https://sourcegraph.com/.api/search/stream"
           "?q=context:global+count:20+%s&v=V3&t=literal&display=20", enc);
  free(enc);
  char *txt = feed_get_text(ctx->http, url, 30000);
  if (!txt) {
    fprintf(stderr, "[SOURCEGRAPH_CODE_SEARCH] no response for %s\n", q);
    return 0;
  }
  /* Server-sent events: `event: <name>` then one or more `data: <json>` lines.
   * Only the `matches` frames carry results; filters/progress/done are noise. */
  int n = 0, in_matches = 0;
  char *p = txt;
  while (*p) {
    char *nl = strchr(p, '\n');
    if (nl) *nl = 0;
    if (!strncmp(p, "event:", 6)) {
      const char *ev = p + 6;
      while (*ev == ' ') ev++;
      in_matches = (strncmp(ev, "matches", 7) == 0);
    } else if (in_matches && !strncmp(p, "data:", 5)) {
      const char *d = p + 5;
      while (*d == ' ') d++;
      cJSON *arr = cJSON_Parse(d);
      if (cJSON_IsArray(arr)) n += sg_emit_matches(sink, arr, q);
      cJSON_Delete(arr);
    }
    if (!nl) break;
    p = nl + 1;
  }
  free(txt);
  fprintf(stderr, "[SOURCEGRAPH_CODE_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_sourcegraph_def = {
  .id = "SOURCEGRAPH_CODE_SEARCH", .collector = "osint",
  .name = "Sourcegraph Public Code Search",
  .update_interval_sec = 0, .run = run_sourcegraph,
  .category = "social", .type = "api",
  .url = "https://sourcegraph.com/.api/search/stream",
  .description = "Literal code search across millions of indexed public repositories, returning the matching line and line number with file path, branch, commit and repo stars.",
  .license = "sourcegraph.com public code search over open-source repositories; streaming endpoint answers unauthenticated, rate limited.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_sourcegraph_def)
