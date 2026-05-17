/* collectors/cyber/sources/wayback_jp.c
 * Port of server/src/collectors/waybackJp.js (intelEnvelope).
 * Wayback CDX (output=json → [header,...rows]) for 10 fixed JP gov/cyber
 * hosts, one GET per host, flattened. uid = wayback-jp|<digest ||
 * <target>|<timestamp>|<flatIdx>>. WAYBACK_TARGETS env override not ported. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TARGETS[] = {
  "kantei.go.jp", "mod.go.jp", "mofa.go.jp", "meti.go.jp", "soumu.go.jp",
  "jpcert.or.jp", "ipa.go.jp", "nisc.go.jp", "pmda.go.jp", "mhlw.go.jp",
};
#define NT ((int)(sizeof(TARGETS) / sizeof(TARGETS[0])))

/* obj[col] as a JS string-or-NULL (CDX gives strings; '' is falsy). */
static const char *cell(cJSON *row, int idx) {
  cJSON *v = cJSON_GetArrayItem(row, idx);
  if (!v) return NULL;
  if (cJSON_IsString(v)) return v->valuestring[0] ? v->valuestring : NULL;
  return NULL;
}

static int is_ts14(const char *s) {
  if (!s) return 0;
  int i = 0;
  for (; s[i]; i++) if (s[i] < '0' || s[i] > '9') return 0;
  return i == 14;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "accept: application/json", NULL };
  int n = 0;
  int flat = 0;                                     /* .flat() index */
  for (int ti = 0; ti < NT; ti++) {
    const char *host = TARGETS[ti];
    char url[384];
    snprintf(url, sizeof url,
      "https://web.archive.org/cdx/search/cdx?url=%s&matchType=prefix"
      "&limit=20&output=json&fl=timestamp,original,statuscode,mimetype,digest"
      "&filter=statuscode:200&collapse=digest", host);
    cJSON *rows = feed_get_json_h(ctx->http, url, hdrs, 35000);
    if (!cJSON_IsArray(rows) || cJSON_GetArraySize(rows) < 2) {
      if (rows) cJSON_Delete(rows);
      continue;
    }
    cJSON *header = cJSON_GetArrayItem(rows, 0);
    /* resolve column indices from the header row */
    int iTs = -1, iOrig = -1, iStat = -1, iMime = -1, iDig = -1;
    int hc = cJSON_GetArraySize(header);
    for (int c = 0; c < hc; c++) {
      cJSON *h = cJSON_GetArrayItem(header, c);
      const char *hs = (h && cJSON_IsString(h)) ? h->valuestring : "";
      if (!strcmp(hs, "timestamp")) iTs = c;
      else if (!strcmp(hs, "original")) iOrig = c;
      else if (!strcmp(hs, "statuscode")) iStat = c;
      else if (!strcmp(hs, "mimetype")) iMime = c;
      else if (!strcmp(hs, "digest")) iDig = c;
    }

    int rn = cJSON_GetArraySize(rows);
    for (int ri = 1; ri < rn; ri++) {
      cJSON *row = cJSON_GetArrayItem(rows, ri);
      const char *ts   = iTs   >= 0 ? cell(row, iTs)   : NULL;
      const char *orig = iOrig >= 0 ? cell(row, iOrig) : NULL;
      const char *stat = iStat >= 0 ? cell(row, iStat) : NULL;
      const char *mime = iMime >= 0 ? cell(row, iMime) : NULL;
      const char *dig  = iDig  >= 0 ? cell(row, iDig)  : NULL;

      /* uid: intelUid(SOURCE_ID, row.digest, `${tgt}|${ts}|${i}`) */
      char fb[160];
      const char *rk;
      if (dig) {
        rk = dig;
      } else {
        snprintf(fb, sizeof fb, "%s|%s|%d", host, ts ? ts : "", flat);
        rk = fb;
      }

      /* title = row.original || row._target */
      const char *title = orig ? orig : host;

      /* summary = `${tgt} · ${mime||'unknown mime'} · ${stat||'?'}` */
      char summ[256];
      snprintf(summ, sizeof summ, "%s \xc2\xb7 %s \xc2\xb7 %s",
        host, mime ? mime : "unknown mime", stat ? stat : "?");

      /* link = (ts && orig) ? https://web.archive.org/web/<ts>/<orig> : null */
      char linkbuf[1024];
      const char *link = NULL;
      if (ts && orig) {
        snprintf(linkbuf, sizeof linkbuf,
          "https://web.archive.org/web/%s/%s", ts, orig);
        link = linkbuf;
      }

      /* published_at: 14-digit ts → ISO */
      char isobuf[24];
      const char *pub = NULL;
      if (is_ts14(ts)) {
        snprintf(isobuf, sizeof isobuf,
          "%.4s-%.2s-%.2sT%.2s:%.2s:%.2sZ",
          ts, ts + 4, ts + 6, ts + 8, ts + 10, ts + 12);
        pub = isobuf;
      }

      /* properties — EXACT JS key order */
      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "target", host);
      cJSON_AddItemToObject(props, "timestamp",
        ts ? cJSON_CreateString(ts) : cJSON_CreateNull());
      cJSON_AddItemToObject(props, "original",
        orig ? cJSON_CreateString(orig) : cJSON_CreateNull());
      cJSON_AddItemToObject(props, "mimetype",
        mime ? cJSON_CreateString(mime) : cJSON_CreateNull());
      cJSON_AddItemToObject(props, "statuscode",
        stat ? cJSON_CreateString(stat) : cJSON_CreateNull());
      cJSON_AddItemToObject(props, "digest",
        dig ? cJSON_CreateString(dig) : cJSON_CreateNull());
      char *pj = cJSON_PrintUnformatted(props);

      char tags[96];
      snprintf(tags, sizeof tags, "[\"wayback\",\"host:%s\"]", host);

      intel_item it = {0};
      it.remote_key     = rk;
      it.title          = title;
      it.summary        = summ;
      it.link           = link;
      it.lang           = "ja";
      it.published_at   = pub;
      it.record_type    = "wayback-jp";
      it.properties_json = pj;
      it.tags_json      = tags;
      if (sink->emit(sink, &it) >= 0) n++;

      free(pj);
      cJSON_Delete(props);
      flat++;
    }
    cJSON_Delete(rows);
  }
  fprintf(stderr, "[wayback-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def wayback_jp_def = {
  .id = "wayback-jp", .collector = "cyber",
  .name = "Wayback CDX (JP gov)", .name_ja = "Wayback Machine 日本政府",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(wayback_jp_def)
