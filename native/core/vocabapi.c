/* core/vocabapi.c — GET /api/vocab. See vocabapi.h for the provenance of
 * every list below and the reason this endpoint exists at all.
 *
 * Two implementation notes:
 *
 * 1. ref_types is fetched through annotations_ref_types() rather than being
 *    retyped here. That accessor exists precisely because casesapi.c once kept
 *    its own copy and the two drifted; adding a third copy in the endpoint
 *    whose job is to STOP drift would be self-defeating.
 * 2. The counted vocabularies are memoised per (tenant) in a tiny fixed table
 *    with a TTL. The alternative — collector_cache — would put a write on a
 *    read path that any client may poll on foreground, and the payload is a
 *    few kilobytes, so process memory is the right place.
 */
#include "vocabapi.h"
#include "annotationsapi.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VOCAB_TTL_SEC   300
#define VOCAB_SLOTS     8
#define VOCAB_MAX_VALUES 600      /* per counted vocabulary                  */

/* ── closed vocabularies ───────────────────────────────────────────────── */
/* Each row is {key, source, NULL-terminated value list}. `source` is the file
 * or constraint that actually enforces the list — if you change one, change
 * the other, and the string tells you where to look. */
typedef struct { const char *key, *source; const char *const *vals; } closed_vocab;

static const char *const V_TENANT_ROLES[]      = { "owner","admin","analyst","viewer",NULL };
static const char *const V_CASE_STATUS[]       = { "open","closed","archived",NULL };
static const char *const V_CASE_MEMBER_ROLES[] = { "lead","contributor","viewer",NULL };
static const char *const V_SAVED_SEARCH_KINDS[]= { "intel","osint","entity","breach","map",NULL };
static const char *const V_ALERT_CHANNELS[]    = { "webhook","email",NULL };
static const char *const V_AOI_KINDS[]         = { "bbox","polygon","circle",NULL };
static const char *const V_MONITOR_KINDS[]     = { "email","domain","username","phone",NULL };
static const char *const V_SOURCE_TYPES[]      = { "api","dataset","scraped","web_request",NULL };
static const char *const V_SOURCE_STATUSES[]   = { "online","offline","degraded","pending",NULL };
static const char *const V_TENANT_PLANS[]      = { "free","pro","team","enterprise",NULL };

static const closed_vocab CLOSED[] = {
  { "tenant_roles",        "core/schema.sql CHECK(role IN …) on memberships",       V_TENANT_ROLES },
  { "case_status",         "core/schema.sql CHECK(status IN …) on cases",           V_CASE_STATUS },
  { "case_member_roles",   "core/schema.sql CHECK(role IN …) on case_members",      V_CASE_MEMBER_ROLES },
  { "saved_search_kinds",  "core/savedsearchapi.c kind_valid()",                    V_SAVED_SEARCH_KINDS },
  { "alert_channel_kinds", "core/alertsapi.c channel type validation",              V_ALERT_CHANNELS },
  { "aoi_kinds",           "core/aoiapi.c kind_valid()",                            V_AOI_KINDS },
  { "breach_monitor_kinds","core/schema.sql CHECK(kind IN …) on breach_monitors",   V_MONITOR_KINDS },
  { "source_types",        "core/schema.sql CHECK(type IN …) on sources",           V_SOURCE_TYPES },
  { "source_statuses",     "core/schema.sql CHECK(status IN …) on sources",         V_SOURCE_STATUSES },
  { "tenant_plans",        "core/schema.sql CHECK(plan IN …) on tenants",           V_TENANT_PLANS },
};

/* ── memoised counted vocabularies ─────────────────────────────────────── */
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static struct { char tenant[128]; char *json; time_t at; } g_slot[VOCAB_SLOTS];

/* Counts one open vocabulary. `tenant_col` is NULL when the table has no
 * tenant column — the caller then documents the wider scope in the payload
 * rather than pretending the numbers are tenant-private.
 *
 * `legacy_form` picks the tenant predicate, and the two are NOT
 * interchangeable: intel_items.tenant_id is NOT NULL DEFAULT 'legacy' so the
 * shared pre-tenancy corpus is the literal 'legacy' (exportapi.c:461), while
 * entities.tenant_id is nullable so its shared rows are NULL
 * (entityapi.c:67). Using one form on the other table returns zero rows and
 * looks exactly like an empty corpus. */
static cJSON *count_vocab(db_handle *db, const char *table, const char *col,
                          const char *tenant_col, const char *tenant,
                          int legacy_form) {
  cJSON *arr = cJSON_CreateArray();
  char sql[360];
  if (tenant_col && legacy_form)
    snprintf(sql, sizeof sql,
      "SELECT %s, COUNT(*) FROM %s WHERE %s IS NOT NULL AND %s<>'' "
      "AND %s IN (?1,'legacy') GROUP BY %s ORDER BY COUNT(*) DESC LIMIT %d",
      col, table, col, col, tenant_col, col, VOCAB_MAX_VALUES);
  else if (tenant_col)
    snprintf(sql, sizeof sql,
      "SELECT %s, COUNT(*) FROM %s WHERE %s IS NOT NULL AND %s<>'' "
      "AND (%s IS NULL OR %s=?1) GROUP BY %s ORDER BY COUNT(*) DESC LIMIT %d",
      col, table, col, col, tenant_col, tenant_col, col, VOCAB_MAX_VALUES);
  else
    snprintf(sql, sizeof sql,
      "SELECT %s, COUNT(*) FROM %s WHERE %s IS NOT NULL AND %s<>'' "
      "GROUP BY %s ORDER BY COUNT(*) DESC LIMIT %d",
      col, table, col, col, col, VOCAB_MAX_VALUES);
  sqlite3_stmt *st = NULL;
  /* A missing table is not an error here: a fresh database legitimately has
   * no entities yet, and an empty list is the truthful answer. */
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) return arr;
  if (tenant_col) sqlite3_bind_text(st, 1, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
  while (sqlite3_step(st) == SQLITE_ROW) {
    const char *v = (const char *) sqlite3_column_text(st, 0);
    if (!v) continue;
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "value", v);
    cJSON_AddNumberToObject(o, "count", (double) sqlite3_column_int64(st, 1));
    cJSON_AddItemToArray(arr, o);
  }
  sqlite3_finalize(st);
  return arr;
}

static char *build(db_handle *db, const char *tenant) {
  cJSON *root = cJSON_CreateObject();

  cJSON *closed = cJSON_CreateObject();
  for (size_t i = 0; i < sizeof CLOSED / sizeof *CLOSED; i++) {
    cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "source", CLOSED[i].source);
    cJSON *vals = cJSON_CreateArray();
    for (const char *const *p = CLOSED[i].vals; *p; p++)
      cJSON_AddItemToArray(vals, cJSON_CreateString(*p));
    cJSON_AddItemToObject(b, "values", vals);
    cJSON_AddItemToObject(closed, CLOSED[i].key, b);
  }
  /* ref_types comes from the one accessor that owns it. */
  { int n = 0;
    const char *const *rt = annotations_ref_types(&n);
    cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "source",
      "core/annotationsapi.c REF_TYPES via annotations_ref_types()");
    cJSON *vals = cJSON_CreateArray();
    for (int i = 0; i < n; i++) cJSON_AddItemToArray(vals, cJSON_CreateString(rt[i]));
    cJSON_AddItemToObject(b, "values", vals);
    cJSON_AddItemToObject(closed, "ref_types", b); }
  cJSON_AddItemToObject(root, "closed", closed);

  cJSON *open = cJSON_CreateObject();
  { cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "source",
      "SELECT record_type, COUNT(*) FROM intel_items GROUP BY record_type "
      "(tenant-scoped). No central list exists in C: the collector corpus "
      "holds 525 distinct record_type literals across 737 emission sites, 462 "
      "of them used exactly once. This is the vocabulary actually in the "
      "corpus, not a hand-maintained enum.");
    cJSON_AddStringToObject(b, "scope", "tenant");
    cJSON_AddItemToObject(b, "values",
      count_vocab(db, "intel_items", "record_type", "tenant_id", tenant, 1));
    cJSON_AddItemToObject(open, "record_types", b); }
  { cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "source",
      "SELECT type, COUNT(*) FROM entities GROUP BY type (tenant-scoped)");
    cJSON_AddStringToObject(b, "scope", "tenant");
    cJSON_AddItemToObject(b, "values",
      count_vocab(db, "entities", "type", "tenant_id", tenant, 0));
    cJSON_AddItemToObject(open, "entity_types", b); }
  { cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "source",
      "SELECT rel_type, COUNT(*) FROM entity_relationships GROUP BY rel_type");
    /* Said out loud rather than left to be inferred from a suspiciously large
     * number: this table has no tenant column to filter on. */
    cJSON_AddStringToObject(b, "scope", "corpus");
    cJSON_AddStringToObject(b, "scope_note",
      "entity_relationships has no tenant_id column; counts are corpus-wide");
    cJSON_AddItemToObject(b, "values",
      count_vocab(db, "entity_relationships", "rel_type", NULL, NULL, 0));
    cJSON_AddItemToObject(open, "rel_types", b); }
  cJSON_AddItemToObject(root, "open", open);

  cJSON *meta = cJSON_CreateObject();
  char ts[40];
  { time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
    snprintf(ts, sizeof ts, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec); }
  cJSON_AddStringToObject(meta, "generated_at", ts);
  cJSON_AddNumberToObject(meta, "ttl_sec", VOCAB_TTL_SEC);
  cJSON_AddNumberToObject(meta, "max_values_per_open_vocab", VOCAB_MAX_VALUES);
  cJSON_AddItemToObject(root, "meta", meta);

  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return out;
}

char *vocabapi_get(db_handle *db, const char *tenant, int refresh) {
  const char *key = tenant ? tenant : "-";
  time_t now = time(NULL);

  pthread_mutex_lock(&g_mu);
  for (int i = 0; i < VOCAB_SLOTS; i++) {
    if (!g_slot[i].json || strcmp(g_slot[i].tenant, key) != 0) continue;
    if (!refresh && now - g_slot[i].at < VOCAB_TTL_SEC) {
      char *copy = strdup(g_slot[i].json);
      pthread_mutex_unlock(&g_mu);
      return copy;
    }
    break;
  }
  pthread_mutex_unlock(&g_mu);

  char *fresh = build(db, tenant);
  if (!fresh) return NULL;

  /* Store under the coldest slot. Eight tenants of hot cache is plenty for a
   * value that costs one GROUP BY to rebuild. */
  pthread_mutex_lock(&g_mu);
  int victim = 0;
  for (int i = 0; i < VOCAB_SLOTS; i++) {
    if (g_slot[i].json && strcmp(g_slot[i].tenant, key) == 0) { victim = i; break; }
    if (!g_slot[i].json) { victim = i; break; }
    if (g_slot[i].at < g_slot[victim].at) victim = i;
  }
  free(g_slot[victim].json);
  g_slot[victim].json = strdup(fresh);
  snprintf(g_slot[victim].tenant, sizeof g_slot[victim].tenant, "%s", key);
  g_slot[victim].at = now;
  pthread_mutex_unlock(&g_mu);
  return fresh;
}
