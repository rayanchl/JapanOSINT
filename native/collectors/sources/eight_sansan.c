/* collectors/social/sources/eight_sansan.c
 * Port of server/src/collectors/eightSansan.js (eight-sansan).
 * Keyed on EIGHT_SESSION (sent as Cookie header). No unauth public API ->
 * honest empty when absent / on failure. Non-spatial intel.
 * uid = eight-sansan|<id>. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOURCE_ID "eight-sansan"

/* c.id || c.card_id as string (string or number). */
static int card_id(const cJSON *c, char *out, size_t cap) {
  const char *s = jo_sv(c, "id");
  if (!s) s = jo_sv(c, "card_id");
  if (s) { snprintf(out, cap, "%s", s); return 1; }
  const cJSON *v = cJSON_GetObjectItem(c, "id");
  if (!v) v = cJSON_GetObjectItem(c, "card_id");
  if (v && cJSON_IsNumber(v)) { snprintf(out, cap, "%g", v->valuedouble); return 1; }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *session = getenv("EIGHT_SESSION");
  if (!session || !*session) {
    fprintf(stderr, "[eight-sansan] gated (no EIGHT_SESSION)\n");
    return 0;
  }
  char cookie[1024];
  snprintf(cookie, sizeof cookie, "Cookie: %s", session);
  const char *hdrs[] = { cookie, "accept: application/json", NULL };

  cJSON *data = feed_get_json_h(ctx->http,
    "https://8card.net/api/v2/cards/search?limit=50", hdrs, 12000);
  cJSON *cards = data ? cJSON_GetObjectItem(data, "cards") : NULL;
  if (!cards || !cJSON_IsArray(cards))
    cards = data ? cJSON_GetObjectItem(data, "data") : NULL;

  int n = 0;
  if (cJSON_IsArray(cards)) {
    cJSON *c;
    cJSON_ArrayForEach(c, cards) {
      char id[64];
      if (!card_id(c, id, sizeof id)) continue;

      const char *name = jo_sv(c, "name");
      const char *ttl = jo_sv(c, "title");
      const char *comp = jo_sv(c, "company");
      const char *email = jo_sv(c, "email");

      char uidb[128];
      snprintf(uidb, sizeof uidb, "%s|%s", SOURCE_ID, id);

      /* title: `${name||''} — ${title||''}`.trim() */
      char title[512];
      snprintf(title, sizeof title, "%s — %s", name ? name : "",
               ttl ? ttl : "");
      { /* JS .trim() both ends */
        char *p = title; while (*p == ' ') p++;
        char *e = p + strlen(p); while (e > p && e[-1] == ' ') *--e = '\0';
        if (p != title) memmove(title, p, strlen(p) + 1);
      }

      /* body: [name,title,company].filter(Boolean).join(' / ') */
      char body[1024]; body[0] = '\0';
      int first = 1;
      const char *parts[3] = { name, ttl, comp };
      for (int i = 0; i < 3; i++) {
        if (parts[i] && parts[i][0]) {
          if (!first) strncat(body, " / ", sizeof body - strlen(body) - 1);
          strncat(body, parts[i], sizeof body - strlen(body) - 1);
          first = 0;
        }
      }

      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("eight"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("business-card"));
      char *tj = cJSON_PrintUnformatted(tags);

      cJSON *props = cJSON_CreateObject();
      if (comp) cJSON_AddStringToObject(props, "company", comp);
      else cJSON_AddNullToObject(props, "company");
      if (ttl) cJSON_AddStringToObject(props, "role", ttl);
      else cJSON_AddNullToObject(props, "role");
      if (email) cJSON_AddStringToObject(props, "email", email);
      else cJSON_AddNullToObject(props, "email");
      cJSON_AddStringToObject(props, "source", "eight_api");
      char *pj = cJSON_PrintUnformatted(props);

      intel_item it = {0};
      it.uid = uidb;
      it.title = title;
      it.summary = comp;
      it.body = body[0] ? body : NULL;
      it.author = name;
      it.lang = "ja";
      it.record_type = SOURCE_ID;
      it.tags_json = tj;
      it.properties_json = pj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(tj); free(pj);
      cJSON_Delete(tags); cJSON_Delete(props);
    }
  }
  int fetched = data != NULL;
  if (data) cJSON_Delete(data);
  fprintf(stderr, "[eight-sansan] emitted %d\n", n);
  /* STATUS code, not a row count: only a failed fetch is an error. An empty
   * card list is an honest empty and must not quarantine the source. */
  return fetched ? 0 : -1;
}

static const source_def eight_sansan_def = {
  .id = "eight-sansan", .collector = "social",
  .name = "Eight / Sansan public cards", .name_ja = "Eight / Sansan 名刺",
  .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(eight_sansan_def)
