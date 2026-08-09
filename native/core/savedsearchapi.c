/* core/savedsearchapi.c — roadmap 38: saved searches, per-user search history,
 * and the canonical permalink state codec.
 *
 * Shaped after core/alertsapi.c and core/annotationsapi.c (one dispatcher per
 * subtree, tenant-scoped, malloc'd JSON out) with the rationale for every
 * departure recorded in savedsearchapi.h and restated at the call sites here.
 *
 * Two invariants run through the whole file and are worth stating once:
 *
 *  1. EVERY statement carries `tenant_id=?1 AND user_id=?2`, including
 *     fetch-by-id and including DELETE. The id is a uuid and therefore
 *     unguessable, but unguessable is not access control, and the second
 *     predicate is the one that stops a tenant admin from reading a
 *     colleague's line of enquiry. There is no code path in this file that
 *     drops the user predicate for a privileged role, and adding one would
 *     be a privacy regression, not a feature.
 *
 *  2. The permalink codec never sees the database, the tenant or the user.
 *     A token is unsigned, unauthenticated input from a pasted URL; it
 *     describes a viewport, and it authorizes nothing. pl_normalize() is the
 *     single choke point where every field is bounded and clamped, and BOTH
 *     directions (encode and decode) go through it, so the two can never
 *     disagree about what is representable.
 */
#include "savedsearchapi.h"
#include "alertsapi.h"
#include "audit.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/rand.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Permalink field bounds. Ids are rejected (not truncated) when over-long: a
 * truncated id is a valid-looking pointer at the wrong row, which is worse
 * than an absent one. */
#define PL_ID_MAX          128
#define PL_TS_MAX          40
#define PL_LAYERS_MAX      32
#define PL_LAYER_NAME_MAX  64

/* Column order shared by every saved_searches read path so rows decode by
 * fixed index. tenant_id/user_id are absent on purpose: they are predicates,
 * never output. */
#define SS_COLS \
  "id,name,kind,params_json,pinned,created_at,last_run_at,run_count"

/* ── shared idioms (verbatim from alertsapi.c / annotationsapi.c) ─────────── */
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
/* Inverse of b64url(). NULL on any character outside the alphabet — for a
 * permalink that is a hard reject, not a "best effort": a token that decodes
 * to something other than what was minted is worse than an error page. */
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

/* ── query string ────────────────────────────────────────────────────────── */
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
static int truthy(const char *v) {
  return v && (!strcmp(v,"1") || !strcmp(v,"true") || !strcmp(v,"yes"));
}

/* owner > admin > analyst > viewer; an unrecognised role ranks 0 so a future
 * role name fails closed instead of inheriting write. */
static int role_rank(const char *r) {
  if (!r) return 0;
  if (!strcmp(r, "owner"))   return 4;
  if (!strcmp(r, "admin"))   return 3;
  if (!strcmp(r, "analyst")) return 2;
  if (!strcmp(r, "viewer"))  return 1;
  return 0;
}

/* The CHECK constraint in the DDL is the backstop; this is the error message.
 * Keep the two lists identical — a kind accepted here and rejected by SQLite
 * surfaces as a 500 on an otherwise valid request. */
static int kind_valid(const char *k) {
  if (!k || !*k) return 0;
  return !strcmp(k,"intel") || !strcmp(k,"osint") || !strcmp(k,"entity") ||
         !strcmp(k,"breach") || !strcmp(k,"map");
}

static int clamp_limit(const char *v, int dflt) {
  int n = (v && *v) ? atoi(v) : 0;
  if (n <= 0) n = dflt;
  if (n < 1) n = 1;
  if (n > 200) n = 200;
  return n;
}

/* ── saved_searches: decode + validation ─────────────────────────────────── */
static cJSON *decode_ss(sqlite3_stmt *s) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "id",   (const char *)sqlite3_column_text(s,0));
  add_str_or_null(o, "name", ctext(s,1));
  cJSON_AddStringToObject(o, "kind", (const char *)sqlite3_column_text(s,2));
  cJSON_AddItemToObject(o, "params", safe_json(ctext(s,3), 0));
  cJSON_AddBoolToObject(o, "pinned", sqlite3_column_int(s,4) != 0);
  cJSON_AddStringToObject(o, "created_at",
                          (const char *)sqlite3_column_text(s,5));
  add_str_or_null(o, "last_run_at", ctext(s,6));
  cJSON_AddItemToObject(o, "run_count",
                        cJSON_CreateNumber((double)sqlite3_column_int64(s,7)));
  return o;
}

static char *one_ss(db_handle *db, const tenant_ctx *t, const char *id,
                    int code, int *st) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT " SS_COLS " FROM saved_searches "
        "WHERE id=?1 AND tenant_id=?2 AND user_id=?3", -1, &s, NULL) != SQLITE_OK)
    return err(st, 500, "server_error");
  sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(s,2,t->tenant_id,-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(s,3,t->user_id,-1,SQLITE_TRANSIENT);
  char *out;
  if (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *w = cJSON_CreateObject();
    cJSON_AddItemToObject(w, "data", decode_ss(s));
    out = cJSON_PrintUnformatted(w); cJSON_Delete(w); *st = code;
  } else {
    out = err(st, 404, "not_found");
  }
  sqlite3_finalize(s);
  return out;
}

/* Accepts `params` (object) or `params_json` (string that must parse to an
 * object). Returns malloc'd compact JSON, or NULL with *emsg set. Bounding the
 * SERIALIZED form rather than the request body is what actually matters: the
 * stored string is what every later read allocates. */
static char *params_from_body(cJSON *b, const char **emsg) {
  cJSON *p = b ? cJSON_GetObjectItem(b, "params") : NULL;
  cJSON *owned = NULL;
  if (!p || cJSON_IsNull(p)) {
    cJSON *ps = b ? cJSON_GetObjectItem(b, "params_json") : NULL;
    if (ps && cJSON_IsString(ps)) {
      if (strlen(ps->valuestring) > SS_PARAMS_MAX) {
        *emsg = "params_too_large"; return NULL;
      }
      owned = cJSON_Parse(ps->valuestring);
      p = owned;
    }
  }
  if (!p || !cJSON_IsObject(p)) {
    if (owned) cJSON_Delete(owned);
    *emsg = "params_must_be_object"; return NULL;
  }
  char *j = cJSON_PrintUnformatted(p);
  if (owned) cJSON_Delete(owned);
  if (!j) { *emsg = "params_must_be_object"; return NULL; }
  if (strlen(j) > SS_PARAMS_MAX) {
    free(j); *emsg = "params_too_large"; return NULL;
  }
  return j;
}

/* Trim + bound `name`. Returns 0 ok (out[0]==0 means "store NULL"), or an
 * error message. name is optional: an unnamed saved search is a legitimate
 * "just take me back there" bookmark. */
static const char *name_from(cJSON *jn, char *out, size_t cap) {
  out[0] = 0;
  if (!jn || cJSON_IsNull(jn)) return NULL;
  if (!cJSON_IsString(jn)) return "name_must_be_string";
  const char *nm = jn->valuestring;
  while (*nm && isspace((unsigned char)*nm)) nm++;
  size_t nl = strlen(nm);
  while (nl && isspace((unsigned char)nm[nl-1])) nl--;
  if (nl == 0) return NULL;                    /* whitespace-only → NULL name */
  if (nl > SS_NAME_MAX) return "name_too_long";
  snprintf(out, cap, "%.*s", (int)nl, nm);
  return NULL;
}

/* ── search_history ──────────────────────────────────────────────────────── */
/* Prune to the newest SS_HISTORY_KEEP rows for THIS user. Ordering is by the
 * AUTOINCREMENT id, not ts: ts has one-second resolution and a burst of
 * searches inside the same second would make an ORDER BY ts prune arbitrary
 * rows. id is insertion order, which is what "newest" means here. */
static void history_prune(db_handle *db, const char *tid, const char *uid) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "DELETE FROM search_history WHERE tenant_id=?1 AND user_id=?2 AND id "
        "NOT IN (SELECT id FROM search_history WHERE tenant_id=?1 AND "
        "user_id=?2 ORDER BY id DESC LIMIT ?3)", -1, &s, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_text(s,1,tid,-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(s,2,uid,-1,SQLITE_TRANSIENT);
  sqlite3_bind_int (s,3,SS_HISTORY_KEEP);
  sqlite3_step(s); sqlite3_finalize(s);
}

void search_history_record(db_handle *db, const char *tenant_id,
                           const char *user_id, const char *kind,
                           const char *params_json, long result_count) {
  if (!db || !db->h || !tenant_id || !*tenant_id || !user_id || !*user_id)
    return;
  if (!kind_valid(kind)) return;
  if (!params_json || !*params_json || strlen(params_json) > SS_PARAMS_MAX)
    return;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO search_history (tenant_id,user_id,kind,params_json,"
        "result_count) VALUES (?1,?2,?3,?4,?5)", -1, &s, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_text(s,1,tenant_id,-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(s,2,user_id,-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(s,3,kind,-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(s,4,params_json,-1,SQLITE_TRANSIENT);
  if (result_count >= 0) sqlite3_bind_int64(s,5,(sqlite3_int64)result_count);
  else                   sqlite3_bind_null (s,5);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc == SQLITE_DONE) history_prune(db, tenant_id, user_id);
}

char *searchhistoryapi(db_handle *db, const tenant_ctx *t, const char *method,
                       const char *qs, int *st) {
  if (!db || !db->h || !t || !method) return err(st, 500, "server_error");

  if (!strcmp(method, "GET")) {
    char v_kind[32] = {0}, v_lim[24] = {0};
    qget(qs, "kind", v_kind, sizeof v_kind);
    qget(qs, "limit", v_lim, sizeof v_lim);
    if (v_kind[0] && !kind_valid(v_kind)) return err(st, 400, "invalid_kind");
    int lim = clamp_limit(v_lim, 50);

    /* Both branches carry user_id=?2. There is no variant without it. */
    const char *sql = v_kind[0]
      ? "SELECT id,kind,params_json,result_count,ts FROM search_history "
        "WHERE tenant_id=?1 AND user_id=?2 AND kind=?4 "
        "ORDER BY ts DESC, id DESC LIMIT ?3"
      : "SELECT id,kind,params_json,result_count,ts FROM search_history "
        "WHERE tenant_id=?1 AND user_id=?2 "
        "ORDER BY ts DESC, id DESC LIMIT ?3";
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK)
      return err(st, 500, "server_error");
    sqlite3_bind_text(s,1,t->tenant_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,t->user_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int (s,3,lim);
    if (v_kind[0]) sqlite3_bind_text(s,4,v_kind,-1,SQLITE_TRANSIENT);

    cJSON *arr = cJSON_CreateArray();
    int n = 0;
    while (sqlite3_step(s) == SQLITE_ROW) {
      cJSON *r = cJSON_CreateObject();
      cJSON_AddItemToObject(r,"id",
        cJSON_CreateNumber((double)sqlite3_column_int64(s,0)));
      cJSON_AddStringToObject(r,"kind",(const char *)sqlite3_column_text(s,1));
      cJSON_AddItemToObject(r,"params", safe_json(ctext(s,2), 0));
      if (sqlite3_column_type(s,3) == SQLITE_NULL)
        cJSON_AddItemToObject(r,"result_count",cJSON_CreateNull());
      else
        cJSON_AddItemToObject(r,"result_count",
          cJSON_CreateNumber((double)sqlite3_column_int64(s,3)));
      cJSON_AddStringToObject(r,"ts",(const char *)sqlite3_column_text(s,4));
      cJSON_AddItemToArray(arr, r);
      n++;
    }
    sqlite3_finalize(s);

    cJSON *w = cJSON_CreateObject();
    cJSON_AddItemToObject(w,"data",arr);
    cJSON *pg = cJSON_CreateObject();
    cJSON_AddNumberToObject(pg,"limit",(double)lim);
    cJSON_AddNumberToObject(pg,"count",(double)n);
    cJSON_AddItemToObject(w,"page",pg);
    cJSON *mt = cJSON_CreateObject();
    cJSON_AddStringToObject(mt,"scope","user");   /* never tenant-wide */
    cJSON_AddNumberToObject(mt,"retained_max",(double)SS_HISTORY_KEEP);
    cJSON_AddItemToObject(w,"meta",mt);
    char *o = cJSON_PrintUnformatted(w); cJSON_Delete(w);
    *st = 200; return o;
  }

  if (!strcmp(method, "DELETE")) {
    /* Clears the caller's own trail only — any role may do this, because
     * forgetting what you searched for is not a privileged operation. Not
     * audited: recording "user X erased their search history" in a table the
     * tenant owner reads would reinstate exactly the visibility the per-user
     * scoping exists to prevent. */
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h,
          "DELETE FROM search_history WHERE tenant_id=?1 AND user_id=?2",
          -1, &s, NULL) != SQLITE_OK)
      return err(st, 500, "server_error");
    sqlite3_bind_text(s,1,t->tenant_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,t->user_id,-1,SQLITE_TRANSIENT);
    sqlite3_step(s);
    int ch = sqlite3_changes(db->h);
    sqlite3_finalize(s);
    char *o = malloc(64);
    if (!o) return err(st, 500, "server_error");
    snprintf(o, 64, "{\"ok\":true,\"deleted\":%d}", ch);
    *st = 200; return o;
  }

  return err(st, 405, "method_not_allowed");
}

/* ── permalink codec ─────────────────────────────────────────────────────── */
/* Read a number under either the long or the short key, require it to be
 * finite, then clamp into [lo,hi]. Clamping rather than rejecting is right for
 * a camera: a zoom of 1e9 in a pasted link should land the user at max zoom,
 * not at an error page. Clamping is also what stops a number from being used
 * as an allocation or iteration bound downstream. */
static int pl_num(cJSON *o, const char *lk, const char *sk,
                  double lo, double hi, double *out) {
  cJSON *v = cJSON_GetObjectItem(o, lk);
  if (!v || !cJSON_IsNumber(v)) v = sk ? cJSON_GetObjectItem(o, sk) : NULL;
  if (!v || !cJSON_IsNumber(v)) return 0;
  double d = v->valuedouble;
  if (!isfinite(d)) return 0;
  if (d < lo) d = lo;
  if (d > hi) d = hi;
  *out = d; return 1;
}
/* Read a string under either key. Over `cap` → treated as absent (see the
 * PL_ID_MAX comment: a truncated id is a wrong id). */
static const char *pl_str(cJSON *o, const char *lk, const char *sk, size_t cap) {
  cJSON *v = cJSON_GetObjectItem(o, lk);
  if (!v || !cJSON_IsString(v)) v = sk ? cJSON_GetObjectItem(o, sk) : NULL;
  if (!v || !cJSON_IsString(v)) return NULL;
  if (!v->valuestring[0] || strlen(v->valuestring) > cap) return NULL;
  return v->valuestring;
}
static cJSON *pl_obj(cJSON *o, const char *lk, const char *sk) {
  cJSON *v = cJSON_GetObjectItem(o, lk);
  if (!v || !cJSON_IsObject(v)) v = sk ? cJSON_GetObjectItem(o, sk) : NULL;
  return (v && cJSON_IsObject(v)) ? v : NULL;
}

/* THE choke point. Takes an arbitrary (hostile) object in either long-key or
 * short-key form and returns the canonical, fully bounded state object, or
 * NULL if the input is not an object at all. Unknown keys are dropped rather
 * than passed through: a token must not be a general-purpose carrier for
 * whatever a caller feels like stapling to a URL.
 *
 * Every absent field is emitted as null (layers as []), so clients get one
 * stable shape and encode(decode(tok)) is byte-stable. */
static cJSON *pl_normalize(cJSON *in) {
  if (!in || !cJSON_IsObject(in)) return NULL;
  cJSON *o = cJSON_CreateObject();

  const char *kind = pl_str(in, "kind", "k", 16);
  add_str_or_null(o, "kind", kind_valid(kind) ? kind : NULL);

  cJSON *par = pl_obj(in, "params", "p");
  if (par) {
    char *pj = cJSON_PrintUnformatted(par);
    if (pj && strlen(pj) <= SS_PARAMS_MAX) {
      cJSON_AddItemToObject(o, "params", cJSON_Duplicate(par, 1));
      free(pj);
    } else {
      free(pj);
      cJSON_AddItemToObject(o, "params", cJSON_CreateNull());
    }
  } else {
    cJSON_AddItemToObject(o, "params", cJSON_CreateNull());
  }

  cJSON *mp = pl_obj(in, "map", "m");
  if (mp) {
    cJSON *m = cJSON_CreateObject();
    double d;
    if (pl_num(mp, "lat", NULL, -90.0, 90.0, &d))
      cJSON_AddNumberToObject(m, "lat", d);
    /* "lng" is both the wire key and the alias MapLibre/Leaflet clients spell
     * it with; a silently-dropped longitude is a very confusing bug report. */
    if (pl_num(mp, "lon", "lng", -180.0, 180.0, &d))
      cJSON_AddNumberToObject(m, "lon", d);
    if (pl_num(mp, "zoom", "z", 0.0, 24.0, &d))
      cJSON_AddNumberToObject(m, "zoom", d);
    if (pl_num(mp, "bearing", "b", 0.0, 360.0, &d))
      cJSON_AddNumberToObject(m, "bearing", d);
    if (pl_num(mp, "pitch", "t", 0.0, 85.0, &d))
      cJSON_AddNumberToObject(m, "pitch", d);
    if (cJSON_GetArraySize(m) > 0) cJSON_AddItemToObject(o, "map", m);
    else { cJSON_Delete(m); cJSON_AddItemToObject(o, "map", cJSON_CreateNull()); }
  } else {
    cJSON_AddItemToObject(o, "map", cJSON_CreateNull());
  }

  cJSON *ly = cJSON_GetObjectItem(in, "layers");
  if (!ly || !cJSON_IsArray(ly)) ly = cJSON_GetObjectItem(in, "l");
  cJSON *lout = cJSON_CreateArray();
  if (ly && cJSON_IsArray(ly)) {
    int kept = 0; cJSON *e;
    cJSON_ArrayForEach(e, ly) {
      if (kept >= PL_LAYERS_MAX) break;         /* hard cap, not an error */
      if (!cJSON_IsString(e) || !e->valuestring[0]) continue;
      if (strlen(e->valuestring) > PL_LAYER_NAME_MAX) continue;
      cJSON_AddItemToArray(lout, cJSON_CreateString(e->valuestring));
      kept++;
    }
  }
  cJSON_AddItemToObject(o, "layers", lout);

  cJSON *tw = pl_obj(in, "time", "t");
  if (tw) {
    const char *f = pl_str(tw, "from", "f", PL_TS_MAX);
    const char *u = pl_str(tw, "to", "u", PL_TS_MAX);
    if (f || u) {
      cJSON *w = cJSON_CreateObject();
      add_str_or_null(w, "from", f);
      add_str_or_null(w, "to", u);
      cJSON_AddItemToObject(o, "time", w);
    } else {
      cJSON_AddItemToObject(o, "time", cJSON_CreateNull());
    }
  } else {
    cJSON_AddItemToObject(o, "time", cJSON_CreateNull());
  }

  /* Ids are carried so a link reopens the right detail pane. They are NOT a
   * grant: the entity/case routes re-check tenancy and case membership from
   * the session, and a token naming a case the viewer cannot open must (and
   * does) still 403/404 there. */
  add_str_or_null(o, "entity_id", pl_str(in, "entity_id", "e", PL_ID_MAX));
  add_str_or_null(o, "case_id",   pl_str(in, "case_id",   "c", PL_ID_MAX));
  return o;
}

/* Canonical → wire (one-letter keys). Nulls and the empty layer array are
 * dropped so the common "just a map camera" token stays short. */
static cJSON *pl_compact(cJSON *c) {
  cJSON *o = cJSON_CreateObject();
  cJSON *v;
  if ((v = cJSON_GetObjectItem(c,"kind")) && cJSON_IsString(v))
    cJSON_AddStringToObject(o,"k",v->valuestring);
  if ((v = cJSON_GetObjectItem(c,"params")) && cJSON_IsObject(v))
    cJSON_AddItemToObject(o,"p",cJSON_Duplicate(v,1));
  if ((v = cJSON_GetObjectItem(c,"map")) && cJSON_IsObject(v)) {
    cJSON *m = cJSON_CreateObject(), *x;
    if ((x = cJSON_GetObjectItem(v,"lat")))     cJSON_AddNumberToObject(m,"lat",x->valuedouble);
    if ((x = cJSON_GetObjectItem(v,"lon")))     cJSON_AddNumberToObject(m,"lng",x->valuedouble);
    if ((x = cJSON_GetObjectItem(v,"zoom")))    cJSON_AddNumberToObject(m,"z",x->valuedouble);
    if ((x = cJSON_GetObjectItem(v,"bearing"))) cJSON_AddNumberToObject(m,"b",x->valuedouble);
    if ((x = cJSON_GetObjectItem(v,"pitch")))   cJSON_AddNumberToObject(m,"t",x->valuedouble);
    cJSON_AddItemToObject(o,"m",m);
  }
  if ((v = cJSON_GetObjectItem(c,"layers")) && cJSON_IsArray(v) &&
      cJSON_GetArraySize(v) > 0)
    cJSON_AddItemToObject(o,"l",cJSON_Duplicate(v,1));
  if ((v = cJSON_GetObjectItem(c,"time")) && cJSON_IsObject(v)) {
    cJSON *w = cJSON_CreateObject(), *x;
    if ((x = cJSON_GetObjectItem(v,"from")) && cJSON_IsString(x))
      cJSON_AddStringToObject(w,"f",x->valuestring);
    if ((x = cJSON_GetObjectItem(v,"to")) && cJSON_IsString(x))
      cJSON_AddStringToObject(w,"u",x->valuestring);
    cJSON_AddItemToObject(o,"t",w);
  }
  if ((v = cJSON_GetObjectItem(c,"entity_id")) && cJSON_IsString(v))
    cJSON_AddStringToObject(o,"e",v->valuestring);
  if ((v = cJSON_GetObjectItem(c,"case_id")) && cJSON_IsString(v))
    cJSON_AddStringToObject(o,"c",v->valuestring);
  return o;
}

/* canonical → "v1.<b64url>". *too_big is set when the compact state exceeds
 * PL_STATE_MAX so the handler can say which of the two failures happened. */
static char *pl_token(cJSON *canon, int *too_big) {
  *too_big = 0;
  cJSON *cm = pl_compact(canon);
  char *j = cJSON_PrintUnformatted(cm);
  cJSON_Delete(cm);
  if (!j) return NULL;
  if (strlen(j) > PL_STATE_MAX) { free(j); *too_big = 1; return NULL; }
  char *b = b64url(j); free(j);
  if (!b) return NULL;
  size_t n = strlen(b) + 4;
  if (n > PL_TOKEN_MAX) { free(b); *too_big = 1; return NULL; }
  char *tok = malloc(n);
  if (!tok) { free(b); return NULL; }
  snprintf(tok, n, "v1.%s", b);
  free(b);
  return tok;
}

char *permalink_decode(const char *token) {
  if (!token || !*token) return NULL;
  size_t tl = strlen(token);
  if (tl > PL_TOKEN_MAX) return NULL;           /* bound BEFORE any allocation */
  /* Version prefix first: an unknown version is rejected, never best-effort
   * parsed, so a v2 link cannot silently decode into a plausible v1 view. */
  if (strncmp(token, "v1.", 3) != 0) return NULL;
  const char *b = token + 3;
  if (!*b) return NULL;
  char *raw = b64url_decode(b);
  if (!raw) return NULL;
  if (strlen(raw) > PL_STATE_MAX) { free(raw); return NULL; }
  cJSON *in = cJSON_Parse(raw);
  free(raw);
  if (!in) return NULL;                          /* not JSON → reject */
  cJSON *canon = pl_normalize(in);
  cJSON_Delete(in);
  if (!canon) return NULL;                       /* JSON but not an object */
  char *out = cJSON_PrintUnformatted(canon);
  cJSON_Delete(canon);
  return out;
}

char *permalinkapi(const char *method, const char *token, const char *body,
                   int *st) {
  if (!method) return err(st, 500, "server_error");

  if (!strcmp(method, "GET")) {
    if (!token || !token[0]) return err(st, 400, "token_required");
    char *state = permalink_decode(token);
    if (!state) return err(st, 400, "invalid_permalink");
    size_t n = strlen(state) + 16;
    char *o = malloc(n);
    if (!o) { free(state); return err(st, 500, "server_error"); }
    snprintf(o, n, "{\"data\":%s}", state);
    free(state);
    *st = 200; return o;
  }

  if (!strcmp(method, "POST")) {
    cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
    if (!jb) return err(st, 400, "invalid_state");
    /* Accept the state at the top level or wrapped in {"state":{...}} — both
     * shapes show up in practice and neither is ambiguous. */
    cJSON *src = cJSON_GetObjectItem(jb, "state");
    if (!src || !cJSON_IsObject(src)) src = jb;
    cJSON *canon = pl_normalize(src);
    if (!canon) { cJSON_Delete(jb); return err(st, 400, "invalid_state"); }
    int tb = 0;
    char *tok = pl_token(canon, &tb);
    if (!tok) {
      cJSON_Delete(canon); cJSON_Delete(jb);
      return err(st, tb ? 400 : 500, tb ? "state_too_large" : "server_error");
    }
    cJSON *w = cJSON_CreateObject();
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "token", tok);
    cJSON_AddStringToObject(d, "version", "v1");
    cJSON_AddItemToObject(d, "state", canon);    /* canon now owned by d */
    cJSON_AddItemToObject(w, "data", d);
    free(tok);
    cJSON_Delete(jb);
    char *o = cJSON_PrintUnformatted(w); cJSON_Delete(w);
    if (!o) return err(st, 500, "server_error");
    *st = 200; return o;
  }

  return err(st, 405, "method_not_allowed");
}

/* ── saved search → alert rule (the upsell path) ─────────────────────────── */
/* Maps intel search params onto the alert predicate vocabulary that
 * alert_eval.c actually understands (q / source_ids / tags_any / tags_all /
 * bbox). Anything the predicate has no equivalent for is collected in
 * `dropped` and reported back, because the failure mode we must avoid is a
 * rule that looks like the saved search but quietly matches a different set.
 *
 * Time bounds (since/until) are dropped by design and not an error: an alert
 * evaluates each new item as it arrives, so a fixed historical window is
 * meaningless there, and silently keeping it would produce a rule that can
 * never fire again. */
static cJSON *predicate_from_intel(cJSON *p, cJSON *dropped) {
  cJSON *pred = cJSON_CreateObject();
  cJSON *x;

  if ((x = cJSON_GetObjectItem(p,"q")) && cJSON_IsString(x) && x->valuestring[0])
    cJSON_AddStringToObject(pred, "q", x->valuestring);

  cJSON *src = cJSON_GetObjectItem(p,"source_ids");
  if (!src || !cJSON_IsArray(src)) src = cJSON_GetObjectItem(p,"sources");
  if (src && cJSON_IsArray(src) && cJSON_GetArraySize(src) > 0) {
    cJSON_AddItemToObject(pred, "source_ids", cJSON_Duplicate(src,1));
  } else if ((x = cJSON_GetObjectItem(p,"source")) && cJSON_IsString(x) &&
             x->valuestring[0]) {
    cJSON *a = cJSON_CreateArray();
    cJSON_AddItemToArray(a, cJSON_CreateString(x->valuestring));
    cJSON_AddItemToObject(pred, "source_ids", a);
  }

  cJSON *tg = cJSON_GetObjectItem(p,"tags_any");
  if (!tg || !cJSON_IsArray(tg)) tg = cJSON_GetObjectItem(p,"tags");
  if (tg && cJSON_IsArray(tg) && cJSON_GetArraySize(tg) > 0) {
    cJSON_AddItemToObject(pred, "tags_any", cJSON_Duplicate(tg,1));
  } else if ((x = cJSON_GetObjectItem(p,"tag")) && cJSON_IsString(x) &&
             x->valuestring[0]) {
    cJSON *a = cJSON_CreateArray();
    cJSON_AddItemToArray(a, cJSON_CreateString(x->valuestring));
    cJSON_AddItemToObject(pred, "tags_any", a);
  }

  if ((x = cJSON_GetObjectItem(p,"tags_all")) && cJSON_IsArray(x) &&
      cJSON_GetArraySize(x) > 0)
    cJSON_AddItemToObject(pred, "tags_all", cJSON_Duplicate(x,1));

  if ((x = cJSON_GetObjectItem(p,"bbox")) && cJSON_IsArray(x) &&
      cJSON_GetArraySize(x) == 4)
    cJSON_AddItemToObject(pred, "bbox", cJSON_Duplicate(x,1));

  static const char *DROP[] = { "since", "until", "lang", "record_type",
    "sub_source_id", "has_geom", "cursor", "limit", "sort", NULL };
  for (int i = 0; DROP[i]; i++) {
    cJSON *d = cJSON_GetObjectItem(p, DROP[i]);
    if (d && !cJSON_IsNull(d))
      cJSON_AddItemToArray(dropped, cJSON_CreateString(DROP[i]));
  }
  return pred;
}

/* Why a non-intel kind is a 400 and not a best-effort conversion: alert_eval.c
 * runs against intel_items at the ingest chokepoint. An 'entity', 'breach',
 * 'map' or 'osint' saved search has no stream of new intel_items rows that
 * corresponds to it, so any predicate we synthesised would either match
 * nothing (a rule that never fires, which the analyst reads as "alerts are
 * broken") or match everything (a mailbox flood). Both are worse than a clear
 * refusal, so we refuse and name the kind. */
static char *to_alert(db_handle *db, const tenant_ctx *t, const char *id,
                      cJSON *jb, int *st) {
  if (role_rank(t->role) < 2) return err(st, 403, "forbidden");

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT name,kind,params_json FROM saved_searches "
        "WHERE id=?1 AND tenant_id=?2 AND user_id=?3", -1, &s, NULL) != SQLITE_OK)
    return err(st, 500, "server_error");
  sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(s,2,t->tenant_id,-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(s,3,t->user_id,-1,SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) {
    sqlite3_finalize(s); return err(st, 404, "not_found");
  }
  const char *sname = ctext(s,0);
  const char *kind  = (const char *)sqlite3_column_text(s,1);
  char nbuf[SS_NAME_MAX + 8] = {0};
  char kbuf[32] = {0};
  if (sname) snprintf(nbuf, sizeof nbuf, "%s", sname);
  snprintf(kbuf, sizeof kbuf, "%s", kind ? kind : "");
  cJSON *params = safe_json(ctext(s,2), 0);
  sqlite3_finalize(s);

  if (strcmp(kbuf, "intel") != 0) {
    cJSON_Delete(params);
    char msg[192];
    snprintf(msg, sizeof msg,
      "only kind='intel' converts to an alert rule; this saved search is "
      "kind='%s' and alert matching runs over intel items only", kbuf);
    return err(st, 400, msg);
  }

  cJSON *dropped = cJSON_CreateArray();
  cJSON *pred = predicate_from_intel(params, dropped);
  cJSON_Delete(params);

  if (cJSON_GetArraySize(pred) == 0) {
    cJSON_Delete(pred); cJSON_Delete(dropped);
    return err(st, 400, "saved search has no alertable filters (need at least "
                        "one of q, source, tag or bbox) — an empty predicate "
                        "would match every incoming item");
  }

  /* Build the alert-rule request body and re-enter the PUBLIC alertsapi()
   * entrypoint. validate_rule() therefore stays a single copy in alertsapi.c
   * and every channel/target/secret/dedup/storm check applies here unchanged —
   * including "at least one channel required", which is why `channels` is
   * copied through verbatim rather than defaulted to something. */
  cJSON *rb = cJSON_CreateObject();
  cJSON *bn = jb ? cJSON_GetObjectItem(jb, "name") : NULL;
  if (bn && cJSON_IsString(bn) && bn->valuestring[0])
    cJSON_AddStringToObject(rb, "name", bn->valuestring);
  else
    cJSON_AddStringToObject(rb, "name", nbuf[0] ? nbuf : "Saved search alert");
  cJSON_AddItemToObject(rb, "predicate", pred);        /* rb owns pred now */
  cJSON *pass[] = { jb ? cJSON_GetObjectItem(jb,"channels") : NULL,
                    jb ? cJSON_GetObjectItem(jb,"dedup_window_sec") : NULL,
                    jb ? cJSON_GetObjectItem(jb,"storm_cap_per_hour") : NULL,
                    jb ? cJSON_GetObjectItem(jb,"enabled") : NULL };
  static const char *PASS_K[] = { "channels", "dedup_window_sec",
                                  "storm_cap_per_hour", "enabled" };
  for (int i = 0; i < 4; i++)
    if (pass[i]) cJSON_AddItemToObject(rb, PASS_K[i], cJSON_Duplicate(pass[i],1));

  char *rbj = cJSON_PrintUnformatted(rb);
  cJSON_Delete(rb);
  if (!rbj) { cJSON_Delete(dropped); return err(st, 500, "server_error"); }

  int rst = 200;
  char *rout = alertsapi(db, t->tenant_id, t->user_id, "POST", "", "", rbj, 0,
                         &rst);
  free(rbj);
  if (rst != 201) {
    /* Propagate alertsapi's own 400 verbatim — its wording IS the contract for
     * channel validation, and paraphrasing it here would fork the message the
     * same way copying the validator would fork the logic. */
    cJSON_Delete(dropped);
    *st = rst;
    return rout ? rout : err(st, rst ? rst : 500, "server_error");
  }

  cJSON *ro = rout ? cJSON_Parse(rout) : NULL;
  free(rout);
  cJSON *rd = ro ? cJSON_GetObjectItem(ro, "data") : NULL;
  cJSON *rid = rd ? cJSON_GetObjectItem(rd, "id") : NULL;
  const char *rule_id = (rid && cJSON_IsString(rid)) ? rid->valuestring : NULL;

  /* Audited: this is the one mutation here that creates tenant-visible,
   * outbound-mailing infrastructure from a private bookmark. The payload
   * names the rule and what was dropped, not the query text. */
  cJSON *ap = cJSON_CreateObject();
  add_str_or_null(ap, "rule_id", rule_id);
  cJSON_AddStringToObject(ap, "kind", "intel");
  cJSON_AddItemToObject(ap, "dropped_params", cJSON_Duplicate(dropped,1));
  char *apj = cJSON_PrintUnformatted(ap); cJSON_Delete(ap);
  audit_write(db, t->tenant_id, t->user_id, "saved_search.to_alert", id, apj);
  free(apj);

  cJSON *w = cJSON_CreateObject();
  cJSON_AddItemToObject(w, "data", rd ? cJSON_Duplicate(rd,1) : cJSON_CreateObject());
  cJSON *mt = cJSON_CreateObject();
  cJSON_AddStringToObject(mt, "saved_search_id", id);
  add_str_or_null(mt, "rule_id", rule_id);
  cJSON_AddItemToObject(mt, "dropped_params", dropped);   /* mt owns it now */
  cJSON_AddItemToObject(w, "meta", mt);
  if (ro) cJSON_Delete(ro);
  char *o = cJSON_PrintUnformatted(w); cJSON_Delete(w);
  if (!o) return err(st, 500, "server_error");
  *st = 201; return o;
}

/* ── POST /api/saved-searches/:id/run ────────────────────────────────────── */
static char *run_saved(db_handle *db, const tenant_ctx *t, const char *id,
                       cJSON *jb, int *st) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT kind,params_json FROM saved_searches "
        "WHERE id=?1 AND tenant_id=?2 AND user_id=?3", -1, &s, NULL) != SQLITE_OK)
    return err(st, 500, "server_error");
  sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(s,2,t->tenant_id,-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(s,3,t->user_id,-1,SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) {
    sqlite3_finalize(s); return err(st, 404, "not_found");
  }
  char kbuf[32] = {0};
  const char *k = (const char *)sqlite3_column_text(s,0);
  snprintf(kbuf, sizeof kbuf, "%s", k ? k : "");
  const char *pj = ctext(s,1);
  char *pcopy = pj ? strdup(pj) : strdup("{}");
  sqlite3_finalize(s);
  if (!pcopy) return err(st, 500, "server_error");

  cJSON *rc = jb ? cJSON_GetObjectItem(jb, "result_count") : NULL;
  long rcount = (rc && cJSON_IsNumber(rc) && isfinite(rc->valuedouble) &&
                 rc->valuedouble >= 0) ? (long)rc->valuedouble : -1;

  /* Counter bump and history row are one unit: a run that is counted but not
   * recorded (or the reverse) makes "last_run_at" and the trail disagree. */
  sqlite3_exec(db->h, "BEGIN", 0, 0, 0);
  if (sqlite3_prepare_v2(db->h,
        "UPDATE saved_searches SET last_run_at=datetime('now'), "
        "run_count=run_count+1 WHERE id=?1 AND tenant_id=?2 AND user_id=?3",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,t->tenant_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,3,t->user_id,-1,SQLITE_TRANSIENT);
    sqlite3_step(s);
  }
  sqlite3_finalize(s);
  search_history_record(db, t->tenant_id, t->user_id, kbuf, pcopy, rcount);
  sqlite3_exec(db->h, "COMMIT", 0, 0, 0);
  free(pcopy);

  char *row = one_ss(db, t, id, 200, st);
  if (*st != 200) return row;

  /* Re-open the envelope to attach meta. Cheap, and it keeps one decoder. */
  cJSON *w = cJSON_Parse(row);
  free(row);
  if (!w) return err(st, 500, "server_error");
  cJSON *mt = cJSON_CreateObject();
  cJSON_AddBoolToObject(mt, "executed", 0);
  cJSON_AddStringToObject(mt, "execute_hint",
    "bookkeeping only: re-issue the query yourself using data.params against "
    "the endpoint for data.kind (intel → /api/intel/items, osint → "
    "/api/search/analyze, entity → /api/entities, breach → /api/breach, "
    "map → the map layer endpoints)");
  cJSON_AddItemToObject(w, "meta", mt);
  char *o = cJSON_PrintUnformatted(w); cJSON_Delete(w);
  if (!o) return err(st, 500, "server_error");
  *st = 200; return o;
}

/* ── dispatcher ──────────────────────────────────────────────────────────── */
char *savedsearchapi(db_handle *db, const tenant_ctx *t, const char *method,
                     const char *seg, const char *act, const char *qs,
                     const char *body, int *st) {
  if (!db || !db->h || !t || !method || !seg || !act)
    return err(st, 500, "server_error");

  const int is_get   = !strcmp(method, "GET");
  const int is_post  = !strcmp(method, "POST");
  const int is_patch = !strcmp(method, "PATCH");
  const int is_del   = !strcmp(method, "DELETE");

  cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
  char *out = NULL;

  /* ── collection ───────────────────────────────────────────────────────── */
  if (!seg[0]) {
    if (is_get) {
      char v_kind[32] = {0}, v_lim[24] = {0}, v_pin[16] = {0};
      qget(qs, "kind", v_kind, sizeof v_kind);
      qget(qs, "limit", v_lim, sizeof v_lim);
      qget(qs, "pinned", v_pin, sizeof v_pin);
      if (v_kind[0] && !kind_valid(v_kind)) {
        out = err(st, 400, "invalid_kind"); goto done;
      }
      int lim = clamp_limit(v_lim, 50);
      int only_pinned = truthy(v_pin);

      /* Four literal statements rather than a concatenated WHERE: the filter
       * combinations are few and fixed, and no user text ever reaches the SQL
       * text this way. */
      const char *sql;
      if (v_kind[0] && only_pinned)
        sql = "SELECT " SS_COLS " FROM saved_searches WHERE tenant_id=?1 AND "
              "user_id=?2 AND kind=?4 AND pinned=1 "
              "ORDER BY pinned DESC, created_at DESC, id DESC LIMIT ?3";
      else if (v_kind[0])
        sql = "SELECT " SS_COLS " FROM saved_searches WHERE tenant_id=?1 AND "
              "user_id=?2 AND kind=?4 "
              "ORDER BY pinned DESC, created_at DESC, id DESC LIMIT ?3";
      else if (only_pinned)
        sql = "SELECT " SS_COLS " FROM saved_searches WHERE tenant_id=?1 AND "
              "user_id=?2 AND pinned=1 "
              "ORDER BY pinned DESC, created_at DESC, id DESC LIMIT ?3";
      else
        sql = "SELECT " SS_COLS " FROM saved_searches WHERE tenant_id=?1 AND "
              "user_id=?2 ORDER BY pinned DESC, created_at DESC, id DESC "
              "LIMIT ?3";

      sqlite3_stmt *s;
      if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
        out = err(st, 500, "server_error"); goto done;
      }
      sqlite3_bind_text(s,1,t->tenant_id,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,2,t->user_id,-1,SQLITE_TRANSIENT);
      sqlite3_bind_int (s,3,lim);
      if (v_kind[0]) sqlite3_bind_text(s,4,v_kind,-1,SQLITE_TRANSIENT);
      cJSON *arr = cJSON_CreateArray();
      int n = 0;
      while (sqlite3_step(s) == SQLITE_ROW) {
        cJSON_AddItemToArray(arr, decode_ss(s)); n++;
      }
      sqlite3_finalize(s);

      cJSON *w = cJSON_CreateObject();
      cJSON_AddItemToObject(w, "data", arr);
      cJSON *pg = cJSON_CreateObject();
      cJSON_AddNumberToObject(pg, "limit", (double)lim);
      cJSON_AddNumberToObject(pg, "count", (double)n);
      cJSON_AddItemToObject(w, "page", pg);
      cJSON *mt = cJSON_CreateObject();
      cJSON_AddStringToObject(mt, "scope", "user");
      cJSON_AddItemToObject(w, "meta", mt);
      out = cJSON_PrintUnformatted(w); cJSON_Delete(w);
      *st = 200; goto done;
    }

    if (is_post) {
      if (!jb || !cJSON_IsObject(jb)) {
        out = err(st, 400, "body_required"); goto done;
      }
      cJSON *jk = cJSON_GetObjectItem(jb, "kind");
      const char *kind = (jk && cJSON_IsString(jk)) ? jk->valuestring : NULL;
      if (!kind_valid(kind)) {
        out = err(st, 400,
          "kind must be one of intel, osint, entity, breach, map"); goto done;
      }
      char nbuf[SS_NAME_MAX + 8];
      const char *ne = name_from(cJSON_GetObjectItem(jb,"name"), nbuf, sizeof nbuf);
      if (ne) { out = err(st, 400, ne); goto done; }
      const char *pe = NULL;
      char *pjson = params_from_body(jb, &pe);
      if (!pjson) { out = err(st, 400, pe ? pe : "params_must_be_object"); goto done; }
      cJSON *jp = cJSON_GetObjectItem(jb, "pinned");
      int pinned = (jp && cJSON_IsBool(jp)) ? (cJSON_IsTrue(jp) ? 1 : 0) : 0;

      char nid[37]; uuid4(nid);
      sqlite3_stmt *s;
      if (sqlite3_prepare_v2(db->h,
            "INSERT INTO saved_searches (id,tenant_id,user_id,name,kind,"
            "params_json,pinned) VALUES (?1,?2,?3,?4,?5,?6,?7)",
            -1, &s, NULL) != SQLITE_OK) {
        free(pjson); out = err(st, 500, "server_error"); goto done;
      }
      sqlite3_bind_text(s,1,nid,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,2,t->tenant_id,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,3,t->user_id,-1,SQLITE_TRANSIENT);
      if (nbuf[0]) sqlite3_bind_text(s,4,nbuf,-1,SQLITE_TRANSIENT);
      else         sqlite3_bind_null(s,4);
      sqlite3_bind_text(s,5,kind,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,6,pjson,-1,SQLITE_TRANSIENT);
      sqlite3_bind_int (s,7,pinned);
      int rc = sqlite3_step(s);
      sqlite3_finalize(s);

      if (rc != SQLITE_DONE) { free(pjson); out = err(st, 500, "server_error"); goto done; }

      /* Audited. The payload carries kind/name/size but NOT params: the query
       * text is the analyst's line of enquiry, and audit_events is read by a
       * wider audience under a different retention regime than the row it
       * describes (same stance as annotation bodies). */
      cJSON *ap = cJSON_CreateObject();
      cJSON_AddStringToObject(ap, "kind", kind);
      add_str_or_null(ap, "name", nbuf[0] ? nbuf : NULL);
      cJSON_AddNumberToObject(ap, "params_bytes", (double)strlen(pjson));
      char *apj = cJSON_PrintUnformatted(ap); cJSON_Delete(ap);
      audit_write(db, t->tenant_id, t->user_id, "saved_search.create", nid, apj);
      free(apj);
      free(pjson);

      out = one_ss(db, t, nid, 201, st); goto done;
    }

    out = err(st, 405, "method_not_allowed"); goto done;
  }

  /* ── item ─────────────────────────────────────────────────────────────── */
  if (!act[0]) {
    if (is_get) { out = one_ss(db, t, seg, 200, st); goto done; }

    if (is_patch) {
      if (!jb || !cJSON_IsObject(jb)) { out = err(st, 400, "body_required"); goto done; }
      /* `kind` is deliberately immutable. It selects both the execution
       * endpoint and whether to-alert is legal; letting an 'intel' bookmark
       * become a 'map' one would leave a rule derived from it pointing at
       * params that no longer mean the same thing. Delete and re-save. */
      if (cJSON_HasObjectItem(jb, "kind")) {
        out = err(st, 400, "kind is immutable; delete and re-save"); goto done;
      }
      sqlite3_stmt *s;
      if (sqlite3_prepare_v2(db->h,
            "SELECT name,params_json,pinned FROM saved_searches "
            "WHERE id=?1 AND tenant_id=?2 AND user_id=?3", -1, &s, NULL) != SQLITE_OK) {
        out = err(st, 500, "server_error"); goto done;
      }
      sqlite3_bind_text(s,1,seg,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,2,t->tenant_id,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,3,t->user_id,-1,SQLITE_TRANSIENT);
      if (sqlite3_step(s) != SQLITE_ROW) {
        sqlite3_finalize(s); out = err(st, 404, "not_found"); goto done;
      }
      const char *cur_name = ctext(s,0);
      const char *cur_par  = ctext(s,1);
      int cur_pin = sqlite3_column_int(s,2);
      char nbuf[SS_NAME_MAX + 8] = {0};
      char *pjson = NULL;
      const char *emsg = NULL;
      int have_name = cJSON_HasObjectItem(jb, "name");
      if (have_name) {
        emsg = name_from(cJSON_GetObjectItem(jb,"name"), nbuf, sizeof nbuf);
      } else if (cur_name) {
        snprintf(nbuf, sizeof nbuf, "%s", cur_name);
      }
      if (!emsg) {
        if (cJSON_HasObjectItem(jb,"params") || cJSON_HasObjectItem(jb,"params_json"))
          pjson = params_from_body(jb, &emsg);
        else
          pjson = strdup(cur_par ? cur_par : "{}");
        if (!pjson && !emsg) emsg = "server_error";
      }
      int pinned = cur_pin;
      cJSON *jp = cJSON_GetObjectItem(jb, "pinned");
      if (jp && cJSON_IsBool(jp)) pinned = cJSON_IsTrue(jp) ? 1 : 0;
      sqlite3_finalize(s);
      if (emsg) { free(pjson); out = err(st, 400, emsg); goto done; }

      if (sqlite3_prepare_v2(db->h,
            "UPDATE saved_searches SET name=?4, params_json=?5, pinned=?6 "
            "WHERE id=?1 AND tenant_id=?2 AND user_id=?3", -1, &s, NULL) != SQLITE_OK) {
        free(pjson); out = err(st, 500, "server_error"); goto done;
      }
      sqlite3_bind_text(s,1,seg,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,2,t->tenant_id,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,3,t->user_id,-1,SQLITE_TRANSIENT);
      if (nbuf[0]) sqlite3_bind_text(s,4,nbuf,-1,SQLITE_TRANSIENT);
      else         sqlite3_bind_null(s,4);
      sqlite3_bind_text(s,5,pjson,-1,SQLITE_TRANSIENT);
      sqlite3_bind_int (s,6,pinned);
      sqlite3_step(s);
      sqlite3_finalize(s);
      free(pjson);
      out = one_ss(db, t, seg, 200, st); goto done;
    }

    if (is_del) {
      sqlite3_stmt *s;
      if (sqlite3_prepare_v2(db->h,
            "DELETE FROM saved_searches WHERE id=?1 AND tenant_id=?2 AND "
            "user_id=?3", -1, &s, NULL) != SQLITE_OK) {
        out = err(st, 500, "server_error"); goto done;
      }
      sqlite3_bind_text(s,1,seg,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,2,t->tenant_id,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,3,t->user_id,-1,SQLITE_TRANSIENT);
      sqlite3_step(s);
      int ch = sqlite3_changes(db->h);
      sqlite3_finalize(s);
      if (ch == 0) { out = err(st, 404, "not_found"); goto done; }
      audit_write(db, t->tenant_id, t->user_id, "saved_search.delete", seg, NULL);
      *st = 204; out = NULL; goto done;
    }

    out = err(st, 405, "method_not_allowed"); goto done;
  }

  /* ── sub-actions ──────────────────────────────────────────────────────── */
  if (is_post && !strcmp(act, "run"))      { out = run_saved(db, t, seg, jb, st); goto done; }
  if (is_post && !strcmp(act, "to-alert")) { out = to_alert (db, t, seg, jb, st); goto done; }

  out = err(st, 404, "not_found");

done:
  if (jb) cJSON_Delete(jb);
  return out;
}
