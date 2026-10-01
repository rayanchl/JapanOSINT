/* US BTS — monthly border crossing/entry volumes with port coordinates.
 * Endpoint: https://data.transportation.gov/resource/keg4-3bc2.json
 *           ?$limit=1000&$order=date%20DESC
 * Emits: one intel row per (port, month, measure) observation — port_name,
 * port_code, state, which border, the observation month, the measure (Trucks,
 * Buses, Personal Vehicles, Pedestrians, Containers …) and its count, pinned on
 * the port's own published latitude/longitude. Keyless.
 * Licence: US Bureau of Transportation Statistics via data.transportation.gov
 * (Socrata) — US Government work, public domain.
 *
 * Parse notes: Socrata SoDA endpoint. $limit defaults to 1000 and $order=date
 * DESC is what pulls the newest month first. latitude/longitude come back as
 * STRINGS (there is also a GeoJSON `point` object holding [lon,lat]); both
 * forms are handled and neither is ever substituted by a state centroid.
 */
#include "lib/jocore.h"
#include "lib/pagewalk.h"
#include "trn_common.inc"

/* `$offset=0` seeds lib/pagewalk.c's offset walk — pw_walk only ever advances a
 * parameter the author already wrote, and never invents one. `,:id` makes the
 * sort TOTAL: this table is one row per (port, month, measure), so a bare
 * `date DESC` ties thousands of rows together and an offset window taken across
 * that tie has no defined content. Measured 2026-09-19: 275,901 rows upstream,
 * of which the single-fetch collector kept 1,000. */
#define BTS_URL "https://data.transportation.gov/resource/keg4-3bc2.json" \
                "?$limit=1000&$offset=0&$order=date%20DESC,:id"

/* pw_emit_fn: one page, reporting what it CONTAINED in `seen`. */
static int bts_emit_page(const source_ctx *ctx, intel_sink *sink,
                         const char *id, cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)id; (void)ud;
  *seen = 0;
  if (!cJSON_IsArray(doc)) return 0;
  *seen = cJSON_GetArraySize(doc);

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    const char *port = jo_sv(r, "port_name");
    const char *code = jo_sv(r, "port_code");
    const char *measure = jo_sv(r, "measure");
    const char *date = jo_sv(r, "date");
    if (!port && !code) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator",
                            "US Bureau of Transportation Statistics");
    trn_put_str(pr, "port_name", port);
    trn_put_str(pr, "port_code", code);
    trn_put_str(pr, "state", jo_sv(r, "state"));
    trn_put_str(pr, "border", jo_sv(r, "border"));
    trn_put_str(pr, "date", date);
    trn_put_str(pr, "measure", measure);
    trn_put_num(pr, "value", r, "value");
    char *pj = cJSON_PrintUnformatted(pr);

    char rk[192], title[320], summary[160];
    snprintf(rk, sizeof rk, "%s|%s|%s", code ? code : port,
             date ? date : "", measure ? measure : "");
    snprintf(title, sizeof title, "%s — %s", port ? port : code,
             measure ? measure : "crossings");
    double val;
    if (trn_num(r, "value", &val))
      snprintf(summary, sizeof summary, "%.0f in %s", val,
               date ? date : "the reported month");
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = rk;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "en";
    it.published_at    = date;
    it.record_type     = "border-crossing-volume";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"border\",\"us\",\"statistics\"]";

    /* latitude/longitude arrive as STRINGS; the GeoJSON `point` is the
     * fallback and stores [lon, lat]. */
    double la, lo;
    if (trn_num(r, "latitude", &la) && trn_num(r, "longitude", &lo) &&
        trn_geo_ok(la, lo)) {
      it.has_geo = 1; it.lat = la; it.lon = lo;
    } else if (trn_geom_first_pos(cJSON_GetObjectItem(r, "point"), &la, &lo)) {
      it.has_geo = 1; it.lat = la; it.lon = lo;
    }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = pw_walk(ctx, sink, "bts-border-crossing-volumes", BTS_URL,
                  pw_fetch_json, bts_emit_page, NULL);
  if (n < 0) {
    fprintf(stderr, "[bts-border-crossing-volumes] fetch/parse failed\n");
    return -1;
  }
  return 0;
}

static const source_def trn_bts_border_crossing_volumes_def = {
  .id = "bts-border-crossing-volumes", .collector = "transport",
  .name = "US BTS border crossing/entry volumes with port coordinates",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset",
  .url = BTS_URL,
  .description = "Monthly inbound crossing counts by mode for every US land port of entry, each carrying the port's real coordinates — the volume baseline that makes a CBP wait-time spike interpretable.",
  .license = "US Government work, public domain (BTS via data.transportation.gov).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_bts_border_crossing_volumes_def)
