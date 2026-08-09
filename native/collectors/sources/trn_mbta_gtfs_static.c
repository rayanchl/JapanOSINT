/* MBTA Boston — GTFS static schedule archive, stops.txt.
 * Endpoint: https://cdn.mbta.com/MBTA_GTFS.zip
 * Emits: one intel row per stop in stops.txt — stop_id, stop_code, stop_name,
 * stop_desc, zone_id, location_type, parent_station, wheelchair_boarding and
 * platform_code, pinned on the stop's own stop_lat/stop_lon. Keyless.
 * Licence: MassDOT/MBTA open data, free to reuse.
 *
 * Parse notes: GTFS static is a ZIP of CSVs. Only stops.txt is expanded —
 * trips.txt alone is 8.7 MB and carries no geography. The archive is fetched as
 * BINARY through http_request (feed_get_text would stop at the first NUL byte),
 * then lib/zipread.h resolves the named entry from the central directory and
 * lib/csv.h parses the header row.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"
#include "../../lib/csv.h"
#include "../../lib/zipread.h"

#define MBTA_GTFS_URL "https://cdn.mbta.com/MBTA_GTFS.zip"

static const char *cell(const cJSON *row, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(row, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  http_response r = {0};
  if (http_request(ctx->http, "GET", MBTA_GTFS_URL, NULL, NULL, 0,
                   120000, 1, &r) != 0 ||
      r.status < 200 || r.status >= 300 || !r.body || r.body_len == 0) {
    fprintf(stderr, "[mbta-gtfs-static] download failed (status %ld)\n", r.status);
    http_response_free(&r);
    return -1;
  }

  size_t len = 0;
  char *stops = zip_find_entry(r.body, r.body_len, "stops.txt", &len);
  http_response_free(&r);
  if (!stops) {
    fprintf(stderr, "[mbta-gtfs-static] stops.txt not found in archive\n");
    return -1;
  }

  cJSON *rows = csv_parse(stops, 1);
  free(stops);
  if (!rows) {
    fprintf(stderr, "[mbta-gtfs-static] stops.txt parse failed\n");
    return -1;
  }

  int n = 0;
  cJSON *row;
  cJSON_ArrayForEach(row, rows) {
    const char *sid = cell(row, "stop_id");
    const char *nm  = cell(row, "stop_name");
    if (!sid && !nm) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "MBTA");
    trn_put_str(pr, "stop_id", sid);
    trn_put_str(pr, "stop_code", cell(row, "stop_code"));
    trn_put_str(pr, "stop_name", nm);
    trn_put_str(pr, "stop_desc", cell(row, "stop_desc"));
    trn_put_str(pr, "zone_id", cell(row, "zone_id"));
    trn_put_str(pr, "location_type", cell(row, "location_type"));
    trn_put_str(pr, "parent_station", cell(row, "parent_station"));
    trn_put_str(pr, "wheelchair_boarding", cell(row, "wheelchair_boarding"));
    trn_put_str(pr, "platform_code", cell(row, "platform_code"));
    trn_put_str(pr, "municipality", cell(row, "municipality"));
    char *pj = cJSON_PrintUnformatted(pr);

    intel_item it = {0};
    it.remote_key      = sid ? sid : nm;
    it.title           = nm ? nm : sid;
    it.summary         = cell(row, "stop_desc");
    it.lang            = "en";
    it.link            = cell(row, "stop_url");
    it.record_type     = "transit-stop";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"transit\",\"boston\",\"gtfs\"]";
    const char *slat = cell(row, "stop_lat"), *slon = cell(row, "stop_lon");
    if (slat && slon) {
      double la = strtod(slat, NULL), lo = strtod(slon, NULL);
      if (trn_geo_ok(la, lo)) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(rows);
  fprintf(stderr, "[mbta-gtfs-static] emitted %d\n", n);
  return 0;
}

static const source_def trn_mbta_gtfs_static_def = {
  .id = "mbta-gtfs-static", .collector = "transport",
  .name = "MBTA Boston — GTFS static stop inventory",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset",
  .url = MBTA_GTFS_URL,
  .description = "Every MBTA stop, platform and station entrance with coordinates, parent station and accessibility flags, taken from the official GTFS static archive.",
  .license = "MassDOT/MBTA open data, free to reuse.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_mbta_gtfs_static_def)
