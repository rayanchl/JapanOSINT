/* Climate TRACE facility-level greenhouse gas emissions.
 * Endpoint: https://api.climatetrace.org/v6/assets?limit=<n>&offset=<n>
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
 * Licence: Climate TRACE data is CC BY 4.0.
 *
 * PAGINATION, and the `sectors` trap behind it. This collector used to make
 * four requests, one per entry in a SECTORS[] list of {power, steel, cement,
 * oil-and-gas-production}, each `?limit=200&sectors=<name>`. Probing v6
 * directly shows the `sectors` parameter is IGNORED: the same query with
 * sectors=steel, sectors=iron-and-steel and no sectors at all returns byte-for
 * byte the same 50 assets, spanning every sector the dataset has (the real
 * sector values are `iron-and-steel`, `electricity-generation`, … — the names
 * in that list matched nothing either way). So the four calls were four copies
 * of the same first 200 rows: three requests spent for zero additional data,
 * and 200 assets kept out of a dataset that still answers at offset=100000.
 *
 * The fetch is now one offset-paged walk. There is no declared total and the
 * asset set is far larger than a daily run should pull, so the page ceiling is
 * a real bound — which means it is stated in the data as a
 * collector-truncation-notice, and $JO_CLIMATETRACE_PAGES raises it. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "climatetrace-assets"
#define WANT_GAS "co2e_100yr"

#define CT_PAGE_SIZE 1000
#define CT_MAX_PAGES 20   /* exhaustive-ok: offset-walk ceiling on an undeclared total; a stop here emits a collector-truncation-notice */

/* "license restricted" is a withheld value, not a unit — drop it. */
static const char *open_str(const cJSON *o, const char *k) {
  const char *s = jo_sv(o, k);
  if (!s || strcmp(s, "license restricted") == 0) return NULL;
  return s;
}

/* One page. *fetched is set once anything came back; *seen counts the assets
 * the page carried (records, not rows — an asset with no measured emissions is
 * a legitimate skip, not a discard). Returns rows emitted. */
static int collect_page(const source_ctx *ctx, intel_sink *sink, int offset,
                        int *fetched, int *seen) {
  char url[160];
  snprintf(url, sizeof url,
           "https://api.climatetrace.org/v6/assets?limit=%d&offset=%d",
           CT_PAGE_SIZE, offset);
  cJSON *doc = feed_get_json(ctx->http, url, 90000);
  if (!doc) return 0;
  *fetched = 1;

  cJSON *assets = cJSON_GetObjectItem(doc, "assets");
  if (!cJSON_IsArray(assets)) { cJSON_Delete(doc); return 0; }
  *seen = cJSON_GetArraySize(assets);

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
    /* The row's headline number is the co2e_100yr figure, but an asset's
     * EmissionsSummary can carry several gases and every one of them was being
     * dropped once the pick was made. The whole array rides along. */

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
    if (cJSON_IsArray(summ) && cJSON_GetArraySize(summ) > 0)
      cJSON_AddItemToObject(p, "emissions_summary", cJSON_Duplicate(summ, 1));
    const char *nat_id = jo_sv(a, "NativeId");
    if (nat_id) cJSON_AddStringToObject(p, "native_id", nat_id);
    const char *rep = jo_sv(a, "ReportingEntity");
    if (rep) cJSON_AddStringToObject(p, "reporting_entity", rep);
    cJSON *conf = cJSON_GetObjectItem(a, "Confidence");
    if (cJSON_IsArray(conf) && cJSON_GetArraySize(conf) > 0)
      cJSON_AddItemToObject(p, "confidence", cJSON_Duplicate(conf, 1));
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
        /* Centroid.Geometry is the fixed [lon, lat] pair, not a list. */
        cJSON *x = cJSON_GetArrayItem(g, 0), *y = cJSON_GetArrayItem(g, 1);  /* exhaustive-ok: fixed-shape [lon,lat] centroid tuple; both ordinates are read */
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
      snprintf(key, sizeof key, "%s|%s", sec ? sec : "asset", nat ? nat : name);
    snprintf(title, sizeof title, "%s — %.0f t %s (%s)", name, q, WANT_GAS,
             sec ? sec : "unclassified sector");

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
  int max_pages = CT_MAX_PAGES;
  const char *penv = getenv("JO_CLIMATETRACE_PAGES");
  if (penv && *penv) { int v = atoi(penv); if (v > 0) max_pages = v; }

  int total = 0, fetched = 0, offset = 0, pages = 0, more_pending = 0;
  long assets_seen = 0;
  for (int page = 0; page < max_pages; page++) {
    int seen = 0;
    total += collect_page(ctx, sink, offset, &fetched, &seen);
    if (!fetched) break;                     /* first page failed outright   */
    if (seen == 0) break;                    /* ran off the end of the set   */
    pages++;
    assets_seen += seen;
    offset += seen;
    if (seen < CT_PAGE_SIZE) break;          /* short page = end of the set  */
    if (page + 1 == max_pages) more_pending = 1;
  }
  if (!fetched) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  fprintf(stderr, "[" SRC "] emitted %d row(s) from %ld asset(s) over %d "
                  "page(s)\n", total, assets_seen, pages);
  /* The walk stopped at our ceiling, not at the end of the dataset. v6
   * publishes no total, so `available` is honestly reported as unknown. */
  if (more_pending)
    jo_trunc_notice(sink, SRC, "https://api.climatetrace.org/v6/assets",
                    total, -1,
                    "the offset walk reached its page ceiling while Climate "
                    "TRACE was still returning full pages; assets past this "
                    "offset were not requested",
                    "raise $JO_CLIMATETRACE_PAGES");
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
