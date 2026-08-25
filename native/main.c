/* JapanOSINT native backend — entry point.
 * Default (no flags): boot self-test (open DB, apply schema, integrity check,
 * optional llama-server health) and exit. Real flags:
 *   (default)/--selftest  run the boot self-test and exit
 *   --serve               run the HTTP server + background scheduler
 *   --sched               run the scheduler loop in the foreground
 *   --run <id> [entity]   run one registered source through the real sink
 *   --dispatch <id> <entity>  run one source through the OSINT dispatcher and
 *                         print the captured {record_count,records} JSON
 *   --list-sources        list the registered sources and exit
 *   --ingest <id> <file> [--type email|username|phone|password|auto]
 *                         offline breach-dump ingest (no DB, no network)
 *   --help                print the usage text and exit
 *
 * An unrecognised flag or a missing operand is an error (exit 2), never a
 * silent fall-through to the self-test — see the validation block in main(). */
#include "core/db.h"
#include "core/url_override.h"
#include "core/httpclient.h"
#include "core/llm.h"
#include "core/httpd.h"
#include "core/scheduler.h"
#include "core/osint_dispatch.h"
#include "core/intel.h"
#include "core/keysapi.h"
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
#include <strings.h>           /* strcasecmp — --type validation below */
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

/* Usage text. Kept in one place so the file header above, --help and the error
 * path below cannot drift apart. */
static void usage(FILE *out) {
  fputs(
    "usage: japanosint [MODE]\n"
    "  (no flags) | --selftest    run the boot self-test and exit\n"
    "  --serve                    HTTP server + background scheduler\n"
    "  --sched                    run the scheduler loop in the foreground\n"
    "  --run <id> [entity]        run one registered source through the real sink\n"
    "  --dispatch <id> <entity>   run one source through the OSINT dispatcher and\n"
    "                             print the captured {record_count,records} JSON\n"
    "  --list-sources             list the registered sources and exit\n"
    "  --ingest <id> <file> [--type email|username|phone|password|auto]\n"
    "           [--materialize] [--dry-run]\n"
    "                             offline breach-dump ingest (no DB, no network)\n"
    "  --help                     this text\n", out);
}

/* Is argv[i] the start of the NEXT flag rather than an operand? Source ids and
 * entities (domains, IPs, emails, corporate numbers) never begin with "--", so
 * this separates "the operand is missing" from "the operand is unusual". */
static int is_flag(const char *a) { return a && a[0] == '-' && a[1] == '-'; }

int main(int argc, char **argv) {
  /* Order matters: the api-keys.json overlay is applied FIRST so it wins over
   * .env, matching keysapi.c's resolved_env(). Both use overwrite=0, so a real
   * shell export still beats either. */
  keysapi_apply_overlay_env();
  load_dotenv();

  /* ARGUMENT VALIDATION, BEFORE ANY SIDE EFFECT.
   *
   * Every mode below is selected by scanning argv for its flag and was silently
   * skipped when its operands were absent. The fall-through landed on the
   * no-flag default -- the boot self-test -- so
   *
   *     japanosint --run "$SOURCE_ID"      # SOURCE_ID unset or empty
   *     japanosint --dispatch DNS_RECORDS  # entity forgotten
   *     japanosint --colllect-everything   # flag typo
   *
   * each printed "[selftest] PASS" and exited 0. An operator script or a CI
   * step asking for a collection run got the strongest success signal this
   * binary can emit while collecting precisely nothing, and the typo case
   * survives review longest because it looks like it worked.
   *
   * That is the same silent-nothing failure the house rules exist against (an
   * EMPTY_RESULTSET source, a row that is registered but unreachable): a run
   * that did no work must not be indistinguishable from one that did. So an
   * unrecognised flag and a missing operand are hard errors with a usage
   * message, and the self-test stays what it always was -- what you get when
   * you ask for nothing in particular.
   *
   * Validated here rather than at each use site so the failure is reported
   * before db_open() creates a database file. */
  /* `--selftest` is in this list for a reason worth recording. The file header
   * has always advertised it and `make selftest` has always invoked it, but it
   * was never parsed — it worked only because an unrecognised flag used to fall
   * through to the no-flag default, which IS the self-test.
   * docs/backend-structure-and-pipeline-report.md:108 already noted the flag
   * "is never parsed". Rejecting unknown flags above turns that latent nothing
   * into a hard failure of the CI step that runs it, so the flag is now real:
   * an explicit request for the run that used to happen by accident. */
  static const char *KNOWN[] = {
    "--serve", "--sched", "--list-sources", "--run", "--dispatch",
    "--ingest", "--type", "--materialize", "--dry-run",
    "--selftest", "--help", "-h", NULL
  };
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') continue;              /* an operand, not a flag */
    if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
      usage(stdout);
      return 0;
    }
    int known = 0;
    for (int k = 0; KNOWN[k]; k++)
      if (!strcmp(argv[i], KNOWN[k])) { known = 1; break; }
    if (!known) {
      fprintf(stderr, "japanosint: unknown option %s\n", argv[i]);
      usage(stderr);
      return 2;
    }
    /* Required operands. `--run`'s trailing entity is optional, so only its
     * first operand is checked. */
    int need = !strcmp(argv[i], "--run")      ? 1
             : !strcmp(argv[i], "--dispatch") ? 2
             : !strcmp(argv[i], "--ingest")   ? 2
             : !strcmp(argv[i], "--type")     ? 1 : 0;
    for (int n = 1; n <= need; n++) {
      if (i + n >= argc || is_flag(argv[i + n])) {
        fprintf(stderr, "japanosint: %s needs %d operand%s\n",
                argv[i], need, need == 1 ? "" : "s");
        usage(stderr);
        return 2;
      }
    }
  }

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
        if (!strcmp(argv[j], "--type") && j + 1 < argc) {
          /* breach_type_parse() answers BT_AUTO for anything it does not
           * recognise, which is the right default for a caller that supplied
           * no type at all (the HTTP ingest path) and the wrong answer for one
           * that spelled a type out. `--type passwrod` silently ingested a
           * Pwned-Passwords "SHA1HEX:count" file in auto mode — every line
           * classified by shape rather than as a password hash — and exited 0.
           * An explicit type that did not survive the parse is a typo, so say
           * so rather than quietly substituting a different mode. */
          t = breach_type_parse(argv[j + 1]);
          if (t == BT_AUTO && strcasecmp(argv[j + 1], "auto") != 0) {
            fprintf(stderr, "japanosint: unknown --type %s "
                            "(email|username|phone|password|auto)\n", argv[j + 1]);
            return 2;
          }
        }
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
    /* --dispatch <source_id> <entity> → run one source through the OSINT
     * dispatcher (not the scheduler) and print the captured result JSON. This
     * is the seam the exhaustive-use rule cares about: `record_count` must
     * equal the number of records the source emitted, and `records` must carry
     * all of them (docs/SOURCE_EXHAUSTIVENESS.md). */
    if (!strcmp(argv[i], "--dispatch") && i + 2 < argc) {
      http_client *hc = http_client_new();
      llm_client lc; llm_init(&lc, hc);
      intel_sink sink = intel_sink_make(&db, argv[i + 1], "legacy");
      osint_result r;
      osint_dispatch(&db, &lc, argv[i + 1], argv[i + 2], NULL, &sink, &r);
      printf("service=%s success=%d records=%d\n%s\n", r.service, r.success,
             r.records, r.data ? r.data : "(no data)");
      if (r.error) printf("error=%s\n", r.error);
      osint_result_free(&r);
      http_client_free(hc);
      db_close(&db);
      return 0;
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
