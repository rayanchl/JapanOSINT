/* Transport for London — Santander Cycles dock inventory and live availability.
 * Endpoint: https://api.tfl.gov.uk/BikePoint
 * Emits: one intel row per docking station — TfL BikePoint id, common name, the
 * dock's own lat/lon, and the live NbBikes / NbEBikes / NbStandardBikes /
 * NbEmptyDocks / NbDocks counters plus the Locked and Temporary flags.
 * Keyless (an app key only raises the rate limit).
 * Licence: TfL Open Data — free to reuse with the attribution
 * "Powered by TfL Open Data".
 *
 * Parse note (trap): the live counts are NOT top-level fields. They live in
 * additionalProperties[] as {key,value} pairs whose value is a STRING, so each
 * counter has to be looked up by key and coerced. lat/lon are top-level numbers.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

/* additionalProperties[] lookup: returns the raw string value for `key`. */
static const char *ap(const cJSON *point, const char *key) {
  const cJSON *arr = cJSON_GetObjectItem(point, "additionalProperties");
  const cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    const char *k = jo_sv(e, "key");
    if (k && strcmp(k, key) == 0) return jo_sv(e, "value");
  }
  return NULL;
}

/* Same lookup, coerced to a number (the values are strings upstream). */
static int ap_num(const cJSON *point, const char *key, double *out) {
  const char *v = ap(point, key);
  if (!v) return 0;
  char *end = NULL;
  double d = strtod(v, &end);
  if (!end || end == v || !isfinite(d)) return 0;
  *out = d;
  return 1;
}

static void put_ap(cJSON *pr, const cJSON *point, const char *key) {
  double d;
  if (ap_num(point, key, &d)) cJSON_AddNumberToObject(pr, key, d);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, "https://api.tfl.gov.uk/BikePoint", 30000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[tfl-bikepoint] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *p;
  cJSON_ArrayForEach(p, doc) {
    const char *id = jo_sv(p, "id");
    const char *nm = jo_sv(p, "commonName");
    if (!id && !nm) continue;                    /* nothing real → no row */

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "TfL Santander Cycles");
    trn_put_str(pr, "bikepoint_id", id);
    trn_put_str(pr, "common_name", nm);
    put_ap(pr, p, "NbBikes");
    put_ap(pr, p, "NbEBikes");
    put_ap(pr, p, "NbStandardBikes");
    put_ap(pr, p, "NbEmptyDocks");
    put_ap(pr, p, "NbDocks");
    trn_put_str(pr, "Locked", ap(p, "Locked"));
    trn_put_str(pr, "Temporary", ap(p, "Temporary"));
    trn_put_str(pr, "InstallDate", ap(p, "InstallDate"));
    char *pj = cJSON_PrintUnformatted(pr);

    char summary[160]; summary[0] = 0;
    double bikes, docks;
    int hb = ap_num(p, "NbBikes", &bikes), hd = ap_num(p, "NbEmptyDocks", &docks);
    if (hb && hd)
      snprintf(summary, sizeof summary, "%.0f bikes / %.0f empty docks",
               bikes, docks);
    else if (hb)
      snprintf(summary, sizeof summary, "%.0f bikes", bikes);

    char link[256];
    snprintf(link, sizeof link, "https://api.tfl.gov.uk/BikePoint/%s",
             id ? id : "");

    intel_item it = {0};
    it.remote_key      = id ? id : nm;
    it.title           = nm ? nm : id;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = id ? link : NULL;
    it.lang            = "en";
    it.record_type     = "bike-share-station";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"micromobility\",\"london\"]";
    double la, lo;
    if (trn_num(p, "lat", &la) && trn_num(p, "lon", &lo) && trn_geo_ok(la, lo)) {
      it.has_geo = 1; it.lat = la; it.lon = lo;
    }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[tfl-bikepoint] emitted %d\n", n);
  return 0;                                      /* parsed fine → 0 (R3) */
}

static const source_def trn_tfl_bikepoint_def = {
  .id = "tfl-bikepoint", .collector = "transport",
  .name = "TfL Santander Cycles — docks and live availability",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://api.tfl.gov.uk/BikePoint",
  .description = "Every Santander Cycles docking station in London with live bike, e-bike and empty-dock counts in a single call.",
  .license = "TfL Open Data — attribution 'Powered by TfL Open Data'.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_tfl_bikepoint_def)
