/* core/aoiapi.c — /api/aoi (roadmap 9) and /api/watchlists (roadmap 21).
 *
 * Shaped after core/annotationsapi.c: one dispatcher per surface, one table
 * each, keyset pagination, tenant_id=? on every single statement. See
 * aoiapi.h for what each surface is FOR and why it is shaped this way; this
 * file restates only the decisions you have to see at their call site.
 *
 * The id in a URL is a uuid and therefore unguessable, but "unguessable" is
 * not access control: ids leak through exports, logs and shared links, so the
 * tenant predicate is what actually keeps tenants apart — including on
 * fetch-by-id, and including on the entity-id existence check, which is a
 * cross-tenant probe if you forget it.
 *
 * No geometry is judged here. alert_eval_shape_check() is the one validator;
 * this file calls it and stores whatever bounding box it reports. */
#include "aoiapi.h"
#include "alert_eval.h"
#include "audit.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/rand.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AOI_NAME_MAX     200
#define AOI_LIST_DEFAULT 50
#define AOI_LIST_MAX     200
/* A watchlist past this is a saved search, not a list of people to watch, and
 * every ingested item would pay a linear scan of it inside the matcher. */
#define WL_ENTITY_MAX    500
#define WL_ENTITY_ID_MAX 128

#define AOI_COLS \
  "id,name,kind,geometry_json,bbox_w,bbox_s,bbox_e,bbox_n," \
  "created_by,created_at,updated_at"

/* ── shared idioms (verbatim from alertsapi.c / annotationsapi.c) ───────── */
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
static cJSON *safe_json(const char *s, int as_array) {
  if (s && *s) { cJSON *j = cJSON_Parse(s); if (j) return j; }
  return as_array ? cJSON_CreateArray() : cJSON_CreateObject();
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

/* ── querystring (same shape as annotationsapi.c's qget) ────────────────── */
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

/* owner > admin > analyst > viewer; an unrecognised role ranks 0 and can do
 * nothing, so a future role name fails closed instead of inheriting write. */
static int role_rank(const char *r) {
  if (!r) return 0;
  if (!strcmp(r, "owner"))   return 4;
  if (!strcmp(r, "admin"))   return 3;
  if (!strcmp(r, "analyst")) return 2;
  if (!strcmp(r, "viewer"))  return 1;
  return 0;
}
static int can_write(const tenant_ctx *t) { return role_rank(t->role) >= 2; }

/* Trim + length-check a required name into `out`. Returns NULL on success. */
static const char *check_name(const cJSON *b, char *out, size_t n) {
  const cJSON *j = b ? cJSON_GetObjectItem(b, "name") : NULL;
  if (!j || !cJSON_IsString(j)) return "name_required";
  const char *p = j->valuestring;
  while (*p && isspace((unsigned char)*p)) p++;
  size_t l = strlen(p);
  while (l && isspace((unsigned char)p[l - 1])) l--;
  if (l == 0) return "name_required";
  if (l > AOI_NAME_MAX) return "name_too_long";
  if (l >= n) l = n - 1;
  memcpy(out, p, l); out[l] = 0;
  return NULL;
}

/* Keyset cursor {"p":created_at,"u":id} — identical encoding to
 * annotationsapi/intelapi so one client decoder covers every list. Malformed
 * cursors are ignored (page 1) rather than 400'd. */
typedef struct { cJSON *root; const char *p, *u; } keyset;
static void keyset_parse(keyset *k, const char *cur) {
  k->root = NULL; k->p = NULL; k->u = NULL;
  if (!cur || !*cur) return;
  char *dec = b64url_decode(cur);
  if (!dec) return;
  k->root = cJSON_Parse(dec);
  free(dec);
  if (!k->root) return;
  cJSON *jp = cJSON_GetObjectItem(k->root, "p");
  cJSON *ju = cJSON_GetObjectItem(k->root, "u");
  if (cJSON_IsString(jp) && cJSON_IsString(ju)) { k->p = jp->valuestring; k->u = ju->valuestring; }
}
static void keyset_free(keyset *k) { if (k->root) cJSON_Delete(k->root); k->root = NULL; }

/* created_at is second-resolution (datetime('now')), so a burst of writes
 * shares a timestamp — the id tiebreaker is what makes the page boundary
 * stable rather than a source of dropped or duplicated rows. */
static void add_page(cJSON *w, cJSON *data, int n, int lim,
                     const char *last_p, const char *last_u) {
  cJSON *page = cJSON_CreateObject();
  if (n == lim && last_p && last_u) {
    cJSON *cur = cJSON_CreateObject();
    cJSON_AddStringToObject(cur, "p", last_p);
    cJSON_AddStringToObject(cur, "u", last_u);
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
  cJSON_AddItemToObject(w, "data", data);
  cJSON_AddItemToObject(w, "page", page);
}
static int clamp_limit(const char *qs) {
  char v[24];
  qget(qs, "limit", v, sizeof v);
  int lim = v[0] ? atoi(v) : AOI_LIST_DEFAULT;
  if (lim <= 0) lim = AOI_LIST_DEFAULT;
  if (lim > AOI_LIST_MAX) lim = AOI_LIST_MAX;
  return lim;
}

/* ══ /api/aoi ═══════════════════════════════════════════════════════════ */

static cJSON *decode_aoi(sqlite3_stmt *s) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "id",   (const char *)sqlite3_column_text(s, 0));
  cJSON_AddStringToObject(o, "name", (const char *)sqlite3_column_text(s, 1));
  cJSON_AddStringToObject(o, "kind", (const char *)sqlite3_column_text(s, 2));
  /* Echoed as a JSON VALUE, not a string, so the shape a client PUTs back is
   * literally the shape it received. */
  const char *gj = ctext(s, 3);
  cJSON *g = gj ? cJSON_Parse(gj) : NULL;
  cJSON_AddItemToObject(o, "geometry", g ? g : cJSON_CreateNull());
  cJSON *bb = cJSON_CreateArray();
  int have = 1;
  for (int i = 4; i <= 7; i++)
    if (sqlite3_column_type(s, i) == SQLITE_NULL) have = 0;
  if (have)
    for (int i = 4; i <= 7; i++)
      cJSON_AddItemToArray(bb, cJSON_CreateNumber(sqlite3_column_double(s, i)));
  cJSON_AddItemToObject(o, "bbox", have ? bb : (cJSON_Delete(bb), cJSON_CreateNull()));
  add_str_or_null(o, "created_by", ctext(s, 8));
  cJSON_AddStringToObject(o, "created_at", (const char *)sqlite3_column_text(s, 9));
  add_str_or_null(o, "updated_at", ctext(s, 10));
  return o;
}

static char *one_aoi(db_handle *db, const char *tid, const char *id,
                     int code, int *st) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT " AOI_COLS " FROM areas_of_interest WHERE id=?1 AND tenant_id=?2",
        -1, &s, NULL) != SQLITE_OK)
    return err(st, 500, "server_error");
  sqlite3_bind_text(s, 1, id,  -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tid, -1, SQLITE_TRANSIENT);
  char *out;
  if (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *w = cJSON_CreateObject();
    cJSON_AddItemToObject(w, "data", decode_aoi(s));
    out = cJSON_PrintUnformatted(w); cJSON_Delete(w); *st = code;
  } else {
    out = err(st, 404, "not_found");
  }
  sqlite3_finalize(s);
  return out;
}

int aoi_exists(db_handle *db, const char *tenant_id, const char *aoi_id) {
  if (!db || !db->h || !tenant_id || !*tenant_id || !aoi_id || !*aoi_id) return 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM areas_of_interest WHERE id=?1 AND tenant_id=?2",
        -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, aoi_id,    -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant_id, -1, SQLITE_TRANSIENT);
  int found = sqlite3_step(s) == SQLITE_ROW;
  sqlite3_finalize(s);
  return found;
}

/* Which of this tenant's rules point at `aoi_id`? Predicates are parsed rather
 * than LIKE'd: a LIKE over predicate_json would depend on cJSON's exact
 * spacing and would match an id that merely appears inside some other string. */
static cJSON *aoi_referencing_rules(db_handle *db, const char *tid,
                                    const char *aoi_id) {
  cJSON *arr = cJSON_CreateArray();
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT id,name,predicate_json FROM alert_rules WHERE tenant_id=?1",
        -1, &s, NULL) != SQLITE_OK) return arr;
  sqlite3_bind_text(s, 1, tid, -1, SQLITE_TRANSIENT);
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *pj = ctext(s, 2);
    if (!pj) continue;
    cJSON *p = cJSON_Parse(pj);
    cJSON *a = p ? cJSON_GetObjectItem(p, "aoi_id") : NULL;
    if (a && cJSON_IsString(a) && strcmp(a->valuestring, aoi_id) == 0) {
      cJSON *r = cJSON_CreateObject();
      const char *rid = ctext(s, 0), *rn = ctext(s, 1);
      cJSON_AddStringToObject(r, "id", rid ? rid : "");
      cJSON_AddStringToObject(r, "name", rn ? rn : "");
      cJSON_AddItemToArray(arr, r);
    }
    if (p) cJSON_Delete(p);
  }
  sqlite3_finalize(s);
  return arr;
}

static int kind_valid(const char *k) {
  return k && (!strcmp(k, "bbox") || !strcmp(k, "polygon") || !strcmp(k, "circle"));
}

/* Validate kind+geometry together and render the geometry back to canonical
 * text. Returns NULL on success, having set *kind_out to a static literal and
 * *geom_out to a malloc'd string the caller frees; otherwise a reason. */
static const char *check_geometry(const cJSON *jk, const cJSON *jg,
                                  const char **kind_out, char **geom_out,
                                  double bb[4]) {
  *geom_out = NULL;
  if (!jk || !cJSON_IsString(jk) || !kind_valid(jk->valuestring))
    return "kind_must_be_bbox_polygon_or_circle";
  if (!jg || cJSON_IsNull(jg)) return "geometry_required";
  /* Canonicalised through cJSON so what is stored is what alert_eval.c will
   * re-parse — no incoming whitespace, no key order surprises. */
  char *gj = cJSON_PrintUnformatted(jg);
  if (!gj) return "server_error";
  const char *why = alert_eval_shape_check(jk->valuestring, gj,
                                           &bb[0], &bb[1], &bb[2], &bb[3]);
  if (why) { free(gj); return why; }
  /* Point at the literal, not at the caller's cJSON, so the string outlives
   * the body it came from. */
  *kind_out = !strcmp(jk->valuestring, "bbox")    ? "bbox"
            : !strcmp(jk->valuestring, "polygon") ? "polygon" : "circle";
  *geom_out = gj;
  return NULL;
}

static char *aoi_list(db_handle *db, const tenant_ctx *t, const char *qs,
                      int *st) {
  int lim = clamp_limit(qs);
  char v_cur[768];
  qget(qs, "cursor", v_cur, sizeof v_cur);
  keyset k; keyset_parse(&k, v_cur);

  const char *sql = (k.p && k.u)
    ? "SELECT " AOI_COLS " FROM areas_of_interest WHERE tenant_id=?1"
      " AND (created_at < ?2 OR (created_at = ?2 AND id > ?3))"
      " ORDER BY created_at DESC, id ASC LIMIT ?4"
    : "SELECT " AOI_COLS " FROM areas_of_interest WHERE tenant_id=?1"
      " ORDER BY created_at DESC, id ASC LIMIT ?2";
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    keyset_free(&k);
    return err(st, 500, "server_error");
  }
  int bi = 1;
  sqlite3_bind_text(s, bi++, t->tenant_id, -1, SQLITE_TRANSIENT);
  if (k.p && k.u) {
    sqlite3_bind_text(s, bi++, k.p, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, bi++, k.p, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, bi++, k.u, -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int(s, bi, lim);

  cJSON *data = cJSON_CreateArray();
  int n = 0;
  char last_p[64] = {0}, last_u[64] = {0};
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON_AddItemToArray(data, decode_aoi(s));
    snprintf(last_p, sizeof last_p, "%s", ctext(s, 9) ? ctext(s, 9) : "");
    snprintf(last_u, sizeof last_u, "%s", ctext(s, 0) ? ctext(s, 0) : "");
    n++;
  }
  sqlite3_finalize(s);
  keyset_free(&k);

  cJSON *w = cJSON_CreateObject();
  add_page(w, data, n, lim, last_p, last_u);
  cJSON_AddItemToObject(w, "meta", cJSON_CreateObject());
  char *o = cJSON_PrintUnformatted(w); cJSON_Delete(w);
  *st = 200;
  return o;
}

static char *aoi_create(db_handle *db, const tenant_ctx *t, const char *body,
                        int *st) {
  if (!can_write(t)) return err(st, 403, "forbidden");
  cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
  if (!jb || !cJSON_IsObject(jb)) {
    if (jb) cJSON_Delete(jb);
    return err(st, 400, "body_required");
  }
  char name[AOI_NAME_MAX + 8];
  const char *e = check_name(jb, name, sizeof name);
  if (e) { cJSON_Delete(jb); return err(st, 400, e); }

  const char *kind = NULL; char *gj = NULL; double bb[4] = {0,0,0,0};
  e = check_geometry(cJSON_GetObjectItem(jb, "kind"),
                     cJSON_GetObjectItem(jb, "geometry"), &kind, &gj, bb);
  if (e) { cJSON_Delete(jb); return err(st, 400, e); }
  cJSON_Delete(jb);

  char id[37]; uuid4(id);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO areas_of_interest"
        " (id,tenant_id,name,kind,geometry_json,bbox_w,bbox_s,bbox_e,bbox_n,"
        "  created_by,created_at,updated_at)"
        " VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,datetime('now'),datetime('now'))",
        -1, &s, NULL) != SQLITE_OK) { free(gj); return err(st, 500, "server_error"); }
  sqlite3_bind_text(s, 1, id,           -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, name,         -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 4, kind,         -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 5, gj,           -1, SQLITE_TRANSIENT);
  for (int i = 0; i < 4; i++) sqlite3_bind_double(s, 6 + i, bb[i]);
  if (t->user_id[0]) sqlite3_bind_text(s, 10, t->user_id, -1, SQLITE_TRANSIENT);
  else               sqlite3_bind_null(s, 10);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  free(gj);
  if (rc != SQLITE_DONE) return err(st, 500, "server_error");

  cJSON *pl = cJSON_CreateObject();
  cJSON_AddStringToObject(pl, "name", name);
  cJSON_AddStringToObject(pl, "kind", kind);
  char *pj = cJSON_PrintUnformatted(pl); cJSON_Delete(pl);
  audit_write(db, t->tenant_id, t->user_id, "aoi.create", id, pj);
  free(pj);
  return one_aoi(db, t->tenant_id, id, 201, st);
}

static char *aoi_patch(db_handle *db, const tenant_ctx *t, const char *id,
                       const char *body, int *st) {
  if (!can_write(t)) return err(st, 403, "forbidden");
  cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
  if (!jb || !cJSON_IsObject(jb)) {
    if (jb) cJSON_Delete(jb);
    return err(st, 400, "body_required");
  }
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT name,kind,geometry_json FROM areas_of_interest"
        " WHERE id=?1 AND tenant_id=?2", -1, &s, NULL) != SQLITE_OK) {
    cJSON_Delete(jb); return err(st, 500, "server_error");
  }
  sqlite3_bind_text(s, 1, id,           -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) {
    sqlite3_finalize(s); cJSON_Delete(jb); return err(st, 404, "not_found");
  }
  /* Merge-then-validate, like alertsapi's PATCH: the merged document is what
   * gets judged, so changing only `kind` still re-validates the geometry it
   * now claims to be — an AOI can never end up stored under a kind its shape
   * does not satisfy. */
  cJSON *merged = cJSON_CreateObject();
  cJSON *bn = cJSON_GetObjectItem(jb, "name");
  cJSON_AddStringToObject(merged, "name",
    (bn && cJSON_IsString(bn)) ? bn->valuestring
                               : (const char *)sqlite3_column_text(s, 0));
  cJSON *bk = cJSON_GetObjectItem(jb, "kind");
  cJSON_AddStringToObject(merged, "kind",
    (bk && cJSON_IsString(bk)) ? bk->valuestring
                               : (const char *)sqlite3_column_text(s, 1));
  cJSON *bg = cJSON_GetObjectItem(jb, "geometry");
  cJSON_AddItemToObject(merged, "geometry",
    (bg && !cJSON_IsNull(bg)) ? cJSON_Duplicate(bg, 1)
                              : safe_json(ctext(s, 2), 1));
  sqlite3_finalize(s);
  cJSON_Delete(jb);

  char name[AOI_NAME_MAX + 8];
  const char *e = check_name(merged, name, sizeof name);
  if (e) { cJSON_Delete(merged); return err(st, 400, e); }
  const char *kind = NULL; char *gj = NULL; double bb[4] = {0,0,0,0};
  e = check_geometry(cJSON_GetObjectItem(merged, "kind"),
                     cJSON_GetObjectItem(merged, "geometry"), &kind, &gj, bb);
  cJSON_Delete(merged);
  if (e) return err(st, 400, e);

  if (sqlite3_prepare_v2(db->h,
        "UPDATE areas_of_interest SET name=?1,kind=?2,geometry_json=?3,"
        " bbox_w=?4,bbox_s=?5,bbox_e=?6,bbox_n=?7,updated_at=datetime('now')"
        " WHERE id=?8 AND tenant_id=?9", -1, &s, NULL) != SQLITE_OK) {
    free(gj); return err(st, 500, "server_error");
  }
  sqlite3_bind_text(s, 1, name, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, kind, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, gj,   -1, SQLITE_TRANSIENT);
  for (int i = 0; i < 4; i++) sqlite3_bind_double(s, 4 + i, bb[i]);
  sqlite3_bind_text(s, 8, id,           -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 9, t->tenant_id, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
  free(gj);

  cJSON *pl = cJSON_CreateObject();
  cJSON_AddStringToObject(pl, "name", name);
  cJSON_AddStringToObject(pl, "kind", kind);
  char *pj = cJSON_PrintUnformatted(pl); cJSON_Delete(pl);
  audit_write(db, t->tenant_id, t->user_id, "aoi.update", id, pj);
  free(pj);
  /* The rule cache keys off areas_of_interest.updated_at, so every rule that
   * geofences on this shape picks up the new geometry within one refresh
   * interval — see read_generation() in alert_eval.c. */
  return one_aoi(db, t->tenant_id, id, 200, st);
}

static char *aoi_delete(db_handle *db, const tenant_ctx *t, const char *id,
                        int *st) {
  if (!can_write(t)) return err(st, 403, "forbidden");
  if (!aoi_exists(db, t->tenant_id, id)) return err(st, 404, "not_found");

  cJSON *refs = aoi_referencing_rules(db, t->tenant_id, id);
  if (cJSON_GetArraySize(refs) > 0) {
    /* See aoiapi.h: dropping the shape is SAFE (those rules would fail closed)
     * and that is exactly why it is refused — a geofence that silently stops
     * firing is the failure this item exists to prevent. */
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "error", "aoi_in_use");
    cJSON_AddItemToObject(o, "rules", refs);
    char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o);
    *st = 409;
    return j;
  }
  cJSON_Delete(refs);

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "DELETE FROM areas_of_interest WHERE id=?1 AND tenant_id=?2",
        -1, &s, NULL) != SQLITE_OK) return err(st, 500, "server_error");
  sqlite3_bind_text(s, 1, id,           -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
  audit_write(db, t->tenant_id, t->user_id, "aoi.delete", id, NULL);
  *st = 204;
  return NULL;
}

char *aoiapi(db_handle *db, const tenant_ctx *t, const char *method,
             const char *seg, const char *qs, const char *body, int *status) {
  if (!db || !db->h || !t || !t->tenant_id[0])
    return err(status, 400, "tenant_required");
  if (!method) return err(status, 405, "method_not_allowed");
  if (!seg) seg = "";
  int is_get   = !strcmp(method, "GET"),   is_post = !strcmp(method, "POST"),
      is_patch = !strcmp(method, "PATCH"), is_del  = !strcmp(method, "DELETE");

  if (!seg[0]) {
    if (is_get)  return aoi_list(db, t, qs, status);
    if (is_post) return aoi_create(db, t, body, status);
    return err(status, 405, "method_not_allowed");
  }
  if (is_get)   return one_aoi(db, t->tenant_id, seg, 200, status);
  if (is_patch) return aoi_patch(db, t, seg, body, status);
  if (is_del)   return aoi_delete(db, t, seg, status);
  return err(status, 405, "method_not_allowed");
}

/* ══ /api/watchlists ════════════════════════════════════════════════════ */

#define WL_COLS \
  "w.id,w.name,w.entity_ids_json,w.rule_id,w.created_by,w.created_at," \
  "w.updated_at,r.enabled,r.muted_until"
#define WL_FROM \
  " FROM watchlists w LEFT JOIN alert_rules r" \
  "   ON r.id = w.rule_id AND r.tenant_id = w.tenant_id"

static cJSON *decode_wl(sqlite3_stmt *s) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "id",   (const char *)sqlite3_column_text(s, 0));
  cJSON_AddStringToObject(o, "name", (const char *)sqlite3_column_text(s, 1));
  cJSON_AddItemToObject(o, "entity_ids", safe_json(ctext(s, 2), 1));
  add_str_or_null(o, "rule_id",    ctext(s, 3));
  add_str_or_null(o, "created_by", ctext(s, 4));
  cJSON_AddStringToObject(o, "created_at", (const char *)sqlite3_column_text(s, 5));
  add_str_or_null(o, "updated_at", ctext(s, 6));
  /* enabled/muted come from the RULE, never from the sugar row: the rule is
   * the thing that actually fires, and /api/alerts can edit it behind our
   * back. A cached copy here would be a second source of truth. */
  cJSON_AddItemToObject(o, "enabled",
    sqlite3_column_type(s, 7) == SQLITE_NULL
      ? cJSON_CreateNull() : cJSON_CreateBool(sqlite3_column_int(s, 7) != 0));
  add_str_or_null(o, "muted_until", ctext(s, 8));
  return o;
}

static char *one_wl(db_handle *db, const char *tid, const char *id,
                    int code, int *st) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT " WL_COLS WL_FROM " WHERE w.id=?1 AND w.tenant_id=?2",
        -1, &s, NULL) != SQLITE_OK)
    return err(st, 500, "server_error");
  sqlite3_bind_text(s, 1, id,  -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tid, -1, SQLITE_TRANSIENT);
  char *out;
  if (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *w = cJSON_CreateObject();
    cJSON_AddItemToObject(w, "data", decode_wl(s));
    out = cJSON_PrintUnformatted(w); cJSON_Delete(w); *st = code;
  } else {
    out = err(st, 404, "not_found");
  }
  sqlite3_finalize(s);
  return out;
}

/* Validate entity_ids and hand back a canonical array (caller cJSON_Delete).
 *
 * Non-empty is a HARD requirement, not a nicety: {"entity_ids":[]} compiles to
 * a predicate with no entity term, which matches every item in the tenant.
 * The membership check is tenant-scoped for the same reason every other query
 * here is — an unscoped existence probe would answer "does this entity id
 * exist" for ids belonging to other tenants. */
static const char *check_entity_ids(db_handle *db, const char *tid,
                                    const cJSON *arr, cJSON **out) {
  *out = NULL;
  if (!arr || !cJSON_IsArray(arr)) return "entity_ids_must_be_array";
  int n = cJSON_GetArraySize(arr);
  if (n == 0) return "entity_ids_required";
  if (n > WL_ENTITY_MAX) return "entity_ids_too_many";

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM entities WHERE entity_id=?1"
        " AND (tenant_id IS NULL OR tenant_id=?2)", -1, &s, NULL) != SQLITE_OK)
    return "server_error";

  cJSON *clean = cJSON_CreateArray();
  const char *why = NULL;
  const cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    if (!cJSON_IsString(e) || !e->valuestring[0] ||
        strlen(e->valuestring) > WL_ENTITY_ID_MAX) {
      why = "entity_ids_must_be_strings"; break;
    }
    /* Duplicates would make the matcher's linear scan longer for no effect. */
    int dup = 0;
    const cJSON *c;
    cJSON_ArrayForEach(c, clean)
      if (cJSON_IsString(c) && !strcmp(c->valuestring, e->valuestring)) { dup = 1; break; }
    if (dup) continue;
    sqlite3_reset(s);
    sqlite3_clear_bindings(s);
    sqlite3_bind_text(s, 1, e->valuestring, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, tid,            -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) != SQLITE_ROW) { why = "unknown_entity_id"; break; }
    cJSON_AddItemToArray(clean, cJSON_CreateString(e->valuestring));
  }
  sqlite3_finalize(s);
  if (why) { cJSON_Delete(clean); return why; }
  if (cJSON_GetArraySize(clean) == 0) { cJSON_Delete(clean); return "entity_ids_required"; }
  *out = clean;
  return NULL;
}

/* {"entity_ids":[…]} — the entire semantic content of a watchlist, and the
 * only place this file writes a predicate. */
static char *wl_predicate(const cJSON *ids) {
  cJSON *p = cJSON_CreateObject();
  cJSON_AddItemToObject(p, "entity_ids", cJSON_Duplicate(ids, 1));
  char *j = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);
  return j;
}

static char *wl_list(db_handle *db, const tenant_ctx *t, const char *qs,
                     int *st) {
  int lim = clamp_limit(qs);
  char v_cur[768];
  qget(qs, "cursor", v_cur, sizeof v_cur);
  keyset k; keyset_parse(&k, v_cur);

  const char *sql = (k.p && k.u)
    ? "SELECT " WL_COLS WL_FROM " WHERE w.tenant_id=?1"
      " AND (w.created_at < ?2 OR (w.created_at = ?2 AND w.id > ?3))"
      " ORDER BY w.created_at DESC, w.id ASC LIMIT ?4"
    : "SELECT " WL_COLS WL_FROM " WHERE w.tenant_id=?1"
      " ORDER BY w.created_at DESC, w.id ASC LIMIT ?2";
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    keyset_free(&k);
    return err(st, 500, "server_error");
  }
  int bi = 1;
  sqlite3_bind_text(s, bi++, t->tenant_id, -1, SQLITE_TRANSIENT);
  if (k.p && k.u) {
    sqlite3_bind_text(s, bi++, k.p, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, bi++, k.p, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, bi++, k.u, -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int(s, bi, lim);

  cJSON *data = cJSON_CreateArray();
  int n = 0;
  char last_p[64] = {0}, last_u[64] = {0};
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON_AddItemToArray(data, decode_wl(s));
    snprintf(last_p, sizeof last_p, "%s", ctext(s, 5) ? ctext(s, 5) : "");
    snprintf(last_u, sizeof last_u, "%s", ctext(s, 0) ? ctext(s, 0) : "");
    n++;
  }
  sqlite3_finalize(s);
  keyset_free(&k);

  cJSON *w = cJSON_CreateObject();
  add_page(w, data, n, lim, last_p, last_u);
  cJSON_AddItemToObject(w, "meta", cJSON_CreateObject());
  char *o = cJSON_PrintUnformatted(w); cJSON_Delete(w);
  *st = 200;
  return o;
}

static char *wl_create(db_handle *db, const tenant_ctx *t, const char *body,
                       int *st) {
  if (!can_write(t)) return err(st, 403, "forbidden");
  cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
  if (!jb || !cJSON_IsObject(jb)) {
    if (jb) cJSON_Delete(jb);
    return err(st, 400, "body_required");
  }
  char name[AOI_NAME_MAX + 8];
  const char *e = check_name(jb, name, sizeof name);
  if (e) { cJSON_Delete(jb); return err(st, 400, e); }
  cJSON *ids = NULL;
  e = check_entity_ids(db, t->tenant_id, cJSON_GetObjectItem(jb, "entity_ids"), &ids);
  cJSON_Delete(jb);
  if (e) return err(st, 400, e);

  char *pj = wl_predicate(ids);
  char *idj = cJSON_PrintUnformatted(ids);
  cJSON_Delete(ids);
  if (!pj || !idj) { free(pj); free(idj); return err(st, 500, "server_error"); }

  char rule_id[37], wl_id[37];
  uuid4(rule_id); uuid4(wl_id);

  /* The rule and its sugar row go in together or not at all: a watchlists row
   * pointing at a rule that does not exist would show up in the UI as a
   * watchlist that can never fire. */
  sqlite3_exec(db->h, "BEGIN", 0, 0, 0);
  sqlite3_stmt *s;
  int ok = 0;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO alert_rules (id,tenant_id,name,enabled,predicate_json,"
        " channels_json,dedup_window_sec,storm_cap_per_hour,created_by)"
        " VALUES (?1,?2,?3,1,?4,'[]',3600,100,?5)", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, rule_id,      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, name,         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, pj,           -1, SQLITE_TRANSIENT);
    if (t->user_id[0]) sqlite3_bind_text(s, 5, t->user_id, -1, SQLITE_TRANSIENT);
    else               sqlite3_bind_null(s, 5);
    ok = sqlite3_step(s) == SQLITE_DONE;
    sqlite3_finalize(s);
  }
  if (ok && sqlite3_prepare_v2(db->h,
        "INSERT INTO watchlists (id,tenant_id,name,entity_ids_json,rule_id,"
        " created_by,created_at,updated_at)"
        " VALUES (?1,?2,?3,?4,?5,?6,datetime('now'),datetime('now'))",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, wl_id,        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, name,         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, idj,          -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 5, rule_id,      -1, SQLITE_TRANSIENT);
    if (t->user_id[0]) sqlite3_bind_text(s, 6, t->user_id, -1, SQLITE_TRANSIENT);
    else               sqlite3_bind_null(s, 6);
    ok = sqlite3_step(s) == SQLITE_DONE;
    sqlite3_finalize(s);
  } else ok = 0;
  sqlite3_exec(db->h, ok ? "COMMIT" : "ROLLBACK", 0, 0, 0);
  free(pj);
  if (!ok) { free(idj); return err(st, 500, "server_error"); }

  cJSON *pl = cJSON_CreateObject();
  cJSON_AddStringToObject(pl, "name", name);
  cJSON_AddStringToObject(pl, "rule_id", rule_id);
  cJSON_AddItemToObject(pl, "entity_ids", safe_json(idj, 1));
  char *plj = cJSON_PrintUnformatted(pl); cJSON_Delete(pl); free(idj);
  audit_write(db, t->tenant_id, t->user_id, "watchlist.create", wl_id, plj);
  free(plj);
  return one_wl(db, t->tenant_id, wl_id, 201, st);
}

static char *wl_patch(db_handle *db, const tenant_ctx *t, const char *id,
                      const char *body, int *st) {
  if (!can_write(t)) return err(st, 403, "forbidden");
  cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
  if (!jb || !cJSON_IsObject(jb)) {
    if (jb) cJSON_Delete(jb);
    return err(st, 400, "body_required");
  }
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT name,entity_ids_json,rule_id FROM watchlists"
        " WHERE id=?1 AND tenant_id=?2", -1, &s, NULL) != SQLITE_OK) {
    cJSON_Delete(jb); return err(st, 500, "server_error");
  }
  sqlite3_bind_text(s, 1, id,           -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) {
    sqlite3_finalize(s); cJSON_Delete(jb); return err(st, 404, "not_found");
  }
  char cur_name[AOI_NAME_MAX + 8], rule_id[64];
  snprintf(cur_name, sizeof cur_name, "%s", (const char *)sqlite3_column_text(s, 0));
  snprintf(rule_id,  sizeof rule_id,  "%s", ctext(s, 2) ? ctext(s, 2) : "");
  /* Heap, not a fixed buffer: 500 ids is ~20 KB and a truncated list would
   * re-parse as [] — i.e. a predicate with no entity term, which matches
   * everything. Silently widening a watchlist into a firehose is the one
   * outcome this file must never produce. */
  char *cur_ids = strdup(ctext(s, 1) ? ctext(s, 1) : "[]");
  sqlite3_finalize(s);
  if (!cur_ids) { cJSON_Delete(jb); return err(st, 500, "server_error"); }

  cJSON *merged = cJSON_CreateObject();
  cJSON *bn = cJSON_GetObjectItem(jb, "name");
  cJSON_AddStringToObject(merged, "name",
    (bn && cJSON_IsString(bn)) ? bn->valuestring : cur_name);
  char name[AOI_NAME_MAX + 8];
  const char *e = check_name(merged, name, sizeof name);
  cJSON_Delete(merged);
  if (e) { cJSON_Delete(jb); free(cur_ids); return err(st, 400, e); }

  cJSON *bi_ids = cJSON_GetObjectItem(jb, "entity_ids");
  cJSON *ids = NULL;
  if (bi_ids && !cJSON_IsNull(bi_ids)) {
    e = check_entity_ids(db, t->tenant_id, bi_ids, &ids);
  } else {
    cJSON *cur = cJSON_Parse(cur_ids);
    e = check_entity_ids(db, t->tenant_id, cur, &ids);
    if (cur) cJSON_Delete(cur);
    /* An id that has since been merged away by the entity resolver would make
     * an unrelated rename fail. Keep what is stored in that case — the caller
     * did not ask to change the list, and carrying a stale id the matcher
     * never hits beats refusing the edit. The emptiness check still stands:
     * an empty list is not a narrower watchlist, it is no watchlist. */
    if (e) {
      ids = safe_json(cur_ids, 1);
      e = (cJSON_IsArray(ids) && cJSON_GetArraySize(ids) > 0)
            ? NULL : "entity_ids_required";
      if (e) { cJSON_Delete(ids); ids = NULL; }
    }
  }
  cJSON *ben = cJSON_GetObjectItem(jb, "enabled");
  int has_enabled = ben && cJSON_IsBool(ben);
  int enabled = has_enabled ? cJSON_IsTrue(ben) : 1;
  cJSON_Delete(jb);
  free(cur_ids);
  if (e) return err(st, 400, e);

  char *pj = wl_predicate(ids);
  char *idj = cJSON_PrintUnformatted(ids);
  cJSON_Delete(ids);
  if (!pj || !idj) { free(pj); free(idj); return err(st, 500, "server_error"); }

  sqlite3_exec(db->h, "BEGIN", 0, 0, 0);
  if (rule_id[0]) {
    const char *sql = has_enabled
      ? "UPDATE alert_rules SET name=?1,predicate_json=?2,enabled=?3,"
        " updated_at=datetime('now') WHERE id=?4 AND tenant_id=?5"
      : "UPDATE alert_rules SET name=?1,predicate_json=?2,"
        " updated_at=datetime('now') WHERE id=?3 AND tenant_id=?4";
    if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK) {
      int bi = 1;
      sqlite3_bind_text(s, bi++, name, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, bi++, pj,   -1, SQLITE_TRANSIENT);
      if (has_enabled) sqlite3_bind_int(s, bi++, enabled ? 1 : 0);
      sqlite3_bind_text(s, bi++, rule_id,      -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, bi,   t->tenant_id, -1, SQLITE_TRANSIENT);
      sqlite3_step(s);
      sqlite3_finalize(s);
    }
  }
  if (sqlite3_prepare_v2(db->h,
        "UPDATE watchlists SET name=?1,entity_ids_json=?2,"
        " updated_at=datetime('now') WHERE id=?3 AND tenant_id=?4",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, name,         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, idj,          -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, id,           -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, t->tenant_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
  sqlite3_exec(db->h, "COMMIT", 0, 0, 0);
  free(pj);

  cJSON *pl = cJSON_CreateObject();
  cJSON_AddStringToObject(pl, "name", name);
  cJSON_AddItemToObject(pl, "entity_ids", safe_json(idj, 1));
  if (has_enabled) cJSON_AddBoolToObject(pl, "enabled", enabled);
  char *plj = cJSON_PrintUnformatted(pl); cJSON_Delete(pl); free(idj);
  audit_write(db, t->tenant_id, t->user_id, "watchlist.update", id, plj);
  free(plj);
  return one_wl(db, t->tenant_id, id, 200, st);
}

static char *wl_delete(db_handle *db, const tenant_ctx *t, const char *id,
                       int *st) {
  if (!can_write(t)) return err(st, 403, "forbidden");
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT rule_id FROM watchlists WHERE id=?1 AND tenant_id=?2",
        -1, &s, NULL) != SQLITE_OK) return err(st, 500, "server_error");
  sqlite3_bind_text(s, 1, id,           -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) {
    sqlite3_finalize(s); return err(st, 404, "not_found");
  }
  char rule_id[64];
  snprintf(rule_id, sizeof rule_id, "%s", ctext(s, 0) ? ctext(s, 0) : "");
  sqlite3_finalize(s);

  /* Same order alertsapi.c deletes a rule in — events first, then the rule —
   * so no alert_events row is ever left pointing at a rule that is gone. */
  sqlite3_exec(db->h, "BEGIN", 0, 0, 0);
  if (rule_id[0]) {
    static const char *DEL[2] = {
      "DELETE FROM alert_events WHERE rule_id=?1 AND tenant_id=?2",
      "DELETE FROM alert_rules  WHERE id=?1 AND tenant_id=?2"
    };
    for (int i = 0; i < 2; i++) {
      if (sqlite3_prepare_v2(db->h, DEL[i], -1, &s, NULL) != SQLITE_OK) continue;
      sqlite3_bind_text(s, 1, rule_id,      -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
      sqlite3_step(s);
      sqlite3_finalize(s);
    }
  }
  if (sqlite3_prepare_v2(db->h,
        "DELETE FROM watchlists WHERE id=?1 AND tenant_id=?2",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id,           -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
  sqlite3_exec(db->h, "COMMIT", 0, 0, 0);

  cJSON *pl = cJSON_CreateObject();
  add_str_or_null(pl, "rule_id", rule_id[0] ? rule_id : NULL);
  char *plj = cJSON_PrintUnformatted(pl); cJSON_Delete(pl);
  audit_write(db, t->tenant_id, t->user_id, "watchlist.delete", id, plj);
  free(plj);
  *st = 204;
  return NULL;
}

char *watchlistsapi(db_handle *db, const tenant_ctx *t, const char *method,
                    const char *seg, const char *qs, const char *body,
                    int *status) {
  if (!db || !db->h || !t || !t->tenant_id[0])
    return err(status, 400, "tenant_required");
  if (!method) return err(status, 405, "method_not_allowed");
  if (!seg) seg = "";
  int is_get   = !strcmp(method, "GET"),   is_post = !strcmp(method, "POST"),
      is_patch = !strcmp(method, "PATCH"), is_del  = !strcmp(method, "DELETE");

  if (!seg[0]) {
    if (is_get)  return wl_list(db, t, qs, status);
    if (is_post) return wl_create(db, t, body, status);
    return err(status, 405, "method_not_allowed");
  }
  if (is_get)   return one_wl(db, t->tenant_id, seg, 200, status);
  if (is_patch) return wl_patch(db, t, seg, body, status);
  if (is_del)   return wl_delete(db, t, seg, status);
  return err(status, 405, "method_not_allowed");
}
