/* cert_circl_vulnlookup.c — CIRCL (Luxembourg) Vulnerability-Lookup, recent
 * entries. Successor to cve-search: it merges CVE, GHSA, PYSEC, OSV and
 * national sources into one OSV-shaped stream with alias cross-mapping, so one
 * flaw can be correlated across ecosystems.
 *
 * Endpoint: https://vulnerability.circl.lu/api/last/50   (keyless)
 * Shape:    bare JSON array at the document root; /api/last/N takes the record
 *           count in the path. Records follow the OSV schema.
 *
 * Emits, all read out of the record: advisory id (remote_key), summary and
 * details, aliases (the CVE/GHSA/PYSEC cross-map), published and modified
 * timestamps, severity entries, the affected package ecosystem+name list, and
 * the first upstream reference URL as `link`. If a record carries no reference
 * the row carries no link — nothing is constructed (R1).
 *
 * Some upstream feeds wrap the OSV body in a container object; both the bare
 * record and a single-key wrapper are handled.
 *
 * R2: no geometry. R3: a fetch that succeeded but returned an empty window
 * returns 0; only a failed fetch/parse returns -1.
 *
 * Licence: CIRCL publishes this as an open service; the aggregated upstream
 * licence applies per record. Keyless. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CIRCL_LAST_URL "https://vulnerability.circl.lu/api/last/50"

static int emit_osv(intel_sink *s, cJSON *r) {
  if (!cJSON_IsObject(r)) return 0;
  /* single-key container -> descend once to the OSV body */
  if (!cJSON_GetObjectItem(r, "id")) {
    cJSON *only = r->child;
    if (only && cJSON_IsObject(only) && cJSON_GetObjectItem(only, "id")) r = only;
    else return 0;
  }
  const char *id = jo_sv(r, "id");
  if (!id) return 0;                       /* no upstream id -> no row (R1) */
  const char *summary = jo_sv(r, "summary");
  const char *details = jo_sv(r, "details");
  const char *pub     = jo_sv(r, "published");

  char title[600];
  if (summary)      snprintf(title, sizeof title, "%s: %s", id, summary);
  else if (details) snprintf(title, sizeof title, "%s: %s", id, details);
  else              snprintf(title, sizeof title, "%s", id);
  jo_utf8_trunc(title, 320);

  const char *link = NULL;
  cJSON *refs = cJSON_GetObjectItem(r, "references");
  if (cJSON_IsArray(refs)) {
    cJSON *rf;
    cJSON_ArrayForEach(rf, refs) {
      const char *u = jo_sv(rf, "url");
      if (u) { link = u; break; }
    }
  }

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "advisory_id", id);
  cJSON *al = cJSON_GetObjectItem(r, "aliases");
  if (cJSON_IsArray(al)) cJSON_AddItemToObject(p, "aliases", cJSON_Duplicate(al, 1));
  const char *v;
  if ((v = jo_sv(r, "modified")))  cJSON_AddStringToObject(p, "modified", v);
  if (pub)                      cJSON_AddStringToObject(p, "published", pub);
  cJSON *sev = cJSON_GetObjectItem(r, "severity");
  if (cJSON_IsArray(sev) && cJSON_GetArraySize(sev) > 0)
    cJSON_AddItemToObject(p, "severity", cJSON_Duplicate(sev, 1));
  cJSON *aff = cJSON_GetObjectItem(r, "affected");
  if (cJSON_IsArray(aff)) {
    cJSON *pkgs = cJSON_CreateArray();
    cJSON *a;
    cJSON_ArrayForEach(a, aff) {
      cJSON *pk = cJSON_GetObjectItem(a, "package");
      if (!pk) continue;
      const char *eco = jo_sv(pk, "ecosystem"), *nm = jo_sv(pk, "name");
      if (!nm) continue;
      char buf[256];
      snprintf(buf, sizeof buf, "%s%s%s", eco ? eco : "", eco ? "/" : "", nm);
      cJSON_AddItemToArray(pkgs, cJSON_CreateString(buf));
    }
    cJSON_AddItemToObject(p, "affected_packages", pkgs);
  }
  cJSON_AddStringToObject(p, "source", "circl_vulnerability_lookup");
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  intel_item it = {0};
  it.remote_key      = id;
  it.title           = title;
  it.body            = details ? details : summary;
  it.summary         = summary ? summary : details;
  it.link            = link;
  it.lang            = "en";
  it.published_at    = jo_looks_iso(pub) ? pub : NULL;
  it.record_type     = "vulnerability";
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = "[\"vulnerability\",\"cyber\",\"osv\",\"circl\"]";
  int rc = s->emit(s, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, CIRCL_LAST_URL, hdrs, 25000);
  if (!doc) {
    fprintf(stderr, "[circl-vulnerability-lookup-recent] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "data");
  int n = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *e;
    cJSON_ArrayForEach(e, arr) n += emit_osv(s, e);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[circl-vulnerability-lookup-recent] emitted %d\n", n);
  return 0;                                  /* fetched fine; 0 is honest (R3) */
}

static const source_def cert_circl_vln_def = {
  .id = "circl-vulnerability-lookup-recent", .collector = "cyber",
  .name = "CIRCL Vulnerability-Lookup - recent entries",
  .update_interval_sec = 1800, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://vulnerability.circl.lu/api/last/50",
  .description = "CIRCL Luxembourg's aggregated vulnerability feed (successor "
                 "to cve-search): CVE, GHSA, PYSEC, OSV and national sources "
                 "merged into one OSV-shaped stream with alias cross-mapping.",
  .license = "CIRCL publishes as an open service; aggregated upstream licences "
             "apply per record.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_circl_vln_def)
