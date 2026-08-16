/* collectors/osint/sources/username_osint.c
 * OSINT service — port of OSINTsaas osint_tools/username_osint.c
 * (username_checker_service → handle_sherlock_search). Canonical SERVICE =
 * SHERLOCK_SEARCH — the FIRST registry name bound to handle_sherlock_search
 * (osint_dispatcher.c:149 {SERVICE_SHERLOCK_SEARCH, handle_sherlock_search,
 * "SHERLOCK_SEARCH", true}; WHATSMYNAME_SEARCH / USERNAME_CHECKER alias the
 * same handler/fn, so SHERLOCK_SEARCH is the dedup id). On-demand (interval 0);
 * ctx->entity = a username. No key. Probes the 511-entry platform table.
 *
 * SOFT-404 DIFFERENTIAL DETECTION: ~98% of the table uses `status_code`==200
 * detection, but many sites (Twitter, Instagram, Facebook, LinkedIn, …) return
 * HTTP 200 for ANY username — including ones that don't exist — especially
 * unauthenticated. Trusting status==200 alone yielded ~40% false positives. So
 * for a status_code==200 platform whose target returns 200, we issue a second
 * probe with a known-nonexistent CONTROL username; the hit is kept ONLY if the
 * control does NOT also return 200 (i.e. the site actually distinguishes real
 * users). message_check/response_text/regex checks additionally require a
 * non-error (2xx/3xx) page so 404/5xx error bodies can't register as hits.
 *
 * CONCURRENCY: a fixed worker pool fans the table across threads (each worker
 * owns its own http_client), turning a ~5-minute sequential scan into seconds.
 * sink->emit() is serialized under a mutex.
 *
 * PER-RECORD EMIT: emits ONE osint_service_result row per CONFIRMED-FOUND
 * profile (keyed "profile:<platform>:<username>"), carrying
 * platform/url/http_status/response_time_ms with link = the profile URL. No
 * confirmed hits → emits nothing (honest empty — no seeded rows). */
#include "source.h"
#include "social_fuse.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <time.h>
#include <pthread.h>

typedef struct {
  const char *name;
  const char *url_template;
  const char *check_type;        /* status_code|response_text|message_check|regex_check */
  int   expected_status;         /* status_code */
  const char *needle;            /* response_text/message_check NOT-FOUND text or regex */
  const char *category;
  int   enabled;
} platform_t;

static const platform_t PLATFORMS[] = {
#include "username_osint_platforms.inc"
};

/* Per-probe timeout. Lower than the old 12s so a few slow sites can't stall a
 * worker; with the pool this bounds total wall-clock to seconds. */
#define PROBE_TIMEOUT_MS 8000
#define NUM_WORKERS 12

/* Build the soft-404 control username: a same-length, same-charset transform of
 * the target (rot13 letters, +5 mod 10 digits) so site-side length/format
 * validation treats it identically — isolating the "does THIS handle exist"
 * signal from "is this a well-formed handle". The transformed string is almost
 * certainly nonexistent. */
static void make_control(char *dst, size_t cap, const char *user) {
  size_t i = 0;
  for (; user[i] && i + 1 < cap; i++) {
    unsigned char c = (unsigned char)user[i];
    if (c >= 'a' && c <= 'z')      dst[i] = 'a' + (c - 'a' + 13) % 26;
    else if (c >= 'A' && c <= 'Z') dst[i] = 'A' + (c - 'A' + 13) % 26;
    else if (c >= '0' && c <= '9') dst[i] = '0' + (c - '0' + 5) % 10;
    else                            dst[i] = c;
  }
  dst[i] = 0;
  /* Guard: if the transform is a no-op (e.g. all punctuation), nudge it so it
   * differs from the real handle. */
  if (strcmp(dst, user) == 0 && i + 1 < cap) { dst[i] = 'q'; dst[i + 1] = 0; }
}

static void fill_url(char *dst, size_t cap, const char *tmpl, const char *user) {
  const char *ph = strstr(tmpl, "{}");
  if (ph) {
    size_t pre = (size_t)(ph - tmpl);
    if (pre >= cap) pre = cap - 1;
    memcpy(dst, tmpl, pre);
    dst[pre] = 0;
    strncat(dst, user, cap - strlen(dst) - 1);
    strncat(dst, ph + 2, cap - strlen(dst) - 1);
  } else {
    strncpy(dst, tmpl, cap - 1);
    dst[cap - 1] = 0;
  }
}

/* Emit one intel row for a single confirmed-found profile. Returns 1 if emitted. */
static int emit_profile(intel_sink *sink, const char *user,
                        const platform_t *p, const char *url,
                        long status, double ms) {
  /* Per-record body: the actual platform hit. */
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "platform", p->name);
  cJSON_AddStringToObject(data, "category", p->category);
  cJSON_AddStringToObject(data, "profile_url", url);
  cJSON_AddNumberToObject(data, "http_status", (double)status);
  cJSON_AddNumberToObject(data, "response_time_ms", ms);
  cJSON_AddStringToObject(data, "username", user);
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SHERLOCK_SEARCH");
  cJSON_AddStringToObject(props, "entity", user);
  cJSON_AddStringToObject(props, "platform", p->name);
  cJSON_AddStringToObject(props, "category", p->category);
  char *pj = cJSON_PrintUnformatted(props);

  /* Stable per-profile key = platform + username (unique within this service). */
  char rk[400]; snprintf(rk, sizeof rk, "profile:%s:%s", p->name, user);
  char title[400]; snprintf(title, sizeof title, "%s: %s", p->name, user);
  char summary[400]; snprintf(summary, sizeof summary, "%s on %s", user, p->name);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = summary;
  it.link            = url;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SHERLOCK_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* Probe one platform for `user` using worker-private http_client `h`.
 * Returns 1 if the username is confirmed present, 0 otherwise. On a hit,
 * fills `url` (caller-owned, urlcap bytes) and *out_status / *out_ms. */
static int detect_platform(http_client *h, const platform_t *p, const char *user,
                           char *url, size_t urlcap, long *out_status, double *out_ms) {
  fill_url(url, urlcap, p->url_template, user);

  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  http_response hr = {0};
  int hc = http_request(h, "GET", url, NULL, NULL, 0, PROBE_TIMEOUT_MS, 0, &hr);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  *out_ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;

  if (hc != 0 || !hr.body) { *out_status = hr.status; http_response_free(&hr); return 0; }
  long status = hr.status;
  *out_status = status;
  int ok_page = (status >= 200 && status < 400 && hr.body[0]);
  int found = 0;

  if (strcmp(p->check_type, "status_code") == 0) {
    if (status == p->expected_status) {
      if (p->expected_status == 200) {
        /* Differential soft-404 guard: a site that returns 200 for a
         * known-nonexistent control username can't distinguish users, so a
         * 200 here is meaningless. Keep the hit only if the control differs. */
        char control[256]; make_control(control, sizeof control, user);
        char curl[1024];
        fill_url(curl, sizeof curl, p->url_template, control);
        http_response cr = {0};
        int chc = http_request(h, "GET", curl, NULL, NULL, 0, PROBE_TIMEOUT_MS, 0, &cr);
        long cstatus = (chc == 0) ? cr.status : 0;
        http_response_free(&cr);
        /* Keep the hit only if the control got a CLEAN NEGATIVE — a redirect or
         * client error that means "no such user" (3xx/4xx). A control that also
         * returns 200 (soft-404), or that was throttled / blocked / errored
         * (429, 0/transport, 5xx, 403), is inconclusive → suppress. */
        found = (cstatus >= 300 && cstatus < 500 && cstatus != 429 && cstatus != 403);
      } else {
        found = 1; /* a specific non-200 expected status is already discriminating */
      }
    }
  } else if (strcmp(p->check_type, "response_text") == 0 ||
             strcmp(p->check_type, "message_check") == 0) {
    /* Found = the NOT-FOUND needle is absent — but only on a real page. */
    found = (ok_page && p->needle && *p->needle) ? (strstr(hr.body, p->needle) == NULL) : 0;
  } else if (strcmp(p->check_type, "regex_check") == 0) {
    regex_t re;
    if (ok_page && p->needle && regcomp(&re, p->needle, REG_EXTENDED) == 0) {
      found = (regexec(&re, hr.body, 0, NULL, 0) == 0);
      regfree(&re);
    }
  }

  http_response_free(&hr);
  return found;
}

/* Shared work-queue + emit guard for the worker pool. */
typedef struct {
  const int *idx;            /* indices into PLATFORMS[] of enabled rows */
  int n;
  int next;                  /* protected by mu */
  pthread_mutex_t mu;        /* guards `next` and sink->emit */
  const char *user;
  intel_sink *sink;
  volatile int *cancel;
} work_t;

static void *worker(void *arg) {
  work_t *w = (work_t *)arg;
  http_client *h = http_client_new();
  if (!h) return NULL;

  for (;;) {
    if (w->cancel && *w->cancel) break;
    pthread_mutex_lock(&w->mu);
    int i = (w->next < w->n) ? w->next++ : -1;
    pthread_mutex_unlock(&w->mu);
    if (i < 0) break;

    const platform_t *p = &PLATFORMS[w->idx[i]];
    char url[1024]; long status = 0; double ms = 0;
    if (detect_platform(h, p, w->user, url, sizeof url, &status, &ms)) {
      pthread_mutex_lock(&w->mu);
      emit_profile(w->sink, w->user, p, url, status, ms);
      pthread_mutex_unlock(&w->mu);
    }
  }

  http_client_free(h);
  return NULL;
}

int jo_sherlock_run(const source_ctx *ctx, intel_sink *sink) {
  const char *user = ctx->entity;
  if (!user || !*user) return -1;

  int total = (int)(sizeof PLATFORMS / sizeof PLATFORMS[0]);
  int idx[sizeof PLATFORMS / sizeof PLATFORMS[0]];
  int n = 0;
  for (int i = 0; i < total; i++) if (PLATFORMS[i].enabled) idx[n++] = i;
  if (n == 0) return 0;

  work_t w = { .idx = idx, .n = n, .next = 0, .user = user,
               .sink = sink, .cancel = ctx->cancel };
  pthread_mutex_init(&w.mu, NULL);

  int nt = NUM_WORKERS < n ? NUM_WORKERS : n;
  pthread_t th[NUM_WORKERS];
  int spawned = 0;
  for (int i = 0; i < nt; i++)
    if (pthread_create(&th[i], NULL, worker, &w) == 0) spawned++;

  if (spawned == 0) {
    /* Fallback: run inline on this thread if no worker could start. */
    worker(&w);
  } else {
    for (int i = 0; i < spawned; i++) pthread_join(th[i], NULL);
  }

  pthread_mutex_destroy(&w.mu);
  /* Honest empty: no confirmed profiles → emit nothing (return 0 either way). */
  return 0;
}

/* Fused into SOCIAL_USERNAME — exposed via social_fuse.h as jo_sherlock_run. */
