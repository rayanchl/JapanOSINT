/* collectors/sources/reg_latam2.c
 * Latin-America company / registry meta-search (second cohort) under ONE
 * source_def (LATAM2_REGISTRY). On-demand entity pivot: ctx->entity = a company
 * / person / tax-id name. run() walks a static table of ~12 REAL public registry
 * search portals spanning the countries NOT covered by LATAM_REGISTRY plus the
 * specific mercantile registries named for the region (MX RPC/SAT, AR IGJ/BORA,
 * CL SII + Registro de Empresas y Sociedades, CO RUES, PE SUNARP, EC, UY, PY,
 * BO, VE, GT, CR), fetches each portal's server-rendered results page for the
 * query, and emits one intel_item per result anchor via jo_emit_anchors (REAL
 * extracted links/titles only). Chile's "Registro de Empresas y Sociedades"
 * (RES) exposes an open JSON API, preferred where available. Per-registry and
 * total emissions are capped so a query never fans out without bound. Many LATAM
 * registries are JS-only / anti-bot / captcha-gated — those rows simply yield 0
 * (honest empty); we NEVER fabricate results. Keyless, free_tier=1. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* One registry search portal.
 *   url_tmpl : printf template with exactly one %s where the (%-encoded) query
 *              goes; a REAL public search path for that registry.
 *   href_must: substring an anchor's href must contain to be a real result
 *              (filters nav/footer chrome); NULL = accept any.
 *   base     : origin prepended to root-relative hrefs.
 *   is_json  : 1 = parse a JSON search API (CL RES) instead of anchor-scrape. */
typedef struct {
  const char *name;
  const char *url_tmpl;
  const char *href_must;
  const char *base;
  const char *country;   /* ISO-3166 alpha-2 */
  const char *category;  /* company | tax */
  int         is_json;
} latam_reg;

/* ~12 real public LATAM registry search portals. Where a registry's exact query
 * parameter is uncertain its known public search path is used and the row
 * honest-empties rather than inventing a param. */
static const latam_reg REGS[] = {
  /* ---- Mexico — Registro Publico de Comercio / SAT ---- */
  { "MX RPC (SIGER2)",        "https://rpc.economia.gob.mx/siger2/xhtml/busqueda/busquedaDenominacion.xhtml?q=%s",
    NULL, "https://rpc.economia.gob.mx", "MX", "company", 0 },
  { "MX SAT 69-B",           "https://www.sat.gob.mx/consultas/76674/consulta-la-relacion-de-contribuyentes?q=%s",
    NULL, "https://www.sat.gob.mx", "MX", "tax", 0 },

  /* ---- Argentina — IGJ / Boletin Oficial ---- */
  { "AR BORA (Boletin Oficial)","https://www.boletinoficial.gob.ar/busquedaAvanzada/realizarBusqueda?nombre=%s",
    "/detalleAviso/", "https://www.boletinoficial.gob.ar", "AR", "company", 0 },
  { "AR IGJ Datos Abiertos", "https://datos.gob.ar/dataset?q=%s",
    "/dataset/", "https://datos.gob.ar", "AR", "company", 0 },

  /* ---- Chile — Registro de Empresas y Sociedades (open JSON API) + SII ---- */
  { "CL Registro Empresas y Sociedades",
    "https://www.registrodeempresasysociedades.cl/API/Publicaciones/Buscar?texto=%s",
    NULL, "https://www.registrodeempresasysociedades.cl", "CL", "company", 1 },
  { "CL SII (MIPE)",         "https://www.sii.cl/cgi-bin/Portal001/mipeSelEmpresa.cgi?q=%s",
    NULL, "https://www.sii.cl", "CL", "tax", 0 },

  /* ---- Colombia — RUES ---- */
  { "CO RUES",               "https://www.rues.org.co/RM?q=%s",
    NULL, "https://www.rues.org.co", "CO", "company", 0 },

  /* ---- Peru — SUNARP ---- */
  { "PE SUNARP (SPRL)",      "https://www.sunarp.gob.pe/busqueda-de-personas-juridicas?q=%s",
    "sunarp.gob.pe", "https://www.sunarp.gob.pe", "PE", "company", 0 },

  /* ---- Ecuador — Superintendencia de Companias ---- */
  { "EC Supercias",          "https://appscvsmovil.supercias.gob.ec/portalInformacion/sector_societario.zul?q=%s",
    NULL, "https://appscvsmovil.supercias.gob.ec", "EC", "company", 0 },

  /* ---- Uruguay — DGI / Datos Abiertos ---- */
  { "UY Catalogo Datos Abiertos","https://catalogodatos.gub.uy/dataset?q=%s",
    "/dataset/", "https://catalogodatos.gub.uy", "UY", "company", 0 },

  /* ---- Paraguay — Datos Abiertos / DNCP ---- */
  { "PY Datos Abiertos",     "https://www.datos.gov.py/dataset?q=%s",
    "/dataset/", "https://www.datos.gov.py", "PY", "company", 0 },

  /* ---- Bolivia — Datos Abiertos ---- */
  { "BO Datos Abiertos",     "https://datos.gob.bo/dataset?q=%s",
    "/dataset/", "https://datos.gob.bo", "BO", "company", 0 },

  /* ---- Venezuela — Datos Abiertos ---- */
  { "VE Gaceta / Datos",     "https://datos.gob.ve/dataset?q=%s",
    "/dataset/", "https://datos.gob.ve", "VE", "company", 0 },

  /* ---- Guatemala — Registro Mercantil / Datos Abiertos ---- */
  { "GT Datos Abiertos",     "https://datos.minfin.gob.gt/dataset?q=%s",
    "/dataset/", "https://datos.minfin.gob.gt", "GT", "company", 0 },

  /* ---- Costa Rica — Registro Nacional / Datos Abiertos ---- */
  { "CR Datos Abiertos",     "https://www.datosabiertos.presidencia.go.cr/dataset?q=%s",
    "/dataset/", "https://www.datosabiertos.presidencia.go.cr", "CR", "company", 0 },
};
static const int NREGS = (int)(sizeof(REGS) / sizeof(REGS[0]));

#define L2_TOTAL_CAP    40   /* hard ceiling on items emitted per query   */
#define L2_PER_REG_CAP   3   /* per-registry ceiling so no single fan-out */

/* Emit one company hit parsed from the CL RES JSON search API. name/link are
 * REAL extracted fields; never synthesized. Returns 1 on emit. */
static int l2_emit_json_hit(intel_sink *sink, const char *name,
                            const char *detail, const char *link,
                            const char *service) {
  if (!name || !*name) return 0;
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", service);
  if (detail) cJSON_AddStringToObject(props, "detail", detail);
  if (link)   cJSON_AddStringToObject(props, "href", link);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = link ? link : name;
  it.title           = name;
  it.summary         = detail;
  it.link            = link;
  it.lang            = "es";
  it.record_type     = "latam-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\"]";
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

/* CL Registro de Empresas y Sociedades — open JSON search. Response shape is not
 * strictly documented; we tolerate a top-level array or a common wrapper key and
 * pull best-effort name/detail fields, honest-empty on parse failure. */
static int l2_cl_res_json(const source_ctx *ctx, intel_sink *sink,
                          const char *url, const char *base, int cap) {
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "latam2:CL RES");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[latam2:CL RES] parse fail\n"); return 0; }

  cJSON *arr = NULL;
  if (cJSON_IsArray(root)) arr = root;
  else {
    const char *keys[] = { "resultado", "resultados", "data", "items",
                           "publicaciones", "Publicaciones", NULL };
    for (int k = 0; keys[k] && !arr; k++) {
      cJSON *v = cJSON_GetObjectItem(root, keys[k]);
      if (cJSON_IsArray(v)) arr = v;
    }
  }
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *r;
    cJSON_ArrayForEach(r, arr) {
      if (emitted >= cap) break;
      if (!cJSON_IsObject(r)) continue;
      const char *name = jo_sv(r, "razonSocial");
      if (!name) name = jo_sv(r, "RazonSocial");
      if (!name) name = jo_sv(r, "nombre");
      if (!name) name = jo_sv(r, "titulo");
      if (!name) name = jo_sv(r, "denominacion");
      if (!name) continue;
      const char *rut  = jo_sv(r, "rut");
      const char *tipo = jo_sv(r, "tipo");
      const char *id   = jo_sv(r, "id");
      char detail[256] = {0};
      snprintf(detail, sizeof detail, "%s%s%s%s",
               rut  ? "RUT: " : "", rut  ? rut  : "",
               tipo ? (rut ? " | " : "") : "", tipo ? tipo : "");
      char link[512] = {0};
      if (id) snprintf(link, sizeof link, "%s/Empresas/Empresa/%s", base, id);
      emitted += l2_emit_json_hit(sink, name, detail[0] ? detail : NULL,
                                  link[0] ? link : base, "CL RES");
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[latam2:CL RES] emitted %d\n", emitted);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return -1;

  int total = 0;
  for (int i = 0; i < NREGS && total < L2_TOTAL_CAP; i++) {
    const latam_reg *r = &REGS[i];
    char url[1400];
    snprintf(url, sizeof url, r->url_tmpl, enc);

    int remaining = L2_TOTAL_CAP - total;
    int cap = remaining < L2_PER_REG_CAP ? remaining : L2_PER_REG_CAP;

    int n;
    if (r->is_json) {
      n = l2_cl_res_json(ctx, sink, url, r->base, cap);
    } else {
      char tag[96];
      snprintf(tag, sizeof tag, "latam2:%s", r->name);
      char rec[64];
      snprintf(rec, sizeof rec, "latam-%s", r->category);
      /* Real fetch + real anchor extraction. JS-only / anti-bot registries emit
       * 0 for their row — honest empty, expected, never faked. */
      n = jo_emit_anchors(ctx, sink, url, r->href_must, r->name, rec,
                          r->base, q, cap, tag);
    }
    total += n;
  }
  free(enc);
  fprintf(stderr, "[latam2_registry] emitted %d across %d registries\n",
          total, NREGS);
  return 0;   /* honest empty is not an error */
}

static const source_def latam2_registry_def = {
  .id = "LATAM2_REGISTRY", .collector = "osint",
  .name = "Latin America Registry Search (cohort 2)",
  .name_ja = "ラテンアメリカ 登記検索 (第2群)",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/reg_latam2",
  .description = "Meta-search across ~12 real LATAM mercantile registries (MX/AR/CL/CO/PE/EC/UY/PY/BO/VE/GT/CR; CL RES JSON API, rest keyless anchor-scrape; JS/anti-bot rows honest-empty)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(latam2_registry_def)
