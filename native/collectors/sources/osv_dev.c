/* collectors/cyber/sources/osv_dev.c
 * Port of server/src/collectors/osvDev.js.
 * POST https://api.osv.dev/v1/querybatch (no auth) with {queries:[{package:
 * {name,ecosystem}}]} for JP-relevant packages. results[qi].vulns[:6] →
 * TOKYO Point features. No SEED branch (always live → source "osv_querybatch").
 */
#include "../../source.h"
#include "../../lib/geojson.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://api.osv.dev/v1/querybatch"
#define TIMEOUT_MS 15000


/* DEFAULT_PACKAGES (env OSV_PACKAGES overrides, ';'-joined). */
static const char *const SEED_PACKAGES[] = {
  "npm:misskey-js",
  "npm:misskey-reversi",
  "npm:cybozu-front-lib",
  "npm:typescript",
  "PyPI:django",
  "PyPI:requests",
  "Go:github.com/yuin/goldmark",
  "Go:github.com/labstack/echo",
  "Go:github.com/gorilla/mux",
  "RubyGems:rails",
  "RubyGems:devise",
  "RubyGems:nokogiri",
  "Maven:com.fasterxml.jackson.core:jackson-databind",
  "Packagist:laravel/framework",
  "crates.io:tokio",
};

/* split spec at first ':' → ecosystem / name(rest joined by ':'). */
static void parse_pkg(const char *spec, char *eco, size_t en,
                      const char **name) {
  const char *colon = strchr(spec, ':');
  if (!colon) { snprintf(eco, en, "%s", spec); *name = spec + strlen(spec); return; }
  size_t l = (size_t)(colon - spec);
  if (l >= en) l = en - 1;
  memcpy(eco, spec, l); eco[l] = 0;
  *name = colon + 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* Build package list: OSV_PACKAGES (split on ';', trim, drop empty) or seed. */
  const char *env = getenv("OSV_PACKAGES");
  char **pkgs = NULL;
  int npkg = 0;
  char *envcopy = NULL;
  if (env && *env) {
    envcopy = strdup(env);
    int cap = 8;
    pkgs = malloc(sizeof(char *) * (size_t)cap);
    char *p = envcopy;
    while (p) {
      char *semi = strchr(p, ';');
      if (semi) *semi = 0;
      char *s = p;
      while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
      size_t L = strlen(s);
      while (L && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='\n'||s[L-1]=='\r')) s[--L]=0;
      if (*s) {
        if (npkg == cap) { cap *= 2; pkgs = realloc(pkgs, sizeof(char *) * (size_t)cap); }
        pkgs[npkg++] = s;
      }
      p = semi ? semi + 1 : NULL;
    }
  }
  int seed = 0;
  if (npkg == 0) {
    seed = 1;
    npkg = (int)(sizeof SEED_PACKAGES / sizeof SEED_PACKAGES[0]);
  }

  /* Request body: { queries: [ { package: { name, ecosystem } }, ... ] }. */
  cJSON *req = cJSON_CreateObject();
  cJSON *queries = cJSON_AddArrayToObject(req, "queries");
  for (int i = 0; i < npkg; i++) {
    const char *spec = seed ? SEED_PACKAGES[i] : pkgs[i];
    char eco[64]; const char *name;
    parse_pkg(spec, eco, sizeof eco, &name);
    cJSON *q = cJSON_CreateObject();
    cJSON *pk = cJSON_AddObjectToObject(q, "package");
    cJSON_AddStringToObject(pk, "name", name);
    cJSON_AddStringToObject(pk, "ecosystem", eco);
    cJSON_AddItemToArray(queries, q);
  }
  char *body = cJSON_PrintUnformatted(req);
  cJSON_Delete(req);

  const char *hdrs[] = { "Content-Type: application/json", NULL };
  cJSON *json = feed_post_json(ctx->http, API_URL, body, hdrs, TIMEOUT_MS);
  free(body);

  cJSON *features = cJSON_CreateArray();
  cJSON *results = json ? cJSON_GetObjectItem(json, "results") : NULL;
  if (cJSON_IsArray(results)) {
    int qi = 0;
    cJSON *group;
    cJSON_ArrayForEach(group, results) {
      const char *pkg = (qi < npkg)
        ? (seed ? SEED_PACKAGES[qi] : (const char *)pkgs[qi]) : NULL;
      cJSON *vulns = cJSON_GetObjectItem(group, "vulns");
      if (cJSON_IsArray(vulns)) {
        int vi = 0;
        cJSON *v;
        cJSON_ArrayForEach(v, vulns) {
          if (vi >= 6) break;                    /* slice(0,6) */
          cJSON *f = cJSON_CreateObject();
          cJSON_AddStringToObject(f, "type", "Feature");
          /* NO GEOMETRY. Every row here used to be emitted at Tokyo Station
           * (35.6895, 139.6917). What this source reports has no location,
           * and stacking every row on one pin is the fabrication the
           * 2026-07-31 audit deleted from cisa-kev-jp, poc-in-github and
           * peeringdb-jp. Confirmed live by tests/contract/source_contract.py.
           * lib/geojson.c handles an absent geometry correctly — do NOT
           * reintroduce a fallback coordinate. */

          cJSON *p = cJSON_CreateObject();       /* EXACT JS key order */
          cJSON_AddNumberToObject(p, "idx", cJSON_GetArraySize(features));
          cJSON *id = cJSON_GetObjectItem(v, "id");
          cJSON_AddItemToObject(p, "osv_id",
            (id && cJSON_IsString(id) && id->valuestring[0])
              ? cJSON_CreateString(id->valuestring) : cJSON_CreateNull());
          cJSON *mod = cJSON_GetObjectItem(v, "modified");
          cJSON_AddItemToObject(p, "modified",
            (mod && cJSON_IsString(mod) && mod->valuestring[0])
              ? cJSON_CreateString(mod->valuestring) : cJSON_CreateNull());
          cJSON_AddItemToObject(p, "package",
            pkg ? cJSON_CreateString(pkg) : cJSON_CreateNull());
          cJSON_AddStringToObject(p, "source", "osv_querybatch");
          /* Readable identity + provenance. geojson_emit_features maps
           * properties.title/link/published_at onto the row; without them the
           * advisory has no heading and no way back to osv.dev. Built purely
           * from the querybatch response. */
          {
            const char *osvid = (id && cJSON_IsString(id) && id->valuestring[0])
                              ? id->valuestring : NULL;
            char title[256];
            if (osvid && pkg) snprintf(title, sizeof title, "%s — %s", osvid, pkg);
            else if (osvid)   snprintf(title, sizeof title, "%s", osvid);
            else if (pkg)     snprintf(title, sizeof title, "OSV advisory — %s", pkg);
            else              snprintf(title, sizeof title, "OSV advisory");
            cJSON_AddStringToObject(p, "title", title);
            if (osvid) {
              char link[256];
              snprintf(link, sizeof link, "https://osv.dev/vulnerability/%s", osvid);
              cJSON_AddStringToObject(p, "link", link);
            }
            if (mod && cJSON_IsString(mod) && mod->valuestring[0])
              cJSON_AddStringToObject(p, "published_at", mod->valuestring);
          }
          cJSON_AddItemToObject(f, "properties", p);
          cJSON_AddItemToArray(features, f);
          vi++;
        }
      }
      qi++;
    }
  }
  if (json) cJSON_Delete(json);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  free(pkgs);
  free(envcopy);
  fprintf(stderr, "[osv-dev] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def osv_dev_def = {
  .id = "osv-dev", .collector = "cyber",
  .name = "OSV.dev (JP packages)", .name_ja = "OSV.dev (日本パッケージ)",
   .update_interval_sec = 21600, .run = run,
};
REGISTER_SOURCE(osv_dev_def)
