/* collectors/telecom/sources/internet_exchanges.c
 * Port of server/src/collectors/internetExchanges.js (tryPeeringDb).
 * Two PeeringDB JSON fetches (fac then ix), concatenated into one
 * FeatureCollection. SEED_IX offline fallback intentionally not ported
 * (JS does `if (!live) features = []`).
 */
#include "../../source.h"
#include "../../lib/geojson.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>

#define IX_URL  "https://www.peeringdb.com/api/ix?country=JP"
#define FAC_URL "https://www.peeringdb.com/api/fac?country=JP"
#define TIMEOUT_MS 10000

/* JS: v || null  — string value or JSON null. */
static cJSON *str_or_null(const cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  if (v && cJSON_IsString(v) && v->valuestring[0])
    return cJSON_CreateString(v->valuestring);
  return cJSON_CreateNull();
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Accept: application/json", NULL };
  cJSON *ixJson  = feed_get_json_h(ctx->http, IX_URL, hdrs, TIMEOUT_MS);
  cJSON *facJson = feed_get_json_h(ctx->http, FAC_URL, hdrs, TIMEOUT_MS);

  cJSON *features = cJSON_CreateArray();

  /* Facilities hosting at least one IX (ix_count > 0). */
  cJSON *facData = facJson ? cJSON_GetObjectItem(facJson, "data") : NULL;
  if (cJSON_IsArray(facData)) {
    cJSON *f;
    cJSON_ArrayForEach(f, facData) {
      cJSON *lat = cJSON_GetObjectItem(f, "latitude");
      cJSON *lon = cJSON_GetObjectItem(f, "longitude");
      if (!(lat && !cJSON_IsNull(lat) && lon && !cJSON_IsNull(lon))) continue;
      cJSON *ixc = cJSON_GetObjectItem(f, "ix_count");
      double ixcv = (ixc && cJSON_IsNumber(ixc)) ? ixc->valuedouble : 0;
      if (ixcv == 0) continue;

      cJSON *feat = cJSON_CreateObject();
      cJSON_AddStringToObject(feat, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_Duplicate(lon, 1));
      cJSON_AddItemToArray(co, cJSON_Duplicate(lat, 1));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(feat, "geometry", g);

      cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
      cJSON *idv = cJSON_GetObjectItem(f, "id");
      char idb[64];
      if (idv && cJSON_IsNumber(idv))
        snprintf(idb, sizeof idb, "PDB_FAC_%g", idv->valuedouble);
      else if (idv && cJSON_IsString(idv))
        snprintf(idb, sizeof idb, "PDB_FAC_%s", idv->valuestring);
      else
        snprintf(idb, sizeof idb, "PDB_FAC_");
      cJSON_AddStringToObject(p, "ix_id", idb);
      cJSON *nm = cJSON_GetObjectItem(f, "name");
      cJSON_AddItemToObject(p, "name",
        nm ? cJSON_Duplicate(nm, 1) : cJSON_CreateNull());
      cJSON *org = cJSON_GetObjectItem(f, "org_name");
      cJSON_AddStringToObject(p, "operator",
        (org && cJSON_IsString(org) && org->valuestring[0])
          ? org->valuestring : "unknown");
      cJSON *city = cJSON_GetObjectItem(f, "city");
      cJSON_AddItemToObject(p, "city",
        city ? cJSON_Duplicate(city, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "address", str_or_null(f, "address1"));
      cJSON_AddItemToObject(p, "ix_count",
        ixc ? cJSON_Duplicate(ixc, 1) : cJSON_CreateNull());
      cJSON *netc = cJSON_GetObjectItem(f, "net_count");
      cJSON_AddItemToObject(p, "net_count",
        netc ? cJSON_Duplicate(netc, 1) : cJSON_CreateNull());
      cJSON_AddStringToObject(p, "country", "JP");
      cJSON_AddStringToObject(p, "source", "peeringdb_fac");
      cJSON_AddItemToObject(feat, "properties", p);
      cJSON_AddItemToArray(features, feat);
    }
  }

  /* Raw IX records (city-level labels). */
  cJSON *ixData = ixJson ? cJSON_GetObjectItem(ixJson, "data") : NULL;
  if (cJSON_IsArray(ixData)) {
    cJSON *ix;
    cJSON_ArrayForEach(ix, ixData) {
      /* AUDIT NOTE (slice a3, 2026-07-31): this used to `continue` whenever an
       * IX record had no latitude/longitude — and PeeringDB's /api/ix objects
       * NEVER carry coordinates (they expose city/country/region only; the
       * coordinates live on /api/fac). So the entire second half of this
       * collector silently discarded every record it fetched, and all the rows
       * the source has ever produced came from the facilities endpoint alone.
       * With /api/fac rate-limited (429 from this host today) the source
       * returns nothing at all. An IX with a name, operator and city is real,
       * useful inventory; it is emitted now with NO geometry rather than
       * dropped — and no coordinate is invented for it.
       * NB the "geometry" key is omitted, not set to JSON null: lib/geojson.c
       * :235 serialises whatever is under it, so a null persists as "null". */
      cJSON *lat = cJSON_GetObjectItem(ix, "latitude");
      cJSON *lon = cJSON_GetObjectItem(ix, "longitude");
      int has_pt = (lat && !cJSON_IsNull(lat) && lon && !cJSON_IsNull(lon));

      cJSON *feat = cJSON_CreateObject();
      cJSON_AddStringToObject(feat, "type", "Feature");
      if (has_pt) {
        cJSON *g = cJSON_CreateObject();
        cJSON_AddStringToObject(g, "type", "Point");
        cJSON *co = cJSON_CreateArray();
        cJSON_AddItemToArray(co, cJSON_Duplicate(lon, 1));
        cJSON_AddItemToArray(co, cJSON_Duplicate(lat, 1));
        cJSON_AddItemToObject(g, "coordinates", co);
        cJSON_AddItemToObject(feat, "geometry", g);
      }

      cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
      cJSON *idv = cJSON_GetObjectItem(ix, "id");
      char idb[64];
      if (idv && cJSON_IsNumber(idv))
        snprintf(idb, sizeof idb, "PDB_IX_%g", idv->valuedouble);
      else if (idv && cJSON_IsString(idv))
        snprintf(idb, sizeof idb, "PDB_IX_%s", idv->valuestring);
      else
        snprintf(idb, sizeof idb, "PDB_IX_");
      /* ix_id is not in lib/geojson.c NATIVE_ID_KEYS, so without a "uid" the row
       * fell back to a sha1 over {geometry, properties} and re-identified itself
       * whenever any field changed. PeeringDB's numeric id is the stable key. */
      cJSON_AddStringToObject(p, "uid", idb);
      cJSON_AddStringToObject(p, "ix_id", idb);
      cJSON *nm = cJSON_GetObjectItem(ix, "name");
      cJSON_AddItemToObject(p, "name",
        nm ? cJSON_Duplicate(nm, 1) : cJSON_CreateNull());
      /* operator: org_name || name_long || 'unknown' */
      cJSON *org = cJSON_GetObjectItem(ix, "org_name");
      cJSON *nl  = cJSON_GetObjectItem(ix, "name_long");
      if (org && cJSON_IsString(org) && org->valuestring[0])
        cJSON_AddStringToObject(p, "operator", org->valuestring);
      else if (nl && cJSON_IsString(nl) && nl->valuestring[0])
        cJSON_AddStringToObject(p, "operator", nl->valuestring);
      else
        cJSON_AddStringToObject(p, "operator", "unknown");
      cJSON *city = cJSON_GetObjectItem(ix, "city");
      cJSON_AddItemToObject(p, "city",
        city ? cJSON_Duplicate(city, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "tech_email", str_or_null(ix, "tech_email"));
      cJSON_AddItemToObject(p, "website", str_or_null(ix, "website"));
      cJSON_AddStringToObject(p, "country", "JP");
      cJSON_AddStringToObject(p, "source", "peeringdb_ix");
      cJSON_AddItemToObject(feat, "properties", p);
      cJSON_AddItemToArray(features, feat);
    }
  }

  if (ixJson)  cJSON_Delete(ixJson);
  if (facJson) cJSON_Delete(facJson);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[internet-exchanges] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def internet_exchanges_def = {
  .id = "internet-exchanges", .collector = "telecom",
  .name = "Internet Exchanges", .name_ja = "インターネットエクスチェンジ",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(internet_exchanges_def)
