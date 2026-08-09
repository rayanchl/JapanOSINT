/* core/casesapi.c — Roadmap 14: Cases. See casesapi.h for the design notes,
 * the required DDL and the httpd.c wiring block.
 *
 * Shape follows core/alertsapi.c (one dispatcher, malloc'd JSON out, *status
 * set on every path) and core/tenantapi.c's members dispatcher (method +
 * seg/act routing). The only structural deviation is a single-exit `goto done`
 * per branch: this dispatcher owns a parsed body plus, on several paths, a
 * synthesized snapshot string, and the alertsapi habit of repeating
 * `if (jb) cJSON_Delete(jb);` before every return does not survive that many
 * exits without leaking. */
#include "casesapi.h"
#include "annotationsapi.h"   /* the one ref_type vocabulary */
#include "audit.h"
#include "intelapi.h"
#include "breach_adapter.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

/* ── shared idioms (verbatim from alertsapi.c / intelapi.c) ─────────────── */

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
static cJSON *safe_json(const char *s, int as_array) {
  if (s && *s) { cJSON *j = cJSON_Parse(s); if (j) return j; }
  return as_array ? cJSON_CreateArray() : cJSON_CreateObject();
}
static void add_str_or_null(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}
/* Node's new Date().toISOString() — meta.fetched_at + snapshot.captured_at. */
static void iso_now(char *buf, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000));
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

/* ── keyset cursor {"p":<sort_val>,"u":<uid>} ───────────────────────────── */

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
static int clamp_limit(const cases_query *q) {
  int n = (q && q->limit > 0) ? q->limit : 50;
  if (n < 1) n = 1;
  if (n > 200) n = 200;
  return n;
}
/* {"data":[...],"page":{next_cursor,limit,total}} — `next` is consumed. */
static char *paged(cJSON *data, char *next, int limit, int *st) {
  cJSON *page = cJSON_CreateObject();
  cJSON_AddItemToObject(page, "next_cursor",
                        next ? cJSON_CreateString(next) : cJSON_CreateNull());
  free(next);
  cJSON_AddNumberToObject(page, "limit", limit);
  cJSON_AddNullToObject(page, "total");          /* keyset: no COUNT(*) */
  char ts[40]; iso_now(ts, sizeof ts);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON *w = cJSON_CreateObject();
  cJSON_AddItemToObject(w, "data", data);
  cJSON_AddItemToObject(w, "page", page);
  cJSON_AddItemToObject(w, "meta", meta);
  char *j = cJSON_PrintUnformatted(w); cJSON_Delete(w);
  *st = 200; return j;
}

/* ── vocabularies + validation ──────────────────────────────────────────── */

static int valid_status(const char *s) {
  return s && (!strcmp(s,"open") || !strcmp(s,"closed") || !strcmp(s,"archived"));
}
/* Single-sourced in annotationsapi.c. This used to be a private copy, and it
 * had already drifted from that one — the way a type ends up pinnable but not
 * annotatable with nothing to catch it. */
static int valid_ref_type(const char *s) {
  return annotations_ref_type_valid(s);
}
static int valid_case_role(const char *s) {
  return s && (!strcmp(s,"lead") || !strcmp(s,"contributor") || !strcmp(s,"viewer"));
}
/* Trim ASCII whitespace into out. Returns the length written, or -1 if the
 * trimmed value does not fit (caller turns that into a "too long" 400). */
static int trim_into(const char *in, char *out, size_t n) {
  out[0] = 0;
  if (!in) return 0;
  while (*in==' '||*in=='\t'||*in=='\n'||*in=='\r') in++;
  size_t l = strlen(in);
  while (l && (in[l-1]==' '||in[l-1]=='\t'||in[l-1]=='\n'||in[l-1]=='\r')) l--;
  if (l >= n) return -1;
  memcpy(out, in, l); out[l] = 0;
  return (int)l;
}
static const char *jstr(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

/* ── tenancy + authorization ────────────────────────────────────────────── */

/* THE tenant gate. Every child-table access in this file is downstream of a
 * successful call here; a case id that fails this is indistinguishable from a
 * case id that does not exist (404, never 403 — existence itself leaks). */
static int case_in_tenant(db_handle *db, const char *tid, const char *id) {
  sqlite3_stmt *s = NULL; int ok = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM cases WHERE id=?1 AND tenant_id=?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, tid, -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(s) == SQLITE_ROW;
  }
  sqlite3_finalize(s);
  return ok;
}
/* This user's role on this case, or NULL if not on the roster (buf ≥ 16). */
static const char *my_case_role(db_handle *db, const char *case_id,
                                const char *user_id, char *buf) {
  sqlite3_stmt *s = NULL; const char *out = NULL;
  if (!user_id || !*user_id) return NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT role FROM case_members WHERE case_id=?1 AND user_id=?2",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, case_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, user_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      snprintf(buf, 16, "%s", (const char *)sqlite3_column_text(s, 0));
      out = buf;
    }
  }
  sqlite3_finalize(s);
  return out;
}
static int tenant_at_least_analyst(const tenant_ctx *t) {
  return !strcmp(t->role,"owner") || !strcmp(t->role,"admin") ||
         !strcmp(t->role,"analyst");
}
static int tenant_is_admin(const tenant_ctx *t) {
  return !strcmp(t->role,"owner") || !strcmp(t->role,"admin");
}
/* Content writes: analyst+ in the tenant, or anyone on the case roster. */
static int can_write(db_handle *db, const tenant_ctx *t, const char *case_id) {
  char rb[16];
  if (tenant_at_least_analyst(t)) return 1;
  return my_case_role(db, case_id, t->user_id, rb) != NULL;
}
/* Roster edits: analyst+ in the tenant, or the case lead. Deliberately
 * narrower than can_write() — a case viewer must not be able to promote
 * themselves by rewriting the roster. */
static int can_manage_roster(db_handle *db, const tenant_ctx *t, const char *case_id) {
  char rb[16]; const char *r;
  if (tenant_at_least_analyst(t)) return 1;
  r = my_case_role(db, case_id, t->user_id, rb);
  return r && !strcmp(r, "lead");
}
/* Destruction: tenant owner/admin, or the case lead. */
static int can_delete(db_handle *db, const tenant_ctx *t, const char *case_id) {
  char rb[16]; const char *r;
  if (tenant_is_admin(t)) return 1;
  r = my_case_role(db, case_id, t->user_id, rb);
  return r && !strcmp(r, "lead");
}
/* Is this user a member of the ACTIVE tenant? Guards roster/mention writes so
 * a case can never name a principal from another workspace. */
static int is_tenant_member(db_handle *db, const char *tid, const char *uid) {
  sqlite3_stmt *s = NULL; int ok = 0;
  if (!uid || !*uid) return 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM memberships WHERE tenant_id=?1 AND user_id=?2",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, tid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, uid, -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(s) == SQLITE_ROW;
  }
  sqlite3_finalize(s);
  return ok;
}

/* ── server-written history + housekeeping ──────────────────────────────── */

/* Append a case_activity row. Silent on failure, like audit_write(): a history
 * write must not abort the mutation it describes. */
static void activity_add(db_handle *db, const char *case_id, const char *actor,
                         const char *kind, const char *body,
                         const char *target_ref, const char *mentions_json) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO case_activity (case_id,actor_id,kind,body,target_ref,"
        "mentions_json,ts) VALUES (?1,?2,?3,?4,?5,?6,datetime('now'))",
        -1, &s, NULL) != SQLITE_OK) { sqlite3_finalize(s); return; }
  sqlite3_bind_text(s, 1, case_id, -1, SQLITE_TRANSIENT);
  if (actor && *actor) sqlite3_bind_text(s, 2, actor, -1, SQLITE_TRANSIENT);
  else                 sqlite3_bind_null(s, 2);
  sqlite3_bind_text(s, 3, kind, -1, SQLITE_TRANSIENT);
  if (body)       sqlite3_bind_text(s, 4, body, -1, SQLITE_TRANSIENT);
  else            sqlite3_bind_null(s, 4);
  if (target_ref) sqlite3_bind_text(s, 5, target_ref, -1, SQLITE_TRANSIENT);
  else            sqlite3_bind_null(s, 5);
  sqlite3_bind_text(s, 6, mentions_json ? mentions_json : "[]", -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
}
/* Bump updated_at so the tenant list ("most recently worked on") reflects pins,
 * comments and roster edits, not only field edits. Tenant-filtered like
 * everything else, so a stale id cannot touch another workspace's row. */
static void touch_case(db_handle *db, const char *tid, const char *case_id) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE cases SET updated_at=datetime('now') WHERE id=?1 AND tenant_id=?2",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, case_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, tid,     -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
  }
  sqlite3_finalize(s);
}

/* ── snapshots ──────────────────────────────────────────────────────────── */

/* Uniform envelope. `data` is consumed (NULL ⇒ resolved:false). */
static char *snapshot_wrap(const char *ref_type, const char *ref_id,
                           const char *origin, cJSON *data) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "ref_type", ref_type);
  cJSON_AddStringToObject(o, "ref_id", ref_id);
  cJSON_AddStringToObject(o, "origin", origin);
  cJSON_AddBoolToObject(o, "resolved", data != NULL);
  char ts[40]; iso_now(ts, sizeof ts);
  cJSON_AddStringToObject(o, "captured_at", ts);
  cJSON_AddItemToObject(o, "data", data ? data : cJSON_CreateNull());
  char *j = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return j;
}
/* Unwrap a {"data":{...}} reply from another module into an owned node.
 * `envelope` is freed here. */
static cJSON *unwrap_data(char *envelope) {
  if (!envelope) return NULL;
  cJSON *root = cJSON_Parse(envelope);
  free(envelope);
  if (!root) return NULL;
  cJSON *d = cJSON_DetachItemFromObjectCaseSensitive(root, "data");
  cJSON_Delete(root);
  if (d && cJSON_IsNull(d)) { cJSON_Delete(d); return NULL; }
  return d;
}
/* `entities` is the one referenced corpus with a real per-tenant column, so it
 * is the one snapshot read that is tenant-scoped. NULL rows are legacy/global. */
static cJSON *entity_snapshot(db_handle *db, const tenant_ctx *t, const char *id) {
  sqlite3_stmt *s = NULL; cJSON *o = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT entity_id,type,canonical,name_ja,name_romaji,aliases_json,"
        "mention_count,first_seen_at,last_seen_at FROM entities "
        "WHERE entity_id=?1 AND (tenant_id IS NULL OR tenant_id=?2)",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id,           -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      o = cJSON_CreateObject();
      add_str_or_null(o, "entity_id",   ctext(s,0));
      add_str_or_null(o, "type",        ctext(s,1));
      add_str_or_null(o, "canonical",   ctext(s,2));
      add_str_or_null(o, "name_ja",     ctext(s,3));
      add_str_or_null(o, "name_romaji", ctext(s,4));
      cJSON_AddItemToObject(o, "aliases", safe_json(ctext(s,5), 1));
      cJSON_AddNumberToObject(o, "mention_count", (double)sqlite3_column_int64(s,6));
      add_str_or_null(o, "first_seen_at", ctext(s,7));
      add_str_or_null(o, "last_seen_at",  ctext(s,8));
    }
  }
  sqlite3_finalize(s);
  return o;
}
/* Caller-supplied snapshot → owned node. Accepts an object/array directly, or
 * a string holding JSON, or (last resort) keeps the string as-is. */
static cJSON *client_snapshot(cJSON *v) {
  if (!v || cJSON_IsNull(v)) return NULL;
  if (cJSON_IsString(v)) {
    cJSON *p = cJSON_Parse(v->valuestring);
    return p ? p : cJSON_CreateString(v->valuestring);
  }
  return cJSON_Duplicate(v, 1);
}
/* The denormalized display copy stored with a pin. Never NULL: an unresolvable
 * reference still gets an envelope, because dropping the pin loses the
 * analyst's intent while an unresolved pin merely renders thinly. */
static char *build_snapshot(db_handle *db, const tenant_ctx *t,
                            const char *ref_type, const char *ref_id,
                            cJSON *supplied) {
  cJSON *data = client_snapshot(supplied);
  if (data) return snapshot_wrap(ref_type, ref_id, "client", data);

  if (!strcmp(ref_type, "intel_item")) {
    data = unwrap_data(intelapi_item_by_uid(db, ref_id));
  } else if (!strcmp(ref_type, "entity")) {
    data = entity_snapshot(db, t, ref_id);
  } else if (!strcmp(ref_type, "breach_item")) {
    /* breach uids are "breach:<keyid>"; accept either form from the client and
     * always go through the adapter, which is what redacts the secret. */
    if (!strncmp(ref_id, "breach:", 7)) {
      data = unwrap_data(breach_adapter_item_by_uid(db, ref_id));
    } else {
      char uid[600];
      snprintf(uid, sizeof uid, "breach:%s", ref_id);
      data = unwrap_data(breach_adapter_item_by_uid(db, uid));
    }
  }
  /* feature / camera / search_run: no canonical row reachable from here. */
  return snapshot_wrap(ref_type, ref_id, "server", data);
}

/* ── row → JSON ─────────────────────────────────────────────────────────── */

static const char *CASE_COLS =
  "id,name,summary,status,priority,created_by,created_at,updated_at,closed_at";

static cJSON *case_row(sqlite3_stmt *s) {
  cJSON *o = cJSON_CreateObject();
  add_str_or_null(o, "id",      ctext(s,0));
  add_str_or_null(o, "name",    ctext(s,1));
  add_str_or_null(o, "summary", ctext(s,2));
  add_str_or_null(o, "status",  ctext(s,3));
  cJSON_AddNumberToObject(o, "priority", (double)sqlite3_column_int64(s,4));
  add_str_or_null(o, "created_by", ctext(s,5));
  add_str_or_null(o, "created_at", ctext(s,6));
  add_str_or_null(o, "updated_at", ctext(s,7));
  add_str_or_null(o, "closed_at",  ctext(s,8));
  return o;
}
static cJSON *item_row(sqlite3_stmt *s) {
  cJSON *o = cJSON_CreateObject();
  add_str_or_null(o, "ref_type", ctext(s,0));
  add_str_or_null(o, "ref_id",   ctext(s,1));
  add_str_or_null(o, "label",    ctext(s,2));
  cJSON_AddItemToObject(o, "snapshot", safe_json(ctext(s,3), 0));
  add_str_or_null(o, "added_by", ctext(s,4));
  add_str_or_null(o, "added_at", ctext(s,5));
  return o;
}
static cJSON *member_array(db_handle *db, const char *case_id) {
  cJSON *arr = cJSON_CreateArray();
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT cm.user_id,u.email,u.display_name,cm.role FROM case_members cm "
        "LEFT JOIN users u ON u.id=cm.user_id WHERE cm.case_id=?1 "
        "ORDER BY cm.role ASC, cm.user_id ASC", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, case_id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(s) == SQLITE_ROW) {
      cJSON *r = cJSON_CreateObject();
      add_str_or_null(r, "user_id",      ctext(s,0));
      add_str_or_null(r, "email",        ctext(s,1));
      add_str_or_null(r, "display_name", ctext(s,2));
      add_str_or_null(r, "role",         ctext(s,3));
      cJSON_AddItemToArray(arr, r);
    }
  }
  sqlite3_finalize(s);
  return arr;
}
/* {"intel_item":3,"entity":1,...} plus a "total". Every ref_type in the
 * vocabulary is present with 0 so the UI can render a stable set of chips. */
static cJSON *item_counts(db_handle *db, const char *case_id, long long *total_out) {
  /* Enumerated from the shared vocabulary, not a local list: a new ref_type
   * has to appear in these chips automatically, or the UI quietly stops
   * showing a category that is being pinned. */
  int ntypes = 0;
  const char *const *TYPES = annotations_ref_types(&ntypes);
  cJSON *o = cJSON_CreateObject();
  for (int i = 0; i < ntypes; i++) cJSON_AddNumberToObject(o, TYPES[i], 0);
  long long total = 0;
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT ref_type,COUNT(*) FROM case_items WHERE case_id=?1 "
        "GROUP BY ref_type", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, case_id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(s) == SQLITE_ROW) {
      const char *rt = ctext(s, 0);
      long long n = sqlite3_column_int64(s, 1);
      total += n;
      if (!rt) continue;
      cJSON *ex = cJSON_GetObjectItem(o, rt);
      if (ex) cJSON_SetNumberValue(ex, (double)n);
      else    cJSON_AddNumberToObject(o, rt, (double)n);
    }
  }
  sqlite3_finalize(s);
  if (total_out) *total_out = total;
  return o;
}
/* GET /api/cases/:id — the case plus everything a detail screen needs in one
 * round trip. 404 (not 403) when the id belongs to another tenant. */
static char *case_detail(db_handle *db, const tenant_ctx *t, const char *id,
                         int code, int *st) {
  sqlite3_stmt *s = NULL;
  char sql[256];
  snprintf(sql, sizeof sql,
           "SELECT %s FROM cases WHERE id=?1 AND tenant_id=?2", CASE_COLS);
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    sqlite3_finalize(s);
    return err(st, 500, "server_error");
  }
  sqlite3_bind_text(s, 1, id,           -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) {
    sqlite3_finalize(s);
    return err(st, 404, "not_found");
  }
  cJSON *c = case_row(s);
  sqlite3_finalize(s);

  long long total = 0;
  cJSON_AddItemToObject(c, "item_counts", item_counts(db, id, &total));
  cJSON_AddNumberToObject(c, "item_count", (double)total);
  cJSON_AddItemToObject(c, "members", member_array(db, id));

  char rb[16];
  const char *mine = my_case_role(db, id, t->user_id, rb);
  add_str_or_null(c, "my_case_role", mine);

  cJSON *w = cJSON_CreateObject();
  cJSON_AddItemToObject(w, "data", c);
  char *j = cJSON_PrintUnformatted(w); cJSON_Delete(w);
  *st = code; return j;
}

/* ── dispatcher ─────────────────────────────────────────────────────────── */

char *casesapi(db_handle *db, const tenant_ctx *t, const char *method,
               const char *seg, const char *act, const char *body,
               const cases_query *q, int *st) {
  if (!db || !db->h || !t || !method || !seg || !act)
    return err(st, 500, "server_error");

  const int is_get   = !strcmp(method, "GET");
  const int is_post  = !strcmp(method, "POST");
  const int is_patch = !strcmp(method, "PATCH");
  const int is_put   = !strcmp(method, "PUT");
  const int is_del   = !strcmp(method, "DELETE");

  cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
  char *out = NULL;
  const int lim = clamp_limit(q);
  char cp[256] = {0}, cu[600] = {0};       /* decoded cursor halves */

  /* ── collection: /api/cases ─────────────────────────────────────────── */
  if (!seg[0]) {
    if (is_get) {
      const char *fstatus = (q && q->status && *q->status) ? q->status : NULL;
      if (fstatus && !valid_status(fstatus)) {
        out = err(st, 400, "invalid_status"); goto done;
      }
      int have_cur = cursor_split(q ? q->cursor : NULL, cp, sizeof cp, cu, sizeof cu);
      char sql[768];
      snprintf(sql, sizeof sql,
        "SELECT c.id,c.name,c.summary,c.status,c.priority,c.created_by,"
        "c.created_at,c.updated_at,c.closed_at,"
        "(SELECT COUNT(*) FROM case_items ci WHERE ci.case_id=c.id) "
        "FROM cases c WHERE c.tenant_id=?%s%s "
        "ORDER BY c.updated_at DESC, c.id ASC LIMIT ?",
        fstatus  ? " AND c.status=?" : "",
        have_cur ? " AND (c.updated_at < ? OR (c.updated_at = ? AND c.id > ?))" : "");
      sqlite3_stmt *s = NULL;
      if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
        sqlite3_finalize(s); out = err(st, 500, "server_error"); goto done;
      }
      int bi = 1;
      sqlite3_bind_text(s, bi++, t->tenant_id, -1, SQLITE_TRANSIENT);
      if (fstatus)  sqlite3_bind_text(s, bi++, fstatus, -1, SQLITE_TRANSIENT);
      if (have_cur) {
        sqlite3_bind_text(s, bi++, cp, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s, bi++, cp, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s, bi++, cu, -1, SQLITE_TRANSIENT);
      }
      sqlite3_bind_int(s, bi++, lim);

      cJSON *data = cJSON_CreateArray();
      int n = 0; char lp[256] = {0}, lu[128] = {0};
      while (sqlite3_step(s) == SQLITE_ROW) {
        cJSON *r = case_row(s);
        cJSON_AddNumberToObject(r, "item_count", (double)sqlite3_column_int64(s,9));
        cJSON_AddItemToArray(data, r);
        snprintf(lp, sizeof lp, "%s", ctext(s,7) ? ctext(s,7) : "");
        snprintf(lu, sizeof lu, "%s", ctext(s,0) ? ctext(s,0) : "");
        n++;
      }
      sqlite3_finalize(s);
      out = paged(data, n == lim ? cursor_make(lp, lu) : NULL, lim, st);
      goto done;
    }

    if (is_post) {
      if (!tenant_at_least_analyst(t)) { out = err(st, 403, "forbidden"); goto done; }
      if (!jb || !cJSON_IsObject(jb)) { out = err(st, 400, "body required"); goto done; }
      char name[512], summ[8192];
      int nl = trim_into(jstr(jb, "name"), name, sizeof name);
      if (nl <= 0)   { out = err(st, 400, nl < 0 ? "name too long" : "name required"); goto done; }
      if (nl > 200)  { out = err(st, 400, "name too long"); goto done; }
      cJSON *jsum = cJSON_GetObjectItem(jb, "summary");
      int has_summary = jsum && cJSON_IsString(jsum);
      if (jsum && !cJSON_IsString(jsum) && !cJSON_IsNull(jsum)) {
        out = err(st, 400, "summary must be a string"); goto done;
      }
      if (has_summary && trim_into(jsum->valuestring, summ, sizeof summ) < 0) {
        out = err(st, 400, "summary too long"); goto done;
      }
      cJSON *jstat = cJSON_GetObjectItem(jb, "status");
      const char *status_v = "open";
      if (jstat && !cJSON_IsNull(jstat)) {
        if (!cJSON_IsString(jstat) || !valid_status(jstat->valuestring)) {
          out = err(st, 400, "invalid_status"); goto done;
        }
        status_v = jstat->valuestring;
      }
      cJSON *jpri = cJSON_GetObjectItem(jb, "priority");
      long long pri = 0;
      if (jpri && !cJSON_IsNull(jpri)) {
        /* Range FIRST, integrality second. (long long)d is undefined when d
         * is outside long long's range, so testing integrality first made
         * {"priority":1e308} — a body anyone authenticated can POST — UB
         * before the 0..5 check could reject it. The positive form also
         * rejects NaN, which fails every comparison. */
        if (!cJSON_IsNumber(jpri) ||
            !(jpri->valuedouble >= 0 && jpri->valuedouble <= 5) ||
            jpri->valuedouble != (double)(long long)jpri->valuedouble) {
          out = err(st, 400, "priority must be an integer 0..5"); goto done;
        }
        pri = (long long)jpri->valuedouble;
      }

      char nid[37]; uuid4(nid);
      sqlite3_stmt *s = NULL;
      if (sqlite3_prepare_v2(db->h,
            "INSERT INTO cases (id,tenant_id,name,summary,status,priority,"
            "created_by,created_at,updated_at,closed_at) VALUES (?1,?2,?3,?4,?5,?6,?7,"
            "datetime('now'),datetime('now'),"
            "CASE WHEN ?5='closed' THEN datetime('now') ELSE NULL END)",
            -1, &s, NULL) != SQLITE_OK) {
        sqlite3_finalize(s); out = err(st, 500, "server_error"); goto done;
      }
      sqlite3_bind_text(s, 1, nid, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 3, name, -1, SQLITE_TRANSIENT);
      if (has_summary && summ[0]) sqlite3_bind_text(s, 4, summ, -1, SQLITE_TRANSIENT);
      else                        sqlite3_bind_null(s, 4);
      sqlite3_bind_text(s, 5, status_v, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(s, 6, pri);
      if (t->user_id[0]) sqlite3_bind_text(s, 7, t->user_id, -1, SQLITE_TRANSIENT);
      else               sqlite3_bind_null(s, 7);
      int rc = sqlite3_step(s);
      sqlite3_finalize(s);
      if (rc != SQLITE_DONE) { out = err(st, 500, "server_error"); goto done; }

      /* The creator is the lead: a case with no roster cannot be led, and the
       * delete/roster gates below are otherwise unreachable for an analyst. */
      if (t->user_id[0]) {
        sqlite3_stmt *m = NULL;
        if (sqlite3_prepare_v2(db->h,
              "INSERT OR IGNORE INTO case_members (case_id,user_id,role) "
              "VALUES (?1,?2,'lead')", -1, &m, NULL) == SQLITE_OK) {
          sqlite3_bind_text(m, 1, nid, -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(m, 2, t->user_id, -1, SQLITE_TRANSIENT);
          sqlite3_step(m);
        }
        sqlite3_finalize(m);
      }
      activity_add(db, nid, t->user_id, "created", name, NULL, NULL);
      {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "name", name);
        cJSON_AddStringToObject(p, "status", status_v);
        cJSON_AddNumberToObject(p, "priority", (double)pri);
        char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
        audit_write(db, t->tenant_id, t->user_id, "case.create", nid, pj);
        free(pj);
      }
      out = case_detail(db, t, nid, 201, st);
      goto done;
    }
    out = err(st, 404, "not_found"); goto done;
  }

  /* Everything below addresses one case. Resolve it under the tenant FIRST —
   * this single check is what keeps the four child tables tenant-safe. */
  if (!case_in_tenant(db, t->tenant_id, seg)) {
    out = err(st, 404, "not_found"); goto done;
  }

  /* ── /api/cases/:id ─────────────────────────────────────────────────── */
  if (!act[0]) {
    if (is_get) { out = case_detail(db, t, seg, 200, st); goto done; }

    if (is_patch) {
      if (!can_write(db, t, seg)) { out = err(st, 403, "forbidden"); goto done; }
      if (!jb || !cJSON_IsObject(jb)) { out = err(st, 400, "body required"); goto done; }

      sqlite3_stmt *s = NULL;
      if (sqlite3_prepare_v2(db->h,
            "SELECT name,summary,status,priority FROM cases "
            "WHERE id=?1 AND tenant_id=?2", -1, &s, NULL) != SQLITE_OK) {
        sqlite3_finalize(s); out = err(st, 500, "server_error"); goto done;
      }
      sqlite3_bind_text(s, 1, seg, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(s) != SQLITE_ROW) {
        sqlite3_finalize(s); out = err(st, 404, "not_found"); goto done;
      }
      char cur_name[512], cur_status[16];
      char cur_summ[8192]; int had_summary;
      snprintf(cur_name, sizeof cur_name, "%s", ctext(s,0) ? ctext(s,0) : "");
      had_summary = ctext(s,1) != NULL;
      snprintf(cur_summ, sizeof cur_summ, "%s", ctext(s,1) ? ctext(s,1) : "");
      snprintf(cur_status, sizeof cur_status, "%s", ctext(s,2) ? ctext(s,2) : "open");
      long long cur_pri = sqlite3_column_int64(s, 3);
      sqlite3_finalize(s);

      char new_name[512]; snprintf(new_name, sizeof new_name, "%s", cur_name);
      char new_summ[8192]; snprintf(new_summ, sizeof new_summ, "%s", cur_summ);
      int new_has_summary = had_summary;
      char new_status[16]; snprintf(new_status, sizeof new_status, "%s", cur_status);
      long long new_pri = cur_pri;

      cJSON *jn = cJSON_GetObjectItem(jb, "name");
      if (jn && !cJSON_IsNull(jn)) {
        if (!cJSON_IsString(jn)) { out = err(st, 400, "name must be a string"); goto done; }
        int nl = trim_into(jn->valuestring, new_name, sizeof new_name);
        if (nl < 0)  { out = err(st, 400, "name too long"); goto done; }
        if (nl == 0) { out = err(st, 400, "name required"); goto done; }
        if (nl > 200){ out = err(st, 400, "name too long"); goto done; }
      }
      cJSON *jsum = cJSON_GetObjectItem(jb, "summary");
      if (jsum) {
        if (cJSON_IsNull(jsum)) { new_has_summary = 0; new_summ[0] = 0; }
        else if (cJSON_IsString(jsum)) {
          if (trim_into(jsum->valuestring, new_summ, sizeof new_summ) < 0) {
            out = err(st, 400, "summary too long"); goto done;
          }
          new_has_summary = new_summ[0] != 0;
        } else { out = err(st, 400, "summary must be a string"); goto done; }
      }
      cJSON *jstat = cJSON_GetObjectItem(jb, "status");
      if (jstat && !cJSON_IsNull(jstat)) {
        if (!cJSON_IsString(jstat) || !valid_status(jstat->valuestring)) {
          out = err(st, 400, "invalid_status"); goto done;
        }
        snprintf(new_status, sizeof new_status, "%s", jstat->valuestring);
      }
      cJSON *jpri = cJSON_GetObjectItem(jb, "priority");
      if (jpri && !cJSON_IsNull(jpri)) {
        /* Same ordering fix as the POST path above: bound the value before
         * the double->long long cast, which is UB outside that range. */
        if (!cJSON_IsNumber(jpri) ||
            !(jpri->valuedouble >= 0 && jpri->valuedouble <= 5) ||
            jpri->valuedouble != (double)(long long)jpri->valuedouble) {
          out = err(st, 400, "priority must be an integer 0..5"); goto done;
        }
        new_pri = (long long)jpri->valuedouble;
      }

      const int status_changed = strcmp(new_status, cur_status) != 0;
      /* closed_at is derived, never client-set: entering 'closed' stamps it,
       * any move out of 'closed' (reopen OR archive) clears it. Re-PATCHing
       * 'closed' onto an already-closed case must not restamp — the original
       * closure time is the fact a report cites. */
      const char *closed_expr =
        !strcmp(new_status, "closed")
          ? (!strcmp(cur_status, "closed") ? "closed_at" : "datetime('now')")
          : "NULL";
      /* When the body omits `summary` the column is assigned FROM ITSELF, not
       * from the staging buffer: cur_summ is a fixed-size read-back, so a
       * name-only PATCH of a case with an oversized summary would otherwise
       * silently truncate it. Absent field ⇒ column untouched, by construction. */
      const char *summ_expr = jsum ? "?2" : "summary";
      char sql[512];
      snprintf(sql, sizeof sql,
        "UPDATE cases SET name=?1,summary=%s,status=?3,priority=?4,"
        "updated_at=datetime('now'),closed_at=%s WHERE id=?5 AND tenant_id=?6",
        summ_expr, closed_expr);
      if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
        sqlite3_finalize(s); out = err(st, 500, "server_error"); goto done;
      }
      sqlite3_bind_text(s, 1, new_name, -1, SQLITE_TRANSIENT);
      if (jsum) {                       /* ?2 exists only in the assigning form */
        if (new_has_summary) sqlite3_bind_text(s, 2, new_summ, -1, SQLITE_TRANSIENT);
        else                 sqlite3_bind_null(s, 2);
      }
      sqlite3_bind_text(s, 3, new_status, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(s, 4, new_pri);
      sqlite3_bind_text(s, 5, seg, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 6, t->tenant_id, -1, SQLITE_TRANSIENT);
      int rc = sqlite3_step(s);
      sqlite3_finalize(s);
      if (rc != SQLITE_DONE) { out = err(st, 500, "server_error"); goto done; }

      if (status_changed) {
        char msg[64];
        snprintf(msg, sizeof msg, "%s → %s", cur_status, new_status);
        activity_add(db, seg, t->user_id, "status_changed", msg, NULL, NULL);
      }
      if (strcmp(new_name, cur_name) || new_pri != cur_pri ||
          new_has_summary != had_summary ||
          (new_has_summary && strcmp(new_summ, cur_summ)))
        activity_add(db, seg, t->user_id, "updated", NULL, NULL, NULL);
      {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "name", new_name);
        cJSON_AddStringToObject(p, "status", new_status);
        cJSON_AddNumberToObject(p, "priority", (double)new_pri);
        char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
        audit_write(db, t->tenant_id, t->user_id, "case.update", seg, pj);
        free(pj);
      }
      out = case_detail(db, t, seg, 200, st);
      goto done;
    }

    if (is_del) {
      if (!can_delete(db, t, seg)) { out = err(st, 403, "forbidden"); goto done; }
      /* Explicit, ordered, transactional child deletes — see the CASCADE
       * DECISION note in casesapi.h: foreign_keys is per-connection and only
       * db_open() sets it, so cascade is not a property we can rely on. */
      static const char *DEL[] = {
        "DELETE FROM case_activity WHERE case_id=?1",
        "DELETE FROM case_items    WHERE case_id=?1",
        "DELETE FROM case_members  WHERE case_id=?1"
      };
      sqlite3_exec(db->h, "BEGIN", NULL, NULL, NULL);
      int ok = 1;
      for (int i = 0; i < 3 && ok; i++) {
        sqlite3_stmt *s = NULL;
        if (sqlite3_prepare_v2(db->h, DEL[i], -1, &s, NULL) != SQLITE_OK) ok = 0;
        else {
          sqlite3_bind_text(s, 1, seg, -1, SQLITE_TRANSIENT);
          if (sqlite3_step(s) != SQLITE_DONE) ok = 0;
        }
        sqlite3_finalize(s);
      }
      if (ok) {
        sqlite3_stmt *s = NULL;
        if (sqlite3_prepare_v2(db->h,
              "DELETE FROM cases WHERE id=?1 AND tenant_id=?2", -1, &s, NULL) != SQLITE_OK) ok = 0;
        else {
          sqlite3_bind_text(s, 1, seg, -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
          if (sqlite3_step(s) != SQLITE_DONE) ok = 0;
        }
        sqlite3_finalize(s);
      }
      sqlite3_exec(db->h, ok ? "COMMIT" : "ROLLBACK", NULL, NULL, NULL);
      if (!ok) { out = err(st, 500, "server_error"); goto done; }
      audit_write(db, t->tenant_id, t->user_id, "case.delete", seg, NULL);
      *st = 204; out = NULL; goto done;
    }
    out = err(st, 404, "not_found"); goto done;
  }

  /* ── /api/cases/:id/items ───────────────────────────────────────────── */
  if (!strcmp(act, "items")) {
    if (is_get) {
      const char *frt = (q && q->ref_type && *q->ref_type) ? q->ref_type : NULL;
      if (frt && !valid_ref_type(frt)) { out = err(st, 400, "invalid_ref_type"); goto done; }
      int have_cur = cursor_split(q ? q->cursor : NULL, cp, sizeof cp, cu, sizeof cu);
      char sql[768];
      snprintf(sql, sizeof sql,
        "SELECT ref_type,ref_id,label,snapshot_json,added_by,added_at "
        "FROM case_items WHERE case_id=?%s%s "
        "ORDER BY added_at DESC, (ref_type||':'||ref_id) ASC LIMIT ?",
        frt      ? " AND ref_type=?" : "",
        have_cur ? " AND (added_at < ? OR (added_at = ? AND "
                   "(ref_type||':'||ref_id) > ?))" : "");
      sqlite3_stmt *s = NULL;
      if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
        sqlite3_finalize(s); out = err(st, 500, "server_error"); goto done;
      }
      int bi = 1;
      sqlite3_bind_text(s, bi++, seg, -1, SQLITE_TRANSIENT);
      if (frt) sqlite3_bind_text(s, bi++, frt, -1, SQLITE_TRANSIENT);
      if (have_cur) {
        sqlite3_bind_text(s, bi++, cp, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s, bi++, cp, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s, bi++, cu, -1, SQLITE_TRANSIENT);
      }
      sqlite3_bind_int(s, bi++, lim);
      cJSON *data = cJSON_CreateArray();
      int n = 0; char lp[256] = {0}, lu[600] = {0};
      while (sqlite3_step(s) == SQLITE_ROW) {
        cJSON_AddItemToArray(data, item_row(s));
        snprintf(lp, sizeof lp, "%s", ctext(s,5) ? ctext(s,5) : "");
        snprintf(lu, sizeof lu, "%s:%s", ctext(s,0) ? ctext(s,0) : "",
                 ctext(s,1) ? ctext(s,1) : "");
        n++;
      }
      sqlite3_finalize(s);
      out = paged(data, n == lim ? cursor_make(lp, lu) : NULL, lim, st);
      goto done;
    }

    if (is_post) {
      if (!can_write(db, t, seg)) { out = err(st, 403, "forbidden"); goto done; }
      if (!jb || !cJSON_IsObject(jb)) { out = err(st, 400, "body required"); goto done; }
      const char *rt = jstr(jb, "ref_type");
      if (!valid_ref_type(rt)) { out = err(st, 400, "invalid_ref_type"); goto done; }
      char rid[600];
      int rl = trim_into(jstr(jb, "ref_id"), rid, sizeof rid);
      if (rl <= 0) { out = err(st, 400, rl < 0 ? "ref_id too long" : "ref_id required"); goto done; }
      char label[512]; int has_label = 0;
      cJSON *jl = cJSON_GetObjectItem(jb, "label");
      if (jl && !cJSON_IsNull(jl)) {
        if (!cJSON_IsString(jl)) { out = err(st, 400, "label must be a string"); goto done; }
        if (trim_into(jl->valuestring, label, sizeof label) < 0) {
          out = err(st, 400, "label too long"); goto done;
        }
        has_label = label[0] != 0;
      }

      /* Existence decides 200-vs-201 AND whether we re-snapshot: the whole
       * point of the stored copy is that it is the state at FIRST pin, so a
       * repeat pin must not overwrite it. */
      int existed = 0;
      {
        sqlite3_stmt *s = NULL;
        if (sqlite3_prepare_v2(db->h,
              "SELECT 1 FROM case_items WHERE case_id=?1 AND ref_type=?2 AND ref_id=?3",
              -1, &s, NULL) == SQLITE_OK) {
          sqlite3_bind_text(s, 1, seg, -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(s, 2, rt,  -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(s, 3, rid, -1, SQLITE_TRANSIENT);
          existed = sqlite3_step(s) == SQLITE_ROW;
        }
        sqlite3_finalize(s);
      }
      char *snap = build_snapshot(db, t, rt, rid,
                                  cJSON_GetObjectItem(jb, "snapshot_json"));
      sqlite3_stmt *s = NULL;
      if (sqlite3_prepare_v2(db->h,
            "INSERT INTO case_items (case_id,ref_type,ref_id,label,snapshot_json,"
            "added_by,added_at) VALUES (?1,?2,?3,?4,?5,?6,datetime('now')) "
            "ON CONFLICT(case_id,ref_type,ref_id) DO UPDATE SET "
            "label=COALESCE(excluded.label,case_items.label)",
            -1, &s, NULL) != SQLITE_OK) {
        sqlite3_finalize(s); free(snap);
        out = err(st, 500, "server_error"); goto done;
      }
      sqlite3_bind_text(s, 1, seg, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, rt,  -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 3, rid, -1, SQLITE_TRANSIENT);
      if (has_label) sqlite3_bind_text(s, 4, label, -1, SQLITE_TRANSIENT);
      else           sqlite3_bind_null(s, 4);
      sqlite3_bind_text(s, 5, snap, -1, SQLITE_TRANSIENT);
      if (t->user_id[0]) sqlite3_bind_text(s, 6, t->user_id, -1, SQLITE_TRANSIENT);
      else               sqlite3_bind_null(s, 6);
      int rc = sqlite3_step(s);
      sqlite3_finalize(s);
      free(snap);
      if (rc != SQLITE_DONE) { out = err(st, 500, "server_error"); goto done; }

      char tref[640];
      snprintf(tref, sizeof tref, "%s:%s", rt, rid);
      if (!existed) {
        activity_add(db, seg, t->user_id, "item_pinned",
                     has_label ? label : NULL, tref, NULL);
        touch_case(db, t->tenant_id, seg);
      }
      audit_write(db, t->tenant_id, t->user_id, "case.item.pin", seg, NULL);

      /* Echo the stored row so the client renders the snapshot it will see on
       * reload, not the one it hoped for. */
      sqlite3_stmt *r = NULL;
      if (sqlite3_prepare_v2(db->h,
            "SELECT ref_type,ref_id,label,snapshot_json,added_by,added_at "
            "FROM case_items WHERE case_id=?1 AND ref_type=?2 AND ref_id=?3",
            -1, &r, NULL) != SQLITE_OK) {
        sqlite3_finalize(r); out = err(st, 500, "server_error"); goto done;
      }
      sqlite3_bind_text(r, 1, seg, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(r, 2, rt,  -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(r, 3, rid, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(r) != SQLITE_ROW) {
        sqlite3_finalize(r); out = err(st, 500, "server_error"); goto done;
      }
      cJSON *w = cJSON_CreateObject();
      cJSON_AddItemToObject(w, "data", item_row(r));
      sqlite3_finalize(r);
      out = cJSON_PrintUnformatted(w); cJSON_Delete(w);
      *st = existed ? 200 : 201;
      goto done;
    }

    if (is_del) {
      if (!can_write(db, t, seg)) { out = err(st, 403, "forbidden"); goto done; }
      /* Body wins over the query string: a DELETE with a body is the explicit
       * form, the query string is the convenience form for link-style calls. */
      const char *rt = jb ? jstr(jb, "ref_type") : NULL;
      const char *ri = jb ? jstr(jb, "ref_id")   : NULL;
      if (!rt && q && q->ref_type && *q->ref_type) rt = q->ref_type;
      if (!ri && q && q->ref_id   && *q->ref_id)   ri = q->ref_id;
      if (!valid_ref_type(rt)) { out = err(st, 400, "invalid_ref_type"); goto done; }
      char rid[600];
      int rl = trim_into(ri, rid, sizeof rid);
      if (rl <= 0) { out = err(st, 400, rl < 0 ? "ref_id too long" : "ref_id required"); goto done; }

      sqlite3_stmt *s = NULL;
      if (sqlite3_prepare_v2(db->h,
            "DELETE FROM case_items WHERE case_id=?1 AND ref_type=?2 AND ref_id=?3",
            -1, &s, NULL) != SQLITE_OK) {
        sqlite3_finalize(s); out = err(st, 500, "server_error"); goto done;
      }
      sqlite3_bind_text(s, 1, seg, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, rt,  -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 3, rid, -1, SQLITE_TRANSIENT);
      sqlite3_step(s);
      int changed = sqlite3_changes(db->h);
      sqlite3_finalize(s);
      if (changed == 0) { out = err(st, 404, "not_found"); goto done; }

      char tref[640];
      snprintf(tref, sizeof tref, "%s:%s", rt, rid);
      activity_add(db, seg, t->user_id, "item_unpinned", NULL, tref, NULL);
      touch_case(db, t->tenant_id, seg);
      audit_write(db, t->tenant_id, t->user_id, "case.item.unpin", seg, NULL);
      *st = 204; out = NULL; goto done;
    }
    out = err(st, 404, "not_found"); goto done;
  }

  /* ── /api/cases/:id/activity ────────────────────────────────────────── */
  if (!strcmp(act, "activity")) {
    if (is_get) {
      int have_cur = cursor_split(q ? q->cursor : NULL, cp, sizeof cp, cu, sizeof cu);
      char sql[640];
      snprintf(sql, sizeof sql,
        "SELECT id,actor_id,kind,body,target_ref,mentions_json,ts "
        "FROM case_activity WHERE case_id=?%s ORDER BY ts DESC, id DESC LIMIT ?",
        have_cur ? " AND (ts < ? OR (ts = ? AND id < ?))" : "");
      sqlite3_stmt *s = NULL;
      if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
        sqlite3_finalize(s); out = err(st, 500, "server_error"); goto done;
      }
      int bi = 1;
      sqlite3_bind_text(s, bi++, seg, -1, SQLITE_TRANSIENT);
      if (have_cur) {
        sqlite3_bind_text(s, bi++, cp, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s, bi++, cp, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(s, bi++, (sqlite3_int64)strtoll(cu, NULL, 10));
      }
      sqlite3_bind_int(s, bi++, lim);
      cJSON *data = cJSON_CreateArray();
      int n = 0; char lp[256] = {0}, lu[32] = {0};
      while (sqlite3_step(s) == SQLITE_ROW) {
        sqlite3_int64 rid = sqlite3_column_int64(s, 0);
        cJSON *r = cJSON_CreateObject();
        cJSON_AddNumberToObject(r, "id", (double)rid);
        add_str_or_null(r, "actor_id",   ctext(s,1));
        add_str_or_null(r, "kind",       ctext(s,2));
        add_str_or_null(r, "body",       ctext(s,3));
        add_str_or_null(r, "target_ref", ctext(s,4));
        cJSON_AddItemToObject(r, "mentions", safe_json(ctext(s,5), 1));
        add_str_or_null(r, "ts", ctext(s,6));
        cJSON_AddItemToArray(data, r);
        snprintf(lp, sizeof lp, "%s", ctext(s,6) ? ctext(s,6) : "");
        snprintf(lu, sizeof lu, "%lld", (long long)rid);
        n++;
      }
      sqlite3_finalize(s);
      out = paged(data, n == lim ? cursor_make(lp, lu) : NULL, lim, st);
      goto done;
    }

    if (is_post) {
      if (!can_write(db, t, seg)) { out = err(st, 403, "forbidden"); goto done; }
      if (!jb || !cJSON_IsObject(jb)) { out = err(st, 400, "body required"); goto done; }
      char text[8192];
      int bl = trim_into(jstr(jb, "body"), text, sizeof text);
      if (bl <= 0) { out = err(st, 400, bl < 0 ? "body too long" : "body required"); goto done; }

      /* Mentions are filtered to the active tenant's members rather than
       * rejected: a stale @ from a removed colleague should not fail the
       * comment, but it must never persist a foreign principal's id. */
      cJSON *jm = cJSON_GetObjectItem(jb, "mentions");
      if (jm && !cJSON_IsNull(jm) && !cJSON_IsArray(jm)) {
        out = err(st, 400, "mentions must be an array"); goto done;
      }
      cJSON *ment = cJSON_CreateArray();
      if (jm && cJSON_IsArray(jm)) {
        cJSON *e; int kept = 0;
        cJSON_ArrayForEach(e, jm) {
          if (kept >= 50) break;
          if (!cJSON_IsString(e) || !e->valuestring[0]) continue;
          if (!is_tenant_member(db, t->tenant_id, e->valuestring)) continue;
          cJSON_AddItemToArray(ment, cJSON_CreateString(e->valuestring));
          kept++;
        }
      }
      char *mj = cJSON_PrintUnformatted(ment);
      cJSON_Delete(ment);
      activity_add(db, seg, t->user_id, "comment", text, NULL, mj);
      sqlite3_int64 nid = sqlite3_last_insert_rowid(db->h);
      free(mj);
      touch_case(db, t->tenant_id, seg);
      audit_write(db, t->tenant_id, t->user_id, "case.activity.create", seg, NULL);

      sqlite3_stmt *r = NULL;
      if (sqlite3_prepare_v2(db->h,
            "SELECT id,actor_id,kind,body,target_ref,mentions_json,ts "
            "FROM case_activity WHERE id=?1 AND case_id=?2", -1, &r, NULL) != SQLITE_OK) {
        sqlite3_finalize(r); out = err(st, 500, "server_error"); goto done;
      }
      sqlite3_bind_int64(r, 1, nid);
      sqlite3_bind_text(r, 2, seg, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(r) != SQLITE_ROW) {
        sqlite3_finalize(r); out = err(st, 500, "server_error"); goto done;
      }
      cJSON *o = cJSON_CreateObject();
      cJSON_AddNumberToObject(o, "id", (double)sqlite3_column_int64(r,0));
      add_str_or_null(o, "actor_id",   ctext(r,1));
      add_str_or_null(o, "kind",       ctext(r,2));
      add_str_or_null(o, "body",       ctext(r,3));
      add_str_or_null(o, "target_ref", ctext(r,4));
      cJSON_AddItemToObject(o, "mentions", safe_json(ctext(r,5), 1));
      add_str_or_null(o, "ts", ctext(r,6));
      sqlite3_finalize(r);
      cJSON *w = cJSON_CreateObject();
      cJSON_AddItemToObject(w, "data", o);
      out = cJSON_PrintUnformatted(w); cJSON_Delete(w);
      *st = 201;
      goto done;
    }
    out = err(st, 404, "not_found"); goto done;
  }

  /* ── /api/cases/:id/members ─────────────────────────────────────────── */
  if (!strcmp(act, "members")) {
    if (is_get) {
      cJSON *w = cJSON_CreateObject();
      cJSON_AddItemToObject(w, "data", member_array(db, seg));
      out = cJSON_PrintUnformatted(w); cJSON_Delete(w);
      *st = 200; goto done;
    }
    if (is_put) {
      if (!can_manage_roster(db, t, seg)) { out = err(st, 403, "forbidden"); goto done; }
      cJSON *list = NULL;
      if (jb && cJSON_IsArray(jb)) list = jb;
      else if (jb && cJSON_IsObject(jb)) {
        cJSON *m = cJSON_GetObjectItem(jb, "members");
        if (m && cJSON_IsArray(m)) list = m;
      }
      if (!list) { out = err(st, 400, "members array required"); goto done; }
      if (cJSON_GetArraySize(list) > 200) { out = err(st, 400, "too many members"); goto done; }

      /* Validate the WHOLE roster before writing anything — a half-applied
       * roster is an access-control state nobody asked for. */
      cJSON *e;
      cJSON_ArrayForEach(e, list) {
        const char *uid = NULL, *role = "contributor";
        if (cJSON_IsString(e)) uid = e->valuestring;
        else if (cJSON_IsObject(e)) {
          uid = jstr(e, "user_id");
          const char *r = jstr(e, "role");
          if (r) role = r;
        }
        if (!uid || !*uid) { out = err(st, 400, "member user_id required"); goto done; }
        if (!valid_case_role(role)) { out = err(st, 400, "invalid_case_role"); goto done; }
        if (!is_tenant_member(db, t->tenant_id, uid)) {
          out = err(st, 400, "member not in tenant"); goto done;
        }
      }

      sqlite3_exec(db->h, "BEGIN", NULL, NULL, NULL);
      int ok = 1;
      {
        sqlite3_stmt *s = NULL;
        if (sqlite3_prepare_v2(db->h, "DELETE FROM case_members WHERE case_id=?1",
                               -1, &s, NULL) != SQLITE_OK) ok = 0;
        else {
          sqlite3_bind_text(s, 1, seg, -1, SQLITE_TRANSIENT);
          if (sqlite3_step(s) != SQLITE_DONE) ok = 0;
        }
        sqlite3_finalize(s);
      }
      int count = 0;
      cJSON_ArrayForEach(e, list) {
        if (!ok) break;
        const char *uid = NULL, *role = "contributor";
        if (cJSON_IsString(e)) uid = e->valuestring;
        else { uid = jstr(e, "user_id");
               const char *r = jstr(e, "role"); if (r) role = r; }
        sqlite3_stmt *s = NULL;
        if (sqlite3_prepare_v2(db->h,
              "INSERT OR REPLACE INTO case_members (case_id,user_id,role) "
              "VALUES (?1,?2,?3)", -1, &s, NULL) != SQLITE_OK) ok = 0;
        else {
          sqlite3_bind_text(s, 1, seg,  -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(s, 2, uid,  -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(s, 3, role, -1, SQLITE_TRANSIENT);
          if (sqlite3_step(s) != SQLITE_DONE) ok = 0; else count++;
        }
        sqlite3_finalize(s);
      }
      sqlite3_exec(db->h, ok ? "COMMIT" : "ROLLBACK", NULL, NULL, NULL);
      if (!ok) { out = err(st, 500, "server_error"); goto done; }

      char msg[64];
      snprintf(msg, sizeof msg, "roster set to %d member(s)", count);
      activity_add(db, seg, t->user_id, "members_changed", msg, NULL, NULL);
      touch_case(db, t->tenant_id, seg);
      {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddNumberToObject(p, "count", count);
        char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
        audit_write(db, t->tenant_id, t->user_id, "case.members.set", seg, pj);
        free(pj);
      }
      cJSON *w = cJSON_CreateObject();
      cJSON_AddItemToObject(w, "data", member_array(db, seg));
      out = cJSON_PrintUnformatted(w); cJSON_Delete(w);
      *st = 200; goto done;
    }
    out = err(st, 404, "not_found"); goto done;
  }

  out = err(st, 404, "not_found");

done:
  if (jb) cJSON_Delete(jb);
  return out;
}
