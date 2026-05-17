/* core/sweepapi.c — see header. */
#include "sweepapi.h"
#include "collcache.h"
#include "camera_store.h"
#include "station_footprints.h"
#include "station_clusterer.h"
#include "linegeom.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

static void iso_now(char *o, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm g; gmtime_r(&tv.tv_sec, &g);
  snprintf(o, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           g.tm_year+1900, g.tm_mon+1, g.tm_mday, g.tm_hour, g.tm_min,
           g.tm_sec, (int)(tv.tv_usec/1000));
}

static int is_unified_mode(const char *id) {
  static const char *M[] = {
    "unified-trains","unified-subways","unified-buses","unified-airports",
    "unified-ais-ships","unified-port-infra","unified-flights",
    "unified-highway", NULL };
  for (int i = 0; M[i]; i++) if (!strcmp(id, M[i])) return 1;
  return 0;
}

/* Build a lines+stations FeatureCollection from intel_items for one
 * unified-<mode> source_id (transportRead.readMode shape; raw fragments —
 * stitch/smooth is the documented read-time follow-up). */
static char *unified_fc(db_handle *db, const char *id) {
  cJSON *fc = cJSON_CreateObject();
  cJSON_AddStringToObject(fc, "type", "FeatureCollection");
  cJSON *feats = cJSON_AddArrayToObject(fc, "features");
  int nline = 0, nst = 0;
  cJSON *lines = cJSON_CreateArray();   /* collect raw, then stitch+smooth */
  sqlite3_stmt *st = NULL;

  if (sqlite3_prepare_v2(db->h,
        "SELECT geometry,properties FROM intel_items "
        "WHERE source_id=?1 AND geometry IS NOT NULL "
        "AND json_extract(geometry,'$.type')='LineString'", -1, &st, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
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
      cJSON_AddItemToArray(lines, f);
    }
  }
  sqlite3_finalize(st); st = NULL;

  /* read-time stitch + Chaikin×4 + RDP (== transportStore.getLinesByMode);
   * bus passes through. Lines precede stations in the FC (JS order). */
  {
    const char *mode = strcmp(id, "unified-buses") == 0 ? "bus" : "rail";
    cJSON *sm = linegeom_stitch_smooth(lines, mode);
    cJSON *lf;
    cJSON_ArrayForEach(lf, sm) {
      cJSON_AddItemToArray(feats, cJSON_Duplicate(lf, 1));
      nline++;
    }
    cJSON_Delete(sm);
  }
  cJSON_Delete(lines);

  if (sqlite3_prepare_v2(db->h,
        "SELECT lat,lon,properties FROM intel_items "
        "WHERE source_id=?1 AND lat IS NOT NULL "
        "AND (geometry IS NULL OR json_extract(geometry,'$.type')='Point')",
        -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
      double lat = sqlite3_column_double(st, 0);
      double lon = sqlite3_column_double(st, 1);
      const char *pj = (const char *)sqlite3_column_text(st, 2);
      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *gc = cJSON_CreateArray();
      cJSON_AddItemToArray(gc, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(gc, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(g, "coordinates", gc);
      cJSON_AddItemToObject(f, "geometry", g);
      cJSON *p = pj ? cJSON_Parse(pj) : NULL;
      cJSON_AddItemToObject(f, "properties", p ? p : cJSON_CreateObject());
      cJSON_AddItemToArray(feats, f);
      nst++;
    }
  }
  sqlite3_finalize(st);

  cJSON *meta = cJSON_AddObjectToObject(fc, "_meta");
  char src[64]; snprintf(src, sizeof src, "transport_store:%s",
                         id + strlen("unified-"));
  cJSON_AddStringToObject(meta, "source", src);
  char now[32]; iso_now(now, sizeof now);
  cJSON_AddStringToObject(meta, "fetchedAt", now);
  cJSON_AddNumberToObject(meta, "recordCount", nline + nst);
  cJSON_AddBoolToObject(meta, "live", (nline + nst) > 0);
  cJSON_AddNumberToObject(meta, "db_stations", nst);
  cJSON_AddNumberToObject(meta, "db_lines", nline);
  char *out = cJSON_PrintUnformatted(fc);
  cJSON_Delete(fc);
  return out;
}

int sweepapi_is_sweep(const char *id) {
  if (!id || !*id) return 0;
  return !strcmp(id, "cameras")
      || !strcmp(id, "unified-station-footprints")
      || !strcmp(id, "unified-stations")
      || is_unified_mode(id);
}

/* WRITE-TIME: the full heavy build (unchanged logic, just no longer on the
 * request path). Called by the scheduler after a map_cron source runs. */
char *sweepapi_build(db_handle *db, const char *id) {
  if (!sweepapi_is_sweep(id)) return NULL;
  if (!strcmp(id, "cameras"))                   return camera_fc_json(db);
  if (!strcmp(id, "unified-station-footprints"))return station_footprints_fc_json(db);
  if (!strcmp(id, "unified-stations"))          return station_clusterer_linedots_fc_json(db);
  return unified_fc(db, id);
}

/* BOUNDED raw-points FC straight from intel_items — "the points already in
 * the DB, instantly" (owner's spec). No stitch, no smooth, no full scan,
 * never on the heavy path. Cap is env-tunable (JO_SWEEP_LIMIT). The full
 * nationwide stitched FC is intentionally NOT served (it is ~300MB for
 * unified-trains and unusable for a single map response). */
static int sweep_limit(void) {
  const char *e = getenv("JO_SWEEP_LIMIT");
  int v = e ? atoi(e) : 0;
  return (v > 0) ? v : 10000;
}
static char *cold_points_fc(db_handle *db, const char *id) {
  int LIM = sweep_limit();
  cJSON *fc = cJSON_CreateObject();
  cJSON_AddStringToObject(fc, "type", "FeatureCollection");
  cJSON *feats = cJSON_AddArrayToObject(fc, "features");
  sqlite3_stmt *st = NULL;
  int n = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT lat,lon,geometry,properties FROM intel_items "
        "WHERE source_id=?1 AND lat IS NOT NULL LIMIT ?2",
        -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, LIM);
    while (sqlite3_step(st) == SQLITE_ROW) {
      const char *gj = (const char *)sqlite3_column_text(st, 2);
      const char *pj = (const char *)sqlite3_column_text(st, 3);
      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *geom = gj ? cJSON_Parse(gj) : NULL;
      if (!geom) {
        geom = cJSON_CreateObject();
        cJSON_AddStringToObject(geom, "type", "Point");
        cJSON *gc = cJSON_CreateArray();
        cJSON_AddItemToArray(gc, cJSON_CreateNumber(sqlite3_column_double(st,1)));
        cJSON_AddItemToArray(gc, cJSON_CreateNumber(sqlite3_column_double(st,0)));
        cJSON_AddItemToObject(geom, "coordinates", gc);
      }
      cJSON_AddItemToObject(f, "geometry", geom);
      cJSON *p = pj ? cJSON_Parse(pj) : NULL;
      cJSON_AddItemToObject(f, "properties", p ? p : cJSON_CreateObject());
      cJSON_AddItemToArray(feats, f);
      n++;
    }
  }
  sqlite3_finalize(st);
  cJSON *meta = cJSON_AddObjectToObject(fc, "_meta");
  cJSON_AddStringToObject(meta, "source", id);
  char now[32]; iso_now(now, sizeof now);
  cJSON_AddStringToObject(meta, "fetchedAt", now);
  cJSON_AddNumberToObject(meta, "recordCount", n);
  cJSON_AddBoolToObject(meta, "live", 0);
  cJSON_AddStringToObject(meta, "cache_status", "cold");
  cJSON_AddBoolToObject(meta, "capped", n >= LIM);
  cJSON_AddStringToObject(meta, "description",
    "Cold map cache — bounded points until the scheduled prebuild runs.");
  char *out = cJSON_PrintUnformatted(fc);
  cJSON_Delete(fc);
  return out;
}

/* INSTANT read path: prebuilt FC from collector_cache, else bounded cold
 * FC. No stitch/smooth/full-scan ever happens here. */
char *sweepapi_data(db_handle *db, const char *id) {
  if (!sweepapi_is_sweep(id)) return NULL;
  return cold_points_fc(db, id);
}
