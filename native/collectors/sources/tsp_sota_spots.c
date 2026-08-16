/* collectors/sources/tsp_sota_spots.c
 * Summits on the Air live spots — amateur stations operating from catalogued
 * mountain summits, the mountaintop/portable counterpart to POTA.
 * Endpoint: https://api-db2.sota.org.uk/api/spots/10/all   (keyless; the path
 *   segment is the number of spots to return, '/-N/all' would return the last
 *   N hours instead)
 * Emits: spot id, timestamp, spotting callsign, activator callsign and name,
 *   association code, summit code, the summit display string, frequency (MHz),
 *   mode, comments and the UI highlight colour.
 *
 * parse_notes honoured, quoted:
 *  - "NO coordinates in this payload — summitDetails is a display string
 *    ('Cerro Noroeste, 2530m, 8 points'), NOT a location. Resolve
 *    associationCode + summitCode via the SOTA_SUMMIT pivot below to obtain
 *    real lat/lon; do not attempt to geocode the summit name." Every row here
 *    is has_geo=0 (R2), and the association/summit codes are emitted so the
 *    SOTA_SUMMIT pivot can resolve them on demand.
 *  - "frequency is a STRING in MHz."
 * Licence: SOTA's public API is keyless and documented for community client
 *   use; attribution to the SOTA database expected.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOTA_SPOTS_URL "https://api-db2.sota.org.uk/api/spots/30/all"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, SOTA_SPOTS_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[sota-spots] fetch failed\n");
    return -1;
  }
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[sota-spots] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, doc) {
    const char *act = jo_sv(s, "activatorCallsign");
    if (!act) act = jo_sv(s, "callsign");
    const char *summit = jo_sv(s, "summitCode");
    const char *assoc  = jo_sv(s, "associationCode");
    if (!act) continue;                           /* no identity -> no row */

    const char *freq = jo_sv(s, "frequency");
    const char *mode = jo_sv(s, "mode");
    const char *ts   = jo_sv(s, "timeStamp");
    const char *det  = jo_sv(s, "summitDetails");

    cJSON *pr = cJSON_CreateObject();
    double d;
    if (jo_num(s, "id", &d))     cJSON_AddNumberToObject(pr, "spot_id", d);
    if (jo_num(s, "userID", &d)) cJSON_AddNumberToObject(pr, "user_id", d);
    jo_add_str(pr, "time_stamp", ts);
    jo_add_str(pr, "spotter_callsign", jo_sv(s, "callsign"));
    jo_add_str(pr, "activator_callsign", act);
    jo_add_str(pr, "activator_name", jo_sv(s, "activatorName"));
    jo_add_str(pr, "association_code", assoc);
    jo_add_str(pr, "summit_code", summit);
    jo_add_str(pr, "summit_details", det);
    jo_add_str(pr, "frequency_mhz", freq);
    jo_add_str(pr, "mode", mode);
    jo_add_str(pr, "comments", jo_sv(s, "comments"));
    jo_add_str(pr, "highlight_color", jo_sv(s, "highlightColor"));
    cJSON_AddStringToObject(pr, "geo_note",
      "the SOTA spot feed publishes no coordinates; summitDetails is a display "
      "string, not a location. Resolve association_code + summit_code through "
      "the SOTA_SUMMIT pivot for surveyed coordinates");
    cJSON_AddStringToObject(pr, "source", "SOTA database spot API");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[288], summary[288], key[160];
    snprintf(key, sizeof key, "%s|%s|%s", act, summit ? summit : "",
             ts ? ts : "");
    snprintf(title, sizeof title, "%s activating %s", act,
             summit ? summit : (assoc ? assoc : "a summit"));
    char ftxt[48];
    if (freq) snprintf(ftxt, sizeof ftxt, "%s MHz", freq);
    else      snprintf(ftxt, sizeof ftxt, "frequency n/a");
    snprintf(summary, sizeof summary, "%s%s%s%s%s", ftxt,
             mode ? " " : "", mode ? mode : "",
             det ? " · " : "", det ? det : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://sotawatch.sota.org.uk/";
    it.lang            = "en";
    it.published_at    = ts;
    it.record_type     = "amateur-radio-spot";
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"amateur-radio\",\"sota\"]";
    /* R2: has_geo stays 0 — this payload carries no coordinates. */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[sota-spots] emitted %d\n", n);
  return 0;
}

static const source_def tsp_sota_spots_def = {
  .id = "sota-spots", .collector = "telecom",
  .name = "Summits on the Air live spots",
  .update_interval_sec = 300, .run = run,
  .category = "telecom", .type = "api", .url = SOTA_SPOTS_URL,
  .description = "Live feed of amateur stations operating from catalogued mountain summits: activator callsign, summit reference, frequency, mode and comments (including automated skimmer reports).",
  .license = "SOTA database public API — keyless, documented for community client use, attribution expected.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_sota_spots_def)
