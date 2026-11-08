/* collectors/geospatial/sources/geospatial_jp_ckan.c
 * Port of server/src/collectors/geospatialJpCkan.js (intelEnvelope).
 * geospatial.jp CKAN action/package_search → one intel row/package.
 *
 * PAGED 2026-11-07. It read `rows=50&sort=id asc` in ONE request, so it
 * stored the same first 50 datasets by id every run out of `result.count`
 * 17,308. Measured the same day, one request each: rows is capped at 1,000
 * (rows=2000 answers 1,000); start=3 is disjoint from start=0; `id asc` and
 * `id desc` answer opposite ends (00050d40… vs fffffe33…) and the control
 * `sort=frobnicate asc` is a Solr 400 naming the field, so the sort is
 * validated and honoured. 18 pages; ceiling 22 for headroom, any stop
 * disclosed by lib/pagewalk.c.
 * uid = geospatial-jp-ckan|<p.id||p.name>. _meta envelope dropped. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/pagewalk.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL \
  "https://www.geospatial.jp/ckan/api/3/action/package_search?rows=1000&start=0&sort=id%20asc"
#define GSJ_PAGE_MAX 22  /* exhaustive-ok: page ceiling handed to pw_walk_max(); 18 pages measured, a stop files a collector-truncation-notice */

static cJSON *gsj_fetch(const source_ctx *ctx, const char *url, void *ud) {
  (void)ud;
  return feed_get_json(ctx->http, url, 60000);
}

static int gsj_page(const source_ctx *ctx, intel_sink *sink, const char *id,
                    cJSON *data, void *ud, int *seen) {
  (void)ctx; (void)id; (void)ud;
  *seen = 0;
  cJSON *result = cJSON_GetObjectItem(data, "result");
  cJSON *results = result ? cJSON_GetObjectItem(result, "results") : NULL;
  int n = 0;
  if (cJSON_IsArray(results)) {
    *seen = cJSON_GetArraySize(results);
    cJSON *p;
    cJSON_ArrayForEach(p, results) {
      const char *pid   = jo_sv(p, "id");
      const char *pname = jo_sv(p, "name");
      const char *ptitle= jo_sv(p, "title");
      const char *pnotes= jo_sv(p, "notes");
      const cJSON *org  = cJSON_GetObjectItem(p, "organization");
      const char *orgt  = (org && cJSON_IsObject(org)) ? jo_sv(org, "title") : NULL;
      const char *plic  = jo_sv(p, "license_id");
      const char *pmod  = jo_sv(p, "metadata_modified");
      const char *pcre  = jo_sv(p, "metadata_created");

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

      /* tags = ['geospatial','mlit', ...p.tags.map('tag:'+name)] — every tag
       * (2026-11-07: the JS port's slice(0,5) dropped the rest, rule 2) */
      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("geospatial"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("mlit"));
      const cJSON *ptags = cJSON_GetObjectItem(p, "tags");
      if (cJSON_IsArray(ptags)) {
        cJSON *t;
        cJSON_ArrayForEach(t, ptags) {
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
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = pw_walk_max(ctx, sink, "geospatial-jp-ckan", API_URL, gsj_fetch,
                      gsj_page, NULL, GSJ_PAGE_MAX);
  fprintf(stderr, "[geospatial-jp-ckan] emitted %d\n", n);
  /* run() is a STATUS code, not a row count: pw_walk_max returns -1 only when
   * the FIRST page failed, so reaching here with zero rows is an honest empty. */
  return n < 0 ? -1 : 0;
}

static const source_def geospatial_jp_ckan_def = {
  .id = "geospatial-jp-ckan", .collector = "geospatial",
  .name = "geospatial.jp CKAN catalog", .name_ja = "地理空間情報 CKAN",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(geospatial_jp_ckan_def)
