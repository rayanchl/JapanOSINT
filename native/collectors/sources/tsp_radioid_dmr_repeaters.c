/* collectors/sources/tsp_radioid_dmr_repeaters.c
 * RadioID.net DMR repeater registry — the official DMR-MARC / RadioID id
 * registry, the authoritative id-to-callsign mapping that BrandMeister traffic
 * is keyed on.
 * Endpoint: https://radioid.net/static/rptrs.json   (keyless, ~5.4 MB)
 * Emits: DMR repeater id (locator/id), callsign, city, state, country, output
 *   frequency, offset, colour code, timeslot linking, the IPSC network the
 *   repeater is bonded to, trustee callsigns, assigned role, status,
 *   talkgroups and the registry's map flags.
 *
 * TRAPS handled, quoted from the manifest:
 *  - "GEO: there is NO latitude/longitude in this file — only city/state/
 *    country strings. Do not geocode them; emit with has_geo=0 and let
 *    brandmeister-devices supply coordinates where the DMR ids match." Every
 *    row here is has_geo=0 (R2) and the DMR id is the join key.
 *  - "Envelope {\"rptrs\":[...]}, 5.4 MB."
 *  - "frequency and offset are strings in MHz."
 *  - "'id'/'locator' are the DMR repeater id (6-7 digits, first 3 = MCC country
 *    code)": the MCC prefix is recorded as a derived field.
 * STATED BOUND: only repeaters whose status is ACTIVE are emitted, capped at
 *   MAX_ROWS; both facts are recorded in properties.
 * Licence: RadioID.net publishes the static JSON exports for community use with
 *   no key. Data is operator-supplied.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RADIOID_URL "https://radioid.net/static/rptrs.json"
#define MAX_ROWS 15000

static void add_strlist(cJSON *dst, const char *key, const cJSON *src) {
  if (!cJSON_IsArray(src)) return;
  cJSON *out = cJSON_CreateArray();
  const cJSON *e;
  cJSON_ArrayForEach(e, src)
    if (cJSON_IsString(e) && e->valuestring && e->valuestring[0])
      cJSON_AddItemToArray(out, cJSON_CreateString(e->valuestring));
  cJSON_AddItemToObject(dst, key, out);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, RADIOID_URL, 90000);
  if (!doc) {
    fprintf(stderr, "[radioid-dmr-repeaters] fetch failed\n");
    return -1;
  }
  cJSON *arr = cJSON_GetObjectItem(doc, "rptrs");
  if (!cJSON_IsArray(arr)) {
    fprintf(stderr, "[radioid-dmr-repeaters] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0, seen = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    seen++;
    if (n >= MAX_ROWS) break;
    double id = 0;
    int has_id = jo_num(r, "id", &id);
    if (!has_id) has_id = jo_num(r, "locator", &id);
    const char *call = jo_sv(r, "callsign");
    if (!has_id || !call) continue;               /* no identity -> no row */
    const char *status = jo_sv(r, "status");
    if (!status || strcmp(status, "ACTIVE") != 0) continue;

    const char *freq = jo_sv(r, "frequency");
    const char *city = jo_sv(r, "city");
    const char *state = jo_sv(r, "state");
    const char *ctry = jo_sv(r, "country");
    const char *net  = jo_sv(r, "ipsc_network");

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddNumberToObject(pr, "dmr_id", id);
    jo_add_str(pr, "callsign", call);
    jo_add_str(pr, "city", city);
    jo_add_str(pr, "state", state);
    jo_add_str(pr, "country", ctry);
    jo_add_str(pr, "frequency_mhz", freq);
    jo_add_str(pr, "offset_mhz", jo_sv(r, "offset"));
    jo_add_str(pr, "assigned", jo_sv(r, "assigned"));
    jo_add_str(pr, "ts_linked", jo_sv(r, "ts_linked"));
    jo_add_str(pr, "ipsc_network", net);
    jo_add_str(pr, "status", status);
    jo_add_str(pr, "map_info", jo_sv(r, "map_info"));
    double d;
    if (jo_num(r, "color_code", &d)) cJSON_AddNumberToObject(pr, "colour_code", d);
    if (jo_num(r, "map", &d))        cJSON_AddNumberToObject(pr, "map_flag", d);
    add_strlist(pr, "trustee", cJSON_GetObjectItem(r, "trustee"));
    add_strlist(pr, "talkgroups", cJSON_GetObjectItem(r, "talkgroups"));
    /* first three digits of a DMR id are the MCC country code */
    char mcc[8];
    snprintf(mcc, sizeof mcc, "%.0f", id);
    if (strlen(mcc) >= 6) {
      char pfx[4] = { mcc[0], mcc[1], mcc[2], 0 };
      cJSON_AddStringToObject(pr, "dmr_mcc_prefix", pfx);
    }
    cJSON_AddStringToObject(pr, "geo_note",
      "RadioID publishes no coordinates for repeaters; the DMR id joins to "
      "brandmeister-devices where a position is available");
    cJSON_AddStringToObject(pr, "emitted_filter", "status=ACTIVE only");
    cJSON_AddNumberToObject(pr, "row_cap", MAX_ROWS);
    cJSON_AddStringToObject(pr, "source", "RadioID.net rptrs.json");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[288], summary[288], key[32];
    snprintf(key, sizeof key, "%.0f", id);
    snprintf(title, sizeof title, "%s (DMR %.0f)%s%s%s", call, id,
             freq ? " on " : "", freq ? freq : "", freq ? " MHz" : "");
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s",
             net ? net : "network n/a",
             city ? " · " : "", city ? city : "",
             state ? ", " : "", state ? state : "",
             ctry ? " · " : "", ctry ? ctry : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://radioid.net/database/search#!";
    it.lang            = "en";
    it.record_type     = "dmr-repeater-registration";
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"amateur-radio\",\"dmr\",\"radioid\"]";
    /* R2: has_geo stays 0 — this file has no coordinates. */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[radioid-dmr-repeaters] emitted %d ACTIVE of %d registered\n",
          n, seen);
  return 0;
}

static const source_def tsp_radioid_dmr_repeaters_def = {
  .id = "radioid-dmr-repeaters", .collector = "telecom",
  .name = "RadioID.net DMR repeater registry",
  .update_interval_sec = 86400, .run = run,
  .category = "telecom", .type = "dataset", .url = RADIOID_URL,
  .description = "The official DMR-MARC / RadioID repeater id registry: allocated DMR id, callsign, output frequency, offset, colour code, timeslot linking, IPSC network, trustee and status — the id-to-callsign mapping DMR traffic is keyed on.",
  .license = "RadioID.net static JSON exports — free community use, no key. Data is operator-supplied.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_radioid_dmr_repeaters_def)
