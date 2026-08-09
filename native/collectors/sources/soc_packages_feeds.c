/* soc_packages_feeds.c — package-registry release firehoses (scheduled feeds).
 * Typosquat / dependency-confusion detection surface: a package name is only
 * newly registered once, and after the fact the signal is gone.
 *
 *  go-module-index (hourly) GET https://index.golang.org/index?since=&limit=200
 *      TRAP: newline-delimited JSON, NOT an array, and the field names are
 *      Capitalised (Path/Version/Timestamp). `since` is set to now-6h each run.
 *      emits: Path, Version, Timestamp
 *      licence: official Go module index operated by Google as public proxy
 *               infrastructure; keyless, documented at index.golang.org.
 *  pypi-new-packages (15 min) GET https://pypi.org/rss/packages.xml
 *      Standard RSS 2.0 — collected through lib/rss_atom.h.
 *      emits: title ("<name> added to PyPI"), link, description, pubDate, guid
 *      licence: official PyPI RSS feed published by the PSF for public
 *               consumption; keyless (a real User-Agent is sent, per policy).
 *  crates-io-new-crates (30 min) GET https://crates.io/api/v1/summary
 *      Six parallel arrays in one document — emitted one row per entry with a
 *      role tag, never one row for the document.
 *      emits: name, description, max_version/newest_version, created_at,
 *             updated_at, downloads, recent_downloads, repository, homepage,
 *             and for the keyword/category arrays the keyword/category plus
 *             its crate count
 *      licence: crates.io public API; the Rust Foundation crawl policy requires
 *               a User-Agent identifying the client, which we send. Keyless.
 *  homebrew-install-analytics (daily) GET https://formulae.brew.sh/api/analytics/
 *                                        install/30d.json
 *      TRAP: 'count' is a STRING with thousands separators ("557,601") —
 *      commas are stripped before conversion.
 *      emits: formula, install count (numeric), percent, rank, window
 *             start_date/end_date, total_count
 *      licence: Homebrew publishes its aggregated opt-out analytics openly
 *               (BSD-2-Clause project); keyless.
 *
 * None of these upstreams returns coordinates → has_geo stays 0 (R2).
 */
#include "../../lib/osintemit.h"
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/rss_atom.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *const SOC_UA[] = {
  "User-Agent: JapanOSINT-native/1.0 (OSINT research collector; +https://github.com/)",
  "Accept: application/json", NULL
};

/* ---- go-module-index ---------------------------------------------------- */

static int run_go_index(const source_ctx *ctx, intel_sink *sink) {
  time_t t = time(NULL) - 6 * 3600;
  struct tm g; gmtime_r(&t, &g);
  char since[40], url[220];
  strftime(since, sizeof since, "%Y-%m-%dT%H:%M:%SZ", &g);
  snprintf(url, sizeof url, "https://index.golang.org/index?since=%s&limit=200",
           since);
  char *txt = feed_get_text(ctx->http, url, 30000);
  if (!txt) { fprintf(stderr, "[go-module-index] fetch failed\n"); return -1; }

  int n = 0;
  char *p = txt;
  while (*p) {                       /* newline-delimited JSON, not an array */
    char *nl = strchr(p, '\n');
    if (nl) *nl = 0;
    if (*p == '{') {
      cJSON *o = cJSON_Parse(p);
      if (o) {
        const char *path = jo_sv(o, "Path");
        const char *ver  = jo_sv(o, "Version");
        const char *ts   = jo_sv(o, "Timestamp");
        if (path && ver) {
          cJSON *pr = cJSON_CreateObject();
          if (pr) {
            cJSON_AddStringToObject(pr, "source", "index.golang.org");
            cJSON_AddStringToObject(pr, "ecosystem", "go");
            cJSON_AddStringToObject(pr, "path", path);
            cJSON_AddStringToObject(pr, "version", ver);
            if (ts) cJSON_AddStringToObject(pr, "timestamp", ts);
            char title[400], rk[420], link[420];
            snprintf(title, sizeof title, "%s %s", path, ver);
            snprintf(rk, sizeof rk, "%s@%s", path, ver);
            snprintf(link, sizeof link, "https://pkg.go.dev/%s@%s", path, ver);
            n += soc_emit(sink, pr, "package-release",
                          "[\"packages\",\"go\",\"release\",\"supply-chain\"]",
                          rk, title, ts, link, ts, NULL);
          }
        }
        cJSON_Delete(o);
      }
    }
    if (!nl) break;
    p = nl + 1;
  }
  free(txt);
  fprintf(stderr, "[go-module-index] emitted %d since %s\n", n, since);
  return 0;
}

static const source_def soc_go_index_def = {
  .id = "go-module-index", .collector = "social",
  .name = "Go Module Index (new releases)",
  .update_interval_sec = 3600, .run = run_go_index,
  .category = "social", .type = "api",
  .url = "https://index.golang.org/index",
  .description = "Chronological log of every Go module version published to the public proxy, with exact publication timestamps — the shape needed for typosquat and dependency-confusion detection.",
  .license = "Official Go module index operated by Google as public proxy infrastructure; keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_go_index_def)

/* ---- pypi-new-packages (RSS) -------------------------------------------- */

static int run_pypi_new(const source_ctx *ctx, intel_sink *sink) {
  int n = rss_collect(ctx, sink, "https://pypi.org/rss/packages.xml", "en",
                      "[\"packages\",\"pypi\",\"new-package\",\"supply-chain\"]");
  if (n < 0) { fprintf(stderr, "[pypi-new-packages] fetch/parse failed\n"); return -1; }
  fprintf(stderr, "[pypi-new-packages] emitted %d\n", n);
  return 0;
}

static const source_def soc_pypi_new_def = {
  .id = "pypi-new-packages", .collector = "social",
  .name = "PyPI Newest Packages Feed",
  .update_interval_sec = 900, .run = run_pypi_new,
  .category = "social", .type = "web_request",
  .url = "https://pypi.org/rss/packages.xml",
  .description = "Every brand-new package name registered on PyPI with its summary and registration time — the primary detection surface for typosquats of popular libraries.",
  .license = "Official PyPI RSS feed published by the PSF for public consumption; keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_pypi_new_def)

/* ---- crates-io-new-crates ----------------------------------------------- */

static int crates_array(intel_sink *sink, const cJSON *doc, const char *key,
                        const char *role) {
  const cJSON *arr = cJSON_GetObjectItem(doc, key);
  if (!cJSON_IsArray(arr)) return 0;
  int n = 0;
  const cJSON *c;
  cJSON_ArrayForEach(c, arr) {
    /* crate rows key on "name"; keyword rows on "keyword"; category rows on
     * "category" — take whichever this array carries. */
    const char *name = jo_sv(c, "name");
    const char *kind = "crate";
    if (!name) { name = jo_sv(c, "keyword"); if (name) kind = "keyword"; }
    if (!name) { name = jo_sv(c, "category"); if (name) kind = "category"; }
    if (!name) continue;
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "crates.io");
    cJSON_AddStringToObject(p, "ecosystem", "cargo");
    cJSON_AddStringToObject(p, "role", role);
    cJSON_AddStringToObject(p, "kind", kind);
    cJSON_AddStringToObject(p, "name", name);
    jcopy(p, "description", c, "description");
    jcopy(p, "max_version", c, "max_version");
    jcopy(p, "newest_version", c, "newest_version");
    jcopy(p, "created_at", c, "created_at");
    jcopy(p, "updated_at", c, "updated_at");
    jcopy(p, "downloads", c, "downloads");
    jcopy(p, "recent_downloads", c, "recent_downloads");
    jcopy(p, "repository", c, "repository");
    jcopy(p, "homepage", c, "homepage");
    jcopy(p, "documentation", c, "documentation");
    jcopy(p, "crates_cnt", c, "crates_cnt");
    jcopy(p, "slug", c, "slug");
    char link[400], rk[300], title[300], summary[300];
    if (!strcmp(kind, "crate"))
      snprintf(link, sizeof link, "https://crates.io/crates/%s", name);
    else if (!strcmp(kind, "keyword"))
      snprintf(link, sizeof link, "https://crates.io/keywords/%s", name);
    else
      snprintf(link, sizeof link, "https://crates.io/categories/%s",
               jo_sv(c, "slug") ? jo_sv(c, "slug") : name);
    snprintf(rk, sizeof rk, "crates:%s:%s:%s", role, kind, name);
    snprintf(title, sizeof title, "%s %s", name,
             jo_sv(c, "max_version") ? jo_sv(c, "max_version") : "");
    snprintf(summary, sizeof summary, "%s · %.200s", role,
             jo_sv(c, "description") ? jo_sv(c, "description") : "");
    n += soc_emit(sink, p, "package-registry-summary",
                  "[\"packages\",\"crates-io\",\"rust\",\"supply-chain\"]",
                  rk, title, summary, link,
                  jo_sv(c, "created_at"), NULL);
  }
  return n;
}

static int run_crates_summary(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http, "https://crates.io/api/v1/summary",
                               SOC_UA, 25000);
  if (!cJSON_IsObject(doc)) {
    fprintf(stderr, "[crates-io-new-crates] fetch/parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  n += crates_array(sink, doc, "new_crates", "new");
  n += crates_array(sink, doc, "most_recently_updated", "recently_updated");
  n += crates_array(sink, doc, "just_updated", "just_updated");
  n += crates_array(sink, doc, "most_downloaded", "most_downloaded");
  n += crates_array(sink, doc, "popular_keywords", "popular_keyword");
  n += crates_array(sink, doc, "popular_categories", "popular_category");
  cJSON_Delete(doc);
  fprintf(stderr, "[crates-io-new-crates] emitted %d\n", n);
  return 0;
}

static const source_def soc_crates_summary_def = {
  .id = "crates-io-new-crates", .collector = "social",
  .name = "crates.io New and Trending Crates",
  .update_interval_sec = 1800, .run = run_crates_summary,
  .category = "social", .type = "api",
  .url = "https://crates.io/api/v1/summary",
  .description = "Rust registry summary: newest crates, just-updated and most-downloaded sets with repository URLs and timestamps — the new-crates half is the typosquat surface.",
  .license = "crates.io public API; keyless. Rust Foundation crawl policy requires an identifying User-Agent, which we send.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_crates_summary_def)

/* ---- homebrew-install-analytics ----------------------------------------- */

/* "557,601" → 557601. Returns -1 when the value is not usable. */
static double strip_commas_num(const cJSON *v) {
  if (cJSON_IsNumber(v)) return v->valuedouble;
  if (!cJSON_IsString(v) || !v->valuestring) return -1;
  char buf[32]; size_t j = 0;
  for (const char *p = v->valuestring; *p && j < sizeof buf - 1; p++)
    if (*p != ',') buf[j++] = *p;
  buf[j] = 0;
  if (!j) return -1;
  return atof(buf);
}

static int run_brew_analytics(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://formulae.brew.sh/api/analytics/install/30d.json", SOC_UA, 40000);
  if (!cJSON_IsObject(doc)) {
    fprintf(stderr, "[homebrew-install-analytics] fetch/parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  const cJSON *items = cJSON_GetObjectItem(doc, "items");
  if (!cJSON_IsArray(items)) {
    fprintf(stderr, "[homebrew-install-analytics] unexpected shape\n");
    cJSON_Delete(doc); return -1;
  }
  const char *start = jo_sv(doc, "start_date"), *end = jo_sv(doc, "end_date");
  const cJSON *totalv = cJSON_GetObjectItem(doc, "total_count");
  int n = 0;
  const cJSON *i;
  cJSON_ArrayForEach(i, items) {
    if (n >= 300) break;                     /* top 300 of ~25,000 formulae */
    const char *formula = jo_sv(i, "formula");
    if (!formula) continue;
    double cnt = strip_commas_num(cJSON_GetObjectItem(i, "count"));
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "formulae.brew.sh");
    cJSON_AddStringToObject(p, "ecosystem", "homebrew");
    cJSON_AddStringToObject(p, "formula", formula);
    if (cnt >= 0) cJSON_AddNumberToObject(p, "install_count", cnt);
    jcopy(p, "percent", i, "percent");
    jcopy(p, "rank", i, "number");
    if (start) cJSON_AddStringToObject(p, "start_date", start);
    if (end)   cJSON_AddStringToObject(p, "end_date", end);
    if (cJSON_IsNumber(totalv))
      cJSON_AddNumberToObject(p, "window_total_count", totalv->valuedouble);
    char link[300], rk[200], summary[220];
    snprintf(link, sizeof link, "https://formulae.brew.sh/formula/%s", formula);
    snprintf(rk, sizeof rk, "brew30d:%s:%s", end ? end : "", formula);
    snprintf(summary, sizeof summary, "%.0f installs in 30d (%s%%)", cnt,
             jo_sv(i, "percent") ? jo_sv(i, "percent") : "?");
    n += soc_emit(sink, p, "package-install-analytics",
                  "[\"packages\",\"homebrew\",\"telemetry\"]",
                  rk, formula, summary, link, NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[homebrew-install-analytics] emitted %d\n", n);
  return 0;
}

static const source_def soc_brew_analytics_def = {
  .id = "homebrew-install-analytics", .collector = "social",
  .name = "Homebrew Install Analytics (30 day)",
  .update_interval_sec = 86400, .run = run_brew_analytics,
  .category = "social", .type = "api",
  .url = "https://formulae.brew.sh/api/analytics/install/30d.json",
  .description = "Real installation counts for Homebrew formulae over a rolling 30 days — turns 'is this dependency widely used' into a number when triaging a disclosed vulnerability.",
  .license = "Homebrew publishes its aggregated opt-out analytics openly (BSD-2-Clause project); keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_brew_analytics_def)
