/* test_intel_sink_write.c — what the sink writes, and what it no longer
 * rewrites.
 *
 * Holds:
 *   1. an UNCHANGED re-fetch writes only last_seen_at: fetched_at, the FTS row
 *      (rowid AND the `keywords` translate.c put there) and the uid_map are
 *      untouched, and emit() returns 0 (not new) — so no alert fires;
 *   2. it still counts in `stored` (distinct uids), exactly as a full write;
 *   3. a CHANGED re-fetch is a full write: new fetched_at, new FTS row, new
 *      content hash, and the new text is searchable;
 *   4. a row whose FTS row went missing is re-indexed by the next re-fetch even
 *      when its content is unchanged (the self-heal the full write gave);
 *   5. an unchanged re-fetch does not make the row eligible for the embedding
 *      pod's delta walk again (it keys on fetched_at, which no longer moves);
 *   6. two remote_keys longer than the uid buffer that share their first 600
 *      bytes get DIFFERENT uids (they used to truncate onto one), the uid is
 *      deterministic, and it ends in '#' + 64 hex;
 *   7. a long key that would be cut inside a multibyte character leaves a
 *      valid UTF-8 prefix;
 *   8. the sink's cached statements are finalized by intel_sink_free(): the
 *      connection then closes cleanly with sqlite3_close (not _v2). */
#include "../../core/intel.c"
#include "../../core/db.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static db_handle db;

static void sql(const char *q) {
  char *e = NULL;
  if (sqlite3_exec(db.h, q, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", q);
    exit(1);
  }
}

static char *text1(const char *q, const char *uid) {
  sqlite3_stmt *s; char *r = NULL;
  assert(sqlite3_prepare_v2(db.h, q, -1, &s, NULL) == SQLITE_OK);
  if (uid) sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL)
    r = strdup((const char *)sqlite3_column_text(s, 0));
  sqlite3_finalize(s);
  return r;
}

static long long int1(const char *q, const char *uid) {
  sqlite3_stmt *s; long long r = -1;
  assert(sqlite3_prepare_v2(db.h, q, -1, &s, NULL) == SQLITE_OK);
  if (uid) sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) r = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return r;
}

static int put(intel_sink *k, const char *key, const char *title, const char *body) {
  intel_item it = {0};
  it.remote_key = key; it.title = title; it.body = body;
  it.record_type = "observation"; it.properties_json = "{\"n\":1}";
  return k->emit(k, &it);
}

static int valid_utf8(const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    int n = *p < 0x80 ? 1 : (*p >> 5) == 6 ? 2 : (*p >> 4) == 14 ? 3 : (*p >> 3) == 30 ? 4 : 0;
    if (!n) return 0;
    for (int i = 1; i < n; i++) if ((p[i] & 0xC0) != 0x80) return 0;
    p += n;
  }
  return 1;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  assert(db_open(&db, NULL, NULL) == 0);
  const char *U = "t-src|k1";

  /* ── 1/2: unchanged re-fetch ───────────────────────────────────────── */
  intel_sink k = intel_sink_make(&db, "t-src", NULL);
  assert(put(&k, "k1", "東京の観測点", "本文テキスト") == 1);
  intel_sink_free(&k);
  char *fetched0 = text1("SELECT fetched_at FROM intel_items WHERE uid=?1", U);
  long long rid0 = int1("SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?1", U);
  assert(fetched0 && rid0 > 0);
  /* translate.c's write — the one an unconditional FTS re-insert wiped. */
  sql("UPDATE intel_items_fts SET keywords='tokyo observation point' "
      "WHERE uid='t-src|k1'");
  usleep(5000);                         /* make a moved timestamp visible */

  k = intel_sink_make(&db, "t-src", NULL);
  assert(put(&k, "k1", "東京の観測点", "本文テキスト") == 0 && "not new");
  int exact = 0;
  assert(intel_sink_stored(&k, &exact) == 1 && exact == 1);
  intel_sink_free(&k);
  char *fetched1 = text1("SELECT fetched_at FROM intel_items WHERE uid=?1", U);
  char *seen1 = text1("SELECT last_seen_at FROM intel_items WHERE uid=?1", U);
  assert(strcmp(fetched0, fetched1) == 0 && "fetched_at must not move");
  assert(seen1 && strcmp(seen1, fetched1) > 0 && "last_seen_at must move");
  assert(int1("SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?1", U) == rid0);
  char *kw = text1("SELECT keywords FROM intel_items_fts WHERE uid=?1", U);
  assert(kw && strcmp(kw, "tokyo observation point") == 0 && "keywords survive");
  free(kw);
  printf("  unchanged re-fetch: last_seen_at only; fetched_at, FTS row and "
         "keywords untouched; stored=1: ok\n");

  /* ── 5: the embedding delta keys on fetched_at ─────────────────────── */
  {
    /* embed_pod's DELTA predicate is `embedded_at <= fetched_at`; with an
     * embedded_at taken between the two passes the row stays out. */
    char q[256];
    snprintf(q, sizeof q, "SELECT count(*) FROM intel_items WHERE uid=?1 AND "
             "fetched_at >= '%s'", seen1);
    assert(int1(q, U) == 0 && "an unchanged re-fetch is not re-eligible");
  }
  printf("  unchanged re-fetch does not re-qualify for the embed delta: ok\n");

  /* ── 3: changed re-fetch ───────────────────────────────────────────── */
  usleep(5000);
  k = intel_sink_make(&db, "t-src", NULL);
  assert(put(&k, "k1", "大阪の観測点", "本文テキスト") == 0);
  intel_sink_free(&k);
  char *fetched2 = text1("SELECT fetched_at FROM intel_items WHERE uid=?1", U);
  assert(strcmp(fetched2, fetched1) > 0 && "a change is a full write");
  /* The FTS row was re-inserted (FTS5 may reuse the rowid of a deleted last
   * row, so the stale translation is the visible proof). */
  kw = text1("SELECT keywords FROM intel_items_fts WHERE uid=?1", U);
  assert(kw && !*kw && "a changed row's stale translation is reset");
  free(kw);
  assert(int1("SELECT count(*) FROM intel_items_fts WHERE intel_items_fts MATCH "
              "'\"大阪\"'", NULL) == 1 && "the new text is searchable");
  assert(int1("SELECT count(*) FROM intel_items_fts", NULL) == 1 && "old FTS row gone");
  printf("  changed re-fetch: full write, re-indexed: ok\n");

  /* ── 4: self-heal of a missing FTS row ─────────────────────────────── */
  sql("DELETE FROM intel_items_fts WHERE uid='t-src|k1'");
  sql("DELETE FROM intel_items_fts_uid_map WHERE uid='t-src|k1'");
  k = intel_sink_make(&db, "t-src", NULL);
  assert(put(&k, "k1", "大阪の観測点", "本文テキスト") == 0);
  intel_sink_free(&k);
  assert(int1("SELECT count(*) FROM intel_items_fts_uid_map WHERE uid=?1", U) == 1);
  assert(int1("SELECT count(*) FROM intel_items_fts", NULL) == 1);
  printf("  unchanged content but missing FTS row is re-indexed: ok\n");

  /* ── 6/7: long remote keys ─────────────────────────────────────────── */
  char ka[900], kb[900];
  memset(ka, 'x', 600); memset(kb, 'x', 600);
  snprintf(ka + 600, sizeof ka - 600, "-tail-A");
  snprintf(kb + 600, sizeof kb - 600, "-tail-B");
  k = intel_sink_make(&db, "long-src", NULL);
  assert(put(&k, ka, "a", NULL) == 1);
  assert(put(&k, kb, "b", NULL) == 1 && "distinct long keys must not collapse");
  assert(put(&k, ka, "a", NULL) == 0 && "the long-key uid is deterministic");
  assert(intel_sink_stored(&k, &exact) == 2);
  intel_sink_free(&k);
  assert(int1("SELECT count(*) FROM intel_items WHERE source_id='long-src'", NULL) == 2);
  char *u = text1("SELECT uid FROM intel_items WHERE source_id='long-src' AND title='a'", NULL);
  size_t ul = strlen(u);
  assert(ul == 511 && u[ul - 65] == '#' && strncmp(u, "long-src|xxxx", 13) == 0);
  for (size_t i = ul - 64; i < ul; i++) assert(strchr("0123456789abcdef", u[i]));
  free(u);
  /* A key that fits is stored verbatim, exactly as before. */
  assert(int1("SELECT count(*) FROM intel_items WHERE uid=?1", U) == 1);

  /* 3-byte characters straddling the cut: "long-src|" is 9 bytes, so the
   * 446-byte prefix ends mid-character for one of these offsets. */
  for (int pad = 0; pad < 3; pad++) {
    char kj[1400]; size_t o = 0;
    for (int i = 0; i < pad; i++) kj[o++] = 'p';
    while (o + 3 < 1300) { memcpy(kj + o, "観", 3); o += 3; }
    kj[o] = 0;
    char uidb[UID_CAP];
    intel_item it = {0}; it.remote_key = kj;
    assert(make_uid(uidb, "long-src", &it) == 0);
    assert(valid_utf8(uidb) && "the readable prefix must stay valid UTF-8");
    assert(strchr(uidb, '#'));
  }
  printf("  long keys: hashed tail, no collapse, deterministic, valid UTF-8: ok\n");

  /* ── 8: statements are finalized with the sink ─────────────────────── */
  free(fetched0); free(fetched1); free(fetched2); free(seen1);
  assert(sqlite3_close(db.h) == SQLITE_OK && "no statement left open by a sink");
  db.h = NULL;
  printf("  sink frees its cached statements; connection closes cleanly: ok\n");

  printf("\nall passed\n");
  return 0;
}
