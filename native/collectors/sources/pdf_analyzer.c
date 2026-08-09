/* collectors/osint/sources/pdf_analyzer.c — PDF_ANALYZER, ported from OSINTsaas
 * document_analyzer.c onto the JapanOSINT source ABI (mirrors the existing
 * DOCUMENT_ANALYZER PDF metadata path, but pivots on a PDF *URL*: fetch then
 * parse). Extracts forensic document metadata — version, /Author, /Title,
 * /Producer, /Creator, /CreationDate, /ModDate — plus an approximate page
 * count, by scanning the raw bytes (Info-dict values are plaintext in most
 * PDFs). No external parser. Honest-empty for non-PDF / no metadata. */
#include "../../source.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *PDF_KEYS[] = {
  "/Title", "/Author", "/Subject", "/Keywords", "/Creator",
  "/Producer", "/CreationDate", "/ModDate", NULL
};

/* Find `key` then read a following "(...)" literal string into out. */
static int pdf_value(const char *buf, size_t len, const char *key, char *out, size_t cap) {
  const char *p = buf; size_t klen = strlen(key);
  while ((p = memmem(p, (size_t)(buf + len - p), key, klen)) != NULL) {
    const char *q = p + klen;
    while (q < buf + len && (*q == ' ' || *q == '\t')) q++;
    if (q < buf + len && *q == '(') {
      q++; size_t o = 0; int depth = 1;
      while (q < buf + len && o < cap - 1) {
        if (*q == '\\' && q + 1 < buf + len) { out[o++] = q[1]; q += 2; continue; }
        if (*q == '(') depth++;
        else if (*q == ')') { if (--depth == 0) break; }
        out[o++] = *q++;
      }
      out[o] = 0;
      if (o > 0) return 1;
    }
    p += klen;
  }
  return 0;
}

static int count_occurrences(const char *buf, size_t len, const char *needle) {
  int c = 0; size_t nl = strlen(needle); const char *p = buf;
  while ((p = memmem(p, (size_t)(buf + len - p), needle, nl)) != NULL) { c++; p += nl; }
  return c;
}

static int pdf_run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = ctx->entity;
  if (!url || strncmp(url, "http", 4) != 0) return 0;
  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, NULL, NULL, 0, 20000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body || hr.body_len < 5) { http_response_free(&hr); return 0; }
  if (memcmp(hr.body, "%PDF-", 5) != 0) { http_response_free(&hr); return 0; }  /* not a PDF */

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "source", "pdf-metadata");
  cJSON_AddStringToObject(data, "pdf_url", url);
  char ver[16] = {0};
  for (int i = 0; i < 8 && hr.body[5 + i] && hr.body[5 + i] != '\n' && hr.body[5 + i] != '\r'; i++)
    ver[i] = hr.body[5 + i];
  if (*ver) cJSON_AddStringToObject(data, "pdf_version", ver);

  int found = 0; char val[1024];
  for (int i = 0; PDF_KEYS[i]; i++)
    if (pdf_value(hr.body, hr.body_len, PDF_KEYS[i], val, sizeof val)) {
      cJSON_AddStringToObject(data, PDF_KEYS[i] + 1, val);  /* drop leading '/' */
      found++;
    }
  int pages = count_occurrences(hr.body, hr.body_len, "/Type /Page");
  if (!pages) pages = count_occurrences(hr.body, hr.body_len, "/Type/Page");
  if (pages > 0) cJSON_AddNumberToObject(data, "approx_pages", pages);
  cJSON_AddNumberToObject(data, "bytes", (double)hr.body_len);
  http_response_free(&hr);

  if (!found && !pages) { cJSON_Delete(data); return 0; }   /* nothing useful */

  char *bj = cJSON_PrintUnformatted(data);
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "PDF_ANALYZER");
  cJSON_AddStringToObject(props, "entity", url);
  char *pj = cJSON_PrintUnformatted(props);
  char rk[256], title[160];
  snprintf(rk, sizeof rk, "pdf:%s", url);
  snprintf(title, sizeof title, "PDF metadata — %.80s", url);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.summary = "pdf metadata";
  it.link = url; it.record_type = "osint_service_result"; it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"PDF_ANALYZER\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); cJSON_Delete(data); cJSON_Delete(props);
  /* AUDIT NOTE (slice a3): this was `return rc >= 0 ? 1 : 0`, i.e. it returned
   * 1 on SUCCESS and 0 on emit failure — exactly inverted. core/scheduler.c:70
   * treats any non-zero rc as status='error', so every successful PDF analysis
   * logged an error and opened a collector_anomaly (verified: rc=1 records=1
   * → "anomaly opened: PDF_ANALYZER verdict=status_bad"), while a failed emit
   * looked healthy. run() returns a STATUS: 0 ok, -1 failure. */
  return rc >= 0 ? 0 : -1;
}

static const source_def pdf_def = {
  .id = "PDF_ANALYZER", .collector = "osint", .name = "PDF Analyzer", .run = pdf_run,
  .category = "investigation", .type = "api", .url = "internal://pdf",
  .description = "PDF URL → document metadata (author/title/producer/dates, version, pages).",
  .free_tier = 1,
};
REGISTER_SOURCE(pdf_def)
