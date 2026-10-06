/* core/embed_pod.c — see embed_pod.h.
 *
 * SHAPE. A `_maint` pod on a 120 s interval, the same pattern as
 * collectors/pod/simhash_pod.c: `_maint` because it emits no intel_items (a
 * real collector id would trip records_drop against a permanent records=0),
 * and the scheduler's skip-if-running rule serialises ticks so two sweeps
 * never interleave.
 *
 * WHICH ROWS ARE ELIGIBLE. A row is in scope when its DATE is within
 * JO_EMBED_SINCE_DAYS, and its date is published_at only when published_at
 * looks like ISO-8601 text (`GLOB '[0-9][0-9][0-9][0-9]-*'`), else fetched_at.
 * It used to be COALESCE(published_at, fetched_at) compared as TEXT against an
 * ISO cutoff, and collectors store published_at in whatever shape the upstream
 * used: an epoch ("1727000000") or a dd/mm/yyyy date sorts below "2026-…"
 * and never qualified, an RFC 822 date ("Mon, 02 Oct …") sorts above it and
 * always did. ROW_DATE below is the one definition; the walk, the delta and
 * the eligible_count coverage reports all use it.
 *
 * RESUMABILITY. Two phases, both keyed in intel_vec_meta:
 *   1. the FULL sweep walks eligible rows newest-first behind a keyset
 *      watermark. It walks idx_intel_items_pub (COALESCE(published_at,
 *      fetched_at) DESC, uid), which is the index that exists — an index on
 *      ROW_DATE would be another ~1 GB on a 12.9M-row table. That index
 *      orders every row whose ROW_DATE equals its COALESCE (ISO or NULL
 *      published_at), plus non-ISO rows whose published_at happens to sort
 *      at or after the cutoff (filtered on ROW_DATE). The rest — non-ISO
 *      published_at sorting BELOW the cutoff while fetched_at is inside it —
 *      is phase "B", walked on idx_intel_items_fetched. A and B partition the
 *      eligible set exactly, so the sweep visits each eligible row once.
 *      Each page is a bounded index range from the watermark (`<= wm AND
 *      (< wm OR uid > wu)`), not a scan from the top of the index down to it.
 *   2. once the full sweep runs dry, `full_done_at` is set to the time the
 *      sweep STARTED and every later tick is a DELTA: a walk of
 *      idx_intel_items_fetched in (fetched_at, uid) order from a persisted
 *      keyset (dwm_f, dwm_u). The walk only returns rows that need a look:
 *      not embedded, or re-fetched since they were last checked
 *      (intel_vec_done.embedded_at <= fetched_at). A row re-fetched with
 *      identical text is MARKED (embedded_at := this tick's mark) so it
 *      drops out of the walk; one whose text changed is re-embedded (DELETE +
 *      INSERT on the vec0 table).
 *
 * WHY THE DELTA LOOKS LIKE THAT. It used to restart at uid "" every tick,
 * select re-fetched-but-unchanged rows forever (their embedded_at never
 * moved), and charge them against the 2,000-row cap. Once more than 2,000
 * rows had been re-fetched since the full sweep, every tick walked the same
 * first 2,000 uids, sent nothing, and a NEW row whose uid sorted later was
 * never embedded — rc=0, no error, forever. Now: unchanged rows are marked,
 * the keyset persists, the cap (JO_EMBED_MAX_PER_RUN) counts only rows SENT
 * to the server, and a separate walk cap (JO_EMBED_MAX_WALK) bounds the
 * cheap hash comparisons so a tick still ends.
 *
 * TIMESTAMPS. fetched_at is `YYYY-MM-DDTHH:MM:SS.mmmZ` (core/intel.c), so
 * every mark this pod writes uses the same shape — a `…:SSZ` mark compares
 * GREATER than a `…:SS.123Z` fetched_at in the same second ('Z' > '.'), and a
 * row fetched in that second was skipped. And a sink stamps fetched_at BEFORE
 * it commits, so a row can land with a stamp earlier than the moment this
 * pod looked. The mark is therefore the tick's start minus
 * EMBED_OVERLAP_SEC, and the delta keyset is clamped back to it at the end of
 * every tick: rows in that window are looked at once more (a hash compare),
 * which is the price of never missing one.
 *
 * A ROW THE SERVER CANNOT TAKE. Invalid UTF-8 from a sink, an input the model
 * answers with NaN, an input over the server's limit: any of these used to
 * fail the whole batch, and the batch was retried at the same watermark every
 * tick — the sweep stopped there for good. Text is now sanitised to valid
 * UTF-8 before it is sent; a failed batch is retried one row at a time; a row
 * that fails on its own while the server demonstrably works (another row
 * succeeded, or the dimension probe does) is recorded in intel_vec_failed
 * with its hash and reason and skipped. It is retried when its text changes.
 * The count and a bounded sample are in the coverage block — skipped rows are
 * data, not a log line. A transient refusal (unreachable, timeout, 503, 429)
 * marks nothing and is retried next tick.
 *
 * MODEL IDENTITY. The model is JO_EMBED_MODEL, else what GET /v1/models says.
 * If neither answers, the tick is skipped with last_error set — "unknown" is
 * never recorded, because a placeholder recorded on a bad first tick made
 * every later (successful) detection a "model change" and the pod refused
 * forever. The model is persisted together with the dimension, in the same
 * transaction as the first rows written.
 *
 * WHY A SIDE TABLE NEXT TO THE vec0 TABLE. vec0 owns its own shadow tables
 * and a text primary key lookup through the virtual table is not a plain
 * B-tree probe. intel_vec_done(uid PRIMARY KEY, text_hash, embedded_at) is
 * an ordinary table: NOT EXISTS against it is an index seek, count(*) on it
 * is the embedded_count the coverage block reports, and the hash is what
 * makes "re-fetched but unchanged" free. The two are written in one
 * transaction (BEGIN IMMEDIATE, checked) so they cannot disagree.
 *
 * THE THREAD IT RUNS ON. llm_embed() routes through core/llm_worker.c, which
 * keys its worker threads on base_url; JO_EMBED_URL is a different host:port
 * from LLM_BASE_URL, so the embedding pod owns its own worker and never sits
 * in the generation queue. A user's semantic QUERY shares this worker (same
 * base_url) on the interactive lane, and bounds its wait in the queue
 * (llm_client.bound_queue_wait) so it cannot be held for a whole batch. */
#include "embed_pod.h"
#include "../source.h"
#include "llm.h"
#include "httpclient.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define EMBED_DONE_TABLE "intel_vec_done"
#define EMBED_SID        "embedding-backfill"
/* Seconds subtracted from every mark/watermark — see TIMESTAMPS above. */
#define EMBED_OVERLAP_SEC 60
/* Rows read per selection. A delta page is mostly unchanged rows that cost a
 * hash compare each, so it is read in larger pages than it is embedded. */
#define SEL_CHUNK 256
/* Failed rows listed in the coverage block. The count is always complete. */
#define FAIL_SAMPLE 5

/* ---- env knobs ----------------------------------------------------------- */

static long env_long(const char *k, long dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  char *end = NULL;
  long n = strtol(v, &end, 10);
  return (end && end != v) ? n : dflt;
}

const char *embed_base_url(void) {
  const char *u = getenv("JO_EMBED_URL");
  return (u && *u) ? u : NULL;
}

static long since_days(void)  { long d = env_long("JO_EMBED_SINCE_DAYS", 180); return d > 0 ? d : 180; }
static int  batch_size(void)  { long b = env_long("JO_EMBED_BATCH", 32); return (b > 0 && b <= 256) ? (int)b : 32; }
static int  max_per_run(void) { long m = env_long("JO_EMBED_MAX_PER_RUN", 2000); return m > 0 ? (int)m : 2000; }
static int  max_chars(void)   { long m = env_long("JO_EMBED_MAX_CHARS", 1000); return (m >= 64) ? (int)m : 1000; }
/* Rows LOOKED AT per tick (embedded + checked-unchanged + skipped-failed). */
static long max_walk(void) {
  long w = env_long("JO_EMBED_MAX_WALK", 0);
  return w > 0 ? w : 25L * max_per_run();
}

/* JO_EMBED_RECORD_TYPES as a JSON array string for json_each(), or NULL for
 * the default (everything but the two collector notices). */
static char *record_types_json(void) {
  const char *v = getenv("JO_EMBED_RECORD_TYPES");
  if (!v || !*v) return NULL;
  cJSON *a = cJSON_CreateArray();
  char *copy = strdup(v);
  if (!copy) { cJSON_Delete(a); return NULL; }
  /* strtok_r, not strtok: this pod runs on a scheduler worker thread, and
   * strtok keeps its cursor in a single process-wide static. Another thread
   * calling strtok between two of these iterations — the same race the
   * 2026-08-16 audit found in lib/jsonlist.c and core/operatorgate.c — moves
   * this loop's cursor into someone else's string. Low frequency here (a pod
   * tick reading one env var) is a reason it had not bitten, not a reason for
   * it to stay: this was the last bare strtok() in the tree. */
  char *save = NULL;
  for (char *tok = strtok_r(copy, ",", &save); tok;
       tok = strtok_r(NULL, ",", &save)) {
    while (*tok == ' ') tok++;
    size_t n = strlen(tok);
    while (n && tok[n - 1] == ' ') tok[--n] = 0;
    if (n) cJSON_AddItemToArray(a, cJSON_CreateString(tok));
  }
  free(copy);
  char *out = cJSON_GetArraySize(a) ? cJSON_PrintUnformatted(a) : NULL;
  cJSON_Delete(a);
  return out;
}

/* ---- text ---------------------------------------------------------------- */

/* Length of the valid UTF-8 sequence at `s` (RFC 3629: no overlongs, no
 * surrogates, nothing past U+10FFFF), or 0 when the bytes there are not one.
 * That is exactly what llama-server's JSON parser accepts. */
static size_t utf8_seq(const unsigned char *s) {
  unsigned char c = s[0];
  if (c < 0x80) return 1;
#define CONT(x) (((x) & 0xC0) == 0x80)
  if (c >= 0xC2 && c <= 0xDF) return CONT(s[1]) ? 2 : 0;
  if (c == 0xE0) return (s[1] >= 0xA0 && s[1] <= 0xBF && CONT(s[2])) ? 3 : 0;
  if ((c >= 0xE1 && c <= 0xEC) || c == 0xEE || c == 0xEF)
    return (CONT(s[1]) && CONT(s[2])) ? 3 : 0;
  if (c == 0xED) return (s[1] >= 0x80 && s[1] <= 0x9F && CONT(s[2])) ? 3 : 0;
  if (c == 0xF0) return (s[1] >= 0x90 && s[1] <= 0xBF && CONT(s[2]) && CONT(s[3])) ? 4 : 0;
  if (c >= 0xF1 && c <= 0xF3) return (CONT(s[1]) && CONT(s[2]) && CONT(s[3])) ? 4 : 0;
  if (c == 0xF4) return (s[1] >= 0x80 && s[1] <= 0x8F && CONT(s[2]) && CONT(s[3])) ? 4 : 0;
#undef CONT
  return 0;
}

char *embed_utf8_sanitize(const char *text, size_t *replaced) {
  if (replaced) *replaced = 0;
  if (!text) return strdup("");
  size_t n = strlen(text);
  char *o = malloc(n * 3 + 1);            /* worst case: every byte → U+FFFD */
  if (!o) return NULL;
  const unsigned char *p = (const unsigned char *)text;
  size_t j = 0;
  while (*p) {
    size_t k = utf8_seq(p);
    if (k) { memcpy(o + j, p, k); j += k; p += k; continue; }
    /* One invalid byte → one U+FFFD, then resync on the next byte. The
     * character is replaced, not dropped, so the text keeps its shape. */
    o[j++] = (char)0xEF; o[j++] = (char)0xBF; o[j++] = (char)0xBD;
    p++;
    if (replaced) (*replaced)++;
  }
  o[j] = 0;
  return o;
}

char *embed_bound_text(const char *text) {
  char *clean = embed_utf8_sanitize(text, NULL);
  if (!clean) return NULL;
  size_t cap = (size_t)max_chars();
  size_t n = strlen(clean);
  if (n <= cap) return clean;
  /* Back up to a UTF-8 lead byte so a multibyte character is never split —
   * a half character is a different token to the model, not a shorter one. */
  while (cap > 0 && ((unsigned char)clean[cap] & 0xC0) == 0x80) cap--;
  clean[cap] = 0;
  return clean;
}

/* ---- time ---------------------------------------------------------------- */

static long long wall_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

/* The shape of intel_items.fetched_at (core/intel.c iso_now). */
static void iso_ms(char *buf, size_t n, long long ms) {
  time_t t = (time_t)(ms / 1000);
  struct tm tm;
  gmtime_r(&t, &tm);
  snprintf(buf, n, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
           (unsigned)(tm.tm_year + 1900) % 10000u, (unsigned)(tm.tm_mon + 1) % 100u,
           (unsigned)tm.tm_mday % 100u, (unsigned)tm.tm_hour % 100u,
           (unsigned)tm.tm_min % 100u, (unsigned)tm.tm_sec % 100u,
           (unsigned)(ms % 1000) % 1000u);
}

/* Parse "YYYY-MM-DDTHH:MM:SS[.mmm]Z" to epoch ms; -1 if it is not that. */
static long long parse_iso_ms(const char *s) {
  struct tm tm = {0};
  int ms = 0;
  if (!s || sscanf(s, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                   &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) return -1;
  const char *dot = strchr(s, '.');
  if (dot) ms = atoi(dot + 1);
  tm.tm_year -= 1900; tm.tm_mon -= 1;
  return (long long)timegm(&tm) * 1000LL + ms;
}

/* ---- meta table ---------------------------------------------------------- */

static int meta_get(db_handle *db, const char *k, char *out, size_t cap) {
  out[0] = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "SELECT v FROM " EMBED_META_TABLE " WHERE k=?1",
                         -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, k, -1, SQLITE_STATIC);
  int found = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *v = (const char *)sqlite3_column_text(s, 0);
    snprintf(out, cap, "%s", v ? v : "");
    found = 1;
  }
  sqlite3_finalize(s);
  return found;
}

static int meta_set(db_handle *db, const char *k, const char *v) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO " EMBED_META_TABLE "(k,v) VALUES(?1,?2) "
        "ON CONFLICT(k) DO UPDATE SET v=excluded.v", -1, &s, NULL) != SQLITE_OK)
    return -1;
  sqlite3_bind_text(s, 1, k, -1, SQLITE_STATIC);
  if (v) sqlite3_bind_text(s, 2, v, -1, SQLITE_TRANSIENT);
  else   sqlite3_bind_null(s, 2);
  int rc = sqlite3_step(s) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(s);
  return rc;
}

static int meta_set_long(db_handle *db, const char *k, long v) {
  char b[32];
  snprintf(b, sizeof b, "%ld", v);
  return meta_set(db, k, b);
}

static long meta_long(db_handle *db, const char *k) {
  char b[32];
  return meta_get(db, k, b, sizeof b) ? strtol(b, NULL, 10) : 0;
}

static int table_exists(db_handle *db, const char *name) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1",
        -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, name, -1, SQLITE_STATIC);
  int ok = sqlite3_step(s) == SQLITE_ROW;
  sqlite3_finalize(s);
  return ok;
}

static long count_rows(db_handle *db, const char *table) {
  if (!table_exists(db, table)) return 0;
  char sql[128];
  snprintf(sql, sizeof sql, "SELECT count(*) FROM %s", table);
  sqlite3_stmt *s;
  long n = 0;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  return n;
}

int embed_table_exists(db_handle *db) {
  return db && db->h && table_exists(db, EMBED_VEC_TABLE);
}

/* The meta table is created on every path that might read it, so the
 * coverage block can be built on a database the pod has never touched. */
static void ensure_meta(db_handle *db) {
  sqlite3_exec(db->h,
    "CREATE TABLE IF NOT EXISTS " EMBED_META_TABLE "(k TEXT PRIMARY KEY, v TEXT)",
    NULL, NULL, NULL);
}

static void ensure_side_tables(db_handle *db) {
  sqlite3_exec(db->h,
    "CREATE TABLE IF NOT EXISTS " EMBED_DONE_TABLE
    "(uid TEXT PRIMARY KEY, text_hash INTEGER NOT NULL, embedded_at TEXT NOT NULL);"
    "CREATE TABLE IF NOT EXISTS " EMBED_FAIL_TABLE
    "(uid TEXT PRIMARY KEY, text_hash INTEGER NOT NULL, failed_at TEXT NOT NULL,"
    " reason TEXT NOT NULL)",
    NULL, NULL, NULL);
}

int embed_index_dim(db_handle *db, char *model, size_t cap) {
  if (model && cap) model[0] = 0;
  if (!db || !db->h || !table_exists(db, EMBED_META_TABLE)) return 0;
  if (model && cap) meta_get(db, "model", model, cap);
  return (int)meta_long(db, "dim");
}

int embed_index_refused(db_handle *db, char *why, size_t cap) {
  if (why && cap) why[0] = 0;
  if (!db || !db->h || !table_exists(db, EMBED_META_TABLE)) return 0;
  char buf[600];
  if (!meta_get(db, "refused", buf, sizeof buf) || !buf[0]) return 0;
  if (why && cap) snprintf(why, cap, "%s", buf);
  return 1;
}

cJSON *embed_coverage_json(db_handle *db) {
  cJSON *o = cJSON_CreateObject();
  const char *url = embed_base_url();
  cJSON_AddBoolToObject(o, "enabled", url != NULL);
  cJSON_AddBoolToObject(o, "index_present", embed_table_exists(db));
  char buf[600];
  int have_meta = db && db->h && table_exists(db, EMBED_META_TABLE);
  if (have_meta && meta_get(db, "model", buf, sizeof buf) && buf[0])
    cJSON_AddStringToObject(o, "model", buf);
  else cJSON_AddNullToObject(o, "model");
  cJSON_AddNumberToObject(o, "dim", have_meta ? (double)meta_long(db, "dim") : 0);
  cJSON_AddNumberToObject(o, "embedded_count",
                          have_meta ? (double)meta_long(db, "embedded_count") : 0);
  /* eligible_count is a measured number with a timestamp, or null: an
   * estimate would let a caller compute a coverage ratio that was never
   * observed. */
  if (have_meta && meta_get(db, "eligible_count", buf, sizeof buf) && buf[0]) {
    cJSON_AddNumberToObject(o, "eligible_count", (double)strtol(buf, NULL, 10));
    meta_get(db, "eligible_counted_at", buf, sizeof buf);
    cJSON_AddStringToObject(o, "eligible_counted_at", buf);
  } else {
    cJSON_AddNullToObject(o, "eligible_count");
    cJSON_AddNullToObject(o, "eligible_counted_at");
  }
  /* Rows the server would not embed, as data: the count is exact, the list
   * is a bounded sample (newest first) with each row's reason. */
  int have_fail = db && db->h && table_exists(db, EMBED_FAIL_TABLE);
  cJSON_AddNumberToObject(o, "failed_count",
                          have_fail ? (double)count_rows(db, EMBED_FAIL_TABLE) : 0);
  cJSON *fs = cJSON_CreateArray();
  if (have_fail) {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h,
          "SELECT uid, reason, failed_at FROM " EMBED_FAIL_TABLE
          " ORDER BY failed_at DESC, uid LIMIT ?1", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_int(s, 1, FAIL_SAMPLE);
      while (sqlite3_step(s) == SQLITE_ROW) {
        cJSON *f = cJSON_CreateObject();
        const char *u = (const char *)sqlite3_column_text(s, 0);
        const char *r = (const char *)sqlite3_column_text(s, 1);
        const char *a = (const char *)sqlite3_column_text(s, 2);
        cJSON_AddStringToObject(f, "uid", u ? u : "");
        cJSON_AddStringToObject(f, "reason", r ? r : "");
        cJSON_AddStringToObject(f, "checked_at", a ? a : "");
        cJSON_AddItemToArray(fs, f);
      }
      sqlite3_finalize(s);
    }
  }
  cJSON_AddItemToObject(o, "failed_sample", fs);
  cJSON_AddNumberToObject(o, "since_days", (double)since_days());
  char *rt = record_types_json();
  if (rt) { cJSON *a = cJSON_Parse(rt); cJSON_AddItemToObject(o, "record_types", a ? a : cJSON_CreateNull()); free(rt); }
  else cJSON_AddStringToObject(o, "record_types",
         "all except collector-truncation-notice, collector-shape-notice");
  cJSON_AddStringToObject(o, "date_rule",
         "published_at when it is ISO-8601 text, else fetched_at");
  cJSON_AddNumberToObject(o, "max_chars", (double)max_chars());
  if (have_meta && meta_get(db, "full_done_at", buf, sizeof buf) && buf[0]) {
    cJSON_AddStringToObject(o, "sweep", "delta");
    cJSON_AddStringToObject(o, "full_sweep_done_at", buf);
    /* How far the delta walk has got: rows fetched after this are not yet
     * looked at. A watermark falling behind the clock is a backlog, shown. */
    if (meta_get(db, "dwm_f", buf, sizeof buf) && buf[0])
      cJSON_AddStringToObject(o, "delta_watermark", buf);
    else cJSON_AddNullToObject(o, "delta_watermark");
  } else if (have_meta && meta_get(db, "sweep_start", buf, sizeof buf) && buf[0]) {
    /* sweep_start is written by the first full tick, so this is "started",
     * including the instant between phase A draining and phase B's first
     * page, when there is no watermark yet. */
    cJSON_AddStringToObject(o, "sweep", "full_in_progress");
    cJSON_AddStringToObject(o, "sweep_started_at", buf);
    if (meta_get(db, "wm_p", buf, sizeof buf) && buf[0])
      cJSON_AddStringToObject(o, "watermark", buf);
    else cJSON_AddNullToObject(o, "watermark");
    char ph[8];
    meta_get(db, "wm_phase", ph, sizeof ph);
    cJSON_AddStringToObject(o, "phase", ph[0] ? ph : "A");
  } else {
    cJSON_AddStringToObject(o, "sweep", url ? "not_started" : "disabled");
  }
  if (have_meta && meta_get(db, "last_tick", buf, sizeof buf) && buf[0]) {
    cJSON *lt = cJSON_Parse(buf);
    cJSON_AddItemToObject(o, "last_tick", lt ? lt : cJSON_CreateNull());
  } else cJSON_AddNullToObject(o, "last_tick");
  if (have_meta && meta_get(db, "last_error", buf, sizeof buf) && buf[0])
    cJSON_AddStringToObject(o, "last_error", buf);
  else cJSON_AddNullToObject(o, "last_error");
  if (have_meta && meta_get(db, "refused", buf, sizeof buf) && buf[0])
    cJSON_AddStringToObject(o, "refused", buf);
  else cJSON_AddNullToObject(o, "refused");
  if (have_meta && meta_get(db, "last_run_at", buf, sizeof buf) && buf[0])
    cJSON_AddStringToObject(o, "last_run_at", buf);
  else cJSON_AddNullToObject(o, "last_run_at");
  return o;
}

/* ---- schema -------------------------------------------------------------- */

static int ensure_index(db_handle *db, int dim) {
  char sql[256];
  snprintf(sql, sizeof sql,
    "CREATE VIRTUAL TABLE IF NOT EXISTS " EMBED_VEC_TABLE
    " USING vec0(uid TEXT PRIMARY KEY, embedding float[%d])", dim);
  char *err = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "[embed] cannot create %s: %s\n", EMBED_VEC_TABLE, err ? err : "?");
    sqlite3_free(err);
    return -1;
  }
  return 0;
}

/* ---- model identity ------------------------------------------------------ */

int embed_detect_model(http_client *http, const char *base, char *out,
                       size_t cap) {
  const char *m = getenv("JO_EMBED_MODEL");
  if (m && *m) { snprintf(out, cap, "%s", m); return 0; }
  if (!base || !*base) { snprintf(out, cap, "no embedding server configured"); return -1; }
  http_client *own = NULL;
  if (!http) http = own = http_client_new();
  char url[1024];
  size_t bl = strlen(base);
  snprintf(url, sizeof url, "%.*s/v1/models",
           (int)(bl && base[bl - 1] == '/' ? bl - 1 : bl), base);
  http_response r = {0};
  int rc = -1;
  if (!http) snprintf(out, cap, "no http client");
  else if (http_request(http, "GET", url, NULL, NULL, 0, 5000, 0, &r) != 0 ||
           r.status == 0)
    snprintf(out, cap, "GET /v1/models: no answer");
  else if (r.status < 200 || r.status >= 300 || !r.body)
    snprintf(out, cap, "GET /v1/models: HTTP %ld", (long)r.status);
  else {
    cJSON *j = cJSON_Parse(r.body);
    cJSON *data = j ? cJSON_GetObjectItem(j, "data") : NULL;
    cJSON *d0 = data ? cJSON_GetArrayItem(data, 0) : NULL;
    cJSON *id = d0 ? cJSON_GetObjectItem(d0, "id") : NULL;
    if (cJSON_IsString(id) && id->valuestring[0]) {
      /* Basename only: the path is the deployment's, the model is the name. */
      const char *b = strrchr(id->valuestring, '/');
      snprintf(out, cap, "%s", b ? b + 1 : id->valuestring);
      rc = 0;
    } else snprintf(out, cap, "GET /v1/models: no data[0].id in the answer");
    cJSON_Delete(j);
  }
  http_response_free(&r);
  if (own) http_client_free(own);
  return rc;
}

/* A request path asks on every query; a model swap is an operator action, so
 * a few seconds of staleness is the right trade for not adding a round trip
 * to each search. Only SUCCESSFUL answers are cached.
 * JO_EMBED_MODEL_CHECK_TTL_MS overrides the 10 s (0 = ask every time). */
int embed_live_model(char *out, size_t cap) {
  static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
  static char cached[256], cached_base[512];
  static long long cached_at = 0;
  const char *base = embed_base_url();
  long long now = wall_ms();
  long ttl = env_long("JO_EMBED_MODEL_CHECK_TTL_MS", 10000);
  pthread_mutex_lock(&mu);
  if (base && cached_at && now - cached_at < ttl &&
      !strcmp(cached_base, base) && !getenv("JO_EMBED_MODEL")) {
    snprintf(out, cap, "%s", cached);
    pthread_mutex_unlock(&mu);
    return 0;
  }
  pthread_mutex_unlock(&mu);
  char got[256];
  int rc = embed_detect_model(NULL, base, got, sizeof got);
  snprintf(out, cap, "%s", got);
  if (rc == 0 && base) {
    pthread_mutex_lock(&mu);
    snprintf(cached, sizeof cached, "%s", got);
    snprintf(cached_base, sizeof cached_base, "%s", base);
    cached_at = now;
    pthread_mutex_unlock(&mu);
  }
  return rc;
}

/* ---- the sweep ----------------------------------------------------------- */

static unsigned long long fnv1a(const char *s) {
  unsigned long long h = 1469598103934665603ULL;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
  return h;
}

/* One row picked up by a selection query. */
typedef struct {
  char *uid, *text, *key;           /* key = the keyset column of its walk */
  unsigned long long hash;
  int in_done, in_failed;
  unsigned long long done_hash, failed_hash;
} pick;

static void pick_free(pick *p) {
  free(p->uid); free(p->text); free(p->key);
  p->uid = p->text = p->key = NULL;
}

/* title + ' ' + summary, falling back to the first 512 bytes of body;
 * sanitised to valid UTF-8 and bounded (embed_bound_text). */
static char *compose_text(const char *title, const char *summary,
                          const char *body512) {
  const char *t = title ? title : "";
  const char *s = (summary && *summary) ? summary : (body512 ? body512 : "");
  size_t n = strlen(t) + 1 + strlen(s) + 1;
  char *o = malloc(n);
  if (!o) return NULL;
  snprintf(o, n, "%s%s%s", t, (*t && *s) ? " " : "", s);
  char *b = embed_bound_text(o);
  free(o);
  return b;
}

/* Selection SQL. The record-type predicate is bound as a JSON array through
 * json_each so the allowlist never touches SQL text. ?1 = since cutoff,
 * ?2 = record types (NULL = default), ?3/?4 = keyset, ?5 = page size. */
#define ISO_PUB  "intel_items.published_at GLOB '[0-9][0-9][0-9][0-9]-*'"
#define EFF_DATE "COALESCE(intel_items.published_at,intel_items.fetched_at)"
/* THE date of a row — see WHICH ROWS ARE ELIGIBLE. */
#define ROW_DATE "(CASE WHEN " ISO_PUB " THEN intel_items.published_at " \
                 "ELSE intel_items.fetched_at END)"
#define SEL_COLS(KEY) \
  "intel_items.uid, intel_items.title, intel_items.summary, " \
  "substr(intel_items.body,1,512), " KEY ", " \
  "d.text_hash, (d.uid IS NOT NULL), f.text_hash, (f.uid IS NOT NULL)"
#define SEL_FROM \
  " FROM intel_items " \
  "LEFT JOIN " EMBED_DONE_TABLE " d ON d.uid = intel_items.uid " \
  "LEFT JOIN " EMBED_FAIL_TABLE " f ON f.uid = intel_items.uid "
#define SEL_TYPE \
  "((?2 IS NULL AND (intel_items.record_type IS NULL OR intel_items.record_type " \
  "NOT IN ('collector-truncation-notice','collector-shape-notice'))) OR " \
  "(?2 IS NOT NULL AND intel_items.record_type IN " \
  "(SELECT value FROM json_each(?2))))"

/* FULL, phase A: idx_intel_items_pub, newest first. The keyset form bounds
 * the range at the watermark on BOTH sides, so each page starts where the
 * last one stopped instead of re-walking the index from its top. */
#define SQL_FULL_A(KEYSET) \
  "SELECT " SEL_COLS(EFF_DATE) SEL_FROM \
  "WHERE " EFF_DATE " >= ?1 " KEYSET \
  "AND " ROW_DATE " >= ?1 AND " SEL_TYPE " " \
  "AND d.uid IS NULL AND f.uid IS NULL " \
  "ORDER BY " EFF_DATE " DESC, intel_items.uid ASC LIMIT ?5"
static const char *SQL_FULL_A0 = SQL_FULL_A("AND ?3 IS NULL AND ?4 IS NULL ");
static const char *SQL_FULL_AK = SQL_FULL_A(
  "AND " EFF_DATE " <= ?3 AND (" EFF_DATE " < ?3 OR intel_items.uid > ?4) ");

/* FULL, phase B: non-ISO published_at that sorts below the cutoff while the
 * row was fetched inside it — idx_intel_items_fetched, newest first. The
 * unary `+` keeps published_at out of index selection: left to itself the
 * planner took idx_intel_items_source for `published_at < ?1`, a range that
 * holds every OLD ISO-dated row too, and sorted all of them for one page. */
#define SQL_FULL_B(KEYSET) \
  "SELECT " SEL_COLS("intel_items.fetched_at") SEL_FROM \
  "WHERE intel_items.fetched_at >= ?1 " KEYSET \
  "AND +intel_items.published_at IS NOT NULL AND NOT (" ISO_PUB ") " \
  "AND +intel_items.published_at < ?1 AND " SEL_TYPE " " \
  "AND d.uid IS NULL AND f.uid IS NULL " \
  "ORDER BY intel_items.fetched_at DESC, intel_items.uid ASC LIMIT ?5"
static const char *SQL_FULL_B0 = SQL_FULL_B("AND ?3 IS NULL AND ?4 IS NULL ");
static const char *SQL_FULL_BK = SQL_FULL_B(
  "AND intel_items.fetched_at <= ?3 AND (intel_items.fetched_at < ?3 "
  "OR intel_items.uid > ?4) ");

/* DELTA: idx_intel_items_fetched, oldest first from the persisted keyset,
 * only rows that need a look (never embedded or failed, or re-fetched at or
 * after their last check). */
static const char *SQL_DELTA =
  "SELECT " SEL_COLS("intel_items.fetched_at") SEL_FROM
  "WHERE intel_items.fetched_at >= ?3 "
  "AND (intel_items.fetched_at > ?3 OR intel_items.uid > ?4) "
  "AND " ROW_DATE " >= ?1 AND " SEL_TYPE " "
  "AND (d.uid IS NULL OR d.embedded_at <= intel_items.fetched_at) "
  "AND (f.uid IS NULL OR f.failed_at <= intel_items.fetched_at) "
  "ORDER BY intel_items.fetched_at ASC, intel_items.uid ASC LIMIT ?5";

static const char *SQL_ELIGIBLE =
  "SELECT count(*) FROM intel_items WHERE " ROW_DATE " >= ?1 AND " SEL_TYPE;

/* Which walk a tick is on. */
typedef enum { WALK_FULL_A, WALK_FULL_B, WALK_DELTA } walk_kind;

/* Fill `out[0..cap)` from one selection query; returns the number picked, or
 * -1 on a SQL failure. */
static int select_batch(db_handle *db, walk_kind w, const char *since,
                        const char *types, const char *wm_k, const char *wm_u,
                        int cap, pick *out) {
  int keyed = wm_k && *wm_k;
  const char *sql = w == WALK_DELTA ? SQL_DELTA
                  : w == WALK_FULL_A ? (keyed ? SQL_FULL_AK : SQL_FULL_A0)
                  : (keyed ? SQL_FULL_BK : SQL_FULL_B0);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    fprintf(stderr, "[embed] select failed: %s\n", sqlite3_errmsg(db->h));
    return -1;
  }
  sqlite3_bind_text(s, 1, since, -1, SQLITE_STATIC);
  if (types) sqlite3_bind_text(s, 2, types, -1, SQLITE_STATIC); else sqlite3_bind_null(s, 2);
  if (keyed) {
    sqlite3_bind_text(s, 3, wm_k, -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 4, wm_u ? wm_u : "", -1, SQLITE_STATIC);
  } else if (w == WALK_DELTA) {
    /* The delta always has a lower bound; an empty one is "from the start". */
    sqlite3_bind_text(s, 3, "", -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 4, "", -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(s, 3);
    sqlite3_bind_null(s, 4);
  }
  sqlite3_bind_int(s, 5, cap);
  int n = 0, rc;
  while (n < cap && (rc = sqlite3_step(s)) == SQLITE_ROW) {
    const char *uid = (const char *)sqlite3_column_text(s, 0);
    if (!uid) continue;
    pick *p = &out[n];
    memset(p, 0, sizeof *p);
    p->uid  = strdup(uid);
    p->text = compose_text((const char *)sqlite3_column_text(s, 1),
                           (const char *)sqlite3_column_text(s, 2),
                           (const char *)sqlite3_column_text(s, 3));
    const char *kk = (const char *)sqlite3_column_text(s, 4);
    p->key = strdup(kk ? kk : "");
    if (!p->uid || !p->text || !p->key) { pick_free(p); break; }
    p->hash = fnv1a(p->text);
    p->done_hash   = (unsigned long long)sqlite3_column_int64(s, 5);
    p->in_done     = sqlite3_column_int(s, 6);
    p->failed_hash = (unsigned long long)sqlite3_column_int64(s, 7);
    p->in_failed   = sqlite3_column_int(s, 8);
    n++;
  }
  sqlite3_finalize(s);
  return n;
}

/* Per-tick tallies, recorded in meta as `last_tick` so the coverage block can
 * show what the most recent tick actually did. */
typedef struct {
  long walked;       /* rows looked at                                     */
  long sent;         /* rows sent to /v1/embeddings (counted once each)    */
  long embedded;     /* rows stored (new or re-embedded)                   */
  long unchanged;    /* re-fetched, same text: marked, not sent            */
  long failed;       /* newly recorded as failed this tick                 */
  long failed_seen;  /* already failed with this same text: skipped        */
} tick_stats;

/* Record an SQL failure as data and on stderr. */
static void sql_error(db_handle *db, const char *now, const char *what) {
  char msg[400];
  snprintf(msg, sizeof msg, "%s: %s failed: %.300s", now, what, sqlite3_errmsg(db->h));
  fprintf(stderr, "[embed] %s\n", msg);
  meta_set(db, "last_error", msg);
}

/* BEGIN IMMEDIATE, CHECKED. Unchecked, a BUSY here left every statement that
 * followed in autocommit, so a vec row could land without its done row and
 * every later tick hit "UNIQUE constraint failed" with last_error NULL. */
static int begin(db_handle *db, const char *now) {
  if (sqlite3_exec(db->h, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
    sql_error(db, now, "BEGIN IMMEDIATE");
    return -1;
  }
  return 0;
}

static int finish(db_handle *db, int ok, const char *now) {
  if (ok) {
    if (sqlite3_exec(db->h, "COMMIT", NULL, NULL, NULL) == SQLITE_OK) return 0;
    sql_error(db, now, "COMMIT");
  }
  sqlite3_exec(db->h, "ROLLBACK", NULL, NULL, NULL);
  return -1;
}

static int exec_uid(db_handle *db, const char *sql, const char *uid) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_STATIC);
  int rc = sqlite3_step(s) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(s);
  return rc;
}

/* Mark rows looked at and found unchanged: done rows get embedded_at := mark,
 * failed rows failed_at := mark. One transaction. */
static int mark_seen(db_handle *db, pick **rows, int n, const char *mark,
                     const char *now) {
  if (n <= 0) return 0;
  if (begin(db, now) != 0) return -1;
  sqlite3_stmt *ud = NULL, *uf = NULL;
  int ok = sqlite3_prepare_v2(db->h, "UPDATE " EMBED_DONE_TABLE
                              " SET embedded_at=?2 WHERE uid=?1", -1, &ud, NULL) == SQLITE_OK &&
           sqlite3_prepare_v2(db->h, "UPDATE " EMBED_FAIL_TABLE
                              " SET failed_at=?2 WHERE uid=?1", -1, &uf, NULL) == SQLITE_OK;
  for (int i = 0; ok && i < n; i++) {
    sqlite3_stmt *s = rows[i]->in_done && rows[i]->done_hash == rows[i]->hash ? ud : uf;
    sqlite3_reset(s);
    sqlite3_bind_text(s, 1, rows[i]->uid, -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 2, mark, -1, SQLITE_STATIC);
    if (sqlite3_step(s) != SQLITE_DONE) ok = 0;
  }
  if (!ok) sql_error(db, now, "marking unchanged rows");
  sqlite3_finalize(ud); sqlite3_finalize(uf);
  return finish(db, ok, now);
}

/* Store vectors for rows[0..m). On the very first write (*dim_io == 0) the
 * vec0 table is created and model + dim are recorded INSIDE the same
 * transaction, so the meta never names a model nothing was embedded with.
 * Returns 0, -1 (SQL failure, recorded), or -2 (dimension refused). */
static int write_rows(db_handle *db, pick **rows, int m, const float *vecs,
                      int dim, int *dim_io, const char *model,
                      const char *mark, const char *now) {
  if (*dim_io && dim != *dim_io) {
    char msg[300];
    snprintf(msg, sizeof msg, "server returned dim %d but the index is %d-d; "
             "refusing to mix (rebuild: DROP TABLE intel_vec, intel_vec_done, "
             "intel_vec_failed; DELETE FROM intel_vec_meta)", dim, *dim_io);
    meta_set(db, "refused", msg);
    fprintf(stderr, "[embed] %s\n", msg);
    return -2;
  }
  if (begin(db, now) != 0) return -1;
  int ok = 1, first = (*dim_io == 0);
  if (first) {
    ok = ensure_index(db, dim) == 0 &&
         meta_set_long(db, "dim", dim) == 0 &&
         meta_set(db, "model", model) == 0;
    if (!ok) sql_error(db, now, "creating the vector index");
  }
  sqlite3_stmt *del = NULL, *ins = NULL, *done = NULL, *unfail = NULL;
  if (ok &&
      (sqlite3_prepare_v2(db->h, "DELETE FROM " EMBED_VEC_TABLE " WHERE uid=?1", -1, &del, NULL) != SQLITE_OK ||
       sqlite3_prepare_v2(db->h, "INSERT INTO " EMBED_VEC_TABLE "(uid,embedding) VALUES(?1,?2)", -1, &ins, NULL) != SQLITE_OK ||
       sqlite3_prepare_v2(db->h, "INSERT INTO " EMBED_DONE_TABLE "(uid,text_hash,embedded_at) VALUES(?1,?2,?3) "
                                 "ON CONFLICT(uid) DO UPDATE SET text_hash=excluded.text_hash, embedded_at=excluded.embedded_at",
                          -1, &done, NULL) != SQLITE_OK ||
       sqlite3_prepare_v2(db->h, "DELETE FROM " EMBED_FAIL_TABLE " WHERE uid=?1", -1, &unfail, NULL) != SQLITE_OK)) {
    ok = 0;
    sql_error(db, now, "prepare");
  }
  for (int j = 0; ok && j < m; j++) {
    pick *p = rows[j];
    /* DELETE first, ALWAYS — not only when the row is known to have changed.
     * vec0 has no upsert, and a vec row without its done row (left by any
     * earlier partial write) would otherwise fail this INSERT forever. */
    sqlite3_reset(del); sqlite3_bind_text(del, 1, p->uid, -1, SQLITE_STATIC);
    if (sqlite3_step(del) != SQLITE_DONE) { ok = 0; sql_error(db, now, "delete from intel_vec"); break; }
    sqlite3_reset(ins);
    sqlite3_bind_text(ins, 1, p->uid, -1, SQLITE_STATIC);
    sqlite3_bind_blob(ins, 2, vecs + (size_t)j * dim, (int)(dim * sizeof(float)), SQLITE_STATIC);
    if (sqlite3_step(ins) != SQLITE_DONE) { ok = 0; sql_error(db, now, "insert into intel_vec"); break; }
    sqlite3_reset(done);
    sqlite3_bind_text(done, 1, p->uid, -1, SQLITE_STATIC);
    sqlite3_bind_int64(done, 2, (sqlite3_int64)p->hash);
    sqlite3_bind_text(done, 3, mark, -1, SQLITE_STATIC);
    if (sqlite3_step(done) != SQLITE_DONE) { ok = 0; sql_error(db, now, "insert into intel_vec_done"); break; }
    if (p->in_failed) {
      sqlite3_reset(unfail); sqlite3_bind_text(unfail, 1, p->uid, -1, SQLITE_STATIC);
      if (sqlite3_step(unfail) != SQLITE_DONE) { ok = 0; sql_error(db, now, "delete from intel_vec_failed"); break; }
    }
  }
  sqlite3_finalize(del); sqlite3_finalize(ins); sqlite3_finalize(done); sqlite3_finalize(unfail);
  if (finish(db, ok, now) != 0) return -1;
  if (first) *dim_io = dim;
  return 0;
}

/* Record one row the server will not embed. If an OLDER text of the row was
 * embedded, that vector is removed with it: the index must not keep claiming
 * a row by text the row no longer has. */
static int record_failure(db_handle *db, pick *p, const char *reason,
                          const char *mark, const char *now) {
  if (begin(db, now) != 0) return -1;
  int ok = 1;
  if (p->in_done) {
    ok = (!table_exists(db, EMBED_VEC_TABLE) ||
          exec_uid(db, "DELETE FROM " EMBED_VEC_TABLE " WHERE uid=?1", p->uid) == 0) &&
         exec_uid(db, "DELETE FROM " EMBED_DONE_TABLE " WHERE uid=?1", p->uid) == 0;
  }
  sqlite3_stmt *s = NULL;
  if (ok && sqlite3_prepare_v2(db->h,
        "INSERT INTO " EMBED_FAIL_TABLE "(uid,text_hash,failed_at,reason) VALUES(?1,?2,?3,?4) "
        "ON CONFLICT(uid) DO UPDATE SET text_hash=excluded.text_hash, "
        "failed_at=excluded.failed_at, reason=excluded.reason", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, p->uid, -1, SQLITE_STATIC);
    sqlite3_bind_int64(s, 2, (sqlite3_int64)p->hash);
    sqlite3_bind_text(s, 3, mark, -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 4, reason, -1, SQLITE_STATIC);
    ok = sqlite3_step(s) == SQLITE_DONE;
  } else ok = 0;
  sqlite3_finalize(s);
  if (!ok) sql_error(db, now, "recording a failed row");
  return finish(db, ok, now);
}

/* A failure that says nothing about the INPUT: nothing answered, it ran out
 * of time, or the server said "not now". Such a row is retried, never
 * recorded as failed. */
static int transient(llm_status st, long http) {
  if (st == LLM_ERR_UNREACHABLE || st == LLM_ERR_TIMEOUT) return 1;
  if (st == LLM_ERR_HTTP && (http == 0 || http == 503 || http == 429)) return 1;
  return st == LLM_ERR_BAD_REQUEST;           /* our side: OOM building JSON */
}

static void fail_reason(char *buf, size_t cap, llm_status st, long http) {
  if (st == LLM_ERR_HTTP) snprintf(buf, cap, "%s %ld", llm_status_code(st), http);
  else snprintf(buf, cap, "%s", llm_status_code(st));
}

/* The server works, in general, right now? One known-good input. Used only
 * when EVERY row of a split batch failed on its own — then either all of
 * those rows are bad, or the server is, and recording them as failed must
 * wait until it is clear which. */
static int server_answers(llm_client *llm, int dim) {
  const char *probe[1] = { "dimension probe" };
  float *pv = NULL; int pdim = 0; llm_status pst;
  int ok = llm_embed(llm, probe, 1, &pv, &pdim, 30000, &pst) == 0 &&
           (dim == 0 || pdim == dim);
  free(pv);
  return ok;
}

/* Embed and store rows[0..m). On return *n_done is how many LEADING rows are
 * settled (stored, or recorded as failed): the caller advances its keyset to
 * exactly there. Returns 0 (all m settled), -1 (stop the tick: the server or
 * the database failed, rows from *n_done on are untouched and will be
 * retried), -2 (refused: dimension mismatch). */
static int embed_rows(db_handle *db, llm_client *llm, pick **rows, int m,
                      int *dim_io, const char *model, const char *mark,
                      const char *now, tick_stats *ts, int *n_done) {
  *n_done = 0;
  if (m <= 0) return 0;
  const char *texts[256];
  for (int i = 0; i < m; i++) texts[i] = rows[i]->text;
  float *vecs = NULL; int dim = 0; llm_status st; long http = 0;
  ts->sent += m;
  if (llm_embed_ex(llm, texts, m, &vecs, &dim, 120000, &st, &http) == 0) {
    int w = write_rows(db, rows, m, vecs, dim, dim_io, model, mark, now);
    free(vecs);
    if (w != 0) return w;
    ts->embedded += m;
    *n_done = m;
    return 0;
  }
  char why[96];
  fail_reason(why, sizeof why, st, http);
  if (transient(st, http)) {
    char msg[200];
    snprintf(msg, sizeof msg, "%s: /v1/embeddings failed for a batch of %d (%s); "
             "retried next tick", now, m, why);
    meta_set(db, "last_error", msg);
    fprintf(stderr, "[embed] %s\n", msg);
    return -1;
  }

  /* The server ANSWERED and refused this batch. One bad input fails the whole
   * request, so find out which: one row at a time, in order. Failures are
   * held until the server is shown to work — by any row succeeding, or by
   * the probe — so a server that refuses everything never gets the corpus
   * recorded as "failed". */
  fprintf(stderr, "[embed] batch of %d refused (%s); retrying row by row\n", m, why);
  int pend[256], np = 0, any_ok = 0, rc = 0, stop = m;
  char preason[256][48];
  for (int i = 0; i < m; i++) {
    float *v1 = NULL; int d1 = 0; llm_status s1; long h1 = 0;
    const char *t1[1] = { rows[i]->text };
    if (llm_embed_ex(llm, t1, 1, &v1, &d1, 120000, &s1, &h1) == 0) {
      int w = write_rows(db, &rows[i], 1, v1, d1, dim_io, model, mark, now);
      free(v1);
      if (w != 0) { rc = w; stop = i; break; }
      ts->embedded++;
      any_ok = 1;
      continue;
    }
    if (transient(s1, h1)) {
      char msg[200], w1[96];
      fail_reason(w1, sizeof w1, s1, h1);
      snprintf(msg, sizeof msg, "%s: /v1/embeddings failed mid-split (%s); retried next tick",
               now, w1);
      meta_set(db, "last_error", msg);
      rc = -1; stop = i;
      break;
    }
    fail_reason(preason[np], sizeof preason[np], s1, h1);
    pend[np++] = i;
  }
  if (np > 0) {
    if (!any_ok && !server_answers(llm, *dim_io)) {
      char msg[200];
      snprintf(msg, sizeof msg, "%s: every row of a batch of %d was refused and the "
               "dimension probe failed too — treating it as a server fault; nothing "
               "recorded, retried next tick", now, m);
      meta_set(db, "last_error", msg);
      fprintf(stderr, "[embed] %s\n", msg);
      /* Nothing at or after the first refused row is settled. */
      if (pend[0] < stop) stop = pend[0];
      rc = -1;
    } else {
      for (int k = 0; k < np; k++) {
        if (pend[k] >= stop) break;
        char reason[160];
        snprintf(reason, sizeof reason, "embedding server refused this input (%s)",
                 preason[k]);
        if (record_failure(db, rows[pend[k]], reason, mark, now) != 0) {
          rc = -1; stop = pend[k];
          break;
        }
        ts->failed++;
        fprintf(stderr, "[embed] row %s recorded as failed: %s\n", rows[pend[k]]->uid, reason);
      }
    }
  }
  *n_done = stop;
  return rc;
}

static void refresh_counts(db_handle *db, const char *since, const char *types,
                           const char *now, int force) {
  long embedded = count_rows(db, EMBED_DONE_TABLE);
  meta_set_long(db, "embedded_count", embedded);
  /* embedded > eligible is impossible in reality and possible in the meta
   * only because eligible is refreshed lazily — a burst of new rows since the
   * last count. Do not let the coverage block publish a ratio above 1. */
  if (embedded > meta_long(db, "eligible_count")) force = 1;
  /* eligible_count is a full scan of intel_items (ROW_DATE is not indexed),
   * so it is refreshed at most hourly, and the timestamp of the count travels
   * with it. */
  char at[64];
  long refresh = env_long("JO_EMBED_ELIGIBLE_REFRESH_SEC", 3600);
  if (!force && meta_get(db, "eligible_counted_at", at, sizeof at) && at[0]) {
    long long then = parse_iso_ms(at);
    if (then > 0 && wall_ms() - then < refresh * 1000LL) return;
  }
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, SQL_ELIGIBLE, -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, since, -1, SQLITE_STATIC);
    if (types) sqlite3_bind_text(s, 2, types, -1, SQLITE_STATIC); else sqlite3_bind_null(s, 2);
    if (sqlite3_step(s) == SQLITE_ROW) {
      meta_set_long(db, "eligible_count", sqlite3_column_int64(s, 0));
      meta_set(db, "eligible_counted_at", now);
    }
    sqlite3_finalize(s);
  }
}

/* Persist the keyset of the walk in progress. */
static void save_keyset(db_handle *db, walk_kind w, const char *k, const char *u) {
  if (w == WALK_DELTA) { meta_set(db, "dwm_f", k); meta_set(db, "dwm_u", u); }
  else {
    meta_set(db, "wm_phase", w == WALK_FULL_A ? "A" : "B");
    meta_set(db, "wm_p", k); meta_set(db, "wm_u", u);
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                               /* writes intel_vec, not intel */
  const char *base = embed_base_url();
  if (!base) {
    fprintf(stderr, "[embed] disabled: JO_EMBED_URL is unset; /api/intel/semantic "
                    "will answer 503 with coverage.enabled=false\n");
    return 0;
  }
  /* Own connection: every batch is a transaction and a transaction belongs
   * to a connection, not a thread (db.h). */
  db_handle own;
  db_handle *db = db_worker_open(&own, ctx->db);
  ensure_meta(db);
  ensure_side_tables(db);
  long long tick_ms = wall_ms();
  char now[40], mark[40];
  iso_ms(now, sizeof now, tick_ms);
  iso_ms(mark, sizeof mark, tick_ms - EMBED_OVERLAP_SEC * 1000LL);
  meta_set(db, "last_run_at", now);

  /* ---- who is the server? --------------------------------------------- */
  char model[256], stored_model[256];
  if (embed_detect_model(ctx->http, base, model, sizeof model) != 0) {
    char msg[400];
    snprintf(msg, sizeof msg, "%s: cannot identify the embedding model (%.200s); "
             "tick skipped, nothing embedded — set JO_EMBED_MODEL or fix the "
             "server's /v1/models", now, model);
    meta_set(db, "last_error", msg);
    fprintf(stderr, "[embed] %s\n", msg);
    db_worker_close(&own);
    return -1;
  }
  int dim = (int)meta_long(db, "dim");
  meta_get(db, "model", stored_model, sizeof stored_model);
  if (stored_model[0] && strcmp(stored_model, model) != 0) {
    /* An older build recorded the literal "unknown" when /v1/models failed on
     * its first tick. With nothing embedded under it that placeholder names
     * no index and is simply cleared; with rows under it, which model made
     * them cannot be shown, so it is refused like any other mismatch. */
    if (!strcmp(stored_model, "unknown") &&
        (dim == 0 || count_rows(db, EMBED_DONE_TABLE) == 0)) {
      fprintf(stderr, "[embed] clearing the placeholder model 'unknown' (nothing "
                      "was embedded under it)\n");
      meta_set(db, "model", NULL);
      meta_set(db, "dim", NULL);
      /* No done rows means the vec table (if any) holds nothing the index
       * claims; dropped so the first write creates it at the server's width. */
      sqlite3_exec(db->h, "DROP TABLE IF EXISTS " EMBED_VEC_TABLE, NULL, NULL, NULL);
      stored_model[0] = 0;
      dim = 0;
    } else {
      char msg[600];
      snprintf(msg, sizeof msg, "index was built with model '%.200s' but the server "
               "is '%.200s'; refusing to mix (rebuild: DROP TABLE intel_vec, "
               "intel_vec_done, intel_vec_failed; DELETE FROM intel_vec_meta)",
               stored_model, model);
      meta_set(db, "refused", msg);
      fprintf(stderr, "[embed] %s\n", msg);
      db_worker_close(&own);
      return -1;
    }
  }
  if (dim && ensure_index(db, dim) != 0) {
    sql_error(db, now, "opening the vector index");
    db_worker_close(&own);
    return -1;
  }

  llm_client llm = { .http = ctx->http, .base_url = base, .interactive = 0 };
  /* Probe the dimension UP FRONT when an index exists. Without this a server
   * swapped for one of a different width is only noticed when a row needs
   * embedding, so a quiet corpus could sit behind a mismatched server for
   * days with every query embedded into the wrong space. One tiny request
   * per tick buys the refusal at the moment the mismatch appears. */
  if (dim) {
    const char *probe[1] = { "dimension probe" };
    float *pv = NULL; int pdim = 0; llm_status pst;
    if (llm_embed(&llm, probe, 1, &pv, &pdim, 30000, &pst) != 0) {
      char msg[160];
      snprintf(msg, sizeof msg, "%s: /v1/embeddings unreachable for the dimension probe (%s)",
               now, llm_status_code(pst));
      meta_set(db, "last_error", msg);
      fprintf(stderr, "[embed] %s\n", msg);
      db_worker_close(&own);
      return -1;
    }
    free(pv);
    if (pdim != dim) {
      char msg[240];
      snprintf(msg, sizeof msg, "server returns %d-d vectors but the index is %d-d; "
               "refusing to mix (rebuild: DROP TABLE intel_vec, intel_vec_done, "
               "intel_vec_failed; DELETE FROM intel_vec_meta)", pdim, dim);
      meta_set(db, "refused", msg);
      fprintf(stderr, "[embed] %s\n", msg);
      db_worker_close(&own);
      return -1;
    }
    /* An index with a dimension and no model (meta edited by hand) gets the
     * model the dimension probe just vouched for. */
    if (!stored_model[0]) meta_set(db, "model", model);
  }
  meta_set(db, "refused", NULL);

  /* ---- which walk, from where ----------------------------------------- */
  char since[40]; iso_ms(since, sizeof since, tick_ms - since_days() * 86400000LL);
  char *types = record_types_json();
  char full_done[64], wm_k[256], wm_u[600], sweep_start[64], ph[8];
  walk_kind walk;
  meta_get(db, "full_done_at", full_done, sizeof full_done);
  if (!full_done[0]) {
    meta_get(db, "wm_phase", ph, sizeof ph);
    walk = (ph[0] == 'B') ? WALK_FULL_B : WALK_FULL_A;
    meta_get(db, "wm_p", wm_k, sizeof wm_k);
    meta_get(db, "wm_u", wm_u, sizeof wm_u);
    if (!meta_get(db, "sweep_start", sweep_start, sizeof sweep_start) || !sweep_start[0]) {
      snprintf(sweep_start, sizeof sweep_start, "%s", now);
      meta_set(db, "sweep_start", now);
    }
  } else {
    walk = WALK_DELTA;
    if (!meta_get(db, "dwm_f", wm_k, sizeof wm_k) || !wm_k[0]) {
      /* First delta tick, or a database from before the keyset was kept:
       * start one overlap before the full sweep's mark. */
      long long fd = parse_iso_ms(full_done);
      if (fd > 0) iso_ms(wm_k, sizeof wm_k, fd - EMBED_OVERLAP_SEC * 1000LL);
      else snprintf(wm_k, sizeof wm_k, "%s", full_done);
      wm_u[0] = 0;
    } else meta_get(db, "dwm_u", wm_u, sizeof wm_u);
  }
  refresh_counts(db, since, types, now, 0);

  /* ---- the walk ------------------------------------------------------- */
  int bs = batch_size(), budget = max_per_run(), rc = 0;
  long walk_cap = max_walk();
  tick_stats ts = {0};
  const char *stopped = "drained";
  pick rows[SEL_CHUNK];
  for (;;) {
    if (ctx->cancel && *ctx->cancel) { stopped = "cancelled"; break; }
    if (ts.sent >= budget) { stopped = "send_budget"; break; }
    if (ts.walked >= walk_cap) { stopped = "walk_budget"; break; }
    int want = SEL_CHUNK;
    if (walk != WALK_DELTA && budget - ts.sent < want) want = (int)(budget - ts.sent);
    int n = select_batch(db, walk, since, types, wm_k, wm_u, want, rows);
    if (n < 0) { sql_error(db, now, "selecting rows"); rc = -1; stopped = "error"; break; }
    if (n == 0) {
      if (walk == WALK_FULL_A) {
        /* Phase A is exhausted; phase B (non-ISO dates below the cutoff by
         * text, inside it by fetch time) is the rest of the full sweep. */
        walk = WALK_FULL_B; wm_k[0] = wm_u[0] = 0;
        meta_set(db, "wm_phase", "B"); meta_set(db, "wm_p", NULL); meta_set(db, "wm_u", NULL);
        continue;
      }
      if (walk == WALK_FULL_B) {
        /* The full sweep is complete: from now on only what arrived since it
         * began needs looking at. Its watermark is retired; the delta keyset
         * starts one overlap before the sweep began. */
        meta_set(db, "full_done_at", sweep_start);
        meta_set(db, "wm_phase", NULL); meta_set(db, "wm_p", NULL); meta_set(db, "wm_u", NULL);
        long long ss = parse_iso_ms(sweep_start);
        char dk[40];
        if (ss > 0) iso_ms(dk, sizeof dk, ss - EMBED_OVERLAP_SEC * 1000LL);
        else snprintf(dk, sizeof dk, "%s", sweep_start);
        meta_set(db, "dwm_f", dk); meta_set(db, "dwm_u", "");
        fprintf(stderr, "[embed] full sweep complete (started %s); switching to delta\n", sweep_start);
      }
      break;
    }
    /* Split the page: rows already settled with this exact text are marked
     * (cheap); everything else is sent, in page order. */
    pick *seen[SEL_CHUNK], *need[SEL_CHUNK];
    int idx_need[SEL_CHUNK], ns = 0, nn = 0;
    for (int i = 0; i < n; i++) {
      pick *p = &rows[i];
      if ((p->in_done && p->done_hash == p->hash) ||
          (!p->in_done && p->in_failed && p->failed_hash == p->hash)) {
        seen[ns++] = p;
        if (p->in_done) ts.unchanged++; else ts.failed_seen++;
      } else { idx_need[nn] = i; need[nn++] = p; }
    }
    int settled = n;              /* leading rows of the page that are done */
    if (mark_seen(db, seen, ns, mark, now) != 0) { rc = -1; settled = 0; stopped = "error"; }
    for (int off = 0; rc == 0 && off < nn; ) {
      if (ts.sent >= budget || (ctx->cancel && *ctx->cancel)) {
        /* Everything before the first unsent row is settled; the rest of
         * the page is the next tick's. */
        settled = idx_need[off];
        stopped = ts.sent >= budget ? "send_budget" : "cancelled";
        break;
      }
      int cnt = nn - off < bs ? nn - off : bs;
      if (cnt > budget - ts.sent) cnt = (int)(budget - ts.sent);
      int done_n = 0;
      int e = embed_rows(db, &llm, &need[off], cnt, &dim, model, mark, now, &ts, &done_n);
      if (e != 0) {
        settled = off + done_n < nn ? idx_need[off + done_n] : n;
        rc = -1;
        stopped = e == -2 ? "refused" : "error";
        break;
      }
      off += cnt;
    }
    ts.walked += settled;
    if (settled > 0) {
      snprintf(wm_k, sizeof wm_k, "%s", rows[settled - 1].key);
      snprintf(wm_u, sizeof wm_u, "%s", rows[settled - 1].uid);
      save_keyset(db, walk, wm_k, wm_u);
    }
    for (int i = 0; i < n; i++) pick_free(&rows[i]);
    if (rc != 0 || settled < n) break;
  }

  if (walk == WALK_DELTA) {
    /* Clamp the delta keyset back to this tick's mark (see TIMESTAMPS): rows
     * stamped just before the tick but committed after it looked are inside
     * that window, and rows already settled there are excluded by their own
     * mark, so the re-walk costs an index range and nothing else. */
    if (!wm_k[0] || strcmp(wm_k, mark) > 0) {
      snprintf(wm_k, sizeof wm_k, "%s", mark);
      wm_u[0] = 0;
      save_keyset(db, walk, wm_k, wm_u);
    }
  }

  char lt[400];
  snprintf(lt, sizeof lt,
           "{\"at\":\"%s\",\"walk\":\"%s\",\"walked\":%ld,\"sent\":%ld,"
           "\"embedded\":%ld,\"unchanged\":%ld,\"failed\":%ld,"
           "\"failed_skipped\":%ld,\"stopped\":\"%s\",\"send_budget\":%d,"
           "\"walk_budget\":%ld}",
           now, walk == WALK_DELTA ? "delta" : walk == WALK_FULL_A ? "full_a" : "full_b",
           ts.walked, ts.sent, ts.embedded, ts.unchanged, ts.failed,
           ts.failed_seen, stopped, budget, walk_cap);
  meta_set(db, "last_tick", lt);
  refresh_counts(db, since, types, now, 0);
  /* A tick that ended cleanly — including one with nothing to embed — has
   * proven the server answers, so a stale last_error from an earlier outage
   * must not keep showing in coverage. */
  if (rc == 0) meta_set(db, "last_error", NULL);
  fprintf(stderr, "[embed] %s tick: walked %ld, sent %ld, embedded %ld, unchanged %ld, "
          "failed %ld (+%ld skipped), stopped=%s; index now %ld row(s) at %d-d (%s)\n",
          walk == WALK_DELTA ? "delta" : "full", ts.walked, ts.sent, ts.embedded,
          ts.unchanged, ts.failed, ts.failed_seen, stopped,
          meta_long(db, "embedded_count"), dim, model);
  free(types);
  db_worker_close(&own);
  return rc;
}

static const source_def embed_def = {
  .id = EMBED_SID, .collector = "_maint",
  .name = "Semantic Embedding Backfill",
  .name_ja = "セマンティック埋め込み補完",
  .description = "Embeds intel_items title+summary into the sqlite-vec index "
                 "behind /api/intel/semantic. Inert unless JO_EMBED_URL is set.",
  .url = "internal://embedding-backfill",
  .update_interval_sec = 120, .run = run,
  .category = "maintenance" };
REGISTER_SOURCE(embed_def)
