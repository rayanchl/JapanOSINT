/* tests/unit/test_twitter_geo.c — twitter-geo reads the whole geocoded-post
 * table over a cycle (collectors/sources/twitter_geo.c).
 *
 * WHAT IS BEING PROTECTED. The layer used to read `ORDER BY fetched_at DESC
 * LIMIT 5000`, so every geocoded post older than the newest 5,000 dropped out
 * of it silently (house rule 2). It walks a keyset on (fetched_at, uid) now:
 *   1. a run reads one slice, oldest first, and stores where it stopped;
 *   2. the next run resumes PAST that point — no post read twice in a walk, and
 *      a tie on fetched_at across the slice boundary loses nothing (uid breaks
 *      the tie);
 *   3. while behind, the run files a truncation notice whose numbers are the
 *      slice and the slice plus what remains;
 *   4. once caught up, a run reads only posts that arrived since, keeps its
 *      position when nothing arrived, and files no notice;
 *   5. a token in another shape is treated as absent (source.h's contract).
 *
 * Includes twitter_geo.c directly to reach run() and g_tg_batch, so run.sh
 * links every object EXCEPT obj/collectors/sources/twitter_geo.o. */

#include "../../collectors/sources/twitter_geo.c"

#include <assert.h>

static db_handle g_db;
static char g_tok[512];
static int  g_tok_set;

static void put(const char *uid, const char *fetched_at) {
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(g_db.h,
    "insert into intel_items(uid, source_id, title, fetched_at, record_type, "
    "sub_source_id, lat, lon, geom_source) "
    "values(?1, 'misskey-timeline', 'p', ?2, 'post', NULL, 35.0, 139.0, 'gps')",
    -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 2, fetched_at, -1, SQLITE_STATIC);
  assert(sqlite3_step(s) == SQLITE_DONE);
  sqlite3_finalize(s);
}

static void save_tok(const source_ctx *c, const char *t) {
  (void)c;
  g_tok_set = 1;
  snprintf(g_tok, sizeof g_tok, "%s", t ? t : "");
}

typedef struct { int posts, notices; long used, avail; char ids[64][64]; } cap_t;

static int cap_emit(intel_sink *s, const intel_item *it) {
  cap_t *c = (cap_t *)s->ctx;
  if (it->record_type && !strcmp(it->record_type, "collector-truncation-notice")) {
    c->notices++;
    cJSON *p = cJSON_Parse(it->properties_json);
    c->used  = (long)cJSON_GetObjectItem(p, "records_used")->valuedouble;
    c->avail = (long)cJSON_GetObjectItem(p, "records_available")->valuedouble;
    cJSON_Delete(p);
    return 1;
  }
  cJSON *p = it->properties_json ? cJSON_Parse(it->properties_json) : NULL;
  cJSON *id = p ? cJSON_GetObjectItem(p, "id") : NULL;
  assert(id && cJSON_IsString(id) && c->posts < 64);
  snprintf(c->ids[c->posts++], 64, "%s", id->valuestring);
  cJSON_Delete(p);
  return 1;
}

static cap_t one_run(const char *tok_in) {
  cap_t c; memset(&c, 0, sizeof c);
  intel_sink s = { .ctx = &c, .emit = cap_emit };
  source_ctx ctx = { .source_id = "twitter-geo", .db = &g_db,
                     .resume_in = tok_in, .resume_out = save_tok };
  g_tok_set = 0;
  assert(run(&ctx, &s) == 0);
  return c;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  assert(dbp && "run.sh sets JO_DB to a scratch database");
  if (db_open(&g_db, dbp, NULL) != 0) return 2;
  sqlite3_exec(g_db.h, "delete from intel_items where record_type='post'",
               NULL, NULL, NULL);

  /* Seven posts; three share one fetched_at, and the slice boundary (3) falls
   * INSIDE that tie, which is the case a fetched_at-only keyset gets wrong. */
  put("m|a", "2026-11-01T00:00:00Z");
  put("m|b", "2026-11-02T00:00:00Z");
  put("m|c", "2026-11-03T00:00:00Z");
  put("m|d", "2026-11-03T00:00:00Z");
  put("m|e", "2026-11-03T00:00:00Z");
  put("m|f", "2026-11-04T00:00:00Z");
  put("m|g", "2026-11-05T00:00:00Z");
  g_tg_batch = 3;

  cap_t r1 = one_run(NULL);
  assert(r1.posts == 3 && !strcmp(r1.ids[0], "a") && !strcmp(r1.ids[2], "c"));
  assert(g_tok_set && !strncmp(g_tok, "k=2026-11-03T00:00:00Z\tm|c", 26));
  assert(r1.notices == 1 && r1.used == 3 && r1.avail == 7);
  printf("  a run reads one slice oldest-first and says what remains: ok\n");

  char t1[512]; snprintf(t1, sizeof t1, "%s", g_tok);
  cap_t r2 = one_run(t1);
  assert(r2.posts == 3 && !strcmp(r2.ids[0], "d") && !strcmp(r2.ids[1], "e")
         && !strcmp(r2.ids[2], "f") && "resumes inside the fetched_at tie");
  assert(r2.notices == 1 && r2.used == 3 && r2.avail == 4);
  printf("  the next run resumes past it, a tie split across the boundary intact: ok\n");

  char t2[512]; snprintf(t2, sizeof t2, "%s", g_tok);
  cap_t r3 = one_run(t2);
  assert(r3.posts == 1 && !strcmp(r3.ids[0], "g") && r3.notices == 0);
  char t3[512]; snprintf(t3, sizeof t3, "%s", g_tok);
  cap_t r4 = one_run(t3);
  assert(r4.posts == 0 && r4.notices == 0 && !g_tok_set && "caught up: position kept");
  put("m|h", "2026-11-06T00:00:00Z");
  cap_t r5 = one_run(t3);
  assert(r5.posts == 1 && !strcmp(r5.ids[0], "h"));
  printf("  caught up, it reads only what arrived since: ok\n");

  cap_t r6 = one_run("p=17");
  assert(r6.posts == 3 && !strcmp(r6.ids[0], "a") && "a foreign token is absent");
  printf("  a token in another shape starts over: ok\n");

  db_close(&g_db);
  printf("test_twitter_geo: all passed\n");
  return 0;
}
