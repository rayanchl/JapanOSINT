/* collectors/cyber/sources/wayback_jp.c
 * Port of server/src/collectors/waybackJp.js (intelEnvelope).
 * Wayback CDX (output=json → [header,...rows]) for 10 fixed JP gov/cyber
 * hosts, one GET per host, flattened. uid = wayback-jp|<digest>, or
 * <digest>|<host>|<timestamp> for a digest captured more than once (see the
 * IDENTITY note in run()). WAYBACK_TARGETS env override not ported. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/keyqual.h"
#include "third_party/cJSON.h"
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

/* One host's CDX answer, held until all ten are in: a digest recurs ACROSS
 * hosts, so the key census must see every host before any uid is decided. */
typedef struct {
  const char *host;
  cJSON *rows;                  /* [header, row, row, …], owned */
  int iTs, iOrig, iStat, iMime, iDig;
} wb_host;

/* base key and discriminator for one CDX row. A row with a digest is keyed on
 * it and told apart by host|timestamp; one without (never seen live, kept for
 * the shape) is keyed on host|timestamp and told apart by its original URL. */
static void wb_key(const wb_host *h, cJSON *row, char *base, size_t bcap,
                   char *disc, size_t dcap) {
  const char *ts   = h->iTs   >= 0 ? cell(row, h->iTs)   : NULL;
  const char *orig = h->iOrig >= 0 ? cell(row, h->iOrig) : NULL;
  const char *dig  = h->iDig  >= 0 ? cell(row, h->iDig)  : NULL;
  if (dig) {
    snprintf(base, bcap, "%s", dig);
    snprintf(disc, dcap, "%s|%s", h->host, ts ? ts : "");
  } else {
    snprintf(base, bcap, "%s|%s", h->host, ts ? ts : "");
    snprintf(disc, dcap, "%s", orig ? orig : "");
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "accept: application/json", NULL };
  int n = 0, folded = 0, hosts_ok = 0;
  wb_host H[NT];
  memset(H, 0, sizeof H);
  for (int ti = 0; ti < NT; ti++) {
    wb_host *h = &H[ti];
    h->host = TARGETS[ti];
    h->iTs = h->iOrig = h->iStat = h->iMime = h->iDig = -1;
    char url[384];
    snprintf(url, sizeof url,
      "https://web.archive.org/cdx/search/cdx?url=%s&matchType=prefix"
      "&limit=20&output=json&fl=timestamp,original,statuscode,mimetype,digest"
      "&filter=statuscode:200&collapse=digest", h->host);
    cJSON *rows = feed_get_json_h(ctx->http, url, hdrs, 35000);
    if (!cJSON_IsArray(rows)) { cJSON_Delete(rows); continue; }
    hosts_ok++;
    if (cJSON_GetArraySize(rows) < 2) { cJSON_Delete(rows); continue; }
    h->rows = rows;
    cJSON *header = cJSON_GetArrayItem(rows, 0);  /* exhaustive-ok: CDX header row */
    /* resolve column indices from the header row */
    int hc = cJSON_GetArraySize(header);
    for (int c = 0; c < hc; c++) {
      cJSON *hd = cJSON_GetArrayItem(header, c);
      const char *hs = (hd && cJSON_IsString(hd)) ? hd->valuestring : "";
      if (!strcmp(hs, "timestamp")) h->iTs = c;
      else if (!strcmp(hs, "original")) h->iOrig = c;
      else if (!strcmp(hs, "statuscode")) h->iStat = c;
      else if (!strcmp(hs, "mimetype")) h->iMime = c;
      else if (!strcmp(hs, "digest")) h->iDig = c;
    }
  }

  /* IDENTITY. `collapse=digest` collapses only ADJACENT captures with one
   * digest, so a digest recurs within one host's answer (jpcert.or.jp's
   * unchanged page captured again months later) as well as across the ten
   * hosts (a shared error page or asset) — live 2026-10-06: 200 rows, 5
   * digests carried by 12 of them. Pass 1 counts every digest over all ten
   * hosts; pass 2 qualifies EVERY capture of a repeated digest by host and
   * timestamp, falling back to a hash of the row. It used to be
   * first-come-plain: the plain digest went to whichever host was queried
   * first (and, before that, a digest-less row was keyed on its position in
   * the run), so the uid of a capture depended on which other captures came
   * back. See lib/keyqual.h. */
  keyqual kq = {0};
  for (int ti = 0; ti < NT; ti++) {
    if (!H[ti].rows) continue;
    int rn = cJSON_GetArraySize(H[ti].rows);
    for (int ri = 1; ri < rn; ri++) {
      char base[160], disc[160];
      wb_key(&H[ti], cJSON_GetArrayItem(H[ti].rows, ri), base, sizeof base,
             disc, sizeof disc);
      keyqual_add(&kq, base, disc);
    }
  }
  keyqual_seal(&kq);

  for (int ti = 0; ti < NT; ti++) {
    const wb_host *h = &H[ti];
    if (!h->rows) continue;
    const char *host = h->host;
    int rn = cJSON_GetArraySize(h->rows);
    for (int ri = 1; ri < rn; ri++) {
      cJSON *row = cJSON_GetArrayItem(h->rows, ri);
      const char *ts   = h->iTs   >= 0 ? cell(row, h->iTs)   : NULL;
      const char *orig = h->iOrig >= 0 ? cell(row, h->iOrig) : NULL;
      const char *stat = h->iStat >= 0 ? cell(row, h->iStat) : NULL;
      const char *mime = h->iMime >= 0 ? cell(row, h->iMime) : NULL;
      const char *dig  = h->iDig  >= 0 ? cell(row, h->iDig)  : NULL;

      char base[160], disc[160], rkbuf[512];
      wb_key(h, row, base, sizeof base, disc, sizeof disc);
      const char *rk = base;
      if (keyqual_count(&kq, base) > 1) {
        char *raw = cJSON_PrintUnformatted(row);
        char content[1024];
        snprintf(content, sizeof content, "%s|%s", host, raw ? raw : "");
        free(raw);
        rk = keyqual_uid(&kq, base, disc, content, rkbuf, sizeof rkbuf);
        if (!keyqual_claim(&kq, base, rk)) { folded++; continue; }
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
    }
  }
  keyqual_free(&kq);
  for (int ti = 0; ti < NT; ti++) cJSON_Delete(H[ti].rows);
  fprintf(stderr, "[wayback-jp] emitted %d (%d byte-identical repeats folded; "
          "%d of %d hosts answered)\n", n, folded, hosts_ok, NT);
  /* Every host failing is a dead upstream, not an empty archive. */
  if (hosts_ok == 0) return -1;
  return 0;
}

static const source_def wayback_jp_def = {
  .id = "wayback-jp", .collector = "cyber",
  .name = "Wayback CDX (JP gov)", .name_ja = "Wayback Machine 日本政府",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(wayback_jp_def)
