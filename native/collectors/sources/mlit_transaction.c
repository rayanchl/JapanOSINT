/* collectors/economy/sources/mlit_transaction.c
 * Port of server/src/collectors/mlitTransaction.js (tryMlit live path).
 * Single MLIT webland TradeListSearch call (Tokyo Q4 2023, area=13),
 * slice(0,200), pinned at Tokyo. SEED branch dropped (rule 8). */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://www.land.mlit.go.jp/webland/api/TradeListSearch?from=20234&to=20234&area=13"

/* The query is hardwired to area=13 (Tokyo-to), so every transaction really is
 * somewhere in Tokyo prefecture — but MLIT anonymises transaction locations and
 * publishes no coordinate, so this point is the prefecture, not the property.
 * It is emitted with geo_precision:"prefecture" rather than bare, which is what
 * made it read as an exact address on the map. */
#define PIN_LON 139.6917
#define PIN_LAT 35.6896

/* JS: parseInt(s) || null  → leading-int parse, falsy(0/NaN) → null */
static cJSON *parse_int_or_null(cJSON *v) {
  if (!v) return cJSON_CreateNull();
  long x = 0; int ok = 0;
  if (cJSON_IsNumber(v)) { x = (long)cJSON_GetNumberValue(v); ok = 1; }
  else if (cJSON_IsString(v) && v->valuestring) {
    char *end; x = strtol(v->valuestring, &end, 10);
    ok = (end != v->valuestring);
  }
  if (!ok || x == 0) return cJSON_CreateNull();
  return cJSON_CreateNumber((double)x);
}

/* JS: tx.X || null  → null for missing/empty-string/0/false, else Duplicate */
static cJSON *str_or_null(cJSON *v) {
  if (!v || cJSON_IsNull(v)) return cJSON_CreateNull();
  if (cJSON_IsString(v) && !v->valuestring[0]) return cJSON_CreateNull();
  if (cJSON_IsBool(v) && !cJSON_IsTrue(v)) return cJSON_CreateNull();
  if (cJSON_IsNumber(v) && cJSON_GetNumberValue(v) == 0) return cJSON_CreateNull();
  return cJSON_Duplicate(v, 1);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *data = feed_get_json(ctx->http, API_URL, 10000);
  if (!data) return -1;

  cJSON *items = cJSON_GetObjectItem(data, "data");
  if (!items || !cJSON_IsArray(items) || cJSON_GetArraySize(items) == 0) {
    cJSON_Delete(data);
    return -1;                                  /* JS: return null → not live */
  }

  cJSON *features = cJSON_CreateArray();
  int i = 0;
  cJSON *tx;
  cJSON_ArrayForEach(tx, items) {
    if (i++ >= 200) break;                       /* slice(0,200) */
    cJSON *typ = cJSON_GetObjectItem(tx, "Type");
    cJSON *mun = cJSON_GetObjectItem(tx, "Municipality");
    cJSON *dis = cJSON_GetObjectItem(tx, "DistrictName");
    cJSON *tp  = cJSON_GetObjectItem(tx, "TradePrice");
    cJSON *ar  = cJSON_GetObjectItem(tx, "Area");
    cJSON *per = cJSON_GetObjectItem(tx, "Period");
    cJSON *pur = cJSON_GetObjectItem(tx, "Purpose");
    cJSON *up  = cJSON_GetObjectItem(tx, "UnitPrice");

    const char *parts[6] = { jo_vstr(typ), jo_vstr(mun), jo_vstr(dis), jo_vstr(tp), jo_vstr(ar), jo_vstr(per) };
    char hk[21]; feed_hash_key(hk, parts, 6);
    char tid[40]; snprintf(tid, sizeof tid, "TX_%s", hk);

    cJSON *f = gj_point_feature(PIN_LON, PIN_LAT);

    cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
    cJSON_AddStringToObject(p, "tx_id", tid);
    cJSON_AddItemToObject(p, "type", str_or_null(typ));
    cJSON_AddItemToObject(p, "municipality", str_or_null(mun));
    cJSON_AddItemToObject(p, "district", str_or_null(dis));
    cJSON_AddItemToObject(p, "price", parse_int_or_null(tp));
    cJSON_AddItemToObject(p, "area_m2", parse_int_or_null(ar));
    /* price_per_m2 = tx.UnitPrice ? parseInt(tx.UnitPrice) : null */
    int up_truthy = 0;
    if (up) {
      if (cJSON_IsNumber(up)) up_truthy = (cJSON_GetNumberValue(up) != 0);
      else if (cJSON_IsString(up)) up_truthy = (up->valuestring[0] != 0);
      else if (cJSON_IsBool(up)) up_truthy = cJSON_IsTrue(up);
      else if (cJSON_IsObject(up) || cJSON_IsArray(up)) up_truthy = 1;
    }
    if (up_truthy) {
      long u = cJSON_IsNumber(up) ? (long)cJSON_GetNumberValue(up)
                                  : strtol(up->valuestring, NULL, 10);
      cJSON_AddItemToObject(p, "price_per_m2", cJSON_CreateNumber((double)u));
    } else {
      cJSON_AddItemToObject(p, "price_per_m2", cJSON_CreateNull());
    }
    cJSON_AddItemToObject(p, "purpose", str_or_null(pur));
    cJSON_AddStringToObject(p, "country", "JP");
    cJSON_AddStringToObject(p, "source", "mlit_api");
    /* unified provenance vocabulary — never "exact": MLIT anonymises the
     * location of a transaction by design. */
    cJSON_AddStringToObject(p, "geo_provenance", "query-prefecture-tokyo");
    cJSON_AddStringToObject(p, "geo_precision", "prefecture");
    cJSON_AddBoolToObject(p, "geo_uncertain", 1);
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[mlit-transaction] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def mlit_transaction_def = {
  .id = "mlit-transaction", .collector = "economy",
  .name = "MLIT Land Transactions", .name_ja = "国交省 不動産取引価格",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(mlit_transaction_def)
