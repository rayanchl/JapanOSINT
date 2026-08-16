/* collectors/sources/cyi_apnic_delegated_stats.c
 * APNIC delegated extended statistics — the authoritative record of which
 * country and which member holds every APNIC IPv4/IPv6 block and ASN.
 * Endpoint: https://ftp.apnic.net/stats/apnic/delegated-apnic-extended-latest
 * Keyless, ~9 MB, pipe-delimited.
 * parse_notes: "9 MB per fetch: stream it, do not hold two copies" — we take
 * the body straight off http_request (no feed_get_text strdup) and parse it in
 * place, one pass, no per-line allocation. "Lines starting with # are comments,
 * the 2.x version line is not a record"; summary lines (cc == "*") are skipped
 * too. Only delegations DATED IN THE LAST 90 DAYS are emitted — the whole file
 * is 100k+ static rows and the new allocations are the signal.
 * Emits: registry, cc, type (ipv4|ipv6|asn), start, value/count, date, status,
 * opaque-id. No coordinates -> has_geo 0 (R2).
 * Licence: the file carries its own "CONDITIONS OF USE" header block — APNIC
 * publishes it for public use, and redistribution should keep that notice.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CYI_URL "https://ftp.apnic.net/stats/apnic/delegated-apnic-extended-latest"
#define WINDOW_DAYS 90
#define MAX_ROWS 5000

/* In-place line splitter; returns the next line (trimmed of CR), advances *p. */
static char *next_line(char **p) {
  char *s = *p;
  if (!s || !*s) return NULL;
  char *e = strchr(s, '\n');
  if (e) { *e = '\0'; *p = e + 1; } else { *p = s + strlen(s); }
  size_t n = strlen(s);
  while (n && (s[n - 1] == '\r' || s[n - 1] == ' ')) s[--n] = '\0';
  return s;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", CYI_URL, NULL, NULL, 0, 60000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[apnic-delegated-stats] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return -1;
  }
  char *body = hr.body;             /* single copy, parsed in place */
  hr.body = NULL;
  http_response_free(&hr);

  /* cutoff as YYYYMMDD, the format of the date column */
  time_t cut = time(NULL) - (time_t)WINDOW_DAYS * 24 * 3600;
  struct tm tmv;
  char cutoff[16] = "00000000";
#if defined(_WIN32)
  if (gmtime_s(&tmv, &cut) == 0) strftime(cutoff, sizeof cutoff, "%Y%m%d", &tmv);
#else
  if (gmtime_r(&cut, &tmv)) strftime(cutoff, sizeof cutoff, "%Y%m%d", &tmv);
#endif

  int n = 0, seen = 0;
  char *cur = body, *line;
  while ((line = next_line(&cur)) != NULL) {
    if (!line[0] || line[0] == '#') continue;

    /* registry|cc|type|start|value|date|status|opaque-id */
    char *f[8] = {0};
    int nf = 0;
    char *tok = line;
    while (nf < 8) {
      f[nf++] = tok;
      char *bar = strchr(tok, '|');
      if (!bar) break;
      *bar = '\0';
      tok = bar + 1;
    }
    if (nf < 6) continue;
    const char *registry = f[0], *cc = f[1], *type = f[2];
    const char *start = f[3], *value = f[4], *date = f[5];
    const char *status = nf > 6 ? f[6] : NULL;
    const char *opaque = nf > 7 ? f[7] : NULL;

    /* the "2|apnic|..." version line and the "*|apnic|ipv4|*|..." summary
     * lines are not delegations */
    if (!cc[0] || cc[0] == '*' || !start[0] || start[0] == '*') continue;
    if (strcmp(type, "ipv4") && strcmp(type, "ipv6") && strcmp(type, "asn")) continue;
    seen++;
    if (strlen(date) != 8 || strcmp(date, cutoff) < 0) continue;
    if (n >= MAX_ROWS) continue;

    char iso[16];
    snprintf(iso, sizeof iso, "%.4s-%.2s-%.2s", date, date + 4, date + 6);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "registry", registry);
    cJSON_AddStringToObject(p, "country", cc);
    cJSON_AddStringToObject(p, "resource_type", type);
    cJSON_AddStringToObject(p, "start", start);
    cJSON_AddStringToObject(p, "count", value);
    cJSON_AddStringToObject(p, "delegated_date", iso);
    if (status && status[0]) cJSON_AddStringToObject(p, "status", status);
    if (opaque && opaque[0]) cJSON_AddStringToObject(p, "opaque_id", opaque);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[192];
    if (strcmp(type, "asn") == 0)
      snprintf(title, sizeof title, "AS%s delegated to %s", start, cc);
    else
      snprintf(title, sizeof title, "%s %s (%s addresses) delegated to %s",
               type, start, value, cc);
    char summary[192];
    snprintf(summary, sizeof summary, "%s · %s · %s",
             registry, iso, (status && status[0]) ? status : "delegated");
    char key[128];
    snprintf(key, sizeof key, "%s|%s|%s", type, start, value);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = CYI_URL;
    it.lang            = "en";
    it.published_at    = iso;
    it.record_type     = "rir-delegation";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"rir\",\"allocation\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[apnic-delegated-stats] emitted %d of %d delegations (>= %s)\n",
          n, seen, cutoff);
  return 0;                        /* a quiet 90 days is not an error (R3) */
}

static const source_def cyi_apnic_delegated_stats_def = {
  .id = "apnic-delegated-stats", .collector = "cyber",
  .name = "APNIC delegated extended statistics (RIR allocations)",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "Ground truth for which country and which member holds every APNIC IPv4/IPv6 block and ASN, with the allocation date; recent delegations are emitted as rows.",
  .license = "APNIC delegated stats — the file's own CONDITIONS OF USE header applies; public data, attribute APNIC.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_apnic_delegated_stats_def)
