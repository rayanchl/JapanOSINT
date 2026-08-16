/* collectors/cyber/sources/github_leaks_jp.c
 * Port of server/src/collectors/githubLeaksJp.js (intelEnvelope, env-gated).
 * env GITHUB_TOKEN → GitHub code search over DEFAULT_QUERIES → one intel row
 * per hit. The intel envelope IS the product. No token → 0 rows (JS returns
 * an empty live=false envelope; the curated _meta is dropped). */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE "https://api.github.com/search/code"

/* DEFAULT_QUERIES — verbatim JS order (env override not modelled). */
static const char *QUERIES[] = {
  "\"smtp.co.jp\" password",
  "\".go.jp\" api_key",
  "\".co.jp\" client_secret",
  "extension:env \".jp\"",
  "extension:yml \".co.jp\" password",
};

/* Minimal percent-encoder for the `q` query value (URLSearchParams). */
static void urlenc(const char *s, char *out, size_t n) {
  size_t o = 0;
  for (; *s && o + 4 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out[o++] = (char)c;
    } else {
      snprintf(out + o, n - o, "%%%02X", c);
      o += 3;
    }
  }
  out[o] = '\0';
}

static long env_int(const char *k, long def) {
  const char *v = getenv(k);
  if (!v || !*v) return def;
  long x = strtol(v, NULL, 10);
  return x;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *token = getenv("GITHUB_TOKEN");
  if (!token || !*token) {
    /* The comment below already said what this should be — "0 rows", not a
     * failure — but -1 makes the scheduler log status='error' and open a
     * collector_anomaly every tick. Gated is its own state (credtab.c). */
    fprintf(stderr, "[github-leaks-jp] gated (no GITHUB_TOKEN)\n");
    return 0;                                      /* live=false, 0 rows */
  }
  long per = env_int("GITHUB_LEAK_PER_QUERY", 20);

  char authh[512];
  snprintf(authh, sizeof authh, "Authorization: Bearer %s", token);
  const char *hdrs[] = {
    "Accept: application/vnd.github+json",
    authh,
    "X-GitHub-Api-Version: 2022-11-28",
    "User-Agent: JapanOSINT-leakcheck",
    NULL,
  };

  int n = 0;
  for (size_t qi = 0; qi < sizeof(QUERIES) / sizeof(QUERIES[0]); qi++) {
    const char *q = QUERIES[qi];
    char qenc[512];
    urlenc(q, qenc, sizeof qenc);
    char url[768];
    snprintf(url, sizeof url, "%s?q=%s&per_page=%ld", BASE, qenc, per);

    cJSON *json = feed_get_json_h(ctx->http, url, hdrs, 20000);
    if (!json) continue;                           /* HTTP error → items:[] */
    cJSON *items = cJSON_GetObjectItem(json, "items");
    if (cJSON_IsArray(items)) {
      cJSON *it;
      cJSON_ArrayForEach(it, items) {
        const char *sha  = jo_sv(it, "sha");
        const char *path = jo_sv(it, "path");
        const cJSON *repo = cJSON_GetObjectItem(it, "repository");
        const char *full = (repo && cJSON_IsObject(repo)) ? jo_sv(repo, "full_name") : NULL;
        const cJSON *owner = (repo && cJSON_IsObject(repo))
                               ? cJSON_GetObjectItem(repo, "owner") : NULL;
        const char *login = (owner && cJSON_IsObject(owner)) ? jo_sv(owner, "login") : NULL;
        const char *html = jo_sv(it, "html_url");
        const char *vis  = (repo && cJSON_IsObject(repo)) ? jo_sv(repo, "visibility") : NULL;

        /* uid = intelUid(SOURCE_ID, it.sha, `${full}_${path}`) */
        char repopath[512];
        snprintf(repopath, sizeof repopath, "%s_%s",
                 full ? full : "undefined", path ? path : "undefined");
        const char *rk = (sha && *sha) ? sha : repopath;

        /* title = `${full||'?'} — ${path||'?'}` */
        char title[640];
        snprintf(title, sizeof title, "%s \xE2\x80\x94 %s",
                 full ? full : "?", path ? path : "?");

        /* summary = `Match for: ${q}` */
        char summary[560];
        snprintf(summary, sizeof summary, "Match for: %s", q);

        /* tags = ['github-leak','defensive', `query:${q.slice(0,32)}`] */
        cJSON *tags = cJSON_CreateArray();
        cJSON_AddItemToArray(tags, cJSON_CreateString("github-leak"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("defensive"));
        char qtag[64];
        char qslice[33];
        size_t ql = strlen(q);
        if (ql > 32) ql = 32;
        memcpy(qslice, q, ql);
        qslice[ql] = '\0';
        snprintf(qtag, sizeof qtag, "query:%s", qslice);
        cJSON_AddItemToArray(tags, cJSON_CreateString(qtag));
        char *tj = cJSON_PrintUnformatted(tags);

        /* properties — EXACT JS key order */
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "query", q);
        cJSON_AddItemToObject(p, "repo",
          full ? cJSON_CreateString(full) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "path",
          path ? cJSON_CreateString(path) : cJSON_CreateNull());
        const cJSON *score = cJSON_GetObjectItem(it, "score");
        cJSON_AddItemToObject(p, "score",
          (score && cJSON_IsNumber(score)) ? cJSON_Duplicate(score, 1)
                                           : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "repo_visibility", vis ? vis : "public");
        const cJSON *stars = (repo && cJSON_IsObject(repo))
                               ? cJSON_GetObjectItem(repo, "stargazers_count")
                               : NULL;
        cJSON_AddItemToObject(p, "repo_stars",
          (stars && cJSON_IsNumber(stars)) ? cJSON_Duplicate(stars, 1)
                                           : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "sha",
          sha ? cJSON_CreateString(sha) : cJSON_CreateNull());
        char *pj = cJSON_PrintUnformatted(p);

        intel_item row = {0};
        row.remote_key     = rk;
        row.title          = title;
        row.summary        = summary;
        row.link           = html;             /* it.html_url || null */
        row.author         = login;            /* repo.owner.login || null */
        row.lang           = "en";
        row.properties_json = pj;
        row.tags_json      = tj;
        if (sink->emit(sink, &row) >= 0) n++;

        free(pj); free(tj);
        cJSON_Delete(p); cJSON_Delete(tags);
      }
    }
    cJSON_Delete(json);
  }
  fprintf(stderr, "[github-leaks-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def github_leaks_jp_def = {
  .id = "github-leaks-jp", .collector = "cyber",
  .name = "GitHub Code Search (JP leaks)",
  .name_ja = "GitHub \xE3\x82\xB3\xE3\x83\xBC\xE3\x83\x89\xE6\xA4\x9C\xE7\xB4\xA2 (JP\xE6\xBC\x8F\xE6\xB4\xA9)",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(github_leaks_jp_def)
