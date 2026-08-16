/* GDACS CAP — the full Common Alerting Protocol product from the EC/UN Global
 * Disaster Alert and Coordination System.
 *
 * Endpoint (keyless): https://www.gdacs.org/xml/gdacs_cap.xml
 * Emits per <item>: title, pubDate, the <geo:Point> latitude/longitude,
 *   cap:event, cap:severity, cap:certainty, cap:urgency, cap:areaDesc,
 *   cap:web, every cap:parameter name/value pair (eventid, glide, episodeid,
 *   population impact, …) and the affected-area polygon as geometry.
 * Licence: stated in the document itself — "Copyright, Global Disaster Alert
 *   and Coordination System. Licensed under CC BY 4.0." Reuse permitted with
 *   attribution.
 *
 * parse_notes honoured:
 *  - Distinct endpoint from the existing gdacs source (rss.xml/geteventlist).
 *  - 6.2 MB on ONE line with no whitespace, so nothing here is line-oriented;
 *    the document is walked by element blocks.
 *  - Each item carries BOTH a <geo:Point> and a <cap:alert>; the point is used
 *    for the map pin and the polygon is kept as geometry_geojson, exactly as
 *    the notes recommend.
 *  - CAP POLYGON AXIS ORDER: a <cap:polygon> is a space-separated list of
 *    "LAT,LON" pairs (CAP/OASIS convention) — the OPPOSITE of GeoJSON's
 *    [lon,lat]. The ordinates are swapped on the way out; copying them across
 *    unswapped puts every European alert in the Indian Ocean.
 *  - cap:parameter is a repeated valueName/value pair LIST, not an object, so
 *    it is iterated; <cap:value /> self-closing empties (e.g. glide) yield no
 *    property rather than an empty one.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "geoeo_common.inc"
#include <strings.h>

static const char *URL = "https://www.gdacs.org/xml/gdacs_cap.xml";

/* "LAT,LON LAT,LON ..." (CAP order) → a closed GeoJSON Polygon in [lon,lat].
 * Returns NULL when fewer than three vertices decode. */
static cJSON *cap_polygon(const char *s) {
  double la[512], lo[512];
  int np = 0;
  const char *p = s;
  while (*p && np < 512) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) break;
    char *e = NULL;
    double a = strtod(p, &e);
    if (e == p || *e != ',') {                 /* not a pair — skip the token */
      while (*p && *p != ' ') p++;
      continue;
    }
    const char *q = e + 1;
    char *e2 = NULL;
    double b = strtod(q, &e2);
    if (e2 == q) { p = q; continue; }
    if (geoeo_ll_ok(a, b)) { la[np] = a; lo[np] = b; np++; }
    p = e2;
  }
  if (np < 3) return NULL;

  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Polygon");
  cJSON *rings = cJSON_AddArrayToObject(g, "coordinates");
  cJSON *ring = cJSON_CreateArray();
  for (int i = 0; i <= np; i++) {
    int k = (i == np) ? 0 : i;                 /* close the ring */
    cJSON *pt = cJSON_CreateArray();
    cJSON_AddItemToArray(pt, cJSON_CreateNumber(lo[k]));  /* lon first */
    cJSON_AddItemToArray(pt, cJSON_CreateNumber(la[k]));  /* then lat  */
    cJSON_AddItemToArray(ring, pt);
  }
  cJSON_AddItemToArray(rings, ring);
  return g;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, URL, 90000);
  if (!body) {
    fprintf(stderr, "[gdacs-cap-alerts] fetch failed\n");
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

    char title[512], pub[96], sev[64], cert[64], urg[64], ev[96], areadesc[512];
    char web[512], headline[512];
    int has_title = geoeo_xml_text(blk, "title", title, sizeof title);
    geoeo_xml_text(blk, "pubDate", pub, sizeof pub);
    geoeo_xml_text(blk, "severity", sev, sizeof sev);
    geoeo_xml_text(blk, "certainty", cert, sizeof cert);
    geoeo_xml_text(blk, "urgency", urg, sizeof urg);
    geoeo_xml_text(blk, "event", ev, sizeof ev);
    geoeo_xml_text(blk, "areaDesc", areadesc, sizeof areadesc);
    geoeo_xml_text(blk, "web", web, sizeof web);
    geoeo_xml_text(blk, "headline", headline, sizeof headline);
    if (!has_title) { free(blk); continue; }

    double lat = 0, lon = 0;
    int has_lat = geoeo_xml_num(blk, "lat", &lat);
    int has_lon = geoeo_xml_num(blk, "long", &lon);
    int geo = has_lat && has_lon && geoeo_ll_ok(lat, lon);

    /* Polygon → geometry; point → pin. */
    cJSON *poly = NULL;
    {
      char *pbuf = (char *)malloc(65536);
      if (pbuf) {
        if (geoeo_xml_text(blk, "polygon", pbuf, 65536))
          poly = cap_polygon(pbuf);
        free(pbuf);
      }
    }
    char *gj = NULL;
    if (poly) {
      gj = cJSON_PrintUnformatted(poly);
      if (!geo) {
        double px = 0, py = 0;
        if (geoeo_ring_mean(cJSON_GetArrayItem(
                                cJSON_GetObjectItem(poly, "coordinates"), 0),
                            &px, &py) &&
            geoeo_ll_ok(py, px)) {
          lon = px; lat = py; geo = 1;
        }
      }
      cJSON_Delete(poly);
    } else if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "title", title);
    geoeo_add_str(props, "pubDate", pub);
    geoeo_add_str(props, "cap_event", ev);
    geoeo_add_str(props, "cap_severity", sev);
    geoeo_add_str(props, "cap_certainty", cert);
    geoeo_add_str(props, "cap_urgency", urg);
    geoeo_add_str(props, "cap_areaDesc", areadesc);
    geoeo_add_str(props, "cap_headline", headline);
    geoeo_add_str(props, "cap_web", web);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }

    /* cap:parameter is a repeated valueName/value pair list. */
    char eventid[96] = {0};
    {
      const char *pc = blk, *pin = NULL;
      int plen = 0;
      while ((pc = html_block(pc, "parameter", &pin, &plen)) != NULL) {
        if (plen <= 0) continue;
        char *pb = geoeo_dupn(pin, (size_t)plen);
        if (!pb) break;
        char vn[96], vv[256];
        if (geoeo_xml_text(pb, "valueName", vn, sizeof vn) &&
            geoeo_xml_text(pb, "value", vv, sizeof vv)) {
          char pk[128];
          snprintf(pk, sizeof pk, "cap_%s", vn);
          cJSON_AddStringToObject(props, pk, vv);
          if (strcasecmp(vn, "eventid") == 0)
            snprintf(eventid, sizeof eventid, "%s", vv);
        }
        free(pb);
      }
    }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char key[320];
    if (eventid[0]) snprintf(key, sizeof key, "%s|%s", eventid, ev);
    else snprintf(key, sizeof key, "%s|%s", title, pub);

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = headline[0] ? headline : (areadesc[0] ? areadesc : NULL);
    it.link = web[0] ? web : NULL;
    it.lang = "en";
    it.record_type = "gdacs-cap-alert";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"disaster\",\"cap\",\"alert\",\"gdacs\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj);
    free(gj);
    free(blk);
  }

  free(body);
  if (!items) {
    fprintf(stderr, "[gdacs-cap-alerts] no <item> elements — unparseable\n");
    return -1;
  }
  fprintf(stderr, "[gdacs-cap-alerts] emitted %d of %d alerts\n", n, items);
  return 0;
}

static const source_def geoeo_gdacs_cap_def = {
  .id = "gdacs-cap-alerts", .collector = "environment",
  .name = "GDACS CAP Alerts",
  .update_interval_sec = 900, .run = run,
  .category = "environment", .type = "api",
  .url = "https://www.gdacs.org/xml/gdacs_cap.xml",
  .description = "The full CAP product from the EC/UN Global Disaster Alert and Coordination System — severity, certainty, population impact and an actual affected-area polygon per alert.",
  .license = "CC BY 4.0 (Global Disaster Alert and Coordination System), as declared in the document.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_gdacs_cap_def)
