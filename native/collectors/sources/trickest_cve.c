/* collectors/cyber/sources/trickest_cve.c
 * Port of server/src/collectors/trickestCve.js. Lists the trickest/cve repo
 * year directory via GitHub contents API (year, then year-1), samples up to
 * TRICKEST_CVE_LIMIT (default 50) CVE-YYYY-N.md files (sorted desc by name),
 * fetches each raw markdown and regex-extracts up to 6 PoC github repo URLs.
 * Features at TOKYO; sha1 hash-fallback uid; props in exact JS key order.
 * The seed/_meta envelope is dropped (live rows only). */
#include "../../source.h"
#include "../../lib/seenset.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../core/intel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define REPO_API "https://api.github.com/repos/trickest/cve/contents"
#define RAW_BASE "https://raw.githubusercontent.com/trickest/cve/main"
#define TIMEOUT_MS 15000

/* CVE-\d{4}-\d+\.md */
static int is_cve_md(const char *s) {
  if (!s) return 0;
  if (strncmp(s, "CVE-", 4) != 0) return 0;
  const char *p = s + 4;
  int d = 0;
  while (*p >= '0' && *p <= '9') { p++; d++; }
  if (d != 4 || *p != '-') return 0;
  p++;
  d = 0;
  while (*p >= '0' && *p <= '9') { p++; d++; }
  if (d < 1 || *p != '.') return 0;
  return strcmp(p, ".md") == 0;
}

/* substring match (JS .test on a literal) */
static int contains(const char *h, const char *n) {
  return h && strstr(h, n) != NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* SAMPLE_LIMIT = Number(process.env.TRICKEST_CVE_LIMIT || 50) */
  long limit = 50;
  const char *lenv = getenv("TRICKEST_CVE_LIMIT");
  if (lenv && lenv[0]) { long v = strtol(lenv, NULL, 10); if (v > 0) limit = v; }

  /* year = new Date().getUTCFullYear(); try [year, year-1] */
  time_t now = time(NULL);
  struct tm gt; gmtime_r(&now, &gt);
  int year = gt.tm_year + 1900;

  const char *token = getenv("GITHUB_TOKEN");
  const char *hdrs[5];
  int hi = 0;
  hdrs[hi++] = "accept: application/vnd.github+json";
  hdrs[hi++] = "user-agent: japanosint-collector";
  char authbuf[512];
  if (token && token[0]) {
    snprintf(authbuf, sizeof authbuf, "authorization: Bearer %s", token);
    hdrs[hi++] = authbuf;
  }
  hdrs[hi] = NULL;

  cJSON *listing = NULL;
  int live = 0;
  for (int k = 0; k < 2 && !live; k++) {
    int y = year - k;
    char url[256];
    snprintf(url, sizeof url, "%s/%d?per_page=100", REPO_API, y);
    cJSON *l = feed_get_json_h(ctx->http, url, hdrs, TIMEOUT_MS);
    if (l && cJSON_IsArray(l) && cJSON_GetArraySize(l) > 0) {
      listing = l;
      live = 1;
    } else if (l) {
      cJSON_Delete(l);
    }
  }
  if (!listing) return -1;

  /* collect {name,path} for type==file && name matches CVE-YYYY-N.md */
  int count = cJSON_GetArraySize(listing);
  const char **names = calloc(count, sizeof(char *));
  const char **paths = calloc(count, sizeof(char *));
  int nf = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, listing) {
    cJSON *tp = cJSON_GetObjectItem(f, "type");
    cJSON *nm = cJSON_GetObjectItem(f, "name");
    cJSON *pa = cJSON_GetObjectItem(f, "path");
    if (!tp || !cJSON_IsString(tp) || strcmp(tp->valuestring, "file") != 0) continue;
    if (!nm || !cJSON_IsString(nm) || !is_cve_md(nm->valuestring)) continue;
    if (!pa || !cJSON_IsString(pa)) continue;
    names[nf] = nm->valuestring;
    paths[nf] = pa->valuestring;
    nf++;
  }

  /* sort desc by name (b.name.localeCompare(a.name) → plain strcmp desc) */
  for (int a = 0; a < nf; a++)
    for (int b = a + 1; b < nf; b++)
      if (strcmp(names[a], names[b]) < 0) {
        const char *t = names[a]; names[a] = names[b]; names[b] = t;
        t = paths[a]; paths[a] = paths[b]; paths[b] = t;
      }

  if (nf > limit) nf = (int)limit;

  cJSON *features = cJSON_CreateArray();
  for (int idx = 0; idx < nf; idx++) {
    char rawurl[512];
    snprintf(rawurl, sizeof rawurl, "%s/%s", RAW_BASE, paths[idx]);
    char *md = feed_get_text(ctx->http, rawurl, TIMEOUT_MS);
    if (!md || !md[0]) { free(md); continue; }

    /* cveId = name without trailing .md */
    char cveId[64];
    snprintf(cveId, sizeof cveId, "%s", names[idx]);
    size_t cl = strlen(cveId);
    if (cl > 3 && strcmp(cveId + cl - 3, ".md") == 0) cveId[cl - 3] = 0;

    /* parsePocRepos: /https:\/\/github\.com\/[^\s)<>"]+/g, dedupe, skip
     * shields.io / cve.mitre.org, strip trailing [.,].
     *
     * The old "max 6" was an arbitrary editorial cut: a CVE with 30 published
     * proof-of-concept repositories reported 6 and dropped 24, which is exactly
     * what the exhaustive-use rule forbids (docs/SOURCE_EXHAUSTIVENESS.md).
     * Every distinct PoC repo in the advisory is now emitted. */
    seen_set seen = {0};
    const char *p = md;
    for (;;) {
      const char *hit = strstr(p, "https://github.com/");
      if (!hit) break;
      const char *e = hit;
      while (*e && *e != ' ' && *e != '\t' && *e != '\n' && *e != '\r' &&
             *e != ')' && *e != '<' && *e != '>' && *e != '"')
        e++;
      size_t ulen = (size_t)(e - hit);
      /* strip trailing [.,]+ */
      while (ulen > 0 && (hit[ulen - 1] == '.' || hit[ulen - 1] == ',')) ulen--;
      char *url = malloc(ulen + 1);
      memcpy(url, hit, ulen);
      url[ulen] = 0;
      p = e;
      if (contains(url, "img.shields.io") || contains(url, "cve.mitre.org")) {
        free(url);
        continue;
      }
      if (!seen_add(&seen, url)) { free(url); continue; }  /* per-CVE dedupe */
      const char *label = url + strlen("https://github.com/");

      /* audit-09: this used to pin every row at TOKYO (139.6917, 35.6895).
       * A CVE proof-of-concept repository has no location; that coordinate
       * was invented and put 16 fake pins on the map every six hours. No
       * geometry now — the row is a document, not a place. */
      cJSON *feat = cJSON_CreateObject();
      cJSON_AddStringToObject(feat, "type", "Feature");
      /* no "geometry" key at all — a null one would serialise the string
       * "null" into intel_items.geometry */

      char title[256];
      snprintf(title, sizeof title, "%s — PoC: %s", cveId, label);
      cJSON *pr = cJSON_CreateObject();
      /* uid: stable per (cve, repo) so re-runs update instead of duplicating */
      char uid[320];
      snprintf(uid, sizeof uid, "%s|%s", cveId, label);
      cJSON_AddStringToObject(pr, "uid", uid);
      cJSON_AddStringToObject(pr, "title", title);   /* was missing entirely */
      cJSON_AddStringToObject(pr, "record_type", "cve-poc");
      cJSON_AddNumberToObject(pr, "idx", cJSON_GetArraySize(features));
      cJSON_AddStringToObject(pr, "cve_id", cveId);
      cJSON_AddStringToObject(pr, "repo_label", label);
      cJSON_AddStringToObject(pr, "repo_url", url);
      cJSON_AddStringToObject(pr, "link", url);
      char cvelink[128];
      snprintf(cvelink, sizeof cvelink, "https://nvd.nist.gov/vuln/detail/%s",
               cveId);
      cJSON_AddStringToObject(pr, "cve_url", cvelink);
      cJSON_AddStringToObject(pr, "source", "trickest_cve");
      cJSON *tg = cJSON_CreateArray();
      cJSON_AddItemToArray(tg, cJSON_CreateString("cve"));
      cJSON_AddItemToArray(tg, cJSON_CreateString("exploit-poc"));
      cJSON_AddItemToObject(pr, "tags", tg);
      cJSON_AddItemToObject(feat, "properties", pr);
      cJSON_AddItemToArray(features, feat);
      free(url);            /* the seen-set kept its own copy */
    }
    seen_free(&seen);
    free(md);
  }

  free(names);
  free(paths);
  cJSON_Delete(listing);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[trickest-cve] emitted %d\n", n);
  /* audit-09: was `n > 0 ? 0 : -1` — an upstream day with no new PoC files is
   * an honest empty, not a run error, and -1 quarantines the source. */
  return 0;
}

static const source_def trickest_cve_def = {
  .id = "trickest-cve", .collector = "cyber",
  .name = "Trickest CVE", .name_ja = "Trickest CVE",
   .update_interval_sec = 21600, .run = run,
};
REGISTER_SOURCE(trickest_cve_def)
