/* collectors/government/sources/edinet_filings.c
 * Port of server/src/collectors/edinetFilings.js (intelEnvelope).
 * FSA EDINET documents.json (Subscription-Key) → one intel row/filing.
 * Gated on EDINET_API_KEY (JS: no key → seed, which we drop → 0 rows).
 * uid = edinet-filings|<d.docID || idx_<i>> (intelUid first non-empty).
 * published_at mirrors d.submitDateTime (source string passthrough; JS does
 * new Date(x).toISOString() — server-tz-coupled, not in uid/props). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* JS truthy string. */
static const char *sv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("EDINET_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[edinet-filings] gated (no EDINET_API_KEY)\n");
    return 0;
  }

  /* today() = new Date().toISOString().slice(0,10) (UTC date) */
  time_t now = time(NULL);
  struct tm g; gmtime_r(&now, &g);
  char day[11];
  snprintf(day, sizeof day, "%04d-%02d-%02d",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday);

  char url[512];
  snprintf(url, sizeof url,
    "https://api.edinet-fsa.go.jp/api/v2/documents.json"
    "?date=%s&type=2&Subscription-Key=%s", day, key);

  cJSON *data = feed_get_json(ctx->http, url, 10000);
  if (!data) return -1;

  cJSON *list = cJSON_GetObjectItem(data, "results");
  int n = 0, i = 0;
  if (cJSON_IsArray(list)) {
    cJSON *d;
    cJSON_ArrayForEach(d, list) {
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
      const char *docID  = sv(d, "docID");
      const char *filer  = sv(d, "filerName");
      const char *ddesc  = sv(d, "docDescription");
      const char *fcode  = sv(d, "formCode");
      const char *dtcode = sv(d, "docTypeCode");
      const char *seccode= sv(d, "secCode");
      const char *edcode = sv(d, "edinetCode");
      const char *subdt  = sv(d, "submitDateTime");

      /* uid: intelUid(SOURCE_ID, d.docID, 'idx_'+i) → first non-empty */
      char idxbuf[24];
      snprintf(idxbuf, sizeof idxbuf, "idx_%d", i);
      const char *rk = docID ? docID : idxbuf;

      /* title = d.filerName
       *   ? `${filerName} — ${docDescription||formCode||'filing'}`
       *   : (docDescription || `Filing ${i}`) */
      char title[512];
      if (filer) {
        const char *tail = ddesc ? ddesc : (fcode ? fcode : "filing");
        snprintf(title, sizeof title, "%s — %s", filer, tail);
      } else if (ddesc) {
        snprintf(title, sizeof title, "%s", ddesc);
      } else {
        snprintf(title, sizeof title, "Filing %d", i);
      }

      /* link = d.docID ? disclosure2 URL : null */
      char *link = NULL;
      if (docID) {
        size_t need = strlen(docID) + 64;
        link = malloc(need);
        snprintf(link, need,
          "https://disclosure2.edinet-fsa.go.jp/WEEK0010.aspx?docID=%s",
          docID);
      }

      /* tags = ['filing', formCode ? 'form:'+formCode : null].filter */
      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("filing"));
      if (fcode) {
        char tb[96];
        snprintf(tb, sizeof tb, "form:%s", fcode);
        cJSON_AddItemToArray(tags, cJSON_CreateString(tb));
      }
      char *tj = cJSON_PrintUnformatted(tags);

      /* properties — EXACT JS key order, each is `X || null` */
      cJSON *p = cJSON_CreateObject();
      cJSON_AddItemToObject(p, "doc_id",
        docID ? cJSON_CreateString(docID) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "form",
        fcode ? cJSON_CreateString(fcode) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "doc_type_code",
        dtcode ? cJSON_CreateString(dtcode) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "sec_code",
        seccode ? cJSON_CreateString(seccode) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "edinet_code",
        edcode ? cJSON_CreateString(edcode) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "submit_date",
        subdt ? cJSON_CreateString(subdt) : cJSON_CreateNull());
      char *pj = cJSON_PrintUnformatted(p);

      intel_item it = {0};
      it.remote_key     = rk;
      it.title          = title;
      it.summary        = ddesc;            /* d.docDescription || null */
      it.author         = filer;            /* d.filerName || null */
      it.lang           = "ja";
      it.published_at   = subdt;            /* d.submitDateTime (passthrough) */
      it.link           = link;
      it.record_type    = "edinet-filings";
      it.properties_json = pj;
      it.tags_json      = tj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(pj); free(tj); free(link);
      cJSON_Delete(p); cJSON_Delete(tags);
      i++;
    }
  }
  cJSON_Delete(data);
  fprintf(stderr, "[edinet-filings] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def edinet_filings_def = {
  .id = "edinet-filings", .collector = "government",
  .name = "FSA EDINET Corporate Filings", .name_ja = "EDINET 有価証券報告書",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(edinet_filings_def)
