/* collectors/safety/sources/npa_missing_persons.c — port of
 * server/src/collectors/npaMissingPersons.js.
 * Shift_JIS multi-table CSV (csv.h iconv path). Try current & prev-2 Reiwa
 * year filenames; first that yields >=1 year wins. Header = first row with
 * >=3 令和..年 cells → year→column map. Capture labeled metric rows
 * (total/male/female/age groups/dementia); one feature per year w/
 * total!=null pinned at NPA HQ, plus ONE index intel item
 * (uid npa-missing-persons|index). SEED/_meta dropped (rule 7). */
#include "../../source.h"
#include "../../lib/csv.h"
#include "../../lib/geojson.h"
#include "../../lib/probe.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INDEX_URL "https://www.npa.go.jp/publications/statistics/safetylife/yukue.html"
#define NPA_LAT 35.6749
#define NPA_LON 139.7531
#define TIMEOUT_MS 15000

/* reiwaToYear: '令和２年' / '令和6年' / '令和六年' → 2018+n, else 0. */
static int reiwa_to_year(const char *s) {
  if (!s) return 0;
  const char *gen = strstr(s, "令和");
  if (!gen) return 0;
  const char *p = gen + strlen("令和");
  while (*p == ' ' || *p == '\t') p++;
  static const char *FW[10] = {"０","１","２","３","４","５","６","７","８","９"};
  static const char *KJ[10] = {"一","二","三","四","五","六","七","八","九","十"};
  /* the label must end in 年 after the number */
  long n = 0; int got = 0;
  if (*p >= '0' && *p <= '9') { char *e; n = strtol(p, &e, 10); got = (e != p); }
  else {
    long val = 0; int any = 0; const char *q = p;
    for (;;) {
      int hit = -1;
      for (int d = 0; d < 10; d++)
        if (strncmp(q, FW[d], strlen(FW[d])) == 0) { hit = d; break; }
      if (hit < 0) break;
      val = val * 10 + hit; any = 1; q += strlen(FW[hit]);
    }
    if (any) { n = val; got = 1; }
    else for (int d = 0; d < 10; d++)
      if (strncmp(p, KJ[d], strlen(KJ[d])) == 0) { n = d + 1; got = 1; break; }
  }
  if (!got || n <= 0) return 0;
  return (int)(2018 + n);
}

/* toInt: strip " , whitespace; parseInt; *ok set when finite. */
static long to_int(const char *s, int *ok) {
  *ok = 0;
  if (!s) return 0;
  char buf[64]; int j = 0;
  for (const char *p = s; *p && j < 63; p++) {
    if (*p=='"'||*p==','||*p==' '||*p=='\t'||*p=='\r'||*p=='\n') continue;
    buf[j++] = *p;
  }
  buf[j] = 0;
  if (!j) return 0;
  char *e; long v = strtol(buf, &e, 10);
  if (e == buf) return 0;
  *ok = 1; return v;
}

static const char *rcell(cJSON *row, int i) {
  cJSON *c = cJSON_GetArrayItem(row, i);
  return (c && cJSON_IsString(c)) ? c->valuestring : NULL;
}
static void trimcpy(char *d, size_t n, const char *s) {
  if (!s) { d[0]=0; return; }
  while (*s==' '||*s=='\t'||*s=='\r'||*s=='\n') s++;
  size_t len = strlen(s);
  while (len && (s[len-1]==' '||s[len-1]=='\t'||s[len-1]=='\r'||s[len-1]=='\n')) len--;
  if (len >= n) len = n-1;
  memcpy(d, s, len); d[len]=0;
}

enum { K_MALE, K_FEMALE, K_TOTAL, K_JUV, K_TEENS, K_TWENTIES, K_DEMENTIA, K_N };

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* reiwaYear = currentYear - 2018; try r..r-2 */
  time_t tnow = time(NULL);
  struct tm tmv; gmtime_r(&tnow, &tmv);
  int reiwa = (tmv.tm_year + 1900) - 2018;

  char wonUrl[256] = {0};
  cJSON *rows = NULL;
  int years[64]; int nyears = 0;
  long byYear[64][K_N];
  int hasYear[64][K_N];

  for (int rr = reiwa; rr >= reiwa - 2 && !rows; rr--) {
    char url[256];
    snprintf(url, sizeof url,
      "https://www.npa.go.jp/safetylife/seianki/fumei/R0%dyukuefumeisha_csv.csv", rr);
    http_response r = {0};
    int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, TIMEOUT_MS, 0, &r);
    if (rc != 0 || r.status < 200 || r.status >= 300 || !r.body) {
      http_response_free(&r); continue;
    }
    char *text = csv_decode_sjis(r.body, r.body_len);
    http_response_free(&r);
    cJSON *cand = csv_parse(text, 0);
    free(text);

    /* headers = first row with >=3 cells matching /令和.+年/ */
    int nrows = cJSON_GetArraySize(cand);
    cJSON *hdr = NULL;
    for (int i = 0; i < nrows; i++) {
      cJSON *row = cJSON_GetArrayItem(cand, i);
      int ncol = cJSON_GetArraySize(row), cnt = 0;
      for (int c = 0; c < ncol; c++) {
        const char *v = rcell(row, c);
        if (v && strstr(v, "令和") && strstr(v, "年")) cnt++;
      }
      if (cnt >= 3) { hdr = row; break; }
    }
    if (!hdr) { cJSON_Delete(cand); continue; }

    /* yearCols: index → year; years = sorted unique */
    int colYear[256]; int hcol = cJSON_GetArraySize(hdr);
    if (hcol > 256) hcol = 256;
    nyears = 0;
    for (int c = 0; c < hcol; c++) {
      int y = reiwa_to_year(rcell(hdr, c));
      colYear[c] = y;
      if (y) {
        int seen = 0;
        for (int k = 0; k < nyears; k++) if (years[k] == y) { seen = 1; break; }
        if (!seen && nyears < 64) years[nyears++] = y;
      }
    }
    /* sort years ascending (Object.keys→Number→sort default == numeric here) */
    for (int a = 0; a < nyears; a++)
      for (int b = a + 1; b < nyears; b++)
        if (years[a] > years[b]) { int t = years[a]; years[a] = years[b]; years[b] = t; }
    memset(hasYear, 0, sizeof hasYear);
    memset(byYear, 0, sizeof byYear);

    /* map year → its column (first column carrying that year) */
    int yearColIdx[64];
    for (int k = 0; k < nyears; k++) {
      yearColIdx[k] = -1;
      for (int c = 0; c < hcol; c++) if (colYear[c] == years[k]) { yearColIdx[k] = c; break; }
    }

    for (int i = 0; i < nrows; i++) {
      cJSON *row = cJSON_GetArrayItem(cand, i);
      int ncol = cJSON_GetArraySize(row);
      char l0[128], l1[128];
      trimcpy(l0, sizeof l0, rcell(row, 0));
      trimcpy(l1, sizeof l1, rcell(row, 1));
      const char *label = l0[0] ? l0 : (l1[0] ? l1 : "");
      if (!label[0]) continue;
      int key = -1;
      if (!strcmp(label,"男性")) key = K_MALE;
      else if (!strcmp(label,"女性")) key = K_FEMALE;
      else if (!strcmp(label,"総数") || !strcmp(label,"合計")) key = K_TOTAL;
      else if (!strcmp(label,"９歳以下") || !strcmp(label,"9歳以下")) key = K_JUV;
      else if (!strcmp(label,"10歳代")) key = K_TEENS;
      else if (!strcmp(label,"20歳代")) key = K_TWENTIES;
      else if (!strcmp(label,"認知症")) key = K_DEMENTIA;
      if (key < 0) continue;
      for (int k = 0; k < nyears; k++) {
        int ci = yearColIdx[k];
        if (ci < 0 || ci >= ncol) continue;
        int ok; long v = to_int(rcell(row, ci), &ok);
        if (ok && !hasYear[k][key]) { hasYear[k][key] = 1; byYear[k][key] = v; }
      }
    }

    if (nyears > 0) {
      rows = cand;                              /* keep this candidate */
      snprintf(wonUrl, sizeof wonUrl, "%s", url);
    } else {
      cJSON_Delete(cand);
    }
  }

  if (!rows) {
    fprintf(stderr, "[npa-missing-persons] no live CSV — 0 rows\n");
    return 0;                                   /* JS parsed==null → 0 rows */
  }

  char now[32]; probe_iso_now(now, sizeof now);
  cJSON *features = cJSON_CreateArray();
  for (int k = 0; k < nyears; k++) {
    if (!hasYear[k][K_TOTAL]) continue;         /* stats.total == null skip */
    int y = years[k];
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    /* AUDIT NOTE (slice a3): these rows are NATIONWIDE ANNUAL TOTALS. They used
     * to be pinned at the NPA headquarters in Kasumigaseki, which put five
     * "missing persons" markers on one office building. A national aggregate
     * has no location, so no geometry is emitted — the key is OMITTED rather
     * than set to JSON null, because lib/geojson.c:235 serialises whatever it
     * finds under "geometry" and an explicit null persists as the literal
     * string "null". */
    cJSON *p = cJSON_CreateObject();            /* EXACT JS key order */
    char idb[32]; snprintf(idb, sizeof idb, "MISSING_%d", y);
    cJSON_AddStringToObject(p, "id", idb);
    char tb[96];
    snprintf(tb, sizeof tb,
             "\xE8\xA1\x8C\xE6\x96\xB9\xE4\xB8\x8D\xE6\x98\x8E\xE8\x80\x85 %d — %ld \x28national total\x29",
             y, byYear[k][K_TOTAL]);
    cJSON_AddStringToObject(p, "title", tb);
    cJSON_AddStringToObject(p, "record_type", "missing-persons-annual");
    cJSON_AddStringToObject(p, "link", INDEX_URL);
    cJSON_AddStringToObject(p, "scope", "national");
    char ym[16]; snprintf(ym, sizeof ym, "%d-12", y);
    cJSON_AddStringToObject(p, "year_month", ym);
    /* Without a published_at every one of these rows landed on the timeline at
     * ingest time, so five different years all stacked on "today". The NPA
     * reporting period is the calendar year, so the row is dated at its close;
     * `year`/`year_month` remain the authoritative period fields. */
    char pub[24]; snprintf(pub, sizeof pub, "%d-12-31T00:00:00Z", y);
    cJSON_AddStringToObject(p, "published_at", pub);
    cJSON_AddNumberToObject(p, "year", y);
    cJSON_AddItemToObject(p, "total",
      hasYear[k][K_TOTAL] ? cJSON_CreateNumber((double)byYear[k][K_TOTAL]) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "male",
      hasYear[k][K_MALE] ? cJSON_CreateNumber((double)byYear[k][K_MALE]) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "female",
      hasYear[k][K_FEMALE] ? cJSON_CreateNumber((double)byYear[k][K_FEMALE]) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "dementia",
      hasYear[k][K_DEMENTIA] ? cJSON_CreateNumber((double)byYear[k][K_DEMENTIA]) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "juvenile_under10",
      hasYear[k][K_JUV] ? cJSON_CreateNumber((double)byYear[k][K_JUV]) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "teens",
      hasYear[k][K_TEENS] ? cJSON_CreateNumber((double)byYear[k][K_TEENS]) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "twenties",
      hasYear[k][K_TWENTIES] ? cJSON_CreateNumber((double)byYear[k][K_TWENTIES]) : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "source", "npa-missing-persons");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);

  /* index intel item (uid npa-missing-persons|index) — only when parsed */
  {
    cJSON *yc = cJSON_CreateArray();
    for (int k = 0; k < nyears; k++) cJSON_AddItemToArray(yc, cJSON_CreateNumber(years[k]));
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("safety"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("statistics"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("missing-persons"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("national"));
    char *tj = cJSON_PrintUnformatted(tags);

    cJSON *p = cJSON_CreateObject();            /* EXACT JS key order */
    cJSON_AddStringToObject(p, "csv_url", wonUrl);
    cJSON_AddItemToObject(p, "years_covered", yc);
    int lastK = nyears - 1;
    cJSON_AddItemToObject(p, "latest_total",
      (nyears > 0 && hasYear[lastK][K_TOTAL])
        ? cJSON_CreateNumber((double)byYear[lastK][K_TOTAL]) : cJSON_CreateNull());
    char *pj = cJSON_PrintUnformatted(p);

    intel_item it = {0};
    it.remote_key      = "index";
    it.title           = "NPA Annual Missing-Persons Statistics (行方不明者統計)";
    it.summary         = "Annual nationwide totals with breakdowns by sex, age group, and cause (incl. dementia).";
    it.link            = INDEX_URL;
    it.lang            = "ja";
    it.published_at    = now;
    it.tags_json       = tj;
    it.properties_json = pj;
    sink->emit(sink, &it);
    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(p);
  }

  cJSON_Delete(rows);
  fprintf(stderr, "[npa-missing-persons] emitted %d features + 1 index\n", n);
  return 0;
}

static const source_def npa_missing_persons_def = {
  .id = "npa-missing-persons", .collector = "safety",
  .name = "NPA Missing Persons", .name_ja = "警察庁 行方不明者統計",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(npa_missing_persons_def)
