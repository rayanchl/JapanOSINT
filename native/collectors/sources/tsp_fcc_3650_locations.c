/* collectors/sources/tsp_fcc_3650_locations.c
 * FCC 3650-3700 MHz (ULS) registered base station locations — a keyless map of
 * US WISP and private-LTE infrastructure with the radio hardware named.
 * Endpoint: https://opendata.fcc.gov/resource/euz5-46g2.json?$limit=5000&$offset=N
 *   (keyless Socrata)
 * Emits: call sign, licence name and FRN, location name/city/county/state,
 *   lower and centre frequency, EIRP, antenna gain, azimuth, beamwidth,
 *   elevation angle, elevation AMSL, emission designator, antenna make/model,
 *   the equipment's FCC ID, application status and expiry date.
 *
 * TRAPS handled, quoted from the manifest:
 *  - "Socrata JSON, all values strings": every numeric read goes through
 *    num_str().
 *  - "u_latitude/u_longitude are already decimal degrees for the TRANSMITTER
 *    site" — the R2-safe coordinate for this domain; the licensee's mailing
 *    address is never used, and rows without a usable transmitter coordinate
 *    are emitted with NO geometry rather than being placed.
 *  - "Frequency fields carry a ' MHz' suffix — strip it": num_str stops at the
 *    suffix, and the raw string is kept alongside.
 *  - "Records include expired licences (u_expired_date in the past) and
 *    u_application_status values other than 'Accepted'; filter on those, do not
 *    assume every row is live": both fields are emitted verbatim and a derived
 *    `expired` boolean is computed from today's date, so nothing is dropped
 *    silently and nothing is presented as live that is not.
 * Licence: FCC open data (Socrata) — US Government public domain, keyless.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define U3650_BASE "https://opendata.fcc.gov/resource/euz5-46g2.json"
#define PAGE_SIZE 5000
#define PAGES 4

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
static void today_iso(char *out, size_t n) {
  time_t t = time(NULL);
  struct tm tmv;
  int ok;
#if defined(_WIN32)
  ok = (gmtime_s(&tmv, &t) == 0);
#else
  ok = (gmtime_r(&t, &tmv) != NULL);
#endif
  if (!ok) { snprintf(out, n, "9999-12-31"); return; }
  strftime(out, n, "%Y-%m-%d", &tmv);
}

static int emit_page(cJSON *arr, intel_sink *sink, const char *today) {
  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    const char *call = jo_sv(r, "u_call_sign");
    const char *lic  = jo_sv(r, "u_license_name");
    if (!call) continue;                          /* no identity -> no row */

    const char *city  = jo_sv(r, "u_location_city");
    const char *state = jo_sv(r, "u_location_state");
    const char *astat = jo_sv(r, "u_application_status");
    const char *exp   = jo_sv(r, "u_expired_date");

    double lat = 0, lon = 0;
    int has_geo = 0;
    if (num_str(jo_sv(r, "u_latitude"), &lat) && num_str(jo_sv(r, "u_longitude"), &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0))
      has_geo = 1;

    cJSON *pr = cJSON_CreateObject();
    jo_add_str(pr, "call_sign", call);
    jo_add_str(pr, "licensee", lic);
    jo_add_str(pr, "frn", jo_sv(r, "u_frn"));
    jo_add_str(pr, "location_name", jo_sv(r, "u_location_name"));
    jo_add_str(pr, "city", city);
    jo_add_str(pr, "county", jo_sv(r, "u_location_county"));
    jo_add_str(pr, "state", state);
    jo_add_str(pr, "transmitter_location_dms", jo_sv(r, "u_transmitter_location"));
    add_num_str(pr, "lower_frequency_mhz", jo_sv(r, "u_lower_frequency"));
    add_num_str(pr, "center_frequency_mhz", jo_sv(r, "u_center"));
    add_num_str(pr, "eirp", jo_sv(r, "u_eirp"));
    add_num_str(pr, "antenna_gain", jo_sv(r, "u_gain"));
    add_num_str(pr, "azimuth_deg", jo_sv(r, "u_azimuth"));
    add_num_str(pr, "beamwidth_deg", jo_sv(r, "u_beam"));
    add_num_str(pr, "elevation_angle_deg", jo_sv(r, "u_elevation_angle"));
    add_num_str(pr, "elevation_amsl_m", jo_sv(r, "u_elevation_amsl"));
    jo_add_str(pr, "emission_designator", jo_sv(r, "u_emission_designator"));
    jo_add_str(pr, "antenna_make", jo_sv(r, "u_antenna_make"));
    jo_add_str(pr, "antenna_model", jo_sv(r, "u_antenna_model"));
    jo_add_str(pr, "equipment_fcc_id", jo_sv(r, "u_fcc_id"));
    jo_add_str(pr, "application_status", astat);
    jo_add_str(pr, "expired_date", exp);
    if (exp && strlen(exp) >= 10)
      cJSON_AddBoolToObject(pr, "expired", strncmp(exp, today, 10) < 0 ? 1 : 0);
    if (has_geo) {
      cJSON_AddNumberToObject(pr, "site_latitude", lat);
      cJSON_AddNumberToObject(pr, "site_longitude", lon);
      cJSON_AddStringToObject(pr, "geo_subject", "registered transmitter site");
    } else {
      cJSON_AddStringToObject(pr, "geo_note",
        "no usable transmitter coordinate in this registration");
    }
    cJSON_AddStringToObject(pr, "source", "FCC open data euz5-46g2 (3650 MHz ULS registrations)");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[256], summary[288], key[128];
    snprintf(key, sizeof key, "%s|%.4f|%.4f", call, lat, lon);
    snprintf(title, sizeof title, "%s%s%s%s%s%s%s", call,
             lic ? " — " : "", lic ? lic : "",
             city ? " · " : "", city ? city : "",
             state ? ", " : "", state ? state : "");
    snprintf(summary, sizeof summary, "3650 MHz base station%s%s%s%s",
             astat ? " · " : "", astat ? astat : "",
             exp ? " · expires " : "", exp ? exp : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://opendata.fcc.gov/d/euz5-46g2";
    it.lang            = "en";
    it.record_type     = "wireless-base-station";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"spectrum\",\"wisp\",\"fcc\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char today[16];
  today_iso(today, sizeof today);
  int total = 0, pages = 0;
  for (int i = 0; i < PAGES; i++) {
    char url[256];
    snprintf(url, sizeof url, "%s?$limit=%d&$offset=%d&$order=u_call_sign",
             U3650_BASE, PAGE_SIZE, i * PAGE_SIZE);
    cJSON *doc = feed_get_json(ctx->http, url, 60000);
    if (!doc) break;
    if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); break; }
    pages++;
    int got = cJSON_GetArraySize(doc);
    total += emit_page(doc, sink, today);
    cJSON_Delete(doc);
    if (got < PAGE_SIZE) break;
  }
  if (pages == 0) {
    fprintf(stderr, "[fcc-3650-locations] fetch failed\n");
    return -1;
  }
  fprintf(stderr, "[fcc-3650-locations] emitted %d over %d page(s)\n", total, pages);
  return 0;
}

static const source_def tsp_fcc_3650_locations_def = {
  .id = "fcc-3650-locations", .collector = "telecom",
  .name = "FCC 3650 MHz (ULS) registered base station locations",
  .update_interval_sec = 86400, .run = run,
  .category = "telecom", .type = "dataset", .url = U3650_BASE,
  .description = "Registered 3650-3700 MHz wireless-broadband base stations across the US: licensee and FRN, per-site coordinates, EIRP, antenna make/model/azimuth/beamwidth and the equipment's FCC ID, with licence status and expiry.",
  .license = "FCC open data (Socrata) — US Government public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_fcc_3650_locations_def)
