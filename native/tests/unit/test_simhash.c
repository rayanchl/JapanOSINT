/* tests/unit/test_simhash.c — core/simhash.c: the near-duplicate fingerprint
 * and the clustering it drives.
 *
 * Two layers. (1) The PURE fingerprint (simhash_text, simhash_distance): it
 * must be deterministic, give identical text distance 0, put near-duplicates
 * closer than unrelated text, and return 0 ("unclusterable") below
 * SIMHASH_MIN_TOKENS so thin records never fuse into one false cluster — the
 * failure mode documented in CLAUDE.md for JSON-body collectors. (2) The
 * database layer through the real intel sink: near-duplicate items from two
 * sources share a cluster_id; an unrelated item and a too-short item stay
 * singletons; tenants never cluster into each other; the cluster is the same
 * regardless of arrival order. Text is passed already segmented (space-
 * separated tokens), which is the input simhash_text documents. */
#include "../../core/simhash.c"
#include "../../core/intel.h"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

/* a deterministic pseudo-text of `n` distinct tokens, seeded */
static void words(char *out, size_t cap, int n, unsigned seed) {
  size_t o = 0; out[0] = 0;
  unsigned x = seed * 2654435761u + 12345u;
  for (int i = 0; i < n && o + 16 < cap; i++) {
    x = x * 1664525u + 1013904223u;
    o += (size_t)snprintf(out + o, cap - o, "w%05x%s", (x >> 8) & 0xfffff, i + 1 < n ? " " : "");
  }
}

static char *cluster_of(db_handle *db, const char *uid) {
  sqlite3_stmt *s; char *r = NULL;
  sqlite3_prepare_v2(db->h, "SELECT cluster_id FROM intel_items WHERE uid=?1", -1, &s, NULL);
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL)
    r = strdup((const char *)sqlite3_column_text(s, 0));
  sqlite3_finalize(s);
  return r;
}

static void put(db_handle *db, const char *src, const char *tenant, const char *key,
                const char *title, const char *body) {
  intel_sink k = intel_sink_make(db, src, tenant);
  intel_item it = {0};
  it.remote_key = key; it.title = title; it.body = body; it.record_type = "news";
  it.published_at = "2026-11-01T00:00:00Z";
  assert(k.emit(&k, &it) >= 0);
  intel_sink_free(&k);
}

int main(void) {
  char a[4096], b[4096], c[4096];
  int nt = -1;

  printf("-- distance\n");
  ok(simhash_distance(0, 0) == 0 && simhash_distance(~0ULL, ~0ULL) == 0, "identical fingerprints: distance 0");
  ok(simhash_distance(0, ~0ULL) == 64, "complement: distance 64");
  ok(simhash_distance(1, 0) == 1 && simhash_distance(0x8000000000000000ULL, 0) == 1, "single bit at either end");
  ok(simhash_distance(0xF0F0, 0x0F0F) == simhash_distance(0x0F0F, 0xF0F0), "distance is symmetric");

  printf("-- fingerprint\n");
  words(a, sizeof a, 120, 1);
  uint64_t fa = simhash_text("", a, &nt);
  ok(fa != 0 && nt == 120, "120 distinct tokens: non-zero fingerprint, token count reported");
  ok(simhash_text("", a, NULL) == fa, "out_tokens may be NULL; fingerprint is deterministic");
  /* near duplicate: one token swapped out of 120 */
  strcpy(b, a); b[1] = 'z'; b[2] = 'z';
  uint64_t fb = simhash_text("", b, NULL);
  words(c, sizeof c, 120, 2);
  uint64_t fc = simhash_text("", c, NULL);
  int dnear = simhash_distance(fa, fb), dfar = simhash_distance(fa, fc);
  printf("   near=%d far=%d\n", dnear, dfar);
  ok(dnear <= SIMHASH_MAX_DISTANCE, "one token changed in 120: within the clustering distance");
  ok(dfar > 10 && dfar > dnear + 8, "an unrelated 120-token text is far away (>10 bits)");
  ok(simhash_text("", c, NULL) == fc && simhash_text(NULL, c, NULL) == fc, "NULL and empty title are equivalent");

  printf("-- unclusterable below the token floor\n");
  words(a, sizeof a, SIMHASH_MIN_TOKENS - 1, 3);
  ok(simhash_text("", a, &nt) == 0 && nt == SIMHASH_MIN_TOKENS - 1, "11 distinct tokens: fingerprint 0, count still reported");
  words(a, sizeof a, SIMHASH_MIN_TOKENS, 3);
  ok(simhash_text("", a, &nt) != 0 && nt == SIMHASH_MIN_TOKENS, "12 distinct tokens: clusterable (floor is inclusive)");
  ok(simhash_text(NULL, NULL, &nt) == 0 && nt == 0, "no text at all: 0 tokens, fingerprint 0");
  {
    char rep[2048]; rep[0] = 0;
    for (int i = 0; i < 300; i++) strcat(rep, "same ");
    ok(simhash_text("", rep, &nt) == 0 && nt == 1, "300 repeats of one word count as ONE distinct token: unclusterable");
  }

  printf("-- title and size handling\n");
  words(a, sizeof a, 40, 5);
  uint64_t f_body = simhash_text("", a, NULL);
  char t[256]; words(t, sizeof t, 12, 6);
  uint64_t f_title = simhash_text(t, a, NULL);
  ok(f_title != f_body && simhash_distance(f_title, f_body) > 0, "the title contributes to the fingerprint");
  {
    size_t n = 5000 * 8; char *huge = malloc(n + 1); size_t o = 0;
    for (int i = 0; i < 5000; i++) o += (size_t)snprintf(huge + o, n + 1 - o, "t%05d ", i);
    uint64_t fh = simhash_text("", huge, &nt);
    ok(fh != 0 && nt <= SIMHASH_MAX_TOKENS, "5000-token document is capped at SIMHASH_MAX_TOKENS distinct tokens (bounded work)");
    free(huge);
  }
  ok(simhash_text("a b c d e f g h i j k l m", "", &nt) != 0, "a title alone can carry a clusterable document");

  printf("-- clustering through the sink\n");
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  simhash_ensure_schema(&db);
  simhash_ensure_schema(&db);   /* idempotent */
  ok(1, "ensure_schema twice is safe");

  char story[4096], story2[4096], other[4096], thin[64];
  words(story, sizeof story, 150, 11);
  strcpy(story2, story); story2[1] = 'q'; story2[2] = 'q';     /* one token differs */
  words(other, sizeof other, 150, 99);
  snprintf(thin, sizeof thin, "tiny record");

  put(&db, "src-a", "legacy", "k1", "", story);
  put(&db, "src-b", "legacy", "k2", "", story2);
  put(&db, "src-c", "legacy", "k3", "", other);
  put(&db, "src-d", "legacy", "k4", "", thin);
  put(&db, "src-e", "legacy", "k5", "", thin);
  char *c1 = cluster_of(&db, "src-a|k1"), *c2 = cluster_of(&db, "src-b|k2"), *c3 = cluster_of(&db, "src-c|k3");
  char *c4 = cluster_of(&db, "src-d|k4"), *c5 = cluster_of(&db, "src-e|k5");
  ok(c1 && c2 && !strcmp(c1, c2), "near-duplicate stories from two sources share a cluster_id");
  ok(c3 && (!c1 || strcmp(c3, c1) != 0), "an unrelated story is in a different cluster");
  ok(!(c4 && c5 && !strcmp(c4, c5)), "two identical-but-too-thin records do NOT fuse (below the token floor)");

  /* order independence: the same pair arriving in the opposite order */
  put(&db, "src-g", "legacy", "k7", "", story2);
  put(&db, "src-f", "legacy", "k6", "", story);
  char *c6 = cluster_of(&db, "src-f|k6"), *c7 = cluster_of(&db, "src-g|k7");
  ok(c6 && c7 && !strcmp(c6, c7) && !strcmp(c6, c1), "late arrivals join the existing cluster whichever variant comes first");

  /* tenant isolation: identical text in another tenant must not join legacy's cluster */
  put(&db, "src-h", "tenant-B", "k8", "", story);
  char *c8 = cluster_of(&db, "src-h|k8");
  ok(!(c8 && c1 && !strcmp(c8, c1)), "the same story in another tenant does not cluster into the shared corpus's cluster");

  printf("-- ingest hook is total\n");
  simhash_on_item(&db, "legacy", "no-such|uid", "t", "body words here");
  simhash_on_item(&db, "legacy", NULL, "t", "b");
  simhash_on_item(&db, NULL, "src-a|k1", NULL, NULL);
  ok(1, "hook on a missing uid / NULL arguments returns silently");
  ok(simhash_backfill(&db, 0, NULL) >= 0, "backfill on a fully fingerprinted corpus returns a non-negative count");
  { volatile int cancel = 1;
    ok(simhash_backfill(&db, 0, &cancel) >= 0, "backfill with the cancel flag already set stops cleanly"); }

  free(c1); free(c2); free(c3); free(c4); free(c5); free(c6); free(c7); free(c8);
  db_close(&db);
  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
