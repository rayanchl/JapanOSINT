/* core/searchapi.c — see header. Port of routes/search.js (analyze/suggest/
 * results). The SSE /stream lives in httpd.c (it owns the connection loop). */
#include "searchapi.h"
#include "savedsearchapi.h"
#include "pipeline.h"
#include "progress.h"
#include "llm.h"
#include "httpclient.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include <openssl/rand.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void gen_id(char *out33) {
  unsigned char b[16];
  if (RAND_bytes(b, 16) != 1) { for (int i = 0; i < 16; i++) b[i] = (unsigned char)rand(); }
  for (int i = 0; i < 16; i++) sprintf(out33 + i * 2, "%02x", b[i]);
  out33[32] = 0;
}

typedef struct {
  db_handle *db;
  char id[40];
  char *query;
  int max_rounds;
} run_arg;

/* The single-slot llama-server is the only real bottleneck, so serialization
 * lives in the llm_client (it locks just around the generation HTTP call) —
 * NOT around the whole pipeline. Gating the entire run was wrong: the slow
 * collector phase (e.g. an unresponsive Overpass fetch with a 60s-per-endpoint
 * budget) would hold the lock for minutes, so a queued search never reached
 * its first llm_complete and sat in phase "queued" with the laptop idle.
 * Now pipelines run concurrently; only their LLM calls queue, so a wedged
 * collector in one run no longer stalls another run's analysis. Each thread
 * keeps its own http_client (== scheduler) so a slow collector's connection
 * pool stays isolated to that run. */
/* Runs are detached threads, each holding a collector connection pool and a
 * slot in the LLM queue, so an unbounded fan-out of them is how the box falls
 * over: N pipelines all fetching while all but one block on the single-slot
 * llama-server. Cap the number in flight and tell the client to retry (429)
 * rather than accepting work we cannot start. */
static pthread_mutex_t g_run_lock = PTHREAD_MUTEX_INITIALIZER;
static int             g_runs_live = 0;

static int max_concurrent_runs(void) {
  const char *e = getenv("JO_MAX_CONCURRENT_SEARCHES");
  if (e && *e) {
    int n = atoi(e);
    if (n > 0) return n;
  }
  return 4;
}

/* Returns 1 if a slot was taken. */
static int run_slot_acquire(void) {
  int ok = 0;
  pthread_mutex_lock(&g_run_lock);
  if (g_runs_live < max_concurrent_runs()) { g_runs_live++; ok = 1; }
  pthread_mutex_unlock(&g_run_lock);
  return ok;
}

static void run_slot_release(void) {
  pthread_mutex_lock(&g_run_lock);
  if (g_runs_live > 0) g_runs_live--;
  pthread_mutex_unlock(&g_run_lock);
}

static void *run_thread(void *vp) {
  run_arg *a = vp;
  http_client *http = http_client_new();   /* own client (== scheduler) */
  llm_client llm; llm_init(&llm, http);
  llm.interactive = 1;   /* live user search → high-priority LLM lane */
  osint_pipeline_run(a->db, &llm, a->id, a->query, a->max_rounds);
  http_client_free(http);
  free(a->query);
  free(a);
  run_slot_release();
  return NULL;
}

/* Persist the run's owner so the restart path (searchapi_results reading the
 * stored run row) can still answer "whose run is this", and so the workspace's
 * run list (searchapi_runs) can name the query and its author for a run that
 * never reached its summary row — in flight, failed, or lost to a restart.
 * 0 on success. */
static int owner_persist(db_handle *db, const char *id, const char *tenant_id,
                         const char *user_id, const char *query) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT OR REPLACE INTO search_run_owners(request_id,tenant_id,user_id,"
        "query) VALUES (?1,?2,?3,?4)", -1, &s, NULL) != SQLITE_OK) {
    fprintf(stderr, "[search] cannot record the owner of run %s: %s\n", id,
            sqlite3_errmsg(db->h));
    return -1;
  }
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant_id, -1, SQLITE_TRANSIENT);
  if (user_id && *user_id) sqlite3_bind_text(s, 3, user_id, -1, SQLITE_TRANSIENT);
  else sqlite3_bind_null(s, 3);
  sqlite3_bind_text(s, 4, query ? query : "", -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  return rc == SQLITE_DONE ? 0 : -1;
}

/* 1 when `tenant_id` started the stored run `id`. A run with no owner row
 * (started before ownership was recorded) belongs to nobody we can name, and
 * is refused: "unknown owner" must not read as "every tenant". */
static int owner_matches(db_handle *db, const char *id, const char *tenant_id) {
  if (!tenant_id || !*tenant_id) return 0;
  sqlite3_stmt *s;
  int ok = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM search_run_owners WHERE request_id=?1 AND tenant_id=?2",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, tenant_id, -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(s) == SQLITE_ROW;
    sqlite3_finalize(s);
  }
  return ok;
}

char *searchapi_analyze(db_handle *db, const char *tenant_id,
                        const char *user_id, const char *query, int max_rounds,
                        int *out_status) {
  if (out_status) *out_status = 400;
  if (!query) return NULL;
  /* No owner, no run: an ownerless run would be readable by every tenant. */
  if (!tenant_id || !*tenant_id) { if (out_status) *out_status = 500; return NULL; }
  while (*query == ' ' || *query == '\t' || *query == '\n' || *query == '\r') query++;
  if (!*query) return NULL;

  /* max_rounds was guarded only by "> 0", so {"max_rounds":2147483647} was
   * accepted verbatim: pipeline.c's phase-2 loop (`while (need_more && round <
   * max_rounds)`) would keep spawning entity-pivot rounds, each one a full
   * collector fan-out plus an LLM call, holding one of the four concurrent-run
   * slots for as long as the caller liked. One request, unbounded work.
   *
   * Clamped here rather than in pipeline.c so that the number the client is
   * TOLD (progress.c stamps max_rounds into the /api/search/results payload)
   * is the number that will actually run — clamping deeper would have left the
   * progress record advertising 2147483647 rounds it was never going to do.
   * Ceiling and floor are applied the way every other numeric parameter in
   * this tree is clamped (intelapi's limit 1..200, timelineapi's 1..500):
   * silently to the bound, not as a 400. */
  if (max_rounds > SEARCH_MAX_ROUNDS_CEILING) max_rounds = SEARCH_MAX_ROUNDS_CEILING;
  if (max_rounds < 0) max_rounds = 0;   /* 0 still means "use the default 5" */

  if (!run_slot_acquire()) {
    if (out_status) *out_status = 429;
    return NULL;
  }

  char id[40], skey[40];
  gen_id(id);
  gen_id(skey);                 /* the SSE capability — never the request_id */
  if (owner_persist(db, id, tenant_id, user_id, query) != 0) {
    run_slot_release();
    if (out_status) *out_status = 500;
    return NULL;
  }
  if (!progress_create_owned(id, query, max_rounds > 0 ? max_rounds : 5,
                             tenant_id, skey)) {
    run_slot_release();
    return NULL;
  }
  /* Starting a run is always a committed search, so it goes into the
   * workspace's search history here, for every client (decided 2026-10-09).
   * The result count is not known yet: the run's own row carries it. */
  {
    cJSON *hp = cJSON_CreateObject();
    if (hp && cJSON_AddStringToObject(hp, "q", query)) {
      char *pj = cJSON_PrintUnformatted(hp);
      if (pj) search_history_record(db, tenant_id, user_id, "osint", pj, -1);
      free(pj);
    }
    cJSON_Delete(hp);
  }

  run_arg *a = calloc(1, sizeof *a);
  /* Unchecked calloc used to dereference NULL on the next line. This runs on
   * every POST /api/search/analyze — a request an unauthenticated rate limit
   * still lets through in bulk — so an allocation failure here must fail the
   * request, not crash the server for every other in-flight investigation. */
  if (!a) {
    run_slot_release();
    return NULL;
  }
  a->db = db;
  snprintf(a->id, sizeof a->id, "%s", id);
  a->query = strdup(query);
  a->max_rounds = max_rounds;
  pthread_t t;
  if (pthread_create(&t, NULL, run_thread, a) == 0) {
    pthread_detach(t);
  } else {
    free(a->query); free(a);
    run_slot_release();
    return NULL;
  }
  if (out_status) *out_status = 200;

  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "request_id", id);
  /* Open /api/search/stream/:request_id?key=<stream_key>. The stream is
   * pre-auth (EventSource sends no headers), so this — not the request_id,
   * which share links carry — is what proves the reader is the owner. */
  cJSON_AddStringToObject(o, "stream_key", skey);
  cJSON_AddStringToObject(o, "status", "processing");
  cJSON_AddStringToObject(o, "query", query);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

char *searchapi_suggest(const char *q) {
  cJSON *o = cJSON_CreateObject();
  if (!q || !*q) {
    cJSON_AddItemToObject(o, "suggestions", cJSON_CreateArray());
  } else {
    http_client *http = http_client_new();
    llm_client llm; llm_init_suggest(&llm, http);  /* dedicated suggest LLM */
    char *arr = osint_suggest(&llm, q);          /* JSON array string */
    http_client_free(http);
    cJSON *a = arr ? cJSON_Parse(arr) : NULL;
    free(arr);
    cJSON_AddItemToObject(o, "suggestions",
                          (a && cJSON_IsArray(a)) ? a : cJSON_CreateArray());
    if (a && !cJSON_IsArray(a)) cJSON_Delete(a);
  }
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s ? s : strdup("{\"suggestions\":[]}");
}

/* `{"stream_key":"<k>",` spliced in front of a snapshot object. The key is
 * 32 hex characters, so no escaping is involved; re-parsing a multi-megabyte
 * snapshot to add one field would be the expensive way to do the same. */
static char *with_stream_key(char *snap, const char *key) {
  if (!snap || !key || !*key || snap[0] != '{') return snap;
  size_t sl = strlen(snap), kl = strlen(key);
  char *o = malloc(sl + kl + 20);
  if (!o) return snap;
  int n = sprintf(o, "{\"stream_key\":\"%s\"%s", key, snap[1] == '}' ? "" : ",");
  memcpy(o + n, snap + 1, sl);              /* includes the NUL */
  free(snap);
  return o;
}

char *searchapi_results(db_handle *db, const char *tenant_id, const char *id) {
  if (!id || !*id) return NULL;
  /* Find + check ownership + serialise under one lock: progress_create()
   * frees the oldest FINISHED request past 200, and a pointer taken from
   * progress_get() and used after the unlock can be that entry.
   *
   * TENANT-SCOPED. This answered any authenticated caller holding the
   * request_id — and the request_id is exactly what a share link carries, so
   * a link forwarded outside the workspace was a cross-tenant read, while the
   * share sheet tells the user the server re-checks the recipient's
   * workspace. A run belongs to the tenant that started it; anyone else gets
   * the same 404 as an unknown id. */
  char key[48] = {0};
  char *live = progress_snapshot_for(id, tenant_id, NULL, NULL, key, sizeof key);
  if (live) return with_stream_key(live, key);

  /* Server restarted: reconstruct from the persisted run row (== JS else) —
   * for its owner only. */
  if (!owner_matches(db, id, tenant_id)) return NULL;

  /* Server restarted: reconstruct from the persisted run row (== JS else). */
  char uid[128];
  snprintf(uid, sizeof uid, "osint-search|run:%s", id);
  sqlite3_stmt *st = NULL;
  char *out = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT title,summary,properties FROM intel_items WHERE uid=?1",
        -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) {
      const char *title = (const char *)sqlite3_column_text(st, 0);
      const char *summary = (const char *)sqlite3_column_text(st, 1);
      const char *pjs = (const char *)sqlite3_column_text(st, 2);
      cJSON *p = pjs ? cJSON_Parse(pjs) : NULL;
      cJSON *o = cJSON_CreateObject();
      cJSON_AddStringToObject(o, "request_id", id);
      cJSON *pq = p ? cJSON_GetObjectItem(p, "query") : NULL;
      cJSON_AddStringToObject(o, "query",
        (pq && cJSON_IsString(pq)) ? pq->valuestring : (title ? title : ""));
      cJSON *pp = p ? cJSON_GetObjectItem(p, "phase") : NULL;
      cJSON_AddStringToObject(o, "phase",
        (pp && cJSON_IsString(pp)) ? pp->valuestring : "completed");
      cJSON_AddNumberToObject(o, "progress_percent", 100);
      cJSON *pe = p ? cJSON_GetObjectItem(p, "entities") : NULL;
      cJSON_AddItemToObject(o, "entities",
        pe ? cJSON_Duplicate(pe, 1) : cJSON_CreateArray());
      cJSON *ps = p ? cJSON_GetObjectItem(p, "stats") : NULL;
      cJSON_AddItemToObject(o, "stats",
        ps ? cJSON_Duplicate(ps, 1) : cJSON_CreateObject());
      /* Carry the degradation verdict across the restart. The live progress
       * record is gone by definition on this path, so if the flag were not
       * persisted with the run row (pipeline.c writes it into `properties`) a
       * reload of a run whose analysis stage never happened would come back
       * looking like a clean completed investigation — the same silent success
       * one restart later. Absent from an OLD row => absent here, rather than
       * a fabricated `false`. */
      cJSON *pd = p ? cJSON_GetObjectItem(p, "degraded") : NULL;
      cJSON *pse = p ? cJSON_GetObjectItem(p, "stage_errors") : NULL;
      cJSON *r = cJSON_CreateObject();
      cJSON_AddStringToObject(r, "synthesis", summary ? summary : "");
      if (pd) {
        cJSON_AddBoolToObject(o, "degraded", cJSON_IsTrue(pd));
        cJSON_AddBoolToObject(r, "degraded", cJSON_IsTrue(pd));
      }
      if (pse) {
        cJSON_AddItemToObject(o, "stage_errors", cJSON_Duplicate(pse, 1));
        cJSON_AddItemToObject(r, "stage_errors", cJSON_Duplicate(pse, 1));
      }
      cJSON_AddItemToObject(o, "results", r);
      cJSON_AddBoolToObject(o, "from_store", 1);
      out = cJSON_PrintUnformatted(o);
      cJSON_Delete(o);
      if (p) cJSON_Delete(p);
    }
  }
  sqlite3_finalize(st);
  return out;   /* NULL → caller 404 not_found */
}

/* ── GET /api/search/runs — the workspace's investigations ────────────────
 *
 * A run was always readable by every member of the workspace that started it
 * (searchapi_results is tenant-scoped), but nothing LISTED runs: the web
 * client kept the runs it started in this tab's memory, so a teammate's
 * investigation — the query, the LLM synthesis, the entities — was visible
 * only to someone handed its id. Decided 2026-10-05: everything in a
 * workspace is visible to its members. This lists search_run_owners for the
 * tenant (every member's runs; ?mine=1 narrows to the caller), newest first,
 * with each row's author, and the live phase when the run is still in memory.
 *
 * The synthesis is shown as a preview with its full length beside it; the
 * whole of it is GET /api/search/results/:id. Runs carry no edit or delete,
 * so there is no author-only half here. */

/* Byte length of the longest prefix of `s` that is at most `max` bytes and
 * does not split a UTF-8 sequence. */
static size_t utf8_prefix(const char *s, size_t max) {
  size_t n = strlen(s);
  if (n <= max) return n;
  size_t k = max;
  while (k > 0 && ((unsigned char) s[k] & 0xC0) == 0x80) k--;
  return k;
}

#define RUNS_PREVIEW_BYTES 280

char *searchapi_runs(db_handle *db, const char *tenant_id, const char *user_id,
                     int limit, int mine_only, const char *cursor,
                     int *out_status) {
  if (out_status) *out_status = 500;
  if (!db || !db->h || !tenant_id || !*tenant_id) return NULL;
  if (limit <= 0) limit = 50;
  if (limit > 200) limit = 200;

  /* cursor = "<created_at>|<request_id>" of the last row of the previous
   * page. Opaque to clients; a malformed one is ignored and disclosed. */
  char cur_at[40] = {0}, cur_id[64] = {0};
  int cursor_given = cursor && *cursor, cursor_used = 0;
  if (cursor_given) {
    const char *bar = strchr(cursor, '|');
    if (bar && (size_t) (bar - cursor) < sizeof cur_at && strlen(bar + 1) < sizeof cur_id &&
        bar > cursor && bar[1]) {
      memcpy(cur_at, cursor, (size_t) (bar - cursor));
      snprintf(cur_id, sizeof cur_id, "%s", bar + 1);
      cursor_used = 1;
    }
  }
  const char *who = (mine_only && user_id && *user_id) ? user_id : NULL;

  sqlite3_stmt *s;
  const char *sql = cursor_used
    ? "SELECT o.request_id,o.user_id,o.created_at,o.query,i.title,i.summary,"
      "i.properties FROM search_run_owners o "
      "LEFT JOIN intel_items i ON i.uid='osint-search|run:'||o.request_id "
      "WHERE o.tenant_id=?1 AND (?2 IS NULL OR o.user_id=?2) "
      "AND (o.created_at<?4 OR (o.created_at=?4 AND o.request_id<?5)) "
      "ORDER BY o.created_at DESC, o.request_id DESC LIMIT ?3"
    : "SELECT o.request_id,o.user_id,o.created_at,o.query,i.title,i.summary,"
      "i.properties FROM search_run_owners o "
      "LEFT JOIN intel_items i ON i.uid='osint-search|run:'||o.request_id "
      "WHERE o.tenant_id=?1 AND (?2 IS NULL OR o.user_id=?2) "
      "ORDER BY o.created_at DESC, o.request_id DESC LIMIT ?3";
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, tenant_id, -1, SQLITE_TRANSIENT);
  if (who) sqlite3_bind_text(s, 2, who, -1, SQLITE_TRANSIENT);
  else     sqlite3_bind_null(s, 2);
  sqlite3_bind_int(s, 3, limit);
  if (cursor_used) {
    sqlite3_bind_text(s, 4, cur_at, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 5, cur_id, -1, SQLITE_TRANSIENT);
  }

  cJSON *arr = cJSON_CreateArray();
  int n = 0;
  char last_at[40] = {0}, last_id[64] = {0};
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *rid = (const char *) sqlite3_column_text(s, 0);
    if (!rid) continue;
    const char *author = (const char *) sqlite3_column_text(s, 1);
    const char *at     = (const char *) sqlite3_column_text(s, 2);
    const char *q      = (const char *) sqlite3_column_text(s, 3);
    const char *title  = (const char *) sqlite3_column_text(s, 4);
    const char *synth  = (const char *) sqlite3_column_text(s, 5);
    const char *pjs    = (const char *) sqlite3_column_text(s, 6);
    cJSON *props = pjs ? cJSON_Parse(pjs) : NULL;

    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "request_id", rid);
    /* The query: the owner row (every run since it was recorded there), else
     * the stored run row (older runs), else unknown — never invented. */
    const char *qq = (q && *q) ? q : NULL;
    if (!qq && props) {
      cJSON *pq = cJSON_GetObjectItem(props, "query");
      if (cJSON_IsString(pq) && pq->valuestring[0]) qq = pq->valuestring;
    }
    if (!qq && title && !strncmp(title, "OSINT search: ", 14)) qq = title + 14;
    if (qq) cJSON_AddStringToObject(r, "query", qq);
    else    cJSON_AddNullToObject(r, "query");
    if (author) cJSON_AddStringToObject(r, "user_id", author);
    else        cJSON_AddNullToObject(r, "user_id");
    cJSON_AddBoolToObject(r, "mine", author && user_id && !strcmp(author, user_id));
    if (at) cJSON_AddStringToObject(r, "created_at", at);
    else    cJSON_AddNullToObject(r, "created_at");

    char phase[48] = {0};
    int pct = 0, done = 0, degr = 0;
    if (progress_brief_for(rid, tenant_id, phase, sizeof phase, &pct, &done, &degr)) {
      cJSON_AddStringToObject(r, "status", done
        ? (!strcmp(phase, "error") ? "error" : "completed") : "running");
      cJSON_AddStringToObject(r, "phase", phase);
      cJSON_AddNumberToObject(r, "progress_percent", pct);
      cJSON_AddBoolToObject(r, "degraded", degr);
    } else if (title || synth || props) {
      cJSON *pp = props ? cJSON_GetObjectItem(props, "phase") : NULL;
      const char *ph = cJSON_IsString(pp) ? pp->valuestring : "completed";
      cJSON_AddStringToObject(r, "status", !strcmp(ph, "error") ? "error" : "completed");
      cJSON_AddStringToObject(r, "phase", ph);
      cJSON *pd = props ? cJSON_GetObjectItem(props, "degraded") : NULL;
      if (pd) cJSON_AddBoolToObject(r, "degraded", cJSON_IsTrue(pd));
    } else {
      /* Neither live nor stored: started before a restart and never
       * finished, or failed before its summary row. Said, not guessed. */
      cJSON_AddStringToObject(r, "status", "unknown");
      cJSON_AddNullToObject(r, "phase");
    }
    if (synth && *synth) {
      size_t full = strlen(synth), k = utf8_prefix(synth, RUNS_PREVIEW_BYTES);
      char *pv = malloc(k + 1);
      if (pv) { memcpy(pv, synth, k); pv[k] = 0;
                cJSON_AddStringToObject(r, "synthesis_preview", pv); free(pv); }
      cJSON_AddNumberToObject(r, "synthesis_bytes", (double) full);
      cJSON_AddBoolToObject(r, "synthesis_truncated", k < full);
    } else {
      cJSON_AddNullToObject(r, "synthesis_preview");
    }
    if (props) cJSON_Delete(props);
    cJSON_AddItemToArray(arr, r);
    snprintf(last_at, sizeof last_at, "%s", at ? at : "");
    snprintf(last_id, sizeof last_id, "%s", rid);
    n++;
  }
  sqlite3_finalize(s);

  long total = -1;
  if (sqlite3_prepare_v2(db->h,
        "SELECT COUNT(*) FROM search_run_owners WHERE tenant_id=?1 "
        "AND (?2 IS NULL OR user_id=?2)", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, tenant_id, -1, SQLITE_TRANSIENT);
    if (who) sqlite3_bind_text(s, 2, who, -1, SQLITE_TRANSIENT);
    else     sqlite3_bind_null(s, 2);
    if (sqlite3_step(s) == SQLITE_ROW) total = (long) sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }

  cJSON *w = cJSON_CreateObject();
  cJSON_AddItemToObject(w, "data", arr);
  cJSON *pg = cJSON_CreateObject();
  cJSON_AddNumberToObject(pg, "limit", limit);
  cJSON_AddNumberToObject(pg, "count", n);
  if (total >= 0) cJSON_AddNumberToObject(pg, "total", (double) total);
  else            cJSON_AddNullToObject(pg, "total");
  if (n == limit && last_id[0]) {
    char nc[128];
    snprintf(nc, sizeof nc, "%s|%s", last_at, last_id);
    cJSON_AddStringToObject(pg, "next_cursor", nc);
  } else {
    cJSON_AddNullToObject(pg, "next_cursor");
  }
  cJSON_AddItemToObject(w, "page", pg);
  cJSON *mt = cJSON_CreateObject();
  cJSON_AddStringToObject(mt, "scope", who ? "user" : "workspace");
  cJSON_AddNumberToObject(mt, "synthesis_preview_bytes", RUNS_PREVIEW_BYTES);
  if (cursor_given && !cursor_used) {
    cJSON *notes = cJSON_CreateArray();
    cJSON_AddItemToArray(notes, cJSON_CreateString("cursor_ignored"));
    cJSON_AddItemToObject(mt, "notes", notes);
  }
  cJSON_AddItemToObject(w, "meta", mt);
  char *out = cJSON_PrintUnformatted(w);
  cJSON_Delete(w);
  if (out && out_status) *out_status = 200;
  return out;
}
