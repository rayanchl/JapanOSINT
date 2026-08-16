/* collectors/sources/reg2_es_contratacion.c
 *
 * Spain — Plataforma de Contratacion del Sector Publico, CODICE/UBL syndication.
 *   GET https://contrataciondelestado.es/sindicacion/sindicacion_643/
 *       licitacionesPerfilesContratanteCompleto3.atom
 *
 * Atom 1.0 where each <entry> embeds the full CODICE (UBL) contract XML with
 * cac:/cbc: namespaces. Per entry we emit only what is actually present in the
 * document: the entry <title>, the entry <summary> (which the platform fills
 * with "Id licitación / Órgano de Contratación / Importe / Estado"), the
 * alternate <link href>, <updated>, cbc:ContractFolderID, the contracting
 * party name taken from inside the LocatedContractingParty block, the
 * ProcurementProject name, cbc:TotalAmount and the folder status code.
 * Anything the entry does not carry is omitted — never filled in.
 *
 * Keyless. The feed publishes no coordinates, so no row claims geo (R2).
 * Licence: official syndication service of the Ministerio de Hacienda; reuse
 * permitted under the Spanish public-sector reuse regime (Ley 37/2007).
 *
 * Separate feeds exist for contratos menores (sindicacion_1044) and for awards;
 * only the probed one above is wired here.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define ES_MAX 120
#define ES_FEED "https://contrataciondelestado.es/sindicacion/sindicacion_643/" \
                "licitacionesPerfilesContratanteCompleto3.atom"

static void es_unescape(char *s) {
  char *w = s;
  for (char *p = s; *p; ) {
    if (*p == '&') {
      if (strncmp(p, "&amp;", 5) == 0)  { *w++ = '&';  p += 5; continue; }
      if (strncmp(p, "&lt;", 4) == 0)   { *w++ = '<';  p += 4; continue; }
      if (strncmp(p, "&gt;", 4) == 0)   { *w++ = '>';  p += 4; continue; }
      if (strncmp(p, "&quot;", 6) == 0) { *w++ = '"';  p += 6; continue; }
      if (strncmp(p, "&apos;", 6) == 0) { *w++ = '\''; p += 6; continue; }
      if (strncmp(p, "&#39;", 5) == 0)  { *w++ = '\''; p += 5; continue; }
    }
    *w++ = *p++;
  }
  *w = 0;
}

/* Collapse runs of whitespace and trim; returns s. */
static char *es_squash(char *s) {
  char *w = s;
  int sp = 0;
  for (char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
      if (!sp && w != s) *w++ = ' ';
      sp = 1;
    } else { *w++ = (char)c; sp = 0; }
  }
  while (w > s && w[-1] == ' ') w--;
  *w = 0;
  return s;
}

/* Inner text of the first <tag> at/after `from`, unescaped + squashed. */
static char *es_tag(const char *from, const char *tag) {
  if (!from) return NULL;
  const char *cur = from;
  char *v = jo_tag_inner(&cur, tag);
  if (!v) return NULL;
  es_unescape(v);
  es_squash(v);
  if (!v[0]) { free(v); return NULL; }
  return v;
}

static int es_run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Accept: application/atom+xml, application/xml", NULL };
  char *xml = jo_get(ctx, ES_FEED, hdrs, "es-contratacion-estado");
  if (!xml) { fprintf(stderr, "[es-contratacion-estado] fetch failed\n"); return -1; }

  int n = 0;
  const char *cur = xml;
  while (n < ES_MAX) {
    const char *hit = jo_next_entry(&cur);
    if (!hit) break;
    const char *close = strstr(hit, "</entry>");
    if (!close) break;
    size_t len = (size_t)(close - hit);
    char *chunk = (char *)malloc(len + 1);
    if (!chunk) break;
    memcpy(chunk, hit, len);
    chunk[len] = 0;
    cur = close + 8;

    char *title = es_tag(chunk, "title");
    if (!title) { free(chunk); continue; }    /* no real title -> no row (R1) */

    char *summary = es_tag(chunk, "summary");
    char *updated = es_tag(chunk, "updated");
    char *folder  = es_tag(chunk, "cbc:ContractFolderID");
    char *amount  = es_tag(chunk, "cbc:TotalAmount");
    char *status  = es_tag(chunk, "cbc-place-ext:ContractFolderStatusCode");
    if (!status)  status = es_tag(chunk, "cbc:ContractFolderStatusCode");

    /* Buyer: the cbc:Name inside the LocatedContractingParty block. */
    char *buyer = NULL;
    const char *lc = strstr(chunk, "LocatedContractingParty");
    if (lc) buyer = es_tag(lc, "cbc:Name");

    /* Project title: the cbc:Name inside ProcurementProject. */
    char *project = NULL;
    const char *pp = strstr(chunk, "ProcurementProject");
    if (pp) project = es_tag(pp, "cbc:Name");

    char link[900] = "";
    const char *h = strstr(chunk, "href=\"");
    if (h) {
      h += 6;
      const char *he = strchr(h, '"');
      if (he && (size_t)(he - h) < sizeof link) {
        snprintf(link, sizeof link, "%.*s", (int)(he - h), h);
        es_unescape(link);
      }
    }

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "es-contratacion-estado");
    cJSON_AddStringToObject(props, "source", "contrataciondelestado.es");
    cJSON_AddStringToObject(props, "title", title);
    if (summary) cJSON_AddStringToObject(props, "entry_summary", summary);
    if (folder)  cJSON_AddStringToObject(props, "contract_folder_id", folder);
    if (buyer)   cJSON_AddStringToObject(props, "contracting_party", buyer);
    if (project) cJSON_AddStringToObject(props, "procurement_project", project);
    if (amount)  cJSON_AddStringToObject(props, "total_amount", amount);
    if (status)  cJSON_AddStringToObject(props, "status_code", status);
    if (updated) cJSON_AddStringToObject(props, "updated", updated);
    if (link[0]) cJSON_AddStringToObject(props, "href", link);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char sum[600];
    snprintf(sum, sizeof sum, "%s%s%s",
             buyer ? buyer : "", (buyer && summary) ? " · " : "",
             summary ? summary : "");

    intel_item it = {0};
    it.remote_key      = folder ? folder : (link[0] ? link : title);
    it.title           = title;
    it.summary         = sum[0] ? sum : NULL;
    it.body            = pj;
    it.link            = link[0] ? link : ES_FEED;
    it.lang            = "es";
    it.published_at    = updated;
    it.record_type     = "procurement-notice";
    it.properties_json = pj;
    it.tags_json       = "[\"procurement\",\"spain\",\"codice\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj);
    free(title); free(summary); free(updated); free(folder);
    free(amount); free(status); free(buyer); free(project);
    free(chunk);
  }

  free(xml);
  fprintf(stderr, "[es-contratacion-estado] emitted %d\n", n);
  return 0;
}

static const source_def reg2_es_contratacion_def = {
  .id = "es-contratacion-estado", .collector = "government",
  .name = "Spain Plataforma de Contratacion del Sector Publico (CODICE Atom)",
  .update_interval_sec = 10800, .run = es_run,
  .category = "government", .type = "api",
  .url = ES_FEED,
  .description = "Spain's national contracting platform syndicates every tender "
                 "and award as Atom entries carrying full CODICE/UBL contract "
                 "XML — buyer, object, amount and status. Keyless.",
  .license = "Official syndication service of the Ministerio de Hacienda; reuse "
             "permitted under the Spanish public-sector reuse regime (Ley 37/2007).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_es_contratacion_def)
