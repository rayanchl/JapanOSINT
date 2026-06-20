#include "tenantapi.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static void add_str_or_null(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}

/* RFC-4122 v4 UUID (values need only be valid+unique, not Node-identical). */
static void uuid4(char out[37]) {
  unsigned char b[16];
  RAND_bytes(b, 16);
  b[6] = (b[6] & 0x0F) | 0x40;
  b[8] = (b[8] & 0x3F) | 0x80;
  snprintf(out, 37,
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
    b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
}

/* slugify(email) === middleware/tenant.js: lower, drop @…, [^a-z0-9]+→-,
 * trim -, cap 40, fallback "user"; on tenants.slug collision append -<6hex>. */
static void slugify(sqlite3 *h, const char *email, char *out, size_t cap) {
  char base[64]; size_t bi = 0;
  int prev_dash = 0;
  for (const char *p = email ? email : "user"; *p && bi < sizeof base - 1; p++) {
    char c = *p;
    if (c == '@') break;
    if (c >= 'A' && c <= 'Z') c += 32;
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      base[bi++] = c; prev_dash = 0;
    } else if (!prev_dash) {
      base[bi++] = '-'; prev_dash = 1;
    }
  }
  base[bi] = 0;
  /* trim leading/trailing '-' */
  char *s = base; while (*s == '-') s++;
  size_t sl = strlen(s); while (sl && s[sl-1] == '-') s[--sl] = 0;
  if (sl > 40) { s[40] = 0; sl = 40; }
  if (!*s) { s = (char *)"user"; }

  sqlite3_stmt *st;
  int taken = 0;
  if (sqlite3_prepare_v2(h, "SELECT 1 FROM tenants WHERE slug=?1", -1, &st, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(st, 1, s, -1, SQLITE_TRANSIENT);
    taken = sqlite3_step(st) == SQLITE_ROW;
  }
  sqlite3_finalize(st);
  if (!taken) { snprintf(out, cap, "%s", s); return; }
  char u[37]; uuid4(u); u[6] = 0;
  snprintf(out, cap, "%s-%s", s, u);
}

int tenant_resolve(db_handle *db, const auth_user *u,
                   const char *x_tenant_id, tenant_ctx *out) {
  if (!u || !u->id[0]) return -401;
  sqlite3 *h = db->h;
  sqlite3_stmt *st;

  /* ensureUser */
  if (sqlite3_prepare_v2(h,
        "SELECT id,email,display_name FROM users WHERE supabase_user_id=?1",
        -1, &st, NULL) != SQLITE_OK) return -500;
  sqlite3_bind_text(st, 1, u->id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(st) != SQLITE_ROW) {
    sqlite3_finalize(st);
    char lid[37]; uuid4(lid);
    char mail[256];
    if (u->email[0]) snprintf(mail, sizeof mail, "%s", u->email);
    else snprintf(mail, sizeof mail, "%s@unknown.local", u->id);
    if (sqlite3_prepare_v2(h,
          "INSERT INTO users (id,supabase_user_id,email) VALUES (?1,?2,?3) "
          "ON CONFLICT(supabase_user_id) DO NOTHING", -1, &st, NULL) != SQLITE_OK)
      return -500;
    sqlite3_bind_text(st, 1, lid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, u->id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, mail, -1, SQLITE_TRANSIENT);
    sqlite3_step(st); sqlite3_finalize(st);
    if (sqlite3_prepare_v2(h,
          "SELECT id,email,display_name FROM users WHERE supabase_user_id=?1",
          -1, &st, NULL) != SQLITE_OK) return -500;
    sqlite3_bind_text(st, 1, u->id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); return -500; }
  }
  snprintf(out->user_id, sizeof out->user_id, "%s",
           (const char *)sqlite3_column_text(st, 0));
  snprintf(out->email, sizeof out->email, "%s",
           ctext(st,1) ? (const char *)sqlite3_column_text(st,1) : "");
  const char *dn = ctext(st, 2);
  out->has_display = dn != NULL;
  snprintf(out->display_name, sizeof out->display_name, "%s", dn ? dn : "");
  sqlite3_finalize(st);

  /* Claim any pending invites addressed to this email: a freshly-invited
   * user becomes a member of the inviting workspace on their first
   * authenticated request, and the invite is consumed. */
  if (out->email[0]) {
    char itids[16][64], iroles[16][32]; int nv = 0;
    if (sqlite3_prepare_v2(h,
          "SELECT tenant_id,role FROM tenant_invites WHERE lower(email)=lower(?1)",
          -1, &st, NULL) == SQLITE_OK) {
      sqlite3_bind_text(st, 1, out->email, -1, SQLITE_TRANSIENT);
      while (sqlite3_step(st) == SQLITE_ROW && nv < 16) {
        snprintf(itids[nv], sizeof itids[nv], "%s",
                 (const char *)sqlite3_column_text(st, 0));
        snprintf(iroles[nv], sizeof iroles[nv], "%s",
                 (const char *)sqlite3_column_text(st, 1));
        nv++;
      }
    }
    sqlite3_finalize(st);
    for (int i = 0; i < nv; i++) {
      if (sqlite3_prepare_v2(h,
            "INSERT OR IGNORE INTO memberships (user_id,tenant_id,role) "
            "VALUES (?1,?2,?3)", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, out->user_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, itids[i], -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, iroles[i], -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
      }
      sqlite3_finalize(st);
    }
    if (nv > 0) {
      if (sqlite3_prepare_v2(h,
            "DELETE FROM tenant_invites WHERE lower(email)=lower(?1)",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, out->email, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
      }
      sqlite3_finalize(st);
    }
  }

  /* listMemberships; provision personal tenant on first sign-in */
  int has_membership = 0;
  if (sqlite3_prepare_v2(h,
        "SELECT tenant_id,role FROM memberships WHERE user_id=?1 "
        "ORDER BY created_at ASC", -1, &st, NULL) != SQLITE_OK) return -500;
  sqlite3_bind_text(st, 1, out->user_id, -1, SQLITE_TRANSIENT);
  char first_tid[64] = {0}, first_role[32] = {0}, picked_tid[64] = {0},
       picked_role[32] = {0};
  while (sqlite3_step(st) == SQLITE_ROW) {
    const char *tid = (const char *)sqlite3_column_text(st, 0);
    const char *role = (const char *)sqlite3_column_text(st, 1);
    if (!has_membership) {
      snprintf(first_tid, sizeof first_tid, "%s", tid);
      snprintf(first_role, sizeof first_role, "%s", role);
    }
    if (x_tenant_id && *x_tenant_id && strcmp(tid, x_tenant_id) == 0) {
      snprintf(picked_tid, sizeof picked_tid, "%s", tid);
      snprintf(picked_role, sizeof picked_role, "%s", role);
    }
    has_membership = 1;
  }
  sqlite3_finalize(st);

  if (!has_membership) {
    char tid[37]; uuid4(tid);
    char slug[160]; slugify(h, out->email, slug, sizeof slug);
    char name[400]; snprintf(name, sizeof name, "%s's workspace", out->email);
    if (sqlite3_prepare_v2(h,
          "INSERT INTO tenants (id,slug,name,plan) VALUES (?1,?2,?3,'free')",
          -1, &st, NULL) != SQLITE_OK) return -500;
    sqlite3_bind_text(st,1,tid,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,2,slug,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,3,name,-1,SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_DONE) { sqlite3_finalize(st); return -500; }
    sqlite3_finalize(st);
    if (sqlite3_prepare_v2(h,
          "INSERT OR IGNORE INTO memberships (user_id,tenant_id,role) "
          "VALUES (?1,?2,'owner')", -1, &st, NULL) != SQLITE_OK) return -500;
    sqlite3_bind_text(st,1,out->user_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(st,2,tid,-1,SQLITE_TRANSIENT);
    sqlite3_step(st); sqlite3_finalize(st);
    snprintf(first_tid, sizeof first_tid, "%s", tid);
    snprintf(first_role, sizeof first_role, "owner");
  }

  const char *use_tid  = picked_tid[0]  ? picked_tid  : first_tid;
  const char *use_role = picked_tid[0]  ? picked_role : first_role;

  if (sqlite3_prepare_v2(h,
        "SELECT id,slug,name,plan,require_sso FROM tenants WHERE id=?1",
        -1, &st, NULL) != SQLITE_OK) return -500;
  sqlite3_bind_text(st, 1, use_tid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); return -500; }
  snprintf(out->tenant_id, sizeof out->tenant_id, "%s",
           (const char *)sqlite3_column_text(st,0));
  snprintf(out->slug, sizeof out->slug, "%s",
           (const char *)sqlite3_column_text(st,1));
  snprintf(out->tname, sizeof out->tname, "%s",
           (const char *)sqlite3_column_text(st,2));
  snprintf(out->plan, sizeof out->plan, "%s",
           (const char *)sqlite3_column_text(st,3));
  out->require_sso = sqlite3_column_int(st,4);
  sqlite3_finalize(st);
  snprintf(out->role, sizeof out->role, "%s", use_role);
  return 0;
}

char *tenantapi_me(db_handle *db, const tenant_ctx *t) {
  cJSON *user = cJSON_CreateObject();
  cJSON_AddStringToObject(user, "id", t->user_id);
  cJSON_AddStringToObject(user, "email", t->email);
  cJSON_AddItemToObject(user, "display_name",
    t->has_display ? cJSON_CreateString(t->display_name) : cJSON_CreateNull());

  cJSON *tn = cJSON_CreateObject();
  cJSON_AddStringToObject(tn, "id", t->tenant_id);
  cJSON_AddStringToObject(tn, "slug", t->slug);
  cJSON_AddStringToObject(tn, "name", t->tname);
  cJSON_AddStringToObject(tn, "plan", t->plan);
  cJSON_AddItemToObject(tn, "role",
    t->role[0] ? cJSON_CreateString(t->role) : cJSON_CreateNull());

  cJSON *ms = cJSON_CreateArray();
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h,
        "SELECT t.id,t.slug,t.name,t.plan,m.role FROM memberships m "
        "JOIN tenants t ON t.id=m.tenant_id WHERE m.user_id=?1 "
        "ORDER BY m.created_at ASC", -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, t->user_id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
      cJSON *r = cJSON_CreateObject();
      cJSON_AddStringToObject(r, "id",   (const char *)sqlite3_column_text(st,0));
      cJSON_AddStringToObject(r, "slug", (const char *)sqlite3_column_text(st,1));
      cJSON_AddStringToObject(r, "name", (const char *)sqlite3_column_text(st,2));
      cJSON_AddStringToObject(r, "plan", (const char *)sqlite3_column_text(st,3));
      cJSON_AddStringToObject(r, "role", (const char *)sqlite3_column_text(st,4));
      cJSON_AddItemToArray(ms, r);
    }
  }
  sqlite3_finalize(st);

  cJSON *o = cJSON_CreateObject();
  cJSON_AddItemToObject(o, "user", user);
  cJSON_AddItemToObject(o, "tenant", tn);
  cJSON_AddItemToObject(o, "memberships", ms);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

char *tenantapi_audit_list(db_handle *db, const char *tenant_id,
                           int limit, int offset) {
  int lim = limit; if (lim < 1) lim = 1; if (lim > 500) lim = 500;
  if (offset < 0) offset = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT id,user_id,action,target,ts,ip,ua,prev_hash,row_hash,chain_seq "
        "FROM audit_events WHERE tenant_id=?1 "
        "ORDER BY chain_seq DESC LIMIT ?2 OFFSET ?3", -1, &s, NULL) != SQLITE_OK)
    return NULL;
  sqlite3_bind_text(s, 1, tenant_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, lim);
  sqlite3_bind_int(s, 3, offset);
  static const char *K[] = {"id","user_id","action","target","ts","ip","ua",
                            "prev_hash","row_hash","chain_seq"};
  cJSON *arr = cJSON_CreateArray();
  int count = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *r = cJSON_CreateObject();
    for (int i = 0; i < 10; i++) {
      if (sqlite3_column_type(s, i) == SQLITE_NULL)
        cJSON_AddNullToObject(r, K[i]);
      else if (i == 9)
        cJSON_AddNumberToObject(r, K[i], (double)sqlite3_column_int64(s, i));
      else
        cJSON_AddStringToObject(r, K[i],
          (const char *)sqlite3_column_text(s, i));
    }
    cJSON_AddItemToArray(arr, r);
    count++;
  }
  sqlite3_finalize(s);

  cJSON *page = cJSON_CreateObject();
  cJSON_AddNumberToObject(page, "limit", lim);
  cJSON_AddNumberToObject(page, "offset", offset);
  cJSON_AddNumberToObject(page, "count", count);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddItemToObject(o, "data", arr);
  cJSON_AddItemToObject(o, "page", page);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* ── auditChain.verifyAuditChain ──────────────────────────────────────────
 * row_hash = sha256Hex(canonicalJson({sorted keys})). canonicalJson sorts
 * keys lexicographically; with the fixed schema the order is constant, so
 * we add them to a cJSON object in that order and PrintUnformatted (==
 * JSON.stringify; cJSON string-escaping matches for our data). */
static void sha256_hex(const char *in, char out[65]) {
  unsigned char d[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char *)in, strlen(in), d);
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    snprintf(out + i*2, 3, "%02x", d[i]);
}
static void put(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}
/* keys sorted: action,chain_seq,id,ip,payload_json,prev_hash,target,
 * tenant_id,ts,ua,user_id */
static void row_hash_of(const char *id, const char *tenant_id,
    const char *user_id, const char *action, const char *target,
    const char *payload_json, const char *ts, const char *ip,
    const char *ua, const char *prev_hash, long chain_seq, char out[65]) {
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
  sha256_hex(cj, out);
  free(cj);
}

char *tenantapi_audit_verify(db_handle *db, const char *tenant_id) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT id,tenant_id,user_id,action,target,payload_json,ts,ip,ua,"
        "prev_hash,row_hash,chain_seq FROM audit_events "
        "WHERE tenant_id=?1 AND row_hash IS NOT NULL "
        "ORDER BY chain_seq ASC LIMIT 100000", -1, &s, NULL) != SQLITE_OK)
    return NULL;
  sqlite3_bind_text(s, 1, tenant_id, -1, SQLITE_TRANSIENT);

  /* materialise rows (need count up-front for the broken-case payload) */
  typedef struct { char *id,*tid,*uid,*act,*tgt,*pj,*ts,*ip,*ua,*ph,*rh;
                   long seq; } row_t;
  row_t *R = NULL; int n = 0, cap = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    if (n == cap) { cap = cap ? cap*2 : 64; R = realloc(R, cap*sizeof *R); }
    row_t *r = &R[n++];
    #define DUP(idx) (ctext(s,idx) ? strdup((const char*)sqlite3_column_text(s,idx)) : NULL)
    r->id=DUP(0); r->tid=DUP(1); r->uid=DUP(2); r->act=DUP(3); r->tgt=DUP(4);
    r->pj=DUP(5); r->ts=DUP(6); r->ip=DUP(7); r->ua=DUP(8); r->ph=DUP(9);
    r->rh=DUP(10); r->seq=sqlite3_column_int64(s,11);
    #undef DUP
  }
  sqlite3_finalize(s);

  cJSON *o = cJSON_CreateObject();
  const char *expected_prev = "GENESIS";
  long expected_seq = 1;
  int broken = -1; const char *reason = NULL; char rbuf[96];
  for (int i = 0; i < n; i++) {
    row_t *r = &R[i];
    if (r->seq != expected_seq) {
      snprintf(rbuf, sizeof rbuf, "chain_seq gap (expected %ld, got %ld)",
               expected_seq, r->seq);
      reason = rbuf; broken = i; break;
    }
    if (strcmp(r->ph ? r->ph : "", expected_prev) != 0) {
      reason = "prev_hash linkage mismatch (row inserted/removed)";
      broken = i; break;
    }
    char rh[65];
    row_hash_of(r->id, r->tid, r->uid, r->act, r->tgt, r->pj, r->ts,
                r->ip, r->ua, r->ph, r->seq, rh);
    if (strcmp(rh, r->rh ? r->rh : "") != 0) {
      reason = "row_hash mismatch (row altered)"; broken = i; break;
    }
    expected_prev = r->rh;
    expected_seq += 1;
  }

  if (broken >= 0) {
    cJSON_AddBoolToObject(o, "ok", 0);
    cJSON_AddNumberToObject(o, "count", n);
    cJSON_AddNumberToObject(o, "brokenAt", broken);
    add_str_or_null(o, "brokenId", R[broken].id);
    cJSON_AddStringToObject(o, "reason", reason);
  } else {
    cJSON_AddBoolToObject(o, "ok", 1);
    cJSON_AddNumberToObject(o, "count", n);
    cJSON_AddItemToObject(o, "tip",
      strcmp(expected_prev, "GENESIS") == 0 ? cJSON_CreateNull()
                                            : cJSON_CreateString(expected_prev));
  }
  for (int i = 0; i < n; i++) {
    free(R[i].id); free(R[i].tid); free(R[i].uid); free(R[i].act);
    free(R[i].tgt); free(R[i].pj); free(R[i].ts); free(R[i].ip);
    free(R[i].ua); free(R[i].ph); free(R[i].rh);
  }
  free(R);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* ── /api/members — member roster + pending-invite management ───────────────
 * Tenant-scoped; writes require owner/admin role in the active tenant (NOT
 * the platform-operator gate). Routes (seg/act = path tail after
 * /api/members/, parsed in httpd.c):
 *   GET    (seg="")              → { members:[…], invites:[…] }
 *   POST   invite                → invite by email (or add now if user exists)
 *   PATCH  :userId/role          → change a member's role
 *   DELETE :userId               → remove a member
 *   DELETE invite/:id            → revoke a pending invite                  */

static int member_can_manage(const tenant_ctx *t) {
  return strcmp(t->role, "owner") == 0 || strcmp(t->role, "admin") == 0;
}
static int valid_role(const char *r) {
  return r && (!strcmp(r,"owner") || !strcmp(r,"admin") ||
               !strcmp(r,"analyst") || !strcmp(r,"viewer"));
}
static char *merr(int *status, int code, const char *msg) {
  *status = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}
static char *mok(int *status, int code, cJSON *o) {
  *status = code;
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}
/* Raw, unchained audit row (row_hash/chain_seq left NULL → excluded from
 * audit_verify, like break-glass logins). */
static void member_audit(sqlite3 *h, const tenant_ctx *t, const char *action,
                         const char *target, const char *payload_json) {
  char id[37]; uuid4(id);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
        "INSERT INTO audit_events (id,tenant_id,user_id,action,target,"
        "payload_json,ts) VALUES (?1,?2,?3,?4,?5,?6,datetime('now'))",
        -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, t->user_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 4, action, -1, SQLITE_TRANSIENT);
  if (target) sqlite3_bind_text(s, 5, target, -1, SQLITE_TRANSIENT);
  else        sqlite3_bind_null(s, 5);
  sqlite3_bind_text(s, 6, payload_json ? payload_json : "{}", -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
}
/* Count owners in the active tenant — used to block losing the last owner. */
static int owner_count(sqlite3 *h, const char *tenant_id) {
  sqlite3_stmt *s; int n = 0;
  if (sqlite3_prepare_v2(h,
        "SELECT COUNT(*) FROM memberships WHERE tenant_id=?1 AND role='owner'",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, tenant_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
  }
  sqlite3_finalize(s);
  return n;
}
/* Current role of a member, or NULL if not a member (buf must be ≥32). */
static const char *member_role(sqlite3 *h, const char *tenant_id,
                               const char *user_id, char *buf) {
  sqlite3_stmt *s; const char *out = NULL;
  if (sqlite3_prepare_v2(h,
        "SELECT role FROM memberships WHERE tenant_id=?1 AND user_id=?2",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, tenant_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, user_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      snprintf(buf, 32, "%s", (const char *)sqlite3_column_text(s, 0));
      out = buf;
    }
  }
  sqlite3_finalize(s);
  return out;
}

char *tenantapi_members(db_handle *db, const tenant_ctx *t,
                        const char *method, const char *seg,
                        const char *act, const char *body, int *status) {
  sqlite3 *h = db->h;

  /* GET /api/members — roster + pending invites (any member may read) */
  if (!seg[0] && strcmp(method, "GET") == 0) {
    cJSON *members = cJSON_CreateArray();
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(h,
          "SELECT u.id,u.email,m.role FROM memberships m JOIN users u "
          "ON u.id=m.user_id WHERE m.tenant_id=?1 ORDER BY m.created_at ASC",
          -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, t->tenant_id, -1, SQLITE_TRANSIENT);
      while (sqlite3_step(s) == SQLITE_ROW) {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddStringToObject(r, "user_id", (const char *)sqlite3_column_text(s,0));
        cJSON_AddStringToObject(r, "email",   (const char *)sqlite3_column_text(s,1));
        cJSON_AddStringToObject(r, "role",    (const char *)sqlite3_column_text(s,2));
        cJSON_AddItemToArray(members, r);
      }
    }
    sqlite3_finalize(s);
    cJSON *invites = cJSON_CreateArray();
    if (sqlite3_prepare_v2(h,
          "SELECT id,email,role,created_at FROM tenant_invites "
          "WHERE tenant_id=?1 ORDER BY created_at ASC", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, t->tenant_id, -1, SQLITE_TRANSIENT);
      while (sqlite3_step(s) == SQLITE_ROW) {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddStringToObject(r, "id",         (const char *)sqlite3_column_text(s,0));
        cJSON_AddStringToObject(r, "email",      (const char *)sqlite3_column_text(s,1));
        cJSON_AddStringToObject(r, "role",       (const char *)sqlite3_column_text(s,2));
        cJSON_AddStringToObject(r, "created_at", (const char *)sqlite3_column_text(s,3));
        cJSON_AddItemToArray(invites, r);
      }
    }
    sqlite3_finalize(s);
    cJSON *o = cJSON_CreateObject();
    cJSON_AddItemToObject(o, "members", members);
    cJSON_AddItemToObject(o, "invites", invites);
    return mok(status, 200, o);
  }

  /* All remaining routes mutate membership → owner/admin only. */
  if (!member_can_manage(t))
    return merr(status, 403, "Workspace owner or admin role required");

  /* POST /api/members/invite { email, role } */
  if (strcmp(seg, "invite") == 0 && !act[0] && strcmp(method, "POST") == 0) {
    cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
    cJSON *je = jb ? cJSON_GetObjectItem(jb, "email") : NULL;
    cJSON *jr = jb ? cJSON_GetObjectItem(jb, "role") : NULL;
    const char *em = (je && cJSON_IsString(je)) ? je->valuestring : NULL;
    const char *role = (jr && cJSON_IsString(jr)) ? jr->valuestring : "analyst";
    if (!em || !strchr(em, '@')) { if (jb) cJSON_Delete(jb);
      return merr(status, 400, "valid email required"); }
    if (!valid_role(role)) { if (jb) cJSON_Delete(jb);
      return merr(status, 400, "invalid role"); }
    char mail[256]; int mi = 0;
    for (const char *p = em; *p && mi < 255; p++) mail[mi++] = (char)tolower((unsigned char)*p);
    mail[mi] = 0;

    /* Already a user? Add membership immediately rather than queue an invite. */
    sqlite3_stmt *s; char existing_uid[64] = {0};
    if (sqlite3_prepare_v2(h, "SELECT id FROM users WHERE lower(email)=?1",
          -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, mail, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(s) == SQLITE_ROW)
        snprintf(existing_uid, sizeof existing_uid, "%s",
                 (const char *)sqlite3_column_text(s, 0));
    }
    sqlite3_finalize(s);

    cJSON *o = cJSON_CreateObject();
    if (existing_uid[0]) {
      char rb[32];
      if (member_role(h, t->tenant_id, existing_uid, rb)) {
        if (jb) cJSON_Delete(jb);
        cJSON_AddBoolToObject(o, "ok", 1);
        cJSON_AddStringToObject(o, "status", "already_member");
        return mok(status, 200, o);
      }
      if (sqlite3_prepare_v2(h,
            "INSERT OR IGNORE INTO memberships (user_id,tenant_id,role) "
            "VALUES (?1,?2,?3)", -1, &s, NULL) == SQLITE_OK) {
        sqlite3_bind_text(s, 1, existing_uid, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s, 3, role, -1, SQLITE_TRANSIENT);
        sqlite3_step(s);
      }
      sqlite3_finalize(s);
      member_audit(h, t, "member.add", mail, NULL);
      cJSON_AddBoolToObject(o, "ok", 1);
      cJSON_AddStringToObject(o, "status", "added");
      cJSON_AddStringToObject(o, "user_id", existing_uid);
      cJSON_AddStringToObject(o, "role", role);
      if (jb) cJSON_Delete(jb);
      return mok(status, 200, o);
    }

    /* Otherwise queue / refresh a pending invite. */
    char id[37]; uuid4(id);
    if (sqlite3_prepare_v2(h,
          "INSERT INTO tenant_invites (id,tenant_id,email,role,invited_by) "
          "VALUES (?1,?2,?3,?4,?5) ON CONFLICT(tenant_id,email) "
          "DO UPDATE SET role=excluded.role, invited_by=excluded.invited_by",
          -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 3, mail, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 4, role, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 5, t->user_id, -1, SQLITE_TRANSIENT);
      sqlite3_step(s);
    }
    sqlite3_finalize(s);
    member_audit(h, t, "invite.create", mail, NULL);
    cJSON_AddBoolToObject(o, "ok", 1);
    cJSON_AddStringToObject(o, "status", "invited");
    cJSON_AddStringToObject(o, "email", mail);
    cJSON_AddStringToObject(o, "role", role);
    if (jb) cJSON_Delete(jb);
    return mok(status, 200, o);
  }

  /* DELETE /api/members/invite/:id — revoke a pending invite */
  if (strcmp(seg, "invite") == 0 && act[0] && strcmp(method, "DELETE") == 0) {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(h,
          "DELETE FROM tenant_invites WHERE id=?1 AND tenant_id=?2",
          -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, act, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
      sqlite3_step(s);
    }
    sqlite3_finalize(s);
    member_audit(h, t, "invite.revoke", act, NULL);
    cJSON *o = cJSON_CreateObject(); cJSON_AddBoolToObject(o, "ok", 1);
    return mok(status, 200, o);
  }

  /* PATCH /api/members/:userId/role { role } */
  if (seg[0] && strcmp(act, "role") == 0 && strcmp(method, "PATCH") == 0) {
    cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
    cJSON *jr = jb ? cJSON_GetObjectItem(jb, "role") : NULL;
    const char *role = (jr && cJSON_IsString(jr)) ? jr->valuestring : NULL;
    if (!valid_role(role)) { if (jb) cJSON_Delete(jb);
      return merr(status, 400, "invalid role"); }
    char cur[32];
    if (!member_role(h, t->tenant_id, seg, cur)) { if (jb) cJSON_Delete(jb);
      return merr(status, 404, "member not found"); }
    if (strcmp(cur, "owner") == 0 && strcmp(role, "owner") != 0 &&
        owner_count(h, t->tenant_id) <= 1) {
      if (jb) cJSON_Delete(jb);
      return merr(status, 400, "cannot demote the last owner");
    }
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(h,
          "UPDATE memberships SET role=?1 WHERE tenant_id=?2 AND user_id=?3",
          -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, role, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 3, seg, -1, SQLITE_TRANSIENT);
      sqlite3_step(s);
    }
    sqlite3_finalize(s);
    member_audit(h, t, "member.role", seg, NULL);
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "ok", 1);
    cJSON_AddStringToObject(o, "user_id", seg);
    cJSON_AddStringToObject(o, "role", role);
    if (jb) cJSON_Delete(jb);
    return mok(status, 200, o);
  }

  /* DELETE /api/members/:userId — remove a member */
  if (seg[0] && !act[0] && strcmp(method, "DELETE") == 0) {
    char cur[32];
    if (!member_role(h, t->tenant_id, seg, cur))
      return merr(status, 404, "member not found");
    if (strcmp(cur, "owner") == 0 && owner_count(h, t->tenant_id) <= 1)
      return merr(status, 400, "cannot remove the last owner");
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(h,
          "DELETE FROM memberships WHERE tenant_id=?1 AND user_id=?2",
          -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, t->tenant_id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, seg, -1, SQLITE_TRANSIENT);
      sqlite3_step(s);
    }
    sqlite3_finalize(s);
    member_audit(h, t, "member.remove", seg, NULL);
    cJSON *o = cJSON_CreateObject(); cJSON_AddBoolToObject(o, "ok", 1);
    return mok(status, 200, o);
  }

  return merr(status, 404, "not_found");
}
