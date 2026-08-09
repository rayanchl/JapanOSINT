/* collectors/cyber/sources/google_dorking.c
 * Port of server/src/collectors/googleDorking.js (intelEnvelope).
 * Keyed anyOf: GOOGLE_CSE_KEY(+GOOGLE_CSE_CX) | SERPAPI_KEY. No key → 0 rows.
 * uid = google-dorking|<r.link> (remote_key = link). No seed. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *DORKS[] = {
  "site:go.jp filetype:xlsx",
  "site:co.jp intitle:\"index of\" \"backup\"",
  "site:lg.jp filetype:pdf \"\xE5\x80\x8B\xE4\xBA\xBA\xE6\x83\x85\xE5\xA0\xB1\"",
  "site:jp inurl:admin intitle:login",
  "site:ac.jp filetype:env OR filetype:sql",
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *cseKey = getenv("GOOGLE_CSE_KEY");
  const char *cseCx  = getenv("GOOGLE_CSE_CX");
  const char *serpKey = getenv("SERPAPI_KEY");
  int hasCse  = cseKey && *cseKey && cseCx && *cseCx;
  int hasSerp = serpKey && *serpKey;
  if (!(hasCse || hasSerp)) {
    fprintf(stderr, "[google-dorking] gated (no GOOGLE_CSE_KEY/CX or SERPAPI_KEY)\n");
    return 0;
  }

  int n = 0;
  size_t nd = sizeof(DORKS) / sizeof(DORKS[0]);
  for (size_t di = 0; di < nd; di++) {
    const char *q = DORKS[di];
    char qenc[512];
    jo_uri_encode_buf(q, qenc, sizeof qenc);
    char url[1024];
    if (hasCse)
      snprintf(url, sizeof url,
        "https://www.googleapis.com/customsearch/v1?key=%s&cx=%s&num=10&q=%s",
        cseKey, cseCx, qenc);
    else
      snprintf(url, sizeof url,
        "https://serpapi.com/search.json?engine=google&gl=jp&hl=ja&num=10"
        "&api_key=%s&q=%s", serpKey, qenc);

    cJSON *data = feed_get_json(ctx->http, url, hasCse ? 15000 : 20000);
    if (!data) continue;
    cJSON *list = cJSON_GetObjectItem(data,
      hasCse ? "items" : "organic_results");
    if (cJSON_IsArray(list)) {
      cJSON *r;
      cJSON_ArrayForEach(r, list) {
        const char *link = jo_sv(r, "link");
        const char *title = jo_sv(r, "title");
        const char *snippet = jo_sv(r, "snippet");
        if (!link) continue;            /* uid keyed on link */

        cJSON *tags = cJSON_CreateArray();
        cJSON_AddItemToArray(tags, cJSON_CreateString("google-dork"));
        cJSON_AddItemToArray(tags,
          cJSON_CreateString(hasCse ? "cse" : "serpapi"));
        char qtag[560];
        snprintf(qtag, sizeof qtag, "q:%s", q);
        cJSON_AddItemToArray(tags, cJSON_CreateString(qtag));
        char *tj = cJSON_PrintUnformatted(tags);

        cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
        cJSON_AddStringToObject(p, "dork", q);
        if (hasCse) {
          const char *dl = jo_sv(r, "displayLink");
          cJSON_AddItemToObject(p, "display_link",
            dl ? cJSON_CreateString(dl) : cJSON_CreateNull());
          cJSON_AddStringToObject(p, "source", "google_cse_api");
        } else {
          cJSON *pos = cJSON_GetObjectItem(r, "position");
          cJSON_AddItemToObject(p, "position",
            (pos && cJSON_IsNumber(pos)) ? cJSON_Duplicate(pos, 1)
                                         : cJSON_CreateNull());
          cJSON_AddStringToObject(p, "source", "serpapi");
        }
        char *pj = cJSON_PrintUnformatted(p);

        const char *author = hasCse ? jo_sv(r, "displayLink")
                                    : jo_sv(r, "displayed_link");
        const char *pub = hasCse ? NULL : jo_sv(r, "date");

        intel_item it = {0};
        it.remote_key     = link;
        it.title          = title ? title : link;
        it.summary        = snippet;
        it.body           = snippet;
        it.link           = link;
        it.author         = author;
        it.lang           = "ja";
        it.published_at   = pub;
        it.record_type    = "google-dorking";
        it.properties_json = pj;
        it.tags_json      = tj;
        if (sink->emit(sink, &it) >= 0) n++;

        free(pj); free(tj);
        cJSON_Delete(p); cJSON_Delete(tags);
      }
    }
    cJSON_Delete(data);
  }
  fprintf(stderr, "[google-dorking] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def google_dorking_def = {
  .id = "google-dorking", .collector = "cyber",
  .name = "Google Dorking Results",
  .name_ja = "Google Dork \xE7\xB5\x90\xE6\x9E\x9C",
   .update_interval_sec = 7200, .run = run,
};
REGISTER_SOURCE(google_dorking_def)
