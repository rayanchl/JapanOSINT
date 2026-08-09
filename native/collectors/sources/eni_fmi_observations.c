/* Finnish Meteorological Institute WFS observations (XML, not JSON).
 * Endpoint (keyless since 2022; the old apikey path is retired):
 *   https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0&request=getFeature
 *   &storedquery_id=fmi::observations::weather::simple&place=<place>
 *   &starttime=<ISO Z>&endtime=<ISO Z>
 * One BsWfs:BsWfsElement per (station, time, parameter) triple — records are
 * LONG-format, not wide. Emits one row per triple: value with the unit looked
 * up from the parameter id (t2m degC, ws_10min/wg_10min m/s, p_sea hPa, rh %,
 * r_1h mm, ri_10min mm/h, snow_aws cm, td degC, wd_10min deg, vis m).
 *
 * NaN TRAP: ParameterValue is the literal string "NaN" when missing — it is
 * rejected, never parsed into a number.
 * COORDINATE ORDER TRAP: gml:pos is "LATITUDE LONGITUDE" (EPSG:4258), the
 * REVERSE of GeoJSON order. Swapping them would put every Finnish station in
 * the Indian Ocean. Parsed lat-first below.
 * The window is bounded with starttime/endtime because the default 12 h for one
 * place is already ~570 KB.
 * Licence: CC BY 4.0 (FMI open data). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SRC "fmi-observations"

static const char *PLACES[] = { "helsinki", "tampere", "oulu", "turku", NULL };

static const struct { const char *param, *unit; } UNITS[] = {
  { "t2m",       "degC" }, { "td",        "degC" },
  { "ws_10min",  "m/s"  }, { "wg_10min",  "m/s"  },
  { "wd_10min",  "deg"  }, { "p_sea",     "hPa"  },
  { "rh",        "%"    }, { "r_1h",      "mm"   },
  { "ri_10min",  "mm/h" }, { "snow_aws",  "cm"   },
  { "vis",       "m"    }, { "n_man",     "1/8"  },
  { NULL, NULL }
};

static const char *unit_of(const char *p) {
  for (int i = 0; UNITS[i].param; i++)
    if (strcmp(UNITS[i].param, p) == 0) return UNITS[i].unit;
  return NULL;
}

static int collect_place(const source_ctx *ctx, intel_sink *sink,
                         const char *place, const char *t0, const char *t1,
                         int *fetched) {
  char url[448];
  snprintf(url, sizeof url,
    "https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0&request=getFeature"
    "&storedquery_id=fmi::observations::weather::simple&place=%s"
    "&starttime=%s&endtime=%s", place, t0, t1);
  char *xml = feed_get_text(ctx->http, url, 45000);
  if (!xml) return 0;
  *fetched = 1;

  int n = 0;
  const char *cur = xml, *inner = NULL;
  int ilen = 0;
  while ((cur = html_block(cur, "BsWfsElement", &inner, &ilen)) != NULL) {
    if (ilen <= 0) continue;
    char blk[2048];
    int copy = ilen < (int)sizeof blk - 1 ? ilen : (int)sizeof blk - 1;
    memcpy(blk, inner, (size_t)copy);
    blk[copy] = 0;

    char pos[128], when[64], pname[64], pval[64];
    if (!html_tag(blk, "pos", pos, sizeof pos)) continue;
    if (!html_tag(blk, "Time", when, sizeof when)) continue;
    if (!html_tag(blk, "ParameterName", pname, sizeof pname)) continue;
    if (!html_tag(blk, "ParameterValue", pval, sizeof pval)) continue;

    char *nm = html_strip(pname); char *vl = html_strip(pval);
    char *tm = html_strip(when);  char *ps = html_strip(pos);
    if (!nm || !vl || !tm || !ps) { free(nm); free(vl); free(tm); free(ps); continue; }

    /* "NaN" is the missing-value token: never parse it as a number */
    int bad = (strcmp(vl, "NaN") == 0 || strcmp(vl, "nan") == 0 || !vl[0]);
    char *e = NULL;
    double v = bad ? 0 : strtod(vl, &e);
    if (bad || e == vl) { free(nm); free(vl); free(tm); free(ps); continue; }

    /* gml:pos is "LATITUDE LONGITUDE" — lat first, unlike GeoJSON */
    double lat = 0, lon = 0;
    if (sscanf(ps, "%lf %lf", &lat, &lon) != 2 ||
        lat < -90 || lat > 90 || lon < -180 || lon > 180) {
      free(nm); free(vl); free(tm); free(ps); continue;
    }
    const char *unit = unit_of(nm);
    if (!unit) { free(nm); free(vl); free(tm); free(ps); continue; }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "place_query", place);
    cJSON_AddStringToObject(p, "parameter", nm);
    cJSON_AddNumberToObject(p, "value", v);
    cJSON_AddStringToObject(p, "unit", unit);
    cJSON_AddStringToObject(p, "observed_at_utc", tm);
    cJSON_AddStringToObject(p, "country", "FI");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[160], title[256];
    snprintf(key, sizeof key, "%.5f,%.5f|%s|%s", lat, lon, nm, tm);
    snprintf(title, sizeof title, "FMI %s %s = %.2f %s", place, nm, v, unit);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = tm;
    row.lang            = "en";
    row.link            = "https://en.ilmatieteenlaitos.fi/open-data";
    row.record_type     = "weather-station";
    row.has_geo         = 1;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"weather\",\"finland\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
    free(nm); free(vl); free(tm); free(ps);
  }
  free(xml);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL);
  time_t then = now - 3600;
  struct tm a, b;
  gmtime_r(&then, &a); gmtime_r(&now, &b);
  char t0[32], t1[32];
  strftime(t0, sizeof t0, "%Y-%m-%dT%H:%M:%SZ", &a);
  strftime(t1, sizeof t1, "%Y-%m-%dT%H:%M:%SZ", &b);

  int total = 0, fetched = 0;
  for (int i = 0; PLACES[i]; i++)
    total += collect_place(ctx, sink, PLACES[i], t0, t1, &fetched);
  if (!fetched) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  fprintf(stderr, "[" SRC "] emitted %d\n", total);
  return 0;
}

static const source_def eni_fmi_observations_def = {
  .id = SRC, .collector = "environment",
  .name = "Finnish Meteorological Institute WFS observations",
  .update_interval_sec = 1800, .run = run,
  .category = "environment", .type = "api",
  .url = "https://opendata.fmi.fi/wfs?storedquery_id=fmi::observations::weather::simple",
  .description = "Finnish national weather station observations (10-minute) via the FMI open WFS: temperature, wind, gust, pressure, humidity, precipitation, snow depth, with coordinates.",
  .license = "CC BY 4.0 (FMI open data), keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_fmi_observations_def)
