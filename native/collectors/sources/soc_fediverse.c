/* soc_fediverse.c — census + firehose collectors for the open social web
 * (ActivityPub / Lemmy / Misskey / PeerTube / Invidious). Nine scheduled
 * sources, all keyless, all emitting ONLY fields returned by the upstream.
 * No source here returns coordinates, so has_geo stays 0 everywhere (R2) —
 * fedidb's location.city / peertube's country are strings, not geometry.
 *
 *  1. fediverse-mastodon-servers  GET https://api.joinmastodon.org/servers
 *       emits: domain, version, description, region, languages[], categories[],
 *              total_users, last_week_users, approval_required
 *       licence: public unauthenticated joinmastodon.org endpoint; attribute
 *                joinmastodon.org.
 *  2. fedidb-servers              GET https://api.fedidb.org/v1/servers?limit=40
 *       emits: domain, software.name/version, stats.{status,user,mau} counts,
 *              location.city/country, open_registration, description
 *       licence: keyless public API (fedidb.org/docs), free tier, rate limited.
 *  3. fedidb-software             GET https://api.fedidb.org/v1/software?limit=40
 *       emits: name, license, website, source_repo, instance_count, user_count,
 *              status_count, monthly_active_users, latest_version{version,
 *              published_at}
 *       licence: keyless public API, fedidb.org.
 *  4. mastodon-trending-links     GET https://mastodon.social/api/v1/trends/links
 *       emits: url, title, description, provider_name, author_name, language,
 *              published_at, history accounts/uses totals
 *       licence: public Mastodon REST API; trends unauthenticated on
 *                mastodon.social.
 *  5. lemmy-world-firehose        GET https://lemmy.world/api/v3/post/list
 *       emits: post.name/url/ap_id/published/nsfw, creator.name/actor_id,
 *              community.name/title, counts.{score,comments}
 *       licence: AGPL Lemmy HTTP API serving public community data, keyless.
 *  6. lemmyverse-instances        GET https://data.lemmyverse.net/data/instance.min.json
 *       emits: name, base (hostname), score
 *       licence: static JSON published by the Lemmyverse crawler for reuse.
 *  7. misskey-instance-directory  GET https://instanceapp.misskey.page/instances.json
 *       emits: instancesInfos[].url/langs/value, meta.version/maintainerName/
 *              name/description; plus network stats in each row's props
 *       licence: public JSON served by the Misskey instance-list app, keyless.
 *  8. peertube-instance-directory GET https://instances.joinpeertube.org/api/v1/instances
 *       emits: host, name, shortDescription, version, totalUsers, totalVideos,
 *              totalLocalVideos, country, health, signupAllowed, supportsIPv6
 *       licence: public API of the Framasoft-run index, keyless, documented.
 *  9. invidious-instances         GET https://api.invidious.io/instances.json
 *       emits: host, region, type, uri, api, cors, version, users total,
 *              monitor uptime / down
 *       licence: public directory of the AGPL Invidious project, keyless.
 */
#include "lib/osintemit.h"
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const SOC_UA[] = {
  "User-Agent: JapanOSINT-native/1.0 (OSINT research collector; +https://github.com/)",
  "Accept: application/json", NULL
};

/* ---- tiny helpers (static: private to this translation unit) ------------ */

/* ---- 1. joinmastodon.org server directory ------------------------------ */

static int run_mastodon_servers(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http, "https://api.joinmastodon.org/servers",
                               SOC_UA, 25000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[fediverse-mastodon-servers] fetch/parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *it;
  cJSON_ArrayForEach(it, doc) {
    const char *domain = jo_sv(it, "domain");
    if (!domain) continue;
    const char *ver = jo_sv(it, "version");
    const cJSON *tu = cJSON_GetObjectItem(it, "total_users");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "joinmastodon.org");
    cJSON_AddStringToObject(p, "software", "Mastodon");
    jcopy(p, "domain", it, "domain");
    jcopy(p, "version", it, "version");
    jcopy(p, "description", it, "description");
    jcopy(p, "region", it, "region");
    jcopy(p, "languages", it, "languages");
    jcopy(p, "categories", it, "categories");
    jcopy(p, "total_users", it, "total_users");
    jcopy(p, "last_week_users", it, "last_week_users");
    jcopy(p, "approval_required", it, "approval_required");
    char summary[160], link[300];
    snprintf(summary, sizeof summary, "Mastodon %s · %ld users",
             ver ? ver : "(version n/a)",
             cJSON_IsNumber(tu) ? (long)tu->valuedouble : 0L);
    snprintf(link, sizeof link, "https://%s/", domain);
    n += soc_emit(sink, p, "fediverse-server",
                  "[\"fediverse\",\"mastodon\",\"instance\"]",
                  domain, domain, summary, link, NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[fediverse-mastodon-servers] emitted %d\n", n);
  return 0;
}

static const source_def soc_mastodon_servers_def = {
  .id = "fediverse-mastodon-servers", .collector = "social",
  .name = "joinmastodon.org Server Directory",
  .update_interval_sec = 86400, .run = run_mastodon_servers,
  .category = "social", .type = "api",
  .url = "https://api.joinmastodon.org/servers",
  .description = "Vetted public Mastodon instances with software version, total/weekly-active user counts, languages, region and topical categories.",
  .license = "Public unauthenticated joinmastodon.org endpoint; attribute joinmastodon.org.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_mastodon_servers_def)

/* ---- 2. FediDB server census ------------------------------------------- */

static int run_fedidb_servers(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
                               "https://api.fedidb.org/v1/servers?limit=40",
                               SOC_UA, 25000);
  if (!doc) { fprintf(stderr, "[fedidb-servers] fetch failed\n"); return -1; }
  const cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(arr)) {
    fprintf(stderr, "[fedidb-servers] unexpected shape\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *it;
  cJSON_ArrayForEach(it, arr) {
    const char *domain = jo_sv(it, "domain");
    if (!domain) continue;
    const cJSON *sw = cJSON_GetObjectItem(it, "software");
    const cJSON *st = cJSON_GetObjectItem(it, "stats");
    const cJSON *lo = cJSON_GetObjectItem(it, "location");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "fedidb.org");
    jcopy(p, "domain", it, "domain");
    jcopy(p, "description", it, "description");
    jcopy(p, "open_registration", it, "open_registration");
    if (cJSON_IsObject(sw)) {
      jcopy(p, "software", sw, "name");
      jcopy(p, "software_version", sw, "version");
    }
    if (cJSON_IsObject(st)) {
      jcopy(p, "status_count", st, "status_count");
      jcopy(p, "user_count", st, "user_count");
      jcopy(p, "monthly_active_users", st, "monthly_active_users");
    }
    /* location is city/country STRINGS only — never coordinates (R2). */
    if (cJSON_IsObject(lo)) {
      jcopy(p, "city", lo, "city");
      jcopy(p, "country", lo, "country");
    }
    const char *swn = cJSON_IsObject(sw) ? jo_sv(sw, "name") : NULL;
    const char *swv = cJSON_IsObject(sw) ? jo_sv(sw, "version") : NULL;
    const cJSON *uc = cJSON_IsObject(st) ? cJSON_GetObjectItem(st, "user_count") : NULL;
    char summary[200], link[300];
    snprintf(summary, sizeof summary, "%s %s · %ld users",
             swn ? swn : "(software n/a)", swv ? swv : "",
             cJSON_IsNumber(uc) ? (long)uc->valuedouble : 0L);
    snprintf(link, sizeof link, "https://%s/", domain);
    n += soc_emit(sink, p, "fediverse-server",
                  "[\"fediverse\",\"instance\",\"census\"]",
                  domain, domain, summary, link, NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[fedidb-servers] emitted %d\n", n);
  return 0;
}

static const source_def soc_fedidb_servers_def = {
  .id = "fedidb-servers", .collector = "social",
  .name = "FediDB Fediverse Server Census",
  .update_interval_sec = 86400, .run = run_fedidb_servers,
  .category = "social", .type = "api",
  .url = "https://api.fedidb.org/v1/servers",
  .description = "Cross-software fediverse census: per-instance software fingerprint, version, hosting city/country and post/user/MAU statistics.",
  .license = "Keyless public API documented at fedidb.org/docs; free tier, rate limited.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_fedidb_servers_def)

/* ---- 3. FediDB software population ------------------------------------- */

static int run_fedidb_software(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
                               "https://api.fedidb.org/v1/software?limit=40",
                               SOC_UA, 25000);
  if (!doc) { fprintf(stderr, "[fedidb-software] fetch failed\n"); return -1; }
  const cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(arr)) {
    fprintf(stderr, "[fedidb-software] unexpected shape\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *it;
  cJSON_ArrayForEach(it, arr) {
    const char *name = jo_sv(it, "name");
    if (!name) continue;
    /* latest_version is an object and is null on small projects — guard. */
    const cJSON *lv = cJSON_GetObjectItem(it, "latest_version");
    const char *lvv = cJSON_IsObject(lv) ? jo_sv(lv, "version") : NULL;
    const char *lvp = cJSON_IsObject(lv) ? jo_sv(lv, "published_at") : NULL;
    const cJSON *ic = cJSON_GetObjectItem(it, "instance_count");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "fedidb.org");
    jcopy(p, "name", it, "name");
    jcopy(p, "license", it, "license");
    jcopy(p, "website", it, "website");
    jcopy(p, "source_repo", it, "source_repo");
    jcopy(p, "description", it, "description");
    jcopy(p, "instance_count", it, "instance_count");
    jcopy(p, "user_count", it, "user_count");
    jcopy(p, "status_count", it, "status_count");
    jcopy(p, "monthly_active_users", it, "monthly_active_users");
    if (lvv) cJSON_AddStringToObject(p, "latest_version", lvv);
    if (lvp) cJSON_AddStringToObject(p, "latest_version_published_at", lvp);
    char summary[200];
    snprintf(summary, sizeof summary, "%ld instances%s%s",
             cJSON_IsNumber(ic) ? (long)ic->valuedouble : 0L,
             lvv ? " · latest " : "", lvv ? lvv : "");
    n += soc_emit(sink, p, "fediverse-software",
                  "[\"fediverse\",\"software\",\"version\"]",
                  name, name, summary, jo_sv(it, "website"), lvp, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[fedidb-software] emitted %d\n", n);
  return 0;
}

static const source_def soc_fedidb_software_def = {
  .id = "fedidb-software", .collector = "social",
  .name = "FediDB Fediverse Software Population",
  .update_interval_sec = 86400, .run = run_fedidb_software,
  .category = "social", .type = "api",
  .url = "https://api.fedidb.org/v1/software",
  .description = "Per-software rollup of the fediverse: instances and users per server implementation plus the current upstream release and its publication date (version-drift analysis).",
  .license = "Keyless public API, fedidb.org, free tier.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_fedidb_software_def)

/* ---- 4. mastodon.social trending links --------------------------------- */

static int run_mastodon_trends(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://mastodon.social/api/v1/trends/links?limit=20", SOC_UA, 25000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[mastodon-trending-links] fetch/parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *it;
  cJSON_ArrayForEach(it, doc) {
    const char *url = jo_sv(it, "url");
    const char *title = jo_sv(it, "title");
    if (!url || !title) continue;
    /* history[] accounts/uses come back as STRINGS, not numbers. */
    long accounts = 0, uses = 0;
    const cJSON *hist = cJSON_GetObjectItem(it, "history"), *h;
    if (cJSON_IsArray(hist)) cJSON_ArrayForEach(h, hist) {
      const char *a = jo_sv(h, "accounts"), *u = jo_sv(h, "uses");
      const cJSON *an = cJSON_GetObjectItem(h, "accounts");
      const cJSON *un = cJSON_GetObjectItem(h, "uses");
      accounts += a ? atol(a) : (cJSON_IsNumber(an) ? (long)an->valuedouble : 0);
      uses     += u ? atol(u) : (cJSON_IsNumber(un) ? (long)un->valuedouble : 0);
    }
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "mastodon.social");
    jcopy(p, "url", it, "url");
    jcopy(p, "title", it, "title");
    jcopy(p, "description", it, "description");
    jcopy(p, "provider_name", it, "provider_name");
    jcopy(p, "author_name", it, "author_name");
    jcopy(p, "language", it, "language");
    jcopy(p, "published_at", it, "published_at");
    cJSON_AddNumberToObject(p, "history_accounts", (double)accounts);
    cJSON_AddNumberToObject(p, "history_uses", (double)uses);
    char summary[256];
    snprintf(summary, sizeof summary, "%s · %ld accounts / %ld shares",
             jo_sv(it, "provider_name") ? jo_sv(it, "provider_name") : "(publisher n/a)",
             accounts, uses);
    n += soc_emit(sink, p, "trending-link",
                  "[\"mastodon\",\"trending\",\"news\"]",
                  url, title, summary, url, jo_sv(it, "published_at"),
                  jo_sv(it, "language"));
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[mastodon-trending-links] emitted %d\n", n);
  return 0;
}

static const source_def soc_mastodon_trends_def = {
  .id = "mastodon-trending-links", .collector = "social",
  .name = "Mastodon Trending Links (mastodon.social)",
  .update_interval_sec = 1800, .run = run_mastodon_trends,
  .category = "media", .type = "api",
  .url = "https://mastodon.social/api/v1/trends/links",
  .description = "External news URLs the largest Mastodon instance is amplifying, ranked by distinct sharing accounts with a per-day share history.",
  .license = "Public Mastodon REST API; trends unauthenticated on mastodon.social.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_mastodon_trends_def)

/* ---- 5. lemmy.world active post firehose -------------------------------- */

static int lemmy_emit(intel_sink *sink, const cJSON *arr) {
  int n = 0;
  const cJSON *it;
  cJSON_ArrayForEach(it, arr) {
    const cJSON *post = cJSON_GetObjectItem(it, "post");
    if (!cJSON_IsObject(post)) continue;
    const char *name = jo_sv(post, "name");
    const char *apid = jo_sv(post, "ap_id");
    if (!name || !apid) continue;
    const cJSON *cr = cJSON_GetObjectItem(it, "creator");
    const cJSON *co = cJSON_GetObjectItem(it, "community");
    const cJSON *cn = cJSON_GetObjectItem(it, "counts");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "lemmy.world");
    jcopy(p, "title", post, "name");
    jcopy(p, "url", post, "url");
    jcopy(p, "ap_id", post, "ap_id");
    jcopy(p, "published", post, "published");
    jcopy(p, "nsfw", post, "nsfw");
    jcopy(p, "body", post, "body");
    if (cJSON_IsObject(cr)) {
      jcopy(p, "creator", cr, "name");
      jcopy(p, "creator_actor_id", cr, "actor_id");
    }
    if (cJSON_IsObject(co)) {
      jcopy(p, "community", co, "name");
      jcopy(p, "community_title", co, "title");
      jcopy(p, "community_actor_id", co, "actor_id");
    }
    if (cJSON_IsObject(cn)) {
      jcopy(p, "score", cn, "score");
      jcopy(p, "comments", cn, "comments");
    }
    char summary[300];
    snprintf(summary, sizeof summary, "!%s by %s",
             (cJSON_IsObject(co) && jo_sv(co, "name")) ? jo_sv(co, "name") : "?",
             (cJSON_IsObject(cr) && jo_sv(cr, "name")) ? jo_sv(cr, "name") : "?");
    n += soc_emit(sink, p, "lemmy-post",
                  "[\"fediverse\",\"lemmy\",\"post\"]",
                  apid, name, summary, apid, jo_sv(post, "published"), NULL);
  }
  return n;
}

static int run_lemmy_firehose(const source_ctx *ctx, intel_sink *sink) {
  /* Lemmy 1.0 renumbers this API to /api/v4 — pin v3, fall back to v4. */
  static const char *const urls[] = {
    "https://lemmy.world/api/v3/post/list?type_=All&sort=Active&limit=20",
    "https://lemmy.world/api/v4/post/list?type_=All&sort=Active&limit=20",
  };
  for (int i = 0; i < 2; i++) {
    cJSON *doc = feed_get_json_h(ctx->http, urls[i], SOC_UA, 25000);
    if (!doc) continue;
    const cJSON *arr = cJSON_GetObjectItem(doc, "posts");
    if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); continue; }
    int n = lemmy_emit(sink, arr);
    cJSON_Delete(doc);
    fprintf(stderr, "[lemmy-world-firehose] emitted %d (api v%d)\n", n, i + 3);
    return 0;
  }
  fprintf(stderr, "[lemmy-world-firehose] fetch failed (v3 and v4)\n");
  return -1;
}

static const source_def soc_lemmy_firehose_def = {
  .id = "lemmy-world-firehose", .collector = "social",
  .name = "Lemmy (lemmy.world) Active Post Firehose",
  .update_interval_sec = 900, .run = run_lemmy_firehose,
  .category = "social", .type = "api",
  .url = "https://lemmy.world/api/v3/post/list",
  .description = "Federated link-aggregator firehose from the largest Lemmy instance, carrying local and federated posts with creator, community and score.",
  .license = "AGPL Lemmy HTTP API serving public community data unauthenticated.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_lemmy_firehose_def)

/* ---- 6. Lemmyverse instance index -------------------------------------- */

static int run_lemmyverse(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://data.lemmyverse.net/data/instance.min.json", SOC_UA, 40000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[lemmyverse-instances] fetch/parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *it;
  cJSON_ArrayForEach(it, doc) {
    if (n >= 600) break;
    const char *base = jo_sv(it, "base");
    if (!base) continue;
    const char *name = jo_sv(it, "name");
    const cJSON *sc = cJSON_GetObjectItem(it, "score");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "data.lemmyverse.net");
    jcopy(p, "name", it, "name");
    jcopy(p, "base", it, "base");
    jcopy(p, "score", it, "score");
    char summary[128], link[300];
    snprintf(summary, sizeof summary, "Lemmy instance · score %.2f",
             cJSON_IsNumber(sc) ? sc->valuedouble : 0.0);
    snprintf(link, sizeof link, "https://%s/", base);
    n += soc_emit(sink, p, "fediverse-server",
                  "[\"fediverse\",\"lemmy\",\"instance\"]",
                  base, name ? name : base, summary, link, NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[lemmyverse-instances] emitted %d\n", n);
  return 0;
}

static const source_def soc_lemmyverse_def = {
  .id = "lemmyverse-instances", .collector = "social",
  .name = "Lemmyverse Instance Index",
  .update_interval_sec = 86400, .run = run_lemmyverse,
  .category = "social", .type = "dataset",
  .url = "https://data.lemmyverse.net/data/instance.min.json",
  .description = "Every crawled Lemmy instance with display name, hostname and activity score — the fan-out list for pivoting beyond lemmy.world.",
  .license = "Static JSON published by the Lemmyverse crawler for reuse; keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_lemmyverse_def)

/* ---- 7. Misskey instance directory -------------------------------------- */

static int run_misskey_dir(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://instanceapp.misskey.page/instances.json", SOC_UA, 40000);
  if (!doc) { fprintf(stderr, "[misskey-instance-directory] fetch failed\n"); return -1; }
  const cJSON *arr = cJSON_GetObjectItem(doc, "instancesInfos");
  if (!cJSON_IsArray(arr)) {
    fprintf(stderr, "[misskey-instance-directory] unexpected shape\n");
    cJSON_Delete(doc); return -1;
  }
  const cJSON *stats = cJSON_GetObjectItem(doc, "stats");
  const char *latest = jo_sv(doc, "latestMisskeyVersion");
  int n = 0;
  const cJSON *it;
  cJSON_ArrayForEach(it, arr) {
    if (n >= 600) break;
    const char *host = jo_sv(it, "url");
    if (!host) continue;
    /* Some entries carry a null meta — guard before touching it. */
    const cJSON *meta = cJSON_GetObjectItem(it, "meta");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "instanceapp.misskey.page");
    cJSON_AddStringToObject(p, "software", "Misskey");
    jcopy(p, "host", it, "url");
    jcopy(p, "langs", it, "langs");
    jcopy(p, "value", it, "value");
    jcopy(p, "isAlive", it, "isAlive");
    const char *ver = NULL, *maint = NULL, *iname = NULL;
    if (cJSON_IsObject(meta)) {
      ver = jo_sv(meta, "version"); maint = jo_sv(meta, "maintainerName");
      iname = jo_sv(meta, "name");
      jcopy(p, "version", meta, "version");
      jcopy(p, "maintainer_name", meta, "maintainerName");
      jcopy(p, "instance_name", meta, "name");
      jcopy(p, "instance_description", meta, "description");
      jcopy(p, "disable_registration", meta, "disableRegistration");
    }
    if (latest) cJSON_AddStringToObject(p, "network_latest_version", latest);
    if (cJSON_IsObject(stats)) {
      jcopy(p, "network_notes_count", stats, "notesCount");
      jcopy(p, "network_users_count", stats, "usersCount");
      jcopy(p, "network_instances_count", stats, "instancesCount");
    }
    char summary[256], link[300];
    snprintf(summary, sizeof summary, "Misskey %s%s%s",
             ver ? ver : "(version n/a)",
             maint ? " · maintainer " : "", maint ? maint : "");
    snprintf(link, sizeof link, "https://%s/", host);
    n += soc_emit(sink, p, "fediverse-server",
                  "[\"fediverse\",\"misskey\",\"instance\"]",
                  host, iname ? iname : host, summary, link, NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[misskey-instance-directory] emitted %d\n", n);
  return 0;
}

static const source_def soc_misskey_dir_def = {
  .id = "misskey-instance-directory", .collector = "social",
  .name = "Misskey Instance Directory",
  .update_interval_sec = 86400, .run = run_misskey_dir,
  .category = "social", .type = "api",
  .url = "https://instanceapp.misskey.page/instances.json",
  .description = "~900 Misskey servers with per-instance version, declared languages, maintainer handle and activity value, plus network-wide note/user counts.",
  .license = "Public JSON served by the Misskey instance-list app; keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_misskey_dir_def)

/* ---- 8. PeerTube instance directory ------------------------------------- */

static int run_peertube_dir(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://instances.joinpeertube.org/api/v1/instances?count=100",
      SOC_UA, 30000);
  if (!doc) { fprintf(stderr, "[peertube-instance-directory] fetch failed\n"); return -1; }
  const cJSON *arr = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(arr)) {
    fprintf(stderr, "[peertube-instance-directory] unexpected shape\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *it;
  cJSON_ArrayForEach(it, arr) {
    const char *host = jo_sv(it, "host");
    if (!host) continue;
    const cJSON *tv = cJSON_GetObjectItem(it, "totalVideos");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "instances.joinpeertube.org");
    cJSON_AddStringToObject(p, "software", "PeerTube");
    jcopy(p, "host", it, "host");
    jcopy(p, "name", it, "name");
    jcopy(p, "short_description", it, "shortDescription");
    jcopy(p, "version", it, "version");
    jcopy(p, "total_users", it, "totalUsers");
    jcopy(p, "total_videos", it, "totalVideos");
    jcopy(p, "total_local_videos", it, "totalLocalVideos");
    jcopy(p, "country", it, "country");     /* ISO2 string, not geometry (R2) */
    jcopy(p, "health", it, "health");
    jcopy(p, "signup_allowed", it, "signupAllowed");
    jcopy(p, "supports_ipv6", it, "supportsIPv6");
    jcopy(p, "categories", it, "categories");
    jcopy(p, "languages", it, "languages");
    char summary[220], link[300];
    snprintf(summary, sizeof summary, "PeerTube %s · %ld videos · %s",
             jo_sv(it, "version") ? jo_sv(it, "version") : "(version n/a)",
             cJSON_IsNumber(tv) ? (long)tv->valuedouble : 0L,
             jo_sv(it, "country") ? jo_sv(it, "country") : "??");
    snprintf(link, sizeof link, "https://%s/", host);
    n += soc_emit(sink, p, "fediverse-server",
                  "[\"fediverse\",\"peertube\",\"video\",\"instance\"]",
                  host, jo_sv(it, "name") ? jo_sv(it, "name") : host,
                  summary, link, NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[peertube-instance-directory] emitted %d\n", n);
  return 0;
}

static const source_def soc_peertube_dir_def = {
  .id = "peertube-instance-directory", .collector = "social",
  .name = "PeerTube Instance Directory",
  .update_interval_sec = 86400, .run = run_peertube_dir,
  .category = "media", .type = "api",
  .url = "https://instances.joinpeertube.org/api/v1/instances",
  .description = "Registry of federated PeerTube video servers with software version, catalogue size, user count, declared country and health score.",
  .license = "Public API of the Framasoft-run instance index; keyless, documented.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_peertube_dir_def)

/* ---- 9. Invidious public instance directory ----------------------------- */

static int run_invidious(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http, "https://api.invidious.io/instances.json",
                               SOC_UA, 25000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[invidious-instances] fetch/parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *pair;
  /* Array of [hostname, details] PAIRS, not objects. */
  cJSON_ArrayForEach(pair, doc) {
    if (!cJSON_IsArray(pair) || cJSON_GetArraySize(pair) < 2) continue;
    const cJSON *hostv = cJSON_GetArrayItem(pair, 0);
    const cJSON *d     = cJSON_GetArrayItem(pair, 1);
    if (!cJSON_IsString(hostv) || !hostv->valuestring || !cJSON_IsObject(d)) continue;
    const char *host = hostv->valuestring;
    const cJSON *st = cJSON_GetObjectItem(d, "stats");
    const cJSON *mon = cJSON_GetObjectItem(d, "monitor");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "api.invidious.io");
    cJSON_AddStringToObject(p, "host", host);
    jcopy(p, "flag", d, "flag");
    jcopy(p, "region", d, "region");
    jcopy(p, "type", d, "type");
    jcopy(p, "uri", d, "uri");
    jcopy(p, "api", d, "api");     /* honour this flag before using the host */
    jcopy(p, "cors", d, "cors");
    const char *ver = NULL;
    if (cJSON_IsObject(st)) {
      const cJSON *sw = cJSON_GetObjectItem(st, "software");
      const cJSON *us = cJSON_GetObjectItem(st, "usage");
      if (cJSON_IsObject(sw)) { ver = jo_sv(sw, "version");
                                jcopy(p, "version", sw, "version"); }
      if (cJSON_IsObject(us)) {
        const cJSON *u = cJSON_GetObjectItem(us, "users");
        if (cJSON_IsObject(u)) {
          jcopy(p, "users_total", u, "total");
          jcopy(p, "users_active_halfyear", u, "activeHalfyear");
        }
      }
    }
    if (cJSON_IsObject(mon)) {
      jcopy(p, "uptime", mon, "uptime");
      jcopy(p, "down", mon, "down");
    }
    char summary[220];
    snprintf(summary, sizeof summary, "Invidious %s · region %s%s",
             ver ? ver : "(version n/a)",
             jo_sv(d, "region") ? jo_sv(d, "region") : "??",
             (cJSON_IsObject(mon) && cJSON_IsTrue(cJSON_GetObjectItem(mon, "down")))
               ? " · DOWN" : "");
    n += soc_emit(sink, p, "invidious-instance",
                  "[\"invidious\",\"youtube-proxy\",\"instance\"]",
                  host, host, summary, jo_sv(d, "uri"), NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[invidious-instances] emitted %d\n", n);
  return 0;
}

static const source_def soc_invidious_def = {
  .id = "invidious-instances", .collector = "social",
  .name = "Invidious Public Instance Directory",
  .update_interval_sec = 86400, .run = run_invidious,
  .category = "media", .type = "api",
  .url = "https://api.invidious.io/instances.json",
  .description = "Live roster of public Invidious servers with per-host uptime, region, software version and whether the JSON API is enabled — the keyless fallback path for YouTube metadata.",
  .license = "Public directory of the AGPL Invidious project; keyless. Honour each instance's own 'api' flag.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_invidious_def)
