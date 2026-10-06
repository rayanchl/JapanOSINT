/* test_intel_tenancy.c — an investigation row is visible to its own tenant only.
 *
 * The search pipeline wrote each run's summary row (the query, the entities it
 * was about, the synthesis) under the shared 'legacy' tenant, and the intel
 * feed, search and item routes passed no tenant at all — so every workspace
 * could list every other workspace's investigations through /api/intel/items.
 * The run row is now written under the run's owner and the routes scope reads
 * to "this tenant + the shared corpus".
 *
 * Holds, at the query layer the routes call:
 *   - intelapi_list_items_st() with Q.tenant = B lists the shared corpus and
 *     B's rows, never A's; with A it lists A's run row;
 *   - intelapi_item_by_uid_tenant() refuses A's row to B and serves it to A;
 *   - a shared-corpus row is served to both. */
#include "../../core/intelapi.h"
#include "../../core/intel.h"
#include "../../core/db.h"
#include "../../third_party/cJSON.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put(db_handle *db, const char *tenant, const char *uid, const char *title) {
  intel_sink k = intel_sink_make(db, "osint-search", tenant);
  intel_item it = {0};
  it.uid = uid; it.title = title; it.record_type = "osint_search_run";
  it.published_at = "2026-10-03T00:00:00Z";
  assert(k.emit(&k, &it) >= 0);
  intel_sink_free(&k);
}

static int lists(db_handle *db, const char *tenant, const char *needle) {
  intel_items_query Q = {0};
  Q.source = "osint-search"; Q.tenant = tenant; Q.limit = 100;
  int st = 200;
  char *b = intelapi_list_items_st(db, &Q, &st);
  assert(b && st == 200);
  int hit = strstr(b, needle) != NULL;
  free(b);
  return hit;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);

  put(&db, "tenant-A", "rid-a-secret", "OSINT search: who is tenant A watching");
  put(&db, "legacy",   "rid-shared",   "OSINT search: shared corpus row");

  assert(lists(&db, "tenant-A", "rid-a-secret") && "the owner lists its run row");
  assert(!lists(&db, "tenant-B", "rid-a-secret") && "another tenant must not list it");
  assert(lists(&db, "tenant-B", "rid-shared") && lists(&db, "tenant-A", "rid-shared") &&
         "the shared corpus stays visible to every tenant");
  printf("  feed: owner sees its run, other tenants do not, shared rows for all: ok\n");

  char *r = intelapi_item_by_uid_tenant(&db, "osint-search|rid-a-secret", "tenant-B");
  if (!r) r = intelapi_item_by_uid_tenant(&db, "rid-a-secret", "tenant-B");
  assert(!r && "another tenant must not open the run row by uid");
  char *ra = intelapi_item_by_uid_tenant(&db, "osint-search|rid-a-secret", "tenant-A");
  if (!ra) ra = intelapi_item_by_uid_tenant(&db, "rid-a-secret", "tenant-A");
  assert(ra && "the owner opens its run row by uid");
  free(ra);
  printf("  item: refused to another tenant, served to the owner: ok\n");

  db_close(&db);
  printf("\nall passed\n");
  return 0;
}
