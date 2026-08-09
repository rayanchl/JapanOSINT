/* collectors/cyber/sources/nicter_stats.c
 * Port of server/src/collectors/nicterStats.js.
 * GET https://www.nicter.jp/atlas/ , regex-scrape three aggregate counters
 * off the JS-rendered page, emit ONE synthetic stats feature pinned at NICT
 * HQ (Koganei). Numbers are best-effort and may be null. _meta dropped; when
 * the fetch fails JS emits 0 features → we emit 0. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* NOTE 2026-07: the trailing-slash form 404s — https://www.nicter.jp/atlas/
 * returns "404 Not Found" so feed_get_text yielded NULL and the source
 * returned -1 on every run (permanent quarantine). The slash-less path is a
 * live 200. */
#define NICTER_STATS_URL "https://www.nicter.jp/atlas"

#define NICT_HQ_LON 139.4878
#define NICT_HQ_LAT 35.7100

/* digit set incl. fullwidth comma/space (JS strips /[,，\s]/g before parseInt).
 * A "digit" for the [0-9,，] capture class is ASCII 0-9, ',' or the UTF-8
 * fullwidth comma U+FF0C (EF BC 8C) — whitespace is NOT in the class. */
static int is_cap_digit(const unsigned char *p, int *adv) {
  if (p[0] >= '0' && p[0] <= '9') { *adv = 1; return 1; }
  if (p[0] == ',') { *adv = 1; return 1; }
  if (p[0] == 0xEF && p[1] == 0xBC && p[2] == 0x8C) { *adv = 3; return 1; } /* ， */
  return 0;
}

/* Returns 1 if the UTF-8 sequence at p is one of the [^0-9] gap chars that
 * the JS class {0,60} allows (anything that is not an ASCII digit). We count
 * a "gap unit" per byte for simplicity (60-byte budget ≈ 60-char in practice
 * for this page's ASCII gaps; CJK gaps are rare between label and number). */

/* JS `parseInt(str.replace(/[,，\s]/g,''),10)` — leading int of the digit run
 * with separators removed. Returns 1 on success (val set), 0 if no digits. */
static int parse_counter(const unsigned char *start, long *out) {
  char buf[64]; int bo = 0;
  const unsigned char *p = start;
  int adv;
  while (*p && is_cap_digit(p, &adv) && bo < (int)sizeof(buf) - 1) {
    if (p[0] >= '0' && p[0] <= '9') buf[bo++] = (char)p[0];
    /* commas (ascii or fullwidth) are stripped */
    p += adv;
  }
  buf[bo] = 0;
  if (!bo) return 0;
  char *end;
  long v = strtol(buf, &end, 10);
  if (end == buf) return 0;
  *out = v;
  return 1;
}

/* JS: html.match(/<label>[^0-9]{0,60}([0-9,，]+)/) — find <label> (UTF-8
 * bytes), then within the next up-to-60 bytes that contain NO ascii digit,
 * reach the first [0-9,，] run and parse it. Returns 1 if matched. */
static int extract_int(const char *html, const char *label, long *out) {
  if (!html) return 0;
  size_t llen = strlen(label);
  const char *s = html;
  while ((s = strstr(s, label))) {
    const unsigned char *p = (const unsigned char *)(s + llen);
    int gap = 0;
    while (*p && gap <= 60) {
      if (p[0] >= '0' && p[0] <= '9') {           /* start of capture run */
        if (parse_counter(p, out)) return 1;
        break;                                    /* digit but unparsable */
      }
      /* a fullwidth comma can't start the run before a digit in practice;
       * treat any non-ascii-digit byte as a gap byte */
      p++; gap++;
    }
    s += llen;                                    /* try next occurrence */
  }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *html = feed_get_text(ctx->http, NICTER_STATS_URL, 15000);
  if (!html) {
    fprintf(stderr, "[nicter-stats] fetch failed: %s\n", NICTER_STATS_URL);
    return -1;                                    /* !ok → 0 features */
  }

  long today = 0, total = 0, sensors = 0;
  int hasToday   = extract_int(html, "\xe6\x9c\xac\xe6\x97\xa5", &today);   /* 本日 */
  int hasTotal   = extract_int(html, "\xe7\xb4\xaf\xe8\xa8\x88", &total);   /* 累計 */
  int hasSensors = extract_int(html,
    "\xe7\xa8\xbc\xe5\x83\x8d\xe3\x82\xbb\xe3\x83\xb3\xe3\x82\xb5",          /* 稼働センサ */
    &sensors);

  size_t html_len = strlen(html);

  /* Every counter absent means the page carries no measurement at all (the
   * Atlas page is now a JS-rendered description with no server-side numbers).
   * Emitting a Feature whose three metrics are all null would be a row that
   * shows a label and no data — an honest empty is the correct result. */
  if (!hasToday && !hasTotal && !hasSensors) {
    fprintf(stderr, "[nicter-stats] no counters in %zu bytes of %s "
                    "(page is JS-rendered; live figures are served at "
                    "https://www.nicter.jp/top10) — emitting 0\n",
            html_len, NICTER_STATS_URL);
    free(html);
    return 0;
  }

  /* observed_at = new Date().toISOString() */
  char iso[32];
  time_t now = time(NULL);
  struct tm tmv;
  gmtime_r(&now, &tmv);
  strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%S.000Z", &tmv);

  cJSON *features = cJSON_CreateArray();
  cJSON *f = gj_point_feature(NICT_HQ_LON, NICT_HQ_LAT);

  cJSON *p = cJSON_CreateObject();                /* EXACT JS key order */
  cJSON_AddStringToObject(p, "id", "NICTER_STATS");
  cJSON_AddItemToObject(p, "packets_today",
    hasToday ? cJSON_CreateNumber((double)today) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "packets_total",
    hasTotal ? cJSON_CreateNumber((double)total) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "sensors_online",
    hasSensors ? cJSON_CreateNumber((double)sensors) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "observed_at", iso);
  cJSON_AddNumberToObject(p, "html_length", (double)html_len);
  cJSON_AddStringToObject(p, "source", "nicter_atlas_scrape");
  /* The Point is NICT's Koganei headquarters, not where the packets were seen —
   * NICTER is a nationwide darknet sensor aggregate and publishes no location.
   * The pin is kept (it is the real publisher) but is now labelled as such
   * instead of reading as an observation site. */
  cJSON_AddStringToObject(p, "geo_provenance", "nict-headquarters");
  cJSON_AddStringToObject(p, "geo_precision", "publisher-hq");
  cJSON_AddBoolToObject(p, "geo_uncertain", 1);
  cJSON_AddStringToObject(p, "geo_note",
    "nationwide darknet aggregate; the point is NICT's Koganei HQ, not an "
    "observation site");
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(features, f);

  free(html);
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[nicter-stats] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def nicter_stats_def = {
  .id = "nicter-stats", .collector = "cyber",
  .name = "NICTER Darknet Stats", .name_ja = "NICTER ダークネット統計",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(nicter_stats_def)
