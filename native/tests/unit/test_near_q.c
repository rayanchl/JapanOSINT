/* test_near_q.c — the ?near= proximity mode honours q and lang_view.
 *
 * Audit of 2026-10-02, open item 11: nearapi.c read only source, record_type,
 * since and until, so a text filter the client sent was dropped and every row
 * in the radius came back as if it had matched; and httpd.c's near branch
 * returned before the lang_view shaping, so near rows never carried a
 * translation.
 *
 * Holds, at the layer the route calls:
 *   - q filters the radius by the feed's own MATCH expression;
 *   - qAlt widens it (OR), exactly as on the feed;
 *   - a q with no searchable token is NOT applied and the envelope says so
 *     (q_applied=false + note), rather than looking like a filtered answer;
 *   - another tenant's row in range stays invisible under q;
 *   - an over-long filter is a 414, never a silently clipped filter;
 *   - lang_view=both on the near envelope attaches the stored translation
 *     (the same translate_shape_items() call httpd.c makes for the feed). */
#include "../../core/nearapi.h"
#include "../../core/intelapi.h"
#include "../../core/intel.h"
#include "../../core/translate.h"
#include "../../core/db.h"
#include "../../third_party/cJSON.h"
#include "../../third_party/sqlite3.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put(db_handle *db, const char *tenant, const char *key,
                const char *title, double lat, double lon) {
  intel_sink k = intel_sink_make(db, "near-test", tenant);
  intel_item it = {0};
  it.remote_key = key; it.title = title; it.record_type = "near_test";
  it.published_at = "2026-10-03T00:00:00Z";
  it.has_geo = 1; it.lat = lat; it.lon = lon;
  assert(k.emit(&k, &it) >= 0);
  intel_sink_free(&k);
}

static char *near(db_handle *db, const char *tenant, const char *qs, int *st) {
  char full[1024];
  snprintf(full, sizeof full, "near=35.6800,139.7600&radius_m=5000%s%s",
           qs && *qs ? "&" : "", qs ? qs : "");
  return nearapi_items(db, tenant, "35.6800,139.7600", full, st);
}

static int count_rows(const char *body) {
  cJSON *j = cJSON_Parse(body);
  assert(j);
  int n = cJSON_GetArraySize(cJSON_GetObjectItem(j, "data"));
  cJSON_Delete(j);
  return n;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);

  put(&db, "legacy",   "k1", "kobe port fire",          35.6801, 139.7601);
  put(&db, "legacy",   "k2", "shibuya station closure", 35.6810, 139.7610);
  put(&db, "legacy",   "k3", "tokyo bay ferry delay",   35.6820, 139.7620);
  put(&db, "legacy",   "k4", "osaka fire far away",     34.6900, 135.5000);
  put(&db, "tenant-B", "k5", "tenant b warehouse fire", 35.6802, 139.7602);

  int st = 0;
  char *b = near(&db, "tenant-A", "", &st);
  assert(b && st == 200 && count_rows(b) == 3 && "baseline: three rows in range for A");
  assert(!strstr(b, "q_applied") && "no q sent, nothing to disclose");
  free(b);

  b = near(&db, "tenant-A", "q=fire", &st);
  assert(b && st == 200);
  assert(strstr(b, "kobe port fire") && "the matching row is returned");
  assert(!strstr(b, "shibuya") && !strstr(b, "ferry") && "non-matching rows are filtered out");
  assert(!strstr(b, "osaka") && "the radius still applies");
  assert(!strstr(b, "tenant b warehouse") && "another tenant's row stays invisible under q");
  assert(strstr(b, "\"q_applied\":true") && strstr(b, "\"q\":\"fire\""));
  assert(count_rows(b) == 1);
  free(b);
  printf("  q filters the radius by the feed's MATCH expression, tenancy kept: ok\n");

  b = near(&db, "tenant-B", "q=fire", &st);
  assert(b && st == 200 && strstr(b, "tenant b warehouse") && strstr(b, "kobe port fire") &&
         count_rows(b) == 2 && "B sees its own row and the shared corpus");
  free(b);

  b = near(&db, "tenant-A", "q=fire&qAlt=ferry", &st);
  assert(b && st == 200 && count_rows(b) == 2 && strstr(b, "kobe port fire") &&
         strstr(b, "tokyo bay ferry") && "qAlt is OR'd with q, as on the feed");
  free(b);
  printf("  qAlt widens the match: ok\n");

  b = near(&db, "tenant-A", "q=%3B%3B%3B", &st);       /* ";;;" */
  assert(b && st == 200 && count_rows(b) == 3);
  assert(strstr(b, "\"q_applied\":false") && strstr(b, "q_ignored_no_searchable_token") &&
         "an unusable q is disclosed, not passed off as a filtered answer");
  free(b);
  printf("  a q with no searchable token is disclosed as not applied: ok\n");

  { char qs[700] = "q=";
    for (int i = 0; i < 300; i++) strcat(qs, "a");
    b = near(&db, "tenant-A", qs, &st);
    assert(b && st == 414 && strstr(b, "filter_too_long"));
    free(b); }
  { char qs[300] = "source=";
    for (int i = 0; i < 200; i++) strcat(qs, "s");
    b = near(&db, "tenant-A", qs, &st);
    assert(b && st == 414 && "a clipped source id is a different filter: refused");
    free(b); }
  printf("  over-long filters are refused with 414, not clipped: ok\n");

  /* lang_view: give one row a stored translation, then shape the near
   * envelope the way httpd.c does for every intel_items route. */
  translate_migrate(&db);
  { char *e = NULL;
    int rc = sqlite3_exec(db.h,
      "UPDATE intel_items SET title_en='Kobe port fire (EN)', "
      "translated_at='2026-10-03T00:00:00Z' WHERE uid='near-test|k1'",
      NULL, NULL, &e);
    if (rc != SQLITE_OK) { fprintf(stderr, "%s\n", e ? e : "?"); return 1; } }
  b = near(&db, "tenant-A", "q=fire&lang_view=both", &st);
  assert(b && st == 200);
  char *shaped = translate_shape_items(&db, b, translate_view_parse("both"));
  assert(shaped && strstr(shaped, "Kobe port fire (EN)") &&
         strstr(shaped, "\"lang_view\":\"both\"") && strstr(shaped, "\"distance_m\"") &&
         "the near envelope takes the translation shaping intact");
  free(shaped); free(b);
  printf("  lang_view shaping applies to the near envelope: ok\n");

  db_close(&db);
  printf("\nall passed\n");
  return 0;
}
