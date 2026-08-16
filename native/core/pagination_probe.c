/* core/pagination_probe.c — see pagination_probe.h for why this exists. */
#include "pagination_probe.h"
#include "httpclient.h"
#include "url_override.h"
#include "../source.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

/* ── candidate detection ───────────────────────────────────────────────────
 * A candidate is a registered source whose URL declares a page size and gives
 * us no way to ask for the next page. Everything else is either uncapped or
 * already walkable by lib/pagewalk.c. */

static const char *const SIZE_PARAMS[] = {
  "limit", "rows", "$limit", "resultRecordCount", "page_size", "per_page",
  "pageSize", "$top", "size", "maxRecords", "count", "retmax", "itemsPerPage",
  NULL
};
static const char *const OFF_PARAMS[] = {
  "offset", "$skip", "skip", "resultOffset", "startIndex", "start", "from",
  "page", "pageNumber", "p", NULL
};

/* A candidate continuation parameter. `is_page` distinguishes a 1-based PAGE
 * NUMBER from a record OFFSET: page 2 of a 25-per-page feed is `page=2`, not
 * `page=25`, and getting that backwards reads a miss as "does not paginate". */
typedef struct { const char *name; int is_page; } pw_cand;

/* Families whose continuation parameter is published in a specification. The
 * probe still proves each one — a spec says what the API should honour, not
 * what this deployment of it does. */
typedef struct { const char *needle; pw_cand cands[3]; } fam_t;
static const fam_t FAMILIES[] = {
  { "/api/3/action/",  { {"offset", 0}, {NULL, 0} } },              /* CKAN     */
  { "/FeatureServer/", { {"resultOffset", 0}, {NULL, 0} } },        /* ArcGIS   */
  { "/MapServer/",     { {"resultOffset", 0}, {NULL, 0} } },        /* ArcGIS   */
  { "$top=",           { {"$skip", 0}, {NULL, 0} } },               /* OData    */
  { "wt=json",         { {"start", 0}, {NULL, 0} } },               /* Solr     */
  { "/solr/",          { {"start", 0}, {NULL, 0} } },               /* Solr     */
  { NULL,              { {NULL, 0} } }
};
/* Tried when nothing matched. A wrong name is simply ignored by the server,
 * which this reports as "ignored" and discards — that is the whole design. */
static const pw_cand GENERIC[]      = { {"offset",0}, {"start",0}, {"skip",0}, {"page",1}, {NULL,0} };
static const pw_cand GENERIC_PAGED[] = { {"page",1}, {"offset",0}, {"start",0}, {NULL,0} };

static int find_num_param(const char *url, const char *name, long *val) {
  size_t nl = strlen(name);
  const char *q = strchr(url, '?');
  if (!q) return 0;
  for (const char *p = q; *p; p++) {
    if (*p != '?' && *p != '&') continue;
    const char *k = p + 1;
    if (strncasecmp(k, name, nl) != 0 || k[nl] != '=') continue;
    if (!isdigit((unsigned char)k[nl + 1])) continue;
    if (val) *val = strtol(k + nl + 1, NULL, 10);
    return 1;
  }
  return 0;
}

static const char *size_param_of(const char *url, long *size) {
  for (int i = 0; SIZE_PARAMS[i]; i++)
    if (find_num_param(url, SIZE_PARAMS[i], size)) return SIZE_PARAMS[i];
  return NULL;
}
static int has_off_param(const char *url) {
  for (int i = 0; OFF_PARAMS[i]; i++)
    if (find_num_param(url, OFF_PARAMS[i], NULL)) return 1;
  return 0;
}
static const pw_cand *candidates_for(const char *url, const char *sp) {
  for (int i = 0; FAMILIES[i].needle; i++)
    if (strstr(url, FAMILIES[i].needle)) return FAMILIES[i].cands;
  /* A per-page/page-size cap names a page-NUMBERED API far more often. */
  if (sp && (!strcasecmp(sp, "per_page") || !strcasecmp(sp, "page_size") ||
             !strcasecmp(sp, "pageSize") || !strcasecmp(sp, "itemsPerPage")))
    return GENERIC_PAGED;
  return GENERIC;
}

/* 1 when this source is worth probing. */
static int is_candidate(const source_def *d, long *size, const char **sp) {
  if (!d || !d->url) return 0;
  if (strncmp(d->url, "http://", 7) && strncmp(d->url, "https://", 8)) return 0;
  const char *p = size_param_of(d->url, size);
  if (!p || has_off_param(d->url)) return 0;
  *sp = p;
  return 1;
}

int pagination_probe_candidates(void) {
  const source_def **all = registry_all();
  int n = registry_count(), c = 0;
  long sz; const char *sp;
  for (int i = 0; i < n; i++) if (is_candidate(all[i], &sz, &sp)) c++;
  return c;
}

static char *url_with(const char *url, const char *key, long value) {
  size_t need = strlen(url) + strlen(key) + 32;
  char *out = malloc(need);
  if (!out) return NULL;
  snprintf(out, need, "%s%c%s=%ld", url, strchr(url, '?') ? '&' : '?',
           key, value);
  return out;
}

/* ── record fingerprints ───────────────────────────────────────────────────
 * Comparing raw bodies would be wrong: many APIs stamp a generated-at time or
 * a request id into the envelope, so two identical result pages differ. Compare
 * the RECORDS. */

#define FP_MAX 4096
typedef struct { unsigned long long h[FP_MAX]; int n; } fpset;

static unsigned long long fnv1a(const char *s) {
  unsigned long long h = 1469598103934665603ULL;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    h ^= *p; h *= 1099511628211ULL;
  }
  return h;
}
static int ull_cmp(const void *a, const void *b) {
  unsigned long long x = *(const unsigned long long *)a;
  unsigned long long y = *(const unsigned long long *)b;
  return (x > y) - (x < y);
}

/* The most plausible record array — the same rule lib/jsonlist.c's "*" uses:
 * the longest array of objects. An EMPTY array is a valid answer (it is how
 * "page 2 came back empty, so page 1 was everything" is expressed), so it is
 * returned when nothing better exists rather than treated as unreadable. */
static cJSON *record_array(cJSON *doc, int depth) {
  if (!doc || depth > 6) return NULL;
  if (cJSON_IsArray(doc)) return doc;
  if (!cJSON_IsObject(doc)) return NULL;
  cJSON *best = NULL, *empty = NULL, *v;
  cJSON_ArrayForEach(v, doc) {
    if (cJSON_IsArray(v)) {
      cJSON *first = v->child;
      if (first && (cJSON_IsObject(first) || cJSON_IsArray(first))) {
        if (!best || cJSON_GetArraySize(v) > cJSON_GetArraySize(best)) best = v;
      } else if (!first && !empty) empty = v;
    } else if (cJSON_IsObject(v)) {
      cJSON *sub = record_array(v, depth + 1);
      if (sub && cJSON_GetArraySize(sub) > 0) {
        if (!best || cJSON_GetArraySize(sub) > cJSON_GetArraySize(best)) best = sub;
      } else if (sub && !empty) empty = sub;
    }
  }
  return best ? best : empty;
}

/* -> 1 on a readable shape (fps filled), 0 when the body is not a record list. */
static int fingerprint(const char *body, fpset *out) {
  out->n = 0;
  if (!body) return 0;
  cJSON *doc = cJSON_Parse(body);
  if (!doc) {
    /* Not JSON — treat as line-oriented (CSV), skipping the header. */
    const char *p = body;
    int line = 0;
    while (*p && out->n < FP_MAX) {
      const char *e = strchr(p, '\n');
      size_t len = e ? (size_t)(e - p) : strlen(p);
      if (len && line++) {
        char *tmp = malloc(len + 1);
        if (!tmp) break;
        memcpy(tmp, p, len); tmp[len] = 0;
        out->h[out->n++] = fnv1a(tmp);
        free(tmp);
      }
      if (!e) break;
      p = e + 1;
    }
    return line > 0;
  }
  cJSON *arr = record_array(doc, 0);
  if (!arr) { cJSON_Delete(doc); return 0; }
  cJSON *it;
  cJSON_ArrayForEach(it, arr) {
    if (out->n >= FP_MAX) break;
    char *s = cJSON_PrintUnformatted(it);
    if (!s) continue;
    out->h[out->n++] = fnv1a(s);
    free(s);
  }
  cJSON_Delete(doc);
  qsort(out->h, (size_t)out->n, sizeof out->h[0], ull_cmp);
  return 1;
}

/* Fraction of `b` that also appears in `a`, 0..1. Both are sorted. */
static double overlap(const fpset *a, const fpset *b) {
  if (b->n == 0) return 0.0;
  int i = 0, j = 0, both = 0;
  while (i < a->n && j < b->n) {
    if (a->h[i] == b->h[j]) { both++; i++; j++; }
    else if (a->h[i] < b->h[j]) i++;
    else j++;
  }
  return (double)both / (double)b->n;
}

/* ── job table (same shape as core/breach_jobs.c) ──────────────────────── */

#define PJ_MAX 4
typedef struct {
  int  used;
  char id[24];
  char state[12];              /* queued | running | done | error */
  char msg[192];
  int  total, done, paginates, ignored, complete_already, errored;
  long started, ended;
} pjob;

static pjob g_jobs[PJ_MAX];
static int  g_seq = 0, g_busy = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* A full survey is thousands of paced HTTP requests and runs for the better
 * part of an hour. Without this the thread had no way to be told to stop, and
 * — worse — did not observe process shutdown: db_worker_open() documents that
 * it falls back to returning the SHARED handle, which db_close() would then
 * close underneath a live survey. Checked between candidates and between the
 * two page fetches, so a stop lands within one request rather than one job. */
static volatile sig_atomic_t g_probe_stop = 0;

void pagination_probe_stop_all(void) { g_probe_stop = 1; }

static cJSON *job_json_locked(const pjob *j) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "job_id", j->id);
  cJSON_AddStringToObject(o, "state", j->state);
  if (j->msg[0]) cJSON_AddStringToObject(o, "message", j->msg);
  cJSON_AddNumberToObject(o, "candidates", j->total);
  cJSON_AddNumberToObject(o, "probed", j->done);
  cJSON_AddNumberToObject(o, "paginates", j->paginates);
  cJSON_AddNumberToObject(o, "ignores_offset", j->ignored);
  cJSON_AddNumberToObject(o, "already_complete", j->complete_already);
  cJSON_AddNumberToObject(o, "errors", j->errored);
  cJSON_AddNumberToObject(o, "started_at", (double)j->started);
  if (j->ended) cJSON_AddNumberToObject(o, "ended_at", (double)j->ended);
  return o;
}

char *pagination_probe_status(const char *job_id) {
  cJSON *env = cJSON_CreateObject();
  pthread_mutex_lock(&g_lock);
  if (job_id && *job_id) {
    cJSON *found = NULL;
    for (int i = 0; i < PJ_MAX; i++)
      if (g_jobs[i].used && !strcmp(g_jobs[i].id, job_id))
        found = job_json_locked(&g_jobs[i]);
    if (found) cJSON_AddItemToObject(env, "data", found);
    else       cJSON_AddNullToObject(env, "data");
  } else {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < PJ_MAX; i++)
      if (g_jobs[i].used) cJSON_AddItemToArray(arr, job_json_locked(&g_jobs[i]));
    cJSON_AddItemToObject(env, "data", arr);
  }
  pthread_mutex_unlock(&g_lock);
  cJSON_AddNumberToObject(env, "candidates_now", pagination_probe_candidates());
  char *js = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return js;
}

/* ── proposing the fix ────────────────────────────────────────────────────
 * A proven source becomes an anomaly + a 'url_swap' repair, which is what the
 * existing operator approval route already knows how to apply. Nothing here
 * changes a URL. */
static void propose_swap(db_handle *db, const source_def *d, const char *new_url,
                         const char *param, int n1, int n2) {
  sqlite3_stmt *s = NULL;
  long long anomaly_id = 0;

  char reason[256], evidence[1024];
  snprintf(reason, sizeof reason,
           "pagination gap: fetches one page and stops; '%s' is honoured", param);
  snprintf(evidence, sizeof evidence,
           "{\"probe\":\"pagination\",\"param\":\"%s\",\"page1_records\":%d,"
           "\"page2_new_records\":%d}", param, n1, n2);

  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO collector_anomaly (source_id,verdict,reason,evidence,created_at)"
        " VALUES (?1,'manual',?2,?3,datetime('now'))", -1, &s, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_text(s, 1, d->id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, reason, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, evidence, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_DONE) anomaly_id = sqlite3_last_insert_rowid(db->h);
  sqlite3_finalize(s);
  if (!anomaly_id) return;         /* no anomaly, no repair — never a dangling row */

  cJSON *patch = cJSON_CreateObject();
  cJSON_AddStringToObject(patch, "old_url", d->url);
  cJSON_AddStringToObject(patch, "new_url", new_url);
  cJSON_AddStringToObject(patch, "rationale",
    "verified by the pagination probe: page 2 returned records page 1 did not");
  char *pj = cJSON_PrintUnformatted(patch);
  cJSON_Delete(patch);

  s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO collector_repair (anomaly_id,source_id,status,action,patch,"
        " model,created_at) VALUES (?1,?2,'verified','url_swap',?3,"
        " 'pagination-probe',datetime('now'))", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int64(s, 1, anomaly_id);
    sqlite3_bind_text(s, 2, d->id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, pj ? pj : "{}", -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) != SQLITE_DONE)
      fprintf(stderr, "[pagination] %s: repair row not written: %s\n",
              d->id, sqlite3_errmsg(db->h));
  }
  sqlite3_finalize(s);
  free(pj);
}

/* ── the survey ───────────────────────────────────────────────────────────── */

typedef struct { db_handle *shared; int slot; int limit; } parg;

#define PROBE_TIMEOUT_MS 20000
#define HOST_GAP_US      600000   /* 0.6 s between requests to the same host */

static void bump(int slot, const char *field) {
  pthread_mutex_lock(&g_lock);
  pjob *j = &g_jobs[slot];
  j->done++;
  if      (!strcmp(field, "paginates")) j->paginates++;
  else if (!strcmp(field, "ignored"))   j->ignored++;
  else if (!strcmp(field, "complete"))  j->complete_already++;
  else                                  j->errored++;
  pthread_mutex_unlock(&g_lock);
}

static void *probe_thread(void *vp) {
  parg *a = vp;
  db_handle own;
  db_handle *db = db_worker_open(&own, a->shared);
  http_client *http = http_client_new();
  const source_def **all = registry_all();
  int n = registry_count(), probed = 0;

  if (!db || !http) {
    pthread_mutex_lock(&g_lock);
    snprintf(g_jobs[a->slot].state, sizeof g_jobs[a->slot].state, "error");
    snprintf(g_jobs[a->slot].msg, sizeof g_jobs[a->slot].msg,
             "could not open a worker connection");
    g_jobs[a->slot].ended = (long)time(NULL);
    g_busy = 0;
    pthread_mutex_unlock(&g_lock);
    if (http) http_client_free(http);
    if (db) db_worker_close(&own);
    free(a);
    return NULL;
  }

  for (int i = 0; i < n; i++) {
    long size = 0; const char *sp = NULL;
    if (g_probe_stop) break;                 /* shutdown or operator stop */
    if (!is_candidate(all[i], &size, &sp)) continue;
    if (a->limit > 0 && probed >= a->limit) break;
    probed++;

    /* Plan against the URL that would ACTUALLY be fetched: an override may
     * already have rewritten it. */
    const char *url = url_override_apply(all[i]->url);

    http_response r1 = {0};
    int hard = http_request(http, "GET", url, NULL, NULL, 0,
                            PROBE_TIMEOUT_MS, 0, &r1);
    if (hard || r1.status != 200 || !r1.body) {
      http_response_free(&r1);
      bump(a->slot, "error");
      usleep(HOST_GAP_US);
      continue;
    }
    fpset fp1;
    int ok1 = fingerprint(r1.body, &fp1);
    http_response_free(&r1);
    if (!ok1) { bump(a->slot, "error"); usleep(HOST_GAP_US); continue; }

    /* Page 1 did not even fill the cap, so the cap is not what bounds us —
     * this source already returns everything it has. */
    if (fp1.n < size) { bump(a->slot, "complete"); usleep(HOST_GAP_US); continue; }

    const pw_cand *cands = candidates_for(url, sp);
    const char *verdict = "error";
    for (int c = 0; cands[c].name && !g_probe_stop; c++) {
      usleep(HOST_GAP_US);
      char *u2 = url_with(url, cands[c].name, cands[c].is_page ? 2 : size);
      if (!u2) break;
      http_response r2 = {0};
      int h2 = http_request(http, "GET", u2, NULL, NULL, 0,
                            PROBE_TIMEOUT_MS, 0, &r2);
      if (h2 || r2.status != 200 || !r2.body) {
        http_response_free(&r2); free(u2); continue;
      }
      fpset fp2;
      int ok2 = fingerprint(r2.body, &fp2);
      http_response_free(&r2);
      if (!ok2) { free(u2); continue; }
      if (fp2.n == 0) { verdict = "complete"; free(u2); break; }
      if (overlap(&fp1, &fp2) > 0.5) { verdict = "ignored"; free(u2); continue; }

      /* Proven. Propose the swap starting at the first page, and let an
       * operator decide — this function never rewrites a URL itself. */
      char *proposed = url_with(url, cands[c].name, 0);
      if (proposed) {
        propose_swap(db, all[i], proposed, cands[c].name, fp1.n, fp2.n);
        free(proposed);
      }
      verdict = "paginates";
      free(u2);
      break;
    }
    bump(a->slot, verdict);
  }

  pthread_mutex_lock(&g_lock);
  pjob *j = &g_jobs[a->slot];
  snprintf(j->state, sizeof j->state, "done");
  snprintf(j->msg, sizeof j->msg,
           "%d proven; %d ignore the offset; %d already complete; %d errored. "
           "Proven ones are queued as url_swap repairs for approval.",
           j->paginates, j->ignored, j->complete_already, j->errored);
  j->ended = (long)time(NULL);
  g_busy = 0;
  pthread_mutex_unlock(&g_lock);

  http_client_free(http);
  db_worker_close(&own);
  free(a);
  return NULL;
}

char *pagination_probe_start(db_handle *shared, int limit, int *http_status) {
  cJSON *o = cJSON_CreateObject();
  if (!shared || !shared->h) {
    *http_status = 503;
    cJSON_AddStringToObject(o, "error", "no_database");
    char *js = cJSON_PrintUnformatted(o); cJSON_Delete(o); return js;
  }

  pthread_mutex_lock(&g_lock);
  if (g_busy) {
    pthread_mutex_unlock(&g_lock);
    *http_status = 409;
    cJSON_AddStringToObject(o, "error", "probe_already_running");
    char *js = cJSON_PrintUnformatted(o); cJSON_Delete(o); return js;
  }
  int slot = -1;
  for (int i = 0; i < PJ_MAX; i++) if (!g_jobs[i].used) { slot = i; break; }
  if (slot < 0) {
    for (int i = 0; i < PJ_MAX; i++)
      if (slot < 0 || g_jobs[i].started < g_jobs[slot].started) slot = i;
  }
  pjob *j = &g_jobs[slot];
  memset(j, 0, sizeof *j);
  j->used = 1;
  snprintf(j->id, sizeof j->id, "pp-%d", ++g_seq);
  snprintf(j->state, sizeof j->state, "running");
  j->total = pagination_probe_candidates();
  if (limit > 0 && limit < j->total) j->total = limit;
  j->started = (long)time(NULL);
  g_busy = 1;
  pthread_mutex_unlock(&g_lock);

  /* Populate the argument BEFORE the thread exists. Setting these after
   * pthread_create is a race the worker wins about as often as it loses: it
   * would read a calloc'd NULL db_handle and abort the survey immediately. */
  parg *a = calloc(1, sizeof *a);
  if (a) { a->shared = shared; a->slot = slot; a->limit = limit; }
  pthread_t th;
  if (!a || pthread_create(&th, NULL, probe_thread, a) != 0) {
    free(a);
    pthread_mutex_lock(&g_lock);
    snprintf(j->state, sizeof j->state, "error");
    snprintf(j->msg, sizeof j->msg, "could not start the probe thread");
    g_busy = 0;
    pthread_mutex_unlock(&g_lock);
    *http_status = 500;
    cJSON_AddStringToObject(o, "error", "thread_spawn_failed");
    char *js = cJSON_PrintUnformatted(o); cJSON_Delete(o); return js;
  }
  pthread_detach(th);

  *http_status = 202;
  cJSON_AddStringToObject(o, "job_id", j->id);
  cJSON_AddStringToObject(o, "state", "running");
  cJSON_AddNumberToObject(o, "candidates", j->total);
  cJSON_AddStringToObject(o, "note",
    "Proven sources are queued as url_swap repairs; approve them at "
    "POST /api/admin/repairs/:id/approve. Nothing changes until you do.");
  char *js = cJSON_PrintUnformatted(o); cJSON_Delete(o); return js;
}
