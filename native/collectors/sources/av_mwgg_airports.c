/* collectors/sources/av_mwgg_airports.c
 * mwgg/Airports — ICAO-keyed global airport reference table (JSON).
 * Endpoint: https://raw.githubusercontent.com/mwgg/Airports/master/airports.json
 *
 * Emits per airport, all straight from the file: icao, iata, name, city,
 * state, country, elevation and — the part nothing else in the fleet has —
 * `tz`, the IANA timezone, which is what lets a local departure time in a
 * schedule be converted to UTC.
 *
 * TRAP (quoted from the work list): "The top level is an OBJECT keyed by ICAO,
 * not an array — iterate the object's children, and take the key as the ICAO
 * when the inner `icao` disagrees. `iata` is an empty string, not null, when
 * absent." Both are handled below (jo_sv maps "" → NULL).
 *
 * GEOMETRY (R2): `lat`/`lon` per airport object, present for every sampled
 * entry. An entry without both is emitted with has_geo=0; nothing is derived
 * from the city or country name.
 *
 * R3: fetch/parse failure → -1; parsed but empty → 0.
 *
 * Licence: MIT-licensed community dataset (mwgg/Airports on GitHub), derived
 * from public airport data. Keyless.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_common.inc"

#define MW_FLUSH 1000

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *text = feed_get_text(ctx->http,
      "https://raw.githubusercontent.com/mwgg/Airports/master/airports.json",
      60000);
  if (!text || strlen(text) < 64) {
    free(text);
    fprintf(stderr, "[mwgg-airports] fetch failed\n");
    return -1;
  }
  cJSON *doc = cJSON_Parse(text);
  free(text);
  if (!doc || !cJSON_IsObject(doc)) {
    cJSON_Delete(doc);
    fprintf(stderr, "[mwgg-airports] unparseable JSON object\n");
    return -1;
  }

  cJSON *feats = cJSON_CreateArray();
  int emitted = 0;
  cJSON *ap;
  cJSON_ArrayForEach(ap, doc) {           /* children of an OBJECT, keyed ICAO */
    if (!cJSON_IsObject(ap)) continue;
    /* The object KEY is the authoritative ICAO when the inner field differs. */
    const char *key = ap->string;
    const char *inner = jo_sv(ap, "icao");
    const char *icao = key && key[0] ? key : inner;
    const char *name = jo_sv(ap, "name");
    if (!icao || !name) continue;
    const char *iata = jo_sv(ap, "iata");   /* "" when absent → NULL */
    const char *city = jo_sv(ap, "city");
    const char *cc   = jo_sv(ap, "country");
    const char *tz   = jo_sv(ap, "tz");

    double lat, lon;
    cJSON *geom = NULL;
    if (av_dbl(ap, "lat", &lat) && av_dbl(ap, "lon", &lon) &&
        lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
      geom = cJSON_CreateObject();
      cJSON_AddStringToObject(geom, "type", "Point");
      cJSON *c = cJSON_CreateArray();
      cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(geom, "coordinates", c);
    }

    char title[288];
    snprintf(title, sizeof title, "%s%s%s — %s%s%s%s%s", icao,
             iata ? " / " : "", iata ? iata : "", name,
             city ? ", " : "", city ? city : "", cc ? ", " : "", cc ? cc : "");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "uid", icao);
    cJSON_AddStringToObject(p, "title", title);
    cJSON_AddStringToObject(p, "icao", icao);
    if (inner && strcmp(inner, icao) != 0)
      cJSON_AddStringToObject(p, "icao_field", inner);
    if (iata) cJSON_AddStringToObject(p, "iata", iata);
    cJSON_AddStringToObject(p, "name", name);
    av_copy(p, ap, "city");
    av_copy(p, ap, "state");
    av_copy(p, ap, "country");
    av_copy(p, ap, "elevation");
    if (tz) cJSON_AddStringToObject(p, "tz", tz);
    if (!geom) cJSON_AddStringToObject(p, "geometry_note",
                 "no lat/lon published for this entry");
    cJSON_AddStringToObject(p, "record_type", "airport");
    cJSON_AddStringToObject(p, "source", "mwgg/Airports (MIT)");
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("airport"));
    cJSON_AddItemToObject(p, "tags", tags);
    cJSON_AddItemToArray(feats, av_feature(geom, p));

    if (cJSON_GetArraySize(feats) >= MW_FLUSH) {
      emitted += geojson_emit_features(sink, ctx->source_id, feats);
      cJSON_Delete(feats);
      feats = cJSON_CreateArray();
    }
  }
  emitted += geojson_emit_features(sink, ctx->source_id, feats);
  cJSON_Delete(feats);
  cJSON_Delete(doc);
  fprintf(stderr, "[mwgg-airports] emitted %d\n", emitted);
  return 0;
}

static const source_def av_mwgg_airports_def = {
  .id = "mwgg-airports", .collector = "transport",
  .name = "mwgg/Airports ICAO-keyed Global Airport Reference",
  .update_interval_sec = 2592000, .run = run,
  .category = "transport", .type = "dataset",
  .url = "https://raw.githubusercontent.com/mwgg/Airports/master/airports.json",
  .description = "A compact ICAO-keyed airport table with IATA cross-reference, elevation, country/state and IANA timezone — the timezone field being what lets a local schedule time be converted to UTC.",
  .license = "MIT-licensed community dataset (mwgg/Airports on GitHub), derived from public airport data. Keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_mwgg_airports_def)
