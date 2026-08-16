/* collectors/sources/hazard_world.c
 * Global natural-hazard scheduled feeds. Each source REAL-fetches an official
 * public endpoint, parses it, and emits one intel_item per event (with geo when
 * the feed carries coordinates). Nothing is synthesized: parse failure / empty
 * feed → honest empty (return 0); a key-gated feed with no key → gated + 0.
 * These are scheduled global feeds — ctx->entity is normally NULL and unused.
 *
 *   USGS_QUAKES      USGS earthquakes (FDSNWS GeoJSON)             — has_geo
 *   EMSC_QUAKES      EMSC/seismicportal earthquakes (FDSNWS JSON)  — has_geo
 *   GVP_VOLCANOES    Smithsonian GVP weekly volcanic activity (RSS)
 *   GDACS_DISASTERS  GDACS multi-hazard disasters (RSS)            — has_geo (georss)
 *   FIRMS_GLOBAL     NASA FIRMS active fires (area CSV)  gate FIRMS_MAP_KEY — has_geo
 *
 * One run() dispatches on ctx->source_id.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

/* ---- USGS earthquakes (GeoJSON FeatureCollection) ----------------------- *
 * features[].geometry.coordinates = [lon, lat, depth]
 * features[].properties = { mag, place, time(ms), url, title } */
static int hz_usgs(const source_ctx *ctx, intel_sink *sink) {
  const char *url =
    "https://earthquake.usgs.gov/fdsnws/event/1/query?format=geojson&limit=50&orderby=time";
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (hazard-feed)", NULL };
  char *body = jo_get(ctx, url, hdrs, "USGS_QUAKES");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[USGS_QUAKES] parse fail\n"); return 0; }

  cJSON *feats = cJSON_GetObjectItem(root, "features");
  int emitted = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    const char *id = jo_sv(f, "id");
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    cJSON *gm = cJSON_GetObjectItem(f, "geometry");
    if (!pr) continue;
    const char *place = jo_sv(pr, "place");
    const char *turl  = jo_sv(pr, "url");
    const char *title = jo_sv(pr, "title");
    cJSON *mag = cJSON_GetObjectItem(pr, "mag");
    cJSON *tms = cJSON_GetObjectItem(pr, "time");

    double lat = 0, lon = 0; int has_geo = 0;
    cJSON *co = gm ? cJSON_GetObjectItem(gm, "coordinates") : NULL;
    if (cJSON_IsArray(co) && cJSON_GetArraySize(co) >= 2) {
      cJSON *c0 = cJSON_GetArrayItem(co, 0), *c1 = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(c0) && cJSON_IsNumber(c1)) {
        lon = c0->valuedouble; lat = c1->valuedouble; has_geo = 1;
      }
    }
    char iso[40] = {0};
    if (cJSON_IsNumber(tms)) {
      time_t t = (time_t)(tms->valuedouble / 1000.0);
      struct tm g; gmtime_r(&t, &g);
      strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", &g);
    }

    cJSON *data = cJSON_CreateObject();
    if (title) cJSON_AddStringToObject(data, "title", title);
    if (place) cJSON_AddStringToObject(data, "place", place);
    if (cJSON_IsNumber(mag)) cJSON_AddNumberToObject(data, "magnitude", mag->valuedouble);
    if (has_geo) { cJSON_AddNumberToObject(data, "lat", lat);
                   cJSON_AddNumberToObject(data, "lon", lon); }
    cJSON_AddStringToObject(data, "source", "USGS");
    char *bj = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "USGS_QUAKES");
    cJSON_AddStringToObject(props, "hazard", "earthquake");
    if (cJSON_IsNumber(mag)) cJSON_AddNumberToObject(props, "magnitude", mag->valuedouble);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key      = id ? id : (turl ? turl : (title ? title : "usgs"));
    it.title           = title ? title : place;
    it.summary         = place;
    it.body            = bj;
    it.lang            = "en";
    it.published_at    = iso[0] ? iso : NULL;
    it.link            = turl;
    it.has_geo         = has_geo; it.lat = lat; it.lon = lon;
    it.record_type     = "usgs-earthquake";
    it.properties_json = pj;
    it.tags_json       = "[\"hazard\",\"earthquake\",\"USGS\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[USGS_QUAKES] emitted %d\n", emitted);
  return 0;
}

/* ---- EMSC earthquakes (FDSNWS GeoJSON — same feature shape as USGS) ------ *
 * seismicportal fdsnws returns a GeoJSON FeatureCollection whose
 * properties carry { mag, flynn_region, time, unid, source_id, url }. */
static int hz_emsc(const source_ctx *ctx, intel_sink *sink) {
  const char *url =
    "https://www.seismicportal.eu/fdsnws/event/1/query?format=json&limit=50";
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (hazard-feed)", NULL };
  char *body = jo_get(ctx, url, hdrs, "EMSC_QUAKES");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[EMSC_QUAKES] parse fail\n"); return 0; }

  cJSON *feats = cJSON_GetObjectItem(root, "features");
  int emitted = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    const char *id = jo_sv(f, "id");
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    cJSON *gm = cJSON_GetObjectItem(f, "geometry");
    if (!pr) continue;
    const char *region = jo_sv(pr, "flynn_region");
    const char *when   = jo_sv(pr, "time");
    const char *turl   = jo_sv(pr, "url");
    const char *unid   = jo_sv(pr, "unid");
    cJSON *mag = cJSON_GetObjectItem(pr, "mag");

    double lat = 0, lon = 0; int has_geo = 0;
    cJSON *co = gm ? cJSON_GetObjectItem(gm, "coordinates") : NULL;
    if (cJSON_IsArray(co) && cJSON_GetArraySize(co) >= 2) {
      cJSON *c0 = cJSON_GetArrayItem(co, 0), *c1 = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(c0) && cJSON_IsNumber(c1)) {
        lon = c0->valuedouble; lat = c1->valuedouble; has_geo = 1;
      }
    }

    char title[300];
    if (cJSON_IsNumber(mag) && region)
      snprintf(title, sizeof title, "M%.1f %s", mag->valuedouble, region);
    else if (region) snprintf(title, sizeof title, "%s", region);
    else snprintf(title, sizeof title, "EMSC earthquake");

    cJSON *data = cJSON_CreateObject();
    if (region) cJSON_AddStringToObject(data, "region", region);
    if (cJSON_IsNumber(mag)) cJSON_AddNumberToObject(data, "magnitude", mag->valuedouble);
    if (has_geo) { cJSON_AddNumberToObject(data, "lat", lat);
                   cJSON_AddNumberToObject(data, "lon", lon); }
    cJSON_AddStringToObject(data, "source", "EMSC");
    char *bj = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "EMSC_QUAKES");
    cJSON_AddStringToObject(props, "hazard", "earthquake");
    if (cJSON_IsNumber(mag)) cJSON_AddNumberToObject(props, "magnitude", mag->valuedouble);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key      = unid ? unid : (id ? id : title);
    it.title           = title;
    it.summary         = region;
    it.body            = bj;
    it.lang            = "en";
    it.published_at    = when;
    it.link            = turl;
    it.has_geo         = has_geo; it.lat = lat; it.lon = lon;
    it.record_type     = "emsc-earthquake";
    it.properties_json = pj;
    it.tags_json       = "[\"hazard\",\"earthquake\",\"EMSC\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[EMSC_QUAKES] emitted %d\n", emitted);
  return 0;
}

/* ---- RSS scan (GVP volcanoes / GDACS disasters) ------------------------- *
 * Walk <item> blocks; pull <title>, <link>, <pubDate>, <description>, and for
 * GDACS a <georss:point>lat lon</georss:point> when present. */
static int hz_rss(const source_ctx *ctx, intel_sink *sink, const char *url,
                  const char *service, const char *rectype, const char *hazard,
                  const char *tags_json, int want_geo) {
  const char *hdrs[] = { "Accept: application/rss+xml, application/xml, text/xml, */*",
                         "User-Agent: JapanOSINT/1.0 (hazard-feed)", NULL };
  char *xml = jo_get(ctx, url, hdrs, service);
  if (!xml) return 0;
  int emitted = 0;
  const char *cur = xml;
  while (emitted < 60) {
    const char *item = jo_next_entry(&cur);
    if (!item) break;
    /* bound the scan to this item block so tag_inner doesn't leak into next */
    const char *iend = strstr(item + 1, "<item");
    const char *iend2 = strstr(item + 1, "<entry");
    if (!iend || (iend2 && iend2 < iend)) iend = iend2;

    const char *tc = item;
    char *title = jo_tag_inner(&tc, "title");
    const char *lc = item;
    char *link  = jo_tag_inner(&lc, "link");
    const char *dc = item;
    char *date  = jo_tag_inner(&dc, "pubDate");
    const char *sc = item;
    char *desc  = jo_tag_inner(&sc, "description");
    /* GVP gives EVERY item the same <link> (reports_weekly.cfm); keying on it
     * collapsed all 22 weekly volcano reports into one persisted row. The
     * <guid> (…reports_weekly.cfm#vn_<volcano number>) is the per-report
     * identity. One-time re-key for GDACS, which also carries a guid. */
    const char *gc2 = item;
    char *guid  = jo_tag_inner(&gc2, "guid");

    /* advance cur to end of this item so the outer loop moves forward */
    cur = iend ? iend : (item + 1);

    double lat = 0, lon = 0; int has_geo = 0;
    if (want_geo) {
      const char *gp = strstr(item, "georss:point");
      if (gp && (!iend || gp < iend)) {
        const char *g = strchr(gp, '>');
        if (g) { g++; double a, b; if (sscanf(g, "%lf %lf", &a, &b) == 2) {
          lat = a; lon = b; has_geo = 1; } }
      }
    }

    if (title && title[0]) {
      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", service);
      cJSON_AddStringToObject(props, "hazard", hazard);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      intel_item it = {0};
      it.remote_key      = (guid && guid[0]) ? guid
                         : ((link && link[0]) ? link : title);
      it.title           = title;
      it.summary         = desc;
      it.body            = desc;
      it.lang            = "en";
      it.published_at    = date;
      it.link            = link;
      it.has_geo         = has_geo; it.lat = lat; it.lon = lon;
      it.record_type     = rectype;
      it.properties_json = pj;
      it.tags_json       = tags_json;
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(pj);
    }
    free(title); free(link); free(date); free(desc); free(guid);
  }
  free(xml);
  fprintf(stderr, "[%s] emitted %d\n", service, emitted);
  return 0;
}

/* ---- NASA FIRMS active fires (area CSV, key-gated) ---------------------- *
 * CSV header: latitude,longitude,bright_ti4,scan,track,acq_date,acq_time,
 *             satellite,instrument,confidence,version,bright_ti5,frp,daynight
 * We parse latitude/longitude/acq_date/acq_time/confidence/frp positionally. */
static int hz_firms(const source_ctx *ctx, intel_sink *sink) {
  const char *key = jo_env("FIRMS_MAP_KEY");
  if (!key) { fprintf(stderr, "[FIRMS_GLOBAL] gated (no FIRMS_MAP_KEY)\n"); return 0; }
  char url[512];
  /* VIIRS_SNPP_NRT, world bbox (-180,-90,180,90), last 1 day */
  snprintf(url, sizeof url,
    "https://firms.modaps.eosdis.nasa.gov/api/area/csv/%s/VIIRS_SNPP_NRT/-180,-90,180,90/1",
    key);
  const char *hdrs[] = { "Accept: text/csv, */*",
                         "User-Agent: JapanOSINT/1.0 (hazard-feed)", NULL };
  char *body = jo_get(ctx, url, hdrs, "FIRMS_GLOBAL");
  if (!body) return 0;

  int emitted = 0, row = 0;
  const char *line = body;
  /* skip header line */
  const char *nl = strchr(line, '\n');
  if (nl) line = nl + 1;
  while (line && *line && emitted < 200) {
    nl = strchr(line, '\n');
    size_t llen = nl ? (size_t)(nl - line) : strlen(line);
    if (llen > 3) {
      char buf[512];
      size_t cp = llen < sizeof buf - 1 ? llen : sizeof buf - 1;
      memcpy(buf, line, cp); buf[cp] = 0;
      /* split up to 14 CSV fields */
      char *fld[14] = {0}; int nf = 0;
      char *s = buf;
      for (char *p = buf; ; p++) {
        if (*p == ',' || *p == 0 || *p == '\r') {
          int last = (*p == 0);
          char was = *p; *p = 0;
          if (nf < 14) fld[nf++] = s;
          s = p + 1;
          if (last) break;
          if (was == '\r') { if (p[1] == 0) break; }
        }
      }
      if (nf >= 7 && fld[0][0] && fld[1][0]) {
        double lat = atof(fld[0]), lon = atof(fld[1]);
        const char *acq_date = fld[5];
        const char *acq_time = fld[6];
        const char *conf = nf > 9 ? fld[9] : NULL;
        const char *frp  = nf > 12 ? fld[12] : NULL;

        cJSON *data = cJSON_CreateObject();
        cJSON_AddNumberToObject(data, "lat", lat);
        cJSON_AddNumberToObject(data, "lon", lon);
        if (acq_date && acq_date[0]) cJSON_AddStringToObject(data, "acq_date", acq_date);
        if (acq_time && acq_time[0]) cJSON_AddStringToObject(data, "acq_time", acq_time);
        if (conf && conf[0]) cJSON_AddStringToObject(data, "confidence", conf);
        if (frp && frp[0])   cJSON_AddStringToObject(data, "frp", frp);
        cJSON_AddStringToObject(data, "source", "NASA FIRMS");
        cJSON_AddStringToObject(data, "instrument", "VIIRS_SNPP_NRT");
        char *bj = cJSON_PrintUnformatted(data);
        cJSON_Delete(data);

        cJSON *props = cJSON_CreateObject();
        cJSON_AddStringToObject(props, "service", "FIRMS_GLOBAL");
        cJSON_AddStringToObject(props, "hazard", "wildfire");
        if (conf && conf[0]) cJSON_AddStringToObject(props, "confidence", conf);
        cJSON_AddBoolToObject(props, "success", 1);
        char *pj = cJSON_PrintUnformatted(props);
        cJSON_Delete(props);

        char key_[128], title[128];
        snprintf(key_, sizeof key_, "%.4f|%.4f|%s|%s", lat, lon,
                 acq_date ? acq_date : "", acq_time ? acq_time : "");
        snprintf(title, sizeof title, "Active fire %.3f, %.3f", lat, lon);
        char iso[40] = {0};
        if (acq_date && acq_date[0]) {
          if (acq_time && strlen(acq_time) >= 3)
            snprintf(iso, sizeof iso, "%sT%.2s:%sZ", acq_date, acq_time, acq_time + 2);
          else snprintf(iso, sizeof iso, "%s", acq_date);
        }

        intel_item it = {0};
        it.remote_key      = key_;
        it.title           = title;
        it.body            = bj;
        it.lang            = "en";
        it.published_at    = iso[0] ? iso : NULL;
        it.has_geo         = 1; it.lat = lat; it.lon = lon;
        it.record_type     = "firms-active-fire";
        it.properties_json = pj;
        it.tags_json       = "[\"hazard\",\"wildfire\",\"FIRMS\"]";
        if (sink->emit(sink, &it) >= 0) emitted++;
        free(bj); free(pj);
      }
    }
    row++;
    if (!nl) break;
    line = nl + 1;
  }
  free(body);
  fprintf(stderr, "[FIRMS_GLOBAL] emitted %d\n", emitted);
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *id = ctx->source_id;
  if (!id) return -1;
  if (strcmp(id, "USGS_QUAKES") == 0)     return hz_usgs(ctx, sink);
  if (strcmp(id, "EMSC_QUAKES") == 0)     return hz_emsc(ctx, sink);
  if (strcmp(id, "GVP_VOLCANOES") == 0)
    /* want_geo=1: every GVP item carries <georss:point>lat lon</georss:point>
     * for the volcano itself — it was being ignored, so an erupting volcano
     * could never pin on the map. */
    return hz_rss(ctx, sink, "https://volcano.si.edu/news/WeeklyVolcanoRSS.xml",
                  "GVP_VOLCANOES", "gvp-volcano", "volcano",
                  "[\"hazard\",\"volcano\",\"GVP\"]", 1);
  if (strcmp(id, "GDACS_DISASTERS") == 0)
    return hz_rss(ctx, sink, "https://www.gdacs.org/xml/rss.xml",
                  "GDACS_DISASTERS", "gdacs-disaster", "disaster",
                  "[\"hazard\",\"disaster\",\"GDACS\"]", 1);
  if (strcmp(id, "FIRMS_GLOBAL") == 0)    return hz_firms(ctx, sink);
  return -1;
}

#define HZ_DEF(SYM, ID, NAME, NAMEJA, IVAL, URL, DESC, FREE) \
  static const source_def SYM = { .id = ID, .collector = "environment", \
    .name = NAME, .name_ja = NAMEJA, .update_interval_sec = IVAL, .run = run, \
    .category = "environment", .type = "api", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = FREE }; \
  REGISTER_SOURCE(SYM)

HZ_DEF(hz_usgs_def, "USGS_QUAKES", "USGS Global Earthquakes", "USGS 世界地震",
  300, "https://earthquake.usgs.gov/fdsnws/event/1/query",
  "USGS FDSNWS — latest global earthquakes (GeoJSON): magnitude, place, time, geo", 1);
HZ_DEF(hz_emsc_def, "EMSC_QUAKES", "EMSC Global Earthquakes", "EMSC 世界地震",
  300, "https://www.seismicportal.eu/fdsnws/event/1/query",
  "EMSC/seismicportal FDSNWS — latest global earthquakes (JSON): magnitude, region, geo", 1);
HZ_DEF(hz_gvp_def, "GVP_VOLCANOES", "Smithsonian GVP Volcanoes", "スミソニアン GVP 火山",
  86400, "https://volcano.si.edu/news/WeeklyVolcanoRSS.xml",
  "Smithsonian Global Volcanism Program — weekly volcanic activity report (RSS)", 1);
HZ_DEF(hz_gdacs_def, "GDACS_DISASTERS", "GDACS Global Disasters", "GDACS 世界災害",
  1800, "https://www.gdacs.org/xml/rss.xml",
  "GDACS — global multi-hazard disaster alerts (RSS): quakes, cyclones, floods, geo", 1);
HZ_DEF(hz_firms_def, "FIRMS_GLOBAL", "NASA FIRMS Active Fires", "NASA FIRMS 火災",
  3600, "https://firms.modaps.eosdis.nasa.gov/",
  "NASA FIRMS — global active fires (VIIRS area CSV; needs FIRMS_MAP_KEY): geo, confidence, FRP", 1);
