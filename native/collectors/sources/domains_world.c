/* collectors/sources/domains_world.c
 * OSINT services — domain-intelligence pivots (entity = a domain name). Four
 * registered sources, one shared run() switching on ctx->source_id:
 *
 *   CERTSPOTTER     — SSL Mate Cert Spotter issuances API
 *                     (api.certspotter.com/v1/issuances). Keyless works but is
 *                     rate/scope limited; an optional CERTSPOTTER_TOKEN raises
 *                     limits (Bearer). One item per issued certificate.
 *   URLSCAN_SEARCH  — urlscan.io search API (urlscan.io/api/v1/search).
 *                     Keyless works for public scans; optional URLSCAN_API_KEY
 *                     (API-Key header) widens access. One item per scan result.
 *   CIRCL_PDNS      — CIRCL Passive DNS (www.circl.lu/pdns/query). GATED on
 *                     CIRCL_AUTH ("user:pass" Basic) — honest-empty when unset.
 *                     One item per historical resolution record.
 *   HACKERTARGET    — hackertarget.com hostsearch (plaintext CSV, keyless,
 *                     daily-limited). One item per "hostname,ip" line.
 *
 * HONESTY: every branch REAL-fetches and emits only parsed API/response data,
 * or honest-empty (return 0) on failure/no-match, or gated (stderr + 0). No
 * constructed URLs, names or counts are ever emitted as records. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- CERTSPOTTER -------------------------------------------------------- */
/* One issuance object → one intel_item. Fields per Cert Spotter v1 spec:
 * id, tbs_sha256, dns_names[], pubkey_sha256, not_before, not_after, cert{}. */
static int cs_emit(intel_sink *sink, const char *domain, const cJSON *iss) {
  if (!iss) return 0;
  const char *id     = jo_sv(iss, "id");
  const char *nb     = jo_sv(iss, "not_before");
  const char *na     = jo_sv(iss, "not_after");
  const char *tbs    = jo_sv(iss, "tbs_sha256");
  const cJSON *names = cJSON_GetObjectItem(iss, "dns_names");

  /* comma-join dns_names (real SANs from the cert) */
  char *san = NULL; size_t cap = 0, len = 0; int n = 0;
  if (cJSON_IsArray(names)) {
    const cJSON *d;
    cJSON_ArrayForEach(d, names) {
      if (!cJSON_IsString(d) || !d->valuestring) continue;
      const char *s = d->valuestring;
      size_t need = len + strlen(s) + 3;
      if (need > cap) { cap = need * 2; char *t = realloc(san, cap); if (!t) break; san = t; }
      if (n) { strcpy(san + len, ", "); len += 2; }
      strcpy(san + len, s); len += strlen(s);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  if (!id && !n) { free(san); return 0; }

  cJSON *data = cJSON_CreateObject();
  if (id)  cJSON_AddStringToObject(data, "issuance_id", id);
  if (tbs) cJSON_AddStringToObject(data, "tbs_sha256", tbs);
  if (san) cJSON_AddStringToObject(data, "dns_names", san);
  if (nb)  cJSON_AddStringToObject(data, "not_before", nb);
  if (na)  cJSON_AddStringToObject(data, "not_after", na);
  cJSON_AddStringToObject(data, "source", "Cert Spotter");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CERTSPOTTER");
  cJSON_AddStringToObject(props, "domain", domain);
  if (na) cJSON_AddStringToObject(props, "not_after", na);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = id ? id : (tbs ? tbs : san);
  it.title           = san ? san : (id ? id : domain);
  it.summary         = na;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = nb;
  it.record_type     = "certspotter-issuance";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"CERTSPOTTER\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(san);
  return rc >= 0 ? 1 : 0;
}

static int run_certspotter(const source_ctx *ctx, intel_sink *sink,
                           const char *domain, char *enc) {
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.certspotter.com/v1/issuances?domain=%s&include_subdomains=true"
    "&expand=dns_names&expand=not_before&expand=not_after", enc);

  const char *tok = jo_env("CERTSPOTTER_TOKEN");
  char auth[256];
  const char *hdrs_key[] = { auth, "Accept: application/json", NULL };
  const char *hdrs_none[] = { "Accept: application/json", NULL };
  const char *const *hdrs = hdrs_none;
  if (tok) { snprintf(auth, sizeof auth, "Authorization: Bearer %s", tok); hdrs = hdrs_key; }

  char *body = jo_get(ctx, url, hdrs, "certspotter");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  int emitted = 0;
  if (cJSON_IsArray(root)) {
    const cJSON *iss;
    cJSON_ArrayForEach(iss, root) emitted += cs_emit(sink, domain, iss);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[certspotter] emitted %d\n", emitted);
  return 0;
}

/* ---- URLSCAN_SEARCH ----------------------------------------------------- */
static int us_emit(intel_sink *sink, const char *domain, const cJSON *r) {
  if (!r) return 0;
  const cJSON *page = cJSON_GetObjectItem(r, "page");
  const cJSON *task = cJSON_GetObjectItem(r, "task");
  const char *purl = page ? jo_sv(page, "url") : NULL;
  const char *pdom = page ? jo_sv(page, "domain") : NULL;
  const char *pip  = page ? jo_sv(page, "ip") : NULL;
  const char *pcn  = page ? jo_sv(page, "country") : NULL;
  const char *psrv = page ? jo_sv(page, "server") : NULL;
  const char *ttime = task ? jo_sv(task, "time") : NULL;
  const char *tvis  = task ? jo_sv(task, "visibility") : NULL;
  const char *uid   = jo_sv(r, "_id");
  const char *result = jo_sv(r, "result");     /* urlscan result API URL */
  const char *screen = jo_sv(r, "screenshot");
  if (!purl && !uid) return 0;

  cJSON *data = cJSON_CreateObject();
  if (purl) cJSON_AddStringToObject(data, "url", purl);
  if (pdom) cJSON_AddStringToObject(data, "domain", pdom);
  if (pip)  cJSON_AddStringToObject(data, "ip", pip);
  if (pcn)  cJSON_AddStringToObject(data, "country", pcn);
  if (psrv) cJSON_AddStringToObject(data, "server", psrv);
  if (ttime) cJSON_AddStringToObject(data, "scanned_at", ttime);
  if (tvis) cJSON_AddStringToObject(data, "visibility", tvis);
  if (screen) cJSON_AddStringToObject(data, "screenshot", screen);
  if (result) cJSON_AddStringToObject(data, "result", result);
  cJSON_AddStringToObject(data, "source", "urlscan.io");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "URLSCAN_SEARCH");
  cJSON_AddStringToObject(props, "domain", domain);
  if (pip) cJSON_AddStringToObject(props, "ip", pip);
  if (pcn) cJSON_AddStringToObject(props, "country", pcn);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = uid ? uid : purl;
  it.title           = purl ? purl : (pdom ? pdom : domain);
  it.summary         = pip;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = ttime;
  it.link            = result ? result : purl;
  it.record_type     = "urlscan-result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"URLSCAN\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_urlscan(const source_ctx *ctx, intel_sink *sink,
                       const char *domain, char *enc) {
  char url[1024];
  snprintf(url, sizeof url,
    "https://urlscan.io/api/v1/search/?q=domain:%s&size=25", enc);

  const char *key = jo_env("URLSCAN_API_KEY");
  char auth[256];
  const char *hdrs_key[] = { auth, "Accept: application/json", NULL };
  const char *hdrs_none[] = { "Accept: application/json", NULL };
  const char *const *hdrs = hdrs_none;
  if (key) { snprintf(auth, sizeof auth, "API-Key: %s", key); hdrs = hdrs_key; }

  char *body = jo_get(ctx, url, hdrs, "urlscan");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) emitted += us_emit(sink, domain, r);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[urlscan] emitted %d\n", emitted);
  return 0;
}

/* ---- CIRCL_PDNS --------------------------------------------------------- */
/* CIRCL PDNS returns newline-delimited JSON (one object per line): rrname,
 * rrtype, rdata, time_first, time_last, count. GATED on CIRCL_AUTH. */
static int pdns_emit(intel_sink *sink, const char *domain, const cJSON *rec) {
  if (!rec) return 0;
  const char *rrname = jo_sv(rec, "rrname");
  const char *rrtype = jo_sv(rec, "rrtype");
  const char *rdata  = jo_sv(rec, "rdata");
  if (!rrname && !rdata) return 0;
  const cJSON *tf = cJSON_GetObjectItem(rec, "time_first");
  const cJSON *tl = cJSON_GetObjectItem(rec, "time_last");
  const cJSON *cnt = cJSON_GetObjectItem(rec, "count");

  cJSON *data = cJSON_CreateObject();
  if (rrname) cJSON_AddStringToObject(data, "rrname", rrname);
  if (rrtype) cJSON_AddStringToObject(data, "rrtype", rrtype);
  if (rdata)  cJSON_AddStringToObject(data, "rdata", rdata);
  if (tf && cJSON_IsNumber(tf)) cJSON_AddNumberToObject(data, "time_first", tf->valuedouble);
  if (tl && cJSON_IsNumber(tl)) cJSON_AddNumberToObject(data, "time_last", tl->valuedouble);
  if (cnt && cJSON_IsNumber(cnt)) cJSON_AddNumberToObject(data, "count", cnt->valuedouble);
  cJSON_AddStringToObject(data, "source", "CIRCL Passive DNS");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CIRCL_PDNS");
  cJSON_AddStringToObject(props, "domain", domain);
  if (rrtype) cJSON_AddStringToObject(props, "rrtype", rrtype);
  if (rdata)  cJSON_AddStringToObject(props, "rdata", rdata);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char title[512];
  snprintf(title, sizeof title, "%s %s %s",
           rrname ? rrname : "", rrtype ? rrtype : "", rdata ? rdata : "");

  intel_item it = {0};
  it.remote_key      = NULL;   /* sink derives; rrname+rdata combo not unique enough */
  it.title           = title;
  it.summary         = rdata;
  it.body            = bj;
  it.lang            = "en";
  it.record_type     = "circl-pdns";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"CIRCL_PDNS\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_circl(const source_ctx *ctx, intel_sink *sink,
                     const char *domain) {
  const char *auth = jo_env("CIRCL_AUTH");   /* "user:pass" */
  if (!auth) {
    fprintf(stderr, "[circl_pdns] gated (no CIRCL_AUTH)\n");
    return 0;
  }
  char url[1024];
  snprintf(url, sizeof url, "https://www.circl.lu/pdns/query/%s", domain);

  /* Basic auth: base64(user:pass) */
  static const char b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t al = strlen(auth);
  char enc64[512]; size_t o = 0;
  for (size_t i = 0; i < al && o + 4 < sizeof enc64; i += 3) {
    unsigned v = (unsigned char)auth[i] << 16;
    int rem = (int)(al - i);
    if (rem > 1) v |= (unsigned char)auth[i+1] << 8;
    if (rem > 2) v |= (unsigned char)auth[i+2];
    enc64[o++] = b64[(v >> 18) & 63];
    enc64[o++] = b64[(v >> 12) & 63];
    enc64[o++] = rem > 1 ? b64[(v >> 6) & 63] : '=';
    enc64[o++] = rem > 2 ? b64[v & 63] : '=';
  }
  enc64[o] = 0;

  char hdr[600];
  snprintf(hdr, sizeof hdr, "Authorization: Basic %s", enc64);
  const char *hdrs[] = { hdr, "Accept: application/json", NULL };

  char *body = jo_get(ctx, url, hdrs, "circl_pdns");
  if (!body) return 0;

  /* newline-delimited JSON objects */
  int emitted = 0;
  char *line = body, *nl;
  while (line && *line) {
    nl = strchr(line, '\n');
    if (nl) *nl = 0;
    while (*line == ' ' || *line == '\r') line++;
    if (*line) {
      cJSON *rec = cJSON_Parse(line);
      if (rec) { emitted += pdns_emit(sink, domain, rec); cJSON_Delete(rec); }
    }
    if (!nl) break;
    line = nl + 1;
  }
  free(body);
  fprintf(stderr, "[circl_pdns] emitted %d\n", emitted);
  return 0;
}

/* ---- HACKERTARGET ------------------------------------------------------- */
/* hostsearch returns plaintext CSV lines "hostname,ip". Keyless, daily-limited;
 * on limit the body is an error sentence — we parse only well-formed lines. */
static int run_hackertarget(const source_ctx *ctx, intel_sink *sink,
                            const char *domain, char *enc) {
  char url[512];
  snprintf(url, sizeof url,
    "https://api.hackertarget.com/hostsearch/?q=%s", enc);
  const char *hdrs[] = { "Accept: text/plain", NULL };
  char *body = jo_get(ctx, url, hdrs, "hackertarget");
  if (!body) return 0;

  /* API rate-limit / error responses are a single English sentence, no comma
   * splitting a valid host — reject any line without a comma or with a space. */
  int emitted = 0;
  char *line = body, *nl;
  while (line && *line) {
    nl = strchr(line, '\n');
    if (nl) *nl = 0;
    /* trim trailing CR */
    size_t ll = strlen(line);
    while (ll && (line[ll-1] == '\r' || line[ll-1] == ' ')) line[--ll] = 0;

    char *comma = strchr(line, ',');
    if (comma && ll > 2 && !strchr(line, ' ')) {
      *comma = 0;
      const char *host = line;
      const char *ip   = comma + 1;
      if (*host && *ip) {
        cJSON *data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "hostname", host);
        cJSON_AddStringToObject(data, "ip", ip);
        cJSON_AddStringToObject(data, "source", "HackerTarget");
        char *bj = cJSON_PrintUnformatted(data);
        cJSON_Delete(data);

        cJSON *props = cJSON_CreateObject();
        cJSON_AddStringToObject(props, "service", "HACKERTARGET");
        cJSON_AddStringToObject(props, "domain", domain);
        cJSON_AddStringToObject(props, "hostname", host);
        cJSON_AddStringToObject(props, "ip", ip);
        cJSON_AddBoolToObject(props, "success", 1);
        char *pj = cJSON_PrintUnformatted(props);
        cJSON_Delete(props);

        intel_item it = {0};
        it.remote_key      = host;
        it.title           = host;
        it.summary         = ip;
        it.body            = bj;
        it.lang            = "en";
        it.record_type     = "hackertarget-host";
        it.properties_json = pj;
        it.tags_json       = "[\"osint-search\",\"HACKERTARGET\"]";
        if (sink->emit(sink, &it) >= 0) emitted++;
        free(bj); free(pj);
      }
    }
    if (!nl) break;
    line = nl + 1;
  }
  free(body);
  fprintf(stderr, "[hackertarget] emitted %d\n", emitted);
  return 0;
}

/* ---- dispatch ----------------------------------------------------------- */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *sid = ctx->source_id ? ctx->source_id : "";

  /* CIRCL takes the raw domain in the path (a domain has no chars needing
   * encoding); the query-param services get a URL-encoded copy. */
  if (strcmp(sid, "CIRCL_PDNS") == 0)
    return run_circl(ctx, sink, q);

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int rc;
  if (strcmp(sid, "CERTSPOTTER") == 0)       rc = run_certspotter(ctx, sink, q, enc);
  else if (strcmp(sid, "URLSCAN_SEARCH") == 0) rc = run_urlscan(ctx, sink, q, enc);
  else if (strcmp(sid, "HACKERTARGET") == 0)   rc = run_hackertarget(ctx, sink, q, enc);
  else rc = 0;   /* unknown id → honest empty */
  free(enc);
  return rc;
}

static const source_def certspotter_def = {
  .id = "CERTSPOTTER", .collector = "osint",
  .name = "Cert Spotter Issuances", .name_ja = "Cert Spotter 証明書発行",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://api.certspotter.com/v1/issuances",
  .description = "SSL Mate Cert Spotter — certificate issuances / subdomains for a domain (keyless limited; CERTSPOTTER_TOKEN raises limits)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(certspotter_def)

static const source_def urlscan_search_def = {
  .id = "URLSCAN_SEARCH", .collector = "osint",
  .name = "urlscan.io Domain Search", .name_ja = "urlscan.io ドメイン検索",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://urlscan.io/api/v1/search/",
  .description = "urlscan.io — public scans referencing a domain (URL, IP, country, server); keyless, URLSCAN_API_KEY widens access",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(urlscan_search_def)

static const source_def circl_pdns_def = {
  .id = "CIRCL_PDNS", .collector = "osint",
  .name = "CIRCL Passive DNS", .name_ja = "CIRCL パッシブDNS",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://www.circl.lu/pdns/query",
  .description = "CIRCL Passive DNS — historical DNS resolutions for a domain (needs CIRCL_AUTH user:pass)",
  .layer = NULL, .free_tier = 0,
};
REGISTER_SOURCE(circl_pdns_def)

static const source_def hackertarget_def = {
  .id = "HACKERTARGET", .collector = "osint",
  .name = "HackerTarget Host Search", .name_ja = "HackerTarget ホスト検索",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://api.hackertarget.com/hostsearch/",
  .description = "HackerTarget hostsearch — subdomains + resolved IPs for a domain (keyless, daily-limited)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(hackertarget_def)
