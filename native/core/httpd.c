#include "httpd.h"
#include "auth.h"
#include "intelapi.h"
#include "statusapi.h"
#include "miscapi.h"
#include "entityapi.h"
#include "tenantapi.h"
#include "alertsapi.h"
#include "alert_eval.h"
#include "casesapi.h"
#include "annotationsapi.h"
#include "timelineapi.h"
#include "exportapi.h"
#include "reportapi.h"
#include "savedsearchapi.h"
#include "breach_monitor.h"
#include "evidence.h"
#include "translate.h"
#include "simhash.h"
#include "aoiapi.h"
#include "isochrone.h"
#include "vocabapi.h"
#include "nearapi.h"
#include "camera_stills.h"
#include "cameraproxy.h"
#include "uploadapi.h"
#include "ffmpeg.h"
#include "docmeta.h"
#include "media.h"
#include "audit.h"
#include "keysapi.h"
#include "operatorgate.h"
#include "ratelimit.h"
#include "dbexplorerapi.h"
#include "maintenanceapi.h"
#include "geoproxyapi.h"
#include "sweepapi.h"
#include "searchapi.h"
#include "progress.h"
#include "dataapi.h"
#include "transitapi.h"
#include "camera_store.h"
#include "breach_store.h"
#include "breach_jobs.h"
#include "breach_meta.h"
#include "breach_adapter.h"
#include "../third_party/mongoose.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include "scheduler.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
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

/* export_write_fn over a mongoose connection: one Transfer-Encoding chunk per
 * batch. Returning non-zero once the peer is gone is what makes export_run()
 * abandon a 250k-row walk instead of formatting it into a socket nobody is
 * reading. See exportapi.h on the remaining caveat: mg_http_write_chunk()
 * appends to c->send, which only drains on the event loop, so the HTTP layer
 * still buffers the whole response — bounded by the per-plan row cap, not by
 * database size. True socket-paced backpressure needs the mg_wakeup() thread
 * pattern used by suggest_thread() above. */
static int export_mg_write(void *ctx, const char *buf, size_t len) {
  struct mg_connection *c = (struct mg_connection *) ctx;
  if (c->is_closing || c->is_draining) return 1;
  mg_http_write_chunk(c, buf, len);
  return 0;
}

/* String field of a cJSON object, or NULL. */
static const char *jget_str(const cJSON *o, const char *k) {
  const cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
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
static int ends_with(const char *s, const char *suf) {
  if (!s || !suf) return 0;
  size_t n = strlen(s), m = strlen(suf);
  return m <= n && strcmp(s + n - m, suf) == 0;
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

/* ══ POST /api/data/cameras/trigger — camera-discovery fan-out ═════════════
 *
 * In Node this was ONE collector that internally walked ~20 channels. The C
 * port split it into one registered source per channel, so "run the camera
 * collector" is now a fan-out over every source whose `collector` field is
 * "camera-discovery" — the same grouping idiom osint_dispatch.c uses for
 * `collector == "osint"`. Membership is derived, never hardcoded: a new
 * cam_*.c joins the fan-out by existing.
 *
 * SEQUENTIAL, deliberately. run_begin's table is 16 slots; 15 channels plus the
 * outer key plus anything else in flight would overflow it, and 15 concurrent
 * SQLite writers would fight over a busy_timeout=5000 lock for no wall-clock
 * win against sites that are mostly latency-bound anyway. Running one at a time
 * holds at most two keys.
 *
 * Single-flight is two-level. The outer key "camera-discovery" keeps the
 * endpoint's {started, already_running} contract literally true for the fan-out
 * as a whole. The inner per-source run_begin stops the fan-out from
 * double-running a channel the scheduler or POST /api/intel/sources/:id/run
 * already has going — those two paths key on the source id, so the guards
 * interlock correctly. */

#define CAM_COLLECTOR "camera-discovery"
#define CAM_MAX_CHANNELS 64

/* Live progress for GET /api/data/cameras/run-status. Written by the fan-out
 * thread, read by the event loop, so every field moves under g_camlock. */
static pthread_mutex_t g_camlock = PTHREAD_MUTEX_INITIALIZER;
static struct {
  int      running;
  char     run_id[40];
  char     started_at[40];      /* ISO-8601 UTC */
  char     current[80];         /* channel in flight, "" when between */
  int      channels_total, channels_done, channels_skipped;
  long long ingested;
  time_t   last_finished;       /* 0 = never; drives the cooldown */
  long     last_duration_ms;
  long long last_ingested;
} g_cam;

/* Minimum gap between fan-outs. MapPage.jsx fires this POST automatically every
 * time the cameras layer is switched on, for any authenticated user — without a
 * cooldown that is 15 scrapers hammering third-party sites on a UI toggle. */
#define CAM_COOLDOWN_SEC 900

/* Collect the fan-out membership. Returns #found (capped), filling `out`. */
static int cam_channels(const source_def **out, int cap) {
  const source_def **all = registry_all();
  int n = registry_count(), k = 0;
  for (int i = 0; i < n && k < cap; i++)
    if (all[i]->collector && strcmp(all[i]->collector, CAM_COLLECTOR) == 0)
      out[k++] = all[i];
  return k;
}

typedef struct { db_handle *db; } cam_trig_arg;

static void *cam_trigger_thread(void *vp) {
  cam_trig_arg *a = vp;
  /* Its own connection, for the reason spelled out on srcrun_thread below:
   * this fan-out runs up to 15 collectors, each emitting through
   * BEGIN/COMMIT, and it was doing so on the event loop's shared handle. */
  db_handle own;
  db_handle *db = db_worker_open(&own, a->db);
  const source_def *ch[CAM_MAX_CHANNELS];
  int n = cam_channels(ch, CAM_MAX_CHANNELS);
  uint64_t t0 = mg_millis();
  long long total = 0;

  for (int i = 0; i < n; i++) {
    const source_def *d = ch[i];
    /* Already running via the scheduler or /api/intel/sources/:id/run → skip
     * this channel rather than run it twice; the rest of the fan-out proceeds. */
    if (!run_begin(d->id)) {
      pthread_mutex_lock(&g_camlock);
      g_cam.channels_skipped++;
      g_cam.channels_done++;
      pthread_mutex_unlock(&g_camlock);
      continue;
    }
    pthread_mutex_lock(&g_camlock);
    snprintf(g_cam.current, sizeof g_cam.current, "%s", d->id);
    pthread_mutex_unlock(&g_camlock);

    long long before = si_count(db, d->id);
    scheduler_run_source(db, d, NULL);
    long long delta = si_count(db, d->id) - before;
    if (delta < 0) delta = 0;
    total += delta;
    run_end(d->id);

    pthread_mutex_lock(&g_camlock);
    g_cam.channels_done++;
    g_cam.ingested += delta;
    g_cam.current[0] = 0;
    pthread_mutex_unlock(&g_camlock);
  }

  db_worker_close(&own);
  long dur = (long)(mg_millis() - t0);
  pthread_mutex_lock(&g_camlock);
  g_cam.running = 0;
  g_cam.current[0] = 0;
  g_cam.last_finished = time(NULL);
  g_cam.last_duration_ms = dur;
  g_cam.last_ingested = total;
  pthread_mutex_unlock(&g_camlock);

  fprintf(stderr, "[cameras] fan-out done: %d channels, %lld ingested, %ldms\n",
          n, total, dur);
  run_end(CAM_COLLECTOR);
  free(a);
  return NULL;
}

/* GET /api/search/suggest runs an LLM call. Doing it inline would block the
 * single mongoose event loop (freezing the whole app per keystroke), so we
 * compute it on a detached thread and deliver the result via mg_wakeup() —
 * MG_EV_WAKEUP fires on the originating connection and sends the reply. The
 * event loop stays free; suggestions appear when they're ready. */
/* Wakeup payloads are "NNN <json>": the 3-digit HTTP status, a space, then the
 * body. Deferred replies would otherwise all be forced to 200. */

/* mongoose's wakeup channel is a UDP socketpair (mg_socketpair() opens
 * SOCK_DGRAM), so a wakeup payload has to fit in ONE datagram of MG_IO_SIZE.
 * A larger send() is discarded whole — and mg_wakeup() still returns true, so
 * the worker believes it replied while the client is left holding a connection
 * that is never answered and never closed, until it times out. Measured on
 * /api/search/suggest: a 1403-byte body is delivered in 4 ms, a 1494-byte body
 * never arrives at all. Nine realistic OSINT suggestions clear 1.4 KB easily,
 * so this was the normal case for that route, not an edge one.
 * The parking path below already solves this for the isochrone; route anything
 * that does not comfortably fit through it too rather than dropping it. The
 * margin covers the 8-byte connection id and the 4-byte status prefix. */
#define WAKEUP_INLINE_MAX 1400
static void wakeup_reply_big(struct mg_mgr *mgr, unsigned long cid,
                             int status, char *owned);   /* defined below */

static void wakeup_reply(struct mg_mgr *mgr, unsigned long cid,
                         int status, const char *json) {
  size_t jn = strlen(json);
  if (jn > WAKEUP_INLINE_MAX) {
    char *owned = malloc(jn + 1);
    if (!owned) return;
    memcpy(owned, json, jn + 1);
    wakeup_reply_big(mgr, cid, status, owned);      /* consumes `owned` */
    return;
  }
  size_t n = jn + 8;
  char *buf = malloc(n);
  if (!buf) return;
  int len = snprintf(buf, n, "%03d %s", status, json);
  if (len > 0) mg_wakeup(mgr, cid, buf, (size_t)len);   /* copied by mongoose */
  free(buf);
}

/* ── deferred LARGE replies ────────────────────────────────────────────────
 * mg_wakeup() copies its payload through alloca() and then a NON-BLOCKING
 * send on the manager's socketpair, so it is only safe for the short JSON the
 * suggest and source-run paths hand it. An isochrone FeatureCollection is tens
 * to hundreds of kilobytes: that would put an unbounded alloca on the worker's
 * stack and then be truncated by the pipe.
 *
 * So the worker parks the body here and wakes the loop with a token that is
 * always under 16 bytes; the loop takes the pointer back and replies with it.
 * Slots are freed on delivery, and a park that finds the table full evicts the
 * oldest entry — a wakeup whose connection died before it landed would
 * otherwise hold a slot forever and eventually wedge every large reply.
 *
 * THE TOKEN IS A TICKET, NOT A SLOT INDEX. It used to be the raw index, which
 * made eviction actively dangerous rather than merely lossy: with 17 large
 * replies in flight, request A's slot is evicted and immediately re-used by
 * request B, and A's wakeup — still queued — then takes B's body and sends
 * B's response, HTTP 200, on A's connection. Silent cross-request delivery.
 * A monotonic ticket makes a stale token match nothing, so the evicted
 * request degrades to the "reply_lost" 500 it was always meant to get. */
#define WBODY_SLOTS 16
static pthread_mutex_t g_wbody_mu = PTHREAD_MUTEX_INITIALIZER;
static struct { char *body; uint64_t at; unsigned tick; } g_wbody[WBODY_SLOTS];
static unsigned g_wbody_tick;           /* 0 is reserved for "no ticket"      */

/* Takes ownership of `owned`; returns its ticket (never 0). */
static unsigned wbody_park(char *owned) {
  int slot = -1;
  pthread_mutex_lock(&g_wbody_mu);
  for (int i = 0; i < WBODY_SLOTS; i++)
    if (!g_wbody[i].body) { slot = i; break; }
  if (slot < 0) {                       /* evict the coldest */
    slot = 0;
    for (int i = 1; i < WBODY_SLOTS; i++)
      if (g_wbody[i].at < g_wbody[slot].at) slot = i;
    free(g_wbody[slot].body);
    g_wbody[slot].tick = 0;             /* the evicted token is now stale */
  }
  if (++g_wbody_tick == 0) g_wbody_tick = 1;
  g_wbody[slot].body = owned;
  g_wbody[slot].at   = mg_millis();
  g_wbody[slot].tick = g_wbody_tick;
  unsigned tk = g_wbody_tick;
  pthread_mutex_unlock(&g_wbody_mu);
  return tk;
}
static char *wbody_take(unsigned tick) {
  char *b = NULL;
  if (!tick) return NULL;
  pthread_mutex_lock(&g_wbody_mu);
  for (int i = 0; i < WBODY_SLOTS; i++)
    if (g_wbody[i].body && g_wbody[i].tick == tick) {
      b = g_wbody[i].body;
      g_wbody[i].body = NULL;
      g_wbody[i].tick = 0;
      break;
    }
  pthread_mutex_unlock(&g_wbody_mu);
  return b;
}
/* Deferred reply for a body too big for the wakeup pipe. Consumes `owned`. */
static void wakeup_reply_big(struct mg_mgr *mgr, unsigned long cid,
                             int status, char *owned) {
  if (!owned) { wakeup_reply(mgr, cid, 500, "{\"error\":\"server_error\"}"); return; }
  unsigned tk = wbody_park(owned);
  char tok[24];
  int len = snprintf(tok, sizeof tok, "%03d \x01%u", status, tk);
  if (len > 0 && !mg_wakeup(mgr, cid, tok, (size_t) len)) free(wbody_take(tk));
}

/* GET /api/isochrone — GTFS reachability. Seconds of CPU and ~1500 timetable
 * queries on a cache miss (core/isochrone.h), so it runs off-loop on its own
 * DB connection: a transaction belongs to a connection, and more to the point
 * a multi-second query on the shared handle stalls every other request. */
typedef struct {
  struct mg_mgr *mgr; unsigned long cid; db_handle *db; char qs[1024];
} iso_arg;

static void *iso_thread(void *vp) {
  iso_arg *a = vp;
  db_handle own;
  db_handle *db = db_worker_open(&own, a->db);
  int status = 200;
  char *body = isochrone_run(db, a->qs, &status);
  db_worker_close(&own);
  wakeup_reply_big(a->mgr, a->cid, status, body);
  free(a);
  return NULL;
}

typedef struct { struct mg_mgr *mgr; unsigned long cid; char *q; } suggest_arg;
static void *suggest_thread(void *vp) {
  suggest_arg *a = vp;
  char *body = searchapi_suggest(a->q);              /* blocks on suggest worker */
  wakeup_reply(a->mgr, a->cid, 200, body ? body : "{\"suggestions\":[]}");
  free(body); free(a->q); free(a);
  return NULL;
}

/* POST /api/intel/sources/:id/run — the collector runs on a detached thread and
 * the reply is delivered through MG_EV_WAKEUP, so the response contract is
 * unchanged while the event loop stays free. Running it inline froze the entire
 * server for the collector's duration (minutes, for a tiled Overpass source):
 * no /api/health, and every open SSE stream stalled because MG_EV_POLL could
 * not fire. */
typedef struct {
  struct mg_mgr *mgr; unsigned long cid;
  db_handle *db; const source_def *d; char id[128];
} srcrun_arg;

static void *srcrun_thread(void *vp) {
  srcrun_arg *a = vp;
  /* Its OWN connection, like iso_thread above and the scheduler pool.
   *
   * This ran the collector on `a->db`, which is g_db — the handle the event
   * loop is serving every other request on. A collector run reaches
   * core/intel.c emit(), and emit() wraps each item in an explicit
   * BEGIN/COMMIT. SQLite serialises ACCESS to a shared handle but not
   * TRANSACTIONS: a BEGIN issued here lands inside whatever transaction an
   * event-loop handler (an upload, a tenant write) already opened on the same
   * connection, so this thread's COMMIT publishes their half-finished write
   * and their ROLLBACK discards the rows this collector just ingested. That is
   * the exact hazard the pipeline and isochrone paths were converted away
   * from; this endpoint and the camera fan-out below were simply missed. */
  db_handle own;
  db_handle *db = db_worker_open(&own, a->db);
  uint64_t t0 = mg_millis();
  long long before = si_count(db, a->id);
  int rc = scheduler_run_source(db, a->d, NULL);
  long long delta = si_count(db, a->id) - before;
  if (delta < 0) delta = 0;
  db_worker_close(&own);
  run_end(a->id);

  char b[256];
  if (rc < 0) {
    snprintf(b, sizeof b,
      "{\"ran\":false,\"source_id\":\"%.80s\",\"error\":\"collector_run_failed\"}", a->id);
    wakeup_reply(a->mgr, a->cid, 500, b);
  } else {
    snprintf(b, sizeof b,
      "{\"ran\":true,\"source_id\":\"%.80s\",\"ingested\":%lld,"
      "\"duration_ms\":%llu,\"kind\":null,\"meta\":null}",
      a->id, delta, (unsigned long long)(mg_millis() - t0));
    wakeup_reply(a->mgr, a->cid, 200, b);
  }
  free(a);
  return NULL;
}

/* Parse the /api/intel/items|search query string into intel_items_query and
 * run it. Locals outlive the call (SQLITE_TRANSIENT copies bound values, the
 * envelope is malloc'd). mg_http_get_var > 0 == present & non-empty → NULL
 * means "filter not applied" (== Node `req.query.x ? String(x) : null`). */
static char *intel_items_run(struct mg_http_message *hm) {
  char src[160]={0}, q[256]={0}, qalt[256]={0}, lang[16]={0}, since[40]={0},
       until[40]={0}, rt[48]={0}, ssid[120]={0}, hg[8]={0}, tag[120]={0},
       cur[768]={0}, lim[16]={0};
  intel_items_query Q = {0};
  if (mg_http_get_var(&hm->query, "source",        src,  sizeof src ) > 0) Q.source = src;
  if (mg_http_get_var(&hm->query, "q",             q,    sizeof q   ) > 0) Q.q = q;
  /* The iOS client has always sent qAlt (the translated counterpart of q) and
   * this parser has always ignored it, so bilingual search was a no-op and the
   * "Also searching:" banner it drives was dead UI. intelapi_list_items() ORs
   * the two. */
  if (mg_http_get_var(&hm->query, "qAlt",          qalt, sizeof qalt) > 0) Q.q_alt = qalt;
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

/* ── breach corpus gate ────────────────────────────────────────────────────
 * /api/breach/search gates identical data behind opgate_check with the comment
 * "Breach data is sensitive, so gate it like /api/admin." — but the SAME rows
 * were reachable with plain auth through two other doors, because
 * intelapi_list_items() reroutes ?source=<breach slug> into
 * breach_adapter_list() and intelapi_item_by_uid() reroutes a "breach:" uid
 * into breach_adapter_item_by_uid(). Those adapters redact the leaked SECRET
 * but emit the breached IDENTIFIER in cleartext, which is the part that names
 * a person. A gate on one of four doors is not a gate.
 *
 * The check lives here rather than inside the adapters because the adapters
 * take no caller identity — they are also called from the alert inbox, where
 * the redacted preview is legitimate and already authorised. This is where
 * `usr` exists, and it covers both remaining HTTP entry points.
 *
 * Returns 1 when it has already replied and the caller must return. */
static int breach_gate(struct mg_connection *c, const auth_user *usr) {
  int oc = opgate_check(usr);
  if (oc == 0) return 0;
  if (oc == -401) reply_json(c, 401, "{\"error\":\"Auth required\"}");
  else if (oc == -1403)
    reply_json(c, 403, "{\"error\":\"Platform operator access not configured\"}");
  else
    reply_json(c, 403, "{\"error\":\"Platform operator role required\"}");
  return 1;
}

/* True when this /api/intel/items request is really a breach-corpus read. */
static int intel_query_is_breach(struct mg_http_message *hm) {
  char src[160] = {0};
  if (mg_http_get_var(&hm->query, "source", src, sizeof src) <= 0) return 0;
  return breach_meta_is_source(g_db, src);
}

static void fn(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_POLL) { search_stream_poll(c); return; }
  if (ev == MG_EV_CLOSE) { search_stream_close(c); return; }
  if (ev == MG_EV_WAKEUP) {            /* deferred reply from an off-loop thread */
    struct mg_str *d = (struct mg_str *) ev_data;
    int status = 200;
    const char *body = d->buf; int blen = (int) d->len;
    if (d->len > 4 && d->buf[0] >= '0' && d->buf[0] <= '9' &&
        d->buf[1] >= '0' && d->buf[1] <= '9' &&
        d->buf[2] >= '0' && d->buf[2] <= '9' && d->buf[3] == ' ') {
      status = (d->buf[0]-'0')*100 + (d->buf[1]-'0')*10 + (d->buf[2]-'0');
      body = d->buf + 4; blen = (int) d->len - 4;
    }
    /* "\x01<ticket>" == the body is parked (too big for the wakeup pipe). */
    if (blen > 1 && body[0] == '\x01') {
      char sb[16] = {0};
      int sl = blen - 1 < (int) sizeof sb - 1 ? blen - 1 : (int) sizeof sb - 1;
      memcpy(sb, body + 1, (size_t) sl);
      char *big = wbody_take((unsigned) strtoul(sb, NULL, 10));
      if (!big) { reply_json(c, 500, "{\"error\":\"reply_lost\"}"); return; }
      mg_http_reply(c, status,
        "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
        "%s", big);
      free(big);
      return;
    }
    mg_http_reply(c, status,
      "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n",
      "%.*s", blen, body);
    return;
  }
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
    /* Pre-auth TOTP compare, so an unthrottled caller gets to walk 10^6 codes
     * against a ±1-step window. 5 attempts/minute/IP leaves a fat-fingered
     * operator room and makes brute force take longer than the emergency it
     * exists for. Charged BEFORE the body is read so the limiter itself is
     * cheap; the denial is deliberately indistinguishable from a wrong code
     * apart from the status. */
    { int retry = 0;
      if (!ratelimit_allow(RL_BREAKGLASS, ip, 5, 60, &retry)) {
        char rb[96];
        snprintf(rb, sizeof rb,
          "{\"error\":\"too_many_attempts\",\"retry_after_sec\":%d}", retry);
        reply_json(c, 429, rb);
        return;
      } }
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
      int ast = 200;
      char *body = searchapi_analyze(g_db,
        (qj && cJSON_IsString(qj)) ? qj->valuestring : NULL,
        (mr && cJSON_IsNumber(mr)) ? (int)mr->valuedouble : 0, &ast);
      if (jb) cJSON_Delete(jb);
      if (!body && ast == 429) {
        reply_json(c, 429,
          "{\"error\":\"too_many_searches\",\"detail\":\"concurrent search limit "
          "reached; retry shortly\"}"); return;
      }
      if (!body) { reply_json(c, 400, "{\"error\":\"query_required\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }
    if (eq(u, "/api/search/suggest")) {
      char q[512] = {0};
      mg_http_get_var(&hm->query, "q", q, sizeof q);
      suggest_arg *a = calloc(1, sizeof *a);
      if (a) {
        a->mgr = c->mgr; a->cid = c->id; a->q = strdup(q);
        pthread_t th;
        if (a->q && pthread_create(&th, NULL, suggest_thread, a) == 0) {
          pthread_detach(th);
          return;        /* reply deferred to MG_EV_WAKEUP — loop stays free */
        }
        free(a->q); free(a);
      }
      /* fallback: thread spawn failed (rare) → reply inline */
      char *body = searchapi_suggest(q);
      reply_json(c, 200, body ? body : "{\"suggestions\":[]}"); free(body); return;
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

    /* GET /api/layers — registry layers, STRIP-filtered, with each layer's
     * sources + time-slider disposition. Node crashed here (a registry source
     * had layer:null → null.replace → 500); miscapi_list_layers skips
     * layerless sources instead so the Map layer picker actually loads. */
    if (eq(u, "/api/layers")) {
      char *body = miscapi_list_layers();
      if (!body) { reply_json(c, 500, "{\"error\":\"Failed to list layers\"}"); return; }
      reply_json(c, 200, body);
      free(body);
      return;
    }

    /* GET /api/status — per-source health + credential configuration.
     * Plain-auth, but operators additionally get the breach catalog inlined so
     * the Sources screen can show breaches beside real collectors. Ordinary
     * users get exactly the payload they always got (no 403, and none of the
     * ~1k extra rows), so this is a silent widening rather than a new gate. */
    if (eq(u, "/api/status")) {
      char *body = statusapi_build(g_db, opgate_check(&usr) == 0);
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
      if (intel_query_is_breach(hm) && breach_gate(c, &usr)) return;
      /* roadmap 32 — ?near=lat,lon[&radius_m=]. A distinct query MODE, not a
       * filter on the existing one: it orders by distance, so it cannot share
       * the published_at keyset cursor. Dispatched before intel_items_run so
       * the two never half-apply each other's semantics. Tenant-resolved
       * because nearapi_items() scopes rows to the caller's tenant. */
      { char nv[128] = {0};
        if (mg_http_get_var(&hm->query, "near", nv, sizeof nv) > 0) {
          struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
          char xtid[128] = {0};
          if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
          tenant_ctx tc;
          int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
          if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
          if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
          char nqs[1024] = {0};
          if (hm->query.len >= sizeof nqs) {
            reply_json(c, 414, "{\"error\":\"query_string_too_long\"}"); return; }
          memcpy(nqs, hm->query.buf, hm->query.len);
          int nst = 200;
          char *nb = nearapi_items(g_db, tc.tenant_id, nv, nqs, &nst);
          if (!nb) { reply_json(c, 500, "{\"error\":\"server_error\"}"); return; }
          reply_json(c, nst, nb); free(nb); return;
        } }
      char *body = intel_items_run(hm);
      if (!body) { reply_json(c, 500, "{\"error\":\"failed_to_list_intel_items\"}"); return; }
      /* Post-passes over the envelope intelapi already built, so every filter,
       * the FTS branch and the keyset cursor keep working untouched. Both
       * return NULL for the default/no-op case, in which case the original
       * bytes ship unchanged — existing clients see no difference. */
      { char cv[8] = {0};
        if (mg_http_get_var(&hm->query, "collapse", cv, sizeof cv) > 0 &&
            cv[0] == '1') {                                  /* roadmap 25 */
          char *col = simhash_collapse(g_db, "legacy", body);
          if (col) { free(body); body = col; }
        } }
      { char lv[16] = {0};
        if (mg_http_get_var(&hm->query, "lang_view", lv, sizeof lv) > 0) {
          translate_view tv = translate_view_parse(lv);      /* roadmap 29 */
          char *sh = translate_shape_items(g_db, body, tv);
          if (sh) { free(body); body = sh; }
        } }
      reply_json(c, 200, body);
      free(body);
      return;
    }

    /* Item sub-routes — must precede the /:uid catch-all so the suffix isn't
     * swallowed into the uid. Own buffer since `p` is declared further down. */
    {
      char pe[600];   /* seg() URL-decodes the segment into pe — already decoded */
      /* GET /api/intel/items/:uid/entities — entities mentioned in an item
       * (works for both breach records and normal intel items). Plain-auth read. */
      if (seg(u, "/api/intel/items/", "/entities", pe, sizeof pe)) {
        char *body = entityapi_item_entities(g_db, pe);
        if (!body) { reply_json(c, 500, "{\"error\":\"failed_to_list_item_entities\"}"); return; }
        reply_json(c, 200, body); free(body); return;
      }
      /* GET /api/intel/items/:uid/media — EXIF / pHash / OCR for the item's
       * images (roadmap 27). Plain-auth read; returns derived metadata only,
       * never the image bytes (those live in the evidence blob store). */
      if (seg(u, "/api/intel/items/", "/media", pe, sizeof pe)) {
        char lv[16] = {0};
        int hl = mg_http_get_var(&hm->query, "limit", lv, sizeof lv);
        char *body = media_list_for_item(g_db, pe, hl > 0 ? atoi(lv) : 0);
        if (!body) { reply_json(c, 500, "{\"error\":\"failed_to_list_media\"}"); return; }
        reply_json(c, 200, body); free(body); return;
      }
      /* GET /api/intel/items/:uid/evidence — chain of custody for the item
       * (roadmap 17). Plain-auth read; raw bytes stay operator-gated. */
      if (seg(u, "/api/intel/items/", "/evidence", pe, sizeof pe)) {
        char *body = evidence_list_for_item(g_db, pe, 100);
        if (!body) { reply_json(c, 500, "{\"error\":\"failed_to_list_evidence\"}"); return; }
        reply_json(c, 200, body); free(body); return;
      }
      /* GET /api/intel/items/:uid/reveal — decrypt a breach record's leaked
       * secret(s). Operator-gated like /api/breach/search; breach uids only. */
      if (seg(u, "/api/intel/items/", "/reveal", pe, sizeof pe)) {
        int oc = opgate_check(&usr);
        if (oc == -401)  { reply_json(c,401,"{\"error\":\"Auth required\"}"); return; }
        if (oc == -1403) { reply_json(c,403,"{\"error\":\"Platform operator access not configured\"}"); return; }
        if (oc != 0)     { reply_json(c,403,"{\"error\":\"Platform operator role required\"}"); return; }
        char *body = breach_adapter_reveal_by_uid(g_db, pe);
        if (!body) { reply_json(c, 404, "{\"error\":\"not_found\"}"); return; }
        reply_json(c, 200, body); free(body); return;
      }
    }

    /* GET /api/intel/items/:uid  (uid is URL-encoded; may contain '|',':') */
    if (starts(u, "/api/intel/items/") && u.len > 17) {
      char enc[600] = {0}, uid[600] = {0};
      size_t n = u.len - 17;
      if (n >= sizeof enc) n = sizeof enc - 1;
      memcpy(enc, u.buf + 17, n);
      mg_url_decode(enc, strlen(enc), uid, sizeof uid, 0);
      /* "breach:<keyid>" reroutes into breach_adapter_item_by_uid() — same
       * corpus as /api/breach/search, so the same gate. */
      if (strncmp(uid, "breach:", 7) == 0 && breach_gate(c, &usr)) return;
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
      /* Per-source schedule mode sets server-wide collection cadence (not a
       * per-tenant setting), so it's a platform-operator decision — gate it
       * like /api/admin and /api/db rather than leaving it plain-auth. */
      int oc = opgate_check(&usr);
      if (oc == -401)  { reply_json(c,401,"{\"error\":\"Auth required\"}"); return; }
      if (oc == -1403) { reply_json(c,403,"{\"error\":\"Platform operator access not configured\"}"); return; }
      if (oc != 0)     { reply_json(c,403,"{\"error\":\"Platform operator role required\"}"); return; }
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
      if (intel_query_is_breach(hm) && breach_gate(c, &usr)) return;
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
      /* POST only. This route is a write — one observed GET ingested 474 rows
       * — and without the guard any link-prefetching client, crawler or
       * browser history restore mutates the corpus. 14 sibling routes already
       * carry this exact guard; this was simply not among them. */
      if (hm->method.len != 4 || memcmp(hm->method.buf, "POST", 4) != 0) {
        reply_json(c, 405, "{\"error\":\"method_not_allowed\"}"); return;
      }
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
      srcrun_arg *ra = calloc(1, sizeof *ra);
      if (ra) {
        ra->mgr = c->mgr; ra->cid = c->id; ra->db = g_db; ra->d = d;
        snprintf(ra->id, sizeof ra->id, "%s", p);
        pthread_t th;
        if (pthread_create(&th, NULL, srcrun_thread, ra) == 0) {
          pthread_detach(th);
          return;      /* reply deferred to MG_EV_WAKEUP — loop stays free */
        }
        free(ra);
      }
      /* Thread spawn failed (rare): fall back to running inline rather than
       * dropping the request, and release the single-flight slot either way. */
      {
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
      }
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
      reply_json(c, 501, body); free(body); return;
    }

    /* ---- /api/admin/* and /api/db/* — requirePlatformOperator ---- */
    if (starts(u,"/api/admin/") ||
        eq(u,"/api/db/tables") || starts(u,"/api/db/tables/") ||
        eq(u,"/api/db/scheduler")) {
      int oc = opgate_check(&usr);
      if (oc == -401) { reply_json(c,401,"{\"error\":\"Auth required\"}"); return; }
      if (oc == -1403) { reply_json(c,403,"{\"error\":\"Platform operator access not configured\"}"); return; }
      if (oc != 0)    { reply_json(c,403,"{\"error\":\"Platform operator role required\"}"); return; }

      /* Roadmap 29 — corpus-wide JA→EN backfill. Deliberately behind the
       * PLATFORM OPERATOR gate, not a tenant owner/admin check: translate_run
       * has no tenant predicate (it is background maintenance over the whole
       * intel_items table), so letting any one tenant's owner start a global
       * job would be a privilege error. The 202 carries a request_id that
       * streams over the existing pre-auth SSE route /api/search/stream/:id,
       * since the job reports through core/progress.h. Never automatic. */
      if (eq(u,"/api/admin/translate/backfill")) {
        int status = 200; char *body = NULL;
        if (hm->method.len == 3 && memcmp(hm->method.buf, "GET", 3) == 0) {
          body = translate_backfill_status(g_db);
        } else if (hm->method.len == 4 && memcmp(hm->method.buf, "POST", 4) == 0) {
          char mv[16] = {0};
          int hv = mg_http_get_var(&hm->query, "max_items", mv, sizeof mv);
          body = translate_backfill_start(g_db, hv > 0 ? atoi(mv) : 0, &status);
        } else if (hm->method.len == 6 && memcmp(hm->method.buf, "DELETE", 6) == 0) {
          body = translate_backfill_cancel(&status);
        } else {
          reply_json(c, 405, "{\"error\":\"method_not_allowed\"}"); return;
        }
        if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
        reply_json(c, status, body); free(body); return;
      }

      /* Which ffmpeg answered, and whether it ran at all. "available: true"
       * is not much help when two are installed, and the honest answer to
       * "why is RTSP capture failing" is usually here. */
      if (eq(u,"/api/admin/ffmpeg/capabilities")) {
        char *body = ffmpeg_capabilities();
        if (!body) { reply_json(c, 500, "{\"error\":\"server_error\"}"); return; }
        reply_json(c, 200, body); free(body); return;
      }

      if (eq(u,"/api/admin/restart")) {
        reply_json(c,501,"{\"error\":\"not_implemented\",\"detail\":"
          "\"restart is not supported by the native server\"}"); return;
      }
      if (eq(u,"/api/admin/maintenance")) {
        char hv[16]={0}; int hh=mg_http_get_var(&hm->query,"hours",hv,sizeof hv);
        char *b=maintenance_digest(g_db, hh>0?atoi(hv):24);
        reply_json(c,200,b); free(b); return;
      }
      /* GET /api/admin/maintenance/source/:id — per-source pipeline detail */
      {
        char sid[128]={0};
        if (seg(u,"/api/admin/maintenance/source/","",sid,sizeof sid)) {
          char *b=maintenance_source_detail(g_db,sid);
          if (!b){ reply_json(c,404,"{\"error\":\"source_not_found\"}"); return; }
          reply_json(c,200,b); free(b); return;
        }
      }
      /* POST repair/anomaly/source operator actions */
      {
        int is_post = (mg_strcmp(hm->method,mg_str("POST"))==0);
        char idb[128]={0}; int st=200; char *b=NULL;
        if (seg(u,"/api/admin/repairs/","/approve",idb,sizeof idb)) {
          if (!is_post){ reply_json(c,405,"{\"error\":\"method_not_allowed\"}"); return; }
          b=maintenance_repair_action(g_db,atol(idb),1,&st);
          reply_json(c,st,b); free(b); return;
        }
        if (seg(u,"/api/admin/repairs/","/reject",idb,sizeof idb)) {
          if (!is_post){ reply_json(c,405,"{\"error\":\"method_not_allowed\"}"); return; }
          b=maintenance_repair_action(g_db,atol(idb),0,&st);
          reply_json(c,st,b); free(b); return;
        }
        if (seg(u,"/api/admin/sources/","/unquarantine",idb,sizeof idb)) {
          if (!is_post){ reply_json(c,405,"{\"error\":\"method_not_allowed\"}"); return; }
          b=maintenance_unquarantine(g_db,idb,&st);
          reply_json(c,st,b); free(b); return;
        }
        /* Roadmap 17 — the switch that makes evidence capture reachable.
         * sources.capture_evidence defaults to 0 and the capture hook fails
         * closed, so without this endpoint the whole chain-of-custody feature
         * is unreachable by any operator: correct-but-inert. Operator-gated
         * (we are inside the opgate block) and audited, because turning on
         * capture starts writing third-party content to local disk.
         *   POST   /api/admin/sources/:id/capture-evidence  → on
         *   DELETE /api/admin/sources/:id/capture-evidence  → off */
        if (seg(u,"/api/admin/sources/","/capture-evidence",idb,sizeof idb)) {
          int on;
          if (is_post) on = 1;
          else if (hm->method.len == 6 &&
                   memcmp(hm->method.buf, "DELETE", 6) == 0) on = 0;
          else { reply_json(c,405,"{\"error\":\"method_not_allowed\"}"); return; }
          sqlite3_stmt *cs;
          if (sqlite3_prepare_v2(g_db->h,
                "UPDATE sources SET capture_evidence=?1 WHERE id=?2",
                -1,&cs,NULL) != SQLITE_OK) {
            reply_json(c,500,"{\"error\":\"server_error\"}"); return; }
          sqlite3_bind_int(cs,1,on);
          sqlite3_bind_text(cs,2,idb,-1,SQLITE_TRANSIENT);
          sqlite3_step(cs);
          int ch = sqlite3_changes(g_db->h);
          sqlite3_finalize(cs);
          if (ch == 0) { reply_json(c,404,"{\"error\":\"not_found\"}"); return; }
          char pj[160];
          snprintf(pj,sizeof pj,"{\"source_id\":\"%.80s\",\"enabled\":%s}",
                   idb, on ? "true" : "false");
          audit_write(g_db, "platform", usr.id, "evidence.capture.toggle", idb, pj);
          char ob[128];
          snprintf(ob,sizeof ob,"{\"ok\":true,\"source_id\":\"%.80s\",\"capture_evidence\":%s}",
                   idb, on ? "true" : "false");
          reply_json(c,200,ob); return;
        }
        /* Roadmap 28 — the same reachability problem as capture-evidence:
         * capture_stills defaults to 0, so without a switch the feature is
         * inert. Cameras are intel_items rows with no tenant_id to authorize
         * against, hence operator-gated rather than tenant-scoped.
         *   POST   /api/admin/cameras/:uid/capture-stills  → on
         *   DELETE /api/admin/cameras/:uid/capture-stills  → off */
        if (seg(u,"/api/admin/cameras/","/capture-stills",idb,sizeof idb)) {
          int on;
          if (is_post) on = 1;
          else if (hm->method.len == 6 &&
                   memcmp(hm->method.buf, "DELETE", 6) == 0) on = 0;
          else { reply_json(c,405,"{\"error\":\"method_not_allowed\"}"); return; }
          sqlite3_stmt *cs;
          if (sqlite3_prepare_v2(g_db->h,
                "UPDATE intel_items SET capture_stills=?1 WHERE uid=?2",
                -1,&cs,NULL) != SQLITE_OK) {
            reply_json(c,500,"{\"error\":\"server_error\"}"); return; }
          sqlite3_bind_int(cs,1,on);
          sqlite3_bind_text(cs,2,idb,-1,SQLITE_TRANSIENT);
          sqlite3_step(cs);
          int ch = sqlite3_changes(g_db->h);
          sqlite3_finalize(cs);
          if (ch == 0) { reply_json(c,404,"{\"error\":\"not_found\"}"); return; }
          char pj[200];
          snprintf(pj,sizeof pj,"{\"camera_id\":\"%.80s\",\"enabled\":%s}",
                   idb, on ? "true" : "false");
          audit_write(g_db, "platform", usr.id, "camera.stills.toggle", idb, pj);
          /* Frames are only PRESERVED if the camera-stills source is also
           * opted into evidence capture — the module reuses that blob store
           * rather than building a second one, so the byte budget still
           * applies. Say so in the reply instead of leaving it to be
           * discovered when blob_path comes back NULL. */
          char ob[256];
          snprintf(ob,sizeof ob,
            "{\"ok\":true,\"camera_id\":\"%.80s\",\"capture_stills\":%s,"
            "\"note\":\"raw frames also require capture_evidence on source "
            "'camera-stills'\"}", idb, on ? "true" : "false");
          reply_json(c,200,ob); return;
        }
        if (seg(u,"/api/admin/anomalies/","/requeue",idb,sizeof idb)) {
          if (!is_post){ reply_json(c,405,"{\"error\":\"method_not_allowed\"}"); return; }
          b=maintenance_requeue_anomaly(g_db,atol(idb),&st);
          reply_json(c,st,b); free(b); return;
        }
      }
      /* ---- /api/admin/breach/* — one-shot corpus fetch / ingest (async jobs)
       * + job status. Ingest/fetch run on detached threads (own DB connection),
       * so a multi-GB job never blocks the event loop. Operator-gated above. */
      if (eq(u,"/api/admin/breach/ingest") || eq(u,"/api/admin/breach/fetch")) {
        if (mg_strcmp(hm->method,mg_str("POST"))!=0) {
          reply_json(c,405,"{\"error\":\"method_not_allowed\"}"); return; }
        char *body=NULL;
        if (hm->body.len) { body=malloc(hm->body.len+1);
          if (body){ memcpy(body,hm->body.buf,hm->body.len); body[hm->body.len]=0; } }
        cJSON *j = body ? cJSON_Parse(body) : NULL;
        free(body);
        int st=200; char *rb=NULL;
        if (eq(u,"/api/admin/breach/ingest")) {
          int mat = j && cJSON_IsTrue(cJSON_GetObjectItem(j,"materialize"));
          int dry = j && cJSON_IsTrue(cJSON_GetObjectItem(j,"dry_run"));
          rb = breach_job_ingest(jget_str(j,"source_id"), jget_str(j,"path"),
                                 jget_str(j,"type"), mat, dry, &st);
        } else {
          rb = breach_job_fetch(jget_str(j,"source_id"), jget_str(j,"url"), &st);
        }
        if (j) cJSON_Delete(j);
        reply_json(c,st,rb); free(rb); return;
      }
      /* GET /api/admin/breach/catalog/preview?path=&limit= — parse the seed and
       * report what came out WITHOUT writing a single row. This is what lets the
       * console show the operator the normalized result (and how many rows are
       * new vs already catalogued) before they commit to a load. */
      if (eq(u,"/api/admin/breach/catalog/preview")) {
        if (mg_strcmp(hm->method,mg_str("GET"))!=0) {
          reply_json(c,405,"{\"error\":\"method_not_allowed\"}"); return; }
        char pv[512]={0}, lv[16]={0};
        mg_http_get_var(&hm->query,"path",pv,sizeof pv);
        int hl = mg_http_get_var(&hm->query,"limit",lv,sizeof lv);
        int limit = hl>0 ? atoi(lv) : 50;
        if (limit < 1)   limit = 1;
        if (limit > 200) limit = 200;   /* the sample is for eyeballing, not export */

        breach_seed_row *rows=NULL; int nr=0;
        breach_seed_stats stt;
        int parsed = breach_meta_parse_seed(g_db, pv[0]?pv:NULL, limit,
                                            &rows, &nr, &stt);
        if (parsed < 0) {
          free(rows);
          reply_json(c,400,"{\"error\":\"breach_seed_unreadable\"}"); return; }

        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o,"path", stt.path);
        cJSON_AddStringToObject(o,"format", stt.format);
        cJSON_AddNumberToObject(o,"rows_read", stt.rows_read);
        cJSON_AddNumberToObject(o,"rows_parsed", stt.rows_parsed);
        cJSON_AddNumberToObject(o,"rows_skipped", stt.rows_skipped);
        cJSON_AddNumberToObject(o,"rows_new", stt.rows_new);
        cJSON_AddNumberToObject(o,"rows_existing", stt.rows_existing);
        cJSON *arr = cJSON_CreateArray();
        for (int i=0;i<nr;i++) {
          cJSON *r = cJSON_CreateObject();
          cJSON_AddStringToObject(r,"breach_id", rows[i].breach_id);
          cJSON_AddStringToObject(r,"name", rows[i].name);
          if (rows[i].pwn_count >= 0)
            cJSON_AddNumberToObject(r,"pwn_count",(double)rows[i].pwn_count);
          else cJSON_AddNullToObject(r,"pwn_count");
          cJSON_AddStringToObject(r,"added_date", rows[i].added_date);
          cJSON_AddStringToObject(r,"breach_date", rows[i].breach_date);
          cJSON_AddItemToArray(arr,r);
        }
        cJSON_AddItemToObject(o,"sample",arr);
        free(rows);
        char *rb = cJSON_PrintUnformatted(o);
        cJSON_Delete(o);
        if (!rb) { reply_json(c,500,"{\"error\":\"server_error\"}"); return; }
        reply_json(c,200,rb); free(rb); return;
      }
      /* POST /api/admin/breach/catalog/load {path?,format?} — (re)load the breach
       * catalog into breach_meta so breaches surface as intel sources. `format` is
       * "tsv" | "json" | "auto" (default): auto reads the committed seed TSV unless
       * an explicit non-.tsv path says otherwise. Also run best-effort at startup;
       * this is the manual hook. */
      if (eq(u,"/api/admin/breach/catalog/load")) {
        if (mg_strcmp(hm->method,mg_str("POST"))!=0) {
          reply_json(c,405,"{\"error\":\"method_not_allowed\"}"); return; }
        char *body=NULL;
        if (hm->body.len) { body=malloc(hm->body.len+1);
          if (body){ memcpy(body,hm->body.buf,hm->body.len); body[hm->body.len]=0; } }
        cJSON *j = body ? cJSON_Parse(body) : NULL;
        free(body);
        const char *path = jget_str(j,"path");
        const char *fmt  = jget_str(j,"format");
        if (path && !*path) path = NULL;

        int use_tsv;
        if      (fmt && strcmp(fmt,"tsv")  == 0) use_tsv = 1;
        else if (fmt && strcmp(fmt,"json") == 0) use_tsv = 0;
        else use_tsv = !path || ends_with(path, ".tsv");

        breach_seed_stats stt;
        memset(&stt, 0, sizeof stt);
        int n = use_tsv ? breach_meta_load_seed_tsv(g_db, path, &stt)
                        : breach_meta_load_manifest(g_db, path);
        if (j) cJSON_Delete(j);
        if (n < 0) { reply_json(c,500,"{\"error\":\"breach_catalog_load_failed\"}"); return; }

        char rb[256];
        if (use_tsv)
          snprintf(rb,sizeof rb,
            "{\"loaded\":%d,\"format\":\"tsv\",\"rows_read\":%d,\"rows_skipped\":%d,"
            "\"rows_new\":%d,\"rows_existing\":%d}",
            n, stt.rows_read, stt.rows_skipped, stt.rows_new, stt.rows_existing);
        else
          snprintf(rb,sizeof rb,"{\"loaded\":%d,\"format\":\"json\"}", n);
        reply_json(c,200,rb); return;
      }
      if (eq(u,"/api/admin/breach/jobs")) {
        char *b=breach_job_status(NULL); reply_json(c,200,b); free(b); return;
      }
      { char jid[32]={0};
        if (seg(u,"/api/admin/breach/jobs/","",jid,sizeof jid)) {
          char *b=breach_job_status(jid); reply_json(c,200,b); free(b); return;
        }
      }
      if (eq(u,"/api/db/tables")) {
        char *b=dbexplorer_tables(g_db); reply_json(c,200,b); free(b); return;
      }
      if (eq(u,"/api/db/scheduler")) {
        char *b=dbexplorer_scheduler(g_db); reply_json(c,200,b); free(b); return;
      }
      /* /api/db/tables/:name
       *
       * This is the tail of the block, so anything that entered it and matched
       * no route above lands here — including every unmatched /api/admin/*
       * path. The offset arithmetic below is blind, so `GET /api/admin/xxxsources`
       * used to slice out "sources" and dump that table. Only real
       * /api/db/tables/ URIs may reach the explorer. */
      if (!starts(u,"/api/db/tables/")) {
        reply_json(c,404,"{\"error\":\"not_found\"}"); return;
      }
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

    /* ---- roadmap 32: GET /api/isochrone — GTFS travel-time reachability.
     *
     * Plain-auth, deliberately: gtfs_* is shared reference data with no
     * tenant column, exactly like /api/transit, /api/data and /api/geocode
     * beside it. There is nothing here to tenant-scope, and inventing a
     * tenant predicate over a table that has no tenant would be theatre.
     *
     * Rate-limited because it is the most expensive read in the server: a
     * cache miss is seconds of CPU and ~1500 timetable queries. 12/minute per
     * client leaves an analyst dragging a pin on the map room to work while
     * making a scripted sweep of the country pointless. The reply is deferred
     * to a worker thread (see iso_thread) so none of that runs on the loop. */
    if (eq(u, "/api/isochrone")) {
      if (hm->method.len != 3 || memcmp(hm->method.buf, "GET", 3) != 0) {
        reply_json(c, 405, "{\"error\":\"method_not_allowed\"}"); return;
      }
      char ip[64] = {0};
      mg_snprintf(ip, sizeof ip, "%M", mg_print_ip, &c->rem);
      { int retry = 0;
        if (!ratelimit_allow(RL_ISOCHRONE, ip, 12, 60, &retry)) {
          char rb[96];
          snprintf(rb, sizeof rb,
            "{\"error\":\"too_many_requests\",\"retry_after_sec\":%d}", retry);
          reply_json(c, 429, rb); return;
        } }
      if (hm->query.len >= 1024) {
        reply_json(c, 414, "{\"error\":\"query_string_too_long\"}"); return; }
      iso_arg *ia = calloc(1, sizeof *ia);
      if (!ia) { reply_json(c, 500, "{\"error\":\"oom\"}"); return; }
      ia->mgr = c->mgr; ia->cid = c->id; ia->db = g_db;
      memcpy(ia->qs, hm->query.buf, hm->query.len);
      pthread_t th;
      if (pthread_create(&th, NULL, iso_thread, ia) == 0) {
        pthread_detach(th);
        return;            /* reply deferred to MG_EV_WAKEUP */
      }
      free(ia);
      reply_json(c, 503, "{\"error\":\"isochrone_worker_unavailable\"}");
      return;
    }

    /* ---- GET /api/vocab — the server's own enumerations (audit §8 item 9 /
     * recommendation 19). Tenant-resolved because the counted vocabularies
     * (record_type, entity type) are scoped to the caller's rows; the closed
     * lists are the same for everyone. Read-only, so GET only. */
    if (eq(u, "/api/vocab")) {
      if (hm->method.len != 3 || memcmp(hm->method.buf, "GET", 3) != 0) {
        reply_json(c, 405, "{\"error\":\"method_not_allowed\"}"); return;
      }
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
      char rv[8] = {0};
      int refresh = mg_http_get_var(&hm->query, "refresh", rv, sizeof rv) > 0 &&
                    rv[0] == '1';
      char *body = vocabapi_get(g_db, tc.tenant_id, refresh);
      if (!body) { reply_json(c, 500, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }

    /* GET /api/breach/search?q=&type=&limit= — search materialized breach
     * datapoints. Breach data is sensitive, so gate it like /api/admin (platform
     * operator). Returns metadata only; leaked secrets never cross this path. */
    if (eq(u,"/api/breach/search")) {
      int oc = opgate_check(&usr);
      if (oc == -401)  { reply_json(c,401,"{\"error\":\"Auth required\"}"); return; }
      if (oc == -1403) { reply_json(c,403,"{\"error\":\"Platform operator access not configured\"}"); return; }
      if (oc != 0)     { reply_json(c,403,"{\"error\":\"Platform operator role required\"}"); return; }
      char qv[256]={0}, tv[32]={0}, lv[16]={0};
      int hq = mg_http_get_var(&hm->query,"q",qv,sizeof qv);
      int ht = mg_http_get_var(&hm->query,"type",tv,sizeof tv);
      int hl = mg_http_get_var(&hm->query,"limit",lv,sizeof lv);
      if (hq<=0 && ht<=0) { reply_json(c,400,"{\"error\":\"q or type required\"}"); return; }
      char *b = breach_search(g_db, hq>0?qv:NULL, ht>0?tv:NULL, hl>0?atoi(lv):0);
      if (!b) { reply_json(c,400,"{\"error\":\"invalid breach query\"}"); return; }
      reply_json(c,200,b); free(b); return;
    }

    /* ---- roadmap 10: POST /api/alerts/preview — backtest a predicate over
     * history WITHOUT writing alert_events. Shares alert_eval.c's matcher
     * with the live ingest path, so what you preview is what will fire.
     * Must precede the /api/alerts/* block, which would otherwise treat
     * "preview" as a rule id. ---- */
    if (eq(u, "/api/alerts/preview")) {
      if (mg_strcmp(hm->method, mg_str("POST")) != 0) {
        reply_json(c, 405, "{\"error\":\"method_not_allowed\"}"); return; }
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
      /* Reads tenant intel, so viewer is not enough. */
      if (strcmp(tc.role, "viewer") == 0) {
        reply_json(c, 403, "{\"error\":\"forbidden\"}"); return; }
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
      cJSON *b = bdy ? cJSON_Parse(bdy) : NULL;
      free(bdy);
      cJSON *p = b ? cJSON_GetObjectItem(b, "predicate") : NULL;
      cJSON *sn = b ? cJSON_GetObjectItem(b, "since") : NULL;
      char *pj = p ? cJSON_PrintUnformatted(p) : NULL;
      int status = 200;
      char *body = alert_eval_preview(g_db, tc.tenant_id, pj,
        (sn && cJSON_IsString(sn)) ? sn->valuestring : NULL, &status);
      free(pj);
      if (b) cJSON_Delete(b);
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
    }
    /* ---- /api/alert-events[...] — notification inbox (roadmap item 11).
     * "/api/alert-events" does not share the "/api/alerts" prefix, but the
     * two are kept adjacent so the ordering is obvious to the next reader. */
    if (eq(u, "/api/alert-events") || starts(u, "/api/alert-events/")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }

      /* One segment after "/api/alert-events/" (18 chars). A trailing
       * "/read" on an event id is stripped — POST :id/read and POST :id are
       * the same operation, and accepting both keeps the client simple. */
      char seg1[160] = {0};
      if (u.len > 18) {
        char rest[256] = {0};
        size_t rl = u.len - 18; if (rl >= sizeof rest) rl = sizeof rest - 1;
        memcpy(rest, u.buf + 18, rl);
        char *sl = strchr(rest, '/');
        if (sl) *sl = 0;                 /* drop "/read" */
        mg_url_decode(rest, strlen(rest), seg1, sizeof seg1, 0);
      }
      char meth[12] = {0};
      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
      memcpy(meth, hm->method.buf, ml);
      char qsb[512] = {0};
      if (hm->query.len && hm->query.len < sizeof qsb)
        memcpy(qsb, hm->query.buf, hm->query.len);
      int status = 200;
      char *body = alerteventsapi(g_db, tc.tenant_id, tc.user_id, meth,
                                  seg1, qsb, &status);
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
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

    /* ---- GET /api/export/:kind — streaming row export (roadmap item 7).
     * Highest-risk egress path in the product: plan row caps, mandatory
     * ?source= for breach, and an audit_events row per run all live inside
     * exportapi.c. Headers must go out BEFORE the first chunk — after that
     * there is no error status left to send. ---- */
    if (starts(u, "/api/export/") && u.len > 12) {
      if (hm->method.len != 3 || memcmp(hm->method.buf, "GET", 3) != 0) {
        reply_json(c, 405, "{\"error\":\"method_not_allowed\"}"); return;
      }
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }

      char kind[32] = {0};
      { size_t kl = u.len - 12;
        if (kl >= sizeof kind) kl = sizeof kind - 1;
        memcpy(kind, u.buf + 12, kl); }
      /* The fifth breach door. exportapi.c already demands ?source= and
       * analyst-or-better, but this streams the same cleartext identifiers
       * /api/breach/search gates on the platform operator — and in bulk. The
       * corpus decides the gate, not the route. */
      if (strcmp(kind, "breach") == 0 && breach_gate(c, &usr)) return;
      /* REJECT an over-long query string; do NOT truncate it. `format` is read
       * from the untruncated mg_str while every FILTER is read from this
       * fixed buffer, so padding the URL past 2048 bytes used to drop the
       * filters silently and hand back a correctly-named CSV containing the
       * whole dataset. A 414 is the only safe answer: there is no way to
       * honour half a filter set. */
      char qs[2048] = {0};
      if (hm->query.len >= sizeof qs) {
        reply_json(c, 414,
          "{\"error\":\"query_string_too_long\",\"detail\":\"export filters "
          "must fit in 2047 bytes; they are never silently dropped\"}");
        return;
      }
      memcpy(qs, hm->query.buf, hm->query.len);
      char fmt[16] = {0};
      if (mg_http_get_var(&hm->query, "format", fmt, sizeof fmt) <= 0)
        snprintf(fmt, sizeof fmt, "json");

      export_plan plan; int status = 200;
      if (export_plan_request(g_db, &tc, kind, fmt, qs, &plan, &status) != 0) {
        char eb[96];
        snprintf(eb, sizeof eb, "{\"error\":\"%s\"}", plan.error);
        reply_json(c, status, eb); return;
      }
      mg_printf(c,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Disposition: attachment; filename=\"%s\"\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Cache-Control: no-store\r\n"
        "X-Accel-Buffering: no\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n",
        plan.content_type, plan.filename);
      c->is_resp = 0;                 /* we own the framing (== the SSE path) */
      long rows = 0;
      export_run(g_db, &tc, kind, fmt, qs, export_mg_write, c, &rows, &status);
      mg_http_write_chunk(c, "", 0);  /* terminating zero-length chunk */
      return;
    }

    /* ---- roadmap 18: GET /api/timeline (tenant-scoped unified timeline) ---- */
    if (eq(u, "/api/timeline")) {
      if (mg_strcmp(hm->method, mg_str("GET")) != 0) {
        reply_json(c, 405, "{\"error\":\"method_not_allowed\"}"); return; }
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
      char qs[1024] = {0};
      size_t ql = hm->query.len < sizeof qs - 1 ? hm->query.len : sizeof qs - 1;
      memcpy(qs, hm->query.buf, ql);
      int status = 200;
      char *body = timelineapi_query(g_db, &tc, qs, &status);
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
    }


    /* ---- Roadmap 16: POST /api/cases/:id/report — streaming report.
     * MUST precede the /api/cases block, which matches "/api/cases/" and
     * would otherwise swallow this route and 404 on act="report". ---- */
    if (starts(u, "/api/cases/") && u.len > 18 &&
        memcmp(u.buf + u.len - 7, "/report", 7) == 0) {
      if (hm->method.len != 4 || memcmp(hm->method.buf, "POST", 4) != 0) {
        reply_json(c, 405, "{\"error\":\"method_not_allowed\"}"); return;
      }
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
      char cid[128] = {0};
      { char raw[256] = {0};
        size_t rl = u.len - 11 - 7;      /* between "/api/cases/" and "/report" */
        if (rl >= sizeof raw) rl = sizeof raw - 1;
        memcpy(raw, u.buf + 11, rl);
        mg_url_decode(raw, strlen(raw), cid, sizeof cid, 0); }
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
      report_plan plan; int status = 200;
      if (report_plan_request(g_db, &tc, cid, bdy, &plan, &status) != 0) {
        char eb[96];
        snprintf(eb, sizeof eb, "{\"error\":\"%s\"}", plan.error);
        free(bdy); reply_json(c, status, eb); return;
      }
      mg_printf(c,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Disposition: attachment; filename=\"%s\"\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Cache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "X-Accel-Buffering: no\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n",
        plan.content_type, plan.filename);
      c->is_resp = 0;
      long rbytes = 0;
      report_run(g_db, &tc, cid, bdy, export_mg_write, c, &rbytes, &status);
      free(bdy);
      mg_http_write_chunk(c, "", 0);
      return;
    }

    /* ---- Roadmap 14: /api/cases subtree (tenant-scoped analyst workspace) ---- */
    if (eq(u, "/api/cases") || starts(u, "/api/cases/")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }

      char cid[128] = {0}, act[32] = {0};
      if (u.len > 11) {                          /* after "/api/cases/" */
        char rest[256] = {0};
        size_t rl = u.len - 11; if (rl >= sizeof rest) rl = sizeof rest - 1;
        memcpy(rest, u.buf + 11, rl);
        char *sl = strchr(rest, '/');
        if (sl) { *sl = 0; snprintf(act, sizeof act, "%s", sl + 1); }
        mg_url_decode(rest, strlen(rest), cid, sizeof cid, 0);
      }
      char meth[12] = {0};
      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
      memcpy(meth, hm->method.buf, ml);
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }

      char qst[32]={0}, qrt[32]={0}, qri[512]={0}, qcu[512]={0}, qli[16]={0};
      mg_http_get_var(&hm->query, "status",   qst, sizeof qst);
      mg_http_get_var(&hm->query, "ref_type", qrt, sizeof qrt);
      mg_http_get_var(&hm->query, "ref_id",   qri, sizeof qri);
      mg_http_get_var(&hm->query, "cursor",   qcu, sizeof qcu);
      int hl = mg_http_get_var(&hm->query, "limit", qli, sizeof qli);
      cases_query cq;
      cq.status   = qst[0] ? qst : NULL;
      cq.ref_type = qrt[0] ? qrt : NULL;
      cq.ref_id   = qri[0] ? qri : NULL;
      cq.cursor   = qcu[0] ? qcu : NULL;
      cq.limit    = hl > 0 ? atoi(qli) : 0;

      int status = 200;
      char *body = casesapi(g_db, &tc, meth, cid, act, bdy, &cq, &status);
      free(bdy);
      if (!body && status == 204) { mg_http_reply(c, 204, "", ""); return; }
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
    }

    /* ---- Roadmap 15: /api/annotations subtree. Passes the raw query string
     * through (six params, two free-text) rather than pre-extracting. ---- */
    if (eq(u, "/api/annotations") || starts(u, "/api/annotations/")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }

      char aid[160] = {0};
      if (u.len > 17) {                          /* after "/api/annotations/" */
        char rest[256] = {0};
        size_t rl = u.len - 17; if (rl >= sizeof rest) rl = sizeof rest - 1;
        memcpy(rest, u.buf + 17, rl);
        char *sl = strchr(rest, '/');
        if (sl) *sl = 0;
        mg_url_decode(rest, strlen(rest), aid, sizeof aid, 0);
      }
      char meth[12] = {0};
      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
      memcpy(meth, hm->method.buf, ml);
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
      char qsb[768] = {0};
      if (hm->query.len && hm->query.len < sizeof qsb)
        memcpy(qsb, hm->query.buf, hm->query.len);

      int status = 200;
      char *body = annotationsapi(g_db, &tc, meth, aid, qsb, bdy, &status);
      free(bdy);
      if (!body && status == 204) { mg_http_reply(c, 204, "", ""); return; }
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
    }

    /* ---- Roadmap 17: chain of custody ----
     * GET /api/intel/items/:uid/evidence  — custody records (plain auth)
     * GET /api/evidence/verify            — re-walk the hash chain (operator)
     * GET /api/evidence/:id/raw           — the captured bytes (operator)
     * Raw evidence is whatever a source returned: untrusted bytes, served as
     * octet-stream with nosniff so it can never execute as markup. */
    if (eq(u, "/api/evidence/verify")) {
      if (opgate_check(&usr) != 0) {
        reply_json(c, 403, "{\"error\":\"forbidden\"}"); return; }
      char *body = evidence_verify(g_db);
      if (!body) { reply_json(c, 500, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }
    { char eid[128] = {0};
      if (seg(u, "/api/evidence/", "/raw", eid, sizeof eid)) {
        unsigned char *bytes = NULL; size_t blen = 0;
        char sha[72] = {0}; int status = 200;
        char *errj = evidence_raw(g_db, &usr, eid, &bytes, &blen, sha, &status);
        if (errj) { reply_json(c, status, errj); free(errj); return; }
        mg_printf(c,
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: application/octet-stream\r\n"
          "Content-Length: %lu\r\n"
          "X-JO-Evidence-SHA256: %s\r\n"
          "Content-Disposition: attachment; filename=\"evidence-%s.bin\"\r\n"
          "X-Content-Type-Options: nosniff\r\n"
          "Cache-Control: no-store\r\n\r\n",
          (unsigned long)blen, sha, eid);
        mg_send(c, bytes, blen);
        c->is_resp = 0;
        free(bytes);
        return;
      } }

    /* ---- Roadmap 28: camera stills. Raw frames are untrusted bytes from
     * arbitrary internet cameras, so the raw route is operator-gated and
     * served as octet-stream + nosniff, never inline. ---- */
    { char cid2[192] = {0};
      if (seg(u, "/api/cameras/", "/stills", cid2, sizeof cid2)) {
        char lv[16] = {0}, cv2[512] = {0};
        int hl2 = mg_http_get_var(&hm->query, "limit", lv, sizeof lv);
        mg_http_get_var(&hm->query, "cursor", cv2, sizeof cv2);
        char *body = camera_stills_list(g_db, cid2, hl2 > 0 ? atoi(lv) : 0,
                                        cv2[0] ? cv2 : NULL);
        if (!body) { reply_json(c, 404, "{\"error\":\"not_found\"}"); return; }
        reply_json(c, 200, body); free(body); return;
      } }
    { char sid2[128] = {0};
      if (seg(u, "/api/camera-stills/", "/raw", sid2, sizeof sid2)) {
        unsigned char *bytes = NULL; size_t blen = 0;
        char sha[72] = {0}; int status = 200;
        char *errj = camera_stills_raw(g_db, &usr, sid2, &bytes, &blen, sha, &status);
        if (errj) { reply_json(c, status, errj); free(errj); return; }
        mg_printf(c,
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: application/octet-stream\r\n"
          "Content-Length: %lu\r\n"
          "X-JO-Still-SHA256: %s\r\n"
          "Content-Disposition: attachment; filename=\"still-%s.jpg\"\r\n"
          "X-Content-Type-Options: nosniff\r\n"
          "Cache-Control: no-store\r\n\r\n",
          (unsigned long)blen, sha, sid2);
        mg_send(c, bytes, blen);
        c->is_resp = 0;
        free(bytes);
        return;
      } }

    /* ---- Roadmap 9: /api/aoi — named geofence shapes. One shape can back
     * several rules and is resolved tenant-scoped at match time, so a rule
     * can never geofence on another tenant's area. ---- */
    if (eq(u, "/api/aoi") || starts(u, "/api/aoi/")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
      char aid[160] = {0};
      if (u.len > 9) {                            /* after "/api/aoi/" */
        char rest[256] = {0};
        size_t rl = u.len - 9; if (rl >= sizeof rest) rl = sizeof rest - 1;
        memcpy(rest, u.buf + 9, rl);
        char *sl = strchr(rest, '/');
        if (sl) *sl = 0;
        mg_url_decode(rest, strlen(rest), aid, sizeof aid, 0);
      }
      char meth[12] = {0};
      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
      memcpy(meth, hm->method.buf, ml);
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
      char qsb[512] = {0};
      if (hm->query.len && hm->query.len < sizeof qsb)
        memcpy(qsb, hm->query.buf, hm->query.len);
      int status = 200;
      char *body = aoiapi(g_db, &tc, meth, aid, qsb, bdy, &status);
      free(bdy);
      if (!body && status == 204) { mg_http_reply(c, 204, "", ""); return; }
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
    }

    /* ---- Roadmap 21: /api/watchlists — sugar over an alert rule carrying an
     * entity predicate, so there is one engine, one delivery path, one inbox. */
    if (eq(u, "/api/watchlists") || starts(u, "/api/watchlists/")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
      char wid[160] = {0};
      if (u.len > 16) {                           /* after "/api/watchlists/" */
        char rest[256] = {0};
        size_t rl = u.len - 16; if (rl >= sizeof rest) rl = sizeof rest - 1;
        memcpy(rest, u.buf + 16, rl);
        char *sl = strchr(rest, '/');
        if (sl) *sl = 0;
        mg_url_decode(rest, strlen(rest), wid, sizeof wid, 0);
      }
      char meth[12] = {0};
      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
      memcpy(meth, hm->method.buf, ml);
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
      char qsb[512] = {0};
      if (hm->query.len && hm->query.len < sizeof qsb)
        memcpy(qsb, hm->query.buf, hm->query.len);
      int status = 200;
      char *body = watchlistsapi(g_db, &tc, meth, wid, qsb, bdy, &status);
      free(bdy);
      if (!body && status == 204) { mg_http_reply(c, 204, "", ""); return; }
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
    }

    /* ---- Chunked upload: begin / part / commit / status / abort.
     *
     * The ONE deviation from every neighbouring block: this passes
     * hm->body.buf and hm->body.len straight through instead of the usual
     * malloc(len+1) NUL-terminated copy. A part body is BINARY — it contains
     * NUL bytes and is up to a megabyte — so treating it as a C string would
     * silently truncate the file at the first zero byte. ---- */
    if (eq(u, "/api/uploads") || starts(u, "/api/uploads/")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }

      char uid2[128] = {0}, act[32] = {0};
      if (u.len > 13) {                          /* after "/api/uploads/" */
        char rest[256] = {0};
        size_t rl = u.len - 13; if (rl >= sizeof rest) rl = sizeof rest - 1;
        memcpy(rest, u.buf + 13, rl);
        char *sl = strchr(rest, '/');
        if (sl) { *sl = 0; snprintf(act, sizeof act, "%s", sl + 1); }
        mg_url_decode(rest, strlen(rest), uid2, sizeof uid2, 0);
      }
      char meth[12] = {0};
      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
      memcpy(meth, hm->method.buf, ml);
      char qsb[256] = {0};
      if (hm->query.len && hm->query.len < sizeof qsb)
        memcpy(qsb, hm->query.buf, hm->query.len);
      int status = 200;
      char *body = uploadapi(g_db, &tc, meth, uid2, act, qsb,
                             hm->body.buf, hm->body.len, &status);
      if (!body && status == 204) { mg_http_reply(c, 204, "", ""); return; }
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
    }

    /* ---- Roadmap 24: /api/breach-monitors — standing breach exposure
     * orders. Only the SHA-1 of the identifier is ever stored; nothing here
     * echoes a plaintext address back, including audit payloads. ---- */
    if (eq(u, "/api/breach-monitors") || starts(u, "/api/breach-monitors/")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }

      char mid[128] = {0}, act[32] = {0};
      if (u.len > 21) {                        /* after "/api/breach-monitors/" */
        char rest[256] = {0};
        size_t rl = u.len - 21; if (rl >= sizeof rest) rl = sizeof rest - 1;
        memcpy(rest, u.buf + 21, rl);
        char *sl = strchr(rest, '/');
        if (sl) { *sl = 0; snprintf(act, sizeof act, "%s", sl + 1); }
        mg_url_decode(rest, strlen(rest), mid, sizeof mid, 0);
      }
      char meth[12] = {0};
      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
      memcpy(meth, hm->method.buf, ml);
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
      char cv[512] = {0}, lv[16] = {0};
      mg_http_get_var(&hm->query, "cursor", cv, sizeof cv);
      int hl = mg_http_get_var(&hm->query, "limit", lv, sizeof lv);
      int status = 200;
      char *body = breach_monitors_api(g_db, &tc, meth, mid, act, bdy,
                                       cv[0] ? cv : NULL,
                                       hl > 0 ? atoi(lv) : 0, &status);
      free(bdy);
      if (!body && status == 204) { mg_http_reply(c, 204, "", ""); return; }
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
    }

    /* ---- Roadmap 38: /api/saved-searches subtree ---- */
    if (eq(u, "/api/saved-searches") || starts(u, "/api/saved-searches/")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
      char sid[160] = {0}, act[64] = {0};
      if (u.len > 20) {                          /* after "/api/saved-searches/" */
        char rest[256] = {0};
        size_t rl = u.len - 20; if (rl >= sizeof rest) rl = sizeof rest - 1;
        memcpy(rest, u.buf + 20, rl);
        char *sl = strchr(rest, '/');
        if (sl) { *sl = 0; snprintf(act, sizeof act, "%s", sl + 1); }
        mg_url_decode(rest, strlen(rest), sid, sizeof sid, 0);
      }
      char meth[12] = {0};
      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
      memcpy(meth, hm->method.buf, ml);
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
      char qsb[512] = {0};
      if (hm->query.len && hm->query.len < sizeof qsb)
        memcpy(qsb, hm->query.buf, hm->query.len);
      int status = 200;
      char *body = savedsearchapi(g_db, &tc, meth, sid, act, qsb, bdy, &status);
      free(bdy);
      if (!body && status == 204) { mg_http_reply(c, 204, "", ""); return; }
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
    }

    /* ---- Roadmap 38: /api/search-history (collection only). Per-USER, not
     * per-tenant: an admin must not be able to read what a colleague is
     * investigating. The user predicate is enforced inside the module. ---- */
    if (eq(u, "/api/search-history")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
      char meth[12] = {0};
      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
      memcpy(meth, hm->method.buf, ml);
      char qsb[512] = {0};
      if (hm->query.len && hm->query.len < sizeof qsb)
        memcpy(qsb, hm->query.buf, hm->query.len);
      int status = 200;
      char *body = searchhistoryapi(g_db, &tc, meth, qsb, &status);
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
    }

    /* ---- Roadmap 38: /api/permalink — stateless view-state codec. No
     * tenant_resolve: the token carries VIEW STATE ONLY and must never carry
     * authorization. It stays inside the authenticated region anyway, so an
     * anonymous scanner can't use it as a free JSON parser. ---- */
    if (eq(u, "/api/permalink") || starts(u, "/api/permalink/")) {
      static char tok[PL_TOKEN_MAX + 8];         /* 4 KB — too big for stack */
      tok[0] = 0;
      if (u.len > 15) {                          /* after "/api/permalink/" */
        size_t rl = u.len - 15;
        if (rl <= PL_TOKEN_MAX)
          mg_url_decode(u.buf + 15, rl, tok, sizeof tok, 0);
        else tok[0] = 0;                         /* over-long → 400 below */
      }
      char meth[12] = {0};
      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
      memcpy(meth, hm->method.buf, ml);
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
      int status = 200;
      char *body = permalinkapi(meth, tok, bdy, &status);
      free(bdy);
      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
      reply_json(c, status, body); free(body); return;
    }

    /* ---- /api/members[/*] — member roster + invites (tenant-scoped) ---- */
    if (eq(u, "/api/members") || starts(u, "/api/members/")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx tc;
      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }

      char seg[128] = {0}, act[128] = {0};
      if (u.len > 13) {                          /* after "/api/members/" */
        char rest[256] = {0};
        size_t rl = u.len - 13; if (rl >= sizeof rest) rl = sizeof rest - 1;
        memcpy(rest, u.buf + 13, rl);
        char *sl = strchr(rest, '/');
        if (sl) { *sl = 0; mg_url_decode(sl + 1, strlen(sl + 1), act, sizeof act, 0); }
        mg_url_decode(rest, strlen(rest), seg, sizeof seg, 0);
      }
      char meth[12] = {0};
      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
      memcpy(meth, hm->method.buf, ml);
      char *bdy = NULL;
      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
      int status = 200;
      char *body = tenantapi_members(g_db, &tc, meth, seg, act, bdy, &status);
      free(bdy);
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
      /* GET /api/tenant-keys/:NAME returns the DECRYPTED key. keysapi.c now
       * requires owner/admin for it, and this is the second half of that fix:
       * the same platform-operator gate the breach corpus carries, for the
       * same reason — a stored third-party credential in cleartext is not an
       * ordinary tenant read. Listing (/api/tenant-keys) and writing (PUT)
       * are untouched, so BYOK self-service still works for tenant admins. */
      if (tenantk && seg[0] && strcmp(seg,"policy") != 0 &&
          strcmp(meth,"GET") == 0) {
        int oc = opgate_check(&usr);
        if (oc != 0) {
          free(bdy);
          if (oc == -401) reply_json(c,401,"{\"error\":\"Auth required\"}");
          else if (oc == -1403)
            reply_json(c,403,"{\"error\":\"Platform operator access not configured\"}");
          else reply_json(c,403,"{\"error\":\"Platform operator role required\"}");
          return;
        }
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

    /* ---- P7 Wave 2: /api/entities/* (entity graph, pure SQLite) ----
     * These routes reached entityapi without ever resolving a tenant, and
     * entityapi had no tenant predicate — the whole subtree was cross-tenant
     * readable. Resolve once here, at the top of the subtree, and pass the id
     * down; casesapi/aoiapi/exportapi all do exactly this. */
    if (eq(u, "/api/entities/stats") || eq(u, "/api/entities/search") ||
        starts(u, "/api/entities/")) {
      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
      char xtid[128] = {0};
      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
      tenant_ctx etc;
      int etr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &etc);
      if (etr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
      if (etr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }

    if (eq(u, "/api/entities/stats")) {
      char *body = entityapi_stats(g_db, etc.tenant_id);
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
                                    hl > 0 ? atoi(lv) : 0, etc.tenant_id);
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
          body = entityapi_get(g_db, type, eid, etc.tenant_id);
          notfound = !body;
        } else if (strcmp(tail, "graph") == 0) {
          char dv[16] = {0}, rtv[256] = {0}, xhv[16] = {0}, mnv[16] = {0};
          int hd = mg_http_get_var(&hm->query, "depth", dv, sizeof dv);
          int hr = mg_http_get_var(&hm->query, "rel_types", rtv, sizeof rtv);
          int hx = mg_http_get_var(&hm->query, "exclude_hubs", xhv, sizeof xhv);
          int hmn = mg_http_get_var(&hm->query, "max_nodes", mnv, sizeof mnv);
          body = entityapi_graph(g_db, type, eid, hd > 0 ? atoi(dv) : 1,
                                 hr > 0 ? rtv : NULL,
                                 hx > 0 ? atoi(xhv) : 0,
                                 hmn > 0 ? atoi(mnv) : 0, etc.tenant_id);
          notfound = !body;
        } else if (strcmp(tail, "mentions") == 0) {
          char lv[16] = {0}, ov[16] = {0};
          int hl = mg_http_get_var(&hm->query, "limit", lv, sizeof lv);
          int ho = mg_http_get_var(&hm->query, "offset", ov, sizeof ov);
          body = entityapi_mentions(g_db, type, eid, hl > 0 ? atoi(lv) : 0,
                                    ho > 0 ? atoi(ov) : 0, etc.tenant_id);
          notfound = !body;
        } else if (strcmp(tail, "breaches") == 0) {
          /* roadmap item 23 — entity → its breaches (catalog metadata only;
           * secrets stay behind breach_adapter's operator reveal path). */
          char lv[16] = {0}, ov[16] = {0};
          int hl = mg_http_get_var(&hm->query, "limit", lv, sizeof lv);
          int ho = mg_http_get_var(&hm->query, "offset", ov, sizeof ov);
          /* Breach CATALOG metadata only (names/dates/data classes) — the
           * identifiers and secrets stay behind breach_gate()/the operator
           * reveal path, so this stays plain-auth as documented. */
          body = entityapi_breaches(g_db, type, eid, hl > 0 ? atoi(lv) : 0,
                                    ho > 0 ? atoi(ov) : 0, etc.tenant_id);
          notfound = !body;
        }
        if (notfound) { reply_json(c, 404, "{\"error\":\"not_found\"}"); return; }
        if (body) { reply_json(c, 200, body); free(body); return; }
      }
      reply_json(c, 404, "{\"error\":\"not_found\"}");
      return;
    }
    }   /* end /api/entities/* tenant-resolved block */

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

    /* GET /api/data/cameras/proxy?camera_uid= — port of data.js cameras/proxy.
     * Serves the upstream camera image over our own origin so iOS doesn't need
     * an ATS exception per discovered IP. Bytes are from an arbitrary internet
     * camera, so: nosniff, and the content type is echoed only after it was
     * checked to be image/*. Explicit route — the generic /api/data/ matcher
     * below would treat "cameras/proxy" as a layer id. */
    if (eq(u, "/api/data/cameras/proxy")) {
      char cu[192] = {0};
      mg_http_get_var(&hm->query, "camera_uid", cu, sizeof cu);
      unsigned char *img = NULL; size_t ilen = 0;
      char ictype[64] = {0}; int istatus = 400;
      char *ej = camera_proxy_fetch(g_db, cu, &img, &ilen,
                                    ictype, sizeof ictype, &istatus);
      if (ej) { reply_json(c, istatus, ej); free(ej); return; }
      mg_printf(c,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lu\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "Cache-Control: public, max-age=30\r\n\r\n",
        ictype[0] ? ictype : "image/jpeg", (unsigned long)ilen);
      mg_send(c, img, ilen);
      c->is_resp = 0;
      free(img);
      return;
    }

    /* GET /api/data/cameras/run-status — live fan-out progress. Plain-auth,
     * beside discovery-feed. The clients' progress UI (useCameraDiscoveryStream
     * / LayersTab's `triggering`) was written against WebSocket events the C
     * backend has never emitted — there is no /ws server here at all — so this
     * poll endpoint is what makes a running fan-out observable. */
    if (eq(u, "/api/data/cameras/run-status")) {
      pthread_mutex_lock(&g_camlock);
      int running = g_cam.running, tot = g_cam.channels_total,
          done = g_cam.channels_done, skip = g_cam.channels_skipped;
      long long ing = g_cam.ingested, lastIng = g_cam.last_ingested;
      long lastDur = g_cam.last_duration_ms;
      time_t fin = g_cam.last_finished;
      char rid[40], sat[40], cur[80];
      snprintf(rid, sizeof rid, "%s", g_cam.run_id);
      snprintf(sat, sizeof sat, "%s", g_cam.started_at);
      snprintf(cur, sizeof cur, "%s", g_cam.current);
      pthread_mutex_unlock(&g_camlock);

      long cooldown = 0;
      if (fin) {
        long since = (long)(time(NULL) - fin);
        cooldown = since >= CAM_COOLDOWN_SEC ? 0 : CAM_COOLDOWN_SEC - since;
      }
      cJSON *o = cJSON_CreateObject();
      cJSON_AddBoolToObject(o, "running", running);
      if (rid[0]) cJSON_AddStringToObject(o, "run_id", rid);
      else        cJSON_AddNullToObject(o, "run_id");
      if (sat[0]) cJSON_AddStringToObject(o, "started_at", sat);
      else        cJSON_AddNullToObject(o, "started_at");
      if (cur[0]) cJSON_AddStringToObject(o, "current_channel", cur);
      else        cJSON_AddNullToObject(o, "current_channel");
      cJSON_AddNumberToObject(o, "channels_total",   tot);
      cJSON_AddNumberToObject(o, "channels_done",    done);
      cJSON_AddNumberToObject(o, "channels_skipped", skip);
      cJSON_AddNumberToObject(o, "cameras_ingested", (double)ing);
      cJSON_AddNumberToObject(o, "cooldown_remaining_sec", (double)cooldown);
      if (fin) {
        cJSON_AddNumberToObject(o, "last_duration_ms", (double)lastDur);
        cJSON_AddNumberToObject(o, "last_ingested",    (double)lastIng);
      } else {
        cJSON_AddNullToObject(o, "last_duration_ms");
        cJSON_AddNullToObject(o, "last_ingested");
      }
      char *body = cJSON_PrintUnformatted(o);
      cJSON_Delete(o);
      if (!body) { reply_json(c, 500, "{\"error\":\"oom\"}"); return; }
      reply_json(c, 200, body); free(body); return;
    }

    /* POST /api/data/cameras/trigger — fan out over every camera-discovery
     * channel on a detached thread (== Node withCollectorRun, not awaited).
     * Keeps the {started, already_running} contract both clients parse;
     * run_id / channels_total / skipped are additive. */
    if (eq(u, "/api/data/cameras/trigger")) {
      /* POST only — this fans out over every camera-discovery channel (16
       * scrapers) on a detached thread. A GET that does that is a write path
       * any prefetch can fire. */
      if (hm->method.len != 4 || memcmp(hm->method.buf, "POST", 4) != 0) {
        reply_json(c, 405, "{\"error\":\"method_not_allowed\"}"); return;
      }
      const source_def *ch[CAM_MAX_CHANNELS];
      int nch = cam_channels(ch, CAM_MAX_CHANNELS);
      /* No channels registered is a real fault, not a quiet no-op — the old
       * code returned started:false here and both clients read that as "fine",
       * which is how a dead trigger went unnoticed. */
      if (nch == 0) {
        reply_json(c, 500,
          "{\"started\":false,\"already_running\":false,"
          "\"error\":\"no_camera_channels_registered\"}");
        return;
      }
      /* Cooldown before the single-flight check: a caller inside the window
       * gets told why, not just "not started". */
      pthread_mutex_lock(&g_camlock);
      time_t fin = g_cam.last_finished;
      pthread_mutex_unlock(&g_camlock);
      if (fin) {
        long since = (long)(time(NULL) - fin);
        if (since < CAM_COOLDOWN_SEC) {
          char b[160];
          snprintf(b, sizeof b,
            "{\"started\":false,\"already_running\":false,"
            "\"skipped\":\"cooldown\",\"retry_after_sec\":%ld}",
            CAM_COOLDOWN_SEC - since);
          reply_json(c, 200, b);
          return;
        }
      }
      if (!run_begin(CAM_COLLECTOR)) {
        reply_json(c, 200,
          "{\"started\":false,\"already_running\":true}"); return;
      }
      cam_trig_arg *ta = calloc(1, sizeof *ta);
      if (!ta) {
        run_end(CAM_COLLECTOR);
        reply_json(c, 500,
          "{\"started\":false,\"already_running\":false}"); return;
      }
      ta->db = g_db;

      /* Arm the progress block BEFORE the thread exists, so a run-status poll
       * that lands between the reply and the thread's first tick already sees
       * running:true rather than a stale idle snapshot. */
      pthread_mutex_lock(&g_camlock);
      g_cam.running = 1;
      g_cam.channels_total = nch;
      g_cam.channels_done = 0;
      g_cam.channels_skipped = 0;
      g_cam.ingested = 0;
      g_cam.current[0] = 0;
      iso_now(g_cam.started_at, sizeof g_cam.started_at);
      snprintf(g_cam.run_id, sizeof g_cam.run_id, "cam-%llu",
               (unsigned long long)mg_millis());
      char run_id[40];
      snprintf(run_id, sizeof run_id, "%s", g_cam.run_id);
      pthread_mutex_unlock(&g_camlock);

      pthread_t th;
      if (pthread_create(&th, NULL, cam_trigger_thread, ta) == 0) {
        pthread_detach(th);
        char b[192];
        snprintf(b, sizeof b,
          "{\"started\":true,\"already_running\":false,"
          "\"run_id\":\"%.38s\",\"channels_total\":%d}", run_id, nch);
        reply_json(c, 200, b);
      } else {
        pthread_mutex_lock(&g_camlock);
        g_cam.running = 0;
        pthread_mutex_unlock(&g_camlock);
        run_end(CAM_COLLECTOR); free(ta);
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
      if (body) {
        /* Unported transit routes (hydrate, station-boundaries) return a
         * {"error":"not_implemented",...} body → send it as HTTP 501. */
        int code = strstr(body, "\"error\":\"not_implemented\"") ? 501 : 200;
        reply_json(c, code, body); free(body); return;
      }
    }


    reply_json(c, 404, "{\"error\":\"not_found\"}");
    return;
  }
  reply_json(c, 404, "{\"error\":\"not_found\"}");
}

/* Set by the signal handler, read by the poll loop. `volatile sig_atomic_t` is
 * the only object type a handler may portably write. */
static volatile sig_atomic_t g_shutdown_signal = 0;

static void on_shutdown_signal(int sig) { g_shutdown_signal = sig; }

static void httpd_install_signal_handlers(void) {
  /* sigaction, not signal(): no SA_RESTART, so a poll already in progress
   * returns EINTR promptly instead of waiting out its timeout, and the
   * disposition does not reset on some platforms. SIGPIPE is ignored because a
   * client vanishing mid-response must not kill the server — mongoose already
   * handles the short write. */
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = on_shutdown_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  struct sigaction ign;
  memset(&ign, 0, sizeof ign);
  ign.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &ign, NULL);
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
  mg_wakeup_init(&mgr);   /* enable off-loop replies (async suggest) */
  fprintf(stderr, "[httpd] listening on %s\n", url);

  /* This loop used to be `for (;;)`, i.e. httpd_serve never returned and the
   * process could only ever die by signal — SIGTERM's default action, taken
   * while ~8-16 scheduler workers were mid-fetch and possibly mid-emit(). The
   * shutdown sequence main() carefully performs after this call (drain the
   * collector pool, stop the pods, close the DB) was unreachable dead code.
   * Measured before this change: `kill -TERM` gave exit status 143, i.e.
   * death by signal, with collectors in flight.
   *
   * A flag written by a handler and read here is the whole mechanism; it must
   * be `volatile sig_atomic_t`, the only type the C standard permits a handler
   * to touch. The 100 ms poll bounds how long the flag goes unnoticed. */
  httpd_install_signal_handlers();
  while (!g_shutdown_signal) mg_mgr_poll(&mgr, 100);

  fprintf(stderr, "[httpd] signal %d received; shutting down\n",
          (int)g_shutdown_signal);
  mg_mgr_free(&mgr);
  return 0;
}
