/* tests/unit/test_service_vec.c — semantic service routing (core/service_vec.c)
 * against a stub embedding server, with no model and no network beyond
 * loopback.
 *
 * WHAT IS BEING PROTECTED. This module decides WHICH services a
 * natural-language query is routed to. Every way it can fail is quiet:
 *
 *   1. It must be completely INERT with no JO_EMBED_URL — no table created, no
 *      query attempted, NULL returned so the caller keeps the registry-order
 *      path. A module that half-activates would replace a bounded-but-honest
 *      menu with an empty one.
 *   2. A build must be all-or-nothing. A partial index answers confidently
 *      from whatever fraction embedded before the failure, which is the
 *      "confident wrong answer" rule 4d is about.
 *   3. The catalogue must STATE that it is a subset (rule 2: a bounded view
 *      announces its bound) and must carry descriptions — dropping them is the
 *      thing the registry-order path had to do and this exists to avoid.
 *   4. The ids handed back must be exactly the ids listed, because the caller
 *      builds the model's permitted enum from them. Briefing and vocabulary
 *      diverging means the model reads about a service it cannot name.
 *   5. Ranking must actually depend on the query — a router that returns the
 *      same K for every question is registry order wearing a hat.
 *   6. A model or dimension change must REBUILD rather than compare vectors
 *      across two embedding spaces (embed_pod.h's rule, restated here).
 *
 * The stub server is a hashed bag-of-words, not a model: it makes ranking
 * DETERMINISTIC and query-dependent, which is what these invariants need. It
 * says nothing about embedding quality and is not claimed to.
 *
 * Includes service_vec.c directly to reach its statics, so run.sh links every
 * object EXCEPT obj/core/service_vec.o and obj/main.o. */

#include "../../core/service_vec.c"

#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t g_stub = 0;
static int   g_port = 0;

/* The stub, inline, so the test needs no external file. */
static const char *STUB_PY =
  "import hashlib,json,os,re\n"
  "from http.server import BaseHTTPRequestHandler,HTTPServer\n"
  "D=int(os.environ.get('D','64'));M=os.environ.get('M','stub-a')\n"
  "def vec(t):\n"
  " v=[0.0]*D\n"
  " for w in re.findall(r'[a-z0-9]+',(t or '').lower()):\n"
  "  v[int(hashlib.sha1(w.encode()).hexdigest()[:8],16)%D]+=1.0\n"
  " n=sum(x*x for x in v)**0.5\n"
  " return [x/n for x in v] if n else v\n"
  "class H(BaseHTTPRequestHandler):\n"
  " def s(self,o,c=200):\n"
  "  b=json.dumps(o).encode();self.send_response(c)\n"
  "  self.send_header('Content-Type','application/json')\n"
  "  self.send_header('Content-Length',str(len(b)));self.end_headers()\n"
  "  self.wfile.write(b)\n"
  " def do_GET(self):\n"
  "  self.s({'data':[{'id':M}]}) if '/v1/models' in self.path else self.s({'ok':1})\n"
  " def do_POST(self):\n"
  "  n=int(self.headers.get('Content-Length',0))\n"
  "  b=json.loads(self.rfile.read(n) or b'{}');i=b.get('input',[])\n"
  "  i=[i] if isinstance(i,str) else i\n"
  "  self.s({'data':[{'index':k,'embedding':vec(t)} for k,t in enumerate(i)]})\n"
  " def log_message(self,*a):pass\n"
  "HTTPServer(('127.0.0.1',int(os.environ['P'])),H).serve_forever()\n";

static void stub_stop(void) {
  if (g_stub > 0) { kill(g_stub, SIGKILL); waitpid(g_stub, NULL, 0); g_stub = 0; }
}

/* Is anything already answering on `port`? Used to refuse a port we do not own.
 *
 * WHY THIS EXISTS. The stub used to bind a FIXED 18099. When a run of this
 * test aborted (an assert, or TSan halting it) its python child outlived the
 * process, and the next run's stub then failed to bind while the poll below
 * happily succeeded against the ORPHAN — a server nobody could stop, possibly
 * left at a different dimension by the test case that had crashed. The visible
 * symptoms were "indexed 1838 of 1846" and a `failed build` case that passed
 * because the stub it thought it had killed was still answering. Both are
 * environment, not product, and both look exactly like a real regression at
 * 2 a.m. Own the port or fail. */
static int port_busy(int port) {
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
  llm_client c = { .http = NULL, .base_url = url, .interactive = 1 };
  const char *t[1] = { "ping" };
  float *v = NULL; int d = 0; llm_status st;
  int ok = (llm_embed(&c, t, 1, &v, &d, 500, &st) == 0);
  free(v);
  return ok;
}

/* Start the stub on `port` with dimension `dim` and model name `model`.
 * `port` is a starting point: if it is occupied the next free one is used, so
 * two runs of this suite (or one run plus a leaked child) cannot collide. */
static int stub_start(int port, int dim, const char *model) {
  stub_stop();
  for (int tries = 0; tries < 40 && port_busy(port); tries++) port++;
  if (port_busy(port)) return -1;                 /* nothing free nearby */
  char path[256];
  snprintf(path, sizeof path, "/tmp/jo_svec_stub_%d.py", (int)getpid());
  FILE *f = fopen(path, "w");
  if (!f) return -1;
  fputs(STUB_PY, f);
  fclose(f);
  pid_t p = fork();
  if (p < 0) return -1;
  if (p == 0) {
    char ds[16], ps[16];
    snprintf(ds, sizeof ds, "%d", dim);
    snprintf(ps, sizeof ps, "%d", port);
    setenv("D", ds, 1); setenv("P", ps, 1); setenv("M", model, 1);
    if (!freopen("/dev/null", "w", stderr)) { /* stub noise is harmless */ }
    execlp("python3", "python3", path, (char *)NULL);
    _exit(127);
  }
  g_stub = p; g_port = port;
  /* Wait for it to answer rather than sleeping a guess. */
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
  for (int i = 0; i < 100; i++) {
    llm_client c = { .http = NULL, .base_url = url, .interactive = 1 };
    const char *t[1] = { "ping" };
    float *v = NULL; int d = 0; llm_status st;
    if (llm_embed(&c, t, 1, &v, &d, 2000, &st) == 0 && d == dim) { free(v); return 0; }
    free(v);
    usleep(100000);
  }
  return -1;
}

static void use_stub(int port) {
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
  setenv("JO_EMBED_URL", url, 1);
}

/* 1: inert with no embedding server. */
static void test_inert(db_handle *db) {
  unsetenv("JO_EMBED_URL");
  assert(service_vec_build(db) == 0);
  osint_catalogue_note n = {0};
  char **ids = NULL; int cnt = -1;
  char *cat = service_vec_catalogue(db, "who owns example.com", 0, &n, &ids, &cnt);
  assert(cat == NULL);          /* caller must fall back */
  assert(ids == NULL && cnt == 0);
  /* and nothing was created on the way */
  sqlite3_stmt *s = NULL;
  int found = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT count(*) FROM sqlite_master WHERE name LIKE 'service_vec%'",
        -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) found = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
  }
  assert(found == 0);
  printf("  inert without JO_EMBED_URL: ok\n");
}

/* 2 + 3 + 4: build, then a catalogue that discloses its bound, keeps
 * descriptions, and hands back exactly the ids it listed. */
static void test_build_and_catalogue(db_handle *db) {
  use_stub(g_port);
  int n = service_vec_build(db);
  assert(n > 0);
  printf("  indexed %d entity-pivot services\n", n);

  /* idempotent: an unchanged registry must not re-embed */
  int again = service_vec_build(db);
  assert(again == n);

  osint_catalogue_note note = {0};
  char **ids = NULL; int cnt = 0;
  char *cat = service_vec_catalogue(db, "who owns this domain name registrar whois",
                                    12, &note, &ids, &cnt);
  assert(cat != NULL);
  assert(cnt > 0 && cnt <= 12);
  assert(note.shown == cnt);
  assert(note.total >= note.shown);
  assert(note.descriptions == 1);            /* descriptions survive */
  /* the bound is stated in-band, and says HOW the subset was chosen */
  assert(strstr(cat, "registered entity-pivot services") != NULL);
  assert(strstr(cat, "embedding similarity") != NULL);
  /* every id handed back appears in the text the model will read */
  for (int i = 0; i < cnt; i++) {
    assert(ids[i] && *ids[i]);
    assert(strstr(cat, ids[i]) != NULL);
    assert(registry_get(ids[i]) != NULL);    /* and is really registered */
  }
  printf("  catalogue: %d of %d, descriptions kept, bound stated: ok\n",
         note.shown, note.total);
  service_vec_free_ids(ids, cnt);
  free(cat);
}

/* 5: the ranking depends on the query. */
static void test_query_dependent(db_handle *db) {
  use_stub(g_port);
  char **a = NULL, **b = NULL; int an = 0, bn = 0;
  char *ca = service_vec_catalogue(db, "domain whois registrar ownership lookup",
                                   10, NULL, &a, &an);
  char *cb = service_vec_catalogue(db, "earthquake seismic magnitude tsunami",
                                   10, NULL, &b, &bn);
  assert(ca && cb && an > 0 && bn > 0);
  int same = 0;
  for (int i = 0; i < an; i++)
    for (int j = 0; j < bn; j++)
      if (!strcmp(a[i], b[j])) { same++; break; }
  /* Two unrelated questions must not produce the same menu; if they did, the
   * router is not routing. */
  assert(same < an);
  printf("  ranking is query-dependent (%d of %d shared between two "
         "unrelated queries): ok\n", same, an);
  service_vec_free_ids(a, an); service_vec_free_ids(b, bn);
  free(ca); free(cb);
}

/* 6: a different model/dimension rebuilds instead of mixing spaces. */
static void test_model_change_rebuilds(db_handle *db) {
  char *dim_before = svec_meta_get(db, "dim");
  assert(dim_before && atoi(dim_before) == 64);

  assert(stub_start(g_port + 1, 96, "stub-b") == 0);
  use_stub(g_port);                          /* g_port updated by stub_start */
  int n = service_vec_build(db);
  assert(n > 0);
  char *dim_after = svec_meta_get(db, "dim");
  assert(dim_after && atoi(dim_after) == 96);

  /* and the index is queryable in the NEW space, not a mix */
  char **ids = NULL; int cnt = 0;
  char *cat = service_vec_catalogue(db, "domain whois", 5, NULL, &ids, &cnt);
  assert(cat && cnt > 0);
  service_vec_free_ids(ids, cnt);
  free(cat);
  printf("  model/dim change rebuilt %s-d -> %s-d, no mixing: ok\n",
         dim_before, dim_after);
  free(dim_before); free(dim_after);
}

/* 2 (negative): a server that dies mid-build must never leave a QUERYABLE
 * half-index. Rows may remain on disk — what matters is that nothing answers
 * from them, because a partial catalogue routes confidently from an arbitrary
 * fraction of the registry. */
static void test_failed_build_is_not_queryable(db_handle *db) {
  stub_stop();                                /* nothing answers now */
  use_stub(g_port);
  svec_meta_set(db, "registry_sig", "0");     /* force a rebuild */
  int rc = service_vec_build(db);
  assert(rc < 0);                             /* honest failure */

  char *state = svec_meta_get(db, "state");
  assert(state && !strcmp(state, "building")); /* never promoted to ready */
  free(state);

  /* and the query path refuses rather than answering from the fragment */
  char **ids = NULL; int cnt = 0;
  char *cat = service_vec_catalogue(db, "domain whois", 10, NULL, &ids, &cnt);
  assert(cat == NULL && cnt == 0);
  char *err = svec_meta_get(db, "last_error");
  assert(err && *err);                        /* the reason is data */
  printf("  failed build is not queryable (state=building, catalogue NULL, "
         "reason recorded): ok\n");
  free(err);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  assert(dbp && "run.sh sets JO_DB to a scratch database");
  db_handle db;
  if (db_open(&db, dbp, NULL) != 0) {
    fprintf(stderr, "test_service_vec: cannot open db\n");
    return 2;
  }

  test_inert(&db);

  if (stub_start(18099, 64, "stub-a") != 0) {
    /* No python3 here is not a test failure: the inert path — the one that
     * matters on a host with no embedding server — has already been proven. */
    printf("test_service_vec: stub unavailable, active-path tests skipped\n");
    stub_stop();
    db_close(&db);
    return 0;
  }

  test_build_and_catalogue(&db);
  test_query_dependent(&db);
  test_failed_build_is_not_queryable(&db);
  /* stub_start() moves to a free port when the suggested one is taken, and
   * records it in g_port — so the restart and the use_stub() that follows must
   * both read g_port rather than repeating the number. */
  assert(stub_start(18099, 64, "stub-a") == 0);
  use_stub(g_port);
  svec_meta_set(&db, "registry_sig", "0");
  assert(service_vec_build(&db) > 0);
  test_model_change_rebuilds(&db);

  stub_stop();
  db_close(&db);
  printf("test_service_vec: ok\n");
  return 0;
}
