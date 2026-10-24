/* core/alert_sem.c — see alert_sem.h. */
#include "alert_sem.h"
#include "thresholds.h"         /* operator-tunable similarity cut-offs */
#include "embed_pod.h"
#include "qembed.h"
#include "../lib/vecindex.h"
#include "../third_party/sqlite3.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A failed embed is retried by the sweep no sooner than this. */
#define ALERT_SEM_RETRY_SEC 300

void alert_sem_migrate(db_handle *db) {
  sqlite3_exec(db->h,
    "CREATE TABLE IF NOT EXISTS alert_rule_vec ("
    " rule_id     TEXT PRIMARY KEY,"
    " tenant_id   TEXT NOT NULL,"
    " text_hash   TEXT NOT NULL,"
    " model       TEXT,"
    " dim         INTEGER,"
    " vec         BLOB,"
    " embedded_at TEXT,"
    " attempted_at TEXT NOT NULL DEFAULT (datetime('now')),"
    " error       TEXT)", NULL, NULL, NULL);
}

int alert_sem_is_semantic(const cJSON *pred) {
  if (!pred || !cJSON_IsObject(pred)) return 0;
  const cJSON *m = cJSON_GetObjectItem(pred, "mode");
  const cJSON *nl = cJSON_GetObjectItem(pred, "nl_query");
  return m && cJSON_IsString(m) && !strcmp(m->valuestring, "llm") &&
         nl && cJSON_IsString(nl) && nl->valuestring[0];
}

double alert_sem_threshold(const cJSON *pred) {
  const cJSON *t = pred ? cJSON_GetObjectItem(pred, "threshold") : NULL;
  if (t && cJSON_IsNumber(t) && t->valuedouble > 0 && t->valuedouble <= 1)
    return t->valuedouble;
  /* No per-rule threshold: the operator-tunable default (core/thresholds.c:
   * runtime override > JO_ALERT_SEM_THRESHOLD > ALERT_SEM_THRESHOLD_DEFAULT). */
  return threshold_get("alert_sem_threshold");
}

static void text_hash(const char *s, char out[20]) {
  snprintf(out, 20, "%016llx", vecindex_hash(s ? s : ""));
}

/* The model the intel index was built with: a rule vector from any other
 * model cannot be compared with an item vector. "" when there is no index. */
static void intel_model(db_handle *db, char *out, size_t cap) {
  out[0] = 0;
  vecindex_meta_get(db, &embed_intel_spec, "model", out, cap);
}

typedef struct {
  char *pred_json, *tenant;
  int enabled;
} rule_row;

static int read_rule(db_handle *db, const char *rule_id, rule_row *r) {
  memset(r, 0, sizeof *r);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "SELECT predicate_json,tenant_id,enabled FROM"
                         " alert_rules WHERE id=?1", -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, rule_id, -1, SQLITE_TRANSIENT);
  int ok = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *p = (const char *)sqlite3_column_text(s, 0);
    const char *t = (const char *)sqlite3_column_text(s, 1);
    r->pred_json = strdup(p ? p : "{}");
    r->tenant = strdup(t ? t : "");
    r->enabled = sqlite3_column_int(s, 2);
    ok = r->pred_json && r->tenant;
  }
  sqlite3_finalize(s);
  return ok;
}

static void store_error(db_handle *db, const char *rule_id, const char *tenant,
                        const char *hash, const char *why) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
      "INSERT INTO alert_rule_vec(rule_id,tenant_id,text_hash,error,attempted_at)"
      " VALUES(?1,?2,?3,?4,datetime('now')) ON CONFLICT(rule_id) DO UPDATE SET"
      " tenant_id=excluded.tenant_id, error=excluded.error,"
      " attempted_at=excluded.attempted_at,"
      /* a vector from older text must not survive under the new hash */
      " vec=CASE WHEN text_hash=excluded.text_hash THEN vec END,"
      " text_hash=excluded.text_hash", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, rule_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, tenant, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, why, -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
}

int alert_sem_embed_rule(db_handle *db, const char *rule_id) {
  if (!db || !rule_id) return -1;
  rule_row r;
  if (!read_rule(db, rule_id, &r)) {
    /* the rule is gone: its vector goes with it */
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h, "DELETE FROM alert_rule_vec WHERE rule_id=?1",
                           -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, rule_id, -1, SQLITE_TRANSIENT);
      sqlite3_step(s);
      sqlite3_finalize(s);
    }
    return 0;
  }
  cJSON *pred = cJSON_Parse(r.pred_json);
  int rc = 0;
  if (!alert_sem_is_semantic(pred)) goto out;
  const char *nl = cJSON_GetObjectItem(pred, "nl_query")->valuestring;
  char hash[20];
  text_hash(nl, hash);
  char want_model[256];
  intel_model(db, want_model, sizeof want_model);
  /* current already? same text, a vector, and (when the intel index exists)
   * the intel index's model */
  {
    sqlite3_stmt *s;
    int current = 0;
    if (sqlite3_prepare_v2(db->h, "SELECT text_hash,model,vec IS NOT NULL FROM"
                           " alert_rule_vec WHERE rule_id=?1", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, rule_id, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(s) == SQLITE_ROW) {
        const char *h = (const char *)sqlite3_column_text(s, 0);
        const char *m = (const char *)sqlite3_column_text(s, 1);
        current = h && !strcmp(h, hash) && sqlite3_column_int(s, 2) &&
                  (!want_model[0] || (m && !strcmp(m, want_model)));
      }
      sqlite3_finalize(s);
    }
    if (current) goto out;
  }
  if (!qembed_enabled()) {
    /* Nothing recorded: no attempt was made, so configuring a server later
     * must not wait out a retry delay. alert_sem_status names the reason. */
    rc = -1;
    goto out;
  }
  char live[256];
  if (vecindex_live_query_model(live, sizeof live) != 0) {
    store_error(db, rule_id, r.tenant, hash, live);
    rc = -1;
    goto out;
  }
  if (want_model[0] && strcmp(live, want_model) != 0) {
    char why[600];
    snprintf(why, sizeof why, "the query server serves '%.200s' but the intel "
             "index was built with '%.200s'", live, want_model);
    store_error(db, rule_id, r.tenant, hash, why);
    rc = -1;
    goto out;
  }
  const char *texts[1] = { nl };
  float *v = NULL; int dim = 0; llm_status st;
  if (qembed_texts(texts, 1, &v, &dim, &st) != 0) {
    char why[128];
    snprintf(why, sizeof why, "embedding failed (%s)", llm_status_code(st));
    store_error(db, rule_id, r.tenant, hash, why);
    rc = -1;
    goto out;
  }
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
      "INSERT INTO alert_rule_vec(rule_id,tenant_id,text_hash,model,dim,vec,"
      "embedded_at,attempted_at,error) VALUES(?1,?2,?3,?4,?5,?6,"
      "strftime('%Y-%m-%dT%H:%M:%SZ','now'),datetime('now'),NULL)"
      " ON CONFLICT(rule_id) DO UPDATE SET tenant_id=excluded.tenant_id,"
      " text_hash=excluded.text_hash, model=excluded.model, dim=excluded.dim,"
      " vec=excluded.vec, embedded_at=excluded.embedded_at,"
      " attempted_at=excluded.attempted_at, error=NULL", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, rule_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, r.tenant, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, live, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 5, dim);
    sqlite3_bind_blob(s, 6, v, (int)((size_t)dim * sizeof *v), SQLITE_TRANSIENT);
    rc = sqlite3_step(s) == SQLITE_DONE ? 1 : -1;
    sqlite3_finalize(s);
  } else rc = -1;
  free(v);
out:
  if (pred) cJSON_Delete(pred);
  free(r.pred_json); free(r.tenant);
  return rc;
}

int alert_sem_embed_missing(db_handle *db) {
  if (!db || !qembed_enabled()) return 0;
  char **ids = NULL;
  int n = 0, cap = 0;
  sqlite3_stmt *s;
  /* the LIKE is a cheap pre-filter; alert_sem_embed_rule() decides */
  if (sqlite3_prepare_v2(db->h,
      "SELECT r.id FROM alert_rules r LEFT JOIN alert_rule_vec v ON v.rule_id=r.id"
      " WHERE r.enabled=1 AND r.predicate_json LIKE '%\"llm\"%'"
      "   AND (v.rule_id IS NULL OR v.vec IS NOT NULL"
      "        OR v.attempted_at <= datetime('now',?1))", -1, &s, NULL) != SQLITE_OK)
    return 0;
  char ago[32];
  snprintf(ago, sizeof ago, "-%d seconds", ALERT_SEM_RETRY_SEC);
  sqlite3_bind_text(s, 1, ago, -1, SQLITE_TRANSIENT);
  while (sqlite3_step(s) == SQLITE_ROW) {
    if (n == cap) {
      int nc = cap ? cap * 2 : 16;
      char **nv = realloc(ids, (size_t)nc * sizeof *nv);
      if (!nv) break;
      ids = nv; cap = nc;
    }
    ids[n] = strdup((const char *)sqlite3_column_text(s, 0));
    if (ids[n]) n++;
  }
  sqlite3_finalize(s);
  int done = 0;
  for (int i = 0; i < n; i++) {
    if (alert_sem_embed_rule(db, ids[i]) == 1) done++;
    free(ids[i]);
  }
  free(ids);
  /* vectors of rules that no longer exist */
  sqlite3_exec(db->h, "DELETE FROM alert_rule_vec WHERE rule_id NOT IN"
                      " (SELECT id FROM alert_rules)", NULL, NULL, NULL);
  if (done) fprintf(stderr, "[alert-sem] embedded %d semantic rule(s)\n", done);
  return done;
}

static void *embed_thread(void *arg) {
  char *rule_id = arg;
  db_handle own = {0};
  if (db_attach(&own, NULL) == 0) {
    alert_sem_embed_rule(&own, rule_id);
    db_close(&own);
  }
  free(rule_id);
  return NULL;
}

void alert_sem_embed_async(const char *rule_id) {
  if (!rule_id || !qembed_enabled()) return;
  char *id = strdup(rule_id);
  if (!id) return;
  pthread_t th;
  if (pthread_create(&th, NULL, embed_thread, id) == 0) pthread_detach(th);
  else free(id);         /* the sweep thread retries it within a pass */
}

int alert_sem_load(db_handle *db, const char *rule_id, const char *nl_query,
                   float **vec, int *dim, char *model, size_t cap) {
  *vec = NULL; *dim = 0;
  if (model && cap) model[0] = 0;
  char hash[20];
  text_hash(nl_query, hash);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "SELECT dim,vec,model FROM alert_rule_vec"
      " WHERE rule_id=?1 AND text_hash=?2 AND vec IS NOT NULL", -1, &s, NULL) != SQLITE_OK)
    return 0;
  sqlite3_bind_text(s, 1, rule_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, hash, -1, SQLITE_TRANSIENT);
  int ok = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    int d = sqlite3_column_int(s, 0);
    int bytes = sqlite3_column_bytes(s, 1);
    const void *b = sqlite3_column_blob(s, 1);
    if (d > 0 && b && bytes == (int)((size_t)d * sizeof(float))) {
      *vec = malloc((size_t)bytes);
      if (*vec) {
        memcpy(*vec, b, (size_t)bytes);
        *dim = d;
        const char *m = (const char *)sqlite3_column_text(s, 2);
        if (model && cap) snprintf(model, cap, "%s", m ? m : "");
        ok = 1;
      }
    }
  }
  sqlite3_finalize(s);
  return ok;
}

cJSON *alert_sem_status(db_handle *db, const char *rule_id,
                        const char *predicate_json) {
  cJSON *pred = predicate_json ? cJSON_Parse(predicate_json) : NULL;
  if (!alert_sem_is_semantic(pred)) { if (pred) cJSON_Delete(pred); return NULL; }
  cJSON *o = cJSON_CreateObject();
  cJSON_AddNumberToObject(o, "threshold", alert_sem_threshold(pred));
  cJSON *nm = cJSON_GetObjectItem(pred, "novelty_min");
  if (nm && cJSON_IsNumber(nm)) cJSON_AddNumberToObject(o, "novelty_min", nm->valuedouble);
  else cJSON_AddNullToObject(o, "novelty_min");
  float *v = NULL; int dim = 0; char model[256];
  int have = alert_sem_load(db, rule_id,
                            cJSON_GetObjectItem(pred, "nl_query")->valuestring,
                            &v, &dim, model, sizeof model);
  free(v);
  cJSON_AddBoolToObject(o, "embedded", have);
  char im[256];
  intel_model(db, im, sizeof im);
  const char *reason = NULL;
  char why[700] = "";
  if (!have) {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h, "SELECT error FROM alert_rule_vec WHERE rule_id=?1",
                           -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, rule_id, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL)
        snprintf(why, sizeof why, "%s", (const char *)sqlite3_column_text(s, 0));
      sqlite3_finalize(s);
    }
    reason = why[0] ? why : (qembed_enabled() ? "the description has not been "
             "embedded yet (it is retried every sweep)"
             : "no embedding server configured (JO_EMBED_URL is unset)");
  } else if (!im[0]) {
    reason = "the intel embedding index has not been built yet";
  } else if (strcmp(im, model) != 0) {
    snprintf(why, sizeof why, "rule vector is from '%.200s', the intel index from "
             "'%.200s'; it is re-embedded by the next sweep", model, im);
    reason = why;
  }
  if (have) {
    cJSON_AddStringToObject(o, "model", model);
    cJSON_AddNumberToObject(o, "dim", dim);
  }
  cJSON_AddBoolToObject(o, "evaluable", reason == NULL);
  if (reason) cJSON_AddStringToObject(o, "reason", reason);
  else cJSON_AddNullToObject(o, "reason");
  cJSON_AddStringToObject(o, "evaluates",
      "items are matched after the embedding pod embeds them (minutes after "
      "ingest, not at ingest); items outside the embedding window "
      "(JO_EMBED_SINCE_DAYS, JO_EMBED_RECORD_TYPES) are never evaluated; "
      "every other predicate term must also hold");
  cJSON_Delete(pred);
  return o;
}
