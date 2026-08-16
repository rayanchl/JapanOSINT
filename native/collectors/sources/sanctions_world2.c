/* collectors/sources/sanctions_world2.c
 * OSINT services — net-new keyless watchlist / wanted-person lookups pivoting on
 * ctx->entity (a person or org name). Shared run() switches on ctx->source_id.
 * Real fetch or honest-empty. No API keys.
 *
 *   FBI_WANTED        api.fbi.gov/@wanted?title=<name>            (JSON, keyless)
 *   OPENSANCTIONS     api.opensanctions.org/search/default?q=<n>  (JSON, key req) */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *SW_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

/* ---- FBI Wanted (api.fbi.gov) -------------------------------------------- */
static int run_fbi(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://api.fbi.gov/@wanted?pageSize=20&title=%s", enc);
  const char *hdrs[] = { "Accept: application/json", SW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "FBI_WANTED");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *items = cJSON_GetObjectItem(root, "items");
  int emitted = 0;
  if (cJSON_IsArray(items)) {
    const cJSON *it0;
    cJSON_ArrayForEach(it0, items) {
      const char *title = jo_sv(it0, "title");
      const char *uid = jo_sv(it0, "uid");
      const char *urlf = jo_sv(it0, "url");
      const char *desc = jo_sv(it0, "description");
      if (!title && !uid) continue;
      cJSON *d = cJSON_CreateObject();
      if (title) cJSON_AddStringToObject(d, "name", title);
      if (desc)  cJSON_AddStringToObject(d, "description", desc);
      const char *subj = NULL;
      const cJSON *subs = cJSON_GetObjectItem(it0, "subjects");
      if (cJSON_IsArray(subs)) { const cJSON *s0 = cJSON_GetArrayItem(subs, 0);  /* exhaustive-ok: display pick; subjects_all carries every subject */ if (cJSON_IsString(s0)) subj = s0->valuestring; }
      if (cJSON_IsArray(subs) && cJSON_GetArraySize(subs) > 1)
        cJSON_AddItemToObject(d, "subjects_all", cJSON_Duplicate(subs, 1));
      if (subj) cJSON_AddStringToObject(d, "category", subj);
      cJSON_AddStringToObject(d, "source", "FBI Wanted");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "FBI_WANTED");
      if (uid) cJSON_AddStringToObject(p, "uid", uid);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      intel_item it = {0};
      it.remote_key = uid ? uid : title;
      it.title = title ? title : uid;
      it.summary = subj;
      it.body = bj; it.link = urlf; it.lang = "en";
      it.record_type = "wanted-person";
      it.properties_json = pj;
      it.tags_json = "[\"osint-search\",\"FBI_WANTED\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return emitted;
}

/* ---- OpenSanctions (hosted API; key optional via OPENSANCTIONS_API_KEY) --- */
static int run_opensanctions(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url,
    "https://api.opensanctions.org/search/default?q=%s&limit=20", enc);
  const char *key = jo_env("OPENSANCTIONS_API_KEY");
  char authhdr[128] = {0};
  if (key) snprintf(authhdr, sizeof authhdr, "Authorization: ApiKey %s", key);
  const char *hdrs[] = { "Accept: application/json", SW_UA, key ? authhdr : NULL, NULL };
  char *body = jo_get(ctx, url, hdrs, "OPENSANCTIONS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *caption = jo_sv(r, "caption");
      const char *schema = jo_sv(r, "schema");
      const char *id = jo_sv(r, "id");
      if (!caption && !id) continue;
      cJSON *d = cJSON_CreateObject();
      if (caption) cJSON_AddStringToObject(d, "name", caption);
      if (schema)  cJSON_AddStringToObject(d, "schema", schema);
      const cJSON *datasets = cJSON_GetObjectItem(r, "datasets");
      if (cJSON_IsArray(datasets)) cJSON_AddItemReferenceToObject(d, "datasets", (cJSON *)datasets);
      cJSON_AddStringToObject(d, "source", "OpenSanctions");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "OPENSANCTIONS");
      if (id) cJSON_AddStringToObject(p, "os_id", id);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char link[256] = {0};
      if (id) snprintf(link, sizeof link, "https://www.opensanctions.org/entities/%s/", id);
      intel_item it = {0};
      it.remote_key = id ? id : caption;
      it.title = caption ? caption : id;
      it.summary = schema;
      it.body = bj; it.link = link[0] ? link : NULL; it.lang = "en";
      it.record_type = "sanctions-entity";
      it.properties_json = pj;
      it.tags_json = "[\"osint-search\",\"OPENSANCTIONS\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "FBI_WANTED"))    n = run_fbi(ctx, sink, enc);
  else if (!strcmp(sid, "OPENSANCTIONS")) n = run_opensanctions(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define SW_DEF(sym, ID, NM, CAT, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = CAT, \
    .type = "api", .url = "internal://osint/sanctions", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

SW_DEF(sw_fbi_def, "FBI_WANTED", "FBI Wanted", "investigation",
  "FBI Wanted keyless API: wanted persons by name (title, description, category).");
SW_DEF(sw_opensanctions_def, "OPENSANCTIONS", "OpenSanctions Search", "government",
  "OpenSanctions consolidated sanctions/PEP search by name (key optional).");
