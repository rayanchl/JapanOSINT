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

#define TOKYO_LON 139.6917
#define TOKYO_LAT 35.6895
#define IODA_BASE "https://api.ioda.inetintel.cc.gatech.edu/v2"

static void add_tokyo_geom(cJSON *f) {
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *co = cJSON_CreateArray();
  cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LON));
  cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LAT));
  cJSON_AddItemToObject(g, "coordinates", co);
  cJSON_AddItemToObject(f, "geometry", g);
}

static cJSON *dup_or_null(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && !cJSON_IsNull(v)) ? cJSON_Duplicate(v, 1) : cJSON_CreateNull();
}

/* summarise(label,payload): series = payload.data[] ; sample_values =
 * flatMap(s => s.values.slice(-3)). */
static void summarise(cJSON *features, const char *label, cJSON *payload) {
  cJSON *data = payload ? cJSON_GetObjectItem(payload, "data") : NULL;
  int series_count = cJSON_IsArray(data) ? cJSON_GetArraySize(data) : 0;
  cJSON *samples = cJSON_CreateArray();
  if (cJSON_IsArray(data)) {
    cJSON *s;
    cJSON_ArrayForEach(s, data) {
      cJSON *vals = cJSON_GetObjectItem(s, "values");
      if (cJSON_IsArray(vals)) {
        int n = cJSON_GetArraySize(vals);
        int start = n > 3 ? n - 3 : 0;
        for (int k = start; k < n; k++)
          cJSON_AddItemToArray(samples,
            cJSON_Duplicate(cJSON_GetArrayItem(vals, k), 1));
      }
    }
  }
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  add_tokyo_geom(f);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "kind", "signal");
  cJSON_AddStringToObject(p, "datasource", label);
  cJSON_AddNumberToObject(p, "series_count", series_count);
  cJSON_AddItemToObject(p, "sample_values", samples);
  cJSON_AddItemToObject(p, "err", dup_or_null(payload, "err"));
  cJSON_AddStringToObject(p, "source", "ioda_signals");
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
