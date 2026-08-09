/* OpenRailwayMap — rail facility lookup.
 * Endpoint: https://api.openrailwaymap.org/v2/facility?q=<name>
 * Emits: one intel row per matched facility — OSM id, name, the railway= class
 * (station/halt/yard/junction/…), ref and uic_ref, uic_name, infrastructure
 * operator, Wikidata id, opening date, wheelchair flag and addr:city. Keyless.
 * Licence: OpenRailwayMap / OpenStreetMap contributors, ODbL 1.0 — attribution
 * and share-alike are required on derived geodata.
 *
 * This is a name-to-facility resolver rather than a bulk feed, so it answers
 * OSINT entity pivots (ctx->entity) and otherwise sweeps a fixed list of major
 * hubs. The query list only decides which records are fetched.
 *
 * Parse note: the v2/facility response carries OSM tags but NO lat/lon in the
 * probed payload, so every row is emitted with has_geo = 0. Deriving a position
 * from addr:city would be exactly the invented geometry R2 forbids; the osm_id
 * is emitted so geometry can be resolved against the OSM API when it is needed.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static const char *const FACILITIES[] = {
  "Berlin Hbf", "Wien Hbf", "Zurich HB", "Milano Centrale", "Roma Termini",
  "Madrid Chamartin", "Paris Nord", "London Waterloo", "Amsterdam Centraal",
  "Warszawa Centralna", "Praha hlavni nadrazi", "Budapest-Keleti",
  "Kobenhavn H", "Stockholm C", "Oslo S", "Helsinki asema", "Moskva",
  "Istanbul", "Beijing", "Tokyo", NULL };

static int emit_query(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = trn_urlencode(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
           "https://api.openrailwaymap.org/v2/facility?q=%s", enc);
  free(enc);

  cJSON *doc = feed_get_json(ctx->http, url, 20000);
  if (!doc || !cJSON_IsArray(doc)) { cJSON_Delete(doc); return 0; }

  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, doc) {
    char osmid[64];
    trn_idfield(f, "osm_id", osmid, sizeof osmid);
    const char *nm = jo_sv(f, "name");
    if (!osmid[0] && !nm) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator_source", "OpenRailwayMap / OSM");
    trn_put_str(pr, "osm_id", osmid);
    trn_put_str(pr, "name", nm);
    trn_put_str(pr, "railway", jo_sv(f, "railway"));
    trn_put_str(pr, "ref", jo_sv(f, "ref"));
    trn_put_str(pr, "uic_ref", jo_sv(f, "uic_ref"));
    trn_put_str(pr, "uic_name", jo_sv(f, "uic_name"));
    trn_put_str(pr, "operator", jo_sv(f, "operator"));
    trn_put_str(pr, "wikidata", jo_sv(f, "wikidata"));
    trn_put_str(pr, "start_date", jo_sv(f, "start_date"));
    trn_put_str(pr, "wheelchair", jo_sv(f, "wheelchair"));
    trn_put_str(pr, "addr_city", jo_sv(f, "addr:city"));
    cJSON_AddStringToObject(pr, "matched_query", q);
    char *pj = cJSON_PrintUnformatted(pr);

    char link[160]; link[0] = 0;
    if (osmid[0])
      snprintf(link, sizeof link, "https://www.openstreetmap.org/node/%s", osmid);

    char summary[256];
    const char *rw = jo_sv(f, "railway"), *op = jo_sv(f, "operator");
    if (rw && op) snprintf(summary, sizeof summary, "%s · %s", rw, op);
    else if (rw)  snprintf(summary, sizeof summary, "%s", rw);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = osmid[0] ? osmid : nm;
    it.title           = nm ? nm : osmid;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = link[0] ? link : NULL;
    it.record_type     = "rail-facility";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"rail\",\"osm\",\"infrastructure\"]";
    /* no coordinates in this response → no geo (R2) */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0;
  if (ctx->entity && ctx->entity[0]) {
    n += emit_query(ctx, sink, ctx->entity);
  } else {
    for (int i = 0; FACILITIES[i]; i++) n += emit_query(ctx, sink, FACILITIES[i]);
  }
  fprintf(stderr, "[openrailwaymap-facility-lookup] emitted %d\n", n);
  return 0;
}

static const source_def trn_openrailwaymap_facility_def = {
  .id = "openrailwaymap-facility-lookup", .collector = "transport",
  .name = "OpenRailwayMap — rail facility lookup",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "api",
  .url = "https://api.openrailwaymap.org/v2/facility",
  .description = "Name-to-rail-facility resolution over the global OpenRailwayMap dataset: OSM id, UIC reference, infrastructure operator, Wikidata id and opening date for stations, halts, yards and junctions.",
  .license = "OpenRailwayMap / OpenStreetMap contributors, ODbL 1.0 (attribution and share-alike required).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_openrailwaymap_facility_def)
