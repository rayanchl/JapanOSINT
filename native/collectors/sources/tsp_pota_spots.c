/* collectors/sources/tsp_pota_spots.c
 * Parks on the Air live activator spots — a continuous stream of who is on the
 * air, on what frequency, from a precisely known location.
 * Endpoint: https://api.pota.app/spot/activator   (keyless)
 * Emits: spot id, activator callsign, frequency (kHz), mode, park reference,
 *   park name, location descriptor, Maidenhead grid (4 and 6 character),
 *   spot time, spotter callsign, comments, source (self-spot / RBN / FT8
 *   gateway), duplicate count and expiry countdown.
 *
 * parse_notes honoured, quoted:
 *  - "frequency is a STRING in kHz" — kept as published and also converted.
 *  - "latitude/longitude are numbers and belong to the PARK, which is the
 *    operator's actual location — real, but park-area precision, so tag
 *    geo_precision='park-area'": done. R2 is satisfied because the coordinate
 *    is the upstream's own park position and its coarseness is declared; a spot
 *    without coordinates is emitted without geometry.
 *  - "Duplicate spots for the same activator are normal; key on
 *    activator+reference+frequency": that triple is the remote_key.
 * Licence: POTA's public API is keyless and used by the project's own map and
 *   third-party clients; attribution to Parks on the Air appreciated.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POTA_URL "https://api.pota.app/spot/activator"

static int numv(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (v && cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *e = NULL;
    double d = strtod(v->valuestring, &e);
    if (e && e != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, POTA_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[pota-spots] fetch failed\n");
    return -1;
  }
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[pota-spots] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, doc) {
    const char *act = jo_sv(s, "activator");
    const char *ref = jo_sv(s, "reference");
    if (!act) continue;                           /* no identity -> no row */

    const char *freq = jo_sv(s, "frequency");
    const char *mode = jo_sv(s, "mode");
    const char *park = jo_sv(s, "name");
    if (!park) park = jo_sv(s, "parkName");
    const char *when = jo_sv(s, "spotTime");
    const char *by   = jo_sv(s, "spotter");

    double lat = 0, lon = 0;
    int has_geo = 0;
    if (numv(s, "latitude", &lat) && numv(s, "longitude", &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0))
      has_geo = 1;

    cJSON *pr = cJSON_CreateObject();
    double d;
    if (numv(s, "spotId", &d)) cJSON_AddNumberToObject(pr, "spot_id", d);
    jo_add_str(pr, "activator", act);
    jo_add_str(pr, "frequency_khz", freq);
    if (freq) {
      char *e = NULL;
      double khz = strtod(freq, &e);
      if (e && e != freq) cJSON_AddNumberToObject(pr, "frequency_mhz", khz / 1000.0);
    }
    jo_add_str(pr, "mode", mode);
    jo_add_str(pr, "park_reference", ref);
    jo_add_str(pr, "park_name", park);
    jo_add_str(pr, "location_desc", jo_sv(s, "locationDesc"));
    jo_add_str(pr, "grid4", jo_sv(s, "grid4"));
    jo_add_str(pr, "grid6", jo_sv(s, "grid6"));
    jo_add_str(pr, "spot_time", when);
    jo_add_str(pr, "spotter", by);
    jo_add_str(pr, "comments", jo_sv(s, "comments"));
    jo_add_str(pr, "spot_source", jo_sv(s, "source"));
    if (numv(s, "count", &d))  cJSON_AddNumberToObject(pr, "count", d);
    if (numv(s, "expire", &d)) cJSON_AddNumberToObject(pr, "expire_sec", d);
    if (has_geo) {
      cJSON_AddNumberToObject(pr, "latitude", lat);
      cJSON_AddNumberToObject(pr, "longitude", lon);
      cJSON_AddStringToObject(pr, "geo_precision", "park-area");
      cJSON_AddStringToObject(pr, "geo_subject",
        "the park the operator is transmitting from");
    }
    cJSON_AddStringToObject(pr, "source", "Parks on the Air spot API");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[288], summary[288], link[160], key[160];
    snprintf(key, sizeof key, "%s|%s|%s", act, ref ? ref : "", freq ? freq : "");
    snprintf(title, sizeof title, "%s activating %s%s%s", act,
             ref ? ref : "a park", park ? " — " : "", park ? park : "");
    char ftxt[48];
    if (freq) snprintf(ftxt, sizeof ftxt, "%s kHz", freq);
    else      snprintf(ftxt, sizeof ftxt, "frequency n/a");
    snprintf(summary, sizeof summary, "%s%s%s%s%s", ftxt,
             mode ? " " : "", mode ? mode : "",
             by ? " · spotted by " : "", by ? by : "");
    if (ref) snprintf(link, sizeof link, "https://pota.app/#/park/%s", ref);
    else     link[0] = '\0';

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = link[0] ? link : NULL;
    it.lang            = "en";
    it.published_at    = when;
    it.record_type     = "amateur-radio-spot";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"amateur-radio\",\"pota\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[pota-spots] emitted %d\n", n);
  return 0;                    /* no one on the air is not an error (R3) */
}

static const source_def tsp_pota_spots_def = {
  .id = "pota-spots", .collector = "telecom",
  .name = "Parks on the Air live activator spots",
  .update_interval_sec = 300, .run = run,
  .category = "telecom", .type = "api", .url = POTA_URL,
  .description = "Real-time feed of amateur operators transmitting from named parks: callsign, exact frequency, mode, park reference/name/coordinates, who spotted them and by what means.",
  .license = "Parks on the Air public API — keyless, used by the project's own map; attribution appreciated.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_pota_spots_def)
