/* collectors/social/sources/note_com_trending.c
 * Port of server/src/collectors/noteComTrending.js (intelEnvelope — the
 * collector's whole purpose is the trending item list, so it is ported).
 * Tries 2 unofficial note.com endpoints (first non-error wins), recursively
 * walks the JSON pulling {title,url,kind} triples, emits up to 100 as intel
 * items. uid = intelUid(SOURCE_ID, it.url, idx_i) → remote_key=it.url. The
 * meta/envelope wrapper is dropped (rule 7 — only live rows). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOURCE_ID "note-com-trending"

static const char *ENDPOINTS[] = {
  "https://note.com/api/v3/hashtags/trending",
  "https://note.com/api/v2/categories/trending",
};
#define NE ((int)(sizeof(ENDPOINTS) / sizeof(ENDPOINTS[0])))

/* String(node.url) — accept string or number; returns malloc'd or NULL. */
static char *as_string(const cJSON *v) {
  if (!v) return NULL;
  if (cJSON_IsString(v) && v->valuestring) return strdup(v->valuestring);
  if (cJSON_IsNumber(v)) {
    char b[40]; snprintf(b, sizeof b, "%g", cJSON_GetNumberValue(v));
    return strdup(b);
  }
  return NULL;
}

/* truthy: JS `node.name && node.url` — present, non-empty string (or number). */
static int truthy(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return 0;
  if (cJSON_IsString(v)) return v->valuestring && v->valuestring[0];
  if (cJSON_IsNumber(v)) return cJSON_GetNumberValue(v) != 0;
  if (cJSON_IsBool(v)) return cJSON_IsTrue(v);
  if (cJSON_IsObject(v) || cJSON_IsArray(v)) return 1;
  return 0;
}

/* Recursive walk mirroring JS `walk`. Pushes into `items` array as
 * {title,url,kind}. JS order: detect-this-node THEN recurse children. */
static void walk(const cJSON *node, cJSON *items) {
  if (!node || !(cJSON_IsObject(node) || cJSON_IsArray(node))) return;
  if (cJSON_IsArray(node)) {
    const cJSON *e;
    cJSON_ArrayForEach(e, node) walk(e, items);
    return;
  }
  /* object */
  const cJSON *name    = cJSON_GetObjectItem(node, "name");
  const cJSON *url     = cJSON_GetObjectItem(node, "url");
  const cJSON *title   = cJSON_GetObjectItem(node, "title");
  const cJSON *noteUrl = cJSON_GetObjectItem(node, "note_url");

  if (truthy(node, "name") && truthy(node, "url")) {
    char *t = as_string(name);
    char *u = as_string(url);
    if (t && u) {
      cJSON *o = cJSON_CreateObject();
      cJSON_AddStringToObject(o, "title", t);
      cJSON_AddStringToObject(o, "url", u);
      cJSON_AddStringToObject(o, "kind", "hashtag");
      cJSON_AddItemToArray(items, o);
    }
    free(t); free(u);
  } else if (truthy(node, "title") && truthy(node, "note_url")) {
    char *t = as_string(title);
    char *u = as_string(noteUrl);
    if (t && u) {
      cJSON *o = cJSON_CreateObject();
      cJSON_AddStringToObject(o, "title", t);
      cJSON_AddStringToObject(o, "url", u);
      cJSON_AddStringToObject(o, "kind", "note");
      cJSON_AddItemToArray(items, o);
    }
    free(t); free(u);
  } else if (truthy(node, "title") && truthy(node, "url")) {
    char *t = as_string(title);
    char *u = as_string(url);
    if (t && u) {
      cJSON *o = cJSON_CreateObject();
      cJSON_AddStringToObject(o, "title", t);
      cJSON_AddStringToObject(o, "url", u);
      cJSON_AddStringToObject(o, "kind", "note");
      cJSON_AddItemToArray(items, o);
    }
    free(t); free(u);
  }
  /* recurse over Object.values(node) */
  const cJSON *child = node->child;
  for (; child; child = child->next) walk(child, items);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = {
    "user-agent: JapanOSINT/1.0",
    "accept: application/json",
    NULL,
  };
  cJSON *data = NULL;
  for (int i = 0; i < NE; i++) {
    data = feed_get_json_h(ctx->http, ENDPOINTS[i], hdrs, 12000);
    if (data) break;
  }
  if (!data) return -1;

  cJSON *items = cJSON_CreateArray();
  walk(data, items);
  cJSON_Delete(data);

  int n = 0, idx = 0;
  cJSON *it;
  cJSON_ArrayForEach(it, items) {
    if (idx >= 100) break;                        /* slice(0,100) */
    const cJSON *jt = cJSON_GetObjectItem(it, "title");
    const cJSON *ju = cJSON_GetObjectItem(it, "url");
    const cJSON *jk = cJSON_GetObjectItem(it, "kind");
    const char *title = (jt && cJSON_IsString(jt)) ? jt->valuestring : NULL;
    const char *url   = (ju && cJSON_IsString(ju)) ? ju->valuestring : NULL;
    const char *kind  = (jk && cJSON_IsString(jk)) ? jk->valuestring : "note";

    /* tags: ['note-com', `kind:${it.kind}`] */
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("note-com"));
    char kbuf[64]; snprintf(kbuf, sizeof kbuf, "kind:%s", kind);
    cJSON_AddItemToArray(tags, cJSON_CreateString(kbuf));
    char *tj = cJSON_PrintUnformatted(tags);

    /* properties: { kind } */
    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "kind", kind);
    char *pj = cJSON_PrintUnformatted(props);

    intel_item item = {0};
    item.remote_key     = url;                    /* intelUid → SOURCE_ID|url */
    item.title          = title;
    item.link           = url;
    item.lang           = "ja";
    item.tags_json      = tj;
    item.properties_json = pj;
    if (sink->emit(sink, &item) >= 0) n++;

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(props);
    idx++;
  }
  cJSON_Delete(items);
  fprintf(stderr, "[note-com-trending] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def note_com_trending_def = {
  .id = "note-com-trending", .collector = "social",
  .name = "note.com Trending", .name_ja = "note トレンド",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(note_com_trending_def)
