/* Environment and Climate Change Canada — two collections of the standards
 * compliant OGC API - Features service at api.weather.gc.ca.
 *
 * Endpoints (keyless, clean GeoJSON):
 *   .../collections/hydrometric-realtime/items?limit=500&f=json&sortby=-DATETIME&datetime=<1h ago>/..
 *   .../collections/swob-realtime/items?limit=500&f=json&sortby=-date_tm-value&datetime=<1h ago>/..
 * Emits:
 *   hydrometric — STATION_NUMBER, STATION_NAME, PROV_TERR_STATE_LOC, DATETIME,
 *                 LEVEL (m), DISCHARGE (m3/s), LEVEL_SYMBOL_EN + the station
 *                 Point geometry.
 *   swob        — stn_nam-value, tc_id-value, wmo_synop_id-value, date_tm-value,
 *                 every other '<element>-value' observation the station
 *                 reported, the SWOB-ML source url, and the station Point.
 * Licence: Open Government Licence — Canada 2.0, reuse with attribution.
 *
 * parse_notes honoured:
 *  - The item id is PER-OBSERVATION ("05AB047.2026-07-02T07:00:00Z"), so the
 *    SAME station repeats with identical geometry across consecutive
 *    timestamps. Both collections are de-duplicated to the newest observation
 *    per station (STATION_NUMBER / tc_id-value); without that, hundreds of
 *    markers stack on one point.
 *  - A default unsorted request returns OLD timestamps, so a rolling datetime
 *    filter is applied to both requests.
 *
 * ORDER, WINDOW AND PAGING (2026-10-12). Both requests were a single
 * `?limit=500` over a one-to-two-day window. Measured against the live
 * service, that window holds 600,297 hydrometric and 478,334 SWOB
 * observations, so each run kept 500 of them — the seventh discard shape
 * (tools/audit_url_limits.py; docs/measured-url-limits-2026-10-11.tsv). And
 * WHICH 500 was the worse half: the service's own ordering is not newest-first
 * even with the datetime filter, so the "current observation per station" this
 * collector exists to produce was computed from an arbitrary slice.
 *
 * Three changes, and they depend on each other:
 *   - `sortby=-DATETIME` / `sortby=-date_tm-value` makes the response
 *     newest-first, so the records a bound keeps are the ones wanted. The SWOB
 *     key keeps its '-value' suffix; plain `-date_tm` is HTTP 400 "bad sort
 *     property".
 *   - the window is ONE HOUR, not one-to-two days. A day of per-minute
 *     observations is ~60x redundant for a question about the current reading,
 *     and the narrower window is what lets the walk finish: measured
 *     2026-10-12, one hour held 4,743 hydrometric observations (inside the
 *     ceiling) and 18,879 SWOB (not, and disclosed). Both schedules — 30 min
 *     and 60 min — stay inside the window, so no station falls between runs.
 *   - the walk follows the service's own `links[rel=next]` to a ceiling and
 *     files a collector-truncation-notice naming numberMatched when it does
 *     not reach the end.
 *
 * Paging the whole 600,297 was considered and rejected: at `limit=10000` one
 * SWOB page is 82 MB, so a complete read of its window is ~160 MB every hour
 * for data that is 60x redundant against the question. A stated bound over the
 * newest records is the honest read; silence over an arbitrary 500 was not.
 *  - SWOB GEOMETRY HAS THREE ORDINATES: [lon, lat, elevation_m]. Code that
 *    validates array length == 2 drops every record; geoeo_geom() reads the
 *    first two and ignores the rest.
 *  - SWOB property names use the unusual '<element>-value' / '<element>-uom'
 *    convention, so a lookup for "stn_nam" finds nothing — the "-value" suffix
 *    is spelled out at every use.
 *  - LEVEL_SYMBOL_EN/FR and DISCHARGE are frequently null and are dropped
 *    rather than stringified.
 *  - 'alerts' is NOT a valid collection id on this service (404) and is not
 *    requested.
 */
#include "source.h"
#include "lib/jocore.h"
#include "lib/feedlib.h"
#include "lib/seenset.h"
#include "geoeo_common.inc"
#include "_timefmt.inc"

/* The window start, `hours` back, to the second. It used to be rendered to the
 * HOUR from a whole number of DAYS, which was fine for a two-day window and is
 * not for the one-hour one below: truncating to the hour makes a "1 hour"
 * window anywhere from 1 to 2 hours wide depending on when the run fires. */
static int iso_hours_ago(int hours, char *out, size_t n) {
  return jo_time_fmt(time(NULL) - (time_t)hours * 3600, "%Y-%m-%dT%H:%M:%SZ",
                     out, n) != NULL;
}

/* The next page the SERVICE published: OGC API - Features puts it in the
 * `links` array under rel=next, selected by rel and never by position. Only an
 * absolute http(s) URL is accepted, for the reason lib/pagewalk.c gives — a
 * relative or templated value would have to be resolved against assumptions
 * about the API. malloc'd; caller frees. */
static char *ogc_next_link(const cJSON *doc) {
  const cJSON *arr = cJSON_GetObjectItem(doc, "links"), *l;
  if (!cJSON_IsArray(arr)) return NULL;
  cJSON_ArrayForEach(l, arr) {
    const cJSON *rel = cJSON_GetObjectItem(l, "rel");
    const cJSON *href = cJSON_GetObjectItem(l, "href");
    if (!cJSON_IsString(rel) || !cJSON_IsString(href)) continue;
    if (strcasecmp(rel->valuestring, "next") != 0) continue;
    const char *u = href->valuestring;
    if (!u) return NULL;
    if (strncasecmp(u, "http://", 7) && strncasecmp(u, "https://", 8))
      return NULL;
    return strdup(u);
  }
  return NULL;
}

/* ---- the newest-first dedupe ---------------------------------------------
 *
 * This used to answer "is a newer observation for this station present" by
 * scanning the WHOLE feature array per feature (superseded(), removed). That
 * is O(n^2), which was tolerable at the 500 features one request held and is
 * not at the 10,000 the paged walk collects — a hundred million string
 * compares per run.
 *
 * With the request sorted newest-first the question collapses: the FIRST
 * feature a station appears in IS its current observation, so one pass plus
 * lib/seenset.c answers it. That set GROWS, which is the property that matters
 * here — a fixed ring would start dropping stations once it filled, which is
 * the dedupe-ring violation docs/SOURCE_EXHAUSTIVENESS.md is about.
 *
 * The set is keyed on the SAME value the record is keyed on (see each caller).
 * Keying the dedupe on one field and the row on another is how `emitted` and
 * `stored` come apart silently — measured here: deduping on `tc_id-value`
 * while rows keyed on `tc_id-value` OR `stn_nam-value` let every tc_id-less
 * station through the dedupe and collapse at the sink, 25 of 384 on
 * 2026-10-12. */

#define OGC_PAGE_SIZE 500
/* 20 pages = 10,000 observations, newest first. The window below is sized so
 * that number covers it where it can; where it cannot, the shortfall is a
 * disclosed truncation notice naming numberMatched, not silence. */
#define OGC_PAGE_MAX 20  /* exhaustive-ok: page ceiling; a stop at it files a collector-truncation-notice naming numberMatched */
/* One hour. See the WINDOW note in the header for why it is not two days. */
#define OGC_WINDOW_HOURS 1

/* Fetch the window newest-first and follow the service's own rel=next links,
 * merging every page's features into the FIRST document, which is returned
 * (caller owns and deletes). The merge matters: the newest-per-station pass in
 * each caller needs one array, not a page at a time, or a station straddling a
 * page boundary would be emitted twice with the older observation winning.
 *
 * `*matched` gets numberMatched when the service published it, else -1, and a
 * shortfall against it is disclosed here so neither caller has to remember. */
static cJSON *ogc_collect(const source_ctx *ctx, intel_sink *sink,
                          const char *sid, const char *collection,
                          const char *sortby, long *matched) {
  *matched = -1;
  char since[40];
  /* The rolling datetime filter is not optional here - without it the service
   * answers with OLD timestamps (see the header note). */
  if (!iso_hours_ago(OGC_WINDOW_HOURS, since, sizeof since)) {
    fprintf(stderr, "[%s] cannot render the query window as a date\n", sid);
    return NULL;
  }
  char first_url[512];
  snprintf(first_url, sizeof first_url,
           "https://api.weather.gc.ca/collections/%s/items"
           "?limit=%d&f=json&sortby=%s&datetime=%s/..",
           collection, OGC_PAGE_SIZE, sortby, since);

  cJSON *acc = NULL, *acc_feats = NULL;
  char *cur = strdup(first_url);
  if (!cur) return NULL;
  int pages = 0, collected = 0, stopped_short = 0;

  while (cur) {
    cJSON *doc = feed_get_json(ctx->http, cur, 60000);
    if (!doc) {
      if (pages == 0) {
        fprintf(stderr, "[%s] fetch/parse failed\n", sid);
        free(cur);
        return NULL;
      }
      /* A later page failing is not an error: keep what we have, say so. */
      fprintf(stderr, "[%s] page %d failed; keeping %d observations\n", sid,
              pages + 1, collected);
      stopped_short = 1;
      break;
    }
    cJSON *feats = cJSON_GetObjectItem(doc, "features");
    if (!cJSON_IsArray(feats)) {
      fprintf(stderr, "[%s] no features array\n", sid);
      cJSON_Delete(doc);
      if (pages == 0) { free(cur); return NULL; }
      stopped_short = 1;
      break;
    }
    int held = cJSON_GetArraySize(feats);
    double nm = 0;
    if (geoeo_num(doc, "numberMatched", &nm)) *matched = (long)nm;

    char *next = ogc_next_link(doc);
    if (next && strcmp(next, cur) == 0) { free(next); next = NULL; }

    if (!acc) {
      acc = doc;                              /* page 1 IS the accumulator */
      acc_feats = feats;
    } else {
      /* Move, never copy: detaching leaves ownership with acc_feats. */
      while (cJSON_GetArraySize(feats) > 0)
        cJSON_AddItemToArray(acc_feats, cJSON_DetachItemFromArray(feats, 0));
      cJSON_Delete(doc);
    }
    pages++;
    collected += held;

    free(cur);
    cur = NULL;
    /* A short page is the end of the data; re-asking would be a wasted hop. */
    if (held < OGC_PAGE_SIZE) { free(next); break; }
    if (!next) { stopped_short = 1; break; }
    if (pages >= OGC_PAGE_MAX) { free(next); stopped_short = 1; break; }
    cur = next;
  }
  free(cur);
  if (!acc) return NULL;

  if (stopped_short || (*matched >= 0 && *matched > collected))
    jo_trunc_notice(sink, sid, first_url, collected, *matched,
                    "the observation window holds more than this walk "
                    "collected: the page ceiling, a failed page, or a service "
                    "that offered no next link stopped it",
                    "narrow OGC_WINDOW_HOURS so the window fits the ceiling, "
                    "or raise OGC_PAGE_MAX — the records kept are the NEWEST, "
                    "because the request is sorted descending");

  fprintf(stderr, "[%s] collected %d observations across %d page(s)"
                  " of a window holding %ld\n", sid, collected, pages,
          *matched);
  return acc;
}

static int hydro_run(const source_ctx *ctx, intel_sink *sink) {
  long matched = -1;
  cJSON *doc = ogc_collect(ctx, sink, "gc-hydrometric", "hydrometric-realtime",
                           "-DATETIME", &matched);
  if (!doc) return -1;
  cJSON *feats = cJSON_GetObjectItem(doc, "features");

  static const char *KEYS[] = { "STATION_NUMBER", "STATION_NAME",
                                "PROV_TERR_STATE_LOC", "DATETIME",
                                "LEVEL_SYMBOL_EN", "DISCHARGE_SYMBOL_EN",
                                "IDENTIFIER", NULL };
  seen_set seen = {0};
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    const char *stn = geoeo_str(p, "STATION_NUMBER");
    const char *name = geoeo_str(p, "STATION_NAME");
    if (!stn) continue;
    /* Newest first, so the first time a station appears is its current
     * observation; a later one is the superseded reading. `stn` is also this
     * row's remote_key, so the dedupe and the identity cannot disagree. */
    if (!seen_add(&seen, stn)) continue;

    const char *when = geoeo_str(p, "DATETIME");
    double level = 0, disch = 0;
    int has_level = geoeo_numlax(p, "LEVEL", &level);
    int has_disch = geoeo_numlax(p, "DISCHARGE", &disch);

    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, p, KEYS);
    if (has_level) cJSON_AddNumberToObject(props, "LEVEL_m", level);
    if (has_disch) cJSON_AddNumberToObject(props, "DISCHARGE_m3s", disch);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    cJSON_AddStringToObject(props, "provider",
                            "Environment and Climate Change Canada");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[320];
    if (has_level)
      snprintf(title, sizeof title, "%s (%s) — level %.3f m",
               name ? name : stn, stn, level);
    else if (has_disch)
      snprintf(title, sizeof title, "%s (%s) — discharge %.1f m3/s",
               name ? name : stn, stn, disch);
    else
      snprintf(title, sizeof title, "%s (%s)", name ? name : stn, stn);

    char summary[160];
    if (has_level && has_disch)
      snprintf(summary, sizeof summary, "discharge %.1f m3/s", disch);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key = stn;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.lang = "en";
    it.published_at = when;
    it.record_type = "hydrometric-station";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"water\",\"hydrometric\",\"canada\",\"eccc\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  seen_free(&seen);
  fprintf(stderr, "[gc-hydrometric] emitted %d stations (newest per station)\n",
          n);
  return 0;
}

static int swob_run(const source_ctx *ctx, intel_sink *sink) {
  long matched = -1;
  cJSON *doc = ogc_collect(ctx, sink, "gc-swob-observations", "swob-realtime",
                           /* the sort key carries the '-value' suffix too;
                            * plain "-date_tm" is HTTP 400 "bad sort
                            * property". */
                           "-date_tm-value", &matched);
  if (!doc) return -1;
  cJSON *feats = cJSON_GetObjectItem(doc, "features");

  seen_set seen = {0};
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    /* '<element>-value' naming: a lookup for "stn_nam" finds nothing. */
    const char *tcid = geoeo_str(p, "tc_id-value");
    const char *name = geoeo_str(p, "stn_nam-value");
    const char *wmo = geoeo_str(p, "wmo_synop_id-value");
    const char *when = geoeo_str(p, "date_tm-value");
    if (!tcid && !name) continue;
    /* Newest first: the first appearance of a station is its current SWOB.
     * Keyed on `tcid ? tcid : name` — this row's own remote_key below — and
     * NOT on tcid alone: a station with no tc_id would then pass the dedupe
     * every time it reported and collapse at the sink instead. */
    const char *station_key = tcid ? tcid : name;
    if (!seen_add(&seen, station_key)) continue;

    /* [lon, lat, elevation_m] — three ordinates, not two. */
    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_scalars(props, p);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    cJSON_AddStringToObject(props, "provider",
                            "Environment and Climate Change Canada");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[320];
    snprintf(title, sizeof title, "%s%s%s%s", name ? name : tcid,
             tcid ? " (" : "", tcid ? tcid : "", tcid ? ")" : "");

    char summary[160];
    snprintf(summary, sizeof summary, "%s%s", wmo ? "WMO " : "",
             wmo ? wmo : "");

    intel_item it = {0};
    it.remote_key = station_key;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.link = geoeo_str(p, "url");
    it.lang = "en";
    it.published_at = when;
    it.record_type = "surface-weather-observation";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"weather\",\"observation\",\"canada\",\"swob\",\"eccc\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  seen_free(&seen);
  fprintf(stderr, "[gc-swob-observations] emitted %d stations\n", n);
  return 0;
}

static const source_def geoeo_gc_hydro_def = {
  .id = "gc-hydrometric", .collector = "environment",
  .name = "Environment Canada Hydrometric Realtime (OGC API)",
  .update_interval_sec = 1800, .run = hydro_run,
  .category = "environment", .type = "api",
  .url = "https://api.weather.gc.ca/collections/hydrometric-realtime/items?limit=500&f=json&sortby=-DATETIME&datetime={date-1h}/..",
  .description = "ECCC's OGC API - Features endpoint for real-time water level and discharge at national hydrometric stations, returned as clean GeoJSON.",
  .license = "Open Government Licence — Canada 2.0; reuse with attribution.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_gc_hydro_def)

static const source_def geoeo_gc_swob_def = {
  .id = "gc-swob-observations", .collector = "environment",
  .name = "Environment Canada Surface Weather Observations (SWOB)",
  .update_interval_sec = 3600, .run = swob_run,
  .category = "environment", .type = "api",
  .url = "https://api.weather.gc.ca/collections/swob-realtime/items?limit=500&f=json&sortby=-date_tm-value&datetime={date-1h}/..",
  .description = "Per-minute and hourly automated surface weather observations from the Canadian station network as GeoJSON points, with the raw SWOB-ML document linked per record.",
  .license = "Open Government Licence — Canada 2.0.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_gc_swob_def)
