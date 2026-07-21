/* collectors/_maint/sources/collector_repair.c — maintenance pod Stage 3.
 *
 * Faithful C port of server/src/utils/collectorRepair.js attemptRepair. A
 * registered pseudo-source on the scheduler path (update_interval_sec=120),
 * gated on LLM_ENABLED. Each tick processes up to REPAIR_BATCH triaged-open
 * anomalies through the same conservative decision tree as Node:
 *
 *   exhausted (escalation/attempts capped) -> needs_human, resolve
 *   transient   -> auto_dismiss if recovered, else await_recovery + re-triage
 *   needs-human classes -> needs_human, resolve
 *   url_move / site_dead -> URL-swap flow behind the deterministic gate
 *
 * The Node gate text-patched sourceRegistry.js + lint + git PR. The C engine
 * has no per-collector registry file, so a verified swap is applied at runtime:
 * UPSERT into collector_url_overrides + url_override_reload(), after which
 * httpclient.c rewrites the old URL to the new one for every collector with no
 * recompile. Honesty rule (kept from Node): only 'merged' when old_url is a
 * real http(s) URL the fetch layer can match; an internal:// or dynamically
 * built URL can't be matched by the override, so we record 'verified' (a human
 * suggestion) and leave the anomaly open — never a faked live fix. */
#include "../../source.h"
#include "../../core/prompts.h"
#include "../../core/source_registry.h"
#include "../../core/url_override.h"
#include "../../third_party/cJSON.h"
#include "../../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define BODY_CAP 2000
#define SAMPLE_CAP 1500

static int env_int(const char *k, int dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  int n = atoi(v);
  return n > 0 ? n : dflt;
}
static double env_double(const char *k, double dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  double d = atof(v);
  return d >= 0.0 ? d : dflt;
}
static int env_truthy(const char *k) {
  const char *v = getenv(k);
  return v && *v && strcmp(v, "0") && strcasecmp(v, "false");
}

/* first balanced {...} -> cJSON (== llmClient completeJSON tolerance). */
static cJSON *extract_json(const char *raw) {
  if (!raw) return NULL;
  const char *s = strchr(raw, '{');
  if (!s) return NULL;
  int d = 0;
  for (const char *p = s; *p; p++) {
    if (*p == '{') d++;
    else if (*p == '}' && --d == 0) {
      char *sub = strndup(s, (size_t)(p - s + 1));
      cJSON *j = sub ? cJSON_Parse(sub) : NULL;
      free(sub);
      return j;
    }
  }
  return NULL;
}

static char *col_dup(sqlite3_stmt *s, int i) {
  const char *c = (const char *)sqlite3_column_text(s, i);
  return c ? strdup(c) : NULL;
}
static void add_str(cJSON *o, const char *k, const char *v) {
  if (v) cJSON_AddStringToObject(o, k, v); else cJSON_AddNullToObject(o, k);
}
static void add_trunc(cJSON *o, const char *k, const char *v, size_t cap) {
  if (!v) { cJSON_AddNullToObject(o, k); return; }
  size_t n = strlen(v);
  if (n <= cap) { cJSON_AddStringToObject(o, k, v); return; }
  char *t = malloc(cap + 16);
  if (!t) { cJSON_AddStringToObject(o, k, v); return; }
  memcpy(t, v, cap); strcpy(t + cap, "...[trunc]");
  cJSON_AddStringToObject(o, k, t); free(t);
}

/* absolute http(s), <=2048, no control chars, and (when old given) != old. */
static int is_clean_url(const char *u, const char *old) {
  if (!u) return 0;
  size_t n = strlen(u);
  if (n == 0 || n > 2048) return 0;
  for (size_t i = 0; i < n; i++) { unsigned char c = (unsigned char)u[i];
    if (c < 0x20 || c == 0x7f) return 0; }
  if (strncmp(u, "http://", 7) && strncmp(u, "https://", 8)) return 0;
  if (old && strcmp(u, old) == 0) return 0;
  return 1;
}

static char *source_url(sqlite3 *h, const char *id) {
  const src_meta *m = src_meta_get(id);
  if (m && m->url && *m->url) return strdup(m->url);
  sqlite3_stmt *s; char *u = NULL;
  if (sqlite3_prepare_v2(h, "SELECT url FROM sources WHERE id=?1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) u = col_dup(s, 0);
    sqlite3_finalize(s);
  }
  return u;
}

/* Mirror collectorRepair.js countRecords: array length, or the length of
 * data/results/features/items, or 1 for any other object, 0 for non-JSON. */
static int count_records(const char *body) {
  if (!body) return 0;
  cJSON *j = cJSON_Parse(body);
  if (!j) return 0;
  int n = 0;
  if (cJSON_IsArray(j)) n = cJSON_GetArraySize(j);
  else if (cJSON_IsObject(j)) {
    const char *keys[] = { "data", "results", "features", "items" };
    cJSON *arr = NULL;
    for (int i = 0; i < 4 && !arr; i++) {
      cJSON *c = cJSON_GetObjectItem(j, keys[i]);
      if (c && cJSON_IsArray(c)) arr = c;
    }
    n = arr ? cJSON_GetArraySize(arr) : 1;
  }
  cJSON_Delete(j);
  return n;
}

/* recent healthy baseline mean (== detection's). returns 1 + sets *out. */
static int records_baseline(sqlite3 *h, const char *id, double *out) {
  sqlite3_stmt *s; int have = 0;
  if (sqlite3_prepare_v2(h,
        "SELECT AVG(records_fetched) FROM (SELECT records_fetched FROM fetch_log"
        " WHERE source_id=?1 AND status='ok' AND records_fetched>0"
        " ORDER BY id DESC LIMIT 20)", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s,0) != SQLITE_NULL) {
      *out = sqlite3_column_double(s, 0); have = (*out > 0.0);
    }
    sqlite3_finalize(s);
  }
  return have;
}

/* ── small DB ops (database.js helpers) ─────────────────────────────────── */
static int repair_attempt_count(sqlite3 *h, long anomaly_id) {
  sqlite3_stmt *s; int n = 0;
  if (sqlite3_prepare_v2(h, "SELECT COUNT(*) FROM collector_repair WHERE anomaly_id=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int64(s, 1, (sqlite3_int64)anomaly_id);
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
  }
  return n;
}
static int has_staged_repair(sqlite3 *h, long anomaly_id) {
  sqlite3_stmt *s; int yes = 0;
  if (sqlite3_prepare_v2(h, "SELECT 1 FROM collector_repair WHERE anomaly_id=?1"
        " AND status='verified' LIMIT 1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int64(s, 1, (sqlite3_int64)anomaly_id);
    yes = (sqlite3_step(s) == SQLITE_ROW);
    sqlite3_finalize(s);
  }
  return yes;
}
static int is_quarantined(sqlite3 *h, const char *id) {
  sqlite3_stmt *s; int q = 0;
  if (sqlite3_prepare_v2(h, "SELECT 1 FROM sources WHERE id=?1"
        " AND quarantined_until IS NOT NULL AND quarantined_until>datetime('now')"
        " LIMIT 1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    q = (sqlite3_step(s) == SQLITE_ROW);
    sqlite3_finalize(s);
  }
  return q;
}
/* latest ok run with records>0 strictly after `created_at`. */
static int healthy_after(sqlite3 *h, const char *id, const char *created_at,
                         char *ts_out, int ts_sz, int *records_out) {
  sqlite3_stmt *s; int found = 0;
  if (sqlite3_prepare_v2(h,
        "SELECT timestamp,records_fetched FROM fetch_log WHERE source_id=?1"
        " AND status='ok' AND records_fetched>0 AND timestamp>?2"
        " ORDER BY id DESC LIMIT 1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, created_at ? created_at : "", -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *ts = (const char *)sqlite3_column_text(s, 0);
      if (ts && ts_out) snprintf(ts_out, ts_sz, "%s", ts);
      if (records_out) *records_out = sqlite3_column_int(s, 1);
      found = 1;
    }
    sqlite3_finalize(s);
  }
  return found;
}
static void record_repair(sqlite3 *h, long anomaly_id, const char *source_id,
                          const char *status, const char *action,
                          const char *patch, const char *gate,
                          const char *model, const char *triage_class) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
        "INSERT INTO collector_repair (anomaly_id,source_id,status,action,patch,"
        "gate,model,triage_class) VALUES (?1,?2,?3,?4,?5,?6,?7,?8)",
        -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_int64(s, 1, (sqlite3_int64)anomaly_id);
  sqlite3_bind_text(s, 2, source_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, status, -1, SQLITE_TRANSIENT);
  if (action) sqlite3_bind_text(s, 4, action, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(s, 4);
  if (patch) sqlite3_bind_text(s, 5, patch, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(s, 5);
  if (gate) sqlite3_bind_text(s, 6, gate, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(s, 6);
  if (model) sqlite3_bind_text(s, 7, model, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(s, 7);
  if (triage_class) sqlite3_bind_text(s, 8, triage_class, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(s, 8);
  sqlite3_step(s);
  sqlite3_finalize(s);
}
static void resolve_anomaly(sqlite3 *h, long anomaly_id, const char *resolution) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, "UPDATE collector_anomaly SET resolved_at=datetime('now'),"
        " resolution=?1 WHERE id=?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, resolution, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 2, (sqlite3_int64)anomaly_id);
    sqlite3_step(s); sqlite3_finalize(s);
  }
}
static void reopen_for_retriage(sqlite3 *h, long anomaly_id) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, "UPDATE collector_anomaly SET"
        " escalation_level=escalation_level+1, triaged_at=NULL, triage_class=NULL,"
        " triage_confidence=NULL, triage_evidence=NULL, triage_suggested_fix=NULL,"
        " triage_model=NULL WHERE id=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int64(s, 1, (sqlite3_int64)anomaly_id);
    sqlite3_step(s); sqlite3_finalize(s);
  }
}
static int count_recent_failed(sqlite3 *h, const char *id, int window_h) {
  sqlite3_stmt *s; int n = 0;
  char win[40]; snprintf(win, sizeof win, "-%d hours", window_h);
  if (sqlite3_prepare_v2(h, "SELECT COUNT(*) FROM collector_repair WHERE source_id=?1"
        " AND status IN ('rejected','error') AND created_at>=datetime('now',?2)",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, win, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
  }
  return n;
}
/* Trip the circuit breaker if too many failed cycles in the window. */
static void maybe_quarantine(db_handle *db, const char *id) {
  sqlite3 *h = db->h;
  if (is_quarantined(h, id)) return;
  int thr = env_int("REPAIR_BREAKER_THRESHOLD", 3);
  int win = env_int("REPAIR_BREAKER_WINDOW_HOURS", 24);
  int qh  = env_int("REPAIR_QUARANTINE_HOURS", 24);
  int fails = count_recent_failed(h, id, win);
  if (fails < thr) return;
  char reason[160], mod[40];
  snprintf(reason, sizeof reason, "breaker: %d failed repair cycles in %dh", fails, win);
  snprintf(mod, sizeof mod, "+%d hours", qh);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, "UPDATE sources SET quarantined_at=datetime('now'),"
        " quarantined_until=datetime('now',?1), quarantine_reason=?2 WHERE id=?3",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, mod, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, reason, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, id, -1, SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
  }
  fprintf(stderr, "[repair] quarantined %s for %dh — %s\n", id, qh, reason);
}

/* candidate URL embedded in the triage suggested_fix blob, if any. */
static char *candidate_url_from_triage(const char *suggested_fix) {
  if (!suggested_fix) return NULL;
  const char *p = strstr(suggested_fix, "http://");
  const char *q = strstr(suggested_fix, "https://");
  const char *start = (!p) ? q : (!q ? p : (p < q ? p : q));
  if (!start) return NULL;
  const char *e = start;
  while (*e && *e != ' ' && *e != '"' && *e != '\\' && *e != '\n' && *e != '\t')
    e++;
  return strndup(start, (size_t)(e - start));
}

/* current re-fetch node (also returns the body for downstream gates). */
static cJSON *fetch_node(http_client *http, const char *url, int timeout_ms,
                         http_response *out /* may be NULL */) {
  cJSON *o = cJSON_CreateObject();
  if (!url || !*url) { cJSON_AddBoolToObject(o, "ok", 0);
    cJSON_AddStringToObject(o, "error", "no url"); return o; }
  http_response r = {0};
  int hard = http_request(http, "GET", url, NULL, NULL, 0, timeout_ms, 0, &r);
  int ok = !hard && r.status >= 200 && r.status < 300;
  cJSON_AddBoolToObject(o, "ok", ok);
  cJSON_AddNumberToObject(o, "status", (double)r.status);
  add_trunc(o, "body_head", r.body, BODY_CAP);
  if (out) *out = r; else http_response_free(&r);
  return o;
}

static const char *SYS_PROPOSAL =
  "You repair a Japan OSINT data-collector fleet. One collector has an anomaly "
  "that triage classified as a moved or dead endpoint. Your ONLY available "
  "action is to replace the single source URL. You cannot edit parsing code, "
  "headers, or credentials.\n\n"
  "Propose action \"url_swap\" with a concrete new_url ONLY when you have strong "
  "evidence of a specific replacement endpoint that serves the same data (a "
  "permanent redirect Location, a triage suggested_fix naming a versioned path, "
  "an obvious http->https or host rename, a documented API base change). The "
  "new_url must be a fully-qualified absolute http(s) URL.\n\n"
  "If you cannot identify a concrete, same-data replacement with confidence, "
  "return action \"no_fix\" — guessing corrupts the collector silently, which is "
  "worse than leaving it broken. Output JSON only.";

static const char *SYS_SANITY =
  "You inspect a small sample of the raw response from a Japan OSINT data "
  "collector and decide whether it looks like a legitimate, useful payload. A "
  "bad sample looks like: an error page, a Cloudflare challenge, an "
  "empty/placeholder page, content in an unrelated domain, or corrupted output. "
  "Be permissive: legitimate-but-unusual content is fine. Output JSON only.";

/* Build a sanity sample (json slice of first 3, or raw prefix), capped. */
static char *build_sample(const char *body) {
  if (!body || !*body) return strdup("");
  cJSON *j = cJSON_Parse(body);
  cJSON *arr = NULL;
  if (j) {
    if (cJSON_IsArray(j)) arr = j;
    else if (cJSON_IsObject(j)) {
      const char *keys[] = { "data", "results", "features", "items" };
      for (int i = 0; i < 4 && !arr; i++) {
        cJSON *c = cJSON_GetObjectItem(j, keys[i]);
        if (c && cJSON_IsArray(c)) arr = c;
      }
    }
  }
  char *sample = NULL;
  if (arr && cJSON_GetArraySize(arr) > 0) {
    cJSON *slice = cJSON_CreateArray();
    int lim = cJSON_GetArraySize(arr); if (lim > 3) lim = 3;
    for (int i = 0; i < lim; i++)
      cJSON_AddItemToArray(slice, cJSON_Duplicate(cJSON_GetArrayItem(arr, i), 1));
    sample = cJSON_PrintUnformatted(slice);
    cJSON_Delete(slice);
  }
  if (j) cJSON_Delete(j);
  if (!sample) sample = strdup(body);
  if (sample && strlen(sample) > SAMPLE_CAP) sample[SAMPLE_CAP] = '\0';
  return sample;
}

/* Gate (c): 4B-style sanity re-check. Returns 1 if looks_correct, sets reason. */
static int sanity_ok(llm_client *llm, const char *name, const char *category,
                     const char *type, const char *body, int records,
                     char *reason_out, int reason_sz) {
  if (!body || !*body) { snprintf(reason_out, reason_sz, "empty body"); return 0; }
  char *sample = build_sample(body);
  size_t plen = strlen(SYS_SANITY) + strlen(sample) + 400;
  char *prompt = malloc(plen);
  if (!prompt) { free(sample); snprintf(reason_out, reason_sz, "oom"); return 0; }
  snprintf(prompt, plen,
    "%s\n\nCollector: %s\nCategory: %s\nType: %s\nRecords reported: %d\n"
    "Sample:\n%s\n\nDoes this look like a healthy payload? "
    "JSON: {\"looks_correct\": boolean, \"reason\": short string}",
    SYS_SANITY, name ? name : "?", category ? category : "?",
    type ? type : "?", records, sample);
  free(sample);
  int timeout = env_int("LLM_REPAIR_SANITY_TIMEOUT_MS", 8000);
  const char *grammar = grammar_load("repair_sanity");
  char *raw = llm_complete(llm, prompt, grammar && *grammar ? grammar : NULL,
                           256, 0.1, timeout);
  free(prompt);
  cJSON *out = extract_json(raw);
  free(raw);
  int ok = 0;
  if (out) {
    cJSON *lc = cJSON_GetObjectItem(out, "looks_correct");
    cJSON *rs = cJSON_GetObjectItem(out, "reason");
    ok = lc && cJSON_IsTrue(lc);
    if (rs && cJSON_IsString(rs)) snprintf(reason_out, reason_sz, "%s", rs->valuestring);
    else snprintf(reason_out, reason_sz, "%s", ok ? "ok" : "rejected");
    cJSON_Delete(out);
  } else {
    snprintf(reason_out, reason_sz, "sanity classifier unavailable");
  }
  return ok;
}

typedef struct {
  long id; char *source_id, *triage_class, *triage_evidence, *triage_fix, *created_at;
  int escalation;
} cand_row;

/* reject path: record + re-triage + maybe quarantine. gate is consumed. */
static void do_reject(db_handle *db, const cand_row *a, const char *new_url,
                      cJSON *gate, const char *model) {
  char *patch = NULL;
  if (new_url) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "proposed_url", new_url);
    patch = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);
  }
  char *gj = gate ? cJSON_PrintUnformatted(gate) : NULL;
  if (gate) cJSON_Delete(gate);
  record_repair(db->h, a->id, a->source_id, "rejected", "url_swap", patch, gj,
                model, a->triage_class);
  free(patch); free(gj);
  reopen_for_retriage(db->h, a->id);
  maybe_quarantine(db, a->source_id);
}

static void attempt_repair(db_handle *db, llm_client *llm, http_client *http,
                           const cand_row *a) {
  sqlite3 *h = db->h;
  const char *cls = a->triage_class;
  if (!cls) return;

  /* Guards: quarantined source, or a verified fix already staged for this
   * anomaly (re-running would just re-derive the same patch). */
  if (is_quarantined(h, a->source_id)) return;
  if (has_staged_repair(h, a->id)) return;

  /* Give-up rails. */
  int max_esc = env_int("REPAIR_MAX_ESCALATION", 3);
  int max_att = env_int("REPAIR_MAX_ATTEMPTS", 4);
  int attempts = repair_attempt_count(h, a->id);
  if (a->escalation >= max_esc || attempts >= max_att) {
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "reason", "repair exhausted");
    cJSON_AddNumberToObject(g, "escalation_level", a->escalation);
    cJSON_AddNumberToObject(g, "attempts", attempts);
    char *gj = cJSON_PrintUnformatted(g); cJSON_Delete(g);
    record_repair(h, a->id, a->source_id, "needs_human", "exhausted", NULL, gj,
                  NULL, cls);
    free(gj);
    char res[160];
    snprintf(res, sizeof res, "needs_human: repair exhausted (esc %d, %d attempts)",
             a->escalation, attempts);
    resolve_anomaly(h, a->id, res);
    return;
  }

  /* transient: resolve only if the source actually recovered. */
  if (!strcmp(cls, "transient")) {
    char ts[40] = {0}; int rec = 0;
    if (healthy_after(h, a->source_id, a->created_at, ts, sizeof ts, &rec)) {
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "recovered_run", ts);
      cJSON_AddNumberToObject(g, "records", rec);
      char *gj = cJSON_PrintUnformatted(g); cJSON_Delete(g);
      record_repair(h, a->id, a->source_id, "verified", "auto_dismiss", NULL, gj,
                    NULL, cls);
      free(gj);
      char res[120]; snprintf(res, sizeof res, "auto: transient recovered at %s", ts);
      resolve_anomaly(h, a->id, res);
    } else {
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "reason", "no healthy run since anomaly");
      char *gj = cJSON_PrintUnformatted(g); cJSON_Delete(g);
      record_repair(h, a->id, a->source_id, "rejected", "await_recovery", NULL, gj,
                    NULL, cls);
      free(gj);
      reopen_for_retriage(h, a->id);
    }
    return;
  }

  /* needs-human classes: fix lives in parsing/credentials/quota, out of scope. */
  if (!strcmp(cls, "selector_drift") || !strcmp(cls, "structural") ||
      !strcmp(cls, "auth_break") || !strcmp(cls, "rate_limit") ||
      !strcmp(cls, "unknown")) {
    cJSON *g = cJSON_CreateObject();
    char rs[80]; snprintf(rs, sizeof rs, "class %s not machine-fixable", cls);
    cJSON_AddStringToObject(g, "reason", rs);
    add_str(g, "evidence", a->triage_evidence);
    char *gj = cJSON_PrintUnformatted(g); cJSON_Delete(g);
    record_repair(h, a->id, a->source_id, "needs_human", cls, NULL, gj, NULL, cls);
    free(gj);
    char res[120]; snprintf(res, sizeof res, "needs_human: %s", cls);
    resolve_anomaly(h, a->id, res);
    return;
  }

  /* Only url_move / site_dead are actionable; anything else → re-triage. */
  if (strcmp(cls, "url_move") && strcmp(cls, "site_dead")) {
    reopen_for_retriage(h, a->id);
    return;
  }

  /* ── URL-swap flow ───────────────────────────────────────────────────── */
  int escalate_at = env_int("REPAIR_ESCALATE_AT", 1);
  int escalated = a->escalation >= escalate_at;
  const char *fast = getenv("LLM_REPAIR_MODEL");
  const char *hard = getenv("LLM_REPAIR_MODEL_HARD");
  const char *model = escalated ? (hard && *hard ? hard : fast) : fast;
  if (!model) model = "default";
  int timeout = env_int("LLM_REPAIR_TIMEOUT_MS", 60000);
  int refetch_to = env_int("REPAIR_REFETCH_TIMEOUT_MS", 15000);

  char *old_url = source_url(h, a->source_id);
  int matchable = is_clean_url(old_url, NULL);  /* override can rewrite it */

  /* source metadata for bundle + sanity */
  char *name=NULL,*type=NULL,*cat=NULL,*status=NULL,*errmsg=NULL;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, "SELECT name,type,category,status,error_message"
        " FROM sources WHERE id=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, a->source_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      name=col_dup(s,0); type=col_dup(s,1); cat=col_dup(s,2);
      status=col_dup(s,3); errmsg=col_dup(s,4);
    }
    sqlite3_finalize(s);
  }
  const src_meta *m = src_meta_get(a->source_id);
  if (!name && m && m->name) name = strdup(m->name);
  if (!type && m && m->type) type = strdup(m->type);
  if (!cat && m && m->category) cat = strdup(m->category);

  /* bundle */
  cJSON *b = cJSON_CreateObject();
  cJSON *jt = cJSON_CreateObject();
  add_str(jt, "class", cls);
  add_str(jt, "evidence", a->triage_evidence);
  add_str(jt, "suggested_fix", a->triage_fix);
  cJSON_AddItemToObject(b, "triage", jt);
  cJSON *js = cJSON_CreateObject();
  add_str(js, "id", a->source_id);
  add_str(js, "name", name);
  add_str(js, "type", type);
  add_str(js, "category", cat);
  add_str(js, "url", old_url);
  add_str(js, "status", status);
  add_str(js, "error_message", errmsg);
  cJSON_AddItemToObject(b, "source", js);
  cJSON_AddNumberToObject(b, "escalation_level", a->escalation);
  cJSON_AddItemToObject(b, "current_fetch", fetch_node(http, old_url, refetch_to, NULL));

  char *cand = candidate_url_from_triage(a->triage_fix);
  if (cand && is_clean_url(cand, old_url)) {
    cJSON *cf = fetch_node(http, cand, refetch_to, NULL);
    cJSON_AddStringToObject(cf, "url", cand);
    cJSON_AddItemToObject(b, "candidate_fetch", cf);
  }
  free(cand);

  char *bundle = cJSON_PrintUnformatted(b);
  cJSON_Delete(b);
  size_t plen = strlen(SYS_PROPOSAL) + (bundle ? strlen(bundle) : 0) + 64;
  char *prompt = malloc(plen);
  if (prompt) snprintf(prompt, plen, "%s\n\nInput:\n%s\n\nOutput JSON only.",
                       SYS_PROPOSAL, bundle ? bundle : "{}");
  free(bundle);

  const char *grammar = grammar_load("repair_proposal");
  char *raw = prompt ? llm_complete(llm, prompt,
                                    grammar && *grammar ? grammar : NULL,
                                    512, 0.2, timeout) : NULL;
  free(prompt);
  cJSON *prop = extract_json(raw);
  free(raw);

  if (!prop) {
    /* LLM unreachable — no row, retry next tick (same contract as Node). */
    fprintf(stderr, "[repair] anomaly #%ld: LLM null — retry next tick\n", a->id);
    goto cleanup;
  }

  cJSON *jact = cJSON_GetObjectItem(prop, "action");
  cJSON *jurl = cJSON_GetObjectItem(prop, "new_url");
  const char *action = (jact && cJSON_IsString(jact)) ? jact->valuestring : NULL;
  const char *new_url = (jurl && cJSON_IsString(jurl)) ? jurl->valuestring : NULL;

  if (!action || strcmp(action, "url_swap") || !new_url) {
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "failed", "model returned no_fix");
    do_reject(db, a, NULL, g, model);
    goto cleanup_prop;
  }
  if (!is_clean_url(new_url, old_url)) {
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "failed", "proposed url failed validation");
    add_trunc(g, "new_url", new_url, 120);
    do_reject(db, a, NULL, g, model);
    goto cleanup_prop;
  }

  /* Gate (b): the new URL must serve data within ±tolerance of baseline. */
  http_response vr = {0};
  cJSON *vnode = fetch_node(http, new_url, refetch_to, &vr);
  cJSON_Delete(vnode);
  int vok = vr.status >= 200 && vr.status < 300;
  if (!vok) {
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "failed", "new url did not return 2xx");
    cJSON_AddNumberToObject(g, "status", (double)vr.status);
    http_response_free(&vr);
    do_reject(db, a, new_url, g, model);
    goto cleanup_prop;
  }
  int got = count_records(vr.body);
  double tol = env_double("REPAIR_RECORDS_TOLERANCE", 0.5);
  double mean = 0.0; int have_base = records_baseline(h, a->source_id, &mean);
  if (have_base) {
    double lo = mean * (1.0 - tol), hi = mean * (1.0 + tol);
    if (got < lo || got > hi) {
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "failed", "records outside ±tolerance of baseline");
      cJSON_AddNumberToObject(g, "records", got);
      cJSON_AddNumberToObject(g, "baseline_mean", mean);
      cJSON_AddNumberToObject(g, "tolerance", tol);
      http_response_free(&vr);
      do_reject(db, a, new_url, g, model);
      goto cleanup_prop;
    }
  } else if (got <= 0) {
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "failed", "new url returned zero records and no baseline");
    cJSON_AddNumberToObject(g, "records", got);
    http_response_free(&vr);
    do_reject(db, a, new_url, g, model);
    goto cleanup_prop;
  }

  /* Gate (c): sanity re-check on the new payload. */
  char sreason[260];
  int sok = sanity_ok(llm, name, cat, type, vr.body, got, sreason, sizeof sreason);
  http_response_free(&vr);
  if (!sok) {
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "failed", "sanity re-check failed");
    cJSON_AddStringToObject(g, "sanity", sreason);
    do_reject(db, a, new_url, g, model);
    goto cleanup_prop;
  }

  /* ── Gate passed ─────────────────────────────────────────────────────── */
  cJSON *gp = cJSON_CreateObject();
  cJSON_AddNumberToObject(gp, "records", got);
  if (have_base) cJSON_AddNumberToObject(gp, "baseline_mean", mean);
  else cJSON_AddNullToObject(gp, "baseline_mean");
  cJSON_AddStringToObject(gp, "sanity", sreason);
  cJSON_AddStringToObject(gp, "new_url", new_url);

  int dry = env_truthy("REPAIR_DRY");
  /* Honesty rule: merged only when old_url is matchable AND not dry. */
  int live = matchable && !dry;

  cJSON *patch = cJSON_CreateObject();
  add_str(patch, "old_url", old_url);
  cJSON_AddStringToObject(patch, "new_url", new_url);
  cJSON_AddBoolToObject(patch, "runtime_override", live);
  char *patchj = cJSON_PrintUnformatted(patch);
  cJSON_Delete(patch);

  if (live) {
    sqlite3_stmt *u;
    if (sqlite3_prepare_v2(h,
          "INSERT INTO collector_url_overrides (source_id,old_url,new_url,anomaly_id,created_at)"
          " VALUES (?1,?2,?3,?4,datetime('now'))"
          " ON CONFLICT(source_id) DO UPDATE SET old_url=excluded.old_url,"
          " new_url=excluded.new_url, anomaly_id=excluded.anomaly_id,"
          " created_at=excluded.created_at", -1, &u, NULL) == SQLITE_OK) {
      sqlite3_bind_text(u, 1, a->source_id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(u, 2, old_url, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(u, 3, new_url, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(u, 4, (sqlite3_int64)a->id);
      sqlite3_step(u); sqlite3_finalize(u);
    }
    url_override_reload(db);   /* take effect immediately */
    char *gj = cJSON_PrintUnformatted(gp);
    record_repair(h, a->id, a->source_id, "merged", "url_swap", patchj, gj, model, cls);
    free(gj);
    char res[2200];
    snprintf(res, sizeof res, "auto-repair url_swap -> %s (merged, runtime override)", new_url);
    resolve_anomaly(h, a->id, res);
    fprintf(stderr, "[repair] MERGED %s: %s -> %s\n", a->source_id, old_url, new_url);
  } else {
    /* Verified suggestion: gate passed but we can't (or won't, in dry mode)
     * apply a live override. Leave the anomaly open for a human / the digest. */
    cJSON_AddStringToObject(gp, "applied", dry ? "dry-run (override skipped)"
                            : "old_url not matchable by runtime override");
    char *gj = cJSON_PrintUnformatted(gp);
    record_repair(h, a->id, a->source_id, "verified", "url_swap", patchj, gj, model, cls);
    free(gj);
    fprintf(stderr, "[repair] VERIFIED (parked) %s -> %s (%s)\n", a->source_id,
            new_url, dry ? "dry" : "unmatchable old_url");
  }
  free(patchj);
  cJSON_Delete(gp);

cleanup_prop:
  if (prop) cJSON_Delete(prop);
cleanup:
  free(old_url); free(name); free(type); free(cat); free(status); free(errmsg);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  const char *le = getenv("LLM_ENABLED");
  if (!le || strcmp(le, "true") != 0) {
    fprintf(stderr, "[repair] skipped (LLM_ENABLED != true)\n");
    return 0;
  }
  int batch = env_int("REPAIR_BATCH", 3);

  cand_row *rows = calloc(batch, sizeof(cand_row));
  int nr = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(ctx->db->h,
        "SELECT id,source_id,triage_class,triage_evidence,triage_suggested_fix,"
        " created_at,escalation_level FROM collector_anomaly"
        " WHERE resolved_at IS NULL AND triaged_at IS NOT NULL"
        " ORDER BY created_at ASC LIMIT ?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int(s, 1, batch);
    while (nr < batch && sqlite3_step(s) == SQLITE_ROW) {
      rows[nr].id = (long)sqlite3_column_int64(s, 0);
      rows[nr].source_id = col_dup(s, 1);
      rows[nr].triage_class = col_dup(s, 2);
      rows[nr].triage_evidence = col_dup(s, 3);
      rows[nr].triage_fix = col_dup(s, 4);
      rows[nr].created_at = col_dup(s, 5);
      rows[nr].escalation = sqlite3_column_int(s, 6);
      if (rows[nr].source_id) nr++;
    }
    sqlite3_finalize(s);
  }

  for (int i = 0; i < nr; i++) {
    attempt_repair(ctx->db, ctx->llm, ctx->http, &rows[i]);
    free(rows[i].source_id); free(rows[i].triage_class);
    free(rows[i].triage_evidence); free(rows[i].triage_fix); free(rows[i].created_at);
  }
  free(rows);
  if (nr) fprintf(stderr, "[repair] tick: processed %d candidate(s)\n", nr);
  return 0;
}

static const source_def collector_repair_def = {
  .id = "collector-repair", .collector = "_maint",
  .name = "Collector Repair", .name_ja = "コレクター修復",
  .update_interval_sec = 120, .run = run };
REGISTER_SOURCE(collector_repair_def)
