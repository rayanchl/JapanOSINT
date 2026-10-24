/* core/sourceapi_summary.c — GET /api/sources/:id/summary.
 *
 * A small model is asked to describe one source and the kind of records it
 * yields, from the MEASURED facts the detail route reports and nothing else:
 * the registry row, the declared engine row, the record-type histogram, the
 * field inventory, three stored titles. The prompt says "only these facts";
 * the answer is constrained to a JSON schema; and the result is cached in the
 * source_summary table by (source id, a fingerprint of the facts, the model),
 * so a restart does not re-pay 18,883 prompts and a source whose data moved
 * gets a fresh summary while an unchanged one does not.
 *
 * House rule 1 applies to prose as much as to records. When the model cannot
 * be reached, times out, answers non-2xx or produces nothing that parses, the
 * route returns the llm_status code and THE FACTS it would have summarised —
 * never a summary that was not generated, never an empty string standing in
 * for one. The client shows the facts under the error. */
#include "sourceapi.h"
#include "../source.h"
#include "llm.h"
#include "httpclient.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static void iso_now(char *buf, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(buf, n, "%04u-%02u-%02uT%02u:%02u:%02uZ",
           (unsigned)(tm.tm_year + 1900) % 10000u, (unsigned)(tm.tm_mon + 1) % 100u,
           (unsigned)tm.tm_mday % 100u, (unsigned)tm.tm_hour % 100u,
           (unsigned)tm.tm_min % 100u, (unsigned)tm.tm_sec % 100u);
}
static unsigned long long fnv_str(unsigned long long h, const char *s) {
  for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++)
    h = (h ^ *p) * 1099511628211ULL;
  return (h ^ 0x1f) * 1099511628211ULL;
}
static void copy_if(cJSON *dst, const cJSON *src, const char *key) {
  const cJSON *v = src ? cJSON_GetObjectItem(src, key) : NULL;
  if (!v || cJSON_IsNull(v)) return;
  cJSON_AddItemToObject(dst, key, cJSON_Duplicate(v, 1));
}
/* The facts handed to the model: a compact, bounded projection of the detail
 * body. Bounded on purpose (a prompt is a consumer that cannot take
 * everything), and the bounds are stated in `facts.bounds`. */
#define SUMMARY_RT_MAX     8
#define SUMMARY_FIELDS_MAX 15
#define SUMMARY_TITLES_MAX 3
static cJSON *facts_from_detail(db_handle *db, const char *id, const cJSON *det) {
  cJSON *f = cJSON_CreateObject();
  const cJSON *reg = cJSON_GetObjectItem(det, "registry");
  cJSON *r = cJSON_CreateObject();
  for (const char **k = (const char *[]){"id","name","name_ja","collector","category","type",
                                        "engine","kind","interval","url","host","description",
                                        "license",NULL}; *k; k++) copy_if(r, reg, *k);
  cJSON_AddItemToObject(f, "registry", r);

  const cJSON *eng = cJSON_GetObjectItem(det, "engine");
  cJSON *e = cJSON_CreateObject();
  copy_if(e, eng, "engine"); copy_if(e, eng, "mode"); copy_if(e, eng, "want");
  copy_if(e, eng, "kind"); copy_if(e, eng, "entity_tokens"); copy_if(e, eng, "uses_date_tokens");
  {
    const cJSON *req = cJSON_GetObjectItem(eng, "request");
    cJSON *rq = cJSON_CreateObject();
    copy_if(rq, req, "method"); copy_if(rq, req, "url"); copy_if(rq, req, "key_env");
    cJSON_AddItemToObject(e, "request", rq);
    const cJSON *pa = cJSON_GetObjectItem(eng, "parsing");
    cJSON *pj = cJSON_CreateObject();
    copy_if(pj, pa, "array_path"); copy_if(pj, pa, "title_keys"); copy_if(pj, pa, "id_keys");
    copy_if(pj, pa, "record_type"); copy_if(pj, pa, "lang");
    cJSON_AddItemToObject(e, "parsing", pj);
    const cJSON *pg = cJSON_GetObjectItem(eng, "paging");
    copy_if(e, pg, "strategy");
    const cJSON *dh = cJSON_GetObjectItem(eng, "detail_hop");
    const cJSON *dd = dh ? cJSON_GetObjectItem(dh, "declared") : NULL;
    if (dd) cJSON_AddBoolToObject(e, "detail_hop", cJSON_IsTrue(dd));
    copy_if(e, eng, "note");
  }
  cJSON_AddItemToObject(f, "engine", e);

  const cJSON *data = cJSON_GetObjectItem(det, "data");
  cJSON *d = cJSON_CreateObject();
  copy_if(d, data, "item_count"); copy_if(d, data, "geocoded"); copy_if(d, data, "notices");
  copy_if(d, data, "first_fetched"); copy_if(d, data, "last_fetched");
  copy_if(d, data, "first_published"); copy_if(d, data, "last_published");
  copy_if(d, data, "languages");
  {
    const cJSON *rt = cJSON_GetObjectItem(data, "record_types");
    const cJSON *vals = rt ? cJSON_GetObjectItem(rt, "values") : NULL;
    cJSON *arr = cJSON_CreateArray(); int n = 0; const cJSON *v;
    cJSON_ArrayForEach(v, vals) {
      if (n++ >= SUMMARY_RT_MAX) break;   /* exhaustive-ok: a prompt's bounded view, stated in facts.bounds */
      cJSON_AddItemToArray(arr, cJSON_Duplicate(v, 1));
    }
    cJSON_AddItemToObject(d, "record_types", arr);
    const cJSON *fi = cJSON_GetObjectItem(data, "field_inventory");
    const cJSON *ff = fi ? cJSON_GetObjectItem(fi, "fields") : NULL;
    const cJSON *fv = ff ? cJSON_GetObjectItem(ff, "values") : NULL;
    cJSON *fa = cJSON_CreateArray(); n = 0;
    cJSON_ArrayForEach(v, fv) {
      if (n++ >= SUMMARY_FIELDS_MAX) break;   /* exhaustive-ok: a prompt's bounded view, stated in facts.bounds */
      const cJSON *k = cJSON_GetObjectItem(v, "key");
      if (k && cJSON_IsString(k)) cJSON_AddItemToArray(fa, cJSON_CreateString(k->valuestring));
    }
    cJSON_AddItemToObject(d, "fields", fa);
    if (fi) copy_if(d, fi, "sample_size");
  }
  {
    cJSON *titles = cJSON_CreateArray();
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db->h,
          "SELECT title FROM intel_items WHERE source_id=?1 AND title IS NOT NULL "
          "AND (record_type IS NULL OR record_type NOT LIKE 'collector-%') "
          "ORDER BY fetched_at DESC LIMIT ?2", -1, &st, NULL) == SQLITE_OK) {
      sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(st, 2, SUMMARY_TITLES_MAX);
      while (sqlite3_step(st) == SQLITE_ROW) {
        const char *t = (const char *)sqlite3_column_text(st, 0);
        if (!t) continue;
        char cut[241]; snprintf(cut, sizeof cut, "%s", t);
        cJSON_AddItemToArray(titles, cJSON_CreateString(cut));
      }
      sqlite3_finalize(st);
    }
    cJSON_AddItemToObject(d, "sample_titles", titles);
  }
  cJSON_AddItemToObject(f, "data", d);

  const cJSON *status = cJSON_GetObjectItem(det, "status");
  cJSON *s = cJSON_CreateObject();
  copy_if(s, status, "status"); copy_if(s, status, "last_success"); copy_if(s, status, "error_message");
  {
    const cJSON *tr = status ? cJSON_GetObjectItem(status, "trust") : NULL;
    copy_if(s, tr, "grade");
  }
  cJSON_AddItemToObject(f, "status", s);
  const cJSON *sch = cJSON_GetObjectItem(det, "schedule");
  cJSON *c = cJSON_CreateObject();
  copy_if(c, sch, "kind"); copy_if(c, sch, "declaredInterval"); copy_if(c, sch, "effectiveInterval");
  cJSON_AddItemToObject(f, "schedule", c);
  const cJSON *cred = cJSON_GetObjectItem(det, "credentials");
  cJSON *cr = cJSON_CreateObject();
  copy_if(cr, cred, "requiresKey"); copy_if(cr, cred, "configured");
  cJSON_AddItemToObject(f, "credentials", cr);

  cJSON *bounds = cJSON_CreateObject();
  cJSON_AddNumberToObject(bounds, "record_types_max", SUMMARY_RT_MAX);
  cJSON_AddNumberToObject(bounds, "fields_max", SUMMARY_FIELDS_MAX);
  cJSON_AddNumberToObject(bounds, "sample_titles_max", SUMMARY_TITLES_MAX);
  cJSON_AddItemToObject(f, "bounds", bounds);
  return f;
}

/* A fingerprint of what the model is shown: when none of it changed, the
 * cached summary still describes this source. */
static void facts_fingerprint(const cJSON *facts, char *out, size_t cap) {
  char *js = cJSON_PrintUnformatted(facts);
  unsigned long long h = fnv_str(1469598103934665603ULL, js ? js : "");
  free(js);
  snprintf(out, cap, "%016llx", h);
}

/* The model the suggest server is running, from GET /v1/models — cached for
 * a minute per process because every summary would otherwise ask twice. An
 * unreachable server yields "" and the chat call below reports WHY. */
static char g_model[128]; static long long g_model_at = 0;
static pthread_mutex_t g_model_mu = PTHREAD_MUTEX_INITIALIZER;
static void model_name(llm_client *llm, http_client *http, char *out, size_t cap) {
  pthread_mutex_lock(&g_model_mu);
  long long now = (long long)time(NULL);
  if (g_model_at && now - g_model_at < 60) { snprintf(out, cap, "%s", g_model); pthread_mutex_unlock(&g_model_mu); return; }
  char url[600];
  size_t n = strlen(llm->base_url);
  snprintf(url, sizeof url, "%.*s/v1/models", (int)(n && llm->base_url[n - 1] == '/' ? n - 1 : n), llm->base_url);
  http_response r = {0};
  g_model[0] = 0;
  if (http_request(http, "GET", url, NULL, NULL, 0, 3000, 0, &r) == 0 && r.status == 200 && r.body) {
    cJSON *j = cJSON_Parse(r.body);
    const cJSON *data = j ? cJSON_GetObjectItem(j, "data") : NULL;
    const cJSON *first = data ? cJSON_GetArrayItem(data, 0) : NULL;
    const cJSON *idj = first ? cJSON_GetObjectItem(first, "id") : NULL;
    if (idj && cJSON_IsString(idj)) {
      /* llama-server reports the gguf path; keep the file name */
      const char *b = strrchr(idj->valuestring, '/');
      snprintf(g_model, sizeof g_model, "%s", b ? b + 1 : idj->valuestring);
    }
    cJSON_Delete(j);
  }
  http_response_free(&r);
  g_model_at = now;
  snprintf(out, cap, "%s", g_model);
  pthread_mutex_unlock(&g_model_mu);
}

static const char *SUMMARY_SCHEMA =
  "{\"type\":\"object\",\"properties\":{"
  "\"summary\":{\"type\":\"string\"},"
  "\"result_type\":{\"type\":\"string\"},"
  "\"caveats\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"maxItems\":4}},"
  "\"required\":[\"summary\",\"result_type\",\"caveats\"]}";

static const char *SYSTEM_PROMPT =
  "You describe ONE data source of an OSINT platform to an analyst who is deciding "
  "whether to rely on it. You are given measured facts as JSON: the registry row, how "
  "the engine fetches and parses it, what records it has stored (counts, record types, "
  "field names, a few titles), its status and schedule. Use ONLY those facts. Never "
  "invent a count, a coverage claim, a country or a date that is not in the facts; if "
  "something is unknown, say it is not recorded. Answer as JSON with: summary (2-3 "
  "plain sentences: what the upstream is, what one record represents, how it is "
  "reached), result_type (a 2-6 word noun phrase naming the kind of record it yields, "
  "e.g. \"company registry entries\" or \"earthquake events\"), caveats (0-4 short "
  "strings: a missing credential, zero stored records, a notice, an unreachable kind, "
  "a stale last fetch). Write in English; keep Japanese names as given.";

static char *messages_json(const cJSON *facts) {
  cJSON *arr = cJSON_CreateArray();
  cJSON *sys = cJSON_CreateObject();
  cJSON_AddStringToObject(sys, "role", "system");
  cJSON_AddStringToObject(sys, "content", SYSTEM_PROMPT);
  cJSON_AddItemToArray(arr, sys);
  cJSON *usr = cJSON_CreateObject();
  cJSON_AddStringToObject(usr, "role", "user");
  char *fj = cJSON_PrintUnformatted(facts);
  char *content = NULL;
  if (fj) {
    size_t n = strlen(fj) + 64;
    content = malloc(n);
    if (content) snprintf(content, n, "Facts about the source:\n%s", fj);
  }
  cJSON_AddStringToObject(usr, "content", content ? content : "Facts about the source: (unavailable)");
  free(fj); free(content);
  cJSON_AddItemToArray(arr, usr);
  char *js = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return js;
}

/* A model's reply may wrap the object in prose or a code fence; take the
 * outermost {...}. */
static cJSON *extract_obj(const char *raw) {
  if (!raw) return NULL;
  cJSON *j = cJSON_Parse(raw);
  if (j && cJSON_IsObject(j)) return j;
  if (j) cJSON_Delete(j);
  const char *a = strchr(raw, '{'); const char *b = strrchr(raw, '}');
  if (!a || !b || b <= a) return NULL;
  char *cut = malloc((size_t)(b - a) + 2);
  if (!cut) return NULL;
  memcpy(cut, a, (size_t)(b - a) + 1); cut[b - a + 1] = 0;
  j = cJSON_Parse(cut);
  free(cut);
  if (j && !cJSON_IsObject(j)) { cJSON_Delete(j); j = NULL; }
  return j;
}

static char *cached_summary(db_handle *db, const char *id, const char *fp, const char *model) {
  sqlite3_stmt *st;
  char *out = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT summary_json FROM source_summary WHERE source_id=?1 AND fingerprint=?2 AND model=?3",
        -1, &st, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 2, fp, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 3, model, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(st) == SQLITE_ROW) {
    const char *v = (const char *)sqlite3_column_text(st, 0);
    if (v) out = strdup(v);
  }
  sqlite3_finalize(st);
  return out;
}
static void store_summary(db_handle *db, const char *id, const char *fp, const char *model,
                          const char *json, int facts_used) {
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h,
        "INSERT OR REPLACE INTO source_summary(source_id,fingerprint,model,summary_json,facts_used,generated_at) "
        "VALUES(?1,?2,?3,?4,?5,datetime('now'))", -1, &st, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 2, fp, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 3, model, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 4, json, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(st, 5, facts_used);
  sqlite3_step(st);
  sqlite3_finalize(st);
}
static int count_facts(const cJSON *facts) {
  int n = 0; const cJSON *sec;
  cJSON_ArrayForEach(sec, facts) {
    if (!cJSON_IsObject(sec)) { n++; continue; }
    const cJSON *k; cJSON_ArrayForEach(k, sec) n++;
  }
  return n;
}

char *sourceapi_summary(db_handle *db, const char *id, int refresh) {
  if (!registry_get(id)) return NULL;
  char *dj = sourceapi_detail(db, id);
  cJSON *det = dj ? cJSON_Parse(dj) : NULL;
  free(dj);
  if (!det) return strdup("{\"error\":\"detail_unavailable\"}");
  cJSON *facts = facts_from_detail(db, id, det);
  cJSON_Delete(det);
  char fp[24]; facts_fingerprint(facts, fp, sizeof fp);
  int nfacts = count_facts(facts);

  http_client *http = http_client_new();
  llm_client llm; llm_init_suggest(&llm, http);
  llm.bound_queue_wait = 1;          /* a page is waiting; never queue forever */
  char model[128]; model_name(&llm, http, model, sizeof model);
  const char *model_key = model[0] ? model : "unknown";
  char now[32]; iso_now(now, sizeof now);

  if (!refresh) {
    char *hit = cached_summary(db, id, fp, model_key);
    if (hit) {
      cJSON *o = cJSON_Parse(hit);
      free(hit);
      if (o) {
        cJSON_ReplaceItemInObject(o, "cached", cJSON_CreateBool(1));
        cJSON_AddStringToObject(o, "served_at", now);
        char *js = cJSON_PrintUnformatted(o);
        cJSON_Delete(o); cJSON_Delete(facts); http_client_free(http);
        return js;
      }
    }
  }

  char *msgs = messages_json(facts);
  llm_status st = LLM_ERR_BAD_REQUEST; long http_st = 0;
  char *raw = msgs ? llm_chat_ex(&llm, msgs, SUMMARY_SCHEMA, 400, 0.2, 25000, &st, &http_st) : NULL;
  free(msgs);
  http_client_free(http);

  cJSON *ans = extract_obj(raw);
  free(raw);
  const cJSON *summ = ans ? cJSON_GetObjectItem(ans, "summary") : NULL;
  if (!ans || !summ || !cJSON_IsString(summ) || !summ->valuestring[0]) {
    if (ans) cJSON_Delete(ans);
    if (st == LLM_OK) st = LLM_ERR_EMPTY;      /* answered, but not with a summary */
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "error", llm_status_code(st));
    cJSON_AddNumberToObject(o, "http_status", (double)http_st);
    cJSON_AddStringToObject(o, "model", model_key);
    cJSON_AddStringToObject(o, "llm_base_url", llm.base_url);
    cJSON_AddNumberToObject(o, "facts_used", nfacts);
    cJSON_AddStringToObject(o, "fingerprint", fp);
    cJSON_AddStringToObject(o, "attempted_at", now);
    cJSON_AddItemToObject(o, "facts", facts);
    char *js = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return js;
  }
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "summary", summ->valuestring);
  const cJSON *rt = cJSON_GetObjectItem(ans, "result_type");
  if (rt && cJSON_IsString(rt)) cJSON_AddStringToObject(o, "result_type", rt->valuestring);
  else cJSON_AddNullToObject(o, "result_type");
  const cJSON *cv = cJSON_GetObjectItem(ans, "caveats");
  cJSON_AddItemToObject(o, "caveats", (cv && cJSON_IsArray(cv)) ? cJSON_Duplicate(cv, 1) : cJSON_CreateArray());
  cJSON_AddStringToObject(o, "model", model_key);
  cJSON_AddNumberToObject(o, "facts_used", nfacts);
  cJSON_AddStringToObject(o, "fingerprint", fp);
  cJSON_AddStringToObject(o, "generated_at", now);
  cJSON_AddBoolToObject(o, "cached", 0);
  cJSON_Delete(ans);
  cJSON_Delete(facts);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  if (js) store_summary(db, id, fp, model_key, js, nfacts);
  return js;
}
