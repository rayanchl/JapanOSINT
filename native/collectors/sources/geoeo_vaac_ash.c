/* VAAC Volcanic Ash Advisories — the raw ICAO bulletins as issued to aviation,
 * from all nine Volcanic Ash Advisory Centres (Buenos Aires, Washington,
 * Anchorage, Toulouse, London, Darwin, Tokyo, Wellington, Montreal).
 *
 * Endpoint (keyless, two-step):
 *   1. https://tgftp.nws.noaa.gov/data/raw/fv/          (Apache directory index)
 *   2. https://tgftp.nws.noaa.gov/data/raw/fv/<file>.txt (one VAA bulletin)
 * Emits per bulletin: VAAC, VOLCANO (+ Smithsonian number), AREA, SUMMIT ELEV,
 *   ADVISORY NR, DTG, OBS VA DTG, OBS VA CLD, FCST VA CLD, the decoded summit
 *   position and, when the observed ash cloud is given as a closed set of
 *   points, its polygon.
 * Licence: NWS/NOAA public domain; tgftp.nws.noaa.gov is the official public
 *   bulletin mirror. (vaac.arh.noaa.gov timed out at probe time and the
 *   ospo.noaa.gov VAAC page is HTML-only — this raw directory is the
 *   machine-readable path.)
 *
 * GEOMETRY IS ICAO SEXAGESIMAL, NOT DECIMAL (from parse_notes):
 *   "PSN: S1547 W07150" means 15 deg 47 min South, 71 deg 50 min West →
 *   -15.783, -71.833. It is parsed as (hemisphere)(DD)(MM) for latitude and
 *   (hemisphere)(DDD)(MM) for longitude. Treating "1547" as a decimal degree
 *   puts the volcano in the Pacific. Advisories carrying "PSN: UNKNOWN" are
 *   emitted with has_geo=0.
 * The OBS VA CLD block is a polygon of the same sexagesimal pairs and becomes a
 * real geometry_geojson when at least three points decode.
 * The directory listing's mtime is used to skip stale bulletins.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "geoeo_common.inc"
#include <strings.h>

#define VAAC_BASE "https://tgftp.nws.noaa.gov/data/raw/fv/"
#define VAAC_MAX_FILES 60
#define VAAC_MAX_AGE_DAYS 4

/* ---- ICAO sexagesimal ---- */

/* One "S1547" / "W07150" / "N4530" / "E13712" token (optionally DDMMSS /
 * DDDMMSS). Returns 1 and sets *lat_flag when the token is a latitude. */
static int icao_ord(const char *tok, double *out, int *lat_flag) {
  char h = (char)toupper((unsigned char)tok[0]);
  int is_lat;
  if (h == 'N' || h == 'S') is_lat = 1;
  else if (h == 'E' || h == 'W') is_lat = 0;
  else return 0;

  const char *d = tok + 1;
  int nd = 0;
  while (isdigit((unsigned char)d[nd])) nd++;
  int deg_digits;
  if (is_lat && (nd == 4 || nd == 6)) deg_digits = 2;
  else if (!is_lat && (nd == 5 || nd == 7)) deg_digits = 3;
  else return 0;

  char buf[8];
  memcpy(buf, d, (size_t)deg_digits);
  buf[deg_digits] = 0;
  double deg = atof(buf);
  memcpy(buf, d + deg_digits, 2);
  buf[2] = 0;
  double min = atof(buf);
  double sec = 0;
  if (nd == deg_digits + 4) {
    memcpy(buf, d + deg_digits + 2, 2);
    buf[2] = 0;
    sec = atof(buf);
  }
  if (min >= 60 || sec >= 60) return 0;
  double v = deg + min / 60.0 + sec / 3600.0;
  if (h == 'S' || h == 'W') v = -v;
  if (is_lat ? (v < -90 || v > 90) : (v < -180 || v > 180)) return 0;
  *out = v;
  *lat_flag = is_lat;
  return 1;
}

/* Collect ordered lat/lon pairs from a text run. Returns #pairs written. */
static int icao_pairs(const char *s, double *lat, double *lon, int max) {
  int np = 0, have_lat = 0;
  double plat = 0;
  for (const char *p = s; *p; p++) {
    if (p != s && !isspace((unsigned char)p[-1])) continue;
    double v = 0;
    int is_lat = 0;
    if (!icao_ord(p, &v, &is_lat)) continue;
    if (is_lat) { plat = v; have_lat = 1; }
    else if (have_lat && np < max) {
      lat[np] = plat;
      lon[np] = v;
      np++;
      have_lat = 0;
    }
  }
  return np;
}

/* ---- bulletin field extraction ---- */

/* Does the line at p start a new "KEY:" field? */
static int is_field_start(const char *p) {
  int i = 0;
  while (p[i] == ' ') i++;
  int start = i;
  while (p[i] && p[i] != '\n' && i - start < 28) {
    char c = p[i];
    if (c == ':') return i > start;
    if (!(isupper((unsigned char)c) || isdigit((unsigned char)c) || c == ' ' ||
          c == '/' || c == '+' || c == '-' || c == '(' || c == ')'))
      return 0;
    i++;
  }
  return 0;
}

/* Value of "KEY:" (key includes the colon), continuation lines appended. */
static int vaa_field(const char *txt, const char *key, char *out, size_t n) {
  out[0] = 0;
  size_t kl = strlen(key);
  const char *line = txt;
  while (line && *line) {
    const char *ls = line;
    while (*ls == ' ') ls++;
    if (strncasecmp(ls, key, kl) == 0) {
      const char *v = ls + kl;
      size_t w = 0;
      for (;;) {
        while (*v == ' ' || *v == '\t') v++;
        const char *eol = strchr(v, '\n');
        size_t len = eol ? (size_t)(eol - v) : strlen(v);
        while (len && (v[len - 1] == '\r' || v[len - 1] == ' ')) len--;
        if (len) {
          if (w && w + 1 < n) out[w++] = ' ';
          if (w + len >= n) len = n - w - 1;
          memcpy(out + w, v, len);
          w += len;
          out[w] = 0;
        }
        if (!eol) break;
        v = eol + 1;
        if (!*v || *v == '\n' || is_field_start(v)) break;
      }
      return out[0] != 0;
    }
    const char *nl = strchr(line, '\n');
    line = nl ? nl + 1 : NULL;
  }
  return 0;
}

/* "20260801/1620Z" → "2026-08-01T16:20:00Z" */
static int dtg_iso(const char *s, char *out, size_t n) {
  int y, mo, d, hh, mi;
  if (!s || sscanf(s, "%4d%2d%2d/%2d%2d", &y, &mo, &d, &hh, &mi) != 5) return 0;
  if (y < 1990 || mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
  snprintf(out, n, "%04d-%02d-%02dT%02d:%02d:00Z", y, mo, d, hh, mi);
  return 1;
}

/* "01-Aug-2026 16:25" in the Apache index → epoch seconds, or 0. */
static long long index_mtime(const char *p, const char *limit) {
  static const char *MON[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  for (; p < limit && *p; p++) {
    int d, y, hh, mi;
    char mon[8];
    if (sscanf(p, "%2d-%3[A-Za-z]-%4d %2d:%2d", &d, mon, &y, &hh, &mi) == 5) {
      for (int i = 0; i < 12; i++)
        if (strncasecmp(mon, MON[i], 3) == 0)
          return geoeo_utc_epoch(y, i + 1, d, hh, mi, 0);
      return 0;
    }
  }
  return 0;
}

static int emit_bulletin(intel_sink *sink, const char *fname, const char *txt,
                         int *n) {
  char vaac[96], volcano[160], psn[96], area[96], elev[64], nr[64];
  char dtg[64], obsdtg[64], obscld[1024], fcst[1024];
  vaa_field(txt, "VAAC:", vaac, sizeof vaac);
  vaa_field(txt, "VOLCANO:", volcano, sizeof volcano);
  vaa_field(txt, "PSN:", psn, sizeof psn);
  vaa_field(txt, "AREA:", area, sizeof area);
  vaa_field(txt, "SUMMIT ELEV:", elev, sizeof elev);
  vaa_field(txt, "ADVISORY NR:", nr, sizeof nr);
  vaa_field(txt, "DTG:", dtg, sizeof dtg);
  vaa_field(txt, "OBS VA DTG:", obsdtg, sizeof obsdtg);
  vaa_field(txt, "OBS VA CLD:", obscld, sizeof obscld);
  vaa_field(txt, "FCST VA CLD +6HR:", fcst, sizeof fcst);
  if (!vaac[0] && !volcano[0]) return 0;          /* not a VA advisory file */

  /* Summit position. "PSN: UNKNOWN" (and anything else that does not decode)
   * leaves has_geo=0 — no fallback position. */
  double slat = 0, slon = 0;
  int has_psn = 0;
  {
    double la[2], lo[2];
    if (icao_pairs(psn, la, lo, 2) >= 1) {
      slat = la[0];
      slon = lo[0];
      has_psn = geoeo_ll_ok(slat, slon);
    }
  }

  /* Observed ash cloud outline, same sexagesimal encoding. */
  double clat[64], clon[64];
  int npts = icao_pairs(obscld, clat, clon, 64);

  cJSON *geom = NULL;
  double plat = slat, plon = slon;
  int geo = has_psn;
  if (npts >= 3) {
    geom = cJSON_CreateObject();
    cJSON_AddStringToObject(geom, "type", "Polygon");
    cJSON *rings = cJSON_AddArrayToObject(geom, "coordinates");
    cJSON *ring = cJSON_CreateArray();
    double sx = 0, sy = 0;
    for (int i = 0; i <= npts; i++) {
      int k = (i == npts) ? 0 : i;               /* close the ring */
      cJSON *pt = cJSON_CreateArray();
      cJSON_AddItemToArray(pt, cJSON_CreateNumber(clon[k]));
      cJSON_AddItemToArray(pt, cJSON_CreateNumber(clat[k]));
      cJSON_AddItemToArray(ring, pt);
      if (i < npts) { sx += clon[k]; sy += clat[k]; }
    }
    cJSON_AddItemToArray(rings, ring);
    if (!has_psn) { plon = sx / npts; plat = sy / npts; geo = 1; }
  } else if (has_psn) {
    geom = geoeo_mk_point(slon, slat);
  }
  char *gj = geom ? cJSON_PrintUnformatted(geom) : NULL;
  if (geom) cJSON_Delete(geom);

  char iso[40];
  int has_iso = dtg_iso(dtg, iso, sizeof iso);

  char title[384];
  snprintf(title, sizeof title, "Volcanic ash advisory — %s%s%s",
           volcano[0] ? volcano : "volcano", vaac[0] ? " · VAAC " : "",
           vaac[0] ? vaac : "");

  char summary[1280];
  snprintf(summary, sizeof summary, "%s%s%s", obscld[0] ? obscld : "",
           (obscld[0] && area[0]) ? " · " : "", area[0] ? area : "");

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "bulletin_file", fname);
  geoeo_add_str(props, "vaac", vaac);
  geoeo_add_str(props, "volcano", volcano);
  geoeo_add_str(props, "psn_raw", psn);
  geoeo_add_str(props, "area", area);
  geoeo_add_str(props, "summit_elev", elev);
  geoeo_add_str(props, "advisory_nr", nr);
  geoeo_add_str(props, "dtg", dtg);
  geoeo_add_str(props, "obs_va_dtg", obsdtg);
  geoeo_add_str(props, "obs_va_cld", obscld);
  geoeo_add_str(props, "fcst_va_cld_6hr", fcst);
  if (has_psn) {
    cJSON_AddNumberToObject(props, "summit_lat", slat);
    cJSON_AddNumberToObject(props, "summit_lon", slon);
  }
  if (npts >= 3) cJSON_AddNumberToObject(props, "obs_cloud_vertices", npts);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char key[192];
  snprintf(key, sizeof key, "%s|%s", fname, dtg[0] ? dtg : nr);

  char link[320];
  snprintf(link, sizeof link, "%s%s", VAAC_BASE, fname);

  intel_item it = {0};
  it.remote_key = key;
  it.title = title;
  it.summary = summary;
  it.body = txt;
  it.link = link;
  it.lang = "en";
  it.published_at = has_iso ? iso : NULL;
  it.record_type = "volcanic-ash-advisory";
  it.has_geo = geo;
  it.lat = plat;
  it.lon = plon;
  it.geometry_geojson = gj;
  it.properties_json = pj ? pj : "{}";
  it.tags_json = "[\"volcano\",\"aviation\",\"ash\",\"vaac\",\"noaa\"]";
  if (sink->emit(sink, &it) >= 0) (*n)++;
  free(pj);
  free(gj);
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *index = feed_get_text(ctx->http, VAAC_BASE, 30000);
  if (!index) {
    fprintf(stderr, "[vaac-ash-advisories] directory index fetch failed\n");
    return -1;
  }

  long long cutoff = (long long)time(NULL) - VAAC_MAX_AGE_DAYS * 86400LL;
  int n = 0, fetched = 0, listed = 0;
  const char *p = index;
  while (fetched < VAAC_MAX_FILES && (p = strstr(p, "href=\"")) != NULL) {
    p += 6;
    const char *q = strchr(p, '"');
    if (!q) break;
    size_t len = (size_t)(q - p);
    if (len == 0 || len > 96) { p = q + 1; continue; }
    char fname[128];
    memcpy(fname, p, len);
    fname[len] = 0;
    p = q + 1;
    if (strchr(fname, '/')) continue;                      /* parent dir link */
    size_t fl = strlen(fname);
    if (fl < 5 || strcasecmp(fname + fl - 4, ".txt") != 0) continue;
    if (strncasecmp(fname, "fv", 2) != 0) continue;
    listed++;

    /* Skip bulletins whose listed mtime is stale — the directory is a rolling
     * mirror and re-fetching month-old files every 30 minutes is abusive. */
    const char *limit = p + 160;
    long long mt = index_mtime(p, limit);
    if (mt && mt < cutoff) continue;

    char url[320];
    snprintf(url, sizeof url, "%s%s", VAAC_BASE, fname);
    char *txt = feed_get_text(ctx->http, url, 20000);
    fetched++;
    if (!txt) continue;
    emit_bulletin(sink, fname, txt, &n);
    free(txt);
  }
  free(index);

  if (!listed) {
    fprintf(stderr, "[vaac-ash-advisories] no fvXXNN*.txt links in index\n");
    return -1;
  }
  fprintf(stderr, "[vaac-ash-advisories] %d listed, %d fetched, emitted %d\n",
          listed, fetched, n);
  return 0;
}

static const source_def geoeo_vaac_def = {
  .id = "vaac-ash-advisories", .collector = "environment",
  .name = "VAAC Volcanic Ash Advisories (NWS raw bulletins)",
  .update_interval_sec = 1800, .run = run,
  .category = "environment", .type = "dataset",
  .url = "https://tgftp.nws.noaa.gov/data/raw/fv/",
  .description = "Raw ICAO volcanic ash advisories from all nine VAACs as issued to aviation — summit position, observed ash cloud polygon, flight levels and drift vector.",
  .license = "NWS/NOAA public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_vaac_def)
