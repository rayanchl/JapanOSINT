/* test_unified.c — lib/unified.c, the fusion of already-registered upstreams.
 *
 *   - unified_norm_name: lower-case, station words and separators stripped,
 *     NULL and bounded buffers safe;
 *   - a missing upstream and an upstream whose run() fails contribute nothing
 *     and do not stop the others (Promise.allSettled), a failing upstream's
 *     partial emissions are NOT merged;
 *   - `kind` is tagged only where the feature has none;
 *   - dedupe: the first key function that applies wins the slot, later
 *     duplicates fill only NULL/missing properties of the kept feature (never
 *     overwrite), `sources` lists each distinct source once, id-less features
 *     fall back to a rounded-coordinate + normalized-name key at the requested
 *     precision, and features with neither a key nor finite coordinates are
 *     dropped rather than merged into one another;
 *   - no key functions: a raw merge keeps every feature;
 *   - filter runs before dedupe and post after; features without geometry are
 *     never merged (the mergeFC guard);
 *   - unified_capture appends an upstream's features and returns the count; a
 *     missing source, a failed run and a non-array target append nothing. */
#include "../../lib/unified.c"

#include <assert.h>

/* ── fake upstreams ──────────────────────────────────────────────────────── */
static const char *g_json_a, *g_json_b, *g_json_c;
static int g_rc_c = 0;

static int emit_json(intel_sink *s, const char *id, const char *json) {
  cJSON *arr = cJSON_Parse(json);
  assert(arr);
  int n = geojson_emit_features(s, id, arr);
  cJSON_Delete(arr);
  return n;
}
static int run_a(const source_ctx *c, intel_sink *s) { (void)c; emit_json(s, "UNI_TEST_A", g_json_a); return 0; }
static int run_b(const source_ctx *c, intel_sink *s) { (void)c; emit_json(s, "UNI_TEST_B", g_json_b); return 0; }
static int run_c(const source_ctx *c, intel_sink *s) {          /* emits, then fails */
  (void)c; emit_json(s, "UNI_TEST_C", g_json_c); return g_rc_c; }
static const source_def d_a = { .id = "UNI_TEST_A", .collector = "osint", .name = "a",
  .run = run_a, .category = "test" };
static const source_def d_b = { .id = "UNI_TEST_B", .collector = "osint", .name = "b",
  .run = run_b, .category = "test" };
static const source_def d_c = { .id = "UNI_TEST_C", .collector = "osint", .name = "c",
  .run = run_c, .category = "test" };
REGISTER_SOURCE(d_a)
REGISTER_SOURCE(d_b)
REGISTER_SOURCE(d_c)

/* ── the sink that records what unified_collect emitted ─────────────────── */
typedef struct { cJSON *props[64]; char geom[64][200]; int n; } rec;
static int rec_emit(struct intel_sink *s, const intel_item *it) {
  rec *r = s->ctx;
  if (r->n < 64) {
    r->props[r->n] = cJSON_Parse(it->properties_json ? it->properties_json : "{}");
    snprintf(r->geom[r->n], sizeof r->geom[0], "%s", it->geometry_geojson ? it->geometry_geojson : "");
    r->n++;
  }
  return 1;
}
static void rec_free(rec *r) { for (int i = 0; i < r->n; i++) cJSON_Delete(r->props[i]); r->n = 0; }
static cJSON *by_name(rec *r, const char *name) {
  for (int i = 0; i < r->n; i++) {
    cJSON *nm = cJSON_GetObjectItem(r->props[i], "name");
    if (nm && cJSON_IsString(nm) && !strcmp(nm->valuestring, name)) return r->props[i];
  }
  return NULL;
}

/* ── key / filter / post functions ───────────────────────────────────────── */
static const char *key_station(cJSON *f, char *buf, size_t n) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *v = p ? cJSON_GetObjectItem(p, "station_id") : NULL;
  if (!v || !cJSON_IsString(v) || !v->valuestring[0]) return NULL;
  snprintf(buf, n, "sid:%s", v->valuestring);
  return buf;
}
static const char *key_code(cJSON *f, char *buf, size_t n) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *v = p ? cJSON_GetObjectItem(p, "code") : NULL;
  if (!v || !cJSON_IsString(v) || !v->valuestring[0]) return NULL;
  snprintf(buf, n, "code:%s", v->valuestring);
  return buf;
}
static int filt_no_closed(cJSON *f) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *c = p ? cJSON_GetObjectItem(p, "closed") : NULL;
  return !(c && cJSON_IsTrue(c));
}
static void post_mark(cJSON *f) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON_AddBoolToObject(p, "post_ran", 1);
}

static char *F(const char *props, double lon, double lat) {
  static char buf[8][400]; static int k;
  char *b = buf[k++ & 7];
  snprintf(b, 400, "{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[%.6f,%.6f]},"
                   "\"properties\":%s}", lon, lat, props);
  return b;
}
static char *arr(const char *a, const char *b, const char *c) {
  static char out[4][1400]; static int k;
  char *o = out[k++ & 3];
  snprintf(o, 1400, "[%s%s%s%s%s]", a ? a : "", (a && b) ? "," : "", b ? b : "",
           ((a || b) && c) ? "," : "", c ? c : "");
  return o;
}

int main(void) {
  /* ── normName ────────────────────────────────────────────────────────── */
  char nm[64];
  unified_norm_name("Tokyo Station", nm, sizeof nm);        assert(!strcmp(nm, "tokyo"));
  unified_norm_name("東京駅", nm, sizeof nm);                assert(!strcmp(nm, "東京"));
  unified_norm_name("Shin-Osaka (Stop)", nm, sizeof nm);    assert(!strcmp(nm, "shinosaka"));
  unified_norm_name("  A . b,C  ", nm, sizeof nm);           assert(!strcmp(nm, "abc"));
  unified_norm_name("市役所前バス停", nm, sizeof nm);         assert(!strcmp(nm, "市役所前"));
  unified_norm_name(NULL, nm, sizeof nm);                   assert(nm[0] == 0);
  unified_norm_name("", nm, sizeof nm);                     assert(nm[0] == 0);
  { char tiny[8]; unified_norm_name("abcdefghijklmnop", tiny, sizeof tiny);
    assert(strlen(tiny) < sizeof tiny && "output is bounded"); }
  printf("  normName: case, station words, separators, NULL, bounds: ok\n");

  rec r = {0};
  intel_sink sink = { &r, rec_emit };
  source_ctx ctx = {0};
  unified_upstream ups[] = { {"UNI_TEST_A", "rail"}, {"UNI_TEST_B", "bus"},
                             {"UNI_TEST_C", NULL}, {"UNI_NO_SUCH", "x"} };
  unified_keyfn keys[] = { key_station, key_code };

  /* ── allSettled, kind tagging, dedupe merge ──────────────────────────── */
  g_json_a = arr(F("{\"name\":\"Alpha\",\"station_id\":\"S1\",\"source\":\"a\",\"line\":null}", 139.0, 35.0),
                 F("{\"name\":\"Beta\",\"station_id\":\"S2\",\"source\":\"a\",\"kind\":\"metro\"}", 139.5, 35.5), NULL);
  g_json_b = arr(F("{\"name\":\"Alpha B\",\"station_id\":\"S1\",\"source\":\"b\",\"line\":\"Yamanote\",\"extra\":7}", 139.0, 35.0),
                 F("{\"name\":\"Gamma\",\"code\":\"G\",\"source\":\"b\"}", 140.0, 36.0), NULL);
  g_json_c = arr(F("{\"name\":\"Failing\",\"station_id\":\"S9\",\"source\":\"c\"}", 141.0, 37.0), NULL, NULL);
  g_rc_c = -1;
  int n = unified_collect(&ctx, &sink, "unified-test", ups, 4, keys, 2, 4, NULL, NULL);
  assert(n == 3 && r.n == 3 && "S1 merged, S2, Gamma; failing + missing upstreams add nothing");
  assert(!by_name(&r, "Failing") && "a failed upstream's partial emissions are not merged");
  cJSON *s1 = by_name(&r, "Alpha");
  assert(s1 && "the first sighting is the one kept");
  assert(!strcmp(cJSON_GetObjectItem(s1, "line")->valuestring, "Yamanote") &&
         "a later duplicate fills a NULL property");
  assert(cJSON_GetObjectItem(s1, "extra")->valueint == 7 && "and adds a missing one");
  assert(!strcmp(cJSON_GetObjectItem(s1, "kind")->valuestring, "rail") && "kind tagged where unset");
  cJSON *srcs = cJSON_GetObjectItem(s1, "sources");
  assert(cJSON_GetArraySize(srcs) == 2 && !strcmp(cJSON_GetArrayItem(srcs, 0)->valuestring, "a") &&
         !strcmp(cJSON_GetArrayItem(srcs, 1)->valuestring, "b"));
  assert(!strcmp(cJSON_GetObjectItem(by_name(&r, "Beta"), "kind")->valuestring, "metro") &&
         "an existing kind is never overwritten");
  assert(!strcmp(cJSON_GetObjectItem(by_name(&r, "Gamma"), "kind")->valuestring, "bus"));
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(by_name(&r, "Gamma"), "sources")) == 1);
  rec_free(&r);
  printf("  allSettled, kind tagging, fill-only merge, sources list: ok\n");

  /* ── an upstream that succeeds with the same data as another ─────────── */
  g_rc_c = 0;
  g_json_c = arr(F("{\"name\":\"Alpha C\",\"station_id\":\"S1\",\"source\":\"b\",\"line\":\"Other\"}", 139.0, 35.0), NULL, NULL);
  n = unified_collect(&ctx, &sink, "unified-test", ups, 3, keys, 2, 4, NULL, NULL);
  s1 = by_name(&r, "Alpha");
  assert(s1 && !strcmp(cJSON_GetObjectItem(s1, "line")->valuestring, "Yamanote") &&
         "a filled value is never overwritten by a third source");
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(s1, "sources")) == 2 && "'b' twice is listed once");
  rec_free(&r);

  /* ── coordinate fallback key, precision, name normalization ──────────── */
  g_json_a = arr(F("{\"name\":\"Foo Station\",\"source\":\"a\"}", 139.70001, 35.69001), NULL, NULL);
  g_json_b = arr(F("{\"name\":\"foo\",\"source\":\"b\"}", 139.70004, 35.69004),            /* same at 4 dp */
                 F("{\"name\":\"foo\",\"source\":\"b\",\"far\":1}", 139.7100, 35.69001), NULL); /* 0.01 off */
  unified_upstream two[] = { {"UNI_TEST_A", NULL}, {"UNI_TEST_B", NULL} };
  n = unified_collect(&ctx, &sink, "unified-test", two, 2, keys, 2, 4, NULL, NULL);
  assert(n == 2 && "same rounded cell + same normalized name merge; a different cell does not");
  rec_free(&r);
  n = unified_collect(&ctx, &sink, "unified-test", two, 2, keys, 2, 1, NULL, NULL);
  assert(n == 1 && "at 1 dp all three share a cell");
  rec_free(&r);
  n = unified_collect(&ctx, &sink, "unified-test", two, 2, keys, 2, 0, NULL, NULL);
  assert(n == 2 && "precision 0 means the default 4");
  rec_free(&r);
  g_json_b = arr(F("{\"name\":\"Other\",\"source\":\"b\"}", 139.70001, 35.69001), NULL, NULL);
  n = unified_collect(&ctx, &sink, "unified-test", two, 2, keys, 2, 4, NULL, NULL);
  assert(n == 2 && "same place, different name: two features");
  rec_free(&r);
  printf("  coordinate fallback key: precision and name decide: ok\n");

  /* ── raw merge, filter, post, geometry guard ─────────────────────────── */
  g_json_a = arr(F("{\"name\":\"A1\",\"station_id\":\"S1\",\"closed\":false}", 139, 35),
                 F("{\"name\":\"A2\",\"station_id\":\"S1\",\"closed\":true}", 139, 35), NULL);
  g_json_b = arr("{\"type\":\"Feature\",\"geometry\":null,\"properties\":{\"name\":\"NoGeom\"}}",
                 "{\"type\":\"Feature\",\"properties\":{\"name\":\"NoGeom2\"}}",
                 F("{\"name\":\"B1\",\"station_id\":\"S3\"}", 138, 34));
  n = unified_collect(&ctx, &sink, "unified-test", two, 2, NULL, 0, 4, NULL, NULL);
  assert(n == 3 && "no key functions: a raw merge keeps both S1 rows; geometry-less features are dropped");
  assert(by_name(&r, "A1") && by_name(&r, "A2") && !by_name(&r, "NoGeom"));
  rec_free(&r);
  n = unified_collect(&ctx, &sink, "unified-test", two, 2, keys, 2, 4, filt_no_closed, post_mark);
  assert(n == 2 && !by_name(&r, "A2") && "the filter runs before the merge");
  assert(cJSON_IsTrue(cJSON_GetObjectItem(by_name(&r, "A1"), "post_ran")) &&
         cJSON_IsTrue(cJSON_GetObjectItem(by_name(&r, "B1"), "post_ran")) && "post runs on every kept feature");
  rec_free(&r);
  n = unified_collect(&ctx, &sink, "unified-test", NULL, 0, keys, 2, 4, NULL, NULL);
  assert(n == 0 && r.n == 0 && "no upstreams: an honest zero");
  printf("  raw merge, filter-before-dedupe, post, geometry guard: ok\n");

  /* ── unified_capture ─────────────────────────────────────────────────── */
  cJSON *out = cJSON_CreateArray();
  assert(unified_capture(&ctx, "UNI_TEST_A", out) == 2 && cJSON_GetArraySize(out) == 2);
  assert(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(out, 0), "type")->valuestring, "Feature"));
  assert(unified_capture(&ctx, "UNI_NO_SUCH", out) == 0 && cJSON_GetArraySize(out) == 2);
  g_rc_c = -1;
  assert(unified_capture(&ctx, "UNI_TEST_C", out) == 0 && cJSON_GetArraySize(out) == 2 &&
         "a failed run appends nothing");
  g_rc_c = 0;
  cJSON *notarr = cJSON_CreateObject();
  assert(unified_capture(&ctx, "UNI_TEST_A", notarr) == 0 && "a non-array target is refused");
  assert(unified_capture(&ctx, "UNI_TEST_A", NULL) == 0);
  cJSON_Delete(notarr); cJSON_Delete(out);
  printf("  unified_capture: append, missing, failed, bad target: ok\n");

  printf("test_unified: all passed\n");
  return 0;
}
