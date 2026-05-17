/* collectors/cyber/sources/poc_in_github.c
 * Port of server/src/collectors/pocInGithub.js.
 * GitHub contents API listing for current UTC year (fallback prev year),
 * filter CVE-YYYY-N.json files, sort by name desc, slice 60, fetch each raw
 * JSON (array of repos), take first 5 repos per CVE → one Tokyo-pinned
 * feature each. _meta dropped; SAMPLE_LIMIT env override not ported. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define REPO_API "https://api.github.com/repos/nomi-sec/PoC-in-GitHub/contents"
#define RAW_BASE "https://raw.githubusercontent.com/nomi-sec/PoC-in-GitHub/master"
#define SAMPLE_LIMIT 60
#define TOKYO_LON 139.6917
#define TOKYO_LAT 35.6895

/* /^CVE-\d{4}-\d+\.json$/ */
static int is_cve_json(const char *s) {
  if (!s) return 0;
  if (strncmp(s, "CVE-", 4)) return 0;
  const char *p = s + 4;
  int d = 0;
  while (*p >= '0' && *p <= '9') { p++; d++; }
  if (d != 4 || *p != '-') return 0;
  p++;
  d = 0;
  while (*p >= '0' && *p <= '9') { p++; d++; }
  if (d < 1) return 0;
  return strcmp(p, ".json") == 0;
}

/* a.name; for qsort descending localeCompare-ish (byte compare, JS uses
 * localeCompare but CVE ids are ASCII so byte order matches well enough). */
static int cmp_desc(const void *a, const void *b) {
  return strcmp(*(const char *const *)b, *(const char *const *)a);
}

static const char *sv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

/* repo.X ?? null  numeric */
static cJSON *num_or_null(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (v && cJSON_IsNumber(v)) return cJSON_CreateNumber(cJSON_GetNumberValue(v));
  return cJSON_CreateNull();
}

/* repo.X || null  string */
static cJSON *str_or_null(const cJSON *o, const char *k) {
  const char *s = sv(o, k);
  return s ? cJSON_CreateString(s) : cJSON_CreateNull();
}

/* GitHub contents listing for a given year (with auth header if token). */
static cJSON *gh_listing(const source_ctx *ctx, int year) {
  char url[160];
  snprintf(url, sizeof url, "%s/%d", REPO_API, year);
  const char *tok = getenv("GITHUB_TOKEN");
  char authbuf[256];
  const char *hdrs_tok[] = {
    "accept: application/vnd.github+json",
    "user-agent: japanosint-collector",
    authbuf,
    NULL,
  };
  const char *hdrs_notok[] = {
    "accept: application/vnd.github+json",
    "user-agent: japanosint-collector",
    NULL,
  };
  const char *const *hdrs;
  if (tok && *tok) {
    snprintf(authbuf, sizeof authbuf, "authorization: Bearer %s", tok);
    hdrs = hdrs_tok;
  } else {
    hdrs = hdrs_notok;
  }
  return feed_get_json_h(ctx->http, url, hdrs, 15000);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL);
  struct tm tmv;
  gmtime_r(&now, &tmv);
  int year = tmv.tm_year + 1900;

  cJSON *listing = gh_listing(ctx, year);
  if (!cJSON_IsArray(listing) || cJSON_GetArraySize(listing) == 0) {
    if (listing) cJSON_Delete(listing);
    listing = gh_listing(ctx, year - 1);
  }
  if (!cJSON_IsArray(listing)) {
    if (listing) cJSON_Delete(listing);
    return -1;
  }

  /* filter type==file && CVE-YYYY-N.json, collect names */
  int cap = cJSON_GetArraySize(listing);
  char **names = calloc(cap > 0 ? cap : 1, sizeof *names);
  int nn = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, listing) {
    const char *type = sv(e, "type");
    const char *name = sv(e, "name");
    if (type && !strcmp(type, "file") && is_cve_json(name))
      names[nn++] = strdup(name);
  }
  cJSON_Delete(listing);

  qsort(names, nn, sizeof *names, cmp_desc);
  if (nn > SAMPLE_LIMIT) {
    for (int k = SAMPLE_LIMIT; k < nn; k++) free(names[k]);
    nn = SAMPLE_LIMIT;
  }

  cJSON *features = cJSON_CreateArray();
  for (int fi = 0; fi < nn; fi++) {
    const char *name = names[fi];
    /* RAW path: name.startsWith('CVE-') ? name.slice(4,8) : year */
    char yr[8];
    if (!strncmp(name, "CVE-", 4)) { memcpy(yr, name + 4, 4); yr[4] = 0; }
    else snprintf(yr, sizeof yr, "%d", year);
    char rawurl[256];
    snprintf(rawurl, sizeof rawurl, "%s/%s/%s", RAW_BASE, yr, name);
    cJSON *arr = feed_get_json(ctx->http, rawurl, 15000);
    if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) == 0) {
      if (arr) cJSON_Delete(arr);
      continue;
    }
    /* cveId = name without .json */
    char cveId[64];
    snprintf(cveId, sizeof cveId, "%.*s",
             (int)(strlen(name) - 5), name);   /* drop ".json" (5 chars) */

    int ri = 0;
    cJSON *repo;
    cJSON_ArrayForEach(repo, arr) {
      if (ri++ >= 5) break;                    /* slice(0,5) */
      /* description: (repo.description||'').slice(0,400) — always string */
      const char *desc = sv(repo, "description");
      char dbuf[420];
      if (desc) snprintf(dbuf, sizeof dbuf, "%.400s", desc);
      else dbuf[0] = 0;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LON));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LAT));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();          /* EXACT JS key order */
      cJSON_AddNumberToObject(p, "idx", cJSON_GetArraySize(features));
      cJSON_AddStringToObject(p, "cve_id", cveId);
      /* repo.full_name || repo.name || null */
      const char *rn = sv(repo, "full_name");
      if (!rn) rn = sv(repo, "name");
      cJSON_AddItemToObject(p, "repo_name",
        rn ? cJSON_CreateString(rn) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "repo_url", str_or_null(repo, "html_url"));
      cJSON_AddItemToObject(p, "stars", num_or_null(repo, "stargazers_count"));
      cJSON_AddItemToObject(p, "created", str_or_null(repo, "created_at"));
      cJSON_AddItemToObject(p, "updated", str_or_null(repo, "updated_at"));
      cJSON_AddStringToObject(p, "description", dbuf);
      cJSON_AddStringToObject(p, "source", "poc_in_github");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
    cJSON_Delete(arr);
  }

  for (int k = 0; k < nn; k++) free(names[k]);
  free(names);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[poc-in-github] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def poc_in_github_def = {
  .id = "poc-in-github", .collector = "cyber",
  .name = "PoC-in-GitHub (nomi-sec)", .name_ja = "PoC-in-GitHub",
   .update_interval_sec = 21600, .run = run,
};
REGISTER_SOURCE(poc_in_github_def)
