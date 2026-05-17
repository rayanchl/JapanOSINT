/* collectors/cyber/sources/grayhat_buckets.c
 * Port of server/src/collectors/grayhatBuckets.js (intelEnvelope).
 * GrayhatWarfare /api/v2/buckets?keywords=...&limit=100, Bearer key.
 * Gated on GRAYHAT_API_KEY (JS: no key → empty → 0 rows).
 * uid = grayhat-buckets|<b.id || b.bucket||b.name || idx_<i>>. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEYWORDS \
  "co.jp,go.jp,japan,tokyo,rakuten,mizuho,mufg,smbc," \
  "softbank,docomo,kddi,ntt"

/* JS truthy string (non-empty). */
static const char *sv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}
/* a || b (string-truthy). */
static const char *sor(const cJSON *o, const char *a, const char *b) {
  const char *x = sv(o, a);
  return x ? x : sv(o, b);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("GRAYHAT_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[grayhat-buckets] gated (no GRAYHAT_API_KEY)\n");
    return 0;
  }

  const char *url =
    "https://buckets.grayhatwarfare.com/api/v2/buckets"
    "?keywords=" KEYWORDS "&limit=100";
  char authh[256];
  snprintf(authh, sizeof authh, "Authorization: Bearer %s", key);
  const char *hdrs[] = { "Accept: application/json", authh, NULL };

  cJSON *json = feed_get_json_h(ctx->http, url, hdrs, 15000);
  if (!json) return -1;

  /* buckets = json.buckets || json.results || (Array.isArray(json)?json:[]) */
  cJSON *buckets = cJSON_GetObjectItem(json, "buckets");
  if (!cJSON_IsArray(buckets)) buckets = cJSON_GetObjectItem(json, "results");
  if (!cJSON_IsArray(buckets) && cJSON_IsArray(json)) buckets = json;

  int n = 0, i = 0;
  if (cJSON_IsArray(buckets)) {
    cJSON *b;
    cJSON_ArrayForEach(b, buckets) {
      const char *bucket = sor(b, "bucket", "name");  /* b.bucket || b.name */
      const char *btype  = sv(b, "type");
      const char *region = sv(b, "region");
      const char *provider = sv(b, "provider");
      const char *lupd   = sor(b, "lastUpdated", "last_updated");

      /* files_count: b.filesCount ?? b.files_count ?? null (nullish, 0 ok) */
      const cJSON *fc = cJSON_GetObjectItem(b, "filesCount");
      if (!fc || cJSON_IsNull(fc)) fc = cJSON_GetObjectItem(b, "files_count");
      int has_fc = fc && !cJSON_IsNull(fc);

      const cJSON *pubv = cJSON_GetObjectItem(b, "public");

      /* uid: intelUid(SOURCE_ID, b.id, b.bucket||b.name, 'idx_'+i) */
      const cJSON *idv = cJSON_GetObjectItem(b, "id");
      char idbuf[64]; const char *idcand = NULL;
      if (idv && cJSON_IsString(idv) && idv->valuestring && idv->valuestring[0])
        idcand = idv->valuestring;
      else if (idv && cJSON_IsNumber(idv)) {
        if (idv->valuedouble == (double)(long long)idv->valuedouble)
          snprintf(idbuf, sizeof idbuf, "%lld", (long long)idv->valuedouble);
        else snprintf(idbuf, sizeof idbuf, "%g", idv->valuedouble);
        idcand = idbuf;
      }
      char idxbuf[24]; snprintf(idxbuf, sizeof idxbuf, "idx_%d", i);
      const char *rk = idcand ? idcand : (bucket ? bucket : idxbuf);

      /* title = b.bucket || b.name || `bucket-${i}` */
      char titlebuf[24]; const char *title = bucket;
      if (!title) { snprintf(titlebuf, sizeof titlebuf, "bucket-%d", i);
                    title = titlebuf; }

      /* summary = `${b.type||'unknown'} · ${filesCount ?? files_count ?? '?'} files` */
      char summary[128];
      char fcs[40];
      if (has_fc) {
        if (cJSON_IsNumber(fc)) {
          if (fc->valuedouble == (double)(long long)fc->valuedouble)
            snprintf(fcs, sizeof fcs, "%lld", (long long)fc->valuedouble);
          else snprintf(fcs, sizeof fcs, "%g", fc->valuedouble);
        } else if (cJSON_IsString(fc) && fc->valuestring) {
          snprintf(fcs, sizeof fcs, "%s", fc->valuestring);
        } else snprintf(fcs, sizeof fcs, "?");
      } else snprintf(fcs, sizeof fcs, "?");
      snprintf(summary, sizeof summary, "%s \xc2\xb7 %s files",
               btype ? btype : "unknown", fcs);

      /* tags = ['exposed-bucket', b.type ? 'provider:'+type : null].filter */
      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("exposed-bucket"));
      if (btype) {
        char tb[128]; snprintf(tb, sizeof tb, "provider:%s", btype);
        cJSON_AddItemToArray(tags, cJSON_CreateString(tb));
      }
      char *tj = cJSON_PrintUnformatted(tags);

      /* properties — EXACT JS key order */
      cJSON *p = cJSON_CreateObject();
      cJSON_AddItemToObject(p, "bucket",
        bucket ? cJSON_CreateString(bucket) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "type",
        btype ? cJSON_CreateString(btype) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "files_count",
        has_fc ? cJSON_Duplicate(fc, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "last_updated",
        lupd ? cJSON_CreateString(lupd) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "public",
        (pubv && !cJSON_IsNull(pubv)) ? cJSON_Duplicate(pubv, 1)
                                      : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "region",
        region ? cJSON_CreateString(region) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "provider",
        provider ? cJSON_CreateString(provider) : cJSON_CreateNull());
      char *pj = cJSON_PrintUnformatted(p);

      intel_item it = {0};
      it.remote_key     = rk;
      it.title          = title;
      it.summary        = summary;
      it.lang           = "en";
      it.published_at   = lupd;          /* b.lastUpdated||b.last_updated||null */
      it.record_type    = "grayhat-buckets";
      it.properties_json = pj;
      it.tags_json      = tj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(pj); free(tj);
      cJSON_Delete(p); cJSON_Delete(tags);
      i++;
    }
  }
  cJSON_Delete(json);
  fprintf(stderr, "[grayhat-buckets] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def grayhat_buckets_def = {
  .id = "grayhat-buckets", .collector = "cyber",
  .name = "GrayhatWarfare (JP buckets)", .name_ja = "GrayhatWarfare (JPバケット)",
   .update_interval_sec = 21600, .run = run,
};
REGISTER_SOURCE(grayhat_buckets_def)
