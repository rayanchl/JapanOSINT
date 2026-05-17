/* core/pipeline.c — faithful port of server/src/osint/pipeline.js. */
#include "pipeline.h"
#include "prompts.h"
#include "progress.h"
#include "osint_dispatch.h"
#include "intel.h"
#include "entitystore.h"
#include "../source.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* first balanced {...} → cJSON (== JS extractJson). */
static cJSON *extract_json(const char *raw) {
  if (!raw) return NULL;
  const char *s = strchr(raw, '{');
  if (!s) return NULL;
  int depth = 0;
  for (const char *p = s; *p; p++) {
    if (*p == '{') depth++;
    else if (*p == '}') {
      if (--depth == 0) {
        char *sub = strndup(s, (size_t)(p - s + 1));
        cJSON *j = sub ? cJSON_Parse(sub) : NULL;
        free(sub);
        return j;
      }
    }
  }
  return NULL;
}

static char *lower_dup(const char *s) {
  if (!s) return strdup("");
  char *o = strdup(s);
  for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
  return o;
}

/* executed-combo set (handlerKey|entityLower) — small dynamic string set. */
typedef struct { char **k; int n, cap; } strset;
static int strset_has(strset *s, const char *k) {
  for (int i = 0; i < s->n; i++) if (!strcmp(s->k[i], k)) return 1;
  return 0;
}
static void strset_add(strset *s, const char *k) {
  if (strset_has(s, k)) return;
  if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 32;
    s->k = realloc(s->k, sizeof(char *) * s->cap); }
  s->k[s->n++] = strdup(k);
}
static void strset_free(strset *s) {
  for (int i = 0; i < s->n; i++) free(s->k[i]); free(s->k);
}

typedef struct { char *service, *entity, *type; } task_t;
typedef struct { task_t *t; int n, cap; } tasklist;
static void task_push(tasklist *tl, const char *svc, const char *ent,
                      const char *ty) {
  if (tl->n == tl->cap) { tl->cap = tl->cap ? tl->cap * 2 : 16;
    tl->t = realloc(tl->t, sizeof(task_t) * tl->cap); }
  tl->t[tl->n].service = strdup(svc);
  tl->t[tl->n].entity  = strdup(ent);
  tl->t[tl->n].type    = strdup(ty ? ty : "unknown");
  tl->n++;
}
static void tasklist_free(tasklist *tl) {
  for (int i = 0; i < tl->n; i++) {
    free(tl->t[i].service); free(tl->t[i].entity); free(tl->t[i].type);
  }
  free(tl->t);
}

/* addTask: canon + handlerKey|entityLower dedup (== JS addTask). */
static void add_task(tasklist *tl, strset *exec, const char *svc,
                     const char *entity, const char *type) {
  if (!svc || !entity || !*entity) return;
  char canon[64];
  if (!osint_canon(svc, canon, sizeof canon)) return;
  char hk[64]; osint_handler_key(canon, hk, sizeof hk);
  char *el = lower_dup(entity);
  size_t kl = strlen(hk) + strlen(el) + 2;
  char *key = malloc(kl);
  snprintf(key, kl, "%s|%s", hk, el);
  free(el);
  if (!strset_has(exec, key)) {
    strset_add(exec, key);
    task_push(tl, canon, entity, type);
  }
  free(key);
}

/* assignServices = unique task service names → progress. */
static void assign_services(osint_request *rp, tasklist *tl) {
  const char **names = malloc(sizeof(char *) * (tl->n ? tl->n : 1));
  int nn = 0;
  for (int i = 0; i < tl->n; i++) {
    int dup = 0;
    for (int j = 0; j < nn; j++) if (!strcmp(names[j], tl->t[i].service)) { dup = 1; break; }
    if (!dup) names[nn++] = tl->t[i].service;
  }
  progress_assign_services(rp, names, nn);
  free(names);
}

/* runTasks: dispatch each (persisting LIVE via `sink`), append to results,
 * update progress (== JS runTasks; sequential — faithful, dispatch is the
 * unit of work). */
static void run_tasks(osint_request *rp, db_handle *db, llm_client *llm,
                      intel_sink *sink, tasklist *tl, cJSON *results) {
  for (int i = 0; i < tl->n; i++) {
    task_t *t = &tl->t[i];
    char msg[256];
    snprintf(msg, sizeof msg, "querying %s", t->entity);
    progress_service_status(rp, t->service, "running", msg, -1, t->entity);

    osint_result r;
    osint_dispatch(db, llm, t->service, t->entity, t->type, sink, &r);

    cJSON *sr = cJSON_CreateObject();
    cJSON_AddStringToObject(sr, "name", t->service);
    cJSON_AddStringToObject(sr, "entity", t->entity);
    cJSON_AddBoolToObject(sr, "success", r.success);
    cJSON_AddNumberToObject(sr, "confidence", r.confidence);
    if (r.data) {
      cJSON *d = cJSON_Parse(r.data);            /* keep structure if JSON */
      cJSON_AddItemToObject(sr, "data", d ? d : cJSON_CreateString(r.data));
    } else cJSON_AddItemToObject(sr, "data", cJSON_CreateNull());
    cJSON_AddItemToObject(sr, "error",
      r.error ? cJSON_CreateString(r.error) : cJSON_CreateNull());
    cJSON_AddItemToArray(results, sr);

    if (!osint_is_implemented(t->service))
      progress_service_status(rp, t->service, "skipped", "not_implemented",
                              0, t->entity);
    else
      progress_service_status(rp, t->service, r.success ? "completed" : "failed",
                              r.error ? r.error : "done",
                              r.success ? 1 : 0, t->entity);
    osint_result_free(&r);
  }
}

void osint_pipeline_run(db_handle *db, llm_client *llm,
                        const char *request_id, const char *query,
                        int max_rounds) {
  osint_request *rp = progress_get(request_id);
  if (!rp) return;
  intel_sink sink = intel_sink_make(db, "osint-search", "legacy");
  if (max_rounds <= 0) max_rounds = 5;

  progress_set_phase(rp, "queued", 5);

  /* ── Phase 1: analysis ─────────────────────────────────────────────── */
  progress_set_phase(rp, "gpt_analyzing", 15);
  char *svcs = osint_services_list();
  char *p1 = prompt_analysis(query, svcs ? svcs : "");
  char *araw = llm_complete(llm, p1, grammar_load("osint_analysis"),
                            2048, 0.2, 60000);
  free(p1);
  cJSON *analysis = extract_json(araw);
  free(araw);
  cJSON *qents = analysis ? cJSON_GetObjectItem(analysis, "entities") : NULL;
  if (!qents || !cJSON_IsArray(qents)) qents = NULL;
  cJSON *rec = analysis ? cJSON_GetObjectItem(analysis, "recommended_services") : NULL;
  int chain_reason = analysis &&
    cJSON_IsTrue(cJSON_GetObjectItem(analysis, "chain_reason"));

  int ne = qents ? cJSON_GetArraySize(qents) : 0;
  if (ne > 0) {
    const char **vals = malloc(sizeof(char *) * ne);
    const char **tys  = malloc(sizeof(char *) * ne);
    for (int i = 0; i < ne; i++) {
      cJSON *e = cJSON_GetArrayItem(qents, i);
      cJSON *v = cJSON_GetObjectItem(e, "value");
      cJSON *ty = cJSON_GetObjectItem(e, "type");
      vals[i] = (v && cJSON_IsString(v)) ? v->valuestring : "";
      tys[i]  = (ty && cJSON_IsString(ty)) ? ty->valuestring : "unknown";
    }
    progress_set_entities(rp, vals, tys, ne);
    free(vals); free(tys);
  }
  cJSON *an = analysis ? cJSON_GetObjectItem(analysis, "analysis") : NULL;
  progress_set_thinking(rp, (an && cJSON_IsString(an)) ? an->valuestring : "");

  /* ── Phase 2: assign services ──────────────────────────────────────── */
  progress_set_phase(rp, "services_assigned", 20);
  strset executed = {0};
  tasklist tasks = {0};
  for (int i = 0; i < ne; i++) {
    cJSON *e = cJSON_GetArrayItem(qents, i);
    cJSON *v = cJSON_GetObjectItem(e, "value");
    const char *ev = (v && cJSON_IsString(v)) ? v->valuestring : NULL;
    cJSON *et = cJSON_GetObjectItem(e, "type");
    const char *ety = (et && cJSON_IsString(et)) ? et->valuestring : "unknown";
    cJSON *esv = cJSON_GetObjectItem(e, "services");
    cJSON *use = (esv && cJSON_IsArray(esv) && cJSON_GetArraySize(esv) > 0)
                 ? esv : rec;
    if (use && cJSON_IsArray(use)) {
      cJSON *s;
      cJSON_ArrayForEach(s, use)
        if (cJSON_IsString(s)) add_task(&tasks, &executed, s->valuestring, ev, ety);
    }
    add_task(&tasks, &executed, "JP_CORPUS_LOOKUP", ev, ety);
  }
  if (tasks.n == 0 && query && *query)
    add_task(&tasks, &executed, "JP_CORPUS_LOOKUP", query, "keyword");
  assign_services(rp, &tasks);

  /* ── Phase 3: dispatch round 0 ─────────────────────────────────────── */
  progress_set_phase(rp, "agents_working", 25);
  cJSON *results = cJSON_CreateArray();
  cJSON *discovered = cJSON_CreateArray();   /* for searchIngest step-2 */
  run_tasks(rp, db, llm, &sink, &tasks, results);
  progress_set_phase(rp, "preliminary_results", 60);
  tasklist_free(&tasks);

  /* ── Phases 4+: recursive follow-up rounds ─────────────────────────── */
  int need_more = chain_reason, round = 0;
  while (need_more && round < max_rounds) {
    round++;
    progress_set_round(rp, round);
    int pct = 60 + round * 6; if (pct > 85) pct = 85;
    progress_set_phase(rp, "followup_analyzing", pct);

    cJSON *wrap = cJSON_CreateObject();
    cJSON_AddItemToObject(wrap, "services", cJSON_Duplicate(results, 1));
    char *rj = cJSON_PrintUnformatted(wrap);
    cJSON_Delete(wrap);
    char *p2 = prompt_phase2(query, rj, svcs ? svcs : "");
    free(rj);
    char *p2raw = llm_complete(llm, p2, NULL, 2048, 0.2, 60000);
    free(p2);
    cJSON *ph2 = extract_json(p2raw);
    free(p2raw);
    cJSON *chain = ph2 ? cJSON_GetObjectItem(ph2, "chain_services") : NULL;

    strset rexec = {0};
    tasklist rtasks = {0};
    if (chain && cJSON_IsArray(chain)) {
      cJSON *c;
      cJSON_ArrayForEach(c, chain) {
        cJSON *ce = cJSON_GetObjectItem(c, "entity");
        const char *cev = (ce && cJSON_IsString(ce)) ? ce->valuestring : NULL;
        cJSON *cet = cJSON_GetObjectItem(c, "entity_type");
        const char *cety = (cet && cJSON_IsString(cet)) ? cet->valuestring : "unknown";
        cJSON *cs = cJSON_GetObjectItem(c, "source_service");
        const char *csrc = (cs && cJSON_IsString(cs)) ? cs->valuestring : "phase2";
        progress_add_discovered(rp, cev ? cev : "", cety, csrc);
        if (cev) {
          cJSON *de = cJSON_CreateObject();
          cJSON_AddStringToObject(de, "value", cev);
          cJSON_AddStringToObject(de, "type", cety);
          cJSON_AddStringToObject(de, "discovered_by", csrc);
          cJSON_AddItemToArray(discovered, de);
        }
        cJSON *csv = cJSON_GetObjectItem(c, "service");
        if (csv && cJSON_IsString(csv) && cev) {
          int before = rtasks.n;
          add_task(&rtasks, &executed, csv->valuestring, cev, cety);
          if (rtasks.n > before) {
            char *el = lower_dup(cev);
            char jk[256]; snprintf(jk, sizeof jk, "JP_CORPUS_LOOKUP|%s", el);
            free(el);
            if (!strset_has(&executed, jk)) {
              strset_add(&executed, jk);
              task_push(&rtasks, "JP_CORPUS_LOOKUP", cev, cety);
            }
          }
        }
      }
    }
    strset_free(&rexec);
    if (rtasks.n == 0) { tasklist_free(&rtasks); cJSON_Delete(ph2); break; }
    assign_services(rp, &rtasks);
    run_tasks(rp, db, llm, &sink, &rtasks, results);
    tasklist_free(&rtasks);
    need_more = ph2 && cJSON_IsTrue(cJSON_GetObjectItem(ph2, "needs_newphase"));
    cJSON_Delete(ph2);
  }
  free(svcs);

  /* ── Aggregate + complete ──────────────────────────────────────────── */
  progress_set_phase(rp, "aggregating", 90);
  int total = cJSON_GetArraySize(results), ok = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, results)
    if (cJSON_IsTrue(cJSON_GetObjectItem(r, "success"))) ok++;
  int uniq = ne;                       /* query entities in scope (approx) */
  char synth[512];
  snprintf(synth, sizeof synth,
    "Investigated \"%s\". Ran %d service call(s) across %d round(s); "
    "%d returned data. %d unique entit%s in scope.",
    query, total, round + 1, ok, uniq, uniq == 1 ? "y" : "ies");

  cJSON *res = cJSON_CreateObject();
  cJSON_AddStringToObject(res, "query", query);
  cJSON_AddItemToObject(res, "services", cJSON_Duplicate(results, 1));
  cJSON_AddStringToObject(res, "synthesis", synth);
  char *rjson = cJSON_PrintUnformatted(res);
  cJSON_Delete(res);
  progress_set_results(rp, rjson);
  free(rjson);
  progress_finish(rp, "completed");

  /* Run-summary row → intel_sink (port of searchIngest item[0]; per-service
   * rows already persisted live by osint_dispatch's dual sink). */
  char uid[128];
  snprintf(uid, sizeof uid, "osint-search|run:%s", request_id);
  char title[320];
  snprintf(title, sizeof title, "OSINT search: %s", query);
  char nowiso[32];
  time_t tt = time(NULL); struct tm g; gmtime_r(&tt, &g);
  strftime(nowiso, sizeof nowiso, "%Y-%m-%dT%H:%M:%SZ", &g);
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "query", query);
  cJSON_AddStringToObject(props, "phase", "completed");
  cJSON_AddNumberToObject(props, "rounds", round);
  if (qents) cJSON_AddItemToObject(props, "entities", cJSON_Duplicate(qents, 1));
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.uid = uid;
  it.title = title;
  it.summary = synth;
  it.body = synth;
  it.record_type = "osint_search_run";
  it.published_at = nowiso;
  it.properties_json = pj;
  it.tags_json = "[\"osint-search\"]";
  sink.emit(&sink, &it);
  free(pj);

  /* searchIngest.js step-2: entity graph via the SAME es_* surface the NER
   * enricher uses (one path). seeds = query entities (field 'query', 0.9);
   * discovered = phase-2 pivots (field service:<by>, 0.6); seed->discovered
   * = pivot_discovered weight 1.0 evidence=run uid. */
  {
    char **seeds = NULL; int nseed = 0, cseed = 0;
    for (int i = 0; i < ne; i++) {
      cJSON *e = cJSON_GetArrayItem(qents, i);
      cJSON *v = cJSON_GetObjectItem(e, "value");
      cJSON *ty = cJSON_GetObjectItem(e, "type");
      if (!v || !cJSON_IsString(v) || !v->valuestring[0]) continue;
      char *id = es_upsert_entity(db,
        (ty && cJSON_IsString(ty)) ? ty->valuestring : "unknown",
        v->valuestring);
      if (!id) continue;
      es_add_mention(db, id, uid, "osint-search", v->valuestring,
                     "query", 0.9, "osint-search");
      if (nseed == cseed) { cseed = cseed ? cseed * 2 : 8;
        seeds = realloc(seeds, sizeof(char *) * cseed); }
      seeds[nseed++] = id;
    }
    int nd = cJSON_GetArraySize(discovered);
    for (int i = 0; i < nd; i++) {
      cJSON *e = cJSON_GetArrayItem(discovered, i);
      cJSON *v = cJSON_GetObjectItem(e, "value");
      if (!v || !cJSON_IsString(v) || !v->valuestring[0]) continue;
      cJSON *ty = cJSON_GetObjectItem(e, "type");
      cJSON *by = cJSON_GetObjectItem(e, "discovered_by");
      char fld[160];
      snprintf(fld, sizeof fld, "service:%s",
               (by && cJSON_IsString(by)) ? by->valuestring : "phase2");
      char *id = es_upsert_entity(db,
        (ty && cJSON_IsString(ty)) ? ty->valuestring : "unknown",
        v->valuestring);
      if (!id) continue;
      es_add_mention(db, id, uid, "osint-search", v->valuestring,
                     fld, 0.6, "osint-search");
      for (int sx = 0; sx < nseed; sx++)
        es_add_relationship(db, seeds[sx], id, "pivot_discovered", 1.0, uid);
      free(id);
    }
    for (int sx = 0; sx < nseed; sx++) free(seeds[sx]);
    free(seeds);
  }
  cJSON_Delete(discovered);
  cJSON_Delete(results);
  if (analysis) cJSON_Delete(analysis);
  strset_free(&executed);
  fprintf(stderr, "[pipeline] %s done: %d svc, %d ok, %d round(s)\n",
          request_id, total, ok, round + 1);
}

char *osint_suggest(llm_client *llm, const char *query) {
  /* Native /completion path (raw few-shot prompt + GBNF), NOT llm_chat:
     prompt_suggestions() is a completion-style few-shot prompt. Routed
     through the chat template, gpt-oss's harmony scaffolding primes channel
     tokens that the suggestions grammar masks away, collapsing generation
     into dot-spam. Raw /completion has no harmony wrapper, so the grammar
     agrees with the model's natural continuation. */
  char *p = prompt_suggestions(query);
  char *out = llm_complete(llm, p, grammar_load("suggestions"), 1024, 0.3, 20000);
  free(p);
  cJSON *arr = extract_json(out);            /* tolerant; may be [...] too */
  if (!arr && out) arr = cJSON_Parse(out);
  free(out);
  cJSON *result = cJSON_CreateArray();
  if (arr && cJSON_IsArray(arr)) {
    int i = 0; cJSON *s;
    cJSON_ArrayForEach(s, arr) {
      if (i++ >= 9) break;
      if (cJSON_IsString(s)) cJSON_AddItemToArray(result, cJSON_CreateString(s->valuestring));
    }
  }
  if (arr) cJSON_Delete(arr);
  char *rs = cJSON_PrintUnformatted(result);
  cJSON_Delete(result);
  return rs ? rs : strdup("[]");
}
