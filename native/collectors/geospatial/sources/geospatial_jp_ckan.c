/* collectors/geospatial/sources/geospatial_jp_ckan.c
 * Port of server/src/collectors/geospatialJpCkan.js (intelEnvelope).
 * geospatial.jp CKAN action/package_search?rows=50 → one intel row/package.
 * uid = geospatial-jp-ckan|<p.id||p.name>. _meta envelope dropped. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL \
  "https://www.geospatial.jp/ckan/api/3/action/package_search?rows=50"

static const char *sv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *data = feed_get_json(ctx->http, API_URL, 10000);
  if (!data) return -1;

  cJSON *result = cJSON_GetObjectItem(data, "result");
  cJSON *results = result ? cJSON_GetObjectItem(result, "results") : NULL;
  int n = 0;
  if (cJSON_IsArray(results)) {
    cJSON *p;
    cJSON_ArrayForEach(p, results) {
      const char *pid   = sv(p, "id");
      const char *pname = sv(p, "name");
      const char *ptitle= sv(p, "title");
      const char *pnotes= sv(p, "notes");
      const cJSON *org  = cJSON_GetObjectItem(p, "organization");
      const char *orgt  = (org && cJSON_IsObject(org)) ? sv(org, "title") : NULL;
      const char *plic  = sv(p, "license_id");
      const char *pmod  = sv(p, "metadata_modified");
      const char *pcre  = sv(p, "metadata_created");

      const char *rk = pid ? pid : pname;
      if (!rk) continue;

      const char *title = ptitle ? ptitle : pname;

      char *summary = NULL;
      if (pnotes) {
        size_t L = strlen(pnotes);
        if (L > 240) L = 240;
        summary = malloc(L + 1);
        memcpy(summary, pnotes, L);
        summary[L] = 0;
        if (!summary[0]) { free(summary); summary = NULL; }
      }

      char *link = NULL;
      if (pname) {
        size_t need = strlen(pname) + 48;
        link = malloc(need);
        snprintf(link, need,
                 "https://www.geospatial.jp/ckan/dataset/%s", pname);
      }

      const char *pub = pmod ? pmod : pcre;

      cJSON *props = cJSON_CreateObject();
      cJSON_AddItemToObject(props, "ckan_id",
        pid ? cJSON_CreateString(pid) : cJSON_CreateNull());
      cJSON_AddItemToObject(props, "ckan_name",
        pname ? cJSON_CreateString(pname) : cJSON_CreateNull());
      cJSON_AddItemToObject(props, "organization",
        orgt ? cJSON_CreateString(orgt) : cJSON_CreateNull());
      cJSON_AddItemToObject(props, "license_id",
        plic ? cJSON_CreateString(plic) : cJSON_CreateNull());
      const cJSON *res = cJSON_GetObjectItem(p, "resources");
      cJSON_AddNumberToObject(props, "resources_count",
        cJSON_IsArray(res) ? cJSON_GetArraySize(res) : 0);
      char *pj = cJSON_PrintUnformatted(props);

      /* tags = ['geospatial','mlit', ...p.tags.slice(0,5).map('tag:'+name)] */
      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("geospatial"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("mlit"));
      const cJSON *ptags = cJSON_GetObjectItem(p, "tags");
      if (cJSON_IsArray(ptags)) {
        int ti = 0;
        cJSON *t;
        cJSON_ArrayForEach(t, ptags) {
          if (ti++ >= 5) break;
          const cJSON *tn = cJSON_GetObjectItem(t, "name");
          char buf[256];
          snprintf(buf, sizeof buf, "tag:%s",
            (tn && cJSON_IsString(tn) && tn->valuestring)
              ? tn->valuestring : "");
          cJSON_AddItemToArray(tags, cJSON_CreateString(buf));
        }
      }
      char *tj = cJSON_PrintUnformatted(tags);

      intel_item it = {0};
      it.remote_key     = rk;
      it.title          = title;
      it.body           = pnotes;
      it.summary        = summary;
      it.link           = link;
      it.author         = orgt;
      it.lang           = "ja";
      it.published_at   = pub;
      it.record_type    = "geospatial-jp-ckan";
      it.properties_json = pj;
      it.tags_json      = tj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(pj); free(tj); free(summary); free(link);
      cJSON_Delete(props); cJSON_Delete(tags);
    }
  }
  cJSON_Delete(data);
  fprintf(stderr, "[geospatial-jp-ckan] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def geospatial_jp_ckan_def = {
  .id = "geospatial-jp-ckan", .collector = "geospatial",
  .name = "geospatial.jp CKAN catalog", .name_ja = "地理空間情報 CKAN",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(geospatial_jp_ckan_def)
