/* collectors/sources/cyi_circl_cve_last.c
 * CIRCL Vulnerability-Lookup — the most recent vulnerabilities, a mixed stream
 * of OSV records, CVE JSON 5 records and CSAF advisories with cross-source
 * aliases.
 * Endpoint: https://cve.circl.lu/api/vulnerability/last/50            (keyless)
 * (measured 2026-09-15: 8.9 MB for 50 records; the older /api/last/50 now
 * serialises the same kind of stream at 73.5 MB — see CYI_URL below.)
 * parse_notes: "request a small n ... aliases[] is the
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
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Re-pointed 2026-09-15. /api/last/50 still answers, but it now serialises its
 * 50 records at 73.5 MB (single CVE records of 5-7 MB), over the engine's 64 MB
 * body ceiling, so every run aborted as "fetch/parse failed". CIRCL's
 * Vulnerability-Lookup route for the same stream, /api/vulnerability/last/50,
 * answers the latest 50 in 8.9 MB.
 *
 * The stream is NOT only OSV. Measured on that route: 2 OSV records (top-level
 * id), 6 CVE JSON 5 records (cveMetadata + containers.cna) and 42 CSAF 2.0
 * advisories (document.tracking + vulnerabilities[]). The reader used to take
 * only a top-level `id`, so 48 of 50 records were skipped. Each format's own
 * identifier, dates, description, affected products and references are read
 * below; nothing is synthesised, and the format is recorded on the row. */
#define CYI_URL "https://cve.circl.lu/api/vulnerability/last/50"

/* Dotted-path lookup ("cveMetadata.cveId"). */
static const cJSON *cyi_at(const cJSON *o, const char *path) {
  char seg[64];
  const char *p = path;
  while (o && *p) {
    const char *dot = strchr(p, '.');
    size_t n = dot ? (size_t)(dot - p) : strlen(p);
    if (n >= sizeof seg) return NULL;
    memcpy(seg, p, n);
    seg[n] = '\0';
    o = cJSON_GetObjectItem(o, seg);
    p = dot ? dot + 1 : p + n;
  }
  return o;
}
static const char *cyi_str(const cJSON *o, const char *path) {
  const cJSON *v = cyi_at(o, path);
  return (cJSON_IsString(v) && v->valuestring && *v->valuestring) ? v->valuestring : NULL;
}

/* Every `url` string in the array at `arr` into `out`; the first one is
 * returned as the row link candidate. */
static const char *cyi_urls(const cJSON *arr, cJSON *out) {
  const char *first = NULL;
  const cJSON *ref;
  cJSON_ArrayForEach(ref, arr) {
    const char *u = jo_sv(ref, "url");
    if (!u) continue;
    if (!first) first = u;
    cJSON_AddItemToArray(out, cJSON_CreateString(u));
  }
  return first;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 60000);
  if (!doc) { fprintf(stderr, "[circl-cve-last] fetch/parse failed\n"); return -1; }

  const cJSON *rows = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "results");

  int n = 0, skipped = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const cJSON *cna = cyi_at(r, "containers.cna");
    const cJSON *csaf = cJSON_GetObjectItem(r, "document");
    const char *format = NULL, *id = NULL, *published = NULL, *modified = NULL;
    const char *details = NULL;
    if ((id = jo_sv(r, "id")) != NULL) {
      format = "osv";
      published = jo_sv(r, "published");
      modified = jo_sv(r, "modified");
      details = jo_sv(r, "details");
      if (!details) details = jo_sv(r, "summary");
    } else if ((id = cyi_str(r, "cveMetadata.cveId")) != NULL) {
      format = "cve5";
      published = cyi_str(r, "cveMetadata.datePublished");
      modified = cyi_str(r, "cveMetadata.dateUpdated");
      const cJSON *d;
      cJSON_ArrayForEach(d, cJSON_GetObjectItem(cna, "descriptions")) {
        const char *lang = jo_sv(d, "lang"), *val = jo_sv(d, "value");
        if (!val) continue;
        if (!details || (lang && strncmp(lang, "en", 2) == 0)) details = val;
        if (lang && strncmp(lang, "en", 2) == 0) break;
      }
      if (!details) details = jo_sv(cna, "title");
    } else if ((id = cyi_str(csaf, "tracking.id")) != NULL) {
      format = "csaf";
      published = cyi_str(csaf, "tracking.initial_release_date");
      modified = cyi_str(csaf, "tracking.current_release_date");
      details = jo_sv(csaf, "title");
    } else {
      skipped++;                                /* no identifier in any format */
      continue;
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "vuln_id", id);
    cJSON_AddStringToObject(p, "format", format);
    if (published) cJSON_AddStringToObject(p, "published", published);
    if (modified)  cJSON_AddStringToObject(p, "modified", modified);

    /* aliases: OSV aliases[]; for a CSAF advisory, the CVEs it covers */
    const cJSON *aliases = cJSON_GetObjectItem(r, "aliases");
    if (cJSON_IsArray(aliases) && cJSON_GetArraySize(aliases) > 0) {
      cJSON_AddItemToObject(p, "aliases", cJSON_Duplicate(aliases, 1));
    } else if (csaf) {
      cJSON *cves = cJSON_CreateArray();
      const cJSON *v;
      cJSON_ArrayForEach(v, cJSON_GetObjectItem(r, "vulnerabilities")) {
        const char *cve = jo_sv(v, "cve");
        if (cve) cJSON_AddItemToArray(cves, cJSON_CreateString(cve));
      }
      if (cJSON_GetArraySize(cves) > 0) cJSON_AddItemToObject(p, "aliases", cves);
      else cJSON_Delete(cves);
    }
    if (csaf) {
      const char *sevtxt = cyi_str(csaf, "aggregate_severity.text");
      const char *pub = cyi_str(csaf, "publisher.name");
      if (sevtxt) cJSON_AddStringToObject(p, "aggregate_severity", sevtxt);
      if (pub) cJSON_AddStringToObject(p, "publisher", pub);
    }

    /* affected: OSV affected[].package.name; CVE 5 affected[].vendor/product */
    cJSON *pkgs = cJSON_CreateArray();
    const cJSON *aff;
    cJSON_ArrayForEach(aff, cJSON_GetObjectItem(r, "affected")) {
      const cJSON *pkg = cJSON_GetObjectItem(aff, "package");
      const char *pn = cJSON_IsObject(pkg) ? jo_sv(pkg, "name") : NULL;
      if (pn) cJSON_AddItemToArray(pkgs, cJSON_CreateString(pn));
    }
    cJSON_ArrayForEach(aff, cJSON_GetObjectItem(cna, "affected")) {
      const char *vendor = jo_sv(aff, "vendor"), *product = jo_sv(aff, "product");
      if (!vendor && !product) continue;
      char vp[256];
      snprintf(vp, sizeof vp, "%s%s%s", vendor ? vendor : "",
               (vendor && product) ? " " : "", product ? product : "");
      cJSON_AddItemToArray(pkgs, cJSON_CreateString(vp));
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

    /* references[].url from whichever container the format uses */
    const char *link = NULL;
    cJSON *refs = cJSON_CreateArray();
    link = cyi_urls(cJSON_GetObjectItem(r, "references"), refs);
    if (!link) link = cyi_urls(cJSON_GetObjectItem(cna, "references"), refs);
    if (!link && csaf) link = cyi_urls(cJSON_GetObjectItem(csaf, "references"), refs);
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
  fprintf(stderr, "[circl-cve-last] emitted %d (%d record(s) carried no OSV id, "
                  "cveMetadata.cveId or CSAF tracking.id)\n", n, skipped);
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
