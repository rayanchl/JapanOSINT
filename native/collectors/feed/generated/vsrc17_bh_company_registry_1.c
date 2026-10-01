/* Verified-live bh_company-registry sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "lib/pagewalk.h"

/* bh-travel-cargo-establishments: an OpenDataSoft record carries no id field
 * the jsonlist precedence list knows, so rows fell back to a hash of
 * (title, link, date) — here the trading name alone, and Bahrain's register
 * has several establishments trading under the same name at different
 * addresses. Live-verified 2026-09-07 on a 100-record page: 100 byte-distinct
 * records but only 95 distinct "name"; name plus the establishment's own
 * published coordinates is 100 of 100. All three values are the upstream's,
 * and the record keeps every field it arrived with. jsonlist_emit_paged_keyed
 * reads a single field, so this uses the hand-rolled composite pattern of
 * vsrc_environment_3.c (geo-tidesandcurrents-currents). */
typedef struct { const char *path, *record_type, *lang, *tags_json; } bh_composite_opts;

static int bh_emit_page_composite(const source_ctx *c, intel_sink *s,
                                  const char *id, cJSON *doc, void *ud,
                                  int *seen) {
  (void)c;
  bh_composite_opts *o = (bh_composite_opts *)ud;
  cJSON *arr = jsonlist_find_array(doc, o->path);
  if (arr) {
    cJSON *rec;
    cJSON_ArrayForEach(rec, arr) {
      if (!cJSON_IsObject(rec)) continue;
      cJSON *nm = cJSON_GetObjectItemCaseSensitive(rec, "name");
      cJSON *lo = cJSON_GetObjectItemCaseSensitive(rec, "x_longitude");
      cJSON *la = cJSON_GetObjectItemCaseSensitive(rec, "y_latitude");
      if (!cJSON_IsString(nm) || !nm->valuestring) continue;
      char buf[256];
      if (cJSON_IsNumber(lo) && cJSON_IsNumber(la))
        snprintf(buf, sizeof buf, "%s|%.8f|%.8f", nm->valuestring,
                 lo->valuedouble, la->valuedouble);
      else
        snprintf(buf, sizeof buf, "%s", nm->valuestring);
      cJSON_DeleteItemFromObjectCaseSensitive(rec, "id");
      cJSON_AddStringToObject(rec, "id", buf);
    }
  }
  return jsonlist_emit_ex(s, id, doc, o->path, o->record_type, o->lang,
                          o->tags_json, seen);
}

static int run_bh_travel_cargo_establishments(const source_ctx *c, intel_sink *s) {
  bh_composite_opts o = { "results", "company-registry", "en",
    "[\"bh\",\"company-registry\",\"batch17\",\"high-penetrancy\"]" };
  int n = pw_walk(c, s, "bh-travel-cargo-establishments",
                  "https://data.gov.bh/api/explore/v2.1/catalog/datasets/travel-and-cargo-services/records?limit=100",
                  pw_fetch_json, bh_emit_page_composite, &o);
  if (n < 0) {
    fprintf(stderr, "[bh-travel-cargo-establishments] fetch failed\n");
    return -1;
  }
  return 0;
}
static const source_def bh_travel_cargo_establishments = {
  .id = "bh-travel-cargo-establishments", .collector = "bh_company-registry",
  .name = "Bahrain — travel and cargo service establishments",
  .name_ja = "Bahrain — travel and cargo service establishments",
  .update_interval_sec = 86400, .run = run_bh_travel_cargo_establishments,
  .category = "company-registry", .type = "api",
  .url = "https://data.gov.bh/api/explore/v2.1/catalog/datasets/travel-and-cargo-services/records?limit=100",
  .description = "352 NAMED commercial establishments: trading name in English and Arabic, activity type and subtype in both languages (e.g. boat rentals), block number and longitude/latitude. A geolocated business directory, not a statistical table.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(bh_travel_cargo_establishments);
