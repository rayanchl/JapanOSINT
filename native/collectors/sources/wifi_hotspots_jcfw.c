/* collectors/infrastructure/sources/wifi_hotspots_jcfw.c
 * Port of server/src/collectors/wifiHotspotsJcfw.js (fetchText + regex rows).
 * NTT-BP "Japan Connected Free Wi-Fi" venue directory → intel items
 * (no coordinates). JS rowRegex:
 *   /<tr[^>]*>[\s\S]*?<td[^>]*>(.*?)<\/td>[\s\S]*?<td[^>]*>(.*?)<\/td>/gi
 * → iterate <tr> blocks, take first two <td> inner, strip tags, trim.
 * uid = wifi-hotspots-jcfw|<area>|<venue> (== intelUid(SOURCE_ID,`${area}|${venue}`)). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOURCE_URL "https://www.ntt-bp.net/jcfw/area.html"

/* strndup(inner,len) then html_strip; returns malloc'd (caller frees). */
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
  /* iterate <tr>...</tr> blocks */
  while ((cur = html_block(cur, "tr", &tr_inner, &tr_len)) != NULL) {
    char *row = strndup(tr_inner, (size_t)tr_len);
    if (!row) continue;

    const char *c1_in, *c2_in; int c1_len, c2_len;
    const char *after1 = html_block(row, "td", &c1_in, &c1_len);
    if (!after1) { free(row); continue; }            /* <2 td → regex no match */
    const char *after2 = html_block(after1, "td", &c2_in, &c2_len);
    if (!after2) { free(row); continue; }

    char *area  = strip_dup(c1_in, c1_len);
    char *venue = strip_dup(c2_in, c2_len);
    if (!area || !venue || !*area || !*venue) {       /* if (!area || !venue) continue */
      free(area); free(venue); free(row); continue;
    }

    char remote_key[1024];
    snprintf(remote_key, sizeof remote_key, "%s|%s", area, venue);
    char title[1024];
    snprintf(title, sizeof title, "%s (%s)", venue, area);
    char body[1024];
    snprintf(body, sizeof body, "Area: %s\nVenue: %s\nOperator: NTT-BP", area, venue);

    cJSON *pj = cJSON_CreateObject();                 /* {area, venue, operator} */
    cJSON_AddStringToObject(pj, "area", area);
    cJSON_AddStringToObject(pj, "venue", venue);
    cJSON_AddStringToObject(pj, "operator", "NTT-BP");
    char *pjs = cJSON_PrintUnformatted(pj);

    cJSON *tg = cJSON_CreateArray();                  /* ['free-wifi','jcfw',`area:${area}`] */
    cJSON_AddItemToArray(tg, cJSON_CreateString("free-wifi"));
    cJSON_AddItemToArray(tg, cJSON_CreateString("jcfw"));
    char areatag[640];
    snprintf(areatag, sizeof areatag, "area:%s", area);
    cJSON_AddItemToArray(tg, cJSON_CreateString(areatag));
    char *tgs = cJSON_PrintUnformatted(tg);

    intel_item it = {0};
    it.remote_key = remote_key;          /* uid = wifi-hotspots-jcfw|<area>|<venue> */
    it.title = title;
    it.summary = "Free WiFi venue listed by NTT-BP Japan Connected Free Wi-Fi";
    it.body = body;
    it.link = SOURCE_URL;
    it.author = "NTT-BP";
    it.lang = "ja";
    it.published_at = NULL;              /* JS published_at: null */
    it.properties_json = pjs;
    it.tags_json = tgs;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pjs); cJSON_Delete(pj);
    free(tgs); cJSON_Delete(tg);
    free(area); free(venue); free(row);
  }
  free(html);
  fprintf(stderr, "[wifi-hotspots-jcfw] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def wifi_hotspots_jcfw_def = {
  .id = "wifi-hotspots-jcfw", .collector = "infrastructure",
  .name = "Japan Connected Free WiFi Directory",
  .name_ja = "Japan Connected Free WiFi 一覧",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(wifi_hotspots_jcfw_def)
