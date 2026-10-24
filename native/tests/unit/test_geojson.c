/* tests/unit/test_geojson.c — feature identity in lib/geojson.c.
 *
 * WHAT IS BEING PROTECTED. geojson.c is the third of this tree's three record
 * paths (with lib/jsonlist.c and lib/hpengine.c) and, until this file, the only
 * one with no test at all — while carrying the same collision guard and the
 * same silent failure mode: `it.uid` is what the sink upserts on, so two
 * features that produce one uid store as ONE row while the caller counts both.
 * `records=N` stays honest about the emit and says nothing about the storage.
 *
 * The defect this file was written for (measured 2026-09-08 on
 * nam-ags-california-transit-stops, 88,247 features; the full reading is in
 * that row's header in collectors/feed/generated/vsrc13_us_state_1.c):
 * NATIVE_ID_KEYS carried `stop_id` AHEAD of the Feature's own `id`. A GTFS stop
 * id is unique per AGENCY, not globally, so on a multi-agency layer it keys a
 * GROUP. 10,000 features held only 9,084 distinct stop_ids ("105" eight times)
 * while the layer's own id — ArcGIS OBJECTID under f=geojson — was unique for
 * all 10,000. 916 collided; the in-page guard rescued 111 and 805 were LOST per
 * pass, because colliding agencies land in different 500-feature pages and
 * gj_collision_map() only ever sees one page.
 *
 * So the two halves below are both load-bearing, and they pull in opposite
 * directions — which is why a test is worth more here than a comment:
 *   - a key that is unique only within a group must NOT outrank feature.id;
 *   - the *_uid names, and the *_id names a collector SYNTHESIZES a stable id
 *     into (k_net.c and national_parks.c both carry an AUDIT NOTE about
 *     replacing a positional id with an OSM node id in exactly that field),
 *     must still outrank it, or that deliberate repair is silently discarded.
 * A future blanket reorder of NATIVE_ID_KEYS fails here by name.
 *
 * Includes geojson.c directly, so the static identity functions are reachable;
 * the harness links every object EXCEPT obj/lib/geojson.o and obj/main.o. See
 * tests/unit/run.sh. */

#include "../../lib/geojson.c"

static int g_fail = 0;
static void ok(int cond, const char *what) {
  printf("%s  %s\n", cond ? "  ok  " : "FAIL  ", what);
  if (!cond) g_fail++;
}
static void eqi(int got, int want, const char *what) {
  int c = got == want;
  printf("%s  %s", c ? "  ok  " : "FAIL  ", what);
  if (!c) printf("  (got %d, want %d)", got, want);
  printf("\n");
  if (!c) g_fail++;
}
static void eq(const char *got, const char *want, const char *what) {
  int c = got && strcmp(got, want) == 0;
  printf("%s  %s", c ? "  ok  " : "FAIL  ", what);
  if (!c) printf("  (got \"%s\", want \"%s\")", got ? got : "(null)", want);
  printf("\n");
  if (!c) g_fail++;
}

/* ── the uid of one feature, straight out of feature_uid() ───────────────── */

static void uid_of(const char *feat_json, char *out, size_t n, int *hashed) {
  out[0] = 0;
  cJSON *f = cJSON_Parse(feat_json);
  if (!f) { ok(0, "fixture parses"); return; }
  feature_uid(f, "src", out, n, hashed);
  cJSON_Delete(f);
}

/* ── a capturing sink, for the whole-page path ───────────────────────────── */

#define CAP_MAX 64
typedef struct { char uid[CAP_MAX][640]; int n; } cap;

static int cap_emit(intel_sink *s, const intel_item *it) {
  cap *c = s->ctx;
  if (c->n < CAP_MAX)
    snprintf(c->uid[c->n], sizeof c->uid[0], "%s", it->uid ? it->uid : "(null)");
  c->n++;
  return 1;
}

/* Emit a FeatureCollection (or bare array) and keep every uid. Returns the
 * count geojson_emit_features reported. */
static int emit_doc(cap *c, const char *json) {
  c->n = 0;
  cJSON *doc = cJSON_Parse(json);
  if (!doc) { ok(0, "fixture parses"); return -1; }
  intel_sink s = { .ctx = c, .emit = cap_emit };
  int n = geojson_emit_doc(&s, "src", doc);
  cJSON_Delete(doc);
  return n;
}

/* How many of the captured uids are distinct — the number of ROWS the sink
 * upsert leaves behind, which is the quantity the defect destroyed. */
static int distinct_uids(const cap *c) {
  int d = 0;
  for (int i = 0; i < c->n && i < CAP_MAX; i++) {
    int seen = 0;
    for (int j = 0; j < i; j++)
      if (!strcmp(c->uid[i], c->uid[j])) { seen = 1; break; }
    if (!seen) d++;
  }
  return d;
}

static int any_uid_has(const cap *c, const char *needle) {
  for (int i = 0; i < c->n && i < CAP_MAX; i++)
    if (strstr(c->uid[i], needle)) return 1;
  return 0;
}

/* ── 1. precedence ──────────────────────────────────────────────────────── */

static void test_precedence(void) {
  printf("-- featureUid precedence: NATIVE_ID_KEYS -> feature.id -> NATIVE_ID_GROUP_KEYS\n");
  char u[640];

  /* THE REGRESSION. RFC 7946 3.2 makes the Feature own `id` the record
   * identity; stop_id names an agency numbering scheme. Before the fix this
   * was "src|105" for every "105" on the layer. */
  uid_of("{\"id\":\"OBJECTID-7\",\"properties\":{\"stop_id\":\"105\"}}",
         u, sizeof u, NULL);
  eq(u, "src|OBJECTID-7", "feature.id outranks a properties stop_id");

  /* Our own five stop_id collectors set no top-level id, so for them nothing
   * changed — this is the half that made the fix safe to ship, and the half a
   * careless re-order would break. */
  uid_of("{\"properties\":{\"stop_id\":\"105\"}}", u, sizeof u, NULL);
  eq(u, "src|105", "stop_id is still used when the feature offers no id");

  /* The deliberate non-change: station_id/buoy_id/spring_id stay AHEAD of
   * feature.id because k_net.c and national_parks.c synthesize a stable OSM
   * node id into them on purpose, replacing a positional KNET_LIVE_<i+1> that
   * re-identified every row whenever the upstream reordered. */
  uid_of("{\"id\":9001,\"properties\":{\"station_id\":\"osm-node-123\"}}",
         u, sizeof u, NULL);
  eq(u, "src|osm-node-123", "station_id still outranks feature.id (OSM-id repair)");
  uid_of("{\"id\":9001,\"properties\":{\"buoy_id\":\"46026\"}}", u, sizeof u, NULL);
  eq(u, "src|46026", "buoy_id still outranks feature.id");
  uid_of("{\"id\":9001,\"properties\":{\"spring_id\":\"S-7\"}}", u, sizeof u, NULL);
  eq(u, "src|S-7", "spring_id still outranks feature.id");
  uid_of("{\"id\":9001,\"properties\":{\"uid\":\"u-1\"}}", u, sizeof u, NULL);
  eq(u, "src|u-1", "a collector-chosen uid still outranks feature.id");

  /* A numeric id prints as an integer, not 9.001e+03 — id_to_str bound-check
   * path, which a float-formatted id would silently re-key the fleet through. */
  uid_of("{\"id\":9001,\"properties\":{\"stop_id\":\"105\"}}", u, sizeof u, NULL);
  eq(u, "src|9001", "a numeric feature.id keys as an integer");

  /* An absent, null or empty value is not an id at any of the three steps. */
  uid_of("{\"id\":\"OBJ-1\",\"properties\":{\"stop_id\":null}}", u, sizeof u, NULL);
  eq(u, "src|OBJ-1", "a null stop_id is skipped");
  uid_of("{\"id\":\"OBJ-1\",\"properties\":{\"stop_id\":\"\"}}", u, sizeof u, NULL);
  eq(u, "src|OBJ-1", "an empty stop_id is skipped");
  uid_of("{\"id\":null,\"properties\":{\"stop_id\":\"105\"}}", u, sizeof u, NULL);
  eq(u, "src|105", "a null feature.id falls through to the group key");
}

/* ── 2. the hash fallback fingerprint is frozen ──────────────────────────── */

static void test_hash_fallback_is_frozen(void) {
  printf("-- the content-hash fallback keys the existing fleet: do not re-key it\n");
  /* Every hash-fallback row already in the database is keyed by THIS
   * fingerprint — sha1 of {"g":<geometry>,"p":<properties>} printed in source
   * key order, first 8 bytes. Changing the recipe re-keys the fleet and causes
   * exactly the unbounded growth the comment on feature_uid() warns about, so
   * the expected value is a literal, computed independently of this code. */
  char u[640]; int hashed = 0;
  uid_of("{\"geometry\":{\"type\":\"Point\",\"coordinates\":[139,35]},"
         "\"properties\":{\"foo\":\"bar\"}}", u, sizeof u, &hashed);
  eq(u, "src|h:bad0de12bb276389", "the fingerprint recipe is unchanged");
  eqi(hashed, 1, "the hash fallback is counted (the run line reports it)");

  /* The three-step lookup must not reach the hash when ANY step answers. */
  hashed = 0;
  uid_of("{\"geometry\":{\"type\":\"Point\",\"coordinates\":[139,35]},"
         "\"properties\":{\"stop_id\":\"105\"}}", u, sizeof u, &hashed);
  eqi(hashed, 0, "a group key still prevents the hash fallback");
}

/* ── 3. the measured page ────────────────────────────────────────────────── */

/* Six features in the shape the California layer serves: a unique OBJECTID per
 * feature, a stop_id repeated across agencies. */
static const char *PAGE_WITH_IDS =
  "{\"type\":\"FeatureCollection\",\"features\":["
  "{\"id\":1,\"properties\":{\"stop_id\":\"105\",\"name\":\"a\"}},"
  "{\"id\":2,\"properties\":{\"stop_id\":\"105\",\"name\":\"b\"}},"
  "{\"id\":3,\"properties\":{\"stop_id\":\"105\",\"name\":\"c\"}},"
  "{\"id\":4,\"properties\":{\"stop_id\":\"154\",\"name\":\"d\"}},"
  "{\"id\":5,\"properties\":{\"stop_id\":\"154\",\"name\":\"e\"}},"
  "{\"id\":6,\"properties\":{\"stop_id\":\"154\",\"name\":\"f\"}}]}";

static void test_measured_page(void) {
  printf("-- the measured layer shape: 6 features, 2 stop_ids, 6 OBJECTIDs\n");
  cap c;
  int n = emit_doc(&c, PAGE_WITH_IDS);
  eqi(n, 6, "all six features emit");
  eqi(c.n, 6, "the sink sees all six");
  eqi(distinct_uids(&c), 6, "six DISTINCT uids — six stored rows");
  eq(c.uid[0], "src|1", "the uid is the feature own id");
  eq(c.uid[5], "src|6", "...for the last one too");
  /* No feature collided, so the collision guard did not have to run: each uid
   * is the upstream own id and stays stable across runs. Under the old order
   * all six were "src|105"/"src|154", the guard content-hashed them WITHIN
   * this page, and the same keys on a later page had nothing to rescue them. */
  ok(!any_uid_has(&c, "|c:"), "no feature needed the collision guard");
}

/* ── 4. real dedupe and real collisions both still work ──────────────────── */

static void test_group_key_collisions(void) {
  printf("-- a stop_id-only page: byte-identical collapses, differing is kept\n");
  cap c;

  /* Two byte-identical features: collapsing them is correct deduplication,
   * and it is the property that makes a surviving same-page collapse PROOF
   * that the records were identical. */
  int n = emit_doc(&c,
    "{\"features\":["
    "{\"properties\":{\"stop_id\":\"105\",\"name\":\"a\"}},"
    "{\"properties\":{\"stop_id\":\"105\",\"name\":\"a\"}}]}");
  eqi(n, 2, "both byte-identical features emit");
  eqi(distinct_uids(&c), 1, "...and collapse onto one row (real dedupe)");

  /* Two features that merely SHARE the key are all kept, disambiguated by
   * content hash — nothing invented, nothing that differs merged. */
  n = emit_doc(&c,
    "{\"features\":["
    "{\"properties\":{\"stop_id\":\"105\",\"name\":\"a\"}},"
    "{\"properties\":{\"stop_id\":\"105\",\"name\":\"b\"}}]}");
  eqi(n, 2, "both differing features emit");
  eqi(distinct_uids(&c), 2, "...and are kept as two rows");
  ok(any_uid_has(&c, "|c:"), "the collision guard disambiguated by content");

  /* A feature whose id was already unique is NOT re-keyed by a neighbour
   * collision — otherwise every run of a layer with one duplicate would
   * re-uid the rest of the page and the table would grow without bound. */
  n = emit_doc(&c,
    "{\"features\":["
    "{\"properties\":{\"stop_id\":\"105\",\"name\":\"a\"}},"
    "{\"properties\":{\"stop_id\":\"105\",\"name\":\"b\"}},"
    "{\"properties\":{\"stop_id\":\"999\",\"name\":\"z\"}}]}");
  eqi(n, 3, "three features emit");
  eq(c.uid[2], "src|999", "the uncollided feature keeps its plain uid");
}

/* ── 5. identity across pages ───────────────────────────────────────────── */

static void test_identity_across_pages(void) {
  printf("-- paging is where a group key does its damage\n");
  /* gj_collision_map scope is ONE FeatureCollection, so a key that is not a
   * record identity does its damage at the page seam — and the identity of a
   * feature that HAS one must not drift there, or a paged layer inserts a new
   * row per page instead of updating. */
  cap p1, p2;
  emit_doc(&p1, "{\"features\":[{\"id\":7,\"properties\":{\"stop_id\":\"105\"}}]}");
  emit_doc(&p2, "{\"features\":[{\"id\":7,\"properties\":{\"stop_id\":\"105\"}}]}");
  eq(p1.uid[0], "src|7", "page 1 keys on the feature id");
  eq(p2.uid[0], p1.uid[0], "page 2 keys the same feature onto the same row");

  /* And two DIFFERENT features that share only the group key now key apart
   * across pages, which is precisely the 805-per-pass loss: within one page
   * the guard rescued them, across pages nothing did. */
  emit_doc(&p1, "{\"features\":[{\"id\":10,\"properties\":{\"stop_id\":\"105\"}}]}");
  emit_doc(&p2, "{\"features\":[{\"id\":11,\"properties\":{\"stop_id\":\"105\"}}]}");
  ok(strcmp(p1.uid[0], p2.uid[0]) != 0,
     "two agencies stop 105 key apart across pages (the 805/pass loss)");
}

int main(void) {
  test_precedence();
  test_hash_fallback_is_frozen();
  test_measured_page();
  test_group_key_collisions();
  test_identity_across_pages();
  if (g_fail) printf("\nFAILED (%d)\n", g_fail);
  else printf("\nall geojson identity tests passed\n");
  return g_fail ? 1 : 0;
}
