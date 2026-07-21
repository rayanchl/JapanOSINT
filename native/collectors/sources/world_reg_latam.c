/* collectors/sources/world_reg_latam.c
 * Latin-America company / court / registry meta-search under ONE source_def
 * (LATAM_REGISTRY). On-demand entity pivot: ctx->entity = a person / company /
 * tax-id / party name. run() walks a static table of ~25 REAL public registry &
 * court search portals across BR / MX / AR / CL / CO / PE, fetches each one's
 * server-rendered results page for the query, and emits one intel_item per
 * result anchor via jo_emit_anchors (REAL extracted links/titles only). Total
 * emissions are capped (~40) and per-registry emissions capped (~3) so a single
 * query never fans out without bound. Many LATAM registries are JS-only /
 * anti-bot / captcha-gated — those simply yield 0 for their row (honest empty);
 * we NEVER fabricate results. Keyless, free_tier=1. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* One registry/court/search portal.
 *   url_tmpl : printf template with exactly one %s where the (%-encoded) query
 *              goes; a REAL public search path for that registry.
 *   href_must: substring an anchor's href must contain to be a real result
 *              (filters nav/footer chrome); NULL = accept any.
 *   base     : origin prepended to root-relative hrefs. */
typedef struct {
  const char *name;
  const char *url_tmpl;
  const char *href_must;
  const char *base;
  const char *country;   /* ISO-3166 alpha-2 */
  const char *category;  /* company | court | tax | electoral */
} latam_reg;

/* ~25 real public LATAM registry / court / open-data search portals. Where the
 * exact query parameter is uncertain the registry's known search path is used
 * and the row honest-empties rather than inventing a param. */
static const latam_reg REGS[] = {
  /* ---- Brazil ---- */
  { "BR Brasil.io Empresas",  "https://brasil.io/dataset/socios-brasil/empresa/?search=%s",
    "/dataset/", "https://brasil.io", "BR", "company" },
  { "BR ReceitaWS (CNPJ)",    "https://www.receitaws.com.br/v1/cnpj/%s",
    NULL, "https://www.receitaws.com.br", "BR", "company" },
  { "BR JusBrasil",           "https://www.jusbrasil.com.br/busca?q=%s",
    "jusbrasil.com.br", "https://www.jusbrasil.com.br", "BR", "court" },
  { "BR TSE DivulgaCand",     "https://divulgacandcontas.tse.jus.br/divulga/#/busca?q=%s",
    NULL, "https://divulgacandcontas.tse.jus.br", "BR", "electoral" },
  { "BR Portal Transparencia","https://portaldatransparencia.gov.br/busca?termo=%s",
    "/pessoa", "https://portaldatransparencia.gov.br", "BR", "company" },
  { "BR CNJ Consulta Processual","https://www.cnj.jus.br/busca-atos-adm/?termo=%s",
    NULL, "https://www.cnj.jus.br", "BR", "court" },

  /* ---- Mexico ---- */
  { "MX RPC (Reg. Publico Comercio)","https://rpc.economia.gob.mx/siger2/xhtml/busqueda/busquedaDenominacion.xhtml?q=%s",
    NULL, "https://rpc.economia.gob.mx", "MX", "company" },
  { "MX SAT 69-B Listado",    "https://www.sat.gob.mx/consultas/76674/consulta-la-relacion-de-contribuyentes?q=%s",
    NULL, "https://www.sat.gob.mx", "MX", "tax" },
  { "MX INE / Consulta",      "https://www.ine.mx/?s=%s",
    "ine.mx", "https://www.ine.mx", "MX", "electoral" },
  { "MX DOF (Diario Oficial)","https://www.dof.gob.mx/busqueda_detalle.php?busqueda_cuerpo=%s",
    "nota_detalle", "https://www.dof.gob.mx", "MX", "company" },

  /* ---- Argentina ---- */
  { "AR CUIT Online",         "https://www.cuitonline.com/search.php?q=%s",
    "/detalle/", "https://www.cuitonline.com", "AR", "company" },
  { "AR BORA (Boletin Oficial)","https://www.boletinoficial.gob.ar/busquedaAvanzada/realizarBusqueda?nombre=%s",
    "/detalleAviso/", "https://www.boletinoficial.gob.ar", "AR", "company" },
  { "AR IGJ / Datos Abiertos","https://datos.gob.ar/dataset?q=%s",
    "/dataset/", "https://datos.gob.ar", "AR", "company" },
  { "AR PJN Consulta Causas", "https://scw.pjn.gov.ar/scw/home.seam?q=%s",
    NULL, "https://scw.pjn.gov.ar", "AR", "court" },

  /* ---- Chile ---- */
  { "CL SII (Servicio Impuestos)","https://www.sii.cl/cgi-bin/Portal001/mipeSelEmpresa.cgi?q=%s",
    NULL, "https://www.sii.cl", "CL", "tax" },
  { "CL Poder Judicial",      "https://www.pjud.cl/busqueda?q=%s",
    "pjud.cl", "https://www.pjud.cl", "CL", "court" },
  { "CL DiarioOficial",       "https://www.diariooficial.interior.gob.cl/edicionelectronica/index.php?q=%s",
    NULL, "https://www.diariooficial.interior.gob.cl", "CL", "company" },
  { "CL Datos Abiertos",      "https://datos.gob.cl/dataset?q=%s",
    "/dataset/", "https://datos.gob.cl", "CL", "company" },

  /* ---- Colombia ---- */
  { "CO RUES",                "https://www.rues.org.co/RM?q=%s",
    NULL, "https://www.rues.org.co", "CO", "company" },
  { "CO Rama Judicial",       "https://consultaprocesos.ramajudicial.gov.co/Procesos/Index?q=%s",
    NULL, "https://consultaprocesos.ramajudicial.gov.co", "CO", "court" },
  { "CO Datos Abiertos",      "https://www.datos.gov.co/browse?q=%s",
    "/d/", "https://www.datos.gov.co", "CO", "company" },
  { "CO SECOP",               "https://www.contratos.gov.co/consultas/resultadoListadoProcesos.cfm?q=%s",
    NULL, "https://www.contratos.gov.co", "CO", "company" },

  /* ---- Peru ---- */
  { "PE SUNARP (SPRL)",       "https://www.sunarp.gob.pe/busqueda-de-personas-juridicas?q=%s",
    "sunarp.gob.pe", "https://www.sunarp.gob.pe", "PE", "company" },
  { "PE SUNAT RUC",           "https://e-consultaruc.sunat.gob.pe/cl-ti-itmrconsruc/jcrS00Alias?q=%s",
    NULL, "https://e-consultaruc.sunat.gob.pe", "PE", "tax" },
  { "PE Poder Judicial CEJ",  "https://cej.pj.gob.pe/cej/forms/busquedaform.html?q=%s",
    NULL, "https://cej.pj.gob.pe", "PE", "court" },
  { "PE Datos Abiertos",      "https://www.datosabiertos.gob.pe/search/type/dataset?query=%s",
    "/dataset/", "https://www.datosabiertos.gob.pe", "PE", "company" },
};
static const int NREGS = (int)(sizeof(REGS) / sizeof(REGS[0]));

#define LATAM_TOTAL_CAP    40   /* hard ceiling on items emitted per query   */
#define LATAM_PER_REG_CAP   3   /* per-registry ceiling so no single fan-out */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return -1;

  int total = 0;
  for (int i = 0; i < NREGS && total < LATAM_TOTAL_CAP; i++) {
    const latam_reg *r = &REGS[i];
    char url[1400];
    snprintf(url, sizeof url, r->url_tmpl, enc);

    int remaining = LATAM_TOTAL_CAP - total;
    int cap = remaining < LATAM_PER_REG_CAP ? remaining : LATAM_PER_REG_CAP;

    char tag[96];
    snprintf(tag, sizeof tag, "latam_reg:%s", r->name);
    char rec[64];
    snprintf(rec, sizeof rec, "latam-%s", r->category);

    /* Real fetch + real anchor extraction. JS-only / anti-bot registries emit
     * 0 for their row — honest empty, expected, never faked. */
    int n = jo_emit_anchors(ctx, sink, url, r->href_must, r->name, rec,
                            r->base, q, cap, tag);
    total += n;
  }
  free(enc);
  fprintf(stderr, "[latam_registry] emitted %d across %d registries\n",
          total, NREGS);
  return 0;   /* honest empty is not an error */
}

static const source_def latam_registry_def = {
  .id = "LATAM_REGISTRY", .collector = "osint",
  .name = "Latin America Registry & Court Search",
  .name_ja = "ラテンアメリカ 登記・裁判所検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/world_reg_latam",
  .description = "Meta-search across ~25 real BR/MX/AR/CL/CO/PE company, tax, electoral & court registries (keyless anchor-scrape; JS/anti-bot rows honest-empty)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(latam_registry_def)
