/* JapanOSINT native backend — entry point.
 * Default (no flags): boot self-test (open DB, apply schema, integrity check,
 * optional llama-server health) and exit. Real flags:
 *   (default)             run the boot self-test and exit
 *   --serve               run the HTTP server + background scheduler
 *   --sched               run the scheduler loop in the foreground
 *   --run <id> [entity]   run one registered source through the real sink
 *   --list-sources        list the registered sources and exit
 *   --ingest <id> <file> [--type email|username|phone|password|auto]
 *                         offline breach-dump ingest (no DB, no network) */
#include "core/db.h"
#include "core/url_override.h"
#include "core/httpclient.h"
#include "core/llm.h"
#include "core/httpd.h"
#include "core/scheduler.h"
#include "core/breach_index.h"
#include "core/breach_meta.h"
#include "core/alert_deliver.h"
#include "core/alert_eval.h"
#include "core/uploadapi.h"
#include "core/evidence.h"
#include "source.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>            /* _exit — see the serve-path shutdown below */

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

  /* Offline breach-dump ingest (no DB, no network):
   *   --ingest <source_id> <file> [--type email|username|phone|password|auto]
   * Runs the streaming ingest pipeline (core/breach_index.c) into the sharded
   * local index under $JO_BREACH_DIR. Password ingest expects Pwned Passwords
   * "SHA1HEX:count" lines. See docs/breach-check-pipeline.md. */
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--ingest")) {
      if (i + 2 >= argc) {
        fprintf(stderr, "usage: --ingest <source_id> <file> "
                        "[--type email|username|phone|password|auto] "
                        "[--materialize] [--dry-run]\n");
        return 2;
      }
      const char *sid = argv[i + 1], *file = argv[i + 2];
      breach_type t = BT_AUTO;
      int materialize = 0, dry_run = 0;
      for (int j = 1; j < argc; j++) {
        if (!strcmp(argv[j], "--type") && j + 1 < argc) t = breach_type_parse(argv[j + 1]);
        if (!strcmp(argv[j], "--materialize")) materialize = 1;
        if (!strcmp(argv[j], "--dry-run")) dry_run = 1;
      }
      /* --materialize also indexes each datapoint as searchable intel in the
       * breach_items table; without it, ingest stays offline/DB-free.
       * --dry-run persists nothing and just projects the row/disk cost. */
      db_handle mdb = {0};
      db_handle *dbp = NULL;
      if (materialize && !dry_run) {
        if (db_open(&mdb, NULL, NULL) != 0) {
          fprintf(stderr, "[ingest] cannot open database for --materialize\n");
          return 1;
        }
        dbp = &mdb;
      }
      unsigned long long in = 0, nw = 0;
      int rc = breach_index_ingest(sid, file, t, &in, &nw, dbp, materialize, dry_run);
      if (dbp) db_close(&mdb);
      if (rc != 0) { fprintf(stderr, "[ingest] cannot open %s\n", file); return 1; }
      if (dry_run) {
        /* rough breach_items footprint: ~150 B/row incl. the FTS index. */
        double mb = (double)nw * 150.0 / (1024.0 * 1024.0);
        printf("[ingest:dry-run] source=%s type=%s rows_in=%llu rows_new=%llu "
               "est_breach_items=%llu est_disk=%.1f MB (nothing written)\n",
               sid, breach_type_name(t), in, nw, nw, mb);
      } else {
        printf("[ingest] source=%s type=%s rows_in=%llu rows_new=%llu%s\n",
               sid, breach_type_name(t), in, nw,
               materialize ? " (materialized to breach_items)" : "");
      }
      return 0;
    }
  }

  db_handle db = {0};
  if (db_open(&db, NULL, NULL) != 0) return 1;
  url_override_reload(&db);   /* apply any verified collector URL-swap repairs */

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
     * delay the listener). PORT env overrides; default 4000. */
    const char *pe = getenv("PORT");
    int port = pe && *pe ? atoi(pe) : 4000;
    /* Best-effort: load the breach catalog so breaches surface as intel sources.
     * Idempotent upsert; a missing file just logs and continues. The committed
     * seed TSV is the source of truth and needs no Node preprocessing step, so
     * try it first and fall back to the generated JSON manifest only if it is
     * unreadable (older checkouts that have the JSON but not the TSV). */
    if (breach_meta_load_seed_tsv(&db, NULL, NULL) < 0)
      breach_meta_load_manifest(&db, NULL);
    /* Pre-warm the alert rule cache so the first ingested row of the process
     * doesn't pay the load. Purely an optimisation — the cache is lazy and
     * its mutex is statically initialised, so everything works without it. */
    uploadapi_migrate(&db);
    alert_eval_init(&db);
    /* Entity-watchlist reconciling sweep (roadmap 21). Entity extraction runs
     * asynchronously, long after emit() — so an entity-term rule evaluated
     * only at ingest would match nothing and the feature would silently never
     * fire. WITHOUT THIS LINE, WATCHLISTS DO NOT WORK. */
    alert_eval_sweep_start(&db);
    scheduler_start_background(&db);   /* serve + refresh collectors (Node parity) */
    /* Alert delivery worker (P0.2). Started after the scheduler so a matched
     * event can be delivered as soon as ingest produces it; stopped BEFORE
     * db_close() so the worker is never holding a statement on a closed
     * handle. Set JO_NO_ALERT_DELIVER=1 to run a collector-only node. */
    alert_deliver_start(&db);
    /* Evidence reaper (roadmap 17). Keeps the content-addressed blob store
     * under its byte budget; it unlinks blobs only, never evidence rows —
     * deleting a row would break the hash chain and be indistinguishable
     * from tampering. JO_NO_EVIDENCE_GC disables it. */
    evidence_gc_start(&db);
    int rc = httpd_serve(&db, port);
    /* Order matters. Drain the collector pool FIRST: its workers are the ones
     * inside libcurl/OpenSSL, and the pods below are quick to stop. */
    scheduler_stop_background(3000);
    evidence_gc_stop();
    alert_eval_sweep_stop();
    alert_deliver_stop();
    db_close(&db);

    /* _exit, not return.
     *
     * The scheduler's workers are detached and unjoinable, so after the drain
     * above there may still be one parked in a multi-second network call.
     * Returning from main runs atexit, and OPENSSL_cleanup then frees the
     * CRYPTO lock objects out from under that worker. ThreadSanitizer caught
     * exactly this: four heap-use-after-frees in CRYPTO_THREAD_read_lock with
     * the freeing thread (main, via OPENSSL_cleanup) and the reading thread
     * (a scheduler worker inside curl_easy_perform) both named, terminating in
     * a SEGV in CRYPTO_THREAD_write_lock. A clean shutdown exited by signal.
     *
     * There is nothing to reclaim by hand here — the process is ending and the
     * kernel takes the rest — so skipping the teardown is strictly safer than
     * racing it. stdio is flushed explicitly since _exit does not. Any
     * interrupted SQLite transaction rolls back on next open; that guarantee
     * comes from the WAL, not from orderly shutdown. */
    fflush(NULL);
    _exit(rc);
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
