/* tests/unit/test_overpass.c — the offline halves of lib/overpass.c: query
 * body encoding, element filtering, coordinate resolution, the per-call dedupe
 * set and emit_points(). The network halves (endpoint fallback, tiled fetch)
 * need a live Overpass and are not exercised here.
 *
 * Behaviours worth a test: an element with no usable coordinates is dropped
 * (never emitted at 0,0), `center` is the fallback for ways/relations, a way
 * needs >= 2 geometry points, an OSM id outside long long range is "not
 * derivable" rather than UB, the dedupe prefilter may report false positives
 * but never a false negative, and two dedupe sets never share state (the
 * scheduler runs ~90 Overpass collectors on a worker pool). */
#include "../../lib/overpass.c"
#include <assert.h>
#include <math.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

typedef struct { int n; char key[64][64]; double lon[64], lat[64]; } cap_t;
static int cap_emit(intel_sink *s, const intel_item *it) {
  cap_t *c = s->ctx;
  if (c->n < 64) { snprintf(c->key[c->n], 64, "%s", it->remote_key ? it->remote_key : ""); c->n++; }
  return 1;
}
static cJSON *mapper(cJSON *el, int idx, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject(); cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray(); cJSON_AddItemToArray(c, cJSON_CreateNumber(lon)); cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c); cJSON_AddItemToObject(f, "geometry", g);
  cJSON *p = cJSON_CreateObject(); char id[32]; snprintf(id, sizeof id, "n%d", idx);
  cJSON_AddStringToObject(p, "id", id);
  const char *nm = ov_tag(el, "name"); cJSON_AddStringToObject(p, "name", nm ? nm : "");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

int main(void) {
  printf("-- uri_encode (encodeURIComponent)\n");
  { char *e;
    e = uri_encode("abcXYZ019-_.!~*'()"); ok(!strcmp(e, "abcXYZ019-_.!~*'()"), "unreserved set kept verbatim"); free(e);
    e = uri_encode("[out:json];node(1,2,3,4);out;"); ok(!strcmp(e, "%5Bout%3Ajson%5D%3Bnode(1%2C2%2C3%2C4)%3Bout%3B"), "an Overpass QL body is percent-encoded exactly"); free(e);
    e = uri_encode("a b&c=d"); ok(!strcmp(e, "a%20b%26c%3Dd"), "space, & and = encoded so the POST form cannot be split"); free(e);
    e = uri_encode("東"); ok(!strcmp(e, "%E6%9D%B1"), "UTF-8 bytes encoded uppercase hex"); free(e);
    e = uri_encode(""); ok(e && !*e, "empty string"); free(e);
    e = uri_encode("\xff\x01"); ok(!strcmp(e, "%FF%01"), "high and control bytes encoded without sign-extension"); free(e); }

  printf("-- ov_tag\n");
  { cJSON *el = cJSON_Parse("{\"tags\":{\"name\":\"X\",\"empty\":\"\",\"n\":5}}");
    ok(!strcmp(ov_tag(el, "name"), "X"), "present tag");
    ok(!ov_tag(el, "empty") && !ov_tag(el, "n") && !ov_tag(el, "missing"), "empty / non-string / missing tag -> NULL");
    ok(!ov_tag(NULL, "x"), "NULL element -> NULL");
    cJSON *bare = cJSON_Parse("{\"id\":1}"); ok(!ov_tag(bare, "name"), "element without tags -> NULL");
    cJSON_Delete(bare); cJSON_Delete(el); }

  printf("-- coordinates\n");
  { double lo = 0, la = 0; cJSON *e;
    e = cJSON_Parse("{\"lat\":35.5,\"lon\":139.5}"); ok(el_coords(e, &lo, &la) && lo == 139.5 && la == 35.5, "node lat/lon"); cJSON_Delete(e);
    e = cJSON_Parse("{\"center\":{\"lat\":34.1,\"lon\":135.2}}"); ok(el_coords(e, &lo, &la) && lo == 135.2 && la == 34.1, "way/relation center fallback"); cJSON_Delete(e);
    e = cJSON_Parse("{\"lat\":1,\"lon\":2,\"center\":{\"lat\":9,\"lon\":9}}"); ok(el_coords(e, &lo, &la) && lo == 2 && la == 1, "own lat/lon outranks center"); cJSON_Delete(e);
    e = cJSON_Parse("{\"lat\":0,\"lon\":0}"); ok(el_coords(e, &lo, &la) && lo == 0 && la == 0, "a genuine 0,0 is accepted (zero is a number, not 'missing')"); cJSON_Delete(e);
    e = cJSON_Parse("{\"lat\":35.5}"); ok(!el_coords(e, &lo, &la), "lat without lon -> no coordinates"); cJSON_Delete(e);
    e = cJSON_Parse("{\"lat\":\"35.5\",\"lon\":\"139.5\"}"); ok(!el_coords(e, &lo, &la), "string coordinates are not accepted"); cJSON_Delete(e);
    e = cJSON_Parse("{\"lat\":null,\"lon\":null,\"center\":{\"lat\":1,\"lon\":2}}"); ok(el_coords(e, &lo, &la) && la == 1, "null lat/lon falls through to center"); cJSON_Delete(e);
    e = cJSON_Parse("{\"center\":{\"lat\":1}}"); ok(!el_coords(e, &lo, &la), "half a center -> none"); cJSON_Delete(e);
    e = cJSON_Parse("{}"); ok(!el_coords(e, &lo, &la), "no coordinates at all -> 0"); cJSON_Delete(e);
    /* TODO(characterisation): range is not validated — lat 999 passes. Asserted so a future
     * clamp/reject is a deliberate, visible change. */
    e = cJSON_Parse("{\"lat\":999,\"lon\":-999}"); ok(el_coords(e, &lo, &la), "[characterisation] out-of-range lat/lon is passed through unvalidated"); cJSON_Delete(e); }

  printf("-- element filters\n");
  { cJSON *doc = cJSON_Parse("{\"elements\":[{\"type\":\"node\",\"id\":1,\"lat\":1,\"lon\":2},"
        "{\"type\":\"way\",\"id\":2,\"center\":{\"lat\":3,\"lon\":4}},{\"type\":\"node\",\"id\":3},{\"type\":\"relation\",\"id\":4}]}");
    cJSON *out = filter_point_elements(doc);
    ok(out && cJSON_GetArraySize(out) == 2, "point filter keeps nodes with lat/lon and ways with center, drops coordinate-less elements");
    cJSON *orig = cJSON_GetObjectItem(doc, "elements");
    ok(cJSON_GetArraySize(orig) == 4, "the source document is left intact (filter duplicates)");
    cJSON_Delete(out); cJSON_Delete(doc);
    ok(filter_point_elements(NULL) == NULL, "NULL doc -> NULL");
    doc = cJSON_Parse("{\"elements\":\"nope\"}"); ok(filter_point_elements(doc) == NULL, "elements not an array -> NULL"); cJSON_Delete(doc);
    doc = cJSON_Parse("{\"remark\":\"runtime error: Query timed out\"}"); ok(filter_point_elements(doc) == NULL, "a timeout remark with no elements -> NULL (a failure, not an empty set)"); cJSON_Delete(doc);
    doc = cJSON_Parse("{\"elements\":[]}"); out = filter_point_elements(doc); ok(out && cJSON_GetArraySize(out) == 0, "empty elements -> empty array (an honest empty)"); cJSON_Delete(out); cJSON_Delete(doc);

    doc = cJSON_Parse("{\"elements\":[{\"type\":\"way\",\"id\":1,\"geometry\":[{\"lat\":1,\"lon\":2},{\"lat\":3,\"lon\":4}]},"
        "{\"type\":\"way\",\"id\":2,\"geometry\":[{\"lat\":1,\"lon\":2}]},{\"type\":\"node\",\"id\":3,\"geometry\":[{},{}]},{\"type\":\"way\",\"id\":4}]}");
    out = filter_way_elements(doc);
    ok(out && cJSON_GetArraySize(out) == 1 && cJSON_GetObjectItem(cJSON_GetArrayItem(out, 0), "id")->valueint == 1,
       "way filter keeps only ways with >= 2 geometry points");
    cJSON_Delete(out); cJSON_Delete(doc);
    ok(filter_way_elements(NULL) == NULL, "way filter: NULL -> NULL"); }

  printf("-- dedupe key\n");
  { char k[64]; cJSON *e;
    e = cJSON_Parse("{\"type\":\"node\",\"id\":123456789012}"); ok(el_key(e, k, sizeof k) && !strcmp(k, "node/123456789012"), "type/id key"); cJSON_Delete(e);
    e = cJSON_Parse("{\"type\":\"way\",\"id\":\"7\"}"); ok(!el_key(e, k, sizeof k), "string id -> not derivable (always kept)"); cJSON_Delete(e);
    e = cJSON_Parse("{\"id\":7}"); ok(!el_key(e, k, sizeof k), "no type -> not derivable"); cJSON_Delete(e);
    e = cJSON_Parse("{\"type\":\"node\",\"id\":1e30}"); ok(!el_key(e, k, sizeof k), "id beyond long long is not derivable (the cast would be UB)"); cJSON_Delete(e);
    e = cJSON_Parse("{\"type\":\"node\",\"id\":-1e30}"); ok(!el_key(e, k, sizeof k), "very negative id likewise"); cJSON_Delete(e);
    e = cJSON_Parse("{\"type\":\"node\",\"id\":9.2e18}"); ok(el_key(e, k, 64) , "id just inside the range is derivable"); cJSON_Delete(e);
    char tiny[6]; e = cJSON_Parse("{\"type\":\"relation\",\"id\":5}"); el_key(e, tiny, sizeof tiny); ok(strlen(tiny) < sizeof tiny, "key buffer is respected"); cJSON_Delete(e); }

  printf("-- seen set\n");
  { seen_set a = {0}, b = {0};
    a.filter = calloc(SEEN_FILTER_BYTES, 1); b.filter = calloc(SEEN_FILTER_BYTES, 1);
    char *ka[3] = { "node/1", "node/2", "way/3" }; a.keys = ka; a.n = 3;
    for (int i = 0; i < 3; i++) seen_mark(&a, ka[i]);
    ok(seen_has(&a, "node/1") && seen_has(&a, "way/3"), "marked keys are found");
    ok(!seen_has(&a, "node/9"), "an unmarked key is not found");
    ok(!seen_has(&b, "node/1"), "a second set shares nothing with the first (no cross-collector contamination)");
    a.n = 2; ok(seen_has(&a, "node/2") && !seen_has(&a, "way/3"), "membership is decided by the exact array, the filter only short-circuits misses");
    seen_set nf = {0}; nf.keys = ka; nf.n = 3; nf.filter = NULL;
    ok(seen_has(&nf, "node/2") && !seen_has(&nf, "x"), "filter disabled (NULL): still correct");
    seen_mark(&nf, "node/1"); ok(1, "mark with no filter is a no-op");
    /* no false negatives under load: mark 5000, every one must be found */
    char **many = malloc(5000 * sizeof *many); seen_set big = {0}; big.filter = calloc(SEEN_FILTER_BYTES, 1); big.keys = many;
    for (int i = 0; i < 5000; i++) { char t[32]; snprintf(t, sizeof t, "node/%d", i); many[i] = strdup(t); big.n++; seen_mark(&big, t); }
    int miss = 0; for (int i = 0; i < 5000; i++) if (!seen_has(&big, many[i])) miss++;
    ok(miss == 0, "5000 marked keys: zero false negatives");
    for (int i = 0; i < 5000; i++) free(many[i]); free(many); free(big.filter); free(a.filter); free(b.filter); }

  printf("-- emit_points\n");
  { cap_t cap; memset(&cap, 0, sizeof cap); intel_sink sink = { &cap, cap_emit };
    source_ctx ctx; memset(&ctx, 0, sizeof ctx); ctx.source_id = "ov-test";
    cJSON *els = cJSON_Parse("[{\"type\":\"node\",\"id\":1,\"lat\":35.1,\"lon\":139.1,\"tags\":{\"name\":\"A\"}},"
        "{\"type\":\"node\",\"id\":2},{\"type\":\"way\",\"id\":3,\"center\":{\"lat\":34,\"lon\":135},\"tags\":{\"name\":\"B\"}}]");
    int n = emit_points(&ctx, &sink, els, mapper, NULL);
    ok(n == 2 && cap.n == 2, "coordinate-less element is skipped, the two located ones are emitted");
    ok(emit_points(&ctx, &sink, NULL, mapper, NULL) == 0, "no elements -> 0");
    cJSON_Delete(els); }

  printf("-- tiles and endpoints\n");
  { int good = 1;
    for (int i = 0; i < 12; i++) {
      double s = TILES[i][0], w = TILES[i][1], n = TILES[i][2], e = TILES[i][3];
      if (!(s < n && w < e && s >= 20 && n <= 50 && w >= 120 && e <= 150)) good = 0;
    }
    ok(good, "all 12 tiles are well-formed [south,west,north,east] boxes inside the Japan region");
    int https = 1; for (int i = 0; i < N_ENDPOINTS; i++) if (strncmp(ENDPOINTS[i], "https://", 8)) https = 0;
    ok(https && N_ENDPOINTS == 4, "all Overpass endpoints are https");
  }

  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
