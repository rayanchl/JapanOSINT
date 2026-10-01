/* collectors/sources/global_wikidata.c
 * OSINT service — WIKIDATA. On-demand entity pivot (person/company/place/etc.)
 * against the Wikidata search API (wbsearchentities). Keyless, free, LIVE.
 * GET https://www.wikidata.org/w/api.php?action=wbsearchentities&search=<q>
 *     &language=en&format=json&limit=10 → "search" array of hits.
 * One intel_item per hit (Q-id, label, description). No matches / fetch
 * failure → emits nothing (honest empty). */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[1024];
  snprintf(url, sizeof url,
    "https://www.wikidata.org/w/api.php?action=wbsearchentities&search=%s"
    "&language=en&format=json&limit=10", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "wikidata");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *search = cJSON_GetObjectItem(root, "search");
  int emitted = 0;
  if (cJSON_IsArray(search)) {
    cJSON *hit;
    cJSON_ArrayForEach(hit, search) {
      const char *id    = jo_sv(hit, "id");
      const char *label = jo_sv(hit, "label");
      const char *desc  = jo_sv(hit, "description");
      const char *concept = jo_sv(hit, "concepturi");
      const char *page  = jo_sv(hit, "url");   /* //www.wikidata.org/wiki/Qxx */
      if (!id) continue;

      char link[512];
      if (concept) snprintf(link, sizeof link, "%s", concept);
      else if (page) {
        if (strncmp(page, "//", 2) == 0)
          snprintf(link, sizeof link, "https:%s", page);
        else snprintf(link, sizeof link, "%s", page);
      } else {
        snprintf(link, sizeof link, "https://www.wikidata.org/wiki/%s", id);
      }

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "id", id);
      if (label) cJSON_AddStringToObject(data, "label", label);
      if (desc)  cJSON_AddStringToObject(data, "description", desc);
      cJSON_AddStringToObject(data, "uri", link);
      cJSON_AddStringToObject(data, "source", "Wikidata");
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "WIKIDATA");
      cJSON_AddStringToObject(props, "source", "Wikidata");
      cJSON_AddStringToObject(props, "entity_id", id);
      cJSON_AddStringToObject(props, "query", q);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      intel_item it = {0};
      it.remote_key      = id;
      it.title           = label ? label : id;
      it.summary         = desc;
      it.body            = bj;
      it.lang            = "en";
      it.link            = link;
      it.record_type     = "wikidata-entity";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"WIKIDATA\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[wikidata] emitted %d\n", emitted);
  return 0;
}

static const source_def wikidata_def = {
  .id = "WIKIDATA", .collector = "osint",
  .name = "Wikidata Entity Search", .name_ja = "Wikidata エンティティ検索",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "internal://osint/wikidata",
  .description = "Wikidata wbsearchentities — keyless entity search (Q-id, label, description)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(wikidata_def)
