/* collectors/sources/cyi_ripe_whois_search.c
 * OSINT service — RIPE_WHOIS_SEARCH. ASN pivot (ctx->entity = "AS3333"/"3333")
 * against the RIPE database REST search, returning the raw RPSL objects
 * (aut-num, route, inetnum, as-block, mntner, organisation) that RDAP flattens
 * away — including import/export routing policy.
 * Endpoint: https://rest.db.ripe.net/search.json
 *           ?query-string=<entity>&flags=no-referenced                (keyless)
 * parse_notes: "Deeply nested: objects.object[].attributes.attribute[] is a
 * list of {name,value} pairs — emit SELECTED names (descr, origin, mnt-by,
 * country), not the whole blob. flags=no-referenced keeps the response small."
 * Exactly that: a whitelist of attribute names is copied out, plus the object
 * type and primary-key. Personal contact objects are not harvested.
 * One row per returned RPSL object. Nothing found is an honest empty (return 0)
 * — RIPE answers 404 for a query with no objects.
 * No coordinates -> has_geo 0 (R2).
 * Licence: RIPE Database terms and conditions; personal data in contact objects
 * must not be bulk-harvested.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *WANTED[] = {
  "aut-num", "as-name", "as-block", "descr", "org", "origin", "route",
  "route6", "inetnum", "inet6num", "netname", "country", "mnt-by", "status",
  "import", "export", "created", "last-modified", "organisation", "org-name",
};
#define NWANTED ((int)(sizeof(WANTED)/sizeof(WANTED[0])))

static int wanted(const char *name) {
  for (int i = 0; i < NWANTED; i++) if (strcmp(name, WANTED[i]) == 0) return 1;
  return 0;
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
  if (!asn) return 0;                             /* wrong shape -> no-op */

  char url[224];
  snprintf(url, sizeof url,
           "https://rest.db.ripe.net/search.json?query-string=AS%lu&flags=no-referenced",
           asn);
  const char *hdrs[] = { "Accept: application/json", NULL };

  long status = 0;
  char *body = http_get(ctx, url, hdrs, &status);
  if (!body) {
    fprintf(stderr, "[RIPE_WHOIS_SEARCH] http status=%ld\n", status);
    if (status >= 400 && status < 500) return 0;  /* no objects for this query */
    return -1;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[RIPE_WHOIS_SEARCH] unparseable body\n"); return -1; }

  const cJSON *objs = cJSON_GetObjectItem(
      cJSON_GetObjectItem(root, "objects"), "object");

  int n = 0;
  const cJSON *o;
  cJSON_ArrayForEach(o, objs) {
    const char *type = jo_sv(o, "type");
    if (!type) continue;

    /* primary-key.attribute[] -> the object's key value */
    char pkey[192] = "";
    const cJSON *pk = cJSON_GetObjectItem(
        cJSON_GetObjectItem(o, "primary-key"), "attribute");
    const cJSON *pa;
    cJSON_ArrayForEach(pa, pk) {
      const char *val = jo_sv(pa, "value");
      if (!val) continue;
      size_t l = strlen(pkey);
      snprintf(pkey + l, sizeof pkey - l, "%s%s", l ? " " : "", val);
    }
    if (!pkey[0]) continue;                       /* no key -> no row */

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "service", "RIPE_WHOIS_SEARCH");
    cJSON_AddStringToObject(p, "object_type", type);
    cJSON_AddStringToObject(p, "primary_key", pkey);
    cJSON_AddNumberToObject(p, "queried_asn", (double)asn);

    char descr[512] = "";
    const cJSON *attrs = cJSON_GetObjectItem(
        cJSON_GetObjectItem(o, "attributes"), "attribute");
    const cJSON *at;
    cJSON_ArrayForEach(at, attrs) {
      const char *name = jo_sv(at, "name");
      const char *val  = jo_sv(at, "value");
      if (!name || !val || !wanted(name)) continue;
      /* repeated names (descr, mnt-by, import…) accumulate into an array */
      cJSON *slot = cJSON_GetObjectItem(p, name);
      if (!slot) cJSON_AddStringToObject(p, name, val);
      else if (cJSON_IsString(slot)) {
        cJSON *arr = cJSON_CreateArray();
        cJSON_AddItemToArray(arr, cJSON_CreateString(slot->valuestring));
        cJSON_AddItemToArray(arr, cJSON_CreateString(val));
        cJSON_ReplaceItemInObject(p, name, arr);
      } else if (cJSON_IsArray(slot)) {
        cJSON_AddItemToArray(slot, cJSON_CreateString(val));
      }
      if (strcmp(name, "descr") == 0 && !descr[0])
        snprintf(descr, sizeof descr, "%s", val);
    }

    const cJSON *srcid = cJSON_GetObjectItem(cJSON_GetObjectItem(o, "source"), "id");
    if (cJSON_IsString(srcid) && srcid->valuestring)
      cJSON_AddStringToObject(p, "source_registry", srcid->valuestring);
    cJSON_AddBoolToObject(p, "success", 1);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[256];
    snprintf(title, sizeof title, "%s %s", type, pkey);
    char summary[512];
    snprintf(summary, sizeof summary, "RIPE DB%s%s", descr[0] ? " · " : "", descr);
    char key[288];
    snprintf(key, sizeof key, "%s|%s", type, pkey);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = url;
    it.lang            = "en";
    it.record_type     = "rpsl-object";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"cyber\",\"whois\",\"rpsl\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(root);
  fprintf(stderr, "[RIPE_WHOIS_SEARCH] emitted %d (AS%lu)\n", n, asn);
  return 0;
}

static const source_def cyi_ripe_whois_search_def = {
  .id = "RIPE_WHOIS_SEARCH", .collector = "osint",
  .name = "RIPE database REST search (whois objects)",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://rest.db.ripe.net/search.json",
  .description = "Raw RPSL objects (aut-num, route, inetnum, as-block, organisation) for an ASN — the routing-policy detail RDAP flattens away.",
  .license = "RIPE Database terms and conditions; personal data in contact objects must not be bulk-harvested.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_ripe_whois_search_def)
