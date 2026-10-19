/* tests/bench/bench_sink_rewrite.c — what a scheduled re-fetch of an
 * UNCHANGED feed costs the sink.
 *
 * Emits N records (default 20,000; argv[1] overrides) through one intel sink,
 * then emits the identical N again through a fresh sink — the second pass is
 * what every scheduled refresh of a static feed does. Reports wall time and
 * the bytes each pass appended to the WAL. Auto-checkpointing is off for the
 * measurement and the WAL is truncated between passes, so the -wal file size
 * IS the pass's write volume.
 *
 * Half the records carry Japanese text so MeCab is on the path the way it is
 * for the Japanese half of the corpus.
 *
 * Build and run from native/ after `make` (links every object but main.o):
 *
 *   cc -O2 -pthread -Ithird_party -iquote . tests/bench/bench_sink_rewrite.c \
 *      $(find obj -name '*.o' ! -name main.o) -o /tmp/bench_sink \
 *      $(make -s print-ldlibs) $(make -s print-cflags)
 *   JO_DB=$(mktemp -d)/b.db JO_SCHEMA=$PWD/core/schema.sql /tmp/bench_sink 20000
 */
#include "../../core/intel.h"
#include "../../core/db.h"
#include "../../third_party/sqlite3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

static double now_s(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1e6;
}

static long long wal_bytes(const char *dbp) {
  char p[1024]; struct stat st;
  snprintf(p, sizeof p, "%s-wal", dbp);
  return stat(p, &st) == 0 ? (long long)st.st_size : 0;
}

static double pass(db_handle *db, int n) {
  intel_sink k = intel_sink_make(db, "bench-src", NULL);
  char key[32], title[160], body[512], props[256];
  double t0 = now_s();
  for (int i = 0; i < n; i++) {
    snprintf(key, sizeof key, "rec-%d", i);
    if (i & 1) {
      snprintf(title, sizeof title, "東京都千代田区の観測点 %d 号", i);
      snprintf(body, sizeof body, "気象庁が発表した観測データ。地点番号 %d、"
               "東京都千代田区大手町一丁目。最新の値は毎時更新されます。", i);
    } else {
      snprintf(title, sizeof title, "Observation station %d", i);
      snprintf(body, sizeof body, "Hourly observation record %d from the "
               "national network; values update on the hour.", i);
    }
    snprintf(props, sizeof props, "{\"station\":%d,\"network\":\"bench\","
             "\"elevation_m\":%d}", i, i % 900);
    intel_item it = {0};
    it.remote_key = key; it.title = title; it.body = body;
    it.summary = "bench"; it.link = "https://example.invalid/obs";
    it.published_at = "2026-10-01T00:00:00Z";
    it.has_geo = 1; it.lat = 35.0 + (i % 1000) / 1000.0; it.lon = 139.0;
    it.record_type = "observation"; it.properties_json = props;
    k.emit(&k, &it);
  }
  double dt = now_s() - t0;
  int exact = 1;
  long stored = intel_sink_stored(&k, &exact);
  intel_sink_free(&k);
  if (stored != n) fprintf(stderr, "stored=%ld != %d\n", stored, n);
  return dt;
}

int main(int argc, char **argv) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  int n = argc > 1 ? atoi(argv[1]) : 20000;
  if (n <= 0) n = 20000;
  db_handle db = {0};
  if (db_open(&db, NULL, NULL) != 0) return 1;
  sqlite3_exec(db.h, "PRAGMA wal_autocheckpoint=0", NULL, NULL, NULL);

  const char *label[2] = {"first (all new)", "second (unchanged)"};
  for (int p = 0; p < 2; p++) {
    sqlite3_exec(db.h, "PRAGMA wal_checkpoint(TRUNCATE)", NULL, NULL, NULL);
    long long w0 = wal_bytes(dbp);
    double dt = pass(&db, n);
    long long w1 = wal_bytes(dbp);
    printf("%-20s n=%d  %.2fs  %.1f us/emit  WAL +%lld bytes (%.0f B/emit)\n",
           label[p], n, dt, dt * 1e6 / n, w1 - w0, (double)(w1 - w0) / n);
  }
  db_close(&db);
  return 0;
}
