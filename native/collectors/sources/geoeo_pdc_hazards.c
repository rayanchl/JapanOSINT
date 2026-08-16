/* Pacific Disaster Center global hazard feed — active worldwide hazards
 * (floods, storms, quakes, fires, volcanoes and man-made incidents), each with
 * a real coordinate, severity band and a description naming the issuing
 * national agency.
 *
 * Endpoint (keyless): https://hpxml.pdc.org/public.xml
 * Emits per <hazardBean>: hazard_ID, hazard_Name, type_ID, category_ID,
 *   severity_ID, latitude, longitude, start_Date, end_Date, last_Update,
 *   status, description, uuid, snc_url.
 * Licence: PDC (University of Hawaii applied science center) publishes
 *   public.xml as its public hazard feed — no credential, no stated
 *   prohibition; PDC attribution carried on every row. Distinct host from the
 *   www.pdc.org/feed/ RSS already wired.
 *
 * parse_notes honoured:
 *  - 2.5 MB of flat XML: <hazardBeans> containing ~961 <hazardBean> elements
 *    with every field as a child element (no attributes).
 *  - Many optional elements are EMPTY (<charter_Uri></charter_Uri>). Every
 *    read goes through a helper that treats an empty element as absent, so no
 *    blank properties and no 0.0 from an empty numeric element.
 *  - Both *_hst (Hawaii local) and UTC variants of every timestamp exist; the
 *    NON-_hst (UTC) elements are the ones read.
 *  - latitude/longitude are decimal element text; a bean where either fails a
 *    full-string numeric parse is emitted with has_geo=0.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "geoeo_common.inc"

static const char *URL = "https://hpxml.pdc.org/public.xml";

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, URL, 90000);
  if (!body) {
    fprintf(stderr, "[pdc-global-hazards] fetch failed\n");
    return -1;
  }

  int n = 0, beans = 0;
  const char *cur = body, *inner = NULL;
  int ilen = 0;
  while ((cur = html_block(cur, "hazardBean", &inner, &ilen)) != NULL) {
    if (ilen <= 0) continue;
    beans++;
    char *blk = geoeo_dupn(inner, (size_t)ilen);
    if (!blk) break;

    char hid[64], name[512], type[64], cat[64], sev[64], status[64];
    char start[64], end[64], upd[64], uuid[96], snc[512], desc[2048];
    geoeo_xml_text(blk, "hazard_ID", hid, sizeof hid);
    int has_name = geoeo_xml_text(blk, "hazard_Name", name, sizeof name);
    geoeo_xml_text(blk, "type_ID", type, sizeof type);
    geoeo_xml_text(blk, "category_ID", cat, sizeof cat);
    geoeo_xml_text(blk, "severity_ID", sev, sizeof sev);
    geoeo_xml_text(blk, "status", status, sizeof status);
    /* UTC variants, not the *_hst Hawaii-local ones. */
    geoeo_xml_text(blk, "start_Date", start, sizeof start);
    geoeo_xml_text(blk, "end_Date", end, sizeof end);
    geoeo_xml_text(blk, "last_Update", upd, sizeof upd);
    geoeo_xml_text(blk, "uuid", uuid, sizeof uuid);
    geoeo_xml_text(blk, "snc_url", snc, sizeof snc);
    geoeo_xml_text(blk, "description", desc, sizeof desc);
    if (!has_name && !hid[0]) { free(blk); continue; }

    double lat = 0, lon = 0;
    int geo = geoeo_xml_num(blk, "latitude", &lat) &&
              geoeo_xml_num(blk, "longitude", &lon) && geoeo_ll_ok(lat, lon);

    char title[576];
    snprintf(title, sizeof title, "%s%s%s", has_name ? name : hid,
             type[0] ? " · " : "", type[0] ? type : "");

    char summary[192];
    snprintf(summary, sizeof summary, "%s%s%s", sev[0] ? sev : "",
             (sev[0] && start[0]) ? " · from " : "", start[0] ? start : "");

    cJSON *props = cJSON_CreateObject();
    geoeo_add_str(props, "hazard_ID", hid);
    geoeo_add_str(props, "hazard_Name", name);
    geoeo_add_str(props, "type_ID", type);
    geoeo_add_str(props, "category_ID", cat);
    geoeo_add_str(props, "severity_ID", sev);
    geoeo_add_str(props, "status", status);
    geoeo_add_str(props, "start_Date", start);
    geoeo_add_str(props, "end_Date", end);
    geoeo_add_str(props, "last_Update", upd);
    geoeo_add_str(props, "uuid", uuid);
    geoeo_add_str(props, "snc_url", snc);
    geoeo_add_str(props, "description", desc);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    cJSON_AddStringToObject(props, "provider", "Pacific Disaster Center");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    char key[160];
    snprintf(key, sizeof key, "%s", uuid[0] ? uuid : (hid[0] ? hid : name));

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.body = desc[0] ? desc : NULL;
    it.link = snc[0] ? snc : NULL;
    it.lang = "en";
    it.published_at = start[0] ? start : NULL;
    it.record_type = "hazard";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"hazard\",\"disaster\",\"pdc\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj);
    free(gj);
    free(blk);
  }

  free(body);
  if (!beans) {
    fprintf(stderr, "[pdc-global-hazards] no <hazardBean> elements\n");
    return -1;
  }
  fprintf(stderr, "[pdc-global-hazards] emitted %d of %d hazards\n", n, beans);
  return 0;
}

static const source_def geoeo_pdc_def = {
  .id = "pdc-global-hazards", .collector = "environment",
  .name = "Pacific Disaster Center Global Hazard XML",
  .update_interval_sec = 1800, .run = run,
  .category = "environment", .type = "api",
  .url = "https://hpxml.pdc.org/public.xml",
  .description = "Active worldwide hazards from the Pacific Disaster Center's operational feed — floods, storms, quakes, fires, volcanoes and man-made incidents with coordinate, severity band and description.",
  .license = "PDC public hazard feed (hpxml.pdc.org); no credential, no stated prohibition; carry PDC attribution.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_pdc_def)
