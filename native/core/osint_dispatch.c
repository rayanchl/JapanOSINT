/* core/osint_dispatch.c — see header. Registry-filtered OSINT dispatcher. */
#include "osint_dispatch.h"
#include "httpclient.h"
#include "llm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void osint_result_free(osint_result *r) {
  if (!r) return;
  free(r->data); free(r->error);
  r->data = r->error = NULL;
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

char *osint_services_list(void) {
  const source_def **all = registry_all();
  int n = registry_count();
  size_t cap = 256, len = 0;
  char *buf = malloc(cap);
  if (!buf) return NULL;
  buf[0] = 0;
  for (int i = 0; i < n; i++) {            /* every source is eligible */
    size_t need = len + strlen(all[i]->id) + 3;
    if (need >= cap) { while (need >= cap) cap *= 2; buf = realloc(buf, cap); }
    if (len) { memcpy(buf + len, ", ", 2); len += 2; }
    size_t il = strlen(all[i]->id);
    memcpy(buf + len, all[i]->id, il); len += il; buf[len] = 0;
  }
  return buf;
}

/* dual sink: persist through the real intel_sink (live intel_items) AND
 * capture the emitted result JSON for the pipeline's Phase-2 chaining. */
typedef struct {
  intel_sink  base;          /* what the source sees */
  intel_sink *real;          /* the true intel_sink (may be NULL) */
  char       *cap;           /* captured result JSON (last emitted) */
  int         n_emit;
  int         any_new;
} dual_sink;

static int dual_emit(struct intel_sink *s, const intel_item *it) {
  dual_sink *d = (dual_sink *)s->ctx;
  int rc = d->real ? d->real->emit(d->real, it) : 1;
  /* capture the service's payload: body preferred, else properties */
  const char *payload = (it->body && *it->body) ? it->body
                       : (it->properties_json ? it->properties_json : NULL);
  if (payload) { free(d->cap); d->cap = strdup(payload); }
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
  http_client_free(http);

  out->success    = (rc >= 0 && ds.n_emit > 0) ? 1 : 0;
  out->confidence = out->success ? 70 : 0;     /* JS default */
  out->data       = ds.cap;                    /* hand ownership to caller */
  ds.cap = NULL;
  if (!out->success && !out->error)
    out->error = strdup(rc < 0 ? "service_error" : "no_data");
  fprintf(stderr, "[osint] %s(%s) success=%d emit=%d\n",
          canon, entity, out->success, ds.n_emit);
  return 0;
}
