/* collectors/social/sources/linkedin_jp_search.c
 * Port of server/src/collectors/linkedinJpSearch.js (linkedin-jp-search).
 * Keyed on LINKEDIN_COOKIE (full cookie incl. li_at + JSESSIONID). The
 * quoted JSESSIONID value doubles as the Csrf-Token. Voyager blended search
 * for JP-relevant terms -> non-spatial intel. Dedup by id across queries.
 * honest empty when gated / no csrf / on failure. uid = linkedin-jp-search|<id>. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOURCE_ID "linkedin-jp-search"

static const char *QUERIES[]    = { "security%20Japan", "OSINT%20Tokyo",
                                    "CISO%20%E6%97%A5%E6%9C%AC" };
static const char *QUERY_RAW[]  = { "security Japan", "OSINT Tokyo",
                                    "CISO 日本" };
#define NQ ((int)(sizeof(QUERIES)/sizeof(QUERIES[0])))

/* el.title?.text style: object .text */
static const char *txt(const cJSON *o, const char *k) {
  if (!o) return NULL;
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return v ? jo_sv(v, "text") : NULL;
}

/* /JSESSIONID="?([^";]+)"?/ */
static int csrf_from_cookie(const char *cookie, char *out, size_t cap) {
  const char *p = cookie ? strstr(cookie, "JSESSIONID=") : NULL;
  if (!p) return 0;
  p += 11;
  if (*p == '"') p++;
  size_t i = 0;
  while (*p && *p != '"' && *p != ';' && i + 1 < cap) out[i++] = *p++;
  out[i] = '\0';
  return i > 0;
}

static int seen_has(char **a, int n, const char *s) {
  for (int i = 0; i < n; i++) if (strcmp(a[i], s) == 0) return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *cookie = getenv("LINKEDIN_COOKIE");
  if (!cookie || !*cookie) {
    fprintf(stderr, "[linkedin-jp-search] gated (no LINKEDIN_COOKIE)\n");
    return 0;
  }
  char csrf[256];
  if (!csrf_from_cookie(cookie, csrf, sizeof csrf)) {
    fprintf(stderr, "[linkedin-jp-search] no JSESSIONID -> unavailable\n");
    return -1;
  }
  char ch[2048], cf[300];
  snprintf(ch, sizeof ch, "Cookie: %s", cookie);
  snprintf(cf, sizeof cf, "Csrf-Token: %s", csrf);
  const char *hdrs[] = {
    ch, cf,
    "X-RestLi-Protocol-Version: 2.0.0",
    "Accept: application/json",
    "Referer: https://www.linkedin.com/search/results/all/",
    NULL,
  };

  char **seen = NULL; int sn = 0, scap = 0, n = 0, fetched = 0;

  for (int qi = 0; qi < NQ; qi++) {
    char url[256];
    snprintf(url, sizeof url,
      "https://www.linkedin.com/voyager/api/search/blended"
      "?keywords=%s&origin=GLOBAL_SEARCH_HEADER&count=20", QUERIES[qi]);
    cJSON *data = feed_get_json_h(ctx->http, url, hdrs, 15000);
    if (!data) continue;
    fetched++;

    cJSON *d = cJSON_GetObjectItem(data, "data");
    cJSON *elements = d ? cJSON_GetObjectItem(d, "elements") : NULL;
    if (!elements) elements = cJSON_GetObjectItem(data, "elements");

    if (cJSON_IsArray(elements)) {
      cJSON *grp;
      cJSON_ArrayForEach(grp, elements) {
        cJSON *inner = cJSON_GetObjectItem(grp, "elements");
        cJSON *list = (inner && cJSON_IsArray(inner)) ? inner : NULL;
        cJSON *el;
        /* for (el of grp.elements || [grp]) */
        cJSON *single[1] = { grp };
        int useSingle = (list == NULL);
        int idx = 0;
        for (;;) {
          if (useSingle) { if (idx >= 1) break; el = single[0]; }
          else { el = cJSON_GetArrayItem(list, idx);
                 if (!el) break; }
          idx++;

          const char *urn = jo_sv(el, "targetUrn");
          if (!urn) urn = jo_sv(el, "trackingUrn");
          if (!urn) urn = jo_sv(el, "objectUrn");
          const char *pub = jo_sv(el, "publicIdentifier");
          if (!pub) pub = jo_sv(el, "navigationUrl");
          const char *id = pub ? pub : urn;
          if (!id || !id[0]) continue;
          if (seen_has(seen, sn, id)) continue;
          if (sn == scap) { scap = scap ? scap * 2 : 32;
            seen = realloc(seen, scap * sizeof(char *)); }
          seen[sn++] = strdup(id);

          const char *title = txt(el, "title");
          if (!title) title = txt(el, "headline");
          if (!title) title = id;
          const char *summary = txt(el, "subline");
          if (!summary) summary = txt(el, "headline");
          const char *bodyT = txt(el, "summary");

          char uidb[256];
          snprintf(uidb, sizeof uidb, "%s|%s", SOURCE_ID, id);
          const char *nav = jo_sv(el, "navigationUrl");
          const char *pubId = jo_sv(el, "publicIdentifier");
          char linkb[256]; const char *link = NULL;
          if (nav) link = nav;
          else if (pubId) {
            snprintf(linkb, sizeof linkb,
                     "https://www.linkedin.com/in/%s", pubId);
            link = linkb;
          }

          cJSON *tags = cJSON_CreateArray();
          cJSON_AddItemToArray(tags, cJSON_CreateString("linkedin"));
          cJSON_AddItemToArray(tags, cJSON_CreateString("profile"));
          char qtag[64]; snprintf(qtag, sizeof qtag, "q:%s", QUERY_RAW[qi]);
          cJSON_AddItemToArray(tags, cJSON_CreateString(qtag));
          char *tj = cJSON_PrintUnformatted(tags);

          cJSON *props = cJSON_CreateObject();
          if (urn) cJSON_AddStringToObject(props, "urn", urn);
          else cJSON_AddNullToObject(props, "urn");
          cJSON_AddStringToObject(props, "matched_query", QUERY_RAW[qi]);
          cJSON_AddStringToObject(props, "source", "linkedin_voyager");
          char *pj = cJSON_PrintUnformatted(props);

          intel_item it = {0};
          it.uid = uidb;
          it.title = title;
          it.summary = summary;
          it.body = bodyT;
          it.link = link;
          it.author = title;
          it.record_type = SOURCE_ID;
          it.tags_json = tj;
          it.properties_json = pj;
          if (sink->emit(sink, &it) >= 0) n++;

          free(tj); free(pj);
          cJSON_Delete(tags); cJSON_Delete(props);
        }
      }
    }
    cJSON_Delete(data);
  }

  for (int i = 0; i < sn; i++) free(seen[i]);
  free(seen);
  fprintf(stderr, "[linkedin-jp-search] emitted %d (%d/%d queries fetched)\n",
          n, fetched, NQ);
  /* STATUS code, not a row count: a query that returns no profiles is an honest
   * empty. Only a total fetch failure is a real error. */
  return fetched > 0 ? 0 : -1;
}

static const source_def linkedin_jp_search_def = {
  .id = "linkedin-jp-search", .collector = "social",
  .name = "LinkedIn JP search", .name_ja = "LinkedIn 日本検索",
  .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(linkedin_jp_search_def)
