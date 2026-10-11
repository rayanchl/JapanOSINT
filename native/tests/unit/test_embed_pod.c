/* tests/unit/test_embed_pod.c — the embedding backfill pod (core/embed_pod.c),
 * the query side that reads its index (core/semsearchapi.c) and the
 * embedding transport under them (core/llm.c, core/llm_worker.c), against a
 * stub embedding server on loopback.
 *
 * WHAT IS BEING PROTECTED. Every failure this pod has had was SILENT — rc=0,
 * nothing in last_error, an index that simply stopped growing or answered
 * from the wrong space:
 *
 *   1. delta stall: once more rows had been re-fetched unchanged than one
 *      tick's cap, every tick walked the same unchanged rows and a NEW row
 *      whose uid sorted later was never embedded;
 *   3. one input the server rejects (invalid UTF-8, an input it answers with
 *      NaN, an oversize one) failed its whole batch at the same watermark on
 *      every tick, forever;
 *   4. a failed GET /v1/models on the first tick recorded "unknown" as the
 *      model, and every later tick refused the real one;
 *   5. eligibility compared published_at as TEXT, so epoch / dd/mm/yyyy dates
 *      never qualified and RFC 822 dates always did;
 *   6. the query side ranked after a same-dimension model swap the pod had
 *      already refused;
 *  12. a fetched_at in the same second as the pod's watermark was skipped;
 *  13. tenants were told how many OTHER tenants' rows matched;
 *  14. a response repeating an `index` left a zero vector stored as embedded;
 *   2. a request-path embed queued behind a background batch for its whole
 *      duration, whatever its own timeout said;
 *  F4. the tenant filter ran AFTER the KNN, so a tenant's row below k other
 *      tenants' rows was never returned; a re-fetch that moved a row's date
 *      must move its `ts` column without a re-embed; an index built before
 *      the metadata columns must be migrated by copy, never re-embedded and
 *      never renamed;
 *  F7. the post-batch hook sees exactly the committed rows, with metadata.
 *
 * Includes embed_pod.c directly to reach run() and its statics, so run.sh
 * links every object EXCEPT obj/core/embed_pod.o and obj/main.o. */

#include "../../core/embed_pod.c"
#include "../../core/semsearchapi.h"

#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t g_stub = 0;
static int   g_port = 0;
static char  g_ctl[256], g_log[256], g_py[256];

/* Behaviour is read from a JSON control file on EVERY request, so a case can
 * change it between pod ticks without restarting the stub. Every input it is
 * sent is appended to `log`, one JSON string per line, so a case can assert
 * what was (and was not) SENT — the thing the delta cap is about. */
static const char *STUB_PY =
  "import hashlib,json,math,os,time\n"
  "from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer\n"
  "CTL=os.environ['CTL']\n"
  "def ctl():\n"
  " try:\n"
  "  with open(CTL) as f: return json.load(f)\n"
  " except Exception: return {}\n"
  "def vec(t,d):\n"
  " h=hashlib.sha256(t.encode('utf-8','surrogateescape')).digest()\n"
  " v=[((h[i%32]+i)%17)-8+0.5 for i in range(d)]\n"
  " n=math.sqrt(sum(x*x for x in v));return [x/n for x in v]\n"
  "class H(BaseHTTPRequestHandler):\n"
  " def log_message(self,*a):pass\n"
  " def s(self,c,o):\n"
  "  b=json.dumps(o).encode();self.send_response(c)\n"
  "  self.send_header('Content-Type','application/json')\n"
  "  self.send_header('Content-Length',str(len(b)));self.end_headers()\n"
  "  self.wfile.write(b)\n"
  " def do_GET(self):\n"
  "  c=ctl()\n"
  "  if '/v1/models' in self.path:\n"
  "   st=c.get('models_status',200)\n"
  "   if st!=200: return self.s(st,{'error':'down'})\n"
  "   return self.s(200,{'data':[{'id':'/models/'+c.get('model','stub-a.gguf')}]})\n"
  "  self.s(200,{'ok':1})\n"
  " def do_POST(self):\n"
  "  c=ctl();raw=self.rfile.read(int(self.headers.get('Content-Length',0)))\n"
  "  if c.get('delay'): time.sleep(c['delay'])\n"
  "  st=c.get('embed_status',200)\n"
  "  if st!=200: return self.s(st,{'error':'forced'})\n"
  "  try: b=json.loads(raw.decode('utf-8'))\n"
  "  except Exception: return self.s(500,{'error':{'code':500,'message':'[json.exception.parse_error.101] invalid UTF-8'}})\n"
  "  i=b.get('input',[]);i=[i] if isinstance(i,str) else i\n"
  "  lg=c.get('log')\n"
  "  if lg:\n"
  "   with open(lg,'a') as f:\n"
  "    for t in i: f.write(json.dumps(t)+'\\n')\n"
  "  p=c.get('poison')\n"
  "  if p and any(p in t for t in i): return self.s(500,{'error':{'code':500,'message':'input is too large to process'}})\n"
  "  d=c.get('dim',8);nan=c.get('nan')\n"
  "  data=[{'index':k,'embedding':([None]*d if nan and nan in t else vec(t,d))} for k,t in enumerate(i)]\n"
  "  if c.get('dup_index') and len(data)>1: data[1]['index']=0\n"
  "  self.s(200,{'object':'list','data':data,'model':'x'})\n"
  "ThreadingHTTPServer(('127.0.0.1',int(os.environ['P'])),H).serve_forever()\n";

static void ctl_write(const char *extra) {
  FILE *f = fopen(g_ctl, "w");
  assert(f);
  fprintf(f, "{\"log\":\"%s\"%s%s}", g_log, (extra && *extra) ? "," : "",
          extra ? extra : "");
  fclose(f);
}

static void log_reset(void) { FILE *f = fopen(g_log, "w"); if (f) fclose(f); }

/* Inputs sent since log_reset() that contain `sub` (NULL = all), not counting
 * the pod's own dimension probe. */
static int log_count(const char *sub) {
  FILE *f = fopen(g_log, "r");
  if (!f) return 0;
  char line[8192];
  int n = 0;
  while (fgets(line, sizeof line, f)) {
    if (strstr(line, "\"dimension probe\"")) continue;
    if (!sub || strstr(line, sub)) n++;
  }
  fclose(f);
  return n;
}

static void stub_stop(void) {
  if (g_stub > 0) { kill(g_stub, SIGKILL); waitpid(g_stub, NULL, 0); g_stub = 0; }
}

static int answers(int port) {
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
  llm_client c = { .http = NULL, .base_url = url, .interactive = 1 };
  const char *t[1] = { "ping" };
  float *v = NULL; int d = 0; llm_status st;
  int ok = (llm_embed(&c, t, 1, &v, &d, 500, &st) == 0);
  free(v);
  return ok;
}

/* Own the port or fail: see test_service_vec.c's port_busy() for why. */
static int stub_start(int port) {
  stub_stop();
  for (int tries = 0; tries < 40 && answers(port); tries++) port++;
  if (answers(port)) return -1;
  FILE *f = fopen(g_py, "w");
  if (!f) return -1;
  fputs(STUB_PY, f);
  fclose(f);
  pid_t p = fork();
  if (p < 0) return -1;
  if (p == 0) {
    char ps[16];
    snprintf(ps, sizeof ps, "%d", port);
    setenv("P", ps, 1); setenv("CTL", g_ctl, 1);
    if (!freopen("/dev/null", "w", stderr)) { /* stub noise is harmless */ }
    execlp("python3", "python3", g_py, (char *)NULL);
    _exit(127);
  }
  g_stub = p; g_port = port;
  for (int i = 0; i < 100; i++) { if (answers(port)) return 0; usleep(100000); }
  return -1;
}

/* ---- database helpers ---------------------------------------------------- */

static db_handle g_db;

static void sql(const char *q) {
  char *e = NULL;
  if (sqlite3_exec(g_db.h, q, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "SQL failed: %s\n  %s\n", e ? e : "?", q);
    sqlite3_free(e);
    assert(0);
  }
}

static long sql_long(const char *q) {
  sqlite3_stmt *s;
  long v = -1;
  if (sqlite3_prepare_v2(g_db.h, q, -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) v = (long)sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  return v;
}

static void meta_str(const char *k, char *out, size_t cap) { meta_get(&g_db, k, out, cap); }

static void ago(char *buf, size_t n, long long ms_ago) { iso_ms(buf, n, wall_ms() - ms_ago); }

static void insert_row(const char *uid, const char *title, const char *pub,
                       const char *fetched, const char *tenant) {
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(g_db.h,
    "INSERT INTO intel_items(uid,source_id,title,summary,published_at,fetched_at,"
    "record_type,tenant_id) VALUES(?1,'test-src',?2,'summary of '||?1,?3,?4,'news',?5)",
    -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 2, title, -1, SQLITE_STATIC);
  if (pub) sqlite3_bind_text(s, 3, pub, -1, SQLITE_STATIC); else sqlite3_bind_null(s, 3);
  sqlite3_bind_text(s, 4, fetched, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 5, tenant ? tenant : "legacy", -1, SQLITE_STATIC);
  assert(sqlite3_step(s) == SQLITE_DONE);
  sqlite3_finalize(s);
}

/* Wipe the corpus and every trace of the pod. */
static void reset_all(void) {
  sql("DELETE FROM intel_items");
  sql("DROP TABLE IF EXISTS intel_vec");
  sql("DROP TABLE IF EXISTS intel_vec_v2");
  sql("DROP TABLE IF EXISTS intel_vec_done");
  sql("DROP TABLE IF EXISTS intel_vec_failed");
  sql("DROP TABLE IF EXISTS intel_vec_meta");
  log_reset();
}

static int tick(void) {
  http_client *hc = http_client_new();
  volatile int cancel = 0;
  source_ctx ctx = { .source_id = EMBED_SID, .http = hc, .db = &g_db,
                     .cancel = &cancel };
  int rc = run(&ctx, NULL);
  http_client_free(hc);
  return rc;
}

static void url_env(void) {
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", g_port);
  setenv("JO_EMBED_URL", url, 1);
}

/* Run ticks until the full sweep is done (or `max` ticks). */
static void sweep_to_delta(int max) {
  char b[64];
  for (int i = 0; i < max; i++) {
    meta_str("full_done_at", b, sizeof b);
    if (b[0]) return;
    assert(tick() == 0);
  }
  meta_str("full_done_at", b, sizeof b);
  assert(b[0] && "full sweep did not complete");
}

static long done_count(void) { return sql_long("SELECT count(*) FROM intel_vec_done"); }
static long failed_count(void) {
  return table_exists(&g_db, EMBED_FAIL_TABLE)
    ? sql_long("SELECT count(*) FROM intel_vec_failed") : 0;
}

/* ---- pure functions ------------------------------------------------------ */

static void test_utf8_sanitize(void) {
  size_t r = 0;
  char *o = embed_utf8_sanitize("plain ascii", &r);
  assert(o && !strcmp(o, "plain ascii") && r == 0); free(o);
  /* valid 2/3/4-byte sequences survive untouched */
  o = embed_utf8_sanitize("caf\xC3\xA9 \xE6\x9D\xB1\xE4\xBA\xAC \xF0\x9F\x98\x80", &r);
  assert(o && !strcmp(o, "caf\xC3\xA9 \xE6\x9D\xB1\xE4\xBA\xAC \xF0\x9F\x98\x80") && r == 0); free(o);
  /* stray byte, overlong '/', surrogate, > U+10FFFF, truncated tail */
  o = embed_utf8_sanitize("a\xFF" "b", &r);
  assert(!strcmp(o, "a\xEF\xBF\xBD" "b") && r == 1); free(o);
  o = embed_utf8_sanitize("\xC0\xAF", &r);
  assert(!strcmp(o, "\xEF\xBF\xBD\xEF\xBF\xBD") && r == 2); free(o);
  o = embed_utf8_sanitize("\xED\xA0\x80", &r);
  assert(r == 3); free(o);
  o = embed_utf8_sanitize("\xF4\x90\x80\x80", &r);
  assert(r == 4); free(o);
  o = embed_utf8_sanitize("ok\xE6\x9D", &r);
  assert(!strncmp(o, "ok", 2) && r == 2); free(o);
  /* the bound never splits a character and never yields invalid UTF-8 */
  setenv("JO_EMBED_MAX_CHARS", "64", 1);
  char big[200];
  memset(big, 0, sizeof big);
  for (int i = 0; i < 30; i++) memcpy(big + i * 3, "\xE6\x9D\xB1", 3);
  big[0] = (char)0xFF;                     /* and one invalid byte up front */
  o = embed_bound_text(big);
  assert(o && strlen(o) <= 64 && strlen(o) % 3 == 0);
  size_t r2 = 99; char *o2 = embed_utf8_sanitize(o, &r2);
  assert(r2 == 0); free(o2); free(o);
  unsetenv("JO_EMBED_MAX_CHARS");
  printf("  utf8 sanitise + bound: ok\n");
}

static void test_iso_ms_shape(void) {
  char b[40];
  iso_ms(b, sizeof b, 1727000000123LL);
  assert(!strcmp(b, "2024-09-22T10:13:20.123Z"));    /* intel.c's shape */
  assert(parse_iso_ms(b) == 1727000000123LL);
  assert(parse_iso_ms("2024-09-22T10:13:20Z") == 1727000000000LL);
  printf("  timestamps in fetched_at's shape: ok\n");
}

/* ---- 14: a duplicated index is rejected, never a zero vector ------------- */

static void test_duplicate_index(void) {
  url_env();
  ctl_write("\"dup_index\":true");
  llm_client c = { .http = NULL, .base_url = getenv("JO_EMBED_URL"), .interactive = 0 };
  const char *t[2] = { "first", "second" };
  float *v = NULL; int d = 0; llm_status st = LLM_OK;
  assert(llm_embed(&c, t, 2, &v, &d, 5000, &st) != 0);
  assert(v == NULL && st == LLM_ERR_EMPTY);
  ctl_write("");
  assert(llm_embed(&c, t, 2, &v, &d, 5000, &st) == 0 && v && d == 8);
  free(v);
  printf("  duplicated response index rejected (no zero vector): ok\n");
}

/* ---- 4: an undetected model is never recorded ---------------------------- */

static void test_model_detection(void) {
  reset_all();
  url_env();
  char now[40]; ago(now, sizeof now, 5000);
  insert_row("m1", "model test one", NULL, now, NULL);
  insert_row("m2", "model test two", NULL, now, NULL);

  ctl_write("\"models_status\":500");
  assert(tick() != 0);                       /* skipped, and says so */
  char b[600];
  meta_str("model", b, sizeof b);
  assert(b[0] == 0);                         /* nothing recorded — not "unknown" */
  assert(meta_long(&g_db, "dim") == 0);
  assert(done_count() <= 0);
  meta_str("last_error", b, sizeof b);
  assert(strstr(b, "cannot identify the embedding model"));

  ctl_write("");                             /* /v1/models answers again */
  assert(tick() == 0);
  meta_str("model", b, sizeof b);
  assert(!strcmp(b, "stub-a.gguf"));         /* recorded WITH the first write */
  assert(meta_long(&g_db, "dim") == 8);
  assert(done_count() == 2);
  meta_str("refused", b, sizeof b);
  assert(b[0] == 0);

  /* A database an older build left with the placeholder and nothing under
   * it recovers by itself. */
  reset_all();
  insert_row("m3", "model test three", NULL, now, NULL);
  ensure_meta(&g_db);
  meta_set(&g_db, "model", "unknown");
  assert(tick() == 0);
  meta_str("model", b, sizeof b);
  assert(!strcmp(b, "stub-a.gguf") && done_count() == 1);

  /* A real model change is still refused. */
  ctl_write("\"model\":\"stub-other.gguf\"");
  assert(tick() != 0);
  meta_str("refused", b, sizeof b);
  assert(strstr(b, "stub-other.gguf"));
  ctl_write("");
  assert(tick() == 0);
  printf("  model: detection failure skips the tick, nothing recorded; "
         "recorded with the first write; change refused: ok\n");
}

/* ---- 5: eligibility uses a real date ------------------------------------- */

static void test_eligibility_dates(void) {
  reset_all();
  url_env();
  ctl_write("");
  setenv("JO_EMBED_ELIGIBLE_REFRESH_SEC", "0", 1);
  char recent[40], old[40], pub_recent[40], pub_old[40];
  ago(recent, sizeof recent, 3600LL * 1000);
  ago(old, sizeof old, 400LL * 86400 * 1000);
  ago(pub_recent, sizeof pub_recent, 86400LL * 1000);
  ago(pub_old, sizeof pub_old, 400LL * 86400 * 1000);
  insert_row("e1-epoch",   "epoch dated",  "1727000000", recent, NULL);           /* in  */
  insert_row("e2-dmy",     "dmy dated",    "02/10/2026", recent, NULL);           /* in  */
  insert_row("e3-rfc",     "rfc dated",    "Mon, 02 Oct 2000 10:00:00 GMT", recent, NULL); /* in (fetched) */
  insert_row("e4-rfc-old", "rfc old",      "Mon, 02 Oct 2000 10:00:00 GMT", old, NULL);    /* out */
  insert_row("e5-iso",     "iso recent",   pub_recent, recent, NULL);             /* in  */
  insert_row("e6-iso-old", "iso old",      pub_old, recent, NULL);                /* out */
  insert_row("e7-null",    "no published", NULL, recent, NULL);                   /* in  */
  insert_row("e8-epoch-old","epoch old",   "1727000000", old, NULL);              /* out */
  /* out: the pipeline's own synthesis is model output, not a collected row */
  insert_row("e9-run",     "OSINT search: x", pub_recent, recent, NULL);
  sql("UPDATE intel_items SET record_type='osint_search_run' WHERE uid='e9-run'");
  sweep_to_delta(6);
  assert(done_count() == 5);
  assert(sql_long("SELECT count(*) FROM intel_vec_done WHERE uid IN "
                  "('e1-epoch','e2-dmy','e3-rfc','e5-iso','e7-null')") == 5);
  /* coverage counts by the same rule the sweep walks by */
  assert(meta_long(&g_db, "eligible_count") == 5);
  unsetenv("JO_EMBED_ELIGIBLE_REFRESH_SEC");
  printf("  eligibility: epoch/dmy/RFC dates fall back to fetched_at, old ISO "
         "dates excluded, eligible_count agrees (5 of 8): ok\n");
}

/* ---- 1: the delta cannot stall on unchanged re-fetches ------------------- */

static void test_delta_progress(void) {
  reset_all();
  url_env();
  ctl_write("");
  setenv("JO_EMBED_MAX_PER_RUN", "20", 1);
  setenv("JO_EMBED_BATCH", "8", 1);
  char t0[40]; ago(t0, sizeof t0, 600000);
  for (int i = 0; i < 60; i++) {
    char uid[16], title[64];
    snprintf(uid, sizeof uid, "a%03d", i);
    snprintf(title, sizeof title, "steady row %d", i);
    insert_row(uid, title, NULL, t0, NULL);
  }
  sweep_to_delta(8);
  assert(done_count() == 60);

  /* All 60 re-fetched with IDENTICAL text — three times the per-tick cap —
   * and three new rows whose uids sort after every one of them. */
  char now[40]; ago(now, sizeof now, 0);
  char q[256];
  snprintf(q, sizeof q, "UPDATE intel_items SET fetched_at='%s'", now);
  sql(q);
  insert_row("zzz1", "brand new one", NULL, now, NULL);
  insert_row("zzz2", "brand new two", NULL, now, NULL);
  insert_row("zzz3", "brand new three", NULL, now, NULL);
  log_reset();
  assert(tick() == 0);
  assert(done_count() == 63);               /* the new rows ARE embedded */
  assert(log_count(NULL) == 3);             /* and only they were sent */
  assert(log_count("steady row") == 0);
  char lt[400];
  meta_str("last_tick", lt, sizeof lt);
  assert(strstr(lt, "\"unchanged\":60"));
  assert(strstr(lt, "\"sent\":3"));

  /* The next tick sends nothing at all: unchanged rows do not come back as
   * work, and do not eat the cap. */
  log_reset();
  assert(tick() == 0);
  assert(log_count(NULL) == 0);

  /* A changed text IS re-embedded, and only that row. */
  ago(now, sizeof now, 0);
  snprintf(q, sizeof q, "UPDATE intel_items SET title='steady row 7 (edited)', "
                        "fetched_at='%s' WHERE uid='a007'", now);
  sql(q);
  log_reset();
  assert(tick() == 0);
  assert(log_count(NULL) == 1 && log_count("edited") == 1);

  /* 12: a row stamped in the very millisecond of the last tick's start is
   * still looked at (the old `…:SSZ` mark compared greater than `…:SS.mmmZ`). */
  char last[64];
  meta_str("last_run_at", last, sizeof last);
  assert(strlen(last) == 24 && last[19] == '.');     /* fetched_at's shape */
  insert_row("same-instant", "stamped at the mark", NULL, last, NULL);
  log_reset();
  assert(tick() == 0);
  assert(sql_long("SELECT count(*) FROM intel_vec_done WHERE uid='same-instant'") == 1);

  /* The cap now counts SENT rows: 45 new rows at a cap of 20 drain in three
   * ticks, each sending at most 20. */
  ago(now, sizeof now, 0);
  for (int i = 0; i < 45; i++) {
    char uid[16]; snprintf(uid, sizeof uid, "n%03d", i);
    char title[64]; snprintf(title, sizeof title, "fresh row %d", i);
    insert_row(uid, title, NULL, now, NULL);
  }
  for (int k = 0; k < 3; k++) {
    log_reset();
    assert(tick() == 0);
    assert(log_count(NULL) <= 20);
  }
  assert(sql_long("SELECT count(*) FROM intel_vec_done WHERE uid LIKE 'n%'") == 45);
  unsetenv("JO_EMBED_MAX_PER_RUN");
  unsetenv("JO_EMBED_BATCH");
  printf("  delta: 60 unchanged re-fetches (3x the cap) cost no sends, new rows "
         "embedded in one tick, edits re-embedded, same-millisecond row seen, "
         "cap counts sends: ok\n");
}

/* ---- 3: one bad row cannot stop the sweep -------------------------------- */

static void test_bad_rows(void) {
  reset_all();
  url_env();
  ctl_write("\"poison\":\"POISON\",\"nan\":\"NANME\"");
  char now[40]; ago(now, sizeof now, 30000);
  for (int i = 0; i < 10; i++) {
    char uid[16], title[64];
    snprintf(uid, sizeof uid, "g%02d", i);
    snprintf(title, sizeof title, "good row %d", i);
    insert_row(uid, title, NULL, now, NULL);
  }
  insert_row("p-poison", "this one carries POISON", NULL, now, NULL);
  insert_row("p-nan", "this one says NANME", NULL, now, NULL);
  /* invalid UTF-8 straight from a sink: A <FF> B */
  char q[512];
  snprintf(q, sizeof q, "INSERT INTO intel_items(uid,source_id,title,summary,fetched_at,"
           "record_type) VALUES('p-utf8','test-src',CAST(X'41FF42' AS TEXT),'bytes','%s','news')", now);
  sql(q);

  assert(tick() == 0);                      /* the tick completes */
  assert(done_count() == 11);               /* 10 good + the sanitised one */
  assert(sql_long("SELECT count(*) FROM intel_vec_done WHERE uid='p-utf8'") == 1);
  assert(failed_count() == 2);
  assert(sql_long("SELECT count(*) FROM intel_vec_failed WHERE uid IN ('p-poison','p-nan')") == 2);
  cJSON *cov = embed_coverage_json(&g_db);  /* in-band, not a log line */
  assert(cJSON_GetObjectItem(cov, "failed_count")->valuedouble == 2);
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(cov, "failed_sample")) == 2);
  cJSON *f0 = cJSON_GetArrayItem(cJSON_GetObjectItem(cov, "failed_sample"), 0);
  assert(strstr(cJSON_GetObjectItem(f0, "reason")->valuestring, "refused"));
  cJSON_Delete(cov);
  sweep_to_delta(4);

  /* Re-fetched UNCHANGED, a failed row is skipped, not re-sent. */
  ago(now, sizeof now, 0);
  snprintf(q, sizeof q, "UPDATE intel_items SET fetched_at='%s' WHERE uid IN ('p-poison','p-nan')", now);
  sql(q);
  log_reset();
  assert(tick() == 0);
  assert(log_count("POISON") == 0 && log_count("NANME") == 0);
  char lt[400];
  meta_str("last_tick", lt, sizeof lt);
  assert(strstr(lt, "\"failed_skipped\":2"));

  /* Its text changes: it is tried again, and leaves the failed list. */
  ago(now, sizeof now, 0);
  snprintf(q, sizeof q, "UPDATE intel_items SET title='this one is fine now', fetched_at='%s' "
                        "WHERE uid='p-poison'", now);
  sql(q);
  assert(tick() == 0);
  assert(failed_count() == 1);
  assert(sql_long("SELECT count(*) FROM intel_vec_done WHERE uid='p-poison'") == 1);

  /* A server that refuses EVERYTHING is a server fault, not a corpus of bad
   * rows: nothing is recorded as failed, the reason is in last_error. */
  reset_all();
  insert_row("h1", "healthy text one", NULL, now, NULL);
  insert_row("h2", "healthy text two", NULL, now, NULL);
  ctl_write("\"embed_status\":500");
  assert(tick() != 0);
  assert(failed_count() == 0 && done_count() == 0);
  char b[600];
  meta_str("last_error", b, sizeof b);
  assert(strstr(b, "server fault"));
  ctl_write("");
  assert(tick() == 0 && done_count() == 2);
  printf("  bad rows: invalid UTF-8 sanitised and embedded, POISON/NaN rows "
         "recorded (count + sample in coverage) and skipped until edited, a "
         "refuse-everything server records nothing: ok\n");
}

/* ---- 7 (part): a vec row with no done row no longer wedges the pod ------- */

static void test_orphan_vec_row(void) {
  reset_all();
  url_env();
  ctl_write("");
  char now[40]; ago(now, sizeof now, 30000);
  insert_row("o1", "orphan case", NULL, now, NULL);
  assert(tick() == 0 && done_count() == 1);
  /* Simulate the half-write the unchecked BEGIN allowed: vec row, no done. */
  sql("DELETE FROM intel_vec_done WHERE uid='o1'");
  sql("DELETE FROM intel_vec_meta WHERE k IN ('full_done_at','wm_p','wm_u','wm_phase',"
      "'sweep_start','dwm_f','dwm_u')");
  assert(tick() == 0);                      /* was: UNIQUE constraint failed, forever */
  assert(done_count() == 1);
  char b[600];
  meta_str("last_error", b, sizeof b);
  assert(b[0] == 0);
  printf("  vec row without its done row is replaced, not a permanent "
         "UNIQUE failure: ok\n");
}

/* ---- 6 + 13: the query side ---------------------------------------------- */

static void test_query_side(void) {
  reset_all();
  url_env();
  ctl_write("");
  setenv("JO_EMBED_MODEL_CHECK_TTL_MS", "0", 1);
  char now[40]; ago(now, sizeof now, 30000);
  insert_row("q-mine", "harbour inspection report", NULL, now, "t-mine");
  insert_row("q-other", "harbour inspection report", NULL, now, "t-other");
  assert(tick() == 0 && done_count() == 2);

  int st = 0;
  char *b = semsearchapi_query(&g_db, "t-mine", "harbour inspection report",
                               "vector", 10, 50, 0, &st);
  assert(st == 200 && b);
  assert(strstr(b, "q-mine") && !strstr(b, "q-other"));
  assert(!strstr(b, "tenant_withheld"));    /* 13: not told about others */
  free(b);
  b = semsearchapi_query(&g_db, "t-mine", "harbour inspection report",
                         "vector", 10, 50, 1, &st);
  assert(st == 200 && strstr(b, "\"tenant_withheld\":1"));   /* operator is */
  free(b);

  /* 6: same dimension, different model — 503, not a 200 with meaningless
   * rankings. */
  ctl_write("\"model\":\"stub-swapped.gguf\"");
  b = semsearchapi_query(&g_db, NULL, "harbour", "vector", 10, 50, 0, &st);
  assert(st == 503 && strstr(b, "stub-swapped.gguf") && strstr(b, "coverage"));
  free(b);
  ctl_write("");
  /* and a refused index is not queried either */
  meta_set(&g_db, "refused", "test: refused");
  b = semsearchapi_query(&g_db, NULL, "harbour", "vector", 10, 50, 0, &st);
  assert(st == 503 && strstr(b, "test: refused"));
  free(b);
  meta_set(&g_db, "refused", NULL);
  b = semsearchapi_query(&g_db, NULL, "harbour", "vector", 10, 50, 0, &st);
  assert(st == 200);
  free(b);
  unsetenv("JO_EMBED_MODEL_CHECK_TTL_MS");
  printf("  query: model swap and refused index answer 503 with coverage; "
         "tenant_withheld only for operators: ok\n");
}

/* ---- F4: metadata inside the KNN ----------------------------------------- */

/* A tenant whose row sits below k OTHER tenants' identical rows used to get
 * nothing: the KNN ran across every tenant and the tenant filter ran after
 * it. With tenant_id as the partition key the KNN only reads what the caller
 * may see. */
static void test_partitioned_recall(void) {
  reset_all();
  url_env();
  ctl_write("");
  setenv("JO_EMBED_MODEL_CHECK_TTL_MS", "0", 1);
  char now[40]; ago(now, sizeof now, 30000);
  /* identical text = identical vector = equal distance, and the uids of the
   * other tenant's rows sort first, so a global top-3 is all theirs */
  insert_row("a-other-1", "vessel tracking notice", NULL, now, "t-other");
  insert_row("a-other-2", "vessel tracking notice", NULL, now, "t-other");
  insert_row("a-other-3", "vessel tracking notice", NULL, now, "t-other");
  insert_row("z-mine",    "vessel tracking notice", NULL, now, "t-mine");
  assert(tick() == 0 && done_count() == 4);
  assert(sql_long("SELECT count(*) FROM intel_vec_v2 WHERE tenant_id='t-other'") == 3);
  assert(sql_long("SELECT count(*) FROM intel_vec_v2 WHERE tenant_id='t-mine' "
                  "AND source_id='test-src' AND record_type='news' AND ts > 0") == 1);
  int st = 0;
  char *b = semsearchapi_query(&g_db, "t-mine", "vessel tracking notice",
                               "vector", 10, 3, 0, &st);
  assert(st == 200 && strstr(b, "z-mine") && !strstr(b, "a-other"));
  assert(strstr(b, "\"knn_partitioned\":true"));
  free(b);
  /* the operator is still told what the partition kept out */
  b = semsearchapi_query(&g_db, "t-mine", "vessel tracking notice", "vector",
                         10, 3, 1, &st);
  assert(st == 200 && strstr(b, "z-mine"));
  /* the unfiltered top 3 holds at least two of the other tenant's three
   * (four rows tie at distance 0; which three vec0 returns is its choice) */
  cJSON *j = cJSON_Parse(b);
  cJSON *w = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "meta"), "tenant_withheld");
  assert(w && w->valuedouble >= 2);
  cJSON_Delete(j);
  free(b);
  unsetenv("JO_EMBED_MODEL_CHECK_TTL_MS");
  printf("  partitioned KNN: a tenant's row below k other-tenant rows is "
         "found; operator still told what was withheld: ok\n");
}

/* Same text, moved date: the ts column follows without a re-embed. */
static void test_metadata_rewrite(void) {
  reset_all();
  url_env();
  ctl_write("");
  char now[40], pub1[40], pub2[40];
  ago(now, sizeof now, 30000);
  ago(pub1, sizeof pub1, 5LL * 86400 * 1000);
  ago(pub2, sizeof pub2, 2LL * 86400 * 1000);
  insert_row("md1", "date moves under me", pub1, now, NULL);
  sweep_to_delta(4);
  long ts1 = sql_long("SELECT ts FROM intel_vec_v2 WHERE uid='md1'");
  assert(ts1 == parse_iso_ms(pub1) / 1000);
  char q[256];
  ago(now, sizeof now, 0);
  snprintf(q, sizeof q, "UPDATE intel_items SET published_at='%s', fetched_at='%s' "
           "WHERE uid='md1'", pub2, now);
  sql(q);
  log_reset();
  assert(tick() == 0);
  assert(log_count(NULL) == 0);                       /* nothing re-embedded */
  assert(sql_long("SELECT ts FROM intel_vec_v2 WHERE uid='md1'") == parse_iso_ms(pub2) / 1000);
  char lt[440];
  meta_str("last_tick", lt, sizeof lt);
  assert(strstr(lt, "\"rewritten\":1"));
  printf("  metadata-only change rewritten from the stored vector, no embed: ok\n");
}

/* An index built before metadata columns is copied, not re-embedded. */
static void test_legacy_migration(void) {
  reset_all();
  url_env();
  ctl_write("");
  char now[40]; ago(now, sizeof now, 30000);
  insert_row("L1", "legacy row one", NULL, now, NULL);
  insert_row("L2", "legacy row two", NULL, now, "t-x");
  insert_row("L3", "legacy row three, vector lost", NULL, now, NULL);
  /* the old layout, as an older build left it */
  sql("CREATE VIRTUAL TABLE intel_vec USING vec0(uid TEXT PRIMARY KEY, embedding float[8])");
  sql("INSERT INTO intel_vec(uid, embedding) VALUES "
      "('L1','[1,0,0,0,0,0,0,0]'),('L2','[0,1,0,0,0,0,0,0]'),"
      "('GONE','[0,0,1,0,0,0,0,0]')");
  ensure_meta(&g_db);
  vecindex_ensure_side_tables(&g_db, SPEC);
  sql("ALTER TABLE intel_vec_done DROP COLUMN meta_hash");   /* the old shape */
  /* the hashes the pod itself would have stored, so the copied rows read as
   * unchanged afterwards and a send of them would be a real regression */
  char *t1 = compose_text("legacy row one", "summary of L1", NULL);
  char *t2 = compose_text("legacy row two", "summary of L2", NULL);
  char ins[700];
  snprintf(ins, sizeof ins, "INSERT INTO intel_vec_done(uid,text_hash,embedded_at) VALUES "
           "('L1',%lld,'%s'),('L2',%lld,'%s'),('L3',3,'%s'),('GONE',4,'%s')",
           (long long)vecindex_hash(t1), now, (long long)vecindex_hash(t2), now, now, now);
  free(t1); free(t2);
  sql(ins);
  meta_set(&g_db, "model", "stub-a.gguf");
  meta_set(&g_db, "dim", "8");
  meta_set(&g_db, "full_done_at", now);
  /* before the tick, queries read the legacy table, unfiltered */
  assert(embed_query_spec(&g_db) == &legacy_spec);

  log_reset();
  assert(tick() == 0);
  assert(!table_exists(&g_db, "intel_vec"));                  /* dropped */
  assert(embed_query_spec(&g_db) == SPEC);
  assert(sql_long("SELECT count(*) FROM intel_vec_v2 WHERE uid IN ('L1','L2')") == 2);
  assert(sql_long("SELECT count(*) FROM intel_vec_v2 WHERE uid='L2' AND tenant_id='t-x'") == 1);
  assert(sql_long("SELECT count(*) FROM intel_vec_done WHERE uid='GONE'") == 0);
  float *v = NULL; int d = 0;
  assert(vecindex_load(&g_db, SPEC, "L1", &v, &d) == 0 && d == 8 && v[0] == 1.0f);
  free(v);
  /* the copied vectors were NOT sent; the one whose vector was lost was */
  assert(log_count("legacy row one") == 0 && log_count("legacy row two") == 0);
  assert(meta_long(&g_db, "migrate_copied") == 2);
  assert(meta_long(&g_db, "migrate_orphans") == 1);
  assert(meta_long(&g_db, "migrate_missing") == 1);
  /* L3 was eligible and fetched after the delta watermark: re-embedded in the
   * walk that followed, or by the next tick */
  if (sql_long("SELECT count(*) FROM intel_vec_done WHERE uid='L3'") != 1) assert(tick() == 0);
  assert(sql_long("SELECT count(*) FROM intel_vec_done WHERE uid='L3'") == 1);
  cJSON *cov = embed_coverage_json(&g_db);
  cJSON *m = cJSON_GetObjectItem(cov, "migration");
  assert(m && !strcmp(cJSON_GetObjectItem(m, "state")->valuestring, "done"));
  assert(cJSON_IsTrue(cJSON_GetObjectItem(cov, "knn_filters")));
  cJSON_Delete(cov);
  printf("  legacy intel_vec migrated by copy (2 copied, 1 orphan dropped, 1 "
         "lost vector re-embedded), old table dropped, never renamed: ok\n");
}

/* ---- F7: the post-batch hook ---------------------------------------------- */

static int  g_hk_rows = 0, g_hk_dim = 0, g_hk_calls = 0;
static char g_hk_tenant[32], g_hk_uid[32];
static long long g_hk_ts = 0;
static void test_hook_fn(db_handle *db, int n, const char *const *uids,
                         const float *vectors, int dim, const embed_row_meta *meta,
                         void *user) {
  assert(user == &g_hk_calls && db && vectors);
  g_hk_calls++;
  g_hk_rows += n;
  g_hk_dim = dim;
  for (int i = 0; i < n; i++) {
    assert(!strcmp(meta[i].uid, uids[i]));
    if (!strcmp(uids[i], "hk-2")) {
      snprintf(g_hk_tenant, sizeof g_hk_tenant, "%s", meta[i].tenant_id);
      snprintf(g_hk_uid, sizeof g_hk_uid, "%s", meta[i].record_type);
      g_hk_ts = meta[i].ts;
    }
  }
  /* it may write in its own small transaction */
  assert(sqlite3_exec(db->h, "BEGIN IMMEDIATE; CREATE TABLE IF NOT EXISTS hook_seen(n INT);"
                             "INSERT INTO hook_seen VALUES(1); COMMIT;", NULL, NULL, NULL) == SQLITE_OK);
}

static void test_post_batch_hook(void) {
  reset_all();
  url_env();
  ctl_write("");
  sql("DROP TABLE IF EXISTS hook_seen");
  assert(embed_hook_register(test_hook_fn, &g_hk_calls) == 0);
  char now[40]; ago(now, sizeof now, 30000);
  insert_row("hk-1", "hook row one", NULL, now, NULL);
  insert_row("hk-2", "hook row two", NULL, now, "t-hook");
  insert_row("hk-3", "hook row three", NULL, now, NULL);
  log_reset();
  assert(tick() == 0);
  assert(g_hk_rows == 3 && g_hk_dim == 8 && g_hk_calls >= 1);
  assert(!strcmp(g_hk_tenant, "t-hook") && !strcmp(g_hk_uid, "news"));
  assert(g_hk_ts == parse_iso_ms(now) / 1000);
  assert(sql_long("SELECT count(*) FROM hook_seen") == g_hk_calls);
  /* the hook never called the server: only the three rows were sent */
  assert(log_count(NULL) == 3);
  /* an unchanged tick delivers nothing */
  assert(tick() == 0 && g_hk_rows == 3);
  printf("  post-batch hook: 3 rows with tenant/source/ts/record_type after "
         "commit, own transaction allowed, unchanged rows not redelivered: ok\n");
}

/* ---- 2: a bounded caller does not wait out somebody else's batch --------- */

typedef struct { int rc; } bg_res;
static void *bg_embed(void *vp) {
  bg_res *r = vp;
  llm_client c = { .http = NULL, .base_url = getenv("JO_EMBED_URL"), .interactive = 0 };
  const char *t[1] = { "slow background batch" };
  float *v = NULL; int d = 0; llm_status st;
  r->rc = llm_embed(&c, t, 1, &v, &d, 20000, &st);
  free(v);
  return NULL;
}

static void test_bounded_queue_wait(void) {
  url_env();
  ctl_write("\"delay\":3");
  bg_res r = { -9 };
  pthread_t th;
  assert(pthread_create(&th, NULL, bg_embed, &r) == 0);
  usleep(300000);                           /* the background job is in flight */
  llm_client c = { .http = NULL, .base_url = getenv("JO_EMBED_URL"),
                   .interactive = 1, .bound_queue_wait = 1 };
  const char *t[1] = { "user query" };
  float *v = NULL; int d = 0; llm_status st = LLM_OK;
  long long t0 = wall_ms();
  int rc = llm_embed(&c, t, 1, &v, &d, 800, &st);
  long long took = wall_ms() - t0;
  /* LLM_ERR_QUEUE_TIMEOUT, not LLM_ERR_TIMEOUT: it was never sent, and
   * calling that a timeout tells an operator the model is slow. */
  assert(rc != 0 && st == LLM_ERR_QUEUE_TIMEOUT);
  assert(took < 1500);                      /* not the ~2.7 s left of the batch */
  pthread_join(th, NULL);
  assert(r.rc == 0);                        /* the background job was unharmed */
  ctl_write("");
  /* and the worker still serves after a withdrawal */
  assert(llm_embed(&c, t, 1, &v, &d, 5000, &st) == 0);
  free(v);
  printf("  bounded queue wait: gave up after %lld ms behind a 3 s batch, "
         "worker intact: ok\n", took);
}

/* ---- F6: the query slot (JO_EMBED_QUERY_URL) ----------------------------- */

/* A second stub on its own control file, so it can serve a different model. */
static pid_t stub2_start(int port, const char *ctl) {
  for (int tries = 0; tries < 40 && answers(port); tries++) port++;
  pid_t p = fork();
  if (p < 0) return -1;
  if (p == 0) {
    char ps[16];
    snprintf(ps, sizeof ps, "%d", port);
    setenv("P", ps, 1); setenv("CTL", ctl, 1);
    if (!freopen("/dev/null", "w", stderr)) { /* stub noise is harmless */ }
    execlp("python3", "python3", g_py, (char *)NULL);
    _exit(127);
  }
  for (int i = 0; i < 100; i++) {
    if (answers(port)) { char u[64]; snprintf(u, sizeof u, "%d", port); setenv("STUB2_PORT", u, 1); return p; }
    usleep(100000);
  }
  kill(p, SIGKILL); waitpid(p, NULL, 0);
  return -1;
}

static void test_query_slot(void) {
  url_env();
  setenv("JO_EMBED_MODEL_CHECK_TTL_MS", "0", 1);
  char live[600];
  const char *base = getenv("JO_EMBED_URL");

  /* unset: queries share JO_EMBED_URL */
  unsetenv("JO_EMBED_QUERY_URL");
  assert(!strcmp(embed_query_url(), base));
  assert(embed_live_query_model(live, sizeof live) == 0);

  /* the same server under a second spelling (launch.sh's --parallel 2 slot):
   * same model by construction, and a worker of its own — so a query does
   * not wait behind a background batch on JO_EMBED_URL's worker. */
  char q[160];
  snprintf(q, sizeof q, "%s/", base);
  setenv("JO_EMBED_QUERY_URL", q, 1);
  assert(!strcmp(embed_query_url(), q));
  assert(embed_live_query_model(live, sizeof live) == 0);
  ctl_write("\"delay\":2");
  bg_res r = { -9 };
  pthread_t th;
  assert(pthread_create(&th, NULL, bg_embed, &r) == 0);
  usleep(300000);                           /* the batch holds JO_EMBED_URL's worker */
  llm_client c = { .http = NULL, .base_url = embed_query_url(),
                   .interactive = 1, .bound_queue_wait = 1 };
  const char *t[1] = { "user query" };
  float *v = NULL; int d = 0; llm_status st = LLM_OK;
  long long t0 = wall_ms();
  int rc = llm_embed(&c, t, 1, &v, &d, 3500, &st);
  long long took = wall_ms() - t0;
  free(v);
  /* queued behind the batch it would need ~1.7 s + 2 s > its 3.5 s budget */
  assert(rc == 0 && took < 3000);
  pthread_join(th, NULL);
  assert(r.rc == 0);
  ctl_write("");

  /* nothing answering on the query URL: cannot verify, refuse */
  setenv("JO_EMBED_QUERY_URL", "http://127.0.0.1:1", 1);
  assert(embed_live_query_model(live, sizeof live) != 0);
  assert(strstr(live, "JO_EMBED_QUERY_URL"));

  /* a second instance serving a DIFFERENT model: refuse, even though
   * JO_EMBED_MODEL pins a name (the pin says what the index was built with;
   * it cannot vouch for what a second server serves) */
  char ctl2[300];
  snprintf(ctl2, sizeof ctl2, "%s.2", g_ctl);
  FILE *f = fopen(ctl2, "w");
  assert(f);
  fputs("{\"model\":\"other-model.gguf\"}", f);
  fclose(f);
  pid_t p2 = stub2_start(g_port + 50, ctl2);
  if (p2 > 0) {
    char u2[64];
    snprintf(u2, sizeof u2, "http://127.0.0.1:%s", getenv("STUB2_PORT"));
    setenv("JO_EMBED_QUERY_URL", u2, 1);
    assert(embed_live_query_model(live, sizeof live) != 0);
    assert(strstr(live, "other-model.gguf") && strstr(live, "refused"));
    setenv("JO_EMBED_MODEL", "stub-a.gguf", 1);
    assert(embed_live_query_model(live, sizeof live) != 0);
    unsetenv("JO_EMBED_MODEL");
    kill(p2, SIGKILL); waitpid(p2, NULL, 0);
  } else printf("  (second stub unavailable: model-mismatch case skipped)\n");
  unsetenv("JO_EMBED_QUERY_URL");
  unsetenv("JO_EMBED_MODEL_CHECK_TTL_MS");
  printf("  query slot: own worker (%lld ms beside a 2 s batch), unverifiable "
         "or different model refused: ok\n", took);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  assert(dbp && "run.sh sets JO_DB to a scratch database");
  if (db_open(&g_db, dbp, NULL) != 0) {
    fprintf(stderr, "test_embed_pod: cannot open db\n");
    return 2;
  }
  snprintf(g_ctl, sizeof g_ctl, "%s.ctl.json", dbp);
  snprintf(g_log, sizeof g_log, "%s.inputs.log", dbp);
  snprintf(g_py, sizeof g_py, "%s.stub.py", dbp);
  unsetenv("JO_EMBED_MODEL");
  unsetenv("JO_EMBED_RECORD_TYPES");

  test_utf8_sanitize();
  test_iso_ms_shape();

  ctl_write("");
  if (stub_start(18190) != 0) {
    printf("test_embed_pod: stub unavailable, server-backed cases skipped\n");
    stub_stop();
    db_close(&g_db);
    return 0;
  }
  test_duplicate_index();
  test_model_detection();
  test_eligibility_dates();
  test_delta_progress();
  test_bad_rows();
  test_orphan_vec_row();
  test_query_side();
  test_partitioned_recall();
  test_metadata_rewrite();
  test_legacy_migration();
  test_post_batch_hook();
  test_bounded_queue_wait();
  test_query_slot();

  stub_stop();
  db_close(&g_db);
  printf("test_embed_pod: ok\n");
  return 0;
}
