/* Climate TRACE facility-level greenhouse gas emissions.
 * Endpoint: https://api.climatetrace.org/v6/assets?limit=200&sectors=<sector>
 * Keyless. Emits one row per asset that returned an emissions figure:
 *   emissions_quantity (UNIT: tonnes of the gas named in `gas`, we take
 *   gas="co2e_100yr" explicitly rather than summing across gases),
 *   emissions_factor + its own units string, capacity + capacity units,
 *   owner company names, sector, country.
 *
 * GEOMETRY (R2): the per-asset Centroid is {"Geometry":[lon,lat],"SRID":4326}
 * and is the ONLY location emitted. The response's TOP-LEVEL `bbox` is the
 * bounding box of the whole page and is never used as a facility location.
 * LICENCE TRAP: ActivityUnits / CapacityUnits come back literally as the string
 * "license restricted" where the commercial input cannot be redistributed —
 * those are emitted as ABSENT, never as a value.
 * Licence: Climate TRACE data is CC BY 4.0. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "climatetrace-assets"
#define WANT_GAS "co2e_100yr"

static const char *SECTORS[] = { "power", "oil-and-gas-production", "steel",
                                 "cement", NULL };

/* "license restricted" is a withheld value, not a unit — drop it. */
static const char *open_str(const cJSON *o, const char *k) {
  const char *s = jo_sv(o, k);
  if (!s || strcmp(s, "license restricted") == 0) return NULL;
  return s;
}

static int collect_sector(const source_ctx *ctx, intel_sink *sink,
                          const char *sector, int *fetched) {
  char url[256];
  snprintf(url, sizeof url,
           "https://api.climatetrace.org/v6/assets?limit=200&sectors=%s", sector);
  cJSON *doc = feed_get_json(ctx->http, url, 40000);
  if (!doc) return 0;
  *fetched = 1;

  cJSON *assets = cJSON_GetObjectItem(doc, "assets");
  if (!cJSON_IsArray(assets)) { cJSON_Delete(doc); return 0; }

  int n = 0;
  cJSON *a;
  cJSON_ArrayForEach(a, assets) {
    const char *name = jo_sv(a, "Name");
    if (!name) continue;

    /* pick ONE gas explicitly rather than summing heterogeneous gases */
    cJSON *summ = cJSON_GetObjectItem(a, "EmissionsSummary");
    cJSON *pick = NULL, *es;
    if (cJSON_IsArray(summ)) {
      cJSON_ArrayForEach(es, summ) {
        const char *g = jo_sv(es, "Gas");
        if (g && strcmp(g, WANT_GAS) == 0 &&
            cJSON_IsNumber(cJSON_GetObjectItem(es, "EmissionsQuantity"))) {
          pick = es; break;
        }
      }
    }
    if (!pick) continue;             /* no measured emissions -> no row (R1) */
    double q = cJSON_GetObjectItem(pick, "EmissionsQuantity")->valuedouble;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "facility_name", name);
    const char *cc = jo_sv(a, "Country");
    if (cc) cJSON_AddStringToObject(p, "country", cc);
    const char *sec = jo_sv(a, "Sector");
    if (sec) cJSON_AddStringToObject(p, "sector", sec);
    const char *at = jo_sv(a, "AssetType");
    if (at) cJSON_AddStringToObject(p, "asset_type", at);
    cJSON_AddStringToObject(p, "gas", WANT_GAS);
    cJSON_AddNumberToObject(p, "emissions_quantity", q);
    cJSON_AddStringToObject(p, "emissions_unit", "tonnes");
    cJSON *ef = cJSON_GetObjectItem(pick, "EmissionsFactor");
    const char *efu = open_str(pick, "EmissionsFactorUnits");
    if (cJSON_IsNumber(ef) && efu) {
      cJSON_AddNumberToObject(p, "emissions_factor", ef->valuedouble);
      cJSON_AddStringToObject(p, "emissions_factor_unit", efu);
    }
    cJSON *cap = cJSON_GetObjectItem(pick, "Capacity");
    const char *capu = open_str(pick, "CapacityUnits");
    if (cJSON_IsNumber(cap) && capu) {
      cJSON_AddNumberToObject(p, "capacity", cap->valuedouble);
      cJSON_AddStringToObject(p, "capacity_unit", capu);
    }
    cJSON *owners = cJSON_GetObjectItem(a, "Owners");
    if (cJSON_IsArray(owners)) {
      cJSON *ol = cJSON_CreateArray(), *o;
      cJSON_ArrayForEach(o, owners) {
        const char *cn = jo_sv(o, "CompanyName");
        if (!cn) continue;
        int dup = 0, m = cJSON_GetArraySize(ol);
        for (int i = 0; i < m; i++) {
          cJSON *x = cJSON_GetArrayItem(ol, i);
          if (cJSON_IsString(x) && strcmp(x->valuestring, cn) == 0) { dup = 1; break; }
        }
        if (!dup) cJSON_AddItemToArray(ol, cJSON_CreateString(cn));
      }
      if (cJSON_GetArraySize(ol) > 0) cJSON_AddItemToObject(p, "owners", ol);
      else cJSON_Delete(ol);
    }

    /* per-asset Centroid ONLY (never the page bbox) */
    int has_geo = 0; double lat = 0, lon = 0;
    cJSON *cen = cJSON_GetObjectItem(a, "Centroid");
    if (cJSON_IsObject(cen)) {
      cJSON *srid = cJSON_GetObjectItem(cen, "SRID");
      cJSON *g = cJSON_GetObjectItem(cen, "Geometry");
      if (cJSON_IsArray(g) && cJSON_GetArraySize(g) >= 2 &&
          (!cJSON_IsNumber(srid) || (int)srid->valuedouble == 4326)) {
        cJSON *x = cJSON_GetArrayItem(g, 0), *y = cJSON_GetArrayItem(g, 1);  /* exhaustive-ok: fixed [lon,lat] pair */
        if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
          lon = x->valuedouble; lat = y->valuedouble;   /* [lon, lat] */
          if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) has_geo = 1;
        }
      }
    }
    if (has_geo) cJSON_AddStringToObject(p, "geo_precision", "facility-centroid");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[160], title[320];
    cJSON *id = cJSON_GetObjectItem(a, "Id");
    const char *nat = jo_sv(a, "NativeId");
    if (cJSON_IsNumber(id))
      snprintf(key, sizeof key, "climatetrace:%lld", (long long)id->valuedouble);
    else
      snprintf(key, sizeof key, "%s|%s", sector, nat ? nat : name);
    snprintf(title, sizeof title, "%s — %.0f t %s (%s)", name, q, WANT_GAS,
             sec ? sec : sector);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = "https://climatetrace.org/";
    row.record_type     = "emitting-facility";
    row.has_geo         = has_geo;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"industry\",\"emissions\",\"climate-trace\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, fetched = 0;
  for (int i = 0; SECTORS[i]; i++)
    total += collect_sector(ctx, sink, SECTORS[i], &fetched);
  if (!fetched) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  fprintf(stderr, "[" SRC "] emitted %d\n", total);
  return 0;
}

static const source_def eni_climatetrace_assets_def = {
  .id = SRC, .collector = "industry",
  .name = "Climate TRACE facility-level greenhouse gas emissions",
  .update_interval_sec = 86400, .run = run,
  .category = "industry", .type = "api",
  .url = "https://api.climatetrace.org/v6/assets",
  .description = "Satellite-derived emissions estimates (tonnes CO2e-100yr) for individual power stations, refineries, steel mills, cement plants and oil/gas fields worldwide, with owner attribution and coordinates.",
  .license = "Climate TRACE, CC BY 4.0 (some per-asset activity/capacity fields are licence-restricted and are omitted).",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_climatetrace_assets_def)
