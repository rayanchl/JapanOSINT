/* core/station_merges.c — see station_merges.h. */
#include "station_merges.h"
#include "audit.h"
#include "llm.h"
#include "llm_fault.h"
#include "prompts.h"
#include "httpclient.h"
#include "../lib/vecindex.h"       /* vecindex_served_model: GET /v1/models */
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SM_BUDGET_DEFAULT 20
#define SM_BUDGET_MAX     500
#define SM_LIST_MAX       500
#define SM_NOTE_MAX       500

/* ---- migration ---------------------------------------------------------- */

static int has_column(db_handle *db, const char *col) {
  sqlite3_stmt *s; int found = 0;
  if (sqlite3_prepare_v2(db->h, "PRAGMA table_info(llm_station_merges)", -1,
                         &s, NULL) != SQLITE_OK) return 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    const unsigned char *n = sqlite3_column_text(s, 1);
    if (n && !strcmp((const char *)n, col)) { found = 1; break; }
  }
  sqlite3_finalize(s);
  return found;
}

static void ensure(db_handle *db, const char *col, const char *decl) {
  if (has_column(db, col)) return;
  char sql[320];
  snprintf(sql, sizeof sql, "ALTER TABLE llm_station_merges ADD COLUMN %s %s",
           col, decl);
  sqlite3_exec(db->h, sql, NULL, NULL, NULL);
}

void station_merges_migrate(db_handle *db) {
  if (!db || !db->h) return;
  /* core/schema.sql's declaration, for a database created without it. */
  sqlite3_exec(db->h,
    "CREATE TABLE IF NOT EXISTS llm_station_merges ("
    " uid_a TEXT NOT NULL, uid_b TEXT NOT NULL,"
    " same INTEGER NOT NULL CHECK(same IN (0,1)),"
    " confidence REAL NOT NULL, reason TEXT,"
    " decided_at TEXT NOT NULL DEFAULT (datetime('now')),"
    " PRIMARY KEY (uid_a, uid_b))", NULL, NULL, NULL);
  /* A stable id for the review routes. The table's key is the uid pair and
   * its rowid is not stable across VACUUM, so an approval addressed by rowid
   * could land on a different pair after one. A random id, backfilled for any
   * row a past writer left, and unique. */
  ensure(db, "id", "TEXT");
  sqlite3_exec(db->h,
    "UPDATE llm_station_merges SET id = lower(hex(randomblob(8))) "
    "WHERE id IS NULL", NULL, NULL, NULL);
  sqlite3_exec(db->h,
    "CREATE UNIQUE INDEX IF NOT EXISTS idx_llm_station_merges_id "
    "ON llm_station_merges(id)", NULL, NULL, NULL);
  /* pending until a person decides; see station_merges.h for what each
   * status means to the reader. */
  ensure(db, "status", "TEXT NOT NULL DEFAULT 'pending' "
                       "CHECK(status IN ('pending','approved','rejected'))");
  ensure(db, "model", "TEXT");          /* what /v1/models said; NULL = unknown */
  ensure(db, "reviewed_by", "TEXT");
  ensure(db, "reviewed_at", "TEXT");
  /* What the reviewer needs to decide without a second lookup: the names the
   * judge was shown and how far apart the two were. */
  ensure(db, "name_a", "TEXT");
  ensure(db, "name_b", "TEXT");
  ensure(db, "dist_m", "REAL");
  sqlite3_exec(db->h,
    "CREATE INDEX IF NOT EXISTS idx_llm_station_merges_status "
    "ON llm_station_merges(status, decided_at)", NULL, NULL, NULL);
}

/* ---- the judge ---------------------------------------------------------- */

int station_merges_judge_budget(void) {
  const char *e = getenv("JO_STATION_JUDGE_MAX");
  if (!e || !*e) return SM_BUDGET_DEFAULT;
  char *end = NULL;
  long v = strtol(e, &end, 10);
  if (!end || end == e || *end || v < 0) return SM_BUDGET_DEFAULT;
  return v > SM_BUDGET_MAX ? SM_BUDGET_MAX : (int)v;
}

int station_merges_judge_enabled(void) {
  /* llm_init() falls back to http://localhost:8080 when LLM_BASE_URL is
   * unset, so "configured" is asked of the variable itself: an unconfigured
   * deployment writes nothing rather than probing a port nobody chose. */
  const char *b = getenv("LLM_BASE_URL");
  return b && *b && station_merges_judge_budget() > 0;
}

/* Canonical order, so (a,b) and (b,a) are one row. */
static void order_pair(const char **a, const char **b) {
  if (strcmp(*a, *b) > 0) { const char *t = *a; *a = *b; *b = t; }
}

int station_merges_known(db_handle *db, const char *uid_a, const char *uid_b) {
  if (!uid_a || !uid_b) return 0;
  order_pair(&uid_a, &uid_b);
  sqlite3_stmt *s; int k = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM llm_station_merges WHERE uid_a=?1 AND uid_b=?2",
        -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, uid_a, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, uid_b, -1, SQLITE_TRANSIENT);
  k = sqlite3_step(s) == SQLITE_ROW;
  sqlite3_finalize(s);
  return k;
}

static void bind_opt(sqlite3_stmt *s, int i, const char *v) {
  if (v && *v) sqlite3_bind_text(s, i, v, -1, SQLITE_TRANSIENT);
  else sqlite3_bind_null(s, i);
}

int station_merges_record(db_handle *db, const station_merge_pair *p, int same,
                          double confidence, const char *reason,
                          const char *model) {
  if (!p || !p->uid_a || !p->uid_b || !strcmp(p->uid_a, p->uid_b)) return -1;
  if (!isfinite(confidence) || confidence < 0 || confidence > 1) return -1;
  const char *ua = p->uid_a, *ub = p->uid_b;
  const char *na = p->name_a, *nb = p->name_b;
  if (strcmp(ua, ub) > 0) { ua = p->uid_b; ub = p->uid_a; na = p->name_b; nb = p->name_a; }
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT OR IGNORE INTO llm_station_merges(uid_a, uid_b, same, "
        "confidence, reason, id, status, model, name_a, name_b, dist_m) "
        "VALUES(?1, ?2, ?3, ?4, ?5, lower(hex(randomblob(8))), 'pending', "
        "?6, ?7, ?8, ?9)", -1, &s, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(s, 1, ua, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, ub, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 3, same ? 1 : 0);
  sqlite3_bind_double(s, 4, confidence);
  bind_opt(s, 5, reason);
  bind_opt(s, 6, model);
  bind_opt(s, 7, na);
  bind_opt(s, 8, nb);
  if (isfinite(p->dist_m) && p->dist_m >= 0) sqlite3_bind_double(s, 9, p->dist_m);
  else sqlite3_bind_null(s, 9);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) return -1;
  return sqlite3_changes(db->h) > 0 ? 1 : 0;
}

static void add_opt(cJSON *o, const char *k, const char *v) {
  if (v && *v) cJSON_AddStringToObject(o, k, v);
}

/* The prompt. The station records are third-party text (OSM names,
 * operators, lines), so they travel as ONE JSON document — cJSON escapes a
 * newline, so a name cannot write a line of the prompt — inside the standing
 * untrusted fence (prompts.h). Caller frees. */
static char *build_prompt(const station_merge_pair *p) {
  cJSON *d = cJSON_CreateObject();
  cJSON *a = cJSON_AddObjectToObject(d, "station_a");
  add_opt(a, "name", p->name_a);   add_opt(a, "name_ja", p->name_ja_a);
  add_opt(a, "mode", p->mode_a);   add_opt(a, "operator", p->operator_a);
  add_opt(a, "line", p->line_a);
  cJSON *b = cJSON_AddObjectToObject(d, "station_b");
  add_opt(b, "name", p->name_b);   add_opt(b, "name_ja", p->name_ja_b);
  add_opt(b, "mode", p->mode_b);   add_opt(b, "operator", p->operator_b);
  add_opt(b, "line", p->line_b);
  cJSON_AddNumberToObject(d, "distance_m", round(p->dist_m));
  char *js = cJSON_PrintUnformatted(d);
  cJSON_Delete(d);
  if (!js) return NULL;
  char *fenced = prompt_fence_untrusted("STATIONS", js);
  free(js);
  if (!fenced) return NULL;
  static const char *HEAD =
    "You decide whether two Japanese railway or subway station records name "
    "the SAME station: one station, or one interchange complex served under "
    "one name, as opposed to two different stations that happen to be close "
    "together. Account for kanji/kana/romaji spellings, operator prefixes and "
    "suffixes such as 駅, 前 or 口. Output JSON only.\n\n";
  static const char *TAIL =
    "\nSame station? Respond with JSON only: "
    "{\"same\": true|false, \"confidence\": 0.0-1.0, \"reason\": \"...\"}";
  size_t n = strlen(HEAD) + strlen(fenced) + strlen(TAIL) + 1;
  char *out = malloc(n);
  if (out) snprintf(out, n, "%s%s%s", HEAD, fenced, TAIL);
  free(fenced);
  return out;
}

/* The first {...} in `raw` as a verdict. 0 with the fields filled, -1 when
 * the answer is not one (missing/mistyped field, confidence outside [0,1]).
 * *reason is malloc'd or NULL. */
static int parse_verdict(const char *raw, int *same, double *conf, char **reason) {
  *reason = NULL;
  if (!raw) return -1;
  const char *a = strchr(raw, '{'), *b = strrchr(raw, '}');
  if (!a || !b || b < a) return -1;
  cJSON *j = cJSON_ParseWithLength(a, (size_t)(b - a + 1));
  if (!j) return -1;
  const cJSON *sm = cJSON_GetObjectItem(j, "same");
  const cJSON *cf = cJSON_GetObjectItem(j, "confidence");
  const cJSON *rs = cJSON_GetObjectItem(j, "reason");
  int ok = cJSON_IsBool(sm) && cJSON_IsNumber(cf) && isfinite(cf->valuedouble) &&
           cf->valuedouble >= 0 && cf->valuedouble <= 1;
  if (ok) {
    *same = cJSON_IsTrue(sm);
    *conf = cf->valuedouble;
    if (cJSON_IsString(rs) && rs->valuestring[0]) *reason = strdup(rs->valuestring);
  }
  cJSON_Delete(j);
  return ok ? 0 : -1;
}

int station_merges_judge(db_handle *db, const station_merge_pair *pairs, int n,
                         int *asked, int *unparsed) {
  if (asked) *asked = 0;
  if (unparsed) *unparsed = 0;
  if (!db || !db->h || !station_merges_judge_enabled()) return -1;
  station_merges_migrate(db);
  int budget = station_merges_judge_budget();
  if (!pairs || n <= 0) return 0;
  /* Nothing undecided, nothing to ask: no request at all, not even the model
   * identity — a clusterer run whose near misses are all recorded costs the
   * generation server nothing. */
  {
    int open_q = 0;
    for (int i = 0; i < n && !open_q; i++)
      if (pairs[i].uid_a && pairs[i].uid_b && strcmp(pairs[i].uid_a, pairs[i].uid_b) &&
          !station_merges_known(db, pairs[i].uid_a, pairs[i].uid_b)) open_q = 1;
    if (!open_q) return 0;
  }

  http_client *http = http_client_new();
  if (!http) return -1;
  llm_client llm; llm_init(&llm, http);       /* background lane */
  /* The model identity, once per batch. A verdict whose model is unknown
   * cannot be weighed by its reviewer, so no identity = no batch (the
   * embedding pod's rule for a failed detection). */
  char model[256];
  if (vecindex_served_model(http, llm.base_url, model, sizeof model) != 0) {
    fprintf(stderr, "[station-judge] skipped: model identity unknown (%s)\n", model);
    http_client_free(http);
    return -1;
  }
  int timeout = 30000;
  { const char *e = getenv("LLM_TIMEOUT_MS");
    if (e && atoi(e) > 0) timeout = atoi(e); }

  int nasked = 0, nrec = 0, nbad = 0, deferred = 0;
  for (int i = 0; i < n && nasked < budget; i++) {
    const station_merge_pair *p = &pairs[i];
    if (!p->uid_a || !p->uid_b || !strcmp(p->uid_a, p->uid_b)) continue;
    if (station_merges_known(db, p->uid_a, p->uid_b)) continue;
    char *pr = build_prompt(p);
    if (!pr) break;
    long t0 = llm_fault_now_ms();
    char *raw = llm_complete(&llm, pr, NULL, 256, 0.2, timeout);
    long el = llm_fault_now_ms() - t0;
    free(pr);
    /* An outage says nothing about the pair: stop, and leave it and the rest
     * unasked for the next run. */
    if (!raw && llm_fault_is_transport(&llm, el, timeout)) {
      deferred = 1;
      break;
    }
    nasked++;
    int same = 0; double conf = 0; char *reason = NULL;
    if (parse_verdict(raw, &same, &conf, &reason) == 0) {
      if (station_merges_record(db, p, same, conf, reason, model) == 1) nrec++;
    } else {
      nbad++;            /* counted, not stored: there is no verdict to keep */
    }
    free(reason);
    free(raw);
  }
  http_client_free(http);
  fprintf(stderr, "[station-judge] model=%s asked=%d recorded=%d unparsed=%d%s\n",
          model, nasked, nrec, nbad,
          deferred ? " (stopped: LLM unreachable or timed out)" : "");
  if (asked) *asked = nasked;
  if (unparsed) *unparsed = nbad;
  return nrec;
}

/* ---- review API --------------------------------------------------------- */

static char *err_body(const char *error, const char *detail) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", error);
  if (detail) cJSON_AddStringToObject(o, "detail", detail);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

static const char *COLS =
  "id, uid_a, uid_b, same, confidence, reason, decided_at, status, model, "
  "reviewed_by, reviewed_at, name_a, name_b, dist_m";

static void add_text_or_null(cJSON *o, const char *k, sqlite3_stmt *s, int c) {
  const char *v = (const char *)sqlite3_column_text(s, c);
  if (v) cJSON_AddStringToObject(o, k, v);
  else   cJSON_AddNullToObject(o, k);
}

static cJSON *row_json(sqlite3_stmt *s) {
  cJSON *o = cJSON_CreateObject();
  add_text_or_null(o, "id", s, 0);
  add_text_or_null(o, "uid_a", s, 1);
  add_text_or_null(o, "uid_b", s, 2);
  cJSON_AddBoolToObject(o, "same", sqlite3_column_int(s, 3));
  cJSON_AddNumberToObject(o, "confidence", sqlite3_column_double(s, 4));
  add_text_or_null(o, "reason", s, 5);
  add_text_or_null(o, "decided_at", s, 6);
  add_text_or_null(o, "status", s, 7);
  add_text_or_null(o, "model", s, 8);
  add_text_or_null(o, "reviewed_by", s, 9);
  add_text_or_null(o, "reviewed_at", s, 10);
  add_text_or_null(o, "name_a", s, 11);
  add_text_or_null(o, "name_b", s, 12);
  if (sqlite3_column_type(s, 13) == SQLITE_NULL) cJSON_AddNullToObject(o, "dist_m");
  else cJSON_AddNumberToObject(o, "dist_m", sqlite3_column_double(s, 13));
  /* What the reader will do with this row on the clusterer's next run. */
  const char *st = (const char *)sqlite3_column_text(s, 7);
  cJSON_AddBoolToObject(o, "applied",
    sqlite3_column_int(s, 3) == 1 && st && !strcmp(st, "approved"));
  return o;
}

static long count_status(db_handle *db, const char *status) {
  sqlite3_stmt *s; long v = -1;
  const char *q = status
    ? "SELECT count(*) FROM llm_station_merges WHERE status=?1"
    : "SELECT count(*) FROM llm_station_merges";
  if (sqlite3_prepare_v2(db->h, q, -1, &s, NULL) != SQLITE_OK) return -1;
  if (status) sqlite3_bind_text(s, 1, status, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}

static int status_ok(const char *s) {
  return !strcmp(s, "pending") || !strcmp(s, "approved") ||
         !strcmp(s, "rejected") || !strcmp(s, "all");
}

char *station_merges_api_list(db_handle *db, const char *status, int limit,
                              int offset, int *http_status) {
  if (!status || !*status) status = "pending";
  if (!status_ok(status)) {
    *http_status = 400;
    return err_body("bad_request", "status must be pending, approved, rejected or all");
  }
  if (limit <= 0) limit = 100;
  if (limit > SM_LIST_MAX) limit = SM_LIST_MAX;
  if (offset < 0) offset = 0;
  station_merges_migrate(db);
  int all = !strcmp(status, "all");
  char sql[512];
  snprintf(sql, sizeof sql,
           "SELECT %s FROM llm_station_merges %s "
           "ORDER BY decided_at DESC, id LIMIT ?2 OFFSET ?3",
           COLS, all ? "WHERE ?1 IS NOT NULL" : "WHERE status=?1");
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    *http_status = 500;
    return err_body("server_error", NULL);
  }
  sqlite3_bind_text(s, 1, status, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, limit);
  sqlite3_bind_int(s, 3, offset);
  cJSON *root = cJSON_CreateObject();
  cJSON *arr = cJSON_AddArrayToObject(root, "data");
  int cnt = 0;
  while (sqlite3_step(s) == SQLITE_ROW) { cJSON_AddItemToArray(arr, row_json(s)); cnt++; }
  sqlite3_finalize(s);
  long total = count_status(db, all ? NULL : status);
  /* The page names how much exists, so a bounded list is never silent. */
  cJSON *pg = cJSON_AddObjectToObject(root, "page");
  cJSON_AddNumberToObject(pg, "limit", limit);
  cJSON_AddNumberToObject(pg, "offset", offset);
  cJSON_AddNumberToObject(pg, "count", cnt);
  cJSON_AddNumberToObject(pg, "total", (double)total);
  cJSON_AddBoolToObject(pg, "has_more", total >= 0 && offset + cnt < total);
  cJSON *m = cJSON_AddObjectToObject(root, "meta");
  cJSON_AddStringToObject(m, "status", status);
  cJSON *c = cJSON_AddObjectToObject(m, "counts");
  cJSON_AddNumberToObject(c, "pending", (double)count_status(db, "pending"));
  cJSON_AddNumberToObject(c, "approved", (double)count_status(db, "approved"));
  cJSON_AddNumberToObject(c, "rejected", (double)count_status(db, "rejected"));
  cJSON_AddBoolToObject(m, "judge_enabled", station_merges_judge_enabled());
  cJSON_AddNumberToObject(m, "judge_budget_per_run", station_merges_judge_budget());
  cJSON_AddStringToObject(m, "applies",
    "same=1 AND status=approved, on the station clusterer's next run");
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  *http_status = 200;
  return out;
}

char *station_merges_api_review(db_handle *db, const char *id, int approve,
                                const char *body, size_t body_len,
                                const char *user_id, int *http_status) {
  if (!id || !*id || strlen(id) > 64) {
    *http_status = 400;
    return err_body("bad_request", "invalid id");
  }
  char note[SM_NOTE_MAX + 1] = "";
  if (body && body_len) {
    cJSON *jb = cJSON_ParseWithLength(body, body_len);
    if (!cJSON_IsObject(jb)) {
      cJSON_Delete(jb);
      *http_status = 400;
      return err_body("bad_request", "body must be empty or {\"note\": string}");
    }
    const cJSON *jn = cJSON_GetObjectItem(jb, "note");
    if (jn && !cJSON_IsString(jn) && !cJSON_IsNull(jn)) {
      cJSON_Delete(jb);
      *http_status = 400;
      return err_body("bad_request", "note must be a string");
    }
    if (cJSON_IsString(jn) && strlen(jn->valuestring) > SM_NOTE_MAX) {
      cJSON_Delete(jb);
      *http_status = 400;
      return err_body("bad_request", "note is longer than 500 bytes");
    }
    if (cJSON_IsString(jn)) snprintf(note, sizeof note, "%s", jn->valuestring);
    cJSON_Delete(jb);
  }
  station_merges_migrate(db);
  const char *to = approve ? "approved" : "rejected";
  char from[16] = "";
  char *ua = NULL, *ub = NULL;
  int same = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT status, uid_a, uid_b, same FROM llm_station_merges WHERE id=?1",
        -1, &s, NULL) != SQLITE_OK) {
    *http_status = 500;
    return err_body("server_error", NULL);
  }
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  int found = sqlite3_step(s) == SQLITE_ROW;
  if (found) {
    const char *f = (const char *)sqlite3_column_text(s, 0);
    const char *a = (const char *)sqlite3_column_text(s, 1);
    const char *b = (const char *)sqlite3_column_text(s, 2);
    snprintf(from, sizeof from, "%s", f ? f : "");
    ua = strdup(a ? a : "");
    ub = strdup(b ? b : "");
    same = sqlite3_column_int(s, 3);
  }
  sqlite3_finalize(s);
  if (!found) { *http_status = 404; return err_body("not_found", id); }
  if (!ua || !ub) { free(ua); free(ub); *http_status = 500; return err_body("server_error", NULL); }
  /* A decision that is already in force is not a change: refusing it keeps
   * the audit chain to real transitions. The other way round is allowed, so
   * an approval made in error can be reversed. */
  if (!strcmp(from, to)) {
    free(ua); free(ub);
    *http_status = 409;
    char det[64]; snprintf(det, sizeof det, "already %s", to);
    return err_body("no_change", det);
  }
  if (sqlite3_prepare_v2(db->h,
        "UPDATE llm_station_merges SET status=?2, reviewed_by=?3, "
        "reviewed_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?1",
        -1, &s, NULL) != SQLITE_OK) {
    free(ua); free(ub);
    *http_status = 500;
    return err_body("server_error", NULL);
  }
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, to, -1, SQLITE_STATIC);
  bind_opt(s, 3, user_id);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) {
    free(ua); free(ub);
    *http_status = 500; return err_body("server_error", NULL);
  }

  cJSON *ap = cJSON_CreateObject();
  cJSON_AddStringToObject(ap, "id", id);
  cJSON_AddStringToObject(ap, "uid_a", ua);
  cJSON_AddStringToObject(ap, "uid_b", ub);
  cJSON_AddBoolToObject(ap, "same", same);
  cJSON_AddStringToObject(ap, "old_status", from);
  cJSON_AddStringToObject(ap, "new_status", to);
  if (note[0]) cJSON_AddStringToObject(ap, "note", note);
  free(ua); free(ub);
  char *pj = cJSON_PrintUnformatted(ap);
  cJSON_Delete(ap);
  audit_write(db, "platform", user_id,
              approve ? "station_merge.approve" : "station_merge.reject",
              id, pj ? pj : "{}");
  free(pj);

  char sql[256];
  snprintf(sql, sizeof sql, "SELECT %s FROM llm_station_merges WHERE id=?1", COLS);
  cJSON *root = cJSON_CreateObject();
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) cJSON_AddItemToObject(root, "data", row_json(s));
    sqlite3_finalize(s);
  }
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  *http_status = 200;
  return out;
}
