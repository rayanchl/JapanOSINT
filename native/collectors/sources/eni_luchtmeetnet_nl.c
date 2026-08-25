/* RIVM Luchtmeetnet — Dutch national air quality measurements.
 * Endpoints (keyless):
 *   https://api.luchtmeetnet.nl/open_api/measurements?page=<n>
 *     &order_by=timestamp_measured&order_direction=desc&start=<iso>&end=<iso>
 *   https://api.luchtmeetnet.nl/open_api/stations/<station_number>
 * Emits one row per measurement returned: value with its UNIT
 *   (ug/m3 for PM10/PM25/NO2/NO/O3/SO2/BC; mg/m3 for CO), the pollutant
 *   formula, the station number and the measurement window.
 *
 * PAGINATION, and the redirect that hid it. The response carries
 * `pagination.last_page`, and over the API's default seven-day window it is 94
 * — ~94,000 measurements, of which this collector read the first 1,000, about
 * one percent, with no notice that the other 93 pages existed.
 *
 * The reason a naive fix does not work is the redirect: a request WITHOUT an
 * explicit window answers HTTP 302 to
 *   /open_api/measurements?station_number=&order_direction=desc&formula=
 *   &order_by=timestamp_measured&start=<now-7d>&end=<now>
 * — note there is no `page` in the target. So `?page=2` alone follows the
 * redirect and comes back as page 1 again, byte-identical, forever. The window
 * has to be sent explicitly for `page` to survive, so the walk below pins the
 * same seven-day window the API itself redirects to, and only then pages.
 * Deep pages can answer HTTP 504; a walk that ends that way says so as a
 * collector-truncation-notice rather than looking complete.
 *
 * GEO (R2): coordinates are NOT in the measurements response. They are
 * resolved per station from /open_api/stations/{number}
 * (data.geometry.coordinates = [lon, lat]); when that lookup fails the row is
 * emitted WITHOUT geometry — never with a guessed location. A row whose
 * station was never looked up because the per-run station budget was spent
 * carries "station_geometry_pending": true, so an absent pin is legible as
 * "not resolved" rather than "no coordinates exist".
 * Station lookups are bounded (JO_LUCHTMEETNET_STATIONS, default 128).
 * Licence: RIVM Luchtmeetnet open API, keyless, open government data. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "_timefmt.inc"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "luchtmeetnet-nl"
#define MAXST 128
/* Runaway guard only: the walk normally ends on pagination.last_page (94 at
 * the last probe). A stop here is disclosed as a truncation notice. */
#define LM_MAX_PAGES 250   /* exhaustive-ok: page-walk runaway guard; an early stop emits a collector-truncation-notice */

/* The request window, and why it is NOT the seven days the API's own redirect
 * picks. Seven days is 94 pages of 1,000 measurements — ~94,000 rows, most of
 * them re-emitted unchanged — and this source runs hourly, so a full walk of
 * that window could not finish inside its own cadence. Asking instead for the
 * last two hours is ~2 pages, walks to the end every time, and with runs an
 * hour apart the windows overlap by 2x, so consecutive runs TILE the timeline
 * with no gap: nothing is discarded rather than something being discarded
 * quietly. (Rows are keyed station|formula|timestamp, so the overlap upserts
 * rather than duplicating.) Raise it with $JO_LUCHTMEETNET_WINDOW_SEC to
 * backfill a longer stretch. */
#define LM_WINDOW_SEC 7200

/* `tried` records that the station endpoint WAS called for this station, so a
 * row without a pin can say whether the lookup failed or never happened. */
typedef struct { char id[24]; double lat, lon; int ok, tried; } st_t;

/* ug/m3 for everything the network reports except CO, which is mg/m3. */
static const char *unit_for(const char *formula) {
  if (formula && strcmp(formula, "CO") == 0) return "mg/m3";
  return "ug/m3";
}

static st_t *lookup(const source_ctx *ctx, st_t *cache, int *ncache,
                    int budget, const char *num) {
  for (int i = 0; i < *ncache; i++)
    if (strcmp(cache[i].id, num) == 0) return &cache[i];
  if (*ncache >= MAXST || *ncache >= budget) return NULL;
  st_t *e = &cache[(*ncache)++];
  snprintf(e->id, sizeof e->id, "%s", num);
  e->ok = 0;
  e->tried = 1;
  char url[192];
  snprintf(url, sizeof url, "https://api.luchtmeetnet.nl/open_api/stations/%s", num);
  cJSON *doc = feed_get_json(ctx->http, url, 20000);
  if (!doc) return e;
  cJSON *d = cJSON_GetObjectItem(doc, "data");
  cJSON *g = d ? cJSON_GetObjectItem(d, "geometry") : NULL;
  cJSON *c = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
  if (cJSON_IsArray(c) && cJSON_GetArraySize(c) >= 2) {
    cJSON *x = cJSON_GetArrayItem(c, 0), *y = cJSON_GetArrayItem(c, 1);
    if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
      e->lon = x->valuedouble; e->lat = y->valuedouble;   /* [lon, lat] */
      if (e->lat >= -90 && e->lat <= 90 && e->lon >= -180 && e->lon <= 180 &&
          (e->lat != 0 || e->lon != 0)) e->ok = 1;
    }
  }
  cJSON_Delete(doc);
  return e;
}

/* One page of measurements. Returns rows emitted; *seen is what the page
 * carried, *last_page the pagination total when the response declared one. */
static int lm_page(const source_ctx *ctx, intel_sink *sink, st_t *cache,
                   int *ncache, int budget, int page, const char *start,
                   const char *end, int *seen, int *last_page, int *failed) {
  char url[320];
  snprintf(url, sizeof url,
    "https://api.luchtmeetnet.nl/open_api/measurements?page=%d"
    "&order_by=timestamp_measured&order_direction=desc&start=%s&end=%s",
    page, start, end);
  cJSON *doc = feed_get_json(ctx->http, url, 45000);
  if (!doc) { *failed = 1; return 0; }

  cJSON *pg = cJSON_GetObjectItem(doc, "pagination");
  cJSON *lp = pg ? cJSON_GetObjectItem(pg, "last_page") : NULL;
  if (cJSON_IsNumber(lp)) *last_page = (int)lp->valuedouble;

  cJSON *data = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(data)) { cJSON_Delete(doc); *failed = 1; return 0; }
  *seen = cJSON_GetArraySize(data);

  int n = 0;
  cJSON *m;
  cJSON_ArrayForEach(m, data) {
    const char *num = jo_sv(m, "station_number");
    const char *formula = jo_sv(m, "formula");
    cJSON *val = cJSON_GetObjectItem(m, "value");
    const char *when = jo_sv(m, "timestamp_measured");
    if (!num || !formula || !cJSON_IsNumber(val)) continue;

    st_t *st = lookup(ctx, cache, ncache, budget, num);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "station_number", num);
    cJSON_AddStringToObject(p, "pollutant", formula);
    cJSON_AddNumberToObject(p, "value", val->valuedouble);
    cJSON_AddStringToObject(p, "unit", unit_for(formula));
    if (when) cJSON_AddStringToObject(p, "timestamp_measured", when);
    const char *ws = jo_sv(m, "timestamp_measured_start");
    const char *we = jo_sv(m, "timestamp_measured_end");
    if (ws) cJSON_AddStringToObject(p, "window_start", ws);
    if (we) cJSON_AddStringToObject(p, "window_end", we);
    cJSON_AddStringToObject(p, "country", "NL");
    /* No pin because the station was never looked up (budget spent) is a
     * different fact from no pin because the station has no coordinates. Say
     * which, in the row, instead of leaving has_geo=0 to mean both. */
    if (!st || (!st->ok && !st->tried))
      cJSON_AddBoolToObject(p, "station_geometry_pending", 1);
    else if (st->ok)
      cJSON_AddStringToObject(p, "geo_precision", "station-point");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[96], title[224];
    snprintf(key, sizeof key, "%s|%s|%s", num, formula, when ? when : "");
    snprintf(title, sizeof title, "%s %s = %.2f %s", num, formula,
             val->valuedouble, unit_for(formula));

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = when;
    row.lang            = "en";
    row.link            = "https://www.luchtmeetnet.nl/";
    row.record_type     = "air-quality-measurement";
    if (st && st->ok) { row.has_geo = 1; row.lat = st->lat; row.lon = st->lon; }
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"air-quality\",\"netherlands\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int budget = MAXST;
  const char *env = getenv("JO_LUCHTMEETNET_STATIONS");
  if (env && *env) { int b = atoi(env); if (b > 0 && b <= MAXST) budget = b; }

  int max_pages = LM_MAX_PAGES;
  const char *penv = getenv("JO_LUCHTMEETNET_PAGES");
  if (penv && *penv) { int v = atoi(penv); if (v > 0) max_pages = v; }

  /* Pin the window explicitly, so `page` survives the 302 (see the header
   * note), and keep it to the run cadence so the walk can actually reach the
   * last page. */
  long window = LM_WINDOW_SEC;
  const char *wenv = getenv("JO_LUCHTMEETNET_WINDOW_SEC");
  if (wenv && *wenv) { long w = atol(wenv); if (w > 0) window = w; }
  time_t now = time(NULL);
  char start[32], end[32];
  /* ISO-8601 UTC, the shape the API's own redirect uses. The window has to be
   * pinned for `page` to survive the 302, so an unrenderable one is a dead
   * run rather than a narrower one. */
  if (!jo_time_fmt(now - (time_t)window, "%Y-%m-%dT%H:%M:%SZ", start, sizeof start) ||
      !jo_time_fmt(now, "%Y-%m-%dT%H:%M:%SZ", end, sizeof end)) {
    fprintf(stderr, "[" SRC "] cannot render the query window as a date\n");
    return -1;
  }

  st_t cache[MAXST]; int ncache = 0;
  int n = 0, pages = 0, last_page = -1, stopped_early = 0;

  for (int page = 1; page <= max_pages; page++) {
    int seen = 0, failed = 0;
    n += lm_page(ctx, sink, cache, &ncache, budget, page, start, end, &seen,
                 &last_page, &failed);
    if (failed) {
      if (page == 1) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
      /* Deep pages answer HTTP 504 on this API often enough that a silent
       * stop here would be the normal case, not the exception. */
      fprintf(stderr, "[" SRC "] page %d failed after %d rows\n", page, n);
      stopped_early = 1;
      break;
    }
    pages++;
    if (seen == 0) break;
    if (last_page > 0 && page >= last_page) break;
    if (page == max_pages) stopped_early = 1;
  }

  fprintf(stderr, "[" SRC "] emitted %d over %d of %d page(s), %d station "
                  "lookup(s)\n", n, pages, last_page, ncache);
  if (stopped_early) {
    /* records_available stays unknown on purpose: the API declares a page
     * count, not a record count, and multiplying pages by an average would be
     * a made-up number. The page arithmetic goes in the reason, where it is
     * plainly the page count it actually is. */
    char reason[256];
    snprintf(reason, sizeof reason,
             "the measurement page walk read %d of %d page(s) and stopped "
             "early — a page failed (this API answers HTTP 504 on deep pages) "
             "or the page ceiling was reached", pages, last_page);
    jo_trunc_notice(sink, SRC,
                    "https://api.luchtmeetnet.nl/open_api/measurements", n, -1,
                    reason,
                    "re-run the collector, or raise $JO_LUCHTMEETNET_PAGES");
  }
  return 0;
}

static const source_def eni_luchtmeetnet_nl_def = {
  .id = SRC, .collector = "environment",
  .name = "RIVM Luchtmeetnet Netherlands air quality",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.luchtmeetnet.nl/open_api/measurements",
  .description = "Hourly regulatory air quality measurements (PM10, PM2.5, NO2, NO, O3, SO2, black carbon) from the Dutch national network, with station coordinates resolved from the stations endpoint.",
  .license = "RIVM Luchtmeetnet open API, keyless, open government data.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_luchtmeetnet_nl_def)
