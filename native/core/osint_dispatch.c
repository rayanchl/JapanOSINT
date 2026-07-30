/* core/osint_dispatch.c — see header. Registry-filtered OSINT dispatcher. */
#include "osint_dispatch.h"
#include "httpclient.h"
#include "llm.h"
#include "prompts.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void osint_result_free(osint_result *r) {
  if (!r) return;
  free(r->data); free(r->error); free(r->sources_json);
  r->data = r->error = r->sources_json = NULL;
}

int osint_canon(const char *name, char *out, size_t n) {
  if (!name) { if (n) out[0] = 0; return 0; }
  while (*name == ' ' || *name == '\t' || *name == '\n' || *name == '\r') name++;
  size_t w = 0;
  for (; name[w] && w < n - 1; w++) out[w] = (char)toupper((unsigned char)name[w]);
  while (w && (out[w-1]==' '||out[w-1]=='\t'||out[w-1]=='\n'||out[w-1]=='\r')) w--;
  out[w] = 0;
  return w > 0;
}

/* Unified: ANY registered source is OSINT-dispatchable (a JapanOSINT
 * collector and an OSINTsaas service are indistinguishable here). */
static const source_def *osint_lookup(const char *canon) {
  return registry_get(canon);
}

int osint_is_implemented(const char *name) {
  char c[64];
  return osint_canon(name, c, sizeof c) && osint_lookup(c) != NULL;
}

int osint_handler_key(const char *name, char *out, size_t n) {
  return osint_canon(name, out, n);   /* unified: canonical id == key (v1) */
}

/* Append `s` to a growing heap buffer, doubling as needed. On OOM the buffer is
 * freed and *buf set NULL (caller checks). */
static void sl_append(char **buf, size_t *len, size_t *cap, const char *s) {
  if (!*buf) return;
  size_t sl = strlen(s);
  if (*len + sl + 1 > *cap) {
    while (*len + sl + 1 > *cap) *cap *= 2;
    char *p = realloc(*buf, *cap);
    if (!p) { free(*buf); *buf = NULL; return; }
    *buf = p;
  }
  memcpy(*buf + *len, s, sl + 1);
  *len += sl;
}

/* Catalogue handed to the analysis LLM. One service per line as
 *   ID — description (free|paid)
 * so the model can route on what a service actually does and whether it is
 * credential-gated, instead of guessing from the bare ID. No entity-type tag:
 * any service can be dispatched for any entity, so routing is purely semantic.
 * Only entity-pivot OSINT services are listed (collector == "osint"); scheduled
 * map-layer collectors are not entity-dispatchable and would only be noise. */
char *osint_services_list(void) {
  const source_def **all = registry_all();
  int n = registry_count();
  size_t cap = 4096, len = 0;
  char *buf = malloc(cap);
  if (!buf) return NULL;
  buf[0] = 0;
  for (int i = 0; i < n; i++) {
    const source_def *d = all[i];
    if (!d->collector || strcmp(d->collector, "osint") != 0) continue;
    const char *desc = (d->description && *d->description)
                         ? d->description : "(no description)";
    sl_append(&buf, &len, &cap, d->id);
    sl_append(&buf, &len, &cap, " \xE2\x80\x94 ");   /* " — " (em dash, UTF-8) */
    sl_append(&buf, &len, &cap, desc);
    sl_append(&buf, &len, &cap, d->free_tier ? " (free)\n" : " (paid)\n");
    if (!buf) return NULL;   /* OOM mid-build */
  }
  return buf;
}

/* cJSON array of every registered entity-pivot service id (collector=="osint"),
 * i.e. exactly the catalogue osint_services_list() advertises. */
static cJSON *osint_service_id_array(void) {
  cJSON *a = cJSON_CreateArray();
  const source_def **all = registry_all();
  int n = registry_count();
  for (int i = 0; i < n; i++)
    if (all[i]->collector && strcmp(all[i]->collector, "osint") == 0)
      cJSON_AddItemToArray(a, cJSON_CreateString(all[i]->id));
  return a;
}

/* The osint_analysis JSON schema with its `recommended_services` and per-entity
 * `services` enums replaced by the LIVE registry, so the analysis LLM can only
 * recommend services that actually exist — and EVERY registered service is
 * recommendable — with zero manual enum maintenance when the registry changes.
 * malloc'd; caller frees. NULL → caller falls back to the static schema file. */
char *osint_analysis_schema_dynamic(void) {
  const char *base = schema_load("osint_analysis");
  if (!base || !*base) return NULL;
  cJSON *s = cJSON_Parse(base);
  if (!s) return NULL;
  cJSON *props = cJSON_GetObjectItem(s, "properties");
  cJSON *ids = osint_service_id_array();

  /* properties.recommended_services.items.enum */
  cJSON *rs = props ? cJSON_GetObjectItem(props, "recommended_services") : NULL;
  cJSON *rsi = rs ? cJSON_GetObjectItem(rs, "items") : NULL;
  if (rsi) {
    cJSON_DeleteItemFromObject(rsi, "enum");
    cJSON_AddItemToObject(rsi, "enum", cJSON_Duplicate(ids, 1));
  }
  /* properties.entities.items.properties.services.items.enum */
  cJSON *ent = props ? cJSON_GetObjectItem(props, "entities") : NULL;
  cJSON *enti = ent ? cJSON_GetObjectItem(ent, "items") : NULL;
  cJSON *entp = enti ? cJSON_GetObjectItem(enti, "properties") : NULL;
  cJSON *esvc = entp ? cJSON_GetObjectItem(entp, "services") : NULL;
  cJSON *esvci = esvc ? cJSON_GetObjectItem(esvc, "items") : NULL;
  if (esvci) {
    cJSON_DeleteItemFromObject(esvci, "enum");
    cJSON_AddItemToObject(esvci, "enum", cJSON_Duplicate(ids, 1));
  }

  cJSON_Delete(ids);
  char *out = cJSON_PrintUnformatted(s);
  cJSON_Delete(s);
  return out;
}

/* dual sink: persist through the real intel_sink (live intel_items) AND
 * capture the emitted result JSON for the pipeline's Phase-2 chaining. */
typedef struct { char *name; int records; } src_acc;

typedef struct {
  intel_sink  base;          /* what the source sees */
  intel_sink *real;          /* the true intel_sink (may be NULL) */
  /* captured result JSON — EVERY emitted payload, in emit order. Keeping only
   * the last one (what this did before) silently discarded N-1 of N fetched
   * records at the dispatcher seam; see docs/SOURCE_EXHAUSTIVENESS.md. */
  cJSON      *caps;          /* JSON array, created lazily */
  int         n_emit;
  int         any_new;
  /* per-emit source attribution, deduped by name (empty name = "the service
   * itself", resolved to the canonical id at finalize). */
  src_acc    *srcs;
  int         n_srcs, cap_srcs;
} dual_sink;

/* Bump the record count for `name`, or append it. `name` may be "". */
static void acc_add(dual_sink *d, const char *name) {
  for (int i = 0; i < d->n_srcs; i++)
    if (strcmp(d->srcs[i].name, name) == 0) { d->srcs[i].records++; return; }
  if (d->n_srcs >= d->cap_srcs) {
    int nc = d->cap_srcs ? d->cap_srcs * 2 : 4;
    src_acc *p = realloc(d->srcs, (size_t)nc * sizeof *p);
    if (!p) return;
    d->srcs = p; d->cap_srcs = nc;
  }
  d->srcs[d->n_srcs].name = strdup(name ? name : "");
  d->srcs[d->n_srcs].records = 1;
  d->n_srcs++;
}

static int dual_emit(struct intel_sink *s, const intel_item *it) {
  dual_sink *d = (dual_sink *)s->ctx;
  int rc = d->real ? d->real->emit(d->real, it) : 1;
  /* capture the service's payload: body preferred, else properties. Every
   * record is appended — parsed when it is JSON so downstream keeps the
   * structure, otherwise kept verbatim as a string. */
  const char *payload = (it->body && *it->body) ? it->body
                       : (it->properties_json ? it->properties_json : NULL);
  if (payload) {
    if (!d->caps) d->caps = cJSON_CreateArray();
    if (d->caps) {
      cJSON *p = cJSON_Parse(payload);
      cJSON_AddItemToArray(d->caps, p ? p : cJSON_CreateString(payload));
    }
  }
  /* attribute this row to an underlying source: the collector's explicit
   * sub_source_id, else "" — finalize fills "" from the real HTTP host(s) the
   * collector contacted (automatic for every HTTP collector), or the service
   * name for purely local ones. */
  acc_add(d, (it->sub_source_id && *it->sub_source_id) ? it->sub_source_id : "");
  d->n_emit++;
  if (rc > 0) d->any_new = 1;
  return rc;
}

int osint_dispatch(db_handle *db, llm_client *llm, const char *service,
                   const char *entity, const char *entity_type,
                   intel_sink *persist, osint_result *out) {
  memset(out, 0, sizeof *out);
  char canon[64];
  if (!osint_canon(service, canon, sizeof canon)) {
    out->error = strdup("not_implemented");
    return 0;
  }
  snprintf(out->service, sizeof out->service, "%s", canon);

  const source_def *def = osint_lookup(canon);
  if (!def || !entity || !*entity) {
    out->error = strdup("not_implemented");   /* graceful, == JS */
    return 0;
  }

  dual_sink ds = {0};
  ds.base.ctx = &ds;
  ds.base.emit = dual_emit;
  ds.real = persist;

  http_client *http = http_client_new();
  volatile int cancel = 0;
  source_ctx ctx = {0};
  ctx.source_id   = canon;
  ctx.entity      = entity;
  ctx.entity_type = entity_type;
  ctx.db          = db;
  ctx.http        = http;
  ctx.llm         = llm;
  ctx.cancel      = &cancel;

  int rc = def->run(&ctx, &ds.base);

  out->success    = (rc >= 0 && ds.n_emit > 0) ? 1 : 0;
  out->confidence = out->success ? 70 : 0;     /* JS default */
  /* Hand over EVERY captured record, with its own count so a consumer can
   * bound its view without guessing how much it is not seeing. */
  out->records = ds.caps ? cJSON_GetArraySize(ds.caps) : 0;
  if (ds.caps) {
    cJSON *wrap = cJSON_CreateObject();
    cJSON_AddNumberToObject(wrap, "record_count", out->records);
    cJSON_AddItemToObject(wrap, "records", ds.caps);   /* wrap takes ownership */
    ds.caps = NULL;
    out->data = cJSON_PrintUnformatted(wrap);
    cJSON_Delete(wrap);
  }
  if (!out->success && !out->error)
    out->error = strdup(rc < 0 ? "service_error" : "no_data");

  /* Build the source attribution array, in precedence order:
   *  1. explicit sub_source_id labels from the emits (corpus per-source,
   *     weather/ip providers, any future tagging) — most precise;
   *  2. else the real HTTP host(s) the collector contacted (automatic for
   *     every HTTP collector, incl. ones that returned no data — those show
   *     the host + an error/empty status);
   *  3. else the service itself (purely local collectors). */
  int n_labeled = 0;
  for (int i = 0; i < ds.n_srcs; i++)
    if (ds.srcs[i].name && *ds.srcs[i].name) n_labeled++;
  int nh = http_client_host_count(http);

  cJSON *arr = cJSON_CreateArray();
  if (n_labeled > 0) {
    for (int i = 0; i < ds.n_srcs; i++) {
      if (!ds.srcs[i].name || !*ds.srcs[i].name) continue;  /* skip "" bucket */
      cJSON *o = cJSON_CreateObject();
      cJSON_AddStringToObject(o, "name", ds.srcs[i].name);
      cJSON_AddStringToObject(o, "status", "ok");
      cJSON_AddNumberToObject(o, "records", ds.srcs[i].records);
      cJSON_AddItemToArray(arr, o);
    }
  } else if (nh > 0) {
    for (int i = 0; i < nh; i++) {
      int reqs = 0, ok = 0;
      const char *h = http_client_host_at(http, i, &reqs, &ok);
      if (!h) continue;
      cJSON *o = cJSON_CreateObject();
      cJSON_AddStringToObject(o, "name", h);
      cJSON_AddStringToObject(o, "status",
                              (out->success && ok) ? "ok" : (ok ? "empty" : "error"));
      cJSON_AddNumberToObject(o, "records", out->success ? ds.n_emit : 0);
      cJSON_AddItemToArray(arr, o);
    }
  } else {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "name", canon);
    cJSON_AddStringToObject(o, "status",
                            out->success ? "ok" : (rc < 0 ? "error" : "empty"));
    cJSON_AddNumberToObject(o, "records", out->success ? ds.n_emit : 0);
    if (!out->success && out->error) cJSON_AddStringToObject(o, "detail", out->error);
    cJSON_AddItemToArray(arr, o);
  }
  out->sources_json = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  for (int i = 0; i < ds.n_srcs; i++) free(ds.srcs[i].name);
  free(ds.srcs);
  cJSON_Delete(ds.caps);        /* NULL unless the wrap above never ran */

  http_client_free(http);   /* after reading its host log */

  fprintf(stderr, "[osint] %s(%s) success=%d emit=%d sources=%d hosts=%d\n",
          canon, entity, out->success, ds.n_emit, n_labeled, nh);
  return 0;
}
