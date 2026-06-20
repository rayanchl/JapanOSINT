/* core/searchapi.c — see header. Port of routes/search.js (analyze/suggest/
 * results). The SSE /stream lives in httpd.c (it owns the connection loop). */
#include "searchapi.h"
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
static void *run_thread(void *vp) {
  run_arg *a = vp;
  http_client *http = http_client_new();   /* own client (== scheduler) */
  llm_client llm; llm_init(&llm, http);
  llm.interactive = 1;   /* live user search → high-priority LLM lane */
  osint_pipeline_run(a->db, &llm, a->id, a->query, a->max_rounds);
  http_client_free(http);
  free(a->query);
  free(a);
  return NULL;
}

char *searchapi_analyze(db_handle *db, const char *query, int max_rounds) {
  if (!query) return NULL;
  while (*query == ' ' || *query == '\t' || *query == '\n' || *query == '\r') query++;
  if (!*query) return NULL;

  char id[40];
  gen_id(id);
  if (!progress_create(id, query, max_rounds > 0 ? max_rounds : 5))
    return NULL;

  run_arg *a = calloc(1, sizeof *a);
  a->db = db;
  snprintf(a->id, sizeof a->id, "%s", id);
  a->query = strdup(query);
  a->max_rounds = max_rounds;
  pthread_t t;
  if (pthread_create(&t, NULL, run_thread, a) == 0) {
    pthread_detach(t);
  } else {
    free(a->query); free(a);
    return NULL;
  }

  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "request_id", id);
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

char *searchapi_results(db_handle *db, const char *id) {
  if (!id || !*id) return NULL;
  osint_request *rp = progress_get(id);
  if (rp) return progress_to_json(rp);

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
      cJSON *r = cJSON_CreateObject();
      cJSON_AddStringToObject(r, "synthesis", summary ? summary : "");
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
