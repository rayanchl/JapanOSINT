/* soc_video_search.c — four on-demand video / broadcast lookups (entity pivots).
 * All keyless, all emitting only fields the upstream returned; none of these
 * APIs returns coordinates, so has_geo stays 0 (R2).
 *
 *  SEPIASEARCH_PEERTUBE (keyword)
 *      GET https://sepiasearch.org/api/v1/search/videos?search=&count=20
 *      envelope {total, data[]}
 *      emits: uuid, shortUUID, name, description, duration, publishedAt,
 *             originallyPublishedAt, thumbnailUrl, url, account.name,
 *             channel.name/host (which PeerTube server actually holds the
 *             file), views
 *      licence: public keyless API run by Framasoft over public federated
 *               video metadata.
 *  TVMAZE_SHOW_SEARCH (keyword)
 *      GET https://api.tvmaze.com/search/shows?q=  — bare array of {score, show}
 *      emits: show id, name, type, language, genres, status, premiered, ended,
 *             network name + country (or webChannel when network is null),
 *             officialSite, externals.imdb/thetvdb/tvrage, summary
 *      LICENCE RESTRICTION: TVmaze is free and keyless for NON-COMMERCIAL use
 *      (CC BY-SA data); commercial use requires their paid tier.
 *  DAILYMOTION_SEARCH (keyword)
 *      GET https://api.dailymotion.com/videos?search=&fields=...&limit=20
 *      TRAP: you MUST pass fields= or the API returns id/title only, and the
 *      dotted field names come back as LITERAL keys ("owner.screenname").
 *      envelope {page, limit, total, has_more, list[]}
 *      emits: id, title, created_time (→ ISO), owner.screenname, url,
 *             duration, views_total
 *      licence: Dailymotion Graph API; public read of public videos is
 *               unauthenticated, rate limited per IP.
 *  ODYSEE_SEARCH (keyword)
 *      GET https://lighthouse.odysee.tv/search?s=&size=20 — bare array
 *      emits: name, claimId, channel_claim_id (names starting with @ are
 *             channels, the rest individual claims). The permanent claim ID
 *             keeps a channel trackable across display-name changes.
 *      licence: Odysee's public Lighthouse search service; keyless,
 *               open-source (LBRY). Metadata only.
 */
#include "../../lib/osintemit.h"
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *const SOC_UA[] = {
  "User-Agent: JapanOSINT-native/1.0 (OSINT research collector; +https://github.com/)",
  "Accept: application/json", NULL
};

/* ---- SEPIASEARCH_PEERTUBE ----------------------------------------------- */

static int run_sepia(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[400];
  snprintf(url, sizeof url,
           "https://sepiasearch.org/api/v1/search/videos?search=%s&count=20", enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 25000);
  if (!doc) { fprintf(stderr, "[SEPIASEARCH_PEERTUBE] no response for %s\n", q); return 0; }
  const cJSON *arr = cJSON_GetObjectItem(doc, "data");
  const cJSON *tot = cJSON_GetObjectItem(doc, "total");
  int n = 0;
  const cJSON *v;
  if (cJSON_IsArray(arr)) cJSON_ArrayForEach(v, arr) {
    const char *uuid = jo_sv(v, "uuid");
    const char *name = jo_sv(v, "name");
    if (!uuid || !name) continue;
    const cJSON *acct = cJSON_GetObjectItem(v, "account");
    const cJSON *chan = cJSON_GetObjectItem(v, "channel");
    const cJSON *dur  = cJSON_GetObjectItem(v, "duration");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "SEPIASEARCH_PEERTUBE");
    cJSON_AddStringToObject(p, "source", "sepiasearch.org");
    cJSON_AddStringToObject(p, "entity", q);
    if (cJSON_IsNumber(tot))
      cJSON_AddNumberToObject(p, "total_matches", tot->valuedouble);
    jcopy(p, "uuid", v, "uuid");
    jcopy(p, "short_uuid", v, "shortUUID");
    jcopy(p, "name", v, "name");
    jcopy(p, "description", v, "description");
    jcopy(p, "duration", v, "duration");
    jcopy(p, "published_at", v, "publishedAt");
    jcopy(p, "originally_published_at", v, "originallyPublishedAt");
    jcopy(p, "thumbnail_url", v, "thumbnailUrl");
    jcopy(p, "url", v, "url");
    jcopy(p, "views", v, "views");
    if (cJSON_IsObject(acct)) {
      jcopy(p, "account_name", acct, "name");
      jcopy(p, "account_display_name", acct, "displayName");
      jcopy(p, "account_host", acct, "host");
    }
    if (cJSON_IsObject(chan)) {
      jcopy(p, "channel_name", chan, "name");
      jcopy(p, "channel_display_name", chan, "displayName");
      jcopy(p, "channel_host", chan, "host");  /* the instance holding the file */
    }
    cJSON_AddBoolToObject(p, "success", 1);
    char summary[400];
    snprintf(summary, sizeof summary, "%lds on %s · by %s",
             cJSON_IsNumber(dur) ? (long)dur->valuedouble : 0L,
             (cJSON_IsObject(chan) && jo_sv(chan, "host")) ? jo_sv(chan, "host") : "?",
             (cJSON_IsObject(acct) && jo_sv(acct, "name")) ? jo_sv(acct, "name") : "?");
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"SEPIASEARCH_PEERTUBE\",\"video\"]",
                  uuid, name, summary, jo_sv(v, "url"), jo_sv(v, "publishedAt"), NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[SEPIASEARCH_PEERTUBE] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_sepia_def = {
  .id = "SEPIASEARCH_PEERTUBE", .collector = "osint",
  .name = "SepiaSearch Federated PeerTube Video Search",
  .update_interval_sec = 0, .run = run_sepia,
  .category = "media", .type = "api",
  .url = "https://sepiasearch.org/api/v1/search/videos",
  .description = "Keyword search across the federated PeerTube corpus — video title, description, upload timestamp, duration, uploader account and the instance actually hosting the file.",
  .license = "Public keyless API run by Framasoft over public federated video metadata.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_sepia_def)

/* ---- TVMAZE_SHOW_SEARCH -------------------------------------------------- */

static int run_tvmaze(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[400];
  snprintf(url, sizeof url, "https://api.tvmaze.com/search/shows?q=%s", enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[TVMAZE_SHOW_SEARCH] no usable response for %s\n", q);
    cJSON_Delete(doc); return 0;
  }
  int n = 0;
  const cJSON *hit;
  cJSON_ArrayForEach(hit, doc) {
    const cJSON *show = cJSON_GetObjectItem(hit, "show");
    if (!cJSON_IsObject(show)) continue;
    const char *name = jo_sv(show, "name");
    const cJSON *idv = cJSON_GetObjectItem(show, "id");
    if (!name || !cJSON_IsNumber(idv)) continue;
    /* network is null for web-only shows — fall back to webChannel */
    const cJSON *net = cJSON_GetObjectItem(show, "network");
    if (!cJSON_IsObject(net)) net = cJSON_GetObjectItem(show, "webChannel");
    const cJSON *ext = cJSON_GetObjectItem(show, "externals");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "TVMAZE_SHOW_SEARCH");
    cJSON_AddStringToObject(p, "source", "api.tvmaze.com");
    cJSON_AddStringToObject(p, "entity", q);
    jcopy(p, "score", hit, "score");
    jcopy(p, "id", show, "id");
    jcopy(p, "name", show, "name");
    jcopy(p, "type", show, "type");
    jcopy(p, "language", show, "language");
    jcopy(p, "genres", show, "genres");
    jcopy(p, "status", show, "status");
    jcopy(p, "premiered", show, "premiered");
    jcopy(p, "ended", show, "ended");
    jcopy(p, "official_site", show, "officialSite");
    jcopy(p, "url", show, "url");
    const char *netname = NULL, *country = NULL;
    if (cJSON_IsObject(net)) {
      netname = jo_sv(net, "name");
      jcopy(p, "network", net, "name");
      const cJSON *co = cJSON_GetObjectItem(net, "country");
      if (cJSON_IsObject(co)) {
        country = jo_sv(co, "name");
        jcopy(p, "network_country", co, "name");
        jcopy(p, "network_country_code", co, "code");   /* ISO2, not geometry */
        jcopy(p, "network_timezone", co, "timezone");
      }
    }
    if (cJSON_IsObject(ext)) {
      jcopy(p, "imdb", ext, "imdb");
      jcopy(p, "thetvdb", ext, "thetvdb");
      jcopy(p, "tvrage", ext, "tvrage");
    }
    cJSON_AddBoolToObject(p, "success", 1);
    char rk[64], summary[400];
    snprintf(rk, sizeof rk, "tvmaze-%ld", (long)idv->valuedouble);
    snprintf(summary, sizeof summary, "%s · %s · %s%s%s",
             jo_sv(show, "type") ? jo_sv(show, "type") : "?",
             jo_sv(show, "language") ? jo_sv(show, "language") : "?",
             netname ? netname : "(no network)",
             country ? " / " : "", country ? country : "");
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"TVMAZE_SHOW_SEARCH\",\"broadcast\"]",
                  rk, name, summary, jo_sv(show, "url"),
                  jo_sv(show, "premiered"), jo_sv(show, "language"));
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[TVMAZE_SHOW_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_tvmaze_def = {
  .id = "TVMAZE_SHOW_SEARCH", .collector = "osint",
  .name = "TVmaze Show and Schedule Index",
  .update_interval_sec = 0, .run = run_tvmaze,
  .category = "media", .type = "api",
  .url = "https://api.tvmaze.com/search/shows",
  .description = "Broadcast metadata index — resolves a programme or network name to a broadcaster, country, status and cross-IDs to IMDb and TheTVDB.",
  .license = "TVmaze API: free and keyless for NON-COMMERCIAL use (CC BY-SA data); commercial use requires their paid tier.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_tvmaze_def)

/* ---- DAILYMOTION_SEARCH -------------------------------------------------- */

static int run_dailymotion(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[500];
  /* fields= is mandatory: without it the API returns id/title only. */
  snprintf(url, sizeof url,
           "https://api.dailymotion.com/videos?search=%s"
           "&fields=id,title,created_time,owner.screenname,url,duration,views_total"
           "&limit=20", enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!doc) { fprintf(stderr, "[DAILYMOTION_SEARCH] no response for %s\n", q); return 0; }
  const cJSON *list = cJSON_GetObjectItem(doc, "list");
  int n = 0;
  const cJSON *v;
  if (cJSON_IsArray(list)) cJSON_ArrayForEach(v, list) {
    const char *id = jo_sv(v, "id");
    const char *title = jo_sv(v, "title");
    if (!id || !title) continue;
    const cJSON *ct = cJSON_GetObjectItem(v, "created_time");
    const cJSON *dur = cJSON_GetObjectItem(v, "duration");
    const cJSON *vt = cJSON_GetObjectItem(v, "views_total");
    char iso[40]; iso[0] = 0;
    if (cJSON_IsNumber(ct)) {
      time_t t = (time_t)ct->valuedouble;
      struct tm g; gmtime_r(&t, &g);
      strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", &g);
    }
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "DAILYMOTION_SEARCH");
    cJSON_AddStringToObject(p, "source", "api.dailymotion.com");
    cJSON_AddStringToObject(p, "entity", q);
    jcopy(p, "id", v, "id");
    jcopy(p, "title", v, "title");
    jcopy(p, "url", v, "url");
    jcopy(p, "duration", v, "duration");
    jcopy(p, "views_total", v, "views_total");
    /* dotted field names arrive as literal keys */
    jcopy(p, "owner_screenname", v, "owner.screenname");
    if (iso[0]) cJSON_AddStringToObject(p, "created_time", iso);
    cJSON_AddBoolToObject(p, "success", 1);
    char summary[300];
    snprintf(summary, sizeof summary, "by %s · %lds · %ld views",
             jo_sv(v, "owner.screenname") ? jo_sv(v, "owner.screenname") : "?",
             cJSON_IsNumber(dur) ? (long)dur->valuedouble : 0L,
             cJSON_IsNumber(vt) ? (long)vt->valuedouble : 0L);
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"DAILYMOTION_SEARCH\",\"video\"]",
                  id, title, summary, jo_sv(v, "url"), iso[0] ? iso : NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[DAILYMOTION_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_dailymotion_def = {
  .id = "DAILYMOTION_SEARCH", .collector = "osint",
  .name = "Dailymotion Public Video Search",
  .update_interval_sec = 0, .run = run_dailymotion,
  .category = "media", .type = "api",
  .url = "https://api.dailymotion.com/videos",
  .description = "Keyless keyword video search on a platform carrying non-Western broadcaster and wire footage that never reaches YouTube — title, uploader, upload time, duration and view count.",
  .license = "Dailymotion Graph API; public read of public videos is unauthenticated, rate limited per IP.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_dailymotion_def)

/* ---- ODYSEE_SEARCH ------------------------------------------------------- */

static int run_odysee(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[400];
  snprintf(url, sizeof url, "https://lighthouse.odysee.tv/search?s=%s&size=20", enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[ODYSEE_SEARCH] no usable response for %s\n", q);
    cJSON_Delete(doc); return 0;
  }
  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    const char *name = jo_sv(r, "name");
    const char *claim = jo_sv(r, "claimId");
    if (!name || !claim) continue;
    int is_channel = (name[0] == '@');
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "ODYSEE_SEARCH");
    cJSON_AddStringToObject(p, "source", "lighthouse.odysee.tv");
    cJSON_AddStringToObject(p, "entity", q);
    jcopy(p, "name", r, "name");
    jcopy(p, "claim_id", r, "claimId");
    jcopy(p, "channel_claim_id", r, "channel_claim_id");
    cJSON_AddBoolToObject(p, "is_channel", is_channel);
    cJSON_AddBoolToObject(p, "success", 1);
    /* canonical LBRY permalink form: <name>:<claimId> */
    char link[400], rk[200], summary[160];
    snprintf(link, sizeof link, "https://odysee.com/%s:%s", name, claim);
    snprintf(rk, sizeof rk, "%s#%s", name, claim);
    snprintf(summary, sizeof summary, "%s · claim %s",
             is_channel ? "Odysee channel" : "Odysee claim", claim);
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"ODYSEE_SEARCH\",\"video\"]",
                  rk, name, summary, link, NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[ODYSEE_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_odysee_def = {
  .id = "ODYSEE_SEARCH", .collector = "osint",
  .name = "Odysee/LBRY Lighthouse Claim Search",
  .update_interval_sec = 0, .run = run_odysee,
  .category = "media", .type = "api",
  .url = "https://lighthouse.odysee.tv/search",
  .description = "Search index for the LBRY/Odysee blockchain video network — returns the permanent claim ID, so a channel stays trackable even when its display name changes.",
  .license = "Odysee's public Lighthouse search service; keyless, open-source (LBRY). Metadata only.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_odysee_def)
