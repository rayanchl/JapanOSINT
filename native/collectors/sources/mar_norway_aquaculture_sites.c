/* collectors/sources/mar_norway_aquaculture_sites.c
 * Norwegian Directorate of Fisheries — aquaculture site register: every
 * licensed site with its registered position, biomass capacity and licences.
 *
 * Endpoint: https://api.fiskeridir.no/pub-aqua/api/v1/sites?range=0-99
 * Emits (all fetched): siteId, siteNr, name, placementType, waterType,
 *   capacity + capacityUnitType, firstClearanceTime/Type, placement
 *   (municipality, county, production area and its sea-lice traffic-light
 *   status), the connected licence numbers, and the registered site position.
 * Keyless. Licence: Fiskeridirektoratet open API, Norwegian Licence for Open
 *   Government Data (NLOD).
 *
 * parse_notes — "Real coordinate is the top-level latitude / longitude
 * numerics (decimal degrees, WGS84) — the licensed site's registered position,
 * not a municipality centroid." Sites missing either numeric are emitted with
 * has_geo=0 (R2).
 * parse_notes — "Paging is via the non-standard 'range=start-end' query
 * parameter." NOTE: the service now enforces a maximum window of 100
 * (range=0-499 returns HTTP 400 "Limit is set to: 100"), so one page of
 * range=0-99 is taken per run.
 * parse_notes — "placement.prodAreaStatus (RØD/GUL/GRØNN) is the sea-lice
 * traffic-light status for the production area"; carried through verbatim.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define AQUA_URL "https://api.fiskeridir.no/pub-aqua/api/v1/sites?range=0-99"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, AQUA_URL, 60000);
  if (!doc) {
    fprintf(stderr, "[norway-aquaculture-sites] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "sites");
  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, arr) {
    if (!cJSON_IsObject(s)) continue;
    const cJSON *snr = cJSON_GetObjectItem(s, "siteNr");
    const char *name = jo_sv(s, "name");
    if (!cJSON_IsNumber(snr) && !name) continue;
    char key[48];
    if (cJSON_IsNumber(snr)) snprintf(key, sizeof key, "%lld",
                                      (long long)snr->valuedouble);
    else snprintf(key, sizeof key, "%.40s", name);

    const cJSON *latj = cJSON_GetObjectItem(s, "latitude");
    const cJSON *lonj = cJSON_GetObjectItem(s, "longitude");
    int geo = cJSON_IsNumber(latj) && cJSON_IsNumber(lonj);
    double lat = geo ? latj->valuedouble : 0;
    double lon = geo ? lonj->valuedouble : 0;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "siteNr", key);
    jo_copy_str(p, s, "name");
    jo_copy_str(p, s, "placementType");
    jo_copy_str(p, s, "waterType");
    jo_copy_str(p, s, "capacityUnitType");
    jo_copy_str(p, s, "firstClearanceTime");
    jo_copy_str(p, s, "firstClearanceType");
    jo_copy_num(p, s, "siteId");
    jo_copy_num(p, s, "capacity");
    jo_copy_num(p, s, "tempCapacity");
    const cJSON *pl = cJSON_GetObjectItem(s, "placement");
    if (pl) {
      cJSON *o = cJSON_CreateObject();
      jo_copy_str(o, pl, "municipalityName");
      jo_copy_str(o, pl, "countyName");
      jo_copy_str(o, pl, "prodAreaName");
      jo_copy_str(o, pl, "prodAreaStatus");   /* sea-lice traffic light */
      cJSON_AddItemToObject(p, "placement", o);
    }
    const cJSON *conns = cJSON_GetObjectItem(s, "connections");
    if (cJSON_IsArray(conns)) {
      cJSON *lic = cJSON_CreateArray();
      const cJSON *c;
      cJSON_ArrayForEach(c, conns) {
        const char *ln = jo_sv(c, "licenseNr");
        if (ln) cJSON_AddItemToArray(lic, cJSON_CreateString(ln));
      }
      cJSON_AddItemToObject(p, "licenses", lic);
    }
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "registered-site-position");
    }
    cJSON_AddStringToObject(p, "source", "Fiskeridirektoratet pub-aqua");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *county = NULL, *status = NULL;
    if (pl) { county = jo_sv(pl, "countyName"); status = jo_sv(pl, "prodAreaStatus"); }
    const cJSON *cap = cJSON_GetObjectItem(s, "capacity");
    char title[220], summary[240];
    snprintf(title, sizeof title, "Aquaculture site %s (%s)",
             name ? name : key, key);
    if (cJSON_IsNumber(cap))
      snprintf(summary, sizeof summary, "%.0f %s%s%s%s%s", cap->valuedouble,
               jo_sv(s, "capacityUnitType") ? jo_sv(s, "capacityUnitType") : "",
               county ? " · " : "", county ? county : "",
               status ? " · prod-area " : "", status ? status : "");
    else
      snprintf(summary, sizeof summary, "%s%s", county ? county : "",
               status ? status : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://www.fiskeridir.no/Akvakultur/registre-og-skjema/akvakulturregisteret";
    it.lang            = "no";
    it.record_type     = "aquaculture-site";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"aquaculture\",\"norway\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[norway-aquaculture-sites] emitted %d\n", n);
  return 0;
}

static const source_def mar_norway_aquaculture_sites_def = {
  .id = "norway-aquaculture-sites", .collector = "maritime",
  .name = "Norwegian Directorate of Fisheries — aquaculture site register",
  .update_interval_sec = 604800, .run = run,
  .category = "industry", .type = "api", .url = AQUA_URL,
  .description = "Licensed Norwegian aquaculture sites with exact position, biomass capacity, production-area sea-lice status and the licence holders connected to each — fixed marine installations from a fully open national register.",
  .license = "Fiskeridirektoratet open API, Norwegian Licence for Open Government Data (NLOD)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_norway_aquaculture_sites_def)
