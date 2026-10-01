/* UK Environment Agency flood-monitoring river levels.
 * Endpoints (keyless):
 *   https://environment.data.gov.uk/flood-monitoring/id/measures?parameter=level&_limit=500
 *   https://environment.data.gov.uk/flood-monitoring/id/stations?parameter=level&_limit=5000
 * Emits one row per MEASURE with a latestReading: value plus the measure's own
 * unitName (mASD = metres above stage datum, mAOD = above ordnance datum,
 * m3/s for flow).
 *
 * DATUM TRAP: these datums are NOT comparable between stations, so unitName is
 * always carried on the row and never normalised away.
 * OFFLINE TRAP: a measure whose latestReading is absent (offline telemetry) is
 * SKIPPED, never emitted as 0.
 * GEO (R2): items[] are measures and carry no coordinates; lat/long are joined
 * from /id/stations on stationReference, and a measure whose station is not in
 * that list is emitted without geometry.
 * Licence: Open Government Licence v3 (response meta.licence = OGL v3). */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "uk-flood-monitoring"

/* Station coordinate index, keyed by stationReference. */
typedef struct { const char *ref; double lat, lon; } sref_t;

static const sref_t *find_ref(const sref_t *a, int n, const char *ref) {
  for (int i = 0; i < n; i++) if (strcmp(a[i].ref, ref) == 0) return &a[i];
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *sdoc = feed_get_json(ctx->http,
    "https://environment.data.gov.uk/flood-monitoring/id/stations"
    "?parameter=level&_limit=5000", 60000);
  sref_t *refs = NULL; int nrefs = 0;
  if (sdoc) {
    cJSON *items = cJSON_GetObjectItem(sdoc, "items");
    if (cJSON_IsArray(items)) {
      int cap = cJSON_GetArraySize(items);
      refs = calloc(cap > 0 ? (size_t)cap : 1, sizeof(sref_t));
      cJSON *s;
      if (refs) cJSON_ArrayForEach(s, items) {
        const char *ref = jo_sv(s, "stationReference");
        cJSON *la = cJSON_GetObjectItem(s, "lat");
        cJSON *lo = cJSON_GetObjectItem(s, "long");
        if (!ref || !cJSON_IsNumber(la) || !cJSON_IsNumber(lo)) continue;
        if (la->valuedouble == 0 && lo->valuedouble == 0) continue;
        if (nrefs >= cap) break;
        refs[nrefs].ref = ref;
        refs[nrefs].lat = la->valuedouble;
        refs[nrefs].lon = lo->valuedouble;
        nrefs++;
      }
    }
  }

  cJSON *doc = feed_get_json(ctx->http,
    "https://environment.data.gov.uk/flood-monitoring/id/measures"
    "?parameter=level&_limit=500", 45000);
  if (!doc) {
    free(refs); if (sdoc) cJSON_Delete(sdoc);
    fprintf(stderr, "[" SRC "] fetch failed\n");
    return -1;
  }
  cJSON *items = cJSON_GetObjectItem(doc, "items");
  if (!cJSON_IsArray(items)) {
    cJSON_Delete(doc); free(refs); if (sdoc) cJSON_Delete(sdoc);
    fprintf(stderr, "[" SRC "] no items[]\n");
    return -1;
  }

  int n = 0;
  cJSON *m;
  cJSON_ArrayForEach(m, items) {
    const char *label = jo_sv(m, "label");
    const char *unit  = jo_sv(m, "unitName");
    const char *ref   = jo_sv(m, "stationReference");
    cJSON *lr = cJSON_GetObjectItem(m, "latestReading");
    if (!label || !unit || !cJSON_IsObject(lr)) continue;   /* offline -> skip */
    cJSON *val = cJSON_GetObjectItem(lr, "value");
    const char *when = jo_sv(lr, "dateTime");
    if (!cJSON_IsNumber(val) || !when) continue;

    const sref_t *st = (ref && refs) ? find_ref(refs, nrefs, ref) : NULL;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "measure_label", label);
    cJSON_AddNumberToObject(p, "value", val->valuedouble);
    cJSON_AddStringToObject(p, "unit", unit);
    cJSON_AddStringToObject(p, "datum_note",
      "station datums (mASD/mAOD) are not comparable between stations");
    cJSON_AddStringToObject(p, "observed_at", when);
    if (ref) cJSON_AddStringToObject(p, "station_reference", ref);
    const char *par = jo_sv(m, "parameter");
    if (par) cJSON_AddStringToObject(p, "parameter", par);
    cJSON_AddStringToObject(p, "country", "GB-ENG");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *id = jo_sv(m, "@id");
    char title[320];
    snprintf(title, sizeof title, "%s: %.3f %s", label, val->valuedouble, unit);

    intel_item row = {0};
    row.remote_key      = id ? id : label;
    row.title           = title;
    row.summary         = title;
    row.published_at    = when;
    row.lang            = "en";
    row.link            = id ? id : "https://environment.data.gov.uk/flood-monitoring/";
    row.record_type     = "river-gauge";
    if (st) { row.has_geo = 1; row.lat = st->lat; row.lon = st->lon; }
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"hydrology\",\"flood\",\"uk\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  free(refs);
  if (sdoc) cJSON_Delete(sdoc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_uk_flood_monitoring_def = {
  .id = SRC, .collector = "environment",
  .name = "UK Environment Agency flood monitoring river levels",
  .update_interval_sec = 900, .run = run,
  .category = "environment", .type = "api",
  .url = "https://environment.data.gov.uk/flood-monitoring/id/measures?parameter=level",
  .description = "Near-real-time water level from England river/tidal/groundwater gauges, 15-minute telemetry, each reading carrying its own datum unit.",
  .license = "Open Government Licence v3 (Environment Agency).",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_uk_flood_monitoring_def)
