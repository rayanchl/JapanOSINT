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
#include "_timefmt.inc"
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
/* Today's date, or 0 with out[0]=0 when the clock cannot be rendered.
 *
 * This used to fall back to "9999-12-31", which is worse than no answer: the
 * only consumer is the `expired` boolean below, and that sentinel silently
 * asserts that EVERY registration has expired. Asserting a wrong fact is the
 * fabrication rule; omitting a fact we cannot establish is not. So the caller
 * now passes NULL and the `expired` property is simply absent — `expired_date`
 * itself is still emitted verbatim, so nothing the upstream said is lost and a
 * consumer can compute the comparison for itself.
 *
 * (strftime's 0 return was also unchecked and left `out` unspecified; both are
 * handled inside jo_now_fmt, which also carries the _WIN32 split.) */
static int today_iso(char *out, size_t n) {
  return jo_now_fmt("%Y-%m-%d", out, n) != NULL;
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
    if (exp && strlen(exp) >= 10 && today)
      cJSON_AddBoolToObject(pr, "expired", strncmp(exp, today, 10) < 0 ? 1 : 0);
    if (has_geo) {
      cJSON_AddNumberToObject(pr, "site_latitude", lat);
      cJSON_AddNumberToObject(pr, "site_longitude", lon);
      cJSON_AddStringToObject(pr, "geo_subject", "registered transmitter site");
    } else {
      cJSON_AddStringToObject(pr, "geo_note",
        "no usable transmitter coordinate in this registration");
    }
    /* Rule 2 (docs/SOURCE_EXHAUSTIVENESS.md): the block above is the curated
     * DISPLAY set, and it was also the only thing kept — u_location_id,
     * u_location_number, u_license_id, u_status_date, sys_updated_on,
     * u_upper_frequency, u_fcc_equipment_designation_type, the u_yn_*
     * certification flags, the certifier name and the receipt/certification
     * dates were all fetched and then dropped at the collector seam. Forward
     * every scalar the row carried under its own upstream name, which is what
     * the sibling collector in this family (tsp_ised_spectrum_sites.c) already
     * does; the friendly keys above stay as the display aliases. */
    const cJSON *a;
    cJSON_ArrayForEach(a, r) {
      if (!a->string || cJSON_GetObjectItem(pr, a->string)) continue;
      if (cJSON_IsString(a) && a->valuestring && a->valuestring[0])
        cJSON_AddStringToObject(pr, a->string, a->valuestring);
      else if (cJSON_IsNumber(a))
        cJSON_AddNumberToObject(pr, a->string, a->valuedouble);
      else if (cJSON_IsBool(a))
        cJSON_AddBoolToObject(pr, a->string, cJSON_IsTrue(a));
    }
    cJSON_AddStringToObject(pr, "source", "FCC open data euz5-46g2 (3650 MHz ULS registrations)");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    /* IDENTITY — house rule 4b, measured. The key used to be
     * `call_sign|lat|lon`, which is a DIMENSION, not this record's identity:
     * one call sign registers several sectors at one mast, all sharing the
     * transmitter coordinate and differing only in azimuth/antenna. Measured
     * against the live dataset on 2026-08-24:
     *
     *   total rows                        7,829
     *   distinct u_location_id            7,829      <- one per registration
     *   emitted / stored under the old key 7,829 / 3,993
     *
     * i.e. 3,836 real registrations collapsed onto a uid another row had
     * already written, every single pass, with rc=0 and a healthy-looking
     * records=7829. WQVF475 is the shape of it: four rows, one lat/lon,
     * azimuths 45/135/225/315, location_ids 16193934-7 — four sectors of one
     * base station, stored as one.
     *
     * `u_location_id` is the upstream's own per-registration key and is unique
     * across the whole table. Falling back to the old composite (plus the
     * location number, which also disambiguates the sectors) keeps a row that
     * arrives without one rather than dropping it. */
    const char *locid = jo_sv(r, "u_location_id");
    const char *locno = jo_sv(r, "u_location_number");
    char title[256], summary[288], key[160];
    if (locid)
      snprintf(key, sizeof key, "%s", locid);
    else
      snprintf(key, sizeof key, "%s|%.4f|%.4f|%s", call, lat, lon,
               locno ? locno : "");
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
  char todaybuf[16];
  const char *today = today_iso(todaybuf, sizeof todaybuf) ? todaybuf : NULL;
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
