/* collectors/sources/tsp_brandmeister_devices.c
 * BrandMeister DMR repeater and hotspot network — the world's largest DMR
 * digital-voice network, device by device.
 * Endpoint: https://api.brandmeister.network/v2/device   (keyless, ~10 MB)
 * Emits: DMR id, callsign, link name, hardware make/model string, firmware
 *   revision, transmit and receive frequencies (MHz), colour code, status,
 *   last known master server, city (which often carries a Maidenhead locator),
 *   peak power, antenna height above ground and last_seen.
 *
 * TRAPS handled, quoted from the manifest:
 *  - "lat/lng are numeric and operator-declared; a nontrivial minority are 0/0
 *    or obviously bogus, so reject 0,0 and out-of-range values rather than
 *    plotting them": exactly that filter, and rejected rows are emitted WITHOUT
 *    geometry rather than dropped or relocated (R2). properties.geo_precision
 *    records that the position is operator-declared, not surveyed.
 *  - "tx/rx are MHz as strings."
 *  - "'city' often carries a Maidenhead locator appended ('Volos, KM19MJ')":
 *    the raw string is kept verbatim; no attempt is made to re-derive a
 *    position from it.
 *  - "the response is ~10 MB, so poll infrequently": interval 21600 s, and only
 *    devices seen within RECENT_DAYS are emitted (STATED BOUND — that is the
 *    live network; the full historical roster is tens of thousands of rows and
 *    does not change), capped at MAX_ROWS.
 * Licence: BrandMeister publishes this v2 API openly with no key; the data is
 *   self-declared by repeater operators.
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

#define BM_URL "https://api.brandmeister.network/v2/device"
#define RECENT_DAYS 3
#define MAX_ROWS 12000

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, BM_URL, 120000);
  if (!doc) {
    fprintf(stderr, "[brandmeister-devices] fetch failed\n");
    return -1;
  }
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[brandmeister-devices] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  char cutoff[16];
  jo_days_ago_iso(RECENT_DAYS, cutoff, sizeof cutoff);

  int n = 0, seen = 0, nogeo = 0;
  cJSON *dv;
  cJSON_ArrayForEach(dv, doc) {
    seen++;
    if (n >= MAX_ROWS) break;
    double id;
    if (!jo_num(dv, "id", &id)) continue;
    const char *call = jo_sv(dv, "callsign");
    const char *last = jo_sv(dv, "last_seen");
    if (!call) continue;                          /* no identity -> no row */
    /* only the recently-active set; comparison is on the YYYY-MM-DD prefix */
    if (!last || strncmp(last, cutoff, 10) < 0) continue;

    const char *tx = jo_sv(dv, "tx"), *rx = jo_sv(dv, "rx");
    const char *hw = jo_sv(dv, "hardware"), *fw = jo_sv(dv, "firmware");
    const char *city = jo_sv(dv, "city");

    double lat = 0, lon = 0;
    int has_geo = 0;
    if (jo_num(dv, "lat", &lat) && jo_num(dv, "lng", &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0))
      has_geo = 1;
    else
      nogeo++;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddNumberToObject(pr, "dmr_id", id);
    jo_add_str(pr, "callsign", call);
    jo_add_str(pr, "link_name", jo_sv(dv, "linkname"));
    jo_add_str(pr, "hardware", hw);
    jo_add_str(pr, "firmware", fw);
    jo_add_str(pr, "tx_mhz", tx);
    jo_add_str(pr, "rx_mhz", rx);
    jo_add_str(pr, "city_raw", city);
    jo_add_str(pr, "last_known_master", jo_sv(dv, "lastKnownMaster"));
    jo_add_str(pr, "last_seen", last);
    double d;
    if (jo_num(dv, "colorcode", &d)) cJSON_AddNumberToObject(pr, "colour_code", d);
    if (jo_num(dv, "status", &d))    cJSON_AddNumberToObject(pr, "status_code", d);
    if (jo_num(dv, "pep", &d))       cJSON_AddNumberToObject(pr, "peak_power", d);
    if (jo_num(dv, "agl", &d))       cJSON_AddNumberToObject(pr, "antenna_agl_m", d);
    if (has_geo) {
      cJSON_AddNumberToObject(pr, "latitude", lat);
      cJSON_AddNumberToObject(pr, "longitude", lon);
      cJSON_AddStringToObject(pr, "geo_precision",
        "operator-declared device position (self-reported, not surveyed)");
    } else {
      cJSON_AddStringToObject(pr, "geo_note",
        "device published no usable coordinates (0,0 or out of range)");
    }
    cJSON_AddNumberToObject(pr, "active_window_days", RECENT_DAYS);
    cJSON_AddStringToObject(pr, "source", "BrandMeister v2 device API");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[288], summary[288], link[128], key[32];
    snprintf(key, sizeof key, "%.0f", id);
    if (tx) snprintf(title, sizeof title, "%s (DMR %.0f) TX %s MHz", call, id, tx);
    else    snprintf(title, sizeof title, "%s (DMR %.0f)", call, id);
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             hw ? hw : "hardware n/a",
             fw ? " · fw " : "", fw ? fw : "",
             city ? " · " : "", city ? city : "");
    snprintf(link, sizeof link, "https://brandmeister.network/?page=repeater&id=%.0f", id);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = last;
    it.record_type     = "dmr-device";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"amateur-radio\",\"dmr\",\"brandmeister\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[brandmeister-devices] emitted %d of %d devices "
                  "(last seen since %s; %d without usable coordinates)\n",
          n, seen, cutoff, nogeo);
  return 0;
}

static const source_def tsp_brandmeister_devices_def = {
  .id = "brandmeister-devices", .collector = "telecom",
  .name = "BrandMeister DMR repeater and hotspot network",
  .update_interval_sec = 21600, .run = run,
  .category = "telecom", .type = "api", .url = BM_URL,
  .description = "The world's largest DMR network device by device: DMR id, callsign, TX/RX frequencies, colour code, antenna height, peak power, hardware and firmware revision, master server and last-seen time, with operator-declared coordinates.",
  .license = "BrandMeister v2 API — open and keyless; data is self-declared by repeater operators. Response is ~10 MB, so polling is infrequent.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_brandmeister_devices_def)
