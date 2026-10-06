/* tests/unit/test_cameraproxy.c — /api/camera-proxy may not be turned into an
 * internal-network fetcher or an oracle (core/cameraproxy.c). No network: the
 * only dials are to 127.0.0.1:9, and only with the LAN opt-in set.
 *
 * WHAT IS BEING PROTECTED. Camera URLs are written by discovery collectors from
 * whatever their upstream reported, and any signed-in user picks the uid. The
 * route checked only the floor, which admits loopback and RFC1918, and its
 * errors echoed "upstream <status>" — so a poisoned record made it a port and
 * status probe of the server's own network.
 *   1. loopback / RFC1918 / metadata camera URLs are refused by default;
 *   2. every upstream-side failure answers with ONE generic body (502), with
 *      nothing about the upstream in it — refused, unreachable, or not an
 *      image all read the same;
 *   3. JO_CAMERA_ALLOW_LAN=1 lets a LAN camera be dialled (and its failure
 *      still reads generic).
 *
 * Includes cameraproxy.c to share its statics (none needed; it keeps run.sh
 * from linking a duplicate object). */

#include "../../core/cameraproxy.c"

#include <assert.h>

static db_handle g_db;

static void put_camera(const char *id, const char *url) {
  char uid[128], props[512];
  snprintf(uid, sizeof uid, "camera-discovery|%s", id);
  snprintf(props, sizeof props, "{\"url\":\"%s\"}", url);
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(g_db.h,
    "INSERT OR REPLACE INTO intel_items(uid,source_id,title,fetched_at,"
    "record_type,properties) VALUES(?1,'camera-discovery','cam',"
    "'2026-10-06T00:00:00Z','camera',?2)", -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 2, props, -1, SQLITE_STATIC);
  assert(sqlite3_step(s) == SQLITE_DONE);
  sqlite3_finalize(s);
}

static void expect_generic(const char *id) {
  unsigned char *img = NULL; size_t n = 0; char ct[64] = {0}; int st = 0;
  char *ej = camera_proxy_fetch(&g_db, id, &img, &n, ct, sizeof ct, &st);
  assert(ej && !img);
  assert(st == 502);
  assert(strcmp(ej, "{\"error\":\"" CAM_PROXY_GENERIC_ERR "\"}") == 0);
  assert(!strstr(ej, "private") && !strstr(ej, "status") && !strstr(ej, "127."));
  free(ej);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  assert(dbp && "run.sh sets JO_DB to a scratch database");
  assert(db_open(&g_db, dbp, NULL) == 0);
  unsetenv("JO_HTTP_BLOCK_PRIVATE");
  unsetenv("JO_CAMERA_ALLOW_LAN");

  put_camera("loop", "http://127.0.0.1:9/snap.jpg");
  put_camera("lan", "http://192.168.1.20/snap.jpg");
  put_camera("meta", "http://169.254.169.254/latest/meta-data/");
  put_camera("name", "http://localhost:9/snap.jpg");
  put_camera("scheme", "file:///etc/passwd");

  expect_generic("loop");
  expect_generic("lan");
  expect_generic("meta");
  expect_generic("name");
  expect_generic("scheme");

  /* Unknown uid is a DB answer, not an upstream one, and stays a 404. */
  { unsigned char *img = NULL; size_t n = 0; char ct[64]; int st = 0;
    char *ej = camera_proxy_fetch(&g_db, "nope", &img, &n, ct, sizeof ct, &st);
    assert(ej && st == 404); free(ej); }

  /* Opt-in: the loopback camera is now DIALLED (port 9 is closed, so the
   * connect fails) — and the failure is still the generic body. */
  setenv("JO_CAMERA_ALLOW_LAN", "1", 1);
  expect_generic("loop");
  expect_generic("meta");                 /* metadata stays refused */
  unsetenv("JO_CAMERA_ALLOW_LAN");

  db_close(&g_db);
  printf("test_cameraproxy: OK\n");
  return 0;
}
