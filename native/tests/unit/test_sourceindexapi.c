/* tests/unit/test_sourceindexapi.c — GET /api/sources/search
 * (core/sourceindexapi.c) and service_vec_route_sources() over the widened
 * service index, against tools/embed_stub_server.py.
 *
 * WHAT IS BEING PROTECTED.
 *   1. Inert without an embedding server: an honest 503 naming the reason,
 *      never an empty 200 that reads as "no source matches".
 *   2. A kind filter is a filter: kind=scheduled returns scheduled sources
 *      only, on both arms (rule 4d's confident wrong answer, in miniature).
 *   3. Bad input is refused, not guessed at: no q, an unknown kind.
 *   4. A capped k says it was capped (rule 2).
 *   5. route_sources returns SCHEDULED sources only, really registered.
 *
 * The stub's vectors are hashed character trigrams — deterministic and
 * query-dependent, with no claim about embedding quality.
 *
 * Includes sourceindexapi.c directly, so run.sh links every object EXCEPT
 * obj/core/sourceindexapi.o and obj/main.o. */

#include "../../core/sourceindexapi.c"
#include "../../core/llm.h"

#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t g_stub = 0;

static void stub_stop(void) {
  if (g_stub > 0) { kill(g_stub, SIGKILL); waitpid(g_stub, NULL, 0); g_stub = 0; }
}

static int answers(int port) {
  char url[64];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
  llm_client c = { .http = NULL, .base_url = url, .interactive = 1 };
  const char *t[1] = { "ping" };
  float *v = NULL; int d = 0; llm_status st;
  int ok = llm_embed(&c, t, 1, &v, &d, 1000, &st) == 0;
  free(v);
  return ok;
}

/* tools/embed_stub_server.py on the first free port from `port`. */
static int stub_start(int port) {
  for (int tries = 0; tries < 40 && answers(port); tries++) port++;
  if (answers(port)) return -1;
  fflush(stdout);                    /* freopen in the child would flush it again */
  pid_t p = fork();
  if (p < 0) return -1;
  if (p == 0) {
    char ps[16];
    snprintf(ps, sizeof ps, "%d", port);
    if (!freopen("/dev/null", "w", stdout)) { /* stub noise is harmless */ }
    execlp("python3", "python3", "tools/embed_stub_server.py", ps, "64", (char *)NULL);
    _exit(127);
  }
  g_stub = p;
  for (int i = 0; i < 100; i++) {
    if (answers(port)) {
      char url[64];
      snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
      setenv("JO_EMBED_URL", url, 1);
      return 0;
    }
    usleep(100000);
  }
  return -1;
}

static cJSON *call(db_handle *db, const char *qs, int *st) {
  *st = 0;
  char *body = sourceindexapi_search(db, NULL, "", qs, 0, st);
  assert(body);
  cJSON *o = cJSON_Parse(body);
  free(body);
  assert(o);
  return o;
}

static void test_inert(db_handle *db) {
  unsetenv("JO_EMBED_URL");
  int st;
  cJSON *o = call(db, "q=who%20owns%20example.com", &st);
  assert(st == 503);
  cJSON *r = cJSON_GetObjectItem(o, "reason");
  assert(cJSON_IsString(r) && strstr(r->valuestring, "JO_EMBED_URL"));
  assert(cJSON_GetObjectItem(o, "coverage"));
  cJSON_Delete(o);
  char **ids = NULL;
  assert(service_vec_route_sources(db, "earthquake", NULL, 5, &ids) == -1);
  assert(ids == NULL);
  printf("  inert: 503 with the reason, route_sources -1: ok\n");
}

static void test_bad_input(db_handle *db) {
  int st;
  cJSON *o = call(db, "kind=pivot", &st);
  assert(st == 400); cJSON_Delete(o);
  o = call(db, "q=x&kind=feeds", &st);
  assert(st == 400); cJSON_Delete(o);
  printf("  no q / unknown kind refused with 400: ok\n");
}

static void test_search(db_handle *db) {
  int st;
  cJSON *o = call(db, "q=domain%20whois%20registrar&k=10", &st);
  assert(st == 200);
  cJSON *res = cJSON_GetObjectItem(o, "results");
  assert(cJSON_IsArray(res) && cJSON_GetArraySize(res) > 0 &&
         cJSON_GetArraySize(res) <= 10);
  cJSON *it;
  cJSON_ArrayForEach(it, res) {
    const char *id = cJSON_GetObjectItem(it, "id")->valuestring;
    assert(registry_get(id));
    assert(cJSON_IsString(cJSON_GetObjectItem(it, "category")));
    assert(cJSON_IsString(cJSON_GetObjectItem(it, "kind")));
    assert(cJSON_IsNumber(cJSON_GetObjectItem(it, "score")));
  }
  cJSON *cov = cJSON_GetObjectItem(o, "coverage");
  assert(cov && cJSON_GetArraySize(cJSON_GetObjectItem(cov, "searched_kinds")) == 4);
  assert(cJSON_GetObjectItem(cov, "cards_searched")->valueint > 18000);
  printf("  search: %d results with id/name/category/kind/score over %d cards: ok\n",
         cJSON_GetArraySize(res), cJSON_GetObjectItem(cov, "cards_searched")->valueint);
  cJSON_Delete(o);

  o = call(db, "q=earthquake%20seismic%20intensity&kind=scheduled&k=25", &st);
  assert(st == 200);
  res = cJSON_GetObjectItem(o, "results");
  assert(cJSON_GetArraySize(res) > 0);
  cJSON_ArrayForEach(it, res)
    assert(!strcmp(cJSON_GetObjectItem(it, "kind")->valuestring, "scheduled"));
  printf("  kind=scheduled returns scheduled sources only: ok\n");
  cJSON_Delete(o);

  o = call(db, "q=earthquake&k=5000", &st);
  assert(st == 200);
  assert(cJSON_GetObjectItem(o, "k")->valueint == SIDX_K_MAX);
  assert(cJSON_GetObjectItem(o, "k_requested")->valueint == 5000);
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(o, "results")) <= SIDX_K_MAX);
  printf("  k=5000 capped at %d and the cap stated: ok\n", SIDX_K_MAX);
  cJSON_Delete(o);
}

static void test_route_sources(db_handle *db) {
  char **ids = NULL;
  int n = service_vec_route_sources(db, "earthquake seismic intensity reports",
                                    NULL, 8, &ids);
  assert(n > 0 && n <= 8 && ids);
  for (int i = 0; i < n; i++) {
    const source_def *d = registry_get(ids[i]);
    assert(d && d->update_interval_sec > 0);
    assert(source_kind_of(d) == SC_KIND_SCHEDULED);
  }
  printf("  route_sources: %d scheduled sources for a no-entity question "
         "(first %s): ok\n", n, ids[0]);
  service_vec_free_ids(ids, n);
  /* an explicit kind other than scheduled is honoured, never widened */
  n = service_vec_route_sources(db, "domain whois", "pivot", 5, &ids);
  assert(n > 0);
  for (int i = 0; i < n; i++)
    assert(source_kind_of(registry_get(ids[i])) == SC_KIND_PIVOT);
  service_vec_free_ids(ids, n);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  assert(dbp && "run.sh sets JO_DB to a scratch database");
  db_handle db;
  if (db_open(&db, dbp, NULL) != 0) return 2;
  setenv("JO_EMBED_MODEL_CHECK_TTL_MS", "0", 1);
  setenv("JO_SVEC_MAX_PER_RUN", "0", 1);
  unsetenv("JO_EMBED_MODEL");

  test_inert(&db);
  test_bad_input(&db);
  if (stub_start(18240) != 0) {
    printf("test_sourceindexapi: stub unavailable, active-path tests skipped\n");
    stub_stop();
    db_close(&db);
    return 0;
  }
  assert(service_vec_build(&db) > 0);
  test_search(&db);
  test_route_sources(&db);
  stub_stop();
  db_close(&db);
  printf("test_sourceindexapi: ok\n");
  return 0;
}
