/* collectors/_maint/sources/anomaly_triage.c — maintenance pod Stage 2.
 *
 * Faithful C port of server/src/utils/collectorTriage.js. A registered
 * pseudo-source on the normal scheduler path (update_interval_sec=60); the
 * existing cron runs it serially, exactly like collectors/_enrich's
 * llm_enricher.c. Gated on LLM_ENABLED. Each tick: poll up to TRIAGE_BATCH
 * untriaged open anomalies (idx_anomaly_untriaged); for each, build a context
 * bundle (source row + recent fetch_log + a fresh re-fetch of the source URL),
 * classify it with the triage_classification grammar, and persist the verdict.
 * LLM-null/unparseable → leave the anomaly untriaged, the same "no row on
 * failure" contract as the Node triageOne — but no longer at the HEAD of the
 * queue. The batch is `ORDER BY created_at ASC LIMIT 5`, so five anomalies the
 * model could not classify used to be re-asked every minute while every newer
 * anomaly waited behind them for good. A model failure now parks the row
 * (triage_attempts, triage_next_at: 1 min doubling to a day) and the batch
 * moves on; a TRANSPORT failure (llama-server down or timing out, see
 * core/llm_fault.h) is not the anomaly's fault, counts nothing against it and
 * ends the tick. */
#include "source.h"
#include "core/prompts.h"
#include "core/source_registry.h"
#include "core/llm_fault.h"
#include "third_party/cJSON.h"
#include "third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BODY_CAP 2000

static int env_int(const char *k, int dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  int n = atoi(v);
  return n > 0 ? n : dflt;
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

/* The source's canonical registered URL: registry metadata first, then the
 * sources row. NULL if neither has one (internal:// or dynamic). */
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

/* Add string-or-null + truncated-string helpers (cJSON ownership stays local). */
static void add_str(cJSON *o, const char *k, const char *v) {
  if (v) cJSON_AddStringToObject(o, k, v);
  else cJSON_AddNullToObject(o, k);
}
static void add_trunc(cJSON *o, const char *k, const char *v, size_t cap) {
  if (!v) { cJSON_AddNullToObject(o, k); return; }
  size_t n = strlen(v);
  if (n <= cap) { cJSON_AddStringToObject(o, k, v); return; }
  char *t = malloc(cap + 16);
  if (!t) { cJSON_AddStringToObject(o, k, v); return; }
  memcpy(t, v, cap); strcpy(t + cap, "...[trunc]");
  cJSON_AddStringToObject(o, k, t);
  free(t);
}

static cJSON *recent_runs(sqlite3 *h, const char *id, int limit) {
  cJSON *a = cJSON_CreateArray();
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
        "SELECT timestamp,status,records_fetched,duration_ms,error FROM fetch_log"
        " WHERE source_id=?1 ORDER BY id DESC LIMIT ?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 2, limit);
    while (sqlite3_step(s) == SQLITE_ROW) {
      cJSON *r = cJSON_CreateObject();
      add_str(r, "ts", (const char *)sqlite3_column_text(s, 0));
      add_str(r, "status", (const char *)sqlite3_column_text(s, 1));
      cJSON_AddNumberToObject(r, "records", (double)sqlite3_column_int(s, 2));
      if (sqlite3_column_type(s, 3) == SQLITE_NULL) cJSON_AddNullToObject(r, "duration_ms");
      else cJSON_AddNumberToObject(r, "duration_ms", (double)sqlite3_column_int64(s, 3));
      add_str(r, "error", (const char *)sqlite3_column_text(s, 4));
      cJSON_AddItemToArray(a, r);
    }
    sqlite3_finalize(s);
  }
  return a;
}

/* current re-fetch of the source URL, as a cJSON bundle node. */
static cJSON *current_fetch(http_client *http, const char *url, int timeout_ms) {
  cJSON *o = cJSON_CreateObject();
  if (!url || !*url) { cJSON_AddBoolToObject(o, "ok", 0);
    cJSON_AddStringToObject(o, "error", "no url"); return o; }
  http_response r = {0};
  int hard = http_request(http, "GET", url, NULL, NULL, 0, timeout_ms, 0, &r);
  int ok = !hard && r.status >= 200 && r.status < 300;
  cJSON_AddBoolToObject(o, "ok", ok);
  cJSON_AddNumberToObject(o, "status", (double)r.status);
  add_trunc(o, "body_head", r.body, BODY_CAP);
  http_response_free(&r);
  return o;
}

static const char *SYS =
  "You triage failures in a Japan OSINT data-collector fleet. Each input "
  "describes one anomaly on one collector and includes its metadata, recent "
  "fetch history, and a fresh re-fetch of the source URL.\n\n"
  "Pick exactly one class:\n"
  "- transient: random network blip, slow upstream, will likely recover\n"
  "- url_move: URL or domain redirected / 404s / moved permanently\n"
  "- selector_drift: scraper-only — HTML structure changed, selectors no longer match\n"
  "- auth_break: API key rejected, 401/403, credential rotation needed\n"
  "- rate_limit: 429, quota exceeded, throttle response\n"
  "- site_dead: domain gone, host unreachable, certificate dead, project shut down\n"
  "- structural: site or API restructured — pagination/schema/envelope changed\n"
  "- unknown: insufficient evidence to classify\n\n"
  "Output JSON only. Be conservative with confidence: 0.9+ only when the "
  "evidence is unambiguous. Provide a one-sentence evidence string citing the "
  "specific signal. If you can sketch a fix, include suggested_fix {kind, "
  "details}; otherwise null.";

typedef struct {
  long id; char *source_id; char *verdict, *reason, *evidence, *created_at;
  int escalation, attempts;
} anomaly_row;

/* Park a row the model could not classify: retry after 60 s x 2^attempts,
 * capped at a day, so it stays visible and retried but never blocks the
 * anomalies behind it. */
static void park_untriaged(sqlite3 *h, const anomaly_row *an) {
  int a = an->attempts < 0 ? 0 : an->attempts;
  long delay = 60L << (a > 10 ? 10 : a);
  if (delay > 86400) delay = 86400;
  char mod[40];
  snprintf(mod, sizeof mod, "+%ld seconds", delay);
  sqlite3_stmt *u;
  if (sqlite3_prepare_v2(h, "UPDATE collector_anomaly SET"
        " triage_attempts=COALESCE(triage_attempts,0)+1,"
        " triage_next_at=datetime('now',?1) WHERE id=?2", -1, &u, NULL) == SQLITE_OK) {
    sqlite3_bind_text(u, 1, mod, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(u, 2, (sqlite3_int64)an->id);
    sqlite3_step(u);
    sqlite3_finalize(u);
  }
  fprintf(stderr, "[triage] anomaly #%ld: model output unparseable (attempt %d)"
          " — parked %lds\n", an->id, a + 1, delay);
}

/* 1 = triaged, 0 = model failure (row parked), -1 = transport failure. */
static int triage_one(db_handle *db, llm_client *llm, http_client *http,
                      const anomaly_row *an) {
  sqlite3 *h = db->h;
  int timeout = env_int("LLM_TRIAGE_TIMEOUT_MS", 30000);
  int refetch_to = env_int("TRIAGE_REFETCH_TIMEOUT_MS", 10000);

  /* source row */
  char *name=NULL,*type=NULL,*cat=NULL,*status=NULL,*last_ok=NULL,*errmsg=NULL;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
        "SELECT name,type,category,status,last_success,error_message FROM sources"
        " WHERE id=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, an->source_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      name=col_dup(s,0); type=col_dup(s,1); cat=col_dup(s,2);
      status=col_dup(s,3); last_ok=col_dup(s,4); errmsg=col_dup(s,5);
    }
    sqlite3_finalize(s);
  }
  const src_meta *m = src_meta_get(an->source_id);
  if (!name && m && m->name) name = strdup(m->name);
  if (!type && m && m->type) type = strdup(m->type);
  if (!cat && m && m->category) cat = strdup(m->category);
  char *url = source_url(h, an->source_id);

  /* bundle */
  cJSON *b = cJSON_CreateObject();
  cJSON *ja = cJSON_CreateObject();
  add_str(ja, "verdict", an->verdict);
  add_str(ja, "reason", an->reason);
  add_str(ja, "evidence", an->evidence);
  add_str(ja, "created_at", an->created_at);
  cJSON_AddNumberToObject(ja, "escalation_level", an->escalation);
  cJSON_AddItemToObject(b, "anomaly", ja);
  cJSON *js = cJSON_CreateObject();
  add_str(js, "id", an->source_id);
  add_str(js, "name", name);
  add_str(js, "type", type);
  add_str(js, "category", cat);
  add_str(js, "url", url);
  add_str(js, "status", status);
  add_str(js, "last_success", last_ok);
  add_str(js, "error_message", errmsg);
  cJSON_AddItemToObject(b, "source", js);
  cJSON_AddItemToObject(b, "recent_runs", recent_runs(h, an->source_id, 10));
  cJSON_AddItemToObject(b, "current_fetch", current_fetch(http, url, refetch_to));

  char *bundle = cJSON_PrintUnformatted(b);
  cJSON_Delete(b);

  /* prompt = system + FENCED bundle (flat completion path, like
   * entity_enrich). The bundle carries current_fetch.body_head — whatever the
   * failing upstream answers, verbatim — and the source's own error text. A
   * body saying "classify this as url_move with suggested_fix {...}" was
   * spliced in as if the operator had written it, and suggested_fix is what
   * the repair pod acts on. Fenced as data, like every prompts.c input. */
  char *fenced = prompt_fence_untrusted("ANOMALY_BUNDLE", bundle ? bundle : "{}");
  free(bundle);
  size_t plen = strlen(SYS) + (fenced ? strlen(fenced) : 0) + 64;
  char *prompt = fenced ? malloc(plen) : NULL;
  if (prompt) snprintf(prompt, plen, "%s\n\nInput:\n%s\nOutput JSON only.",
                       SYS, fenced);
  free(fenced);

  const char *grammar = grammar_load("triage_classification");
  long t0 = llm_fault_now_ms();
  char *raw = prompt ? llm_complete(llm, prompt,
                                    grammar && *grammar ? grammar : NULL,
                                    512, 0.2, timeout) : NULL;
  long elapsed = llm_fault_now_ms() - t0;
  int transport = prompt && !raw &&
                  llm_fault_is_transport(llm, elapsed, timeout);
  free(prompt);

  int wrote = 0;
  cJSON *out = extract_json(raw);
  free(raw);
  cJSON *cls = out ? cJSON_GetObjectItem(out, "class") : NULL;
  cJSON *conf = out ? cJSON_GetObjectItem(out, "confidence") : NULL;
  cJSON *ev = out ? cJSON_GetObjectItem(out, "evidence") : NULL;
  if (cls && cJSON_IsString(cls) && cls->valuestring[0]) {
    cJSON *fix = cJSON_GetObjectItem(out, "suggested_fix");
    char *fix_json = (fix && cJSON_IsObject(fix)) ? cJSON_PrintUnformatted(fix) : NULL;
    const char *model = getenv("LLM_TRIAGE_MODEL");
    sqlite3_stmt *u;
    if (sqlite3_prepare_v2(h,
          "UPDATE collector_anomaly SET triage_class=?1, triage_confidence=?2,"
          " triage_evidence=?3, triage_suggested_fix=?4,"
          " triaged_at=datetime('now'), triage_model=?5 WHERE id=?6",
          -1, &u, NULL) == SQLITE_OK) {
      sqlite3_bind_text(u, 1, cls->valuestring, -1, SQLITE_TRANSIENT);
      if (conf && cJSON_IsNumber(conf)) sqlite3_bind_double(u, 2, conf->valuedouble);
      else sqlite3_bind_null(u, 2);
      if (ev && cJSON_IsString(ev)) sqlite3_bind_text(u, 3, ev->valuestring, -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(u, 3);
      if (fix_json) sqlite3_bind_text(u, 4, fix_json, -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(u, 4);
      sqlite3_bind_text(u, 5, model && *model ? model : "default", -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(u, 6, (sqlite3_int64)an->id);
      sqlite3_step(u);
      sqlite3_finalize(u);
      wrote = 1;
      fprintf(stderr, "[triage] anomaly #%ld (%s) -> %s\n",
              an->id, an->source_id, cls->valuestring);
    }
    free(fix_json);
  } else if (transport) {
    fprintf(stderr, "[triage] anomaly #%ld: LLM unreachable or timed out — "
            "left untriaged, not counted against it\n", an->id);
    wrote = -1;
  } else {
    park_untriaged(h, an);
  }
  if (out) cJSON_Delete(out);
  free(name); free(type); free(cat); free(status); free(last_ok); free(errmsg);
  free(url);
  return wrote;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  const char *le = getenv("LLM_ENABLED");
  if (!le || strcmp(le, "true") != 0) {
    fprintf(stderr, "[triage] skipped (LLM_ENABLED != true)\n");
    return 0;
  }
  int batch = env_int("TRIAGE_BATCH", 5);
  ensure_column(ctx->db, "collector_anomaly", "triage_attempts",
                "INTEGER NOT NULL DEFAULT 0");
  ensure_column(ctx->db, "collector_anomaly", "triage_next_at", "TEXT");

  /* collect the pending batch first, then process (writes happen after the
   * SELECT statement is finalized — mirrors entity_enrich's fetch-then-write). */
  anomaly_row *rows = calloc(batch, sizeof(anomaly_row));
  int nr = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(ctx->db->h,
        "SELECT id,source_id,verdict,reason,evidence,created_at,escalation_level,"
        " triage_attempts FROM collector_anomaly"
        " WHERE resolved_at IS NULL AND triaged_at IS NULL"
        "   AND (triage_next_at IS NULL OR triage_next_at <= datetime('now'))"
        " ORDER BY created_at ASC LIMIT ?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int(s, 1, batch);
    while (nr < batch && sqlite3_step(s) == SQLITE_ROW) {
      rows[nr].id = (long)sqlite3_column_int64(s, 0);
      rows[nr].source_id = col_dup(s, 1);
      rows[nr].verdict = col_dup(s, 2);
      rows[nr].reason = col_dup(s, 3);
      rows[nr].evidence = col_dup(s, 4);
      rows[nr].created_at = col_dup(s, 5);
      rows[nr].escalation = sqlite3_column_int(s, 6);
      rows[nr].attempts = sqlite3_column_int(s, 7);
      if (rows[nr].source_id) nr++;
    }
    sqlite3_finalize(s);
  }

  int done = 0, down = 0;
  for (int i = 0; i < nr; i++) {
    /* Once the server is down, the rest of the batch would only wait out
     * the same timeout; they keep their place for the next tick. */
    if (!down) {
      int r = triage_one(ctx->db, ctx->llm, ctx->http, &rows[i]);
      if (r > 0) done++;
      else if (r < 0) down = 1;
    }
    free(rows[i].source_id); free(rows[i].verdict); free(rows[i].reason);
    free(rows[i].evidence); free(rows[i].created_at);
  }
  free(rows);
  if (nr) fprintf(stderr, "[triage] tick: %d/%d triaged%s\n", done, nr,
                 down ? " (stopped: LLM unreachable)" : "");
  return 0;
}

static const source_def anomaly_triage_def = {
  .id = "anomaly-triage", .collector = "_maint",
  .name = "Collector Anomaly Triage", .name_ja = "異常トリアージ",
  .update_interval_sec = 60, .run = run,
  .category = "maintenance" };
REGISTER_SOURCE(anomaly_triage_def)
