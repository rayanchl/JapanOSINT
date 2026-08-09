/* collectors/sources/cyi_irr_explorer_prefix.c
 * OSINT service — IRR_EXPLORER_PREFIX. IP pivot cross-checking a prefix across
 * every IRR registry, the RPKI, and what BGP actually carries — including stale
 * or conflicting route objects in third-party registries like RADB. That
 * mismatch is the classic precursor to a hijack.
 * Endpoint: https://irrexplorer.nlnog.net/api/prefixes/prefix/<prefix> (keyless)
 * parse_notes: "irrRoutes is an object keyed by REGISTRY NAME (RADB, APNIC,
 * RIPE...) — iterate keys, do not assume a fixed set. rpslText is a multi-line
 * RPSL blob; extract descr/origin/mnt-by rather than storing it raw." Both are
 * honoured: registry keys are iterated dynamically and only the descr/origin/
 * mnt-by lines are lifted out of rpslText.
 * Emits one row per returned prefix record: rir, bgpOrigins[], rpkiRoutes,
 * per-registry IRR origins, and the upstream `messages`.
 * No coordinates -> has_geo 0 (R2).
 * Licence: NLNOG IRR Explorer public API, keyless.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int looks_like_ip_or_cidr(const char *s) {
  if (!s || !*s) return 0;
  size_t n = strlen(s);
  if (n < 3 || n > 49) return 0;
  if (strchr(s, ':')) {
    for (const char *q = s; *q; q++)
      if (!(isxdigit((unsigned char)*q) || *q == ':' || *q == '/')) return 0;
    return 1;
  }
  int parts = 0;
  const char *p = s;
  while (*p && *p != '/') {
    int v = 0, d = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; d++; if (v > 255) return 0; }
    if (!d || d > 3) return 0;
    parts++;
    if (*p == '.') { p++; if (!*p) return 0; }
    else if (*p && *p != '/') return 0;
  }
  if (parts != 4) return 0;
  if (*p == '/') {
    p++;
    int len = 0, d = 0;
    while (*p >= '0' && *p <= '9') { len = len * 10 + (*p - '0'); p++; d++; }
    if (!d || *p || len > 32) return 0;
  }
  return 1;
}

/* Lift one RPSL attribute out of a multi-line "name: value" blob. */
static const char *rpsl_attr(const char *blob, const char *name,
                             char *out, size_t n) {
  if (!blob) return NULL;
  size_t nl = strlen(name);
  const char *p = blob;
  while (p && *p) {
    if (strncmp(p, name, nl) == 0 && p[nl] == ':') {
      const char *v = p + nl + 1;
      while (*v == ' ' || *v == '\t') v++;
      size_t l = 0;
      while (v[l] && v[l] != '\n' && v[l] != '\r' && l + 1 < n) l++;
      if (!l) return NULL;
      memcpy(out, v, l);
      out[l] = '\0';
      return out;
    }
    p = strchr(p, '\n');
    if (p) p++;
  }
  return NULL;
}

static char *http_get(const source_ctx *ctx, const char *url, long *status) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 25000, 1, &hr);
  *status = hr.status;
  if (rc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  char *b = hr.body; hr.body = NULL; http_response_free(&hr);
  return b;
}

static int emit_one(intel_sink *sink, const cJSON *r, const char *query,
                    const char *url) {
  const char *prefix = jo_sv(r, "prefix");
  if (!prefix) return 0;
  const char *rir = jo_sv(r, "rir");

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "IRR_EXPLORER_PREFIX");
  cJSON_AddStringToObject(p, "query", query);
  cJSON_AddStringToObject(p, "prefix", prefix);
  if (rir) cJSON_AddStringToObject(p, "rir", rir);

  const cJSON *bgp = cJSON_GetObjectItem(r, "bgpOrigins");
  if (cJSON_IsArray(bgp) && cJSON_GetArraySize(bgp) > 0)
    cJSON_AddItemToObject(p, "bgp_origins", cJSON_Duplicate(bgp, 1));

  /* rpkiRoutes[]: asn + rpkiStatus + rpkiMaxLength */
  cJSON *rpki = cJSON_CreateArray();
  const cJSON *rr;
  cJSON_ArrayForEach(rr, cJSON_GetObjectItem(r, "rpkiRoutes")) {
    cJSON *e = cJSON_CreateObject();
    const cJSON *asn = cJSON_GetObjectItem(rr, "asn");
    if (cJSON_IsNumber(asn)) cJSON_AddNumberToObject(e, "asn", asn->valuedouble);
    else { const char *a = jo_sv(rr, "asn"); if (a) cJSON_AddStringToObject(e, "asn", a); }
    const char *st = jo_sv(rr, "rpkiStatus");
    if (st) cJSON_AddStringToObject(e, "rpki_status", st);
    const cJSON *ml = cJSON_GetObjectItem(rr, "rpkiMaxLength");
    if (cJSON_IsNumber(ml)) cJSON_AddNumberToObject(e, "rpki_max_length", ml->valuedouble);
    if (e->child) cJSON_AddItemToArray(rpki, e); else cJSON_Delete(e);
  }
  if (cJSON_GetArraySize(rpki) > 0) cJSON_AddItemToObject(p, "rpki_routes", rpki);
  else cJSON_Delete(rpki);

  /* irrRoutes is keyed by registry name — iterate the keys */
  cJSON *irr = cJSON_CreateObject();
  int nirr = 0;
  const cJSON *reg = cJSON_GetObjectItem(r, "irrRoutes");
  if (cJSON_IsObject(reg)) {
    const cJSON *bucket;
    cJSON_ArrayForEach(bucket, reg) {
      if (!bucket->string || !cJSON_IsArray(bucket)) continue;
      cJSON *rows = cJSON_CreateArray();
      const cJSON *ro;
      cJSON_ArrayForEach(ro, bucket) {
        cJSON *e = cJSON_CreateObject();
        const cJSON *asn = cJSON_GetObjectItem(ro, "asn");
        if (cJSON_IsNumber(asn)) cJSON_AddNumberToObject(e, "asn", asn->valuedouble);
        else { const char *a = jo_sv(ro, "asn"); if (a) cJSON_AddStringToObject(e, "asn", a); }
        const char *txt = jo_sv(ro, "rpslText");
        char buf[256];
        const char *v;
        if ((v = rpsl_attr(txt, "descr", buf, sizeof buf)))
          cJSON_AddStringToObject(e, "descr", v);
        if ((v = rpsl_attr(txt, "origin", buf, sizeof buf)))
          cJSON_AddStringToObject(e, "origin", v);
        if ((v = rpsl_attr(txt, "mnt-by", buf, sizeof buf)))
          cJSON_AddStringToObject(e, "mnt_by", v);
        if (e->child) { cJSON_AddItemToArray(rows, e); nirr++; }
        else cJSON_Delete(e);
      }
      if (cJSON_GetArraySize(rows) > 0) cJSON_AddItemToObject(irr, bucket->string, rows);
      else cJSON_Delete(rows);
    }
  }
  if (irr->child) cJSON_AddItemToObject(p, "irr_routes", irr);
  else cJSON_Delete(irr);

  const cJSON *msgs = cJSON_GetObjectItem(r, "messages");
  if (cJSON_IsArray(msgs) && cJSON_GetArraySize(msgs) > 0)
    cJSON_AddItemToObject(p, "messages", cJSON_Duplicate(msgs, 1));
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char summary[256];
  snprintf(summary, sizeof summary, "%s%s%d IRR route object(s)",
           rir ? rir : "", rir ? " · " : "", nirr);

  intel_item it = {0};
  it.remote_key      = prefix;
  it.title           = prefix;
  it.summary         = summary;
  it.link            = url;
  it.lang            = "en";
  it.record_type     = "irr-rpki-consistency";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"cyber\",\"bgp\",\"irr\"]";
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_ip_or_cidr(q)) return 0;         /* wrong shape -> no-op */

  /* prefix lives in a PATH segment: the CIDR '/' must stay literal, and the
   * validator above already restricted the entity to [0-9a-f.:/]. */
  char url[320];
  snprintf(url, sizeof url,
           "https://irrexplorer.nlnog.net/api/prefixes/prefix/%s", q);

  long status = 0;
  char *body = http_get(ctx, url, &status);
  if (!body) {
    fprintf(stderr, "[IRR_EXPLORER_PREFIX] http status=%ld\n", status);
    if (status >= 400 && status < 500) return 0;
    return -1;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[IRR_EXPLORER_PREFIX] unparseable body\n"); return -1; }

  int n = 0;
  if (cJSON_IsArray(root)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, root) if (cJSON_IsObject(r)) n += emit_one(sink, r, q, url);
  } else if (cJSON_IsObject(root)) {
    n += emit_one(sink, root, q, url);
  }

  cJSON_Delete(root);
  fprintf(stderr, "[IRR_EXPLORER_PREFIX] emitted %d (%s)\n", n, q);
  return 0;                          /* no IRR records is an honest empty */
}

static const source_def cyi_irr_explorer_prefix_def = {
  .id = "IRR_EXPLORER_PREFIX", .collector = "osint",
  .name = "IRR Explorer — IRR vs RPKI vs BGP consistency",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://irrexplorer.nlnog.net/api/prefixes/prefix/",
  .description = "Cross-checks a prefix across every IRR registry, the RPKI and what BGP actually carries — stale or conflicting route objects are the classic hijack precursor.",
  .license = "NLNOG IRR Explorer public API, keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_irr_explorer_prefix_def)
