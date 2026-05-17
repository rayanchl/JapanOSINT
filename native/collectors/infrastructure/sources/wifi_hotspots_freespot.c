/* collectors/infrastructure/sources/wifi_hotspots_freespot.c
 * Port of server/src/collectors/wifiHotspotsFreespot.js (fetchText + regex).
 * FREESPOT venue directory → intel items (no coordinates). JS rowRegex:
 *   /<tr[^>]*>[\s\S]*?<td[^>]*>(.*?)<\/td>[\s\S]*?<td[^>]*>(.*?)<\/td>
 *    [\s\S]*?<td[^>]*>(.*?)<\/td>/gi
 * → iterate <tr> blocks, take first three <td> inner, strip tags, trim.
 * uid = wifi-hotspots-freespot|<pref>|<area>|<venue>
 *       (== intelUid(SOURCE_ID,`${prefecture}|${area}|${venue}`)). */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/htmlparse.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOURCE_URL "https://www.freespot.com/users/map_e.html"

static char *strip_dup(const char *inner, int len) {
  char *raw = strndup(inner, (size_t)len);
  if (!raw) return NULL;
  char *s = html_strip(raw);
  free(raw);
  return s;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *html = feed_get_text(ctx->http, SOURCE_URL, 15000);
  if (!html) return -1;

  int n = 0;
  const char *cur = html;
  const char *tr_inner; int tr_len;
  while ((cur = html_block(cur, "tr", &tr_inner, &tr_len)) != NULL) {
    char *row = strndup(tr_inner, (size_t)tr_len);
    if (!row) continue;

    const char *a_in, *b_in, *c_in; int a_len, b_len, c_len;
    const char *p1 = html_block(row, "td", &a_in, &a_len);
    if (!p1) { free(row); continue; }                /* <3 td → regex no match */
    const char *p2 = html_block(p1, "td", &b_in, &b_len);
    if (!p2) { free(row); continue; }
    const char *p3 = html_block(p2, "td", &c_in, &c_len);
    if (!p3) { free(row); continue; }

    char *prefecture = strip_dup(a_in, a_len);
    char *area       = strip_dup(b_in, b_len);
    char *venue      = strip_dup(c_in, c_len);
    if (!prefecture || !area || !venue) {
      free(prefecture); free(area); free(venue); free(row); continue;
    }
    if (!*area && !*venue) {                          /* if (!area && !venue) continue */
      free(prefecture); free(area); free(venue); free(row); continue;
    }

    /* title = venue || area || prefecture */
    const char *title = *venue ? venue : (*area ? area : prefecture);

    char remote_key[1536];
    snprintf(remote_key, sizeof remote_key, "%s|%s|%s", prefecture, area, venue);

    char summary[640];
    if (*prefecture)
      snprintf(summary, sizeof summary, "FREESPOT directory entry (%s)", prefecture);
    else
      snprintf(summary, sizeof summary, "FREESPOT directory entry");

    /* body: `${x || 'unknown'}` per field */
    const char *pf = *prefecture ? prefecture : "unknown";
    const char *ar = *area ? area : "unknown";
    const char *vn = *venue ? venue : "unknown";
    char body[1536];
    snprintf(body, sizeof body, "Prefecture: %s\nArea: %s\nVenue: %s", pf, ar, vn);

    cJSON *pj = cJSON_CreateObject();                 /* {prefecture, area, venue} */
    cJSON_AddStringToObject(pj, "prefecture", prefecture);
    cJSON_AddStringToObject(pj, "area", area);
    cJSON_AddStringToObject(pj, "venue", venue);
    char *pjs = cJSON_PrintUnformatted(pj);

    cJSON *tg = cJSON_CreateArray();   /* ['free-wifi','freespot',`pref:${prefecture||'unknown'}`] */
    cJSON_AddItemToArray(tg, cJSON_CreateString("free-wifi"));
    cJSON_AddItemToArray(tg, cJSON_CreateString("freespot"));
    char preftag[640];
    snprintf(preftag, sizeof preftag, "pref:%s", pf);
    cJSON_AddItemToArray(tg, cJSON_CreateString(preftag));
    char *tgs = cJSON_PrintUnformatted(tg);

    intel_item it = {0};
    it.remote_key = remote_key;        /* uid = wifi-hotspots-freespot|<pref>|<area>|<venue> */
    it.title = title;
    it.summary = summary;
    it.body = body;
    it.link = SOURCE_URL;
    it.author = "FREESPOT";
    it.lang = "ja";
    it.published_at = NULL;            /* JS published_at: null */
    it.properties_json = pjs;
    it.tags_json = tgs;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pjs); cJSON_Delete(pj);
    free(tgs); cJSON_Delete(tg);
    free(prefecture); free(area); free(venue); free(row);
  }
  free(html);
  fprintf(stderr, "[wifi-hotspots-freespot] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def wifi_hotspots_freespot_def = {
  .id = "wifi-hotspots-freespot", .collector = "infrastructure",
  .name = "FREESPOT Directory", .name_ja = "FREESPOT 一覧",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(wifi_hotspots_freespot_def)
