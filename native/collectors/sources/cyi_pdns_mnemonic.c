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
 *
 * QUOTA (HTTP 402). When the anonymous quota is spent Mnemonic answers
 * `402 {"messages":[{"message":"Resource limit exceeded"}],
 * "metaData":{"millisUntilResourcesAvailable":N}}` — measured 2026-10-06 on
 * google.com: pages at offset 0-800 answer 200, the tenth request 402s. This
 * collector used to read a 402 on the first page as "no history" and return 0
 * with nothing emitted, which is indistinguishable from a domain Mnemonic has
 * never seen. A 402 (or 429) is now a `collector-status-notice` saying the
 * quota refused the request, with the upstream's own message and wait time; a
 * 402 part-way through a walk is named as such in the truncation notice. An
 * Argus API key in MNEMONIC_API_KEY is sent as the `Argus-API-Key` header,
 * which is how Mnemonic authenticates callers above the anonymous tier.
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
 * and `count` is the total the server will serve. This collector used to send
 * no limit at all — so it got the server default of 25 — and then capped
 * itself at 50 rows on top, which meant 25 of 1,000 answers reached the sink
 * and the other 975 were never even requested. Now the walk follows
 * offset/limit to `count`.
 *
 * WHY THE PAGE IS 1,000 AND NOT 100. The answers come back ordered by
 * lastUpdatedTimestamp, newest first, and Mnemonic updates that timestamp
 * live as its sensors keep observing a name. An offset walk over a list that
 * re-sorts itself between requests serves the record at a page boundary twice
 * and never serves the one that slid past it. Measured 2026-10-07 on
 * google.com: ten pages of 100 emitted 1,000 and stored 986
 * (`UID-COLLISION: 14 of 1000`) — and the 14 missing answers are exactly the
 * 14 that one limit=1000 request returns and the walk never saw; that single
 * request has 1,000 distinct query|rrtype|answer keys and no duplicate. So
 * the collisions were not records sharing a key, nor byte-identical copies;
 * they were the walk re-serving records while losing others. 1,000 is the
 * largest page the open tier accepts (`limit=5000` answers HTTP 412 "Maximum
 * result limit for public usage is 1000"), so an anonymous pivot is now ONE
 * request with no page boundary at all — and one request, not ten, against a
 * quota that refuses the tenth. A keyed caller whose set exceeds 1,000 still
 * pages, with a tenth of the boundaries. */
#define PDNS_PAGE_SIZE 1000
/* The open tier's ceiling on a whole result set, from the 412 above: `count`
 * never exceeds it without a key, so count == 1,000 on an anonymous pivot is
 * the ceiling, not a measurement of the name's history. */
#define PDNS_PUBLIC_CAP 1000
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

/* The upstream's refusal, read from a 402/429 body: its message and how long
 * it says to wait. Either may be absent. */
static void pdns_refusal(const char *body, char *msg, size_t mcap, double *wait_ms) {
  msg[0] = 0; *wait_ms = -1;
  cJSON *r = body ? cJSON_Parse(body) : NULL;
  if (!r) return;
  const cJSON *m = cJSON_GetArrayItem(cJSON_GetObjectItem(r, "messages"), 0);  /* exhaustive-ok: the refusal carries one message */
  const char *t = jo_sv(m, "message");
  if (t) snprintf(msg, mcap, "%s", t);
  const cJSON *w = cJSON_GetObjectItem(cJSON_GetObjectItem(r, "metaData"),
                                       "millisUntilResourcesAvailable");
  if (cJSON_IsNumber(w)) *wait_ms = w->valuedouble;
  cJSON_Delete(r);
}

/* The quota said no before anything was collected: say so as data, the way a
 * gated source says it has no credential (_credential_notice.inc) — but this
 * request WAS spent and refused, so the wording and status differ. Scoped by
 * entity so one domain's refusal does not overwrite another's. */
static void pdns_quota_notice(intel_sink *sink, const char *q, const char *url,
                              long status, const char *msg, double wait_ms,
                              int keyed) {
  const char *st = status == 402 ? "payment_required" : "rate_limited";
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "status", st);
  cJSON_AddStringToObject(p, "source_id", "PDNS_MNEMONIC");
  cJSON_AddStringToObject(p, "upstream", "Mnemonic passive DNS");
  cJSON_AddStringToObject(p, "entity", q);
  cJSON_AddNumberToObject(p, "http_status", (double)status);
  if (msg && *msg) cJSON_AddStringToObject(p, "upstream_message", msg);
  if (wait_ms >= 0) cJSON_AddNumberToObject(p, "millis_until_resources_available", wait_ms);
  cJSON *envs = cJSON_CreateArray();
  cJSON_AddItemToArray(envs, cJSON_CreateString("MNEMONIC_API_KEY"));
  cJSON_AddItemToObject(p, "env_vars_accepted", envs);
  cJSON_AddBoolToObject(p, "api_key_sent", keyed);
  cJSON_AddStringToObject(p, "endpoint", url);
  cJSON_AddBoolToObject(p, "request_spent", 1);
  cJSON_AddNumberToObject(p, "records_used", 0);
  cJSON_AddStringToObject(p, "records_available",
                          "unknown — the upstream refused the first page");
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char key[320], title[320], summary[640], waitbuf[96] = "";
  if (wait_ms >= 0)
    snprintf(waitbuf, sizeof waitbuf, " Mnemonic says resources free up again "
             "in %.0f s.", wait_ms / 1000.0);
  snprintf(key, sizeof key, "collector-%s:%s",
           status == 402 ? "payment-required" : "rate-limited", q);
  snprintf(title, sizeof title, "Mnemonic passive DNS — %s refused (HTTP %ld), "
           "nothing collected for %s",
           status == 402 ? "quota" : "rate limit", status, q);
  snprintf(summary, sizeof summary,
           "Mnemonic answered HTTP %ld%s%s%s to the first page, so no passive-DNS "
           "record exists for %s from this run — this is a refusal, not an empty "
           "history.%s Set MNEMONIC_API_KEY (sent as Argus-API-Key) to query "
           "above the anonymous tier, or re-run after the wait.",
           status, msg && *msg ? " \"" : "", msg && *msg ? msg : "",
           msg && *msg ? "\"" : "", q, waitbuf);
  fprintf(stderr, "[PDNS_MNEMONIC] %s: HTTP %ld %s (%s)\n", st, status,
          msg && *msg ? msg : "", q);
  intel_item note = {0};
  note.remote_key      = key;
  note.title           = title;
  note.summary         = summary;
  note.link            = url;
  note.lang            = "en";
  note.record_type     = "collector-status-notice";
  note.sub_source_id   = "PDNS_MNEMONIC";
  note.properties_json = pj ? pj : "{}";
  note.tags_json       = "[\"collector-status\",\"payment-required\"]";
  sink->emit(sink, &note);
  free(pj);
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

  /* Optional Argus API key: lifts the anonymous quota. Never logged. */
  const char *key = getenv("MNEMONIC_API_KEY");
  int keyed = key && *key;
  char keyhdr[320];
  const char *hdrs[3] = { "Accept: application/json", NULL, NULL };
  if (keyed) {
    snprintf(keyhdr, sizeof keyhdr, "Argus-API-Key: %s", key);
    hdrs[1] = keyhdr;
  }

  int n = 0, offset = 0, pages = 0, stopped_early = 0;
  long stop_status = 0;
  char stop_msg[160] = "";
  double stop_wait = -1;
  double total = 0;
  char url[576];

  for (int page = 0; page < max_pages; page++) {
    snprintf(url, sizeof url, "%s?limit=%d&offset=%d", base,
             PDNS_PAGE_SIZE, offset);
    http_response hr = {0};
    int hrc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 20000, 1, &hr);
    long status = hr.status;
    char *body = NULL;
    if (hrc == 0 && status == 200 && hr.body) { body = hr.body; hr.body = NULL; }
    if (!body) {
      fprintf(stderr, "[PDNS_MNEMONIC] http status=%ld at offset %d\n",
              status, offset);
      if (status == 402 || status == 429) {
        /* The quota refused the request: a refusal, never "no history". */
        pdns_refusal(hr.body, stop_msg, sizeof stop_msg, &stop_wait);
        http_response_free(&hr);
        if (page == 0) {
          pdns_quota_notice(sink, q, base, status, stop_msg, stop_wait, keyed);
          return 0;
        }
        stop_status = status;
        stopped_early = 1;
        break;
      }
      http_response_free(&hr);
      /* Any other 4xx on the FIRST page is "no such object" (R3), not an
       * error; a transport failure or 5xx is. */
      if (page == 0) return (status >= 400 && status < 500) ? 0 : -1;
      /* Mid-walk it is a real shortfall — a run that quietly returned the
       * first 300 of 1,000 answers would look exactly like a complete one. */
      stop_status = status;
      stopped_early = 1;
      break;
    }
    http_response_free(&hr);
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
  /* An anonymous pivot whose count reached the open tier's ceiling has been
   * served everything Mnemonic will serve WITHOUT a key, which is not the
   * same as everything it holds. Say so in-band (rule 2) — the true total is
   * unknown, so it is not invented. */
  if (!stopped_early && !keyed && total >= PDNS_PUBLIC_CAP) {
    char reason[320];
    snprintf(reason, sizeof reason,
             "Mnemonic's open tier serves at most %d results per query (HTTP 412 "
             "\"Maximum result limit for public usage is %d\"), and %s reached "
             "it, so this name may have more passive-DNS answers than were "
             "served", PDNS_PUBLIC_CAP, PDNS_PUBLIC_CAP, q);
    jo_trunc_notice_scoped(sink, "PDNS_MNEMONIC", q, base, n, -1, reason,
                           "set MNEMONIC_API_KEY (sent as Argus-API-Key) to query "
                           "above the anonymous tier");
  }
  if (stopped_early) {
    char reason[400];
    if (stop_status == 402 || stop_status == 429)
      snprintf(reason, sizeof reason,
               "Mnemonic refused the page at offset %d with HTTP %ld%s%s%s — the "
               "%s quota ran out before its declared answer count was reached",
               offset, stop_status, stop_msg[0] ? " \"" : "", stop_msg,
               stop_msg[0] ? "\"" : "", keyed ? "keyed" : "anonymous");
    else if (stop_status)
      snprintf(reason, sizeof reason,
               "the page at offset %d failed (HTTP %ld) before Mnemonic's "
               "declared answer count was reached", offset, stop_status);
    else
      snprintf(reason, sizeof reason,
               "the offset walk stopped at offset %d before Mnemonic's declared "
               "answer count was reached — an unparseable page or the "
               "page-walk ceiling", offset);
    jo_trunc_notice_scoped(sink, "PDNS_MNEMONIC", q, base, n,
                    total > 0 ? (long)total : -1, reason,
                    (stop_status == 402 || stop_status == 429)
                      ? "set MNEMONIC_API_KEY (sent as Argus-API-Key), or re-run "
                        "the pivot after the quota window"
                      : "re-run the pivot, or raise $JO_PDNS_MAX_PAGES");
  }
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
