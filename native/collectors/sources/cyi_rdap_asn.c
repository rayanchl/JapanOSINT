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
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"
#include "cyi_common.inc"

/* jCard: every value of the property whose NAME element equals `want`, in
 * document order, appended to `out`. A jCard property is the fixed 4-tuple
 * [name, params, type, value] — index 0 is the NAME and index 3 is the VALUE,
 * which is a tuple read, not a list head (docs/SOURCE_EXHAUSTIVENESS.md lists
 * the RDAP jCard tuple among the legitimate fixed-shape exceptions).
 *
 * It has to be every value, not the first: a RIPE entity routinely carries two
 * `tel` properties (office and fax) and can carry more than one `email`, and
 * an abuse address that happens to be listed second was simply invisible. */
static int vcard_values(const cJSON *ent, const char *want, cJSON *out) {
  const cJSON *vc = cJSON_GetObjectItem(ent, "vcardArray");
  if (!cJSON_IsArray(vc) || cJSON_GetArraySize(vc) < 2) return 0;
  const cJSON *props = cJSON_GetArrayItem(vc, 1);
  if (!cJSON_IsArray(props)) return 0;
  int found = 0;
  const cJSON *prop;
  cJSON_ArrayForEach(prop, props) {
    if (!cJSON_IsArray(prop) || cJSON_GetArraySize(prop) < 4) continue;
    const cJSON *pn = cJSON_GetArrayItem(prop, 0);  /* exhaustive-ok: jCard 4-tuple [name,params,type,value]; index 0 IS the name element */
    const cJSON *pv = cJSON_GetArrayItem(prop, 3);
    if (!cJSON_IsString(pn) || !pn->valuestring ||
        strcmp(pn->valuestring, want) != 0) continue;
    if (!cJSON_IsString(pv) || !pv->valuestring || !pv->valuestring[0]) continue;
    if (out) cJSON_AddItemToArray(out, cJSON_CreateString(pv->valuestring));
    found++;
  }
  return found;
}

/* Add `key` (the display scalar, first value) and `key`s (every value) to `c`
 * when the entity published any. */
static void vcard_put(cJSON *c, const cJSON *ent, const char *want,
                      const char *key, const char *key_all) {
  cJSON *all = cJSON_CreateArray();
  if (!vcard_values(ent, want, all)) { cJSON_Delete(all); return; }
  const cJSON *first = cJSON_GetArrayItem(all, 0);  /* exhaustive-ok: display scalar; `all` itself is attached as key_all when it holds more than one */
  if (cJSON_IsString(first)) cJSON_AddStringToObject(c, key, first->valuestring);
  if (cJSON_GetArraySize(all) > 1) cJSON_AddItemToObject(c, key_all, all);
  else cJSON_Delete(all);
}

/* RDAP entities NEST: on a RIPE autnum the `abuse` entity carries its own
 * entities[] — eleven of them on AS3333 — and reading only the top level threw
 * every one of those contacts away. This walks the whole tree. `depth` is a
 * cycle/blowup guard, not an editorial bound. */
#define RDAP_ENTITY_DEPTH 6   /* exhaustive-ok: recursion guard against a cyclic/absurd RDAP entity tree, not a record bound */

static void rdap_contacts(cJSON *contacts, const cJSON *entities, int depth) {
  if (depth > RDAP_ENTITY_DEPTH) return;
  const cJSON *ent;
  cJSON_ArrayForEach(ent, entities) {
    cJSON *c = cJSON_CreateObject();
    const char *eh = jo_sv(ent, "handle");
    if (eh) cJSON_AddStringToObject(c, "handle", eh);
    if (depth > 0) cJSON_AddNumberToObject(c, "nesting_depth", depth);
    const cJSON *roles = cJSON_GetObjectItem(ent, "roles");
    if (cJSON_IsArray(roles) && cJSON_GetArraySize(roles) > 0)
      cJSON_AddItemToObject(c, "roles", cJSON_Duplicate(roles, 1));
    vcard_put(c, ent, "fn",    "fn",    "fns");
    vcard_put(c, ent, "email", "email", "emails");
    vcard_put(c, ent, "tel",   "tel",   "tels");
    vcard_put(c, ent, "org",   "org",   "orgs");
    vcard_put(c, ent, "kind",  "kind",  "kinds");
    if (c->child) cJSON_AddItemToArray(contacts, c);
    else cJSON_Delete(c);
    rdap_contacts(contacts, cJSON_GetObjectItem(ent, "entities"), depth + 1);
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  unsigned long asn = jo_parse_asn(ctx->entity);
  if (!asn) return 0;                            /* wrong shape -> no-op */

  char url[160];
  snprintf(url, sizeof url, "https://rdap.db.ripe.net/autnum/%lu", asn);
  const char *hdrs[] = { "Accept: application/rdap+json, application/json", NULL };

  long status = 0;
  char *body = cyi_get(ctx, url, hdrs, 20000, &status);
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

  /* contacts: handle + roles + jCard fn/email/tel/org, indexed by property
   * NAME, for every entity in the tree including nested sub-entities. */
  cJSON *contacts = cJSON_CreateArray();
  rdap_contacts(contacts, cJSON_GetObjectItem(root, "entities"), 0);
  if (cJSON_GetArraySize(contacts) > 0) cJSON_AddItemToObject(p, "contacts", contacts);
  else cJSON_Delete(contacts);

  /* The rest of what RIPE handed back. remarks/notices carry the database
   * terms and any redaction note, links carry the self/up RDAP URLs, and all
   * three were being dropped at the seam between fetch and row. */
  const cJSON *rem = cJSON_GetObjectItem(root, "remarks");
  if (cJSON_IsArray(rem) && cJSON_GetArraySize(rem) > 0)
    cJSON_AddItemToObject(p, "remarks", cJSON_Duplicate(rem, 1));
  const cJSON *notices = cJSON_GetObjectItem(root, "notices");
  if (cJSON_IsArray(notices) && cJSON_GetArraySize(notices) > 0)
    cJSON_AddItemToObject(p, "notices", cJSON_Duplicate(notices, 1));
  const cJSON *rlinks = cJSON_GetObjectItem(root, "links");
  if (cJSON_IsArray(rlinks) && cJSON_GetArraySize(rlinks) > 0)
    cJSON_AddItemToObject(p, "links", cJSON_Duplicate(rlinks, 1));
  const cJSON *redacted = cJSON_GetObjectItem(root, "redacted");
  if (cJSON_IsArray(redacted) && cJSON_GetArraySize(redacted) > 0)
    cJSON_AddItemToObject(p, "redacted", cJSON_Duplicate(redacted, 1));
  const char *port43 = jo_sv(root, "port43");
  if (port43) cJSON_AddStringToObject(p, "port43", port43);

  /* registration/last-changed are the two the row surfaces as scalars, but an
   * autnum can publish others (transfer, reinstantiation, …) and those used to
   * vanish; the whole events array is carried alongside. */
  const char *registered = NULL, *changed = NULL;
  const cJSON *evs = cJSON_GetObjectItem(root, "events");
  const cJSON *ev;
  cJSON_ArrayForEach(ev, evs) {
    const char *act = jo_sv(ev, "eventAction");
    const char *dt  = jo_sv(ev, "eventDate");
    if (!act || !dt) continue;
    if (strcmp(act, "registration") == 0) registered = dt;
    else if (strcmp(act, "last changed") == 0) changed = dt;
  }
  if (cJSON_IsArray(evs) && cJSON_GetArraySize(evs) > 0)
    cJSON_AddItemToObject(p, "events", cJSON_Duplicate(evs, 1));
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
