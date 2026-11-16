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
 * maigret.c, the sibling, walks its whole table; this does too, over a pool of
 * workers (each site is a different host, so core/hostgate.c paces per host),
 * under a wall-clock budget. A run the budget or a cancel stops files a
 * collector-truncation-notice with how many of how many were probed. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
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

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.summary         = summary;
  it.body = bj;
  it.link            = url;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WHATSMYNAME\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* The site walk is fanned out over WMN_WORKERS threads, as username_osint.c's
 * Sherlock port already does: every site is a different host, so a sequential
 * walk spent most of its 900 s waiting on one slow server at a time, and an
 * analyst's pivot came back after a quarter of an hour (review, 2026-11-08).
 * Each worker has its own client (a client is not shared across threads),
 * folded into ctx->http when it finishes so the run line still counts every
 * request; emit() and the counters are serialised under one mutex. */
#define WMN_WORKERS 12

typedef struct {
  cJSON **site;          /* the probeable sites, in dataset order */
  int n, next;           /* next is guarded by mu */
  pthread_mutex_t mu;
  const source_ctx *ctx;
  intel_sink *sink;
  const char *user;
  time_t t0;
  int probed, emitted, unanswered, stopped;
} wmn_work;

static void *wmn_worker(void *arg) {
  wmn_work *w = arg;
  http_client *h = http_client_new();
  if (!h) return NULL;
  for (;;) {
    pthread_mutex_lock(&w->mu);
    int i = -1;
    if (!w->stopped) {
      if ((w->ctx->cancel && *w->ctx->cancel) ||
          time(NULL) - w->t0 >= WMN_BUDGET_SEC)
        w->stopped = 1;
      else if (w->next < w->n) { i = w->next++; w->probed++; }
    }
    pthread_mutex_unlock(&w->mu);
    if (i < 0) break;

    cJSON *s = w->site[i];
    const char *name  = jo_sv(s, "name");
    const char *check = jo_sv(s, "uri_check");
    const char *estr  = jo_sv(s, "e_string");
    const char *cat   = jo_sv(s, "cat");
    int ecode = cJSON_GetObjectItem(s, "e_code")->valueint;

    char url[1024];
    wmn_fill(url, sizeof url, check, w->user);
    http_response hr = {0};
    int hc = http_request(h, "GET", url, NULL, NULL, 0, WMN_PROBE_MS, 1, &hr);
    /* WhatsMyName hit rule: HTTP status == e_code AND (no e_string OR the
     * e_string is present in the body). Both conditions are real signals. */
    int found = (hc == 0 && hr.body && (int)hr.status == ecode &&
                 (!estr || strstr(hr.body, estr) != NULL));
    http_response_free(&hr);

    pthread_mutex_lock(&w->mu);
    if (hc != 0) w->unanswered++;
    if (found) w->emitted += wmn_emit(w->sink, w->user, name, cat, url);
    pthread_mutex_unlock(&w->mu);
  }
  pthread_mutex_lock(&w->mu);
  http_client_absorb(w->ctx->http, h);
  pthread_mutex_unlock(&w->mu);
  http_client_free(h);
  return NULL;
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

  /* 2) The probeable sites: a name, a uri_check carrying {account}, and a
   * numeric e_code — the same test the count below has always used. */
  int cap = cJSON_GetArraySize(sites);
  wmn_work w = { .ctx = ctx, .sink = sink, .user = username, .t0 = time(NULL) };
  w.site = cap > 0 ? calloc((size_t)cap, sizeof *w.site) : NULL;
  if (cap > 0 && !w.site) { cJSON_Delete(root); return -1; }
  cJSON *s;
  cJSON_ArrayForEach(s, sites) {
    const char *c0 = jo_sv(s, "uri_check");
    if (jo_sv(s, "name") && c0 && strstr(c0, "{account}") &&
        cJSON_IsNumber(cJSON_GetObjectItem(s, "e_code")))
      w.site[w.n++] = s;
  }

  /* 3) Probe them. */
  pthread_mutex_init(&w.mu, NULL);
  pthread_t th[WMN_WORKERS];
  int spawned = 0;
  for (int i = 0; i < WMN_WORKERS && i < w.n; i++)
    if (pthread_create(&th[i], NULL, wmn_worker, &w) == 0) spawned++;
  if (spawned == 0 && w.n > 0) wmn_worker(&w);       /* no thread: run inline */
  for (int i = 0; i < spawned; i++) pthread_join(th[i], NULL);
  pthread_mutex_destroy(&w.mu);

  if (w.stopped)
    jo_trunc_notice_scoped(sink, "WHATSMYNAME", username, WMN_DATA_URL, w.probed,
                           w.n,
                           "the wall-clock budget or a cancel stopped the site walk; "
                           "records_used/records_available count SITES probed of sites probeable",
                           "raise WMN_BUDGET_SEC in global_whatsmyname.c, or re-ask");
  fprintf(stderr, "[whatsmyname] probed %d of %d emitted %d (%d unanswered)\n",
          w.probed, w.n, w.emitted, w.unanswered);
  free(w.site);
  cJSON_Delete(root);
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
