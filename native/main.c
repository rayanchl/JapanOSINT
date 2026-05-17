/* JapanOSINT native backend — entry point.
 * P1: boot self-test (open DB, apply schema, integrity check, optional
 * llama-server health). The HTTP server / scheduler / sources arrive in
 * P3/P4. Usage: japanosint [--selftest] */
#include "core/db.h"
#include "core/httpclient.h"
#include "core/llm.h"
#include "core/fts.h"
#include "core/httpd.h"
#include "core/scheduler.h"
#include "source.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Load JO_REPO_ROOT/.env (KEY=VALUE lines) into the environment, mirroring
 * the Node entrypoint's `node --env-file=../.env`. Existing env wins; quotes
 * stripped; blank/`#` lines skipped. */
static void load_dotenv(void) {
#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/JapanOSINT"
#endif
  const char *p = getenv("JO_ENV_FILE");
  char path[1024];
  if (p && *p) snprintf(path, sizeof path, "%s", p);
  else snprintf(path, sizeof path, "%s/.env", JO_REPO_ROOT);
  FILE *f = fopen(path, "r");
  if (!f) return;
  char line[4096];
  while (fgets(line, sizeof line, f)) {
    char *s = line;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '#' || *s == '\n' || *s == '\0') continue;
    char *eq = strchr(s, '=');
    if (!eq) continue;
    *eq = '\0';
    char *key = s, *val = eq + 1;
    char *end = key + strlen(key);
    while (end > key && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
    size_t vl = strlen(val);
    while (vl && (val[vl - 1] == '\n' || val[vl - 1] == '\r' ||
                  val[vl - 1] == ' ' || val[vl - 1] == '\t')) val[--vl] = '\0';
    if (vl >= 2 && ((val[0] == '"' && val[vl - 1] == '"') ||
                    (val[0] == '\'' && val[vl - 1] == '\''))) {
      val[vl - 1] = '\0'; val++;
    }
    if (*key) setenv(key, val, 0); /* 0 = don't overwrite real env */
  }
  fclose(f);
}

int main(int argc, char **argv) {
  load_dotenv();
  int selftest = 1; /* P1 default */
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "--serve")) selftest = 0;

  /* P2 parity harness: stdin lines -> segmented stdout (matches Node
   * jpTokenizer.segmentForFts exactly). */
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--wakati")) {
      char *line = NULL; size_t cap = 0; ssize_t n;
      while ((n = getline(&line, &cap, stdin)) != -1) {
        if (n > 0 && line[n - 1] == '\n') line[n - 1] = '\0';
        char *seg = fts_segment(line);
        printf("%s\n", seg);
        free(seg);
      }
      free(line);
      fts_shutdown();
      return 0;
    }
  }

  db_handle db = {0};
  if (db_open(&db, NULL, NULL) != 0) return 1;

  /* P4: --run <source_id> [entity]  → run one source through the real sink. */
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--run") && i + 1 < argc) {
      const source_def *d = registry_get(argv[i + 1]);
      if (!d) { fprintf(stderr, "unknown source %s\n", argv[i + 1]); db_close(&db); return 2; }
      const char *ent = (i + 2 < argc) ? argv[i + 2] : NULL;
      int rc = scheduler_run_source(&db, d, ent);
      db_close(&db);
      return rc == 0 ? 0 : 1;
    }
    if (!strcmp(argv[i], "--list-sources")) {
      const source_def **a = registry_all();
      for (int k = 0; k < registry_count(); k++)
        printf("%-22s collector=%-12s interval=%d\n",
               a[k]->id, a[k]->collector, a[k]->update_interval_sec);
      db_close(&db);
      return 0;
    }
    if (!strcmp(argv[i], "--sched")) { scheduler_loop(&db); db_close(&db); return 0; }
  }

  if (!selftest) {
    /* P3 serve path: no heavy preamble (integrity scan + 5s llm probe would
     * delay the listener). PORT env (Node uses 4000); default 4072 so C runs
     * side-by-side with Node during migration. */
    const char *pe = getenv("PORT");
    int port = pe && *pe ? atoi(pe) : 4072;
    scheduler_start_background(&db);   /* serve + refresh collectors (Node parity) */
    int rc = httpd_serve(&db, port);
    db_close(&db);
    return rc;
  }

  char msg[256] = {0};
  int ok = db_integrity_ok(&db, msg, sizeof msg);
  int objs = db_object_count(&db);
  printf("[selftest] integrity_check: %s\n", ok ? "ok" : msg);
  printf("[selftest] schema objects : %d\n", objs);

  http_client *http = http_client_new();
  llm_client llm;
  llm_init(&llm, http);
  int llm_up = llm_healthy(&llm);
  printf("[selftest] llama-server   : %s (%s)\n",
         llm_up ? "up" : "down (search/enrich degrade gracefully)",
         llm.base_url);

  http_client_free(http);
  db_close(&db);
  int pass = ok && objs > 80;
  printf("[selftest] %s\n", pass ? "PASS" : "FAIL");
  return pass ? 0 : 1;
}
