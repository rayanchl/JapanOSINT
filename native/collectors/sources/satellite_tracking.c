/* collectors/satellite/sources/satellite_tracking.c
 * Port of server/src/collectors/satelliteTracking.js. CelesTrak TLE groups
 * → SGP4 propagation (lib/sgp4) at "now" → geodetic; keep objects inside
 * JAPAN_BBOX. The SEED_ISS fallback + _meta envelope are dropped per RULE 8
 * (0 rows when CelesTrak is unreachable / nothing over Japan, same contract
 * as every other live port). Deep-space objects are skipped (sgp4 near-Earth
 * scope) — documented post-parity vs satellite.js's bundled SDP4. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/sgp4.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

static const struct { const char *group, *category; } GROUPS[] = {
  { "active", "active" }, { "debris", "debris" },
  { "cubesat", "cubesat" }, { "stations", "station" },
};
/* JAPAN_BBOX = [W,S,E,N] */
#define BW 122.0
#define BS 24.0
#define BE 154.0
#define BN 46.0

static double round_to(double v, double q) { return round(v * q) / q; }

/* trim a line in place (JS .trim()) */
static char *trim(char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
  size_t L = strlen(s);
  while (L && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='\r'||s[L-1]=='\n')) s[--L]=0;
  return s;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* jd(now): Unix epoch = JD 2440587.5 (UT1≈UTC ok for gstime/tsince). */
  struct timeval tv; gettimeofday(&tv, NULL);
  double jd_now = 2440587.5 + (tv.tv_sec + tv.tv_usec / 1e6) / 86400.0;
  double gmst = sgp4_gstime(jd_now);

  cJSON *features = cJSON_CreateArray();

  for (size_t gi = 0; gi < sizeof GROUPS / sizeof GROUPS[0]; gi++) {
    char url[160];
    snprintf(url, sizeof url,
      "https://celestrak.org/NORAD/elements/gp.php?GROUP=%s&FORMAT=tle",
      GROUPS[gi].group);
    char *body = feed_get_text(ctx->http, url, 15000);
    if (!body) continue;

    /* parseTleBlock: trimmed non-empty lines, name/l1/l2 triples. */
    char **ln = NULL; int nl = 0, cap = 0;
    for (char *p = strtok(body, "\n"); p; p = strtok(NULL, "\n")) {
      char *t = trim(p);
      if (!*t) continue;
      if (nl == cap) { cap = cap ? cap * 2 : 256; ln = realloc(ln, cap * sizeof *ln); }
      ln[nl++] = t;
    }
    for (int i = 0; i + 2 < nl; i += 3) {
      const char *name = ln[i], *l1 = ln[i+1], *l2 = ln[i+2];
      if (strncmp(l1, "1 ", 2) || strncmp(l2, "2 ", 2)) continue;
      char nb[8] = {0};
      memcpy(nb, l1 + 2, 5);
      long norad = strtol(nb, NULL, 10);
      if (norad <= 0) continue;

      sgp4_rec r;
      if (sgp4_twoline(l1, l2, &r) != 0 || r.deep) continue;
      double tsince = (jd_now - r.jdsatepoch) * 1440.0;
      double pos[3], vel[3];
      if (sgp4_propagate(&r, tsince, pos, vel) != 0) continue;
      double lon, lat, alt;
      sgp4_geodetic(pos, gmst, &lon, &lat, &alt);
      if (!(lon >= BW && lon <= BE && lat >= BS && lat <= BN)) continue;

      double vmag = sqrt(vel[0]*vel[0] + vel[1]*vel[1] + vel[2]*vel[2]);
      double inc_deg = r.inclo * 180.0 / 3.14159265358979323846;

      cJSON *f = gj_point_feature(lon, lat);

      cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
      char idb[24]; snprintf(idb, sizeof idb, "SAT_%ld", norad);
      cJSON_AddStringToObject(p, "id", idb);
      cJSON_AddNumberToObject(p, "norad_id", (double)norad);
      cJSON_AddStringToObject(p, "name", name);
      cJSON_AddItemToObject(p, "country", cJSON_CreateNull());
      cJSON_AddStringToObject(p, "category", GROUPS[gi].category);
      cJSON_AddNumberToObject(p, "altitude_km", round_to(alt, 10.0));
      cJSON_AddNumberToObject(p, "velocity_kms", round_to(vmag, 1000.0));
      cJSON_AddNumberToObject(p, "inclination_deg", round_to(inc_deg, 100.0));
      cJSON_AddItemToObject(p, "next_pass_utc", cJSON_CreateNull());
      cJSON_AddStringToObject(p, "tle_line1", l1);
      cJSON_AddStringToObject(p, "tle_line2", l2);
      cJSON_AddStringToObject(p, "source", "celestrak");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
    free(ln);
    free(body);
  }

  int n = geojson_emit_features(sink, "satellite-tracking", features);
  cJSON_Delete(features);
  fprintf(stderr, "[satellite-tracking] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def satellite_tracking_def = {
  .id = "satellite-tracking", .collector = "satellite",
  .name = "Live Satellite Positions", .name_ja = "人工衛星リアルタイム位置",
   .update_interval_sec = 60, .run = run };
REGISTER_SOURCE(satellite_tracking_def)
