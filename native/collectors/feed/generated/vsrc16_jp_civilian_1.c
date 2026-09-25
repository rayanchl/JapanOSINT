/* Verified-live jp_civilian sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "lib/pagewalk.h"

/* global-taginfo-search-by-value: each hit is {key, value, count_all} — an OSM
 * tag KEY paired with a tag VALUE. The record's identity is the pair, but
 * jsonlist's id precedence list contains "key", so every hit was keyed on the
 * tag key alone: 46 hits across 14 distinct keys (name, name:en, brand, …)
 * collapsed to 14 rows on the 2026-09-05 registry sweep (46 emitted, 14
 * stored). jsonlist_emit_paged_keyed takes a single field and the real key is
 * composite, so this hand-rolls the same paged-emit-with-relabelled-id pattern
 * as geo-tidesandcurrents-currents (vsrc_environment_3.c): an "id" of
 * "<key>=<value>" is composed from the two values already on the record —
 * nothing invented — and jsonlist_emit_ex keeps paging, disclosure, labelling
 * and the collision guard unchanged. Live-verified 2026-09-06: 46 hits, every
 * key=value pair distinct. */
typedef struct { const char *path, *record_type, *lang, *tags_json; } ti_composite_opts;

static int ti_emit_page_composite(const source_ctx *c, intel_sink *s,
                                  const char *id, cJSON *doc, void *ud,
                                  int *seen) {
  (void)c;
  ti_composite_opts *o = (ti_composite_opts *)ud;
  cJSON *arr = jsonlist_find_array(doc, o->path);
  if (arr) {
    cJSON *rec;
    cJSON_ArrayForEach(rec, arr) {
      if (!cJSON_IsObject(rec)) continue;
      cJSON *k = cJSON_GetObjectItemCaseSensitive(rec, "key");
      cJSON *v = cJSON_GetObjectItemCaseSensitive(rec, "value");
      if (!cJSON_IsString(k) || !k->valuestring) continue;
      char buf[1024];
      if (cJSON_IsString(v) && v->valuestring)
        snprintf(buf, sizeof buf, "%s=%s", k->valuestring, v->valuestring);
      else
        snprintf(buf, sizeof buf, "%s", k->valuestring);
      cJSON_DeleteItemFromObjectCaseSensitive(rec, "id");
      cJSON_AddStringToObject(rec, "id", buf);
    }
  }
  return jsonlist_emit_ex(s, id, doc, o->path, o->record_type, o->lang,
                          o->tags_json, seen);
}

static int run_global_taginfo_search_by_value(const source_ctx *c, intel_sink *s) {
  ti_composite_opts o = { "data", "civilian", "en",
                          "[\"jp\",\"civilian\",\"batch16\",\"high-penetrancy\"]" };
  int n = pw_walk(c, s, "global-taginfo-search-by-value",
                  "https://taginfo.openstreetmap.org/api/4/search/by_value?query=TEPCO&page=1&rp=10",   /* exhaustive-ok: pw_walk advances page= (PW_PAGE_PARAMS, lib/pagewalk.c:199) and discloses the remainder */
                  pw_fetch_json, ti_emit_page_composite, &o);
  if (n < 0) {
    fprintf(stderr, "[global-taginfo-search-by-value] fetch failed\n");
    return -1;
  }
  return 0;
}
static const source_def global_taginfo_search_by_value = {
  .id = "global-taginfo-search-by-value", .collector = "jp_civilian",
  .name = "OSM taginfo search by tag value",
  .name_ja = "OSM taginfo search by tag value",
  .update_interval_sec = 21600, .run = run_global_taginfo_search_by_value,
  .category = "civilian", .type = "api",
  .url = "https://taginfo.openstreetmap.org/api/4/search/by_value?query=TEPCO&page=1&rp=10",   /* exhaustive-ok: documentation copy of the walked URL above; run() pages it */
  .description = "Free-text search across all tag values, returning key, value and count_all for each hit (46 matches for TEPCO across brand, name, name:en, name:ko, addr:suburb). Company name -> every OSM tag spelling that references it, which then drives targeted extraction.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(global_taginfo_search_by_value);
