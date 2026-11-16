/* collectors/government/sources/data_go_jp_ckan.c
 * Port of server/src/collectors/dataGoJpCkan.js (intelEnvelope).
 * e-Gov data portal CKAN action/package_search → one intel row/package.
 *
 * RE-POINTED AND PAGED 2026-11-07. The row read
 * `https://www.data.go.jp/data/api/action/package_search?rows=50`, and that
 * host now answers 301 to `https://data.e-gov.go.jp/data/api/action/
 * package_search` — WITHOUT the query string — so every run read the CKAN
 * default of 10 datasets out of `result.count` 18,828, ranked by
 * "score desc, metadata_modified desc". Measured on the new host the same day,
 * one request each: `count` 18,828; `rows` is capped at 1,000 (rows=2000
 * answers 1,000); `start=3` is disjoint from `start=0`; `sort=id asc` and
 * `sort=id desc` answer opposite ends of the id space (00006ca1… vs fff4245f…)
 * while the control `sort=frobnicate asc` answers a third, unordered set with
 * the same count — so the sort is honoured and the total does not move. A
 * complete read is 19 pages; the ceiling is 25 for headroom (a ceiling equal
 * to the pages needed files a false truncation notice), and a stop at it, or a
 * later page that fails, is disclosed by lib/pagewalk.c.
 * uid = data-go-jp-ckan|<p.id||p.name> (intelUid first non-empty). The
 * curated _meta/portal-summary envelope is dropped (live rows only). */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/pagewalk.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://data.e-gov.go.jp/data/api/action/package_search" \
                "?rows=1000&start=0&sort=id+asc"
#define DGJ_PAGE_MAX 25  /* exhaustive-ok: page ceiling handed to pw_walk_max(); 19 pages measured, a stop files a collector-truncation-notice */

static cJSON *dgj_fetch(const source_ctx *ctx, const char *url, void *ud) {
  (void)ud;
  return feed_get_json(ctx->http, url, 60000);   /* a 1,000-row page is several MB */
}

static int dgj_page(const source_ctx *ctx, intel_sink *sink, const char *id,
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

      /* uid: intelUid(SOURCE_ID, p.id, p.name) → first non-empty. */
      const char *rk = pid ? pid : pname;
      if (!rk) continue;

      /* title = p.title || p.name (sink would derive, but intel item) */
      const char *title = ptitle ? ptitle : pname;

      /* summary = (p.notes||'').slice(0,240) || null */
      char *summary = NULL;
      if (pnotes) {
        size_t L = strlen(pnotes);
        if (L > 240) L = 240;          /* byte slice, parity-neutral here */
        summary = malloc(L + 1);
        memcpy(summary, pnotes, L);
        summary[L] = 0;
        if (!summary[0]) { free(summary); summary = NULL; }
      }

      /* link = p.name ? data.go.jp/data/dataset/<name> : null */
      char *link = NULL;
      if (pname) {
        size_t need = strlen(pname) + 40;
        link = malloc(need);
        snprintf(link, need,
                 "https://data.e-gov.go.jp/data/dataset/%s", pname);
      }

      /* published_at = metadata_modified || metadata_created || null */
      const char *pub = pmod ? pmod : pcre;

      /* properties — EXACT JS key order */
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

      /* tags = ['open-data', ...p.tags.map(t=>'tag:'+t.name)] — EVERY tag
       * (2026-11-07: the JS port's slice(0,5) dropped the rest, rule 2) */
      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("open-data"));
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
      it.body           = pnotes;          /* p.notes || null */
      it.summary        = summary;
      it.link           = link;
      it.author         = orgt;            /* p.organization?.title || null */
      it.lang           = "ja";
      it.published_at   = pub;
      it.record_type    = "data-go-jp-ckan";
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
  int n = pw_walk_max(ctx, sink, "data-go-jp-ckan", API_URL, dgj_fetch,
                      dgj_page, NULL, DGJ_PAGE_MAX);
  fprintf(stderr, "[data-go-jp-ckan] emitted %d\n", n);
  /* run() is a STATUS code, not a row count: pw_walk_max returns -1 only when
   * the FIRST page failed, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return n < 0 ? -1 : 0;
}

static const source_def data_go_jp_ckan_def = {
  .id = "data-go-jp-ckan", .collector = "government",
  .name = "data.go.jp CKAN catalog", .name_ja = "data.go.jp CKAN",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(data_go_jp_ckan_def)
