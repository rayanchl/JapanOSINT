/* collectors/sources/tsp_fcc_fss_earth_stations.c
 * FCC protected fixed-satellite-service earth station registrations — the US
 * C-band earth stations claiming protection in the 3.7-4.2 GHz band.
 * Endpoint: https://opendata.fcc.gov/resource/acbv-jbb4.json?$limit=5000&$offset=N
 *   (keyless Socrata)
 * Emits: registration number, location status, call sign and licensee, lower
 *   and upper frequency (MHz), pointing azimuth and elevation angle, antenna
 *   gain, site elevation, antenna height AGL/AMSL, the GSO satellite longitude
 *   the dish is looking at, the telemetry/tracking/command flag, certification
 *   date, and the dish coordinates.
 *
 * TRAPS handled, quoted from the manifest:
 *  - "every value is a STRING, including numbers, and frequency fields carry
 *    thousands separators ('3,700.1' MHz) — strip commas before strtod":
 *    num_str() below removes ',' before conversion.
 *  - "Geo: latitude_decimal/longitude_decimal are the antenna site" — this is
 *    the dish, not the licensee's mailing address, so it is R2-safe geometry.
 *  - "Beware junk rows: the first record read is at -89.999997/-179.016944
 *    with a licensee of 'ANTWANWILLIAMS.US' — filter obvious null-island/pole
 *    values": rows at 0/0 or beyond +/-85 degrees latitude are dropped and
 *    counted rather than plotted.
 * Licence: FCC open data (Socrata) — US Government public domain, keyless.
 *   An app token is optional and only raises the anonymous rate limit.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FSS_BASE "https://opendata.fcc.gov/resource/acbv-jbb4.json"
#define PAGE_SIZE 5000
#define PAGES 4

/* Socrata numbers arrive as strings, sometimes with thousands separators and
 * unit suffixes. Strip ',' then strtod. Returns 1 on success. */
static int num_str(const char *s, double *out) {
  if (!s) return 0;
  char buf[64];
  size_t j = 0;
  for (const char *p = s; *p && j + 1 < sizeof buf; p++)
    if (*p != ',') buf[j++] = *p;
  buf[j] = '\0';
  char *e = NULL;
  double d = strtod(buf, &e);
  if (!e || e == buf) return 0;
  *out = d;
  return 1;
}
static void add_num_str(cJSON *o, const char *k, const char *v) {
  double d;
  if (v && num_str(v, &d)) cJSON_AddNumberToObject(o, k, d);
  else if (v && *v) cJSON_AddStringToObject(o, k, v);
}

static int emit_page(cJSON *arr, intel_sink *sink, int *junk) {
  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    const char *num  = jo_sv(r, "number");
    const char *call = jo_sv(r, "call_sign");
    const char *lic  = jo_sv(r, "call_sign_licensee_name");
    if (!num && !call) continue;                 /* no identity -> no row */

    double lat, lon;
    if (!num_str(jo_sv(r, "latitude_decimal"), &lat) ||
        !num_str(jo_sv(r, "longitude_decimal"), &lon)) continue;
    if ((lat == 0.0 && lon == 0.0) || lat > 85.0 || lat < -85.0 ||
        lon < -180.0 || lon > 180.0) { (*junk)++; continue; }

    cJSON *pr = cJSON_CreateObject();
    jo_add_str(pr, "registration_number", num);
    jo_add_str(pr, "call_sign", call);
    jo_add_str(pr, "licensee", lic);
    jo_add_str(pr, "location_status", jo_sv(r, "location_status"));
    jo_add_str(pr, "latitude_dms", jo_sv(r, "latitude_dms"));
    jo_add_str(pr, "longitude_dms", jo_sv(r, "longitude_dms"));
    add_num_str(pr, "lower_frequency_mhz", jo_sv(r, "lower_frequency"));
    add_num_str(pr, "upper_frequency_mhz", jo_sv(r, "upper_frequency"));
    add_num_str(pr, "pointing_azimuth_deg", jo_sv(r, "pointing_azimuth"));
    add_num_str(pr, "pointing_elevation_deg", jo_sv(r, "pointing_elevation_angle"));
    add_num_str(pr, "antenna_gain_dbi", jo_sv(r, "antenna_gain"));
    add_num_str(pr, "antenna_site_elevation_m", jo_sv(r, "antenna_site_elevation"));
    add_num_str(pr, "antenna_height_agl_m", jo_sv(r, "antenna_height_agl"));
    add_num_str(pr, "antenna_height_amsl_m", jo_sv(r, "antenna_height_amsl"));
    /* upstream spells this field 'logitude' — keep both spellings readable */
    add_num_str(pr, "gso_satellite_longitude_deg",
                jo_sv(r, "gso_satellite_logitude_decimal"));
    jo_add_str(pr, "tracking_telemetry_command", jo_sv(r, "tracking_telemetry_command"));
    jo_add_str(pr, "certification_date", jo_sv(r, "certification_date"));
    cJSON_AddNumberToObject(pr, "site_latitude", lat);
    cJSON_AddNumberToObject(pr, "site_longitude", lon);
    cJSON_AddStringToObject(pr, "geo_subject", "earth station antenna site");
    cJSON_AddStringToObject(pr, "source", "FCC open data acbv-jbb4 (protected FSS earth stations)");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[256], summary[288], key[96];
    snprintf(key, sizeof key, "%s|%s", num ? num : "", call ? call : "");
    snprintf(title, sizeof title, "%s%s%s", call ? call : num,
             lic ? " — " : "", lic ? lic : "");
    double az = 0, gso = 0;
    int has_az = num_str(jo_sv(r, "pointing_azimuth"), &az);
    int has_gso = num_str(jo_sv(r, "gso_satellite_logitude_decimal"), &gso);
    snprintf(summary, sizeof summary,
             "C-band earth station%s%.2f deg azimuth%s%.1f deg GSO longitude",
             has_az ? " · " : " · azimuth n/a", has_az ? az : 0.0,
             has_gso ? " · looking at " : "", has_gso ? gso : 0.0);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://opendata.fcc.gov/d/acbv-jbb4";
    it.lang            = "en";
    it.published_at    = jo_sv(r, "certification_date");
    it.record_type     = "satellite-earth-station";
    it.has_geo         = 1;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"satellite\",\"earth-station\",\"fcc\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, pages = 0, junk = 0;
  for (int i = 0; i < PAGES; i++) {
    char url[256];
    snprintf(url, sizeof url, "%s?$limit=%d&$offset=%d&$order=number",
             FSS_BASE, PAGE_SIZE, i * PAGE_SIZE);
    cJSON *doc = feed_get_json(ctx->http, url, 60000);
    if (!doc) break;
    if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); break; }
    pages++;
    int got = cJSON_GetArraySize(doc);
    total += emit_page(doc, sink, &junk);
    cJSON_Delete(doc);
    if (got < PAGE_SIZE) break;             /* last page */
  }
  if (pages == 0) {
    fprintf(stderr, "[fcc-fss-earth-stations] fetch failed\n");
    return -1;
  }
  fprintf(stderr, "[fcc-fss-earth-stations] emitted %d over %d page(s) "
                  "(%d rows dropped: null-island/pole coordinates)\n",
          total, pages, junk);
  return 0;
}

static const source_def tsp_fcc_fss_earth_stations_def = {
  .id = "fcc-fss-earth-stations", .collector = "satellite",
  .name = "FCC protected fixed-satellite-service earth station registrations",
  .update_interval_sec = 86400, .run = run,
  .category = "satellite", .type = "dataset", .url = FSS_BASE,
  .description = "US C-band satellite earth stations claiming 3.7-4.2 GHz protection: dish coordinates, antenna gain, pointing azimuth and elevation, the GSO satellite longitude targeted, and whether the site carries telemetry/tracking/command.",
  .license = "FCC open data (Socrata) — US Government public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_fcc_fss_earth_stations_def)
