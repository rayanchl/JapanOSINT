/* collectors/sources/tsp_jpl_close_approaches.c
 * NASA/JPL near-Earth object close approaches — space situational awareness
 * for natural objects.
 * Endpoint: https://ssd-api.jpl.nasa.gov/cad.api?dist-max=0.05&date-min=now&limit=200
 *   (keyless)
 * Emits: designation, orbit id, Julian date, calendar close-approach time,
 *   nominal / minimum / maximum miss distance (au and km), relative and
 *   infinity velocity, timing uncertainty (t_sigma_f) and absolute magnitude H.
 *
 * parse_notes honoured:
 *  - "Same columnar shape as the fireball API (fields[] + data[][], all
 *    strings)": indices are resolved from fields[] each run, never hardcoded.
 *  - "dist is in astronomical units — convert if you want km (1 au =
 *    149,597,870.7 km)": the au string is kept verbatim and the km value is
 *    added as a derived field, clearly named.
 *  - "No location by design: a close approach has no ground point, so emit
 *    with has_geo=0" (R2).
 * Licence: NASA/JPL SBDB Close Approach Data API, keyless, US Government
 *   public domain.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAD_URL \
  "https://ssd-api.jpl.nasa.gov/cad.api?dist-max=0.05&date-min=now&limit=200"
#define AU_KM 149597870.7

static int col_of(const cJSON *fields, const char *name) {
  int i = 0;
  const cJSON *f;
  cJSON_ArrayForEach(f, fields) {
    if (cJSON_IsString(f) && f->valuestring && strcmp(f->valuestring, name) == 0)
      return i;
    i++;
  }
  return -1;
}
static const char *cell(const cJSON *row, int idx) {
  if (idx < 0) return NULL;
  const cJSON *c = cJSON_GetArrayItem(row, idx);
  if (!cJSON_IsString(c) || !c->valuestring || !c->valuestring[0]) return NULL;
  return c->valuestring;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CAD_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[jpl-close-approaches] fetch failed\n");
    return -1;
  }
  cJSON *fields = cJSON_GetObjectItem(doc, "fields");
  cJSON *data   = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(fields)) {
    fprintf(stderr, "[jpl-close-approaches] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }
  if (!cJSON_IsArray(data)) {          /* no approaches in the window (R3) */
    cJSON_Delete(doc);
    fprintf(stderr, "[jpl-close-approaches] emitted 0 (none upcoming)\n");
    return 0;
  }

  int i_des = col_of(fields, "des");
  int i_orb = col_of(fields, "orbit_id");
  int i_jd  = col_of(fields, "jd");
  int i_cd  = col_of(fields, "cd");
  int i_d   = col_of(fields, "dist");
  int i_dmn = col_of(fields, "dist_min");
  int i_dmx = col_of(fields, "dist_max");
  int i_vr  = col_of(fields, "v_rel");
  int i_vi  = col_of(fields, "v_inf");
  int i_ts  = col_of(fields, "t_sigma_f");
  int i_h   = col_of(fields, "h");
  if (i_des < 0 || i_cd < 0) {
    fprintf(stderr, "[jpl-close-approaches] no des/cd column; not parsing\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *row;
  cJSON_ArrayForEach(row, data) {
    if (!cJSON_IsArray(row)) continue;
    const char *des = cell(row, i_des);
    const char *cd  = cell(row, i_cd);
    if (!des || !cd) continue;
    const char *dist = cell(row, i_d);
    const char *vrel = cell(row, i_vr);
    const char *h    = cell(row, i_h);

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "designation", des);
    cJSON_AddStringToObject(pr, "close_approach_utc", cd);
    const char *s;
    if ((s = cell(row, i_orb))) cJSON_AddStringToObject(pr, "orbit_id", s);
    if ((s = cell(row, i_jd)))  cJSON_AddStringToObject(pr, "julian_date", s);
    if (dist) {
      cJSON_AddStringToObject(pr, "dist_au", dist);
      char *e = NULL;
      double au = strtod(dist, &e);
      if (e && e != dist) cJSON_AddNumberToObject(pr, "dist_km", au * AU_KM);
    }
    if ((s = cell(row, i_dmn))) cJSON_AddStringToObject(pr, "dist_min_au", s);
    if ((s = cell(row, i_dmx))) cJSON_AddStringToObject(pr, "dist_max_au", s);
    if (vrel) cJSON_AddStringToObject(pr, "v_rel_km_s", vrel);
    if ((s = cell(row, i_vi)))  cJSON_AddStringToObject(pr, "v_inf_km_s", s);
    if ((s = cell(row, i_ts)))  cJSON_AddStringToObject(pr, "t_sigma_f", s);
    if (h) cJSON_AddStringToObject(pr, "absolute_magnitude_h", h);
    cJSON_AddStringToObject(pr, "geo_note",
      "a close approach has no ground point; no geometry is published or invented");
    cJSON_AddStringToObject(pr, "source", "NASA/JPL SBDB close-approach API");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[224], summary[256], key[96];
    snprintf(key, sizeof key, "%s|%s", des, cd);
    if (dist) {
      char *e = NULL;
      double au = strtod(dist, &e);
      if (e && e != dist)
        snprintf(title, sizeof title, "%s passes %.0f km from Earth on %s",
                 des, au * AU_KM, cd);
      else
        snprintf(title, sizeof title, "%s close approach %s", des, cd);
    } else {
      snprintf(title, sizeof title, "%s close approach %s", des, cd);
    }
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s",
             dist ? "miss distance " : "", dist ? dist : "", dist ? " au" : "",
             vrel ? " · v_rel " : "", vrel ? vrel : "", vrel ? " km/s" : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://cneos.jpl.nasa.gov/ca/";
    it.lang            = "en";
    it.record_type     = "neo-close-approach";
    it.properties_json = pj;
    it.tags_json       = "[\"space\",\"neo\",\"nasa\"]";
    /* R2: has_geo stays 0 — deliberately. */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[jpl-close-approaches] emitted %d\n", n);
  return 0;
}

static const source_def tsp_jpl_close_approaches_def = {
  .id = "jpl-close-approaches", .collector = "satellite",
  .name = "NASA/JPL near-Earth object close approaches",
  .update_interval_sec = 43200, .run = run,
  .category = "satellite", .type = "api", .url = CAD_URL,
  .description = "Upcoming NEO close approaches inside 0.05 au with approach time, nominal/min/max miss distance, relative velocity, timing uncertainty and absolute magnitude.",
  .license = "NASA/JPL SBDB Close Approach Data API — US Government public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_jpl_close_approaches_def)
