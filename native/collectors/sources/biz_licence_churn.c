/* collectors/sources/biz_licence_churn.c
 * Scheduled feeds — municipal business licence registries, the public record of
 * which businesses are opening and closing on a given street.
 *
 * A licence issued is a business opening; an expiry or a DBA end date is one
 * closing. Aggregated by area over time that is a retail-vacancy signal, and
 * unlike foot-traffic panels it is a statutory record: complete, dated, free,
 * and published by the city itself.
 *
 *   • socrata-chicago-business-licences  — City of Chicago licence issuances.
 *   • socrata-sf-business-registrations  — San Francisco registered businesses.
 *
 * Endpoints (Socrata SODA, keyless, verified live 2026-08-09):
 *   https://data.cityofchicago.org/resource/r5kz-chrr.json
 *       → [{legal_name, doing_business_as_name, license_description,
 *           date_issued, expiration_date, address, latitude, longitude, …}]
 *   https://data.sfgov.org/resource/g8m3-pdis.json
 *       → [{dba_name, ownership_name, full_business_address, dba_start_date,
 *           dba_end_date, location:{type:"Point",coordinates:[lon,lat]}, …}]
 *
 * Licence: both datasets are published as open data by their city under terms
 * permitting reuse with attribution, carried in each row's `source`.
 *
 * R2 — THIS FILE IS THE CASE WHERE GEOMETRY IS ALLOWED. Both upstreams return
 * a real per-record position that the city geocoded from the licensed premises:
 * Chicago as `latitude`/`longitude` (numeric strings), San Francisco as a
 * GeoJSON Point whose coordinates are [lon, lat] — note the order. has_geo is
 * set ONLY from those, only when finite, and there is no fallback to a city
 * centroid for the rows where the upstream left them blank; those rows are
 * still emitted, just without a position.
 *
 * NOT INCLUDED, deliberately — three candidates were dropped after probing
 * rather than shipped on assumption:
 *   • EUR-Lex Cellar SPARQL. The endpoint answers, but cdm:work_date_document
 *     returned 2027 and 2029 dates for a "recent legislation" query (measured
 *     2026-08-09) — it is not a publication date. A source with mis-stated date
 *     semantics is worse than none, and the correct predicate was not
 *     established, so the EU leg of the regulatory monitor is left open.
 *   • EUIPO trademarks. The IP Tools API needs OAuth client credentials that
 *     were not available, so neither the endpoint nor the response shape could
 *     be verified. TRADEMARK_SEARCH already covers the pivot.
 *   • OpenChargeMap. Confirmed key-gated (403 "You must specify an API key")
 *     but the response shape could not be verified without one, and ev-charging
 *     / france-irve / autobahn-de already cover the theme. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Emit one row. `data` is TAKEN OVER (printed, then deleted). `lat`/`lon` are
 * used only when `has_geo` is non-zero — the caller decides, from what the
 * upstream actually returned. */
static int bl_emit(intel_sink *sink, cJSON *data, const char *record_type,
                   const char *rk, const char *title, const char *summary,
                   const char *link, const char *published, const char *tags,
                   int has_geo, double lat, double lon) {
  if (!data) return 0;
  /* Every row carrying a coordinate must say how good that coordinate is —
   * otherwise the map cannot tell a licensed-premises fix from a city centroid,
   * and the two look identical at the API boundary. Both publishers geocode the
   * street address of the premises itself, so "address" is the honest label:
   * not a rooftop survey, not an area centroid. */
  if (has_geo) cJSON_AddStringToObject(data, "geo_precision", "address");
  char *pj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);
  if (!pj) return 0;

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.summary         = summary;
  it.link            = link;
  it.published_at    = published;
  it.lang            = "en";
  it.record_type     = record_type;
  it.body            = pj;
  it.properties_json = pj;
  it.tags_json       = tags;
  if (has_geo) { it.has_geo = 1; it.lat = lat; it.lon = lon; }
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Finite and inside the WGS84 envelope. Socrata leaves these blank for records
 * it could not geocode, and 0/0 is a real point in the Gulf of Guinea, not a
 * missing value — so both are rejected. */
static int bl_coord_ok(double lat, double lon) {
  if (!isfinite(lat) || !isfinite(lon)) return 0;
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) return 0;
  if (lat == 0.0 && lon == 0.0) return 0;
  return 1;
}

/* Socrata dates arrive as "2026-08-08T00:00:00.000"; the date part is what an
 * intel row wants. Returns 1 when something was written. */
static int bl_date(const char *raw, char *out, size_t cap) {
  if (!raw || strlen(raw) < 10) { if (cap) out[0] = 0; return 0; }
  snprintf(out, cap, "%.10s", raw);
  return 1;
}

/* --------------------------------------------------------------- Chicago */

static int run_chicago(const source_ctx *ctx, intel_sink *sink) {
  const char *url =
    "https://data.cityofchicago.org/resource/r5kz-chrr.json"
    "?$limit=500&$order=date_issued%20DESC";
  const char *hdrs[] = { "Accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(ctx->http, url, hdrs, 45000);
  if (!doc) { fprintf(stderr, "[chicago-licences] fetch failed\n"); return -1; }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    const char *dba   = jo_sv(r, "doing_business_as_name");
    const char *legal = jo_sv(r, "legal_name");
    const char *name  = dba ? dba : legal;
    if (!name) continue;                        /* no label → no row (R1) */
    const char *kind  = jo_sv(r, "license_description");
    const char *lid   = jo_sv(r, "id");
    const char *addr  = jo_sv(r, "address");

    char issued[16], expires[16];
    bl_date(jo_sv(r, "date_issued"), issued, sizeof issued);
    int has_exp = bl_date(jo_sv(r, "expiration_date"), expires, sizeof expires);

    /* Chicago returns latitude/longitude as numeric strings — jo_numf coerces a
     * fully-numeric string and rejects anything partial. */
    double lat = 0, lon = 0;
    int geo = jo_numf(r, "latitude", &lat) && jo_numf(r, "longitude", &lon) &&
              bl_coord_ok(lat, lon);

    cJSON *data = cJSON_Duplicate(r, 1);
    if (!data) continue;
    cJSON_AddStringToObject(data, "source", "City of Chicago open data");
    cJSON_AddStringToObject(data, "city", "Chicago, IL");
    if (has_exp) cJSON_AddStringToObject(data, "licence_expires", expires);

    char rk[320], title[520];
    snprintf(rk, sizeof rk, "chicago-lic:%s",
             lid ? lid : (jo_sv(r, "license_id") ? jo_sv(r, "license_id") : name));
    snprintf(title, sizeof title, "%s — %s%s%s", name,
             kind ? kind : "business licence",
             addr ? " @ " : "", addr ? addr : "");
    n += bl_emit(sink, data, "business-licence", rk, title, kind,
                 "https://data.cityofchicago.org/d/r5kz-chrr",
                 issued[0] ? issued : NULL,
                 "[\"commercial\",\"business-licence\",\"chicago\"]",
                 geo, lat, lon);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[chicago-licences] emitted %d\n", n);
  return 0;
}

/* --------------------------------------------------------- San Francisco */

static int run_sf(const source_ctx *ctx, intel_sink *sink) {
  const char *url =
    "https://data.sfgov.org/resource/g8m3-pdis.json"
    "?$limit=500&$order=dba_start_date%20DESC";
  const char *hdrs[] = { "Accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(ctx->http, url, hdrs, 45000);
  if (!doc) { fprintf(stderr, "[sf-registrations] fetch failed\n"); return -1; }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    const char *dba  = jo_sv(r, "dba_name");
    const char *own  = jo_sv(r, "ownership_name");
    const char *name = dba ? dba : own;
    if (!name) continue;
    const char *addr = jo_sv(r, "full_business_address");
    const char *uid  = jo_sv(r, "uniqueid");
    const char *naic = jo_sv(r, "naic_code_description");

    char started[16], ended[16];
    bl_date(jo_sv(r, "dba_start_date"), started, sizeof started);
    int closed = bl_date(jo_sv(r, "dba_end_date"), ended, sizeof ended);

    /* location is a GeoJSON Point: coordinates are [lon, lat], in that order. */
    double lat = 0, lon = 0;
    int geo = 0;
    const cJSON *loc = cJSON_GetObjectItem(r, "location");
    const cJSON *co  = cJSON_GetObjectItem(loc, "coordinates");
    if (cJSON_IsArray(co) && cJSON_GetArraySize(co) >= 2) {
      const cJSON *x = cJSON_GetArrayItem(co, 0);
      const cJSON *y = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        lon = x->valuedouble; lat = y->valuedouble;
        geo = bl_coord_ok(lat, lon);
      }
    }

    cJSON *data = cJSON_Duplicate(r, 1);
    if (!data) continue;
    cJSON_AddStringToObject(data, "source", "DataSF open data");
    cJSON_AddStringToObject(data, "city", "San Francisco, CA");
    if (closed) cJSON_AddStringToObject(data, "closed_on", ended);

    char rk[320], title[520];
    snprintf(rk, sizeof rk, "sf-biz:%s", uid ? uid : name);
    snprintf(title, sizeof title, "%s — %s%s%s", name,
             closed ? "closed" : "registered",
             addr ? " @ " : "", addr ? addr : "");
    n += bl_emit(sink, data, "business-licence", rk, title,
                 naic ? naic : own,
                 "https://data.sfgov.org/d/g8m3-pdis",
                 closed ? ended : (started[0] ? started : NULL),
                 "[\"commercial\",\"business-licence\",\"san-francisco\"]",
                 geo, lat, lon);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[sf-registrations] emitted %d\n", n);
  return 0;
}

/* ------------------------------------------------------------- definitions */

static const source_def chicago_lic_def = {
  .id = "socrata-chicago-business-licences", .collector = "commercial",
  .name = "Chicago Business Licences", .name_ja = "シカゴ市 事業許可",
  .update_interval_sec = 43200, .run = run_chicago,
  .category = "commercial", .type = "api",
  .url = "https://data.cityofchicago.org/resource/r5kz-chrr.json",
  .description = "Most recently issued City of Chicago business licences — trade "
                 "name, licence type, premises address and expiry. Keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(chicago_lic_def)

static const source_def sf_biz_def = {
  .id = "socrata-sf-business-registrations", .collector = "commercial",
  .name = "San Francisco Registered Businesses", .name_ja = "サンフランシスコ市 事業登録",
  .update_interval_sec = 43200, .run = run_sf,
  .category = "commercial", .type = "api",
  .url = "https://data.sfgov.org/resource/g8m3-pdis.json",
  .description = "Most recent San Francisco business registrations and closures — "
                 "trade name, owner, address, start and end dates. Keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sf_biz_def)
