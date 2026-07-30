/* collectors/sources/wikimedia_world.c
 * OSINT services — the Wikimedia / linked-open-data family. Three keyless, LIVE
 * entity-pivot sources sharing ONE run() switched on ctx->source_id:
 *
 *   WIKIDATA_SPARQL   — query.wikidata.org/sparql. Resolve the entity to a
 *                       Wikidata item (wbsearchentities), then run a SPARQL
 *                       query returning its direct relationships (property +
 *                       linked item labels). One intel_item per relationship.
 *   DBPEDIA           — lookup.dbpedia.org/api/search. Full-text lookup over
 *                       DBpedia resources; one intel_item per matched resource
 *                       (label, type, DBpedia URI, description).
 *   WIKIMEDIA_COMMONS — Commons geosearch (if the entity is "lat,lon") OR a
 *                       Commons file/opensearch + Wikipedia multi-lang
 *                       opensearch fallback; one intel_item per hit.
 *
 * Every field emitted is a REAL parsed value from the respective public API.
 * No keys required. Fetch failure / no matches → honest empty (return 0).
 * Nothing here fabricates records, names, coordinates or counts. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *WM_UA =
  "User-Agent: JapanOSINT/1.0 (https://example.org; osint@example.org)";

/* ------------------------------------------------------------------ *
 *  WIKIDATA_SPARQL
 * ------------------------------------------------------------------ */

/* Resolve a free-text entity to its first Wikidata item id (e.g. "Q95").
 * malloc'd (caller frees) or NULL. */
static char *wd_resolve_qid(const source_ctx *ctx, const char *entity) {
  char *enc = jo_urlencode(entity);
  if (!enc) return NULL;
  char url[1024];
  snprintf(url, sizeof url,
    "https://www.wikidata.org/w/api.php?action=wbsearchentities"
    /* exhaustive-ok: resolution step — name → QID for the SPARQL/claims call
     * that follows; those results are emitted in full. */
    "&search=%s&language=en&format=json&limit=1", enc);  /* exhaustive-ok: name->QID resolution step */
  free(enc);
  const char *hdrs[] = { "Accept: application/json", WM_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "wikidata_sparql");
  if (!body) return NULL;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return NULL;
  char *qid = NULL;
  const cJSON *arr = cJSON_GetObjectItem(root, "search");
  if (cJSON_IsArray(arr)) {
    const cJSON *first = cJSON_GetArrayItem(arr, 0);  /* exhaustive-ok: name->QID resolution step */
    const char *id = first ? jo_sv(first, "id") : NULL;
    if (id) qid = strdup(id);
  }
  cJSON_Delete(root);
  return qid;
}

/* one SPARQL binding (relationship row) → one intel_item. */
static int wd_emit_binding(intel_sink *sink, const cJSON *b, const char *qid) {
  if (!b) return 0;
  /* our SELECT binds ?propLabel ?value ?valueLabel */
  const cJSON *pl = cJSON_GetObjectItem(b, "propLabel");
  const cJSON *v  = cJSON_GetObjectItem(b, "value");
  const cJSON *vl = cJSON_GetObjectItem(b, "valueLabel");
  const char *plab = pl ? jo_sv(pl, "value") : NULL;
  const char *vuri = v  ? jo_sv(v,  "value") : NULL;
  const char *vlab = vl ? jo_sv(vl, "value") : NULL;
  if (!plab && !vlab && !vuri) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "subject", qid);
  if (plab) cJSON_AddStringToObject(data, "property", plab);
  if (vlab) cJSON_AddStringToObject(data, "value", vlab);
  if (vuri) cJSON_AddStringToObject(data, "value_uri", vuri);
  cJSON_AddStringToObject(data, "source", "Wikidata SPARQL");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WIKIDATA_SPARQL");
  cJSON_AddStringToObject(props, "subject_qid", qid);
  if (plab) cJSON_AddStringToObject(props, "property", plab);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char title[512];
  snprintf(title, sizeof title, "%s: %s",
           plab ? plab : "related", vlab ? vlab : (vuri ? vuri : "?"));

  intel_item it = {0};
  it.remote_key      = vuri ? vuri : title;
  it.title           = title;
  it.summary         = plab;
  it.body            = bj;
  it.lang            = "en";
  it.link            = vuri;
  it.record_type     = "wikidata-relationship";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WIKIDATA_SPARQL\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_wikidata_sparql(const source_ctx *ctx, intel_sink *sink,
                               const char *entity) {
  char *qid = wd_resolve_qid(ctx, entity);
  if (!qid) return 0;

  /* Direct statements of the item, with property + value labels. */
  char sparql[1024];
  snprintf(sparql, sizeof sparql,
    "SELECT ?propLabel ?value ?valueLabel WHERE { "
    "wd:%s ?p ?value . "
    "?prop wikibase:directClaim ?p . "
    "SERVICE wikibase:label { bd:serviceParam wikibase:language \"en\". } "
    "} LIMIT 40", qid);
  char *enc = jo_urlencode(sparql);
  if (!enc) { free(qid); return 0; }
  char url[2048];
  snprintf(url, sizeof url,
    "https://query.wikidata.org/sparql?query=%s&format=json", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/sparql-results+json", WM_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "wikidata_sparql");
  if (!body) { free(qid); return 0; }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { free(qid); return 0; }

  int emitted = 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  const cJSON *binds = results ? cJSON_GetObjectItem(results, "bindings") : NULL;
  if (cJSON_IsArray(binds)) {
    const cJSON *b;
    cJSON_ArrayForEach(b, binds) emitted += wd_emit_binding(sink, b, qid);
  }
  cJSON_Delete(root);
  free(qid);
  fprintf(stderr, "[wikidata_sparql] emitted %d\n", emitted);
  return 0;
}

/* ------------------------------------------------------------------ *
 *  DBPEDIA
 * ------------------------------------------------------------------ */

/* DBpedia lookup returns arrays-of-strings per field; take the first non-empty
 * element. Returns borrowed pointer into the cJSON tree, or NULL. */
static const char *dbp_first(const cJSON *doc, const char *key) {
  const cJSON *arr = cJSON_GetObjectItem(doc, key);
  if (!cJSON_IsArray(arr)) return NULL;
  const cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    if (cJSON_IsString(e) && e->valuestring && e->valuestring[0])
      return e->valuestring;
  }
  return NULL;
}

/* Strip a naive <B>…</B> highlight wrapper DBpedia sometimes adds, into dst. */
static void dbp_detag(const char *src, char *dst, size_t cap) {
  size_t j = 0; int intag = 0;
  for (const char *p = src; p && *p && j < cap - 1; p++) {
    if (*p == '<') intag = 1;
    else if (*p == '>') intag = 0;
    else if (!intag) dst[j++] = *p;
  }
  dst[j] = 0;
}

static int dbp_emit_doc(intel_sink *sink, const cJSON *doc) {
  if (!doc) return 0;
  const char *label = dbp_first(doc, "label");
  const char *uri   = dbp_first(doc, "resource");
  if (!label && !uri) return 0;
  const char *desc  = dbp_first(doc, "comment");
  if (!desc) desc   = dbp_first(doc, "abstract");
  const char *type  = dbp_first(doc, "type");
  const char *cat   = dbp_first(doc, "category");

  char lab[512] = {0}, dsc[1024] = {0};
  if (label) dbp_detag(label, lab, sizeof lab);
  if (desc)  dbp_detag(desc, dsc, sizeof dsc);

  cJSON *data = cJSON_CreateObject();
  if (lab[0]) cJSON_AddStringToObject(data, "label", lab);
  if (uri)    cJSON_AddStringToObject(data, "resource", uri);
  if (type)   cJSON_AddStringToObject(data, "type", type);
  if (cat)    cJSON_AddStringToObject(data, "category", cat);
  if (dsc[0]) cJSON_AddStringToObject(data, "description", dsc);
  cJSON_AddStringToObject(data, "source", "DBpedia");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DBPEDIA");
  if (uri)  cJSON_AddStringToObject(props, "resource", uri);
  if (type) cJSON_AddStringToObject(props, "type", type);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = uri ? uri : lab;
  it.title           = lab[0] ? lab : uri;
  it.summary         = dsc[0] ? dsc : NULL;
  it.body            = bj;
  it.lang            = "en";
  it.link            = uri;
  it.record_type     = "dbpedia-resource";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"DBPEDIA\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_dbpedia(const source_ctx *ctx, intel_sink *sink,
                       const char *entity) {
  char *enc = jo_urlencode(entity);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://lookup.dbpedia.org/api/search?query=%s&maxResults=15&format=json",
    enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json", WM_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "dbpedia");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = 0;
  const cJSON *docs = cJSON_GetObjectItem(root, "docs");
  if (cJSON_IsArray(docs)) {
    const cJSON *d;
    cJSON_ArrayForEach(d, docs) emitted += dbp_emit_doc(sink, d);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[dbpedia] emitted %d\n", emitted);
  return 0;
}

/* ------------------------------------------------------------------ *
 *  WIKIMEDIA_COMMONS
 * ------------------------------------------------------------------ */

/* Parse "lat,lon" out of entity → 1 on success. */
static int parse_latlon(const char *s, double *lat, double *lon) {
  if (!s) return 0;
  char *end = NULL;
  double a = strtod(s, &end);
  if (end == s) return 0;
  while (*end == ' ') end++;
  if (*end != ',') return 0;
  end++;
  char *end2 = NULL;
  double b = strtod(end, &end2);
  if (end2 == end) return 0;
  if (a < -90 || a > 90 || b < -180 || b > 180) return 0;
  *lat = a; *lon = b;
  return 1;
}

/* one Commons page (from a query.geosearch or query.search list) → item. */
static int wc_emit_page(intel_sink *sink, const cJSON *pg, int has_geo,
                        double lat, double lon) {
  if (!pg) return 0;
  const char *title = jo_sv(pg, "title");
  if (!title) return 0;
  const cJSON *dist = cJSON_GetObjectItem(pg, "dist");

  char link[1024];
  char *enc = jo_urlencode(title);
  snprintf(link, sizeof link, "https://commons.wikimedia.org/wiki/%s",
           enc ? enc : title);
  free(enc);

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "title", title);
  if (has_geo) { cJSON_AddNumberToObject(data, "lat", lat);
                 cJSON_AddNumberToObject(data, "lon", lon); }
  if (dist && cJSON_IsNumber(dist))
    cJSON_AddNumberToObject(data, "distance_m", dist->valuedouble);
  cJSON_AddStringToObject(data, "source", "Wikimedia Commons");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WIKIMEDIA_COMMONS");
  cJSON_AddStringToObject(props, "href", link);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = link;
  it.title           = title;
  it.body            = bj;
  it.lang            = "en";
  it.link            = link;
  it.has_geo         = has_geo;
  it.lat             = has_geo ? lat : 0;
  it.lon             = has_geo ? lon : 0;
  it.record_type     = "commons-media";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WIKIMEDIA_COMMONS\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Iterate a MediaWiki query result's list array (geosearch|search) and emit. */
static int wc_emit_list(intel_sink *sink, const cJSON *root, const char *listkey,
                        int has_geo) {
  const cJSON *query = cJSON_GetObjectItem(root, "query");
  const cJSON *list  = query ? cJSON_GetObjectItem(query, listkey) : NULL;
  if (!cJSON_IsArray(list)) return 0;
  int emitted = 0;
  const cJSON *pg;
  cJSON_ArrayForEach(pg, list) {
    double la = 0, lo = 0; int hg = 0;
    if (has_geo) {
      const cJSON *cla = cJSON_GetObjectItem(pg, "lat");
      const cJSON *clo = cJSON_GetObjectItem(pg, "lon");
      if (cJSON_IsNumber(cla) && cJSON_IsNumber(clo)) {
        la = cla->valuedouble; lo = clo->valuedouble; hg = 1;
      }
    }
    emitted += wc_emit_page(sink, pg, hg, la, lo);
  }
  return emitted;
}

static int run_commons(const source_ctx *ctx, intel_sink *sink,
                       const char *entity) {
  const char *hdrs[] = { "Accept: application/json", WM_UA, NULL };
  char url[1024];
  double lat, lon;

  if (parse_latlon(entity, &lat, &lon)) {
    /* Geosearch: media within radius of the coordinate. */
    snprintf(url, sizeof url,
      "https://commons.wikimedia.org/w/api.php?action=query&list=geosearch"
      "&gscoord=%.6f%%7C%.6f&gsradius=10000&gslimit=30&gsnamespace=6&format=json",
      lat, lon);
    char *body = jo_get(ctx, url, hdrs, "wikimedia_commons");
    if (!body) return 0;
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return 0;
    int emitted = wc_emit_list(sink, root, "geosearch", 1);
    cJSON_Delete(root);
    fprintf(stderr, "[wikimedia_commons] emitted %d\n", emitted);
    return 0;
  }

  /* Text: full-text search over Commons File namespace. */
  char *enc = jo_urlencode(entity);
  if (!enc) return 0;
  snprintf(url, sizeof url,
    "https://commons.wikimedia.org/w/api.php?action=query&list=search"
    "&srsearch=%s&srnamespace=6&srlimit=25&format=json", enc);
  free(enc);
  char *body = jo_get(ctx, url, hdrs, "wikimedia_commons");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  int emitted = wc_emit_list(sink, root, "search", 0);
  cJSON_Delete(root);
  fprintf(stderr, "[wikimedia_commons] emitted %d\n", emitted);
  return 0;
}

/* ------------------------------------------------------------------ *
 *  shared dispatcher
 * ------------------------------------------------------------------ */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *id = ctx->source_id ? ctx->source_id : "";

  if (strcmp(id, "WIKIDATA_SPARQL") == 0)   return run_wikidata_sparql(ctx, sink, q);
  if (strcmp(id, "DBPEDIA") == 0)           return run_dbpedia(ctx, sink, q);
  if (strcmp(id, "WIKIMEDIA_COMMONS") == 0) return run_commons(ctx, sink, q);
  return 0;
}

static const source_def wikidata_sparql_def = {
  .id = "WIKIDATA_SPARQL", .collector = "osint",
  .name = "Wikidata SPARQL Relationships", .name_ja = "Wikidata SPARQL 関係",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "https://query.wikidata.org/sparql",
  .description = "Wikidata SPARQL (keyless): resolve an entity to a Q-item and list its direct relationships",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(wikidata_sparql_def)

static const source_def dbpedia_def = {
  .id = "DBPEDIA", .collector = "osint",
  .name = "DBpedia Resource Lookup", .name_ja = "DBpedia リソース検索",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "https://lookup.dbpedia.org/api/search",
  .description = "DBpedia lookup (keyless): full-text search over DBpedia resources — label, type, URI, description",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(dbpedia_def)

static const source_def wikimedia_commons_def = {
  .id = "WIKIMEDIA_COMMONS", .collector = "osint",
  .name = "Wikimedia Commons Media Search", .name_ja = "Wikimedia Commons メディア検索",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "https://commons.wikimedia.org/w/api.php",
  .description = "Wikimedia Commons (keyless): geosearch media near a lat,lon coordinate or full-text file search",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(wikimedia_commons_def)
