/* Two national seismology RSS 2.0 feeds that carry W3C Basic Geo coordinates.
 *
 * Endpoints (both keyless, both PLAIN HTTP — neither host serves TLS):
 *   http://quakes.bgs.ac.uk/feeds/MhSeismology.xml   (British Geological Survey)
 *   http://www.ssn.unam.mx/rss/ultimos-sismos.xml    (SSN / UNAM, Mexico)
 * Emits: item title, description text, link, pubDate (normalised to ISO-8601),
 *        the magnitude and depth parsed out of the title/description text, and
 *        latitude/longitude taken from the per-item <geo:lat>/<geo:long>
 *        elements.
 * Licences: BGS/UKRI Open Government Licence for the seismology feed,
 *   attribution to BGS required. SSN/UNAM public feed; SSN asks for citation of
 *   "Servicio Sismologico Nacional, Mexico".
 *
 * parse_notes honoured:
 *  - Both feeds declare xmlns:geo=w3.org/2003/01/geo/wgs84_pos#. The
 *    coordinates arrive as SIBLING <geo:lat>/<geo:long> ELEMENTS inside each
 *    <item> — not as attributes and not inside <description>. html_tag is
 *    namespace-tolerant, so "lat" matches <geo:lat>.
 *  - An item without both elements is emitted with has_geo=0; there is no
 *    fallback coordinate.
 *  - BGS magnitude/origin time exist only in the title text and the spacing is
 *    irregular ("M  3.2" with two spaces), so the scanner skips runs of space.
 *  - SSN's <description> is CDATA-wrapped HTML with entities; the depth is
 *    taken from it, but the COORDINATES are taken from the geo: elements, not
 *    from the "Lat/Lon:" text.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include <strings.h>
#include "geoeo_common.inc"

/* "Fri, 31 Jul 2026 00:31:16 GMT" → "2026-07-31T00:31:16Z". Returns 1 on a
 * complete parse; the raw string is kept in properties either way. */
static int rfc822_iso(const char *s, char *out, size_t n) {
  static const char *MON[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  if (!s) return 0;
  const char *c = strchr(s, ',');
  if (c) s = c + 1;
  while (*s == ' ') s++;
  int d = 0, y = 0, hh = 0, mi = 0, ss = 0;
  char mon[8] = {0};
  if (sscanf(s, "%d %3s %d %d:%d:%d", &d, mon, &y, &hh, &mi, &ss) < 5) return 0;
  int m = 0;
  for (int i = 0; i < 12; i++)
    if (strncasecmp(mon, MON[i], 3) == 0) { m = i + 1; break; }
  if (!m || d < 1 || d > 31 || y < 1900) return 0;
  long long ep = geoeo_utc_epoch(y, m, d, hh, mi, ss);
  /* Numeric zone offset, when present, moves the instant to UTC. */
  const char *z = strrchr(s, '+');
  const char *zm = strstr(s, " -");
  int sign = 0, zh = 0, zmn = 0;
  if (z && sscanf(z + 1, "%2d%2d", &zh, &zmn) == 2) sign = -1;
  else if (zm && sscanf(zm + 2, "%2d%2d", &zh, &zmn) == 2) sign = 1;
  if (sign) ep += sign * (zh * 3600LL + zmn * 60LL);
  long long days = ep / 86400, rem = ep % 86400;
  if (rem < 0) { rem += 86400; days--; }
  /* civil_from_days */
  long long zz = days + 719468;
  long long era = (zz >= 0 ? zz : zz - 146096) / 146097;
  unsigned doe = (unsigned)(zz - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long long yy = (long long)yoe + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  unsigned dd = doy - (153 * mp + 2) / 5 + 1;
  unsigned mm = mp + (mp < 10 ? 3 : -9);
  yy += (mm <= 2);
  snprintf(out, n, "%04lld-%02u-%02uT%02u:%02u:%02uZ", yy, mm, dd,
           (unsigned)(rem / 3600), (unsigned)((rem % 3600) / 60),
           (unsigned)(rem % 60));
  return 1;
}

/* First "M<spaces><number>" in a string. BGS writes "M  3.2". */
static int scan_mag_after_M(const char *s, double *out) {
  for (const char *p = s; *p; p++) {
    if (*p != 'M' && *p != 'm') continue;
    if (p != s && (isalnum((unsigned char)p[-1]) || p[-1] == '.')) continue;
    const char *q = p + 1;
    while (*q == ' ' || *q == '\t') q++;
    if (!isdigit((unsigned char)*q)) continue;
    char *e = NULL;
    double v = strtod(q, &e);
    if (e != q && isfinite(v) && v > -2.0 && v < 12.0) { *out = v; return 1; }
  }
  return 0;
}

/* First number following `key` (e.g. "Profundidad:") in a string. */
static int scan_num_after(const char *s, const char *key, double *out) {
  const char *p = strstr(s, key);
  if (!p) return 0;
  p += strlen(key);
  while (*p && !isdigit((unsigned char)*p) && *p != '-' && *p != '+') {
    if (*p == '<' || *p == '\n') return 0;
    p++;
  }
  char *e = NULL;
  double v = strtod(p, &e);
  if (e == p || !isfinite(v)) return 0;
  *out = v;
  return 1;
}

/* Shared item walk. `mexico` switches the two text conventions. */
static int georss_collect(const source_ctx *ctx, intel_sink *sink,
                          const char *sid, const char *url, const char *lang,
                          const char *tags, int mexico) {
  char *body = feed_get_text(ctx->http, url, 30000);
  if (!body) {
    fprintf(stderr, "[%s] fetch failed\n", sid);
    return -1;
  }

  int n = 0, items = 0;
  const char *cur = body, *inner = NULL;
  int ilen = 0;
  while ((cur = html_block(cur, "item", &inner, &ilen)) != NULL) {
    if (ilen <= 0) continue;
    items++;
    char *blk = geoeo_dupn(inner, (size_t)ilen);
    if (!blk) break;

    char title[512], desc[2048], link[512], pub[128];
    int has_title = geoeo_xml_text(blk, "title", title, sizeof title);
    geoeo_xml_text(blk, "description", desc, sizeof desc);
    geoeo_xml_text(blk, "link", link, sizeof link);
    geoeo_xml_text(blk, "pubDate", pub, sizeof pub);
    if (!has_title) { free(blk); continue; }

    /* Coordinates come only from the geo: elements. */
    double lat = 0, lon = 0;
    int has_lat = geoeo_xml_num(blk, "lat", &lat);
    int has_lon = geoeo_xml_num(blk, "long", &lon);
    int geo = has_lat && has_lon && geoeo_ll_ok(lat, lon);

    char *plain = html_strip(desc);
    if (plain) geoeo_unescape(plain);

    double mag = 0, dep = 0;
    int has_mag = 0, has_dep = 0;
    if (mexico) {
      /* Magnitude is the leading token of the title, before the first comma. */
      char *e = NULL;
      double v = strtod(title, &e);
      if (e != title && isfinite(v)) { mag = v; has_mag = 1; }
      if (plain) has_dep = scan_num_after(plain, "Profundidad", &dep);
    } else {
      has_mag = scan_mag_after_M(title, &mag);
      if (!has_mag && plain) has_mag = scan_mag_after_M(plain, &mag);
      if (plain) has_dep = scan_num_after(plain, "Depth", &dep);
    }

    char iso[40];
    int has_iso = rfc822_iso(pub, iso, sizeof iso);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "title", title);
    if (plain && plain[0]) cJSON_AddStringToObject(props, "description", plain);
    geoeo_add_str(props, "link", link);
    geoeo_add_str(props, "pubDate", pub);
    if (has_mag) cJSON_AddNumberToObject(props, "magnitude", mag);
    if (has_dep) cJSON_AddNumberToObject(props, "depth_km", dep);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    cJSON_AddStringToObject(props, "agency",
                            mexico ? "Servicio Sismologico Nacional (UNAM)"
                                   : "British Geological Survey");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    char key[256];
    if (link[0]) snprintf(key, sizeof key, "%s", link);
    else snprintf(key, sizeof key, "%s|%s", title, pub);

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = (plain && plain[0]) ? plain : NULL;
    it.link = link[0] ? link : NULL;
    it.lang = lang;
    it.published_at = has_iso ? iso : NULL;
    it.record_type = "earthquake";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = tags;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj);
    free(gj);
    free(plain);
    free(blk);
  }

  free(body);
  if (!items) {
    fprintf(stderr, "[%s] no <item> elements — unparseable\n", sid);
    return -1;
  }
  fprintf(stderr, "[%s] emitted %d of %d items\n", sid, n, items);
  return 0;
}

static int bgs_run(const source_ctx *ctx, intel_sink *sink) {
  return georss_collect(ctx, sink, "bgs-uk-quakes",
                        "http://quakes.bgs.ac.uk/feeds/MhSeismology.xml", "en",
                        "[\"earthquake\",\"seismic\",\"uk\",\"bgs\"]", 0);
}

static int ssn_run(const source_ctx *ctx, intel_sink *sink) {
  return georss_collect(ctx, sink, "ssn-quakes-mx",
                        "http://www.ssn.unam.mx/rss/ultimos-sismos.xml", "es",
                        "[\"earthquake\",\"seismic\",\"mexico\",\"ssn\"]", 1);
}

static const source_def geoeo_bgs_def = {
  .id = "bgs-uk-quakes", .collector = "environment",
  .name = "British Geological Survey UK Earthquakes",
  .update_interval_sec = 3600, .run = bgs_run,
  .category = "environment", .type = "api",
  .url = "http://quakes.bgs.ac.uk/feeds/MhSeismology.xml",
  .description = "BGS Seismology recent seismic events in and around the UK, including North Sea and induced/mining events that no global catalogue lists.",
  .license = "Open Government Licence (BGS/UKRI); attribution to BGS required.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_bgs_def)

static const source_def geoeo_ssn_def = {
  .id = "ssn-quakes-mx", .collector = "environment",
  .name = "SSN Mexico Latest Earthquakes",
  .update_interval_sec = 900, .run = ssn_run,
  .category = "environment", .type = "api",
  .url = "http://www.ssn.unam.mx/rss/ultimos-sismos.xml",
  .description = "Servicio Sismologico Nacional (UNAM) latest located earthquakes in Mexico — the national network reports well below the USGS threshold along the Cocos subduction zone.",
  .license = "SSN/UNAM public feed; cite 'Servicio Sismologico Nacional, Mexico'.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_ssn_def)
