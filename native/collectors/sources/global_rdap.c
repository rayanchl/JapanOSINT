/* collectors/sources/global_rdap.c
 * OSINT service — RDAP_LOOKUP. On-demand domain pivot (entity = domain name)
 * against rdap.org, the IANA RDAP bootstrap redirector: GET
 * https://rdap.org/domain/<domain> 302-redirects to the authoritative registry
 * RDAP server (jo_get / libcurl follows the redirect) and returns the canonical
 * RFC-9083 JSON WHOIS record. We extract and emit registrar, domain statuses,
 * nameservers and the registration/expiry events — all REAL parsed fields, one
 * intel_item per domain. Keyless LIVE. Only runs when the entity looks like a
 * domain; non-domain entity or a fetch/parse failure → honest empty (return 0).
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Heuristic: does `s` look like a registrable domain (label.tld, no scheme,
 * no path, no @, at least one dot, plausible TLD)? */
static int looks_like_domain(const char *s) {
  if (!s || !*s) return 0;
  if (strchr(s, '@') || strchr(s, '/') || strchr(s, ' ') || strchr(s, ':'))
    return 0;
  size_t n = strlen(s);
  if (n < 3 || n > 253) return 0;
  const char *dot = strrchr(s, '.');
  if (!dot || dot == s || !dot[1]) return 0;          /* need a dot + a TLD */
  for (const char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    /* allow IDNs: any byte that is alnum, '-', '.', or non-ASCII (UTF-8) */
    if (!(isalnum(c) || c == '-' || c == '.' || c >= 0x80)) return 0;
  }
  /* TLD must be at least 2 chars and start alpha (ASCII) or be an IDN */
  if ((unsigned char)dot[1] < 0x80 && !isalpha((unsigned char)dot[1])) return 0;
  return 1;
}

/* Pull the registrar entity's fn (formatted name) from the RFC-9083 vCard
 * jCard array embedded in an entity object. Returns a malloc'd string or NULL. */
static char *registrar_name(const cJSON *entities) {
  if (!cJSON_IsArray(entities)) return NULL;
  const cJSON *ent;
  cJSON_ArrayForEach(ent, entities) {
    const cJSON *roles = cJSON_GetObjectItem(ent, "roles");
    int is_registrar = 0;
    if (cJSON_IsArray(roles)) {
      const cJSON *r;
      cJSON_ArrayForEach(r, roles)
        if (cJSON_IsString(r) && r->valuestring &&
            strcmp(r->valuestring, "registrar") == 0) { is_registrar = 1; break; }
    }
    if (!is_registrar) continue;
    /* vcardArray = [ "vcard", [ [name,{},type,value], ... ] ] */
    const cJSON *vc = cJSON_GetObjectItem(ent, "vcardArray");
    if (cJSON_IsArray(vc) && cJSON_GetArraySize(vc) >= 2) {
      const cJSON *props = cJSON_GetArrayItem(vc, 1);
      if (cJSON_IsArray(props)) {
        const cJSON *prop;
        cJSON_ArrayForEach(prop, props) {
          if (cJSON_IsArray(prop) && cJSON_GetArraySize(prop) >= 4) {
            const cJSON *pn = cJSON_GetArrayItem(prop, 0);  /* exhaustive-ok: jCard property tuple */
            const cJSON *pv = cJSON_GetArrayItem(prop, 3);
            if (cJSON_IsString(pn) && pn->valuestring &&
                strcmp(pn->valuestring, "fn") == 0 &&
                cJSON_IsString(pv) && pv->valuestring && pv->valuestring[0])
              return strdup(pv->valuestring);
          }
        }
      }
    }
  }
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_domain(q)) return 0;    /* only domains; honest empty */

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[512];
  snprintf(url, sizeof url, "https://rdap.org/domain/%s", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/rdap+json, application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "rdap");   /* follows the bootstrap 302 */
  if (!body) return 0;                            /* NXDOMAIN / no RDAP → empty */

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  const char *ldh = jo_sv(root, "ldhName");       /* canonical domain */
  const char *domain = ldh ? ldh : q;

  /* --- statuses --- */
  cJSON *statuses = cJSON_CreateArray();
  const cJSON *st = cJSON_GetObjectItem(root, "status");
  if (cJSON_IsArray(st)) {
    const cJSON *s;
    cJSON_ArrayForEach(s, st)
      if (cJSON_IsString(s) && s->valuestring)
        cJSON_AddItemToArray(statuses, cJSON_CreateString(s->valuestring));
  }

  /* --- nameservers --- */
  cJSON *nslist = cJSON_CreateArray();
  const cJSON *ns = cJSON_GetObjectItem(root, "nameservers");
  if (cJSON_IsArray(ns)) {
    const cJSON *n;
    cJSON_ArrayForEach(n, ns) {
      const char *nn = jo_sv(n, "ldhName");
      if (!nn) nn = jo_sv(n, "unicodeName");
      if (nn) cJSON_AddItemToArray(nslist, cJSON_CreateString(nn));
    }
  }

  /* --- events (registration / expiration / last changed) --- */
  const char *registered = NULL, *expires = NULL, *changed = NULL;
  const cJSON *events = cJSON_GetObjectItem(root, "events");
  if (cJSON_IsArray(events)) {
    const cJSON *e;
    cJSON_ArrayForEach(e, events) {
      const char *act = jo_sv(e, "eventAction");
      const char *dt  = jo_sv(e, "eventDate");
      if (!act || !dt) continue;
      if (strcmp(act, "registration") == 0)      registered = dt;
      else if (strcmp(act, "expiration") == 0)   expires = dt;
      else if (strcmp(act, "last changed") == 0) changed = dt;
    }
  }

  char *registrar = registrar_name(cJSON_GetObjectItem(root, "entities"));

  /* --- assemble body + properties --- */
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "domain", domain);
  if (registrar)  cJSON_AddStringToObject(data, "registrar", registrar);
  if (registered) cJSON_AddStringToObject(data, "registered", registered);
  if (expires)    cJSON_AddStringToObject(data, "expires", expires);
  if (changed)    cJSON_AddStringToObject(data, "last_changed", changed);
  cJSON_AddItemToObject(data, "statuses", cJSON_Duplicate(statuses, 1));
  cJSON_AddItemToObject(data, "nameservers", cJSON_Duplicate(nslist, 1));
  cJSON_AddStringToObject(data, "source", "RDAP");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "RDAP_LOOKUP");
  cJSON_AddStringToObject(props, "source", "rdap.org");
  cJSON_AddStringToObject(props, "domain", domain);
  if (registrar)  cJSON_AddStringToObject(props, "registrar", registrar);
  if (registered) cJSON_AddStringToObject(props, "registered", registered);
  if (expires)    cJSON_AddStringToObject(props, "expires", expires);
  cJSON_AddItemToObject(props, "statuses", statuses);      /* transferred */
  cJSON_AddItemToObject(props, "nameservers", nslist);     /* transferred */
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char summary[256];
  snprintf(summary, sizeof summary, "RDAP: %s%s%s",
           registrar ? registrar : "(registrar n/a)",
           expires ? " · expires " : "", expires ? expires : "");

  char link[512];
  snprintf(link, sizeof link, "https://rdap.org/domain/%s", domain);

  intel_item it = {0};
  it.remote_key      = domain;
  it.title           = domain;
  it.summary         = summary;
  it.body            = bj;
  it.link            = link;
  it.lang            = "en";
  it.published_at    = registered;
  it.record_type     = "rdap-domain";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"RDAP\",\"whois\"]";
  sink->emit(sink, &it);

  free(bj); free(pj); free(registrar);
  cJSON_Delete(root);
  fprintf(stderr, "[rdap] emitted 1 (%s)\n", domain);
  return 0;
}

static const source_def global_rdap_def = {
  .id = "RDAP_LOOKUP", .collector = "osint",
  .name = "RDAP Domain WHOIS", .name_ja = "RDAP ドメイン WHOIS",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://rdap.org/domain/",
  .description = "RDAP (RFC-9083) domain WHOIS via rdap.org IANA bootstrap — registrar, statuses, nameservers, registration/expiry events. Keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(global_rdap_def)
