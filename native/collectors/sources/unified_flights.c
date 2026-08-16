/* collectors/transport/sources/unified_flights.c
 * Port of server/src/collectors/unifiedFlights.js — a thin passthrough:
 * `return getSnapshot()` from planeAdsbPoller. The poller's OpenSky-live ⊕
 * adsb.lol fusion (mergeLiveByIcao + classifyMilitary) is reproduced by the
 * registered `flight-adsb` source; unified-flights just captures it and
 * re-emits under source_id `unified-flights` so /api/data/unified-flights +
 * the unified read-side work. Mirrors the concurrent session's bespoke
 * unified_ais_ships.c shape (lib/unified.h capture sink); no dedupe — the
 * poller already deduped (JS is a pure passthrough). _meta dropped per
 * RULE 8; emitted via the geojson sink. */
#include "source.h"
#include "lib/unified.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *pstr(cJSON *p, const char *k) {
  cJSON *v = cJSON_GetObjectItem(p, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

/* audit-09: the captured ADS-B features carry callsign / registration /
 * aircraft_type / altitude but no `title`, `published_at` or `link` key — and
 * geojson_emit_features only picks title from title|name|name_ja|label. Every
 * one of the ~90 live aircraft therefore landed in intel_items with a NULL
 * title and no timeline position. Derive both from what the snapshot already
 * has; nothing is invented. */
static void label_feature(cJSON *f) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  if (!p) return;
  const char *cs   = pstr(p, "callsign");
  const char *icao = pstr(p, "icao24");
  const char *reg  = pstr(p, "registration");
  const char *typ  = pstr(p, "aircraft_type");
  cJSON *alt = cJSON_GetObjectItem(p, "altitude_ft");

  /* audit-09 (2nd pass): this used to `return` as soon as the snapshot carried
   * a `title`. The flight-adsb capture now labels its own features, so the
   * early return skipped the published_at and link derivation too and all ~90
   * rows still landed with NULL link and NULL published_at. Each field is
   * filled independently now. */
  if (!cJSON_GetObjectItem(p, "title")) {
    /* Only the fourth append below was bounds-checked. snprintf returns the
     * length it WOULD have written, so any of these three truncating left `o`
     * past the buffer, and the next `title + o` / `sizeof title - o` was an
     * out-of-bounds pointer with a size that wraps. Clamp after each. */
    char title[192]; int o = 0;
    #define TT_ADV(expr) do { int w_ = (expr); \
      if (w_ > 0) o += w_; \
      if (o > (int)sizeof title - 1) o = (int)sizeof title - 1; } while (0)
    TT_ADV(snprintf(title + o, sizeof title - (size_t)o, "%s",
                    (cs && *cs) ? cs : (icao ? icao : "aircraft")));
    if (typ) TT_ADV(snprintf(title + o, sizeof title - (size_t)o, " · %s", typ));
    if (reg) TT_ADV(snprintf(title + o, sizeof title - (size_t)o, " (%s)", reg));
    #undef TT_ADV
    if (alt && cJSON_IsNumber(alt) && alt->valuedouble > 0 &&
        o < (int)sizeof title)
      snprintf(title + o, sizeof title - o, " · %.0f ft", alt->valuedouble);
    cJSON_AddStringToObject(p, "title", title);
  }

  cJSON *lc = cJSON_GetObjectItem(p, "last_contact");
  if (!lc || !cJSON_IsNumber(lc)) lc = cJSON_GetObjectItem(p, "time_position");
  if (cJSON_GetObjectItem(p, "published_at")) lc = NULL;
  if (lc && cJSON_IsNumber(lc) && lc->valuedouble > 0) {
    time_t t = (time_t)lc->valuedouble;
    struct tm tm; gmtime_r(&t, &tm);
    char iso[32]; strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", &tm);
    cJSON_AddStringToObject(p, "published_at", iso);
  }
  if (icao && !cJSON_GetObjectItem(p, "link")) {
    char link[128];
    snprintf(link, sizeof link, "https://globe.adsbexchange.com/?icao=%s", icao);
    cJSON_AddStringToObject(p, "link", link);
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *raw = cJSON_CreateArray();
  unified_capture(ctx, "flight-adsb", raw);   /* == flight-adsb snapshot */
  cJSON *f;
  cJSON_ArrayForEach(f, raw) label_feature(f);
  int n = geojson_emit_features(sink, "unified-flights", raw);
  cJSON_Delete(raw);
  fprintf(stderr, "[unified-flights] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def unified_flights_def = {
  .id = "unified-flights", .collector = "transport",
  .name = "Unified Flights (live ADS-B)", .name_ja = "Unified Flights",
   .update_interval_sec = 60, .run = run };
REGISTER_SOURCE(unified_flights_def)
