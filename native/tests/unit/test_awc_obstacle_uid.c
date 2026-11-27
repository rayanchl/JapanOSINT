/* tests/unit/test_awc_obstacle_uid.c — the one CONSTRUCTED uid in av_awc_bbox.c.
 *
 * WHAT IS BEING PROTECTED. That file runs four quadtree bbox sweeps. Three key
 * each record on the UPSTREAM's own identifier (icaoId/faaId for an airport,
 * `id` for a station, station|obsTime|rawOb for a PIREP) and their large
 * emitted-vs-stored gaps are the refinement working — a tile that comes back at
 * the endpoint's cap is re-queried as four children covering the same ground,
 * so the parent's records are emitted again. `awc-obstacles` is the fourth, and
 * the obstacle feed carries NO identifier: its uid is built here from the
 * record's own fields, so it is the only one that can be wrong.
 *
 * It was wrong. Measured 2026-11-12, one tile the sweep really queries
 * (`bbox=30.7500,-100.2500,34.1875,-93.0000`, 401 records, HTTP 200): the feed
 * serves lat/lon to THREE decimals, so `%.5f,%.5f|type` is a ~111 m grid cell
 * plus a type code, and **393 distinct keys for 401 records**. Eight groups
 * collided: two byte-identical (correct dedupe) and six that DIFFER, varying in
 * height (6 of 6), elev (4) and name (2). Adding `height` gives 399 — exactly
 * the tile's count of distinct WHOLE records, i.e. the ceiling, so nothing
 * further is separable and `elev` would add nothing.
 *
 * WHY A RECORD COUNT NEVER SHOWED IT, and why this test exists at all: the
 * sink's in-page guard (lib/geojson.c gj_collision_map) content-hashes
 * differing same-key features WITHIN one FeatureCollection, so inside a tile
 * nothing is lost and `stored` looks healthy. The damage is at the tile seam —
 * the guard's scope is one response, so a colliding pair split across a parent
 * and its child stores BARE in one and `|c:`-suffixed in the other: the same
 * physical obstruction under two uids. That is the same mechanism this tree
 * already measured costing nam-ags-california-transit-stops 805 features a
 * pass (lib/geojson.c header). Assertion 2 is the one that pins it.
 *
 * CONFIRMED against the old key by dropping the height from the uid, which is
 * exactly what this function built before. Five assertions fail, four controls
 * hold — and the MOST instructive line is one that passes both ways:
 *     ok    two stacks at one plant key apart        <-- passes under the OLD key
 *     FAIL  ...and neither needs the collision guard
 *     FAIL  a tile seam cannot re-uid the same obstruction
 *     FAIL  ...and the child re-emits it onto the row the parent wrote
 *     FAIL  the two measured stacks' KEYS differ
 *     FAIL  ...and the difference is the height the feed published
 *     ok    CONTROL: a byte-identical repeat still collapses to one row
 *     ok    CONTROL: a row with no coordinate is skipped, not emitted
 *     ok    CONTROL: height as a string and as a number key the same
 *     ok    CONTROL: jitter below the served precision keys the same
 * "Two stacks key apart" passes under the old key because the in-page guard
 * content-hashed them, so a distinct-uid count taken INSIDE one page cannot see
 * this defect at all — which is precisely why it survived two registry sweeps
 * and needed a hand probe. The assertions that discriminate are the guard one
 * and the two seam ones.
 *
 * The controls are what make the change safe: C1 proves no distinction was
 * invented (this file's standing prohibition is "do not key a record on its
 * tile", and a counter or a per-emit hash would break exactly C1), and C3 is
 * the av_dbl trap — a key read with jo_sv silently re-keys every row the day
 * the feed serves 51 for "51".
 *
 * The fixture records are VERBATIM from that response, fields unmodified.
 *
 * Includes the collector .c directly, so obst_feature() is reachable; the
 * harness then links every object except obj/collectors/sources/av_awc_bbox.o.
 * See tests/unit/run.sh. */

#include "../../collectors/sources/av_awc_bbox.c"

#include <stdio.h>
#include <string.h>

static int g_fail = 0;
static void ok(int cond, const char *what) {
  printf("%s  %s\n", cond ? "  ok  " : "FAIL  ", what);
  if (!cond) g_fail++;
}
static void eqi(int got, int want, const char *what) {
  if (got == want) { printf("  ok    %s\n", what); return; }
  printf("FAIL    %s  (got %d, want %d)\n", what, got, want);
  g_fail++;
}

/* ── a capturing sink ────────────────────────────────────────────────────── */
#define CAP_MAX 32
typedef struct { int n; char uid[CAP_MAX][600]; } cap;
static int cap_emit(intel_sink *s, const intel_item *it) {
  cap *c = (cap *)s->ctx;
  if (c->n < CAP_MAX) snprintf(c->uid[c->n], sizeof c->uid[0], "%s", it->uid);
  c->n++;
  return 0;
}
static int distinct_uids(const cap *c) {
  int d = 0;
  for (int i = 0; i < c->n && i < CAP_MAX; i++) {
    int seen = 0;
    for (int j = 0; j < i; j++) if (!strcmp(c->uid[i], c->uid[j])) seen = 1;
    if (!seen) d++;
  }
  return d;
}
static int any_uid_has(const cap *c, const char *needle) {
  for (int i = 0; i < c->n && i < CAP_MAX; i++)
    if (strstr(c->uid[i], needle)) return 1;
  return 0;
}

/* The constructed key for ONE record, read back off the Feature the collector
 * builds. Asserting on this rather than on the emitted uid is deliberate for
 * the key controls below: the sink's in-page guard will disambiguate two
 * features whose stored properties differ at all, so going through the emit
 * path would answer "do these records differ?" when the question is "does the
 * KEY separate them?". */
static const char *key_of(const char *one_record, char *out, size_t n) {
  cJSON *r = cJSON_Parse(one_record);
  cJSON *f = obst_feature(r);
  snprintf(out, n, "%s", "<none>");
  if (f) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    cJSON *u = p ? cJSON_GetObjectItem(p, "uid") : NULL;
    if (cJSON_IsString(u)) snprintf(out, n, "%s", u->valuestring);
    cJSON_Delete(f);
  }
  cJSON_Delete(r);
  return out;
}

/* Run a JSON array of obstacle records through the real record builder and the
 * real emit path, exactly as obstacle_tile does. */
static void emit_records(cap *c, const char *json) {
  memset(c, 0, sizeof *c);
  intel_sink sink = { c, cap_emit };
  cJSON *doc = cJSON_Parse(json);
  cJSON *feats = cJSON_CreateArray();
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    if (!cJSON_IsObject(r)) continue;
    cJSON *f = obst_feature(r);
    if (f) cJSON_AddItemToArray(feats, f);
  }
  geojson_emit_features(&sink, "awc-obstacles", feats);
  cJSON_Delete(feats);
  cJSON_Delete(doc);
}

/* The measured differing pair: two stacks in one plant, same 3-dp coordinate,
 * same type, same name, 77 ft and 69 ft. Verbatim from the 2026-11-12 tile. */
#define STACK_A "{\"name\":\"STACK/LGT-D\",\"type\":\"TWRL\",\"lat\":\"33.694\"," \
                "\"lon\":\"-94.419\",\"elev\":\"182\",\"height\":\"77\"}"
#define STACK_B "{\"name\":\"STACK/LGT-D\",\"type\":\"TWRL\",\"lat\":\"33.694\"," \
                "\"lon\":\"-94.419\",\"elev\":\"175\",\"height\":\"69\"}"
/* The measured byte-identical pair: one building, served twice. */
#define BLDG    "{\"name\":\"BLDG\",\"type\":\"TWRL\",\"lat\":\"33.418\"," \
                "\"lon\":\"-94.002\",\"elev\":\"154\",\"height\":\"43\"}"

static void test_differing_obstructions_key_apart(void) {
  printf("-- two charted obstructions in one 111 m cell are two records\n");
  cap c;
  emit_records(&c, "[" STACK_A "," STACK_B "]");
  eqi(c.n, 2, "both records emit");
  eqi(distinct_uids(&c), 2, "two stacks at one plant key apart");
  ok(!any_uid_has(&c, "|c:"),
     "...and neither needs the collision guard (the key itself separates them)");
}

static void test_tile_seam_is_stable(void) {
  printf("-- the tile seam: the guard sees one response, the sweep spans many\n");
  /* The parent tile came back at the cap holding only stack A; the child that
   * refines it holds both. Under the old key A stored BARE from the parent and
   * `|c:`-suffixed from the child — the same obstruction as two rows, which is
   * inflation no emitted/stored arithmetic can see. */
  cap parent, child;
  emit_records(&parent, "[" STACK_A "]");
  emit_records(&child,  "[" STACK_A "," STACK_B "]");
  eqi(parent.n, 1, "the parent slice emits its one record");
  ok(!strcmp(parent.uid[0], child.uid[0]),
     "a tile seam cannot re-uid the same obstruction");
  /* Three uids across the two calls would be the defect; two is correct. */
  int shared = 0;
  for (int i = 0; i < child.n; i++)
    if (!strcmp(child.uid[i], parent.uid[0])) shared = 1;
  ok(shared, "...and the child re-emits it onto the row the parent wrote");
}

/* ── controls: these must pass before AND after ─────────────────────────── */

static void test_controls(void) {
  printf("-- controls\n");
  cap c;

  /* C1. The whole point of the in-page guard is that byte-identical records
   * are REAL duplicates. A key change must not invent a distinction between
   * them — this file's standing prohibition is "do not key a record on its
   * tile", and a per-emit counter or hash would break exactly this. */
  emit_records(&c, "[" BLDG "," BLDG "]");
  eqi(c.n, 2, "both byte-identical records emit");
  eqi(distinct_uids(&c), 1,
      "CONTROL: a byte-identical repeat still collapses to one row");

  /* C2. The coordinate guard the extraction moved. */
  emit_records(&c, "[{\"name\":\"TOWER\",\"type\":\"TWRL\",\"elev\":\"100\","
                   "\"height\":\"50\"}]");
  eqi(c.n, 0, "CONTROL: a row with no coordinate is skipped, not emitted");

  /* C3. The av_dbl trap. This feed serves height as a STRING today and the AWC
   * products disagree per product (av_common.inc), so the key must not change
   * meaning when the encoding does. Asserted on the KEY: the two records'
   * stored properties differ by encoding, so the guard would separate them
   * downstream whatever the key said. */
  char ka[160], kb[160];
  key_of("{\"name\":\"T\",\"type\":\"TWRL\",\"lat\":\"33.1\",\"lon\":\"-94.1\","
         "\"elev\":\"100\",\"height\":\"1549\"}", ka, sizeof ka);
  key_of("{\"name\":\"T\",\"type\":\"TWRL\",\"lat\":33.1,\"lon\":-94.1,"
         "\"elev\":100,\"height\":1549}", kb, sizeof kb);
  ok(!strcmp(ka, kb), "CONTROL: height as a string and as a number key the same");

  /* C4. Coordinate precision was NOT raised, because the feed publishes three
   * decimals — so two records differing below that are one cell, and claiming
   * otherwise would be inventing a distinction the DOF has not got. */
  key_of("{\"name\":\"P\",\"type\":\"TWRL\",\"lat\":33.1000001,\"lon\":-94.1,"
         "\"elev\":\"90\",\"height\":\"32\"}", ka, sizeof ka);
  key_of("{\"name\":\"P\",\"type\":\"TWRL\",\"lat\":33.1000002,\"lon\":-94.1,"
         "\"elev\":\"90\",\"height\":\"32\"}", kb, sizeof kb);
  ok(!strcmp(ka, kb),
     "CONTROL: jitter below the served precision keys the same");

  /* And the key really does carry the height, rather than happening to work:
   * the two measured stacks differ in it and nothing else that is keyed. */
  key_of(STACK_A, ka, sizeof ka);
  key_of(STACK_B, kb, sizeof kb);
  ok(strcmp(ka, kb) != 0, "the two measured stacks' KEYS differ");
  ok(strstr(ka, "|77") && strstr(kb, "|69"),
     "...and the difference is the height the feed published");
}

int main(void) {
  printf("--- awc-obstacles: the only constructed uid in av_awc_bbox.c ---\n");
  test_differing_obstructions_key_apart();
  test_tile_seam_is_stable();
  test_controls();
  if (g_fail) { printf("\nFAILED (%d)\n", g_fail); return 1; }
  printf("\nall awc-obstacles identity tests passed\n");
  return 0;
}
