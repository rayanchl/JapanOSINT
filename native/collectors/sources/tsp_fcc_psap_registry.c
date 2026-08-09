/* collectors/sources/tsp_fcc_psap_registry.c
 * FCC 911 Master PSAP Registry — the routing layer of the US emergency telecom
 * network: every primary and secondary Public Safety Answering Point.
 * Endpoint: https://opendata.fcc.gov/resource/dpq5-ta9j.json?$limit=5000&$offset=N
 *   (keyless Socrata)
 * Emits: psap_id, psap_name, county, city, state and the registry's
 *   type-of-change text.
 *
 * GEO WARNING — handled exactly as the manifest requires, quoted:
 *   "the only coordinate in this dataset is 'geocode_city_level', an FCC
 *    geocode of the CITY, not the PSAP building; several rows have no
 *    coordinate at all and one sample geocodes an Arizona tribal PSAP to
 *    Sedona. Per R2 this is not a facility location: emit with has_geo=0".
 *   So every row here is has_geo=0. The city geocode is still carried in
 *   properties under explicitly-named keys (geocode_city_level_*) with a
 *   geo_note, so an analyst can see it and can never mistake it for the PSAP's
 *   address.
 * parse_notes honoured: "Socrata strings throughout; geocode_city_level is a
 *   nested object."
 * Licence: FCC open data (Socrata) — US Government public domain, keyless.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PSAP_BASE "https://opendata.fcc.gov/resource/dpq5-ta9j.json"
#define PAGE_SIZE 5000
#define PAGES 3

static int emit_page(cJSON *arr, intel_sink *sink) {
  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    const char *id   = jo_sv(r, "psap_id");
    const char *name = jo_sv(r, "psap_name");
    if (!name && !id) continue;                   /* no identity -> no row */

    const char *county = jo_sv(r, "county");
    const char *city   = jo_sv(r, "city");
    const char *state  = jo_sv(r, "state");

    cJSON *pr = cJSON_CreateObject();
    jo_add_str(pr, "psap_id", id);
    jo_add_str(pr, "psap_name", name);
    jo_add_str(pr, "county", county);
    jo_add_str(pr, "city", city);
    jo_add_str(pr, "state", state);
    jo_add_str(pr, "type_of_change", jo_sv(r, "type_of_change_full_text"));
    /* The city geocode is recorded but NEVER used as the row's geometry. */
    const cJSON *g = cJSON_GetObjectItem(r, "geocode_city_level");
    if (cJSON_IsObject(g)) {
      jo_add_str(pr, "geocode_city_level_latitude", jo_sv(g, "latitude"));
      jo_add_str(pr, "geocode_city_level_longitude", jo_sv(g, "longitude"));
      jo_add_str(pr, "geocode_city_level_human_address", jo_sv(g, "human_address"));
    }
    cJSON_AddStringToObject(pr, "geo_note",
      "the FCC publishes only a CITY-level geocode for a PSAP, not the call "
      "centre's location; it is recorded here for reference and deliberately "
      "not used as this row's coordinates");
    cJSON_AddStringToObject(pr, "source", "FCC open data dpq5-ta9j (911 Master PSAP Registry)");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[256], summary[256], key[64];
    snprintf(key, sizeof key, "%s", id ? id : (name ? name : ""));
    snprintf(title, sizeof title, "%s%s%s%s%s", name ? name : "PSAP",
             city ? " — " : "", city ? city : "",
             state ? ", " : "", state ? state : "");
    snprintf(summary, sizeof summary, "PSAP %s%s%s%s",
             id ? id : "", county ? " · " : "", county ? county : "",
             county ? " County" : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://opendata.fcc.gov/d/dpq5-ta9j";
    it.lang            = "en";
    it.record_type     = "psap-911-center";
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"emergency\",\"psap\",\"fcc\"]";
    /* R2: has_geo stays 0 — see the GEO WARNING in the file header. */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, pages = 0;
  for (int i = 0; i < PAGES; i++) {
    char url[256];
    snprintf(url, sizeof url, "%s?$limit=%d&$offset=%d&$order=psap_id",
             PSAP_BASE, PAGE_SIZE, i * PAGE_SIZE);
    cJSON *doc = feed_get_json(ctx->http, url, 45000);
    if (!doc) break;
    if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); break; }
    pages++;
    int got = cJSON_GetArraySize(doc);
    total += emit_page(doc, sink);
    cJSON_Delete(doc);
    if (got < PAGE_SIZE) break;
  }
  if (pages == 0) {
    fprintf(stderr, "[fcc-psap-registry] fetch failed\n");
    return -1;
  }
  fprintf(stderr, "[fcc-psap-registry] emitted %d over %d page(s)\n", total, pages);
  return 0;
}

static const source_def tsp_fcc_psap_registry_def = {
  .id = "fcc-psap-registry", .collector = "telecom",
  .name = "FCC 911 Master PSAP Registry",
  .update_interval_sec = 604800, .run = run,
  .category = "telecom", .type = "dataset", .url = PSAP_BASE,
  .description = "The FCC's authoritative list of US Public Safety Answering Points — every primary and secondary 911 call centre with its id, name, county and state. No facility coordinates are published, so none are asserted.",
  .license = "FCC open data (Socrata) — US Government public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_fcc_psap_registry_def)
