/* core/breach_monitor.c — Roadmap 24: breach exposure monitoring.
 *
 * See breach_monitor.h for the design rationale, the required DDL, the
 * ensure_column() note and the three one-line wirings. This file is the
 * mechanics: normalize → hash → store hash only → match → one alert_events row.
 *
 * Shape follows core/casesapi.c (one dispatcher, tenant_ctx for the role gate,
 * malloc'd JSON out, single-exit `goto done` where a parsed body is owned) and
 * core/alertsapi.c for the shared idioms. Two habits here are not stylistic:
 *
 *  · Every SELECT that feeds a write is FULLY drained and finalized before the
 *    first INSERT. SQLite tolerates writing under an open read cursor on the
 *    same connection, but core/alert_deliver.c already settled on "close before
 *    writing" for the alert tables and a scan that interleaves them across a
 *    transaction boundary is exactly the kind of thing that starts returning
 *    partial result sets after an unrelated change.
 *  · Nothing in this file ever calls a scan from inside a transaction it did
 *    not open. The ingest hook runs after breach_store_finish(); a monitor
 *    scan must never be able to roll back the corpus load that produced it.
 */
#include "breach_monitor.h"
#include "audit.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* id windows for the full value_domain backfill. Big enough that the per-batch
 * transaction overhead disappears, small enough that a cancel is honoured
 * promptly and the WAL does not balloon on a billion-row table. */
#define BACKFILL_WINDOW 50000
/* monitors loaded per page in the rescan walk (keyset over id). Kept small
 * because the page lives on the stack and this runs on a mongoose handler
 * thread; 64 * sizeof(mon_row) is ~32 KB. */
#define MONITOR_PAGE 64
#define DEFAULT_MAX_HITS 500

/* ── shared idioms (verbatim from alertsapi.c / casesapi.c) ─────────────── */

static void uuid4(char out[37]) {
  unsigned char b[16]; RAND_bytes(b, 16);
  b[6] = (b[6] & 0x0F) | 0x40; b[8] = (b[8] & 0x3F) | 0x80;
  snprintf(out, 37,
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
    b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
}
static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static char *err(int *st, int code, const char *msg) {
  *st = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}
static void add_str_or_null(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}
static const char *jstr(const cJSON *o, const char *k) {
  const cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}
/* Buffer.from(str,'utf8').toString('base64url') — no padding, +/ → -_ */
static char *b64url(const char *in) {
  static const char T[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  size_t len = strlen(in);
  char *out = malloc(((len + 2) / 3) * 4 + 1);
  if (!out) return NULL;
  size_t o = 0;
  for (size_t i = 0; i < len; i += 3) {
    unsigned a = (unsigned char)in[i];
    unsigned b = i + 1 < len ? (unsigned char)in[i + 1] : 0;
    unsigned c = i + 2 < len ? (unsigned char)in[i + 2] : 0;
    unsigned v = (a << 16) | (b << 8) | c;
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    if (i + 1 < len) out[o++] = T[(v >> 6) & 63];
    if (i + 2 < len) out[o++] = T[v & 63];
  }
  out[o] = 0;
  return out;
}
/* Inverse of b64url(). NULL on an invalid char — a bad cursor is ignored, not
 * an error (same stance as intelapi.c: pagination tokens are advisory). */
static char *b64url_decode(const char *in) {
  static signed char R[256];
  static int init = 0;
  if (!init) {
    for (int i = 0; i < 256; i++) R[i] = -1;
    const char *T =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (int i = 0; i < 64; i++) R[(unsigned char)T[i]] = (signed char)i;
    init = 1;
  }
  size_t len = strlen(in);
  char *out = malloc(len / 4 * 3 + 4);
  if (!out) return NULL;
  size_t o = 0; unsigned v = 0; int bits = 0;
  for (size_t i = 0; i < len; i++) {
    signed char d = R[(unsigned char)in[i]];
    if (d < 0) { free(out); return NULL; }
    v = (v << 6) | (unsigned)d; bits += 6;
    if (bits >= 8) { bits -= 8; out[o++] = (char)((v >> bits) & 0xFF); }
  }
  out[o] = 0;
  return out;
}
/* keyset cursor {"p":<sort_val>,"u":<uid>} — the house encoder. */
static int cursor_split(const char *cur, char *p, size_t pn, char *u, size_t un) {
  if (!cur || !*cur) return 0;
  char *dec = b64url_decode(cur);
  if (!dec) return 0;
  cJSON *j = cJSON_Parse(dec); free(dec);
  if (!j) return 0;
  cJSON *jp = cJSON_GetObjectItem(j, "p"), *ju = cJSON_GetObjectItem(j, "u");
  int ok = 0;
  if (cJSON_IsString(jp) && cJSON_IsString(ju)) {
    snprintf(p, pn, "%s", jp->valuestring);
    snprintf(u, un, "%s", ju->valuestring);
    ok = 1;
  }
  cJSON_Delete(j);
  return ok;
}
static char *cursor_make(const char *p, const char *u) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "p", p ? p : "");
  cJSON_AddStringToObject(o, "u", u ? u : "");
  char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o);
  char *e = b64url(j); free(j); return e;
}
static int clamp_limit(int n) {
  if (n <= 0) n = 50;
  if (n < 1) n = 1;
  if (n > 200) n = 200;
  return n;
}

/* ── identifier → the exact key breach_items is written with ────────────── */

/* SHA-1 UPPERCASE hex. Case matters: core/breach_index.c's sha1_hex() emits
 * uppercase and that string is what breach_store_put() stores in
 * breach_items.hash, so a lowercase monitor hash would silently never match. */
static void sha1_hex_upper(const char *s, size_t n, char out[41]) {
  unsigned char h[SHA_DIGEST_LENGTH];
  SHA1((const unsigned char *)s, n, h);
  static const char *H = "0123456789ABCDEF";
  for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
    out[i * 2] = H[h[i] >> 4]; out[i * 2 + 1] = H[h[i] & 15];
  }
  out[40] = 0;
}

static int kind_valid(const char *k) {
  return k && (!strcmp(k, "email") || !strcmp(k, "domain") ||
               !strcmp(k, "username") || !strcmp(k, "phone"));
}

/* Normalize exactly the way core/breach_index.c's norm() does for the matching
 * type — that symmetry IS the feature. Returns a malloc'd string, or NULL when
 * the input cannot be a valid identifier of that kind (the caller turns that
 * into a 400). Never mutates `in`. */
static char *norm_ident(const char *kind, const char *in) {
  if (!in) return NULL;
  while (*in == ' ' || *in == '\t' || *in == '\r' || *in == '\n') in++;
  size_t L = strlen(in);
  while (L && (in[L-1]==' '||in[L-1]=='\t'||in[L-1]=='\r'||in[L-1]=='\n')) L--;
  if (!L || L > 320) return NULL;
  char *o = malloc(L + 1);
  if (!o) return NULL;
  memcpy(o, in, L); o[L] = 0;

  if (!strcmp(kind, "phone")) {
    /* "+ then digits only" — identical reduction to breach_index.c's BT_PHONE
     * branch, so "+81 90-1234-5678" and "+819012345678" hash the same. */
    char *w = o; int plus = (o[0] == '+'); int digits = 0;
    if (plus) *w++ = '+';
    for (size_t i = 0; i < L; i++)
      if (isdigit((unsigned char)o[i])) { *w++ = o[i]; digits++; }
    *w = 0;
    if (digits < 7 || digits > 15) { free(o); return NULL; }
    return o;
  }

  for (size_t i = 0; i < L; i++) o[i] = (char)tolower((unsigned char)o[i]);
  for (size_t i = 0; i < L; i++)
    if (isspace((unsigned char)o[i])) { free(o); return NULL; }

  if (!strcmp(kind, "email")) {
    /* Deliberately NOT gmail-canonicalized — see the header. Only structural
     * validation: one '@', something either side, a dot in the host. */
    char *at = strchr(o, '@');
    if (!at || at == o || !at[1] || strchr(at + 1, '@')) { free(o); return NULL; }
    const char *dot = strchr(at + 1, '.');
    if (!dot || dot == at + 1 || !dot[1]) { free(o); return NULL; }
    return o;
  }
  if (!strcmp(kind, "domain")) {
    /* Accept "@acme.co.jp", "*.acme.co.jp" and "acme.co.jp" as the same thing;
     * the stored form is always the bare host, because that is what the
     * value_domain column holds. */
    char *p = o;
    if (*p == '@') p++;
    if (p[0] == '*' && p[1] == '.') p += 2;
    if (p != o) memmove(o, p, strlen(p) + 1);
    if (strchr(o, '@') || strchr(o, '/')) { free(o); return NULL; }
    const char *dot = strchr(o, '.');
    if (!o[0] || !dot || dot == o || !dot[1]) { free(o); return NULL; }
    return o;
  }
  /* username: lowercase + trim, nothing else. */
  return o;
}

/* ── schema dependencies (see header: the index CANNOT live in schema.sql) ─ */

static pthread_mutex_t g_mig_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_migrated = 0;

void breach_monitor_migrate(db_handle *db) {
  if (!db || !db->h) return;
  pthread_mutex_lock(&g_mig_mu);
  if (g_migrated) { pthread_mutex_unlock(&g_mig_mu); return; }
  /* Order is load-bearing: the column must exist before the index over it. */
  ensure_column(db, "breach_items", "value_domain", "TEXT");
  sqlite3_exec(db->h,
    "CREATE INDEX IF NOT EXISTS idx_breach_items_value_domain"
    "  ON breach_items(value_domain);"
    "CREATE INDEX IF NOT EXISTS idx_breach_items_hash ON breach_items(hash);",
    NULL, NULL, NULL);
  g_migrated = 1;
  pthread_mutex_unlock(&g_mig_mu);
}

static int cancelled(volatile int *cancel) { return cancel && *cancel; }

static long long count_scalar(db_handle *db, const char *sql, const char *bind1) {
  sqlite3_stmt *s;
  long long n = 0;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) return 0;
  if (bind1) sqlite3_bind_text(s, 1, bind1, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

static int any_domain_monitor(db_handle *db) {
  return count_scalar(db,
    "SELECT 1 FROM breach_monitors WHERE kind='domain' LIMIT 1", NULL) != 0;
}

/* ── value_domain backfill ─────────────────────────────────────────────── */

/* Narrow path: only the rows of the source that was just ingested. Uses
 * breach_items_source_idx(source_id, id), so it costs the size of one breach,
 * not the size of the corpus. */
static void backfill_source(db_handle *db, const char *source_id) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE breach_items "
        "   SET value_domain = lower(substr(value, instr(value,'@')+1)) "
        " WHERE source_id=?1 AND type='email' AND value IS NOT NULL "
        "   AND value_domain IS NULL AND instr(value,'@')>0",
        -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

/* Wide path: one ordered pass over breach_items in id windows.
 *
 * The obvious alternative — `UPDATE ... WHERE value_domain IS NULL` in a loop
 * with a LIMIT — re-scans the already-populated prefix on every batch and is
 * quadratic in the number of batches; on a corpus this size that is the
 * difference between minutes and never finishing. Windowing on the integer
 * primary key touches each row once. Each window is its own implicit
 * transaction, so a cancel or a crash loses at most one window and the next
 * run simply finds the remaining NULLs. */
static void backfill_all(db_handle *db, volatile int *cancel) {
  long long max_id = count_scalar(db,
    "SELECT COALESCE(MAX(id),0) FROM breach_items", NULL);
  if (max_id <= 0) return;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE breach_items "
        "   SET value_domain = lower(substr(value, instr(value,'@')+1)) "
        " WHERE id > ?1 AND id <= ?2 AND type='email' AND value IS NOT NULL "
        "   AND value_domain IS NULL AND instr(value,'@')>0",
        -1, &s, NULL) != SQLITE_OK) return;
  for (long long lo = 0; lo < max_id; lo += BACKFILL_WINDOW) {
    if (cancelled(cancel)) break;
    sqlite3_reset(s);
    sqlite3_bind_int64(s, 1, lo);
    sqlite3_bind_int64(s, 2, lo + BACKFILL_WINDOW);
    if (sqlite3_step(s) != SQLITE_DONE) break;
  }
  sqlite3_finalize(s);
}

/* ── the monitor → alert_events bridge ─────────────────────────────────── */

typedef struct {
  char id[64], tenant_id[64], kind[16], hash[48], domain[256], rule_id[64];
} mon_row;

static int max_hits(void) {
  const char *e = getenv("JO_BREACH_MONITOR_MAX_HITS");
  long v = (e && *e) ? strtol(e, NULL, 10) : DEFAULT_MAX_HITS;
  if (v < 1) v = 1;
  if (v > 100000) v = 100000;
  return (int)v;
}

/* Write the alert_events row alert_deliver.c's enqueue_pending() drains. The
 * INSERT ... SELECT ... WHERE NOT EXISTS is the dedup: alert_events has no
 * UNIQUE constraint (and this module does not get to add one to a table it
 * does not own), so idempotence rides on idx_alert_events_rule_item. Returns 1
 * when a row was actually written. */
static int emit_event(db_handle *db, const mon_row *m, const char *keyid) {
  const char *rule = m->rule_id[0] ? m->rule_id : m->id;
  char uid[224], reason[128], eid[37];
  snprintf(uid, sizeof uid, "breach:%s", keyid);
  snprintf(reason, sizeof reason, "breach_monitor:%s", m->id);
  uuid4(eid);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO alert_events (id,tenant_id,rule_id,item_uid,matched_at,"
        "delivered_channels_json,suppressed,reason) "
        "SELECT ?1,?2,?3,?4,datetime('now'),'[]',0,?5 "
        " WHERE NOT EXISTS (SELECT 1 FROM alert_events "
        "                    WHERE rule_id=?3 AND item_uid=?4 AND tenant_id=?2)",
        -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, eid,         -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, m->tenant_id,-1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, rule,        -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 4, uid,         -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 5, reason,      -1, SQLITE_TRANSIENT);
  int wrote = (sqlite3_step(s) == SQLITE_DONE) ? sqlite3_changes(db->h) : 0;
  sqlite3_finalize(s);
  return wrote > 0 ? 1 : 0;
}

static void touch_checked(db_handle *db, const char *monitor_id) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE breach_monitors SET last_checked_at=datetime('now') WHERE id=?1",
        -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, monitor_id, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

/* Match ONE monitor. `source_id` non-empty narrows to the breach just ingested
 * (the hook case); NULL/"" scans the whole corpus (the rescan case).
 *
 * The candidate keyids are read out in full and the statement finalized BEFORE
 * any INSERT — see the file header. The cap is what stops a domain monitor on a
 * heavily-breached host from writing a six-figure inbox on its first scan. */
static long long scan_monitor(db_handle *db, const mon_row *m,
                              const char *source_id, volatile int *cancel) {
  int is_domain = strcmp(m->kind, "domain") == 0;
  const char *key = is_domain ? m->domain : m->hash;
  if (!key[0]) return 0;
  int cap = max_hits();
  int scoped = (source_id && *source_id);

  char sql[256];
  snprintf(sql, sizeof sql,
    "SELECT keyid FROM breach_items WHERE %s=?1%s ORDER BY id DESC LIMIT ?2",
    is_domain ? "value_domain" : "hash",
    scoped ? " AND source_id=?3" : "");

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, 2, cap);
  if (scoped) sqlite3_bind_text(s, 3, source_id, -1, SQLITE_TRANSIENT);

  char **keys = calloc((size_t)cap, sizeof *keys);
  if (!keys) { sqlite3_finalize(s); return 0; }
  int n = 0;
  while (n < cap && sqlite3_step(s) == SQLITE_ROW) {
    const char *k = ctext(s, 0);
    if (!k) continue;
    keys[n] = strdup(k);
    if (!keys[n]) break;
    n++;
  }
  sqlite3_finalize(s);

  long long hits = 0;
  if (n > 0 && !cancelled(cancel)) {
    sqlite3_exec(db->h, "BEGIN", NULL, NULL, NULL);
    for (int i = 0; i < n; i++) hits += emit_event(db, m, keys[i]);
    sqlite3_exec(db->h, "COMMIT", NULL, NULL, NULL);
  }
  for (int i = 0; i < n; i++) free(keys[i]);
  free(keys);
  touch_checked(db, m->id);
  return hits;
}

static void mon_from_stmt(sqlite3_stmt *s, mon_row *m) {
  /* cols: id,tenant_id,kind,value_hash,value_domain,rule_id */
  const char *v;
  memset(m, 0, sizeof *m);
  if ((v = ctext(s, 0))) snprintf(m->id,        sizeof m->id,        "%s", v);
  if ((v = ctext(s, 1))) snprintf(m->tenant_id, sizeof m->tenant_id, "%s", v);
  if ((v = ctext(s, 2))) snprintf(m->kind,      sizeof m->kind,      "%s", v);
  if ((v = ctext(s, 3))) snprintf(m->hash,      sizeof m->hash,      "%s", v);
  if ((v = ctext(s, 4))) snprintf(m->domain,    sizeof m->domain,    "%s", v);
  if ((v = ctext(s, 5))) snprintf(m->rule_id,   sizeof m->rule_id,   "%s", v);
}

static const char *MON_COLS =
  "SELECT id,tenant_id,kind,value_hash,value_domain,rule_id FROM breach_monitors ";

/* ── public: ingest hook ───────────────────────────────────────────────── */

long long breach_monitor_scan_new(db_handle *db, const char *source_id,
                                  volatile int *cancel) {
  if (!db || !db->h) return -1;
  breach_monitor_migrate(db);

  /* Idle deploys pay one indexed probe and nothing else. */
  if (!count_scalar(db, "SELECT 1 FROM breach_monitors LIMIT 1", NULL)) return 0;
  if (source_id && *source_id && any_domain_monitor(db))
    backfill_source(db, source_id);

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, MON_COLS, -1, &s, NULL) != SQLITE_OK) return 0;

  mon_row *mons = NULL;
  int n = 0, capacity = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    if (n == capacity) {
      int nc = capacity ? capacity * 2 : 64;
      mon_row *nm = realloc(mons, (size_t)nc * sizeof *nm);
      if (!nm) break;
      mons = nm; capacity = nc;
    }
    mon_from_stmt(s, &mons[n]);
    if (mons[n].id[0] && mons[n].tenant_id[0]) n++;
  }
  sqlite3_finalize(s);

  long long hits = 0;
  for (int i = 0; i < n && !cancelled(cancel); i++)
    hits += scan_monitor(db, &mons[i], source_id, cancel);
  free(mons);
  return hits;
}

/* ── public: full rescan ───────────────────────────────────────────────── */

int breach_monitor_rescan_all(db_handle *shared_db, const char *tenant_id,
                              const char *monitor_id, volatile int *cancel,
                              long long *out_monitors, long long *out_hits) {
  if (out_monitors) *out_monitors = 0;
  if (out_hits) *out_hits = 0;
  if (!shared_db || !shared_db->h) return -1;
  breach_monitor_migrate(shared_db);          /* DDL stays on the primary */

  /* Own connection for the scan itself. Unlike the ingest hook — which is
   * already reached on breach_jobs.c's private handle — this entry point is
   * driven straight from the HTTP surface below on the caller's shared
   * handle, and it is not one transaction but a BEGIN/COMMIT per monitor over
   * an unbounded corpus walk (scan_monitor). Interleaved with a detached
   * thread's transaction on that same handle, one side's ROLLBACK discards
   * the other's rows. */
  db_handle own;
  db_handle *db = db_worker_open(&own, shared_db);

  if (any_domain_monitor(db)) backfill_all(db, cancel);

  int scope_t = (tenant_id && *tenant_id), scope_m = (monitor_id && *monitor_id);
  char sql[512];
  snprintf(sql, sizeof sql,
    "%sWHERE id > ?1%s%s ORDER BY id LIMIT %d", MON_COLS,
    scope_t ? " AND tenant_id=?2" : "",
    scope_m ? " AND id=?3" : "", MONITOR_PAGE);

  char after[64] = "";
  long long seen = 0, hits = 0;
  for (;;) {
    if (cancelled(cancel)) break;
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) break;
    sqlite3_bind_text(s, 1, after, -1, SQLITE_TRANSIENT);
    if (scope_t) sqlite3_bind_text(s, 2, tenant_id, -1, SQLITE_TRANSIENT);
    if (scope_m) sqlite3_bind_text(s, 3, monitor_id, -1, SQLITE_TRANSIENT);

    mon_row page[MONITOR_PAGE];
    int n = 0;
    while (n < MONITOR_PAGE && sqlite3_step(s) == SQLITE_ROW) {
      mon_from_stmt(s, &page[n]);
      if (page[n].id[0] && page[n].tenant_id[0]) n++;
    }
    sqlite3_finalize(s);
    if (n == 0) break;

    for (int i = 0; i < n && !cancelled(cancel); i++) {
      hits += scan_monitor(db, &page[i], NULL, cancel);
      seen++;
    }
    snprintf(after, sizeof after, "%s", page[n - 1].id);
    if (n < MONITOR_PAGE) break;
  }
  if (out_monitors) *out_monitors = seen;
  if (out_hits) *out_hits = hits;
  db_worker_close(&own);       /* no-op if we fell back to the shared handle */
  return 0;
}

/* ── HTTP surface ──────────────────────────────────────────────────────── */

static int can_write(const tenant_ctx *t) {
  return t && (!strcmp(t->role, "owner") || !strcmp(t->role, "admin") ||
               !strcmp(t->role, "analyst"));
}

/* One monitor row → JSON. The identifier is NOT here and must never be: the
 * only fingerprint exposed is the same 10-char SHA-1 prefix breach_index_keyid()
 * already publishes as a non-reversible key. */
static cJSON *mon_json(sqlite3_stmt *s) {
  /* cols: id,kind,label,rule_id,value_domain,created_by,created_at,
   *       last_checked_at,value_hash */
  cJSON *o = cJSON_CreateObject();
  add_str_or_null(o, "id",              ctext(s, 0));
  add_str_or_null(o, "kind",            ctext(s, 1));
  add_str_or_null(o, "label",           ctext(s, 2));
  add_str_or_null(o, "rule_id",         ctext(s, 3));
  add_str_or_null(o, "value_domain",    ctext(s, 4));
  add_str_or_null(o, "created_by",      ctext(s, 5));
  add_str_or_null(o, "created_at",      ctext(s, 6));
  add_str_or_null(o, "last_checked_at", ctext(s, 7));
  const char *h = ctext(s, 8);
  char pfx[11] = {0};
  if (h) snprintf(pfx, sizeof pfx, "%.10s", h);
  add_str_or_null(o, "hash_prefix", pfx[0] ? pfx : NULL);
  cJSON_AddBoolToObject(o, "delivers",
                        (ctext(s, 3) != NULL));  /* has an alert_rules channel set */
  return o;
}

static const char *API_COLS =
  "SELECT id,kind,label,rule_id,value_domain,created_by,created_at,"
  "last_checked_at,value_hash FROM breach_monitors ";

/* Load one monitor into a mon_row, tenant-scoped. 1 on success. */
static int load_mon(db_handle *db, const char *tid, const char *id, mon_row *m) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT id,tenant_id,kind,value_hash,value_domain,rule_id "
        "FROM breach_monitors WHERE id=?1 AND tenant_id=?2",
        -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, id,  -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tid, -1, SQLITE_TRANSIENT);
  int ok = 0;
  if (sqlite3_step(s) == SQLITE_ROW) { mon_from_stmt(s, m); ok = 1; }
  sqlite3_finalize(s);
  return ok;
}

/* {data:{monitor}} (+ hit_count when `detail`, + meta.initial_hits when the
 * caller just created it). 404 when the row is not this tenant's. */
static char *one_monitor(db_handle *db, const char *tid, const char *id,
                         int code, int detail, const long long *initial_hits,
                         int *st) {
  char sql[512];
  snprintf(sql, sizeof sql, "%sWHERE id=?1 AND tenant_id=?2", API_COLS);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK)
    return err(st, 500, "server_error");
  sqlite3_bind_text(s, 1, id,  -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tid, -1, SQLITE_TRANSIENT);
  char *out;
  if (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *m = mon_json(s);
    sqlite3_finalize(s);
    if (detail) {
      mon_row mr;
      long long hc = 0;
      if (load_mon(db, tid, id, &mr)) {
        int is_domain = strcmp(mr.kind, "domain") == 0;
        const char *key = is_domain ? mr.domain : mr.hash;
        if (key[0])
          hc = count_scalar(db, is_domain
                 ? "SELECT COUNT(*) FROM breach_items WHERE value_domain=?1"
                 : "SELECT COUNT(*) FROM breach_items WHERE hash=?1", key);
      }
      cJSON_AddItemToObject(m, "hit_count", cJSON_CreateNumber((double)hc));
    }
    cJSON *w = cJSON_CreateObject();
    cJSON_AddItemToObject(w, "data", m);
    if (initial_hits) {
      cJSON *meta = cJSON_CreateObject();
      cJSON_AddItemToObject(meta, "initial_hits",
                            cJSON_CreateNumber((double)*initial_hits));
      cJSON_AddItemToObject(w, "meta", meta);
    }
    out = cJSON_PrintUnformatted(w); cJSON_Delete(w); *st = code;
  } else {
    sqlite3_finalize(s);
    out = err(st, 404, "not_found");
  }
  return out;
}

/* GET /api/breach-monitors — keyset over (created_at DESC, id ASC). */
static char *list_monitors(db_handle *db, const char *tid, const char *cursor,
                           int limit, int *st) {
  int lim = clamp_limit(limit);
  char cp[64] = "", cu[128] = "";
  int have = cursor_split(cursor, cp, sizeof cp, cu, sizeof cu);
  char sql[640];
  snprintf(sql, sizeof sql,
    "%sWHERE tenant_id=?1%s ORDER BY created_at DESC, id ASC LIMIT ?4", API_COLS,
    have ? " AND (created_at < ?2 OR (created_at = ?2 AND id > ?3))" : "");
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK)
    return err(st, 500, "server_error");
  sqlite3_bind_text(s, 1, tid, -1, SQLITE_TRANSIENT);
  if (have) {
    sqlite3_bind_text(s, 2, cp, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, cu, -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int(s, 4, lim + 1);

  cJSON *arr = cJSON_CreateArray();
  int n = 0;
  char last_p[64] = "", last_u[128] = "";
  int more = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    if (n == lim) { more = 1; break; }
    const char *ca = ctext(s, 6), *rid = ctext(s, 0);
    snprintf(last_p, sizeof last_p, "%s", ca ? ca : "");
    snprintf(last_u, sizeof last_u, "%s", rid ? rid : "");
    cJSON_AddItemToArray(arr, mon_json(s));
    n++;
  }
  sqlite3_finalize(s);

  cJSON *page = cJSON_CreateObject();
  char *next = more ? cursor_make(last_p, last_u) : NULL;
  cJSON_AddItemToObject(page, "next_cursor",
                        next ? cJSON_CreateString(next) : cJSON_CreateNull());
  free(next);
  cJSON_AddItemToObject(page, "limit", cJSON_CreateNumber(lim));
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddItemToObject(meta, "count", cJSON_CreateNumber(n));
  cJSON_AddStringToObject(meta, "note",
    "identifiers are stored hashed and are never returned");
  cJSON *w = cJSON_CreateObject();
  cJSON_AddItemToObject(w, "data", arr);
  cJSON_AddItemToObject(w, "page", page);
  cJSON_AddItemToObject(w, "meta", meta);
  char *out = cJSON_PrintUnformatted(w); cJSON_Delete(w);
  *st = 200;
  return out;
}

/* GET /api/breach-monitors/:id/hits — catalog metadata + the synthetic uid.
 * Deliberately no `value` and no secret: the uid resolves through
 * intelapi_item_by_uid() → breach_adapter_item_by_uid(), which is the ONE place
 * breach redaction is implemented. */
static char *monitor_hits(db_handle *db, const char *tid, const char *id,
                          const char *cursor, int limit, int *st) {
  mon_row m;
  if (!load_mon(db, tid, id, &m)) return err(st, 404, "not_found");
  breach_monitor_migrate(db);

  int lim = clamp_limit(limit);
  int is_domain = strcmp(m.kind, "domain") == 0;
  const char *key = is_domain ? m.domain : m.hash;
  long long after = 0;
  if (cursor && *cursor) after = strtoll(cursor, NULL, 10);
  if (after <= 0) after = 9223372036854775807LL;

  char sql[768];
  snprintf(sql, sizeof sql,
    "SELECT bi.id,bi.keyid,bi.type,bi.source_id,bi.has_secret,bi.count,"
    "       bi.first_seen,bm.name,bm.title,bm.domain,bm.breach_date,"
    "       bm.added_date,bm.pwn_count,bm.data_classes_json,bm.verified,"
    "       bm.sensitive "
    "  FROM breach_items bi "
    "  LEFT JOIN breach_meta bm ON bm.breach_id = bi.source_id "
    " WHERE bi.%s=?1 AND bi.id < ?2 ORDER BY bi.id DESC LIMIT ?3",
    is_domain ? "value_domain" : "hash");

  sqlite3_stmt *s;
  if (!key[0] || sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    if (!key[0]) { /* structurally unmatchable monitor — empty page, not 500 */
      cJSON *w = cJSON_CreateObject();
      cJSON_AddItemToObject(w, "data", cJSON_CreateArray());
      cJSON *pg = cJSON_CreateObject();
      cJSON_AddItemToObject(pg, "next_cursor", cJSON_CreateNull());
      cJSON_AddItemToObject(pg, "limit", cJSON_CreateNumber(lim));
      cJSON_AddItemToObject(w, "page", pg);
      cJSON_AddItemToObject(w, "meta", cJSON_CreateObject());
      char *o = cJSON_PrintUnformatted(w); cJSON_Delete(w);
      *st = 200; return o;
    }
    return err(st, 500, "server_error");
  }
  sqlite3_bind_text (s, 1, key, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(s, 2, after);
  sqlite3_bind_int  (s, 3, lim + 1);

  cJSON *arr = cJSON_CreateArray();
  int n = 0, more = 0;
  long long last_id = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    if (n == lim) { more = 1; break; }
    last_id = sqlite3_column_int64(s, 0);
    const char *keyid = ctext(s, 1);
    cJSON *o = cJSON_CreateObject();
    char uid[224];
    snprintf(uid, sizeof uid, "breach:%s", keyid ? keyid : "");
    cJSON_AddStringToObject(o, "uid", uid);   /* open via GET /api/intel/items/:uid */
    add_str_or_null(o, "type",       ctext(s, 2));
    add_str_or_null(o, "breach_id",  ctext(s, 3));
    cJSON_AddBoolToObject(o, "has_secret", sqlite3_column_int(s, 4) != 0);
    cJSON_AddItemToObject(o, "count",
                          cJSON_CreateNumber((double)sqlite3_column_int64(s, 5)));
    add_str_or_null(o, "first_seen", ctext(s, 6));
    cJSON *b = cJSON_CreateObject();
    add_str_or_null(b, "name",        ctext(s, 7));
    add_str_or_null(b, "title",       ctext(s, 8));
    add_str_or_null(b, "domain",      ctext(s, 9));
    add_str_or_null(b, "breach_date", ctext(s, 10));
    add_str_or_null(b, "added_date",  ctext(s, 11));
    cJSON_AddItemToObject(b, "pwn_count",
      sqlite3_column_type(s, 12) == SQLITE_NULL
        ? cJSON_CreateNull()
        : cJSON_CreateNumber((double)sqlite3_column_int64(s, 12)));
    const char *dc = ctext(s, 13);
    cJSON *dcj = dc ? cJSON_Parse(dc) : NULL;
    cJSON_AddItemToObject(b, "data_classes", dcj ? dcj : cJSON_CreateArray());
    cJSON_AddBoolToObject(b, "verified",  sqlite3_column_int(s, 14) != 0);
    cJSON_AddBoolToObject(b, "sensitive", sqlite3_column_int(s, 15) != 0);
    cJSON_AddItemToObject(o, "breach", b);
    cJSON_AddItemToArray(arr, o);
    n++;
  }
  sqlite3_finalize(s);

  cJSON *page = cJSON_CreateObject();
  if (more) {
    char nc[32]; snprintf(nc, sizeof nc, "%lld", last_id);
    cJSON_AddStringToObject(page, "next_cursor", nc);
  } else {
    cJSON_AddItemToObject(page, "next_cursor", cJSON_CreateNull());
  }
  cJSON_AddItemToObject(page, "limit", cJSON_CreateNumber(lim));
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "monitor_id", m.id);
  cJSON_AddStringToObject(meta, "kind", m.kind);
  cJSON_AddItemToObject(meta, "count", cJSON_CreateNumber(n));
  cJSON_AddStringToObject(meta, "note",
    "open a hit via GET /api/intel/items/<uid>; secrets stay redacted there");
  cJSON *w = cJSON_CreateObject();
  cJSON_AddItemToObject(w, "data", arr);
  cJSON_AddItemToObject(w, "page", page);
  cJSON_AddItemToObject(w, "meta", meta);
  char *out = cJSON_PrintUnformatted(w); cJSON_Delete(w);
  *st = 200;
  return out;
}

/* audit payload — kind/label/rule/domain and a non-reversible hash prefix.
 * The plaintext identifier is not written here for the same reason it is not
 * written to breach_monitors: audit_events is a long-lived, exportable table. */
static char *audit_payload(const char *kind, const char *label,
                           const char *rule_id, const char *domain,
                           const char *hash) {
  cJSON *p = cJSON_CreateObject();
  add_str_or_null(p, "kind", kind);
  add_str_or_null(p, "label", label);
  add_str_or_null(p, "rule_id", rule_id);
  add_str_or_null(p, "value_domain", domain);
  if (hash) {
    char pfx[11]; snprintf(pfx, sizeof pfx, "%.10s", hash);
    cJSON_AddStringToObject(p, "hash_prefix", pfx);
  }
  char *j = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  return j;
}

char *breach_monitors_api(db_handle *db, const tenant_ctx *t,
                          const char *method, const char *seg, const char *act,
                          const char *body, const char *cursor, int limit,
                          int *status) {
  if (!db || !db->h || !t) return err(status, 500, "server_error");
  if (!seg) seg = "";
  if (!act) act = "";
  if (!method) method = "GET";
  const char *tid = t->tenant_id;
  if (!tid || !tid[0]) return err(status, 400, "tenant_required");
  breach_monitor_migrate(db);

  int is_get = !strcmp(method, "GET"),  is_post = !strcmp(method, "POST"),
      is_del = !strcmp(method, "DELETE");

  /* ── collection ── */
  if (!seg[0]) {
    if (is_get) return list_monitors(db, tid, cursor, limit, status);
    if (!is_post) return err(status, 405, "method_not_allowed");
    if (!can_write(t)) return err(status, 403, "forbidden");

    cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
    char *out = NULL, *nv = NULL, *pl = NULL;
    if (!jb || !cJSON_IsObject(jb)) { out = err(status, 400, "body_required"); goto post_done; }
    {
      const char *kind = jstr(jb, "kind");
      const char *raw  = jstr(jb, "value");
      const char *label = jstr(jb, "label");
      const char *rule = jstr(jb, "rule_id");
      if (!kind_valid(kind)) { out = err(status, 400, "invalid_kind"); goto post_done; }
      if (!raw)              { out = err(status, 400, "value_required"); goto post_done; }
      if (label && strlen(label) > 200) { out = err(status, 400, "label_too_long"); goto post_done; }
      nv = norm_ident(kind, raw);
      if (!nv) { out = err(status, 400, "invalid_value"); goto post_done; }

      /* A rule_id is what gives the monitor CHANNELS; it must be this tenant's
       * or delivery would silently resolve someone else's webhook. */
      if (rule) {
        sqlite3_stmt *s;
        int ok = 0;
        if (sqlite3_prepare_v2(db->h,
              "SELECT 1 FROM alert_rules WHERE id=?1 AND tenant_id=?2",
              -1, &s, NULL) == SQLITE_OK) {
          sqlite3_bind_text(s, 1, rule, -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(s, 2, tid,  -1, SQLITE_TRANSIENT);
          ok = (sqlite3_step(s) == SQLITE_ROW);
          sqlite3_finalize(s);
        }
        if (!ok) { out = err(status, 400, "unknown_rule_id"); goto post_done; }
      }

      char hash[41];
      sha1_hex_upper(nv, strlen(nv), hash);
      const char *dom = !strcmp(kind, "domain") ? nv : NULL;

      /* No UNIQUE index (see header) — the duplicate answer is more useful as a
       * 409 naming the existing row than as a constraint failure. */
      {
        sqlite3_stmt *s;
        char existing[64] = "";
        if (sqlite3_prepare_v2(db->h,
              "SELECT id FROM breach_monitors "
              "WHERE tenant_id=?1 AND kind=?2 AND value_hash=?3 LIMIT 1",
              -1, &s, NULL) == SQLITE_OK) {
          sqlite3_bind_text(s, 1, tid,  -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(s, 2, kind, -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(s, 3, hash, -1, SQLITE_TRANSIENT);
          if (sqlite3_step(s) == SQLITE_ROW) {
            const char *e = ctext(s, 0);
            if (e) snprintf(existing, sizeof existing, "%s", e);
          }
          sqlite3_finalize(s);
        }
        if (existing[0]) {
          cJSON *o = cJSON_CreateObject();
          cJSON_AddStringToObject(o, "error", "already_monitored");
          cJSON_AddStringToObject(o, "id", existing);
          out = cJSON_PrintUnformatted(o); cJSON_Delete(o);
          *status = 409;
          goto post_done;
        }
      }

      char nid[37]; uuid4(nid);
      sqlite3_stmt *s;
      if (sqlite3_prepare_v2(db->h,
            "INSERT INTO breach_monitors "
            "(id,tenant_id,kind,value_hash,value_domain,label,rule_id,created_by) "
            "VALUES (?1,?2,?3,?4,?5,?6,?7,?8)", -1, &s, NULL) != SQLITE_OK) {
        out = err(status, 500, "server_error"); goto post_done;
      }
      sqlite3_bind_text(s, 1, nid,  -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, tid,  -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 3, kind, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 4, hash, -1, SQLITE_TRANSIENT);
      if (dom) sqlite3_bind_text(s, 5, dom, -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(s, 5);
      if (label) sqlite3_bind_text(s, 6, label, -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(s, 6);
      if (rule) sqlite3_bind_text(s, 7, rule, -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(s, 7);
      if (t->user_id[0]) sqlite3_bind_text(s, 8, t->user_id, -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(s, 8);
      int rc = sqlite3_step(s);
      sqlite3_finalize(s);
      if (rc != SQLITE_DONE) { out = err(status, 500, "server_error"); goto post_done; }

      pl = audit_payload(kind, label, rule, dom, hash);
      audit_write(db, tid, t->user_id, "breach_monitor.create", nid, pl);

      /* Match the EXISTING corpus immediately. Without this a monitor created
       * after ingest would only ever see future breaches and would miss all
       * 1,018 already-loaded ones — which is the entire product promise. It is
       * one indexed seek plus at most JO_BREACH_MONITOR_MAX_HITS inserts, so it
       * is safe to do inline on the request. */
      long long hits = 0;
      breach_monitor_rescan_all(db, tid, nid, NULL, NULL, &hits);
      out = one_monitor(db, tid, nid, 201, 1, &hits, status);
    }
post_done:
    free(pl); free(nv);
    if (jb) cJSON_Delete(jb);
    return out;
  }

  /* ── item ── */
  if (!act[0]) {
    if (is_get) return one_monitor(db, tid, seg, 200, 1, NULL, status);
    if (is_del) {
      if (!can_write(t)) return err(status, 403, "forbidden");
      mon_row m;
      if (!load_mon(db, tid, seg, &m)) return err(status, 404, "not_found");
      sqlite3_stmt *s;
      if (sqlite3_prepare_v2(db->h,
            "DELETE FROM breach_monitors WHERE id=?1 AND tenant_id=?2",
            -1, &s, NULL) != SQLITE_OK) return err(status, 500, "server_error");
      sqlite3_bind_text(s, 1, seg, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, tid, -1, SQLITE_TRANSIENT);
      sqlite3_step(s);
      sqlite3_finalize(s);
      /* alert_events raised by this monitor are intentionally LEFT IN PLACE:
       * they are the record that the tenant was told, and alert_deliveries
       * rows reference them. Deleting a standing order must not rewrite
       * history. */
      char *pl = audit_payload(m.kind, NULL, m.rule_id[0] ? m.rule_id : NULL,
                               m.domain[0] ? m.domain : NULL, m.hash);
      audit_write(db, tid, t->user_id, "breach_monitor.delete", seg, pl);
      free(pl);
      *status = 204;
      return NULL;
    }
    return err(status, 405, "method_not_allowed");
  }

  /* ── sub-actions ── */
  if (is_get && !strcmp(act, "hits"))
    return monitor_hits(db, tid, seg, cursor, limit, status);

  if (is_post && !strcmp(act, "rescan")) {
    if (!can_write(t)) return err(status, 403, "forbidden");
    mon_row m;
    if (!load_mon(db, tid, seg, &m)) return err(status, 404, "not_found");
    long long mons = 0, hits = 0;
    breach_monitor_rescan_all(db, tid, seg, NULL, &mons, &hits);
    char *pl = audit_payload(m.kind, NULL, m.rule_id[0] ? m.rule_id : NULL,
                             m.domain[0] ? m.domain : NULL, m.hash);
    audit_write(db, tid, t->user_id, "breach_monitor.rescan", seg, pl);
    free(pl);
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "monitor_id", seg);
    cJSON_AddItemToObject(d, "monitors_scanned", cJSON_CreateNumber((double)mons));
    cJSON_AddItemToObject(d, "hits", cJSON_CreateNumber((double)hits));
    cJSON *w = cJSON_CreateObject();
    cJSON_AddItemToObject(w, "data", d);
    char *o = cJSON_PrintUnformatted(w); cJSON_Delete(w);
    *status = 200;
    return o;
  }

  return err(status, 404, "not_found");
}
