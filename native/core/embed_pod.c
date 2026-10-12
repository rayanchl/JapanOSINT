/* core/embed_pod.c — see embed_pod.h.
 *
 * SHAPE. A `_maint` pod on a 120 s interval, the same pattern as
 * collectors/pod/simhash_pod.c: `_maint` because it emits no intel_items (a
 * real collector id would trip records_drop against a permanent records=0),
 * and the scheduler's skip-if-running rule serialises ticks so two sweeps
 * never interleave. The index itself — vec0 table, done/fail side tables,
 * identity, the split-batch retry, coverage, KNN — is lib/vecindex; what is
 * left here is what is particular to intel_items: which rows, in what order,
 * with which metadata.
 *
 * WHICH ROWS ARE ELIGIBLE. A row is in scope when its DATE is within
 * JO_EMBED_SINCE_DAYS, and its date is published_at only when published_at
 * looks like ISO-8601 text (`GLOB '[0-9][0-9][0-9][0-9]-*'`), else fetched_at.
 * It used to be COALESCE(published_at, fetched_at) compared as TEXT against an
 * ISO cutoff, and collectors store published_at in whatever shape the upstream
 * used: an epoch ("1727000000") or a dd/mm/yyyy date sorts below "2026-…"
 * and never qualified, an RFC 822 date ("Mon, 02 Oct …") sorts above it and
 * always did. ROW_DATE below is the one definition; the walk, the delta, the
 * eligible_count coverage reports and the `ts` metadata column all use it.
 *
 * METADATA INSIDE THE KNN. intel_vec_v2 carries tenant_id (the PARTITION KEY),
 * source_id, ts (epoch seconds of ROW_DATE) and record_type, so a query
 * filters INSIDE the nearest-neighbour search instead of after it. Filtering
 * after it was a recall bug, not a style point: /api/intel/semantic asked for
 * the k nearest across EVERY tenant and then dropped the ones the caller may
 * not see, so a tenant whose rows sat below k other-tenant rows got nothing.
 * A partition per tenant also means a tenant's query never reads another
 * tenant's chunks.
 *
 * WHY intel_vec_v2 AND NOT intel_vec. vec0 columns cannot be added to an
 * existing table, and a vec0 table must never be renamed (lib/vecindex.h). So
 * the metadata index is a NEW table, and an index built before it existed
 * (intel_vec: uid + embedding only) is MIGRATED: vectors are copied into
 * intel_vec_v2 in batches with their metadata joined from intel_items — no
 * row is re-embedded — and the old table is dropped in the same transaction
 * that marks the migration done. Until then the query side reads the old
 * table (without in-KNN filters) and the pod embeds nothing new, so the two
 * tables never disagree about what is embedded.
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
 *      INSERT on the vec0 table); one whose text is the same but whose
 *      metadata moved (published_at rewritten on re-fetch) is rewritten from
 *      its stored vector, not re-embedded.
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
 * marks nothing and is retried next tick. (vecindex_embed_items.)
 *
 * MODEL IDENTITY. The model is JO_EMBED_MODEL, else what GET /v1/models says.
 * If neither answers, the tick is skipped with last_error set — "unknown" is
 * never recorded, because a placeholder recorded on a bad first tick made
 * every later (successful) detection a "model change" and the pod refused
 * forever. The model is persisted together with the dimension, in the same
 * transaction as the first rows written.
 *
 * THE THREAD IT RUNS ON. llm_embed() routes through core/llm_worker.c, which
 * keys its worker threads on base_url; JO_EMBED_URL is a different host:port
 * from LLM_BASE_URL, so the embedding pod owns its own worker and never sits
 * in the generation queue. A user's semantic QUERY shares this worker (same
 * base_url) on the interactive lane, and bounds its wait in the queue
 * (llm_client.bound_queue_wait) so it cannot be held for a whole batch. The
 * post-batch hooks (embed_hook_register) run here too, on the maintenance
 * lane, after each batch commits. */
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
/* Vectors copied per migration transaction (legacy intel_vec → v2). Each row
 * is a point read from the old vec0 table plus a write to the new one; a few
 * thousand keeps one transaction's journal small while a 100k-row index still
 * migrates in well under a minute. */
#define MIGRATE_BATCH 2000

#define EMBED_REBUILD_HINT \
  " (rebuild: DROP TABLE " EMBED_VEC_TABLE ", " EMBED_LEGACY_VEC_TABLE ", " \
  EMBED_DONE_TABLE ", " EMBED_FAIL_TABLE "; DELETE FROM " EMBED_META_TABLE ")"

/* tenant_id first: it is the partition key. */
static const vecindex_col INTEL_COLS[] = {
  { "tenant_id",   VI_TEXT, 1 },
  { "source_id",   VI_TEXT, 0 },
  { "ts",          VI_INT,  0 },
  { "record_type", VI_TEXT, 0 },
};
enum { COL_TENANT, COL_SOURCE, COL_TS, COL_RTYPE, NCOLS };

const vecindex_spec embed_intel_spec = {
  .name = "intel", .table = EMBED_VEC_TABLE, .key_col = "uid",
  .meta_table = EMBED_META_TABLE, .done_table = EMBED_DONE_TABLE,
  .fail_table = EMBED_FAIL_TABLE, .cols = INTEL_COLS, .ncols = NCOLS,
  .rebuild_hint = EMBED_REBUILD_HINT };
VECINDEX_REGISTER(embed_intel_spec)

/* The index as it was before metadata columns: same meta/done/fail tables,
 * a vec0 table with only uid + embedding. Read during a migration, never
 * written. */
static const vecindex_spec legacy_spec = {
  .name = "intel_legacy", .table = EMBED_LEGACY_VEC_TABLE, .key_col = "uid",
  .meta_table = EMBED_META_TABLE, .done_table = EMBED_DONE_TABLE,
  .fail_table = EMBED_FAIL_TABLE, .cols = NULL, .ncols = 0,
  .rebuild_hint = EMBED_REBUILD_HINT };

#define SPEC (&embed_intel_spec)

/* ---- env knobs ----------------------------------------------------------- */

static long env_long(const char *k, long dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  char *end = NULL;
  long n = strtol(v, &end, 10);
  return (end && end != v) ? n : dflt;
}

const char *embed_base_url(void)  { return vecindex_base_url(); }
const char *embed_query_url(void) { return vecindex_query_url(); }

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
 * the default (everything but the two collector notices and the search-run
 * summaries — see SEL_TYPE). */
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

char *embed_utf8_sanitize(const char *text, size_t *replaced) {
  return vecindex_utf8_sanitize(text, replaced);
}

char *embed_bound_text(const char *text) {
  return vecindex_bound_text(text, (size_t)max_chars());
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

/* ---- meta table (thin names over vecindex; the tests reach these) -------- */

static int  meta_get(db_handle *db, const char *k, char *out, size_t cap) { return vecindex_meta_get(db, SPEC, k, out, cap); }
static int  meta_set(db_handle *db, const char *k, const char *v) { return vecindex_meta_set(db, SPEC, k, v); }
static int  meta_set_long(db_handle *db, const char *k, long v) { return vecindex_meta_set_long(db, SPEC, k, v); }
static long meta_long(db_handle *db, const char *k) { return vecindex_meta_long(db, SPEC, k); }
static int  table_exists(db_handle *db, const char *name) { return vecindex_table_exists(db, name); }
static long count_rows(db_handle *db, const char *table) { return vecindex_count(db, table); }
static void ensure_meta(db_handle *db) { vecindex_ensure_meta(db, SPEC); }

/* Which vec0 table answers queries right now: the legacy one while a
 * migration is pending (it still holds every vector), else intel_vec_v2. */
const vecindex_spec *embed_query_spec(db_handle *db) {
  if (db && db->h && table_exists(db, EMBED_LEGACY_VEC_TABLE)) return &legacy_spec;
  return SPEC;
}

int embed_table_exists(db_handle *db) {
  return db && db->h && vecindex_exists(db, embed_query_spec(db));
}

int embed_index_dim(db_handle *db, char *model, size_t cap) {
  if (model && cap) model[0] = 0;
  if (!db || !db->h || !table_exists(db, EMBED_META_TABLE)) return 0;
  if (model && cap) meta_get(db, "model", model, cap);
  return (int)meta_long(db, "dim");
}

int embed_index_refused(db_handle *db, char *why, size_t cap) {
  return vecindex_refused(db, SPEC, why, cap);
}

cJSON *embed_coverage_json(db_handle *db) {
  /* The common shape first (lib/vecindex), then what only this index has. */
  cJSON *o = vecindex_coverage_json(db, embed_query_spec(db));
  const char *url = embed_base_url();
  cJSON_AddBoolToObject(o, "enabled", url != NULL);
  /* Where a user's query is embedded, when it is not the backfill's server:
   * an operator reading a slow query wants to know whether it had a slot of
   * its own. Null when queries share JO_EMBED_URL's worker. */
  { const char *q = embed_query_url();
    if (url && q && strcmp(q, url)) cJSON_AddStringToObject(o, "query_url", q);
    else cJSON_AddNullToObject(o, "query_url"); }
  /* Which filters run INSIDE the KNN: none while a migration is pending. */
  cJSON_AddBoolToObject(o, "knn_filters", embed_query_spec(db) == SPEC);
  char buf[600];
  int have_meta = db && db->h && table_exists(db, EMBED_META_TABLE);
  cJSON_AddNumberToObject(o, "since_days", (double)since_days());
  char *rt = record_types_json();
  if (rt) { cJSON *a = cJSON_Parse(rt); cJSON_AddItemToObject(o, "record_types", a ? a : cJSON_CreateNull()); free(rt); }
  else cJSON_AddStringToObject(o, "record_types",
         "all except collector-truncation-notice, collector-shape-notice, "
         "osint_search_run");
  cJSON_AddStringToObject(o, "date_rule",
         "published_at when it is ISO-8601 text, else fetched_at");
  cJSON_AddNumberToObject(o, "max_chars", (double)max_chars());
  if (have_meta && meta_get(db, "migrate_wm", buf, sizeof buf)) {
    /* A migration in progress (or finished, with its counts kept). */
    cJSON *m = cJSON_CreateObject();
    cJSON_AddStringToObject(m, "state", table_exists(db, EMBED_LEGACY_VEC_TABLE)
                                        ? "in_progress" : "done");
    cJSON_AddNumberToObject(m, "copied", (double)meta_long(db, "migrate_copied"));
    cJSON_AddNumberToObject(m, "orphans_dropped", (double)meta_long(db, "migrate_orphans"));
    cJSON_AddNumberToObject(m, "missing_vectors", (double)meta_long(db, "migrate_missing"));
    cJSON_AddItemToObject(o, "migration", m);
  }
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
  return o;
}

/* ---- model identity ------------------------------------------------------ */

int embed_detect_model(http_client *http, const char *base, char *out,
                       size_t cap) {
  return vecindex_detect_model(http, base, out, cap);
}
int embed_live_model(char *out, size_t cap) { return vecindex_live_model(out, cap); }
int embed_live_query_model(char *out, size_t cap) { return vecindex_live_query_model(out, cap); }

/* ---- post-batch hooks ---------------------------------------------------- */

typedef struct { embed_hook_fn fn; void *user; } hook_box;

/* Adapt the generic batch (vecindex_val[n * 4]) to the typed row view. */
static void hook_trampoline(db_handle *db, const vecindex_batch *b, void *user) {
  const hook_box *hb = user;
  embed_row_meta *rm = calloc((size_t)b->n, sizeof *rm);
  if (!rm) return;
  for (int i = 0; i < b->n; i++) {
    const vecindex_val *v = b->vals ? b->vals + (size_t)i * NCOLS : NULL;
    rm[i].uid         = b->keys[i];
    rm[i].tenant_id   = v ? v[COL_TENANT].s : NULL;
    rm[i].source_id   = v ? v[COL_SOURCE].s : NULL;
    rm[i].ts          = v ? v[COL_TS].i : 0;
    rm[i].record_type = v ? v[COL_RTYPE].s : NULL;
  }
  hb->fn(db, b->n, b->keys, b->vecs, b->dim, rm, hb->user);
  free(rm);
}

int embed_hook_register(embed_hook_fn fn, void *user) {
  if (!fn) return -1;
  hook_box *hb = malloc(sizeof *hb);           /* lives as long as the hook */
  if (!hb) return -1;
  hb->fn = fn; hb->user = user;
  if (vecindex_hook_register(SPEC, hook_trampoline, hb) != 0) { free(hb); return -1; }
  return 0;
}

/* ---- the sweep ----------------------------------------------------------- */

/* One row picked up by a selection query. `it` borrows the strings below. */
typedef struct {
  vecindex_item it;
  char *uid, *text, *key;           /* key = the keyset column of its walk */
  char *tenant, *source, *rtype;
  vecindex_val vals[NCOLS];
} pick;

static void pick_free(pick *p) {
  free(p->uid); free(p->text); free(p->key);
  free(p->tenant); free(p->source); free(p->rtype);
  p->uid = p->text = p->key = p->tenant = p->source = p->rtype = NULL;
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
#define ISO_PUB  EMBED_ISO_PUB
#define EFF_DATE "COALESCE(intel_items.published_at,intel_items.fetched_at)"
/* THE date of a row — see WHICH ROWS ARE ELIGIBLE. Defined in embed_pod.h so
 * a query side that checks a row against a `ts` filter reads the same rule. */
#define ROW_DATE EMBED_ROW_DATE
#define ROW_TS   EMBED_ROW_TS
#define SEL_COLS(KEY) \
  "intel_items.uid, intel_items.title, intel_items.summary, " \
  "substr(intel_items.body,1,512), " KEY ", " \
  "d.text_hash, (d.uid IS NOT NULL), f.text_hash, (f.uid IS NOT NULL), " \
  "intel_items.tenant_id, intel_items.source_id, " ROW_TS ", " \
  "intel_items.record_type, d.meta_hash"
#define SEL_FROM \
  " FROM intel_items " \
  "LEFT JOIN " EMBED_DONE_TABLE " d ON d.uid = intel_items.uid " \
  "LEFT JOIN " EMBED_FAIL_TABLE " f ON f.uid = intel_items.uid "
/* osint_search_run is the pipeline's own synthesis (core/pipeline.c): an
 * LLM's prose about collected rows, written back as an intel item. Embedded,
 * it would come back from /api/intel/semantic ranked beside the records it
 * paraphrases, as if it had been collected — and the next synthesis would
 * cite it as evidence. Not a finding; not in the index. */
#define SEL_TYPE \
  "((?2 IS NULL AND (intel_items.record_type IS NULL OR intel_items.record_type " \
  "NOT IN ('collector-truncation-notice','collector-shape-notice'," \
  "'osint_search_run'))) OR " \
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

static char *col_dup(sqlite3_stmt *s, int i) {
  const char *v = (const char *)sqlite3_column_text(s, i);
  return strdup(v ? v : "");
}

/* Point a pick's item at its own strings and metadata. */
static void pick_bind(pick *p) {
  p->vals[COL_TENANT] = VI_S(p->tenant);
  p->vals[COL_SOURCE] = VI_S(p->source);
  p->vals[COL_RTYPE]  = VI_S(p->rtype);
  p->it.key  = p->uid;
  p->it.text = p->text;
  p->it.vals = p->vals;
  p->it.hash = vecindex_hash(p->text);
  p->it.meta_hash = vecindex_vals_hash(SPEC, p->vals);
}

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
    p->key    = col_dup(s, 4);
    p->tenant = col_dup(s, 9);
    p->source = col_dup(s, 10);
    p->rtype  = col_dup(s, 12);
    if (!p->uid || !p->text || !p->key || !p->tenant || !p->source || !p->rtype) {
      pick_free(p); break;
    }
    p->vals[COL_TS] = VI_I(sqlite3_column_int64(s, 11));
    pick_bind(p);
    p->it.done_hash     = (unsigned long long)sqlite3_column_int64(s, 5);
    p->it.in_done       = sqlite3_column_int(s, 6);
    p->it.failed_hash   = (unsigned long long)sqlite3_column_int64(s, 7);
    p->it.in_failed     = sqlite3_column_int(s, 8);
    p->it.has_done_meta = sqlite3_column_type(s, 13) != SQLITE_NULL;
    p->it.done_meta_hash = (unsigned long long)sqlite3_column_int64(s, 13);
    n++;
  }
  sqlite3_finalize(s);
  return n;
}

/* Record an SQL failure as data and on stderr. */
static void sql_error(db_handle *db, const char *now, const char *what) {
  vecindex_sql_error(db, SPEC, now, what);
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

/* ---- migration: intel_vec (no metadata) → intel_vec_v2 ------------------- */

/* Walk intel_vec_done in uid order (an ordinary B-tree — a keyset over the
 * vec0 table itself would be a full scan per page), read each vector from the
 * old table by primary key, and write it with its metadata into the new one.
 *
 *   - a done row whose intel_items row is gone is an ORPHAN: no query could
 *     ever show it (materialisation joins intel_items), it has no tenant to
 *     partition on, and the query side used to report it as index_orphans.
 *     Its done row is deleted and it is counted, not copied.
 *   - a done row whose vector is missing from the old table (a partial write
 *     from before BEGIN was checked) loses its done row, so the walk embeds
 *     it again — the only rows a migration ever sends to the server, later,
 *     through the ordinary walk.
 *
 * Every batch is one transaction with the watermark in it, so a cancelled or
 * crashed migration resumes exactly where it stopped. The last transaction
 * DROPs the old table — the switch and the drop cannot be seen apart.
 * Returns 0 when no migration is pending (anymore), 1 when it stopped early
 * (cancelled), -1 on an SQL failure (recorded). */
static int migrate_legacy(db_handle *db, const source_ctx *ctx, const char *now) {
  if (!table_exists(db, EMBED_LEGACY_VEC_TABLE)) return 0;
  int dim = (int)meta_long(db, "dim");
  if (dim <= 0) {
    /* No dimension recorded: nothing the index claims lives in that table. */
    sqlite3_exec(db->h, "DROP TABLE IF EXISTS " EMBED_LEGACY_VEC_TABLE, NULL, NULL, NULL);
    return 0;
  }
  if (vecindex_create(db, SPEC, dim) != 0) { sql_error(db, now, "creating " EMBED_VEC_TABLE); return -1; }
  char wm[600];
  meta_get(db, "migrate_wm", wm, sizeof wm);
  if (!wm[0]) {
    fprintf(stderr, "[embed] migrating %s (%ld vectors) into %s with metadata "
            "columns; nothing is re-embedded\n", EMBED_LEGACY_VEC_TABLE,
            count_rows(db, EMBED_DONE_TABLE), EMBED_VEC_TABLE);
  }
  const char *sel =
    "SELECT d.uid, d.text_hash, d.embedded_at, (intel_items.uid IS NOT NULL), "
    "intel_items.tenant_id, intel_items.source_id, " ROW_TS ", "
    "intel_items.record_type "
    "FROM " EMBED_DONE_TABLE " d LEFT JOIN intel_items ON intel_items.uid = d.uid "
    "WHERE d.uid > ?1 ORDER BY d.uid LIMIT ?2";
  for (;;) {
    if (ctx && ctx->cancel && *ctx->cancel) return 1;
    if (vecindex_begin(db) != 0) { sql_error(db, now, "BEGIN IMMEDIATE"); return -1; }
    sqlite3_stmt *s = NULL, *undone = NULL;
    vecindex_writer w;
    int ok = sqlite3_prepare_v2(db->h, sel, -1, &s, NULL) == SQLITE_OK &&
             sqlite3_prepare_v2(db->h, "DELETE FROM " EMBED_DONE_TABLE " WHERE uid=?1",
                                -1, &undone, NULL) == SQLITE_OK;
    int have_w = ok && vecindex_writer_open(&w, db, SPEC) == 0;
    ok = ok && have_w;
    long copied = 0, orphans = 0, missing = 0, seen = 0;
    char last[600] = "";
    if (ok) {
      sqlite3_bind_text(s, 1, wm, -1, SQLITE_STATIC);
      sqlite3_bind_int(s, 2, MIGRATE_BATCH);
      while (ok && sqlite3_step(s) == SQLITE_ROW) {
        const char *uid = (const char *)sqlite3_column_text(s, 0);
        if (!uid) continue;
        snprintf(last, sizeof last, "%s", uid);
        seen++;
        int gone = !sqlite3_column_int(s, 3);
        float *v = NULL; int d = 0;
        int lr = gone ? 1 : vecindex_load(db, &legacy_spec, uid, &v, &d);
        if (gone || lr != 0 || d != dim) {
          sqlite3_reset(undone);
          sqlite3_bind_text(undone, 1, uid, -1, SQLITE_STATIC);
          ok = sqlite3_step(undone) == SQLITE_DONE;
          if (gone) orphans++; else missing++;
          free(v);
          continue;
        }
        const char *ten = (const char *)sqlite3_column_text(s, 4);
        const char *src = (const char *)sqlite3_column_text(s, 5);
        const char *rty = (const char *)sqlite3_column_text(s, 7);
        vecindex_val vals[NCOLS];
        vals[COL_TENANT] = VI_S(ten ? ten : "");
        vals[COL_SOURCE] = VI_S(src ? src : "");
        vals[COL_TS]     = VI_I(sqlite3_column_int64(s, 6));
        vals[COL_RTYPE]  = VI_S(rty ? rty : "");
        /* The done row keeps its text hash and embedded_at: the migration
         * changes where the vector lives, not when it was last looked at. */
        const char *at = (const char *)sqlite3_column_text(s, 2);
        char atb[64];
        snprintf(atb, sizeof atb, "%s", at ? at : now);
        ok = vecindex_writer_put(&w, uid, v, d, vals,
                                 (unsigned long long)sqlite3_column_int64(s, 1),
                                 vecindex_vals_hash(SPEC, vals), atb) == 0;
        free(v);
        if (ok) copied++;
      }
    }
    if (have_w) vecindex_writer_close(&w);
    sqlite3_finalize(s); sqlite3_finalize(undone);
    int done = ok && seen == 0;
    if (ok && seen) {
      ok = meta_set(db, "migrate_wm", last) == 0 &&
           meta_set_long(db, "migrate_copied", meta_long(db, "migrate_copied") + copied) == 0 &&
           meta_set_long(db, "migrate_orphans", meta_long(db, "migrate_orphans") + orphans) == 0 &&
           meta_set_long(db, "migrate_missing", meta_long(db, "migrate_missing") + missing) == 0;
      snprintf(wm, sizeof wm, "%s", last);
    }
    if (done) {
      /* The switch: the old table goes in the same transaction that makes
       * the new one the only one. Never a RENAME (lib/vecindex.h). The
       * watermark key stays (non-empty) so coverage can report the counts. */
      ok = sqlite3_exec(db->h, "DROP TABLE " EMBED_LEGACY_VEC_TABLE, NULL, NULL, NULL) == SQLITE_OK &&
           meta_set(db, "migrate_wm", wm[0] ? wm : "(empty)") == 0 &&
           meta_set(db, "migrated_at", now) == 0;
    }
    if (!ok) sql_error(db, now, "migrating " EMBED_LEGACY_VEC_TABLE);
    if (vecindex_end(db, ok) != 0) {
      if (ok) sql_error(db, now, "COMMIT (migration)");
      return -1;
    }
    if (done) {
      fprintf(stderr, "[embed] migration complete: %ld copied, %ld orphans dropped, "
              "%ld missing vectors left to re-embed; %s dropped\n",
              meta_long(db, "migrate_copied"), meta_long(db, "migrate_orphans"),
              meta_long(db, "migrate_missing"), EMBED_LEGACY_VEC_TABLE);
      return 0;
    }
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
  vecindex_ensure_side_tables(db, SPEC);
  long long tick_ms = wall_ms();
  char now[40], mark[40];
  iso_ms(now, sizeof now, tick_ms);
  iso_ms(mark, sizeof mark, tick_ms - EMBED_OVERLAP_SEC * 1000LL);
  meta_set(db, "last_run_at", now);

  /* ---- an index from before metadata columns? ------------------------- */
  /* Needs no server: it copies vectors that already exist. While it is not
   * finished nothing new is embedded (the two tables must agree on what the
   * done table claims), and the query side keeps reading the old table. */
  {
    int m = migrate_legacy(db, ctx, now);
    if (m != 0) {
      char lt[200];
      snprintf(lt, sizeof lt, "{\"at\":\"%s\",\"walk\":\"migration\",\"stopped\":\"%s\"}",
               now, m > 0 ? "cancelled" : "error");
      meta_set(db, "last_tick", lt);
      db_worker_close(&own);
      /* cancelled mid-migration is not a failure; an SQL error is */
      if (m < 0) return -1;
      return 0;
    }
  }

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
      vecindex_drop(db, SPEC);
      stored_model[0] = 0;
      dim = 0;
    } else {
      char msg[600];
      vecindex_check_identity(db, SPEC, model, 0, msg, sizeof msg);
      vecindex_refuse(db, SPEC, msg);
      db_worker_close(&own);
      return -1;
    }
  }
  if (dim && vecindex_create(db, SPEC, dim) != 0) {
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
    llm_status pst;
    int pdim = vecindex_probe_dim(&llm, 30000, &pst);
    if (pdim <= 0) {
      char msg[160];
      snprintf(msg, sizeof msg, "%s: /v1/embeddings unreachable for the dimension probe (%s)",
               now, llm_status_code(pst));
      meta_set(db, "last_error", msg);
      fprintf(stderr, "[embed] %s\n", msg);
      db_worker_close(&own);
      return -1;
    }
    char msg[600];
    if (vecindex_check_identity(db, SPEC, model, pdim, msg, sizeof msg) == VI_ID_DIM_MISMATCH) {
      vecindex_refuse(db, SPEC, msg);
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
  vecindex_stats ts = {0};
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
        /* 64, matching sweep_start, because the ELSE branch copies it
         * verbatim. iso_ms() needs ~24, but a 40-byte buffer silently cut a
         * 63-character sweep_start — and a truncated ISO timestamp is not a
         * short timestamp, it is a DIFFERENT instant, written straight into
         * meta as the delta watermark. The next sweep would then resume from
         * whatever that cut string parsed as. */
        char dk[64];
        if (ss > 0) iso_ms(dk, sizeof dk, ss - EMBED_OVERLAP_SEC * 1000LL);
        else snprintf(dk, sizeof dk, "%s", sweep_start);
        meta_set(db, "dwm_f", dk); meta_set(db, "dwm_u", "");
        fprintf(stderr, "[embed] full sweep complete (started %s); switching to delta\n", sweep_start);
      }
      break;
    }
    /* Split the page: rows already settled with this exact text are marked
     * (cheap); everything else is sent, in page order. */
    vecindex_item *seen[SEL_CHUNK], *need[SEL_CHUNK];
    int idx_need[SEL_CHUNK], ns = 0, nn = 0;
    for (int i = 0; i < n; i++) {
      vecindex_item *p = &rows[i].it;
      if (vecindex_item_unchanged(p)) {
        seen[ns++] = p;
        if (p->in_done) ts.unchanged++; else ts.failed_seen++;
      } else { idx_need[nn] = i; need[nn++] = p; }
    }
    int settled = n;              /* leading rows of the page that are done */
    if (vecindex_mark_seen(db, SPEC, seen, ns, mark, now, &ts) != 0) {
      rc = -1; settled = 0; stopped = "error";
    }
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
      int e = vecindex_embed_items(db, SPEC, &llm, &need[off], cnt, &dim, model,
                                   mark, now, &ts, &done_n);
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

  char lt[440];
  snprintf(lt, sizeof lt,
           "{\"at\":\"%s\",\"walk\":\"%s\",\"walked\":%ld,\"sent\":%ld,"
           "\"embedded\":%ld,\"unchanged\":%ld,\"rewritten\":%ld,\"failed\":%ld,"
           "\"failed_skipped\":%ld,\"stopped\":\"%s\",\"send_budget\":%d,"
           "\"walk_budget\":%ld}",
           now, walk == WALK_DELTA ? "delta" : walk == WALK_FULL_A ? "full_a" : "full_b",
           ts.walked, ts.sent, ts.embedded, ts.unchanged, ts.rewritten, ts.failed,
           ts.failed_seen, stopped, budget, walk_cap);
  meta_set(db, "last_tick", lt);
  refresh_counts(db, since, types, now, 0);
  /* A tick that ended cleanly — including one with nothing to embed — has
   * proven the server answers, so a stale last_error from an earlier outage
   * must not keep showing in coverage. */
  if (rc == 0) meta_set(db, "last_error", NULL);
  fprintf(stderr, "[embed] %s tick: walked %ld, sent %ld, embedded %ld, unchanged %ld, "
          "rewritten %ld, failed %ld (+%ld skipped), stopped=%s; index now %ld row(s) "
          "at %d-d (%s)\n",
          walk == WALK_DELTA ? "delta" : "full", ts.walked, ts.sent, ts.embedded,
          ts.unchanged, ts.rewritten, ts.failed, ts.failed_seen, stopped,
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
