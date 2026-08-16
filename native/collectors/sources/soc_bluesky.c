/* soc_bluesky.c — AT Protocol / Bluesky public read APIs.
 *
 * TRAP (from the probe notes): the public AppView is HALF-GATED.
 * app.bsky.feed.searchPosts on public.api.bsky.app now returns 403 (post
 * search is auth-gated), while app.bsky.actor.searchActors,
 * app.bsky.actor.getProfile and app.bsky.feed.getAuthorFeed on the SAME host
 * still answer unauthenticated. Only the working three are implemented here;
 * the 403 endpoint is deliberately NOT wired up.
 *
 *  BLUESKY_ACTOR_SEARCH  (entity pivot: username/handle fragment)
 *      GET https://public.api.bsky.app/xrpc/app.bsky.actor.searchActors?q=&limit=25
 *      then GET app.bsky.actor.getProfile for an exact handle/DID match
 *      emits: did, handle, displayName, description, avatar, createdAt,
 *             indexedAt, labels[] and (from getProfile) followersCount,
 *             followsCount, postsCount
 *  BLUESKY_AUTHOR_FEED   (entity pivot: username/handle or did:plc:)
 *      GET https://public.api.bsky.app/xrpc/app.bsky.feed.getAuthorFeed?actor=
 *      emits: post.uri, post.cid, record.text, record.createdAt, author.did,
 *             author.handle, author.displayName, reply/repost/like counts,
 *             repost reason when the row is a repost
 *  atproto-plc-directory (scheduled, hourly)
 *      GET https://plc.directory/export?count=1000  — JSONL, NOT a JSON array
 *      emits: did, cid, createdAt, operation.type, handle (alsoKnownAs),
 *             PDS service endpoint, nullified
 *
 * All keyless. No upstream coordinates anywhere → has_geo stays 0 (R2).
 * Licence: public.api.bsky.app is Bluesky's deliberately unauthenticated
 * read-only AppView (public profile/post data only, no private or blocked
 * content); plc.directory is public protocol infrastructure operated by
 * Bluesky PBC and is explicitly exportable.
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

#define BSKY_HOST "https://public.api.bsky.app/xrpc/"

static const char *const SOC_UA[] = {
  "User-Agent: JapanOSINT-native/1.0 (OSINT research collector; +https://github.com/)",
  "Accept: application/json", NULL
};

/* A handle ("alice.bsky.social"), a bare name fragment, or a DID. Rejects
 * anything with a slash/space/@ (a URL or an e-mail is not this pivot). */
static int looks_like_handle(const char *s) {
  if (!s || !*s) return 0;
  size_t n = strlen(s);
  if (n < 2 || n > 253) return 0;
  if (strchr(s, '/') || strchr(s, ' ') || strchr(s, '@') || strchr(s, '?')) return 0;
  for (const char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (!(isalnum(c) || c == '-' || c == '.' || c == '_' || c == ':')) return 0;
  }
  return 1;
}

/* ---- BLUESKY_ACTOR_SEARCH ---------------------------------------------- */

static int emit_actor(intel_sink *sink, const cJSON *a, const char *entity,
                      const char *from) {
  const char *handle = jo_sv(a, "handle");
  const char *did    = jo_sv(a, "did");
  if (!handle || !did) return 0;
  cJSON *p = cJSON_CreateObject();
  if (!p) return 0;
  cJSON_AddStringToObject(p, "service", "BLUESKY_ACTOR_SEARCH");
  cJSON_AddStringToObject(p, "source", "public.api.bsky.app");
  cJSON_AddStringToObject(p, "endpoint", from);
  cJSON_AddStringToObject(p, "entity", entity);
  jcopy(p, "did", a, "did");
  jcopy(p, "handle", a, "handle");
  jcopy(p, "display_name", a, "displayName");
  jcopy(p, "description", a, "description");
  jcopy(p, "avatar", a, "avatar");
  jcopy(p, "created_at", a, "createdAt");
  jcopy(p, "indexed_at", a, "indexedAt");
  jcopy(p, "labels", a, "labels");
  jcopy(p, "followers_count", a, "followersCount");
  jcopy(p, "follows_count", a, "followsCount");
  jcopy(p, "posts_count", a, "postsCount");
  cJSON_AddBoolToObject(p, "success", 1);
  char title[300], summary[400], link[300];
  snprintf(title, sizeof title, "%s (@%s)",
           jo_sv(a, "displayName") ? jo_sv(a, "displayName") : handle, handle);
  snprintf(summary, sizeof summary, "%s · %s",
           did, jo_sv(a, "description") ? jo_sv(a, "description") : "(no bio)");
  snprintf(link, sizeof link, "https://bsky.app/profile/%s", handle);
  return soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"BLUESKY_ACTOR_SEARCH\",\"bluesky\"]",
                  did, title, summary, link, jo_sv(a, "createdAt"), NULL);
}

static int run_actor_search(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_handle(q)) return 0;          /* wrong shape → no-op (R3) */
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[512];
  int n = 0;

  snprintf(url, sizeof url,
           BSKY_HOST "app.bsky.actor.searchActors?q=%s&limit=25", enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (doc) {
    const cJSON *arr = cJSON_GetObjectItem(doc, "actors"), *a;
    if (cJSON_IsArray(arr)) cJSON_ArrayForEach(a, arr)
      n += emit_actor(sink, a, q, "app.bsky.actor.searchActors");
    cJSON_Delete(doc);
  }

  /* If the pivot was a full handle or a DID, the profile view carries the
   * follower/post counts that search results omit. 400 = no such actor,
   * which is a clean empty, not a failure. */
  if (strchr(q, '.') || strncmp(q, "did:", 4) == 0) {
    snprintf(url, sizeof url,
             BSKY_HOST "app.bsky.actor.getProfile?actor=%s", enc);
    cJSON *prof = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
    if (prof) {
      n += emit_actor(sink, prof, q, "app.bsky.actor.getProfile");
      cJSON_Delete(prof);
    }
  }
  free(enc);
  fprintf(stderr, "[BLUESKY_ACTOR_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_bsky_actor_def = {
  .id = "BLUESKY_ACTOR_SEARCH", .collector = "osint",
  .name = "Bluesky Public Actor Search (AT Protocol)",
  .update_interval_sec = 0, .run = run_actor_search,
  .category = "social", .type = "api",
  .url = BSKY_HOST "app.bsky.actor.searchActors",
  .description = "Resolve a handle or name fragment to Bluesky accounts on the unauthenticated public AppView: DID, handle, display name, bio, account-creation date and profile counts.",
  .license = "Bluesky public read-only AppView (public.api.bsky.app); public profile data only, keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_bsky_actor_def)

/* ---- BLUESKY_AUTHOR_FEED ------------------------------------------------ */

static int run_author_feed(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_handle(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
           BSKY_HOST "app.bsky.feed.getAuthorFeed?actor=%s&limit=25", enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);

  /* A bare username is not a valid actor; the canonical form is
   * <name>.bsky.social. Retry once in that shape before giving up. */
  if (!doc && !strchr(q, '.') && strncmp(q, "did:", 4) != 0) {
    char alt[320];
    snprintf(alt, sizeof alt, "%s.bsky.social", q);
    char *enc2 = soc_urlenc(alt);
    if (enc2) {
      snprintf(url, sizeof url,
               BSKY_HOST "app.bsky.feed.getAuthorFeed?actor=%s&limit=25", enc2);
      doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
      free(enc2);
    }
  }
  free(enc);
  if (!doc) {                       /* unknown handle / 400 → honest empty */
    fprintf(stderr, "[BLUESKY_AUTHOR_FEED] no feed for %s\n", q);
    return 0;
  }

  const cJSON *feed = cJSON_GetObjectItem(doc, "feed"), *row;
  int n = 0;
  if (cJSON_IsArray(feed)) cJSON_ArrayForEach(row, feed) {
    const cJSON *post = cJSON_GetObjectItem(row, "post");
    if (!cJSON_IsObject(post)) continue;
    const char *uri = jo_sv(post, "uri");
    if (!uri) continue;
    const cJSON *rec = cJSON_GetObjectItem(post, "record");
    const cJSON *au  = cJSON_GetObjectItem(post, "author");
    const cJSON *rsn = cJSON_GetObjectItem(row, "reason");
    const char *text = cJSON_IsObject(rec) ? jo_sv(rec, "text") : NULL;
    const char *when = cJSON_IsObject(rec) ? jo_sv(rec, "createdAt") : NULL;
    const char *hdl  = cJSON_IsObject(au) ? jo_sv(au, "handle") : NULL;
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "BLUESKY_AUTHOR_FEED");
    cJSON_AddStringToObject(p, "source", "public.api.bsky.app");
    cJSON_AddStringToObject(p, "entity", q);
    jcopy(p, "uri", post, "uri");
    jcopy(p, "cid", post, "cid");
    jcopy(p, "indexed_at", post, "indexedAt");
    jcopy(p, "reply_count", post, "replyCount");
    jcopy(p, "repost_count", post, "repostCount");
    jcopy(p, "like_count", post, "likeCount");
    jcopy(p, "quote_count", post, "quoteCount");
    if (cJSON_IsObject(rec)) {
      jcopy(p, "text", rec, "text");
      jcopy(p, "created_at", rec, "createdAt");
      jcopy(p, "langs", rec, "langs");
    }
    if (cJSON_IsObject(au)) {
      jcopy(p, "author_did", au, "did");
      jcopy(p, "author_handle", au, "handle");
      jcopy(p, "author_display_name", au, "displayName");
    }
    /* reposts arrive with a feed[].reason object */
    if (cJSON_IsObject(rsn)) {
      jcopy(p, "reason_type", rsn, "$type");
      jcopy(p, "reason_indexed_at", rsn, "indexedAt");
      cJSON_AddBoolToObject(p, "is_repost", 1);
    }
    cJSON_AddBoolToObject(p, "success", 1);
    char title[400], link[400];
    snprintf(title, sizeof title, "@%s: %.200s", hdl ? hdl : "?",
             text ? text : "(no text)");
    /* at://did/app.bsky.feed.post/<rkey> → the public permalink uses the rkey */
    const char *rk = strrchr(uri, '/');
    if (hdl && rk && rk[1])
      snprintf(link, sizeof link, "https://bsky.app/profile/%s/post/%s", hdl, rk + 1);
    else
      snprintf(link, sizeof link, "%s", uri);
    const char *lang = NULL;
    if (cJSON_IsObject(rec)) {
      const cJSON *ls = cJSON_GetObjectItem(rec, "langs");
      if (cJSON_IsArray(ls)) {
        const cJSON *l0 = cJSON_GetArrayItem(ls, 0);
        if (cJSON_IsString(l0) && l0->valuestring && l0->valuestring[0])
          lang = l0->valuestring;
      }
    }
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"BLUESKY_AUTHOR_FEED\",\"bluesky\"]",
                  uri, title, text, link, when, lang);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[BLUESKY_AUTHOR_FEED] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_bsky_feed_def = {
  .id = "BLUESKY_AUTHOR_FEED", .collector = "osint",
  .name = "Bluesky Public Author Feed (AT Protocol)",
  .update_interval_sec = 0, .run = run_author_feed,
  .category = "social", .type = "api",
  .url = BSKY_HOST "app.bsky.feed.getAuthorFeed",
  .description = "Public post history for a Bluesky handle or DID — text, timestamps, engagement counts and repost markers.",
  .license = "Bluesky public read-only AppView; public posts only, keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_bsky_feed_def)

/* ---- atproto-plc-directory (JSONL) -------------------------------------- */

static int plc_line(intel_sink *sink, const char *line) {
  cJSON *o = cJSON_Parse(line);
  if (!o) return 0;
  const char *did = jo_sv(o, "did");
  const char *created = jo_sv(o, "createdAt");
  if (!did) { cJSON_Delete(o); return 0; }
  const cJSON *op = cJSON_GetObjectItem(o, "operation");
  const char *type = cJSON_IsObject(op) ? jo_sv(op, "type") : NULL;

  /* handle: legacy `create` ops carry operation.handle, plc_operation ops
   * carry alsoKnownAs[] = ["at://alice.bsky.social", ...] */
  const char *handle = cJSON_IsObject(op) ? jo_sv(op, "handle") : NULL;
  if (!handle && cJSON_IsObject(op)) {
    const cJSON *aka = cJSON_GetObjectItem(op, "alsoKnownAs");
    if (cJSON_IsArray(aka)) {
      const cJSON *a0 = cJSON_GetArrayItem(aka, 0);
      if (cJSON_IsString(a0) && a0->valuestring && a0->valuestring[0]) {
        handle = a0->valuestring;
        if (strncmp(handle, "at://", 5) == 0) handle += 5;
      }
    }
  }
  /* PDS: legacy operation.service, or services.atproto_pds.endpoint */
  const char *pds = cJSON_IsObject(op) ? jo_sv(op, "service") : NULL;
  if (!pds && cJSON_IsObject(op)) {
    const cJSON *svcs = cJSON_GetObjectItem(op, "services");
    if (cJSON_IsObject(svcs)) {
      const cJSON *pdsobj = cJSON_GetObjectItem(svcs, "atproto_pds");
      if (cJSON_IsObject(pdsobj)) pds = jo_sv(pdsobj, "endpoint");
    }
  }

  cJSON *p = cJSON_CreateObject();
  if (!p) { cJSON_Delete(o); return 0; }
  cJSON_AddStringToObject(p, "source", "plc.directory");
  cJSON_AddStringToObject(p, "did", did);
  jcopy(p, "cid", o, "cid");
  jcopy(p, "created_at", o, "createdAt");
  jcopy(p, "nullified", o, "nullified");
  if (type)   cJSON_AddStringToObject(p, "operation_type", type);
  if (handle) cJSON_AddStringToObject(p, "handle", handle);
  if (pds)    cJSON_AddStringToObject(p, "pds", pds);

  char title[400], summary[400], rk[200];
  snprintf(title, sizeof title, "%s → %s", did, handle ? handle : "(no handle)");
  snprintf(summary, sizeof summary, "%s%s%s", type ? type : "plc-op",
           pds ? " · " : "", pds ? pds : "");
  snprintf(rk, sizeof rk, "%s|%s", did, jo_sv(o, "cid") ? jo_sv(o, "cid") : "");
  char link[300];
  snprintf(link, sizeof link, "https://plc.directory/%s", did);
  int r = soc_emit(sink, p, "atproto-plc-op",
                   "[\"atproto\",\"bluesky\",\"identity\",\"plc\"]",
                   rk, title, summary, link, created, NULL);
  cJSON_Delete(o);
  return r;
}

static int run_plc_export(const source_ctx *ctx, intel_sink *sink) {
  char *txt = feed_get_text(ctx->http,
      "https://plc.directory/export?count=1000", 40000);
  if (!txt) { fprintf(stderr, "[atproto-plc-directory] fetch failed\n"); return -1; }
  /* Newline-delimited JSON — NOT a JSON array. Parse line by line. */
  int n = 0;
  char *p = txt;
  while (*p) {
    char *nl = strchr(p, '\n');
    if (nl) *nl = 0;
    if (*p == '{') n += plc_line(sink, p);
    if (!nl) break;
    p = nl + 1;
  }
  free(txt);
  fprintf(stderr, "[atproto-plc-directory] emitted %d\n", n);
  return 0;
}

static const source_def soc_plc_def = {
  .id = "atproto-plc-directory", .collector = "social",
  .name = "AT Protocol PLC Directory Export",
  .update_interval_sec = 3600, .run = run_plc_export,
  .category = "social", .type = "api",
  .url = "https://plc.directory/export",
  .description = "Append-only AT Protocol identity ledger: every DID creation, handle change and PDS migration in timestamp order — the audit trail for account renames and hosting moves.",
  .license = "Public directory operated by Bluesky PBC as protocol infrastructure; explicitly exportable, keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_plc_def)
