/* collectors/sources/cyi_circl_cve_last.c
 * CIRCL Vulnerability-Lookup — the most recent vulnerabilities, in OSV format,
 * spanning CVE, GHSA, PYSEC and vendor advisories with cross-source aliases.
 * Endpoint: https://cve.circl.lu/api/last/50                          (keyless)
 * parse_notes: "1.4 MB for 50 records — request a small n ... aliases[] is the
 * join key to the existing NVD/KEV rows." Both honoured: n is kept at 50 and
 * aliases[] is always carried so a row joins to the NVD/KEV entries the fleet
 * already holds. (vulnerability.circl.lu serves the same content; this
 * collector pins one host, as the notes advise.)
 * Emits: id, published, modified, aliases[], details, affected packages,
 * severity scores, reference URLs. No coordinates -> has_geo 0 (R2) — a CVE
 * pinned to a vendor head office was one of the audit's removals.
 * Licence: CIRCL public service, free to use; the underlying advisories keep
 * their own licences.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://cve.circl.lu/api/last/50"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 40000);
  if (!doc) { fprintf(stderr, "[circl-cve-last] fetch/parse failed\n"); return -1; }

  const cJSON *rows = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "results");

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *id = jo_sv(r, "id");
    if (!id) id = jo_sv(r, "cveMetadata");         /* defensive: alternate shape */
    if (!id) continue;
    const char *published = jo_sv(r, "published");
    const char *modified  = jo_sv(r, "modified");
    const char *details   = jo_sv(r, "details");
    if (!details) details = jo_sv(r, "summary");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "vuln_id", id);
    if (published) cJSON_AddStringToObject(p, "published", published);
    if (modified)  cJSON_AddStringToObject(p, "modified", modified);

    const cJSON *aliases = cJSON_GetObjectItem(r, "aliases");
    if (cJSON_IsArray(aliases) && cJSON_GetArraySize(aliases) > 0)
      cJSON_AddItemToObject(p, "aliases", cJSON_Duplicate(aliases, 1));

    /* affected[].package.name — real upstream package identities only */
    cJSON *pkgs = cJSON_CreateArray();
    const cJSON *aff;
    cJSON_ArrayForEach(aff, cJSON_GetObjectItem(r, "affected")) {
      const cJSON *pkg = cJSON_GetObjectItem(aff, "package");
      const char *pn = cJSON_IsObject(pkg) ? jo_sv(pkg, "name") : NULL;
      if (pn) cJSON_AddItemToArray(pkgs, cJSON_CreateString(pn));
    }
    if (cJSON_GetArraySize(pkgs) > 0) cJSON_AddItemToObject(p, "affected_packages", pkgs);
    else cJSON_Delete(pkgs);

    /* severity[].score (CVSS vector strings) */
    const char *first_score = NULL;
    cJSON *scores = cJSON_CreateArray();
    const cJSON *sev;
    cJSON_ArrayForEach(sev, cJSON_GetObjectItem(r, "severity")) {
      const char *sc = jo_sv(sev, "score");
      if (!sc) continue;
      if (!first_score) first_score = sc;
      cJSON_AddItemToArray(scores, cJSON_CreateString(sc));
    }
    if (cJSON_GetArraySize(scores) > 0) cJSON_AddItemToObject(p, "severity", scores);
    else cJSON_Delete(scores);

    /* references[].url */
    const char *link = NULL;
    cJSON *refs = cJSON_CreateArray();
    const cJSON *ref;
    cJSON_ArrayForEach(ref, cJSON_GetObjectItem(r, "references")) {
      const char *u = jo_sv(ref, "url");
      if (!u) continue;
      if (!link) link = u;
      cJSON_AddItemToArray(refs, cJSON_CreateString(u));
    }
    if (cJSON_GetArraySize(refs) > 0) cJSON_AddItemToObject(p, "references", refs);
    else cJSON_Delete(refs);

    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char summary[320];
    snprintf(summary, sizeof summary, "%s%s%s",
             published ? published : "",
             (published && first_score) ? " · " : "",
             first_score ? first_score : "");

    char fallback_link[160];
    if (!link) {
      snprintf(fallback_link, sizeof fallback_link, "https://cve.circl.lu/vuln/%s", id);
      link = fallback_link;
    }

    intel_item it = {0};
    it.remote_key      = id;
    it.title           = id;
    it.summary         = summary;
    it.body            = details;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = published;
    it.record_type     = "vulnerability";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"vulnerability\",\"circl\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[circl-cve-last] emitted %d\n", n);
  return 0;
}

static const source_def cyi_circl_cve_last_def = {
  .id = "circl-cve-last", .collector = "cyber",
  .name = "CIRCL Vulnerability-Lookup — most recent vulnerabilities",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "CIRCL's aggregated vulnerability stream in OSV format across CVE, GHSA, PYSEC and vendor advisories, with cross-source aliases.",
  .license = "CIRCL public service, free to use; underlying advisories keep their own licences.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_circl_cve_last_def)
