/* core/breach_jobs.c — see breach_jobs.h. */
#include "breach_jobs.h"
#include "breach_index.h"   /* breach_index_ingest, breach_type_parse (+ db.h) */
#include "breach_meta.h"    /* breach_meta_seed_path / _corpus_path            */
#include "hostgate.h"
#include "httpclient.h"
#include "../third_party/cJSON.h"
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/JapanOSINT"
#endif

#define BJ_MAX 16

typedef struct {
  int  used;
  char id[24];
  char kind[12];       /* "ingest" | "fetch" */
  char source[80];
  char state[12];      /* "queued" | "running" | "done" | "error" */
  char msg[192];
  unsigned long long rows_in, rows_new;
  long started, ended;
} bjob;

static bjob g_jobs[BJ_MAX];
static int  g_seq = 0;
static int  g_ingest_busy = 0, g_fetch_busy = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *root_dir(void) {
  const char *e = getenv("JO_BREACH_DIR");
  return (e && *e) ? e : JO_REPO_ROOT "/data/breach";
}

/* ── caller-supplied path confinement ──────────────────────────────────────
 * POST /api/admin/breach/ingest {"path":…} and
 * GET  /api/admin/breach/catalog/preview?path=… handed the caller's string
 * straight to fopen(). Preview then returned the parsed lines in its JSON
 * `sample[]`, so
 *
 *     GET /api/admin/breach/catalog/preview?path=/etc/passwd
 *     → 200 {"rows_read":28,"sample":[{"name":"root:x:0:0:root:/root:/bin/bash"…
 *
 * was an arbitrary file read that echoed its contents back over HTTP, and
 * ingest was the same read with the file's lines written into the breach
 * corpus. Both routes are operator-gated, but an operator token is a licence
 * to administer the breach corpus, not a filesystem read primitive: the whole
 * point of the gate is that a stolen or over-scoped operator token stays
 * bounded by what the route legitimately does.
 *
 * The bound is the breach data directory — JO_BREACH_DIR, the same root
 * breach_index.c shards into and the one ensure_staging() writes downloads to,
 * so every path these routes are actually FOR is already inside it. Both sides
 * are put through realpath() and compared as resolved absolute paths, which
 * settles "..", symlinks, and absolute paths in one check rather than three
 * string tests that each need to be right.
 *
 * The two committed catalogue files (docs/breach-corpus.seed.tsv and
 * docs/breach-corpus.json) are allowed by exact resolved path. They are the
 * defaults these routes use when `path` is omitted, and an operator pasting
 * the path they can see in the docs must not be told no; allowing the two
 * FILES rather than their directory is what keeps the rest of docs/ out.
 *
 * A rejection is deliberately ONE outcome for every cause — outside the base,
 * does not exist, is not readable, is a dangling symlink. realpath() cannot
 * distinguish them to the caller either way, so the 400 does not become an
 * existence oracle for paths the caller is not allowed to read. */
static int path_inside(const char *base, const char *path) {
  size_t bl = strlen(base);
  while (bl > 1 && base[bl - 1] == '/') bl--;      /* tolerate a trailing '/' */
  if (bl == 0) return 0;
  if (strncmp(path, base, bl) != 0) return 0;
  /* The boundary test matters: without it "/data/breach-evil/x" passes the
   * prefix test for base "/data/breach". */
  return path[bl] == '/';
}

/* Exact-resolved-path match against one of the two committed catalogue files.
 * A file that does not resolve simply never matches. */
static int is_allowed_file(const char *resolved, const char *candidate) {
  char rp[PATH_MAX];
  if (!candidate || !*candidate) return 0;
  if (!realpath(candidate, rp)) return 0;
  return strcmp(resolved, rp) == 0;
}

int breach_path_confine(const char *req, char *out, size_t cap) {
  if (!req || !*req || !out || cap == 0) return -1;
  out[0] = 0;

  char rp[PATH_MAX];
  if (!realpath(req, rp)) return -1;

  char base[PATH_MAX];
  /* mkdir first for the same reason ensure_staging() does: on a fresh install
   * the breach root does not exist yet, realpath() would fail on it, and every
   * ingest would be refused with a message about confinement that was really
   * about a missing directory. */
  mkdir(root_dir(), 0755);
  int ok = realpath(root_dir(), base) && path_inside(base, rp);

  if (!ok) ok = is_allowed_file(rp, breach_meta_seed_path());
  if (!ok) ok = is_allowed_file(rp, breach_meta_corpus_path());
  if (!ok) return -1;

  if (strlen(rp) >= cap) return -1;
  snprintf(out, cap, "%s", rp);
  return 0;
}

const char *breach_path_confine_error(void) {
  return "{\"error\":\"path_not_permitted\",\"detail\":\"path must resolve to a "
         "readable file inside the breach data directory (JO_BREACH_DIR) or to "
         "a committed breach catalogue file\"}";
}

/* Reserve a job slot (unused, else recycle the oldest finished one). Caller
 * holds g_lock. Returns slot index or -1 when all slots are busy. */
static int alloc_slot_locked(const char *kind, const char *source) {
  int slot = -1;
  for (int i = 0; i < BJ_MAX; i++) if (!g_jobs[i].used) { slot = i; break; }
  if (slot < 0) {
    for (int i = 0; i < BJ_MAX; i++) {
      const char *st = g_jobs[i].state;
      if (!strcmp(st, "done") || !strcmp(st, "error"))
        if (slot < 0 || g_jobs[i].started < g_jobs[slot].started) slot = i;
    }
  }
  if (slot < 0) return -1;
  bjob *j = &g_jobs[slot];
  memset(j, 0, sizeof *j);
  j->used = 1;
  snprintf(j->id, sizeof j->id, "bj-%d", ++g_seq);
  snprintf(j->kind, sizeof j->kind, "%s", kind);
  snprintf(j->source, sizeof j->source, "%s", source);
  snprintf(j->state, sizeof j->state, "queued");
  j->started = (long)time(NULL);
  return slot;
}

static void set_state(int slot, const char *state, const char *msg) {
  pthread_mutex_lock(&g_lock);
  if (slot >= 0 && slot < BJ_MAX && g_jobs[slot].used) {
    snprintf(g_jobs[slot].state, sizeof g_jobs[slot].state, "%s", state);
    if (msg) snprintf(g_jobs[slot].msg, sizeof g_jobs[slot].msg, "%s", msg);
  }
  pthread_mutex_unlock(&g_lock);
}

static void finish(int slot, const char *state, const char *msg,
                   unsigned long long in, unsigned long long nw) {
  pthread_mutex_lock(&g_lock);
  if (slot >= 0 && slot < BJ_MAX && g_jobs[slot].used) {
    snprintf(g_jobs[slot].state, sizeof g_jobs[slot].state, "%s", state);
    if (msg) snprintf(g_jobs[slot].msg, sizeof g_jobs[slot].msg, "%s", msg);
    g_jobs[slot].rows_in = in;
    g_jobs[slot].rows_new = nw;
    g_jobs[slot].ended = (long)time(NULL);
  }
  pthread_mutex_unlock(&g_lock);
}

/* ── ingest ───────────────────────────────────────────────────────────── */
typedef struct { int slot; char source[80]; char *path; char type[16];
                 int materialize, dry_run; } ing_arg;

static void *ingest_thread(void *vp) {
  ing_arg *a = vp;
  set_state(a->slot, "running", "ingesting");
  unsigned long long in = 0, nw = 0;

  /* Own WAL connection — never the server's shared handle (its bulk transaction
   * + pragmas would otherwise engulf the event loop's writes). DB only needed
   * for a real (non-dry) materialize.
   *
   * db_attach, not db_open: db_open re-executes schema.sql, every ensure_column
   * migration and a 674-row seed transaction against a live database, so this
   * job used to fight the very writers the separate connection exists to avoid.
   * The server process already applied all of that at boot. */
  db_handle db = {0}; db_handle *dbp = NULL; int opened = 0;
  if (a->materialize && !a->dry_run) {
    if (db_attach(&db, NULL) == 0) { dbp = &db; opened = 1; }
    else finish(a->slot, "error", "db open failed", 0, 0);
  }
  if (!(a->materialize && !a->dry_run) || opened) {
    breach_type t = breach_type_parse(a->type[0] ? a->type : NULL);
    int rc = breach_index_ingest(a->source, a->path, t, &in, &nw,
                                 dbp, a->materialize, a->dry_run);
    if (opened) db_close(&db);
    if (rc != 0)
      finish(a->slot, "error", "ingest failed (cannot open file?)", in, nw);
    else
      finish(a->slot, "done",
             a->dry_run ? "dry-run complete"
                        : (a->materialize ? "materialized to breach_items"
                                          : "ingested to shard corpus"),
             in, nw);
  }

  pthread_mutex_lock(&g_lock); g_ingest_busy = 0; pthread_mutex_unlock(&g_lock);
  free(a->path); free(a);
  return NULL;
}

char *breach_job_ingest(const char *source_id, const char *path, const char *type,
                        int materialize, int dry_run, int *http_status) {
  if (!source_id || !*source_id || !path || !*path) {
    *http_status = 400;
    return strdup("{\"error\":\"source_id and path required\"}");
  }
  /* Confine BEFORE a job slot is taken, so a rejected path cannot also park
   * the single ingest slot. The resolved path is what the thread opens — the
   * caller's spelling is never re-derived, which is what stops a check/use
   * gap between this test and the fopen() in ingest_thread(). */
  char safe_path[PATH_MAX];
  if (breach_path_confine(path, safe_path, sizeof safe_path) != 0) {
    *http_status = 400;
    return strdup(breach_path_confine_error());
  }
  pthread_mutex_lock(&g_lock);
  if (g_ingest_busy) {
    pthread_mutex_unlock(&g_lock);
    *http_status = 409;
    return strdup("{\"error\":\"an ingest is already running\"}");
  }
  int slot = alloc_slot_locked("ingest", source_id);
  if (slot < 0) {
    pthread_mutex_unlock(&g_lock);
    *http_status = 503;
    return strdup("{\"error\":\"job table full\"}");
  }
  g_ingest_busy = 1;
  char id[24]; snprintf(id, sizeof id, "%s", g_jobs[slot].id);
  pthread_mutex_unlock(&g_lock);

  ing_arg *a = calloc(1, sizeof *a);
  if (!a) { *http_status = 500;
    pthread_mutex_lock(&g_lock); g_ingest_busy = 0; g_jobs[slot].used = 0; pthread_mutex_unlock(&g_lock);
    return strdup("{\"error\":\"oom\"}"); }
  a->slot = slot;
  snprintf(a->source, sizeof a->source, "%s", source_id);
  a->path = strdup(safe_path);
  snprintf(a->type, sizeof a->type, "%s", type ? type : "");
  a->materialize = materialize; a->dry_run = dry_run;

  pthread_t th;
  if (pthread_create(&th, NULL, ingest_thread, a) != 0) {
    free(a->path); free(a);
    pthread_mutex_lock(&g_lock); g_ingest_busy = 0; g_jobs[slot].used = 0; pthread_mutex_unlock(&g_lock);
    *http_status = 500;
    return strdup("{\"error\":\"cannot start job thread\"}");
  }
  pthread_detach(th);

  *http_status = 202;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "job_id", id);
  cJSON_AddStringToObject(o, "kind", "ingest");
  cJSON_AddStringToObject(o, "source_id", source_id);
  cJSON_AddStringToObject(o, "state", "started");
  cJSON_AddBoolToObject(o, "materialize", materialize != 0);
  cJSON_AddBoolToObject(o, "dry_run", dry_run != 0);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

/* ── fetch ────────────────────────────────────────────────────────────── */
typedef struct { int slot; char source[80]; char *url; } fet_arg;

static void ensure_staging(char *out, size_t n) {
  char root[1024]; snprintf(root, sizeof root, "%s", root_dir()); mkdir(root, 0755);
  snprintf(out, n, "%s/staging", root_dir()); mkdir(out, 0755);
}

static void *fetch_thread(void *vp) {
  fet_arg *a = vp;
  set_state(a->slot, "running", "downloading");
  http_client *h = http_client_new();
  http_response r = {0};
  int rc = h ? http_request(h, "GET", a->url, NULL, NULL, 0, 60000, 2, &r) : -1;
  if (rc == 0 && r.status >= 200 && r.status < 300 && r.body) {
    char dir[1024]; ensure_staging(dir, sizeof dir);
    char path[1200]; snprintf(path, sizeof path, "%s/%s.dat", dir, a->source);
    FILE *f = fopen(path, "wb");
    if (f) {
      fwrite(r.body, 1, r.body_len, f);
      fclose(f);
      char m[192];
      snprintf(m, sizeof m, "staged %zu bytes -> %.120s (status %ld)",
               r.body_len, path, r.status);
      finish(a->slot, "done", m, 0, 0);
    } else finish(a->slot, "error", "cannot write staging file", 0, 0);
  } else {
    char m[128]; snprintf(m, sizeof m, "download failed (status %ld)", r.status);
    finish(a->slot, "error", m, 0, 0);
  }
  http_response_free(&r);
  if (h) http_client_free(h);
  pthread_mutex_lock(&g_lock); g_fetch_busy = 0; pthread_mutex_unlock(&g_lock);
  free(a->url); free(a);
  return NULL;
}

char *breach_job_fetch(const char *source_id, const char *url, int *http_status) {
  if (!source_id || !*source_id || !url || !*url) {
    *http_status = 400;
    return strdup("{\"error\":\"source_id and url required\"}");
  }

  /* `url` is a caller-supplied fetch destination, which is exactly the class
   * alertsapi.c's webhook target belongs to — and it was going out through
   * http_request()'s floor check (hostgate_url_check), which permits loopback
   * and RFC1918 by default because llama-server lives on 127.0.0.1 and LAN
   * cameras are a shipped feature. So
   *
   *     POST /api/admin/breach/fetch {"url":"http://127.0.0.1:4712/api/health"}
   *
   * fetched this server's own API and staged the response body to disk, with
   * the job status line reporting the byte count — an SSRF read primitive with
   * a result channel. The strict ruleset (no loopback, no RFC1918, no
   * link-local, no CGNAT, no unique-local v6, every RESOLVED address judged,
   * not just the literal) is the one alertsapi.c:264 already applies to the
   * webhook target for the identical reason. Refused here, at save time, where
   * there is still a 400 to return and before a job slot is taken. */
  { int gr = hostgate_url_check_strict(url);
    if (gr != HG_URL_OK) {
      *http_status = 400;
      cJSON *e = cJSON_CreateObject();
      cJSON_AddStringToObject(e, "error", "fetch_url_rejected");
      cJSON_AddStringToObject(e, "detail", hostgate_url_reason(gr));
      char *s = cJSON_PrintUnformatted(e);
      cJSON_Delete(e);
      return s ? s : strdup("{\"error\":\"fetch_url_rejected\"}");
    } }

  pthread_mutex_lock(&g_lock);
  if (g_fetch_busy) {
    pthread_mutex_unlock(&g_lock);
    *http_status = 409;
    return strdup("{\"error\":\"a fetch is already running\"}");
  }
  int slot = alloc_slot_locked("fetch", source_id);
  if (slot < 0) {
    pthread_mutex_unlock(&g_lock);
    *http_status = 503;
    return strdup("{\"error\":\"job table full\"}");
  }
  g_fetch_busy = 1;
  char id[24]; snprintf(id, sizeof id, "%s", g_jobs[slot].id);
  pthread_mutex_unlock(&g_lock);

  fet_arg *a = calloc(1, sizeof *a);
  if (!a) { *http_status = 500;
    pthread_mutex_lock(&g_lock); g_fetch_busy = 0; g_jobs[slot].used = 0; pthread_mutex_unlock(&g_lock);
    return strdup("{\"error\":\"oom\"}"); }
  a->slot = slot;
  snprintf(a->source, sizeof a->source, "%s", source_id);
  a->url = strdup(url);

  pthread_t th;
  if (pthread_create(&th, NULL, fetch_thread, a) != 0) {
    free(a->url); free(a);
    pthread_mutex_lock(&g_lock); g_fetch_busy = 0; g_jobs[slot].used = 0; pthread_mutex_unlock(&g_lock);
    *http_status = 500;
    return strdup("{\"error\":\"cannot start job thread\"}");
  }
  pthread_detach(th);

  *http_status = 202;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "job_id", id);
  cJSON_AddStringToObject(o, "kind", "fetch");
  cJSON_AddStringToObject(o, "source_id", source_id);
  cJSON_AddStringToObject(o, "state", "started");
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

/* ── status ───────────────────────────────────────────────────────────── */
static void job_to_json(cJSON *arr, const bjob *j) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "job_id", j->id);
  cJSON_AddStringToObject(o, "kind", j->kind);
  cJSON_AddStringToObject(o, "source_id", j->source);
  cJSON_AddStringToObject(o, "state", j->state);
  if (j->msg[0]) cJSON_AddStringToObject(o, "message", j->msg);
  cJSON_AddNumberToObject(o, "rows_in", (double)j->rows_in);
  cJSON_AddNumberToObject(o, "rows_new", (double)j->rows_new);
  cJSON_AddNumberToObject(o, "started", (double)j->started);
  if (j->ended) cJSON_AddNumberToObject(o, "ended", (double)j->ended);
  cJSON_AddItemToArray(arr, o);
}

char *breach_job_status(const char *job_id) {
  cJSON *arr = cJSON_CreateArray();
  pthread_mutex_lock(&g_lock);
  for (int i = 0; i < BJ_MAX; i++) {
    if (!g_jobs[i].used) continue;
    if (job_id && *job_id && strcmp(g_jobs[i].id, job_id)) continue;
    job_to_json(arr, &g_jobs[i]);
  }
  pthread_mutex_unlock(&g_lock);
  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "jobs", arr);
  char *s = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return s ? s : strdup("{\"jobs\":[]}");
}
