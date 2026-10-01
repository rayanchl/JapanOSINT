/* core/station_footprints.c — faithful port of
 * server/src/utils/stationFootprintsStore.js (170L).
 *
 * Mirrors: computeBbox (outer-ring min/max, finite-only), upsertFootprint
 * (footprint_id from properties; INSERT … ON CONFLICT(footprint_id) DO UPDATE
 * preserving first_seen_at, refreshing geometry/bbox/name/source +
 * last_seen_at), upsertFootprintsTx / linkClustersToFootprintsTx (single
 * SQLite tx each), getAllFootprints (rowToFeature shaping + geometry-null
 * filter, LEFT JOIN station_clusters for mode_set) and footprintCount. */

#include "station_footprints.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* safeJson(text, fallback): JSON.parse or fallback. cJSON parse+print
 * round-trips identically (same key order/formatting as JSON.stringify —
 * the convention already relied on across the core sources). Returns NULL on a
 * failed/empty parse so the caller can apply its own fallback. */
static cJSON *parse_or_null(const char *txt) {
  if (txt && *txt) { cJSON *j = cJSON_Parse(txt); if (j) return j; }
  return NULL;
}

static const char *col_text(sqlite3_stmt *s, int i) {
  return (sqlite3_column_type(s, i) == SQLITE_NULL)
           ? NULL : (const char *)sqlite3_column_text(s, i);
}

/* p.<key> || null  — JS truthiness: a present non-empty string passes; a
 * missing key, JSON null, or "" becomes null. (These props are always
 * strings upstream.) */
static const char *prop_str_or_null(cJSON *props, const char *key) {
  cJSON *v = cJSON_GetObjectItem(props, key);
  if (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
    return v->valuestring;
  return NULL;
}

/* computeBbox(polygon): outer ring = coordinates[0]; scan [lon,lat] pairs,
 * skipping any non-finite component (JS Number.isFinite guard). Returns 0
 * and fills out-params on success; non-zero if the ring is missing/empty or
 * yields no finite vertex (== JS returning null). */
static int compute_bbox(cJSON *geometry, double *mnLon, double *mnLat,
                        double *mxLon, double *mxLat) {
  if (!geometry) return 1;
  cJSON *coords = cJSON_GetObjectItem(geometry, "coordinates");
  if (!coords || !cJSON_IsArray(coords)) return 1;
  cJSON *ring = cJSON_GetArrayItem(coords, 0);
  if (!ring || !cJSON_IsArray(ring) || cJSON_GetArraySize(ring) == 0) return 1;

  double minLon = INFINITY, minLat = INFINITY;
  double maxLon = -INFINITY, maxLat = -INFINITY;
  int any = 0;
  cJSON *pt;
  cJSON_ArrayForEach(pt, ring) {
    cJSON *clon = cJSON_GetArrayItem(pt, 0);
    cJSON *clat = cJSON_GetArrayItem(pt, 1);
    if (!clon || !clat || !cJSON_IsNumber(clon) || !cJSON_IsNumber(clat))
      continue;
    double lon = clon->valuedouble, lat = clat->valuedouble;
    if (!isfinite(lon) || !isfinite(lat)) continue;
    if (lon < minLon) minLon = lon;
    if (lon > maxLon) maxLon = lon;
    if (lat < minLat) minLat = lat;
    if (lat > maxLat) maxLat = lat;
    any = 1;
  }
  if (!any || !isfinite(minLon)) return 1;
  *mnLon = minLon; *mnLat = minLat; *mxLon = maxLon; *mxLat = maxLat;
  return 0;
}

/* upsertFootprint(feature) for one feature, on an already-open transaction.
 * Returns 1 if a row was written, 0 if the feature was skipped (no
 * footprint_id / no usable bbox — == JS null), -1 on a DB error.
 *
 * Single INSERT … ON CONFLICT(footprint_id) DO UPDATE: on a fresh row the
 * table defaults stamp first_seen_at/last_seen_at; on conflict first_seen_at
 * is left untouched and last_seen_at = datetime('now'), exactly as the JS
 * SELECT-then-stmtUpdate path (which never rewrites first_seen_at). */
static int upsert_one(sqlite3 *h, cJSON *feature) {
  if (!feature) return 0;
  cJSON *props = cJSON_GetObjectItem(feature, "properties");
  cJSON *id = props ? cJSON_GetObjectItem(props, "footprint_id") : NULL;

  /* id must be truthy. Strings are the upstream norm; tolerate a number the
   * way better-sqlite3 would bind a JS number param. */
  char idbuf[64];
  const char *idstr = NULL;
  if (id && cJSON_IsString(id) && id->valuestring && id->valuestring[0]) {
    idstr = id->valuestring;
  } else if (id && cJSON_IsNumber(id) && id->valuedouble != 0.0) {
    double d = id->valuedouble;
    if (d == (double)(long long)d)
      snprintf(idbuf, sizeof idbuf, "%lld", (long long)d);
    else
      snprintf(idbuf, sizeof idbuf, "%g", d);
    idstr = idbuf;
  }
  if (!idstr) return 0;

  double mnLon, mnLat, mxLon, mxLat;
  cJSON *geometry = cJSON_GetObjectItem(feature, "geometry");
  if (compute_bbox(geometry, &mnLon, &mnLat, &mxLon, &mxLat) != 0) return 0;

  char *geomstr = cJSON_PrintUnformatted(geometry); /* JSON.stringify(geom) */
  if (!geomstr) return -1;

  static const char *Q =
    "INSERT INTO station_footprints ("
    "footprint_id,cluster_uid,name,name_ja,geometry,"
    "bbox_min_lat,bbox_min_lon,bbox_max_lat,bbox_max_lon,source) "
    "VALUES (?1,NULL,?2,?3,?4,?5,?6,?7,?8,?9) "
    "ON CONFLICT(footprint_id) DO UPDATE SET "
    "name=excluded.name,"
    "name_ja=excluded.name_ja,"
    "geometry=excluded.geometry,"
    "bbox_min_lat=excluded.bbox_min_lat,"
    "bbox_min_lon=excluded.bbox_min_lon,"
    "bbox_max_lat=excluded.bbox_max_lat,"
    "bbox_max_lon=excluded.bbox_max_lon,"
    "source=excluded.source,"
    "last_seen_at=datetime('now')";

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, Q, -1, &s, NULL) != SQLITE_OK) {
    free(geomstr);
    return -1;
  }
  const char *nm  = prop_str_or_null(props, "name");
  const char *nmj = prop_str_or_null(props, "name_ja");
  const char *src = prop_str_or_null(props, "source");

  sqlite3_bind_text(s, 1, idstr, -1, SQLITE_TRANSIENT);
  if (nm)  sqlite3_bind_text(s, 2, nm,  -1, SQLITE_TRANSIENT);
  else     sqlite3_bind_null(s, 2);
  if (nmj) sqlite3_bind_text(s, 3, nmj, -1, SQLITE_TRANSIENT);
  else     sqlite3_bind_null(s, 3);
  sqlite3_bind_text(s, 4, geomstr, -1, SQLITE_TRANSIENT);
  sqlite3_bind_double(s, 5, mnLat);
  sqlite3_bind_double(s, 6, mnLon);
  sqlite3_bind_double(s, 7, mxLat);
  sqlite3_bind_double(s, 8, mxLon);
  if (src) sqlite3_bind_text(s, 9, src, -1, SQLITE_TRANSIENT);
  else     sqlite3_bind_null(s, 9);

  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  free(geomstr);
  if (rc != SQLITE_DONE) return -1;
  return 1;
}

int station_footprints_upsert_tx(db_handle *db, cJSON *features) {
  if (!db || !db->h) return -1;
  if (!features || !cJSON_IsArray(features)) {
    /* JS would iterate `features` (a non-array throws); empty array -> 0.
     * Treat a missing/non-array input as an empty run. */
    return 0;
  }
  sqlite3 *h = db->h;
  if (sqlite3_exec(h, "BEGIN", NULL, NULL, NULL) != SQLITE_OK) return -1;

  int written = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, features) {
    int r = upsert_one(h, f);
    if (r < 0) {
      sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
      return -1;
    }
    written += r;            /* +1 only when a row was actually upserted */
  }

  if (sqlite3_exec(h, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
    return -1;
  }
  return written;
}

/* number-or-skip: JS reads c.lat / c.lon off plain objects. A non-finite or
 * absent coord can't satisfy the bbox predicate, so skip that cluster
 * (it would simply match nothing). */
static int num_field(cJSON *obj, const char *key, double *out) {
  cJSON *v = cJSON_GetObjectItem(obj, key);
  if (!v || !cJSON_IsNumber(v) || !isfinite(v->valuedouble)) return 0;
  *out = v->valuedouble;
  return 1;
}

int station_footprints_link_clusters_tx(db_handle *db, cJSON *clusters) {
  if (!db || !db->h) return -1;
  sqlite3 *h = db->h;
  if (sqlite3_exec(h, "BEGIN", NULL, NULL, NULL) != SQLITE_OK) return -1;

  /* stmtClearClusterLinks.run() */
  if (sqlite3_exec(h, "UPDATE station_footprints SET cluster_uid = NULL",
                   NULL, NULL, NULL) != SQLITE_OK) {
    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
    return -1;
  }

  int linked = 0;
  if (clusters && cJSON_IsArray(clusters) &&
      cJSON_GetArraySize(clusters) > 0) {
    /* A footprint's bbox can contain several cluster points (adjacent or
     * stacked stations), and the old code issued an unconditional UPDATE per
     * candidate inside the per-cluster loop — so the footprint kept whichever
     * cluster happened to be processed LAST, an arbitrary choice driven by
     * array order. Stage the candidates instead and keep the NEAREST one, which
     * is deterministic and is what the link is supposed to mean.
     *
     * Still a bbox test, not point-in-polygon: the box comes from the outer
     * ring only, so a concave footprint can admit a point that is outside the
     * actual geometry. Nearest-wins bounds the damage; a real ring test is the
     * follow-up. */
    sqlite3_exec(h, "CREATE TEMP TABLE IF NOT EXISTS fp_link("
                    "footprint_id TEXT PRIMARY KEY, cluster_uid TEXT, d REAL)",
                 NULL, NULL, NULL);
    sqlite3_exec(h, "DELETE FROM fp_link", NULL, NULL, NULL);

    /* stmtFootprintsContaining: bind(lon,lat); also returns the box centre so
     * the caller can rank candidates by distance. */
    static const char *QF =
      "SELECT footprint_id,(bbox_min_lon+bbox_max_lon)/2.0,"
      "       (bbox_min_lat+bbox_max_lat)/2.0 FROM station_footprints "
      "WHERE bbox_min_lon <= ?1 AND bbox_max_lon >= ?1 "
      "  AND bbox_min_lat <= ?2 AND bbox_max_lat >= ?2";
    static const char *QS =
      "INSERT INTO fp_link(footprint_id,cluster_uid,d) VALUES(?1,?2,?3) "
      "ON CONFLICT(footprint_id) DO UPDATE SET "
      "cluster_uid=excluded.cluster_uid, d=excluded.d WHERE excluded.d < fp_link.d";
    sqlite3_stmt *sf = NULL, *ss = NULL;
    if (sqlite3_prepare_v2(h, QF, -1, &sf, NULL) != SQLITE_OK ||
        sqlite3_prepare_v2(h, QS, -1, &ss, NULL) != SQLITE_OK) {
      if (sf) sqlite3_finalize(sf);
      if (ss) sqlite3_finalize(ss);
      sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
      return -1;
    }

    cJSON *c;
    cJSON_ArrayForEach(c, clusters) {
      double lat, lon;
      if (!num_field(c, "lat", &lat) || !num_field(c, "lon", &lon))
        continue;
      cJSON *cu = cJSON_GetObjectItem(c, "cluster_uid");
      const char *cluid =
        (cu && cJSON_IsString(cu)) ? cu->valuestring : NULL;

      sqlite3_reset(sf);
      sqlite3_clear_bindings(sf);
      sqlite3_bind_double(sf, 1, lon);
      sqlite3_bind_double(sf, 2, lat);
      while (sqlite3_step(sf) == SQLITE_ROW) {
        const char *fid = (const char *)sqlite3_column_text(sf, 0);
        double flon = sqlite3_column_double(sf, 1);
        double flat = sqlite3_column_double(sf, 2);
        /* Squared planar distance with a cos(lat) longitude correction — only
         * used for ranking, so no need for a true geodesic. */
        double coslat = cos(lat * 3.14159265358979323846 / 180.0);
        double dx = (flon - lon) * coslat, dy = flat - lat;
        double d2 = dx * dx + dy * dy;

        sqlite3_reset(ss);
        sqlite3_clear_bindings(ss);
        sqlite3_bind_text(ss, 1, fid, -1, SQLITE_TRANSIENT);
        if (cluid) sqlite3_bind_text(ss, 2, cluid, -1, SQLITE_TRANSIENT);
        else       sqlite3_bind_null(ss, 2);
        sqlite3_bind_double(ss, 3, d2);
        if (sqlite3_step(ss) != SQLITE_DONE) {
          sqlite3_finalize(sf);
          sqlite3_finalize(ss);
          sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
          return -1;
        }
      }
    }
    sqlite3_finalize(sf);
    sqlite3_finalize(ss);

    /* Apply the winners in one pass. */
    sqlite3_exec(h,
      "UPDATE station_footprints SET cluster_uid = "
      "  (SELECT cluster_uid FROM fp_link WHERE fp_link.footprint_id = "
      "   station_footprints.footprint_id) "
      "WHERE footprint_id IN (SELECT footprint_id FROM fp_link)",
      NULL, NULL, NULL);
    linked = sqlite3_changes(h);
    sqlite3_exec(h, "DROP TABLE IF EXISTS fp_link", NULL, NULL, NULL);
  }

  if (sqlite3_exec(h, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
    return -1;
  }
  return linked;
}

/* rowToFeature(row): geometry = safeJson(row.geometry,null) — a row whose
 * geometry fails to parse is dropped (== .filter(f=>f.geometry)).
 * properties = {footprint_id,cluster_uid,name,name_ja,mode_set} with
 * mode_set = safeJson(row.mode_set,[]). Column order:
 * 0 footprint_id, 1 cluster_uid, 2 name, 3 name_ja, 4 geometry, 5 mode_set */
static cJSON *row_to_feature(sqlite3_stmt *s) {
  cJSON *geom = parse_or_null(col_text(s, 4));
  if (!geom) return NULL;                 /* .filter(f => f.geometry) */

  cJSON *feat = cJSON_CreateObject();
  cJSON_AddStringToObject(feat, "type", "Feature");
  cJSON_AddItemToObject(feat, "geometry", geom);

  cJSON *p = cJSON_CreateObject();
  const char *fid  = col_text(s, 0);
  const char *cuid = col_text(s, 1);
  const char *nm   = col_text(s, 2);
  const char *nmj  = col_text(s, 3);

  cJSON_AddItemToObject(p, "footprint_id",
    fid ? cJSON_CreateString(fid) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "cluster_uid",
    cuid ? cJSON_CreateString(cuid) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "name",
    nm ? cJSON_CreateString(nm) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "name_ja",
    nmj ? cJSON_CreateString(nmj) : cJSON_CreateNull());

  cJSON *modeset = parse_or_null(col_text(s, 5));
  if (!modeset) modeset = cJSON_CreateArray();   /* safeJson(.,[]) */
  cJSON_AddItemToObject(p, "mode_set", modeset);

  cJSON_AddItemToObject(feat, "properties", p);
  return feat;
}

char *station_footprints_fc_json(db_handle *db) {
  if (!db || !db->h) return NULL;

  /* stmtAll: LEFT JOIN station_clusters for mode_set. */
  static const char *Q =
    "SELECT f.footprint_id, f.cluster_uid, f.name, f.name_ja, f.geometry, "
    "       c.mode_set "
    "FROM station_footprints f "
    "LEFT JOIN station_clusters c ON c.cluster_uid = f.cluster_uid";
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, Q, -1, &s, NULL) != SQLITE_OK) return NULL;

  cJSON *features = cJSON_CreateArray();
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *f = row_to_feature(s);
    if (f) cJSON_AddItemToArray(features, f);   /* null geom -> dropped */
  }
  sqlite3_finalize(s);

  /* Bare FeatureCollection; the _meta envelope is the read endpoint's job
   * (the JS store returns just the Feature[]). Empty table -> features:[]. */
  cJSON *fc = cJSON_CreateObject();
  cJSON_AddStringToObject(fc, "type", "FeatureCollection");
  cJSON_AddItemToObject(fc, "features", features);
  char *js = cJSON_PrintUnformatted(fc);
  cJSON_Delete(fc);
  return js;
}

int station_footprints_count(db_handle *db) {
  if (!db || !db->h) return -1;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT COUNT(*) FROM station_footprints", -1, &s, NULL)
      != SQLITE_OK)
    return -1;
  int n = -1;
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
  sqlite3_finalize(s);
  return n;
}
