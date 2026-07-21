/* collectors/cyber/sources/trickest_cve.c
 * Port of server/src/collectors/trickestCve.js. Lists the trickest/cve repo
 * year directory via GitHub contents API (year, then year-1), samples up to
 * TRICKEST_CVE_LIMIT (default 50) CVE-YYYY-N.md files (sorted desc by name),
 * fetches each raw markdown and regex-extracts up to 6 PoC github repo URLs.
 * Features at TOKYO; sha1 hash-fallback uid; props in exact JS key order.
 * The seed/_meta envelope is dropped (live rows only). */
#include "../../source.h"
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
     * shields.io / cve.mitre.org, strip trailing [.,], max 6 */
    const char *seen[6];
    int sn = 0;
    const char *p = md;
    while (sn < 6) {
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
      int dup = 0;
      for (int s = 0; s < sn; s++)
        if (strcmp(seen[s], url) == 0) { dup = 1; break; }
      if (dup) { free(url); continue; }
      if (contains(url, "img.shields.io") || contains(url, "cve.mitre.org")) {
        free(url);
        continue;
      }
      seen[sn] = url; /* dedupe set persists for this CVE only */
      sn++;
      const char *label = url + strlen("https://github.com/");

      cJSON *feat = cJSON_CreateObject();
      cJSON_AddStringToObject(feat, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(139.6917)); /* TOKYO */
      cJSON_AddItemToArray(co, cJSON_CreateNumber(35.6895));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(feat, "geometry", g);

      cJSON *pr = cJSON_CreateObject(); /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", cJSON_GetArraySize(features));
      cJSON_AddStringToObject(pr, "cve_id", cveId);
      cJSON_AddStringToObject(pr, "repo_label", label);
      cJSON_AddStringToObject(pr, "repo_url", url);
      cJSON_AddStringToObject(pr, "source", "trickest_cve");
      cJSON_AddItemToObject(feat, "properties", pr);
      cJSON_AddItemToArray(features, feat);
    }
    for (int s = 0; s < sn; s++) free((void *)seen[s]);
    free(md);
  }

  free(names);
  free(paths);
  cJSON_Delete(listing);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[trickest-cve] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def trickest_cve_def = {
  .id = "trickest-cve", .collector = "cyber",
  .name = "Trickest CVE", .name_ja = "Trickest CVE",
   .update_interval_sec = 21600, .run = run,
};
REGISTER_SOURCE(trickest_cve_def)
