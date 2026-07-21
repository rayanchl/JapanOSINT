/* collectors/tourism/sources/google_my_maps.c
 * Port of server/src/collectors/googleMyMaps.js.
 * The primary KML tier needs GOOGLE_MYMAPS_IDS; with none set the JS
 * short-circuits to an empty _meta envelope (not ported, HARD RULE 7). The
 * faithful keyless live path the JS runs when KML is unavailable is
 * tryOsmPois() = fetchOverpassTiled(osmPoiOverpassBody, osmPoiMapFeature, …),
 * IDENTICAL body + feature shape as the famousPlaces collector. Reproduced
 * verbatim (see famous_places.c) for featureUid parity. */
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <string.h>

static void body(const char *b, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"tourism\"=\"attraction\"](%s);way[\"tourism\"=\"attraction\"](%s);"
    "node[\"tourism\"=\"museum\"](%s);way[\"tourism\"=\"museum\"](%s);"
    "node[\"tourism\"=\"viewpoint\"](%s);"
    "node[\"tourism\"=\"artwork\"](%s);"
    "node[\"tourism\"=\"theme_park\"](%s);way[\"tourism\"=\"theme_park\"](%s);"
    "node[\"tourism\"=\"zoo\"](%s);way[\"tourism\"=\"zoo\"](%s);"
    "node[\"tourism\"=\"aquarium\"](%s);way[\"tourism\"=\"aquarium\"](%s);"
    "node[\"tourism\"=\"gallery\"](%s);"
    "node[\"historic\"](%s);way[\"historic\"](%s);"
    "node[\"amenity\"=\"place_of_worship\"](%s);way[\"amenity\"=\"place_of_worship\"](%s);"
    "node[\"amenity\"=\"theatre\"](%s);"
    "node[\"amenity\"=\"arts_centre\"](%s);"
    "node[\"leisure\"=\"park\"](%s);way[\"leisure\"=\"park\"](%s);"
    "node[\"leisure\"=\"garden\"](%s);way[\"leisure\"=\"garden\"](%s);"
    "node[\"leisure\"=\"nature_reserve\"](%s);way[\"leisure\"=\"nature_reserve\"](%s);"
    "node[\"natural\"=\"peak\"](%s);"
    "node[\"natural\"=\"volcano\"](%s);"
    "node[\"natural\"=\"waterfall\"](%s);",
    b, b, b, b, b, b, b, b, b, b, b, b, b, b, b, b, b, b, b,
    b, b, b, b, b, b, b, b, b);
}

static const char *cat_of(cJSON *el, char *buf, size_t n) {
  const char *tour = ov_tag(el, "tourism");
  if (tour) return tour;
  const char *hist = ov_tag(el, "historic");
  if (hist) return hist;
  const char *am = ov_tag(el, "amenity");
  if (am && strcmp(am, "place_of_worship") == 0) {
    const char *rel = ov_tag(el, "religion");
    if (rel) { snprintf(buf, n, "place_of_worship:%s", rel); return buf; }
    return "place_of_worship";
  }
  if (am) return am;
  const char *leis = ov_tag(el, "leisure");
  if (leis) return leis;
  const char *nat = ov_tag(el, "natural");
  if (nat) return nat;
  return "place";
}

static void wikipedia_url(const char *tag, char *buf, size_t n) {
  if (!tag) { buf[0] = 0; return; }
  const char *colon = strchr(tag, ':');
  if (!colon) {
    snprintf(buf, n, "https://en.wikipedia.org/wiki/");
    size_t l = strlen(buf);
    for (const char *p = tag; *p && l + 1 < n; p++)
      buf[l++] = (*p == ' ') ? '_' : *p;
    buf[l] = 0;
    return;
  }
  int lang_len = (int)(colon - tag);
  snprintf(buf, n, "https://%.*s.wikipedia.org/wiki/", lang_len, tag);
  size_t l = strlen(buf);
  for (const char *p = colon + 1; *p && l + 1 < n; p++)
    buf[l++] = (*p == ' ') ? '_' : *p;
  buf[l] = 0;
}

static void commons_url(const char *tag, char *buf, size_t n) {
  if (!tag) { buf[0] = 0; return; }
  char tmp[512];
  if (strncmp(tag, "Category:", 9) == 0 || strncmp(tag, "File:", 5) == 0)
    snprintf(tmp, sizeof tmp, "%s", tag);
  else
    snprintf(tmp, sizeof tmp, "File:%s", tag);
  snprintf(buf, n, "https://commons.wikimedia.org/wiki/");
  size_t l = strlen(buf);
  for (const char *p = tmp; *p && l + 1 < n; p++)
    buf[l++] = (*p == ' ') ? '_' : *p;
  buf[l] = 0;
}

static void add_or_null(cJSON *p, const char *key, cJSON *el,
                        const char *a, const char *b) {
  const char *v = ov_tag(el, a);
  if (!v && b) v = ov_tag(el, b);
  cJSON_AddItemToObject(p, key,
                        v ? cJSON_CreateString(v) : cJSON_CreateNull());
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();
  cJSON *etype = cJSON_GetObjectItem(el, "type");
  cJSON *eid = cJSON_GetObjectItem(el, "id");
  const char *ets = (etype && cJSON_IsString(etype)) ? etype->valuestring : "";
  long long eidv = (eid && cJSON_IsNumber(eid)) ? (long long)eid->valuedouble : 0;
  char idbuf[96];
  snprintf(idbuf, sizeof idbuf, "OSM_%s_%lld", ets, eidv);
  cJSON_AddStringToObject(p, "id", idbuf);
  cJSON_AddStringToObject(p, "platform", "osm_popular_place");
  cJSON_AddStringToObject(p, "osm_type", ets);
  cJSON_AddItemToObject(p, "osm_id", cJSON_CreateNumber((double)eidv));

  const char *nen = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  if (nen)      cJSON_AddStringToObject(p, "name", nen);
  else if (nm)  cJSON_AddStringToObject(p, "name", nm);
  else {
    char nb[32]; snprintf(nb, sizeof nb, "Place %d", i + 1);
    cJSON_AddStringToObject(p, "name", nb);
  }
  cJSON_AddItemToObject(p, "name_ja",
                        nm ? cJSON_CreateString(nm) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "name_en",
                        nen ? cJSON_CreateString(nen) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "name_local",
                        nm ? cJSON_CreateString(nm) : cJSON_CreateNull());
  add_or_null(p, "alt_name", el, "alt_name", NULL);
  add_or_null(p, "official_name", el, "official_name", NULL);

  char catbuf[256];
  cJSON_AddStringToObject(p, "category", cat_of(el, catbuf, sizeof catbuf));
  add_or_null(p, "tourism", el, "tourism", NULL);
  add_or_null(p, "historic", el, "historic", NULL);
  add_or_null(p, "amenity", el, "amenity", NULL);
  add_or_null(p, "leisure", el, "leisure", NULL);
  add_or_null(p, "natural", el, "natural", NULL);
  add_or_null(p, "religion", el, "religion", NULL);
  add_or_null(p, "denomination", el, "denomination", NULL);

  const char *wd = ov_tag(el, "wikidata");
  cJSON_AddItemToObject(p, "wikidata",
                        wd ? cJSON_CreateString(wd) : cJSON_CreateNull());
  if (wd) {
    char wb[160]; snprintf(wb, sizeof wb, "https://www.wikidata.org/wiki/%s", wd);
    cJSON_AddStringToObject(p, "wikidata_url", wb);
  } else cJSON_AddItemToObject(p, "wikidata_url", cJSON_CreateNull());
  const char *wp = ov_tag(el, "wikipedia");
  cJSON_AddItemToObject(p, "wikipedia",
                        wp ? cJSON_CreateString(wp) : cJSON_CreateNull());
  char wpu[600]; wikipedia_url(wp, wpu, sizeof wpu);
  cJSON_AddItemToObject(p, "wikipedia_url",
                        wpu[0] ? cJSON_CreateString(wpu) : cJSON_CreateNull());
  const char *wc = ov_tag(el, "wikimedia_commons");
  cJSON_AddItemToObject(p, "wikimedia_commons",
                        wc ? cJSON_CreateString(wc) : cJSON_CreateNull());
  char wcu[600]; commons_url(wc, wcu, sizeof wcu);
  cJSON_AddItemToObject(p, "wikimedia_commons_url",
                        wcu[0] ? cJSON_CreateString(wcu) : cJSON_CreateNull());
  add_or_null(p, "website", el, "website", "contact:website");
  add_or_null(p, "url", el, "url", NULL);
  add_or_null(p, "image", el, "image", NULL);

  add_or_null(p, "phone", el, "phone", "contact:phone");
  add_or_null(p, "email", el, "email", "contact:email");

  add_or_null(p, "opening_hours", el, "opening_hours", NULL);
  add_or_null(p, "fee", el, "fee", NULL);
  add_or_null(p, "charge", el, "charge", NULL);
  add_or_null(p, "admission", el, "admission", NULL);
  add_or_null(p, "operator", el, "operator", NULL);
  add_or_null(p, "start_date", el, "start_date", NULL);
  add_or_null(p, "heritage", el, "heritage", NULL);
  add_or_null(p, "heritage_operator", el, "heritage:operator", NULL);
  add_or_null(p, "unesco", el, "unesco", NULL);

  add_or_null(p, "ele", el, "ele", NULL);
  add_or_null(p, "addr_full", el, "addr:full", NULL);
  add_or_null(p, "addr_city", el, "addr:city", NULL);
  add_or_null(p, "addr_prefecture", el, "addr:province", "addr:state");
  add_or_null(p, "addr_postcode", el, "addr:postcode", NULL);

  add_or_null(p, "description", el, "description", "description:en");

  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 180, 90000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def google_my_maps_def = {
  .id = "google-my-maps", .collector = "tourism",
  .name = "Google My Maps", .name_ja = "グーグル マイマップ",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(google_my_maps_def)
