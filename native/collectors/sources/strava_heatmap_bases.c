/* collectors/cyber/sources/strava_heatmap_bases.c
 * Port of server/src/collectors/stravaHeatmapBases.js — live z=12 Strava
 * heatmap tile probe around a curated JP installation set; one GeoJSON
 * Feature per base with a tile-size activity heuristic. STRAVA_BASES env
 * override + _meta envelope not ported. */
#include "source.h"
#include "lib/geojson.h"
#include "core/httpclient.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TIMEOUT_MS 12000
#define ZOOM 12

struct base { const char *name, *branch; double lat, lon; };
static const struct base BASES[] = {
  { "Yokota Air Base",            "USAF",  35.7486, 139.3486 },
  { "Misawa Air Base",            "USAF",  40.7028, 141.3681 },
  { "Kadena Air Base",            "USAF",  26.3556, 127.7681 },
  { "MCAS Iwakuni",               "USMC",  34.1442, 132.2356 },
  { "MCAS Futenma",               "USMC",  26.2722, 127.7558 },
  { "Camp Schwab",                "USMC",  26.5239, 128.0556 },
  { "Fleet Activities Yokosuka",  "USN",   35.2917, 139.6611 },
  { "NAF Atsugi",                 "USN",   35.4544, 139.4500 },
  { "Camp Zama",                  "USA",   35.5111, 139.4017 },
  { "JASDF Hyakuri",              "JASDF", 36.1814, 140.4147 },
  { "JASDF Komaki",               "JASDF", 35.2750, 136.9250 },
  { "JGSDF Camp Asaka",           "JGSDF", 35.7903, 139.6094 },
  { "JGSDF Ichigaya HQ",          "JGSDF", 35.6906, 139.7300 },
  { "JMSDF Yokosuka",             "JMSDF", 35.2861, 139.6750 },
};
#define NB ((int)(sizeof(BASES)/sizeof(BASES[0])))

static int lon_to_tx(double lon, int z) {
  return (int)floor(((lon + 180.0) / 360.0) * pow(2, z));
}
static int lat_to_ty(double lat, int z) {
  double r = (lat * M_PI) / 180.0;
  return (int)floor((1 - log(tan(r) + 1 / cos(r)) / M_PI) / 2 * pow(2, z));
}

/* JS: p.name.replace(/\s+/g,'_') (collapse runs of whitespace to one '_'). */
static void slug(const char *in, char *out, size_t n) {
  size_t o = 0; int ws = 0;
  for (size_t i = 0; in[i] && o + 1 < n; i++) {
    unsigned char c = (unsigned char)in[i];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
      ws = 1;
    } else {
      if (ws && o + 1 < n) { out[o++] = '_'; ws = 0; }
      out[o++] = (char)c;
    }
  }
  out[o] = 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "accept: image/png",
                         "user-agent: Mozilla/5.0 JapanOSINT", NULL };
  cJSON *features = cJSON_CreateArray();

  for (int i = 0; i < NB; i++) {
    const struct base *b = &BASES[i];
    int x = lon_to_tx(b->lon, ZOOM);
    int y = lat_to_ty(b->lat, ZOOM);
    char url[160];
    snprintf(url, sizeof url,
      "https://heatmap-external-a.strava.com/tiles/all/hot/%d/%d/%d.png",
      ZOOM, x, y);

    http_response resp = {0};
    int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0,
                          TIMEOUT_MS, 0, &resp);

    int ok = 0;            /* res.ok */
    int have_bytes = 0;    /* tile_bytes !== null */
    long bytes = 0;
    int have_http = 0;     /* http (res.status) only on !res.ok branch */
    long httpst = 0;
    int have_err = (rc != 0);  /* catch branch → err.message present */

    if (rc == 0) {
      if (resp.status >= 200 && resp.status < 300) {
        ok = 1;
        have_bytes = 1;
        bytes = (long)resp.body_len;
      } else {
        ok = 0;
        have_http = 1;
        httpst = resp.status;
      }
    }
    http_response_free(&resp);

    /* The base coordinate is an INPUT to this probe (it is what picked the
     * tile), never something the probe measured. On a success we did observe
     * that tile, so pinning the row at the installation is fair. On a failure
     * we observed nothing at all — and this row was built and appended outside
     * every success branch, so a Strava outage or a blanket 403 still produced
     * fourteen map pins at fourteen named military installations, each one
     * indistinguishable on the map from a live observation. A failed probe
     * carries `"geometry": null` (lib/geojson.c treats that as absent, so no
     * lat/lon reaches intel_items); the row survives as the error report it
     * is, with the tile coordinates still in properties. */
    cJSON *f;
    if (have_bytes) {
      f = gj_point_feature(b->lon, b->lat);
    } else {
      f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON_AddNullToObject(f, "geometry");
    }

    cJSON *p = cJSON_CreateObject();          /* EXACT JS key order */
    char sl[96]; slug(b->name, sl, sizeof sl);
    char idbuf[112]; snprintf(idbuf, sizeof idbuf, "STRAVA_%s", sl);
    cJSON_AddStringToObject(p, "id", idbuf);
    cJSON_AddNumberToObject(p, "idx", i);
    cJSON_AddStringToObject(p, "name", b->name);
    cJSON_AddStringToObject(p, "branch", b->branch);
    cJSON_AddNumberToObject(p, "tile_z", ZOOM);
    cJSON_AddNumberToObject(p, "tile_x", x);
    cJSON_AddNumberToObject(p, "tile_y", y);
    cJSON_AddStringToObject(p, "tile_url", url);
    cJSON_AddItemToObject(p, "tile_bytes",
      have_bytes ? cJSON_CreateNumber((double)bytes) : cJSON_CreateNull());
    /* `activity_detected` used to be `(body_len > 1800)`. 1800 was never
     * calibrated against anything: a heatmap PNG's size tracks TILE COMPLEXITY
     * — coastline, road mesh, palette, the encoder's settings — not human
     * activity, so a coastal base reads "active" while an inland one of equal
     * traffic reads "quiet", and one CDN re-encode flips all fourteen at once.
     * Publishing that as a detection about a named military installation is a
     * measurement nobody took. tile_bytes above IS the real observation and is
     * kept; the byte comparison is kept too, but named for what it compares. */
    cJSON_AddItemToObject(p, "activity_detected", cJSON_CreateNull());
    cJSON_AddStringToObject(p, "activity_detected_basis",
      "not measured: this probe reads a heatmap tile's byte length, which does "
      "not distinguish activity from tile complexity");
    cJSON_AddItemToObject(p, "tile_bytes_over_1800",
      have_bytes ? cJSON_CreateBool(bytes > 1800) : cJSON_CreateNull());
    cJSON_AddBoolToObject(p, "ok", ok);
    /* error: err?.message || null — only set in catch (transport hard fail) */
    cJSON_AddItemToObject(p, "error",
      have_err ? cJSON_CreateString("fetch failed") : cJSON_CreateNull());
    /* http: p.http || null — falsy-0 → null; only set on !res.ok */
    cJSON_AddItemToObject(p, "http",
      (have_http && httpst) ? cJSON_CreateNumber((double)httpst)
                            : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "source", "strava_heatmap_tile_probe");
    cJSON_AddItemToObject(f, "properties", p);

    cJSON_AddItemToArray(features, f);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[strava-heatmap-bases] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def strava_heatmap_bases_def = {
  .id = "strava-heatmap-bases", .collector = "cyber",
  .name = "Strava Heatmap Probes (Bases)",
  .name_ja = "Strava \xe3\x83\x92\xe3\x83\xbc\xe3\x83\x88\xe3\x83\x9e\xe3\x83\x83\xe3\x83\x97 (\xe5\x9f\xba\xe5\x9c\xb0)",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(strava_heatmap_bases_def)
