/* collectors/transport/sources/transport_cluster_runner.c
 * Port of transportRunner.js post-mode steps (#3 in its sequence): after the
 * unified-* ingest sources have populated intel_items, snap train/subway
 * stations to nearest track colour, rebuild station_clusters +
 * station_line_dots, then upsert OSM station-boundary footprints and link
 * them to clusters. Registered SRC_FEED so the existing scheduler runs it on
 * its interval = the runner (camera-discovery similarly self-schedules). */
#include "../../../source.h"
#include "../../../core/station_clusterer.h"
#include "../../../core/station_footprints.h"
#include "../../../third_party/sqlite3.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* osm-transport-station-boundaries emitted Polygon footprints into
 * intel_items; collect them back as Features for the footprints store. */
static cJSON *load_footprint_features(db_handle *db) {
  cJSON *arr = cJSON_CreateArray();
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT geometry,properties FROM intel_items "
        "WHERE source_id='osm-transport-station-boundaries' "
        "AND geometry IS NOT NULL "
        "AND json_extract(geometry,'$.type')='Polygon'", -1, &st, NULL)
      == SQLITE_OK) {
    while (sqlite3_step(st) == SQLITE_ROW) {
      const char *gj = (const char *)sqlite3_column_text(st, 0);
      const char *pj = (const char *)sqlite3_column_text(st, 1);
      cJSON *geom = gj ? cJSON_Parse(gj) : NULL;
      if (!geom) continue;
      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON_AddItemToObject(f, "geometry", geom);
      cJSON *p = pj ? cJSON_Parse(pj) : NULL;
      cJSON_AddItemToObject(f, "properties", p ? p : cJSON_CreateObject());
      cJSON_AddItemToArray(arr, f);
    }
  }
  sqlite3_finalize(st);
  return arr;
}

static cJSON *load_clusters(db_handle *db) {
  cJSON *arr = cJSON_CreateArray();
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT cluster_uid,lat,lon FROM station_clusters", -1, &st, NULL)
      == SQLITE_OK) {
    while (sqlite3_step(st) == SQLITE_ROW) {
      cJSON *c = cJSON_CreateObject();
      cJSON_AddStringToObject(c, "cluster_uid",
        (const char *)sqlite3_column_text(st, 0));
      cJSON_AddNumberToObject(c, "lat", sqlite3_column_double(st, 1));
      cJSON_AddNumberToObject(c, "lon", sqlite3_column_double(st, 2));
      cJSON_AddItemToArray(arr, c);
    }
  }
  sqlite3_finalize(st);
  return arr;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  db_handle *db = ctx->db;
  int sn_t = station_snap_stations(db, "train");
  int sn_s = station_snap_stations(db, "subway");
  int nclu = station_clusterer_run(db);

  cJSON *fps = load_footprint_features(db);
  int nfp = station_footprints_upsert_tx(db, fps);
  cJSON_Delete(fps);
  cJSON *clu = load_clusters(db);
  int nlk = station_footprints_link_clusters_tx(db, clu);
  cJSON_Delete(clu);

  fprintf(stderr,
    "[transport-cluster-runner] snap train=%d subway=%d clusters=%d "
    "footprints=%d linked=%d\n", sn_t, sn_s, nclu, nfp, nlk);
  return nclu >= 0 ? 0 : -1;
}

static const source_def transport_cluster_runner_def = {
  .id = "transport-cluster-runner", .collector = "transport",
  .name = "Transport Station Clusterer", .name_ja = "駅クラスタリング",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(transport_cluster_runner_def)
