/* collectors/cyber/sources/urlscan_jp.c
 * Port of server/src/collectors/urlscanJp.js (intelEnvelope).
 * urlscan.io search?q=page.country:JP&size=100 → one intel row/result.
 * uid = urlscan-jp|<r._id || idx_<i>>. URLSCAN_API_KEY optional (anon works,
 * so NOT gated) — when set it raises rate limits via API-Key header. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://urlscan.io/api/v1/search/?q=page.country:JP&size=100"

static cJSON *go(cJSON *o, const char *k) {
  return o ? cJSON_GetObjectItem(o, k) : NULL;
}
/* truthy JSON string */
static const char *sv(cJSON *o, const char *k) {
  cJSON *v = go(o, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}
/* x ?? null : keep present value (incl false/0), null/absent → null */
static void put_dup_or_null(cJSON *p, const char *k, cJSON *v) {
  cJSON_AddItemToObject(p, k,
    (v && !cJSON_IsNull(v)) ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("URLSCAN_API_KEY");
  char keyh[256];
  const char *hdrs_anon[] = { "accept: application/json", NULL };
  const char *hdrs_key[]  = { "accept: application/json", keyh, NULL };
  const char *const *hdrs = hdrs_anon;
  if (key && *key) {
    snprintf(keyh, sizeof keyh, "API-Key: %s", key);
    hdrs = hdrs_key;
  }

  cJSON *json = feed_get_json_h(ctx->http, API_URL, hdrs, 15000);
  cJSON *results = json ? cJSON_GetObjectItem(json, "results") : NULL;

  int n = 0;
  if (cJSON_IsArray(results)) {
    int i = 0;
    cJSON *r;
    cJSON_ArrayForEach(r, results) {
      cJSON *page = go(r, "page");
      cJSON *task = go(r, "task");
      cJSON *verd = go(r, "verdicts");
      cJSON *overall = go(verd, "overall");

      const char *id = sv(r, "_id");

      /* uid: intelUid(SOURCE_ID, r._id, `idx_${i}`) */
      char idxbuf[32];
      const char *rk = id;
      if (!rk) { snprintf(idxbuf, sizeof idxbuf, "idx_%d", i); rk = idxbuf; }

      /* title = r.page?.url || r.page?.domain || `scan-${i}` */
      char titbuf[32];
      const char *title = sv(page, "url");
      if (!title) title = sv(page, "domain");
      if (!title) { snprintf(titbuf, sizeof titbuf, "scan-%d", i); title = titbuf; }

      /* summary = [domain,country,asnname].filter(Boolean).join(' · ')||null */
      char summ[512];
      summ[0] = '\0';
      {
        const char *parts[3] = { sv(page, "domain"), sv(page, "country"),
                                 sv(page, "asnname") };
        size_t sp = 0;
        int first = 1;
        for (int k = 0; k < 3; k++) {
          if (!parts[k]) continue;
          int wn = snprintf(summ + sp, sizeof summ - sp, "%s%s",
                            first ? "" : " \xc2\xb7 ", parts[k]);
          if (wn > 0) sp += (size_t)wn;
          if (sp >= sizeof summ) { sp = sizeof summ - 1; break; }
          first = 0;
        }
      }
      const char *summary = summ[0] ? summ : NULL;

      /* link = r.result || (r._id ? https://urlscan.io/result/<_id>/ : null) */
      char linkbuf[256];
      const char *link = sv(r, "result");
      if (!link && id) {
        snprintf(linkbuf, sizeof linkbuf, "https://urlscan.io/result/%s/", id);
        link = linkbuf;
      }

      /* published_at = r.task?.time || null */
      const char *pub = sv(task, "time");

      /* properties — EXACT JS key order */
      cJSON *props = cJSON_CreateObject();
      cJSON_AddItemToObject(props, "uuid",
        id ? cJSON_CreateString(id) : cJSON_CreateNull());
      cJSON *purl = go(page, "url");
      if (!purl || cJSON_IsNull(purl)) purl = go(task, "url");
      cJSON_AddItemToObject(props, "url",
        (purl && !cJSON_IsNull(purl)) ? cJSON_Duplicate(purl, 1)
                                      : cJSON_CreateNull());
      put_dup_or_null(props, "domain",   go(page, "domain"));
      put_dup_or_null(props, "ip",       go(page, "ip"));
      put_dup_or_null(props, "asn",      go(page, "asn"));
      put_dup_or_null(props, "asn_name", go(page, "asnname"));
      put_dup_or_null(props, "country",  go(page, "country"));
      put_dup_or_null(props, "city",     go(page, "city"));
      put_dup_or_null(props, "screenshot", go(r, "screenshot"));
      put_dup_or_null(props, "verdicts_malicious", go(overall, "malicious"));
      put_dup_or_null(props, "verdicts_score",     go(overall, "score"));
      char *pj = cJSON_PrintUnformatted(props);

      /* tags = ['urlscan', malicious?'malicious':null,
       *         ...task.tags.slice(0,5)].filter(Boolean) */
      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("urlscan"));
      cJSON *mal = go(overall, "malicious");
      if (cJSON_IsTrue(mal))
        cJSON_AddItemToArray(tags, cJSON_CreateString("malicious"));
      cJSON *ttags = go(task, "tags");
      if (cJSON_IsArray(ttags)) {
        int ti = 0;
        cJSON *t;
        cJSON_ArrayForEach(t, ttags) {
          if (ti++ >= 5) break;
          /* .filter(Boolean): drop falsy entries */
          if (cJSON_IsString(t) && t->valuestring[0])
            cJSON_AddItemToArray(tags, cJSON_CreateString(t->valuestring));
          else if (cJSON_IsNumber(t) && t->valuedouble != 0)
            cJSON_AddItemToArray(tags, cJSON_Duplicate(t, 1));
        }
      }
      char *tj = cJSON_PrintUnformatted(tags);

      intel_item it = {0};
      it.remote_key     = rk;
      it.title          = title;
      it.summary        = summary;
      it.link           = link;
      it.lang           = "en";
      it.published_at   = pub;
      it.record_type    = "urlscan-jp";
      it.properties_json = pj;
      it.tags_json      = tj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(pj); free(tj);
      cJSON_Delete(props); cJSON_Delete(tags);
      i++;
    }
  }
  if (json) cJSON_Delete(json);
  fprintf(stderr, "[urlscan-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def urlscan_jp_def = {
  .id = "urlscan-jp", .collector = "cyber",
  .name = "URLscan.io (JP)", .name_ja = "URLscan.io 日本",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(urlscan_jp_def)
