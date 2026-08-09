/* collectors/sources/cyi_apnic_as_population.c
 * APNIC Labs — estimated internet-user population behind each AS, per country.
 * Endpoint: https://stats.labs.apnic.net/cgi-bin/aspop?c=<CC>&f=j    (keyless)
 * parse_notes: the body is a JSON object with a Data[] array served as
 * application/json with an ISO-8859-1 charset (we parse regardless of MIME);
 * the c= parameter takes any ISO country code, so this is a per-country loop.
 * Emits per AS: AS, AS-Name, CC, Users, percent-of-cc, plus the Date/Window
 * header values — all upstream fields. No coordinates -> has_geo 0 (R2).
 * Licence: (C) APNIC Pty/Ltd — "re-use with attribution permitted" (stated in
 * the payload).
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Countries pulled each run. The endpoint is per-country; this bounded set
 * keeps one run to ~10 fetches. */
static const char *CCS[] = { "JP","US","CN","IN","BR","DE","GB","FR","RU","KR" };
#define NCC ((int)(sizeof(CCS)/sizeof(CCS[0])))
#define PER_CC 25            /* top-N ASNs per country by user estimate */

static int emit_cc(const source_ctx *ctx, intel_sink *sink, const char *cc) {
  char url[160];
  snprintf(url, sizeof url, "https://stats.labs.apnic.net/cgi-bin/aspop?c=%s&f=j", cc);
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) return -1;

  const char *date = jo_sv(doc, "Date");
  const char *window = jo_sv(doc, "Window");
  const cJSON *data = cJSON_GetObjectItem(doc, "Data");

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, data) {
    if (n >= PER_CC) break;                /* bounded: top of the list */
    const char *as = jo_sv(r, "AS");
    double asnum = 0;
    char asbuf[24] = "";
    if (!as && jo_numf(r, "AS", &asnum)) { snprintf(asbuf, sizeof asbuf, "%.0f", asnum); as = asbuf; }
    if (!as) continue;
    const char *name = jo_sv(r, "AS-Name");
    double users = 0, pct = 0;
    int hu = jo_numf(r, "Users", &users), hp = jo_numf(r, "percent-of-cc", &pct);
    const char *rcc = jo_sv(r, "CC");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "asn", as);
    if (name) cJSON_AddStringToObject(p, "as_name", name);
    cJSON_AddStringToObject(p, "country", rcc ? rcc : cc);
    if (hu) cJSON_AddNumberToObject(p, "estimated_users", users);
    if (hp) cJSON_AddNumberToObject(p, "percent_of_country", pct);
    if (date)   cJSON_AddStringToObject(p, "estimate_date", date);
    if (window) cJSON_AddStringToObject(p, "estimate_window", window);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[256];
    snprintf(title, sizeof title, "AS%s %s (%s)", as, name ? name : "", rcc ? rcc : cc);
    char summary[192];
    snprintf(summary, sizeof summary, "%.0f estimated users · %.2f%% of %s",
             users, pct, rcc ? rcc : cc);
    char key[64];
    snprintf(key, sizeof key, "%s|%s", rcc ? rcc : cc, as);
    char link[160];
    snprintf(link, sizeof link, "https://stats.labs.apnic.net/aspop/AS%s", as);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "as-user-population";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"asn\",\"apnic-labs\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, ok = 0;
  for (int i = 0; i < NCC; i++) {
    int r = emit_cc(ctx, sink, CCS[i]);
    if (r < 0) { fprintf(stderr, "[apnic-as-population] fetch failed for %s\n", CCS[i]); continue; }
    ok++; total += r;
  }
  fprintf(stderr, "[apnic-as-population] emitted %d over %d/%d countries\n", total, ok, NCC);
  if (ok == 0) return -1;
  return 0;
}

static const source_def cyi_apnic_as_population_def = {
  .id = "apnic-as-population", .collector = "cyber",
  .name = "APNIC Labs — estimated user population per AS",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://stats.labs.apnic.net/cgi-bin/aspop",
  .description = "APNIC's advertising-based estimate of how many internet users sit behind each AS in a country — the denominator that turns a routing outage into people affected.",
  .license = "(C) APNIC Pty/Ltd — re-use with attribution permitted (stated in the payload).",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_apnic_as_population_def)
