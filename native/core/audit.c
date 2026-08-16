#include "audit.h"
#include "../third_party/sqlite3.h"
#include <openssl/rand.h>
#include <stdio.h>
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

void audit_write(db_handle *db, const char *tenant_id, const char *user_id,
                 const char *action, const char *target,
                 const char *payload_json) {
  if (!db || !db->h || !tenant_id || !action) return;
  char id[37]; uuid4(id);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO audit_events (id,tenant_id,user_id,action,target,"
        "payload_json,ts) VALUES (?1,?2,?3,?4,?5,?6,datetime('now'))",
        -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant_id, -1, SQLITE_TRANSIENT);
  if (user_id) sqlite3_bind_text(s, 3, user_id, -1, SQLITE_TRANSIENT);
  else         sqlite3_bind_null(s, 3);
  sqlite3_bind_text(s, 4, action, -1, SQLITE_TRANSIENT);
  if (target) sqlite3_bind_text(s, 5, target, -1, SQLITE_TRANSIENT);
  else        sqlite3_bind_null(s, 5);
  sqlite3_bind_text(s, 6, payload_json ? payload_json : "{}", -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
}
