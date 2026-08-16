/* collectors/marketplace/sources/tabelog_restaurants.c
 * Port of server/src/collectors/tabelogRestaurants.js.
 * Primary: HotPepper Gourmet API (gated on HOTPEPPER_API_KEY) →
 * fallback: OSM Overpass restaurants/fast_food. SEED_RESTAURANTS not
 * ported (rule 7). */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* JS String(x) for a cJSON scalar into buf; "" if absent. */
static const char *jstr(cJSON *o, const char *k, char *buf, size_t n) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) { buf[0] = '\0'; return buf; }
  if (cJSON_IsString(v) && v->valuestring) return v->valuestring;
  if (cJSON_IsNumber(v)) {
    if (v->valuedouble == (double)(long long)v->valuedouble)
      snprintf(buf, n, "%lld", (long long)v->valuedouble);
    else snprintf(buf, n, "%g", v->valuedouble);
    return buf;
  }
  buf[0] = '\0';
  return buf;
}

/* nested-name: o[k]?.name as string, or NULL. */
static const char *nested_name(cJSON *o, const char *k) {
  cJSON *sub = cJSON_GetObjectItem(o, k);
  if (!sub) return NULL;
  cJSON *nm = cJSON_GetObjectItem(sub, "name");
  return (nm && cJSON_IsString(nm) && nm->valuestring && nm->valuestring[0])
           ? nm->valuestring : NULL;
}

static cJSON *osm_map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char sid[48];
  snprintf(sid, sizeof sid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "facility_id", sid);

  const char *name = ov_tag(el, "name");
  if (name) {
    cJSON_AddStringToObject(p, "name", name);
  } else {
    char nb[32];
    snprintf(nb, sizeof nb, "Restaurant %d", i + 1);
    cJSON_AddStringToObject(p, "name", nb);
  }
  const char *ne = ov_tag(el, "name:en");
  cJSON_AddItemToObject(p, "name_en",
                        ne ? cJSON_CreateString(ne) : cJSON_CreateNull());
  const char *cuisine = ov_tag(el, "cuisine");
  cJSON_AddStringToObject(p, "genre", cuisine ? cuisine : "unknown");
  const char *amenity = ov_tag(el, "amenity");
  cJSON_AddStringToObject(p, "amenity", amenity ? amenity : "");
  const char *addr = ov_tag(el, "addr:full");
  if (!addr) addr = ov_tag(el, "addr:city");
  cJSON_AddItemToObject(p, "address",
                        addr ? cJSON_CreateString(addr) : cJSON_CreateNull());
  const char *phone = ov_tag(el, "phone");
  cJSON_AddItemToObject(p, "phone",
                        phone ? cJSON_CreateString(phone) : cJSON_CreateNull());
  const char *web = ov_tag(el, "website");
  cJSON_AddItemToObject(p, "website",
                        web ? cJSON_CreateString(web) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("HOTPEPPER_API_KEY");
  if (key && *key) {
    char url[320];
    snprintf(url, sizeof url,
      "https://webservice.recruit.co.jp/hotpepper/gourmet/v1/"
      "?key=%s&format=json&count=100&large_area=Z011", key);
    cJSON *data = feed_get_json(ctx->http, url, 10000);
    cJSON *results = data ? cJSON_GetObjectItem(data, "results") : NULL;
    cJSON *shops = results ? cJSON_GetObjectItem(results, "shop") : NULL;
    if (cJSON_IsArray(shops) && cJSON_GetArraySize(shops) > 0) {
      cJSON *features = cJSON_CreateArray();
      cJSON *s;
      cJSON_ArrayForEach(s, shops) {
        char latb[32], lngb[32];
        const char *lats = jstr(s, "lat", latb, sizeof latb);
        const char *lngs = jstr(s, "lng", lngb, sizeof lngb);
        cJSON *f = gj_point_feature(lngs[0] ? strtod(lngs, NULL) : 0, lats[0] ? strtod(lats, NULL) : 0);

        cJSON *p = cJSON_CreateObject();
        cJSON *idv = cJSON_GetObjectItem(s, "id");
        char fid[80];
        if (idv && cJSON_IsString(idv) && idv->valuestring &&
            idv->valuestring[0]) {
          snprintf(fid, sizeof fid, "RESTO_HP_%s", idv->valuestring);
        } else {
          cJSON *nmv = cJSON_GetObjectItem(s, "name");
          const char *nm = (nmv && cJSON_IsString(nmv) && nmv->valuestring)
                             ? nmv->valuestring : "";
          const char *parts[3] = { nm, lats, lngs };
          char hk[24];
          feed_hash_key(hk, parts, 3);
          snprintf(fid, sizeof fid, "RESTO_%s", hk);
        }
        cJSON_AddStringToObject(p, "facility_id", fid);
        cJSON *nmv = cJSON_GetObjectItem(s, "name");
        cJSON_AddStringToObject(p, "name",
          (nmv && cJSON_IsString(nmv) && nmv->valuestring && nmv->valuestring[0])
            ? nmv->valuestring : "Restaurant");
        const char *genre = nested_name(s, "genre");
        cJSON_AddItemToObject(p, "genre",
          genre ? cJSON_CreateString(genre) : cJSON_CreateNull());
        const char *budget = nested_name(s, "budget");
        cJSON_AddItemToObject(p, "budget",
          budget ? cJSON_CreateString(budget) : cJSON_CreateNull());
        cJSON *acc = cJSON_GetObjectItem(s, "access");
        cJSON_AddItemToObject(p, "access",
          (acc && cJSON_IsString(acc) && acc->valuestring && acc->valuestring[0])
            ? cJSON_CreateString(acc->valuestring) : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "country", "JP");
        cJSON_AddStringToObject(p, "source", "hotpepper_api");
        cJSON_AddItemToObject(f, "properties", p);
        cJSON_AddItemToArray(features, f);
      }
      int n = geojson_emit_features(sink, ctx->source_id, features);
      cJSON_Delete(features);
      cJSON_Delete(data);
      fprintf(stderr, "[tabelog-restaurants] hotpepper emitted %d\n", n);
      return n >= 0 ? 0 : -1;
    }
    if (data) cJSON_Delete(data);
  }

  /* fallback: OSM Overpass */
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"restaurant\"][\"name\"](area.jp);"
    "node[\"amenity\"=\"fast_food\"][\"name\"](area.jp);",
    180, 60000, osm_map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def tabelog_restaurants_def = {
  .id = "tabelog-restaurants", .collector = "marketplace",
  .name = "Tabelog / HotPepper Restaurants",
  .name_ja = "食べログ・ホットペッパー グルメ",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(tabelog_restaurants_def)
