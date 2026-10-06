#include "audit.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* RAND_bytes CAN fail (a provider that failed to load, an exhausted entropy
 * source), and its return was discarded: `b` is then UNINITIALISED STACK. That
 * is wrong twice over for an audit row. The value is audit_events.id, a
 * PRIMARY KEY, and two calls made down the same call path see the same stack
 * bytes and produce the SAME "uuid" — so the second write is the one that gets
 * dropped, and it is an audit record. It is also a disclosure: whatever the
 * previous frame left on the stack (this is called from key, tenant and
 * break-glass handlers) would be rendered as hex into a column the audit API
 * serves back.
 *
 * An audit id is a uniqueness key and not a capability — nothing is authorised
 * by holding one — so failing the write and losing the record is the wrong
 * trade. Fall back to something INITIALISED and still unique: a
 * process-lifetime counter, the clock and the pid. */
static void uuid4_fallback(unsigned char b[16]) {
  static unsigned long long seq;
  unsigned long long n = __atomic_add_fetch(&seq, 1, __ATOMIC_RELAXED);
  unsigned long long parts[2] = { (unsigned long long)time(NULL),
                                  (unsigned long long)getpid() };
  unsigned long long h = 1469598103934665603ULL;      /* FNV-1a */
  for (int i = 0; i < 2; i++)
    for (int k = 0; k < 8; k++) { h ^= (parts[i] >> (k * 8)) & 0xFF;
                                  h *= 1099511628211ULL; }
  for (int i = 0; i < 8; i++) b[i]     = (unsigned char)(n >> (i * 8));
  for (int i = 0; i < 8; i++) b[8 + i] = (unsigned char)(h >> (i * 8));
}

static void uuid4(char out[37]) {
  unsigned char b[16];
  if (RAND_bytes(b, 16) != 1) uuid4_fallback(b);
  b[6] = (b[6] & 0x0F) | 0x40; b[8] = (b[8] & 0x3F) | 0x80;
  snprintf(out, 37,
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
    b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
}

/* ── hash chain ─────────────────────────────────────────────────────────────
 * Moved here from tenantapi.c, where only the VERIFIER had it — which is how
 * the writer never chained anything. */
static void put(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}
void audit_row_hash(const char *id, const char *tenant_id,
                    const char *user_id, const char *action, const char *target,
                    const char *payload_json, const char *ts, const char *ip,
                    const char *ua, const char *prev_hash, long chain_seq,
                    char out[65]) {
  cJSON *o = cJSON_CreateObject();
  put(o, "action", action);
  cJSON_AddNumberToObject(o, "chain_seq", (double)chain_seq);
  put(o, "id", id);
  put(o, "ip", ip);
  put(o, "payload_json", payload_json);
  put(o, "prev_hash", prev_hash);
  put(o, "target", target);
  put(o, "tenant_id", tenant_id);
  put(o, "ts", ts);
  put(o, "ua", ua);
  put(o, "user_id", user_id);
  char *cj = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  unsigned char d[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char *)(cj ? cj : ""), cj ? strlen(cj) : 0, d);
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    snprintf(out + i * 2, 3, "%02x", d[i]);
  free(cj);
}

static int exec0(sqlite3 *h, const char *sql) {
  return sqlite3_exec(h, sql, NULL, NULL, NULL);
}

static void bind_opt(sqlite3_stmt *s, int i, const char *v) {
  if (v) sqlite3_bind_text(s, i, v, -1, SQLITE_TRANSIENT);
  else   sqlite3_bind_null(s, i);
}

/* INSERT one row; prev/row_hash NULL and seq <= 0 write it unchained. */
static int insert_row(sqlite3 *h, const char *id, const char *tenant_id,
                      const char *user_id, const char *action,
                      const char *target, const char *payload,
                      const char *ts, const char *ip, const char *ua,
                      const char *prev, const char *rh, long seq) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
        "INSERT INTO audit_events (id,tenant_id,user_id,action,target,"
        "payload_json,ts,ip,ua,prev_hash,row_hash,chain_seq) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12)",
        -1, &s, NULL) != SQLITE_OK) return SQLITE_ERROR;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant_id, -1, SQLITE_TRANSIENT);
  bind_opt(s, 3, user_id);
  sqlite3_bind_text(s, 4, action, -1, SQLITE_TRANSIENT);
  bind_opt(s, 5, target);
  sqlite3_bind_text(s, 6, payload, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 7, ts, -1, SQLITE_TRANSIENT);
  bind_opt(s, 8, ip);
  bind_opt(s, 9, ua);
  bind_opt(s, 10, prev);
  bind_opt(s, 11, rh);
  if (seq > 0) sqlite3_bind_int64(s, 12, seq); else sqlite3_bind_null(s, 12);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

/* Tail of `tenant_id`'s chain, read inside the caller's write transaction. */
static int chain_tail(sqlite3 *h, const char *tenant_id, char prev[65],
                      long *next_seq) {
  snprintf(prev, 65, "GENESIS");
  *next_seq = 1;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
        "SELECT row_hash,chain_seq FROM audit_events "
        "WHERE tenant_id=?1 AND row_hash IS NOT NULL "
        "ORDER BY chain_seq DESC LIMIT 1", -1, &s, NULL) != SQLITE_OK)
    return -1;
  sqlite3_bind_text(s, 1, tenant_id, -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(s);
  if (rc == SQLITE_ROW) {
    const unsigned char *rh = sqlite3_column_text(s, 0);
    if (rh) snprintf(prev, 65, "%s", (const char *)rh);
    *next_seq = (long)sqlite3_column_int64(s, 1) + 1;
  }
  sqlite3_finalize(s);
  return (rc == SQLITE_ROW || rc == SQLITE_DONE) ? 0 : -1;
}

/* Two writers must never both read tail N and both write N+1. Three layers:
 *   - BEGIN IMMEDIATE takes the database write lock BEFORE the tail is read,
 *     so another connection cannot interleave between read and insert;
 *   - a caller already inside a transaction on this connection (BEGIN would
 *     fail "within a transaction") gets a SAVEPOINT instead — its outer
 *     transaction holds or will take the lock;
 *   - UNIQUE(tenant_id, chain_seq) (schema.sql, idx_audit_chain_seq_uniq)
 *     makes a fork impossible regardless: the losing insert fails with
 *     SQLITE_CONSTRAINT and is retried against the new tail.
 * If the chain still cannot be extended (lock held past busy_timeout, every
 * retry lost), the record is written UNCHAINED rather than dropped: an audit
 * row that exists and says "not in the chain" beats one that never existed,
 * and the verifier counts it. */
#define AUDIT_CHAIN_TRIES 8

int audit_write_ex(db_handle *db, const char *tenant_id, const char *user_id,
                   const char *action, const char *target,
                   const char *payload_json, const char *ip, const char *ua) {
  if (!db || !db->h || !tenant_id || !action) return -1;
  sqlite3 *h = db->h;
  const char *payload = payload_json ? payload_json : "{}";
  char id[37]; uuid4(id);
  /* ts is hashed, so it is fixed here rather than by the column default;
   * same text form as datetime('now'). */
  char ts[24];
  { time_t now = time(NULL); struct tm tm; gmtime_r(&now, &tm);
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tm); }

  for (int attempt = 0; attempt < AUDIT_CHAIN_TRIES; attempt++) {
    int sp = 0;
    if (exec0(h, "BEGIN IMMEDIATE") != SQLITE_OK) {
      /* Autocommit and still refused: the lock was busy past busy_timeout
       * (5 s, db.c). Retrying would multiply that wait on the event loop. */
      if (sqlite3_get_autocommit(h)) break;
      if (exec0(h, "SAVEPOINT audit_chain") != SQLITE_OK) break;
      sp = 1;
    }
    char prev[65]; long seq = 0;
    int rc = SQLITE_ERROR;
    if (chain_tail(h, tenant_id, prev, &seq) == 0) {
      char rh[65];
      audit_row_hash(id, tenant_id, user_id, action, target, payload, ts,
                     ip, ua, prev, seq, rh);
      rc = insert_row(h, id, tenant_id, user_id, action, target, payload,
                      ts, ip, ua, prev, rh, seq);
    }
    if (rc == SQLITE_OK) {
      if (sp) { if (exec0(h, "RELEASE audit_chain") == SQLITE_OK) return 0; }
      else if (exec0(h, "COMMIT") == SQLITE_OK) return 0;
    }
    if (sp) { exec0(h, "ROLLBACK TO audit_chain"); exec0(h, "RELEASE audit_chain"); }
    else if (!sqlite3_get_autocommit(h)) exec0(h, "ROLLBACK");
    /* Only a lost race on (tenant_id, chain_seq) is worth another lap: the
     * tail has moved and re-reading it fixes it. Anything else will fail the
     * same way again. */
    if ((rc & 0xFF) != SQLITE_CONSTRAINT) break;
  }
  fprintf(stderr, "[audit] could not extend the %s chain; writing %s "
                  "UNCHAINED\n", tenant_id, action);
  return insert_row(h, id, tenant_id, user_id, action, target, payload, ts,
                    ip, ua, NULL, NULL, 0) == SQLITE_OK ? 1 : -1;
}

void audit_write(db_handle *db, const char *tenant_id, const char *user_id,
                 const char *action, const char *target,
                 const char *payload_json) {
  (void)audit_write_ex(db, tenant_id, user_id, action, target, payload_json,
                       NULL, NULL);
}
