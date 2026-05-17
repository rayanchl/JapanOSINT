/* collectors/social/sources/bird_makeup_jp.c
 * Port of server/src/collectors/birdMakeupJp.js. Polls bird.makeup
 * ActivityPub /outbox for a curated (or BIRD_HANDLES-overridden) handle
 * list → one intel row per post. intelEnvelope wrapper dropped (rule 7). */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BIRD_BASE "https://bird.makeup"

static const char *DEFAULT_HANDLES =
  "earthquakejapan,UN_NERV,TokyoMetro_PR,JR_East_official,japantimes,NHK_PR";

/* strip <...> tags, then trim (JS: content.replace(/<[^>]+>/g,'').trim()) */
static char *strip_tags(const char *in) {
  size_t n = strlen(in);
  char *o = malloc(n + 1);
  size_t j = 0; int intag = 0;
  for (size_t i = 0; i < n; i++) {
    char c = in[i];
    if (c == '<') { intag = 1; continue; }
    if (c == '>') { intag = 0; continue; }
    if (!intag) o[j++] = c;
  }
  o[j] = 0;
  char *s = o;
  while (*s==' '||*s=='\t'||*s=='\r'||*s=='\n') s++;
  size_t L = strlen(s);
  while (L && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='\r'||s[L-1]=='\n')) s[--L]=0;
  if (s != o) memmove(o, s, L + 1);
  return o;
}

/* byte-accurate prefix of `s` capped at `max` bytes (parity-neutral vs JS
 * char slice for the title/summary fields) */
static char *prefix(const char *s, size_t max) {
  size_t L = strlen(s);
  if (L > max) L = max;
  char *r = malloc(L + 1);
  memcpy(r, s, L); r[L] = 0;
  return r;
}

static const char *strv(const cJSON *o, const char *k) {
  const cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

/* JS new Date(s).toISOString() or null if unparseable. We pass valid
 * ISO-8601 through verbatim; anything else → NULL (parity-safe: bird.makeup
 * publishes ISO timestamps). */
static const char *safe_iso(const char *s) {
  if (!s) return NULL;
  int y=0,mo=0,d=0;
  if (sscanf(s, "%4d-%2d-%2d", &y, &mo, &d) == 3 && y > 1900) return s;
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *env = getenv("BIRD_HANDLES");
  char list[1024];
  snprintf(list, sizeof list, "%s",
           (env && *env) ? env : DEFAULT_HANDLES);

  const char *accept_hdr[] = {
    "accept: application/activity+json",
    "user-agent: JapanOSINT/1.0",
    NULL
  };

  int n = 0;
  char *save = list, *handle;
  while ((handle = strsep(&save, ",")) != NULL) {
    while (*handle==' '||*handle=='\t') handle++;
    size_t hl = strlen(handle);
    while (hl && (handle[hl-1]==' '||handle[hl-1]=='\t')) handle[--hl]=0;
    if (!*handle) continue;

    char url[512];
    snprintf(url, sizeof url, "%s/users/%s/outbox?page=true", BIRD_BASE, handle);
    cJSON *outbox = feed_get_json_h(ctx->http, url, accept_hdr, 12000);
    if (!outbox) continue;

    const cJSON *items = cJSON_GetObjectItem(outbox, "orderedItems");
    if (cJSON_IsArray(items)) {
      const cJSON *entry;
      cJSON_ArrayForEach(entry, items) {
        const cJSON *obj = cJSON_GetObjectItem(entry, "object");
        if (!obj || !cJSON_IsObject(obj)) continue;
        const cJSON *content = cJSON_GetObjectItem(obj, "content");
        if (!content || !cJSON_IsString(content)) continue;
        char *raw = strip_tags(content->valuestring);
        if (!raw[0]) { free(raw); continue; }

        const char *pubRaw = strv(obj, "published");
        if (!pubRaw) pubRaw = strv(entry, "published");
        const char *pubIso = safe_iso(pubRaw);

        const char *eid = strv(entry, "id");
        const char *oid = strv(obj, "id");
        char fb[160];
        if (pubRaw) snprintf(fb, sizeof fb, "%s_%s", handle, pubRaw);
        else {
          char *p32 = prefix(raw, 32);
          snprintf(fb, sizeof fb, "%s_%s", handle, p32);
          free(p32);
        }
        const char *rk = eid ? eid : (oid ? oid : fb);

        char *title = prefix(raw, 120);
        char *summary = prefix(raw, 240);
        const char *link = strv(obj, "url");
        if (!link) link = oid;

        /* tags: ['x-twitter','bird-makeup',`handle:<h>`] */
        cJSON *tags = cJSON_CreateArray();
        cJSON_AddItemToArray(tags, cJSON_CreateString("x-twitter"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("bird-makeup"));
        char ht[128]; snprintf(ht, sizeof ht, "handle:%s", handle);
        cJSON_AddItemToArray(tags, cJSON_CreateString(ht));
        char *tj = cJSON_PrintUnformatted(tags);

        /* properties: { handle, remote_id } — EXACT JS key order */
        cJSON *pr = cJSON_CreateObject();
        cJSON_AddStringToObject(pr, "handle", handle);
        const char *rid = eid ? eid : oid;
        cJSON_AddItemToObject(pr, "remote_id",
          rid ? cJSON_CreateString(rid) : cJSON_CreateNull());
        char *pj = cJSON_PrintUnformatted(pr);

        intel_item it = {0};
        it.remote_key = rk;
        it.title = title;
        it.body = raw;
        it.summary = summary;
        it.link = link;
        it.author = handle;
        it.lang = "ja";
        it.published_at = pubIso;
        it.tags_json = tj;
        it.properties_json = pj;
        if (sink->emit(sink, &it) >= 0) n++;

        free(title); free(summary); free(raw);
        free(tj); free(pj);
        cJSON_Delete(tags); cJSON_Delete(pr);
      }
    }
    cJSON_Delete(outbox);
  }
  fprintf(stderr, "[bird-makeup-jp] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def bird_makeup_jp_def = {
  .id = "bird-makeup-jp", .collector = "social",
  .name = "bird.makeup (JP bridge)", .name_ja = "bird.makeup (X ブリッジ)",
   .update_interval_sec = 900, .run = run,
};
REGISTER_SOURCE(bird_makeup_jp_def)
