/* soc_package_search.c — six on-demand package-registry lookups (entity pivots).
 * Supply-chain coverage for the ecosystems the fleet did not reach: JVM, .NET,
 * BEAM, AUR, plus the cross-registry version graph and npm download volume.
 * All keyless; none returns coordinates → has_geo stays 0 (R2).
 *
 *  MAVEN_CENTRAL_SEARCH (keyword)
 *      GET https://search.maven.org/solrsearch/select?q=&rows=20&wt=json
 *      Solr envelope response.docs[]; `timestamp` is epoch MILLIseconds.
 *      TRAP: the host intermittently drops the connection and succeeds on an
 *      immediate retry — we retry once, then fall back to the identical-shape
 *      central.sonatype.com mirror, before reporting nothing.
 *      emits: id, g (group), a (artifact), latestVersion, p (packaging),
 *             repositoryId, versionCount, timestamp (→ ISO)
 *      licence: Sonatype's public Maven Central search; keyless, documented at
 *               central.sonatype.org. Back off on 429.
 *  NUGET_PACKAGE_SEARCH (keyword)
 *      GET https://azuresearch-usnc.nuget.org/query?q=&take=20 → data[]
 *      emits: id, version, title, description, summary, projectUrl, licenseUrl,
 *             tags, authors, totalDownloads, verified
 *      licence: official NuGet V3 search service (discoverable from
 *               api.nuget.org/v3/index.json); keyless, documented.
 *  HEXPM_PACKAGE_SEARCH (keyword)
 *      GET https://hex.pm/api/packages?search=&page=1 — bare array
 *      emits: name, url, inserted_at, updated_at, repository,
 *             meta.description/licenses/links, first and latest release
 *             version + timestamp from the embedded releases[]
 *      licence: Hex.pm public API; keyless read, asks for an identifying
 *               User-Agent, which we send.
 *  AUR_PACKAGE_SEARCH (keyword)
 *      GET https://aur.archlinux.org/rpc/v5/search/<term> → {resultcount, results[]}
 *      emits: Name, PackageBase, Description, Maintainer (null = ORPHANED,
 *             which is itself the signal), Version, NumVotes, Popularity,
 *             OutOfDate, FirstSubmitted / LastModified (epoch → ISO), URL
 *      licence: official AUR RPC interface v5; keyless. AUR asks clients to use
 *               the RPC rather than scrape and to rate-limit — we issue one
 *               request per pivot.
 *  DEPSDEV_PACKAGE (keyword = package name)
 *      GET https://api.deps.dev/v3alpha/systems/<sys>/packages/<name>
 *      Tries npm, then pypi, then cargo, and stops at the first system that
 *      knows the name. Response can be large (288 versions for express) — the
 *      most recent 25 versions are emitted.
 *      emits: packageKey.system/name, purl, version, publishedAt, isDefault,
 *             isDeprecated, deprecatedReason
 *      licence: deps.dev API operated by Google, free and keyless.
 *  NPM_DOWNLOADS (keyword = package name)
 *      GET https://api.npmjs.org/downloads/point/last-week/<pkg>
 *      emits: package, downloads, start, end
 *      licence: npm's public download-counts API, keyless and explicitly
 *               documented for third-party use.
 */
#include "../../lib/osintemit.h"
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *const SOC_UA[] = {
  "User-Agent: JapanOSINT-native/1.0 (OSINT research collector; +https://github.com/)",
  "Accept: application/json", NULL
};

/* A package name: a keyword with no whitespace. */
static int looks_like_package(const char *s) {
  if (!jo_looks_like_keyword(s)) return 0;
  for (const char *p = s; *p; p++) if (isspace((unsigned char)*p)) return 0;
  return 1;
}

static void iso_from_epoch(double secs, char *out, size_t n) {
  time_t t = (time_t)secs;
  struct tm g; gmtime_r(&t, &g);
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &g);
}

/* ---- MAVEN_CENTRAL_SEARCH ----------------------------------------------- */

static int run_maven(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char primary[400], mirror[400];
  snprintf(primary, sizeof primary,
           "https://search.maven.org/solrsearch/select?q=%s&rows=20&wt=json", enc);
  snprintf(mirror, sizeof mirror,
           "https://central.sonatype.com/solrsearch/select?q=%s&rows=20&wt=json", enc);
  free(enc);

  /* search.maven.org intermittently returns a connection failure and then
   * succeeds on an immediate retry — retry once before failing over. */
  cJSON *doc = feed_get_json_h(ctx->http, primary, SOC_UA, 20000);
  if (!doc) doc = feed_get_json_h(ctx->http, primary, SOC_UA, 20000);
  if (!doc) doc = feed_get_json_h(ctx->http, mirror, SOC_UA, 20000);
  if (!doc) {
    fprintf(stderr, "[MAVEN_CENTRAL_SEARCH] no response for %s\n", q);
    return 0;
  }
  const cJSON *resp = cJSON_GetObjectItem(doc, "response");
  const cJSON *docs = cJSON_IsObject(resp) ? cJSON_GetObjectItem(resp, "docs") : NULL;
  const cJSON *found = cJSON_IsObject(resp) ? cJSON_GetObjectItem(resp, "numFound") : NULL;
  int n = 0;
  const cJSON *d;
  if (cJSON_IsArray(docs)) cJSON_ArrayForEach(d, docs) {
    const char *g = jo_sv(d, "g"), *a = jo_sv(d, "a");
    if (!g || !a) continue;
    const cJSON *tsv = cJSON_GetObjectItem(d, "timestamp");
    char iso[40]; iso[0] = 0;
    if (cJSON_IsNumber(tsv))      /* epoch MILLIseconds */
      iso_from_epoch(tsv->valuedouble / 1000.0, iso, sizeof iso);
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "MAVEN_CENTRAL_SEARCH");
    cJSON_AddStringToObject(p, "source", "search.maven.org");
    cJSON_AddStringToObject(p, "entity", q);
    cJSON_AddStringToObject(p, "ecosystem", "maven");
    jcopy(p, "id", d, "id");
    jcopy(p, "group_id", d, "g");
    jcopy(p, "artifact_id", d, "a");
    jcopy(p, "latest_version", d, "latestVersion");
    jcopy(p, "packaging", d, "p");
    jcopy(p, "repository_id", d, "repositoryId");
    jcopy(p, "version_count", d, "versionCount");
    if (iso[0]) cJSON_AddStringToObject(p, "last_published", iso);
    if (cJSON_IsNumber(found))
      cJSON_AddNumberToObject(p, "total_matches", found->valuedouble);
    cJSON_AddBoolToObject(p, "success", 1);
    char title[300], link[500], summary[300];
    snprintf(title, sizeof title, "%s:%s %s", g, a,
             jo_sv(d, "latestVersion") ? jo_sv(d, "latestVersion") : "");
    snprintf(link, sizeof link,
             "https://central.sonatype.com/artifact/%s/%s", g, a);
    const cJSON *vc = cJSON_GetObjectItem(d, "versionCount");
    snprintf(summary, sizeof summary, "%ld versions · last published %s",
             cJSON_IsNumber(vc) ? (long)vc->valuedouble : 0L,
             iso[0] ? iso : "?");
    char rk[300];
    snprintf(rk, sizeof rk, "%s:%s", g, a);
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"MAVEN_CENTRAL_SEARCH\",\"packages\"]",
                  rk, title, summary, link, iso[0] ? iso : NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[MAVEN_CENTRAL_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_maven_def = {
  .id = "MAVEN_CENTRAL_SEARCH", .collector = "osint",
  .name = "Maven Central Coordinate Search",
  .update_interval_sec = 0, .run = run_maven,
  .category = "social", .type = "api",
  .url = "https://search.maven.org/solrsearch/select",
  .description = "Search the Java/JVM ecosystem by group, artifact or free text — coordinates, latest version, packaging, version count and publication timestamp.",
  .license = "Sonatype's public Maven Central search; keyless, documented at central.sonatype.org, rate limited.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_maven_def)

/* ---- NUGET_PACKAGE_SEARCH ----------------------------------------------- */

static int run_nuget(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[400];
  snprintf(url, sizeof url,
           "https://azuresearch-usnc.nuget.org/query?q=%s&take=20", enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!doc) { fprintf(stderr, "[NUGET_PACKAGE_SEARCH] no response for %s\n", q); return 0; }
  const cJSON *arr = cJSON_GetObjectItem(doc, "data");
  const cJSON *hits = cJSON_GetObjectItem(doc, "totalHits");
  int n = 0;
  const cJSON *d;
  if (cJSON_IsArray(arr)) cJSON_ArrayForEach(d, arr) {
    const char *id = jo_sv(d, "id");
    if (!id) continue;
    const cJSON *td = cJSON_GetObjectItem(d, "totalDownloads");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "NUGET_PACKAGE_SEARCH");
    cJSON_AddStringToObject(p, "source", "azuresearch-usnc.nuget.org");
    cJSON_AddStringToObject(p, "entity", q);
    cJSON_AddStringToObject(p, "ecosystem", "nuget");
    jcopy(p, "id", d, "id");
    jcopy(p, "version", d, "version");
    jcopy(p, "title", d, "title");
    jcopy(p, "description", d, "description");
    jcopy(p, "summary", d, "summary");
    jcopy(p, "project_url", d, "projectUrl");
    jcopy(p, "license_url", d, "licenseUrl");
    jcopy(p, "tags", d, "tags");
    jcopy(p, "authors", d, "authors");
    jcopy(p, "total_downloads", d, "totalDownloads");
    jcopy(p, "verified", d, "verified");
    if (cJSON_IsNumber(hits))
      cJSON_AddNumberToObject(p, "total_matches", hits->valuedouble);
    cJSON_AddBoolToObject(p, "success", 1);
    char title[300], link[300], summary[300];
    snprintf(title, sizeof title, "%s %s", id,
             jo_sv(d, "version") ? jo_sv(d, "version") : "");
    snprintf(link, sizeof link, "https://www.nuget.org/packages/%s", id);
    snprintf(summary, sizeof summary, "%.0f downloads%s · %.150s",
             cJSON_IsNumber(td) ? td->valuedouble : 0.0,
             cJSON_IsTrue(cJSON_GetObjectItem(d, "verified")) ? " · verified" : "",
             jo_sv(d, "description") ? jo_sv(d, "description") : "");
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"NUGET_PACKAGE_SEARCH\",\"packages\"]",
                  id, title, summary, link, NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[NUGET_PACKAGE_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_nuget_def = {
  .id = "NUGET_PACKAGE_SEARCH", .collector = "osint",
  .name = "NuGet Gallery Package Search",
  .update_interval_sec = 0, .run = run_nuget,
  .category = "social", .type = "api",
  .url = "https://azuresearch-usnc.nuget.org/query",
  .description = ".NET package search returning id, version, authors, project URL, download totals and the 'verified' flag that makes squatted lookalikes visible in the response itself.",
  .license = "Official NuGet V3 search service endpoint; keyless and documented.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_nuget_def)

/* ---- HEXPM_PACKAGE_SEARCH ------------------------------------------------ */

static int run_hexpm(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[400];
  snprintf(url, sizeof url, "https://hex.pm/api/packages?search=%s&page=1", enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[HEXPM_PACKAGE_SEARCH] no usable response for %s\n", q);
    cJSON_Delete(doc); return 0;
  }
  int n = 0;
  const cJSON *d;
  cJSON_ArrayForEach(d, doc) {
    if (n >= 50) break;
    const char *name = jo_sv(d, "name");
    if (!name) continue;
    const cJSON *meta = cJSON_GetObjectItem(d, "meta");
    const cJSON *rels = cJSON_GetObjectItem(d, "releases");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "HEXPM_PACKAGE_SEARCH");
    cJSON_AddStringToObject(p, "source", "hex.pm");
    cJSON_AddStringToObject(p, "entity", q);
    cJSON_AddStringToObject(p, "ecosystem", "hex");
    jcopy(p, "name", d, "name");
    jcopy(p, "url", d, "url");
    jcopy(p, "html_url", d, "html_url");
    jcopy(p, "inserted_at", d, "inserted_at");
    jcopy(p, "updated_at", d, "updated_at");
    jcopy(p, "repository", d, "repository");
    jcopy(p, "latest_stable_version", d, "latest_stable_version");
    if (cJSON_IsObject(meta)) {
      jcopy(p, "description", meta, "description");
      jcopy(p, "licenses", meta, "licenses");
      const cJSON *links = cJSON_GetObjectItem(meta, "links");
      if (cJSON_IsObject(links))
        cJSON_AddItemToObject(p, "links", cJSON_Duplicate(links, 1));
    }
    /* releases[] gives a free first-published / last-published pair */
    const char *latest_ver = NULL, *latest_at = NULL, *first_at = NULL;
    if (cJSON_IsArray(rels)) {
      const cJSON *r0 = cJSON_GetArrayItem(rels, 0);
      int sz = cJSON_GetArraySize(rels);
      const cJSON *rn = sz > 0 ? cJSON_GetArrayItem(rels, sz - 1) : NULL;
      if (cJSON_IsObject(r0)) { latest_ver = jo_sv(r0, "version");
                                latest_at = jo_sv(r0, "inserted_at"); }
      if (cJSON_IsObject(rn)) first_at = jo_sv(rn, "inserted_at");
      cJSON_AddNumberToObject(p, "release_count", sz);
    }
    if (latest_ver) cJSON_AddStringToObject(p, "latest_version", latest_ver);
    if (latest_at)  cJSON_AddStringToObject(p, "latest_release_at", latest_at);
    if (first_at)   cJSON_AddStringToObject(p, "first_release_at", first_at);
    cJSON_AddBoolToObject(p, "success", 1);
    char title[300], link[300], summary[400];
    snprintf(title, sizeof title, "%s %s", name, latest_ver ? latest_ver : "");
    snprintf(link, sizeof link, "https://hex.pm/packages/%s", name);
    snprintf(summary, sizeof summary, "first %s · latest %s · %.150s",
             first_at ? first_at : "?", latest_at ? latest_at : "?",
             (cJSON_IsObject(meta) && jo_sv(meta, "description"))
               ? jo_sv(meta, "description") : "");
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"HEXPM_PACKAGE_SEARCH\",\"packages\"]",
                  name, title, summary, link, jo_sv(d, "inserted_at"), NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[HEXPM_PACKAGE_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_hexpm_def = {
  .id = "HEXPM_PACKAGE_SEARCH", .collector = "osint",
  .name = "Hex.pm (Elixir/Erlang) Package Search",
  .update_interval_sec = 0, .run = run_hexpm,
  .category = "social", .type = "api",
  .url = "https://hex.pm/api/packages",
  .description = "BEAM-ecosystem package search with declared licences, upstream repository links and a first/last published pair from the embedded release history.",
  .license = "Hex.pm public API; keyless read, identifying User-Agent sent as asked.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_hexpm_def)

/* ---- AUR_PACKAGE_SEARCH -------------------------------------------------- */

static int run_aur(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[400];
  snprintf(url, sizeof url, "https://aur.archlinux.org/rpc/v5/search/%s", enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!doc) { fprintf(stderr, "[AUR_PACKAGE_SEARCH] no response for %s\n", q); return 0; }
  const cJSON *res = cJSON_GetObjectItem(doc, "results");
  const cJSON *cnt = cJSON_GetObjectItem(doc, "resultcount");
  int n = 0;
  const cJSON *d;
  if (cJSON_IsArray(res)) cJSON_ArrayForEach(d, res) {
    if (n >= 60) break;
    const char *name = jo_sv(d, "Name");
    if (!name) continue;
    const cJSON *fs = cJSON_GetObjectItem(d, "FirstSubmitted");
    const cJSON *lm = cJSON_GetObjectItem(d, "LastModified");
    const cJSON *ood = cJSON_GetObjectItem(d, "OutOfDate");
    char first[40] = {0}, last[40] = {0};
    if (cJSON_IsNumber(fs)) iso_from_epoch(fs->valuedouble, first, sizeof first);
    if (cJSON_IsNumber(lm)) iso_from_epoch(lm->valuedouble, last, sizeof last);
    /* Maintainer is null for orphaned packages — that null IS the signal. */
    const cJSON *mv = cJSON_GetObjectItem(d, "Maintainer");
    const char *maint = (cJSON_IsString(mv) && mv->valuestring && mv->valuestring[0])
                          ? mv->valuestring : NULL;
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "AUR_PACKAGE_SEARCH");
    cJSON_AddStringToObject(p, "source", "aur.archlinux.org");
    cJSON_AddStringToObject(p, "entity", q);
    cJSON_AddStringToObject(p, "ecosystem", "aur");
    jcopy(p, "name", d, "Name");
    jcopy(p, "package_base", d, "PackageBase");
    jcopy(p, "description", d, "Description");
    jcopy(p, "version", d, "Version");
    jcopy(p, "num_votes", d, "NumVotes");
    jcopy(p, "popularity", d, "Popularity");
    jcopy(p, "url", d, "URL");
    jcopy(p, "url_path", d, "URLPath");
    if (maint) cJSON_AddStringToObject(p, "maintainer", maint);
    cJSON_AddBoolToObject(p, "orphaned", maint ? 0 : 1);
    if (cJSON_IsNumber(ood)) {
      char ooiso[40];
      iso_from_epoch(ood->valuedouble, ooiso, sizeof ooiso);
      cJSON_AddStringToObject(p, "out_of_date_since", ooiso);
    }
    if (first[0]) cJSON_AddStringToObject(p, "first_submitted", first);
    if (last[0])  cJSON_AddStringToObject(p, "last_modified", last);
    if (cJSON_IsNumber(cnt))
      cJSON_AddNumberToObject(p, "total_matches", cnt->valuedouble);
    cJSON_AddBoolToObject(p, "success", 1);
    char title[300], link[300], summary[400];
    snprintf(title, sizeof title, "%s %s", name,
             jo_sv(d, "Version") ? jo_sv(d, "Version") : "");
    snprintf(link, sizeof link, "https://aur.archlinux.org/packages/%s", name);
    snprintf(summary, sizeof summary, "maintainer %s · submitted %s · %.150s",
             maint ? maint : "ORPHANED", first[0] ? first : "?",
             jo_sv(d, "Description") ? jo_sv(d, "Description") : "");
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"AUR_PACKAGE_SEARCH\",\"packages\"]",
                  name, title, summary, link, first[0] ? first : NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[AUR_PACKAGE_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_aur_def = {
  .id = "AUR_PACKAGE_SEARCH", .collector = "osint",
  .name = "Arch User Repository RPC Search",
  .update_interval_sec = 0, .run = run_aur,
  .category = "social", .type = "api",
  .url = "https://aur.archlinux.org/rpc/v5/search/",
  .description = "User-submitted Linux package repository search exposing maintainer handle, submission date and an out-of-date flag — a recognised supply-chain risk surface and a username pivot.",
  .license = "Official AUR RPC interface v5; keyless. One request per pivot, as AUR's rate-limit guidance asks.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_aur_def)

/* ---- DEPSDEV_PACKAGE ----------------------------------------------------- */

static int run_depsdev(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_package(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  static const char *const systems[] = { "npm", "pypi", "cargo", NULL };
  cJSON *doc = NULL;
  const char *sysname = NULL;
  for (int i = 0; systems[i] && !doc; i++) {
    char url[400];
    snprintf(url, sizeof url,
             "https://api.deps.dev/v3alpha/systems/%s/packages/%s", systems[i], enc);
    doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
    if (doc && !cJSON_IsArray(cJSON_GetObjectItem(doc, "versions"))) {
      cJSON_Delete(doc); doc = NULL;              /* known name, no versions */
    }
    if (doc) sysname = systems[i];
  }
  free(enc);
  if (!doc) {
    fprintf(stderr, "[DEPSDEV_PACKAGE] no package named %s in npm/pypi/cargo\n", q);
    return 0;                                     /* clean empty (R3) */
  }
  const cJSON *pk = cJSON_GetObjectItem(doc, "packageKey");
  const cJSON *vers = cJSON_GetObjectItem(doc, "versions");
  int total = cJSON_GetArraySize(vers);
  int start = total > 25 ? total - 25 : 0;        /* the 25 most recent */
  int n = 0;
  for (int i = start; i < total; i++) {
    const cJSON *v = cJSON_GetArrayItem(vers, i);
    if (!cJSON_IsObject(v)) continue;
    const cJSON *vk = cJSON_GetObjectItem(v, "versionKey");
    const char *ver = cJSON_IsObject(vk) ? jo_sv(vk, "version") : NULL;
    if (!ver) continue;
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "DEPSDEV_PACKAGE");
    cJSON_AddStringToObject(p, "source", "api.deps.dev");
    cJSON_AddStringToObject(p, "entity", q);
    cJSON_AddStringToObject(p, "system", sysname);
    if (cJSON_IsObject(pk)) {
      jcopy(p, "package_system", pk, "system");
      jcopy(p, "package_name", pk, "name");
    }
    jcopy(p, "purl", v, "purl");
    cJSON_AddStringToObject(p, "version", ver);
    jcopy(p, "published_at", v, "publishedAt");
    jcopy(p, "is_default", v, "isDefault");
    jcopy(p, "is_deprecated", v, "isDeprecated");
    jcopy(p, "deprecated_reason", v, "deprecatedReason");
    cJSON_AddNumberToObject(p, "total_versions", total);
    cJSON_AddBoolToObject(p, "success", 1);
    char title[300], rk[300], link[400], summary[400];
    snprintf(title, sizeof title, "%s %s (%s)", q, ver, sysname);
    snprintf(rk, sizeof rk, "depsdev:%s:%s@%s", sysname, q, ver);
    snprintf(link, sizeof link, "https://deps.dev/%s/%s/%s", sysname, q, ver);
    snprintf(summary, sizeof summary, "published %s%s%.150s",
             jo_sv(v, "publishedAt") ? jo_sv(v, "publishedAt") : "?",
             cJSON_IsTrue(cJSON_GetObjectItem(v, "isDeprecated")) ? " · DEPRECATED: " : "",
             jo_sv(v, "deprecatedReason") ? jo_sv(v, "deprecatedReason") : "");
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"DEPSDEV_PACKAGE\",\"packages\"]",
                  rk, title, summary, link, jo_sv(v, "publishedAt"), NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[DEPSDEV_PACKAGE] emitted %d of %d versions for %s (%s)\n",
          n, total, q, sysname);
  return 0;
}

static const source_def soc_depsdev_def = {
  .id = "DEPSDEV_PACKAGE", .collector = "osint",
  .name = "deps.dev Package Version Graph",
  .update_interval_sec = 0, .run = run_depsdev,
  .category = "social", .type = "api",
  .url = "https://api.deps.dev/v3alpha/systems/",
  .description = "Google's Open Source Insights: full version list for a package with exact publish timestamps, deprecation status and package URLs (purl) that join to OSV advisories.",
  .license = "deps.dev API operated by Google, free and keyless, documented at docs.deps.dev.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_depsdev_def)

/* ---- NPM_DOWNLOADS ------------------------------------------------------- */

static int run_npm_downloads(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_package(q)) return 0;
  /* scoped packages (@scope/name) need the slash encoded */
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[400];
  snprintf(url, sizeof url,
           "https://api.npmjs.org/downloads/point/last-week/%s", enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!doc) { fprintf(stderr, "[NPM_DOWNLOADS] no data for %s\n", q); return 0; }
  if (jo_sv(doc, "error")) {          /* "package not found" → clean empty */
    fprintf(stderr, "[NPM_DOWNLOADS] %s: %s\n", q, jo_sv(doc, "error"));
    cJSON_Delete(doc); return 0;
  }
  const cJSON *dl = cJSON_GetObjectItem(doc, "downloads");
  const char *pkg = jo_sv(doc, "package");
  if (!cJSON_IsNumber(dl) || !pkg) { cJSON_Delete(doc); return 0; }
  cJSON *p = cJSON_CreateObject();
  int n = 0;
  if (p) {
    cJSON_AddStringToObject(p, "service", "NPM_DOWNLOADS");
    cJSON_AddStringToObject(p, "source", "api.npmjs.org");
    cJSON_AddStringToObject(p, "entity", q);
    cJSON_AddStringToObject(p, "ecosystem", "npm");
    cJSON_AddStringToObject(p, "period", "last-week");
    jcopy(p, "package", doc, "package");
    jcopy(p, "downloads", doc, "downloads");
    jcopy(p, "start", doc, "start");
    jcopy(p, "end", doc, "end");
    cJSON_AddBoolToObject(p, "success", 1);
    char title[300], link[300], summary[300], rk[300];
    snprintf(title, sizeof title, "%s — %.0f downloads/week", pkg, dl->valuedouble);
    snprintf(link, sizeof link, "https://www.npmjs.com/package/%s", pkg);
    snprintf(summary, sizeof summary, "%s → %s",
             jo_sv(doc, "start") ? jo_sv(doc, "start") : "?",
             jo_sv(doc, "end") ? jo_sv(doc, "end") : "?");
    snprintf(rk, sizeof rk, "npm-dl:%s:%s", pkg,
             jo_sv(doc, "end") ? jo_sv(doc, "end") : "");
    n = soc_emit(sink, p, "osint_service_result",
                 "[\"osint-search\",\"NPM_DOWNLOADS\",\"packages\"]",
                 rk, title, summary, link, jo_sv(doc, "end"), NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[NPM_DOWNLOADS] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_npm_downloads_def = {
  .id = "NPM_DOWNLOADS", .collector = "osint",
  .name = "npm Download Counts",
  .update_interval_sec = 0, .run = run_npm_downloads,
  .category = "social", .type = "api",
  .url = "https://api.npmjs.org/downloads/point/last-week/",
  .description = "Actual weekly download volume for an npm package — the number that separates a real dependency from a squatted lookalike one character away from it.",
  .license = "npm's public download-counts API; keyless, explicitly documented for third-party use.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_npm_downloads_def)
