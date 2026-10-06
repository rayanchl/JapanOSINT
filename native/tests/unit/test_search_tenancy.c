/* test_search_tenancy.c — a search run belongs to the tenant that started it.
 *
 * GET /api/search/results/:id answered any authenticated caller holding the
 * request_id, and the pre-auth SSE stream answered anyone at all. The
 * request_id is exactly what a share link carries (/api/permalink tokens hold
 * it so a recipient can reopen the run), while the share sheet promises "the
 * server re-checks their workspace". So a link forwarded outside the
 * workspace was a cross-tenant read of the run: query, entities, findings.
 *
 * Holds:
 *   - progress_snapshot_for() serves the owner tenant, or the holder of the
 *     run's stream_key, and nobody else (a refusal looks like "not found");
 *     ownerless operator jobs (translate backfill) stay readable as before;
 *   - searchapi_results() hands the stream_key to the owner only, and applies
 *     the same rule to a run reloaded from the store after a restart, where
 *     a run with no recorded owner is refused rather than shown to all.
 *
 * Includes searchapi.c for its statics; links everything else. */
#include "../../core/searchapi.c"
#include "../../core/intel.h"

#include <assert.h>

int main(void) {
  /* ── live runs ──────────────────────────────────────────────────────── */
  assert(progress_create_owned("rid-live", "who is acme", 1, "tenant-A", "k3y-secret"));
  char key[64];
  char *snap = progress_snapshot_for("rid-live", "tenant-A", NULL, NULL, key, sizeof key);
  assert(snap && !strcmp(key, "k3y-secret") && "the owner gets the run and its key");
  free(snap);
  assert(!progress_snapshot_for("rid-live", "tenant-B", NULL, NULL, key, sizeof key) &&
         "another tenant must not read the run");
  assert(key[0] == 0 && "a refused caller must not receive the key");
  assert(!progress_snapshot_for("rid-live", NULL, NULL, NULL, NULL, 0));
  assert(!progress_snapshot_for("rid-live", NULL, "k3y", NULL, NULL, 0) && "prefix of the key");
  assert(!progress_snapshot_for("rid-live", NULL, "k3y-secreT", NULL, NULL, 0));
  snap = progress_snapshot_for("rid-live", NULL, "k3y-secret", NULL, NULL, 0);
  assert(snap && "the stream key opens the stream without a tenant");
  free(snap);
  /* an ownerless operator job is unchanged */
  assert(progress_create("rid-op", "translate:backfill", 1));
  snap = progress_snapshot_for("rid-op", "tenant-B", NULL, NULL, NULL, 0);
  assert(snap); free(snap);
  printf("  live runs: owner and key holder only; operator jobs unchanged: ok\n");

  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);

  /* /results on a live run */
  char *r = searchapi_results(&db, "tenant-A", "rid-live");
  assert(r && strstr(r, "\"stream_key\":\"k3y-secret\"") && strstr(r, "\"request_id\":\"rid-live\""));
  cJSON *j = cJSON_Parse(r);
  assert(j && "splicing the key must leave valid JSON");
  cJSON_Delete(j); free(r);
  assert(!searchapi_results(&db, "tenant-B", "rid-live"));
  printf("  /results live: owner gets the stream_key, other tenants 404: ok\n");

  /* ── a run reloaded from the store (server restarted) ──────────────── */
  intel_sink sink = intel_sink_make(&db, "osint-search", "legacy");
  const char *rids[] = { "rid-stored", "rid-unowned" };
  for (int i = 0; i < 2; i++) {
    char uid[128];
    snprintf(uid, sizeof uid, "osint-search|run:%s", rids[i]);
    intel_item it; memset(&it, 0, sizeof it);
    it.uid = uid; it.title = "stored run"; it.summary = "synthesis";
    it.properties_json = "{\"query\":\"who is acme\",\"phase\":\"completed\"}";
    it.record_type = "osint-search-run";
    assert(sink.emit(&sink, &it) >= 0);
  }
  intel_sink_free(&sink);
  assert(owner_persist(&db, "rid-stored", "tenant-A", "user-1") == 0);

  r = searchapi_results(&db, "tenant-A", "rid-stored");
  assert(r && strstr(r, "\"from_store\":true"));
  free(r);
  assert(!searchapi_results(&db, "tenant-B", "rid-stored") &&
         "a stored run must stay its owner's after a restart");
  assert(!searchapi_results(&db, "tenant-A", "rid-unowned") &&
         "a run with no recorded owner is refused, not shown to everyone");
  printf("  /results from store: owner only; unowned runs refused: ok\n");

  db_close(&db);
  printf("test_search_tenancy: ok\n");
  return 0;
}
