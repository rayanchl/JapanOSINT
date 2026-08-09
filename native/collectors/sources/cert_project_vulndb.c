/* cert_project_vulndb.c — three per-project vulnerability databases that
 * publish machine-readable JSON directly. One table-driven run(), three
 * source_defs.
 *
 *   kubernetes-official-cve-feed
 *       https://kubernetes.io/docs/reference/issues-security/official-cve-feed/index.json
 *       JSON Feed (doc.items[]). The authoritative, auto-refreshed k8s CVE
 *       list. The CVSS vector and rating are embedded in items[].content_text
 *       markdown, so the vector is lifted out of the fetched text (local
 *       computation over fetched bytes — no lookup URL is constructed).
 *       Emits: id, title, content_text, url, date_published, the
 *       kubernetes/kubernetes issue number and the CVSS vector.
 *
 *   curl-vulnerability-json  https://curl.se/docs/vuln.json
 *       Bare array of OSV records: every curl/libcurl vulnerability ever
 *       published, with CWE mapping, severity, affected version ranges and the
 *       originating report. curl is one of the most heavily embedded libraries
 *       in existence, so this is a high-reach dependency feed.
 *       Emits: id, aliases, summary, modified/published,
 *       database_specific.{severity,CWE,last_affected,www}.
 *
 *   go-vulndb-index          https://vuln.go.dev/index/vulns.json
 *       Bare array of {id, modified, aliases}: the Go team's advisory index
 *       with CVE and GHSA cross-references. Per-advisory detail lives at
 *       /ID/<id>.json — this collector deliberately does NOT fan out to
 *       thousands of detail fetches; the index itself is the alias map.
 *
 * R1: every field emitted came out of the response body.
 * R2: no geometry — none of the three publishes a coordinate, and a software
 * flaw has no location.
 * R3: fetch/parse failure -> -1; a successful fetch with nothing in it -> 0.
 *
 * All keyless. Licences: Kubernetes docs CC BY 4.0; curl docs under the curl
 * (MIT-like) licence; the Go vulnerability database is published by the Go team
 * for public reuse. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define K8S_URL  "https://kubernetes.io/docs/reference/issues-security/official-cve-feed/index.json"
#define CURL_URL "https://curl.se/docs/vuln.json"
#define GO_URL   "https://vuln.go.dev/index/vulns.json"

#define GO_MAX_ROWS 5000

static void join_strings(cJSON *arr, char *out, size_t n) {
  out[0] = 0;
  if (!cJSON_IsArray(arr)) return;
  size_t j = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    if (!cJSON_IsString(e) || !e->valuestring || !e->valuestring[0]) continue;
    const char *v = e->valuestring;
    if (j && j + 2 < n) { out[j++] = ','; out[j++] = ' '; }
    while (*v && j + 1 < n) out[j++] = *v++;
  }
  out[j] = 0;
}

/* ---- kubernetes-official-cve-feed ----------------------------------------- */

/* Lift the CVSS vector out of the fetched markdown body. */
static void find_cvss(const char *txt, char *out, size_t n) {
  out[0] = 0;
  if (!txt) return;
  const char *p = strstr(txt, "CVSS:");
  if (!p) return;
  size_t i = 0;
  while (p[i] && !isspace((unsigned char)p[i]) && p[i] != ')' && p[i] != ',' &&
         i + 1 < n) { out[i] = p[i]; i++; }
  out[i] = 0;
}

static int k8s_run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, K8S_URL, hdrs, 25000);
  if (!doc) { fprintf(stderr, "[kubernetes-official-cve-feed] fetch/parse failed\n"); return -1; }
  cJSON *items = cJSON_GetObjectItem(doc, "items");
  int n = 0;
  if (cJSON_IsArray(items)) {
    cJSON *e;
    cJSON_ArrayForEach(e, items) {
      const char *id  = jo_sv(e, "id");
      const char *ttl = jo_sv(e, "title");
      if (!id && !ttl) continue;              /* nothing fetched -> no row (R1) */
      const char *txt  = jo_sv(e, "content_text");
      const char *url  = jo_sv(e, "url");
      const char *pub  = jo_sv(e, "date_published");

      char title[600];
      snprintf(title, sizeof title, "%s", ttl ? ttl : id);
      jo_utf8_trunc(title, 320);

      char vec[128];
      find_cvss(txt, vec, sizeof vec);

      cJSON *p = cJSON_CreateObject();
      if (id) cJSON_AddStringToObject(p, "cve_id", id);
      if (vec[0]) cJSON_AddStringToObject(p, "cvss_vector", vec);
      if (pub) cJSON_AddStringToObject(p, "date_published", pub);
      cJSON *k = cJSON_GetObjectItem(e, "_kubernetes_io");
      if (k) {
        cJSON_AddItemToObject(p, "kubernetes_io", cJSON_Duplicate(k, 1));
        const char *issue = jo_sv(k, "issue_number");
        if (issue) cJSON_AddStringToObject(p, "issue_number", issue);
      }
      const char *ext = jo_sv(e, "external_url");
      if (ext) cJSON_AddStringToObject(p, "external_url", ext);
      cJSON_AddStringToObject(p, "source", "kubernetes_official_cve_feed");
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      intel_item it = {0};
      it.remote_key      = id ? id : ttl;
      it.title           = title;
      it.body            = txt;
      it.summary         = txt;
      it.link            = url;
      it.lang            = "en";
      it.published_at    = jo_looks_iso(pub) ? pub : NULL;
      it.record_type     = "vulnerability";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"vulnerability\",\"cyber\",\"kubernetes\",\"cloud-native\"]";
      if (s->emit(s, &it) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[kubernetes-official-cve-feed] emitted %d\n", n);
  return 0;
}

/* ---- curl-vulnerability-json ---------------------------------------------- */

static int curl_run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, CURL_URL, hdrs, 30000);
  if (!doc) { fprintf(stderr, "[curl-vulnerability-json] fetch/parse failed\n"); return -1; }
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "vulns");
  int n = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *e;
    cJSON_ArrayForEach(e, arr) {
      const char *id = jo_sv(e, "id");
      if (!id) continue;                      /* no OSV id -> no row (R1) */
      const char *summary = jo_sv(e, "summary");
      const char *details = jo_sv(e, "details");
      const char *pub     = jo_sv(e, "published");
      if (!pub) pub = jo_sv(e, "modified");

      char aliases[256];
      join_strings(cJSON_GetObjectItem(e, "aliases"), aliases, sizeof aliases);

      char title[600];
      if (summary) snprintf(title, sizeof title, "%s: %s", id, summary);
      else         snprintf(title, sizeof title, "%s", id);
      jo_utf8_trunc(title, 320);

      cJSON *ds = cJSON_GetObjectItem(e, "database_specific");
      const char *link = ds ? jo_sv(ds, "www") : NULL;
      if (!link && ds) link = jo_sv(ds, "URL");
      if (!link) {
        cJSON *refs = cJSON_GetObjectItem(e, "references");
        if (cJSON_IsArray(refs)) {
          cJSON *rf;
          cJSON_ArrayForEach(rf, refs) { const char *u = jo_sv(rf, "url");
                                         if (u) { link = u; break; } }
        }
      }

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "advisory_id", id);
      if (aliases[0]) cJSON_AddStringToObject(p, "aliases", aliases);
      const char *v;
      if ((v = jo_sv(e, "modified")))  cJSON_AddStringToObject(p, "modified", v);
      if ((v = jo_sv(e, "published"))) cJSON_AddStringToObject(p, "published", v);
      if (ds) {
        if ((v = jo_sv(ds, "severity")))      cJSON_AddStringToObject(p, "severity", v);
        if ((v = jo_sv(ds, "CWE")))           cJSON_AddStringToObject(p, "cwe", v);
        if ((v = jo_sv(ds, "last_affected"))) cJSON_AddStringToObject(p, "last_affected", v);
        if ((v = jo_sv(ds, "package")))       cJSON_AddStringToObject(p, "package", v);
        cJSON *cwe = cJSON_GetObjectItem(ds, "CWE");
        if (cwe && cJSON_IsObject(cwe))
          cJSON_AddItemToObject(p, "cwe_detail", cJSON_Duplicate(cwe, 1));
      }
      cJSON *aff = cJSON_GetObjectItem(e, "affected");
      if (cJSON_IsArray(aff))
        cJSON_AddNumberToObject(p, "affected_ranges", cJSON_GetArraySize(aff));
      cJSON_AddStringToObject(p, "source", "curl_vuln_json");
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
      it.tags_json       = "[\"vulnerability\",\"cyber\",\"curl\",\"osv\"]";
      if (s->emit(s, &it) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[curl-vulnerability-json] emitted %d\n", n);
  return 0;
}

/* ---- go-vulndb-index ------------------------------------------------------ */

static int go_run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, GO_URL, hdrs, 30000);
  if (!doc) { fprintf(stderr, "[go-vulndb-index] fetch/parse failed\n"); return -1; }
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "vulns");
  int n = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *e;
    cJSON_ArrayForEach(e, arr) {
      if (n >= GO_MAX_ROWS) break;
      const char *id = jo_sv(e, "id");
      if (!id) continue;                      /* no GO- id -> no row (R1) */
      const char *mod = jo_sv(e, "modified");

      char aliases[512];
      join_strings(cJSON_GetObjectItem(e, "aliases"), aliases, sizeof aliases);

      char title[700];
      if (aliases[0]) snprintf(title, sizeof title, "%s (%s)", id, aliases);
      else            snprintf(title, sizeof title, "%s", id);
      jo_utf8_trunc(title, 320);

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "advisory_id", id);
      if (mod) cJSON_AddStringToObject(p, "modified", mod);
      cJSON *al = cJSON_GetObjectItem(e, "aliases");
      if (cJSON_IsArray(al)) cJSON_AddItemToObject(p, "aliases", cJSON_Duplicate(al, 1));
      cJSON_AddStringToObject(p, "source", "go_vulndb_index");
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      intel_item it = {0};
      it.remote_key      = id;
      it.title           = title;
      it.body            = aliases[0] ? aliases : NULL;
      it.summary         = aliases[0] ? aliases : NULL;
      it.lang            = "en";
      it.published_at    = jo_looks_iso(mod) ? mod : NULL;
      it.record_type     = "vulnerability";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"vulnerability\",\"cyber\",\"golang\",\"osv\"]";
      if (s->emit(s, &it) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[go-vulndb-index] emitted %d\n", n);
  return 0;
}

static int run(const source_ctx *c, intel_sink *s) {
  if (!c || !c->source_id) return -1;
  if (!strcmp(c->source_id, "kubernetes-official-cve-feed")) return k8s_run(c, s);
  if (!strcmp(c->source_id, "curl-vulnerability-json"))      return curl_run(c, s);
  if (!strcmp(c->source_id, "go-vulndb-index"))              return go_run(c, s);
  fprintf(stderr, "[cert_project_vulndb] unknown source id %s\n", c->source_id);
  return -1;
}

static const source_def cert_k8s_cve_def = {
  .id = "kubernetes-official-cve-feed", .collector = "cyber",
  .name = "Kubernetes Official CVE Feed",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "api", .url = K8S_URL,
  .description = "The Kubernetes project's auto-refreshed official CVE list "
                 "(JSON Feed) with CVSS vector, rating and the "
                 "kubernetes/kubernetes issue number for each entry.",
  .license = "CC BY 4.0 (Kubernetes documentation).",
  .free_tier = 1 };
REGISTER_SOURCE(cert_k8s_cve_def)

static const source_def cert_curl_vuln_def = {
  .id = "curl-vulnerability-json", .collector = "cyber",
  .name = "curl Project Vulnerability Database (OSV format)",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "dataset", .url = CURL_URL,
  .description = "Every curl/libcurl vulnerability ever published, in OSV "
                 "schema with CWE mapping, severity, affected version ranges "
                 "and the originating report - a model per-project database "
                 "for a library embedded almost everywhere.",
  .license = "curl project docs are published under the curl licence "
             "(MIT-like); free reuse.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_curl_vuln_def)

static const source_def cert_go_vulndb_def = {
  .id = "go-vulndb-index", .collector = "cyber",
  .name = "Go Vulnerability Database Index",
  .update_interval_sec = 43200, .run = run,
  .category = "cyber", .type = "dataset", .url = GO_URL,
  .description = "The Go team's vulnerability database index: GO-* advisories "
                 "with CVE and GHSA aliases and modification timestamps - the "
                 "cross-ecosystem alias map for Go module vulnerabilities.",
  .license = "Go vulnerability database, published by the Go team for public "
             "reuse.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_go_vulndb_def)
