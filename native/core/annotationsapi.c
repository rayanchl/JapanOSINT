/* core/annotationsapi.c — roadmap 15: analyst notes & annotations.
 *
 * Shaped after core/alertsapi.c (one dispatcher, one table, tenant-scoped),
 * with three deliberate departures documented in annotationsapi.h and
 * restated at their call sites here: soft delete only, author-only edit, and
 * raw (unsanitized, unrendered) markdown storage.
 *
 * Every statement in this file carries `tenant_id=?`, including fetch-by-id.
 * The id is a uuid and therefore unguessable, but "unguessable" is not an
 * access control: ids leak through exports, logs, and shared links, so the
 * tenant predicate is what actually keeps tenants apart. */
#include "annotationsapi.h"
#include "audit.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/rand.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 64 KB of markdown is far more than a note and far less than a document —
 * past this the analyst wants an attachment, not an annotation. Enforced on
 * the trimmed body so trailing newlines never push a note over the edge. */
#define ANN_BODY_MAX  65536
#define ANN_REF_ID_MAX 512
#define ANN_CASE_ID_MAX 128

/* Column order shared by every read path so rows can be decoded by fixed
 * index. tenant_id is deliberately absent: it is a predicate, never output. */
#define ANN_COLS \
  "id,case_id,ref_type,ref_id,body_md,author_id,created_at,updated_at,deleted_at"

/* ── shared idioms (verbatim from alertsapi.c / intelapi.c) ───────────── */
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
/* Inverse of b64url(); NULL on an invalid char → caller ignores the cursor
 * rather than erroring, matching intelapi's bad-cursor stance. */
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

/* ── querystring (same shape as transitapi.c's qget) ──────────────────── */
static int hexv(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
static void url_decode(const char *s, size_t n, char *out, size_t cap) {
  size_t o = 0;
  for (size_t i = 0; i < n && o + 1 < cap; i++) {
    int c = (unsigned char)s[i];
    if (c == '+') { out[o++] = ' '; continue; }
    if (c == '%' && i + 2 < n) {
      int h = hexv((unsigned char)s[i + 1]), l = hexv((unsigned char)s[i + 2]);
      if (h >= 0 && l >= 0) { out[o++] = (char)((h << 4) | l); i += 2; continue; }
    }
    out[o++] = (char)c;
  }
  out[o] = 0;
}
/* Extract param `key` into `out` (URL-decoded, truncated to cap). 1 if the
 * key is present (even with an empty value), else 0 with out[0]=0. */
static int qget(const char *query, const char *key, char *out, size_t cap) {
  out[0] = 0;
  if (!query || !*query) return 0;
  size_t klen = strlen(key);
  const char *p = query;
  while (*p) {
    const char *amp = strchr(p, '&');
    const char *seg_end = amp ? amp : p + strlen(p);
    const char *eq = memchr(p, '=', (size_t)(seg_end - p));
    size_t kn = eq ? (size_t)(eq - p) : (size_t)(seg_end - p);
    if (kn == klen && memcmp(p, key, klen) == 0) {
      const char *vstart = eq ? eq + 1 : seg_end;
      url_decode(vstart, (size_t)(seg_end - vstart), out, cap);
      return 1;
    }
    if (!amp) break;
    p = amp + 1;
  }
  return 0;
}
static int truthy(const char *v) {
  return v && (!strcmp(v,"1") || !strcmp(v,"true") || !strcmp(v,"yes"));
}

/* ── roles ────────────────────────────────────────────────────────────── */
/* owner > admin > analyst > viewer; an unrecognised role ranks 0 and can do
 * nothing, so a future role name fails closed instead of inheriting read. */
static int role_rank(const char *r) {
  if (!r) return 0;
  if (!strcmp(r, "owner"))   return 4;
  if (!strcmp(r, "admin"))   return 3;
  if (!strcmp(r, "analyst")) return 2;
  if (!strcmp(r, "viewer"))  return 1;
  return 0;
}

int annotations_ref_type_valid(const char *s) {
  if (!s || !*s) return 0;
  return !strcmp(s,"intel_item") || !strcmp(s,"entity") ||
         !strcmp(s,"breach_item") || !strcmp(s,"feature") ||
         !strcmp(s,"camera") || !strcmp(s,"search_run");
}

/* ── text helpers ─────────────────────────────────────────────────────── */
/* Trim ASCII whitespace without copying — body_md can be 64 KB, so the
 * trimmed span is bound by (ptr,len) rather than memmove'd into a buffer. */
static const char *trim_span(const char *s, size_t *len) {
  if (!s) { *len = 0; return ""; }
  while (*s && isspace((unsigned char)*s)) s++;
  size_t n = strlen(s);
  while (n && isspace((unsigned char)s[n - 1])) n--;
  *len = n; return s;
}

/* Validates + trims body_md. Returns NULL on success, setting `out` and
 * `out_len` to a span inside `jb`'s storage (valid until jb is deleted);
 * otherwise returns a snake_case error code. */
static const char *check_body_md(cJSON *jb, const char **out, size_t *out_len) {
  cJSON *j = jb ? cJSON_GetObjectItem(jb, "body_md") : NULL;
  if (!j || !cJSON_IsString(j)) return "body_md_required";
  size_t n; const char *p = trim_span(j->valuestring, &n);
  if (n == 0) return "body_md_empty";
  if (n > ANN_BODY_MAX) return "body_md_too_long";
  *out = p; *out_len = n;
  return NULL;
}

/* ── row decode / single-row envelope ─────────────────────────────────── */
static cJSON *decode_row(sqlite3_stmt *s) {
  /* cols: id,case_id,ref_type,ref_id,body_md,author_id,created_at,
   *       updated_at,deleted_at */
  const char *del = ctext(s, 8);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "id", (const char *)sqlite3_column_text(s, 0));
  add_str_or_null(o, "case_id", ctext(s, 1));
  cJSON_AddStringToObject(o, "ref_type", (const char *)sqlite3_column_text(s, 2));
  cJSON_AddStringToObject(o, "ref_id", (const char *)sqlite3_column_text(s, 3));
  /* body_md verbatim — see the header: rendering and escaping belong to the
   * client, which is the only layer that knows its own sink. */
  cJSON_AddStringToObject(o, "body_md", (const char *)sqlite3_column_text(s, 4));
  add_str_or_null(o, "author_id", ctext(s, 5));
  cJSON_AddStringToObject(o, "created_at", (const char *)sqlite3_column_text(s, 6));
  add_str_or_null(o, "updated_at", ctext(s, 7));
  add_str_or_null(o, "deleted_at", del);
  cJSON_AddBoolToObject(o, "is_deleted", del != NULL);
  return o;
}

/* {"data":{…}} for one note. `allow_deleted`=0 hides tombstones as 404 (the
 * viewer-facing read); mutation responses pass 1 so the caller can see the
 * row it just tombstoned. */
static char *one_note(db_handle *db, const char *tid, const char *id,
                      int allow_deleted, int code, int *st) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT " ANN_COLS " FROM annotations WHERE id=?1 AND tenant_id=?2",
        -1, &s, NULL) != SQLITE_OK)
    return err(st, 500, "server_error");
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tid, -1, SQLITE_TRANSIENT);
  char *out;
  if (sqlite3_step(s) == SQLITE_ROW &&
      (allow_deleted || ctext(s, 8) == NULL)) {
    cJSON *w = cJSON_CreateObject();
    cJSON_AddItemToObject(w, "data", decode_row(s));
    out = cJSON_PrintUnformatted(w); cJSON_Delete(w); *st = code;
  } else {
    out = err(st, 404, "not_found");
  }
  sqlite3_finalize(s);
  return out;
}

/* Enough of a row to make an authorization decision + an audit payload,
 * without hauling a 64 KB body around. 1 = found (in this tenant). */
typedef struct {
  char author[64];  int has_author;
  int  deleted;
  char ref_type[48];
  char ref_id[ANN_REF_ID_MAX + 8];
} ann_meta;

static int load_meta(db_handle *db, const char *tid, const char *id,
                     ann_meta *m) {
  memset(m, 0, sizeof *m);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT author_id,deleted_at,ref_type,ref_id FROM annotations "
        "WHERE id=?1 AND tenant_id=?2", -1, &s, NULL) != SQLITE_OK)
    return 0;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tid, -1, SQLITE_TRANSIENT);
  int found = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    found = 1;
    const char *a = ctext(s, 0);
    if (a) { snprintf(m->author, sizeof m->author, "%s", a); m->has_author = 1; }
    m->deleted = ctext(s, 1) != NULL;
    snprintf(m->ref_type, sizeof m->ref_type, "%s", ctext(s, 2) ? ctext(s, 2) : "");
    snprintf(m->ref_id,  sizeof m->ref_id,  "%s", ctext(s, 3) ? ctext(s, 3) : "");
  }
  sqlite3_finalize(s);
  return found;
}

static int is_author(const ann_meta *m, const tenant_ctx *t) {
  return m->has_author && t->user_id[0] && strcmp(m->author, t->user_id) == 0;
}

/* malloc'd audit payload. body_md is NEVER included: audit_events is read by
 * a wider audience than the note itself, and duplicating evidence text into a
 * second table would put the same content under two different retention and
 * redaction regimes. The length is enough to show an edit happened. */
static char *audit_payload(const char *ref_type, const char *ref_id,
                           const char *case_id, long body_len,
                           const char *extra_k, const char *extra_v) {
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "ref_type", ref_type ? ref_type : "");
  cJSON_AddStringToObject(p, "ref_id", ref_id ? ref_id : "");
  add_str_or_null(p, "case_id", case_id);
  if (body_len >= 0) cJSON_AddNumberToObject(p, "body_len", (double)body_len);
  if (extra_k) add_str_or_null(p, extra_k, extra_v);
  char *j = cJSON_PrintUnformatted(p); cJSON_Delete(p); return j;
}

/* ── GET /api/annotations ─────────────────────────────────────────────── */
static char *list_notes(db_handle *db, const tenant_ctx *t,
                        const char *qs, int *st) {
  char v_rt[48] = {0}, v_rid[ANN_REF_ID_MAX + 8] = {0},
       v_cid[ANN_CASE_ID_MAX + 8] = {0}, v_lim[24] = {0},
       v_cur[768] = {0}, v_inc[16] = {0};
  qget(qs, "ref_type", v_rt, sizeof v_rt);
  qget(qs, "ref_id", v_rid, sizeof v_rid);
  qget(qs, "case_id", v_cid, sizeof v_cid);
  qget(qs, "limit", v_lim, sizeof v_lim);
  qget(qs, "cursor", v_cur, sizeof v_cur);
  qget(qs, "include_deleted", v_inc, sizeof v_inc);

  if (v_rt[0] && !annotations_ref_type_valid(v_rt))
    return err(st, 400, "invalid_ref_type");

  int lim = v_lim[0] ? atoi(v_lim) : 50;
  if (lim <= 0) lim = 50;
  if (lim > 200) lim = 200;

  /* Tombstones are analyst-and-up. A viewer asking for them is not an error
   * — the filter is simply not applied, and meta echoes what we actually
   * did, so a client never has to guess whether it got the full set. */
  int show_deleted = truthy(v_inc) && role_rank(t->role) >= 2;

  /* keyset cursor {"p":created_at,"u":id}; malformed → ignored, page 1. */
  char *cur_p = NULL, *cur_u = NULL; cJSON *curj = NULL;
  if (v_cur[0]) {
    char *dec = b64url_decode(v_cur);
    if (dec) {
      curj = cJSON_Parse(dec); free(dec);
      if (curj) {
        cJSON *jp = cJSON_GetObjectItem(curj, "p");
        cJSON *ju = cJSON_GetObjectItem(curj, "u");
        if (cJSON_IsString(jp) && cJSON_IsString(ju)) {
          cur_p = jp->valuestring; cur_u = ju->valuestring;
        }
      }
    }
  }

  char wbuf[512]; size_t wl = 0; wbuf[0] = 0;
  const char *bnd[8]; int nb = 0;
#define WPUSH(frag) do { int _n = snprintf(wbuf + wl, sizeof wbuf - wl, \
    " AND %s", (frag)); if (_n > 0) wl += (size_t)_n; \
    if (wl >= sizeof wbuf) wl = sizeof wbuf - 1; } while (0)
  if (v_rt[0])  { WPUSH("ref_type=?"); bnd[nb++] = v_rt; }
  if (v_rid[0]) { WPUSH("ref_id=?");   bnd[nb++] = v_rid; }
  if (v_cid[0]) { WPUSH("case_id=?");  bnd[nb++] = v_cid; }
  if (!show_deleted) WPUSH("deleted_at IS NULL");
  /* created_at is second-resolution (datetime('now')), so a burst of notes
   * shares a timestamp — the id tiebreaker is what makes the page boundary
   * stable rather than a source of dropped/duplicated rows. */
  if (cur_p && cur_u) {
    WPUSH("(created_at < ? OR (created_at = ? AND id > ?))");
    bnd[nb++] = cur_p; bnd[nb++] = cur_p; bnd[nb++] = cur_u;
  }
#undef WPUSH

  char sql[1024];
  snprintf(sql, sizeof sql,
    "SELECT " ANN_COLS " FROM annotations WHERE tenant_id=?%s "
    "ORDER BY created_at DESC, id ASC LIMIT ?", wbuf);

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    if (curj) cJSON_Delete(curj);
    return err(st, 500, "server_error");
  }
  int bi = 1;
  sqlite3_bind_text(s, bi++, t->tenant_id, -1, SQLITE_TRANSIENT);
  for (int i = 0; i < nb; i++)
    sqlite3_bind_text(s, bi++, bnd[i], -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, bi++, lim);

  cJSON *data = cJSON_CreateArray();
  int n = 0;
  char last_created[64] = {0}, last_id[64] = {0};
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON_AddItemToArray(data, decode_row(s));
    snprintf(last_created, sizeof last_created, "%s",
             ctext(s, 6) ? ctext(s, 6) : "");
    snprintf(last_id, sizeof last_id, "%s", ctext(s, 0) ? ctext(s, 0) : "");
    n++;
  }
  sqlite3_finalize(s);
  if (curj) cJSON_Delete(curj);

  cJSON *page = cJSON_CreateObject();
  if (n == lim) {
    cJSON *cur = cJSON_CreateObject();
    cJSON_AddStringToObject(cur, "p", last_created);
    cJSON_AddStringToObject(cur, "u", last_id);
    char *cj = cJSON_PrintUnformatted(cur); cJSON_Delete(cur);
    char *enc = cj ? b64url(cj) : NULL;
    if (enc) cJSON_AddStringToObject(page, "next_cursor", enc);
    else     cJSON_AddNullToObject(page, "next_cursor");
    free(cj); free(enc);
  } else {
    cJSON_AddNullToObject(page, "next_cursor");
  }
  cJSON_AddNumberToObject(page, "limit", lim);
  cJSON_AddNullToObject(page, "total");

  cJSON *filters = cJSON_CreateObject();
  add_str_or_null(filters, "ref_type", v_rt[0] ? v_rt : NULL);
  add_str_or_null(filters, "ref_id",   v_rid[0] ? v_rid : NULL);
  add_str_or_null(filters, "case_id",  v_cid[0] ? v_cid : NULL);
  cJSON_AddBoolToObject(filters, "include_deleted", show_deleted);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddItemToObject(meta, "filters", filters);

  cJSON *w = cJSON_CreateObject();
  cJSON_AddItemToObject(w, "data", data);
  cJSON_AddItemToObject(w, "page", page);
  cJSON_AddItemToObject(w, "meta", meta);
  char *out = cJSON_PrintUnformatted(w); cJSON_Delete(w);
  *st = 200;
  return out;
}

/* ── POST /api/annotations ────────────────────────────────────────────── */
/* A case_id from another tenant would silently file this tenant's note into
 * someone else's case, so it is checked against `cases` under the same
 * tenant. The cases table is owned by another module; if it is not present
 * yet (prepare fails) the check is skipped rather than turning every note
 * into a 500 — the tenant_id column on annotations still isolates the row. */
static int case_id_ok(db_handle *db, const char *tid, const char *cid) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM cases WHERE id=?1 AND tenant_id=?2", -1, &s, NULL)
      != SQLITE_OK)
    return 1;
  sqlite3_bind_text(s, 1, cid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tid, -1, SQLITE_TRANSIENT);
  int ok = sqlite3_step(s) == SQLITE_ROW;
  sqlite3_finalize(s);
  return ok;
}

static char *create_note(db_handle *db, const tenant_ctx *t,
                         const char *body, int *st) {
  if (role_rank(t->role) < 2) return err(st, 403, "forbidden");
  cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
  if (!jb || !cJSON_IsObject(jb)) {
    if (jb) cJSON_Delete(jb);
    return err(st, 400, "body_required");
  }

  cJSON *jrt = cJSON_GetObjectItem(jb, "ref_type");
  const char *rt = (jrt && cJSON_IsString(jrt)) ? jrt->valuestring : NULL;
  if (!annotations_ref_type_valid(rt)) {
    cJSON_Delete(jb); return err(st, 400, "invalid_ref_type");
  }
  cJSON *jri = cJSON_GetObjectItem(jb, "ref_id");
  size_t rid_len = 0;
  const char *rid = trim_span((jri && cJSON_IsString(jri)) ? jri->valuestring
                                                          : NULL, &rid_len);
  if (rid_len == 0) { cJSON_Delete(jb); return err(st, 400, "ref_id_required"); }
  if (rid_len > ANN_REF_ID_MAX) {
    cJSON_Delete(jb); return err(st, 400, "ref_id_too_long");
  }

  cJSON *jci = cJSON_GetObjectItem(jb, "case_id");
  char cid[ANN_CASE_ID_MAX + 8] = {0};
  int has_case = 0;
  if (jci && !cJSON_IsNull(jci)) {
    if (!cJSON_IsString(jci)) { cJSON_Delete(jb); return err(st, 400, "invalid_case_id"); }
    size_t cl = 0; const char *cp = trim_span(jci->valuestring, &cl);
    if (cl > ANN_CASE_ID_MAX) { cJSON_Delete(jb); return err(st, 400, "invalid_case_id"); }
    if (cl) { snprintf(cid, sizeof cid, "%.*s", (int)cl, cp); has_case = 1; }
  }
  if (has_case && !case_id_ok(db, t->tenant_id, cid)) {
    cJSON_Delete(jb); return err(st, 400, "unknown_case_id");
  }

  const char *md = NULL; size_t md_len = 0;
  const char *e = check_body_md(jb, &md, &md_len);
  if (e) { cJSON_Delete(jb); return err(st, 400, e); }

  /* ref_id is trimmed in place by binding (ptr,len) — no separate buffer. */
  char nid[37]; uuid4(nid);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO annotations (id,tenant_id,case_id,ref_type,ref_id,"
        "body_md,author_id) VALUES (?1,?2,?3,?4,?5,?6,?7)", -1, &s, NULL)
      != SQLITE_OK) {
    cJSON_Delete(jb); return err(st, 500, "server_error");
  }
  sqlite3_bind_text(s, 1, nid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
  if (has_case) sqlite3_bind_text(s, 3, cid, -1, SQLITE_TRANSIENT);
  else          sqlite3_bind_null(s, 3);
  sqlite3_bind_text(s, 4, rt, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 5, rid, (int)rid_len, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 6, md, (int)md_len, SQLITE_TRANSIENT);
  if (t->user_id[0]) sqlite3_bind_text(s, 7, t->user_id, -1, SQLITE_TRANSIENT);
  else               sqlite3_bind_null(s, 7);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) { cJSON_Delete(jb); return err(st, 500, "server_error"); }

  char ridbuf[ANN_REF_ID_MAX + 8];
  snprintf(ridbuf, sizeof ridbuf, "%.*s", (int)rid_len, rid);
  char *pl = audit_payload(rt, ridbuf, has_case ? cid : NULL,
                           (long)md_len, NULL, NULL);
  audit_write(db, t->tenant_id, t->user_id, "annotation.create", nid, pl);
  free(pl);
  cJSON_Delete(jb);
  return one_note(db, t->tenant_id, nid, 1, 201, st);
}

/* ── PATCH /api/annotations/:id ───────────────────────────────────────── */
static char *update_note(db_handle *db, const tenant_ctx *t, const char *id,
                         const char *body, int *st) {
  ann_meta m;
  if (!load_meta(db, t->tenant_id, id, &m)) return err(st, 404, "not_found");

  /* Author-only, with no owner/admin override. See the header: a note whose
   * text a colleague can quietly change is not an audit trail. */
  if (!is_author(&m, t)) return err(st, 403, "not_author");
  /* A tombstone is a historical record; editing one would rewrite what was
   * deleted after the fact. Undelete, if it is ever wanted, is a new verb. */
  if (m.deleted) return err(st, 409, "annotation_deleted");

  cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
  if (!jb || !cJSON_IsObject(jb)) {
    if (jb) cJSON_Delete(jb);
    return err(st, 400, "body_required");
  }
  /* ref_type/ref_id/case_id are immutable: re-pointing an existing note at a
   * different piece of evidence forges the record (the note keeps its
   * created_at and author but now "annotates" something it never saw).
   * Rejecting loudly beats silently ignoring — the client learns it must
   * create a new note instead. */
  if (cJSON_HasObjectItem(jb, "ref_type") || cJSON_HasObjectItem(jb, "ref_id") ||
      cJSON_HasObjectItem(jb, "case_id")) {
    cJSON_Delete(jb); return err(st, 400, "immutable_field");
  }

  const char *md = NULL; size_t md_len = 0;
  const char *e = check_body_md(jb, &md, &md_len);
  if (e) { cJSON_Delete(jb); return err(st, 400, e); }

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE annotations SET body_md=?1, updated_at=datetime('now') "
        "WHERE id=?2 AND tenant_id=?3 AND deleted_at IS NULL", -1, &s, NULL)
      != SQLITE_OK) {
    cJSON_Delete(jb); return err(st, 500, "server_error");
  }
  sqlite3_bind_text(s, 1, md, (int)md_len, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, t->tenant_id, -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(s);
  int changed = sqlite3_changes(db->h);
  sqlite3_finalize(s);
  cJSON_Delete(jb);
  if (rc != SQLITE_DONE) return err(st, 500, "server_error");
  if (changed == 0) return err(st, 404, "not_found");

  char *pl = audit_payload(m.ref_type, m.ref_id, NULL, (long)md_len, NULL, NULL);
  audit_write(db, t->tenant_id, t->user_id, "annotation.update", id, pl);
  free(pl);
  return one_note(db, t->tenant_id, id, 1, 200, st);
}

/* ── DELETE /api/annotations/:id — tombstone, never a row removal ─────── */
static char *delete_note(db_handle *db, const tenant_ctx *t, const char *id,
                         int *st) {
  ann_meta m;
  if (!load_meta(db, t->tenant_id, id, &m)) return err(st, 404, "not_found");

  /* Author, or a tenant owner/admin moderating content they did not write.
   * Note the asymmetry with PATCH: owner/admin may hide a note but may not
   * change what it says. */
  int mod = role_rank(t->role) >= 3;
  if (!is_author(&m, t) && !mod) return err(st, 403, "forbidden");

  /* Already tombstoned → idempotent success WITHOUT re-stamping deleted_at.
   * The first deletion time is the fact of record; a retry must not move it. */
  if (m.deleted) return one_note(db, t->tenant_id, id, 1, 200, st);

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE annotations SET deleted_at=datetime('now') "
        "WHERE id=?1 AND tenant_id=?2 AND deleted_at IS NULL", -1, &s, NULL)
      != SQLITE_OK)
    return err(st, 500, "server_error");
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(s);
  int changed = sqlite3_changes(db->h);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) return err(st, 500, "server_error");
  if (changed == 0) return err(st, 404, "not_found");

  char *pl = audit_payload(m.ref_type, m.ref_id, NULL, -1,
                           "author_id", m.has_author ? m.author : NULL);
  audit_write(db, t->tenant_id, t->user_id, "annotation.delete", id, pl);
  free(pl);
  return one_note(db, t->tenant_id, id, 1, 200, st);
}

/* ── dispatcher ───────────────────────────────────────────────────────── */
char *annotationsapi(db_handle *db, const tenant_ctx *t, const char *method,
                     const char *seg, const char *query_string,
                     const char *body, int *status) {
  if (!db || !db->h || !t || !method) { *status = 500; return NULL; }
  if (!seg) seg = "";
  /* An unrecognised (or empty) role means tenant_resolve gave us no
   * membership; deny outright rather than leaking row counts via list. */
  if (role_rank(t->role) < 1) return err(status, 403, "forbidden");

  int is_get   = strcmp(method, "GET") == 0,
      is_post  = strcmp(method, "POST") == 0,
      is_patch = strcmp(method, "PATCH") == 0,
      is_del   = strcmp(method, "DELETE") == 0;

  if (!seg[0]) {                                   /* collection */
    if (is_get)  return list_notes(db, t, query_string, status);
    if (is_post) return create_note(db, t, body, status);
    return err(status, 405, "method_not_allowed");
  }

  /* item-level. Reads hide tombstones from everyone below analyst, matching
   * the list default so the two views never disagree. */
  if (is_get)   return one_note(db, t->tenant_id, seg,
                                role_rank(t->role) >= 2, 200, status);
  if (is_patch) return update_note(db, t, seg, body, status);
  if (is_del)   return delete_note(db, t, seg, status);
  return err(status, 405, "method_not_allowed");
}
