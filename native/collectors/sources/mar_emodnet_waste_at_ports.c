/* collectors/sources/mar_emodnet_waste_at_ports.c
 * EMODnet Human Activities — MARPOL ship-generated waste delivered at European
 * port reception facilities, by port and year.
 *
 * Endpoint: https://ows.emodnet-humanactivities.eu/wfs?service=WFS
 *   &version=2.0.0&request=GetFeature&outputFormat=application/json
 *   &typeName=emodnet:wasteatports
 * Emits (all fetched): port_id, year, annex_i / annex_iv / annex_v and
 *   harbour-waste volumes in m3 and tonnes, totals, and the PORT point.
 * Keyless WFS. Licence: EMODnet Human Activities (EU), free reuse with
 *   attribution.
 *
 * *** parse_notes trap — "Geometry is the PORT point (shared across every
 * year-row for that port), describing the port, not a vessel." ***
 * record_type is "port-waste-statistic" and geo_precision is
 * "port-reference-point" with an explicit geo_note; this is never a vessel
 * position.
 * parse_notes trap — "Numeric fields arrive as STRINGS and empty string means
 * 'not reported' — do not coerce '' to 0, that would invent a zero-waste
 * port." add_qty() skips empty strings entirely (R1).
 * parse_notes — "Join on port_id to emodnet:portlocations."
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define WFS_URL "https://ows.emodnet-humanactivities.eu/wfs?service=WFS" \
                "&version=2.0.0&request=GetFeature" \
                "&outputFormat=application/json&typeName=emodnet:wasteatports"

/* '' means "not reported": emit nothing rather than a fabricated 0. */
static inline void add_qty(cJSON *dst, const cJSON *src, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(src, k);
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0])
    cJSON_AddStringToObject(dst, k, v->valuestring);
  else if (cJSON_IsNumber(v))
    cJSON_AddNumberToObject(dst, k, v->valuedouble);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WFS_URL, 60000);
  if (!doc) {
    fprintf(stderr, "[emodnet-waste-at-ports] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *pid = jo_sv(pr, "port_id");
    const char *year = jo_sv(pr, "year");
    if (!pid) continue;

    int geo = 0; double lat = 0, lon = 0;
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    if (g && !cJSON_IsNull(g)) {
      cJSON *co = cJSON_GetObjectItem(g, "coordinates");
      cJSON *x = cJSON_GetArrayItem(co, 0), *y = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        geo = 1; lon = x->valuedouble; lat = y->valuedouble;
      }
    }

    char key[96];
    snprintf(key, sizeof key, "%s|%s", pid, year ? year : "?");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "port_id", pid);
    if (year) cJSON_AddStringToObject(p, "year", year);
    add_qty(p, pr, "annex_i_m3");
    add_qty(p, pr, "annex_i_tonnes");
    add_qty(p, pr, "annex_iv_m3");
    add_qty(p, pr, "annex_iv_tonnes");
    add_qty(p, pr, "annex_v_m3");
    add_qty(p, pr, "annex_v_tonnes");
    add_qty(p, pr, "harbour_waste_m3");
    add_qty(p, pr, "harbour_waste_tonnes");
    add_qty(p, pr, "total_m3");
    add_qty(p, pr, "total_tonnes");
    cJSON_AddStringToObject(p, "not_reported_note",
      "absent quantity fields were empty upstream (not reported), not zero");
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "port-reference-point");
      cJSON_AddStringToObject(p, "geo_note",
        "this is the PORT the statistic describes — NOT a vessel position");
    }
    cJSON_AddStringToObject(p, "source", "EMODnet Human Activities");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *tot = jo_sv(pr, "total_m3");
    char title[160], summary[200];
    snprintf(title, sizeof title, "Ship waste at %s (%s)", pid,
             year ? year : "unknown year");
    if (tot && tot[0])
      snprintf(summary, sizeof summary, "total %s m3 delivered", tot);
    else
      snprintf(summary, sizeof summary, "MARPOL waste reception record");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://emodnet.ec.europa.eu/en/human-activities";
    it.record_type     = "port-waste-statistic";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"port\",\"marpol\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[emodnet-waste-at-ports] emitted %d\n", n);
  return 0;
}

static const source_def mar_emodnet_waste_at_ports_def = {
  .id = "emodnet-waste-at-ports", .collector = "maritime",
  .name = "EMODnet Human Activities — ship-generated waste delivered at ports",
  .update_interval_sec = 2592000, .run = run,
  .category = "industry", .type = "dataset", .url = WFS_URL,
  .description = "MARPOL waste volumes (Annex I oily residues, Annex IV sewage, Annex V garbage) delivered by ships at European port reception facilities, by port and year. Each row is a PORT statistic, not a vessel.",
  .license = "EMODnet Human Activities (EU), free reuse with attribution",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_emodnet_waste_at_ports_def)
