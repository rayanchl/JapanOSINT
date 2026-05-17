/* collectors/osint/sources/document_analyzer.c
 * OSINT service — faithful port of OSINTsaas osint_tools/document_analyzer.c
 * (document_analyze → handle_document_analyzer). Canonical SERVICE id in
 * osint_dispatcher.c: {SERVICE_DOCUMENT_ANALYZER, handle_document_analyzer,
 * "DOCUMENT_ANALYZER", true}. (PDF_ANALYZER alias → handle_pdf_analyzer, a
 * different handler → not this file.) Entity = URL (image→OCR/QR, or doc),
 * local file path, or file hash. OCR_SPACE_API_KEY optional — upstream falls
 * back to the public demo key "helloworld", so NOT a hard gate (the demo
 * path still works). OCR via api.ocr.space, QR via api.qrserver.com (HTTP
 * only — no OpenCV/Tesseract, faithfully pure-C). Reproduces upstream `root`
 * branch-for-branch (url/local_file/hash_or_unknown). success=true conf 80.
 * Emits one osint_service_result row (body = {success,confidence,data}). */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

static void uri_encode(const char *in, char *out, size_t cap) {
  static const char *keep = "-_.!~*'()";
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && w + 4 < cap; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(keep, c)) out[w++] = (char)c;
    else { snprintf(out + w, cap - w, "%%%02X", c); w += 3; }
  }
  out[w] = 0;
}

static const char *PDF_KEYS[] = {
  "/Title","/Author","/Subject","/Keywords","/Creator",
  "/Producer","/CreationDate","/ModDate","/Trapped", NULL };

static char *extract_pdf_value(const char *content, const char *key) {
  const char *pos = strstr(content, key);
  if (!pos) return NULL;
  pos += strlen(key);
  while (*pos && isspace((unsigned char)*pos)) pos++;
  char *v = NULL;
  if (*pos == '(') {
    pos++;
    const char *e = strchr(pos, ')');
    if (e) { size_t L = e - pos; v = malloc(L + 1); if (v) { memcpy(v, pos, L); v[L] = 0; } }
  } else if (*pos == '<') {
    pos++;
    const char *e = strchr(pos, '>');
    if (e) { size_t L = e - pos; v = malloc(L + 1); if (v) { memcpy(v, pos, L); v[L] = 0; } }
  } else if (*pos == '/') {
    pos++;
    const char *e = pos;
    while (*e && !isspace((unsigned char)*e) && *e != '/' && *e != '>') e++;
    size_t L = e - pos; v = malloc(L + 1); if (v) { memcpy(v, pos, L); v[L] = 0; }
  }
  return v;
}

static cJSON *parse_pdf_metadata(const char *path) {
  cJSON *m = cJSON_CreateObject();
  cJSON_AddStringToObject(m, "file_type", "PDF");
  FILE *fp = fopen(path, "rb");
  if (!fp) { cJSON_AddStringToObject(m, "error", "Could not open file"); return m; }
  char *buf = malloc(65536);
  if (!buf) { fclose(fp); return m; }
  size_t br = fread(buf, 1, 65535, fp);
  buf[br] = 0;
  fclose(fp);
  if (strncmp(buf, "%PDF-", 5) == 0) {
    char ver[8] = {0};
    memcpy(ver, buf + 5, 3);
    cJSON_AddStringToObject(m, "pdf_version", ver);
  }
  cJSON *props = cJSON_CreateObject();
  for (int i = 0; PDF_KEYS[i]; i++) {
    char *v = extract_pdf_value(buf, PDF_KEYS[i]);
    if (v) { cJSON_AddStringToObject(props, PDF_KEYS[i] + 1, v); free(v); }
  }
  cJSON_AddItemToObject(m, "properties", props);
  cJSON_AddBoolToObject(m, "encrypted", strstr(buf, "/Encrypt") ? 1 : 0);
  int pc = 0; const char *p = buf;
  while ((p = strstr(p, "/Type/Page")) != NULL) { if (p[10] != 's') pc++; p++; }
  if (pc > 0) cJSON_AddNumberToObject(m, "estimated_pages", pc);
  if (strstr(buf, "/JavaScript") || strstr(buf, "/JS")) {
    cJSON_AddBoolToObject(m, "contains_javascript", 1);
    cJSON_AddStringToObject(m, "security_warning",
      "PDF contains JavaScript which may pose security risks");
  }
  if (strstr(buf, "/EmbeddedFiles")) cJSON_AddBoolToObject(m, "contains_embedded_files", 1);
  if (strstr(buf, "/AcroForm")) cJSON_AddBoolToObject(m, "contains_forms", 1);
  free(buf);
  return m;
}

static cJSON *calc_hashes(const char *path) {
  cJSON *h = cJSON_CreateObject();
  FILE *fp = fopen(path, "rb");
  if (!fp) { cJSON_AddStringToObject(h, "error", "Could not open file"); return h; }
  fseek(fp, 0, SEEK_END);
  long sz = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  cJSON_AddNumberToObject(h, "file_size", sz);
  unsigned char *buf = malloc(sz > 0 ? sz : 1);
  if (!buf) { fclose(fp); cJSON_AddStringToObject(h, "error", "Memory allocation failed"); return h; }
  size_t rd = fread(buf, 1, sz, fp);
  fclose(fp);
  unsigned long sh = 0;
  for (size_t i = 0; i < rd; i++) sh = sh * 31 + buf[i];
  char hs[32];
  snprintf(hs, sizeof hs, "%016lx", sh);
  cJSON_AddStringToObject(h, "checksum", hs);
  free(buf);
  return h;
}

static cJSON *analyze_file(const char *path) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "filepath", path);
  const char *ext = strrchr(path, '.');
  if (ext) {
    cJSON_AddStringToObject(r, "extension", ext + 1);
    if (strcasecmp(ext, ".pdf") == 0)
      cJSON_AddItemToObject(r, "pdf_metadata", parse_pdf_metadata(path));
    else if (strcasecmp(ext, ".doc") == 0)
      cJSON_AddStringToObject(r, "note",
        "Legacy .doc format. Limited metadata extraction available.");
  }
  cJSON_AddItemToObject(r, "hashes", calc_hashes(path));
  struct stat st;
  if (stat(path, &st) == 0) {
    cJSON_AddNumberToObject(r, "size_bytes", st.st_size);
    cJSON_AddNumberToObject(r, "modified_time", (double)st.st_mtime);
    cJSON_AddNumberToObject(r, "access_time", (double)st.st_atime);
  }
  return r;
}

static cJSON *perform_ocr(http_client *h, const char *image_url) {
  const char *key = getenv("OCR_SPACE_API_KEY");
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "service", "OCR.space");
  if (!key || !*key) {
    fprintf(stderr, "[DOCUMENT_ANALYZER] OCR using demo key (no OCR_SPACE_API_KEY)\n");
    key = "helloworld";
    cJSON_AddStringToObject(r, "api_tier", "demo");
  } else {
    cJSON_AddStringToObject(r, "api_tier", "authenticated");
  }
  char enc[1024]; uri_encode(image_url, enc, sizeof enc);
  char post[2048];
  snprintf(post, sizeof post,
    "apikey=%s&url=%s&language=eng&isOverlayRequired=false", key, enc);
  const char *hdr[2] = { "Content-Type: application/x-www-form-urlencoded", NULL };
  http_response hr = {0};
  int hc = http_request(h, "POST", "https://api.ocr.space/parse/image",
                         hdr, post, strlen(post), 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    http_response_free(&hr);
    cJSON_AddStringToObject(r, "error", "OCR API request failed");
    return r;
  }
  cJSON *j = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!j) { cJSON_AddStringToObject(r, "error", "Failed to parse OCR response"); return r; }
  cJSON *pr = cJSON_GetObjectItem(j, "ParsedResults");
  if (pr && cJSON_IsArray(pr)) {
    cJSON *f = cJSON_GetArrayItem(pr, 0);
    if (f) {
      cJSON *t = cJSON_GetObjectItem(f, "ParsedText");
      if (t && cJSON_IsString(t)) {
        cJSON_AddStringToObject(r, "extracted_text", t->valuestring);
        int wc = 0; const char *p = t->valuestring;
        while (*p) {
          while (*p && isspace((unsigned char)*p)) p++;
          if (*p) { wc++; while (*p && !isspace((unsigned char)*p)) p++; }
        }
        cJSON_AddNumberToObject(r, "word_count", wc);
        cJSON_AddNumberToObject(r, "char_count", (double)strlen(t->valuestring));
      }
      cJSON *ec = cJSON_GetObjectItem(f, "FileParseExitCode");
      if (ec && cJSON_IsNumber(ec))
        cJSON_AddBoolToObject(r, "success", ec->valueint == 1);
    }
  }
  cJSON *pt = cJSON_GetObjectItem(j, "ProcessingTimeInMilliseconds");
  if (pt && cJSON_IsNumber(pt))
    cJSON_AddNumberToObject(r, "processing_time_ms", pt->valueint);
  cJSON_Delete(j);
  return r;
}

static cJSON *read_qr(http_client *h, const char *image_url) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "service", "goQR.me");
  char enc[1024]; uri_encode(image_url, enc, sizeof enc);
  char url[1200];
  snprintf(url, sizeof url,
    "https://api.qrserver.com/v1/read-qr-code/?fileurl=%s", enc);
  http_response hr = {0};
  int hc = http_request(h, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    http_response_free(&hr);
    cJSON_AddStringToObject(r, "error", "QR API request failed");
    return r;
  }
  cJSON *j = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!j) { cJSON_AddStringToObject(r, "error", "Failed to parse QR response"); return r; }
  if (cJSON_IsArray(j)) {
    cJSON *f = cJSON_GetArrayItem(j, 0);
    if (f) {
      cJSON *sym = cJSON_GetObjectItem(f, "symbol");
      if (sym && cJSON_IsArray(sym)) {
        cJSON *s0 = cJSON_GetArrayItem(sym, 0);
        if (s0) {
          cJSON *d = cJSON_GetObjectItem(s0, "data");
          cJSON *em = cJSON_GetObjectItem(s0, "error");
          if (d && cJSON_IsString(d) && strlen(d->valuestring) > 0) {
            const char *c = d->valuestring;
            cJSON_AddStringToObject(r, "qr_content", c);
            cJSON_AddBoolToObject(r, "qr_detected", 1);
            const char *ct;
            if (!strncmp(c, "http://", 7) || !strncmp(c, "https://", 8)) ct = "URL";
            else if (!strncmp(c, "mailto:", 7)) ct = "Email";
            else if (!strncmp(c, "tel:", 4)) ct = "Phone";
            else if (!strncmp(c, "WIFI:", 5)) ct = "WiFi";
            else if (!strncmp(c, "BEGIN:VCARD", 11)) ct = "vCard";
            else ct = "Text";
            cJSON_AddStringToObject(r, "content_type", ct);
          } else if (em && cJSON_IsString(em)) {
            cJSON_AddStringToObject(r, "error", em->valuestring);
            cJSON_AddBoolToObject(r, "qr_detected", 0);
          }
        }
      }
    }
  }
  cJSON_Delete(j);
  return r;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", q);
  cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));

  if (!strncmp(q, "http://", 7) || !strncmp(q, "https://", 8)) {
    cJSON_AddStringToObject(root, "query_type", "url");
    const char *ext = strrchr(q, '.');
    int is_img = 0;
    if (ext) {
      is_img = !strcasecmp(ext, ".png") || !strcasecmp(ext, ".jpg") ||
               !strcasecmp(ext, ".jpeg") || !strcasecmp(ext, ".gif") ||
               !strcasecmp(ext, ".bmp") || !strcasecmp(ext, ".webp");
    }
    if (!ext || is_img) {
      cJSON_AddItemToObject(root, "ocr_result", perform_ocr(ctx->http, q));
      cJSON_AddItemToObject(root, "qr_result", read_qr(ctx->http, q));
    }
  } else if (q[0] == '/' || (q[0] == '.' && q[1] == '/')) {
    cJSON_AddStringToObject(root, "query_type", "local_file");
    struct stat st;
    if (stat(q, &st) == 0)
      cJSON_AddItemToObject(root, "file_analysis", analyze_file(q));
    else
      cJSON_AddStringToObject(root, "error", "File not found");
  } else {
    cJSON_AddStringToObject(root, "query_type", "hash_or_unknown");
    size_t len = strlen(q);
    int hex = 1;
    for (size_t i = 0; i < len && hex; i++) {
      char c = tolower((unsigned char)q[i]);
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) hex = 0;
    }
    if (hex && (len == 32 || len == 40 || len == 64)) {
      cJSON_AddStringToObject(root, "detected_type", "file_hash");
      cJSON_AddStringToObject(root, "hash_type",
        len == 32 ? "MD5" : len == 40 ? "SHA1" : "SHA256");
      cJSON_AddStringToObject(root, "virustotal_lookup_url",
        "https://www.virustotal.com/gui/file/");
      cJSON_AddStringToObject(root, "note",
        "Visit VirusTotal URL + hash to check file reputation. "
        "Set VIRUSTOTAL_API_KEY for automated lookup.");
    } else {
      cJSON_AddStringToObject(root, "note",
        "Provide a URL (image for OCR/QR, document for metadata), "
        "local file path, or file hash");
    }
  }

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 80);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DOCUMENT_ANALYZER");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 80);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300], title[320];
  snprintf(rk, sizeof rk, "docanalyze:%s", q);
  snprintf(title, sizeof title, "DOCUMENT_ANALYZER — %s", q);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "document analysis complete";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"DOCUMENT_ANALYZER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def document_analyzer_def = {
  .id = "DOCUMENT_ANALYZER", .collector = "osint",
  .name = "Document Analyzer", .name_ja = "ドキュメント解析",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(document_analyzer_def)
