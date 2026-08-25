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

/* WHICH SOURCES ARE ACTUALLY ROUTABLE BY THE ANALYSIS LLM.
 *
 * The filter used to be `collector == "osint"` alone, and the comment above it
 * claimed that already excluded "scheduled map-layer collectors [that] are not
 * entity-dispatchable and would only be noise". It did not. 2,642 of the 4,177
 * sources tagged collector="osint" declare update_interval_sec > 0 — they are
 * SCHEDULED bulk feeds (gnews-mon-*, DELPHI_EPIDATA_*, ES_AEMPS_CIMA_*) that
 * fetch the same body whatever entity you hand them. Recommending one as the
 * answer to "who owns example.com" routes an entity at a feed that cannot
 * pivot on it.
 *
 * House rule 3 states the distinction this restores: a row is an entity pivot
 * because it takes an entity token, and a row is scheduled because it declares
 * an interval. update_interval_sec == 0 is the registry's own word for
 * "on-demand pivot", which is exactly the set this catalogue is for. */
static int is_entity_pivot(const source_def *d) {
  return d->collector && strcmp(d->collector, "osint") == 0
         && d->update_interval_sec == 0;
}

/* THE PROMPT IS A CONSUMER THAT PHYSICALLY CANNOT TAKE EVERYTHING.
 *
 * This catalogue was emitted in full — every entity-pivot service, each with
 * its whole description — straight into the phase-1 analysis prompt. Measured
 * on this registry that is a 207,353-token request, and llama-server answers
 * it with
 *
 *   request (207353 tokens) exceeds the available context size (16384 tokens)
 *
 * on EVERY search, with every model, at every realistic context size. The
 * analysis call therefore never once succeeded; llm_chat returned NULL,
 * core/pipeline.c read that as "no entities", and the whole investigation
 * collapsed to one keyword corpus lookup while reporting a clean completed
 * run. That is the actual reason the search tab's LLM "was not firing", and it
 * was invisible because nothing looked at WHY the call failed.
 *
 * docs/SOURCE_EXHAUSTIVENESS.md's carve-out applies exactly here: an LLM
 * prompt may bound its own view, but the bound must be the consumer's, it must
 * be explicit, and it must be stated in-band. So the catalogue degrades in
 * announced steps rather than being cut off mid-list:
 *
 *   1. every service WITH its description, when that fits the budget;
 *   2. else every service as a BARE ID, with a line saying the descriptions
 *      were dropped — losing prose about 1,535 services is a far smaller loss
 *      than losing 90% of the services, and every one stays recommendable;
 *   3. else as many ids as fit, with a line saying K of N.
 *
 * The caller gets `note` back so the run can report which step it took instead
 * of the model quietly routing from a partial menu.
 *
 * WHAT STEP 3 COSTS, SO NOBODY REACHES FOR THIS KNOB BLIND. Truncation takes
 * the FIRST K in registry order, and registry order is link order: the
 * batch-generated regional registries register first and the hand-written
 * entity services register LAST. Measured over 1,535 pivots, DNS_RECORDS was
 * #1201, IP_GEOLOCATION #1288, JP_CORPUS_LOOKUP #1293, SOCIAL_EMAIL #1484 and
 * DOMAIN_WHOIS #1529 — the exact indices drift with every batch, the position
 * at the tail does not. So lowering the budget past step 2 drops precisely the
 * services a person typically wants. Step 2 exists so that shrinking the
 * prompt does not have to mean shrinking the menu; prefer dropping
 * descriptions, and treat step 3 as the last resort it is. */
static int catalogue_budget_chars(void) {
  const char *e = getenv("JO_PROMPT_SERVICE_CATALOGUE_CHARS");
  int v = (e && *e) ? atoi(e) : 0;
  /* 32 KB ≈ 8k tokens. With the ~9 KB few-shot preamble around it the analysis
   * request lands near 11k tokens, inside the 16384 default context that
   * scripts/start-llama.sh launches llama-server with. */
  return v > 0 ? v : 32768;
}

char *osint_services_list_bounded(osint_catalogue_note *note) {
  const source_def **all = registry_all();
  int n = registry_count();
  int budget = catalogue_budget_chars();

  int total = 0;
  size_t full_len = 0;
  for (int i = 0; i < n; i++) {
    if (!is_entity_pivot(all[i])) continue;
    total++;
    const char *desc = (all[i]->description && *all[i]->description)
                         ? all[i]->description : "(no description)";
    full_len += strlen(all[i]->id) + strlen(desc) + 16;
  }
  int with_desc = (full_len <= (size_t)budget);

  size_t cap = 4096, len = 0;
  char *buf = malloc(cap);
  if (!buf) return NULL;
  buf[0] = 0;
  int shown = 0;
  for (int i = 0; i < n; i++) {
    const source_def *d = all[i];
    if (!is_entity_pivot(d)) continue;
    /* Stop on the budget rather than half-writing a line: a truncated service
     * id is a name that does not exist, and the model would route to it. */
    size_t need = strlen(d->id) + 2;
    const char *desc = NULL;
    if (with_desc) {
      desc = (d->description && *d->description) ? d->description
                                                 : "(no description)";
      need += strlen(desc) + 12;
    }
    if (len + need > (size_t)budget) break;
    sl_append(&buf, &len, &cap, d->id);
    if (with_desc) {
      sl_append(&buf, &len, &cap, " \xE2\x80\x94 "); /* " — " (em dash, UTF-8) */
      sl_append(&buf, &len, &cap, desc);
      sl_append(&buf, &len, &cap, d->free_tier ? " (free)\n" : " (paid)\n");
    } else {
      sl_append(&buf, &len, &cap, "\n");
    }
    if (!buf) return NULL;   /* OOM mid-build */
    shown++;
  }

  /* Say it IN the prompt. The model is told what it is not being shown, so it
   * routes knowing the menu is partial instead of assuming it saw everything —
   * the same in-band labelling results_view_for_prompt() applies to records. */
  char banner[384];
  if (shown < total)
    snprintf(banner, sizeof banner,
      "\n[CATALOGUE BOUNDED: showing the first %d of %d registered "
      "entity-pivot services in registry order%s. Services not listed here "
      "still exist and can be reached; recommend from what is listed.]\n",
      shown, total, with_desc ? "" : ", as bare ids with descriptions omitted "
                                     "so that every service stays listed");
  else if (!with_desc)
    snprintf(banner, sizeof banner,
      "\n[CATALOGUE BOUNDED: all %d registered entity-pivot services are "
      "listed, as bare ids — their descriptions did not fit the prompt "
      "budget and were omitted, not the services.]\n", total);
  else
    banner[0] = '\0';
  if (banner[0]) sl_append(&buf, &len, &cap, banner);

  if (note) {
    note->total        = total;
    note->shown        = shown;
    note->descriptions = with_desc;
    note->truncated    = (shown < total);
  }
  return buf;
}

char *osint_services_list(void) { return osint_services_list_bounded(NULL); }

/* cJSON array of the entity-pivot service ids the schema enum may contain.
 *
 * `limit` > 0 takes the FIRST `limit` of them, which is exactly the set
 * osint_services_list_bounded() printed — both walk registry_all() in order
 * with the same predicate, so "the first N" is the same N in both places.
 * That equality is the point: the enum is what the model is ALLOWED to say and
 * the catalogue is what it was TOLD about, and letting those two disagree
 * means either offering names it was never shown the meaning of, or rejecting
 * names it was explicitly offered. 0 means no limit. */
static cJSON *osint_service_id_array(int limit) {
  cJSON *a = cJSON_CreateArray();
  const source_def **all = registry_all();
  int n = registry_count(), taken = 0;
  for (int i = 0; i < n; i++) {
    if (!is_entity_pivot(all[i])) continue;
    if (limit > 0 && taken >= limit) break;
    cJSON_AddItemToArray(a, cJSON_CreateString(all[i]->id));
    taken++;
  }
  return a;
}

/* The osint_analysis JSON schema with its `recommended_services` and per-entity
 * `services` enums replaced by the LIVE registry, so the analysis LLM can only
 * recommend services that actually exist — and EVERY registered service is
 * recommendable — with zero manual enum maintenance when the registry changes.
 * malloc'd; caller frees. NULL → caller falls back to the static schema file. */
char *osint_analysis_schema_dynamic(void) {
  return osint_analysis_schema_dynamic_limited(0);
}

char *osint_analysis_schema_dynamic_limited(int limit) {
  const char *base = schema_load("osint_analysis");
  if (!base || !*base) return NULL;
  cJSON *s = cJSON_Parse(base);
  if (!s) return NULL;
  cJSON *props = cJSON_GetObjectItem(s, "properties");
  cJSON *ids = osint_service_id_array(limit);

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
      /* `records` is NULL here, and that is the honest value.
       *
       * This branch fires when the collector labelled none of its emits with a
       * sub_source_id, so all we know is the set of HOSTS it contacted — the
       * host log records requests, not which record came from where. It used
       * to write `ds.n_emit` into every host row, i.e. the service's TOTAL
       * repeated once per host: SOCIAL_EMAIL contacted 60 hosts and emitted
       * 187 records, and the attribution said 187 records for instagram.com,
       * 187 for github.com, 187 for each of the other 58 — 11,220 records
       * claimed out of 187 real ones, including for the hosts whose status was
       * "error" and which returned nothing at all. A per-host figure we do not
       * have is not something to fill in with the total; house rule 1 says a
       * missing measurement degrades to an explicit unknown.
       *
       * `requests` IS measured per host, so it is reported, and the service's
       * real total stays where it is actually true — record_count on the
       * service result. A collector that wants per-source counts already has
       * the way to get them: label its emits with sub_source_id and it lands
       * in the labelled branch above. */
      cJSON_AddItemToObject(o, "records", cJSON_CreateNull());
      cJSON_AddNumberToObject(o, "requests", reqs);
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
