/* camera_geocode_pod.c — upgrade approximate camera coordinates to real ones.
 *
 * Port of the retired Node `utils/cameraGeocode.js` + `cameraStore.applyGeocodeOk/
 * applyGeocodeFail`, which the C port dropped entirely. Without it every camera
 * whose aggregator page carries no coordinate is stuck on a prefecture or city
 * centroid — and because the C collectors correctly refuse to fabricate a
 * position, cameras Node would have PLACED are instead skipped or stacked. The
 * measured effect was ~45% of camera rows sitting on ~20 administrative points.
 *
 * What Node did, and what this reproduces:
 *   - a provider cascade with a persistent cache and a JP bounding-box reject
 *   - name cleaning before the lookup ("View from ...", "[LIVE]", "webcam",
 *     "Japan/日本", prefer the longest dash-separated segment)
 *   - write back the resolved point, or mark the attempt so a miss is not
 *     retried forever (Node's applyGeocodeFail)
 *
 * Differences from Node, deliberate:
 *   - the cascade is core/geoproxyapi.h's existing Nominatim -> Photon -> GSI
 *     chain (24h cached) rather than a second Overpass/GSI/Nominatim stack.
 *     One geocoder in the process, already written and already cached.
 *   - a per-run budget instead of a 4-way worker pool: these are third-party
 *     nominatim-class endpoints and the scheduler is shared. Budget * cadence
 *     is the throughput knob (JO_CAM_GEOCODE_BUDGET, default 25 per 5 min).
 *
 * Runs as a `_maint` pod so scheduler.c skips fetch_log/anomaly for it, exactly
 * like camera_stills.c. */
#include "../../source.h"
#include "../../core/camera_store.h"
#include "../../core/geoproxyapi.h"
#include "../../core/db.h"
#include "../../core/intel.h"   /* intel_fts_remirror: properties is indexed */
#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Node's JP_BBOX from cameraGeocode.js — a hit outside it is a wrong match on a
 * same-named place abroad, which is the common failure for short venue names. */
#define JP_MIN_LAT 20.0
#define JP_MAX_LAT 50.0
#define JP_MIN_LON 120.0
#define JP_MAX_LON 155.0

static int env_int(const char *k, int dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  int n = atoi(v);
  return n > 0 ? n : dflt;
}

/* Node cleanName(): drop framing words that describe the shot rather than the
 * place, then prefer the longest dash-separated segment (aggregator titles are
 * usually "<venue> - <city> - <site>"). ASCII-case-insensitive; multi-byte text
 * passes through untouched. */
static void clean_name(const char *in, char *out, size_t cap) {
  static const char *DROP[] = {
    "view from", "overlooking", "live view", "live cam", "livecam",
    "webcam", "web cam", "[live]", "(live)", "live stream", "streaming",
    "japan", "\xe6\x97\xa5\xe6\x9c\xac", NULL
  };
  char buf[512];
  snprintf(buf, sizeof buf, "%s", in ? in : "");

  /* prefer the longest ' - ' segment before stripping words */
  const char *best = buf;
  size_t bestlen = strlen(buf);
  {
    const char *p = buf, *seg = buf;
    size_t seglen;
    while ((p = strstr(seg, " - ")) != NULL) {
      seglen = (size_t)(p - seg);
      if (seglen > 2 && seglen > bestlen) { best = seg; bestlen = seglen; }
      seg = p + 3;
    }
    seglen = strlen(seg);
    if (seglen > 2 && seglen > bestlen) { best = seg; bestlen = seglen; }
  }
  char seg[512];
  if (bestlen >= sizeof seg) bestlen = sizeof seg - 1;
  memcpy(seg, best, bestlen); seg[bestlen] = 0;

  /* lowercase copy for matching, but emit from the original so Japanese and
   * proper-noun casing survive */
  char low[512];
  size_t n = strlen(seg);
  for (size_t i = 0; i <= n && i < sizeof low; i++)
    low[i] = (seg[i] >= 'A' && seg[i] <= 'Z') ? (char)(seg[i] + 32) : seg[i];

  char work[512];
  snprintf(work, sizeof work, "%s", seg);
  for (int d = 0; DROP[d]; d++) {
    size_t dl = strlen(DROP[d]);
    char *hit;
    while ((hit = strstr(low, DROP[d])) != NULL) {
      size_t off = (size_t)(hit - low);
      memmove(work + off, work + off + dl, strlen(work + off + dl) + 1);
      memmove(low + off, low + off + dl, strlen(low + off + dl) + 1);
    }
  }
  /* squeeze separators and trim */
  size_t o = 0;
  int sp = 1;
  for (size_t i = 0; work[i] && o + 1 < cap; i++) {
    unsigned char c = (unsigned char)work[i];
    if (c == ' ' || c == '\t' || c == '|' || c == ',' || c == '-') {
      if (!sp && o + 1 < cap) { out[o++] = ' '; sp = 1; }
    } else { out[o++] = (char)c; sp = 0; }
  }
  while (o > 0 && out[o - 1] == ' ') o--;
  out[o] = 0;
}

/* Great-circle km. Used for the sanity guard below, not for display. */
static double km_between(double la1, double lo1, double la2, double lo2) {
  const double R = 6371.0, D = 3.14159265358979323846 / 180.0;
  double dla = (la2 - la1) * D, dlo = (lo2 - lo1) * D;
  double a = sin(dla / 2) * sin(dla / 2) +
             cos(la1 * D) * cos(la2 * D) * sin(dlo / 2) * sin(dlo / 2);
  return 2 * R * atan2(sqrt(a), sqrt(1 - a));
}

/* First hit inside the JP box. Returns 1 and fills lat/lon/provider.
 *
 * `anchor` is the centroid the row already carries. A geocode that lands far
 * from the prefecture/city we independently matched is a wrong hit on a
 * same-named place, not a refinement — the first live run resolved
 * "Aeroport de Nagasaki" to 34.64,129.40 (the Korea Strait, ~215 km from the
 * airport) and would have moved the pin there. Rejecting those keeps the row on
 * an honest centroid instead of a confident wrong point, which for an OSINT map
 * is the difference between "approximate" and "false". */
static int first_jp_hit(const char *json, double *lat, double *lon,
                        char *prov, size_t pcap,
                        double anchor_lat, double anchor_lon, double max_km) {
  if (!json) return 0;
  cJSON *doc = cJSON_Parse(json);
  if (!doc) return 0;
  cJSON *res = cJSON_GetObjectItem(doc, "results");
  cJSON *pv = cJSON_GetObjectItem(doc, "provider");
  if (pv && cJSON_IsString(pv) && pv->valuestring) snprintf(prov, pcap, "%s", pv->valuestring);
  int ok = 0;
  cJSON *it;
  cJSON_ArrayForEach(it, res) {
    cJSON *la = cJSON_GetObjectItem(it, "lat"), *lo = cJSON_GetObjectItem(it, "lon");
    if (!la || !lo || !cJSON_IsNumber(la) || !cJSON_IsNumber(lo)) continue;
    double a = la->valuedouble, b = lo->valuedouble;
    if (!isfinite(a) || !isfinite(b)) continue;
    if (a < JP_MIN_LAT || a > JP_MAX_LAT || b < JP_MIN_LON || b > JP_MAX_LON) continue;
    if (max_km > 0 && isfinite(anchor_lat) && isfinite(anchor_lon) &&
        km_between(anchor_lat, anchor_lon, a, b) > max_km)
      continue;                    /* right name, wrong place */
    *lat = a; *lon = b; ok = 1;
    break;
  }
  cJSON_Delete(doc);
  return ok;
}

/* applyGeocodeOk / applyGeocodeFail — write the outcome back onto the row. */
static int apply_result(db_handle *db, const char *uid, const char *props_json,
                        int found, double lat, double lon, const char *prov,
                        const char *now_iso) {
  cJSON *p = cJSON_Parse(props_json ? props_json : "{}");
  if (!p) p = cJSON_CreateObject();

  cJSON_DeleteItemFromObject(p, "geo_geocode_attempted_at");
  cJSON_AddStringToObject(p, "geo_geocode_attempted_at", now_iso);

  if (found) {
    cJSON_DeleteItemFromObject(p, "geo_precision");
    cJSON_AddStringToObject(p, "geo_precision", "exact");
    cJSON_DeleteItemFromObject(p, "geo_provenance");
    char pv[96];
    snprintf(pv, sizeof pv, "geocoded:%s", (prov && *prov) ? prov : "unknown");
    cJSON_AddStringToObject(p, "geo_provenance", pv);
    cJSON_DeleteItemFromObject(p, "geo_uncertain");
    cJSON_AddNumberToObject(p, "geo_uncertain", 0);
  } else {
    cJSON_DeleteItemFromObject(p, "geo_uncertain");
    cJSON_AddNumberToObject(p, "geo_uncertain", 1);
  }
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);
  if (!pj) return -1;

  sqlite3_stmt *s;
  int rc;
  if (found) {
    char geom[128];
    snprintf(geom, sizeof geom,
             "{\"type\":\"Point\",\"coordinates\":[%.6f,%.6f]}", lon, lat);
    rc = sqlite3_prepare_v2(db->h,
        "UPDATE intel_items SET lat=?1, lon=?2, geometry=?3, geom_source='geocoded',"
        " properties=?4 WHERE uid=?5", -1, &s, NULL);
    if (rc == SQLITE_OK) {
      sqlite3_bind_double(s, 1, lat);
      sqlite3_bind_double(s, 2, lon);
      sqlite3_bind_text(s, 3, geom, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 4, pj, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 5, uid, -1, SQLITE_TRANSIENT);
    }
  } else {
    rc = sqlite3_prepare_v2(db->h,
        "UPDATE intel_items SET properties=?1 WHERE uid=?2", -1, &s, NULL);
    if (rc == SQLITE_OK) {
      sqlite3_bind_text(s, 1, pj, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, uid, -1, SQLITE_TRANSIENT);
    }
  }
  if (rc != SQLITE_OK) { free(pj); return -1; }
  rc = sqlite3_step(s);
  sqlite3_finalize(s);
  free(pj);
  if (rc != SQLITE_DONE) return -1;
  /* properties is mirrored into intel_items_fts.props, and this UPDATE goes
   * around intel.c's emit(). Without the re-mirror the address this pod just
   * resolved shows in the camera's detail pane and is permanently unfindable
   * by search — the row's own collector never re-emits it with the new text. */
  intel_fts_remirror(db, uid);
  return 0;
}

static int pod_run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  if (!ctx || !ctx->db || !ctx->db->h) return 0;
  int budget = env_int("JO_CAM_GEOCODE_BUDGET", 25);

  /* Candidates: camera rows whose position is an administrative approximation
   * and that we have not already tried. Ordered oldest-first so a large backlog
   * drains deterministically rather than re-picking the same rows. */
  /* How far a geocode may move a pin from the centroid we already matched.
   * A prefecture is ~100 km across, so 150 km accepts a genuine refinement
   * anywhere in the matched prefecture and rejects a cross-country mismatch. */
  int max_km = env_int("JO_CAM_GEOCODE_MAX_KM", 150);

  static const char *Q =
    "SELECT uid, title, properties, lat, lon FROM intel_items"
    " WHERE record_type='camera' AND uid LIKE 'camera-discovery|%'"
    "   AND json_extract(properties,'$.geo_precision') IN ('prefecture','city')"
    "   AND json_extract(properties,'$.geo_geocode_attempted_at') IS NULL"
    " ORDER BY fetched_at ASC LIMIT ?1";

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(ctx->db->h, Q, -1, &s, NULL) != SQLITE_OK) {
    fprintf(stderr, "[cam-geocode] candidate query failed: %s\n",
            sqlite3_errmsg(ctx->db->h));
    return 0;
  }
  sqlite3_bind_int(s, 1, budget);

  char (*uids)[512] = calloc((size_t)budget, sizeof *uids);
  char (*names)[512] = calloc((size_t)budget, sizeof *names);
  char **props = calloc((size_t)budget, sizeof *props);
  double *alat = calloc((size_t)budget, sizeof *alat);
  double *alon = calloc((size_t)budget, sizeof *alon);
  int n = 0;
  if (uids && names && props && alat && alon) {
    while (n < budget && sqlite3_step(s) == SQLITE_ROW) {
      const unsigned char *u = sqlite3_column_text(s, 0);
      const unsigned char *t = sqlite3_column_text(s, 1);
      const unsigned char *p = sqlite3_column_text(s, 2);
      if (!u) continue;
      snprintf(uids[n], sizeof uids[n], "%s", (const char *)u);
      snprintf(names[n], sizeof names[n], "%s", t ? (const char *)t : "");
      props[n] = p ? strdup((const char *)p) : strdup("{}");
      alat[n] = sqlite3_column_type(s, 3) == SQLITE_NULL
                  ? (0.0 / 0.0) : sqlite3_column_double(s, 3);
      alon[n] = sqlite3_column_type(s, 4) == SQLITE_NULL
                  ? (0.0 / 0.0) : sqlite3_column_double(s, 4);
      n++;
    }
  }
  sqlite3_finalize(s);

  char now[40];
  {
    time_t tt = time(NULL); struct tm g; gmtime_r(&tt, &g);
    strftime(now, sizeof now, "%Y-%m-%dT%H:%M:%SZ", &g);
  }

  int hit = 0, miss = 0;
  for (int i = 0; i < n; i++) {
    char q[512];
    clean_name(names[i], q, sizeof q);
    if (strlen(q) < 3) {                 /* nothing to look up */
      apply_result(ctx->db, uids[i], props[i], 0, 0, 0, NULL, now);
      miss++;
      continue;
    }
    char prov[64] = {0};
    double lat = 0, lon = 0;
    char *json = geoproxy_geocode_forward(q, NULL);
    int ok = first_jp_hit(json, &lat, &lon, prov, sizeof prov,
                          alat[i], alon[i], (double)max_km);
    free(json);
    if (ok) {
      if (apply_result(ctx->db, uids[i], props[i], 1, lat, lon, prov, now) == 0) hit++;
    } else {
      apply_result(ctx->db, uids[i], props[i], 0, 0, 0, NULL, now);
      miss++;
    }
    if (ctx->cancel && *ctx->cancel) break;
  }

  for (int i = 0; i < n; i++) free(props[i]);
  free(uids); free(names); free(props); free(alat); free(alon);

  fprintf(stderr, "[cam-geocode] candidates=%d resolved=%d unresolved=%d "
                  "(budget %d, max %d km from anchor)\n",
          n, hit, miss, budget, max_km);
  return 0;                       /* a pod with nothing to do is not an error */
}

static const source_def camera_geocode_def = {
  .id = "camera-geocode", .collector = "_maint",
  .name = "Camera geocoding (centroid upgrade)",
  .name_ja = "\xe3\x82\xab\xe3\x83\xa1\xe3\x83\xa9\xe4\xbd\x8d\xe7\xbd\xae\xe8\xa3\x9c\xe6\xad\xa3",
  .update_interval_sec = 300, .run = pod_run };
REGISTER_SOURCE(camera_geocode_def)
