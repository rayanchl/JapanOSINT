/* collectors/sources/global_whatsmyname.c
 * OSINT service — WHATSMYNAME. On-demand username pivot (entity = username).
 * Fetches the community WhatsMyName dataset (wmn-data.json) once, then probes a
 * CAPPED subset (~40) of the dataset's sites: for each, a REAL HTTP GET of the
 * site's uri_check with <entity> substituted for {account}, emitting a hit only
 * when the response actually matches the site's e_code/e_string rule (same
 * detection contract maigret.c uses). Real probes only — no synthesized rows;
 * if nothing matches, emits nothing (honest empty). LIVE but heavy, so the site
 * count is hard-capped. Free / keyless. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define WMN_DATA_URL \
  "https://raw.githubusercontent.com/WebBreacher/WhatsMyName/main/wmn-data.json"
#define WMN_MAX_SITES 40

/* Substitute the first "{account}" placeholder in `tmpl` with `user`. */
static void wmn_fill(char *dst, size_t cap, const char *tmpl, const char *user) {
  const char *pos = strstr(tmpl, "{account}");
  if (!pos) { snprintf(dst, cap, "%s", tmpl); return; }
  size_t pre = (size_t)(pos - tmpl);
  if (pre >= cap) pre = cap - 1;
  memcpy(dst, tmpl, pre);
  dst[pre] = '\0';
  strncat(dst, user, cap - strlen(dst) - 1);
  strncat(dst, pos + 9 /*strlen("{account}")*/, cap - strlen(dst) - 1);
}

/* Emit one confirmed profile hit. Returns 1 if emitted. */
static int wmn_emit(intel_sink *sink, const char *username, const char *site,
                    const char *cat, const char *url) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "site", site);
  cJSON_AddStringToObject(data, "url", url);
  if (cat) cJSON_AddStringToObject(data, "category", cat);
  cJSON_AddStringToObject(data, "username", username);
  cJSON_AddStringToObject(data, "source", "WhatsMyName");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WHATSMYNAME");
  cJSON_AddStringToObject(props, "entity", username);
  cJSON_AddStringToObject(props, "site", site);
  if (cat) cJSON_AddStringToObject(props, "category", cat);
  cJSON_AddStringToObject(props, "href", url);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char rk[512];
  snprintf(rk, sizeof rk, "profile:%s:%s", site, username);
  char title[512];
  snprintf(title, sizeof title, "%s: %s", site, username);
  char summary[512];
  snprintf(summary, sizeof summary, "%s on %s", username, site);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.summary         = summary;
  it.body            = bj;
  it.link            = url;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WHATSMYNAME\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *username = ctx->entity;
  if (!username || !*username) return -1;

  /* 1) Fetch the dataset once. */
  char *body = jo_get(ctx, WMN_DATA_URL, NULL, "whatsmyname");
  if (!body) return 0;                     /* honest empty on fetch failure */
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *sites = cJSON_GetObjectItem(root, "sites");
  if (!cJSON_IsArray(sites)) { cJSON_Delete(root); return 0; }

  int emitted = 0, probed = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, sites) {
    if (probed >= WMN_MAX_SITES) break;
    if (ctx->cancel && *ctx->cancel) break;

    const char *name  = jo_sv(s, "name");
    const char *check = jo_sv(s, "uri_check");
    const char *estr  = jo_sv(s, "e_string");
    const char *cat   = jo_sv(s, "cat");
    cJSON *ecode = cJSON_GetObjectItem(s, "e_code");
    if (!name || !check || !ecode || !cJSON_IsNumber(ecode)) continue;
    if (!strstr(check, "{account}")) continue;

    char url[1024];
    wmn_fill(url, sizeof url, check, username);
    probed++;

    http_response hr = {0};
    int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);

    int found = 0;
    if (hc == 0 && hr.body) {
      /* WhatsMyName hit rule: HTTP status == e_code AND (no e_string OR the
       * e_string is present in the body). Both conditions are real signals. */
      if ((int)hr.status == ecode->valueint) {
        found = (!estr || strstr(hr.body, estr) != NULL);
      }
    }
    http_response_free(&hr);

    if (found) emitted += wmn_emit(sink, username, name, cat, url);
  }

  cJSON_Delete(root);
  fprintf(stderr, "[whatsmyname] probed %d emitted %d\n", probed, emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def whatsmyname_def = {
  .id = "WHATSMYNAME", .collector = "osint",
  .name = "WhatsMyName Username Check", .name_ja = "WhatsMyName ユーザー名調査",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "internal://osint/whatsmyname",
  .description = "WhatsMyName dataset — live username-presence probes across a capped site subset (keyless)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(whatsmyname_def)
