/* test_breach_privacy.c — the breach store's reveal, search and lookup-key
 * guarantees.
 *
 *   - reveal: a reason is required, an audit row (breach.reveal, target uid,
 *     requester + reason) is written for every reveal, and nothing for a
 *     refused or unknown one;
 *   - /api/breach/search pages with offset/limit and states total/shown —
 *     it used to stop at 500 with no total and no way to page;
 *   - identifiers are keyed with HMAC under SECRETS_MASTER_KEY when one is
 *     configured, in keyed shards beside the legacy SHA-1 ones, and lookup
 *     reads BOTH — so turning the key on loses nothing already ingested;
 *   - the corpus root derives from JO_REPO_ROOT, not a developer's laptop. */
#include "../../core/breach_adapter.c"
#include "core/breach_store.h"
#include "core/breach_index.h"
#include "core/db.h"
#include "third_party/cJSON.h"

#include <assert.h>
#include <ctype.h>
#include <openssl/sha.h>
#include <stdlib.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}
static long scalar(db_handle *db, const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}
static long reveals(db_handle *db) {
  return scalar(db, "SELECT COUNT(*) FROM audit_events WHERE action='breach.reveal'");
}
static double num(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  assert(cJSON_IsNumber(v));
  return v->valuedouble;
}
static int file_has(const char *path, const char *needle) {
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  char ln[4096]; int hit = 0;
  while (fgets(ln, sizeof ln, f)) if (strstr(ln, needle)) { hit = 1; break; }
  fclose(f);
  return hit;
}
static void sha1_upper(const char *s, char out[41]) {
  unsigned char h[20]; SHA1((const unsigned char *)s, strlen(s), h);
  for (int i = 0; i < 20; i++) sprintf(out + 2 * i, "%02X", h[i]);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  unsetenv("SECRETS_MASTER_KEY");
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  breach_store_migrate(&db);

  /* ── 1. reveal is audited, and needs a reason ─────────────────────────── */
  exec_ok(&db, "INSERT INTO breach_items(keyid,type,value,source_id,hash)"
               " VALUES('email:AAA|demo','email','alice@example.com','demo','AAA')");
  int st = 0;
  char *b = breach_adapter_reveal_audited(&db, "breach:email:AAA|demo", "u-1",
                                          "op@example.com", "", "127.0.0.1", &st);
  assert(b && st == 400 && strstr(b, "reason_required"));
  free(b);
  b = breach_adapter_reveal_audited(&db, "breach:email:AAA|demo", "u-1", NULL,
                                    "  \t ", NULL, &st);
  assert(b && st == 400 && "whitespace is not a reason");
  free(b);
  b = breach_adapter_reveal_audited(&db, "breach:email:NOPE|demo", "u-1", NULL,
                                    "ticket 42", NULL, &st);
  assert(b && st == 404);
  free(b);
  assert(reveals(&db) == 0 && "a refused or unknown reveal writes no row");

  b = breach_adapter_reveal_audited(&db, "breach:email:AAA|demo", "u-1",
                                    "op@example.com", " case JO-17: DSAR ",
                                    "127.0.0.1", &st);
  assert(b && st == 200);
  free(b);
  assert(reveals(&db) == 1);
  {
    sqlite3_stmt *s;
    assert(sqlite3_prepare_v2(db.h,
      "SELECT tenant_id,user_id,target,payload_json,ip,row_hash FROM audit_events"
      " WHERE action='breach.reveal'", -1, &s, NULL) == SQLITE_OK);
    assert(sqlite3_step(s) == SQLITE_ROW);
    assert(!strcmp((const char *)sqlite3_column_text(s, 0), "platform"));
    assert(!strcmp((const char *)sqlite3_column_text(s, 1), "u-1"));
    assert(!strcmp((const char *)sqlite3_column_text(s, 2), "breach:email:AAA|demo"));
    cJSON *pl = cJSON_Parse((const char *)sqlite3_column_text(s, 3));
    assert(pl);
    assert(!strcmp(cJSON_GetObjectItem(pl, "requester")->valuestring, "u-1"));
    assert(!strcmp(cJSON_GetObjectItem(pl, "reason")->valuestring, "case JO-17: DSAR"));
    assert(!strcmp(cJSON_GetObjectItem(pl, "requester_email")->valuestring, "op@example.com"));
    cJSON_Delete(pl);
    assert(!strcmp((const char *)sqlite3_column_text(s, 4), "127.0.0.1"));
    assert(sqlite3_column_text(s, 5) && "the reveal row is in the hash chain");
    sqlite3_finalize(s);
  }

  /* ── 2. search pages, and says how much it is not showing ─────────────── */
  for (int i = 0; i < 7; i++) {
    char sql[256];
    snprintf(sql, sizeof sql, "INSERT INTO breach_items(keyid,type,value,source_id,hash)"
             " VALUES('username:%d|pg','username','pager%d','pg','H%d')", i, i, i);
    exec_ok(&db, sql);
  }
  exec_ok(&db, "INSERT INTO breach_fts(breach_fts) VALUES('rebuild')");
  char *js = breach_search(&db, NULL, "username", 3, 0);
  cJSON *o = cJSON_Parse(js); free(js);
  assert(num(o, "shown") == 3 && num(o, "total") == 7 && num(o, "offset") == 0);
  assert(cJSON_IsTrue(cJSON_GetObjectItem(o, "has_more")) && num(o, "next_offset") == 3);
  assert(cJSON_IsFalse(cJSON_GetObjectItem(o, "total_is_floor")));
  char first[64];
  snprintf(first, sizeof first, "%s",
    cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(o, "results"), 0), "keyid")->valuestring);
  cJSON_Delete(o);
  js = breach_search(&db, NULL, "username", 3, 3);
  o = cJSON_Parse(js); free(js);
  assert(num(o, "shown") == 3 && cJSON_IsTrue(cJSON_GetObjectItem(o, "has_more")));
  assert(strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(o, "results"), 0),
                "keyid")->valuestring, first) != 0 && "page 2 is not page 1");
  cJSON_Delete(o);
  js = breach_search(&db, NULL, "username", 3, 6);
  o = cJSON_Parse(js); free(js);
  assert(num(o, "shown") == 1 && num(o, "total") == 7);
  assert(cJSON_IsFalse(cJSON_GetObjectItem(o, "has_more")) &&
         !cJSON_GetObjectItem(o, "next_offset"));
  cJSON_Delete(o);
  js = breach_search(&db, "pager3", NULL, 50, 0);
  o = cJSON_Parse(js); free(js);
  assert(num(o, "total") == 1 && num(o, "shown") == 1 && "FTS path counts too");
  cJSON_Delete(o);

  /* ── 3. keyed lookup hash, migration-safe ─────────────────────────────── */
  char bdir[] = "/tmp/jo-breach-priv-XXXXXX";
  assert(mkdtemp(bdir));
  setenv("JO_BREACH_DIR", bdir, 1);
  char lh[65];
  assert(breach_lookup_hash(BT_EMAIL, "bob@example.com", lh) == 0 &&
         "no master key -> no keyed hash (the SHA-1 path is the fallback)");

  char f1[512], f2[512];
  snprintf(f1, sizeof f1, "%s/old.txt", bdir);
  snprintf(f2, sizeof f2, "%s/new.txt", bdir);
  FILE *f = fopen(f1, "w"); fputs("bob@example.com:pw1\n", f); fclose(f);
  f = fopen(f2, "w"); fputs("carol@example.com:pw2\n", f); fclose(f);
  unsigned long long ri = 0, rn = 0;
  assert(breach_index_ingest("old-breach", f1, BT_EMAIL, &ri, &rn, &db, 1, 0) == 0 && rn == 1);

  setenv("SECRETS_MASTER_KEY",
         "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", 1);
  assert(breach_lookup_hash(BT_EMAIL, "carol@example.com", lh) == 1 && strlen(lh) == 64);
  char lh2[65];
  assert(breach_lookup_hash(BT_USERNAME, "carol@example.com", lh2) == 1 &&
         strcmp(lh, lh2) != 0 && "the type is part of the keyed input");
  assert(breach_lookup_hash(BT_PASSWORD, "x", lh2) == 0 && "passwords stay SHA-1");
  assert(breach_index_ingest("new-breach", f2, BT_EMAIL, &ri, &rn, &db, 1, 0) == 0 && rn == 1);

  /* carol is in a KEYED shard and her SHA-1 is nowhere on disk. */
  char ksp[1024], csha[41];
  snprintf(ksp, sizeof ksp, "%s/email/k/%c%c.idx", bdir, tolower(lh[0]), tolower(lh[1]));
  assert(file_has(ksp, lh) == 1);
  sha1_upper("carol@example.com", csha);
  char lsp[1024];
  snprintf(lsp, sizeof lsp, "%s/email/%c%c.idx", bdir, tolower(csha[0]), tolower(csha[1]));
  assert(file_has(lsp, csha) != 1 && "a keyed ingest writes no SHA-1 shard line");

  /* Both are found: bob through the legacy shard, carol through the keyed. */
  cJSON *r = NULL;
  assert(breach_index_lookup(BT_EMAIL, "Bob@Example.com", 0, &r) == 1);
  { char *t = cJSON_PrintUnformatted(r); assert(strstr(t, "old-breach")); free(t); }
  cJSON_Delete(r);
  assert(breach_index_lookup(BT_EMAIL, "carol@example.com", 1, &r) == 1);
  { char *t = cJSON_PrintUnformatted(r);
    assert(strstr(t, "new-breach") && strstr(t, "\"secret\":\"pw2\""));
    free(t); }
  cJSON_Delete(r);

  /* breach_items.lookup_hash: set for the keyed row, NULL for the legacy one. */
  {
    char sql[256];
    snprintf(sql, sizeof sql, "SELECT COUNT(*) FROM breach_items WHERE"
             " value='carol@example.com' AND lookup_hash='%s'", lh);
    assert(scalar(&db, sql) == 1);
    assert(scalar(&db, "SELECT COUNT(*) FROM breach_items WHERE"
                       " value='bob@example.com' AND lookup_hash IS NULL") == 1);
  }

  /* ── 4. the corpus root ───────────────────────────────────────────────── */
  unsetenv("JO_BREACH_DIR");
  setenv("JO_REPO_ROOT", "/srv/jo", 1);
  assert(!strcmp(breach_root_dir(), "/srv/jo/data/breach"));
  unsetenv("JO_REPO_ROOT");
  assert(strstr(breach_root_dir(), "/Users/rayan") == NULL &&
         strstr(breach_root_dir(), "/data/breach") != NULL);

  char cmd[160];
  snprintf(cmd, sizeof cmd, "rm -rf %s", bdir);
  if (system(cmd) != 0) { /* scratch; best effort */ }
  db_close(&db);
  printf("test_breach_privacy: ok\n");
  return 0;
}
