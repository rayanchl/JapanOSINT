/* collectors/crime/sources/npa_special_fraud.c — port of
 * server/src/collectors/npaSpecialFraud.js.
 * Shift_JIS multi-section CSV (csv.h iconv path). Walk rows linearly tracking
 * current 令和X年 and a month-column header (１月…１２月); capture four metric
 * rows; pivot to YYYY-MM; one feature per month w/ recognised!=null pinned at
 * NPA HQ, plus ONE directory intel item (uid npa-special-fraud|index).
 * SEED/_meta dropped; the JS unreachable branch → 0 rows. */
#include "../../source.h"
#include "../../lib/csv.h"
#include "../../lib/geojson.h"
#include "../../lib/probe.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CSV_URL   "https://www.npa.go.jp/bureau/criminal/souni/tokusyusagi/hurikomesagi_toukei.csv"
#define INDEX_URL "https://www.npa.go.jp/publications/statistics/sousa/sagi.html"
#define NPA_LAT 35.6749
#define NPA_LON 139.7531
#define TIMEOUT_MS 15000

/* 令和X年 → 2018+X. Accepts ASCII digits, fullwidth digits, kanji 一..十. */
static int reiwa_to_year(const char *s) {
  if (!s) return 0;
  const char *gen = strstr(s, "令和");
  if (!gen) return 0;
  const char *p = gen + strlen("令和");
  while (*p == ' ' || *p == '\t') p++;
  static const char *FW[10] = {"０","１","２","３","４","５","６","７","８","９"};
  static const char *KJ[10] = {"一","二","三","四","五","六","七","八","九","十"};
  long n = 0; int got = 0;
  /* ASCII digits */
  if (*p >= '0' && *p <= '9') {
    char *e; n = strtol(p, &e, 10); got = (e != p);
  } else {
    /* fullwidth digit run, else single kanji (incl. 十=10) */
    int matched_fw = 1;
    const char *q = p;
    long val = 0; int any = 0;
    while (matched_fw) {
      int hit = -1;
      for (int d = 0; d < 10; d++)
        if (strncmp(q, FW[d], strlen(FW[d])) == 0) { hit = d; break; }
      if (hit < 0) { matched_fw = 0; break; }
      val = val * 10 + hit; any = 1; q += strlen(FW[hit]);
    }
    if (any) { n = val; got = 1; }
    else {
      for (int d = 0; d < 10; d++)
        if (strncmp(p, KJ[d], strlen(KJ[d])) == 0) { n = d + 1; got = 1; break; }
    }
  }
  if (!got || n <= 0) return 0;
  return (int)(2018 + n);
}

/* toInt: strip ", \t and 円, parseInt; null→0/none flagged via *ok. */
static long to_int(const char *s, int *ok) {
  *ok = 0;
  if (!s) return 0;
  char buf[64]; int j = 0;
  for (const char *p = s; *p && j < 63; p++) {
    if (*p == '"' || *p == ',' || *p == ' ' || *p == '\t' ||
        *p == '\r' || *p == '\n') continue;
    if ((unsigned char)*p == 0xE5 && strncmp(p, "円", 3) == 0) { p += 2; continue; }
    buf[j++] = *p;
  }
  buf[j] = 0;
  if (!j) return 0;
  char *e; long v = strtol(buf, &e, 10);
  if (e == buf) return 0;
  *ok = 1;
  return v;
}

static const char *MONTH_LABELS[12] = {
  "１月","２月","３月","４月","５月","６月",
  "７月","８月","９月","１０月","１１月","１２月" };

static const char *rcell(cJSON *row, int i) {
  cJSON *c = cJSON_GetArrayItem(row, i);
  return (c && cJSON_IsString(c)) ? c->valuestring : NULL;
}

/* trim() helper for label comparison */
static void trimcpy(char *dst, size_t n, const char *s) {
  if (!s) { dst[0] = 0; return; }
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
  size_t len = strlen(s);
  while (len && (s[len-1]==' '||s[len-1]=='\t'||s[len-1]=='\r'||s[len-1]=='\n')) len--;
  if (len >= n) len = n - 1;
  memcpy(dst, s, len); dst[len] = 0;
}

typedef struct { int year, month; long val; const char *metric; } rec_t;

static int run(const source_ctx *ctx, intel_sink *sink) {
  http_response r = {0};
  int rc = http_request(ctx->http, "GET", CSV_URL, NULL, NULL, 0,
                         TIMEOUT_MS, 0, &r);
  if (rc != 0 || r.status < 200 || r.status >= 300 || !r.body) {
    http_response_free(&r);
    fprintf(stderr, "[npa-special-fraud] CSV unreachable\n");
    return 0;                                  /* JS !buf branch → 0 rows */
  }
  char *text = csv_decode_sjis(r.body, r.body_len);
  http_response_free(&r);
  cJSON *rows = csv_parse(text, 0);
  free(text);

  int nrows = cJSON_GetArraySize(rows);
  size_t rcap = 1024, rn = 0;
  rec_t *recs = malloc(rcap * sizeof(rec_t));
  int currentYear = 0;
  int monthCols[256]; int hasMonth = 0;
  memset(monthCols, 0, sizeof monthCols);

  for (int i = 0; i < nrows; i++) {
    cJSON *row = cJSON_GetArrayItem(rows, i);
    int ncol = cJSON_GetArraySize(row);
    /* joined = row.join(' ') for reiwa detection */
    size_t jl = 1;
    for (int c = 0; c < ncol; c++) { const char *v = rcell(row,c); jl += (v?strlen(v):0)+1; }
    char *joined = malloc(jl);
    joined[0] = 0;
    for (int c = 0; c < ncol; c++) {
      const char *v = rcell(row, c);
      if (c) strcat(joined, " ");
      if (v) strcat(joined, v);
    }
    int yr = reiwa_to_year(joined);
    free(joined);
    if (yr) currentYear = yr;

    /* header row: contains a "１月" cell AND a "１２月" cell */
    int has1 = 0, has12 = 0;
    for (int c = 0; c < ncol; c++) {
      const char *v = rcell(row, c);
      if (!v) continue;
      if (strcmp(v, "１月") == 0) has1 = 1;
      if (strcmp(v, "１２月") == 0) has12 = 1;
    }
    if (has1 && has12) {
      memset(monthCols, 0, sizeof monthCols);
      hasMonth = 1;
      for (int c = 0; c < ncol && c < 256; c++) {
        char t[32]; trimcpy(t, sizeof t, rcell(row, c));
        for (int m = 0; m < 12; m++)
          if (strcmp(t, MONTH_LABELS[m]) == 0) { monthCols[c] = m + 1; break; }
      }
      continue;
    }
    if (!currentYear || !hasMonth) continue;

    /* label = first non-empty trimmed cell */
    char label[128]; label[0] = 0;
    for (int c = 0; c < ncol; c++) {
      char t[128]; trimcpy(t, sizeof t, rcell(row, c));
      if (t[0]) { snprintf(label, sizeof label, "%s", t); break; }
    }
    const char *metric = NULL;
    if (strcmp(label, "認知件数") == 0) metric = "recognised";
    else if (strcmp(label, "実質的な被害総額　（単位：円）") == 0 ||
             strncmp(label, "実質的な被害総額", strlen("実質的な被害総額")) == 0)
      metric = "damage_jpy";
    else if (strcmp(label, "検挙件数") == 0) metric = "arrests";
    else if (strcmp(label, "検挙人員") == 0) metric = "arrested_persons";
    if (!metric) continue;

    for (int c = 0; c < ncol && c < 256; c++) {
      if (!monthCols[c]) continue;
      int ok;
      long v = to_int(rcell(row, c), &ok);
      if (!ok) continue;
      if (rn == rcap) { rcap *= 2; recs = realloc(recs, rcap * sizeof(rec_t)); }
      recs[rn].year = currentYear; recs[rn].month = monthCols[c];
      recs[rn].val = v; recs[rn].metric = metric; rn++;
    }
  }

  /* pivot: Map ym → metrics, insertion order then sorted by ym ascending. */
  typedef struct { char ym[8]; int year, month;
                   int has[4]; long m[4]; } mo_t; /* 0 recog 1 dmg 2 arr 3 ap */
  size_t mcap = 256, mn = 0;
  mo_t *months = malloc(mcap * sizeof(mo_t));
  for (size_t k = 0; k < rn; k++) {
    char ym[8];
    snprintf(ym, sizeof ym, "%04d-%02d", recs[k].year, recs[k].month);
    mo_t *slot = NULL;
    for (size_t j = 0; j < mn; j++)
      if (strcmp(months[j].ym, ym) == 0) { slot = &months[j]; break; }
    if (!slot) {
      if (mn == mcap) { mcap *= 2; months = realloc(months, mcap * sizeof(mo_t)); }
      slot = &months[mn++];
      memset(slot, 0, sizeof *slot);
      snprintf(slot->ym, sizeof slot->ym, "%s", ym);
      slot->year = recs[k].year; slot->month = recs[k].month;
    }
    int mi = !strcmp(recs[k].metric,"recognised") ? 0 :
             !strcmp(recs[k].metric,"damage_jpy") ? 1 :
             !strcmp(recs[k].metric,"arrests") ? 2 : 3;
    slot->has[mi] = 1; slot->m[mi] = recs[k].val;
  }
  free(recs);
  /* sort by ym ascending (localeCompare on "YYYY-MM" == byte order) */
  for (size_t a = 0; a < mn; a++)
    for (size_t b = a + 1; b < mn; b++)
      if (strcmp(months[a].ym, months[b].ym) > 0) {
        mo_t t = months[a]; months[a] = months[b]; months[b] = t;
      }

  cJSON *features = cJSON_CreateArray();
  int fi = 0;
  for (size_t k = 0; k < mn; k++) {
    if (!months[k].has[0]) continue;           /* m.recognised != null */
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(NPA_LON));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(NPA_LAT));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);
    cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
    char idb[32];
    snprintf(idb, sizeof idb, "FRAUD_%s_%d", months[k].ym, fi);
    cJSON_AddStringToObject(p, "id", idb);
    cJSON_AddStringToObject(p, "year_month", months[k].ym);
    cJSON_AddNumberToObject(p, "year", months[k].year);
    cJSON_AddNumberToObject(p, "month", months[k].month);
    cJSON_AddItemToObject(p, "recognised",
      months[k].has[0] ? cJSON_CreateNumber((double)months[k].m[0]) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "damage_jpy",
      months[k].has[1] ? cJSON_CreateNumber((double)months[k].m[1]) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "arrests",
      months[k].has[2] ? cJSON_CreateNumber((double)months[k].m[2]) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "arrested_persons",
      months[k].has[3] ? cJSON_CreateNumber((double)months[k].m[3]) : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "source", "npa-special-fraud");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    fi++;
  }
  cJSON_Delete(rows);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);

  /* directory intel item (uid npa-special-fraud|index) — always */
  {
    char now[32];
    /* JS published_at = fetchedAt (ISO now). Use a fresh ISO timestamp. */
    probe_iso_now(now, sizeof now);

    char summary[160];
    const char *latest_ym = mn ? months[mn-1].ym : "n/a";
    snprintf(summary, sizeof summary,
      "Monthly nationwide totals: %zu months parsed; latest %s.", mn, latest_ym);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("crime"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("fraud"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("statistics"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("national"));
    char *tj = cJSON_PrintUnformatted(tags);

    cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
    cJSON_AddStringToObject(p, "csv_url", CSV_URL);
    cJSON_AddNumberToObject(p, "months_covered", (double)mn);
    cJSON_AddItemToObject(p, "latest_month",
      mn ? cJSON_CreateString(months[mn-1].ym) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "latest_recognised",
      (mn && months[mn-1].has[0]) ? cJSON_CreateNumber((double)months[mn-1].m[0])
                                  : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "latest_damage_jpy",
      (mn && months[mn-1].has[1]) ? cJSON_CreateNumber((double)months[mn-1].m[1])
                                  : cJSON_CreateNull());
    char *pj = cJSON_PrintUnformatted(p);

    intel_item it = {0};
    it.remote_key      = "index";              /* → uid npa-special-fraud|index */
    it.title           = "NPA Special Fraud Monthly Statistics (特殊詐欺認知・検挙状況)";
    it.summary         = summary;
    it.link            = INDEX_URL;
    it.lang            = "ja";
    it.published_at    = now;
    it.tags_json       = tj;
    it.properties_json = pj;
    sink->emit(sink, &it);
    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(p);
  }

  free(months);
  fprintf(stderr, "[npa-special-fraud] emitted %d features + 1 index\n", n);
  return 0;
}

static const source_def npa_special_fraud_def = {
  .id = "npa-special-fraud", .collector = "crime",
  .name = "NPA Special Fraud", .name_ja = "警察庁 特殊詐欺統計",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(npa_special_fraud_def)
