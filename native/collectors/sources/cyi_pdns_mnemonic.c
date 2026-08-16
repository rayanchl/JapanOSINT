/* collectors/sources/cyi_pdns_mnemonic.c
 * OSINT service — PDNS_MNEMONIC. Domain pivot against Mnemonic's open passive
 * DNS tier: real DNS history with first/last-seen timestamps and observation
 * counts. The existing CIRCL pdns source requires credentials; this one answers
 * anonymously.
 * Endpoint: https://api.mnemonic.no/pdns/v3/<domain>                  (keyless)
 * parse_notes: "Rows in data[]; count is the true total while limit caps the
 * page (default 25) ... Timestamps are epoch MILLISECONDS. Reverse lookups work
 * with an IP in place of the domain." The true total is carried on every row,
 * epoch-ms values are converted with a guarded range check, and one row is
 * emitted per passive-DNS answer.
 * Emits: query, answer, rrtype, times, firstSeenTimestamp, lastUpdatedTimestamp,
 * minTtl, tlp. No coordinates -> has_geo 0 (R2).
 * Licence: Mnemonic open passive DNS tier, TLP:WHITE records only; the
 * anonymous quota is limited (results capped and rate-limited).
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

#define MAX_ROWS 50

static int looks_like_domain(const char *s) {
  if (!s || !*s) return 0;
  if (strchr(s, '@') || strchr(s, '/') || strchr(s, ' ') || strchr(s, ':')) return 0;
  size_t n = strlen(s);
  if (n < 3 || n > 253) return 0;
  const char *dot = strrchr(s, '.');
  if (!dot || dot == s || !dot[1]) return 0;
  for (const char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (!(isalnum(c) || c == '-' || c == '.' || c == '_' || c >= 0x80)) return 0;
  }
  if ((unsigned char)dot[1] < 0x80 && !isalpha((unsigned char)dot[1])) return 0;
  return 1;
}

/* epoch milliseconds -> ISO-8601 UTC; NULL when the value is not plausible. */
static const char *ms_to_iso(const cJSON *o, const char *k, char *out, size_t n) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!cJSON_IsNumber(v)) return NULL;
  double ms = v->valuedouble;
  if (!(ms > 946684800000.0 && ms < 4102444800000.0)) return NULL;
  time_t t = (time_t)(ms / 1000.0);
  struct tm tmv;
#if defined(_WIN32)
  if (gmtime_s(&tmv, &t) != 0) return NULL;
#else
  if (!gmtime_r(&t, &tmv)) return NULL;
#endif
  if (strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tmv) == 0) return NULL;
  return out;
}

static char *http_get(const source_ctx *ctx, const char *url, long *status) {
  http_response hr = {0};
  const char *hdrs[] = { "Accept: application/json", NULL };
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 20000, 1, &hr);
  *status = hr.status;
  if (rc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  char *b = hr.body; hr.body = NULL; http_response_free(&hr);
  return b;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_domain(q)) return 0;             /* wrong shape -> no-op */

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[512];
  snprintf(url, sizeof url, "https://api.mnemonic.no/pdns/v3/%s", enc);
  free(enc);

  long status = 0;
  char *body = http_get(ctx, url, &status);
  if (!body) {
    fprintf(stderr, "[PDNS_MNEMONIC] http status=%ld\n", status);
    if (status >= 400 && status < 500) return 0;   /* no history / quota */
    return -1;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[PDNS_MNEMONIC] unparseable body\n"); return -1; }

  const cJSON *cnt = cJSON_GetObjectItem(root, "count");
  double total = cJSON_IsNumber(cnt) ? cnt->valuedouble : 0;

  int n = 0;
  const cJSON *d;
  cJSON_ArrayForEach(d, cJSON_GetObjectItem(root, "data")) {
    if (n >= MAX_ROWS) break;
    const char *answer = jo_sv(d, "answer");
    if (!answer) continue;
    const char *query = jo_sv(d, "query");
    const char *rrtype = jo_sv(d, "rrtype");
    char firstbuf[40], lastbuf[40];
    const char *first = ms_to_iso(d, "firstSeenTimestamp", firstbuf, sizeof firstbuf);
    const char *last  = ms_to_iso(d, "lastUpdatedTimestamp", lastbuf, sizeof lastbuf);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "service", "PDNS_MNEMONIC");
    cJSON_AddStringToObject(p, "query", query ? query : q);
    cJSON_AddStringToObject(p, "answer", answer);
    if (rrtype) cJSON_AddStringToObject(p, "rrtype", rrtype);
    const cJSON *times = cJSON_GetObjectItem(d, "times");
    if (cJSON_IsNumber(times)) cJSON_AddNumberToObject(p, "observations", times->valuedouble);
    if (first) cJSON_AddStringToObject(p, "first_seen", first);
    if (last)  cJSON_AddStringToObject(p, "last_seen", last);
    const cJSON *ttl = cJSON_GetObjectItem(d, "minTtl");
    if (cJSON_IsNumber(ttl)) cJSON_AddNumberToObject(p, "min_ttl", ttl->valuedouble);
    const char *tlp = jo_sv(d, "tlp");
    if (tlp) cJSON_AddStringToObject(p, "tlp", tlp);
    if (total > 0) cJSON_AddNumberToObject(p, "total_known_answers", total);
    cJSON_AddBoolToObject(p, "success", 1);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[320];
    snprintf(title, sizeof title, "%s %s %s", query ? query : q,
             rrtype ? rrtype : "->", answer);
    char summary[288];
    snprintf(summary, sizeof summary, "passive DNS%s%s%s%s",
             first ? " · first seen " : "", first ? first : "",
             last ? " · last seen " : "", last ? last : "");
    char key[400];
    snprintf(key, sizeof key, "%s|%s|%s", query ? query : q,
             rrtype ? rrtype : "-", answer);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = url;
    it.lang            = "en";
    it.published_at    = last;
    it.record_type     = "passive-dns";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"cyber\",\"passive-dns\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(root);
  fprintf(stderr, "[PDNS_MNEMONIC] emitted %d of %.0f known answers (%s)\n", n, total, q);
  return 0;                       /* no passive DNS history is not an error */
}

static const source_def cyi_pdns_mnemonic_def = {
  .id = "PDNS_MNEMONIC", .collector = "osint",
  .name = "Mnemonic passive DNS (open tier)",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://api.mnemonic.no/pdns/v3/",
  .description = "Keyless passive DNS with first/last-seen timestamps and observation counts — genuine DNS history without credentials.",
  .license = "Mnemonic open passive DNS tier, TLP:WHITE records only; anonymous quota is limited.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_pdns_mnemonic_def)
