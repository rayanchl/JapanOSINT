/* tests/unit/test_corpus_hybrid.c — JP_CORPUS_LOOKUP's vector arm
 * (collectors/sources/jp_corpus_lookup.c corpus_hybrid, core/qembed.c).
 *
 * WHAT IS BEING PROTECTED.
 *   1. INERT. With no JO_EMBED_URL the lookup is the FTS-only path byte for
 *      byte: same rows, same order, no `retrieval` key, nothing sent.
 *   2. The vector arm finds what an AND-of-tokens FTS query cannot (a row
 *      sharing some words but not all), and every emitted hit SAYS which arm
 *      found it — a semantic neighbour must never pass for a keyword match.
 *   3. Tenancy holds inside the KNN: another tenant's row is never returned,
 *      however close it is.
 *   4. The 15 cap and its "of N" truncation notice survive the fusion.
 *
 * Includes jp_corpus_lookup.c directly to reach run(), so run.sh links every
 * object EXCEPT obj/collectors/sources/jp_corpus_lookup.o and obj/main.o. */

#include "../../collectors/sources/jp_corpus_lookup.c"
#include "../../core/intel.h"
#include "../../core/embed_pod.h"
#include "embed_stub.h"

#include <assert.h>

static db_handle g_db;

/* ---- a capture sink ------------------------------------------------------ */

typedef struct { cJSON *rows; } cap_t;

static int cap_emit(intel_sink *s, const intel_item *it) {
  cap_t *c = (cap_t *)s->ctx;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "title", it->title ? it->title : "");
  cJSON_AddStringToObject(o, "record_type", it->record_type ? it->record_type : "");
  cJSON *p = it->properties_json ? cJSON_Parse(it->properties_json) : NULL;
  cJSON_AddItemToObject(o, "props", p ? p : cJSON_CreateObject());
  cJSON_AddItemToArray(c->rows, o);
  return 1;
}

static cJSON *lookup(const char *entity, const char *tenant) {
  cap_t c = { cJSON_CreateArray() };
  intel_sink s = { .ctx = &c, .emit = cap_emit };
  source_ctx ctx = { .source_id = "JP_CORPUS_LOOKUP", .entity = entity,
                     .db = &g_db, .tenant_id = tenant };
  assert(run(&ctx, &s) == 0);
  return c.rows;
}

static int count_type(cJSON *rows, const char *rt) {
  int n = 0; cJSON *r;
  cJSON_ArrayForEach(r, rows)
    if (!strcmp(cJSON_GetObjectItem(r, "record_type")->valuestring, rt)) n++;
  return n;
}

static int has_title(cJSON *rows, const char *t) {
  cJSON *r;
  cJSON_ArrayForEach(r, rows)
    if (!strcmp(cJSON_GetObjectItem(r, "title")->valuestring, t)) return 1;
  return 0;
}

static void put(const char *tenant, const char *key, const char *title) {
  intel_sink s = intel_sink_make(&g_db, "corpus-test-src", tenant);
  intel_item it = {0};
  it.remote_key = key;
  it.title = title;
  it.summary = title;
  it.record_type = "news";
  assert(s.emit(&s, &it) >= 0);
  intel_sink_free(&s);
}

static void build_index(void) {
  const source_def *pod = registry_get("embedding-backfill");
  assert(pod);
  http_client *hc = http_client_new();
  volatile int cancel = 0;
  source_ctx ctx = { .source_id = "embedding-backfill", .http = hc,
                     .db = &g_db, .cancel = &cancel };
  for (int i = 0; i < 4; i++) pod->run(&ctx, NULL);
  http_client_free(hc);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  assert(dbp && "run.sh sets JO_DB to a scratch database");
  if (db_open(&g_db, dbp, NULL) != 0) return 2;
  unsetenv("JO_EMBED_URL"); unsetenv("JO_EMBED_QUERY_URL");
  unsetenv("JO_EMBED_MODEL");

  /* Three rows hold all of "oil tanker vessel" (keyword matches); two hold
   * only part (vector-only); one belongs to another tenant. */
  put("legacy", "a", "oil tanker vessel detained at yokohama");
  put("t-me",   "b", "oil tanker vessel insurance lapse");
  put("legacy", "c", "oil tanker vessel flag change");
  put("legacy", "d", "tanker vessel ais gap");
  put("t-me",   "e", "vessel tanker sanctions list");
  put("t-other","f", "oil tanker vessel secret charter");
  put("legacy", "g", "unrelated earthquake bulletin");

  /* 1. inert */
  cJSON *a = lookup("oil tanker vessel", "t-me");
  char *inert = cJSON_PrintUnformatted(a);
  assert(cJSON_GetArraySize(a) == 3);
  assert(!strstr(inert, "\"retrieval\""));
  assert(!has_title(a, "oil tanker vessel secret charter"));
  cJSON_Delete(a);

  if (estub_start(18320, 256, "stub-bow") != 0) {
    printf("test_corpus_hybrid: stub unavailable, hybrid cases skipped\n");
    free(inert); estub_stop(); db_close(&g_db);
    return 0;
  }
  estub_use();
  /* The stub's vectors are word counts, so they are never opposed and an
   * unrelated row sits AT sqrt(2), on the default bound's edge. A tighter bound
   * (cos > ~0.15) makes "unrelated" unambiguous here. */
  setenv("JO_CORPUS_VEC_MAX_DISTANCE", "1.3", 1);
  /* configured but no index yet: still the FTS path, byte for byte */
  a = lookup("oil tanker vessel", "t-me");
  char *noidx = cJSON_PrintUnformatted(a);
  assert(!strcmp(noidx, inert));
  assert(estub_posts() == 0);                  /* nothing was embedded */
  free(noidx); cJSON_Delete(a);
  printf("  inert (no server / no index): FTS path byte for byte: ok\n");

  build_index();
  assert(vecindex_count(&g_db, EMBED_VEC_TABLE) >= 7);

  /* 2 + 3 */
  a = lookup("oil tanker vessel", "t-me");
  int kw = 0, vonly = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, a) {
    cJSON *ret = cJSON_GetObjectItem(cJSON_GetObjectItem(r, "props"), "retrieval");
    assert(ret && "every hybrid hit says which arm found it");
    assert(!strcmp(cJSON_GetObjectItem(ret, "method")->valuestring, "rrf"));
    cJSON *arms = cJSON_GetObjectItem(ret, "arms");
    assert(cJSON_GetArraySize(arms) >= 1);
    int fts = cJSON_IsNumber(cJSON_GetObjectItem(ret, "fts_rank"));
    if (fts) kw++; else vonly++;
  }
  assert(kw == 3);
  assert(vonly == 2);                          /* d and e, sharing two words */
  assert(!has_title(a, "unrelated earthquake bulletin"));   /* past the bound */
  assert(has_title(a, "tanker vessel ais gap"));
  assert(has_title(a, "vessel tanker sanctions list"));
  assert(!has_title(a, "oil tanker vessel secret charter"));
  /* the three keyword matches rank above every vector-only row */
  for (int i = 0; i < 3; i++) {
    cJSON *ret = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetArrayItem(a, i), "props"), "retrieval");
    assert(cJSON_IsNumber(cJSON_GetObjectItem(ret, "fts_rank")));
  }
  cJSON_Delete(a);
  printf("  hybrid: 3 keyword + %d semantic-only hits, arms recorded, other "
         "tenant excluded: ok\n", vonly);

  /* 4. the cap and its notice */
  char k[32], t[96];
  for (int i = 0; i < 20; i++) {
    snprintf(k, sizeof k, "bulk%d", i);
    snprintf(t, sizeof t, "oil tanker vessel report number %d", i);
    put("legacy", k, t);
  }
  build_index();
  a = lookup("oil tanker vessel", "t-me");
  assert(count_type(a, "osint_service_result") == CORPUS_LIMIT);
  assert(count_type(a, "collector-truncation-notice") == 1);
  char *s = cJSON_PrintUnformatted(a);
  assert(strstr(s, "keyword + semantic rank fusion"));
  free(s); cJSON_Delete(a);
  printf("  cap %d kept, \"of N\" notice states the fusion: ok\n", CORPUS_LIMIT);

  free(inert);
  estub_stop();
  db_close(&g_db);
  printf("test_corpus_hybrid: ok\n");
  return 0;
}
