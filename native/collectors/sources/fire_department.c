/* collectors/safety/sources/fire_department.c
 * Port of server/src/collectors/fireDepartment.js (intel-kind) — Tokyo
 * Fire Department public "災害情報" current-dispatch page. fetchText +
 * defensive <tr>/<td|th> row extraction. Each qualifying row → one intel
 * item; uid = fire-department|<sha1(joined)[:20]> (== intelUid(SOURCE_ID,
 * intelHashKey(joined))). Honest empty on fetch/parse failure or no rows
 * (rule 8 — never guess dispatch rows). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TFD_URL "https://www.tfd.metro.tokyo.lg.jp/lfe/saigai/saigai.html"

static char *strip_dup(const char *inner, int len) {
  char *raw = strndup(inner, (size_t)len);
  if (!raw) return NULL;
  char *s = html_strip(raw);
  free(raw);
  return s;
}

/* UTF-8-safe code-point prefix of at most `max` scalar values. */
static void cp_slice(const char *in, size_t max, char *out, size_t outn) {
  size_t cps = 0, w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && cps < max; ) {
    size_t adv = 1;
    if (*p < 0x80) adv = 1;
    else if ((*p >> 5) == 0x6) adv = 2;
    else if ((*p >> 4) == 0xE) adv = 3;
    else if ((*p >> 3) == 0x1E) adv = 4;
    if (w + adv >= outn) break;
    for (size_t k = 0; k < adv && p[k]; k++) out[w++] = (char)p[k];
    p += adv;
    cps++;
  }
  out[w] = 0;
}

static int has_digit(const char *s) {
  for (; *s; s++) if (*s >= '0' && *s <= '9') return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *html = feed_get_text(ctx->http, TFD_URL, 15000);
  if (!html) {                       /* genuine fetch failure — that IS an error */
    fprintf(stderr, "[fire-department] fetch failed: %s\n", TFD_URL);
    return -1;
  }
  const char *src = html;

  int n = 0;
  const char *cur = src;
  const char *tr_in; int tr_len;
  while ((cur = html_block(cur, "tr", &tr_in, &tr_len)) != NULL) {
    char *row = strndup(tr_in, (size_t)tr_len);
    if (!row) continue;

    cJSON *cells = cJSON_CreateArray();
    const char *tdcur = row;
    const char *td_in; int td_len;
    /* JS regex /<t[dh]\b[^>]*>...<\/t[dh]>/ — iterate both td and th. */
    while ((tdcur = html_block(tdcur, "td", &td_in, &td_len)) != NULL) {
      char *t = strip_dup(td_in, td_len);
      if (t && *t) cJSON_AddItemToArray(cells, cJSON_CreateString(t));
      free(t);
    }
    tdcur = row;
    while ((tdcur = html_block(tdcur, "th", &td_in, &td_len)) != NULL) {
      char *t = strip_dup(td_in, td_len);
      if (t && *t) cJSON_AddItemToArray(cells, cJSON_CreateString(t));
      free(t);
    }
    int cn = cJSON_GetArraySize(cells);
    if (cn < 2) { cJSON_Delete(cells); free(row); continue; }

    /* joined = cells.join(' / ') */
    size_t jl = 1;
    for (int i = 0; i < cn; i++) {
      jl += strlen(cJSON_GetArrayItem(cells, i)->valuestring);
      if (i + 1 < cn) jl += 3;
    }
    char *joined = malloc(jl);
    joined[0] = 0;
    for (int i = 0; i < cn; i++) {
      strcat(joined, cJSON_GetArrayItem(cells, i)->valuestring);
      if (i + 1 < cn) strcat(joined, " / ");
    }

    /* if (!/\d/.test(joined)) continue */
    if (!has_digit(joined)) { free(joined); cJSON_Delete(cells); free(row); continue; }
    /* if (/災害種別|発生時刻|発生場所/.test(joined) && cells.length<=3) continue */
    if (cn <= 3 &&
        (strstr(joined, "災害種別") || strstr(joined, "発生時刻") ||
         strstr(joined, "発生場所"))) {
      free(joined); cJSON_Delete(cells); free(row); continue;
    }

    char hk[24];
    const char *parts[] = { joined };
    feed_hash_key(hk, parts, 1);              /* intelHashKey(joined) */

    char title[256], summary[640];
    cp_slice(cJSON_GetArrayItem(cells, 0)->valuestring, 120, title, sizeof title);  /* exhaustive-ok: title column; all cells are in properties */
    cp_slice(joined, 280, summary, sizeof summary);

    cJSON *pj = cJSON_CreateObject();         /* { cells, agency } */
    cJSON_AddItemToObject(pj, "cells", cJSON_Duplicate(cells, 1));
    cJSON_AddStringToObject(pj, "agency", "東京消防庁");
    char *pjs = cJSON_PrintUnformatted(pj);

    intel_item it = {0};
    it.remote_key = hk;                       /* uid = fire-department|<hk> */
    it.title = title;
    it.summary = summary;
    it.body = joined;
    it.link = TFD_URL;
    it.lang = "ja";
    it.record_type = "fire-department";
    it.properties_json = pjs;
    it.tags_json = "[\"fire\",\"dispatch\",\"tokyo\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(pjs); cJSON_Delete(pj);
    free(joined); cJSON_Delete(cells); free(row);
  }
  free(html);

  fprintf(stderr, "[fire-department] emitted %d\n", n);
  /* An empty dispatch board is the NORMAL state for most of the day, not an
   * error. `n > 0 ? 0 : -1` made every quiet poll return rc=-1, which
   * core/scheduler.c logs as status="error" and feeds to anomaly_detect() —
   * i.e. the circuit breaker quarantined the source for being calm. The fetch
   * failure above is the only real error condition. */
  return 0;
}

static const source_def fire_department_def = {
  .id = "fire-department", .collector = "safety",
  .name = "Fire Department Dispatches", .name_ja = "消防庁 出動情報",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(fire_department_def)
