/* collectors/cyber/sources/ioda_jp.c
 * Port of server/src/collectors/iodaJp.js (createThreatIntelCollector).
 * Keyless. IODA 7-day JP signals (3 datasources) + events, TOKYO points. */
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define IODA_BASE "https://api.ioda.inetintel.cc.gatech.edu/v2"

static void add_tokyo_geom(cJSON *f) {
  /* NO GEOMETRY. Every row here used to be emitted at Tokyo Station
   * (35.6895, 139.6917). What this source reports has no location,
   * and stacking every row on one pin is the fabrication the
   * 2026-07-31 audit deleted from cisa-kev-jp, poc-in-github and
   * peeringdb-jp. Confirmed live by tests/contract/source_contract.py.
   * lib/geojson.c handles an absent geometry correctly — do NOT
   * reintroduce a fallback coordinate. */
}

static cJSON *dup_or_null(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && !cJSON_IsNull(v)) ? cJSON_Duplicate(v, 1) : cJSON_CreateNull();
}

/* Pull the last non-null number out of a series' values[]. */
static int series_last(cJSON *vals, double *out) {
  if (!cJSON_IsArray(vals)) return 0;
  for (int i = cJSON_GetArraySize(vals) - 1; i >= 0; i--) {
    cJSON *v = cJSON_GetArrayItem(vals, i);
    if (v && cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  }
  return 0;
}

/* Fold one series object into the summary being built. */
static void take_series(cJSON *s, cJSON *samples, int *count,
                        cJSON *p, int *have_meta) {
  if (!cJSON_IsObject(s)) return;
  (*count)++;
  cJSON *vals = cJSON_GetObjectItem(s, "values");
  if (cJSON_IsArray(vals)) {
    int n = cJSON_GetArraySize(vals);
    int start = n > 3 ? n - 3 : 0;
    for (int k = start; k < n; k++)
      cJSON_AddItemToArray(samples,
        cJSON_Duplicate(cJSON_GetArrayItem(vals, k), 1));
    if (!*have_meta) {
      /* the series envelope carries the window + cadence + entity, all of
       * which the old port threw away, leaving rows with no measurement. */
      double lastv;
      cJSON_AddNumberToObject(p, "value_count", n);
      if (series_last(vals, &lastv))
        cJSON_AddNumberToObject(p, "latest_value", lastv);
      cJSON_AddItemToObject(p, "from",   dup_or_null(s, "from"));
      cJSON_AddItemToObject(p, "until",  dup_or_null(s, "until"));
      cJSON_AddItemToObject(p, "step",   dup_or_null(s, "step"));
      cJSON_AddItemToObject(p, "entity", dup_or_null(s, "entityName"));
      *have_meta = 1;
    }
  }
}

/* summarise(label,payload): series live in payload.data.
 *
 * BUG (fixed): IODA v2 returns data as an array OF ARRAYS of series objects
 * (data[0] is itself the list of series). The original port iterated `data`
 * directly, so `s` was an ARRAY and cJSON_GetObjectItem(s,"values") returned
 * NULL — every row persisted with sample_values:[] and series_count:1, i.e.
 * a label with no measurement in it. Unwrap the extra level. */
static void summarise(cJSON *features, const char *label, cJSON *payload) {
  cJSON *data = payload ? cJSON_GetObjectItem(payload, "data") : NULL;
  int series_count = 0, have_meta = 0;
  cJSON *samples = cJSON_CreateArray();

  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  add_tokyo_geom(f);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "kind", "signal");
  cJSON_AddStringToObject(p, "datasource", label);
  /* Stable identity per datasource: the geojson toolkit otherwise hashes the
   * whole properties blob, and since that blob holds the measured values a new
   * row was minted on every 30-minute poll instead of updating the signal. */
  {
    char sid[64];
    snprintf(sid, sizeof sid, "ioda-jp-signal-%s", label);
    cJSON_AddStringToObject(p, "id", sid);
  }

  if (cJSON_IsArray(data)) {
    cJSON *lvl1;
    cJSON_ArrayForEach(lvl1, data) {
      if (cJSON_IsArray(lvl1)) {
        cJSON *s;
        cJSON_ArrayForEach(s, lvl1)
          take_series(s, samples, &series_count, p, &have_meta);
      } else {
        take_series(lvl1, samples, &series_count, p, &have_meta);
      }
    }
  }

  cJSON_AddNumberToObject(p, "series_count", series_count);
  cJSON_AddItemToObject(p, "sample_values", samples);
  cJSON_AddItemToObject(p, "err", dup_or_null(payload, "err"));
  cJSON_AddStringToObject(p, "source", "ioda_signals");
  {   /* geojson pickText finds no title key otherwise → NULL title */
    cJSON *lv = cJSON_GetObjectItem(p, "latest_value");
    char t[192];
    if (lv && cJSON_IsNumber(lv))
      snprintf(t, sizeof t, "IODA %s (Japan): latest %.0f over %d series",
               label, lv->valuedouble, series_count);
    else
      snprintf(t, sizeof t, "IODA %s (Japan): no signal returned", label);
    cJSON_AddStringToObject(p, "title", t);
  }
  cJSON_AddStringToObject(p, "link",
    "https://ioda.inetintel.cc.gatech.edu/country/JP");
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(features, f);
}

static cJSON *fetch_signals(const source_ctx *ctx, long from, long until,
                            const char *ds) {
  char url[320];
  snprintf(url, sizeof url,
    "%s/signals/raw/country/JP?from=%ld&until=%ld&datasource=%s",
    IODA_BASE, from, until, ds);
  const char *hdrs[] = { "Accept: application/json", NULL };
  return feed_get_json_h(ctx->http, url, hdrs, 15000);
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)key; (void)ud;
  long until = (long)time(NULL);
  long from = until - 7 * 24 * 3600;

  cJSON *bgp = fetch_signals(ctx, from, until, "bgp");
  cJSON *ap = fetch_signals(ctx, from, until, "ping-slash24");
  cJSON *tel = fetch_signals(ctx, from, until, "merit-nt");
  char eurl[320];
  snprintf(eurl, sizeof eurl,
    "%s/events?from=%ld&until=%ld&overall=true&relatedTo=country%%2FJP",
    IODA_BASE, from, until);
  const char *hdrs[] = { "Accept: application/json", NULL };
  cJSON *events = feed_get_json_h(ctx->http, eurl, hdrs, 15000);

  cJSON *features = cJSON_CreateArray();
  summarise(features, "bgp", bgp);
  summarise(features, "ping-slash24", ap);
  summarise(features, "merit-nt", tel);

  cJSON *evd = events ? cJSON_GetObjectItem(events, "data") : NULL;
  if (cJSON_IsArray(evd)) {
    int i = 0;
    cJSON *e;
    cJSON_ArrayForEach(e, evd) {
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      add_tokyo_geom(f);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddNumberToObject(p, "idx", i);
      cJSON_AddStringToObject(p, "kind", "event");
      cJSON_AddItemToObject(p, "from", dup_or_null(e, "from"));
      cJSON_AddItemToObject(p, "until", dup_or_null(e, "until"));
      cJSON_AddItemToObject(p, "score", dup_or_null(e, "score"));
      cJSON_AddItemToObject(p, "relevant_signals", dup_or_null(e, "relevantSignals"));
      cJSON_AddItemToObject(p, "location_code", dup_or_null(e, "locationCode"));
      cJSON_AddItemToObject(p, "location_name", dup_or_null(e, "locationName"));
      cJSON_AddStringToObject(p, "source", "ioda_events");
      {   /* readable title from the fetched event fields */
        cJSON *sc = cJSON_GetObjectItem(p, "score");
        cJSON *ln = cJSON_GetObjectItem(p, "location_name");
        cJSON *fr = cJSON_GetObjectItem(p, "from");
        char t[256];
        snprintf(t, sizeof t, "IODA outage event \xE2\x80\x94 %s%s%.0f%s",
          (ln && cJSON_IsString(ln)) ? ln->valuestring : "Japan",
          (sc && cJSON_IsNumber(sc)) ? " (score " : "",
          (sc && cJSON_IsNumber(sc)) ? sc->valuedouble : 0.0,
          (sc && cJSON_IsNumber(sc)) ? ")" : "");
        if (!(sc && cJSON_IsNumber(sc)))
          snprintf(t, sizeof t, "IODA outage event \xE2\x80\x94 %s",
            (ln && cJSON_IsString(ln)) ? ln->valuestring : "Japan");
        (void)fr;
        cJSON_AddStringToObject(p, "title", t);
      }
      cJSON_AddStringToObject(p, "link",
        "https://ioda.inetintel.cc.gatech.edu/country/JP");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }

  if (bgp) cJSON_Delete(bgp);
  if (ap) cJSON_Delete(ap);
  if (tel) cJSON_Delete(tel);
  if (events) cJSON_Delete(events);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, NULL, NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def ioda_jp_def = {
  .id = "ioda-jp", .collector = "cyber",
  .name = "IODA (JP outages)", .name_ja = "IODA (日本ネット断)",
   .update_interval_sec = 1800, .run = run };
REGISTER_SOURCE(ioda_jp_def)
