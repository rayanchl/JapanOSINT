/* collectors/sources/reg2_cl_mercadopublico.c
 *
 * Chile — Mercado Publico (ChileCompra) tender listing.
 *   GET https://api.mercadopublico.cl/servicios/v1/publico/licitaciones.json
 *       ?fecha=<ddmmyyyy>&ticket=<ticket>
 *
 * NOT keyless: the API requires a &ticket=. The ticket is read from the
 * environment (MERCADOPUBLICO_TICKET, or CHILECOMPRA_TICKET) and, when neither
 * is set, this collector falls back to the ticket ChileCompra PUBLISHES in its
 * own public API documentation as the general demo ticket
 * (F8537A18-6766-4DEF-9E59-426B4FEE2844) — that fallback is deliberate and is
 * recorded in every emitted row as "ticket_source":"public-demo" so an analyst
 * can see which credential produced the data. Set MERCADOPUBLICO_TICKET to use
 * your own.
 *
 * Emits, straight from Listado[]: CodigoExterno (the tender code that keys the
 * per-tender detail call), Nombre, CodigoEstado, FechaCierre. `fecha` is the
 * CLOSING date and must be formatted ddmmyyyy; today is queried first and, only
 * if the API answered cleanly with zero tenders, yesterday is tried once.
 *
 * The API answers HTTP 429 with {"Codigo":10500} when called concurrently —
 * that is treated as "nothing this run" (return 0), never as an invented row.
 * No coordinates are published, so no row claims geo (R2).
 *
 * Licence: ChileCompra public API; terms require a ticket, and a general public
 * demo ticket is published in their documentation.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

/* Published in ChileCompra's own API documentation as the general demo ticket. */
#define CL_PUBLIC_DEMO_TICKET "F8537A18-6766-4DEF-9E59-426B4FEE2844"

static int cl_fetch_day(const source_ctx *ctx, intel_sink *sink,
                        const char *ddmmyyyy, const char *ticket,
                        int from_env, int *out_parsed) {
  char url[280];
  snprintf(url, sizeof url,
           "https://api.mercadopublico.cl/servicios/v1/publico/licitaciones.json"
           "?fecha=%s&ticket=%s", ddmmyyyy, ticket);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "cl-mercadopublico-tenders");
  if (!body) return 0;                    /* 429 / transport → nothing today */

  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) return 0;

  /* Error envelope: {"Codigo":10500,"Mensaje":...} */
  const cJSON *err = cJSON_GetObjectItem(doc, "Codigo");
  const cJSON *arr = cJSON_GetObjectItem(doc, "Listado");
  if (!cJSON_IsArray(arr)) {
    if (cJSON_IsNumber(err))
      fprintf(stderr, "[cl-mercadopublico-tenders] api code %.0f\n", err->valuedouble);
    cJSON_Delete(doc);
    return 0;
  }
  *out_parsed = 1;

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    if (!cJSON_IsObject(r)) continue;
    const char *nombre = jo_sv(r, "Nombre");
    const char *codigo = jo_sv(r, "CodigoExterno");
    if (!nombre && !codigo) continue;

    const char *cierre = jo_sv(r, "FechaCierre");
    const cJSON *estado = cJSON_GetObjectItem(r, "CodigoEstado");

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "cl-mercadopublico-tenders");
    cJSON_AddStringToObject(props, "source", "api.mercadopublico.cl");
    cJSON_AddStringToObject(props, "ticket_source", from_env ? "env" : "public-demo");
    cJSON_AddStringToObject(props, "fecha_consultada", ddmmyyyy);
    if (codigo) cJSON_AddStringToObject(props, "codigo_externo", codigo);
    if (nombre) cJSON_AddStringToObject(props, "nombre", nombre);
    if (cierre) cJSON_AddStringToObject(props, "fecha_cierre", cierre);
    if (cJSON_IsNumber(estado))
      cJSON_AddNumberToObject(props, "codigo_estado", estado->valuedouble);
    else if (cJSON_IsString(estado) && estado->valuestring)
      cJSON_AddStringToObject(props, "codigo_estado", estado->valuestring);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[500];
    snprintf(title, sizeof title, "%s", nombre ? nombre : codigo);

    char summary[300];
    snprintf(summary, sizeof summary, "%s%s%s",
             codigo ? codigo : "", (codigo && cierre) ? " · cierre " : "",
             cierre ? cierre : "");

    char link[256];
    if (codigo)
      snprintf(link, sizeof link,
               "https://www.mercadopublico.cl/Procurement/Modules/RFB/"
               "DetailsAcquisition.aspx?idlicitacion=%s", codigo);
    else
      snprintf(link, sizeof link, "%s", "https://www.mercadopublico.cl/");

    intel_item it = {0};
    it.remote_key      = codigo ? codigo : title;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.body            = pj;
    it.link            = link;
    it.lang            = "es";
    it.published_at    = cierre;
    it.record_type     = "procurement-notice";
    it.properties_json = pj;
    it.tags_json       = "[\"procurement\",\"chile\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  return n;
}

static int cl_run(const source_ctx *ctx, intel_sink *sink) {
  const char *ticket = jo_env("MERCADOPUBLICO_TICKET");
  if (!ticket) ticket = jo_env("CHILECOMPRA_TICKET");
  int from_env = ticket != NULL;
  if (!ticket) {
    fprintf(stderr, "[cl-mercadopublico-tenders] MERCADOPUBLICO_TICKET unset; "
                    "using ChileCompra's published public demo ticket\n");
    ticket = CL_PUBLIC_DEMO_TICKET;
  }

  time_t now = time(NULL);
  int n = 0, parsed = 0;
  for (int back = 0; back <= 1; back++) {
    time_t t = now - (time_t)back * 24 * 3600;
    struct tm g;
    gmtime_r(&t, &g);
    char fecha[16];
    strftime(fecha, sizeof fecha, "%d%m%Y", &g);
    n += cl_fetch_day(ctx, sink, fecha, ticket, from_env, &parsed);
    if (n > 0) break;                    /* got a day's worth; stop polling */
  }

  fprintf(stderr, "[cl-mercadopublico-tenders] emitted %d%s\n",
          n, parsed ? "" : " (api did not answer with a listing)");
  return 0;                              /* a quiet day is not a failure (R3) */
}

static const source_def reg2_cl_mercadopublico_def = {
  .id = "cl-mercadopublico-tenders", .collector = "government",
  .name = "Chile Mercado Publico tender listing",
  .update_interval_sec = 21600, .run = cl_run,
  .category = "government", .type = "api",
  .url = "https://api.mercadopublico.cl/servicios/v1/publico/licitaciones.json",
  .description = "Chile's national procurement platform — all tenders closing "
                 "on a given date, with per-tender detail available by "
                 "CodigoExterno. Needs a ticket (MERCADOPUBLICO_TICKET); falls "
                 "back to ChileCompra's published public demo ticket.",
  .license = "ChileCompra public API. Terms require a ticket; a general public "
             "demo ticket is published in their documentation.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_cl_mercadopublico_def)
