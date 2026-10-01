/* collectors/sources/packages_world.c
 * OSINT services — net-new keyless software supply-chain / package-registry
 * search, pivoting on ctx->entity (a package name or author). Shared run()
 * switches on ctx->source_id. Real fetch or honest-empty. No API keys.
 *
 *   NPM_REGISTRY   registry.npmjs.org/-/v1/search?text=<e>        (JSON)
 *   PYPI_PROJECT   pypi.org/pypi/<e>/json                          (JSON, exact)
 *   CRATES_IO      crates.io/api/v1/crates?q=<e>                   (JSON)
 *   RUBYGEMS       rubygems.org/api/v1/search.json?query=<e>       (JSON)
 *   PACKAGIST      packagist.org/search.json?q=<e>                 (JSON)
 *   DOCKERHUB      hub.docker.com/v2/search/repositories?query=<e> (JSON) */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* crates.io / npm require a descriptive UA or they 403. */
static const char *PK_UA = "User-Agent: JapanOSINT/1.0 (osint-supplychain; osint@example.org)";

static int pk_emit(intel_sink *sink, const char *service, const char *rtype,
                   const char *rk, const char *title, const char *summary,
                   const char *link, cJSON *body, cJSON *props) {
  char *bj = body ? cJSON_PrintUnformatted(body) : NULL;
  if (props) { cJSON_AddStringToObject(props, "service", service); cJSON_AddBoolToObject(props, "success", 1); }
  char *pj = props ? cJSON_PrintUnformatted(props) : NULL;
  char tags[96]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.summary = summary;
  it.body = bj; it.link = (link && link[0]) ? link : NULL; it.lang = "en";
  it.record_type = rtype; it.properties_json = pj; it.tags_json = tags;
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  if (body) cJSON_Delete(body);
  if (props) cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run_npm(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://registry.npmjs.org/-/v1/search?text=%s&size=20", enc);
  const char *hdrs[] = { "Accept: application/json", PK_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "NPM_REGISTRY");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *objs = cJSON_GetObjectItem(root, "objects");
  int emitted = 0;
  if (cJSON_IsArray(objs)) {
    const cJSON *o;
    cJSON_ArrayForEach(o, objs) {
      const cJSON *pkg = cJSON_GetObjectItem(o, "package");
      if (!pkg) continue;
      const char *name = jo_sv(pkg, "name");
      const char *ver = jo_sv(pkg, "version");
      const char *desc = jo_sv(pkg, "description");
      if (!name) continue;
      const cJSON *pub = cJSON_GetObjectItem(pkg, "publisher");
      const char *author = pub ? jo_sv(pub, "username") : NULL;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "name", name);
      if (ver) cJSON_AddStringToObject(d, "version", ver);
      if (desc) cJSON_AddStringToObject(d, "description", desc);
      if (author) cJSON_AddStringToObject(d, "publisher", author);
      cJSON_AddStringToObject(d, "source", "npm");
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "package", name);
      char link[300]; snprintf(link, sizeof link, "https://www.npmjs.com/package/%s", name);
      emitted += pk_emit(sink, "NPM_REGISTRY", "npm-package", name, name, desc ? desc : author, link, d, p);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_pypi(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://pypi.org/pypi/%s/json", enc);
  const char *hdrs[] = { "Accept: application/json", PK_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "PYPI_PROJECT");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *info = cJSON_GetObjectItem(root, "info");
  if (!info) { cJSON_Delete(root); return 0; }
  const char *name = jo_sv(info, "name");
  const char *ver = jo_sv(info, "version");
  const char *summary = jo_sv(info, "summary");
  const char *author = jo_sv(info, "author");
  const char *home = jo_sv(info, "home_page");
  const char *pkgurl = jo_sv(info, "package_url");
  if (!name) { cJSON_Delete(root); return 0; }
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "name", name);
  if (ver) cJSON_AddStringToObject(d, "version", ver);
  if (summary) cJSON_AddStringToObject(d, "summary", summary);
  if (author) cJSON_AddStringToObject(d, "author", author);
  if (home) cJSON_AddStringToObject(d, "home_page", home);
  cJSON_AddStringToObject(d, "source", "PyPI");
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "package", name);
  char link[300];
  if (pkgurl) snprintf(link, sizeof link, "%s", pkgurl);
  else snprintf(link, sizeof link, "https://pypi.org/project/%s/", name);
  int rc = pk_emit(sink, "PYPI_PROJECT", "pypi-package", name, name, summary ? summary : author, link, d, p);
  cJSON_Delete(root);
  return rc;
}

static int run_crates(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://crates.io/api/v1/crates?q=%s&per_page=20", enc);
  const char *hdrs[] = { "Accept: application/json", PK_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "CRATES_IO");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *crates = cJSON_GetObjectItem(root, "crates");
  int emitted = 0;
  if (cJSON_IsArray(crates)) {
    const cJSON *c;
    cJSON_ArrayForEach(c, crates) {
      const char *id = jo_sv(c, "id");
      const char *desc = jo_sv(c, "description");
      const char *repo = jo_sv(c, "repository");
      const cJSON *dl = cJSON_GetObjectItem(c, "downloads");
      if (!id) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "name", id);
      if (desc) cJSON_AddStringToObject(d, "description", desc);
      if (repo) cJSON_AddStringToObject(d, "repository", repo);
      if (dl && cJSON_IsNumber(dl)) cJSON_AddNumberToObject(d, "downloads", dl->valuedouble);
      cJSON_AddStringToObject(d, "source", "crates.io");
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "crate", id);
      char link[300]; snprintf(link, sizeof link, "https://crates.io/crates/%s", id);
      emitted += pk_emit(sink, "CRATES_IO", "rust-crate", id, id, desc, link, d, p);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_rubygems(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://rubygems.org/api/v1/search.json?query=%s", enc);
  const char *hdrs[] = { "Accept: application/json", PK_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "RUBYGEMS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!cJSON_IsArray(root)) { if (root) cJSON_Delete(root); return 0; }
  int emitted = 0;
  const cJSON *g;
  cJSON_ArrayForEach(g, root) {
    const char *name = jo_sv(g, "name");
    const char *info = jo_sv(g, "info");
    const char *proj = jo_sv(g, "project_uri");
    const cJSON *dl = cJSON_GetObjectItem(g, "downloads");
    if (!name) continue;
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "name", name);
    if (info) cJSON_AddStringToObject(d, "info", info);
    if (dl && cJSON_IsNumber(dl)) cJSON_AddNumberToObject(d, "downloads", dl->valuedouble);
    cJSON_AddStringToObject(d, "source", "RubyGems");
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "gem", name);
    char link[300];
    if (proj) snprintf(link, sizeof link, "%s", proj);
    else snprintf(link, sizeof link, "https://rubygems.org/gems/%s", name);
    emitted += pk_emit(sink, "RUBYGEMS", "ruby-gem", name, name, info, link, d, p);
    /* (cap removed: every record of the fetched array is emitted —
     * docs/SOURCE_EXHAUSTIVENESS.md) */
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_packagist(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://packagist.org/search.json?q=%s&per_page=20", enc);
  const char *hdrs[] = { "Accept: application/json", PK_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "PACKAGIST");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *name = jo_sv(r, "name");
      const char *desc = jo_sv(r, "description");
      const char *urlf = jo_sv(r, "url");
      const cJSON *dl = cJSON_GetObjectItem(r, "downloads");
      if (!name) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "name", name);
      if (desc) cJSON_AddStringToObject(d, "description", desc);
      if (dl && cJSON_IsNumber(dl)) cJSON_AddNumberToObject(d, "downloads", dl->valuedouble);
      cJSON_AddStringToObject(d, "source", "Packagist");
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "package", name);
      emitted += pk_emit(sink, "PACKAGIST", "php-package", name, name, desc, urlf, d, p);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_dockerhub(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://hub.docker.com/v2/search/repositories/?query=%s&page_size=20", enc);
  const char *hdrs[] = { "Accept: application/json", PK_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "DOCKERHUB");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *name = jo_sv(r, "repo_name");
      const char *desc = jo_sv(r, "short_description");
      const cJSON *stars = cJSON_GetObjectItem(r, "star_count");
      const cJSON *official = cJSON_GetObjectItem(r, "is_official");
      if (!name) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "repo_name", name);
      if (desc) cJSON_AddStringToObject(d, "description", desc);
      if (stars && cJSON_IsNumber(stars)) cJSON_AddNumberToObject(d, "star_count", stars->valuedouble);
      if (cJSON_IsBool(official)) cJSON_AddBoolToObject(d, "is_official", cJSON_IsTrue(official));
      cJSON_AddStringToObject(d, "source", "Docker Hub");
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "repo", name);
      char link[320];
      if (strchr(name, '/')) snprintf(link, sizeof link, "https://hub.docker.com/r/%s", name);
      else snprintf(link, sizeof link, "https://hub.docker.com/_/%s", name);
      emitted += pk_emit(sink, "DOCKERHUB", "docker-image", name, name, desc, link, d, p);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "NPM_REGISTRY")) n = run_npm(ctx, sink, enc);
  else if (!strcmp(sid, "PYPI_PROJECT")) n = run_pypi(ctx, sink, enc);
  else if (!strcmp(sid, "CRATES_IO"))    n = run_crates(ctx, sink, enc);
  else if (!strcmp(sid, "RUBYGEMS"))     n = run_rubygems(ctx, sink, enc);
  else if (!strcmp(sid, "PACKAGIST"))    n = run_packagist(ctx, sink, enc);
  else if (!strcmp(sid, "DOCKERHUB"))    n = run_dockerhub(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define PK_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "cyber", \
    .type = "api", .url = "internal://osint/packages", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

PK_DEF(pk_npm_def, "NPM_REGISTRY", "npm Registry Search",
  "npm keyless search: packages by name/keyword (version, description, publisher).");
PK_DEF(pk_pypi_def, "PYPI_PROJECT", "PyPI Project Lookup",
  "PyPI keyless project metadata (version, summary, author, homepage).");
PK_DEF(pk_crates_def, "CRATES_IO", "crates.io Search",
  "crates.io keyless Rust crate search (description, repository, downloads).");
PK_DEF(pk_rubygems_def, "RUBYGEMS", "RubyGems Search",
  "RubyGems keyless gem search (info, downloads, project URI).");
PK_DEF(pk_packagist_def, "PACKAGIST", "Packagist Search",
  "Packagist keyless PHP/Composer package search (description, downloads).");
PK_DEF(pk_dockerhub_def, "DOCKERHUB", "Docker Hub Search",
  "Docker Hub keyless image search (description, stars, official flag).");
