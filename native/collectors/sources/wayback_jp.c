/* collectors/cyber/sources/wayback_jp.c
 * Port of server/src/collectors/waybackJp.js (intelEnvelope).
 * Wayback CDX (output=json -> [header, ...rows]) for 10 fixed JP gov/cyber
 * hosts, every capture under each host (matchType=prefix, statuscode 200,
 * adjacent same-digest captures collapsed). WAYBACK_TARGETS env override not
 * ported.
 *
 * HOW MUCH THIS READS, AND WHAT IT SAYS ABOUT THE REST (rule 2).
 * It used to ask each host for `limit=20` and stop: 200 captures a run, the
 * twenty OLDEST captures of each host's alphabetically first URL (kantei.go.jp
 * 1996-2000, mod.go.jp 2007 …), the same 200 every day, and nothing said that
 * anything else existed. What exists is large. The CDX API's own paging
 * (`page=N`, with `showNumPages=true` for the count) divides each host's index
 * into pages — measured 2026-10-07:
 *     kantei 1,310   mod 1,022   mofa 1,080   meti 663   soumu 736
 *     jpcert 61      ipa 217     nisc 35      pmda 84    mhlw 2,064
 * 7,272 pages, each one index block that answers with 55-1,315 filtered
 * captures in 2.5-5 s (kantei pages 0, 655, 1309). Reading all of it daily is
 * ~7,000 requests to one archive; asking for a date window instead does not
 * work either — `from=` on mhlw.go.jp ran the server into a 504 after 60 s,
 * because the index is ordered by URL, not time.
 *
 * So the bound stays, and it is walked and stated rather than hidden:
 *   - each run reads WB_PAGES_DEF consecutive CDX pages per host
 *     ($JO_WAYBACK_PAGES overrides), every capture on every page emitted;
 *   - the walk RESUMES: the page after the last one read is kept in the
 *     host's truncation notice, and the next run starts there, wrapping to
 *     page 0 after the last page — so the whole index is read over a cycle
 *     (mhlw, the largest, in 2,064 / 10 ≈ 207 daily runs) instead of the
 *     same first page forever;
 *   - every host whose index is larger than one run's pages gets a
 *     collector-truncation-notice: captures used, pages read (first / last)
 *     of the host's page count, and the page the next run starts at. The CDX
 *     API counts pages, not captures, so records_available is null rather
 *     than an estimate;
 *   - a host whose page count, or a page, fails is named in its notice with
 *     the status; a 429/503 or an unparseable answer (the archive's
 *     "Temporarily Offline" page) ends the run's remaining requests instead
 *     of hammering an archive that has said stop.
 * A page boundary moves a little as the archive indexes new captures, so a
 * resumed walk can re-read or skip a few captures at the seam; re-reads land
 * on the same uid.
 *
 * IDENTITY. uid = wayback-jp|<digest>|<timestamp>|<original URL> (the URL
 * hashed when it would not fit the sink's uid). A capture IS a URL at an
 * instant; the digest alone is the content, which recurs across URLs, hosts
 * and years (a shared error page, an unchanged page captured again). The
 * previous uid was the digest, qualified only when it recurred within one run
 * (lib/keyqual) — sound while every run read the same 200 rows, wrong once
 * runs read different pages: a digest unique in today's pages would take the
 * plain uid that an earlier run gave a different capture. Two rows with the
 * same digest, timestamp and URL are the same capture served twice, and fold.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/jocore.h"
#include "core/db.h"
#include "core/httpclient.h"
#include "third_party/cJSON.h"
#include "third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TARGETS[] = {
  "kantei.go.jp", "mod.go.jp", "mofa.go.jp", "meti.go.jp", "soumu.go.jp",
  "jpcert.or.jp", "ipa.go.jp", "nisc.go.jp", "pmda.go.jp", "mhlw.go.jp",
};
#define NT ((int)(sizeof(TARGETS) / sizeof(TARGETS[0])))

#define WB_CDX "https://web.archive.org/cdx/search/cdx"
#define WB_FL  "&fl=timestamp,original,statuscode,mimetype,digest" \
               "&filter=statuscode:200&collapse=digest"
/* CDX pages read per host per run. Not a cap on what a page holds — every
 * capture of every page read is emitted — but on how many of a host's index
 * pages one daily run asks for. Each stop is disclosed per host. */
#define WB_PAGES_DEF 10   /* exhaustive-ok: per-run page budget per host; the walk resumes next run and every stop is a per-host truncation notice */
#define WB_GAP_MS 1000    /* pause between archive requests */
#define WB_SRC "wayback-jp"

static void wb_pause(const source_ctx *ctx) {
  for (int i = 0; i < WB_GAP_MS / 100; i++) {
    if (ctx->cancel && *ctx->cancel) return;
    struct timespec ts = { 0, 100 * 1000 * 1000 };
    nanosleep(&ts, NULL);
  }
}

/* obj[col] as a JS string-or-NULL (CDX gives strings; '' is falsy). */
static const char *cell(cJSON *row, int idx) {
  if (idx < 0) return NULL;
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

/* The page the previous run said to start at, from this host's notice (one
 * row: the notice's uid is per source and host); 0 when there is none —
 * first run, no database, or a notice from before the walk resumed. */
static long wb_saved_next(const source_ctx *ctx, const char *host) {
  if (!ctx->db || !ctx->db->h) return 0;
  sqlite3_stmt *st = NULL;
  long v = 0;
  if (sqlite3_prepare_v2(ctx->db->h,
        "SELECT json_extract(properties,'$.next_page') FROM intel_items "
        "WHERE source_id=?1 AND record_type='collector-truncation-notice' "
        "AND json_extract(properties,'$.query')=?2", -1, &st, NULL) != SQLITE_OK)
    return 0;
  sqlite3_bind_text(st, 1, WB_SRC, -1, SQLITE_STATIC);
  sqlite3_bind_text(st, 2, host, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_type(st, 0) != SQLITE_NULL)
    v = (long)sqlite3_column_int64(st, 0);
  sqlite3_finalize(st);
  return v > 0 ? v : 0;
}

/* The host's CDX page count, or -1 with *status set. */
static long wb_num_pages(const source_ctx *ctx, const char *host, long *status) {
  char url[384];
  snprintf(url, sizeof url, "%s?url=%s&matchType=prefix&showNumPages=true",
           WB_CDX, host);
  http_response r = {0};
  int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 60000, 2, &r);
  *status = rc ? 0 : r.status;
  long n = -1;
  if (rc == 0 && r.status == 200 && r.body) {
    char *end = NULL;
    long v = strtol(r.body, &end, 10);
    if (end != r.body && v >= 0) n = v;
  }
  http_response_free(&r);
  return n;
}

/* Is this failure the archive telling us to stop? 429/503, or a 2xx that is
 * not JSON (its "Temporarily Offline" page). */
static int wb_stop_status(long st) {
  return st == 429 || st == 503 || st == FEED_ST_UNPARSED;
}

typedef struct {
  const char *host;
  long pages_total, start, next, read, failed_page, failed_status;
  int used, folded;
} wb_host;

/* Emit every capture of one CDX page. Returns captures emitted. */
static int wb_emit_page(intel_sink *sink, wb_host *h, cJSON *rows, long page) {
  int n = 0, rn = cJSON_GetArraySize(rows);
  if (rn < 2) return 0;                         /* [] or a header alone */
  cJSON *header = cJSON_GetArrayItem(rows, 0);  /* exhaustive-ok: CDX header row */
  int iTs = -1, iOrig = -1, iStat = -1, iMime = -1, iDig = -1;
  int hc = cJSON_GetArraySize(header);
  for (int c = 0; c < hc; c++) {
    cJSON *hd = cJSON_GetArrayItem(header, c);
    const char *hs = (hd && cJSON_IsString(hd)) ? hd->valuestring : "";
    if (!strcmp(hs, "timestamp")) iTs = c;
    else if (!strcmp(hs, "original")) iOrig = c;
    else if (!strcmp(hs, "statuscode")) iStat = c;
    else if (!strcmp(hs, "mimetype")) iMime = c;
    else if (!strcmp(hs, "digest")) iDig = c;
  }
  /* A same-uid row INSIDE one page is the same capture served twice. CDX
   * sorts a page by URL then timestamp, so such a repeat is adjacent and the
   * previous row's key is enough to see it. */
  char prev[512] = "";
  for (int ri = 1; ri < rn; ri++) {
    cJSON *row = cJSON_GetArrayItem(rows, ri);
    const char *ts   = cell(row, iTs);
    const char *orig = cell(row, iOrig);
    const char *stat = cell(row, iStat);
    const char *mime = cell(row, iMime);
    const char *dig  = cell(row, iDig);
    if (!ts && !orig && !dig) continue;

    char rk[480];
    if (orig && strlen(orig) <= 360) {
      snprintf(rk, sizeof rk, "%s|%s|%s", dig ? dig : "", ts ? ts : "", orig);
    } else {
      char hashed[21];
      const char *parts[1] = { orig ? orig : h->host };
      feed_hash_key(hashed, parts, 1);
      snprintf(rk, sizeof rk, "%s|%s|url-sha1:%s", dig ? dig : "", ts ? ts : "",
               hashed);
    }
    if (!strcmp(rk, prev)) { h->folded++; continue; }
    snprintf(prev, sizeof prev, "%s", rk);

    /* title = row.original || row._target */
    const char *title = orig ? orig : h->host;

    char summ[256];
    snprintf(summ, sizeof summ, "%s \xc2\xb7 %s \xc2\xb7 %s",
      h->host, mime ? mime : "unknown mime", stat ? stat : "?");

    char linkbuf[1024];
    const char *link = NULL;
    if (ts && orig) {
      snprintf(linkbuf, sizeof linkbuf, "https://web.archive.org/web/%s/%s", ts, orig);
      link = linkbuf;
    }

    char isobuf[24];
    const char *pub = NULL;
    if (is_ts14(ts)) {
      snprintf(isobuf, sizeof isobuf, "%.4s-%.2s-%.2sT%.2s:%.2s:%.2sZ",
               ts, ts + 4, ts + 6, ts + 8, ts + 10, ts + 12);
      pub = isobuf;
    }

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "target", h->host);
    cJSON_AddItemToObject(props, "timestamp", ts ? cJSON_CreateString(ts) : cJSON_CreateNull());
    cJSON_AddItemToObject(props, "original", orig ? cJSON_CreateString(orig) : cJSON_CreateNull());
    cJSON_AddItemToObject(props, "mimetype", mime ? cJSON_CreateString(mime) : cJSON_CreateNull());
    cJSON_AddItemToObject(props, "statuscode", stat ? cJSON_CreateString(stat) : cJSON_CreateNull());
    cJSON_AddItemToObject(props, "digest", dig ? cJSON_CreateString(dig) : cJSON_CreateNull());
    cJSON_AddNumberToObject(props, "cdx_page", (double)page);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char tags[96];
    snprintf(tags, sizeof tags, "[\"wayback\",\"host:%s\"]", h->host);

    intel_item it = {0};
    it.remote_key      = rk;
    it.title           = title;
    it.summary         = summ;
    it.link            = link;
    it.lang            = "ja";
    it.published_at    = pub;
    it.record_type     = "wayback-jp";
    it.properties_json = pj;
    it.tags_json       = tags;
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

/* The per-host disclosure: what this run read of the host's index, and where
 * the next run starts. Filed whenever the run did not read the whole index. */
static void wb_notice(intel_sink *sink, const wb_host *h, int budget) {
  char reason[400], fdesc[64] = "";
  feed_status_describe(h->failed_status, fdesc, sizeof fdesc);
  if (h->pages_total < 0)
    snprintf(reason, sizeof reason, "the CDX page count for %s %s (%s), so "
             "none of its captures were read this run", h->host,
             h->failed_status == FEED_ST_UNKNOWN ? "was not asked for: the archive had "
             "already refused this run" : "failed", fdesc);
  else if (h->failed_page >= 0)
    snprintf(reason, sizeof reason, "CDX page %ld of %s answered %s, so this "
             "run read %ld of its %ld pages (from page %ld)", h->failed_page,
             h->host, fdesc, h->read, h->pages_total, h->start);
  else
    snprintf(reason, sizeof reason, "each run reads %d of a host's CDX index "
             "pages and resumes where the last stopped; this run read pages "
             "%ld-%ld of %s's %ld", budget, h->start, h->start + h->read - 1,
             h->host, h->pages_total);
  cJSON *extra = cJSON_CreateObject();
  if (extra) {
    char url[256];
    snprintf(url, sizeof url, "%s?url=%s&matchType=prefix", WB_CDX, h->host);
    cJSON_AddStringToObject(extra, "url", url);
    cJSON_AddStringToObject(extra, "target", h->host);
    if (h->pages_total >= 0) cJSON_AddNumberToObject(extra, "cdx_pages_total", (double)h->pages_total);
    else                     cJSON_AddNullToObject(extra, "cdx_pages_total");
    cJSON_AddNumberToObject(extra, "pages_read", (double)h->read);
    if (h->read > 0) {
      cJSON_AddNumberToObject(extra, "first_page_read", (double)h->start);
      cJSON_AddNumberToObject(extra, "last_page_read", (double)(h->start + h->read - 1));
    }
    cJSON_AddNumberToObject(extra, "next_page", (double)h->next);
    cJSON_AddStringToObject(extra, "records_available_note",
                            "the CDX API counts index pages, not captures");
    if (h->failed_page >= 0) {
      cJSON_AddNumberToObject(extra, "failed_page", (double)h->failed_page);
      cJSON_AddNumberToObject(extra, "failed_page_status", (double)h->failed_status);
    }
  }
  jo_truncation_notice_ex(sink, WB_SRC, h->host, h->used, -1, reason,
    h->failed_page >= 0 || h->pages_total < 0
      ? "re-run; the walk resumes from next_page"
      : "the next run continues from next_page; raise $JO_WAYBACK_PAGES to read "
        "more of each host per run",
    extra);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "accept: application/json", NULL };
  int budget = WB_PAGES_DEF;
  const char *be = getenv("JO_WAYBACK_PAGES");
  if (be && *be) { int v = atoi(be); if (v > 0) budget = v; }

  int n = 0, hosts_ok = 0, halted = 0;
  for (int ti = 0; ti < NT; ti++) {
    wb_host h;
    memset(&h, 0, sizeof h);
    h.host = TARGETS[ti];
    h.failed_page = -1;
    if (halted) {
      /* The archive said stop: this host was not asked at all, and its
       * cursor is carried forward untouched. */
      h.pages_total = -1;
      h.failed_status = FEED_ST_UNKNOWN;
      h.next = wb_saved_next(ctx, h.host);
      wb_notice(sink, &h, budget);
      continue;
    }
    if (ti > 0) wb_pause(ctx);
    if (ctx->cancel && *ctx->cancel) break;
    long st = 0;
    h.pages_total = wb_num_pages(ctx, h.host, &st);
    if (h.pages_total < 0) {
      h.failed_status = st;
      h.next = wb_saved_next(ctx, h.host);
      wb_notice(sink, &h, budget);
      fprintf(stderr, "[wayback-jp] %s: page count failed (HTTP %ld)\n", h.host, st);
      if (wb_stop_status(st)) halted = 1;
      continue;
    }
    hosts_ok++;
    /* Resume where the last run stopped — unless one run covers the whole
     * index, in which case there is nothing to resume. */
    h.start = h.pages_total > budget ? wb_saved_next(ctx, h.host) : 0;
    if (h.start >= h.pages_total) h.start = 0;
    long end = h.start + budget;
    if (end > h.pages_total) end = h.pages_total;

    for (long p = h.start; p < end; p++) {
      if (ctx->cancel && *ctx->cancel) break;
      wb_pause(ctx);
      char url[512];
      snprintf(url, sizeof url, "%s?url=%s&matchType=prefix&output=json" WB_FL
               "&page=%ld", WB_CDX, h.host, p);
      feed_last_json_status_reset();
      cJSON *rows = feed_get_json_h(ctx->http, url, hdrs, 120000);
      if (!cJSON_IsArray(rows)) {
        long fs = feed_last_json_status();
        cJSON_Delete(rows);
        h.failed_page = p;
        h.failed_status = fs;
        fprintf(stderr, "[wayback-jp] %s: CDX page %ld failed (status %ld)\n",
                h.host, p, fs);
        if (wb_stop_status(fs)) halted = 1;
        break;
      }
      h.used += wb_emit_page(sink, &h, rows, p);
      h.read++;
      cJSON_Delete(rows);
    }
    long after = h.start + h.read;
    h.next = (after >= h.pages_total) ? 0 : after;   /* wrap after the last page */
    n += h.used;
    fprintf(stderr, "[wayback-jp] %s: %d captures from CDX pages %ld-%ld of %ld "
            "(%d repeats folded); next run starts at page %ld\n", h.host, h.used,
            h.start, h.start + h.read - 1, h.pages_total, h.folded, h.next);
    if (h.read < h.pages_total) wb_notice(sink, &h, budget);
  }
  fprintf(stderr, "[wayback-jp] emitted %d captures (%d of %d hosts answered%s)\n",
          n, hosts_ok, NT, halted ? "; the archive asked us to stop" : "");
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
