/* collectors/social/sources/mastodon_jp_instances.c
 * Port of server/src/collectors/mastodonJpInstances.js.
 * Polls /api/v1/timelines/public?local=true&limit=25 on 4 fixed JP Mastodon
 * instances. On a per-host error an instance_error feature is emitted (live
 * upstream state, not curated SEED). _meta dropped. MASTODON_JP_* env
 * overrides not ported.
 *
 * NO geometry. The JS port pinned every toot at Tokyo Station
 * (139.6917, 35.6895); a public-timeline post carries no location, so that
 * was an invented coordinate that stacked hundreds of false pins on one
 * point. Mastodon's status object has no lat/lon field at all. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *INSTANCES[] = {
  "mstdn.jp", "pawoo.net", "fedibird.com", "mastodon-japan.net",
};
#define NI ((int)(sizeof(INSTANCES) / sizeof(INSTANCES[0])))

/* p?.x ?? null  for a numeric field → real number or cJSON null */
static cJSON *num_or_null(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (v && cJSON_IsNumber(v)) return cJSON_CreateNumber(cJSON_GetNumberValue(v));
  return cJSON_CreateNull();
}

/* x || null  for a string field */
static cJSON *str_or_null(const cJSON *o, const char *k) {
  const char *s = jo_sv(o, k);
  return s ? cJSON_CreateString(s) : cJSON_CreateNull();
}

/* Cut `s` to at most `max` BYTES on a UTF-8 character boundary. */
static void utf8_trunc(char *s, size_t max) {
  if (!s) return;
  size_t L = strlen(s);
  if (L <= max) return;
  size_t c = max;
  while (c > 0 && ((unsigned char)s[c] & 0xC0) == 0x80) c--;
  s[c] = 0;
}

/* String(p.content||'').replace(/<[^>]+>/g,' ').replace(/\s+/g,' ').slice(0,400)
 * (no .trim() in JS). Result malloc'd (caller frees). */
static char *clean_content(const char *in) {
  if (!in) in = "";
  size_t L = strlen(in);
  char *t = malloc(L + 1);
  size_t o = 0;
  /* strip <...> */
  for (size_t i = 0; i < L;) {
    if (in[i] == '<') {
      while (i < L && in[i] != '>') i++;
      if (i < L) i++;             /* skip '>' */
      t[o++] = ' ';
    } else {
      t[o++] = in[i++];
    }
  }
  t[o] = 0;
  /* collapse \s+ → single space */
  char *r = malloc(o + 1);
  size_t ro = 0; int inws = 0;
  for (size_t i = 0; i < o; i++) {
    unsigned char ch = (unsigned char)t[i];
    int ws = (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' ||
              ch == '\f' || ch == '\v');
    if (ws) { if (!inws) { r[ro++] = ' '; inws = 1; } }
    else { r[ro++] = (char)ch; inws = 0; }
  }
  r[ro] = 0;
  free(t);
  /* slice(0,400) — byte slice */
  if (ro > 400) r[400] = 0;
  return r;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = {
    "accept: application/json",
    "user-agent: japanosint-collector",
    NULL,
  };
  cJSON *features = cJSON_CreateArray();

  for (int hi = 0; hi < NI; hi++) {
    const char *host = INSTANCES[hi];
    char url[256];
    snprintf(url, sizeof url,
      "https://%s/api/v1/timelines/public?local=true&limit=25", host);
    cJSON *arr = feed_get_json_h(ctx->http, url, hdrs, 12000);

    if (!cJSON_IsArray(arr)) {
      /* r.err branch — emit instance_error feature */
      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      /* NO geometry — see the file header. */
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "kind", "instance_error");
      cJSON_AddStringToObject(p, "host", host);
      cJSON_AddStringToObject(p, "err", "fetch_failed");
      cJSON_AddStringToObject(p, "source", "mastodon_jp");
      char eb[160];
      snprintf(eb, sizeof eb, "%s unreachable", host);
      cJSON_AddStringToObject(p, "title", eb);
      snprintf(eb, sizeof eb, "instance_error:%s", host);
      cJSON_AddStringToObject(p, "uid", eb);   /* one stable row per host */
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      if (arr) cJSON_Delete(arr);
      continue;
    }

    cJSON *post;
    cJSON_ArrayForEach(post, arr) {
      char srcbuf[96];
      snprintf(srcbuf, sizeof srcbuf, "mastodon_%s", host);
      char *content = clean_content(jo_sv(post, "content"));

      cJSON *acct = cJSON_GetObjectItem(post, "account");

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      /* NO geometry — see the file header. */
      cJSON *p = cJSON_CreateObject();          /* EXACT JS key order */
      cJSON_AddStringToObject(p, "host", host);
      cJSON_AddItemToObject(p, "id", str_or_null(post, "id"));
      cJSON_AddItemToObject(p, "uri", str_or_null(post, "uri"));
      cJSON_AddItemToObject(p, "url", str_or_null(post, "url"));
      cJSON_AddItemToObject(p, "created_at", str_or_null(post, "created_at"));
      cJSON_AddStringToObject(p, "content", content);
      cJSON_AddItemToObject(p, "spoiler_text", str_or_null(post, "spoiler_text"));
      cJSON_AddItemToObject(p, "language", str_or_null(post, "language"));
      cJSON_AddItemToObject(p, "replies_count", num_or_null(post, "replies_count"));
      cJSON_AddItemToObject(p, "reblogs_count", num_or_null(post, "reblogs_count"));
      cJSON_AddItemToObject(p, "favourites_count",
        num_or_null(post, "favourites_count"));
      cJSON_AddItemToObject(p, "author_acct",
        (acct && cJSON_IsObject(acct)) ? str_or_null(acct, "acct")
                                       : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "author_url",
        (acct && cJSON_IsObject(acct)) ? str_or_null(acct, "url")
                                       : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "author_followers",
        (acct && cJSON_IsObject(acct)) ? num_or_null(acct, "followers_count")
                                       : cJSON_CreateNull());
      cJSON_AddStringToObject(p, "source", srcbuf);

      /* lib/geojson.c derives title/summary/published_at/uid from the
       * properties bag. The JS port set none of them, so every toot landed
       * with no title and no timestamp, and the uid was the bare status id —
       * which is only unique WITHIN one instance, so two instances handing
       * out the same numeric id collided into one row. */
      const char *sid = jo_sv(post, "id");
      if (sid) {
        char uidb[192];
        snprintf(uidb, sizeof uidb, "%s:%s", host, sid);
        cJSON_AddStringToObject(p, "uid", uidb);
      }
      const char *spoil = jo_sv(post, "spoiler_text");
      char title[256];
      if (spoil) snprintf(title, sizeof title, "%s", spoil);
      else       snprintf(title, sizeof title, "%s", content);
      /* strip the leading space clean_content() leaves behind */
      char *tp = title; while (*tp == ' ') tp++;
      utf8_trunc(tp, 120);
      if (*tp) cJSON_AddStringToObject(p, "title", tp);
      else {
        char fb[192];
        snprintf(fb, sizeof fb, "%s post by @%s", host,
                 (acct && cJSON_IsObject(acct) && jo_sv(acct, "acct"))
                   ? jo_sv(acct, "acct") : "unknown");
        cJSON_AddStringToObject(p, "title", fb);
      }
      cJSON_AddStringToObject(p, "summary", content);
      const char *cat = jo_sv(post, "created_at");
      if (cat) cJSON_AddStringToObject(p, "published_at", cat);

      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      free(content);
    }
    cJSON_Delete(arr);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[mastodon-jp-instances] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def mastodon_jp_instances_def = {
  .id = "mastodon-jp-instances", .collector = "social",
  .name = "Mastodon JP local timelines", .name_ja = "Mastodon 日本サーバ",
   .update_interval_sec = 600, .run = run,
};
REGISTER_SOURCE(mastodon_jp_instances_def)
