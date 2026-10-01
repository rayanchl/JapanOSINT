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
#include "cyi_common.inc"

/* The endpoint's own page size, not an editorial bound: `limit` caps a page
 * and `count` is the true total (1,000 for google.com on the open tier). This
 * collector used to send no limit at all — so it got the server default of 25
 * — and then capped itself at 50 rows on top, which meant 25 of 1,000 answers
 * reached the sink and the other 975 were never even requested. Now the walk
 * follows offset/limit to `count`. */
#define PDNS_PAGE_SIZE 100
#define PDNS_MAX_PAGES 50   /* exhaustive-ok: offset-walk runaway guard; an early stop emits a collector-truncation-notice */

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

/* One passive-DNS answer. `base` is the unpaged endpoint, used as the row's
 * link so a row does not point at whichever offset happened to carry it.
 * Returns 1 when the sink took it. */
static int pdns_emit(intel_sink *sink, const cJSON *d, const char *q,
                     const char *base, double total) {
    const char *answer = jo_sv(d, "answer");
    if (!answer) return 0;
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
    it.link            = base;
    it.lang            = "en";
    it.published_at    = last;
    it.record_type     = "passive-dns";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"cyber\",\"passive-dns\"]";
    int rc = sink->emit(sink, &it);
    free(pj);
    return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_domain(q)) return 0;             /* wrong shape -> no-op */

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char base[512];
  snprintf(base, sizeof base, "https://api.mnemonic.no/pdns/v3/%s", enc);
  free(enc);

  int max_pages = PDNS_MAX_PAGES;
  const char *penv = getenv("JO_PDNS_MAX_PAGES");
  if (penv && *penv) { int v = atoi(penv); if (v > 0) max_pages = v; }

  int n = 0, offset = 0, pages = 0, stopped_early = 0;
  double total = 0;
  char url[576];

  for (int page = 0; page < max_pages; page++) {
    snprintf(url, sizeof url, "%s?limit=%d&offset=%d", base,
             PDNS_PAGE_SIZE, offset);
    long status = 0;
    char *body = cyi_get_json(ctx, url, 20000, &status);
    if (!body) {
      fprintf(stderr, "[PDNS_MNEMONIC] http status=%ld at offset %d\n",
              status, offset);
      /* 4xx on the FIRST page is "no history / over quota", not an error. */
      if (page == 0) return (status >= 400 && status < 500) ? 0 : -1;
      /* Mid-walk it is a real shortfall — the anonymous tier rate-limits, and
       * a run that quietly returned the first 300 of 1,000 answers would look
       * exactly like a complete one. */
      stopped_early = 1;
      break;
    }
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
      fprintf(stderr, "[PDNS_MNEMONIC] unparseable body at offset %d\n", offset);
      if (page == 0) return -1;
      stopped_early = 1;
      break;
    }
    pages++;
    const cJSON *cnt = cJSON_GetObjectItem(root, "count");
    if (cJSON_IsNumber(cnt)) total = cnt->valuedouble;

    int here = 0;
    const cJSON *d;
    cJSON_ArrayForEach(d, cJSON_GetObjectItem(root, "data")) {
      here++;
      n += pdns_emit(sink, d, q, base, total);
    }
    cJSON_Delete(root);

    offset += here;
    if (here < PDNS_PAGE_SIZE) break;        /* short page = end of the set */
    if (total > 0 && offset >= total) break; /* reached the declared total  */
    if (page + 1 == max_pages) stopped_early = 1;
  }

  fprintf(stderr, "[PDNS_MNEMONIC] emitted %d of %.0f known answers over %d "
                  "page(s) (%s)\n", n, total, pages, q);
  if (stopped_early)
    jo_trunc_notice(sink, "PDNS_MNEMONIC", base, n,
                    total > 0 ? (long)total : -1,
                    "the offset walk stopped before Mnemonic's declared answer "
                    "count was reached — the anonymous tier rate-limited the "
                    "run, or the page-walk ceiling was hit",
                    "re-run the pivot, or raise $JO_PDNS_MAX_PAGES");
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
