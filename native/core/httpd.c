#include "httpd.h"
#include "auth.h"
#include "intelapi.h"
#include "statusapi.h"
#include "miscapi.h"
#include "entityapi.h"
#include "tenantapi.h"
#include "alertsapi.h"
#include "keysapi.h"
#include "operatorgate.h"
#include "dbexplorerapi.h"
#include "maintenanceapi.h"
#include "geoproxyapi.h"
#include "sweepapi.h"
#include "searchapi.h"
#include "progress.h"
#include "dataapi.h"
#include "transitapi.h"
#include "camera_store.h"
#include "../third_party/mongoose.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include "scheduler.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static db_handle *g_db;

/* Node's new Date().toISOString(): YYYY-MM-DDTHH:MM:SS.mmmZ */
static void iso_now(char *buf, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000));
}

static void reply_json(struct mg_connection *c, int code, const char *body) {
  mg_http_reply(c, code,
    "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
    "%s", body);
}

/* GET /api/sources/stats — aggregate counts from the sources table. */
static void route_sources_stats(struct mg_connection *c) {
  int total = 0, online = 0, offline = 0;
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(g_db->h,
        "SELECT COUNT(*),"
        "SUM(status='online'),SUM(status='offline') FROM sources;",
        -1, &st, NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW) {
    total = sqlite3_column_int(st, 0);
    online = sqlite3_column_int(st, 1);
    offline = sqlite3_column_int(st, 2);
  }
  sqlite3_finalize(st);
  char body[256];
  snprintf(body, sizeof body,
           "{\"total\":%d,\"online\":%d,\"offline\":%d}", total, online, offline);
  reply_json(c, 200, body);
}

/* MG_EV_POLL drives the SSE probe (proves incremental streaming through the
 * C server). State packed in c->data: [0]=mode 'S', [1..4]=tick count,
 * [5..12]=next-due ms. */
static void sse_probe_start(struct mg_connection *c) {
  mg_printf(c,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/event-stream\r\n"
    "Cache-Control: no-cache, no-transform\r\n"
    "Connection: keep-alive\r\n"
    "X-Accel-Buffering: no\r\n\r\n");
  c->is_resp = 0;                 /* keep open for manual streaming */
  c->data[0] = 'S';
  *(int *)&c->data[1] = 0;
  *(uint64_t *)&c->data[8] = mg_millis();
}

static void sse_probe_poll(struct mg_connection *c) {
  if (c->data[0] != 'S') return;
  uint64_t now = mg_millis();
  if (now < *(uint64_t *)&c->data[8]) return;
  int n = *(int *)&c->data[1] + 1;
  *(int *)&c->data[1] = n;
  *(uint64_t *)&c->data[8] = now + 400;
  mg_printf(c, "event: tick\r\ndata: {\"n\":%d,\"ts\":%llu}\n\n",
            n, (unsigned long long)now);
  if (n >= 5) {
    mg_printf(c, "event: close\r\ndata: {}\n\n");
    c->is_draining = 1;
  }
}

/* /api/search/stream/:id — pre-auth SSE (UUID = capability). Port of
 * routes/search.js searchStreamHandler: JS 'update'/'done' events become a
 * polled snapshot on MG_EV_POLL (throttled 500ms), terminal close on done. */
#define SSTREAM_MAX 64
static struct { struct mg_connection *c; char id[40]; uint64_t next; } g_sstream[SSTREAM_MAX];

static void search_stream_open(struct mg_connection *c, const char *id) {
  mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
    "Cache-Control: no-cache, no-transform\r\nConnection: keep-alive\r\n"
    "X-Accel-Buffering: no\r\n\r\n");
  c->is_resp = 0;
  osint_request *rp = progress_get(id);
  if (!rp) { mg_printf(c, "event: error\r\ndata: {\"error\":\"not_found\"}\n\n");
             c->is_draining = 1; return; }
  char *snap = progress_to_json(rp);
  if (snap) { mg_printf(c, "event: progress\r\ndata: %s\n\n", snap); free(snap); }
  if (progress_is_done(rp)) {
    mg_printf(c, "event: close\r\ndata: {}\n\n"); c->is_draining = 1; return;
  }
  for (int i = 0; i < SSTREAM_MAX; i++) if (!g_sstream[i].c) {
    g_sstream[i].c = c;
    snprintf(g_sstream[i].id, sizeof g_sstream[i].id, "%s", id);
    g_sstream[i].next = mg_millis() + 500;
    break;
  }
}
static void search_stream_poll(struct mg_connection *c) {
  for (int i = 0; i < SSTREAM_MAX; i++) {
    if (g_sstream[i].c != c) continue;
    if (mg_millis() < g_sstream[i].next) return;
    g_sstream[i].next = mg_millis() + 500;
    osint_request *rp = progress_get(g_sstream[i].id);
    if (!rp) { g_sstream[i].c = NULL; c->is_draining = 1; return; }
    char *snap = progress_to_json(rp);
    if (snap) { mg_printf(c, "event: progress\r\ndata: %s\n\n", snap); free(snap); }
    if (progress_is_done(rp)) {
      mg_printf(c, "event: close\r\ndata: {}\n\n");
      g_sstream[i].c = NULL; c->is_draining = 1;
    }
    return;
  }
}
static void search_stream_close(struct mg_connection *c) {
  for (int i = 0; i < SSTREAM_MAX; i++) if (g_sstream[i].c == c) g_sstream[i].c = NULL;
}

static int starts(struct mg_str s, const char *p) {
  size_t n = strlen(p);
  return s.len >= n && memcmp(s.buf, p, n) == 0;
}
static int eq(struct mg_str s, const char *p) {
  return mg_strcmp(s, mg_str(p)) == 0;
}

/* Extract one URL-decoded path segment that sits between `pre` and `suf`
 * (suf "" = "to end of path"). e.g. pre="/api/sources/" suf="/logs" on
 * "/api/sources/jma-earthquake/logs" → out="jma-earthquake", returns 1.
 * Fails (0) if `pre` doesn't match, the segment is empty, contains '/'
 * (when suf==""), or the tail isn't exactly `suf`. */
static int seg(struct mg_str u, const char *pre, const char *suf,
                char *out, size_t cap) {
  size_t pl = strlen(pre), sl = strlen(suf);
  if (u.len < pl || memcmp(u.buf, pre, pl) != 0) return 0;
  size_t rem = u.len - pl;
  if (rem < sl) return 0;
  size_t mid = rem - sl;                       /* segment byte length */
  if (mid == 0) return 0;
  if (sl && memcmp(u.buf + pl + mid, suf, sl) != 0) return 0;
  char enc[1024];
  if (mid >= sizeof enc) return 0;
  memcpy(enc, u.buf + pl, mid);
  enc[mid] = 0;
  if (!suf[0] && memchr(enc, '/', mid)) return 0;   /* must be one segment */
  return mg_url_decode(enc, mid, out, cap, 0) > 0;
}

/* POST /api/intel/sources/:id/run single-flight guard (== Node `inFlight`
 * Set; manual-vs-manual only, like Node — cron coordination unneeded since the
 * intel_sink upsert is idempotent ON CONFLICT). */
static pthread_mutex_t g_runlock = PTHREAD_MUTEX_INITIALIZER;
static char g_running[16][80];
static int run_begin(const char *id) {
  int ok = 0, slot = -1;
  pthread_mutex_lock(&g_runlock);
  for (int i = 0; i < 16; i++) {
    if (!strcmp(g_running[i], id)) { slot = -2; break; }
    if (slot < 0 && !g_running[i][0]) slot = i;
  }
  if (slot >= 0) { snprintf(g_running[slot], 80, "%s", id); ok = 1; }
  pthread_mutex_unlock(&g_runlock);
  return ok;                                  /* 0 = in-flight (or table full) */
}
static void run_end(const char *id) {
  pthread_mutex_lock(&g_runlock);
  for (int i = 0; i < 16; i++)
    if (!strcmp(g_running[i], id)) { g_running[i][0] = 0; break; }
  pthread_mutex_unlock(&g_runlock);
}
static long long si_count(db_handle *db, const char *id) {
  sqlite3_stmt *s; long long n = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT COUNT(*) FROM intel_items WHERE source_id=?1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  return n;
}

/* POST /api/data/cameras/trigger — run the camera-discovery collector on a
 * detached thread (== Node withCollectorRun, not awaited), single-flighted
 * via run_begin/run_end keyed "camera-discovery". */
typedef struct { db_handle *db; const source_def *d; } cam_trig_arg;
static void *cam_trigger_thread(void *vp) {
  cam_trig_arg *a = vp;
  scheduler_run_source(a->db, a->d, NULL);
  run_end("camera-discovery");
  free(a);
  return NULL;
}

/* Parse the /api/intel/items|search query string into intel_items_query and
 * run it. Locals outlive the call (SQLITE_TRANSIENT copies bound values, the
 * envelope is malloc'd). mg_http_get_var > 0 == present & non-empty → NULL
 * means "filter not applied" (== Node `req.query.x ? String(x) : null`). */
static char *intel_items_run(struct mg_http_message *hm) {
  char src[160]={0}, q[256]={0}, lang[16]={0}, since[40]={0}, until[40]={0},
       rt[48]={0}, ssid[120]={0}, hg[8]={0}, tag[120]={0}, cur[768]={0},
       lim[16]={0};
  intel_items_query Q = {0};
  if (mg_http_get_var(&hm->query, "source",        src,  sizeof src ) > 0) Q.source = src;
  if (mg_http_get_var(&hm->query, "q",             q,    sizeof q   ) > 0) Q.q = q;
  if (mg_http_get_var(&hm->query, "lang",          lang, sizeof lang) > 0) Q.lang = lang;
  if (mg_http_get_var(&hm->query, "since",         since,sizeof since) > 0) Q.since = since;
  if (mg_http_get_var(&hm->query, "until",         until,sizeof until) > 0) Q.until = until;
  if (mg_http_get_var(&hm->query, "record_type",   rt,   sizeof rt  ) > 0) Q.record_type = rt;
  if (mg_http_get_var(&hm->query, "sub_source_id", ssid, sizeof ssid) > 0) Q.sub_source_id = ssid;
  if (mg_http_get_var(&hm->query, "has_geom",      hg,   sizeof hg  ) > 0) Q.has_geom = hg;
  if (mg_http_get_var(&hm->query, "tag",           tag,  sizeof tag ) > 0) Q.tag = tag;
  if (mg_http_get_var(&hm->query, "cursor",        cur,  sizeof cur ) > 0) Q.cursor = cur;
  if (mg_http_get_var(&hm->query, "limit",         lim,  sizeof lim ) > 0) Q.limit = atoi(lim);
  return intelapi_list_items(g_db, &Q);
}

static void fn(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_POLL) { sse_probe_poll(c); search_stream_poll(c); return; }
  if (ev == MG_EV_CLOSE) { search_stream_close(c); return; }
  if (ev != MG_EV_HTTP_MSG) return;
  struct mg_http_message *hm = ev_data;
  struct mg_str u = hm->uri;

  /* ---- pre-auth ---- */
  if (eq(u, "/api/health")) {
    char ts[40]; iso_now(ts, sizeof ts);
    char body[96];
    snprintf(body, sizeof body, "{\"status\":\"ok\",\"timestamp\":\"%s\"}", ts);
    reply_json(c, 200, body);
    return;
  }
  if (starts(u, "/api/_sse_probe")) { sse_probe_start(c); return; }

  /* SSE search stream is pre-auth: EventSource can't send Authorization;
   * the unguessable request_id is the capability (== routes/search.js). */
  { char sid[64];
    if (seg(u, "/api/search/stream/", "", sid, sizeof sid)) {
      search_stream_open(c, sid); return; } }

  /* /admin/break-glass/* is mounted OUTSIDE the /api auth gate (it exists
   * precisely for when Supabase auth is down). Only /login is implemented. */
  if (starts(u, "/admin/break-glass")) {
    if (!eq(u, "/admin/break-glass/login")) { reply_json(c, 404, "{\"error\":\"Not found\"}"); return; }
    char ua[512] = {0};
    struct mg_str *h = mg_http_get_header(hm, "User-Agent");
    if (h && h->len < sizeof ua) { memcpy(ua, h->buf, h->len); ua[h->len] = 0; }
    char ip[64] = {0};
    mg_snprintf(ip, sizeof ip, "%M", mg_print_ip, &c->rem);
    char *bdy = NULL;
    if (hm->body.len) { bdy = malloc(hm->body.len + 1);
      memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
    int status = 200;
    char *body = keysapi_breakglass(g_db, bdy, ip[0] ? ip : NULL,
                                    ua[0] ? ua : NULL, &status);
    free(bdy);
    reply_json(c, status, body ? body : "{\"error\":\"server_error\"}");
    free(body);
    return;
  }

  /* ---- everything else under /api/* passes the auth gate ---- */
  if (starts(u, "/api/")) {
    struct mg_str *h = mg_http_get_header(hm, "Authorization");
    char hdr[2048] = {0};
    if (h && h->len < sizeof hdr) { memcpy(hdr, h->buf, h->len); hdr[h->len] = 0; }
    auth_user usr;
    auth_result r = auth_check(h ? hdr : NULL, &usr);
    if (r != AUTH_ALLOW) { reply_json(c, auth_status(r), auth_body(r)); return; }

    /* ---- /api/search (port of routes/search.js; stream is pre-auth above) */
    if (eq(u, "/api/search/analyze")) {
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
      cJSON *jb = bdy ? cJSON_Parse(bdy) : NULL; free(bdy);
      cJSON *qj = jb ? cJSON_GetObjectItem(jb, "query") : NULL;
      cJSON *mr = jb ? cJSON_GetObjectItem(jb, "max_rounds") : NULL;
      char *body = searchapi_analyze(g_db,
        (qj && cJSON_IsString(qj)) ? qj->valuestring : NULL,
        (mr && cJSON_IsNumber(mr)) ? (int)mr->valuedouble : 0);
      if (jb) cJSON_Delete(jb);
      if (!body) { reply_json(c, 400, "{\"error\":\"query_required\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }
    if (eq(u, "/api/search/suggest")) {
      char q[512] = {0};
      mg_http_get_var(&hm->query, "q", q, sizeof q);
      char *body = searchapi_suggest(q);
      reply_json(c, 200, body); free(body); return;
    }
    { char rid[64];
      if (seg(u, "/api/search/results/", "", rid, sizeof rid)) {
        char *body = searchapi_results(g_db, rid);
        if (!body) { reply_json(c, 404, "{\"error\":\"not_found\"}"); return; }
        reply_json(c, 200, body); free(body); return;
      } }

    if (eq(u, "/api/sources/stats")) { route_sources_stats(c); return; }

    /* GET /api/sources — full source registry/probe rows. */
    if (eq(u, "/api/sources")) {
      char *body = api_sources_list(g_db);
      if (!body) { reply_json(c, 500, "{\"error\":\"Failed to list sources\"}"); return; }
      reply_json(c, 200, body);
      free(body);
      return;
    }

    /* GET /api/layers — Node throws (a registry source has layer:null →
     * null.replace) and returns 500; reproduce byte-for-byte. */
    if (eq(u, "/api/layers")) {
      reply_json(c, 500, "{\"error\":\"Failed to list layers\"}");
      return;
    }

    /* GET /api/status — per-source health + credential configuration. */
    if (eq(u, "/api/status")) {
      char *body = statusapi_build(g_db);
      if (!body) { reply_json(c, 500, "{\"error\":\"Failed to build API status\"}"); return; }
      reply_json(c, 200, body);
      free(body);
      return;
    }

    /* GET /api/intel/sources — registry × intel_items aggregates. */
    if (eq(u, "/api/intel/sources")) {
      char *body = intelapi_intel_sources(g_db);
      if (!body) { reply_json(c, 500, "{\"error\":\"failed_to_list_intel_sources\"}"); return; }
      reply_json(c, 200, body);
      free(body);
      return;
    }

    /* GET /api/intel/items — paginated feed; honors source/q/lang/since/
     * until/tag/record_type/sub_source_id/has_geom/cursor (intelStore
     * .listItems parity). Per-source drill-down depends on ?source=. */
    if (eq(u, "/api/intel/items")) {
      char *body = intel_items_run(hm);
      if (!body) { reply_json(c, 500, "{\"error\":\"failed_to_list_intel_items\"}"); return; }
      reply_json(c, 200, body);
      free(body);
      return;
    }

    /* GET /api/intel/items/:uid  (uid is URL-encoded; may contain '|',':') */
    if (starts(u, "/api/intel/items/") && u.len > 17) {
      char enc[600] = {0}, uid[600] = {0};
      size_t n = u.len - 17;
      if (n >= sizeof enc) n = sizeof enc - 1;
      memcpy(enc, u.buf + 17, n);
      mg_url_decode(enc, strlen(enc), uid, sizeof uid, 0);
      char *body = intelapi_item_by_uid(g_db, uid);
      if (!body) { reply_json(c, 404, "{\"error\":\"not_found\"}"); return; }
      reply_json(c, 200, body);
      free(body);
      return;
    }
    /* ---- P7 Wave 1: self-contained DB-read routes ---- */
    char p[1024];

    /* PUT /api/sources/:id/schedule  {"mode":"map_cron"|"search_only"} */
    if (seg(u, "/api/sources/", "/schedule", p, sizeof p)) {
      char b[256] = {0};
      if (hm->body.len && hm->body.len < sizeof b)
        memcpy(b, hm->body.buf, hm->body.len);
      char *body = miscapi_set_schedule(g_db, p, b);
      if (!body) { reply_json(c, 404, "{\"error\":\"Source not found\"}"); return; }
      if (!strcmp(body, "\1bad")) { free(body);
        reply_json(c, 400, "{\"error\":\"mode must be map_cron or search_only\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }

    /* GET /api/sources/:id/logs  (before /:id) */
    if (seg(u, "/api/sources/", "/logs", p, sizeof p)) {
      char lim[16] = {0};
      int hv = mg_http_get_var(&hm->query, "limit", lim, sizeof lim);
      char *body = miscapi_source_logs(g_db, p, hv > 0 ? atoi(lim) : 0);
      if (!body) { reply_json(c, 404, "{\"error\":\"Source not found\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }
    /* GET /api/sources/:id */
    if (seg(u, "/api/sources/", "", p, sizeof p)) {
      char *body = miscapi_source_by_id(g_db, p);
      if (!body) { reply_json(c, 404, "{\"error\":\"Source not found\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }

    /* GET /api/status/:id */
    if (seg(u, "/api/status/", "", p, sizeof p)) {
      char *body = statusapi_one(g_db, p);
      if (!body) { reply_json(c, 404, "{\"error\":\"Source not found\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }

    /* GET /api/layers/:layerId/geojson */
    if (seg(u, "/api/layers/", "/geojson", p, sizeof p)) {
      char *body = miscapi_layer_geojson(p);
      if (!body) { reply_json(c, 404, "{\"error\":\"Layer not found\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }

    /* GET /api/intel/search — alias of /api/intel/items */
    if (eq(u, "/api/intel/search")) {
      char *body = intel_items_run(hm);
      if (!body) { reply_json(c, 500, "{\"error\":\"failed_to_list_intel_items\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }

    /* POST /api/intel/sources/:id/run — port of routes/intel.js: trigger a
     * registered source NOW; its output upserts via the real intel_sink
     * (== Node mirrorCollectorOutput). registry_get→404 no_collector_
     * registered; single-flight→409 run_in_flight; run rc<0→500. `ingested`
     * = new-row delta (lower bound — idempotent upsert-updates aren't
     * counted; the C sink returns no per-run counts). */
    if (seg(u, "/api/intel/sources/", "/run", p, sizeof p)) {
      const source_def *d = registry_get(p);
      char b[256];
      if (!d) {
        snprintf(b, sizeof b,
          "{\"error\":\"no_collector_registered\",\"source_id\":\"%s\"}", p);
        reply_json(c, 404, b); return;
      }
      if (!run_begin(p)) {
        snprintf(b, sizeof b,
          "{\"error\":\"run_in_flight\",\"source_id\":\"%s\"}", p);
        reply_json(c, 409, b); return;
      }
      uint64_t t0 = mg_millis();
      long long before = si_count(g_db, p);
      int rc = scheduler_run_source(g_db, d, NULL);
      long long delta = si_count(g_db, p) - before;
      if (delta < 0) delta = 0;
      run_end(p);
      if (rc < 0) {
        snprintf(b, sizeof b,
          "{\"ran\":false,\"source_id\":\"%s\",\"error\":\"collector_run_failed\"}", p);
        reply_json(c, 500, b); return;
      }
      snprintf(b, sizeof b,
        "{\"ran\":true,\"source_id\":\"%s\",\"ingested\":%lld,"
        "\"duration_ms\":%llu,\"kind\":null,\"meta\":null}",
        p, delta, (unsigned long long)(mg_millis() - t0));
      reply_json(c, 200, b);
      return;
    }

    /* GET /api/follow/recent — platform-operator only. DIVERGES from Node
     * (index.js mounts /api/follow with NO requirePlatformOperator); gated
     * here by product decision, same opgate triad as /api/db,/api/admin. */
    if (eq(u, "/api/follow/recent")) {
      { int oc = opgate_check(&usr);
        if (oc == -401)  { reply_json(c,401,"{\"error\":\"Auth required\"}"); return; }
        if (oc == -1403) { reply_json(c,403,"{\"error\":\"Platform operator access not configured\"}"); return; }
        if (oc != 0)     { reply_json(c,403,"{\"error\":\"Platform operator role required\"}"); return; } }
      char lim[16] = {0};
      int hv = mg_http_get_var(&hm->query, "limit", lim, sizeof lim);
      char *body = miscapi_follow_recent(hv > 0 ? atoi(lim) : 500);
      reply_json(c, 200, body); free(body); return;
    }

    /* ---- /api/admin/* and /api/db/* — requirePlatformOperator ---- */
    if (eq(u,"/api/admin/restart") || eq(u,"/api/admin/maintenance") ||
        eq(u,"/api/db/tables") || starts(u,"/api/db/tables/") ||
        eq(u,"/api/db/scheduler")) {
      int oc = opgate_check(&usr);
      if (oc == -401) { reply_json(c,401,"{\"error\":\"Auth required\"}"); return; }
      if (oc == -1403) { reply_json(c,403,"{\"error\":\"Platform operator access not configured\"}"); return; }
      if (oc != 0)    { reply_json(c,403,"{\"error\":\"Platform operator role required\"}"); return; }

      if (eq(u,"/api/admin/restart")) {
        reply_json(c,200,"{\"ok\":true,\"restarting\":false}"); return;
      }
      if (eq(u,"/api/admin/maintenance")) {
        char hv[16]={0}; int hh=mg_http_get_var(&hm->query,"hours",hv,sizeof hv);
        char *b=maintenance_digest(g_db, hh>0?atoi(hv):24);
        reply_json(c,200,b); free(b); return;
      }
      if (eq(u,"/api/db/tables")) {
        char *b=dbexplorer_tables(g_db); reply_json(c,200,b); free(b); return;
      }
      if (eq(u,"/api/db/scheduler")) {
        char *b=dbexplorer_scheduler(g_db); reply_json(c,200,b); free(b); return;
      }
      /* /api/db/tables/:name */
      char tn[64]={0}, enc[128]={0};
      size_t off=strlen("/api/db/tables/");
      if (u.len>off){ size_t sl=u.len-off; if(sl<sizeof enc){
        memcpy(enc,u.buf+off,sl); mg_url_decode(enc,sl,tn,sizeof tn,0);} }
      char lv[16]={0},ov[16]={0},qv[256]={0},obv[64]={0},odv[8]={0};
      int hl=mg_http_get_var(&hm->query,"limit",lv,sizeof lv);
      int ho=mg_http_get_var(&hm->query,"offset",ov,sizeof ov);
      mg_http_get_var(&hm->query,"q",qv,sizeof qv);
      mg_http_get_var(&hm->query,"orderBy",obv,sizeof obv);
      mg_http_get_var(&hm->query,"orderDir",odv,sizeof odv);
      char *b=dbexplorer_table(g_db,tn,hl>0?atoi(lv):0,ho>0?atoi(ov):0,qv,obv,odv);
      if (!b){ reply_json(c,400,"{\"error\":\"unknown table\"}"); return; }
      reply_json(c,200,b); free(b); return;
    }

    /* ---- P7 Wave 4: /api/geocode + /api/plateau (under /api auth) ---- */
    if (eq(u,"/api/geocode")) {
      char qv[256]={0},av[256]={0};
      int hq=mg_http_get_var(&hm->query,"q",qv,sizeof qv);
      mg_http_get_var(&hm->query,"qAlt",av,sizeof av);
      char *qs=qv; while(*qs==' ')qs++; size_t ql=strlen(qs);
      while(ql&&qs[ql-1]==' ')qs[--ql]=0;
      char *as=av; while(*as==' ')as++; size_t al=strlen(as);
      while(al&&as[al-1]==' ')as[--al]=0;
      if (hq<=0||!*qs) { reply_json(c,400,"{\"error\":\"missing q parameter\"}"); return; }
      char *b=geoproxy_geocode_forward(qs,as);
      reply_json(c,200,b); free(b); return;
    }
    if (eq(u,"/api/geocode/reverse")) {
      char la[32]={0},lo[32]={0};
      int hla=mg_http_get_var(&hm->query,"lat",la,sizeof la);
      int hlo=mg_http_get_var(&hm->query,"lon",lo,sizeof lo);
      char *e1,*e2; double lat=strtod(la,&e1), lon=strtod(lo,&e2);
      if (hla<=0||hlo<=0||e1==la||e2==lo) { reply_json(c,400,"{\"error\":\"lat and lon required\"}"); return; }
      char *b=geoproxy_geocode_reverse(lat,lon);
      reply_json(c,200,b); free(b); return;
    }
    if (eq(u,"/api/plateau/tilesets")) {
      char lv[16]={0}; int hl=mg_http_get_var(&hm->query,"lod",lv,sizeof lv);
      int status=200; char *b=geoproxy_plateau_tilesets(hl>0?atoi(lv):1,&status);
      reply_json(c,status,b); free(b); return;
    }

    /* ---- P7 Wave 3b: /api/alerts/* (tenant-scoped rule CRUD) ---- */
    if (eq(u, "/api/alerts") || starts(u, "/api/alerts/")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }

      char aid[128] = {0}, act[32] = {0};
      if (u.len > 12) {                         /* after "/api/alerts/" */
        char rest[256] = {0};
        size_t rl = u.len - 12; if (rl >= sizeof rest) rl = sizeof rest - 1;
        memcpy(rest, u.buf + 12, rl);
        char *sl = strchr(rest, '/');
        if (sl) { *sl = 0; snprintf(act, sizeof act, "%s", sl + 1); }
        mg_url_decode(rest, strlen(rest), aid, sizeof aid, 0);
      }
      char meth[12] = {0};
      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
      memcpy(meth, hm->method.buf, ml);
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
      char lv[16] = {0};
      int hl = mg_http_get_var(&hm->query, "limit", lv, sizeof lv);
      int status = 200;
      char *body = alertsapi(g_db, tc.tenant_id, tc.user_id, meth,
                             aid, act, bdy, hl > 0 ? atoi(lv) : 0, &status);
      free(bdy);
      if (!body && status == 204) { mg_http_reply(c, 204, "", ""); return; }
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
    }

    /* ---- P7 Wave 3c: /api/keys + /api/tenant-keys (tenant-resolved) ---- */
    if (eq(u,"/api/keys") || starts(u,"/api/keys/") ||
        eq(u,"/api/tenant-keys") || starts(u,"/api/tenant-keys/")) {
      /* Platform keys (/api/keys*) → platform-operator only (product
       * decision; DIVERGES from Node, which mounts /api/keys with NO
       * requirePlatformOperator). /api/tenant-keys* keeps its existing
       * tenant-resolution guard so tenant admins retain BYOK self-service. */
      if (!starts(u,"/api/tenant-keys")) {
        int oc = opgate_check(&usr);
        if (oc == -401)  { reply_json(c,401,"{\"error\":\"Auth required\"}"); return; }
        if (oc == -1403) { reply_json(c,403,"{\"error\":\"Platform operator access not configured\"}"); return; }
        if (oc != 0)     { reply_json(c,403,"{\"error\":\"Platform operator role required\"}"); return; }
      }
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c,401,"{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c,500,"{\"error\":\"Tenant resolution failed\"}"); return; }
      char meth[12] = {0};
      size_t ml = hm->method.len < 11 ? hm->method.len : 11;
      memcpy(meth, hm->method.buf, ml);
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len+1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len]=0; }
      int status = 200; char *body;
      int tenantk = starts(u,"/api/tenant-keys");
      const char *base = tenantk ? "/api/tenant-keys" : "/api/keys";
      char seg[128] = {0};
      if (u.len > strlen(base) + 1) {
        size_t sl = u.len - strlen(base) - 1;
        char enc[256] = {0};
        if (sl < sizeof enc) { memcpy(enc, u.buf + strlen(base) + 1, sl);
          mg_url_decode(enc, sl, seg, sizeof seg, 0); }
      }
      if (tenantk) body = keysapi_tenant(g_db,&tc,meth,seg,bdy,&status);
      else         body = keysapi_platform(g_db,&tc,meth,seg,bdy,&status);
      free(bdy);
      if (!body) { reply_json(c,status,"{\"error\":\"server_error\"}"); return; }
      reply_json(c,status,body); free(body); return;
    }

    /* ---- P7 Wave 3a: tenant-resolved routes (/api/me, /api/audit) ---- */
    if (eq(u, "/api/me") || eq(u, "/api/audit") || eq(u, "/api/audit/verify")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len] = 0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }

      char *body;
      if (eq(u, "/api/me"))                body = tenantapi_me(g_db, &tc);
      else if (eq(u, "/api/audit/verify")) body = tenantapi_audit_verify(g_db, tc.tenant_id);
      else {
        char lv[16] = {0}, ov[16] = {0};
        int hl = mg_http_get_var(&hm->query, "limit", lv, sizeof lv);
        int ho = mg_http_get_var(&hm->query, "offset", ov, sizeof ov);
        body = tenantapi_audit_list(g_db, tc.tenant_id,
                                    hl > 0 ? atoi(lv) : 100, ho > 0 ? atoi(ov) : 0);
      }
      if (!body) { reply_json(c, 500, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }

    /* ---- P7 Wave 2: /api/entities/* (entity graph, pure SQLite) ---- */
    if (eq(u, "/api/entities/stats")) {
      char *body = entityapi_stats(g_db);
      reply_json(c, 200, body); free(body); return;
    }
    if (eq(u, "/api/entities/search")) {
      char qv[512] = {0}, tv[128] = {0}, lv[16] = {0};
      mg_http_get_var(&hm->query, "q", qv, sizeof qv);
      int ht = mg_http_get_var(&hm->query, "type", tv, sizeof tv);
      int hl = mg_http_get_var(&hm->query, "limit", lv, sizeof lv);
      /* trim q; empty → {"results":[]} (matches entities.js) */
      char *qs = qv; while (*qs == ' ') qs++;
      size_t ql = strlen(qs);
      while (ql && qs[ql-1] == ' ') qs[--ql] = 0;
      if (!*qs) { reply_json(c, 200, "{\"results\":[]}"); return; }
      char *body = entityapi_search(g_db, qs, ht > 0 ? tv : NULL,
                                    hl > 0 ? atoi(lv) : 0);
      if (!body) { reply_json(c, 500, "{\"error\":\"search_failed\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }
    if (starts(u, "/api/entities/")) {
      /* parse "/api/entities/<type>/<id>[/graph|/mentions]" */
      char raw[1024] = {0};
      size_t rl = u.len - 14;                 /* after "/api/entities/" */
      if (rl >= sizeof raw) rl = sizeof raw - 1;
      memcpy(raw, u.buf + 14, rl);
      char type[256] = {0}, eid[512] = {0}, tail[32] = {0};
      char *sl1 = strchr(raw, '/');
      if (sl1) {
        *sl1 = 0;
        char *rest = sl1 + 1;
        char *sl2 = strchr(rest, '/');
        if (sl2) { *sl2 = 0; snprintf(tail, sizeof tail, "%s", sl2 + 1); }
        mg_url_decode(raw, strlen(raw), type, sizeof type, 0);
        mg_url_decode(rest, strlen(rest), eid, sizeof eid, 0);
        char *body = NULL; int notfound = 0;
        if (tail[0] == 0) {
          body = entityapi_get(g_db, type, eid);
          notfound = !body;
        } else if (strcmp(tail, "graph") == 0) {
          char dv[16] = {0};
          int hd = mg_http_get_var(&hm->query, "depth", dv, sizeof dv);
          body = entityapi_graph(g_db, type, eid, hd > 0 ? atoi(dv) : 1);
          notfound = !body;
        } else if (strcmp(tail, "mentions") == 0) {
          char lv[16] = {0}, ov[16] = {0};
          int hl = mg_http_get_var(&hm->query, "limit", lv, sizeof lv);
          int ho = mg_http_get_var(&hm->query, "offset", ov, sizeof ov);
          body = entityapi_mentions(g_db, type, eid, hl > 0 ? atoi(lv) : 0,
                                    ho > 0 ? atoi(ov) : 0);
          notfound = !body;
        }
        if (notfound) { reply_json(c, 404, "{\"error\":\"not_found\"}"); return; }
        if (body) { reply_json(c, 200, body); free(body); return; }
      }
      reply_json(c, 404, "{\"error\":\"not_found\"}");
      return;
    }

    /* GET /api/data/cameras/discovery-feed — port of data.js getDiscoveryFeed
     * route. Explicit (the generic /api/data/ matcher below rejects a
     * 2-segment tail). Plain-auth, not operator-gated (== Node data router). */
    if (eq(u, "/api/data/cameras/discovery-feed")) {
      char lv[16]={0}, cv[1024]={0}, chv[256]={0};
      int hl = mg_http_get_var(&hm->query, "limit",   lv,  sizeof lv);
      int hc = mg_http_get_var(&hm->query, "cursor",  cv,  sizeof cv);
      int hh = mg_http_get_var(&hm->query, "channel", chv, sizeof chv);
      char *body = camera_discovery_feed(g_db, hl > 0 ? atoi(lv) : 500,
                     hc > 0 ? cv : NULL, hh > 0 ? chv : NULL);
      if (!body) { reply_json(c, 500,
        "{\"error\":\"Failed to load discovery feed\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }

    /* POST /api/data/cameras/trigger — port of data.js cameras/trigger:
     * single-flight; already-running → {started:false,already_running:true},
     * else kick the collector (detached) → {started:true,...}. */
    if (eq(u, "/api/data/cameras/trigger")) {
      const source_def *cd = registry_get("camera-discovery");
      if (!cd) { reply_json(c, 200,
        "{\"started\":false,\"already_running\":false}"); return; }
      if (!run_begin("camera-discovery")) {
        reply_json(c, 200,
          "{\"started\":false,\"already_running\":true}"); return;
      }
      cam_trig_arg *ta = calloc(1, sizeof *ta);
      ta->db = g_db; ta->d = cd;
      pthread_t th;
      if (pthread_create(&th, NULL, cam_trigger_thread, ta) == 0) {
        pthread_detach(th);
        reply_json(c, 200, "{\"started\":true,\"already_running\":false}");
      } else {
        run_end("camera-discovery"); free(ta);
        reply_json(c, 500,
          "{\"started\":false,\"already_running\":false}");
      }
      return;
    }

    /* GET /api/data/<sweep-layer> — unified-* / cameras / unified-stations /
     * unified-station-footprints served from the sweep stores. Non-sweep
     * /api/data/ ids fall through to the 501 below (still P5/P6-gated). */
    if (starts(u, "/api/data/")) {
      char did[256];
      if (seg(u, "/api/data/", "", did, sizeof did)) {
        char *body = sweepapi_data(g_db, did);
        if (!body) body = dataapi_layer(g_db, did);  /* generic collector layer */
        if (body) { reply_json(c, 200, body); free(body); return; }
      }
    }

    /* /api/transit* — port of routes/transit.js (transport sweep + gtfs_*). */
    if (starts(u, "/api/transit")) {
      char tp[512] = {0}, tq[512] = {0};
      size_t pre = strlen("/api/transit");
      if (u.len >= pre) {
        size_t sl = u.len - pre;
        if (sl >= sizeof tp) sl = sizeof tp - 1;
        memcpy(tp, u.buf + pre, sl); tp[sl] = 0;
      }
      if (hm->query.len) {
        size_t ql = hm->query.len;
        if (ql >= sizeof tq) ql = sizeof tq - 1;
        memcpy(tq, hm->query.buf, ql); tq[ql] = 0;
      }
      char *body = transitapi(g_db, tp, tq);
      if (body) { reply_json(c, 200, body); free(body); return; }
    }


    reply_json(c, 404, "{\"error\":\"not_found\"}");
    return;
  }
  reply_json(c, 404, "{\"error\":\"not_found\"}");
}

int httpd_serve(db_handle *db, int port) {
  g_db = db;
  auth_init();
  struct mg_mgr mgr;
  mg_mgr_init(&mgr);
  char url[64];
  snprintf(url, sizeof url, "http://0.0.0.0:%d", port);
  if (!mg_http_listen(&mgr, url, fn, NULL)) {
    fprintf(stderr, "[httpd] cannot bind %s\n", url);
    mg_mgr_free(&mgr);
    return 1;
  }
  fprintf(stderr, "[httpd] listening on %s\n", url);
  for (;;) mg_mgr_poll(&mgr, 100);
}
