/* UNESCO World Heritage List — the canonical protected-site registry, and in
 * conflict monitoring a cultural-property no-strike reference layer.
 *
 * Endpoint (keyless): https://whc.unesco.org/en/list/xml/
 * Emits per site: id_number, site name, http_url, iso_code (the raw
 *   comma-separated multi-country string), category, date_inscribed, danger
 *   flag, region, states, short_description, and the site's component
 *   coordinates.
 * Licence: UNESCO World Heritage Centre publishes the XML list for public
 *   reuse; UNESCO requires attribution and prohibits implying endorsement.
 *   No prohibition on programmatic access.
 *
 * parse_notes honoured:
 *  - 2.5 MB, <query columns='216' rows='1273'> with one <row> per site.
 *  - GEOMETRY IS MULTI-POINT AND NESTED: <geolocations> contains one or MORE
 *    <poi> elements, each with its own <latitude>/<longitude>/<iso2>. Serial
 *    and transnational sites have dozens of components. All of them are kept:
 *    a single component becomes a Point, several become a MultiPoint, and the
 *    pin is the FIRST component with geo_precision recorded so the pin is not
 *    mistaken for "the" location of a serial site.
 *  - A handful of marine and serial nominations have an EMPTY <geolocations/>
 *    element. That becomes has_geo=0 and a NULL geometry, never 0,0.
 *  - Several fields (justification) are empty elements; the reader treats an
 *    empty element as absent so no blank properties are written.
 *  - iso_code is a COMMA-SEPARATED multi-country string, not a single code, and
 *    is carried verbatim rather than split.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "geoeo_common.inc"

static const char *URL = "https://whc.unesco.org/en/list/xml/";

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, URL, 90000);
  if (!body) {
    fprintf(stderr, "[unesco-world-heritage] fetch failed\n");
    return -1;
  }

  int n = 0, rows = 0;
  const char *cur = body, *inner = NULL;
  int ilen = 0;
  while ((cur = html_block(cur, "row", &inner, &ilen)) != NULL) {
    if (ilen <= 0) continue;
    rows++;
    char *blk = geoeo_dupn(inner, (size_t)ilen);
    if (!blk) break;

    char idnum[32], site[384], url[384], iso[128], cat[64], inscribed[32];
    char danger[64], region[96], states[256];
    char *desc = (char *)malloc(8192);
    geoeo_xml_text(blk, "id_number", idnum, sizeof idnum);
    int has_site = geoeo_xml_text(blk, "site", site, sizeof site);
    geoeo_xml_text(blk, "http_url", url, sizeof url);
    geoeo_xml_text(blk, "iso_code", iso, sizeof iso);
    geoeo_xml_text(blk, "category", cat, sizeof cat);
    geoeo_xml_text(blk, "date_inscribed", inscribed, sizeof inscribed);
    geoeo_xml_text(blk, "danger", danger, sizeof danger);
    geoeo_xml_text(blk, "region", region, sizeof region);
    geoeo_xml_text(blk, "states", states, sizeof states);
    if (desc) geoeo_xml_text(blk, "short_description", desc, 8192);
    if (!has_site && !idnum[0]) { free(desc); free(blk); continue; }

    /* Component coordinates, scoped to the <geolocations> block. */
    double plat = 0, plon = 0;
    int npoi = 0, geo = 0;
    cJSON *geom = NULL;
    cJSON *coords = NULL;
    {
      char *gl = NULL;
      const char *gi = NULL;
      int glen = 0;
      if (html_block(blk, "geolocations", &gi, &glen) && glen > 0)
        gl = geoeo_dupn(gi, (size_t)glen);
      if (gl) {
        const char *pc = gl, *pin = NULL;
        int plen = 0;
        while ((pc = html_block(pc, "poi", &pin, &plen)) != NULL) {
          if (plen <= 0) continue;
          char *pb = geoeo_dupn(pin, (size_t)plen);
          if (!pb) break;
          double la = 0, lo = 0;
          if (geoeo_xml_num(pb, "latitude", &la) &&
              geoeo_xml_num(pb, "longitude", &lo) && geoeo_ll_ok(la, lo)) {
            if (!coords) coords = cJSON_CreateArray();
            cJSON *pt = cJSON_CreateArray();
            cJSON_AddItemToArray(pt, cJSON_CreateNumber(lo));
            cJSON_AddItemToArray(pt, cJSON_CreateNumber(la));
            cJSON_AddItemToArray(coords, pt);
            if (npoi == 0) { plat = la; plon = lo; }
            npoi++;
          }
          free(pb);
        }
        free(gl);
      }
    }
    if (npoi == 1) {
      geom = geoeo_mk_point(plon, plat);
      cJSON_Delete(coords);
      coords = NULL;
      geo = 1;
    } else if (npoi > 1) {
      geom = cJSON_CreateObject();
      cJSON_AddStringToObject(geom, "type", "MultiPoint");
      cJSON_AddItemToObject(geom, "coordinates", coords);
      coords = NULL;                       /* ownership transferred */
      geo = 1;
    } else if (coords) {
      cJSON_Delete(coords);
      coords = NULL;
    }
    char *gj = geom ? cJSON_PrintUnformatted(geom) : NULL;
    if (geom) cJSON_Delete(geom);

    cJSON *props = cJSON_CreateObject();
    geoeo_add_str(props, "id_number", idnum);
    geoeo_add_str(props, "site", site);
    geoeo_add_str(props, "http_url", url);
    geoeo_add_str(props, "iso_code", iso);          /* comma-separated, raw */
    geoeo_add_str(props, "category", cat);
    geoeo_add_str(props, "date_inscribed", inscribed);
    geoeo_add_str(props, "danger", danger);
    geoeo_add_str(props, "region", region);
    geoeo_add_str(props, "states", states);
    if (desc && desc[0]) cJSON_AddStringToObject(props, "short_description", desc);
    if (geo) {
      cJSON_AddNumberToObject(props, "component_count", npoi);
      cJSON_AddNumberToObject(props, "pin_latitude", plat);
      cJSON_AddNumberToObject(props, "pin_longitude", plon);
      if (npoi > 1)
        cJSON_AddStringToObject(props, "geo_precision",
                                "first component of a serial/transnational site");
    }
    cJSON_AddStringToObject(props, "registry", "UNESCO World Heritage Centre");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[416];
    snprintf(title, sizeof title, "%s", has_site ? site : idnum);

    char summary[256];
    snprintf(summary, sizeof summary, "%s%s%s%s", cat[0] ? cat : "",
             (cat[0] && inscribed[0]) ? " · inscribed " : "",
             inscribed[0] ? inscribed : "",
             (danger[0] && strcmp(danger, "0") != 0) ? " · IN DANGER" : "");

    intel_item it = {0};
    it.remote_key = idnum[0] ? idnum : site;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.body = (desc && desc[0]) ? desc : NULL;
    it.link = url[0] ? url : NULL;
    it.lang = "en";
    it.record_type = "world-heritage-site";
    it.has_geo = geo;
    it.lat = plat;
    it.lon = plon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"heritage\",\"unesco\",\"protected-site\",\"cultural\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj);
    free(gj);
    free(desc);
    free(blk);
  }

  free(body);
  if (!rows) {
    fprintf(stderr, "[unesco-world-heritage] no <row> elements — unparseable\n");
    return -1;
  }
  fprintf(stderr, "[unesco-world-heritage] emitted %d of %d sites\n", n, rows);
  return 0;
}

static const source_def geoeo_unesco_def = {
  .id = "unesco-world-heritage", .collector = "geospatial",
  .name = "UNESCO World Heritage List (XML)",
  .update_interval_sec = 2592000, .run = run,
  .category = "geospatial", .type = "dataset",
  .url = "https://whc.unesco.org/en/list/xml/",
  .description = "All UNESCO World Heritage sites with component coordinates, inscription date, transboundary country codes and in-danger status — a canonical protected-site registry.",
  .license = "UNESCO World Heritage Centre XML list, published for public reuse; attribution required, no implication of endorsement.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_unesco_def)
