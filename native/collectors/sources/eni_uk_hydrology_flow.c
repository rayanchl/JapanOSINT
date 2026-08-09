/* UK Environment Agency hydrology API — river flow (quality-controlled).
 * Distinct API from flood-monitoring: different base path, meta{} + items[]
 * with no @context wrapper. Endpoints (keyless):
 *   https://environment.data.gov.uk/hydrology/id/measures.json?observedProperty=waterFlow&_limit=2000
 *   https://environment.data.gov.uk/hydrology/id/stations.json?observedProperty=waterFlow&_limit=2000
 *   https://environment.data.gov.uk/hydrology/data/readings.json?observedProperty=waterFlow&latest&_limit=2000
 * Emits one row per measure with a latest reading: value + unitName (m3/s for
 * flow), the aggregation PERIOD in seconds and its name, and station lat/long.
 *
 * PERIOD TRAP: `period` is the aggregation window in seconds (86400 = daily
 * mean, 900 = 15-minute). Mixing periods gives nonsense trends, so every row
 * carries period_seconds and period_name and they are never averaged together.
 * GEO (R2): lat/long come from /id/stations joined on the station notation
 * (the measure notation's leading 36-char GUID); no station match -> no
 * geometry.
 * Licence: Open Government Licence v3 (meta.licenseName = "OGL 3"). */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "uk-hydrology-flow"
#define GUIDLEN 36

static const char *last_seg(const char *uri) {
  const char *s = strrchr(uri, '/');
  return s ? s + 1 : uri;
}

typedef struct { const char *guid; double lat, lon; } stn_t;
typedef struct { const char *notation, *label, *unit, *stguid;
                 double period; const char *period_name; } meas_t;

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *sdoc = feed_get_json(ctx->http,
    "https://environment.data.gov.uk/hydrology/id/stations.json"
    "?observedProperty=waterFlow&_limit=2000", 60000);
  stn_t *stns = NULL; int nstn = 0;
  if (sdoc) {
    cJSON *si = cJSON_GetObjectItem(sdoc, "items");
    if (cJSON_IsArray(si)) {
      int cap = cJSON_GetArraySize(si);
      stns = calloc(cap > 0 ? (size_t)cap : 1, sizeof(stn_t));
      cJSON *s;
      if (stns) cJSON_ArrayForEach(s, si) {
        const char *nt = jo_sv(s, "notation");
        cJSON *la = cJSON_GetObjectItem(s, "lat"), *lo = cJSON_GetObjectItem(s, "long");
        if (!nt || !cJSON_IsNumber(la) || !cJSON_IsNumber(lo)) continue;
        if (la->valuedouble == 0 && lo->valuedouble == 0) continue;
        if (nstn >= cap) break;
        stns[nstn].guid = nt; stns[nstn].lat = la->valuedouble;
        stns[nstn].lon = lo->valuedouble; nstn++;
      }
    }
  }

  cJSON *mdoc = feed_get_json(ctx->http,
    "https://environment.data.gov.uk/hydrology/id/measures.json"
    "?observedProperty=waterFlow&_limit=2000", 60000);
  if (!mdoc) {
    free(stns); if (sdoc) cJSON_Delete(sdoc);
    fprintf(stderr, "[" SRC "] measures fetch failed\n");
    return -1;
  }
  cJSON *mi = cJSON_GetObjectItem(mdoc, "items");
  if (!cJSON_IsArray(mi)) {
    cJSON_Delete(mdoc); free(stns); if (sdoc) cJSON_Delete(sdoc);
    fprintf(stderr, "[" SRC "] no measures items[]\n");
    return -1;
  }
  int mcap = cJSON_GetArraySize(mi);
  meas_t *meas = calloc(mcap > 0 ? (size_t)mcap : 1, sizeof(meas_t));
  int nmeas = 0;
  cJSON *m;
  if (meas) cJSON_ArrayForEach(m, mi) {
    const char *nt = jo_sv(m, "notation");
    if (!nt || nmeas >= mcap) continue;
    meas[nmeas].notation = nt;
    meas[nmeas].label    = jo_sv(m, "label");
    meas[nmeas].unit     = jo_sv(m, "unitName");
    cJSON *per = cJSON_GetObjectItem(m, "period");
    meas[nmeas].period   = cJSON_IsNumber(per) ? per->valuedouble : -1;
    meas[nmeas].period_name = jo_sv(m, "periodName");
    cJSON *stn = cJSON_GetObjectItem(m, "station");
    const char *sid = stn ? jo_sv(stn, "@id") : NULL;
    meas[nmeas].stguid = sid ? last_seg(sid) : NULL;
    nmeas++;
  }

  cJSON *rdoc = feed_get_json(ctx->http,
    "https://environment.data.gov.uk/hydrology/data/readings.json"
    "?observedProperty=waterFlow&latest&_limit=2000", 60000);
  if (!rdoc) {
    cJSON_Delete(mdoc); free(meas); free(stns); if (sdoc) cJSON_Delete(sdoc);
    fprintf(stderr, "[" SRC "] readings fetch failed\n");
    return -1;
  }
  cJSON *ri = cJSON_GetObjectItem(rdoc, "items");
  if (!cJSON_IsArray(ri)) {
    cJSON_Delete(rdoc); cJSON_Delete(mdoc); free(meas); free(stns);
    if (sdoc) cJSON_Delete(sdoc);
    fprintf(stderr, "[" SRC "] no readings items[]\n");
    return -1;
  }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, ri) {
    cJSON *mo = cJSON_GetObjectItem(r, "measure");
    const char *mid = mo ? jo_sv(mo, "@id") : NULL;
    cJSON *val = cJSON_GetObjectItem(r, "value");
    if (!mid || !cJSON_IsNumber(val)) continue;
    const char *notation = last_seg(mid);

    const meas_t *mm = NULL;
    for (int i = 0; i < nmeas; i++)
      if (strcmp(meas[i].notation, notation) == 0) { mm = &meas[i]; break; }
    if (!mm || !mm->unit) continue;      /* no unit -> not a safe measurement */

    const stn_t *ss = NULL;
    if (mm->stguid)
      for (int i = 0; i < nstn; i++)
        if (strncmp(stns[i].guid, mm->stguid, GUIDLEN) == 0) { ss = &stns[i]; break; }

    const char *when = jo_sv(r, "dateTime");
    if (!when) when = jo_sv(r, "date");

    cJSON *p = cJSON_CreateObject();
    if (mm->label) cJSON_AddStringToObject(p, "measure_label", mm->label);
    cJSON_AddNumberToObject(p, "value", val->valuedouble);
    cJSON_AddStringToObject(p, "unit", mm->unit);
    if (mm->period >= 0) cJSON_AddNumberToObject(p, "period_seconds", mm->period);
    if (mm->period_name) cJSON_AddStringToObject(p, "period_name", mm->period_name);
    cJSON_AddStringToObject(p, "period_note",
      "readings of different aggregation periods are not comparable");
    if (when) cJSON_AddStringToObject(p, "observed_at", when);
    cJSON_AddStringToObject(p, "measure_notation", notation);
    cJSON_AddStringToObject(p, "country", "GB-ENG");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[352];
    snprintf(title, sizeof title, "%s: %.3f %s",
             mm->label ? mm->label : notation, val->valuedouble, mm->unit);

    intel_item row = {0};
    row.remote_key      = notation;
    row.title           = title;
    row.summary         = title;
    row.published_at    = when;
    row.lang            = "en";
    row.link            = mid;
    row.record_type     = "river-flow";
    if (ss) { row.has_geo = 1; row.lat = ss->lat; row.lon = ss->lon; }
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"hydrology\",\"river\",\"uk\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(rdoc);
  cJSON_Delete(mdoc);
  free(meas); free(stns);
  if (sdoc) cJSON_Delete(sdoc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_uk_hydrology_flow_def = {
  .id = SRC, .collector = "environment",
  .name = "UK Environment Agency hydrology river flow",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://environment.data.gov.uk/hydrology/id/measures.json?observedProperty=waterFlow",
  .description = "Quality-controlled hydrometric river flow (m3/s) for England, each reading carrying its aggregation period and station coordinates.",
  .license = "Open Government Licence v3 (meta.licenseName = OGL 3).",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_uk_hydrology_flow_def)
