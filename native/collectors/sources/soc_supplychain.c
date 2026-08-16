/* soc_supplychain.c — archive, cross-registry and brand-monitoring pivots.
 * Four on-demand services, all keyless; none returns coordinates, so has_geo
 * stays 0 (R2).
 *
 *  SOFTWARE_HERITAGE_ORIGIN (keyword)
 *      GET https://archive.softwareheritage.org/api/1/origin/search/<kw>/?limit=20
 *      Bare array. Rows with a falsy has_visits are indexed-but-never-crawled
 *      and are NOT emitted as archived.
 *      emits: origin url, visit_types, nb_visits, snapshot_id (the SWHID root),
 *             last_visit_date, last_eventful_visit_date, origin_visits_url
 *      licence: UNESCO-backed public archive (Inria); search/browse APIs are
 *               keyless. Archived content keeps its original licences.
 *  ECOSYSTEMS_PACKAGE (keyword = package name)
 *      GET https://packages.ecosyste.ms/api/v1/packages/lookup?ecosystem=&name=
 *      Tried against npm, pypi, cargo and go; the lookup returns an ARRAY even
 *      for a single match.
 *      emits: name, ecosystem, description, homepage, licenses,
 *             normalized_licenses, repository_url, keywords_array,
 *             versions_count, first_release_published_at,
 *             latest_release_published_at, latest_release_number
 *      licence: ecosyste.ms is an Open Source Collective funded public-good
 *               service; keyless and CC-BY. Contact User-Agent sent.
 *  ECOSYSTEMS_REPO (repo URL or owner/repo)
 *      GET https://repos.ecosyste.ms/api/v1/repositories/lookup?url=
 *      Returns a SINGLE object; a 404 for an unknown repo is a zero-row
 *      success, not an error (R3).
 *      emits: full_name, owner, description, archived, fork, pushed_at,
 *             stargazers_count, forks_count, subscribers_count,
 *             open_issues_count, language, license, default_branch,
 *             last_synced_at
 *      licence: ecosyste.ms public-good API; keyless, CC-BY.
 *  DNSTWISTER_TYPOSQUAT (domain)
 *      GET https://dnstwister.report/api/fuzz/<domain hex-encoded ASCII>
 *      TRAP: the domain MUST be hex-encoded in the path
 *      (www.openai.com → 7777772e6f70656e61692e636f6d); the plain form does
 *      not work. ~365 permutations come back per domain, so instead of firing
 *      365 follow-up HTTP calls we resolve the candidates locally with
 *      getaddrinfo (permitted local computation) and emit ONLY the ones that
 *      actually resolve, carrying the resolved address. Candidates that do not
 *      resolve produce no row.
 *      emits: permutation domain, fuzzer that produced it, resolved IP,
 *             address family, the base domain
 *      licence: dnstwister is an open-source (Unlicense) free service with a
 *               documented public API and an explicit no-key policy. Read-only
 *               analysis of domain names; no personal data.
 */
#include "lib/osintemit.h"
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const SOC_UA[] = {
  "User-Agent: JapanOSINT-native/1.0 (OSINT research collector; +https://github.com/)",
  "Accept: application/json", NULL
};

static int looks_like_domain(const char *s) {
  if (!s || !*s) return 0;
  if (strchr(s, '@') || strchr(s, '/') || strchr(s, ' ') || strchr(s, ':')) return 0;
  size_t n = strlen(s);
  if (n < 4 || n > 253) return 0;
  const char *dot = strrchr(s, '.');
  if (!dot || dot == s || !dot[1]) return 0;
  for (const char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (!(isalnum(c) || c == '-' || c == '.')) return 0;
  }
  return isalpha((unsigned char)dot[1]) ? 1 : 0;
}

/* ---- SOFTWARE_HERITAGE_ORIGIN ------------------------------------------- */

static int run_swh(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[400];
  snprintf(url, sizeof url,
           "https://archive.softwareheritage.org/api/1/origin/search/%s/?limit=20",
           enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 25000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[SOFTWARE_HERITAGE_ORIGIN] no usable response for %s\n", q);
    cJSON_Delete(doc); return 0;
  }
  int n = 0;
  const cJSON *o;
  cJSON_ArrayForEach(o, doc) {
    const char *ourl = jo_sv(o, "url");
    if (!ourl) continue;
    /* indexed but never crawled → not archived; do not claim otherwise */
    if (!cJSON_IsTrue(cJSON_GetObjectItem(o, "has_visits"))) continue;
    const cJSON *nv = cJSON_GetObjectItem(o, "nb_visits");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "SOFTWARE_HERITAGE_ORIGIN");
    cJSON_AddStringToObject(p, "source", "archive.softwareheritage.org");
    cJSON_AddStringToObject(p, "entity", q);
    jcopy(p, "origin_url", o, "url");
    jcopy(p, "visit_types", o, "visit_types");
    jcopy(p, "nb_visits", o, "nb_visits");
    jcopy(p, "snapshot_id", o, "snapshot_id");
    jcopy(p, "last_visit_date", o, "last_visit_date");
    jcopy(p, "last_eventful_visit_date", o, "last_eventful_visit_date");
    jcopy(p, "origin_visits_url", o, "origin_visits_url");
    cJSON_AddBoolToObject(p, "success", 1);
    char link[700], summary[300];
    char *oenc = soc_urlenc(ourl);
    if (oenc) {
      snprintf(link, sizeof link,
               "https://archive.softwareheritage.org/browse/origin/directory/?origin_url=%s",
               oenc);
      free(oenc);
    } else snprintf(link, sizeof link, "%s", ourl);
    snprintf(summary, sizeof summary, "%ld archived visits · last %s",
             cJSON_IsNumber(nv) ? (long)nv->valuedouble : 0L,
             jo_sv(o, "last_visit_date") ? jo_sv(o, "last_visit_date") : "?");
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"SOFTWARE_HERITAGE_ORIGIN\",\"archive\"]",
                  ourl, ourl, summary, link, jo_sv(o, "last_visit_date"), NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[SOFTWARE_HERITAGE_ORIGIN] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_swh_def = {
  .id = "SOFTWARE_HERITAGE_ORIGIN", .collector = "osint",
  .name = "Software Heritage Origin Search",
  .update_interval_sec = 0, .run = run_swh,
  .category = "social", .type = "api",
  .url = "https://archive.softwareheritage.org/api/1/origin/search/",
  .description = "The universal source-code archive — permanent content-addressed copies of public repositories including ones since deleted upstream, with crawl history and snapshot IDs.",
  .license = "UNESCO-backed public archive (Inria); search/browse APIs keyless. Archived content keeps its original licences.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_swh_def)

/* ---- ECOSYSTEMS_PACKAGE -------------------------------------------------- */

static int ecosystems_emit(intel_sink *sink, const cJSON *pk, const char *q) {
  const char *name = jo_sv(pk, "name");
  const char *eco  = jo_sv(pk, "ecosystem");
  if (!name || !eco) return 0;
  cJSON *p = cJSON_CreateObject();
  if (!p) return 0;
  cJSON_AddStringToObject(p, "service", "ECOSYSTEMS_PACKAGE");
  cJSON_AddStringToObject(p, "source", "packages.ecosyste.ms");
  cJSON_AddStringToObject(p, "entity", q);
  jcopy(p, "name", pk, "name");
  jcopy(p, "ecosystem", pk, "ecosystem");
  jcopy(p, "description", pk, "description");
  jcopy(p, "homepage", pk, "homepage");
  jcopy(p, "licenses", pk, "licenses");
  jcopy(p, "normalized_licenses", pk, "normalized_licenses");
  jcopy(p, "repository_url", pk, "repository_url");
  jcopy(p, "keywords_array", pk, "keywords_array");
  jcopy(p, "versions_count", pk, "versions_count");
  jcopy(p, "first_release_published_at", pk, "first_release_published_at");
  jcopy(p, "latest_release_published_at", pk, "latest_release_published_at");
  jcopy(p, "latest_release_number", pk, "latest_release_number");
  jcopy(p, "registry_url", pk, "registry_url");
  cJSON_AddBoolToObject(p, "success", 1);
  char title[300], rk[300], summary[400];
  snprintf(title, sizeof title, "%s (%s) %s", name, eco,
           jo_sv(pk, "latest_release_number") ? jo_sv(pk, "latest_release_number") : "");
  snprintf(rk, sizeof rk, "ecosystems:%s:%s", eco, name);
  /* first_release_published_at is the strongest single typosquat signal */
  snprintf(summary, sizeof summary, "first released %s · latest %s · %.150s",
           jo_sv(pk, "first_release_published_at")
             ? jo_sv(pk, "first_release_published_at") : "?",
           jo_sv(pk, "latest_release_published_at")
             ? jo_sv(pk, "latest_release_published_at") : "?",
           jo_sv(pk, "description") ? jo_sv(pk, "description") : "");
  return soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"ECOSYSTEMS_PACKAGE\",\"packages\"]",
                  rk, title, summary,
                  jo_sv(pk, "registry_url") ? jo_sv(pk, "registry_url")
                                          : jo_sv(pk, "repository_url"),
                  jo_sv(pk, "latest_release_published_at"), NULL);
}

static int run_ecosystems_pkg(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  for (const char *p = q; *p; p++) if (isspace((unsigned char)*p)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  static const char *const ecos[] = { "npm", "pypi", "cargo", "go", NULL };
  int n = 0;
  for (int i = 0; ecos[i]; i++) {
    char url[500];
    snprintf(url, sizeof url,
             "https://packages.ecosyste.ms/api/v1/packages/lookup?ecosystem=%s&name=%s",
             ecos[i], enc);
    cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
    if (!doc) continue;
    if (cJSON_IsArray(doc)) {            /* lookup returns an array even for 1 */
      const cJSON *pk;
      cJSON_ArrayForEach(pk, doc) n += ecosystems_emit(sink, pk, q);
    } else if (cJSON_IsObject(doc)) {
      n += ecosystems_emit(sink, doc, q);
    }
    cJSON_Delete(doc);
  }
  free(enc);
  fprintf(stderr, "[ECOSYSTEMS_PACKAGE] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_ecosystems_pkg_def = {
  .id = "ECOSYSTEMS_PACKAGE", .collector = "osint",
  .name = "ecosyste.ms Package Lookup",
  .update_interval_sec = 0, .run = run_ecosystems_pkg,
  .category = "social", .type = "api",
  .url = "https://packages.ecosyste.ms/api/v1/packages/lookup",
  .description = "One normalised schema across every package registry — what a package is, who publishes it, where it lives and when it first and last shipped, regardless of ecosystem.",
  .license = "ecosyste.ms Open Source Collective public-good service; keyless, CC-BY.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_ecosystems_pkg_def)

/* ---- ECOSYSTEMS_REPO ----------------------------------------------------- */

static int run_ecosystems_repo(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  /* Accept a full forge URL, or "owner/repo" which we resolve on GitHub. */
  char target[400];
  if (!strncmp(q, "http://", 7) || !strncmp(q, "https://", 8)) {
    snprintf(target, sizeof target, "%s", q);
  } else {
    const char *slash = strchr(q, '/');
    if (!slash || slash == q || !slash[1] || strchr(slash + 1, '/')) return 0;
    snprintf(target, sizeof target, "https://github.com/%s", q);
  }
  char *enc = soc_urlenc(target);
  if (!enc) return 0;
  char url[900];
  snprintf(url, sizeof url,
           "https://repos.ecosyste.ms/api/v1/repositories/lookup?url=%s", enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!cJSON_IsObject(doc)) {          /* 404 for an unknown repo → 0 rows */
    fprintf(stderr, "[ECOSYSTEMS_REPO] no repository record for %s\n", target);
    cJSON_Delete(doc); return 0;
  }
  const char *full = jo_sv(doc, "full_name");
  int n = 0;
  if (full) {
    const cJSON *st = cJSON_GetObjectItem(doc, "stargazers_count");
    cJSON *p = cJSON_CreateObject();
    if (p) {
      cJSON_AddStringToObject(p, "service", "ECOSYSTEMS_REPO");
      cJSON_AddStringToObject(p, "source", "repos.ecosyste.ms");
      cJSON_AddStringToObject(p, "entity", q);
      cJSON_AddStringToObject(p, "lookup_url", target);
      jcopy(p, "full_name", doc, "full_name");
      jcopy(p, "owner", doc, "owner");
      jcopy(p, "description", doc, "description");
      jcopy(p, "archived", doc, "archived");
      jcopy(p, "fork", doc, "fork");
      jcopy(p, "pushed_at", doc, "pushed_at");
      jcopy(p, "created_at", doc, "created_at");
      jcopy(p, "stargazers_count", doc, "stargazers_count");
      jcopy(p, "forks_count", doc, "forks_count");
      jcopy(p, "subscribers_count", doc, "subscribers_count");
      jcopy(p, "open_issues_count", doc, "open_issues_count");
      jcopy(p, "language", doc, "language");
      jcopy(p, "license", doc, "license");
      jcopy(p, "default_branch", doc, "default_branch");
      jcopy(p, "last_synced_at", doc, "last_synced_at");
      jcopy(p, "html_url", doc, "html_url");
      cJSON_AddBoolToObject(p, "success", 1);
      char summary[400];
      snprintf(summary, sizeof summary, "%ld stars · %s · pushed %s%s",
               cJSON_IsNumber(st) ? (long)st->valuedouble : 0L,
               jo_sv(doc, "language") ? jo_sv(doc, "language") : "?",
               jo_sv(doc, "pushed_at") ? jo_sv(doc, "pushed_at") : "?",
               cJSON_IsTrue(cJSON_GetObjectItem(doc, "archived")) ? " · ARCHIVED" : "");
      n = soc_emit(sink, p, "osint_service_result",
                   "[\"osint-search\",\"ECOSYSTEMS_REPO\",\"forge\"]",
                   full, full, summary,
                   jo_sv(doc, "html_url") ? jo_sv(doc, "html_url") : target,
                   jo_sv(doc, "pushed_at"), NULL);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[ECOSYSTEMS_REPO] emitted %d for %s\n", n, target);
  return 0;
}

static const source_def soc_ecosystems_repo_def = {
  .id = "ECOSYSTEMS_REPO", .collector = "osint",
  .name = "ecosyste.ms Repository Lookup",
  .update_interval_sec = 0, .run = run_ecosystems_repo,
  .category = "social", .type = "api",
  .url = "https://repos.ecosyste.ms/api/v1/repositories/lookup",
  .description = "Repository metadata for any forge URL — GitHub, GitLab, Gitea, Codeberg, SourceHut — through one endpoint: stars, language, licence, archive status and last push.",
  .license = "ecosyste.ms public-good API; keyless, CC-BY.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_ecosystems_repo_def)

/* ---- DNSTWISTER_TYPOSQUAT ------------------------------------------------ */

/* Resolve `host`; on success copy the first address into ip and its family
 * into *fam. Returns 1 when the name resolves, 0 otherwise. */
static int resolve_one(const char *host, char *ip, size_t ipn, const char **fam) {
  struct addrinfo hints, *res = NULL;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) return 0;
  int ok = 0;
  for (struct addrinfo *a = res; a && !ok; a = a->ai_next) {
    if (a->ai_family == AF_INET) {
      struct sockaddr_in *s = (struct sockaddr_in *)a->ai_addr;
      if (inet_ntop(AF_INET, &s->sin_addr, ip, (socklen_t)ipn)) { *fam = "A"; ok = 1; }
    } else if (a->ai_family == AF_INET6) {
      struct sockaddr_in6 *s = (struct sockaddr_in6 *)a->ai_addr;
      if (inet_ntop(AF_INET6, &s->sin6_addr, ip, (socklen_t)ipn)) { *fam = "AAAA"; ok = 1; }
    }
  }
  freeaddrinfo(res);
  return ok;
}

static int run_dnstwister(const source_ctx *ctx, intel_sink *sink) {
  const char *dom = ctx->entity;
  if (!looks_like_domain(dom)) return 0;
  /* The API path takes the domain as hex-encoded ASCII, not the plain form. */
  size_t dl = strlen(dom);
  char hex[520];
  if (dl * 2 + 1 > sizeof hex) return 0;
  static const char hx[] = "0123456789abcdef";
  for (size_t i = 0; i < dl; i++) {
    hex[i * 2]     = hx[(unsigned char)dom[i] >> 4];
    hex[i * 2 + 1] = hx[(unsigned char)dom[i] & 15];
  }
  hex[dl * 2] = 0;

  char url[600];
  snprintf(url, sizeof url, "https://dnstwister.report/api/fuzz/%s", hex);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 30000);
  if (!doc) {
    fprintf(stderr, "[DNSTWISTER_TYPOSQUAT] no response for %s\n", dom);
    return 0;
  }
  const cJSON *fz = cJSON_GetObjectItem(doc, "fuzzy_domains");
  int n = 0, checked = 0;
  const cJSON *f;
  if (cJSON_IsArray(fz)) cJSON_ArrayForEach(f, fz) {
    if (checked >= 120) break;         /* bound the resolver work per pivot */
    const char *cand = jo_sv(f, "domain");
    if (!cand || !strcmp(cand, dom)) continue;
    checked++;
    char ip[64]; const char *fam = NULL;
    if (!resolve_one(cand, ip, sizeof ip, &fam)) continue;  /* no A/AAAA → no row */
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "DNSTWISTER_TYPOSQUAT");
    cJSON_AddStringToObject(p, "source", "dnstwister.report + local DNS");
    cJSON_AddStringToObject(p, "entity", dom);
    cJSON_AddStringToObject(p, "base_domain", dom);
    jcopy(p, "domain", f, "domain");
    jcopy(p, "fuzzer", f, "fuzzer");
    cJSON_AddStringToObject(p, "resolved_ip", ip);
    if (fam) cJSON_AddStringToObject(p, "family", fam);
    cJSON_AddBoolToObject(p, "success", 1);
    char title[300], summary[300], link[400];
    snprintf(title, sizeof title, "%s -> %s", cand, ip);
    snprintf(summary, sizeof summary, "typosquat of %s (%s) resolving to %s",
             dom, jo_sv(f, "fuzzer") ? jo_sv(f, "fuzzer") : "permutation", ip);
    snprintf(link, sizeof link, "https://dnstwister.report/search/%s", cand);
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"DNSTWISTER_TYPOSQUAT\",\"domain\"]",
                  cand, title, summary, link, NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[DNSTWISTER_TYPOSQUAT] %d of %d permutations resolve for %s\n",
          n, checked, dom);
  return 0;
}

static const source_def soc_dnstwister_def = {
  .id = "DNSTWISTER_TYPOSQUAT", .collector = "osint",
  .name = "dnstwister Typosquat Permutation Check",
  .update_interval_sec = 0, .run = run_dnstwister,
  .category = "cyber", .type = "api",
  .url = "https://dnstwister.report/api/fuzz/",
  .description = "Generates the typosquat permutation set for a domain (homoglyph, bitsquat, omission, insertion, TLD swap) and emits only the candidates that actually resolve, with the address they point at.",
  .license = "dnstwister: open-source (Unlicense) free service, documented public API, no key required. Read-only analysis of domain names.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_dnstwister_def)
