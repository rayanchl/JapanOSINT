/* NZ Transport Agency Waka Kotahi — national traffic camera index.
 * Endpoint: https://trafficnz.info/service/traffic/rest/4/cameras/all
 * Emits: one intel row per state-highway camera — camera id, name, description,
 * highway number, direction, region, the offline flag and the absolute image
 * URL, plus the journey and journey leg the camera belongs to, pinned on the
 * camera's OWN latitude/longitude. Keyless.
 * Licence: NZ Transport Agency Waka Kotahi open traffic feed — CC BY 4.0 under
 * the NZTA open data policy.
 *
 * Parse note (TRAP): each <camera> contains a nested <journey> block that
 * carries <startLatitude>/<startLongitude> for the WHOLE journey, and the
 * camera's own <latitude>/<longitude> appear AFTER it. A naive "first latitude
 * in the record" scan therefore picks up the journey's start point and puts the
 * camera tens of kilometres from where it actually is. Two defences are used
 * here: the tag scan matches the local tag name EXACTLY (so "latitude" can
 * never match "startLatitude"), and the search for the camera's own coordinates
 * begins after the closing </journey> tag when one is present.
 *
 * imageUrl is relative to https://trafficnz.info and is expanded here.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

#define NZTA_URL "https://trafficnz.info/service/traffic/rest/4/cameras/all"

/* Case-insensitive search for `needle`; returns a pointer into hay or NULL. */
static const char *ifind(const char *hay, const char *needle) {
  size_t nl = strlen(needle);
  for (const char *p = hay; *p; p++)
    if (strncasecmp(p, needle, nl) == 0) return p;
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *xml = feed_get_text(ctx->http, NZTA_URL, 40000);
  if (!xml) {
    fprintf(stderr, "[nzta-traffic-cameras] fetch failed\n");
    return -1;
  }

  int n = 0;
  const char *cur = xml;
  char *rec;
  while ((rec = trn_xml_record(&cur, "camera")) != NULL) {
    char *cid = trn_xml_text(rec, "id");
    char *nm  = trn_xml_text(rec, "name");
    if ((!cid || !cid[0]) && (!nm || !nm[0])) {
      free(cid); free(nm); free(rec); continue;
    }
    char *descr  = trn_xml_text(rec, "description");
    char *hwy    = trn_xml_text(rec, "highway");
    char *dir    = trn_xml_text(rec, "direction");
    char *region = trn_xml_text(rec, "region");
    char *offl   = trn_xml_text(rec, "offline");
    char *img    = trn_xml_text(rec, "imageUrl");

    /* journey metadata, read from inside the nested block only */
    char *jname = NULL;
    const char *jstart = ifind(rec, "<journey");
    const char *jend   = ifind(rec, "</journey>");
    if (jstart && jend && jend > jstart) jname = trn_xml_text(jstart, "name");

    /* TRAP: start the camera-coordinate scan AFTER the journey block, so the
     * journey's own start point can never be mistaken for the camera. */
    const char *coord_from = rec;
    if (jend) {
      const char *after = strchr(jend, '>');
      if (after) coord_from = after + 1;
    }
    double la, lo;
    int geo = trn_xml_num(coord_from, "latitude", &la) &&
              trn_xml_num(coord_from, "longitude", &lo) && trn_geo_ok(la, lo);

    char imgurl[512]; imgurl[0] = 0;
    if (img && img[0]) {
      if (strncasecmp(img, "http", 4) == 0)
        snprintf(imgurl, sizeof imgurl, "%s", img);
      else
        snprintf(imgurl, sizeof imgurl, "https://trafficnz.info%s%s",
                 img[0] == '/' ? "" : "/", img);
    }

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "NZTA Waka Kotahi");
    trn_put_str(pr, "camera_id", cid);
    trn_put_str(pr, "name", nm);
    trn_put_str(pr, "description", descr);
    trn_put_str(pr, "highway", hwy);
    trn_put_str(pr, "direction", dir);
    trn_put_str(pr, "region", region);
    trn_put_str(pr, "offline", offl);
    trn_put_str(pr, "image_url", imgurl);
    trn_put_str(pr, "journey", jname);
    char *pj = cJSON_PrintUnformatted(pr);

    char title[320], summary[256];
    snprintf(title, sizeof title, "%s", (nm && nm[0]) ? nm : cid);
    if (hwy && region) snprintf(summary, sizeof summary, "%s · %s", hwy, region);
    else if (region)   snprintf(summary, sizeof summary, "%s", region);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = (cid && cid[0]) ? cid : nm;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = imgurl[0] ? imgurl : NULL;
    it.lang            = "en";
    it.record_type     = "traffic-camera";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"camera\",\"new-zealand\",\"road\"]";
    if (geo) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); cJSON_Delete(pr);
    free(cid); free(nm); free(descr); free(hwy); free(dir);
    free(region); free(offl); free(img); free(jname); free(rec);
  }
  free(xml);
  fprintf(stderr, "[nzta-traffic-cameras] emitted %d\n", n);
  return 0;
}

static const source_def trn_nzta_traffic_cameras_def = {
  .id = "nzta-traffic-cameras", .collector = "transport",
  .name = "NZ Waka Kotahi — national traffic camera index",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "api",
  .url = NZTA_URL,
  .description = "Every New Zealand state-highway camera with per-camera coordinates, highway number, region, offline flag and image URL.",
  .license = "CC BY 4.0 (NZ Transport Agency Waka Kotahi open data policy).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_nzta_traffic_cameras_def)
