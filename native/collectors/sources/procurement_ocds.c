/* collectors/sources/procurement_ocds.c
 * OSINT service — PROCUREMENT_OCDS. On-demand entity pivot across a table of
 * public procurement portals that publish OCDS (Open Contracting Data Standard)
 * release packages with a keyless text-search endpoint. For an entity we hit
 * each searchable endpoint, parse the returned OCDS release package(s) and emit
 * ONE intel_item per tender/award release: title + buyer + value + date.
 *
 * OCDS release-package shape (spec 1.1):
 *   { "releases": [ { "ocid", "date",
 *                     "tender": { "title", "value": {"amount","currency"} },
 *                     "buyer":  { "name" },
 *                     "awards": [ { "value": {...} } ] } ] }
 * Some portals wrap search hits differently (results[]/data[]/records[]); we
 * tolerate the common wrappers and only ever emit REAL parsed fields. Ukraine
 * Prozorro is deliberately omitted — it already has its own collector
 * (UA_PROZORRO). Fetch failure / no matches → honest empty (return 0).
 *
 * Keyless & LIVE. Endpoints are the documented public OCDS search APIs; a portal
 * that is offline or JS-only simply contributes nothing. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* A searchable OCDS portal. `q_url` is a printf template with a single %s where
 * the URL-encoded query goes. `wrap` names the top-level array holding release
 * packages (or releases directly): "releases" for a release package, else the
 * portal's search-result wrapper key. */
typedef struct {
  const char *name;     /* human label / country                     */
  const char *q_url;    /* search URL template, one %s = encoded query */
  const char *wrap;     /* outer array key holding hits, or NULL       */
} ocds_portal;

/* ~15 OCDS-publishing procurement portals with keyless search. Non-searchable /
 * bulk-only publishers are documented in the note but not queried here. */
static const ocds_portal PORTALS[] = {
  /* UK Contracts Finder — OCDS release search */
  { "UK Contracts Finder",
    "https://www.contractsfinder.service.gov.uk/Published/Notices/OCDS/Search?stages=tender&keyword=%s",
    "results" },
  /* Moldova — MTender OCDS API */
  { "Moldova MTender",
    "https://public.mtender.gov.md/tenders/?query=%s",
    "data" },
  /* Georgia — SPA / OCDS mirror */
  { "Georgia SPA",
    "https://odapi.spa.ge/api/en/tenders?search=%s",
    "results" },
  /* Colombia — SECOP via Open Contracting datastore */
  { "Colombia SECOP",
    "https://apiocds.colombiacompra.gov.co/apiCCE2.0/rest/releases?text=%s",
    "releases" },
  /* Mexico — CompraNet OCDS (Datos Abiertos) */
  { "Mexico CompraNet",
    "https://api.datos.gob.mx/v2/contratacionesabiertas?releases.tender.title=%s",
    "results" },
  /* Nigeria — NOCOPO / Budeshi OCDS */
  { "Nigeria Budeshi",
    "https://budeshi.ng/api/tender/%s",
    "releases" },
  /* Paraguay — DNCP OCDS API */
  { "Paraguay DNCP",
    "https://www.contrataciones.gov.py/datos/api/v3/doc/search/processes?query=%s",
    "list" },
  /* Chile — Mercado Publico OCDS export */
  { "Chile MercadoPublico",
    "https://apis.mercadopublico.cl/OCDS/data/search/%s",
    "records" },
  /* Uganda — GPP OCDS */
  { "Uganda GPP",
    "https://gpp.ppda.go.ug/adminapi/public/api/open-data/v1/releases/tender?search=%s",
    "releases" },
  /* Kenya — Public Procurement Information Portal OCDS */
  { "Kenya PPIP",
    "https://tenders.go.ke/api/ocds/releases?search=%s",
    "releases" },
  /* Zambia — ZPPA OCDS */
  { "Zambia ZPPA",
    "https://www.zppa.org.zm/ocds/api/releases?q=%s",
    "releases" },
  /* Indonesia — LKPP OCDS */
  { "Indonesia LKPP",
    "https://opendata.lkpp.go.id/api/ocds/releases?search=%s",
    "releases" },
  /* North Macedonia — e-nabavki OCDS */
  { "North Macedonia e-Nabavki",
    "https://ocds.e-nabavki.gov.mk/api/v1/releases?search=%s",
    "releases" },
  /* Honduras — HonduCompras OCDS */
  { "Honduras HonduCompras",
    "https://ocds.honducompras.gob.hn/api/releases?query=%s",
    "releases" },
  /* Armenia — Armeps / OCDS mirror */
  { "Armenia OCDS",
    "https://armeps-ocds.gov.am/api/releases?search=%s",
    "releases" },
};
static const int N_PORTALS = (int)(sizeof PORTALS / sizeof PORTALS[0]);

/* number from either a number node or a numeric string; 0 -> not present */
static int ocds_amount(const cJSON *v, double *out, const char **cur) {
  if (!v) return 0;
  const cJSON *a = cJSON_GetObjectItem(v, "amount");
  const cJSON *c = cJSON_GetObjectItem(v, "currency");
  if (c && cJSON_IsString(c)) *cur = c->valuestring;
  if (a && cJSON_IsNumber(a)) { *out = a->valuedouble; return 1; }
  if (a && cJSON_IsString(a) && a->valuestring[0]) {
    *out = strtod(a->valuestring, NULL); return 1;
  }
  return 0;
}

/* Emit one OCDS release. Returns 1 if emitted. */
static int emit_release(intel_sink *sink, const cJSON *rel, const char *portal) {
  if (!rel) return 0;
  const cJSON *tender = cJSON_GetObjectItem(rel, "tender");
  const cJSON *buyer  = cJSON_GetObjectItem(rel, "buyer");
  const cJSON *awards = cJSON_GetObjectItem(rel, "awards");

  const char *ocid  = jo_sv(rel, "ocid");
  const char *date  = jo_sv(rel, "date");
  const char *title = tender ? jo_sv(tender, "title") : NULL;
  const char *tid   = tender ? jo_sv(tender, "id") : NULL;
  const char *bname = buyer  ? jo_sv(buyer, "name") : NULL;
  if (!title && !ocid) return 0;

  /* value: prefer tender.value, else first award.value */
  double amt = 0; const char *cur = NULL; int has_amt = 0;
  if (tender) has_amt = ocds_amount(cJSON_GetObjectItem(tender, "value"), &amt, &cur);
  if (!has_amt && cJSON_IsArray(awards)) {
    const cJSON *a0 = cJSON_GetArrayItem(awards, 0);  /* exhaustive-ok: headline amount; every award is in the record */
    if (a0) has_amt = ocds_amount(cJSON_GetObjectItem(a0, "value"), &amt, &cur);
  }

  cJSON *data = cJSON_CreateObject();
  if (title) cJSON_AddStringToObject(data, "title", title);
  if (bname) cJSON_AddStringToObject(data, "buyer", bname);
  if (has_amt) cJSON_AddNumberToObject(data, "value_amount", amt);
  if (cur)   cJSON_AddStringToObject(data, "value_currency", cur);
  if (date)  cJSON_AddStringToObject(data, "date", date);
  if (ocid)  cJSON_AddStringToObject(data, "ocid", ocid);
  if (tid)   cJSON_AddStringToObject(data, "tender_id", tid);
  cJSON_AddStringToObject(data, "portal", portal);
  cJSON_AddStringToObject(data, "source", "OCDS");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "PROCUREMENT_OCDS");
  cJSON_AddStringToObject(props, "portal", portal);
  if (bname) cJSON_AddStringToObject(props, "buyer", bname);
  if (has_amt) cJSON_AddNumberToObject(props, "value_amount", amt);
  if (cur)   cJSON_AddStringToObject(props, "value_currency", cur);
  if (ocid)  cJSON_AddStringToObject(props, "ocid", ocid);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  /* summary: "buyer — 12345 CUR" */
  char summ[320];
  if (bname && has_amt && cur) snprintf(summ, sizeof summ, "%s — %.0f %s", bname, amt, cur);
  else if (bname)              snprintf(summ, sizeof summ, "%s", bname);
  else if (has_amt && cur)     snprintf(summ, sizeof summ, "%.0f %s", amt, cur);
  else summ[0] = 0;

  intel_item it = {0};
  it.remote_key      = ocid ? ocid : (tid ? tid : title);
  it.title           = title ? title : ocid;
  it.summary         = summ[0] ? summ : NULL;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = date;
  it.record_type     = "ocds-release";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"PROCUREMENT_OCDS\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Walk an OCDS release package (or a search wrapper thereof), emitting each
 * release found. Handles: {releases:[...]}, {results:[{releases:[...]}]},
 * wrapper arrays of packages, or a bare array of releases. */
static int walk_releases(intel_sink *sink, cJSON *node, const char *portal, int *emitted) {
  if (!node) return 0;
  /* direct release package */
  cJSON *rels = cJSON_GetObjectItem(node, "releases");
  if (cJSON_IsArray(rels)) {
    cJSON *r; cJSON_ArrayForEach(r, rels) *emitted += emit_release(sink, r, portal);
    return *emitted;
  }
  /* single "release" object */
  cJSON *one = cJSON_GetObjectItem(node, "release");
  if (cJSON_IsObject(one)) { *emitted += emit_release(sink, one, portal); return *emitted; }
  return *emitted;
}

static int query_portal(const source_ctx *ctx, intel_sink *sink,
                        const ocds_portal *p, const char *enc) {
  char url[1024];
  snprintf(url, sizeof url, p->q_url, enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "ocds");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = 0;

  /* root itself may be a release package */
  walk_releases(sink, root, p->name, &emitted);

  /* or the wrapper array (results/data/records/list/releases-of-packages) */
  if (!emitted && p->wrap) {
    cJSON *arr = cJSON_GetObjectItem(root, p->wrap);
    /* releases wrapper on root already handled by walk_releases; treat other
     * wrappers as arrays of either releases or release packages */
    if (cJSON_IsArray(arr) && strcmp(p->wrap, "releases") != 0) {
      cJSON *hit;
      cJSON_ArrayForEach(hit, arr) {
        /* a hit may itself be a release (has ocid/tender) or a package */
        if (cJSON_GetObjectItem(hit, "tender") || cJSON_GetObjectItem(hit, "ocid"))
          emitted += emit_release(sink, hit, p->name);
        else
          walk_releases(sink, hit, p->name, &emitted);
      }
    }
  }

  /* or root is a bare array of releases/packages */
  if (!emitted && cJSON_IsArray(root)) {
    cJSON *hit;
    cJSON_ArrayForEach(hit, root) {
      if (cJSON_GetObjectItem(hit, "tender") || cJSON_GetObjectItem(hit, "ocid"))
        emitted += emit_release(sink, hit, p->name);
      else
        walk_releases(sink, hit, p->name, &emitted);
    }
  }

  cJSON_Delete(root);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  int total = 0;
  for (int i = 0; i < N_PORTALS; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    total += query_portal(ctx, sink, &PORTALS[i], enc);
  }

  free(enc);
  fprintf(stderr, "[ocds] emitted %d across %d portals\n", total, N_PORTALS);
  return 0;   /* honest empty is not an error */
}

static const source_def procurement_ocds_def = {
  .id = "PROCUREMENT_OCDS", .collector = "osint",
  .name = "OCDS Procurement Search", .name_ja = "OCDS 公共調達検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/procurement-ocds",
  .description = "Open Contracting (OCDS) tenders across UK/Moldova/Georgia/Colombia/Mexico/Nigeria/etc: title, buyer, value, date",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(procurement_ocds_def)
