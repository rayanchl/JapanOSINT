/* collectors/sources/cyi_rdap_asn.c
 * OSINT service — RDAP_ASN. ASN pivot (ctx->entity = "AS13335" or "13335")
 * against the RIPE RDAP autnum endpoint. The fleet's existing RDAP sources
 * cover IPs (ARIN) and domains (rdap.org); nothing resolved an ASN.
 * Endpoint: https://rdap.db.ripe.net/autnum/<asn>                    (keyless)
 * parse_notes: "vcardArray is the awkward jCard nested-array format:
 * entities[].vcardArray[1] is a list of [name, params, type, value] tuples —
 * index by the NAME element, never by position." The extractor below does that.
 * Other RIRs answer the same path shape (rdap.apnic.net, rdap.arin.net,
 * rdap.lacnic.net, rdap.afrinic.net); IANA's bootstrap at
 * data.iana.org/rdap/autnum.json says which — this pivot pins RIPE.
 * Emits ONE row: handle, name, startAutnum/endAutnum, status, country, the
 * entity handles + roles + contact fn/email, and the registration/last-changed
 * events. An unallocated ASN answers 404 -> honest empty (return 0).
 * No coordinates -> has_geo 0 (R2).
 * Licence: RIPE NCC database terms are linked in every response; bulk
 * re-publication of contact data is restricted.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* jCard: pull the value of the property whose NAME element equals `want`. */
static const char *vcard_value(const cJSON *ent, const char *want) {
  const cJSON *vc = cJSON_GetObjectItem(ent, "vcardArray");
  if (!cJSON_IsArray(vc) || cJSON_GetArraySize(vc) < 2) return NULL;
  const cJSON *props = cJSON_GetArrayItem(vc, 1);
  if (!cJSON_IsArray(props)) return NULL;
  const cJSON *prop;
  cJSON_ArrayForEach(prop, props) {
    if (!cJSON_IsArray(prop) || cJSON_GetArraySize(prop) < 4) continue;
    const cJSON *pn = cJSON_GetArrayItem(prop, 0);
    const cJSON *pv = cJSON_GetArrayItem(prop, 3);
    if (cJSON_IsString(pn) && pn->valuestring && strcmp(pn->valuestring, want) == 0 &&
        cJSON_IsString(pv) && pv->valuestring && pv->valuestring[0])
      return pv->valuestring;
  }
  return NULL;
}

static char *http_get(const source_ctx *ctx, const char *url,
                      const char *const *hdrs, long *status) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 20000, 1, &hr);
  *status = hr.status;
  if (rc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  char *b = hr.body; hr.body = NULL; http_response_free(&hr);
  return b;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  unsigned long asn = jo_parse_asn(ctx->entity);
  if (!asn) return 0;                            /* wrong shape -> no-op */

  char url[160];
  snprintf(url, sizeof url, "https://rdap.db.ripe.net/autnum/%lu", asn);
  const char *hdrs[] = { "Accept: application/rdap+json, application/json", NULL };

  long status = 0;
  char *body = http_get(ctx, url, hdrs, &status);
  if (!body) {
    fprintf(stderr, "[RDAP_ASN] http status=%ld\n", status);
    if (status >= 400 && status < 500) return 0;  /* unallocated / not in RIPE */
    return -1;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[RDAP_ASN] unparseable body\n"); return -1; }

  const char *handle = jo_sv(root, "handle");
  const char *name   = jo_sv(root, "name");
  const char *cc     = jo_sv(root, "country");

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "RDAP_ASN");
  cJSON_AddStringToObject(p, "registry", "RIPE NCC");
  cJSON_AddNumberToObject(p, "asn", (double)asn);
  if (handle) cJSON_AddStringToObject(p, "handle", handle);
  if (name)   cJSON_AddStringToObject(p, "name", name);
  if (cc)     cJSON_AddStringToObject(p, "country", cc);
  const cJSON *sa = cJSON_GetObjectItem(root, "startAutnum");
  if (cJSON_IsNumber(sa)) cJSON_AddNumberToObject(p, "start_autnum", sa->valuedouble);
  const cJSON *ea = cJSON_GetObjectItem(root, "endAutnum");
  if (cJSON_IsNumber(ea)) cJSON_AddNumberToObject(p, "end_autnum", ea->valuedouble);

  cJSON *statuses = cJSON_CreateArray();
  const cJSON *sti;
  cJSON_ArrayForEach(sti, cJSON_GetObjectItem(root, "status"))
    if (cJSON_IsString(sti) && sti->valuestring)
      cJSON_AddItemToArray(statuses, cJSON_CreateString(sti->valuestring));
  cJSON_AddItemToObject(p, "statuses", statuses);

  /* contacts: handle + roles + jCard fn/email, indexed by property NAME */
  cJSON *contacts = cJSON_CreateArray();
  const cJSON *ent;
  cJSON_ArrayForEach(ent, cJSON_GetObjectItem(root, "entities")) {
    cJSON *c = cJSON_CreateObject();
    const char *eh = jo_sv(ent, "handle");
    if (eh) cJSON_AddStringToObject(c, "handle", eh);
    const cJSON *roles = cJSON_GetObjectItem(ent, "roles");
    if (cJSON_IsArray(roles) && cJSON_GetArraySize(roles) > 0)
      cJSON_AddItemToObject(c, "roles", cJSON_Duplicate(roles, 1));
    const char *fn = vcard_value(ent, "fn");
    if (fn) cJSON_AddStringToObject(c, "fn", fn);
    const char *em = vcard_value(ent, "email");
    if (em) cJSON_AddStringToObject(c, "email", em);
    if (c->child) cJSON_AddItemToArray(contacts, c);
    else cJSON_Delete(c);
  }
  if (cJSON_GetArraySize(contacts) > 0) cJSON_AddItemToObject(p, "contacts", contacts);
  else cJSON_Delete(contacts);

  const char *registered = NULL, *changed = NULL;
  const cJSON *ev;
  cJSON_ArrayForEach(ev, cJSON_GetObjectItem(root, "events")) {
    const char *act = jo_sv(ev, "eventAction");
    const char *dt  = jo_sv(ev, "eventDate");
    if (!act || !dt) continue;
    if (strcmp(act, "registration") == 0) registered = dt;
    else if (strcmp(act, "last changed") == 0) changed = dt;
  }
  if (registered) cJSON_AddStringToObject(p, "registered", registered);
  if (changed)    cJSON_AddStringToObject(p, "last_changed", changed);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char title[128];
  snprintf(title, sizeof title, "AS%lu %s", asn, name ? name : "");
  char summary[320];
  snprintf(summary, sizeof summary, "RIPE RDAP%s%s%s%s",
           handle ? " · " : "", handle ? handle : "",
           cc ? " · " : "", cc ? cc : "");

  intel_item it = {0};
  it.remote_key      = handle ? handle : title;
  it.title           = title;
  it.summary         = summary;
  it.link            = url;
  it.lang            = "en";
  it.published_at    = registered;
  it.record_type     = "rdap-autnum";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"cyber\",\"rdap\",\"asn\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[RDAP_ASN] emitted 1 (AS%lu)\n", asn);
  return 0;
}

static const source_def cyi_rdap_asn_def = {
  .id = "RDAP_ASN", .collector = "osint",
  .name = "RDAP autnum lookup (RIPE)",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://rdap.db.ripe.net/autnum/",
  .description = "Structured registry record for an AS number: holder name, org handle, abuse/admin contacts and change events.",
  .license = "RIPE NCC database terms; bulk re-publication of contact data is restricted.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_rdap_asn_def)
