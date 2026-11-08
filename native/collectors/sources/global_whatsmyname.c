/* collectors/sources/global_whatsmyname.c
 * OSINT service — WHATSMYNAME. On-demand username pivot (entity = username).
 * Fetches the community WhatsMyName dataset (wmn-data.json) once, then probes
 * EVERY site in it: for each, a REAL HTTP GET of the
 * site's uri_check with <entity> substituted for {account}, emitting a hit only
 * when the response actually matches the site's e_code/e_string rule (same
 * detection contract maigret.c uses). Real probes only — no synthesized rows;
 * if nothing matches, emits nothing (honest empty). Free / keyless.
 *
 * UNCAPPED 2026-11-07. `WMN_MAX_SITES 40` probed the first 40 sites of the
 * dataset in file order — measured that day, 692 sites of which 670 carry a
 * `{account}` uri_check and a numeric e_code; Reddit is #260 and Instagram
 * #460, so neither was ever asked — and said nothing about the other 630.
 * maigret.c, the sibling, walks its whole table; this does too, sequentially
 * (each site is a different host, so core/hostgate.c paces per host), under a
 * wall-clock budget. A run the budget or a cancel stops files a
 * collector-truncation-notice with how many of how many were probed. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

#define WMN_DATA_URL \
  "https://raw.githubusercontent.com/WebBreacher/WhatsMyName/main/wmn-data.json"
#define WMN_BUDGET_SEC 900   /* exhaustive-ok: a stop at this wall-clock budget files a collector-truncation-notice */
#define WMN_PROBE_MS   10000

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
  jo_props_absorb(props, bj); char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char rk[512];
  snprintf(rk, sizeof rk, "profile:%s:%s", site, username);
  char title[512];
  snprintf(title, sizeof title, "%s: %s", site, username);
  char summary[512];
  snprintf(summary, sizeof summary, "%s on %s", username, site);

  char *bt = jo_values_text(bj);   /* text body; fields in properties (2026-11-07) */
  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.summary         = summary;
  it.body = bt ? bt : bj;
  it.link            = url;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WHATSMYNAME\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(bt); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *username = ctx->entity;
  if (!username || !*username) return -1;

  /* 1) Fetch the dataset once. */
  char *body = jo_get(ctx, WMN_DATA_URL, NULL, "whatsmyname");
  if (!body) return jo_fail_rc(jo_last_status(), 0);                     /* 2026-11-07: a refused fetch is -1, not an honest empty */
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return -1;

  cJSON *sites = cJSON_GetObjectItem(root, "sites");
  if (!cJSON_IsArray(sites)) { cJSON_Delete(root); return -1; }  /* the dataset's own shape changed */

  int emitted = 0, probed = 0, probeable = 0, stopped = 0, unanswered = 0;
  time_t t0 = time(NULL);
  cJSON *s;
  cJSON_ArrayForEach(s, sites) {
    {
      const char *c0 = jo_sv(s, "uri_check");
      cJSON *e0 = cJSON_GetObjectItem(s, "e_code");
      if (jo_sv(s, "name") && c0 && strstr(c0, "{account}") && cJSON_IsNumber(e0))
        probeable++;
    }
    if (stopped) continue;               /* keep counting what was not asked */
    if ((ctx->cancel && *ctx->cancel) || time(NULL) - t0 >= WMN_BUDGET_SEC) {
      stopped = 1;
      continue;
    }

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
    int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, WMN_PROBE_MS, 1, &hr);
    if (hc != 0) unanswered++;

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
  if (stopped)
    jo_trunc_notice_scoped(sink, "WHATSMYNAME", username, WMN_DATA_URL, probed,
                           probeable,
                           "the wall-clock budget or a cancel stopped the site walk; "
                           "records_used/records_available count SITES probed of sites probeable",
                           "raise WMN_BUDGET_SEC in global_whatsmyname.c, or re-ask");
  fprintf(stderr, "[whatsmyname] probed %d of %d emitted %d (%d unanswered)\n",
          probed, probeable, emitted, unanswered);
  return 0;   /* honest empty is not an error; the dataset fetch failing is -1 above */
}

static const source_def whatsmyname_def = {
  .id = "WHATSMYNAME", .collector = "osint_" "investigation",
  .name = "WhatsMyName Username Check", .name_ja = "WhatsMyName ユーザー名調査",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "internal://osint/whatsmyname",
  .description = "WhatsMyName dataset — live username-presence probes across every site in the dataset (keyless)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(whatsmyname_def)
