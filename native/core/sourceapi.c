/* core/sourceapi.c — see sourceapi.h.
 *
 * Three readers share one 10-second SNAPSHOT of the per-source dynamic facts
 * (intel_items aggregates, the sources row, the scheduler state, the trust
 * table), keyed by source id and bsearched, plus one process-lifetime INDEX of
 * the registry (static facts: engine, kind, fetch host). The snapshot is what
 * makes a search over 18,883 rows a few milliseconds of string work instead of
 * 18,883 queries; the index is what keeps `kind` (which asks hpengine whether a
 * row takes an entity) from being re-derived on every request. */
#include "sourceapi.h"
#include "../source.h"
#include "source_registry.h"
#include "source_trust.h"
#include "statusapi.h"
#include "collectortab.h"
#include "hostgate.h"
#include "../lib/hpengine.h"
#include "../lib/jocore.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

/* ── small helpers ──────────────────────────────────────────────────────── */

static void iso_now(char *buf, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(buf, n, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
           (unsigned)(tm.tm_year + 1900) % 10000u, (unsigned)(tm.tm_mon + 1) % 100u,
           (unsigned)tm.tm_mday % 100u, (unsigned)tm.tm_hour % 100u,
           (unsigned)tm.tm_min % 100u, (unsigned)tm.tm_sec % 100u,
           (unsigned)(tv.tv_usec / 1000) % 1000u);
}
static void iso_epoch(long long t, char *buf, size_t n) {
  time_t tt = (time_t)t; struct tm tm; gmtime_r(&tt, &tm);
  strftime(buf, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
}
/* "YYYY-MM-DD HH:MM:SS" (sqlite) or ISO "YYYY-MM-DDTHH:MM:SS[.mmm][Z]" → epoch */
static long long parse_ts(const char *s) {
  if (!s || !*s) return 0;
  struct tm tm; memset(&tm, 0, sizeof tm);
  int y, mo, d, h = 0, mi = 0, se = 0;
  if (sscanf(s, "%4d-%2d-%2d%*c%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &se) < 3) return 0;
  tm.tm_year = y - 1900; tm.tm_mon = mo - 1; tm.tm_mday = d;
  tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = se;
  return (long long)timegm(&tm);
}
static void add_str_or_null(cJSON *o, const char *k, const char *v) {
  if (v && *v) cJSON_AddStringToObject(o, k, v);
  else         cJSON_AddNullToObject(o, k);
}
static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL ? NULL
       : (const char *)sqlite3_column_text(s, i);
}
static void col_num_or_null(cJSON *o, const char *k, sqlite3_stmt *s, int i) {
  if (sqlite3_column_type(s, i) == SQLITE_NULL) cJSON_AddNullToObject(o, k);
  else cJSON_AddNumberToObject(o, k, sqlite3_column_double(s, i));
}
static const char *engine_of(const source_def *d) {
  return (d && d->engine && d->engine[0]) ? d->engine : "native";
}

/* ── the registry index (static facts, built once) ──────────────────────── */

typedef struct {
  const source_def *d;
  const char *kind;        /* pivot | scheduled | scheduled-pivot | unreachable | internal */
  char host[128];          /* fetch host (hp url template) else portal host, or "" */
} reg_ent;

static reg_ent *g_reg = NULL;
static int g_nreg = 0;
static pthread_mutex_t g_reg_mu = PTHREAD_MUTEX_INITIALIZER;

static int reg_cmp(const void *a, const void *b) {
  const reg_ent *x = a, *y = b;
  return strcmp(x->d->id, y->d->id);
}
static int reg_key_cmp(const void *k, const void *e) {
  return strcmp((const char *)k, ((const reg_ent *)e)->d->id);
}
static const char *derive_kind(const source_def *d) {
  if (d->collector && d->collector[0] == '_') return "internal";
  int hp = d->engine && !strcmp(d->engine, "hpengine");
  int uses = hp ? hp_source_uses_entity(d->id) : (d->update_interval_sec == 0);
  if (d->update_interval_sec > 0) return uses ? "scheduled-pivot" : "scheduled";
  return uses ? "pivot" : "unreachable";
}
static void host_of(char *out, size_t cap, const source_def *d) {
  out[0] = 0;
  const hp_source *h = (d->engine && !strcmp(d->engine, "hpengine")) ? hp_source_get(d->id) : NULL;
  const char *u = (h && h->url) ? h->url : d->url;
  if (!u || hostgate_url_host(u, out, cap) != 1) out[0] = 0;
  /* a host that still carries a template token is not a host */
  if (strchr(out, '{')) out[0] = 0;
}
static void reg_build(void) {
  pthread_mutex_lock(&g_reg_mu);
  if (!g_reg) {
    const source_def **a = registry_all();
    int n = registry_count();
    reg_ent *r = calloc((size_t)(n > 0 ? n : 1), sizeof *r);
    if (r) {
      for (int i = 0; i < n; i++) {
        r[i].d = a[i];
        r[i].kind = derive_kind(a[i]);
        host_of(r[i].host, sizeof r[i].host, a[i]);
      }
      qsort(r, (size_t)n, sizeof *r, reg_cmp);
      g_reg = r; g_nreg = n;
    }
  }
  pthread_mutex_unlock(&g_reg_mu);
}
static const reg_ent *reg_find(const char *id) {
  if (!g_reg || !id) return NULL;
  return bsearch(id, g_reg, (size_t)g_nreg, sizeof *g_reg, reg_key_cmp);
}

/* ── the dynamic snapshot (DB facts, 10 s) ──────────────────────────────── */

typedef struct {
  char id[256];
  long items, geocoded, notices;
  char last_fetched[32];
} agg_t;
typedef struct {
  char id[256];
  char status[16], last_check[32], last_success[32], error[200];
  long response_ms, records_count; int has_resp, has_records;
  char schedule_mode[16];
} srow_t;
typedef struct {
  char id[256];
  int failures, empties, quarantined; long effective; long long backoff_until;
} sched_t;

typedef struct {
  agg_t *agg; int nagg;
  srow_t *srow; int nsrow;
  sched_t *sched; int nsched;
  source_trust *trust; int ntrust;
  long long built_ms;
  char built_at[40];
} snapshot;

static snapshot g_snap;
static pthread_mutex_t g_snap_mu = PTHREAD_MUTEX_INITIALIZER;

static long long now_ms(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
static int id_cmp_agg(const void *a, const void *b)   { return strcmp(((const agg_t *)a)->id, ((const agg_t *)b)->id); }
static int id_cmp_srow(const void *a, const void *b)  { return strcmp(((const srow_t *)a)->id, ((const srow_t *)b)->id); }
static int id_cmp_sched(const void *a, const void *b) { return strcmp(((const sched_t *)a)->id, ((const sched_t *)b)->id); }
static int id_cmp_trust(const void *a, const void *b) { return strcmp(((const source_trust *)a)->source_id, ((const source_trust *)b)->source_id); }
static int key_agg(const void *k, const void *e)   { return strcmp((const char *)k, ((const agg_t *)e)->id); }
static int key_srow(const void *k, const void *e)  { return strcmp((const char *)k, ((const srow_t *)e)->id); }
static int key_sched(const void *k, const void *e) { return strcmp((const char *)k, ((const sched_t *)e)->id); }
static int key_trust(const void *k, const void *e) { return strcmp((const char *)k, ((const source_trust *)e)->source_id); }

static void snap_free(snapshot *s) {
  free(s->agg); free(s->srow); free(s->sched); free(s->trust);
  memset(s, 0, sizeof *s);
}

#define GROW(arr, n, cap, T) do { if ((n) == (cap)) { int nc = (cap) ? (cap) * 2 : 1024; \
  T *na = realloc((arr), (size_t)nc * sizeof *(arr)); if (!na) break; (arr) = na; (cap) = nc; } } while (0)

static void snap_load(db_handle *db, snapshot *s) {
  sqlite3_stmt *st;
  memset(s, 0, sizeof *s);
  /* intel_items aggregates: one GROUP BY, the same scan statusapi makes */
  if (sqlite3_prepare_v2(db->h,
        "SELECT source_id,COUNT(*),SUM(lat IS NOT NULL),"
        "SUM(record_type LIKE 'collector-%'),MAX(fetched_at) "
        "FROM intel_items GROUP BY source_id", -1, &st, NULL) == SQLITE_OK) {
    int cap = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
      GROW(s->agg, s->nagg, cap, agg_t);
      if (s->nagg == cap) break;
      agg_t *r = &s->agg[s->nagg++];
      memset(r, 0, sizeof *r);
      snprintf(r->id, sizeof r->id, "%s", (const char *)sqlite3_column_text(st, 0));
      r->items = (long)sqlite3_column_int64(st, 1);
      r->geocoded = (long)sqlite3_column_int64(st, 2);
      r->notices = (long)sqlite3_column_int64(st, 3);
      const char *lf = ctext(st, 4);
      if (lf) snprintf(r->last_fetched, sizeof r->last_fetched, "%s", lf);
    }
    sqlite3_finalize(st);
    qsort(s->agg, (size_t)s->nagg, sizeof *s->agg, id_cmp_agg);
  }
  if (sqlite3_prepare_v2(db->h,
        "SELECT id,status,last_check,last_success,response_time_ms,records_count,"
        "error_message,schedule_mode FROM sources", -1, &st, NULL) == SQLITE_OK) {
    int cap = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
      GROW(s->srow, s->nsrow, cap, srow_t);
      if (s->nsrow == cap) break;
      srow_t *r = &s->srow[s->nsrow++];
      memset(r, 0, sizeof *r);
      snprintf(r->id, sizeof r->id, "%s", (const char *)sqlite3_column_text(st, 0));
      const char *v;
      if ((v = ctext(st, 1))) snprintf(r->status, sizeof r->status, "%s", v);
      if ((v = ctext(st, 2))) snprintf(r->last_check, sizeof r->last_check, "%s", v);
      if ((v = ctext(st, 3))) snprintf(r->last_success, sizeof r->last_success, "%s", v);
      r->has_resp = sqlite3_column_type(st, 4) != SQLITE_NULL;
      r->response_ms = (long)sqlite3_column_int64(st, 4);
      r->has_records = sqlite3_column_type(st, 5) != SQLITE_NULL;
      r->records_count = (long)sqlite3_column_int64(st, 5);
      if ((v = ctext(st, 6))) snprintf(r->error, sizeof r->error, "%s", v);
      if ((v = ctext(st, 7))) snprintf(r->schedule_mode, sizeof r->schedule_mode, "%s", v);
    }
    sqlite3_finalize(st);
    qsort(s->srow, (size_t)s->nsrow, sizeof *s->srow, id_cmp_srow);
  }
  if (sqlite3_prepare_v2(db->h,
        "SELECT source_id,consecutive_failures,consecutive_empties,quarantined,"
        "effective_interval,backoff_until FROM source_sched_state", -1, &st, NULL) == SQLITE_OK) {
    int cap = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
      GROW(s->sched, s->nsched, cap, sched_t);
      if (s->nsched == cap) break;
      sched_t *r = &s->sched[s->nsched++];
      memset(r, 0, sizeof *r);
      snprintf(r->id, sizeof r->id, "%s", (const char *)sqlite3_column_text(st, 0));
      r->failures = sqlite3_column_int(st, 1);
      r->empties = sqlite3_column_int(st, 2);
      r->quarantined = sqlite3_column_int(st, 3);
      r->effective = (long)sqlite3_column_int64(st, 4);
      r->backoff_until = sqlite3_column_type(st, 5) == SQLITE_NULL ? 0 : sqlite3_column_int64(st, 5);
    }
    sqlite3_finalize(st);
    qsort(s->sched, (size_t)s->nsched, sizeof *s->sched, id_cmp_sched);
  }
  s->trust = source_trust_load(db, &s->ntrust);
  if (s->trust) qsort(s->trust, (size_t)s->ntrust, sizeof *s->trust, id_cmp_trust);
  s->built_ms = now_ms();
  iso_now(s->built_at, sizeof s->built_at);
}

/* Hands back a private COPY-BY-REFERENCE of the current snapshot under the
 * lock, refreshing it when older than JO_SOURCEAPI_SNAP_SEC (default 10).
 * Callers hold the lock for the duration of their read: the arrays are
 * replaced wholesale on refresh, so a reader must not outlive the lock. */
static int snap_ttl_ms(void) {
  const char *e = getenv("JO_SOURCEAPI_SNAP_SEC");
  int v = (e && *e) ? atoi(e) : 10;
  return (v < 0 ? 0 : v) * 1000;
}
static snapshot *snap_lock(db_handle *db) {
  pthread_mutex_lock(&g_snap_mu);
  if (!g_snap.built_ms || now_ms() - g_snap.built_ms > snap_ttl_ms()) {
    snapshot fresh;
    snap_load(db, &fresh);
    snap_free(&g_snap);
    g_snap = fresh;
  }
  return &g_snap;
}
static void snap_unlock(void) { pthread_mutex_unlock(&g_snap_mu); }

static const agg_t *snap_agg(const snapshot *s, const char *id) {
  return s->agg ? bsearch(id, s->agg, (size_t)s->nagg, sizeof *s->agg, key_agg) : NULL;
}
static const srow_t *snap_srow(const snapshot *s, const char *id) {
  return s->srow ? bsearch(id, s->srow, (size_t)s->nsrow, sizeof *s->srow, key_srow) : NULL;
}
static const sched_t *snap_sched(const snapshot *s, const char *id) {
  return s->sched ? bsearch(id, s->sched, (size_t)s->nsched, sizeof *s->sched, key_sched) : NULL;
}
static const source_trust *snap_trust(const snapshot *s, const char *id) {
  return s->trust ? bsearch(id, s->trust, (size_t)s->ntrust, sizeof *s->trust, key_trust) : NULL;
}

/* ── the compact row every list shares ──────────────────────────────────── */

static cJSON *sched_json(const sched_t *r, long declared, long long now) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddBoolToObject(o, "tracked", r != NULL);
  cJSON_AddNumberToObject(o, "declaredInterval", (double)declared);
  if (r && r->effective > 0) cJSON_AddNumberToObject(o, "effectiveInterval", (double)r->effective);
  else cJSON_AddNullToObject(o, "effectiveInterval");
  cJSON_AddNumberToObject(o, "consecutiveFailures", r ? r->failures : 0);
  cJSON_AddNumberToObject(o, "consecutiveEmpties", r ? r->empties : 0);
  int backed = r && r->backoff_until > now;
  cJSON_AddBoolToObject(o, "backedOff", backed);
  if (backed) { char ts[32]; iso_epoch(r->backoff_until, ts, sizeof ts); cJSON_AddStringToObject(o, "backoffUntil", ts); }
  else cJSON_AddNullToObject(o, "backoffUntil");
  cJSON_AddBoolToObject(o, "quarantined", r && r->quarantined);
  return o;
}

static cJSON *row_json(const reg_ent *e, const snapshot *s, long long now) {
  const source_def *d = e->d;
  const src_meta *m = src_meta_get(d->id);
  const agg_t *a = snap_agg(s, d->id);
  const srow_t *r = snap_srow(s, d->id);
  const sched_t *sc = snap_sched(s, d->id);
  const source_trust *t = snap_trust(s, d->id);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "id", d->id);
  add_str_or_null(o, "name", (m && m->name && m->name[0]) ? m->name : d->name);
  add_str_or_null(o, "name_ja", (m && m->name_ja && m->name_ja[0]) ? m->name_ja : d->name_ja);
  add_str_or_null(o, "collector", d->collector);
  {
    const collector_meta *cm = collector_meta_get(d->collector);
    add_str_or_null(o, "collector_label", cm ? cm->label : NULL);
    const char *g = collector_group_of(d->collector);
    const collector_meta *gm = collector_meta_get(g);
    cJSON_AddStringToObject(o, "group", g);
    add_str_or_null(o, "group_label", gm ? gm->label : NULL);
  }
  add_str_or_null(o, "category", (m && m->category) ? m->category : d->category);
  add_str_or_null(o, "type", (m && m->type) ? m->type : d->type);
  cJSON_AddStringToObject(o, "engine", engine_of(d));
  cJSON_AddStringToObject(o, "kind", e->kind);
  cJSON_AddNumberToObject(o, "interval", d->update_interval_sec);
  add_str_or_null(o, "url", (m && m->url) ? m->url : d->url);
  add_str_or_null(o, "host", e->host);
  add_str_or_null(o, "description", (m && m->description && m->description[0]) ? m->description : d->description);
  add_str_or_null(o, "layer", (m && m->layer && m->layer[0]) ? m->layer : d->layer);
  cJSON_AddNumberToObject(o, "item_count", a ? (double)a->items : 0);
  cJSON_AddNumberToObject(o, "geocoded", a ? (double)a->geocoded : 0);
  cJSON_AddNumberToObject(o, "notices", a ? (double)a->notices : 0);
  add_str_or_null(o, "last_fetched", a ? a->last_fetched : NULL);
  add_str_or_null(o, "status", r && r->status[0] ? r->status : NULL);
  add_str_or_null(o, "last_check", r ? r->last_check : NULL);
  add_str_or_null(o, "last_success", r ? r->last_success : NULL);
  if (r && r->has_records) cJSON_AddNumberToObject(o, "records_last_run", (double)r->records_count);
  else cJSON_AddNullToObject(o, "records_last_run");
  if (r && r->has_resp) cJSON_AddNumberToObject(o, "response_time_ms", (double)r->response_ms);
  else cJSON_AddNullToObject(o, "response_time_ms");
  add_str_or_null(o, "error_message", r ? r->error : NULL);
  cJSON *tr = cJSON_CreateObject();
  cJSON_AddBoolToObject(tr, "rated", t && t->rated);
  if (t && t->rated) {
    cJSON_AddNumberToObject(tr, "reliability", t->reliability);
    cJSON_AddStringToObject(tr, "grade", t->grade);
  } else { cJSON_AddNullToObject(tr, "reliability"); cJSON_AddNullToObject(tr, "grade"); }
  cJSON_AddItemToObject(o, "trust", tr);
  cJSON_AddItemToObject(o, "sched", sched_json(sc, d->update_interval_sec, now));
  return o;
}

/* ── search ─────────────────────────────────────────────────────────────── */

typedef struct { const char *key; long n; } facet_t;
typedef struct { facet_t *v; int n, cap; } facets;

static void facet_add(facets *f, const char *key) {
  if (!key) key = "";
  for (int i = 0; i < f->n; i++)
    if (f->v[i].key == key || !strcmp(f->v[i].key, key)) { f->v[i].n++; return; }
  if (f->n == f->cap) {
    int nc = f->cap ? f->cap * 2 : 64;
    facet_t *nv = realloc(f->v, (size_t)nc * sizeof *nv);
    if (!nv) return;
    f->v = nv; f->cap = nc;
  }
  f->v[f->n].key = key; f->v[f->n].n = 1; f->n++;
}
static int facet_cmp(const void *a, const void *b) {
  const facet_t *x = a, *y = b;
  if (x->n != y->n) return x->n > y->n ? -1 : 1;
  return strcmp(x->key, y->key);
}
static cJSON *facet_json(facets *f, int max, int with_labels) {
  qsort(f->v, (size_t)f->n, sizeof *f->v, facet_cmp);
  cJSON *o = cJSON_CreateObject();
  cJSON *arr = cJSON_CreateArray();
  int shown = f->n < max ? f->n : max;
  for (int i = 0; i < shown; i++) {
    cJSON *e = cJSON_CreateObject();
    cJSON_AddStringToObject(e, "key", f->v[i].key);
    if (with_labels) {
      const collector_meta *cm = collector_meta_get(f->v[i].key);
      add_str_or_null(e, "label", cm ? cm->label : NULL);
    }
    cJSON_AddNumberToObject(e, "count", (double)f->v[i].n);
    cJSON_AddItemToArray(arr, e);
  }
  cJSON_AddItemToObject(o, "values", arr);
  cJSON_AddNumberToObject(o, "shown", shown);
  cJSON_AddNumberToObject(o, "total", f->n);
  free(f->v); f->v = NULL; f->n = f->cap = 0;
  return o;
}

static int streq(const char *a, const char *b) { return a && b && !strcmp(a, b); }
static int nonempty(const char *s) { return s && *s; }

typedef struct { const reg_ent *e; const agg_t *a; const srow_t *r; const source_trust *t; } hit_t;
static const char *g_sort = "name"; static int g_desc = 0;   /* qsort has no context arg */
static const char *hit_name(const hit_t *h) {
  const src_meta *m = src_meta_get(h->e->d->id);
  const char *n = (m && m->name && m->name[0]) ? m->name : h->e->d->name;
  return n ? n : h->e->d->id;
}
static int hit_cmp(const void *pa, const void *pb) {
  const hit_t *a = pa, *b = pb;
  int c = 0;
  if (!strcmp(g_sort, "items")) {
    long x = a->a ? a->a->items : 0, y = b->a ? b->a->items : 0;
    c = x < y ? -1 : x > y ? 1 : 0;
  } else if (!strcmp(g_sort, "fetched")) {
    c = strcmp(a->a ? a->a->last_fetched : "", b->a ? b->a->last_fetched : "");
  } else if (!strcmp(g_sort, "grade")) {
    /* rated sources first by reliability, then unrated */
    double x = (a->t && a->t->rated) ? a->t->reliability : -1;
    double y = (b->t && b->t->rated) ? b->t->reliability : -1;
    c = x < y ? -1 : x > y ? 1 : 0;
  } else if (!strcmp(g_sort, "collector")) {
    c = strcmp(a->e->d->collector ? a->e->d->collector : "", b->e->d->collector ? b->e->d->collector : "");
  } else if (!strcmp(g_sort, "interval")) {
    c = a->e->d->update_interval_sec - b->e->d->update_interval_sec;
  } else if (!strcmp(g_sort, "id")) {
    c = strcmp(a->e->d->id, b->e->d->id);
  } else {
    c = strcasecmp(hit_name(a), hit_name(b));
  }
  if (c == 0) c = strcmp(a->e->d->id, b->e->d->id);   /* total order: paging never repeats */
  return g_desc ? -c : c;
}

static int hit_matches(const reg_ent *e, const snapshot *s, const sourceapi_query *q,
                       hit_t *out) {
  const source_def *d = e->d;
  if (nonempty(q->collector) && !streq(d->collector, q->collector)) return 0;
  if (nonempty(q->group) && strcmp(collector_group_of(d->collector), q->group)) return 0;
  if (nonempty(q->engine) && strcmp(engine_of(d), q->engine)) return 0;
  if (nonempty(q->kind) && strcmp(e->kind, q->kind)) return 0;
  const src_meta *m = src_meta_get(d->id);
  const char *cat = (m && m->category) ? m->category : d->category;
  if (nonempty(q->category) && !streq(cat, q->category)) return 0;
  const agg_t *a = snap_agg(s, d->id);
  if (q->has_items && !(a && a->items > 0)) return 0;
  const srow_t *r = snap_srow(s, d->id);
  if (nonempty(q->status)) {
    const char *st = (r && r->status[0]) ? r->status : "pending";
    if (strcmp(st, q->status)) return 0;
  }
  const source_trust *t = snap_trust(s, d->id);
  if (nonempty(q->grade)) {
    if (!strcmp(q->grade, "unrated")) { if (t && t->rated) return 0; }
    else if (!(t && t->rated && !strcmp(t->grade, q->grade))) return 0;
  }
  if (nonempty(q->q)) {
    const char *nm = (m && m->name && m->name[0]) ? m->name : d->name;
    const char *ja = (m && m->name_ja && m->name_ja[0]) ? m->name_ja : d->name_ja;
    const char *de = (m && m->description && m->description[0]) ? m->description : d->description;
    int ok = jo_strcasestr(d->id, q->q)
          || (nm && jo_strcasestr(nm, q->q))
          || (ja && jo_strcasestr(ja, q->q))
          || (de && jo_strcasestr(de, q->q))
          || (d->collector && jo_strcasestr(d->collector, q->q))
          || (e->host[0] && jo_strcasestr(e->host, q->q));
    if (!ok) return 0;
  }
  out->e = e; out->a = a; out->r = r; out->t = t;
  return 1;
}

static cJSON *search_obj(db_handle *db, const sourceapi_query *q, const char *force_collector) {
  reg_build();
  snapshot *s = snap_lock(db);
  long long now = (long long)time(NULL);
  sourceapi_query qq = *q;
  if (force_collector) qq.collector = force_collector;
  int lim = qq.limit > 0 ? qq.limit : 50; if (lim > 500) lim = 500;
  int off = qq.offset > 0 ? qq.offset : 0;

  hit_t *hits = malloc((size_t)(g_nreg > 0 ? g_nreg : 1) * sizeof *hits);
  int nh = 0;
  facets f_coll = {0}, f_cat = {0}, f_eng = {0}, f_kind = {0}, f_status = {0}, f_grp = {0};
  if (hits) {
    for (int i = 0; i < g_nreg; i++) {
      hit_t h;
      if (!hit_matches(&g_reg[i], s, &qq, &h)) continue;
      hits[nh++] = h;
      const src_meta *m = src_meta_get(h.e->d->id);
      facet_add(&f_coll, h.e->d->collector ? h.e->d->collector : "(none)");
      facet_add(&f_grp, collector_group_of(h.e->d->collector));
      facet_add(&f_cat, (m && m->category) ? m->category : (h.e->d->category ? h.e->d->category : "(none)"));
      facet_add(&f_eng, engine_of(h.e->d));
      facet_add(&f_kind, h.e->kind);
      facet_add(&f_status, (h.r && h.r->status[0]) ? h.r->status : "pending");
    }
    g_sort = nonempty(qq.sort) ? qq.sort : "name"; g_desc = qq.desc;
    qsort(hits, (size_t)nh, sizeof *hits, hit_cmp);
  }
  cJSON *o = cJSON_CreateObject();
  cJSON *data = cJSON_CreateArray();
  int shown = 0;
  for (int i = off; hits && i < nh && shown < lim; i++, shown++)
    cJSON_AddItemToArray(data, row_json(hits[i].e, s, now));
  cJSON_AddItemToObject(o, "data", data);
  cJSON *page = cJSON_CreateObject();
  cJSON_AddNumberToObject(page, "limit", lim);
  cJSON_AddNumberToObject(page, "offset", off);
  cJSON_AddNumberToObject(page, "count", shown);
  cJSON_AddNumberToObject(page, "total", hits ? nh : 0);
  cJSON_AddBoolToObject(page, "has_more", hits && off + shown < nh);
  cJSON_AddItemToObject(o, "page", page);
  cJSON *fa = cJSON_CreateObject();
  cJSON_AddItemToObject(fa, "groups", facet_json(&f_grp, 40, 1));
  cJSON_AddItemToObject(fa, "collectors", facet_json(&f_coll, 80, 1));
  cJSON_AddItemToObject(fa, "categories", facet_json(&f_cat, 60, 0));
  cJSON_AddItemToObject(fa, "engines", facet_json(&f_eng, 16, 0));
  cJSON_AddItemToObject(fa, "kinds", facet_json(&f_kind, 8, 0));
  cJSON_AddItemToObject(fa, "statuses", facet_json(&f_status, 8, 0));
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddItemToObject(meta, "facets", fa);   /* inside meta: the {data,page,meta} envelope every list shares */
  char ts[40]; iso_now(ts, sizeof ts);
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON_AddStringToObject(meta, "snapshot_at", s->built_at);
  cJSON_AddNumberToObject(meta, "registry_total", g_nreg);
  cJSON *fl = cJSON_CreateObject();
  add_str_or_null(fl, "q", qq.q);
  add_str_or_null(fl, "collector", qq.collector);
  add_str_or_null(fl, "group", qq.group);
  add_str_or_null(fl, "category", qq.category);
  add_str_or_null(fl, "engine", qq.engine);
  add_str_or_null(fl, "kind", qq.kind);
  add_str_or_null(fl, "status", qq.status);
  add_str_or_null(fl, "grade", qq.grade);
  cJSON_AddBoolToObject(fl, "has_items", qq.has_items);
  cJSON_AddStringToObject(fl, "sort", g_sort);
  cJSON_AddBoolToObject(fl, "desc", qq.desc);
  cJSON_AddItemToObject(meta, "filters", fl);
  cJSON_AddItemToObject(o, "meta", meta);
  snap_unlock();
  free(hits);
  return o;
}

char *sourceapi_search(db_handle *db, const sourceapi_query *q) {
  sourceapi_query z = {0};
  cJSON *o = search_obj(db, q ? q : &z, NULL);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js ? js : strdup("{\"error\":\"out_of_memory\"}");
}

/* ── collectors ─────────────────────────────────────────────────────────── */

typedef struct {
  const char *key;
  long sources, scheduled, pivots, internal, unreachable;
  long items, notices;
  long online, degraded, offline, pending;
  char last_fetched[32];
  facets cats, engines;
} coll_t;

static coll_t *coll_find(coll_t *c, int n, const char *key) {
  for (int i = 0; i < n; i++) if (!strcmp(c[i].key, key)) return &c[i];
  return NULL;
}
static int coll_cmp(const void *a, const void *b) {
  const coll_t *x = a, *y = b;
  if (x->sources != y->sources) return x->sources > y->sources ? -1 : 1;
  return strcmp(x->key, y->key);
}
static cJSON *coll_json(coll_t *c) {
  const collector_meta *cm = collector_meta_get(c->key);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "key", c->key);
  add_str_or_null(o, "label", cm ? cm->label : NULL);
  add_str_or_null(o, "label_ja", cm ? cm->label_ja : NULL);
  add_str_or_null(o, "description", cm ? cm->description : NULL);
  add_str_or_null(o, "parent", cm ? cm->parent : NULL);
  {
    int is_group = collector_meta_is_group(cm) || !strcmp(c->key, COLLECTOR_GROUP_UNASSIGNED);
    cJSON_AddBoolToObject(o, "is_group", is_group);
    const char *g = is_group ? c->key : collector_group_of(c->key);
    const collector_meta *gm = collector_meta_get(g);
    cJSON_AddStringToObject(o, "group", g);
    add_str_or_null(o, "group_label", gm ? gm->label : NULL);
  }
  cJSON_AddNumberToObject(o, "sources", (double)c->sources);
  cJSON_AddNumberToObject(o, "scheduled", (double)c->scheduled);
  cJSON_AddNumberToObject(o, "pivots", (double)c->pivots);
  cJSON_AddNumberToObject(o, "internal", (double)c->internal);
  cJSON_AddNumberToObject(o, "unreachable", (double)c->unreachable);
  cJSON_AddNumberToObject(o, "items", (double)c->items);
  cJSON_AddNumberToObject(o, "notices", (double)c->notices);
  add_str_or_null(o, "last_fetched", c->last_fetched);
  cJSON *st = cJSON_CreateObject();
  cJSON_AddNumberToObject(st, "online", (double)c->online);
  cJSON_AddNumberToObject(st, "degraded", (double)c->degraded);
  cJSON_AddNumberToObject(st, "offline", (double)c->offline);
  cJSON_AddNumberToObject(st, "pending", (double)c->pending);
  cJSON_AddItemToObject(o, "status", st);
  cJSON_AddItemToObject(o, "categories", facet_json(&c->cats, 12, 0));
  cJSON_AddItemToObject(o, "engines", facet_json(&c->engines, 8, 0));
  return o;
}

/* Walks the registry once; `only` restricts to one key (collector detail). */
static coll_t *coll_build(const snapshot *s, const char *only, int *out_n) {
  int cap = 0, n = 0;
  coll_t *c = NULL;
  for (int i = 0; i < g_nreg; i++) {
    const source_def *d = g_reg[i].d;
    const char *key = d->collector ? d->collector : "(none)";
    if (only && strcmp(key, only)) continue;
    coll_t *x = coll_find(c, n, key);
    if (!x) {
      if (n == cap) {
        int nc = cap ? cap * 2 : 256;
        coll_t *ncl = realloc(c, (size_t)nc * sizeof *c);
        if (!ncl) break;
        c = ncl; cap = nc;
      }
      x = &c[n++];
      memset(x, 0, sizeof *x);
      x->key = key;
    }
    x->sources++;
    const char *k = g_reg[i].kind;
    if (!strcmp(k, "scheduled") || !strcmp(k, "scheduled-pivot")) x->scheduled++;
    else if (!strcmp(k, "pivot")) x->pivots++;
    else if (!strcmp(k, "internal")) x->internal++;
    else x->unreachable++;
    const agg_t *a = snap_agg(s, d->id);
    if (a) {
      x->items += a->items; x->notices += a->notices;
      if (strcmp(a->last_fetched, x->last_fetched) > 0)
        snprintf(x->last_fetched, sizeof x->last_fetched, "%s", a->last_fetched);
    }
    const srow_t *r = snap_srow(s, d->id);
    const char *st = (r && r->status[0]) ? r->status : "pending";
    if (!strcmp(st, "online")) x->online++;
    else if (!strcmp(st, "degraded")) x->degraded++;
    else if (!strcmp(st, "offline")) x->offline++;
    else x->pending++;
    const src_meta *m = src_meta_get(d->id);
    facet_add(&x->cats, (m && m->category) ? m->category : (d->category ? d->category : "(none)"));
    facet_add(&x->engines, engine_of(d));
  }
  *out_n = n;
  return c;
}

/* Fold the collectors of one group into a group-level coll_t. `members`
 * receives the member keys (a JSON array). The group's facets are rebuilt
 * from the members' facets, which coll_json() has NOT yet consumed. */
static void coll_fold_group(const coll_t *c, int n, const char *gkey, coll_t *g, cJSON *members) {
  memset(g, 0, sizeof *g);
  g->key = gkey;
  for (int i = 0; i < n; i++) {
    if (strcmp(collector_group_of(c[i].key), gkey)) continue;
    cJSON_AddItemToArray(members, cJSON_CreateString(c[i].key));
    g->sources += c[i].sources; g->scheduled += c[i].scheduled; g->pivots += c[i].pivots;
    g->internal += c[i].internal; g->unreachable += c[i].unreachable;
    g->items += c[i].items; g->notices += c[i].notices;
    g->online += c[i].online; g->degraded += c[i].degraded; g->offline += c[i].offline; g->pending += c[i].pending;
    if (strcmp(c[i].last_fetched, g->last_fetched) > 0)
      snprintf(g->last_fetched, sizeof g->last_fetched, "%s", c[i].last_fetched);
    for (int k = 0; k < c[i].cats.n; k++)
      for (long r = 0; r < c[i].cats.v[k].n; r++) facet_add(&g->cats, c[i].cats.v[k].key);
    for (int k = 0; k < c[i].engines.n; k++)
      for (long r = 0; r < c[i].engines.v[k].n; r++) facet_add(&g->engines, c[i].engines.v[k].key);
  }
}

/* Every group that has members (plus grp_unassigned when anything lands
 * there), as a JSON array of group objects carrying `collectors` (keys). */
static cJSON *groups_json(const coll_t *c, int n) {
  cJSON *arr = cJSON_CreateArray();
  int ng = collector_meta_count();
  for (int gi = 0; gi <= ng; gi++) {
    const char *gkey;
    if (gi < ng) {
      const collector_meta *gm = collector_meta_at(gi);
      if (!collector_meta_is_group(gm)) continue;
      gkey = gm->key;
    } else gkey = COLLECTOR_GROUP_UNASSIGNED;
    cJSON *members = cJSON_CreateArray();
    coll_t g;
    coll_fold_group(c, n, gkey, &g, members);
    if (g.sources == 0) { cJSON_Delete(members); continue; }
    cJSON *gj = coll_json(&g);           /* consumes g's facets */
    cJSON_AddItemToObject(gj, "collectors", members);
    cJSON_AddNumberToObject(gj, "collector_count", cJSON_GetArraySize(members));
    cJSON_AddItemToArray(arr, gj);
  }
  return arr;
}

char *sourceapi_collectors(db_handle *db) {
  reg_build();
  snapshot *s = snap_lock(db);
  int n = 0;
  coll_t *c = coll_build(s, NULL, &n);
  qsort(c, (size_t)n, sizeof *c, coll_cmp);
  cJSON *o = cJSON_CreateObject();
  cJSON *groups = groups_json(c, n);     /* before coll_json() eats the facets */
  cJSON *data = cJSON_CreateArray();
  long srcs = 0;
  for (int i = 0; i < n; i++) { srcs += c[i].sources; cJSON_AddItemToArray(data, coll_json(&c[i])); }
  cJSON_AddItemToObject(o, "data", data);
  cJSON_AddItemToObject(o, "groups", groups);
  cJSON *meta = cJSON_CreateObject();
  char ts[40]; iso_now(ts, sizeof ts);
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON_AddStringToObject(meta, "snapshot_at", s->built_at);
  cJSON_AddNumberToObject(meta, "total", n);
  cJSON_AddNumberToObject(meta, "sources", (double)srcs);
  cJSON_AddNumberToObject(meta, "registry_total", g_nreg);
  cJSON_AddNumberToObject(meta, "labelled", collector_meta_count());
  cJSON_AddNumberToObject(meta, "groups", cJSON_GetArraySize(groups));
  {
    long un = 0;
    for (int i = 0; i < n; i++)
      if (!strcmp(collector_group_of(c[i].key), COLLECTOR_GROUP_UNASSIGNED)) un++;
    cJSON_AddNumberToObject(meta, "unassigned_collectors", (double)un);
  }
  cJSON_AddItemToObject(o, "meta", meta);
  snap_unlock();
  free(c);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js ? js : strdup("{\"error\":\"out_of_memory\"}");
}

/* Shared id prefixes inside a collector — the first token before the first
 * '_' or '-' (JP25_, gnews-, us-openfda-…) — reported when 3+ rows share it.
 * A measurement of how the ids are spelled, offered as a sub-grouping hint;
 * nothing is renamed by it. */
static cJSON *coll_groups(const char *key) {
  /* The prefix table is a growing array and the facet list keeps POINTERS
   * into it, so counts are kept beside the keys and the facet list is built
   * only once the table has stopped moving: facet_add() against keys[] while
   * realloc could still move it was a heap-use-after-free (TSan, 2026-10-18). */
  char (*keys)[64] = NULL; long *cnt = NULL; int nk = 0, ck = 0;
  for (int i = 0; i < g_nreg; i++) {
    const source_def *d = g_reg[i].d;
    if (!d->collector || strcmp(d->collector, key)) continue;
    const char *id = d->id;
    size_t n = strcspn(id, "_-");
    if (n == 0 || n >= 63 || !id[n]) continue;
    char tmp[64]; memcpy(tmp, id, n + 1); tmp[n + 1] = 0;   /* keep the separator */
    int found = -1;
    for (int k = 0; k < nk; k++) if (!strcmp(keys[k], tmp)) { found = k; break; }
    if (found < 0) {
      if (nk == ck) {
        int nc = ck ? ck * 2 : 64;
        void *nkeys = realloc(keys, (size_t)nc * sizeof *keys);
        if (!nkeys) break;
        keys = nkeys;
        long *ncnt = realloc(cnt, (size_t)nc * sizeof *cnt);
        if (!ncnt) break;
        cnt = ncnt; ck = nc;
      }
      snprintf(keys[nk], 64, "%s", tmp); cnt[nk] = 0; found = nk++;
    }
    cnt[found]++;
  }
  /* keep groups of 3+ only; the rest are singletons */
  facets f = {0};
  for (int k = 0; k < nk; k++) {
    if (cnt[k] < 3) continue;
    facet_add(&f, keys[k]);
    f.v[f.n - 1].n = cnt[k];
  }
  cJSON *j = facet_json(&f, 40, 0);
  free(keys); free(cnt);            /* facet_json has copied nothing; keys only */
  return j;
}

char *sourceapi_collector(db_handle *db, const char *key, const sourceapi_query *q) {
  if (!key || !*key) return NULL;
  reg_build();
  snapshot *s = snap_lock(db);
  const collector_meta *km = collector_meta_get(key);
  int is_group = collector_meta_is_group(km) || !strcmp(key, COLLECTOR_GROUP_UNASSIGNED);
  cJSON *o = cJSON_CreateObject();
  cJSON *members = cJSON_CreateArray();
  if (is_group) {
    /* a GROUP: its header is the fold of its member collectors, and its
     * sources page is every source whose collector is a member */
    int n = 0;
    coll_t *all = coll_build(s, NULL, &n);
    coll_t g;
    coll_fold_group(all, n, key, &g, members);
    for (int i = 0; i < n; i++) { free(all[i].cats.v); free(all[i].engines.v); }
    free(all);
    if (g.sources == 0) { cJSON_Delete(members); cJSON_Delete(o); snap_unlock(); return NULL; }
    cJSON_AddItemToObject(o, "collector", coll_json(&g));
  } else {
    int n = 0;
    coll_t *c = coll_build(s, key, &n);
    if (n == 0) { snap_unlock(); free(c); cJSON_Delete(members); cJSON_Delete(o); return NULL; }
    cJSON_AddItemToObject(o, "collector", coll_json(&c[0]));
    free(c);
  }
  /* member collectors (with their own tallies) for a group; children by
   * collectors.def parent for a collector (none today, kept for the data) */
  cJSON *children = cJSON_CreateArray();
  if (is_group) {
    int n = 0;
    coll_t *all = coll_build(s, NULL, &n);
    qsort(all, (size_t)n, sizeof *all, coll_cmp);
    for (int i = 0; i < n; i++)
      if (!strcmp(collector_group_of(all[i].key), key)) cJSON_AddItemToArray(children, coll_json(&all[i]));
      else { free(all[i].cats.v); free(all[i].engines.v); }
    free(all);
  }
  cJSON_AddItemToObject(o, "children", children);
  cJSON_AddItemToObject(o, "member_keys", members);
  cJSON_AddBoolToObject(o, "is_group", is_group);
  /* the sources page reads the same snapshot; search_obj takes the lock
   * itself — so release first. */
  snap_unlock();
  cJSON_AddItemToObject(o, "groups", is_group ? cJSON_CreateNull() : coll_groups(key));
  sourceapi_query z = {0};
  sourceapi_query qq = q ? *q : z;
  if (is_group) { qq.group = key; qq.collector = NULL; }
  cJSON_AddItemToObject(o, "sources", search_obj(db, &qq, is_group ? NULL : key));
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* ── provenance: the docs/ rows that mention an id ──────────────────────── */

typedef struct prov_row { char *file; long line_no; char *text; struct prov_row *next; } prov_row;
typedef struct { char *id; prov_row *rows; } prov_ent;
static prov_ent *g_prov = NULL; static size_t g_prov_cap = 0, g_prov_n = 0;
static int g_prov_built = 0, g_prov_files = 0; static long g_prov_rows = 0;
static pthread_mutex_t g_prov_mu = PTHREAD_MUTEX_INITIALIZER;

static unsigned long long fnv64(const char *s) {
  unsigned long long h = 1469598103934665603ULL;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) h = (h ^ *p) * 1099511628211ULL;
  return h;
}
static prov_ent *prov_slot(const char *id, int create) {
  if (!g_prov_cap) {
    if (!create) return NULL;
    g_prov_cap = 1 << 17;
    g_prov = calloc(g_prov_cap, sizeof *g_prov);
    if (!g_prov) { g_prov_cap = 0; return NULL; }
  }
  if (create && g_prov_n * 2 >= g_prov_cap) {
    size_t nc = g_prov_cap * 2;
    prov_ent *np = calloc(nc, sizeof *np);
    if (!np) return NULL;
    for (size_t i = 0; i < g_prov_cap; i++) if (g_prov[i].id) {
      size_t j = fnv64(g_prov[i].id) & (nc - 1);
      while (np[j].id) j = (j + 1) & (nc - 1);
      np[j] = g_prov[i];
    }
    free(g_prov); g_prov = np; g_prov_cap = nc;
  }
  size_t j = fnv64(id) & (g_prov_cap - 1);
  while (g_prov[j].id) {
    if (!strcmp(g_prov[j].id, id)) return &g_prov[j];
    j = (j + 1) & (g_prov_cap - 1);
  }
  if (!create) return NULL;
  g_prov[j].id = strdup(id);
  if (!g_prov[j].id) return NULL;
  g_prov_n++;
  return &g_prov[j];
}
static void prov_scan_file(const char *path, const char *rel) {
  FILE *f = fopen(path, "r");
  if (!f) return;
  g_prov_files++;
  char *line = NULL; size_t cap = 0; ssize_t n; long no = 0;
  while ((n = getline(&line, &cap, f)) >= 0) {
    no++;
    if (n == 0 || line[0] == '#' || line[0] == '\n') continue;
    size_t k = strcspn(line, "\t|\r\n");
    if (k == 0 || k > 200) continue;
    char id[201]; memcpy(id, line, k); id[k] = 0;
    /* a header row names a column, not a source */
    if (!strcmp(id, "id") || !strcmp(id, "source_id") || !strcmp(id, "ID")) continue;
    /* only ids that are registered: everything else in these files is prose */
    if (!registry_get(id)) continue;
    prov_ent *e = prov_slot(id, 1);
    if (!e) continue;
    prov_row *r = calloc(1, sizeof *r);
    if (!r) continue;
    r->file = strdup(rel); r->line_no = no;
    size_t tl = (size_t)n; if (tl && line[tl - 1] == '\n') tl--;
    if (tl > 1500) tl = 1500;
    r->text = malloc(tl + 1);
    if (r->text) { memcpy(r->text, line, tl); r->text[tl] = 0; }
    r->next = e->rows; e->rows = r;
    g_prov_rows++;
  }
  free(line);
  fclose(f);
}
static int prov_wanted(const char *name) {
  size_t n = strlen(name);
  return (n > 4 && !strcmp(name + n - 4, ".tsv")) || (n > 4 && !strcmp(name + n - 4, ".txt"));
}
static void prov_scan_dir(const char *dir, const char *rel, int depth) {
  DIR *d = opendir(dir);
  if (!d) return;
  struct dirent *de;
  while ((de = readdir(d))) {
    if (de->d_name[0] == '.') continue;
    char path[2048], r2[1024];
    snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
    snprintf(r2, sizeof r2, "%s%s%s", rel, rel[0] ? "/" : "", de->d_name);
    struct stat st;
    if (stat(path, &st) != 0) continue;
    if (S_ISDIR(st.st_mode)) { if (depth < 1) prov_scan_dir(path, r2, depth + 1); continue; }
    if (S_ISREG(st.st_mode) && prov_wanted(de->d_name)) prov_scan_file(path, r2);
  }
  closedir(d);
}
static void prov_build(void) {
  pthread_mutex_lock(&g_prov_mu);
  if (!g_prov_built) {
    const char *root = getenv("JO_DOCS_DIR");
    char dflt[1024];
    if (!root || !*root) { snprintf(dflt, sizeof dflt, "%s/docs", JO_REPO_ROOT); root = dflt; }
    prov_scan_dir(root, "docs", 0);
    g_prov_built = 1;
  }
  pthread_mutex_unlock(&g_prov_mu);
}
static cJSON *prov_json(const char *id) {
  prov_build();
  cJSON *o = cJSON_CreateObject();
  cJSON_AddNumberToObject(o, "files_scanned", g_prov_files);
  cJSON_AddNumberToObject(o, "rows_indexed", (double)g_prov_rows);
  cJSON *rows = cJSON_CreateArray();
  pthread_mutex_lock(&g_prov_mu);
  prov_ent *e = prov_slot(id, 0);
  int n = 0;
  for (prov_row *r = e ? e->rows : NULL; r; r = r->next) {
    if (n++ >= 50) break;   /* exhaustive-ok: bounded view, total stated below */
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "file", r->file);
    cJSON_AddNumberToObject(j, "line", (double)r->line_no);
    cJSON_AddStringToObject(j, "text", r->text ? r->text : "");
    cJSON_AddItemToArray(rows, j);
  }
  int total = 0;
  for (prov_row *r = e ? e->rows : NULL; r; r = r->next) total++;
  pthread_mutex_unlock(&g_prov_mu);
  cJSON_AddItemToObject(o, "rows", rows);
  cJSON_AddNumberToObject(o, "shown", n > 50 ? 50 : n);
  cJSON_AddNumberToObject(o, "total", total);
  if (g_prov_files == 0)
    cJSON_AddStringToObject(o, "note", "no docs/ directory was readable at JO_REPO_ROOT (or JO_DOCS_DIR); provenance not measured");
  return o;
}

/* ── one source in full ─────────────────────────────────────────────────── */

static cJSON *record_type_hist(db_handle *db, const char *id) {
  cJSON *o = cJSON_CreateObject();
  cJSON *arr = cJSON_CreateArray();
  sqlite3_stmt *st;
  long distinct = -1;
  if (sqlite3_prepare_v2(db->h, "SELECT COUNT(DISTINCT COALESCE(record_type,'')) FROM intel_items WHERE source_id=?1",
                         -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) distinct = (long)sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
  }
  if (sqlite3_prepare_v2(db->h,
        "SELECT COALESCE(record_type,''),COUNT(*) FROM intel_items WHERE source_id=?1 "
        "GROUP BY 1 ORDER BY 2 DESC, 1 LIMIT 20", -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
      cJSON *e = cJSON_CreateObject();
      const char *rt = (const char *)sqlite3_column_text(st, 0);
      cJSON_AddStringToObject(e, "record_type", rt && *rt ? rt : "(none)");
      cJSON_AddNumberToObject(e, "count", (double)sqlite3_column_int64(st, 1));
      cJSON_AddItemToArray(arr, e);
    }
    sqlite3_finalize(st);
  }
  cJSON_AddItemToObject(o, "values", arr);
  cJSON_AddNumberToObject(o, "shown", cJSON_GetArraySize(arr));
  if (distinct < 0) cJSON_AddNullToObject(o, "total"); else cJSON_AddNumberToObject(o, "total", (double)distinct);
  return o;
}
static cJSON *language_hist(db_handle *db, const char *id) {
  cJSON *arr = cJSON_CreateArray();
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h,
        "SELECT COALESCE(language,''),COUNT(*) FROM intel_items WHERE source_id=?1 "
        "GROUP BY 1 ORDER BY 2 DESC LIMIT 10", -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
      cJSON *e = cJSON_CreateObject();
      const char *l = (const char *)sqlite3_column_text(st, 0);
      cJSON_AddStringToObject(e, "language", l && *l ? l : "(none)");
      cJSON_AddNumberToObject(e, "count", (double)sqlite3_column_int64(st, 1));
      cJSON_AddItemToArray(arr, e);
    }
    sqlite3_finalize(st);
  }
  return arr;
}
/* Which keys the stored records carry, measured over the N most recent rows
 * (N stated). properties is already flattened by every engine, so the top
 * level is the field inventory. */
#define FIELD_SAMPLE 50
static cJSON *field_inventory(db_handle *db, const char *id) {
  cJSON *o = cJSON_CreateObject();
  facets f = {0};
  char **owned = NULL; int nowned = 0, cowned = 0;
  int sampled = 0;
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h,
        "SELECT properties FROM intel_items WHERE source_id=?1 ORDER BY fetched_at DESC LIMIT ?2",
        -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, FIELD_SAMPLE);
    while (sqlite3_step(st) == SQLITE_ROW) {
      sampled++;
      const char *p = (const char *)sqlite3_column_text(st, 0);
      cJSON *j = p ? cJSON_Parse(p) : NULL;
      if (!j) continue;
      cJSON *k;
      cJSON_ArrayForEach(k, j) {
        if (!k->string) continue;
        /* facets keep pointers: own a copy per distinct key */
        const char *key = NULL;
        for (int i = 0; i < nowned; i++) if (!strcmp(owned[i], k->string)) { key = owned[i]; break; }
        if (!key) {
          if (nowned == cowned) {
            int nc = cowned ? cowned * 2 : 128;
            char **no = realloc(owned, (size_t)nc * sizeof *no);
            if (!no) break;
            owned = no; cowned = nc;
          }
          owned[nowned] = strdup(k->string);
          if (!owned[nowned]) break;
          key = owned[nowned++];
        }
        facet_add(&f, key);
      }
      cJSON_Delete(j);
    }
    sqlite3_finalize(st);
  }
  cJSON_AddNumberToObject(o, "sample_size", sampled);
  cJSON *fj = facet_json(&f, 60, 0);
  cJSON_AddItemToObject(o, "fields", fj);
  for (int i = 0; i < nowned; i++) free(owned[i]);
  free(owned);
  return o;
}
static cJSON *notices_json(db_handle *db, const char *id) {
  cJSON *o = cJSON_CreateObject();
  cJSON *latest = cJSON_CreateArray();
  cJSON *counts = cJSON_CreateArray();
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h,
        "SELECT record_type,COUNT(*),MAX(fetched_at) FROM intel_items "
        "WHERE source_id=?1 AND record_type LIKE 'collector-%' GROUP BY 1 ORDER BY 3 DESC",
        -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
      cJSON *e = cJSON_CreateObject();
      cJSON_AddStringToObject(e, "kind", (const char *)sqlite3_column_text(st, 0));
      cJSON_AddNumberToObject(e, "count", (double)sqlite3_column_int64(st, 1));
      add_str_or_null(e, "latest_at", ctext(st, 2));
      cJSON_AddItemToArray(counts, e);
    }
    sqlite3_finalize(st);
  }
  /* the newest notice of each kind, verbatim */
  if (sqlite3_prepare_v2(db->h,
        "SELECT record_type,title,fetched_at,properties,uid FROM intel_items i "
        "WHERE source_id=?1 AND record_type LIKE 'collector-%' AND fetched_at = "
        "(SELECT MAX(fetched_at) FROM intel_items j WHERE j.source_id=i.source_id AND j.record_type=i.record_type) "
        "ORDER BY fetched_at DESC LIMIT 12", -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
      cJSON *e = cJSON_CreateObject();
      cJSON_AddStringToObject(e, "kind", (const char *)sqlite3_column_text(st, 0));
      add_str_or_null(e, "title", ctext(st, 1));
      add_str_or_null(e, "fetched_at", ctext(st, 2));
      add_str_or_null(e, "uid", ctext(st, 4));
      const char *p = ctext(st, 3);
      cJSON *pj = p ? cJSON_Parse(p) : NULL;
      if (pj) cJSON_AddItemToObject(e, "properties", pj); else cJSON_AddNullToObject(e, "properties");
      cJSON_AddItemToArray(latest, e);
    }
    sqlite3_finalize(st);
  }
  cJSON_AddItemToObject(o, "counts", counts);
  cJSON_AddItemToObject(o, "latest", latest);
  return o;
}
static cJSON *runs_json(db_handle *db, const char *id, cJSON **out_stats) {
  cJSON *o = cJSON_CreateObject();
  cJSON *arr = cJSON_CreateArray();
  sqlite3_stmt *st;
  long total = -1;
  if (sqlite3_prepare_v2(db->h, "SELECT COUNT(*) FROM fetch_log WHERE source_id=?1", -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) total = (long)sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
  }
  if (sqlite3_prepare_v2(db->h,
        "SELECT id,timestamp,status,records_fetched,stored,duration_ms,error,bytes_fetched FROM fetch_log "
        "WHERE source_id=?1 ORDER BY timestamp DESC, id DESC LIMIT 20", -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
      cJSON *e = cJSON_CreateObject();
      cJSON_AddNumberToObject(e, "id", (double)sqlite3_column_int64(st, 0));
      add_str_or_null(e, "timestamp", ctext(st, 1));
      add_str_or_null(e, "status", ctext(st, 2));
      col_num_or_null(e, "records_fetched", st, 3);
      col_num_or_null(e, "stored", st, 4);
      col_num_or_null(e, "duration_ms", st, 5);
      add_str_or_null(e, "error", ctext(st, 6));
      col_num_or_null(e, "bytes_fetched", st, 7);
      cJSON_AddItemToArray(arr, e);
    }
    sqlite3_finalize(st);
  }
  cJSON_AddItemToObject(o, "recent", arr);
  cJSON_AddNumberToObject(o, "shown", cJSON_GetArraySize(arr));
  if (total < 0) cJSON_AddNullToObject(o, "total"); else cJSON_AddNumberToObject(o, "total", (double)total);
  cJSON_AddStringToObject(o, "more", "/api/sources/{id}/logs?limit=&offset=");
  /* 30-day statistics */
  cJSON *stats = cJSON_CreateObject();
  if (sqlite3_prepare_v2(db->h,
        "SELECT COUNT(*),SUM(status='ok'),AVG(duration_ms),MAX(timestamp),MIN(timestamp),"
        "SUM(COALESCE(stored,records_fetched,0)),SUM(bytes_fetched),COUNT(bytes_fetched) FROM fetch_log "
        "WHERE source_id=?1 AND timestamp >= datetime('now','-30 days')", -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) {
      long runs = (long)sqlite3_column_int64(st, 0);
      cJSON_AddNumberToObject(stats, "runs_30d", (double)runs);
      cJSON_AddNumberToObject(stats, "ok_30d", (double)sqlite3_column_int64(st, 1));
      if (runs > 0) cJSON_AddNumberToObject(stats, "success_rate_30d", (double)sqlite3_column_int64(st, 1) / (double)runs);
      else cJSON_AddNullToObject(stats, "success_rate_30d");
      col_num_or_null(stats, "avg_duration_ms_30d", st, 2);
      add_str_or_null(stats, "last_run_at", ctext(st, 3));
      add_str_or_null(stats, "first_run_30d_at", ctext(st, 4));
      col_num_or_null(stats, "records_30d", st, 5);
      /* SUM over a column that is NULL on every row is NULL: not measured,
       * not zero. runs_with_bytes says how many of the runs carried it. */
      col_num_or_null(stats, "bytes_30d", st, 6);
      cJSON_AddNumberToObject(stats, "runs_with_bytes_30d", (double)sqlite3_column_int64(st, 7));
    }
    sqlite3_finalize(st);
  }
  *out_stats = stats;
  return o;
}

char *sourceapi_detail(db_handle *db, const char *id) {
  const source_def *d = registry_get(id);
  if (!d) return NULL;
  reg_build();
  const reg_ent *e = reg_find(id);
  const src_meta *m = src_meta_get(id);
  long long now = (long long)time(NULL);
  cJSON *o = cJSON_CreateObject();

  /* registry */
  cJSON *reg = cJSON_CreateObject();
  cJSON_AddStringToObject(reg, "id", d->id);
  add_str_or_null(reg, "name", (m && m->name && m->name[0]) ? m->name : d->name);
  add_str_or_null(reg, "name_ja", (m && m->name_ja && m->name_ja[0]) ? m->name_ja : d->name_ja);
  add_str_or_null(reg, "collector", d->collector);
  {
    const collector_meta *cm = collector_meta_get(d->collector);
    cJSON *cj = cJSON_CreateObject();
    add_str_or_null(cj, "key", d->collector);
    add_str_or_null(cj, "label", cm ? cm->label : NULL);
    add_str_or_null(cj, "label_ja", cm ? cm->label_ja : NULL);
    add_str_or_null(cj, "description", cm ? cm->description : NULL);
    add_str_or_null(cj, "parent", cm ? cm->parent : NULL);
    cJSON_AddItemToObject(reg, "collector_meta", cj);
  }
  add_str_or_null(reg, "category", (m && m->category) ? m->category : d->category);
  add_str_or_null(reg, "type", (m && m->type) ? m->type : d->type);
  cJSON_AddStringToObject(reg, "engine", engine_of(d));
  cJSON_AddStringToObject(reg, "kind", e ? e->kind : derive_kind(d));
  cJSON_AddNumberToObject(reg, "interval", d->update_interval_sec);
  add_str_or_null(reg, "url", (m && m->url) ? m->url : d->url);
  add_str_or_null(reg, "host", e ? e->host : NULL);
  add_str_or_null(reg, "description", (m && m->description && m->description[0]) ? m->description : d->description);
  add_str_or_null(reg, "license", (m && m->license && m->license[0]) ? m->license : d->license);
  add_str_or_null(reg, "layer", (m && m->layer && m->layer[0]) ? m->layer : d->layer);
  cJSON_AddBoolToObject(reg, "free_tier", m ? m->free : d->free_tier);
  cJSON_AddBoolToObject(reg, "curated_overlay", m != NULL);
  cJSON_AddBoolToObject(reg, "is_entity_pivot", registry_is_entity_pivot(d));
  cJSON_AddItemToObject(o, "registry", reg);

  /* engine — the row as declared (hpengine), or what the def alone can say */
  {
    char *hj = hp_describe_json(id);
    cJSON *ej = hj ? cJSON_Parse(hj) : NULL;
    free(hj);
    if (!ej) {
      ej = cJSON_CreateObject();
      cJSON_AddStringToObject(ej, "engine", engine_of(d));
      add_str_or_null(ej, "url", d->url);
      cJSON_AddNumberToObject(ej, "interval", d->update_interval_sec);
      cJSON_AddStringToObject(ej, "note",
        d->engine && d->engine[0]
          ? "a generated feed collector: the macro arguments (path, language, tags, interval) are compiled in; url and interval above are what the registration carries"
          : "a hand-written run(): how it fetches and parses is in its C source, not declared as data");
    }
    cJSON_AddItemToObject(o, "engine", ej);
  }

  /* dynamic facts from the shared snapshot */
  snapshot *s = snap_lock(db);
  const srow_t *r = snap_srow(s, id);
  const sched_t *sc = snap_sched(s, id);
  const source_trust *t = snap_trust(s, id);
  const agg_t *a = snap_agg(s, id);

  cJSON *schedule = sched_json(sc, d->update_interval_sec, now);
  add_str_or_null(schedule, "schedule_mode", r && r->schedule_mode[0] ? r->schedule_mode : NULL);
  cJSON_AddStringToObject(schedule, "kind", e ? e->kind : derive_kind(d));
  cJSON_AddItemToObject(o, "schedule", schedule);

  cJSON *status = cJSON_CreateObject();
  add_str_or_null(status, "status", r && r->status[0] ? r->status : NULL);
  add_str_or_null(status, "last_check", r ? r->last_check : NULL);
  add_str_or_null(status, "last_success", r ? r->last_success : NULL);
  if (r && r->has_resp) cJSON_AddNumberToObject(status, "response_time_ms", (double)r->response_ms);
  else cJSON_AddNullToObject(status, "response_time_ms");
  if (r && r->has_records) cJSON_AddNumberToObject(status, "records_last_run", (double)r->records_count);
  else cJSON_AddNullToObject(status, "records_last_run");
  add_str_or_null(status, "error_message", r ? r->error : NULL);
  cJSON *tr = cJSON_CreateObject();
  cJSON_AddBoolToObject(tr, "rated", t && t->rated);
  if (t && t->rated) {
    cJSON_AddNumberToObject(tr, "reliability", t->reliability);
    cJSON_AddStringToObject(tr, "grade", t->grade);
    cJSON_AddNumberToObject(tr, "success_rate", t->success_rate);
    cJSON_AddNumberToObject(tr, "freshness", t->freshness);
    cJSON_AddNumberToObject(tr, "anomaly_rate", t->anomaly_rate);
    cJSON_AddNumberToObject(tr, "volume_stability", t->volume_stability);
    cJSON_AddNumberToObject(tr, "runs_30d", (double)t->runs_30d);
    cJSON_AddNumberToObject(tr, "anomalies_30d", (double)t->anomalies_30d);
  } else {
    cJSON_AddNullToObject(tr, "reliability"); cJSON_AddNullToObject(tr, "grade");
  }
  cJSON_AddItemToObject(status, "trust", tr);
  cJSON_AddItemToObject(o, "status", status);

  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "item_count", a ? (double)a->items : 0);
  cJSON_AddNumberToObject(data, "geocoded", a ? (double)a->geocoded : 0);
  cJSON_AddNumberToObject(data, "notices", a ? (double)a->notices : 0);
  add_str_or_null(data, "last_fetched", a ? a->last_fetched : NULL);
  snap_unlock();

  /* per-source measurements that need their own queries */
  {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db->h,
          "SELECT SUM(lat IS NULL AND geom_source IS NULL),MIN(fetched_at),"
          "MIN(published_at),MAX(published_at),"
          "SUM(length(COALESCE(title,''))+length(COALESCE(body,''))+length(COALESCE(properties,''))),"
          "MIN(lat),MAX(lat),MIN(lon),MAX(lon),COUNT(*) FROM intel_items WHERE source_id=?1",
          -1, &st, NULL) == SQLITE_OK) {
      sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_int64(st, 9) > 0) {
        col_num_or_null(data, "awaiting_geo", st, 0);
        add_str_or_null(data, "first_fetched", ctext(st, 1));
        add_str_or_null(data, "first_published", ctext(st, 2));
        add_str_or_null(data, "last_published", ctext(st, 3));
        col_num_or_null(data, "db_text_bytes", st, 4);
        if (sqlite3_column_type(st, 5) != SQLITE_NULL) {
          cJSON *bb = cJSON_CreateObject();
          cJSON_AddNumberToObject(bb, "min_lat", sqlite3_column_double(st, 5));
          cJSON_AddNumberToObject(bb, "max_lat", sqlite3_column_double(st, 6));
          cJSON_AddNumberToObject(bb, "min_lon", sqlite3_column_double(st, 7));
          cJSON_AddNumberToObject(bb, "max_lon", sqlite3_column_double(st, 8));
          cJSON_AddItemToObject(data, "bbox", bb);
        } else cJSON_AddNullToObject(data, "bbox");
      } else {
        cJSON_AddNumberToObject(data, "awaiting_geo", 0);
        cJSON_AddNullToObject(data, "first_fetched");
        cJSON_AddNullToObject(data, "first_published");
        cJSON_AddNullToObject(data, "last_published");
        cJSON_AddNumberToObject(data, "db_text_bytes", 0);
        cJSON_AddNullToObject(data, "bbox");
      }
      sqlite3_finalize(st);
    }
  }
  cJSON_AddStringToObject(data, "db_text_bytes_note", "bytes of title+body+properties stored in the DB, not transfer size");
  cJSON_AddItemToObject(data, "record_types", record_type_hist(db, id));
  cJSON_AddItemToObject(data, "languages", language_hist(db, id));
  cJSON_AddItemToObject(data, "field_inventory", field_inventory(db, id));
  cJSON_AddStringToObject(data, "sample", "/api/intel/items?source={id}&limit=3");
  cJSON_AddItemToObject(o, "data", data);

  cJSON_AddItemToObject(o, "notices", notices_json(db, id));
  {
    cJSON *stats = NULL;
    cJSON *runs = runs_json(db, id, &stats);
    cJSON_AddItemToObject(o, "runs", runs);
    cJSON *sch = cJSON_GetObjectItem(o, "schedule");
    if (stats) cJSON_AddItemToObject(sch, "stats_30d", stats);
    /* next due = the last run + the interval the scheduler is actually
     * keeping (effective when the health policy set one, else declared).
     * Only for a scheduled row that has run at least once; pivots and
     * never-run rows say null rather than a guess. */
    const cJSON *lr = stats ? cJSON_GetObjectItem(stats, "last_run_at") : NULL;
    long long last = (lr && cJSON_IsString(lr)) ? parse_ts(lr->valuestring) : 0;
    long iv = (sc && sc->effective > 0) ? sc->effective : d->update_interval_sec;
    if (last > 0 && iv > 0) {
      char ts[32]; iso_epoch(last + iv, ts, sizeof ts);
      cJSON_AddStringToObject(sch, "next_due", ts);
      cJSON_AddBoolToObject(sch, "overdue", last + iv < now);
    } else {
      cJSON_AddNullToObject(sch, "next_due");
      cJSON_AddNullToObject(sch, "overdue");
    }
  }

  /* credentials */
  cJSON_AddItemToObject(o, "credentials", statusapi_cred_json(id));

  /* pacing */
  {
    cJSON *pc = cJSON_CreateObject();
    const char *host = (e && e->host[0]) ? e->host : NULL;
    add_str_or_null(pc, "host", host);
    if (host) cJSON_AddNumberToObject(pc, "min_gap_ms", hostgate_pace_ms(host));
    else cJSON_AddNullToObject(pc, "min_gap_ms");
    cJSON_AddItemToObject(o, "pacing", pc);
  }

  cJSON_AddItemToObject(o, "provenance", prov_json(id));

  cJSON *meta = cJSON_CreateObject();
  char ts[40]; iso_now(ts, sizeof ts);
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON_AddStringToObject(meta, "run", "/api/intel/sources/{id}/run");
  cJSON_AddStringToObject(meta, "items", "/api/intel/items?source={id}");
  cJSON_AddStringToObject(meta, "summary", "/api/sources/{id}/summary");
  cJSON_AddItemToObject(o, "meta", meta);

  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}
