/* BMKG Indonesia recent earthquakes (M5+ list with tsunami-potential verdict).
 *
 * Endpoint (keyless): https://data.bmkg.go.id/DataMKG/TEWS/gempaterkini.json
 * Emits: DateTime, Magnitude, Kedalaman (depth), Wilayah (region), Potensi
 *        (the official tsunami-potential assessment, which USGS does not
 *        carry) and the latitude/longitude decoded from `Coordinates`.
 * Licence: BMKG open data portal (data.bmkg.go.id), published for public and
 *          third-party redistribution; attribution to BMKG.
 *
 * GEOMETRY TRAP (from parse_notes): there is NO numeric lat/lon field.
 * `Coordinates` is a single STRING "-2.80,130.25" — LAT first, comma, LON —
 * and must be split and strtod'd. The sibling `Lintang`/`Bujur` fields are
 * human-readable ("2.80 LS" = 2.80 South, "130.25 BT" = 130.25 East) and
 * parsing those naively gives the WRONG HEMISPHERE silently, so they are
 * carried as text only and never used for the pin. Magnitude and Kedalaman are
 * strings too ("5.1", "10 km").
 *
 * The sibling /autogempa.json returns a SINGLE object plus a 'Dirasakan' felt
 * string and a shakemap filename — a different shape, deliberately not wired
 * into this parser.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

static const char *URL = "https://data.bmkg.go.id/DataMKG/TEWS/gempaterkini.json";

/* "-2.80,130.25" → lat, lon. Both halves must parse completely. */
static int split_coords(const char *s, double *lat, double *lon) {
  if (!s) return 0;
  const char *c = strchr(s, ',');
  if (!c) return 0;
  char head[64];
  size_t hl = (size_t)(c - s);
  if (hl == 0 || hl >= sizeof head) return 0;
  memcpy(head, s, hl);
  head[hl] = 0;
  if (!geoeo_strtod(head, lat)) return 0;
  if (!geoeo_strtod(c + 1, lon)) return 0;
  return geoeo_ll_ok(*lat, *lon);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 25000);
  if (!doc) {
    fprintf(stderr, "[bmkg-quakes] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = doc;
  if (!cJSON_IsArray(arr)) {
    cJSON *info = cJSON_GetObjectItem(doc, "Infogempa");
    arr = info ? cJSON_GetObjectItem(info, "gempa") : NULL;
  }
  if (!cJSON_IsArray(arr)) {
    fprintf(stderr, "[bmkg-quakes] no Infogempa.gempa array\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    const char *when = geoeo_str(e, "DateTime");
    const char *wil = geoeo_str(e, "Wilayah");
    const char *smag = geoeo_str(e, "Magnitude");
    const char *sdep = geoeo_str(e, "Kedalaman");
    const char *pot = geoeo_str(e, "Potensi");
    const char *coords = geoeo_str(e, "Coordinates");
    if (!when && !wil) continue;                 /* nothing identifying → skip */

    double lat = 0, lon = 0;
    int geo = split_coords(coords, &lat, &lon);

    double mag = 0;
    int has_mag = geoeo_strtod(smag, &mag);

    char title[384];
    if (has_mag)
      snprintf(title, sizeof title, "M%.1f — %s", mag, wil ? wil : "Indonesia");
    else
      snprintf(title, sizeof title, "BMKG gempa — %s", wil ? wil : "Indonesia");

    char summary[256];
    snprintf(summary, sizeof summary, "%s%s%s", sdep ? sdep : "",
             (sdep && pot) ? " · " : "", pot ? pot : "");

    cJSON *props = cJSON_CreateObject();
    geoeo_add_str(props, "datetime", when);
    geoeo_add_str(props, "wilayah", wil);
    geoeo_add_str(props, "magnitude", smag);
    geoeo_add_str(props, "kedalaman", sdep);
    geoeo_add_str(props, "potensi", pot);
    /* Kept verbatim as the human-readable strings they are — LS/LU and BT/BB
     * are hemisphere words, not signs. */
    geoeo_add_str(props, "lintang_text", geoeo_str(e, "Lintang"));
    geoeo_add_str(props, "bujur_text", geoeo_str(e, "Bujur"));
    geoeo_add_str(props, "coordinates_raw", coords);
    if (has_mag) cJSON_AddNumberToObject(props, "magnitude_num", mag);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    cJSON_AddStringToObject(props, "agency", "BMKG");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    char key[256];
    snprintf(key, sizeof key, "%s|%s", when ? when : "", coords ? coords : "");

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = summary;
    it.lang = "id";
    it.published_at = when;
    it.record_type = "earthquake";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"earthquake\",\"seismic\",\"indonesia\",\"tsunami\",\"bmkg\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[bmkg-quakes] emitted %d\n", n);
  return 0;
}

static const source_def geoeo_bmkg_def = {
  .id = "bmkg-quakes", .collector = "environment",
  .name = "BMKG Indonesia Recent Earthquakes",
  .update_interval_sec = 300, .run = run,
  .category = "environment", .type = "api",
  .url = "https://data.bmkg.go.id/DataMKG/TEWS/gempaterkini.json",
  .description = "Indonesian Meteorology, Climatology and Geophysical Agency M5+ earthquake list with the official tsunami-potential assessment for each event.",
  .license = "BMKG open data (data.bmkg.go.id), published for public redistribution; attribution to BMKG.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_bmkg_def)
