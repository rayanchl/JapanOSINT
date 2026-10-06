/* Verified-live eu_research sources (13), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "lib/pagewalk.h"

/* eu-cordis-programmes-search: CORDIS publishes one record per programme node
 * PER LANGUAGE, and every language edition repeats the same "id". Live-verified
 * 2026-09-07 on a 50-record page: 25 distinct "id", 50 distinct id+language —
 * the 48% collapse the registry sweep measured. The language editions carry
 * different titles and teasers, so merging them destroys real records. Same
 * hand-rolled paged-emit-with-relabelled-id pattern as vsrc_environment_3.c
 * (geo-tidesandcurrents-currents), because jsonlist_emit_paged_keyed reads a
 * single field and this identity is a pair. */
typedef struct { const char *path, *record_type, *lang, *tags_json; } cordis_lang_opts;

static int cordis_emit_page_lang(const source_ctx *c, intel_sink *s,
                                 const char *id, cJSON *doc, void *ud,
                                 int *seen) {
  (void)c;
  cordis_lang_opts *o = (cordis_lang_opts *)ud;
  cJSON *arr = jsonlist_find_array(doc, o->path);
  if (arr) {
    cJSON *rec;
    cJSON_ArrayForEach(rec, arr) {
      if (!cJSON_IsObject(rec)) continue;
      cJSON *rid = cJSON_GetObjectItemCaseSensitive(rec, "id");
      cJSON *lg  = cJSON_GetObjectItemCaseSensitive(rec, "language");
      if (!cJSON_IsString(rid) || !rid->valuestring) continue;
      if (!cJSON_IsString(lg) || !lg->valuestring) continue;
      char buf[256];
      snprintf(buf, sizeof buf, "%s_%s", rid->valuestring, lg->valuestring);
      cJSON_DeleteItemFromObjectCaseSensitive(rec, "id");
      cJSON_AddStringToObject(rec, "id", buf);
    }
  }
  return jsonlist_emit_ex(s, id, doc, o->path, o->record_type, o->lang,
                          o->tags_json, seen);
}

static int run_eu_cordis_programmes_search(const source_ctx *c, intel_sink *s) {
  cordis_lang_opts o = { "payload.results", "research", "en",
    "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\"]" };
  int n = pw_walk(c, s, "eu-cordis-programmes-search",
                  "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27programme%27&p=1&num=10&format=json&srt=id:increasing",   /* exhaustive-ok: pw_walk advances p= (PW_PAGE_PARAMS, lib/pagewalk.c:199) and discloses the remainder */
                  pw_fetch_json, cordis_emit_page_lang, &o);
  if (n < 0) {
    fprintf(stderr, "[eu-cordis-programmes-search] fetch failed\n");
    return -1;
  }
  return 0;
}
static const source_def eu_cordis_programmes_search = {
  .id = "eu-cordis-programmes-search", .collector = "eu_research",
  .name = "CORDIS (EU) — funding programme hierarchy",
  .name_ja = "CORDIS (EU) — funding programme hierarchy",
  .update_interval_sec = 86400, .run = run_eu_cordis_programmes_search,
  .category = "research", .type = "api",
  .url = "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27programme%27&p=1&num=10&format=json&srt=id:increasing",   /* exhaustive-ok: documentation copy of the walked URL above; run() pages it */
  .description = "8,678 EU funding programme/sub-programme nodes (FP7, H2020, Horizon Europe, Euratom branches) with codes and rcns — the budget-line tree that every CORDIS project attaches to.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(eu_cordis_programmes_search);

VJSON(eu_cordis_results_search, "eu-cordis-results-search", "CORDIS (EU) — research result/deliverable search", "CORDIS (EU) — research result/deliverable search",
  "eu_research", "research",
  "https://cordis.europa.eu/api/search/results?q=contenttype%3D%27result%27&p=1&num=3&format=json",
  "payload.results",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\"]", 86400,
  "884,143 research outputs (deliverables, publications, reports) produced under EU grants, each linkable back to the funding project. Same query grammar and pagination (searchAfter cursor) as the project search.");

VJSON(global_ooni_test_names, "global-ooni-test-names", "OONI test-name registry", "OONI test-name registry",
  "eu_research", "research",
  "https://api.ooni.io/api/_/test_names",
  "test_names",
  "en", "[\"eu\",\"research\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Canonical id/name pairs for every OONI test (bridge_reachability, dnscheck, psiphon, riseupvpn, whatsapp, ...). Needed to enumerate the measurements API exhaustively per test.");

