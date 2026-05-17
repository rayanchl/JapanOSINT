/* collectors/social/sources/bluesky_jetstream_jp.c
 * P6 WS GOLD REF — port of server/src/collectors/blueskyJetstreamJp.js.
 * Opens the Jetstream wss for a short window (env BLUESKY_JETSTREAM_WINDOW_MS
 * default 8000) or until BLUESKY_JETSTREAM_MAX (default 200) JP posts, then
 * closes — fits the run-once scheduler (lib/ws). _meta dropped per RULE 8;
 * features emitted via the geojson sink. */
#include "../../../source.h"
#include "../../../lib/ws.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WS_URL "wss://jetstream2.us-west.bsky.network/subscribe?wantedCollections=app.bsky.feed.post"

typedef struct { cJSON *features; int max; } bsky_ud;

/* String(text).slice(0,400) — by bytes, backed off to a UTF-8 boundary so
 * cJSON emits valid UTF-8 (post-parity vs JS UTF-16 slice; documented). */
static void slice400(const char *s, char *out, size_t outsz) {
  size_t n = s ? strlen(s) : 0;
  if (n > 400) n = 400;
  while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;  /* mid-seq */
  if (n >= outsz) n = outsz - 1;
  if (s && n) memcpy(out, s, n);
  out[n] = 0;
}

static int on_msg(const char *data, size_t len, void *udp) {
  (void)len;
  bsky_ud *u = (bsky_ud *)udp;
  cJSON *msg = cJSON_Parse(data);
  if (!msg) return 0;                                  /* JS: catch → skip */
  cJSON *op = cJSON_GetObjectItem(msg, "commit");
  cJSON *coll = op ? cJSON_GetObjectItem(op, "collection") : NULL;
  cJSON *oper = op ? cJSON_GetObjectItem(op, "operation") : NULL;
  if (!op || !cJSON_IsString(coll) ||
      strcmp(coll->valuestring, "app.bsky.feed.post") ||
      !cJSON_IsString(oper) || strcmp(oper->valuestring, "create")) {
    cJSON_Delete(msg); return 0;
  }
  cJSON *rec = cJSON_GetObjectItem(op, "record");
  cJSON *langs = rec ? cJSON_GetObjectItem(rec, "langs") : NULL;
  int has_ja = 0;
  if (cJSON_IsArray(langs)) {
    cJSON *l;
    cJSON_ArrayForEach(l, langs)
      if (cJSON_IsString(l) && !strcmp(l->valuestring, "ja")) { has_ja = 1; break; }
  }
  if (!has_ja) { cJSON_Delete(msg); return 0; }

  cJSON *did   = cJSON_GetObjectItem(msg, "did");
  cJSON *rkey  = op ? cJSON_GetObjectItem(op, "rkey") : NULL;
  cJSON *tus   = cJSON_GetObjectItem(msg, "time_us");
  cJSON *txt   = rec ? cJSON_GetObjectItem(rec, "text") : NULL;
  cJSON *embed = rec ? cJSON_GetObjectItem(rec, "embed") : NULL;
  cJSON *etype = embed ? cJSON_GetObjectItem(embed, "$type") : NULL;
  char tbuf[512];
  slice400(cJSON_IsString(txt) ? txt->valuestring : "", tbuf, sizeof tbuf);

  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *co = cJSON_CreateArray();
  cJSON_AddItemToArray(co, cJSON_CreateNumber(139.6917));   /* TOKYO */
  cJSON_AddItemToArray(co, cJSON_CreateNumber(35.6895));
  cJSON_AddItemToObject(g, "coordinates", co);
  cJSON_AddItemToObject(f, "geometry", g);
  cJSON *p = cJSON_CreateObject();                          /* EXACT order */
  cJSON_AddItemToObject(p, "did",
    cJSON_IsString(did) ? cJSON_CreateString(did->valuestring) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "rkey",
    cJSON_IsString(rkey) ? cJSON_CreateString(rkey->valuestring) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "time_us",
    cJSON_IsNumber(tus) ? cJSON_CreateNumber(tus->valuedouble) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "text", tbuf);
  cJSON_AddItemToObject(p, "langs", cJSON_Duplicate(langs, 1));
  cJSON_AddItemToObject(p, "embed_type",
    cJSON_IsString(etype) ? cJSON_CreateString(etype->valuestring) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "source", "bluesky_jetstream");
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(u->features, f);
  cJSON_Delete(msg);
  return cJSON_GetArraySize(u->features) >= u->max;        /* stop at MAX */
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *we = getenv("BLUESKY_JETSTREAM_WINDOW_MS");
  const char *me = getenv("BLUESKY_JETSTREAM_MAX");
  int window = we ? atoi(we) : 8000;
  int max    = me ? atoi(me) : 200;
  if (window <= 0) window = 8000;
  if (max <= 0) max = 200;

  bsky_ud u = { cJSON_CreateArray(), max };
  ws_collect(WS_URL, NULL, window, max, on_msg, &u);       /* err → 0 rows */
  int n = geojson_emit_features(sink, "bluesky-jetstream-jp", u.features);
  cJSON_Delete(u.features);
  fprintf(stderr, "[bluesky-jetstream-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def bluesky_jetstream_jp_def = {
  .id = "bluesky-jetstream-jp", .collector = "social",
  .name = "Bluesky Jetstream (JP)", .name_ja = "Bluesky Jetstream 日本語",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(bluesky_jetstream_jp_def)
