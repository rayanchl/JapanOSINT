/* Mobility Database — the global GTFS / GTFS-RT / GBFS feed catalogue.
 * Endpoint:
 *   https://storage.googleapis.com/storage/v1/b/mdb-csv/o/sources.csv?alt=media
 * Emits: one intel row per registered feed — mdb_source_id, data_type,
 * provider, feed name, country code, subdivision and municipality, the direct
 * download URL, the "latest" URL and the feed's own licence URL, plus the
 * publisher-supplied bounding box. Keyless.
 * Licence: MobilityData, CC0-1.0 for the catalogue itself. Each individual feed
 * carries its own licence in the urls.license column, which is re-emitted.
 *
 * Parse notes: ~1.15 MB CSV with a header row — lib/csv.h handles it directly.
 *
 * R2 note on geometry: the catalogue publishes only a BOUNDING BOX per feed,
 * never a point. Where all four bounds are present the row is pinned on the box
 * centre and explicitly tagged geo_precision="bbox-centroid" — that is a coarse
 * but genuine upstream area, not an invented location. A feed with no bounds is
 * emitted with has_geo = 0.
 */
#include "lib/jocore.h"
#include "trn_common.inc"
#include "lib/csv.h"

#define MDB_URL "https://storage.googleapis.com/storage/v1/b/mdb-csv/o/" \
                "sources.csv?alt=media"

static const char *cell(const cJSON *row, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(row, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int cellnum(const cJSON *row, const char *k, double *out) {
  const char *s = cell(row, k);
  if (!s) return 0;
  char *end = NULL;
  double d = strtod(s, &end);
  if (!end || end == s || !isfinite(d)) return 0;
  *out = d;
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *csv = feed_get_text(ctx->http, MDB_URL, 60000);
  if (!csv) {
    fprintf(stderr, "[mobilitydatabase-gtfs-catalog] fetch failed\n");
    return -1;
  }
  cJSON *rows = csv_parse(csv, 1);
  free(csv);
  if (!rows) {
    fprintf(stderr, "[mobilitydatabase-gtfs-catalog] CSV parse failed\n");
    return -1;
  }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *sid = cell(r, "mdb_source_id");
    const char *name = cell(r, "name");
    const char *provider = cell(r, "provider");
    if (!sid && !provider && !name) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "registry", "Mobility Database");
    trn_put_str(pr, "mdb_source_id", sid);
    trn_put_str(pr, "data_type", cell(r, "data_type"));
    trn_put_str(pr, "provider", provider);
    trn_put_str(pr, "name", name);
    trn_put_str(pr, "country_code", cell(r, "location.country_code"));
    trn_put_str(pr, "subdivision_name", cell(r, "location.subdivision_name"));
    trn_put_str(pr, "municipality", cell(r, "location.municipality"));
    trn_put_str(pr, "direct_download_url", cell(r, "urls.direct_download"));
    trn_put_str(pr, "latest_url", cell(r, "urls.latest"));
    trn_put_str(pr, "license_url", cell(r, "urls.license"));
    trn_put_str(pr, "status", cell(r, "status"));

    double la0, la1, lo0, lo1;
    int have_bbox =
        cellnum(r, "location.bounding_box.minimum_latitude", &la0) &&
        cellnum(r, "location.bounding_box.maximum_latitude", &la1) &&
        cellnum(r, "location.bounding_box.minimum_longitude", &lo0) &&
        cellnum(r, "location.bounding_box.maximum_longitude", &lo1);
    double clat = 0, clon = 0; int geo = 0;
    if (have_bbox) {
      cJSON_AddNumberToObject(pr, "bbox_min_lat", la0);
      cJSON_AddNumberToObject(pr, "bbox_max_lat", la1);
      cJSON_AddNumberToObject(pr, "bbox_min_lon", lo0);
      cJSON_AddNumberToObject(pr, "bbox_max_lon", lo1);
      clat = (la0 + la1) / 2.0; clon = (lo0 + lo1) / 2.0;
      if (trn_geo_ok(clat, clon)) {
        geo = 1;
        cJSON_AddStringToObject(pr, "geo_precision", "bbox-centroid");
      }
    }
    char *pj = cJSON_PrintUnformatted(pr);

    char title[384], summary[320];
    if (provider && name) snprintf(title, sizeof title, "%s — %s",
                                   provider, name);
    else snprintf(title, sizeof title, "%s",
                  provider ? provider : (name ? name : sid));
    const char *dt = cell(r, "data_type"), *cc = cell(r, "location.country_code");
    if (dt && cc) snprintf(summary, sizeof summary, "%s · %s", dt, cc);
    else if (dt)  snprintf(summary, sizeof summary, "%s", dt);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = sid ? sid : title;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = cell(r, "urls.direct_download");
    it.lang            = "en";
    it.record_type     = "mobility-feed-catalogue-entry";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"catalogue\",\"gtfs\",\"gbfs\"]";
    if (geo) { it.has_geo = 1; it.lat = clat; it.lon = clon; }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(rows);
  fprintf(stderr, "[mobilitydatabase-gtfs-catalog] emitted %d\n", n);
  return 0;
}

static const source_def trn_mobilitydatabase_gtfs_catalog_def = {
  .id = "mobilitydatabase-gtfs-catalog", .collector = "transport",
  .name = "Mobility Database — global GTFS/GBFS feed catalogue",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset",
  .url = MDB_URL,
  .description = "The world's registry of public-transport feeds: thousands of GTFS, GTFS-RT and GBFS sources with operator, country, download URL, licence URL and a bounding box per feed.",
  .license = "CC0-1.0 (MobilityData catalogue); individual feed licences carried in urls.license.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_mobilitydatabase_gtfs_catalog_def)
