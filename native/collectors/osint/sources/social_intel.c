/* collectors/osint/sources/social_intel.c
 * OSINT service — faithful port of OSINTsaas osint_tools/social_intel.c
 * (handle_social_intel → social_intel_lookup). Canonical SERVICE name in
 * osint_dispatcher.c service_registry[] bound to handle_social_intel is
 * "SOCIAL_INTEL" (line 350; SERVICE_SOCIAL_ANALYZER). On-demand (interval 0);
 * dispatcher runs it with ctx->entity = a username.
 *
 * Reproduces social_intel_lookup() faithfully: username format is validated
 * (≤64, alnum/_/./-); GitHub (api.github.com/users/<u>), Reddit
 * (reddit.com/user/<u>/about.json) and Keybase (keybase.io lookup) are
 * queried via their full JSON APIs (profile + stats + verified identities);
 * the remaining PLATFORMS[] (twitter/instagram/…/telegram) get a simple
 * base-URL GET with the >1000-byte-body heuristic, skipping auth-required
 * platforms whose env var is unset (same env keys). Cross-platform analysis
 * (profile_urls, identity_correlation, presence_level/assessment) and the
 * statistics block are derived identically. Builds the identical root:
 * { username, timestamp, platforms_found:[…], platforms_checked:[…],
 *   statistics:{…}, analysis:{…} }. No API key for the core path. success=
 * true, confidence 75+5*found (cap 95) or 40 if none. Invalid username →
 * upstream returns NULL → here -1. Emits one osint_service_result row. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
  const char *name;
  const char *base_url;
  int requires_auth;
  const char *env_key;
} platform_t;

static const platform_t PLATFORMS[] = {
  {"twitter",    "https://twitter.com/%s",                  1, "TWITTER_BEARER_TOKEN"},
  {"github",     "https://github.com/%s",                   0, NULL},
  {"reddit",     "https://reddit.com/u/%s",                 0, NULL},
  {"instagram",  "https://instagram.com/%s",                1, "INSTAGRAM_TOKEN"},
  {"tiktok",     "https://tiktok.com/@%s",                  1, NULL},
  {"linkedin",   "https://linkedin.com/in/%s",              1, "PROXYCURL_API_KEY"},
  {"facebook",   "https://facebook.com/%s",                 1, NULL},
  {"youtube",    "https://youtube.com/@%s",                 0, NULL},
  {"twitch",     "https://twitch.tv/%s",                    0, NULL},
  {"keybase",    "https://keybase.io/%s",                   0, NULL},
  {"medium",     "https://medium.com/@%s",                  0, NULL},
  {"dev.to",     "https://dev.to/%s",                       0, NULL},
  {"hackernews", "https://news.ycombinator.com/user?id=%s", 0, NULL},
  {"pinterest",  "https://pinterest.com/%s",                0, NULL},
  {"tumblr",     "https://%s.tumblr.com",                   0, NULL},
  {"snapchat",   "https://snapchat.com/add/%s",             0, NULL},
  {"telegram",   "https://t.me/%s",                         0, NULL},
  {"discord",    NULL,                                      0, NULL},
  {NULL, NULL, 0, NULL}
};

static int is_valid_username(const char *u) {
  if (!u || !*u) return 0;
  if (strlen(u) > 64) return 0;
  for (size_t i = 0; u[i]; i++) {
    char c = u[i];
    if (!isalnum((unsigned char)c) && c != '_' && c != '.' && c != '-') return 0;
  }
  return 1;
}

static cJSON *query_github(http_client *http, const char *username) {
  char url[256];
  snprintf(url, sizeof url, "https://api.github.com/users/%s", username);
  const char *headers[] = {
    "Accept: application/vnd.github.v3+json",
    "User-Agent: OSINT-Platform/1.0",
    NULL
  };
  cJSON *json = feed_get_json_h(http, url, headers, 30000);
  if (!json || cJSON_GetObjectItem(json, "message")) { if (json) cJSON_Delete(json); return NULL; }

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "platform", "github");
  cJSON_AddBoolToObject(result, "found", 1);
  cJSON *profile = cJSON_CreateObject();

  cJSON *login = cJSON_GetObjectItem(json, "login");
  cJSON *name = cJSON_GetObjectItem(json, "name");
  cJSON *bio = cJSON_GetObjectItem(json, "bio");
  cJSON *company = cJSON_GetObjectItem(json, "company");
  cJSON *location = cJSON_GetObjectItem(json, "location");
  cJSON *email = cJSON_GetObjectItem(json, "email");
  cJSON *blog = cJSON_GetObjectItem(json, "blog");
  cJSON *avatar = cJSON_GetObjectItem(json, "avatar_url");
  cJSON *followers = cJSON_GetObjectItem(json, "followers");
  cJSON *following = cJSON_GetObjectItem(json, "following");
  cJSON *public_repos = cJSON_GetObjectItem(json, "public_repos");
  cJSON *created_at = cJSON_GetObjectItem(json, "created_at");

  if (login && cJSON_IsString(login)) cJSON_AddStringToObject(profile, "username", login->valuestring);
  if (name && cJSON_IsString(name) && name->valuestring[0]) cJSON_AddStringToObject(profile, "display_name", name->valuestring);
  if (bio && cJSON_IsString(bio) && bio->valuestring[0]) cJSON_AddStringToObject(profile, "bio", bio->valuestring);
  if (company && cJSON_IsString(company) && company->valuestring[0]) cJSON_AddStringToObject(profile, "company", company->valuestring);
  if (location && cJSON_IsString(location) && location->valuestring[0]) cJSON_AddStringToObject(profile, "location", location->valuestring);
  if (email && cJSON_IsString(email) && email->valuestring[0]) cJSON_AddStringToObject(profile, "email", email->valuestring);
  if (blog && cJSON_IsString(blog) && blog->valuestring[0]) cJSON_AddStringToObject(profile, "website", blog->valuestring);
  if (avatar && cJSON_IsString(avatar)) cJSON_AddStringToObject(profile, "avatar_url", avatar->valuestring);
  if (created_at && cJSON_IsString(created_at)) cJSON_AddStringToObject(profile, "account_created", created_at->valuestring);

  cJSON *stats = cJSON_CreateObject();
  if (followers && cJSON_IsNumber(followers)) cJSON_AddNumberToObject(stats, "followers", followers->valueint);
  if (following && cJSON_IsNumber(following)) cJSON_AddNumberToObject(stats, "following", following->valueint);
  if (public_repos && cJSON_IsNumber(public_repos)) cJSON_AddNumberToObject(stats, "public_repos", public_repos->valueint);
  cJSON_AddItemToObject(profile, "stats", stats);

  char profile_url[256];
  snprintf(profile_url, sizeof profile_url, "https://github.com/%s", username);
  cJSON_AddStringToObject(profile, "profile_url", profile_url);
  cJSON_AddItemToObject(result, "profile", profile);
  cJSON_Delete(json);
  return result;
}

static cJSON *query_reddit(http_client *http, const char *username) {
  char url[256];
  snprintf(url, sizeof url, "https://www.reddit.com/user/%s/about.json", username);
  const char *headers[] = {
    "User-Agent: OSINT-Platform/1.0 (Educational Research)",
    NULL
  };
  cJSON *json = feed_get_json_h(http, url, headers, 30000);
  if (!json) return NULL;
  cJSON *data = cJSON_GetObjectItem(json, "data");
  if (!data) { cJSON_Delete(json); return NULL; }

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "platform", "reddit");
  cJSON_AddBoolToObject(result, "found", 1);
  cJSON *profile = cJSON_CreateObject();

  cJSON *name = cJSON_GetObjectItem(data, "name");
  cJSON *created = cJSON_GetObjectItem(data, "created_utc");
  cJSON *link_karma = cJSON_GetObjectItem(data, "link_karma");
  cJSON *comment_karma = cJSON_GetObjectItem(data, "comment_karma");
  cJSON *is_gold = cJSON_GetObjectItem(data, "is_gold");
  cJSON *verified = cJSON_GetObjectItem(data, "verified");
  cJSON *subreddit = cJSON_GetObjectItem(data, "subreddit");

  if (name && cJSON_IsString(name)) cJSON_AddStringToObject(profile, "username", name->valuestring);
  if (created && cJSON_IsNumber(created)) {
    cJSON_AddNumberToObject(profile, "created_timestamp", created->valuedouble);
    time_t ct = (time_t)created->valuedouble;
    char ds[32];
    strftime(ds, sizeof ds, "%Y-%m-%d", gmtime(&ct));
    cJSON_AddStringToObject(profile, "account_created", ds);
  }
  cJSON *stats = cJSON_CreateObject();
  if (link_karma && cJSON_IsNumber(link_karma)) cJSON_AddNumberToObject(stats, "link_karma", link_karma->valueint);
  if (comment_karma && cJSON_IsNumber(comment_karma)) cJSON_AddNumberToObject(stats, "comment_karma", comment_karma->valueint);
  int total_karma = (link_karma ? link_karma->valueint : 0) + (comment_karma ? comment_karma->valueint : 0);
  cJSON_AddNumberToObject(stats, "total_karma", total_karma);
  cJSON_AddItemToObject(profile, "stats", stats);
  if (is_gold && cJSON_IsBool(is_gold)) cJSON_AddBoolToObject(profile, "has_premium", cJSON_IsTrue(is_gold));
  if (verified && cJSON_IsBool(verified)) cJSON_AddBoolToObject(profile, "verified_email", cJSON_IsTrue(verified));
  if (subreddit && cJSON_IsObject(subreddit)) {
    cJSON *title = cJSON_GetObjectItem(subreddit, "title");
    cJSON *pd = cJSON_GetObjectItem(subreddit, "public_description");
    if (title && cJSON_IsString(title) && title->valuestring[0])
      cJSON_AddStringToObject(profile, "display_name", title->valuestring);
    if (pd && cJSON_IsString(pd) && pd->valuestring[0])
      cJSON_AddStringToObject(profile, "bio", pd->valuestring);
  }
  char profile_url[256];
  snprintf(profile_url, sizeof profile_url, "https://reddit.com/u/%s", username);
  cJSON_AddStringToObject(profile, "profile_url", profile_url);
  cJSON_AddItemToObject(result, "profile", profile);
  cJSON_Delete(json);
  return result;
}

static cJSON *query_keybase(http_client *http, const char *username) {
  char url[256];
  snprintf(url, sizeof url,
    "https://keybase.io/_/api/1.0/user/lookup.json?usernames=%s", username);
  cJSON *json = feed_get_json(http, url, 30000);
  if (!json) return NULL;

  cJSON *status = cJSON_GetObjectItem(json, "status");
  cJSON *code = status ? cJSON_GetObjectItem(status, "code") : NULL;
  if (!status || !code || code->valueint != 0) { cJSON_Delete(json); return NULL; }
  cJSON *them = cJSON_GetObjectItem(json, "them");
  if (!them || !cJSON_IsArray(them) || cJSON_GetArraySize(them) == 0) { cJSON_Delete(json); return NULL; }
  cJSON *user = cJSON_GetArrayItem(them, 0);
  if (!user) { cJSON_Delete(json); return NULL; }

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "platform", "keybase");
  cJSON_AddBoolToObject(result, "found", 1);
  cJSON *profile = cJSON_CreateObject();

  cJSON *basics = cJSON_GetObjectItem(user, "basics");
  if (basics) {
    cJSON *ku = cJSON_GetObjectItem(basics, "username");
    if (ku && cJSON_IsString(ku)) cJSON_AddStringToObject(profile, "username", ku->valuestring);
  }
  cJSON *pd = cJSON_GetObjectItem(user, "profile");
  if (pd) {
    cJSON *fn = cJSON_GetObjectItem(pd, "full_name");
    cJSON *loc = cJSON_GetObjectItem(pd, "location");
    cJSON *bio = cJSON_GetObjectItem(pd, "bio");
    if (fn && cJSON_IsString(fn)) cJSON_AddStringToObject(profile, "display_name", fn->valuestring);
    if (loc && cJSON_IsString(loc)) cJSON_AddStringToObject(profile, "location", loc->valuestring);
    if (bio && cJSON_IsString(bio)) cJSON_AddStringToObject(profile, "bio", bio->valuestring);
  }
  cJSON *ps = cJSON_GetObjectItem(user, "proofs_summary");
  if (ps) {
    cJSON *all = cJSON_GetObjectItem(ps, "all");
    if (all && cJSON_IsArray(all)) {
      cJSON *vp = cJSON_CreateArray();
      int pc = cJSON_GetArraySize(all);
      for (int i = 0; i < pc; i++) {
        cJSON *proof = cJSON_GetArrayItem(all, i);
        cJSON *pt = cJSON_GetObjectItem(proof, "proof_type");
        cJSON *nt = cJSON_GetObjectItem(proof, "nametag");
        cJSON *stt = cJSON_GetObjectItem(proof, "state");
        if (pt && nt && stt && cJSON_IsNumber(stt) && stt->valueint == 1 &&
            cJSON_IsString(pt) && cJSON_IsString(nt)) {
          cJSON *v = cJSON_CreateObject();
          cJSON_AddStringToObject(v, "platform", pt->valuestring);
          cJSON_AddStringToObject(v, "username", nt->valuestring);
          cJSON_AddItemToArray(vp, v);
        }
      }
      if (cJSON_GetArraySize(vp) > 0) cJSON_AddItemToObject(profile, "verified_identities", vp);
      else cJSON_Delete(vp);
    }
  }
  char profile_url[256];
  snprintf(profile_url, sizeof profile_url, "https://keybase.io/%s", username);
  cJSON_AddStringToObject(profile, "profile_url", profile_url);
  cJSON_AddItemToObject(result, "profile", profile);
  cJSON_Delete(json);
  return result;
}

/* simple base-URL GET; >1000 bytes ≈ a real profile (upstream heuristic) */
static cJSON *check_platform_simple(http_client *http, const char *username,
                                    const platform_t *p) {
  if (!p->base_url) return NULL;
  char url[256];
  snprintf(url, sizeof url, p->base_url, username);

  char *body = feed_get_text(http, url, 30000);
  if (!body) return NULL;
  int found = strlen(body) > 1000;
  free(body);
  if (!found) return NULL;

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "platform", p->name);
  cJSON_AddBoolToObject(result, "found", 1);
  cJSON *profile = cJSON_CreateObject();
  cJSON_AddStringToObject(profile, "username", username);
  cJSON_AddStringToObject(profile, "profile_url", url);
  cJSON_AddStringToObject(profile, "verification", "url_check_only");
  cJSON_AddItemToObject(result, "profile", profile);
  return result;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *username = ctx->entity;
  if (!username || !*username) return -1;
  if (!is_valid_username(username)) return -1;   /* upstream returns NULL */

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "username", username);
  time_t ts = time(NULL);
  cJSON_AddNumberToObject(root, "timestamp", (double)ts);

  cJSON *found_arr = cJSON_CreateArray();
  cJSON *checked_arr = cJSON_CreateArray();
  int found_count = 0;

  cJSON_AddItemToArray(checked_arr, cJSON_CreateString("github"));
  cJSON *gh = query_github(ctx->http, username);
  if (gh) { cJSON_AddItemToArray(found_arr, gh); found_count++; }

  cJSON_AddItemToArray(checked_arr, cJSON_CreateString("reddit"));
  cJSON *rd = query_reddit(ctx->http, username);
  if (rd) { cJSON_AddItemToArray(found_arr, rd); found_count++; }

  cJSON_AddItemToArray(checked_arr, cJSON_CreateString("keybase"));
  cJSON *kb = query_keybase(ctx->http, username);
  if (kb) { cJSON_AddItemToArray(found_arr, kb); found_count++; }

  for (int i = 0; PLATFORMS[i].name; i++) {
    if (strcmp(PLATFORMS[i].name, "github") == 0 ||
        strcmp(PLATFORMS[i].name, "reddit") == 0 ||
        strcmp(PLATFORMS[i].name, "keybase") == 0) continue;
    if (PLATFORMS[i].requires_auth && PLATFORMS[i].env_key) {
      if (!getenv(PLATFORMS[i].env_key)) continue;
    }
    cJSON_AddItemToArray(checked_arr, cJSON_CreateString(PLATFORMS[i].name));
    cJSON *pr = check_platform_simple(ctx->http, username, &PLATFORMS[i]);
    if (pr) { cJSON_AddItemToArray(found_arr, pr); found_count++; }
  }

  cJSON_AddItemToObject(root, "platforms_found", found_arr);
  cJSON_AddItemToObject(root, "platforms_checked", checked_arr);

  cJSON *stats = cJSON_CreateObject();
  int checked_n = cJSON_GetArraySize(checked_arr);
  cJSON_AddNumberToObject(stats, "profiles_found", found_count);
  cJSON_AddNumberToObject(stats, "platforms_checked", checked_n);
  cJSON_AddNumberToObject(stats, "match_rate",
    checked_n > 0 ? (found_count * 100.0 / checked_n) : 0);
  cJSON_AddItemToObject(root, "statistics", stats);

  cJSON *analysis = cJSON_CreateObject();
  cJSON *all_urls = cJSON_CreateArray();
  for (int i = 0; i < cJSON_GetArraySize(found_arr); i++) {
    cJSON *pf = cJSON_GetArrayItem(found_arr, i);
    cJSON *prof = cJSON_GetObjectItem(pf, "profile");
    if (prof) {
      cJSON *u = cJSON_GetObjectItem(prof, "profile_url");
      if (u && cJSON_IsString(u)) cJSON_AddItemToArray(all_urls, cJSON_CreateString(u->valuestring));
    }
  }
  cJSON_AddItemToObject(analysis, "profile_urls", all_urls);

  cJSON *consistent = cJSON_CreateObject();
  const char *found_name = NULL, *found_location = NULL;
  for (int i = 0; i < cJSON_GetArraySize(found_arr); i++) {
    cJSON *pf = cJSON_GetArrayItem(found_arr, i);
    cJSON *prof = cJSON_GetObjectItem(pf, "profile");
    if (prof) {
      cJSON *nm = cJSON_GetObjectItem(prof, "display_name");
      cJSON *loc = cJSON_GetObjectItem(prof, "location");
      if (nm && cJSON_IsString(nm) && !found_name) {
        found_name = nm->valuestring;
        cJSON_AddStringToObject(consistent, "display_name", found_name);
      }
      if (loc && cJSON_IsString(loc) && !found_location) {
        found_location = loc->valuestring;
        cJSON_AddStringToObject(consistent, "location", found_location);
      }
    }
  }
  if (cJSON_GetObjectItem(consistent, "display_name") ||
      cJSON_GetObjectItem(consistent, "location"))
    cJSON_AddItemToObject(analysis, "identity_correlation", consistent);
  else
    cJSON_Delete(consistent);

  if (found_count >= 5) {
    cJSON_AddStringToObject(analysis, "presence_level", "high");
    cJSON_AddStringToObject(analysis, "assessment", "Strong social media presence - likely real identity");
  } else if (found_count >= 2) {
    cJSON_AddStringToObject(analysis, "presence_level", "medium");
    cJSON_AddStringToObject(analysis, "assessment", "Moderate social media presence");
  } else if (found_count == 1) {
    cJSON_AddStringToObject(analysis, "presence_level", "low");
    cJSON_AddStringToObject(analysis, "assessment", "Limited social media presence");
  } else {
    cJSON_AddStringToObject(analysis, "presence_level", "none");
    cJSON_AddStringToObject(analysis, "assessment", "No social media presence found with this username");
  }
  cJSON_AddItemToObject(root, "analysis", analysis);

  int confidence = found_count > 0 ? 75 + (found_count * 5) : 40;
  if (confidence > 95) confidence = 95;

  char *bj = cJSON_PrintUnformatted(root);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SOCIAL_INTEL");
  cJSON_AddStringToObject(props, "entity", username);
  cJSON_AddBoolToObject(props, "success", 1);          /* upstream success=true */
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "socialintel:%s", username);
  char title[320];
  snprintf(title, sizeof title, "SOCIAL_INTEL — %s", username);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = found_count > 0 ? "social profiles found" : "no profiles found";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SOCIAL_INTEL\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  cJSON_Delete(root);
  return rc >= 0 ? 0 : -1;
}

static const source_def social_intel_def = {
  .id = "SOCIAL_INTEL", .collector = "osint",
  .name = "Social Intelligence", .name_ja = "ソーシャルインテリジェンス",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(social_intel_def)
