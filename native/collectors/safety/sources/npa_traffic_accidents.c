/* collectors/safety/sources/npa_traffic_accidents.c
 * Port of server/src/collectors/npaTrafficAccidents.js (Shift_JIS CSV,
 * multi-year discovery + stream-aggregate to a 2-decimal lat/lon × month
 * grid). For y in [thisYear-1, -2, -3]: discover honhyo_*.csv off the year
 * index page, fetch it, csv_decode_sjis, stream-aggregate; first year whose
 * decoded text > 100000 bytes wins. Emits one feature per grid bucket + one
 * intel item (uid intelUid(SOURCE_ID,'index')). namedCache TTL layer dropped
 * (correctness-neutral). _meta/SEED n/a. Feature uid/title via geojson sink. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/csv.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

/* Math.round (round half toward +Inf); inputs here are positive. */
static double js_round(double x) { return floor(x + 0.5); }

/* JS `${num}` minimal decimal string for a value rounded to 2 dp. */
static void numstr(double v, char *o, size_t n) {
  snprintf(o, n, "%.2f", v);
  char *dot = strchr(o, '.');
  if (dot) {
    char *e = o + strlen(o) - 1;
    while (e > dot && *e == '0') *e-- = 0;
    if (e == dot) *e = 0;
  }
}

/* packedDmsToDecimal — null sentinels return NAN. */
static double packed_dms(const char *s, int latlon /*0=lat,1=lon*/) {
  if (!s) return NAN;
  /* trim */
  while (*s==' '||*s=='\t') s++;
  size_t L = strlen(s);
  while (L && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='\r'||s[L-1]=='\n')) L--;
  if (L == 0) return NAN;
  if (L >= 4 && strncmp(s, "9999", 4) == 0) return NAN;
  int ddLen = latlon == 0 ? 2 : 3;
  if ((int)L < ddLen + 6) return NAN;
  char b[16];
  #define SLICE(a,c) do{ int _n=(c); memcpy(b,s+(a),_n); b[_n]=0; }while(0)
  SLICE(0, ddLen);            long dd = strtol(b, NULL, 10);
  SLICE(ddLen, 2);            long mm = strtol(b, NULL, 10);
  SLICE(ddLen + 2, 2);        long ss = strtol(b, NULL, 10);
  long ms = 0;
  if ((int)L >= ddLen + 7) { SLICE(ddLen + 4, 3); ms = strtol(b, NULL, 10); }
  #undef SLICE
  double dec = dd + mm / 60.0 + (ss + ms / 1000.0) / 3600.0;
  return isfinite(dec) ? dec : NAN;
}

/* parseInt(s||'',10): leading optional sign + digits; not-finite if none. */
static int parse_int_js(const char *s, long *out) {
  if (!s) return 0;
  while (*s==' '||*s=='\t'||*s=='\n'||*s=='\r'||*s=='\f'||*s=='\v') s++;
  const char *p = s;
  if (*p == '+' || *p == '-') p++;
  if (!isdigit((unsigned char)*p)) return 0;
  *out = strtol(s, NULL, 10);
  return 1;
}

typedef struct {
  char key[48];
  double lat, lon; char ym[16];
  long count, fatalities, severity_max;
} bucket_t;

static int find_col(char **hdr, int nh, const char *name) {
  for (int i = 0; i < nh; i++) if (strcmp(hdr[i], name) == 0) return i;
  return -1;
}

/* discoverCsvUrl: index page -> /href="([^"]*honhyo[^"]*\.csv)"/i */
static int discover_csv(http_client *http, int year, char *out, size_t n) {
  char idx[256];
  snprintf(idx, sizeof idx,
    "https://www.npa.go.jp/publications/statistics/koutsuu/opendata/%d/opendata_%d.html",
    year, year);
  char *html = feed_get_text(http, idx, 8000);
  if (!html) return 0;
  int ok = 0;
  for (char *s = html; (s = strstr(s, "href=\"")); s += 6) {
    char *v = s + 6; char *e = strchr(v, '"');
    if (!e) break;
    size_t L = (size_t)(e - v);
    char href[1024]; if (L >= sizeof href) { continue; }
    memcpy(href, v, L); href[L] = 0;
    /* case-insensitive contains "honhyo" and ends ".csv" */
    char low[1024]; size_t i = 0;
    for (; href[i] && i < sizeof low - 1; i++)
      low[i] = (char)tolower((unsigned char)href[i]);
    low[i] = 0;
    if (strstr(low, "honhyo") && strstr(low, ".csv")) {
      if (strncmp(href, "http", 4) == 0) snprintf(out, n, "%s", href);
      else if (href[0] == '/') snprintf(out, n, "https://www.npa.go.jp%s", href);
      else snprintf(out, n,
        "https://www.npa.go.jp/publications/statistics/koutsuu/opendata/%d/%s",
        year, href);
      ok = 1; break;
    }
  }
  free(html);
  return ok;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char iso[32];
  time_t now = time(NULL);
  struct tm tmv; gmtime_r(&now, &tmv);
  strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%S.000Z", &tmv);
  int this_year = tmv.tm_year + 1900;

  char csv_url[1024] = {0}; char *text = NULL; int year = 0;
  for (int k = 1; k <= 3 && !text; k++) {
    int y = this_year - k;
    char u[1024];
    if (!discover_csv(ctx->http, y, u, sizeof u)) continue;
    char *raw = feed_get_text(ctx->http, u, 60000);
    if (!raw) continue;
    size_t rl = strlen(raw);
    char *dec = csv_decode_sjis(raw, rl);
    free(raw);
    if (dec && strlen(dec) > 100000) {
      text = dec; year = y; snprintf(csv_url, sizeof csv_url, "%s", u);
    } else { free(dec); }
  }
  if (!text) return -1;             /* parsed == null -> 0 features */

  /* header line up to first '\n', strip trailing '\r' */
  char *nl = strchr(text, '\n');
  if (!nl) { free(text); return -1; }
  size_t hlen = (size_t)(nl - text);
  if (hlen && text[hlen-1] == '\r') hlen--;
  char *hline = strndup(text, hlen);
  /* split header by ',' (NPA file: unquoted) */
  char *hdr[256]; int nh = 0;
  for (char *p = hline; ; ) {
    hdr[nh++] = p;
    char *c = strchr(p, ',');
    if (!c || nh >= 256) break;
    *c = 0; p = c + 1;
  }
  int iSev   = find_col(hdr, nh, "\xe4\xba\x8b\xe6\x95\x85\xe5\x86\x85\xe5\xae\xb9"); /* 事故内容 */
  int iFatal = find_col(hdr, nh, "\xe6\xad\xbb\xe8\x80\x85\xe6\x95\xb0");             /* 死者数 */
  int iMonth = find_col(hdr, nh, "\xe7\x99\xba\xe7\x94\x9f\xe6\x97\xa5\xe6\x99\x82\xe3\x80\x80\xe3\x80\x80\xe6\x9c\x88"); /* 発生日時　　月 */
  int iLat   = find_col(hdr, nh, "\xe5\x9c\xb0\xe7\x82\xb9\xe3\x80\x80\xe7\xb7\xaf\xe5\xba\xa6\xef\xbc\x88\xe5\x8c\x97\xe7\xb7\xaf\xef\xbc\x89"); /* 地点　緯度（北緯）*/
  int iLon   = find_col(hdr, nh, "\xe5\x9c\xb0\xe7\x82\xb9\xe3\x80\x80\xe7\xb5\x8c\xe5\xba\xa6\xef\xbc\x88\xe6\x9d\xb1\xe7\xb5\x8c\xef\xbc\x89"); /* 地点　経度（東経）*/
  if (iLat < 0 || iLon < 0 || iMonth < 0) {
    free(hline); free(text); return -1;
  }
  int maxIdx = iSev;
  if (iFatal > maxIdx) maxIdx = iFatal;
  if (iMonth > maxIdx) maxIdx = iMonth;
  if (iLat   > maxIdx) maxIdx = iLat;
  if (iLon   > maxIdx) maxIdx = iLon;

  bucket_t *bk = NULL; int nb = 0, cap = 0;
  long badCoords = 0, rowCount = 0;

  char *cur = nl + 1;
  while (*cur) {
    char *eol = strchr(cur, '\n');
    size_t lineEnd = eol ? (size_t)(eol - cur) : strlen(cur);
    char *lstart = cur;
    size_t llen = lineEnd;
    if (llen && lstart[llen-1] == '\r') llen--;
    cur = eol ? eol + 1 : cur + lineEnd;
    if (llen == 0) { if (!eol) break; continue; }
    rowCount++;

    /* manual split up to maxIdx */
    char *fields[260]; int nf = 0;
    char *fp = lstart; size_t consumed = 0;
    while (nf <= maxIdx) {
      char *cm = memchr(fp, ',', llen - consumed);
      if (!cm) { fields[nf++] = strndup(fp, llen - consumed); break; }
      fields[nf++] = strndup(fp, (size_t)(cm - fp));
      consumed += (size_t)(cm - fp) + 1;
      fp = cm + 1;
    }

    double lat = (iLat < nf) ? packed_dms(fields[iLat], 0) : NAN;
    double lon = (iLon < nf) ? packed_dms(fields[iLon], 1) : NAN;
    int skip = 0;
    if (isnan(lat) || isnan(lon)) { badCoords++; skip = 1; }
    else if (lat < 20 || lat > 50 || lon < 120 || lon > 155) {
      badCoords++; skip = 1;
    }
    if (!skip) {
      const char *mraw = (iMonth < nf) ? fields[iMonth] : "";
      char mon[8];
      if (!mraw) mraw = "";
      size_t ml = strlen(mraw);
      if (ml == 0) { skip = 1; }
      else if (ml == 1) { mon[0]='0'; mon[1]=mraw[0]; mon[2]=0; }
      else { snprintf(mon, sizeof mon, "%s", mraw); }
      if (!skip && (!mon[0] || strcmp(mon, "00") == 0)) skip = 1;
      if (!skip) {
        char ym[16];
        snprintf(ym, sizeof ym, "%d-%s", year, mon);
        double lat2 = js_round(lat * 100.0) / 100.0;
        double lon2 = js_round(lon * 100.0) / 100.0;
        char la[24], lo[24];
        numstr(lat2, la, sizeof la); numstr(lon2, lo, sizeof lo);
        char key[48];
        snprintf(key, sizeof key, "%s,%s,%s", la, lo, ym);
        bucket_t *b = NULL;
        for (int i = 0; i < nb; i++)
          if (strcmp(bk[i].key, key) == 0) { b = &bk[i]; break; }
        if (!b) {
          if (nb == cap) {
            cap = cap ? cap * 2 : 1024;
            bk = realloc(bk, (size_t)cap * sizeof *bk);
          }
          b = &bk[nb++];
          memset(b, 0, sizeof *b);
          snprintf(b->key, sizeof b->key, "%s", key);
          b->lat = lat2; b->lon = lon2;
          snprintf(b->ym, sizeof b->ym, "%s", ym);
        }
        b->count += 1;
        if (iFatal >= 0 && iFatal < nf) {
          long fv;
          if (parse_int_js(fields[iFatal], &fv)) b->fatalities += fv;
        }
        if (iSev >= 0 && iSev < nf) {
          long sv;
          if (parse_int_js(fields[iSev], &sv) && sv > b->severity_max)
            b->severity_max = sv;
        }
      }
    }
    for (int i = 0; i < nf; i++) free(fields[i]);
    if (!eol) break;
  }
  free(hline); free(text);

  cJSON *features = cJSON_CreateArray();
  for (int i = 0; i < nb; i++) {
    bucket_t *b = &bk[i];
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(b->lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(b->lat));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);
    cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
    char la[24], lo[24];
    numstr(b->lat, la, sizeof la); numstr(b->lon, lo, sizeof lo);
    char idb[64];
    snprintf(idb, sizeof idb, "ACC_%s_%s_%s", b->ym, la, lo);
    cJSON_AddStringToObject(p, "id", idb);
    cJSON_AddStringToObject(p, "year_month", b->ym);
    cJSON_AddNumberToObject(p, "count", (double)b->count);
    cJSON_AddNumberToObject(p, "fatalities", (double)b->fatalities);
    cJSON_AddNumberToObject(p, "severity_max", (double)b->severity_max);
    cJSON_AddStringToObject(p, "source", ctx->source_id);
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  int bucket_count = cJSON_GetArraySize(features);
  cJSON_Delete(features);

  /* intel item uid intelUid(SOURCE_ID,'index') */
  char idx_url[256];
  snprintf(idx_url, sizeof idx_url,
    "https://www.npa.go.jp/publications/statistics/koutsuu/opendata/%d/opendata_%d.html",
    year, year);
  char summary[256];
  snprintf(summary, sizeof summary,
    "%ld accidents reported nationwide in %d; %d grid buckets after 110m quantisation.",
    rowCount, year, bucket_count);
  cJSON *ip = cJSON_CreateObject();
  cJSON_AddNumberToObject(ip, "year", (double)year);
  cJSON_AddStringToObject(ip, "csv_url", csv_url);
  cJSON_AddNumberToObject(ip, "raw_row_count", (double)rowCount);
  cJSON_AddNumberToObject(ip, "bucket_count", (double)bucket_count);
  cJSON_AddNumberToObject(ip, "dropped_for_bad_coords", (double)badCoords);
  char *ipj = cJSON_PrintUnformatted(ip);
  char title[64];
  snprintf(title, sizeof title, "NPA Traffic Accidents (%d)", year);

  intel_item it = {0};
  it.remote_key = "index";
  it.title = title;
  it.summary = summary;
  it.link = idx_url;
  it.lang = "ja";
  it.published_at = iso;
  it.tags_json = "[\"safety\",\"traffic\",\"statistics\",\"nationwide\"]";
  it.properties_json = ipj;
  if (sink->emit(sink, &it) >= 0 && n >= 0) n++;
  free(ipj); cJSON_Delete(ip); free(bk);

  fprintf(stderr, "[npa-traffic-accidents] year=%d rows=%ld buckets=%d\n",
          year, rowCount, bucket_count);
  return n >= 0 ? 0 : -1;
}

static const source_def npa_traffic_accidents_def = {
  .id = "npa-traffic-accidents", .collector = "safety",
  .name = "NPA Traffic Accidents",
  .name_ja = "\xe8\xad\xa6\xe5\xaf\x9f\xe5\xba\x81 \xe4\xba\xa4\xe9\x80\x9a\xe4\xba\x8b\xe6\x95\x85\xe7\xb5\xb1\xe8\xa8\x88",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(npa_traffic_accidents_def)
