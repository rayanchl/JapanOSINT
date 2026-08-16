/* lib/jsonstream.c — see jsonstream.h. */
#include "jsonstream.h"
#include "jsonlist.h"
#include "core/hostgate.h"
#include "core/httpclient.h"   /* http_client_global_init: the one curl_global_init */
#include "third_party/cJSON.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Elements bigger than this are skipped and counted rather than truncated and
 * guessed at — the same choice lib/bigfile.c makes for over-long lines. A
 * government register record is a few KB; 4 MB means the boundary scan has lost
 * sync or the upstream shape is not what we think. */
#define JS_MAX_ELEM   (4u * 1024u * 1024u)
/* Window kept while hunting for the array start. Generous enough for a large
 * enclosing-object header, bounded so a non-array document cannot grow it. */
#define JS_MAX_PREFIX (1u * 1024u * 1024u)

typedef struct {
  intel_sink *sink;
  const char *source_id, *record_type, *lang, *tags_json;

  char  *buf;            /* rolling window                                  */
  size_t len, cap;

  int    started;        /* have we found the opening '[' of the array?     */
  int    depth;          /* brace/bracket depth INSIDE the current element  */
  int    instr, esc;     /* string state, so braces in strings do not count */
  /* How far into the current element we have ALREADY scanned. An element
   * routinely spans several network chunks, and the first version of this
   * restarted the scan at the element's first byte on every new chunk while
   * keeping the depth counter from the previous pass — so every re-seen '{'
   * incremented depth again, it never returned to zero, and boundaries were
   * missed. Measured: 12 elements found in 105 MB of a 122k-record array.
   * Resume where we stopped instead. */
  size_t scan_pos;
  int    in_elem;

  long   emitted, scanned, oversize;
  long   max_records;    /* 0 = unlimited                                   */
  int    hit_ceiling;
  int    done;           /* array closed                                    */

  const char *path;      /* dotted path to the array, or NULL/"" for root   */
} js_ctx;

static int js_reserve(js_ctx *c, size_t extra) {
  if (c->len + extra + 1 <= c->cap) return 1;
  size_t want = c->cap ? c->cap : 65536;
  while (want < c->len + extra + 1) want *= 2;
  char *n = realloc(c->buf, want);
  if (!n) return 0;
  c->buf = n;
  c->cap = want;
  return 1;
}

/* Drop everything before `upto`, keeping the tail. Called once per element so
 * the buffer never accumulates the file. */
static void js_consume(js_ctx *c, size_t upto) {
  if (upto == 0) return;
  if (upto >= c->len) { c->len = 0; return; }
  memmove(c->buf, c->buf + upto, c->len - upto);
  c->len -= upto;
}

/* Find the '[' that opens the array we want.
 *
 * Root case: the first '[' in the document. Nested case: the last segment of
 * the dotted path used as an object key, then the next '['. This deliberately
 * does not implement full path resolution — anything deeper than one nested
 * array would mean buffering the enclosing object, which is the very thing this
 * file exists to avoid. */
static int js_find_array(js_ctx *c) {
  if (!c->path || !*c->path || !strcmp(c->path, "*")) {
    for (size_t i = 0; i < c->len; i++) {
      if (c->buf[i] == '[') { js_consume(c, i + 1); c->started = 1; return 1; }
      if (c->buf[i] == '{') break;   /* enclosing object: need the key below */
    }
  }
  const char *seg = c->path;
  if (seg && *seg) {
    const char *dot = strrchr(seg, '.');
    if (dot) seg = dot + 1;
    char key[128];
    snprintf(key, sizeof key, "\"%s\"", seg);
    size_t klen = strlen(key);
    if (c->len >= klen) {
      for (size_t i = 0; i + klen <= c->len; i++) {
        if (memcmp(c->buf + i, key, klen) != 0) continue;
        for (size_t j = i + klen; j < c->len; j++) {
          char ch = c->buf[j];
          if (ch == ':' || ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') continue;
          if (ch == '[') { js_consume(c, j + 1); c->started = 1; return 1; }
          break;                       /* key present but not an array here */
        }
      }
    }
  }
  /* Not found yet. Keep a bounded tail so a key split across two chunks is
   * still matched, and discard the rest rather than growing without limit. */
  if (c->len > JS_MAX_PREFIX) js_consume(c, c->len - 4096);
  return 0;
}

/* Emit one complete element. Reuses jsonlist_emit's "." mode so a streamed row
 * is labelled by exactly the same rules as a buffered one — no second copy of
 * the field-name precedence to drift out of sync. */
static void js_emit_elem(js_ctx *c, const char *s, size_t n) {
  c->scanned++;
  if (n == 0 || n > JS_MAX_ELEM) { c->oversize++; return; }
  char *z = malloc(n + 1);
  if (!z) return;
  memcpy(z, s, n);
  z[n] = 0;
  cJSON *el = cJSON_Parse(z);
  free(z);
  if (!el) return;                      /* malformed element: skip, counted  */
  c->emitted += jsonlist_emit(c->sink, c->source_id, el, ".",
                              c->record_type, c->lang, c->tags_json);
  cJSON_Delete(el);
}

/* Scan the buffer for complete elements and emit them, consuming as we go.
 *
 * Invariant: when this returns with in_elem set, buf[0] is the first byte of
 * the partial element and scan_pos is how much of it has been examined, with
 * depth/instr/esc describing the state at exactly that point. The next chunk
 * therefore resumes rather than restarts, which is what keeps depth honest. */
static void js_scan(js_ctx *c) {
  for (;;) {
    if (!c->in_elem) {
      /* skip separators and detect the end of the array */
      size_t i = 0;
      while (i < c->len) {
        char ch = c->buf[i];
        if (ch == ']') { c->done = 1; c->len = 0; return; }
        if (ch == ',' || ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') { i++; continue; }
        break;
      }
      if (i) js_consume(c, i);
      if (c->len == 0) return;
      if (c->max_records && c->emitted >= c->max_records) {
        c->hit_ceiling = 1; c->done = 1; c->len = 0; return;
      }
      /* the element now starts at buf[0] — js_consume above guaranteed it */
      c->in_elem = 1; c->depth = 0; c->instr = 0; c->esc = 0;
      c->scan_pos = 0;
    }

    size_t i = c->scan_pos;
    int closed = 0;
    size_t end = 0;                       /* one past the element's last byte */
    for (; i < c->len; i++) {
      char d = c->buf[i];
      if (c->instr) {
        if (c->esc) c->esc = 0;
        else if (d == '\\') c->esc = 1;
        else if (d == '"') c->instr = 0;
        continue;
      }
      if (d == '"') { c->instr = 1; continue; }
      if (d == '{' || d == '[') { c->depth++; continue; }
      if (d == '}' || d == ']') {
        c->depth--;
        if (c->depth == 0) { closed = 1; end = i + 1; break; }
        continue;
      }
      /* a scalar element (string/number/true/null) ends at a depth-0 delimiter */
      if (c->depth == 0 && (d == ',' || d == ']')) { closed = 1; end = i; break; }
    }

    if (!closed) {
      c->scan_pos = c->len;               /* resume here when more bytes land */
      if (c->len > JS_MAX_ELEM) {         /* lost sync, or an absurd record   */
        c->oversize++;
        c->in_elem = 0; c->len = 0; c->scan_pos = 0;
      }
      return;
    }

    js_emit_elem(c, c->buf, end);
    c->in_elem = 0;
    c->scan_pos = 0;
    js_consume(c, end);
  }
}

static size_t js_write(char *ptr, size_t sz, size_t nm, void *ud) {
  js_ctx *c = (js_ctx *)ud;
  size_t n = sz * nm;
  if (c->done) return n;                 /* array closed; drain the rest */
  if (!js_reserve(c, n)) return 0;
  memcpy(c->buf + c->len, ptr, n);
  c->len += n;
  if (!c->started && !js_find_array(c)) return n;
  js_scan(c);
  return n;
}

int jsonstream_emit(intel_sink *sink, const char *source_id, const char *url,
                    int timeout_ms, const char *path, const char *record_type,
                    const char *lang, const char *tags_json) {
  if (!sink || !url || !*url) return -1;

  /* The SSRF floor, before a byte moves. A blocked destination is an explicit
   * failure, not a silent skip. */
  int hg = hostgate_url_check(url);
  if (hg != HG_URL_OK) {
    fprintf(stderr, "[%s] blocked: %s\n", source_id, hostgate_url_reason(hg));
    return -1;
  }

  js_ctx c;
  memset(&c, 0, sizeof c);
  c.sink = sink; c.source_id = source_id; c.record_type = record_type;
  c.lang = lang; c.tags_json = tags_json; c.path = path;
  const char *env = getenv("JO_JSONSTREAM_MAX_RECORDS");
  c.max_records = (env && *env) ? strtol(env, NULL, 10) : 0;

  http_client_global_init();
  CURL *e = curl_easy_init();
  if (!e) { free(c.buf); return -1; }
  curl_easy_setopt(e, CURLOPT_URL, url);
  curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, js_write);
  curl_easy_setopt(e, CURLOPT_WRITEDATA, &c);
  curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(e, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(e, CURLOPT_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(e, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(e, CURLOPT_ACCEPT_ENCODING, "");   /* let curl gunzip */
  curl_easy_setopt(e, CURLOPT_USERAGENT,
                   "Mozilla/5.0 (compatible; JapanOSINT/1.0)");
  curl_easy_setopt(e, CURLOPT_TIMEOUT_MS,
                   (long)(timeout_ms > 0 ? timeout_ms : 900000));
  curl_easy_setopt(e, CURLOPT_CONNECTTIMEOUT, 20L);
  curl_easy_setopt(e, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(e, CURLOPT_FAILONERROR, 1L);

  CURLcode rc = curl_easy_perform(e);
  long status = 0;
  curl_easy_getinfo(e, CURLINFO_RESPONSE_CODE, &status);
  curl_off_t got = 0;
  curl_easy_getinfo(e, CURLINFO_SIZE_DOWNLOAD_T, &got);
  curl_easy_cleanup(e);

  int transport_failed = (rc != CURLE_OK && c.emitted == 0);
  free(c.buf);

  if (transport_failed) {
    fprintf(stderr, "[%s] stream failed: %s (status %ld)\n",
            source_id, curl_easy_strerror(rc), status);
    return -1;
  }

  /* Anything left on the table is disclosed as data. `done` false means the
   * array never closed — we were cut off mid-file and the true total is
   * unknown, which is exactly the case a log line would hide. */
  int short_read = (!c.done && rc != CURLE_OK) || c.hit_ceiling || c.oversize > 0;
  if (short_read) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "source_id", source_id);
    cJSON_AddStringToObject(p, "endpoint", url);
    cJSON_AddNumberToObject(p, "records_used", (double)c.emitted);
    cJSON_AddNumberToObject(p, "elements_scanned", (double)c.scanned);
    cJSON_AddNumberToObject(p, "bytes_downloaded", (double)got);
    if (c.oversize)
      cJSON_AddNumberToObject(p, "elements_too_large_skipped", (double)c.oversize);
    cJSON_AddStringToObject(p, "records_available",
      c.done ? "all of them" : "unknown — the array never closed");
    cJSON_AddStringToObject(p, "reason",
      c.hit_ceiling ? "JO_JSONSTREAM_MAX_RECORDS ceiling stopped the walk"
                    : (c.oversize && c.done)
                      ? "some elements exceeded the per-element size limit and were skipped"
                      : "the transfer ended before the array closed");
    cJSON_AddStringToObject(p, "remedy",
      "raise $JO_JSONSTREAM_MAX_RECORDS, or re-run — see docs/SOURCE_EXHAUSTIVENESS.md");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);
    char title[224];
    snprintf(title, sizeof title, "%s streamed %ld records and stopped early",
             source_id, c.emitted);
    intel_item note = {0};
    note.remote_key      = "truncation";
    note.title           = title;
    note.lang            = "en";
    note.record_type     = "collector-truncation-notice";
    note.properties_json = pj ? pj : "{}";
    note.tags_json       = "[\"truncation-notice\"]";
    sink->emit(sink, &note);
    free(pj);
  }

  fprintf(stderr, "[%s] streamed %ld of %ld elements from %lld bytes%s\n",
          source_id, c.emitted, c.scanned, (long long)got,
          c.done ? "" : " (INCOMPLETE)");
  return (int)c.emitted;
}
