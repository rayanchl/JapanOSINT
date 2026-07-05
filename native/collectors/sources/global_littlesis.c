/* collectors/sources/global_littlesis.c
 * OSINT service — LITTLESIS. On-demand entity pivot (entity = person or org
 * name) against LittleSis (littlesis.org), the free "power-network" database
 * mapping relationships among influential people and organisations.
 *
 * Keyless LIVE: GET https://littlesis.org/api/entities/search?q=<entity>
 * returns a JSON:API document — { data: [ { type, id, attributes:{ name,
 * primary_ext, blurb, ... } } ] }. One intel_item per matched entity, keyed by
 * LittleSis entity id. No matches / fetch failure → emits nothing (honest empty). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* one JSON:API entity → one intel_item. Returns 1 if emitted. */
static int emit_entity(intel_sink *sink, const cJSON *e) {
  if (!e) return 0;
  const cJSON *attr = cJSON_GetObjectItem(e, "attributes");
  if (!cJSON_IsObject(attr)) return 0;

  const char *name = jo_sv(attr, "name");
  if (!name) return 0;

  const char *type_ext = jo_sv(attr, "primary_ext");  /* "Person" | "Org" */
  const char *blurb    = jo_sv(attr, "blurb");
  const char *summary  = jo_sv(attr, "summary");
  const char *uri      = jo_sv(attr, "uri");           /* littlesis.org page */

  char idbuf[64] = {0};
  const cJSON *idj = cJSON_GetObjectItem(e, "id");
  if (idj && cJSON_IsNumber(idj)) snprintf(idbuf, sizeof idbuf, "%d", idj->valueint);
  else if (idj && cJSON_IsString(idj) && idj->valuestring) {
    snprintf(idbuf, sizeof idbuf, "%s", idj->valuestring);
  }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "name", name);
  if (type_ext) cJSON_AddStringToObject(data, "primary_ext", type_ext);
  if (blurb)    cJSON_AddStringToObject(data, "blurb", blurb);
  if (summary)  cJSON_AddStringToObject(data, "summary", summary);
  if (idbuf[0]) cJSON_AddStringToObject(data, "littlesis_id", idbuf);
  cJSON_AddStringToObject(data, "source", "LittleSis");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "LITTLESIS");
  cJSON_AddStringToObject(props, "source", "LittleSis");
  if (type_ext) cJSON_AddStringToObject(props, "primary_ext", type_ext);
  if (idbuf[0]) cJSON_AddStringToObject(props, "littlesis_id", idbuf);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = idbuf[0] ? idbuf : name;
  it.title           = name;
  it.summary         = blurb ? blurb : type_ext;
  it.body            = bj;
  it.lang            = "en";
  it.link            = uri;
  it.record_type     = "littlesis-entity";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"LITTLESIS\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[1024];
  snprintf(url, sizeof url,
    "https://littlesis.org/api/entities/search?q=%s", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "littlesis");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *data = cJSON_GetObjectItem(root, "data");
  int emitted = 0;
  if (cJSON_IsArray(data)) {
    cJSON *e; cJSON_ArrayForEach(e, data) emitted += emit_entity(sink, e);
  } else if (cJSON_IsObject(data)) {
    emitted += emit_entity(sink, data);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[littlesis] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def littlesis_def = {
  .id = "LITTLESIS", .collector = "osint",
  .name = "LittleSis Power Network", .name_ja = "LittleSis 影響力ネットワーク",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "internal://osint/littlesis",
  .description = "LittleSis power-network — people/org relationships (keyless JSON:API entity search)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(littlesis_def)
