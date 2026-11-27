/* tests/unit/test_unified.c — a fusion must not report a success nobody had.
 *
 * lib/unified.c fuses already-registered upstream sources, and its
 * Promise.allSettled parity skipped an upstream whose run() returned rc<0.
 * Swallowing ONE such refusal is the point of the design; swallowing ALL of
 * them made nine `unified-*` rows report `rc=0 records=0 stored=0` — rule 1's
 * invisible nothing — in the one place a fixed upstream cannot be heard.
 *
 * Measured 2026-11-12 on `unified-airports`, both upstreams refusing in as
 * many words (one of them in the sentence lib/mlit_ksj.h's 2026-11-05 repair
 * added), 134 s and 41,188 bytes spent, run line `rc=0 records=0 stored=0`.
 *
 * The CONTROLS are what make the change safe to ship, and they are the first
 * four assertions rather than an afterthought: a fusion one of whose upstreams
 * RAN stays silent even when the fusion is empty, because a quiet fusion is a
 * legitimate outcome and a failure notice on every quiet run is the noise
 * these disclosures cannot afford. Each assertion below was checked against
 * the pre-change lib/unified.c: 5, 6 and 8 fail there (they are the fix), and
 * 1-4 and 7 pass both ways (they are the controls).
 */
#include "source.h"
#include "lib/unified.h"
#include "third_party/cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ── a counting sink ─────────────────────────────────────────────────── */
typedef struct { int n; } count_ctx;
static int count_emit(intel_sink *s, const intel_item *it) {
  (void)it; ((count_ctx *)s->ctx)->n++; return 1;
}

/* ── fake upstreams ──────────────────────────────────────────────────── */

/* One well-formed Feature. geojson_emit_features needs geometry AND
 * properties, and so does unified.c's own mergeFC guard. */
static int emit_one(intel_sink *sink, const char *id, double lon) {
  cJSON *arr = cJSON_CreateArray();
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(35.0));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "title", id);
  cJSON_AddStringToObject(p, "port_id", id);
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(arr, f);
  extern int geojson_emit_features(intel_sink *, const char *, cJSON *);
  int n = geojson_emit_features(sink, "unit-up", arr);
  cJSON_Delete(arr);
  return n;
}

static int up_serves(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx; emit_one(sink, "PORT_UNIT_A", 139.0); return 1;
}
static int up_serves_b(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx; emit_one(sink, "PORT_UNIT_B", 140.0); return 1;
}
/* RAN and found nothing — the honest empty. Must never be read as a refusal. */
static int up_empty(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx; (void)sink; return 0;
}
/* REFUSED: the upstream itself says it could not fetch. */
static int up_refuses(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx; (void)sink; return -1;
}

static const source_def U_SERVES = { .id = "UNIT_UNI_SERVES",
  .collector = "unit", .name = "unit", .run = up_serves, .category = "test" };
static const source_def U_SERVES_B = { .id = "UNIT_UNI_SERVES_B",
  .collector = "unit", .name = "unit", .run = up_serves_b, .category = "test" };
static const source_def U_EMPTY = { .id = "UNIT_UNI_EMPTY",
  .collector = "unit", .name = "unit", .run = up_empty, .category = "test" };
static const source_def U_REFUSES = { .id = "UNIT_UNI_REFUSES",
  .collector = "unit", .name = "unit", .run = up_refuses, .category = "test" };
REGISTER_SOURCE(U_SERVES)
REGISTER_SOURCE(U_SERVES_B)
REGISTER_SOURCE(U_EMPTY)
REGISTER_SOURCE(U_REFUSES)

static const char *k_port_id_name(cJSON *f, char *b, size_t n) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *v = p ? cJSON_GetObjectItem(p, "port_id") : NULL;
  if (!cJSON_IsString(v) || !v->valuestring[0]) return NULL;
  snprintf(b, n, "%s", v->valuestring);
  return b;
}

/* Fuse the named upstreams and return unified_collect's rc; *emitted gets the
 * number of rows that reached the sink. */
static int fuse(int *emitted, const char *const *names, int n_names) {
  unified_upstream ups[4];
  assert(n_names <= 4);
  for (int i = 0; i < n_names; i++) {
    ups[i].name = names[i];
    ups[i].kind = NULL;
  }
  static const unified_keyfn keys[] = { k_port_id_name };
  count_ctx cc = { 0 };
  intel_sink sink = { &cc, count_emit };
  source_ctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.source_id = "unit-fusion";
  int rc = unified_collect(&ctx, &sink, "unit-fusion", ups, n_names,
                           keys, 1, 4, NULL, NULL);
  if (emitted) *emitted = cc.n;
  return rc;
}

static int fails = 0;
static void ck(const char *what, int got, int want) {
  if (got == want) { printf("  ok   %s\n", what); return; }
  printf("  FAIL %s: got %d, want %d\n", what, got, want);
  fails++;
}

int main(void) {
  printf("--- unified: a fusion must not report a success nobody had ---\n");
  int n;

  /* ── CONTROLS: nothing below changes for a fusion that was served ──── */

  static const char *all_ok[] = { "UNIT_UNI_SERVES", "UNIT_UNI_SERVES_B" };
  ck("two upstreams serve -> rc is the record count", fuse(&n, all_ok, 2), 2);
  ck("  ...and both records reach the sink", n, 2);

  /* ONE refusal among several is the design: the fusion survives it. */
  static const char *partial[] = { "UNIT_UNI_SERVES", "UNIT_UNI_REFUSES" };
  ck("one refuses, one serves -> still a success", fuse(&n, partial, 2), 1);

  /* THE control that matters. Every upstream RAN; the fusion is empty anyway.
   * That is a quiet fusion, not a failed fetch, and it must stay silent — a
   * notice here would fire on every quiet run. */
  static const char *all_empty[] = { "UNIT_UNI_EMPTY", "UNIT_UNI_EMPTY" };
  ck("every upstream ran and gave nothing -> rc=0, SILENT",
     fuse(&n, all_empty, 2), 0);

  /* ── THE FIX: total refusal is a failed fetch ─────────────────────── */

  static const char *all_refused[] = { "UNIT_UNI_REFUSES", "UNIT_UNI_REFUSES" };
  ck("every upstream refused -> rc=-1 so the scheduler discloses",
     fuse(&n, all_refused, 2), -1);
  ck("  ...and nothing was stored under the fusion's own id", n, 0);

  /* An upstream id naming nothing registered is NOT a refusal by the
   * publisher — but it is not a served upstream either, so a fusion whose
   * every name is dangling must disclose rather than read as quiet. All 22
   * real upstream ids resolve today (checked against the binary's registry),
   * so this branch is a guard against a future retirement, not a live case. */
  static const char *dangling[] = { "UNIT_UNI_NO_SUCH_SOURCE_XYZ" };
  ck("an upstream id that resolves to nothing -> rc=-1",
     fuse(&n, dangling, 1), -1);

  /* A fusion declaring NO upstreams at all is a programming error, not a
   * fetch failure: n_ups == 0 must not be reported as a refused fetch. */
  ck("no upstreams declared -> not reported as a failed fetch",
     fuse(&n, all_ok, 0), 0);

  /* ── unified_capture's `served` out-param ─────────────────────────── */

  source_ctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.source_id = "unit-capture";
  {
    cJSON *raw = cJSON_CreateArray();
    int served = 0;
    int got = unified_capture(&ctx, "UNIT_UNI_SERVES", raw, &served);
    ck("capture: a serving upstream counts as served", served, 1);
    ck("  ...and its feature is captured", got, 1);
    cJSON_Delete(raw);
  }
  {
    cJSON *raw = cJSON_CreateArray();
    int served = 0;
    int got = unified_capture(&ctx, "UNIT_UNI_EMPTY", raw, &served);
    /* THE distinction the return value cannot make: an upstream that RAN and
     * gave nothing is served; one that refused is not. Both return 0. */
    ck("capture: an upstream that ran and gave nothing IS served", served, 1);
    ck("  ...and captured nothing", got, 0);
    cJSON_Delete(raw);
  }
  {
    cJSON *raw = cJSON_CreateArray();
    int served = 0;
    int got = unified_capture(&ctx, "UNIT_UNI_REFUSES", raw, &served);
    ck("capture: a refusing upstream is NOT served", served, 0);
    ck("  ...and captured nothing", got, 0);
    cJSON_Delete(raw);
  }
  {
    cJSON *raw = cJSON_CreateArray();
    int served = 7;                 /* it INCREMENTS; it must not assign */
    unified_capture(&ctx, "UNIT_UNI_SERVES", raw, &served);
    ck("capture: served is incremented, not overwritten", served, 8);
    cJSON_Delete(raw);
  }
  {   /* A NULL out-param must still work: the parameter is optional. */
    cJSON *raw = cJSON_CreateArray();
    ck("capture: a NULL served out-param is accepted",
       unified_capture(&ctx, "UNIT_UNI_SERVES", raw, NULL), 1);
    cJSON_Delete(raw);
  }

  if (fails) { printf("FAILED (%d)\n", fails); return 1; }
  printf("all passed\n");
  return 0;
}
