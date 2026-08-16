/* collectors/sources/reg2_latam_tax.c
 *
 * Three Latin-American tax-authority company pivots. All keyless, all
 * on-demand (entity = the national taxpayer id). Each carries a red-flag field
 * that the register itself publishes; those flags are surfaced into
 * properties_json rather than dropped.
 *
 *  PE_RUC_LOOKUP        GET https://api.apis.net.pe/v1/ruc?numero=<11-digit RUC>
 *    Emits: nombre, numeroDocumento, estado (ACTIVO / BAJA …),
 *           condicion — "NO HABIDO" means SUNAT cannot locate the taxpayer at
 *           its registered address, a genuine red flag — direccion, distrito,
 *           ubigeo, viaNombre, numero.
 *    Licence: free mirror of SUNAT's public RUC consultation; keyless tier.
 *
 *  EC_SRI_RUC           GET https://srienlinea.sri.gob.ec/sri-catastro-sujeto-servicio-internet/
 *                           rest/ConsolidadoContribuyente/obtenerPorNumerosRuc?&ruc=<13-digit RUC>
 *    (note the literal "?&ruc=" — that is the working form.) Returns a JSON
 *    ARRAY. Emits: numeroRuc, razonSocial, estadoContribuyenteRuc,
 *    actividadEconomicaPrincipal, tipoContribuyente, regimen,
 *    contribuyenteEspecial, fechaInicioActividades, representantesLegales[]
 *    (named legal representatives — a director pivot) and
 *    contribuyenteFantasma — "SI" is an SRI-declared shell company.
 *    Licence: official SRI public consultation service, no key.
 *
 *  CR_HACIENDA_TAXPAYER GET https://api.hacienda.go.cr/fe/ae?identificacion=<cedula>
 *    Emits: nombre, tipoIdentificacion, regimen.descripcion, situacion.estado,
 *           situacion.moroso (tax delinquent), situacion.omiso (non-filer),
 *           situacion.administracionTributaria, actividades[] (code + text).
 *    Unknown ids answer HTTP 404 — that is "not found", NOT a fetch failure, so
 *    it returns 0.
 *    Licence: official Ministerio de Hacienda e-invoicing API, keyless.
 *
 * R2: none of these publish coordinates; no row ever claims geo, and the
 * registered address is never geocoded. R3: a taxpayer id with no match is a
 * clean empty (return 0).
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Keep only digits from the entity; returns the count written (<= cap). */
static int lt_digits(const char *s, char *out, int cap) {
  int nd = 0;
  for (const char *p = s; *p && nd < cap - 1; p++)
    if (isdigit((unsigned char)*p)) out[nd++] = *p;
  out[nd] = 0;
  return nd;
}

/* A field that may be a string ("SI"/"NO") or a JSON bool. Copies a normalized
 * string into props under `key`; returns 1 when the flag reads as true. */
static int lt_flag(cJSON *props, const cJSON *o, const char *field,
                   const char *key) {
  const cJSON *v = cJSON_GetObjectItem(o, field);
  if (!v) return 0;
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    cJSON_AddStringToObject(props, key, v->valuestring);
    return (strcasecmp(v->valuestring, "SI") == 0 ||
            strcasecmp(v->valuestring, "S") == 0 ||
            strcasecmp(v->valuestring, "true") == 0);
  }
  if (cJSON_IsBool(v)) {
    int t = cJSON_IsTrue(v) ? 1 : 0;
    cJSON_AddStringToObject(props, key, t ? "SI" : "NO");
    return t;
  }
  return 0;
}

/* ------------------------------------------------------------- Peru — RUC */
static int pe_run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  char ruc[20];
  if (lt_digits(q, ruc, (int)sizeof ruc) != 11) return 0;   /* RUC is 11 digits */

  char url[160];
  snprintf(url, sizeof url, "https://api.apis.net.pe/v1/ruc?numero=%s", ruc);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "PE_RUC_LOOKUP");
  if (!body) return 0;                       /* not found / unavailable */
  cJSON *r = cJSON_Parse(body);
  free(body);
  if (!r) return 0;

  const char *nombre = jo_sv(r, "nombre");
  if (!nombre) { cJSON_Delete(r); return 0; }
  const char *estado    = jo_sv(r, "estado");
  const char *condicion = jo_sv(r, "condicion");
  const char *direccion = jo_sv(r, "direccion");
  const char *distrito  = jo_sv(r, "distrito");
  const char *doc       = jo_sv(r, "numeroDocumento");

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "PE_RUC_LOOKUP");
  cJSON_AddStringToObject(props, "source", "SUNAT (via api.apis.net.pe)");
  cJSON_AddStringToObject(props, "ruc", doc ? doc : ruc);
  cJSON_AddStringToObject(props, "nombre", nombre);
  if (estado)    cJSON_AddStringToObject(props, "estado", estado);
  if (condicion) {
    cJSON_AddStringToObject(props, "condicion", condicion);
    /* SUNAT cannot locate the taxpayer at its registered address. */
    if (strcasecmp(condicion, "NO HABIDO") == 0)
      cJSON_AddBoolToObject(props, "no_habido", 1);
  }
  if (direccion) cJSON_AddStringToObject(props, "direccion", direccion);
  if (distrito)  cJSON_AddStringToObject(props, "distrito", distrito);
  if (jo_sv(r, "ubigeo"))    cJSON_AddStringToObject(props, "ubigeo", jo_sv(r, "ubigeo"));
  if (jo_sv(r, "viaNombre")) cJSON_AddStringToObject(props, "via_nombre", jo_sv(r, "viaNombre"));
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char summary[420];
  snprintf(summary, sizeof summary, "RUC %s%s%s%s%s%s%s",
           doc ? doc : ruc,
           estado ? " · " : "", estado ? estado : "",
           condicion ? " · " : "", condicion ? condicion : "",
           distrito ? " · " : "", distrito ? distrito : "");

  intel_item it = {0};
  it.remote_key      = doc ? doc : ruc;
  it.title           = nombre;
  it.summary         = summary;
  it.body            = pj;
  it.link            = url;
  it.lang            = "es";
  it.record_type     = "pe-taxpayer";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"registry\",\"peru\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(r);
  fprintf(stderr, "[PE_RUC_LOOKUP] emitted 1 (%s)\n", ruc);
  return 0;
}

/* ---------------------------------------------------------- Ecuador — SRI */
static int ec_run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  char ruc[24];
  if (lt_digits(q, ruc, (int)sizeof ruc) != 13) return 0;   /* RUC is 13 digits */

  char url[320];
  snprintf(url, sizeof url,
           "https://srienlinea.sri.gob.ec/sri-catastro-sujeto-servicio-internet"
           "/rest/ConsolidadoContribuyente/obtenerPorNumerosRuc?&ruc=%s", ruc);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "EC_SRI_RUC");
  if (!body) return 0;
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) return 0;
  if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); return 0; }

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    if (!cJSON_IsObject(r)) continue;
    const char *razon = jo_sv(r, "razonSocial");
    if (!razon) continue;
    const char *nruc   = jo_sv(r, "numeroRuc");
    const char *estado = jo_sv(r, "estadoContribuyenteRuc");
    const char *act    = jo_sv(r, "actividadEconomicaPrincipal");
    const char *tipo   = jo_sv(r, "tipoContribuyente");
    const char *reg    = jo_sv(r, "regimen");

    const cJSON *fechas = cJSON_GetObjectItem(r, "informacionFechasContribuyente");
    const char *inicio = fechas ? jo_sv(fechas, "fechaInicioActividades") : NULL;
    const char *cese   = fechas ? jo_sv(fechas, "fechaCese") : NULL;

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "EC_SRI_RUC");
    cJSON_AddStringToObject(props, "source", "SRI Ecuador");
    cJSON_AddStringToObject(props, "ruc", nruc ? nruc : ruc);
    cJSON_AddStringToObject(props, "razon_social", razon);
    if (estado) cJSON_AddStringToObject(props, "estado_contribuyente", estado);
    if (act)    cJSON_AddStringToObject(props, "actividad_economica_principal", act);
    if (tipo)   cJSON_AddStringToObject(props, "tipo_contribuyente", tipo);
    if (reg)    cJSON_AddStringToObject(props, "regimen", reg);
    if (inicio) cJSON_AddStringToObject(props, "fecha_inicio_actividades", inicio);
    if (cese)   cJSON_AddStringToObject(props, "fecha_cese", cese);
    lt_flag(props, r, "contribuyenteEspecial", "contribuyente_especial");
    /* SRI-declared shell company — the headline flag of this register. */
    int fantasma = lt_flag(props, r, "contribuyenteFantasma", "contribuyente_fantasma");
    if (fantasma) cJSON_AddBoolToObject(props, "shell_company_flag", 1);
    lt_flag(props, r, "transaccionesInexistente", "transacciones_inexistentes");

    cJSON *reps = cJSON_CreateArray();
    const cJSON *rl = cJSON_GetObjectItem(r, "representantesLegales");
    if (cJSON_IsArray(rl)) {
      const cJSON *p;
      cJSON_ArrayForEach(p, rl) {
        const char *nm = jo_sv(p, "nombre");
        if (!nm) continue;
        cJSON *e = cJSON_CreateObject();
        cJSON_AddStringToObject(e, "nombre", nm);
        if (jo_sv(p, "identificacion"))
          cJSON_AddStringToObject(e, "identificacion", jo_sv(p, "identificacion"));
        cJSON_AddItemToArray(reps, e);
      }
    }
    cJSON_AddItemToObject(props, "representantes_legales", reps);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char summary[460];
    snprintf(summary, sizeof summary, "RUC %s%s%s%s%s%s",
             nruc ? nruc : ruc,
             estado ? " · " : "", estado ? estado : "",
             act ? " · " : "", act ? act : "",
             fantasma ? " · CONTRIBUYENTE FANTASMA" : "");

    intel_item it = {0};
    it.remote_key      = nruc ? nruc : ruc;
    it.title           = razon;
    it.summary         = summary;
    it.body            = pj;
    it.link            = url;
    it.lang            = "es";
    it.published_at    = inicio;
    it.record_type     = "ec-taxpayer";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"registry\",\"ecuador\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[EC_SRI_RUC] emitted %d\n", n);
  return 0;
}

/* ------------------------------------------------------- Costa Rica — ATV */
static int cr_run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  char id[24];
  int nd = lt_digits(q, id, (int)sizeof id);
  if (nd < 9 || nd > 12) return 0;              /* cedula juridica / fisica */

  char url[160];
  snprintf(url, sizeof url,
           "https://api.hacienda.go.cr/fe/ae?identificacion=%s", id);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "CR_HACIENDA_TAXPAYER");
  if (!body) return 0;      /* 404 = unknown id → clean empty, not a failure */
  cJSON *r = cJSON_Parse(body);
  free(body);
  if (!r) return 0;

  const char *nombre = jo_sv(r, "nombre");
  if (!nombre) { cJSON_Delete(r); return 0; }

  const cJSON *sit = cJSON_GetObjectItem(r, "situacion");
  const cJSON *rgm = cJSON_GetObjectItem(r, "regimen");
  const char *estado = sit ? jo_sv(sit, "estado") : NULL;
  const char *regimen = rgm ? jo_sv(rgm, "descripcion") : NULL;

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CR_HACIENDA_TAXPAYER");
  cJSON_AddStringToObject(props, "source", "Ministerio de Hacienda CR");
  cJSON_AddStringToObject(props, "identificacion", id);
  cJSON_AddStringToObject(props, "nombre", nombre);
  if (jo_sv(r, "tipoIdentificacion"))
    cJSON_AddStringToObject(props, "tipo_identificacion", jo_sv(r, "tipoIdentificacion"));
  if (regimen) cJSON_AddStringToObject(props, "regimen", regimen);
  if (estado)  cJSON_AddStringToObject(props, "situacion_estado", estado);
  int moroso = 0, omiso = 0;
  if (sit) {
    /* Delinquency / non-filer flags — the interesting part of this register. */
    moroso = lt_flag(props, sit, "moroso", "moroso");
    omiso  = lt_flag(props, sit, "omiso", "omiso");
    if (jo_sv(sit, "administracionTributaria"))
      cJSON_AddStringToObject(props, "administracion_tributaria",
                              jo_sv(sit, "administracionTributaria"));
    if (jo_sv(sit, "mensaje"))
      cJSON_AddStringToObject(props, "situacion_mensaje", jo_sv(sit, "mensaje"));
  }

  cJSON *acts = cJSON_CreateArray();
  const cJSON *al = cJSON_GetObjectItem(r, "actividades");
  if (cJSON_IsArray(al)) {
    const cJSON *a;
    cJSON_ArrayForEach(a, al) {
      const char *desc = jo_sv(a, "descripcion");
      if (!desc) continue;
      cJSON *e = cJSON_CreateObject();
      cJSON_AddStringToObject(e, "descripcion", desc);
      const cJSON *cod = cJSON_GetObjectItem(a, "codigo");
      if (cJSON_IsString(cod) && cod->valuestring)
        cJSON_AddStringToObject(e, "codigo", cod->valuestring);
      else if (cJSON_IsNumber(cod))
        cJSON_AddNumberToObject(e, "codigo", cod->valuedouble);
      if (jo_sv(a, "estado"))
        cJSON_AddStringToObject(e, "estado", jo_sv(a, "estado"));
      cJSON_AddItemToArray(acts, e);
    }
  }
  cJSON_AddItemToObject(props, "actividades", acts);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char summary[420];
  snprintf(summary, sizeof summary, "Cedula %s%s%s%s%s%s",
           id,
           estado ? " · " : "", estado ? estado : "",
           regimen ? " · " : "", regimen ? regimen : "",
           moroso ? " · MOROSO" : (omiso ? " · OMISO" : ""));

  intel_item it = {0};
  it.remote_key      = id;
  it.title           = nombre;
  it.summary         = summary;
  it.body            = pj;
  it.link            = url;
  it.lang            = "es";
  it.record_type     = "cr-taxpayer";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"registry\",\"costa-rica\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(r);
  fprintf(stderr, "[CR_HACIENDA_TAXPAYER] emitted 1 (%s)\n", id);
  return 0;
}

static const source_def reg2_pe_ruc_def = {
  .id = "PE_RUC_LOOKUP", .collector = "osint",
  .name = "Peru RUC taxpayer lookup",
  .update_interval_sec = 0, .run = pe_run,
  .category = "economy", .type = "api",
  .url = "https://api.apis.net.pe/v1/ruc",
  .description = "Peruvian RUC → registered company name, SUNAT status, the "
                 "'habido' address-verification flag (NO HABIDO = SUNAT cannot "
                 "locate the taxpayer) and full registered address. Keyless.",
  .license = "Free mirror of SUNAT's public RUC consultation; keyless tier.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_pe_ruc_def)

static const source_def reg2_ec_sri_def = {
  .id = "EC_SRI_RUC", .collector = "osint",
  .name = "Ecuador SRI taxpayer register (RUC)",
  .update_interval_sec = 0, .run = ec_run,
  .category = "economy", .type = "api",
  .url = "https://srienlinea.sri.gob.ec/sri-catastro-sujeto-servicio-internet/rest/ConsolidadoContribuyente/obtenerPorNumerosRuc",
  .description = "Ecuador's official tax register: RUC → legal name, status, "
                 "economic activity, regime, start/cease dates, the SRI shell-"
                 "company flag (contribuyenteFantasma) and named legal "
                 "representatives. Keyless.",
  .license = "Official SRI public consultation service, no key, no stated restriction.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_ec_sri_def)

static const source_def reg2_cr_hacienda_def = {
  .id = "CR_HACIENDA_TAXPAYER", .collector = "osint",
  .name = "Costa Rica Hacienda taxpayer situation lookup",
  .update_interval_sec = 0, .run = cr_run,
  .category = "economy", .type = "api",
  .url = "https://api.hacienda.go.cr/fe/ae",
  .description = "Costa Rican cedula juridica → registered name, tax regime, "
                 "registration state and the delinquency (moroso) / non-filer "
                 "(omiso) flags plus declared economic activities. Keyless.",
  .license = "Official Ministerio de Hacienda e-invoicing API, keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_cr_hacienda_def)
