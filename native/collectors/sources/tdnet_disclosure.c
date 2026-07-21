/* collectors/government/sources/tdnet_disclosure.c
 * Port of server/src/collectors/tdnetDisclosure.js (fetchText + regex rows).
 * TDnet (TSE Timely Disclosure) — today's filings index:
 *   https://www.release.tdnet.info/inbs/I_list_001_<YYYYMMDD>.html
 * Per <tr>: first <a href="...pdf">title</a> + all <td> stripped cells.
 * uid = tdnet-disclosure|<pdfUrl>  (== intelUid(SOURCE_ID, r.pdfUrl, …);
 * pdfUrl is always non-empty so it wins over the `${ymd}-${i}` fallback). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HOST "https://www.release.tdnet.info"

/* strndup(inner,len) then html_strip; returns malloc'd (caller frees). */
static char *strip_dup(const char *inner, int len) {
  char *raw = strndup(inner, (size_t)len);
  if (!raw) return NULL;
  char *s = html_strip(raw);
  free(raw);
  return s;
}

/* JS String.prototype.slice(0,max) in code points; copies a UTF-8-safe
 * prefix of at most `max` Unicode scalar values into out. */
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

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* todayYmd(): UTC YYYYMMDD */
  time_t now = time(NULL);
  struct tm g;
  gmtime_r(&now, &g);
  char ymd[16];
  snprintf(ymd, sizeof ymd, "%04d%02d%02d",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday);
  /* new Date().toISOString(): YYYY-MM-DDTHH:MM:SS.000Z */
  char iso[40];
  snprintf(iso, sizeof iso, "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
           g.tm_hour, g.tm_min, g.tm_sec);

  char index_url[128];
  snprintf(index_url, sizeof index_url,
           "%s/inbs/I_list_001_%s.html", HOST, ymd);

  char *html = feed_get_text(ctx->http, index_url, 12000);
  /* JS: try fetch; on throw keep html=''. extractRows('') → []. */
  const char *src = html ? html : "";

  int n = 0, emitted = 0;
  const char *cur = src;
  const char *tr_inner; int tr_len;
  while (emitted < 100 && (cur = html_block(cur, "tr", &tr_inner, &tr_len)) != NULL) {
    char *row = strndup(tr_inner, (size_t)tr_len);
    if (!row) continue;

    /* linkM = first <a href="...pdf">([^<]+)</a> in the block */
    char *pdf_url = NULL, *title = NULL;
    const char *acur = row;
    const char *a_in; int a_len;
    const char *a_after;
    while ((a_after = html_block(acur, "a", &a_in, &a_len)) != NULL) {
      /* opening <a ...> tag = bytes from its '<' up to a_in (just past '>') */
      const char *lt = a_in;
      while (lt > acur && lt[-1] != '<') lt--;
      if (lt > acur) lt--;                 /* include the '<' */
      char tag_attrs[2048];
      size_t tl = (size_t)(a_in - lt);
      if (tl >= sizeof tag_attrs) tl = sizeof tag_attrs - 1;
      memcpy(tag_attrs, lt, tl);
      tag_attrs[tl] = 0;
      char href[1024];
      if (html_attr(tag_attrs, "href", href, sizeof href)) {
        size_t hl = strlen(href);
        if (hl >= 4 && strcasecmp(href + hl - 4, ".pdf") == 0) {
          if (strncmp(href, "http", 4) == 0) {
            pdf_url = strdup(href);
          } else {
            char *u = malloc(strlen(HOST) + 6 + hl + 1);
            if (u) sprintf(u, "%s/inbs/%s", HOST, href);
            pdf_url = u;
          }
          /* title = linkM[2].trim() — inner is [^<]+, strip+trim */
          title = strip_dup(a_in, a_len);
          break;
        }
      }
      acur = a_after;
    }
    if (!pdf_url) { free(title); free(row); continue; }  /* if (!linkM) continue */

    /* cells: every <td> in block, html_strip'd */
    cJSON *cells = cJSON_CreateArray();
    const char *tdcur = row;
    const char *td_in; int td_len;
    while ((tdcur = html_block(tdcur, "td", &td_in, &td_len)) != NULL) {
      char *cell = strip_dup(td_in, td_len);
      cJSON_AddItemToArray(cells, cJSON_CreateString(cell ? cell : ""));
      free(cell);
    }

    /* summary = cells.join(' · ').slice(0,240)   (' · ' = sp U+00B7 sp) */
    size_t jl = 1;
    int cn = cJSON_GetArraySize(cells);
    for (int i = 0; i < cn; i++) {
      cJSON *c = cJSON_GetArrayItem(cells, i);
      jl += strlen(cJSON_IsString(c) ? c->valuestring : "");
      if (i + 1 < cn) jl += 4;  /* " \xC2\xB7 " */
    }
    char *joined = malloc(jl);
    joined[0] = 0;
    for (int i = 0; i < cn; i++) {
      cJSON *c = cJSON_GetArrayItem(cells, i);
      strcat(joined, cJSON_IsString(c) ? c->valuestring : "");
      if (i + 1 < cn) strcat(joined, " \xC2\xB7 ");
    }
    char summary[1024];
    cp_slice(joined, 240, summary, sizeof summary);
    free(joined);

    cJSON *pj = cJSON_CreateObject();          /* {date: ymd, cells: [...]} */
    cJSON_AddStringToObject(pj, "date", ymd);
    cJSON_AddItemToObject(pj, "cells", cells); /* cells now owned by pj */
    char *pjs = cJSON_PrintUnformatted(pj);

    intel_item it = {0};
    it.remote_key = pdf_url;          /* uid = tdnet-disclosure|<pdfUrl> */
    it.title = title;                 /* r.title (linkM[2].trim()) */
    it.summary = summary;
    it.link = pdf_url;
    it.lang = "ja";
    it.published_at = iso;            /* new Date().toISOString() */
    it.properties_json = pjs;
    it.tags_json = "[\"disclosure\",\"tdnet\",\"tse\",\"corporate\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    emitted++;

    free(pjs); cJSON_Delete(pj);      /* frees cells too */
    free(pdf_url); free(title); free(row);
  }
  free(html);
  fprintf(stderr, "[tdnet-disclosure] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def tdnet_disclosure_def = {
  .id = "tdnet-disclosure", .collector = "government",
  .name = "TDnet Timely Disclosure", .name_ja = "TDnet 適時開示",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(tdnet_disclosure_def)
