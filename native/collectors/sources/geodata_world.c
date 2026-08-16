/* collectors/sources/geodata_world.c
 * OSINT geospatial services — global gazetteer / POI pivots. On-demand entity
 * pivot against the OpenStreetMap ecosystem. All keyless and LIVE (OSM only
 * asks for a descriptive User-Agent).
 *
 *  OVERPASS_GLOBAL   — Overpass API (overpass-api.de/api/interpreter). Runs an
 *                      Overpass QL query for nodes/ways/relations whose name
 *                      (name / name:en / int_name) matches the entity, worldwide,
 *                      returning JSON. One intel_item per named element with a
 *                      resolvable lat/lon (center for ways/relations).
 *  NOMINATIM_GLOBAL  — Nominatim (nominatim.openstreetmap.org/search). Free-text
 *                      geocode of the entity, format=json, one intel_item per
 *                      matched place (display_name, class/type, bbox, lat/lon).
 *  WIKIMAPIA_GLOBAL  — Wikimapia API (api.wikimapia.org). GATED on WIKIMAPIA_KEY
 *                      (their API requires a free key); no key → honest empty.
 *
 * HONESTY: every item is a REAL element parsed from the API response, carrying
 * the coordinates/attributes OSM/Wikimapia actually returned. Fetch failure /
 * no matches → honest empty (return 0). Nothing is synthesized. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Descriptive UA per OSM usage policy (Overpass + Nominatim both require it). */
#define GEO_UA "User-Agent: JapanOSINT/1.0 (OSINT geospatial pivot; +https://github.com/JapanOSINT)"

/* ---- OVERPASS_GLOBAL ---------------------------------------------------- */

/* One Overpass element → one intel_item. Uses element lat/lon, or its
 * "center" for ways/relations. Returns 1 if emitted. */
static int op_emit(intel_sink *sink, const cJSON *el) {
  if (!el) return 0;
  const cJSON *tags = cJSON_GetObjectItem(el, "tags");
  const char *name = tags ? jo_sv(tags, "name") : NULL;
  if (!name) name = tags ? jo_sv(tags, "name:en") : NULL;
  if (!name) name = tags ? jo_sv(tags, "int_name") : NULL;
  if (!name) return 0;                 /* only emit named, real features */

  /* coordinates: node has lat/lon directly; way/relation has "center" */
  double lat = 0, lon = 0; int have = 0;
  const cJSON *jlat = cJSON_GetObjectItem(el, "lat");
  const cJSON *jlon = cJSON_GetObjectItem(el, "lon");
  if (cJSON_IsNumber(jlat) && cJSON_IsNumber(jlon)) {
    lat = jlat->valuedouble; lon = jlon->valuedouble; have = 1;
  } else {
    const cJSON *ctr = cJSON_GetObjectItem(el, "center");
    if (ctr) {
      const cJSON *cla = cJSON_GetObjectItem(ctr, "lat");
      const cJSON *clo = cJSON_GetObjectItem(ctr, "lon");
      if (cJSON_IsNumber(cla) && cJSON_IsNumber(clo)) {
        lat = cla->valuedouble; lon = clo->valuedouble; have = 1;
      }
    }
  }
  if (!have) return 0;                  /* no real coordinates → skip */

  const char *type = jo_sv(el, "type");
  const cJSON *jid = cJSON_GetObjectItem(el, "id");
  long long oid = (jid && cJSON_IsNumber(jid)) ? (long long)jid->valuedouble : 0;

  const char *place  = tags ? jo_sv(tags, "place") : NULL;
  const char *amenity= tags ? jo_sv(tags, "amenity") : NULL;
  const char *country= tags ? jo_sv(tags, "addr:country") : NULL;
  const char *city   = tags ? jo_sv(tags, "addr:city") : NULL;
  const char *website= tags ? jo_sv(tags, "website") : NULL;

  char osm_url[128] = {0};
  if (type && oid)
    snprintf(osm_url, sizeof osm_url, "https://www.openstreetmap.org/%s/%lld", type, oid);

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "name", name);
  if (type) cJSON_AddStringToObject(data, "osm_type", type);
  if (oid)  cJSON_AddNumberToObject(data, "osm_id", (double)oid);
  cJSON_AddNumberToObject(data, "lat", lat);
  cJSON_AddNumberToObject(data, "lon", lon);
  if (place)   cJSON_AddStringToObject(data, "place", place);
  if (amenity) cJSON_AddStringToObject(data, "amenity", amenity);
  if (city)    cJSON_AddStringToObject(data, "city", city);
  if (country) cJSON_AddStringToObject(data, "country", country);
  if (website) cJSON_AddStringToObject(data, "website", website);
  cJSON_AddStringToObject(data, "source", "OpenStreetMap/Overpass");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "OVERPASS_GLOBAL");
  cJSON_AddStringToObject(props, "source", "Overpass");
  if (type) cJSON_AddStringToObject(props, "osm_type", type);
  if (oid)  cJSON_AddNumberToObject(props, "osm_id", (double)oid);
  cJSON_AddNumberToObject(props, "lat", lat);
  cJSON_AddNumberToObject(props, "lon", lon);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char keybuf[64];
  if (type && oid) snprintf(keybuf, sizeof keybuf, "%s/%lld", type, oid);
  else             snprintf(keybuf, sizeof keybuf, "%.60s", name);

  intel_item it = {0};
  it.remote_key      = keybuf;
  it.title           = name;
  it.summary         = place ? place : (amenity ? amenity : city);
  it.body            = bj;
  it.lang            = "en";
  it.link            = osm_url[0] ? osm_url : NULL;
  it.has_geo         = 1;
  it.lat             = lat;
  it.lon             = lon;
  it.record_type     = "overpass-poi";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"OVERPASS_GLOBAL\",\"geospatial\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_overpass(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  /* Build an Overpass QL query: nodes/ways/relations whose name (or name:en /
   * int_name) matches the entity, worldwide; ask for way/relation centers.
   * Escape backslashes and double-quotes for the QL string literal. */
  char esc[512]; size_t ej = 0;
  for (const char *p = q; *p && ej < sizeof esc - 2; p++) {
    if (*p == '"' || *p == '\\') esc[ej++] = '\\';
    esc[ej++] = *p;
  }
  esc[ej] = 0;

  char ql[1600];
  snprintf(ql, sizeof ql,
    "[out:json][timeout:25];"
    "("
      "node[\"name\"~\"%s\",i];"
      "way[\"name\"~\"%s\",i];"
      "relation[\"name\"~\"%s\",i];"
      "node[\"name:en\"~\"%s\",i];"
      "way[\"name:en\"~\"%s\",i];"
    ");"
    "out center 40;",
    esc, esc, esc, esc, esc);

  char *enc = jo_urlencode(ql);
  if (!enc) return 0;
  char url[3200];
  snprintf(url, sizeof url,
    "https://overpass-api.de/api/interpreter?data=%s", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json", GEO_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "overpass_global");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = 0;
  cJSON *els = cJSON_GetObjectItem(root, "elements");
  if (cJSON_IsArray(els)) {
    cJSON *e; cJSON_ArrayForEach(e, els) emitted += op_emit(sink, e);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[overpass_global] emitted %d\n", emitted);
  return 0;
}

/* ---- NOMINATIM_GLOBAL --------------------------------------------------- */

/* One Nominatim result → one intel_item. Returns 1 if emitted. */
static int nm_emit(intel_sink *sink, const cJSON *r) {
  if (!r) return 0;
  const char *disp = jo_sv(r, "display_name");
  const char *slat = jo_sv(r, "lat");
  const char *slon = jo_sv(r, "lon");
  if (!disp || !slat || !slon) return 0;
  double lat = strtod(slat, NULL), lon = strtod(slon, NULL);

  const char *cls   = jo_sv(r, "class");
  const char *typ   = jo_sv(r, "type");
  const char *osmt  = jo_sv(r, "osm_type");
  const cJSON *josmid = cJSON_GetObjectItem(r, "osm_id");
  long long osmid = (josmid && cJSON_IsNumber(josmid)) ? (long long)josmid->valuedouble : 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "display_name", disp);
  cJSON_AddNumberToObject(data, "lat", lat);
  cJSON_AddNumberToObject(data, "lon", lon);
  if (cls)  cJSON_AddStringToObject(data, "class", cls);
  if (typ)  cJSON_AddStringToObject(data, "type", typ);
  if (osmt) cJSON_AddStringToObject(data, "osm_type", osmt);
  if (osmid)cJSON_AddNumberToObject(data, "osm_id", (double)osmid);
  const cJSON *bbox = cJSON_GetObjectItem(r, "boundingbox");
  if (cJSON_IsArray(bbox)) cJSON_AddItemReferenceToObject(data, "boundingbox", (cJSON *)bbox);
  cJSON_AddStringToObject(data, "source", "Nominatim/OSM");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "NOMINATIM_GLOBAL");
  cJSON_AddStringToObject(props, "source", "Nominatim");
  if (cls) cJSON_AddStringToObject(props, "class", cls);
  if (typ) cJSON_AddStringToObject(props, "type", typ);
  cJSON_AddNumberToObject(props, "lat", lat);
  cJSON_AddNumberToObject(props, "lon", lon);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[128] = {0};
  if (osmt && osmid)
    snprintf(link, sizeof link, "https://www.openstreetmap.org/%s/%lld", osmt, osmid);
  char keybuf[80];
  if (osmt && osmid) snprintf(keybuf, sizeof keybuf, "%s/%lld", osmt, osmid);
  else               snprintf(keybuf, sizeof keybuf, "%.60s@%.8s,%.8s", disp, slat, slon);

  intel_item it = {0};
  it.remote_key      = keybuf;
  it.title           = disp;
  it.summary         = typ ? typ : cls;
  it.body            = bj;
  it.lang            = "en";
  it.link            = link[0] ? link : NULL;
  it.has_geo         = 1;
  it.lat             = lat;
  it.lon             = lon;
  it.record_type     = "nominatim-place";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"NOMINATIM_GLOBAL\",\"geospatial\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_nominatim(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://nominatim.openstreetmap.org/search?q=%s&format=json"
    "&addressdetails=1&limit=20", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json", GEO_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "nominatim_global");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = 0;
  if (cJSON_IsArray(root)) {
    cJSON *r; cJSON_ArrayForEach(r, root) emitted += nm_emit(sink, r);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[nominatim_global] emitted %d\n", emitted);
  return 0;
}

/* ---- WIKIMAPIA_GLOBAL (gated) ------------------------------------------- */

/* One Wikimapia place → one intel_item. Returns 1 if emitted. */
static int wm_emit(intel_sink *sink, const cJSON *p) {
  if (!p) return 0;
  const char *title = jo_sv(p, "title");
  const cJSON *jid  = cJSON_GetObjectItem(p, "id");
  if (!title && !jid) return 0;
  long long id = (jid && cJSON_IsNumber(jid)) ? (long long)jid->valuedouble : 0;

  /* location.{lat,lon} */
  double lat = 0, lon = 0; int have = 0;
  const cJSON *loc = cJSON_GetObjectItem(p, "location");
  if (loc) {
    const cJSON *jla = cJSON_GetObjectItem(loc, "lat");
    const cJSON *jlo = cJSON_GetObjectItem(loc, "lon");
    if (cJSON_IsNumber(jla) && cJSON_IsNumber(jlo)) {
      lat = jla->valuedouble; lon = jlo->valuedouble; have = 1;
    }
  }
  const char *url = jo_sv(p, "url");

  cJSON *data = cJSON_CreateObject();
  if (title) cJSON_AddStringToObject(data, "title", title);
  if (id)    cJSON_AddNumberToObject(data, "wikimapia_id", (double)id);
  if (have)  { cJSON_AddNumberToObject(data, "lat", lat);
               cJSON_AddNumberToObject(data, "lon", lon); }
  if (url)   cJSON_AddStringToObject(data, "url", url);
  cJSON_AddStringToObject(data, "source", "Wikimapia");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WIKIMAPIA_GLOBAL");
  cJSON_AddStringToObject(props, "source", "Wikimapia");
  if (id)   cJSON_AddNumberToObject(props, "wikimapia_id", (double)id);
  if (have) { cJSON_AddNumberToObject(props, "lat", lat);
              cJSON_AddNumberToObject(props, "lon", lon); }
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char keybuf[48];
  if (id) snprintf(keybuf, sizeof keybuf, "wikimapia:%lld", id);
  else    snprintf(keybuf, sizeof keybuf, "%.40s", title);

  intel_item it = {0};
  it.remote_key      = keybuf;
  it.title           = title ? title : keybuf;
  it.body            = bj;
  it.lang            = "en";
  it.link            = url;
  it.has_geo         = have;
  it.lat             = lat;
  it.lon             = lon;
  it.record_type     = "wikimapia-place";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WIKIMAPIA_GLOBAL\",\"geospatial\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_wikimapia(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *key = jo_env("WIKIMAPIA_KEY");
  if (!key) {
    fprintf(stderr, "[wikimapia_global] gated (no WIKIMAPIA_KEY)\n");
    return 0;
  }

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.wikimapia.org/?function=place.search&q=%s"
    "&format=json&count=20&key=%s", enc, key);
  free(enc);

  const char *hdrs[] = { "Accept: application/json", GEO_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "wikimapia_global");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = 0;
  cJSON *places = cJSON_GetObjectItem(root, "places");
  if (cJSON_IsArray(places)) {
    cJSON *p; cJSON_ArrayForEach(p, places) emitted += wm_emit(sink, p);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[wikimapia_global] emitted %d\n", emitted);
  return 0;
}

/* ---- dispatch ----------------------------------------------------------- */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *id = ctx->source_id ? ctx->source_id : "";
  if (strcmp(id, "OVERPASS_GLOBAL") == 0)   return run_overpass(ctx, sink);
  if (strcmp(id, "NOMINATIM_GLOBAL") == 0)  return run_nominatim(ctx, sink);
  if (strcmp(id, "WIKIMAPIA_GLOBAL") == 0)  return run_wikimapia(ctx, sink);
  return -1;
}

static const source_def overpass_global_def = {
  .id = "OVERPASS_GLOBAL", .collector = "osint",
  .name = "Overpass Global POI", .name_ja = "Overpass 世界POI検索",
  .update_interval_sec = 0, .run = run,
  .category = "geospatial", .type = "api",
  .url = "https://overpass-api.de/api/interpreter",
  .description = "OpenStreetMap Overpass — worldwide named nodes/ways/relations by name (keyless), with coordinates",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(overpass_global_def)

static const source_def nominatim_global_def = {
  .id = "NOMINATIM_GLOBAL", .collector = "osint",
  .name = "Nominatim Global Geocode", .name_ja = "Nominatim 世界ジオコーディング",
  .update_interval_sec = 0, .run = run,
  .category = "geospatial", .type = "api",
  .url = "https://nominatim.openstreetmap.org/search",
  .description = "OpenStreetMap Nominatim — worldwide free-text geocode of an entity (keyless): place, type, lat/lon, bbox",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(nominatim_global_def)

static const source_def wikimapia_global_def = {
  .id = "WIKIMAPIA_GLOBAL", .collector = "osint",
  .name = "Wikimapia Global Places", .name_ja = "Wikimapia 世界地物検索",
  .update_interval_sec = 0, .run = run,
  .category = "geospatial", .type = "api",
  .url = "https://api.wikimapia.org",
  .description = "Wikimapia crowd-mapped places worldwide by name (needs WIKIMAPIA_KEY): title, coordinates, url",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(wikimapia_global_def)
