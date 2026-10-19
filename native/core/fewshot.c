/* core/fewshot.c — see fewshot.h. */
#include "fewshot.h"
#include "qembed.h"
#include "../third_party/cJSON.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const vecindex_spec fewshot_spec = {
  .name = "fewshot", .table = "fewshot_vec", .key_col = "key",
  .meta_table = "fewshot_vec_meta", .done_table = "fewshot_vec_done",
  .fail_table = "fewshot_vec_failed", .cols = NULL, .ncols = 0,
  .rebuild_hint = " (rebuild: DROP TABLE fewshot_vec, fewshot_vec_meta, "
                  "fewshot_vec_done, fewshot_vec_failed — 76 short texts, "
                  "seconds to re-embed)" };
VECINDEX_REGISTER(fewshot_spec)

typedef struct {
  char *query, *response;
  char key[17];
  int spine;
} fs_example;

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static int         g_loaded = 0;
static fs_example *g_ex = NULL;
static int         g_n = 0;
static char       *g_static = NULL;
static time_t      g_sync_tried = 0;      /* last sync attempt (0 = never)    */
static int         g_synced = 0;

/* Load once. Called with g_mu held. A malformed line is reported and skipped
 * — and the static block is then NOT the old prompt, so it says so. */
static void load_locked(void) {
  if (g_loaded) return;
  g_loaded = 1;
  char path[1024];
  snprintf(path, sizeof path, "%s/%s", JO_REPO_ROOT, FEWSHOT_FILE);
  FILE *f = fopen(path, "rb");
  size_t cap = 1, len = 0;
  g_static = malloc(cap);
  if (g_static) g_static[0] = 0;
  if (!f) {
    fprintf(stderr, "[fewshot] %s unreadable: the analysis prompt has NO "
                    "routing examples\n", path);
    return;
  }
  char *line = NULL;
  size_t lcap = 0;
  ssize_t got;
  int lineno = 0, ecap = 0;
  while ((got = getline(&line, &lcap, f)) != -1) {
    lineno++;
    if (got <= 1) continue;
    cJSON *o = cJSON_Parse(line);
    const cJSON *q = cJSON_GetObjectItem(o, "query");
    const cJSON *r = cJSON_GetObjectItem(o, "response");
    if (!cJSON_IsString(q) || !cJSON_IsString(r)) {
      fprintf(stderr, "[fewshot] %s:%d is not {query, response}; skipped\n",
              path, lineno);
      cJSON_Delete(o);
      continue;
    }
    if (g_n == ecap) {
      int nc = ecap ? ecap * 2 : 128;
      fs_example *ne = realloc(g_ex, (size_t)nc * sizeof *ne);
      if (!ne) { cJSON_Delete(o); break; }
      g_ex = ne; ecap = nc;
    }
    fs_example *e = &g_ex[g_n];
    e->query = strdup(q->valuestring);
    e->response = strdup(r->valuestring);
    e->spine = cJSON_IsTrue(cJSON_GetObjectItem(o, "spine"));
    cJSON_Delete(o);
    if (!e->query || !e->response) { free(e->query); free(e->response); break; }
    size_t kl = strlen(e->query) + strlen(e->response) + 2;
    char *kt = malloc(kl);
    if (!kt) { free(e->query); free(e->response); break; }
    snprintf(kt, kl, "%s\n%s", e->query, e->response);
    snprintf(e->key, sizeof e->key, "%016llx", vecindex_hash(kt));
    free(kt);
    /* the static block: "Query: q\nresponse\n\n", exactly as prompts.c wrote */
    size_t add = strlen(e->query) + strlen(e->response) + 10;
    if (g_static && len + add + 1 > cap) {
      while (len + add + 1 > cap) cap *= 2;
      char *ns = realloc(g_static, cap);
      if (!ns) { free(g_static); g_static = NULL; }
      else g_static = ns;
    }
    if (g_static)
      len += (size_t)snprintf(g_static + len, cap - len, "Query: %s\n%s\n\n",
                              e->query, e->response);
    g_n++;
  }
  free(line);
  fclose(f);
}

int fewshot_count(void) {
  pthread_mutex_lock(&g_mu);
  load_locked();
  int n = g_n;
  pthread_mutex_unlock(&g_mu);
  return n;
}

const char *fewshot_static_block(void) {
  pthread_mutex_lock(&g_mu);
  load_locked();
  const char *s = g_static ? g_static : "";
  pthread_mutex_unlock(&g_mu);
  return s;
}

/* Sync the index on first use in this process (and again at most once a
 * minute while it fails). 1 when the index is usable now. */
static int ensure_index(db_handle *db) {
  int dim = 0;
  char why[600];
  pthread_mutex_lock(&g_mu);
  time_t now = time(NULL);
  int try_sync = !g_synced && (g_sync_tried == 0 || now - g_sync_tried >= 60);
  if (try_sync) g_sync_tried = now;
  pthread_mutex_unlock(&g_mu);
  if (try_sync) {
    char model[256];
    if (vecindex_live_query_model(model, sizeof model) != 0) {
      fprintf(stderr, "[fewshot] cannot identify the query server's model: %s\n",
              model);
      return 0;
    }
    vecindex_item *it = calloc((size_t)(g_n > 0 ? g_n : 1), sizeof *it);
    if (!it) return 0;
    for (int i = 0; i < g_n; i++) { it[i].key = g_ex[i].key; it[i].text = g_ex[i].query; }
    llm_client llm = { .http = NULL, .base_url = vecindex_query_url(),
                       .interactive = 1, .bound_queue_wait = 1 };
    char ts[40];
    time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
    strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%S.000Z", &g);
    vecindex_stats st = {0};
    int rc = vecindex_sync(db, &fewshot_spec, &llm, model, it, g_n, 32, ts, ts, &st);
    free(it);
    if (rc == 0) {
      pthread_mutex_lock(&g_mu);
      g_synced = 1;
      pthread_mutex_unlock(&g_mu);
      if (st.embedded)
        fprintf(stderr, "[fewshot] index synced: %ld embedded, %ld unchanged\n",
                st.embedded, st.unchanged);
    } else
      fprintf(stderr, "[fewshot] index sync failed (rc=%d); static examples\n", rc);
  }
  return qembed_index_usable(db, &fewshot_spec, &dim, why, sizeof why);
}

char *fewshot_select(db_handle *db, const char *query, fewshot_note *note) {
  if (note) memset(note, 0, sizeof *note);
  if (!qembed_enabled() || !db || !query || !*query) return NULL;   /* inert */
  pthread_mutex_lock(&g_mu);
  load_locked();
  int n = g_n;
  pthread_mutex_unlock(&g_mu);
  if (n == 0 || !ensure_index(db)) return NULL;

  const char *texts[1] = { query };
  float *qv = NULL; int qd = 0; llm_status st;
  if (qembed_texts(texts, 1, &qv, &qd, &st) != 0) return NULL;
  vecindex_hit *h = NULL;
  char err[256];
  int k = vecindex_knn(db, &fewshot_spec, qv, qd, FEWSHOT_TOP_K, NULL, 0, &h,
                       err, sizeof err);
  free(qv);
  if (k <= 0) { vecindex_hits_free(h, k > 0 ? k : 0); return NULL; }

  /* g_ex is immutable once loaded, so it is read without the lock. */
  char *pick = calloc((size_t)n, 1);
  if (!pick) { vecindex_hits_free(h, k); return NULL; }
  int sim = 0, spine = 0;
  for (int i = 0; i < k; i++)
    for (int j = 0; j < n; j++)
      if (!pick[j] && !strcmp(g_ex[j].key, h[i].key)) { pick[j] = 1; sim++; break; }
  vecindex_hits_free(h, k);
  if (sim == 0) { free(pick); return NULL; }     /* only stale keys: say nothing */
  for (int j = 0; j < n; j++)
    if (g_ex[j].spine && !pick[j]) { pick[j] = 2; spine++; }

  size_t cap = 1;
  for (int j = 0; j < n; j++)
    if (pick[j]) cap += strlen(g_ex[j].query) + strlen(g_ex[j].response) + 10;
  char *out = malloc(cap);
  if (!out) { free(pick); return NULL; }
  size_t len = 0;
  out[0] = 0;
  int chosen = 0;
  for (int j = 0; j < n; j++) {
    if (!pick[j]) continue;
    len += (size_t)snprintf(out + len, cap - len, "Query: %s\n%s\n\n",
                            g_ex[j].query, g_ex[j].response);
    chosen++;
  }
  free(pick);
  if (note) {
    note->total = n; note->chosen = chosen;
    note->by_similarity = sim; note->spine = spine;
  }
  return out;
}
