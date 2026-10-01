/* collectors/cyber/sources/ghsa_advisories.c
 * Port of server/src/collectors/ghsaAdvisories.js (createThreatIntelCollector).
 * Keyless (optional GITHUB_TOKEN env raises rate limit). GHSA REST, recent
 * 100, TOKYO points; ecosystems = unique non-falsy package.ecosystem. */
#include "source.h"
#include "lib/threatintel.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GHSA_URL "https://api.github.com/advisories?per_page=100&sort=published&direction=desc"

/* JS `a.k || null` */
static void put_or_null(cJSON *p, const char *k, cJSON *r, const char *ik) {
  cJSON *v = cJSON_GetObjectItem(r, ik);
  if (!v || cJSON_IsNull(v) || (cJSON_IsString(v) && !v->valuestring[0]))
    cJSON_AddItemToObject(p, k, cJSON_CreateNull());
  else
    cJSON_AddItemToObject(p, k, cJSON_Duplicate(v, 1));
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)key; (void)ud;
  const char *tok = getenv("GITHUB_TOKEN");
  char authh[300];
  const char *hdrs_tok[] = {
    "accept: application/vnd.github+json",
    "X-GitHub-Api-Version: 2022-11-28",
    "user-agent: japanosint-collector",
    authh, NULL };
  const char *hdrs_notok[] = {
    "accept: application/vnd.github+json",
    "X-GitHub-Api-Version: 2022-11-28",
    "user-agent: japanosint-collector",
    NULL };
  const char *const *hdrs;
  if (tok && *tok) {
    snprintf(authh, sizeof authh, "authorization: Bearer %s", tok);
    hdrs = hdrs_tok;
  } else {
    hdrs = hdrs_notok;
  }
  cJSON *arr = feed_get_json_h(ctx->http, GHSA_URL, hdrs, 15000);
  if (!arr) return NULL;

  cJSON *features = cJSON_CreateArray();
  int i = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *a;
    cJSON_ArrayForEach(a, arr) {
      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      /* NO GEOMETRY. Every row here used to be emitted at Tokyo Station
       * (35.6895, 139.6917). What this source reports has no location,
       * and stacking every row on one pin is the fabrication the
       * 2026-07-31 audit deleted from cisa-kev-jp, poc-in-github and
       * peeringdb-jp. Confirmed live by tests/contract/source_contract.py.
       * lib/geojson.c handles an absent geometry correctly — do NOT
       * reintroduce a fallback coordinate. */

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", i);
      put_or_null(pr, "ghsa_id", a, "ghsa_id");
      put_or_null(pr, "cve_id", a, "cve_id");
      put_or_null(pr, "title", a, "summary");
      put_or_null(pr, "severity", a, "severity");
      /* a.cvss?.score ?? null */
      cJSON *cvss = cJSON_GetObjectItem(a, "cvss");
      cJSON *score = cvss ? cJSON_GetObjectItem(cvss, "score") : NULL;
      cJSON_AddItemToObject(pr, "cvss",
        (score && !cJSON_IsNull(score)) ? cJSON_Duplicate(score, 1)
                                        : cJSON_CreateNull());
      put_or_null(pr, "published", a, "published_at");
      put_or_null(pr, "updated", a, "updated_at");
      put_or_null(pr, "withdrawn", a, "withdrawn_at");
      put_or_null(pr, "url", a, "html_url");

      cJSON *ecos = cJSON_CreateArray();       /* unique, insertion-ordered */
      cJSON *vulns = cJSON_GetObjectItem(a, "vulnerabilities");
      if (cJSON_IsArray(vulns)) {
        cJSON *v;
        cJSON_ArrayForEach(v, vulns) {
          cJSON *pkg = cJSON_GetObjectItem(v, "package");
          cJSON *eco = pkg ? cJSON_GetObjectItem(pkg, "ecosystem") : NULL;
          if (!eco || !cJSON_IsString(eco) || !eco->valuestring[0]) continue;
          int dup = 0;
          cJSON *e;
          cJSON_ArrayForEach(e, ecos) {
            if (cJSON_IsString(e) &&
                strcmp(e->valuestring, eco->valuestring) == 0) { dup = 1; break; }
          }
          if (!dup) cJSON_AddItemToArray(ecos, cJSON_CreateString(eco->valuestring));
        }
      }
      cJSON_AddItemToObject(pr, "ecosystems", ecos);
      cJSON_AddStringToObject(pr, "source", "github_ghsa_rest");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  cJSON_Delete(arr);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, NULL, NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def ghsa_advisories_def = {
  .id = "ghsa-advisories", .collector = "cyber",
  .name = "GitHub Security Advisories", .name_ja = "GitHub セキュリティアドバイザリ",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(ghsa_advisories_def)
