/* collectors/sources/biodiversity_world.c
 * OSINT services — global biodiversity / species-occurrence pivots. On-demand
 * entity pivot (ctx->entity = a species name, scientific name, place or free
 * text) against the major keyless, public occurrence APIs:
 *
 *   GBIF_OCCURRENCE  GBIF — api.gbif.org/v1/occurrence/search (global aggregate
 *                    of museum/citizen/survey records; has_geo filters to
 *                    georeferenced hits so we can emit real coordinates)
 *   INATURALIST      iNaturalist — api.inaturalist.org/v1/observations (citizen
 *                    science observations with photos, place & observer)
 *   OBIS_MARINE      OBIS — api.obis.org/v3/occurrence (marine biodiversity;
 *                    filtered by scientificname)
 *
 * All three are keyless and LIVE. We fetch, parse the JSON results array, and
 * emit one intel_item per occurrence record — every field (species, coords,
 * date, locality, dataset) is a REAL parsed value from the API. No entity,
 * fetch failure, or no matches → honest empty (return 0). Nothing synthesized.
 *
 * One run() dispatches on ctx->source_id. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Emit one occurrence hit. Coordinates only carried when really present. */
static int bio_emit(intel_sink *sink, const char *service, const char *rectype,
                    const char *query, const char *key,
                    const char *species, const char *sci,
                    const char *locality, const char *dataset,
                    const char *observed, const char *link,
                    int have_geo, double lat, double lon) {
  const char *title = species ? species : (sci ? sci : "occurrence");

  cJSON *data = cJSON_CreateObject();
  if (species)  cJSON_AddStringToObject(data, "species", species);
  if (sci)      cJSON_AddStringToObject(data, "scientific_name", sci);
  if (locality) cJSON_AddStringToObject(data, "locality", locality);
  if (dataset)  cJSON_AddStringToObject(data, "dataset", dataset);
  if (observed) cJSON_AddStringToObject(data, "observed", observed);
  if (have_geo) { cJSON_AddNumberToObject(data, "lat", lat);
                  cJSON_AddNumberToObject(data, "lon", lon); }
  cJSON_AddStringToObject(data, "source", service);
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", service);
  cJSON_AddStringToObject(props, "source", service);
  if (sci)      cJSON_AddStringToObject(props, "scientific_name", sci);
  if (dataset)  cJSON_AddStringToObject(props, "dataset", dataset);
  if (query)    cJSON_AddStringToObject(props, "query", query);
  if (have_geo) { cJSON_AddNumberToObject(props, "lat", lat);
                  cJSON_AddNumberToObject(props, "lon", lon); }
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = key ? key : title;
  it.title           = title;
  it.summary         = locality ? locality : sci;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = observed;
  it.link            = link;
  it.record_type     = rectype;
  it.has_geo         = have_geo;
  it.lat             = have_geo ? lat : 0;
  it.lon             = have_geo ? lon : 0;
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"biodiversity\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* ---- GBIF occurrence search -------------------------------------------- *
 * /v1/occurrence/search?q=<entity>&has_coordinate=true&limit=30 → { results:
 * [ { key, species, scientificName, decimalLatitude, decimalLongitude,
 *     eventDate, locality/verbatimLocality, datasetName, ... } ] } */
static int bio_gbif(const source_ctx *ctx, intel_sink *sink, const char *enc,
                    const char *q, int max) {
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.gbif.org/v1/occurrence/search"
    "?q=%s&has_coordinate=true&limit=%d", enc, max);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (biodiversity)", NULL };
  char *body = jo_get(ctx, url, hdrs, "GBIF_OCCURRENCE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *sci = jo_sv(r, "scientificName");
      const char *sp  = jo_sv(r, "species");
      const char *loc = jo_sv(r, "locality");
      if (!loc) loc = jo_sv(r, "verbatimLocality");
      const char *ds  = jo_sv(r, "datasetName");
      const char *dt  = jo_sv(r, "eventDate");
      double lat, lon;
      int geo = jo_num(r, "decimalLatitude", &lat) &&
                jo_num(r, "decimalLongitude", &lon);
      const cJSON *kv = cJSON_GetObjectItem(r, "key");
      char key[128], link[160];
      key[0] = 0; link[0] = 0;
      if (kv && cJSON_IsNumber(kv)) {
        long long id = (long long)kv->valuedouble;
        snprintf(key, sizeof key, "GBIF|%lld", id);
        snprintf(link, sizeof link, "https://www.gbif.org/occurrence/%lld", id);
      }
      emitted += bio_emit(sink, "GBIF_OCCURRENCE", "gbif-occurrence", q,
                          key[0] ? key : NULL, sp, sci, loc, ds, dt,
                          link[0] ? link : NULL, geo, lat, lon);
      if (emitted >= max) break;
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[GBIF_OCCURRENCE] emitted %d\n", emitted);
  return emitted;
}

/* ---- iNaturalist observations ------------------------------------------ *
 * /v1/observations?q=<entity>&per_page=30 → { results: [ { id, uri, place_guess,
 *   observed_on, geojson{coordinates:[lon,lat]}, taxon{name,preferred_common_name},
 *   ... } ] } */
static int bio_inat(const source_ctx *ctx, intel_sink *sink, const char *enc,
                    const char *q, int max) {
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.inaturalist.org/v1/observations"
    "?q=%s&per_page=%d&order_by=observed_on", enc, max);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (biodiversity)", NULL };
  char *body = jo_get(ctx, url, hdrs, "INATURALIST");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const cJSON *taxon = cJSON_GetObjectItem(r, "taxon");
      const char *sci = taxon ? jo_sv(taxon, "name") : NULL;
      const char *sp  = taxon ? jo_sv(taxon, "preferred_common_name") : NULL;
      const char *loc = jo_sv(r, "place_guess");
      const char *dt  = jo_sv(r, "observed_on");
      const char *uri = jo_sv(r, "uri");
      /* geojson.coordinates = [lon, lat] */
      double lat = 0, lon = 0; int geo = 0;
      const cJSON *gj = cJSON_GetObjectItem(r, "geojson");
      const cJSON *co = gj ? cJSON_GetObjectItem(gj, "coordinates") : NULL;
      if (co && cJSON_IsArray(co) && cJSON_GetArraySize(co) >= 2) {
        const cJSON *lonj = cJSON_GetArrayItem(co, 0);
        const cJSON *latj = cJSON_GetArrayItem(co, 1);
        if (lonj && latj && cJSON_IsNumber(lonj) && cJSON_IsNumber(latj)) {
          lon = lonj->valuedouble; lat = latj->valuedouble; geo = 1;
        }
      }
      const cJSON *idv = cJSON_GetObjectItem(r, "id");
      char key[96];
      key[0] = 0;
      if (idv && cJSON_IsNumber(idv))
        snprintf(key, sizeof key, "INAT|%lld", (long long)idv->valuedouble);
      emitted += bio_emit(sink, "INATURALIST", "inaturalist-observation", q,
                          key[0] ? key : NULL, sp, sci, loc, "iNaturalist",
                          dt, uri, geo, lat, lon);
      if (emitted >= max) break;
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[INATURALIST] emitted %d\n", emitted);
  return emitted;
}

/* ---- OBIS marine occurrence -------------------------------------------- *
 * /v3/occurrence?scientificname=<entity>&size=30 → { results: [ { id,
 *   scientificName, decimalLatitude, decimalLongitude, eventDate, locality,
 *   dataset_id / datasetName, ... } ] } */
static int bio_obis(const source_ctx *ctx, intel_sink *sink, const char *enc,
                    const char *q, int max) {
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.obis.org/v3/occurrence?scientificname=%s&size=%d", enc, max);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (biodiversity)", NULL };
  char *body = jo_get(ctx, url, hdrs, "OBIS_MARINE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *sci = jo_sv(r, "scientificName");
      const char *sp  = jo_sv(r, "species");
      const char *loc = jo_sv(r, "locality");
      if (!loc) loc = jo_sv(r, "waterBody");
      const char *ds  = jo_sv(r, "datasetName");
      const char *dt  = jo_sv(r, "eventDate");
      double lat, lon;
      int geo = jo_num(r, "decimalLatitude", &lat) &&
                jo_num(r, "decimalLongitude", &lon);
      const char *id  = jo_sv(r, "id");
      char key[160];
      key[0] = 0;
      if (id) snprintf(key, sizeof key, "OBIS|%.140s", id);
      /* OBIS has no stable per-record web page → no link */
      emitted += bio_emit(sink, "OBIS_MARINE", "obis-occurrence", q,
                          key[0] ? key : NULL, sp, sci, loc, ds, dt,
                          NULL, geo, lat, lon);
      if (emitted >= max) break;
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[OBIS_MARINE] emitted %d\n", emitted);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  int emitted = 0;
  if (strcmp(ctx->source_id, "GBIF_OCCURRENCE") == 0)
    emitted = bio_gbif(ctx, sink, enc, q, 30);
  else if (strcmp(ctx->source_id, "INATURALIST") == 0)
    emitted = bio_inat(ctx, sink, enc, q, 30);
  else if (strcmp(ctx->source_id, "OBIS_MARINE") == 0)
    emitted = bio_obis(ctx, sink, enc, q, 30);
  else { free(enc); return -1; }

  (void)emitted;
  free(enc);
  return 0;   /* honest empty is not an error */
}

#define BIO_DEF(SYM, ID, NAME, NAMEJA, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "environment", .type = "api", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

BIO_DEF(bio_gbif_def, "GBIF_OCCURRENCE", "GBIF Species Occurrence", "GBIF 生物種出現記録",
  "https://api.gbif.org/v1/occurrence/search",
  "GBIF global species-occurrence search (keyless): species, coordinates, date, locality, dataset");
BIO_DEF(bio_inat_def, "INATURALIST", "iNaturalist Observations", "iNaturalist 観察記録",
  "https://api.inaturalist.org/v1/observations",
  "iNaturalist citizen-science observations (keyless): taxon, place, date, coordinates");
BIO_DEF(bio_obis_def, "OBIS_MARINE", "OBIS Marine Biodiversity", "OBIS 海洋生物多様性",
  "https://api.obis.org/v3/occurrence",
  "OBIS marine biodiversity occurrence search by scientific name (keyless): species, coordinates, date");
