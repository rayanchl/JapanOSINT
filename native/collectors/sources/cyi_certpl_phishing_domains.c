/* collectors/sources/cyi_certpl_phishing_domains.c
 * CERT Polska (NASK) phishing warning list — the Polish national CERT's
 * legally-mandated phishing domain register, with insert and removal dates.
 * Endpoint: https://hole.cert.pl/domains/v2/domains.csv               (keyless)
 * TRAPS (parse_notes): the file is "TAB-separated despite the .csv extension"
 * and "the header row is Polish" (PozycjaRejestru / AdresDomeny / DataWpisu /
 * DataWykreslenia); the JSON variant is 24 MB so the CSV is preferred. "Rows
 * with an empty DataWykreslenia are currently active; sort by DataWpisu and
 * take only new entries per run" — so only entries still active AND registered
 * within the last WINDOW_DAYS are emitted, which keeps a 200k-row register
 * down to the new listings that are the actual signal.
 * Emits: register position, domain, insert date, removal date (when present).
 * No coordinates -> has_geo 0 (R2).
 * Licence: public register operated by CERT Polska/NASK under Polish law;
 * free to consume.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CYI_URL "https://hole.cert.pl/domains/v2/domains.csv"
#define WINDOW_DAYS 7
#define MAX_ROWS 5000

static char *next_line(char **p) {
  char *s = *p;
  if (!s || !*s) return NULL;
  char *e = strchr(s, '\n');
  if (e) { *e = '\0'; *p = e + 1; } else { *p = s + strlen(s); }
  size_t n = strlen(s);
  while (n && (s[n - 1] == '\r')) s[--n] = '\0';
  return s;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", CYI_URL, NULL, NULL, 0, 60000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[certpl-phishing-domains] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return -1;
  }
  char *body = hr.body;                 /* single copy, parsed in place */
  hr.body = NULL;
  http_response_free(&hr);

  time_t cut = time(NULL) - (time_t)WINDOW_DAYS * 24 * 3600;
  struct tm tmv;
  char cutoff[16] = "0000-00-00";
#if defined(_WIN32)
  if (gmtime_s(&tmv, &cut) == 0) strftime(cutoff, sizeof cutoff, "%Y-%m-%d", &tmv);
#else
  if (gmtime_r(&cut, &tmv)) strftime(cutoff, sizeof cutoff, "%Y-%m-%d", &tmv);
#endif

  int n = 0, active = 0, first = 1;
  char *cur = body, *line;
  while ((line = next_line(&cur)) != NULL) {
    if (!line[0]) continue;
    if (first) { first = 0; if (strstr(line, "AdresDomeny")) continue; }

    /* TAB-separated: PozycjaRejestru \t AdresDomeny \t DataWpisu \t DataWykreslenia */
    char *f[4] = {0};
    int nf = 0;
    char *tok = line;
    while (nf < 4) {
      f[nf++] = tok;
      char *t = strchr(tok, '\t');
      if (!t) break;
      *t = '\0';
      tok = t + 1;
    }
    if (nf < 3) continue;
    const char *pos = f[0], *domain = f[1], *added = f[2];
    const char *removed = (nf > 3 && f[3] && f[3][0]) ? f[3] : NULL;
    if (!domain[0] || !strchr(domain, '.')) continue;
    if (removed) continue;                      /* delisted -> not active */
    active++;
    if (strncmp(added, cutoff, 10) < 0) continue;   /* older than the window */
    if (n >= MAX_ROWS) continue;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "domain", domain);
    if (pos[0]) cJSON_AddStringToObject(p, "register_position", pos);
    cJSON_AddStringToObject(p, "listed_at", added);
    cJSON_AddBoolToObject(p, "active", 1);
    cJSON_AddStringToObject(p, "register", "CERT Polska warning list");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char summary[192];
    snprintf(summary, sizeof summary, "CERT Polska phishing register — listed %s", added);

    intel_item it = {0};
    it.remote_key      = domain;
    it.title           = domain;
    it.summary         = summary;
    it.link            = "https://hole.cert.pl/domains/";
    it.lang            = "pl";
    it.published_at    = added;
    it.record_type     = "phishing-domain";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"phishing\",\"cert-pl\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[certpl-phishing-domains] emitted %d new of %d active entries (>= %s)\n",
          n, active, cutoff);
  return 0;                        /* a week with no new listings is fine */
}

static const source_def cyi_certpl_phishing_domains_def = {
  .id = "certpl-phishing-domains", .collector = "cyber",
  .name = "CERT Polska phishing warning list",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "The Polish national CERT's legally-mandated phishing domain register with dated provenance; newly listed active domains are emitted as rows.",
  .license = "Public register operated by CERT Polska/NASK under Polish law; free to consume.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_certpl_phishing_domains_def)
