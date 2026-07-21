/* core/progress.c — faithful port of server/src/osint/progressTracker.js.
 * See progress.h for the contract / thread-safety model. */
#include "progress.h"
#include "../third_party/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */
#include <time.h>
#include <pthread.h>

/* ---- PHASES (order/spelling identical to progressTracker.js) ---- */
static const char *const PHASES[] = {
  "queued", "image_analyzing", "gpt_analyzing", "services_assigned",
  "services_launching", "agents_working", "preliminary_results",
  "awaiting_review", "followup_analyzing", "aggregating", "completed", "error",
};
static int phase_known(const char *p) {
  for (size_t i = 0; i < sizeof PHASES / sizeof *PHASES; i++)
    if (strcmp(PHASES[i], p) == 0) return 1;
  return 0;
}

/* ---- small dynamic-string helpers (NULL -> "") ---- */
static char *dup_s(const char *s) {
  if (!s) s = "";
  size_t n = strlen(s) + 1;
  char *o = malloc(n);
  if (o) memcpy(o, s, n);
  return o;
}
static void replace_s(char **dst, const char *s) {
  char *n = dup_s(s);
  if (n) { free(*dst); *dst = n; }
}

/* ---- entity / service / phase-history rows ---- */
typedef struct { char *value; char *type; } entity_t;            /* entities[], all_entities[] */
typedef struct { char *value; char *type; char *discovered_by; } disc_t; /* discovered_entities[] */
typedef struct {
  char *name; char *status; char *status_message; char *entities;
  long results_count; int is_followup;
} service_t;
typedef struct { char *phase; long timestamp; long progress; } phist_t;

/* generic grow-by-one vector backing the above (manual; bounded by run size) */
#define VEC(T) struct { T *p; int n, cap; }
static int vec_reserve(void **p, int *cap, int need, size_t elt) {
  if (need <= *cap) return 1;
  int nc = *cap ? *cap * 2 : 8;
  while (nc < need) nc *= 2;
  void *np = realloc(*p, (size_t)nc * elt);
  if (!np) return 0;
  *p = np; *cap = nc; return 1;
}

struct osint_request {
  struct osint_request *next;     /* registry singly-linked list */

  char *request_id;
  char *query;
  char *phase;
  long  progress_percent;
  char *gpt_thinking;
  long  total_results;
  char *preliminary_findings;
  long  created_at;
  long  updated_at;

  VEC(entity_t)  entities;
  VEC(char *)    services_assigned;
  VEC(service_t) services;

  long stat_total, stat_active, stat_completed, stat_failed, stat_skipped;

  VEC(phist_t)  phase_history;
  long current_round;
  long max_rounds;
  int  awaiting_user_action;

  VEC(disc_t)    discovered_entities;
  VEC(entity_t)  confirmed_entities;   /* always [] — JS never populates it */
  VEC(entity_t)  all_entities;

  char *results_json;   /* serialized results object, or NULL */
  int   done;
};

/* ---- process-global registry + single coarse lock ---- */
static osint_request   *g_head = NULL;
static int              g_count = 0;
static pthread_mutex_t  g_lock = PTHREAD_MUTEX_INITIALIZER;

static long now_secs(void) { return (long)time(NULL); } /* Math.floor(Date.now()/1000) */

/* updated_at = now. (JS _touch() also emits 'update'; in C the SSE polls.) */
static void touch_locked(osint_request *r) { r->updated_at = now_secs(); }

/* ---- registry ---- */
static osint_request *find_locked(const char *id) {
  for (osint_request *r = g_head; r; r = r->next)
    if (strcmp(r->request_id, id) == 0) return r;
  return NULL;
}

osint_request *progress_create(const char *request_id, const char *query,
                               int max_rounds) {
  if (!request_id) return NULL;
  pthread_mutex_lock(&g_lock);

  osint_request *r = find_locked(request_id);
  if (r) { pthread_mutex_unlock(&g_lock); return r; }

  r = calloc(1, sizeof *r);
  if (!r) { pthread_mutex_unlock(&g_lock); return NULL; }
  r->request_id          = dup_s(request_id);
  r->query               = dup_s(query);
  r->phase               = dup_s("queued");
  r->progress_percent    = 0;
  r->gpt_thinking        = dup_s("");
  r->total_results       = 0;
  r->preliminary_findings = dup_s("");
  r->created_at          = now_secs();
  r->updated_at          = r->created_at;
  r->current_round       = 0;
  r->max_rounds          = (max_rounds > 0) ? max_rounds : 5; /* JS default 5 */
  r->awaiting_user_action = 0;
  r->results_json        = NULL;
  r->done                = 0;

  r->next = g_head;
  g_head  = r;
  g_count++;

  /* Bounded retention: drop oldest finished request beyond 200 (JS parity). */
  if (g_count > 200) {
    osint_request *oldest = NULL, *oldest_prev = NULL, *prev = NULL;
    for (osint_request *c = g_head; c; prev = c, c = c->next) {
      if (!c->done) continue;
      if (!oldest || c->updated_at < oldest->updated_at) {
        oldest = c; oldest_prev = prev;
      }
    }
    if (oldest) {
      if (oldest_prev) oldest_prev->next = oldest->next;
      else             g_head = oldest->next;
      g_count--;
      /* free the evicted request */
      free(oldest->request_id); free(oldest->query); free(oldest->phase);
      free(oldest->gpt_thinking); free(oldest->preliminary_findings);
      for (int i = 0; i < oldest->entities.n; i++) {
        free(oldest->entities.p[i].value); free(oldest->entities.p[i].type);
      }
      free(oldest->entities.p);
      for (int i = 0; i < oldest->services_assigned.n; i++)
        free(oldest->services_assigned.p[i]);
      free(oldest->services_assigned.p);
      for (int i = 0; i < oldest->services.n; i++) {
        free(oldest->services.p[i].name); free(oldest->services.p[i].status);
        free(oldest->services.p[i].status_message);
        free(oldest->services.p[i].entities);
      }
      free(oldest->services.p);
      for (int i = 0; i < oldest->phase_history.n; i++)
        free(oldest->phase_history.p[i].phase);
      free(oldest->phase_history.p);
      for (int i = 0; i < oldest->discovered_entities.n; i++) {
        free(oldest->discovered_entities.p[i].value);
        free(oldest->discovered_entities.p[i].type);
        free(oldest->discovered_entities.p[i].discovered_by);
      }
      free(oldest->discovered_entities.p);
      for (int i = 0; i < oldest->confirmed_entities.n; i++) {
        free(oldest->confirmed_entities.p[i].value);
        free(oldest->confirmed_entities.p[i].type);
      }
      free(oldest->confirmed_entities.p);
      for (int i = 0; i < oldest->all_entities.n; i++) {
        free(oldest->all_entities.p[i].value);
        free(oldest->all_entities.p[i].type);
      }
      free(oldest->all_entities.p);
      free(oldest->results_json);
      free(oldest);
    }
  }

  pthread_mutex_unlock(&g_lock);
  return r;
}

osint_request *progress_get(const char *request_id) {
  if (!request_id) return NULL;
  pthread_mutex_lock(&g_lock);
  osint_request *r = find_locked(request_id);
  pthread_mutex_unlock(&g_lock);
  return r;
}

/* ---- setters ---- */
void progress_set_phase(osint_request *r, const char *phase, int percent) {
  if (!r) return;
  const char *p = (phase && phase_known(phase)) ? phase : "unknown";
  pthread_mutex_lock(&g_lock);
  replace_s(&r->phase, p);
  if (percent >= 0) r->progress_percent = percent; /* JS Number.isFinite guard */
  if (vec_reserve((void **)&r->phase_history.p, &r->phase_history.cap,
                  r->phase_history.n + 1, sizeof(phist_t))) {
    phist_t *h = &r->phase_history.p[r->phase_history.n++];
    h->phase = dup_s(p);
    h->timestamp = now_secs();
    h->progress = r->progress_percent;
  }
  touch_locked(r);
  pthread_mutex_unlock(&g_lock);
}

void progress_set_thinking(osint_request *r, const char *thinking) {
  if (!r) return;
  pthread_mutex_lock(&g_lock);
  replace_s(&r->gpt_thinking, thinking ? thinking : "");
  touch_locked(r);
  pthread_mutex_unlock(&g_lock);
}

void progress_set_entities(osint_request *r, const char *const *values,
                           const char *const *types, int n) {
  if (!r) return;
  if (n < 0) n = 0;
  pthread_mutex_lock(&g_lock);
  for (int i = 0; i < r->entities.n; i++) {
    free(r->entities.p[i].value); free(r->entities.p[i].type);
  }
  r->entities.n = 0;
  if (vec_reserve((void **)&r->entities.p, &r->entities.cap, n,
                  sizeof(entity_t))) {
    for (int i = 0; i < n; i++) {
      r->entities.p[i].value = dup_s(values ? values[i] : NULL);
      r->entities.p[i].type  = dup_s(types  ? types[i]  : NULL);
      r->entities.n++;
    }
  }
  touch_locked(r);
  pthread_mutex_unlock(&g_lock);
}

/* find/create a service row (caller holds g_lock) */
static service_t *svc_find_locked(osint_request *r, const char *name) {
  for (int i = 0; i < r->services.n; i++)
    if (strcmp(r->services.p[i].name, name) == 0) return &r->services.p[i];
  return NULL;
}
static service_t *svc_push_locked(osint_request *r, const char *name) {
  if (!vec_reserve((void **)&r->services.p, &r->services.cap,
                   r->services.n + 1, sizeof(service_t)))
    return NULL;
  service_t *s = &r->services.p[r->services.n++];
  s->name           = dup_s(name);
  s->status         = dup_s("pending");
  s->status_message = dup_s("");
  s->entities       = dup_s("");
  s->results_count  = 0;
  s->is_followup    = r->current_round > 0;
  return s;
}

void progress_assign_services(osint_request *r, const char *const *names,
                              int n) {
  if (!r) return;
  if (n < 0) n = 0;
  pthread_mutex_lock(&g_lock);
  /* services_assigned = [...names] */
  for (int i = 0; i < r->services_assigned.n; i++)
    free(r->services_assigned.p[i]);
  r->services_assigned.n = 0;
  if (vec_reserve((void **)&r->services_assigned.p,
                  &r->services_assigned.cap, n, sizeof(char *))) {
    for (int i = 0; i < n; i++)
      r->services_assigned.p[r->services_assigned.n++] =
          dup_s(names ? names[i] : NULL);
  }
  for (int i = 0; i < n; i++) {
    const char *nm = names ? names[i] : "";
    if (!svc_find_locked(r, nm)) svc_push_locked(r, nm);
  }
  r->stat_total = r->services.n;
  touch_locked(r);
  pthread_mutex_unlock(&g_lock);
}

static void recompute_stats_locked(osint_request *r) {
  long a = 0, c = 0, f = 0, s = 0;
  for (int i = 0; i < r->services.n; i++) {
    const char *st = r->services.p[i].status;
    if      (strcmp(st, "running")   == 0) a++;
    else if (strcmp(st, "completed") == 0) c++;
    else if (strcmp(st, "failed")    == 0) f++;
    else if (strcmp(st, "skipped")   == 0) s++;
  }
  r->stat_active = a; r->stat_completed = c;
  r->stat_failed = f; r->stat_skipped  = s;
}

void progress_service_status(osint_request *r, const char *name,
                             const char *status, const char *message,
                             int results_count, const char *entities) {
  if (!r || !name) return;
  pthread_mutex_lock(&g_lock);
  service_t *s = svc_find_locked(r, name);
  if (!s) s = svc_push_locked(r, name);
  if (s) {
    if (status) replace_s(&s->status, status);
    if (message != NULL) replace_s(&s->status_message, message);
    if (results_count >= 0) s->results_count = results_count;
    if (entities != NULL) replace_s(&s->entities, entities);
    recompute_stats_locked(r);
  }
  touch_locked(r);
  pthread_mutex_unlock(&g_lock);
}

/* rebuild all_entities: dedup case-insensitively on `type|lower(value)` over
 * entities ++ discovered_entities, preserving first-seen order (JS parity). */
static void rebuild_unified_locked(osint_request *r) {
  for (int i = 0; i < r->all_entities.n; i++) {
    free(r->all_entities.p[i].value); free(r->all_entities.p[i].type);
  }
  r->all_entities.n = 0;

  int total = r->entities.n + r->discovered_entities.n;
  for (int idx = 0; idx < total; idx++) {
    const char *value, *type;
    if (idx < r->entities.n) {
      value = r->entities.p[idx].value;
      type  = r->entities.p[idx].type;
    } else {
      disc_t *d = &r->discovered_entities.p[idx - r->entities.n];
      value = d->value; type = d->type;
    }
    if (!value) value = "";
    if (!type)  type  = "";

    int dup = 0;
    for (int j = 0; j < r->all_entities.n && !dup; j++) {
      const char *ev = r->all_entities.p[j].value;
      const char *et = r->all_entities.p[j].type;
      if (strcmp(et ? et : "", type) == 0 &&
          strcasecmp(ev ? ev : "", value) == 0)
        dup = 1;
    }
    if (dup) continue;
    if (!vec_reserve((void **)&r->all_entities.p, &r->all_entities.cap,
                     r->all_entities.n + 1, sizeof(entity_t)))
      break;
    entity_t *e = &r->all_entities.p[r->all_entities.n++];
    e->value = dup_s(value);
    e->type  = dup_s(type);
  }
}

void progress_add_discovered(osint_request *r, const char *value,
                             const char *type, const char *discovered_by) {
  if (!r) return;
  if (!value) value = "";
  if (!type)  type  = "";
  pthread_mutex_lock(&g_lock);
  /* dedup: exact (value,type) match — same as JS .find() */
  for (int i = 0; i < r->discovered_entities.n; i++) {
    disc_t *d = &r->discovered_entities.p[i];
    if (strcmp(d->value ? d->value : "", value) == 0 &&
        strcmp(d->type  ? d->type  : "", type)  == 0) {
      pthread_mutex_unlock(&g_lock);
      return;
    }
  }
  if (vec_reserve((void **)&r->discovered_entities.p,
                  &r->discovered_entities.cap,
                  r->discovered_entities.n + 1, sizeof(disc_t))) {
    disc_t *d = &r->discovered_entities.p[r->discovered_entities.n++];
    d->value = dup_s(value);
    d->type  = dup_s(type);
    d->discovered_by = dup_s(discovered_by ? discovered_by : "");
    rebuild_unified_locked(r);
  }
  touch_locked(r);
  pthread_mutex_unlock(&g_lock);
}

void progress_set_round(osint_request *r, int round) {
  if (!r) return;
  pthread_mutex_lock(&g_lock);
  r->current_round = round;
  touch_locked(r);
  pthread_mutex_unlock(&g_lock);
}

void progress_set_results(osint_request *r, const char *obj_json) {
  if (!r) return;
  pthread_mutex_lock(&g_lock);
  free(r->results_json);
  r->results_json = NULL;
  r->total_results = 0;
  if (obj_json) {
    cJSON *obj = cJSON_Parse(obj_json);
    if (obj) {
      /* re-serialize canonically (JS stores JSON.stringify(obj)) */
      char *ser = cJSON_PrintUnformatted(obj);
      if (ser) r->results_json = ser;
      cJSON *svcs = cJSON_GetObjectItem(obj, "services");
      if (svcs && cJSON_IsArray(svcs))
        r->total_results = cJSON_GetArraySize(svcs);
      cJSON_Delete(obj);
    }
  }
  touch_locked(r);
  pthread_mutex_unlock(&g_lock);
}

void progress_finish(osint_request *r, const char *phase) {
  if (!r) return;
  const char *ph = phase ? phase : "completed";
  int is_completed = strcmp(ph, "completed") == 0;
  /* progress_set_phase locks internally; capture current percent first */
  pthread_mutex_lock(&g_lock);
  long cur = r->progress_percent;
  pthread_mutex_unlock(&g_lock);
  progress_set_phase(r, ph, is_completed ? 100 : (int)cur);
  pthread_mutex_lock(&g_lock);
  r->done = 1;
  pthread_mutex_unlock(&g_lock);
}

int progress_is_done(osint_request *r) {
  if (!r) return 0;
  pthread_mutex_lock(&g_lock);
  int d = r->done;
  pthread_mutex_unlock(&g_lock);
  return d;
}

/* ---- toJSON() ---- */
static cJSON *ent_array(entity_t *p, int n) {
  cJSON *a = cJSON_CreateArray();
  if (!a) return NULL;
  for (int i = 0; i < n; i++) {
    cJSON *o = cJSON_CreateObject();
    if (!o) continue;
    cJSON_AddStringToObject(o, "value", p[i].value ? p[i].value : "");
    cJSON_AddStringToObject(o, "type",  p[i].type  ? p[i].type  : "");
    cJSON_AddItemToArray(a, o);
  }
  return a;
}

char *progress_to_json(osint_request *r) {
  if (!r) return NULL;
  pthread_mutex_lock(&g_lock);

  cJSON *o = cJSON_CreateObject();
  if (!o) { pthread_mutex_unlock(&g_lock); return NULL; }

  cJSON_AddStringToObject(o, "request_id", r->request_id ? r->request_id : "");
  cJSON_AddStringToObject(o, "query", r->query ? r->query : "");
  cJSON_AddStringToObject(o, "phase", r->phase ? r->phase : "");
  cJSON_AddNumberToObject(o, "progress_percent", (double)r->progress_percent);
  cJSON_AddStringToObject(o, "gpt_thinking",
                          r->gpt_thinking ? r->gpt_thinking : "");
  cJSON_AddNumberToObject(o, "total_results", (double)r->total_results);
  cJSON_AddStringToObject(o, "preliminary_findings",
                          r->preliminary_findings ? r->preliminary_findings : "");
  cJSON_AddNumberToObject(o, "created_at", (double)r->created_at);
  cJSON_AddNumberToObject(o, "updated_at", (double)r->updated_at);

  cJSON_AddItemToObject(o, "entities",
                        ent_array(r->entities.p, r->entities.n));

  cJSON *sa = cJSON_CreateArray();
  for (int i = 0; i < r->services_assigned.n; i++)
    cJSON_AddItemToArray(sa, cJSON_CreateString(
        r->services_assigned.p[i] ? r->services_assigned.p[i] : ""));
  cJSON_AddItemToObject(o, "services_assigned", sa);

  cJSON *sv = cJSON_CreateArray();
  for (int i = 0; i < r->services.n; i++) {
    service_t *s = &r->services.p[i];
    cJSON *so = cJSON_CreateObject();
    cJSON_AddStringToObject(so, "name", s->name ? s->name : "");
    cJSON_AddStringToObject(so, "status", s->status ? s->status : "");
    cJSON_AddNumberToObject(so, "results_count", (double)s->results_count);
    cJSON_AddStringToObject(so, "status_message",
                            s->status_message ? s->status_message : "");
    cJSON_AddBoolToObject(so, "is_followup", s->is_followup ? 1 : 0);
    cJSON_AddStringToObject(so, "entities", s->entities ? s->entities : "");
    cJSON_AddItemToArray(sv, so);
  }
  cJSON_AddItemToObject(o, "services", sv);

  cJSON *st = cJSON_CreateObject();
  cJSON_AddNumberToObject(st, "total_services",     (double)r->stat_total);
  cJSON_AddNumberToObject(st, "active_services",    (double)r->stat_active);
  cJSON_AddNumberToObject(st, "completed_services", (double)r->stat_completed);
  cJSON_AddNumberToObject(st, "failed_services",    (double)r->stat_failed);
  cJSON_AddNumberToObject(st, "skipped_services",   (double)r->stat_skipped);
  cJSON_AddItemToObject(o, "stats", st);

  cJSON *ph = cJSON_CreateArray();
  for (int i = 0; i < r->phase_history.n; i++) {
    phist_t *h = &r->phase_history.p[i];
    cJSON *ho = cJSON_CreateObject();
    cJSON_AddStringToObject(ho, "phase", h->phase ? h->phase : "");
    cJSON_AddNumberToObject(ho, "timestamp", (double)h->timestamp);
    cJSON_AddNumberToObject(ho, "progress", (double)h->progress);
    cJSON_AddItemToArray(ph, ho);
  }
  cJSON_AddItemToObject(o, "phase_history", ph);

  cJSON_AddNumberToObject(o, "current_round", (double)r->current_round);
  cJSON_AddNumberToObject(o, "max_rounds", (double)r->max_rounds);
  cJSON_AddBoolToObject(o, "awaiting_user_action",
                        r->awaiting_user_action ? 1 : 0);

  cJSON *de = cJSON_CreateArray();
  for (int i = 0; i < r->discovered_entities.n; i++) {
    disc_t *d = &r->discovered_entities.p[i];
    cJSON *eo = cJSON_CreateObject();
    cJSON_AddStringToObject(eo, "value", d->value ? d->value : "");
    cJSON_AddStringToObject(eo, "type",  d->type  ? d->type  : "");
    cJSON_AddStringToObject(eo, "discovered_by",
                            d->discovered_by ? d->discovered_by : "");
    cJSON_AddItemToArray(de, eo);
  }
  cJSON_AddItemToObject(o, "discovered_entities", de);

  cJSON_AddItemToObject(o, "confirmed_entities",
                        ent_array(r->confirmed_entities.p,
                                  r->confirmed_entities.n));
  cJSON_AddItemToObject(o, "all_entities",
                        ent_array(r->all_entities.p, r->all_entities.n));

  /* `results` is present ONLY when set (JS spread `...(results ? {results}:{})`) */
  if (r->results_json) {
    cJSON *res = cJSON_Parse(r->results_json);
    if (res) cJSON_AddItemToObject(o, "results", res);
  }

  pthread_mutex_unlock(&g_lock);

  char *out = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return out;
}
