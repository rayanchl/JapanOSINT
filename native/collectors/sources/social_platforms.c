/* collectors/osint/sources/social_platforms.c — per-platform profile lookups,
 * ported from OSINTsaas social_intel.c / social_scanner.c onto the JapanOSINT
 * source ABI. Pivots on a username/handle.
 *
 *   REDDIT_LOOKUP    — Reddit public profile JSON (free)
 *   TWITTER_LOOKUP   — X/Twitter API v2 (requires TWITTER_BEARER_TOKEN)
 *   FACEBOOK_LOOKUP  — Graph API (requires FACEBOOK_ACCESS_TOKEN)
 *   INSTAGRAM_LOOKUP — web profile endpoint (best-effort; often gated)
 *   LINKEDIN_LOOKUP  — no lawful public API → honest-empty
 *
 * Real data only; honest-empty when a platform is auth-gated and no key/usable
 * response is available. */
#include "../../source.h"
#include "social_fuse.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *handle(const char *e) {           /* strip a leading @ */
  return (e && e[0] == '@') ? e + 1 : e;
}
static const char *jstr(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

static int emit(intel_sink *sink, const char *entity, const char *service,
                const char *tags, const char *src, cJSON *data, const char *link) {
  char *bj = cJSON_PrintUnformatted(data);
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", service);
  cJSON_AddStringToObject(props, "entity", entity);
  char *pj = cJSON_PrintUnformatted(props);
  char rk[256], title[256];
  snprintf(rk, sizeof rk, "social:%s:%s", service, entity);
  snprintf(title, sizeof title, "%s — %s", service, entity);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.summary = src;
  it.link = link; it.record_type = "osint_service_result";
  it.properties_json = pj; it.tags_json = tags;
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* ── REDDIT (free public JSON) ──────────────────────────────────────────── */
static int reddit_run(const source_ctx *ctx, intel_sink *sink) {
  const char *u = handle(ctx->entity);
  if (!u || !*u) return 0;
  char url[256]; snprintf(url, sizeof url, "https://www.reddit.com/user/%s/about.json", u);
  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, NULL, NULL, 0, 12000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  if (!j) return 0;
  cJSON *d = cJSON_GetObjectItem(j, "data");
  if (!d) { cJSON_Delete(j); return 0; }
  cJSON *out = cJSON_CreateObject();
  cJSON_AddStringToObject(out, "source", "reddit.com");
  cJSON_AddStringToObject(out, "username", u);
  const char *keepS[] = {"name", "id", NULL};
  for (int i = 0; keepS[i]; i++) { const char *v = jstr(d, keepS[i]); if (v) cJSON_AddStringToObject(out, keepS[i], v); }
  cJSON *tk = cJSON_GetObjectItem(d, "total_karma");
  if (tk && cJSON_IsNumber(tk)) cJSON_AddNumberToObject(out, "total_karma", tk->valuedouble);
  cJSON *cu = cJSON_GetObjectItem(d, "created_utc");
  if (cu && cJSON_IsNumber(cu)) cJSON_AddNumberToObject(out, "created_utc", cu->valuedouble);
  cJSON *sr = cJSON_GetObjectItem(d, "subreddit");
  if (sr) { const char *desc = jstr(sr, "public_description"); if (desc && *desc) cJSON_AddStringToObject(out, "bio", desc); }
  char link[256]; snprintf(link, sizeof link, "https://www.reddit.com/user/%s", u);
  int r = emit(sink, ctx->entity, "REDDIT_LOOKUP", "[\"osint-search\",\"REDDIT_LOOKUP\"]",
               "reddit.com", out, link);
  cJSON_Delete(out); cJSON_Delete(j);
  return r;
}

/* ── TWITTER / X (API v2, key-gated) ────────────────────────────────────── */
static int twitter_run(const source_ctx *ctx, intel_sink *sink) {
  const char *u = handle(ctx->entity);
  if (!u || !*u) return 0;
  const char *tok = getenv("TWITTER_BEARER_TOKEN");
  if (!tok || !*tok) return 0;
  char url[320];
  snprintf(url, sizeof url,
    "https://api.twitter.com/2/users/by/username/%s?user.fields=description,public_metrics,created_at,location,verified", u);
  char auth[320]; snprintf(auth, sizeof auth, "Authorization: Bearer %s", tok);
  const char *headers[] = { auth, NULL };
  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, headers, NULL, 0, 12000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  cJSON *d = j ? cJSON_GetObjectItem(j, "data") : NULL;
  if (!d) { if (j) cJSON_Delete(j); return 0; }
  cJSON_AddStringToObject(d, "source", "api.twitter.com");
  char link[256]; snprintf(link, sizeof link, "https://x.com/%s", u);
  int r = emit(sink, ctx->entity, "TWITTER_LOOKUP", "[\"osint-search\",\"TWITTER_LOOKUP\"]",
               "x.com", d, link);
  cJSON_Delete(j);
  return r;
}

/* ── FACEBOOK (Graph API, key-gated) ────────────────────────────────────── */
static int facebook_run(const source_ctx *ctx, intel_sink *sink) {
  const char *u = handle(ctx->entity);
  if (!u || !*u) return 0;
  const char *tok = getenv("FACEBOOK_ACCESS_TOKEN");
  if (!tok || !*tok) return 0;
  char url[400];
  snprintf(url, sizeof url, "https://graph.facebook.com/v19.0/%s?fields=name,about,link&access_token=%s", u, tok);
  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, NULL, NULL, 0, 12000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  if (!j || cJSON_GetObjectItem(j, "error")) { if (j) cJSON_Delete(j); return 0; }
  cJSON_AddStringToObject(j, "source", "graph.facebook.com");
  int r = emit(sink, ctx->entity, "FACEBOOK_LOOKUP", "[\"osint-search\",\"FACEBOOK_LOOKUP\"]",
               "facebook.com", j, NULL);
  cJSON_Delete(j);
  return r;
}

/* ── INSTAGRAM (web profile endpoint, best-effort) ──────────────────────── */
static int instagram_run(const source_ctx *ctx, intel_sink *sink) {
  const char *u = handle(ctx->entity);
  if (!u || !*u) return 0;
  char url[320];
  snprintf(url, sizeof url,
    "https://i.instagram.com/api/v1/users/web_profile_info/?username=%s", u);
  const char *headers[] = { "X-IG-App-ID: 936619743392459", NULL };
  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, headers, NULL, 0, 12000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  cJSON *data = j ? cJSON_GetObjectItem(j, "data") : NULL;
  cJSON *user = data ? cJSON_GetObjectItem(data, "user") : NULL;
  if (!user) { if (j) cJSON_Delete(j); return 0; }
  cJSON *out = cJSON_CreateObject();
  cJSON_AddStringToObject(out, "source", "instagram.com");
  cJSON_AddStringToObject(out, "username", u);
  const char *full = jstr(user, "full_name"), *bio = jstr(user, "biography");
  if (full && *full) cJSON_AddStringToObject(out, "full_name", full);
  if (bio && *bio) cJSON_AddStringToObject(out, "bio", bio);
  cJSON *ec = cJSON_GetObjectItem(user, "edge_followed_by");
  if (ec) { cJSON *cnt = cJSON_GetObjectItem(ec, "count"); if (cnt && cJSON_IsNumber(cnt)) cJSON_AddNumberToObject(out, "followers", cnt->valuedouble); }
  char link[256]; snprintf(link, sizeof link, "https://www.instagram.com/%s/", u);
  int r = emit(sink, ctx->entity, "INSTAGRAM_LOOKUP", "[\"osint-search\",\"INSTAGRAM_LOOKUP\"]",
               "instagram.com", out, link);
  cJSON_Delete(out); cJSON_Delete(j);
  return r;
}

/* ── LINKEDIN (no lawful public API → honest-empty) ─────────────────────── */
static int linkedin_run(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx; (void)sink;
  return 0;   /* honest-empty: LinkedIn has no public profile API */
}

/* Fused into SOCIAL_USERNAME — the per-platform deep lookups no longer
 * register standalone services. Exposed via social_fuse.h: one pass runs every
 * platform for the handle, key-gated platforms honest-empty without their key. */
int jo_social_platforms_run(const source_ctx *ctx, intel_sink *sink) {
  int t = 0;
  t += reddit_run(ctx, sink);
  t += twitter_run(ctx, sink);
  t += facebook_run(ctx, sink);
  t += instagram_run(ctx, sink);
  t += linkedin_run(ctx, sink);
  return t;
}
