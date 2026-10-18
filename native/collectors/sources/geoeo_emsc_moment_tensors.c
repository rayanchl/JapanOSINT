/* EMSC/CSEM Moment Tensor Web Service — focal mechanisms for significant
 * global earthquakes, aggregated from GCMT, GFZ, INGV, USGS and regional
 * agencies. Tells the analyst the fault geometry and rupture style, not just
 * that shaking happened.
 *
 * Endpoint (keyless):
 *   https://www.seismicportal.eu/mtws/api/search?limit=1000&format=json
 * Emits: mt_id, ev_latitude/ev_longitude, ev_depth, ev_region, nodal plane 1
 *        (strike/dip/rake), scalar moment mt_m0, percent double-couple,
 *        mt_source_catalog, mt_centroid_time.
 * Licence: EMSC/CSEM seismicportal open web service; EMSC asks for
 *          acknowledgement of the contributing agency named in
 *          mt_source_catalog, which is carried on every row.
 *
 * parse_notes honoured:
 *  - Distinct service from the FDSN event endpoint (different path, different
 *    schema); the top level is a BARE JSON ARRAY with no wrapper object.
 *  - 'full_count' is repeated inside every element (a corpus total, not a
 *    per-record value) and is deliberately NOT emitted as a record field. It
 *    is the number the page size was measured against — see the comment on URL.
 *  - 'deltatime' is a human string ("@ 31 mins 31.662 secs"), so it is carried
 *    as text and never parsed as a number.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/pagewalk.h"
#include "geoeo_common.inc"

/* A BIGGER PAGE AND A DISCLOSED BOUND, DELIBERATELY NOT A FULL WALK.
 *
 * Measured 2026-10-18: `full_count` is 48,863 moment tensors, the service caps
 * `limit` at 1,000 (asking 5,000 still returns 1,000), and `offset` IS honoured
 * and is 0-BASED here (offset=2 starts at the third record, so this service
 * does NOT follow the FDSN spec's 1-based convention — see geohazard_extra.c,
 * where the SAME organisation's fdsnws endpoint is 1-based).
 *
 * So the whole catalogue is reachable in principle, and paging it is still the
 * wrong fix: this row runs every 30 minutes, the 20-page ceiling would stop it
 * at 20,000 of 48,863, and it would re-walk those same 20,000 twice an hour —
 * 960 requests a day against a public research service to re-read records we
 * already hold, while never reaching the other 28,863. The newest-first page of
 * 1,000 covers the 30-minute window with days to spare, which is the question
 * this source exists to answer.
 *
 * `offset` is therefore NOT written into the URL (pw_walk advances only a
 * parameter the author wrote, so its absence is what keeps this to one
 * request), and the walk goes through pw_walk anyway so the bound is STATED: a
 * page that comes back exactly full files a collector-truncation-notice instead
 * of leaving the discard silent. Reaching the historical backlog needs a
 * different strategy — a dated window or a bulk catalogue — which is a design
 * decision and not a page-size change. */
static const char *URL =
    "https://www.seismicportal.eu/mtws/api/search?limit=1000&format=json";

/* pw_emit_fn. `seen` is what the page CONTAINED, not what was emitted: an entry
 * with no usable mt_id is skipped below, and reporting the emitted count would
 * make a full page look short — stopping the walk and suppressing its own
 * truncation notice (lib/pagewalk.h). */
static int mt_emit_page(const source_ctx *ctx, intel_sink *sink,
                        const char *sid, cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)ud;
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(arr)) {
    fprintf(stderr, "[%s] top level is not an array\n", sid);
    *seen = 0;
    return 0;
  }
  *seen = cJSON_GetArraySize(arr);

  static const char *KEYS[] = { "mt_id", "ev_region", "ev_id", "mt_source_catalog",
                                "mt_centroid_time", "mt_type", "deltatime",
                                "ev_event_time", NULL };
  int n = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    char idbuf[64];
    const char *mid = geoeo_str(e, "mt_id");
    if (!mid) {
      double v = 0;
      if (!geoeo_num(e, "mt_id", &v)) continue;
      snprintf(idbuf, sizeof idbuf, "%lld", (long long)v);
      mid = idbuf;
    }

    double lat = 0, lon = 0, dep = 0, m0 = 0, dc = 0;
    int geo = geoeo_numlax(e, "ev_latitude", &lat) &&
              geoeo_numlax(e, "ev_longitude", &lon) && geoeo_ll_ok(lat, lon);
    int has_dep = geoeo_numlax(e, "ev_depth", &dep);
    int has_m0 = geoeo_numlax(e, "mt_m0", &m0);
    int has_dc = geoeo_numlax(e, "mt_per_dc", &dc);
    double str1 = 0, dip1 = 0, rak1 = 0;
    int has_np1 = geoeo_numlax(e, "mt_strike_1", &str1) &&
                  geoeo_numlax(e, "mt_dip_1", &dip1) &&
                  geoeo_numlax(e, "mt_rake_1", &rak1);

    const char *region = geoeo_str(e, "ev_region");
    const char *cat = geoeo_str(e, "mt_source_catalog");
    const char *ctime = geoeo_str(e, "mt_centroid_time");

    char title[384];
    snprintf(title, sizeof title, "Moment tensor — %s%s%s%s",
             region ? region : "global event", cat ? " (" : "",
             cat ? cat : "", cat ? ")" : "");

    char summary[256];
    int w = 0;
    summary[0] = 0;
    if (has_np1)
      w += snprintf(summary + w, sizeof summary - (size_t)w,
                    "strike/dip/rake %.0f/%.0f/%.0f", str1, dip1, rak1);
    if (has_dc && w < (int)sizeof summary)
      w += snprintf(summary + w, sizeof summary - (size_t)w, "%s%.1f%% DC",
                    w ? " · " : "", dc);
    if (has_dep && w < (int)sizeof summary)
      snprintf(summary + w, sizeof summary - (size_t)w, "%sdepth %.1f km",
               w ? " · " : "", dep);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "mt_id", mid);
    geoeo_copy_all(props, e, KEYS);
    if (geo) { cJSON_AddNumberToObject(props, "ev_latitude", lat);
               cJSON_AddNumberToObject(props, "ev_longitude", lon); }
    if (has_dep) cJSON_AddNumberToObject(props, "ev_depth_km", dep);
    if (has_m0) cJSON_AddNumberToObject(props, "mt_m0", m0);
    if (has_dc) cJSON_AddNumberToObject(props, "mt_per_dc", dc);
    if (has_np1) {
      cJSON_AddNumberToObject(props, "mt_strike_1", str1);
      cJSON_AddNumberToObject(props, "mt_dip_1", dip1);
      cJSON_AddNumberToObject(props, "mt_rake_1", rak1);
    }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    intel_item it = {0};
    it.remote_key = mid;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.author = cat;
    it.lang = "en";
    it.published_at = ctime;
    it.record_type = "moment-tensor";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"earthquake\",\"seismic\",\"moment-tensor\",\"emsc\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = pw_walk(ctx, sink, "emsc-moment-tensors", URL,
                  pw_fetch_json, mt_emit_page, NULL);
  if (n < 0) {
    fprintf(stderr, "[emsc-moment-tensors] fetch/parse failed\n");
    return -1;
  }
  return 0;
}

static const source_def geoeo_emsc_mt_def = {
  .id = "emsc-moment-tensors", .collector = "environment",
  .name = "EMSC Moment Tensor Web Service",
  .update_interval_sec = 1800, .run = run,
  .category = "environment", .type = "api",
  .url = "https://www.seismicportal.eu/mtws/api/search?limit=1000&format=json",
  .description = "Focal mechanisms (moment tensors) for significant global earthquakes aggregated by EMSC from GCMT, GFZ, INGV, USGS and regional agencies.",
  .license = "EMSC/CSEM seismicportal open web service; acknowledge the contributing agency in mt_source_catalog.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_emsc_mt_def)
