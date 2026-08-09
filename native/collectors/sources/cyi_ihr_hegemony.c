/* collectors/sources/cyi_ihr_hegemony.c
 * IIJ Internet Health Report — AS hegemony (transit dependency).
 * Endpoint: https://ihr.iijlab.net/ihr/api/hegemony/?originasn=<asn>&af=4 (keyless)
 * For each watched origin AS the API returns the upstreams the rest of the
 * internet must cross to reach it, weighted 0..1 (`hege`).
 * Emits: timebin, originasn, dependency asn + asn_name, hege, af — upstream
 * values only. No coordinates -> has_geo 0 (R2).
 * TRAPS handled (parse_notes): the API "rejects unknown query params with HTTP
 * 400 — do NOT append &format=json", so nothing beyond originasn/af is sent;
 * and because bins are 15 minutes wide we keep only the rows carrying the
 * newest timebin in the response instead of re-emitting history every run.
 * Licence: IIJ Research Lab public API; attribution to Internet Health Report.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Origin ASNs watched each run: major transit, cloud and eyeball networks. */
static const char *WATCH[] = {
  "13335",  /* Cloudflare */
  "15169",  /* Google */
  "16509",  /* Amazon */
  "2914",   /* NTT Global */
  "2497",   /* IIJ */
};
#define NWATCH ((int)(sizeof(WATCH)/sizeof(WATCH[0])))
#define MIN_HEGE 0.01     /* below this the dependency is noise */

static int emit_for(const source_ctx *ctx, intel_sink *sink, const char *origin) {
  char url[256];
  snprintf(url, sizeof url,
           "https://ihr.iijlab.net/ihr/api/hegemony/?originasn=%s&af=4", origin);
  cJSON *doc = feed_get_json(ctx->http, url, 25000);
  if (!doc) return -1;

  const cJSON *results = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(results)) { cJSON_Delete(doc); return 0; }

  /* newest timebin present */
  const char *newest = NULL;
  const cJSON *r;
  cJSON_ArrayForEach(r, results) {
    const char *tb = jo_sv(r, "timebin");
    if (tb && (!newest || strcmp(tb, newest) > 0)) newest = tb;
  }
  if (!newest) { cJSON_Delete(doc); return 0; }

  int n = 0;
  cJSON_ArrayForEach(r, results) {
    const char *tb = jo_sv(r, "timebin");
    if (!tb || strcmp(tb, newest) != 0) continue;
    const cJSON *hg = cJSON_GetObjectItem(r, "hege");
    if (!cJSON_IsNumber(hg) || hg->valuedouble < MIN_HEGE) continue;

    char dep[32] = "";
    const cJSON *da = cJSON_GetObjectItem(r, "asn");
    if (cJSON_IsNumber(da)) snprintf(dep, sizeof dep, "%.0f", da->valuedouble);
    else { const char *s = jo_sv(r, "asn"); if (s) snprintf(dep, sizeof dep, "%s", s); }
    if (!dep[0]) continue;
    const char *dname = jo_sv(r, "asn_name");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "origin_asn", origin);
    cJSON_AddStringToObject(p, "dependency_asn", dep);
    if (dname) cJSON_AddStringToObject(p, "dependency_asn_name", dname);
    cJSON_AddNumberToObject(p, "hegemony", hg->valuedouble);
    cJSON_AddStringToObject(p, "timebin", tb);
    const cJSON *af = cJSON_GetObjectItem(r, "af");
    if (cJSON_IsNumber(af)) cJSON_AddNumberToObject(p, "address_family", af->valuedouble);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[256];
    snprintf(title, sizeof title, "AS%s depends on AS%s%s%s", origin, dep,
             dname ? " " : "", dname ? dname : "");
    char summary[128];
    snprintf(summary, sizeof summary, "hegemony %.5f @ %s", hg->valuedouble, tb);
    char key[128];
    snprintf(key, sizeof key, "%s|%s|%s", origin, dep, tb);
    char link[160];
    snprintf(link, sizeof link, "https://ihr.iijlab.net/ihr/en/network/AS%s", origin);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = tb;
    it.record_type     = "as-hegemony";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"bgp\",\"transit-dependency\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, ok = 0;
  for (int i = 0; i < NWATCH; i++) {
    int r = emit_for(ctx, sink, WATCH[i]);
    if (r < 0) { fprintf(stderr, "[ihr-hegemony] fetch failed for AS%s\n", WATCH[i]); continue; }
    ok++; total += r;
  }
  fprintf(stderr, "[ihr-hegemony] emitted %d over %d/%d origins\n", total, ok, NWATCH);
  if (ok == 0) return -1;              /* every fetch failed */
  return 0;                            /* fetched fine, maybe quiet (R3) */
}

static const source_def cyi_ihr_hegemony_def = {
  .id = "ihr-country-hegemony", .collector = "cyber",
  .name = "IIJ Internet Health Report — AS hegemony",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://ihr.iijlab.net/ihr/api/hegemony/",
  .description = "Transit-dependency measurement: which upstreams the rest of the internet must cross to reach an AS, weighted 0..1 — the metric that shows a provider becoming a chokepoint.",
  .license = "IIJ Research Lab public API; attribution to Internet Health Report expected.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_ihr_hegemony_def)
