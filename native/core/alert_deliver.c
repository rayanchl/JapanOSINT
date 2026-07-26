/* core/alert_deliver.c — background delivery of matched alerts.
 *
 * Design notes live in alert_deliver.h (schema DDL, env, retry ladder, the
 * webhook contract, and the exact orchestrator wiring). What is worth saying
 * next to the code:
 *
 *  - The tick is two phases with a hard boundary between them: enqueue (walk
 *    fresh alert_events → INSERT OR IGNORE one alert_deliveries row per
 *    channel) then drain (take due rows and send). Splitting them means a send
 *    that crashes the process leaves a durable, idempotent queue behind rather
 *    than a lost alert.
 *  - Every SELECT is fully read into memory and finalized BEFORE any UPDATE.
 *    We share one sqlite3* with the HTTP server; writing through a live
 *    statement on that connection while a webhook is blocking for 15s is how
 *    you hold a write lock across the network. Read fast, close, then send.
 *  - The channel's secret/target are re-read from alert_rules at send time,
 *    not cached at enqueue time, so a rotated secret or corrected URL takes
 *    effect on the next retry instead of the next event.
 *  - Nothing in the send path is allowed to be fatal. A malformed
 *    channels_json, a deleted rule, a vanished item, a hung upstream: each
 *    resolves to a delivery row status, never to a longjmp out of the thread.
 */
#include "alert_deliver.h"
#include "httpclient.h"
#include "intelapi.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_ATTEMPTS   5
#define DRAIN_BATCH    50
#define ENQUEUE_BATCH  200
#define WEBHOOK_TIMEOUT_MS 15000

/* Backoff after failure N (1-based). The 5th failure is terminal, so four
 * entries: 30s, 2m, 10m, 30m — fast enough to ride out a deploy blip, slow
 * enough that a genuinely dead endpoint is not hammered for an hour. */
static const int BACKOFF_SEC[MAX_ATTEMPTS - 1] = { 30, 120, 600, 1800 };

/* ── small shared idioms (mirrors alertsapi.c) ─────────────────────────── */

static void uuid4(char out[37]) {
  unsigned char b[16]; RAND_bytes(b, 16);
  b[6] = (b[6] & 0x0F) | 0x40; b[8] = (b[8] & 0x3F) | 0x80;
  snprintf(out, 37,
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
    b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
}
static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static char *err_env(int *st, int code, const char *msg) {
  *st = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}
static char *xdup(const char *s) { return s ? strdup(s) : NULL; }

static char *fmtdup(const char *fmt, ...) {
  char buf[512];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  return strdup(buf);
}

/* One channel attempt's verdict. `status` is a static literal; `error` is
 * malloc'd or NULL and owned by the caller. */
typedef struct { const char *status; long http_code; char *error; } outcome;

static const char *sv(const char *s, const char *dflt) { return s ? s : dflt; }

/* ── payload construction ──────────────────────────────────────────────── */

/* The matched item as the standard intel envelope's `data` object. A missing
 * item (purged, or a synthetic test) is NOT a delivery failure: the receiver
 * still gets rule + event, with a stub item carrying the uid. */
static cJSON *fetch_item(db_handle *db, const char *uid) {
  if (!uid || !*uid) return NULL;
  char *js = intelapi_item_by_uid(db, uid);
  cJSON *item = NULL;
  if (js) {
    cJSON *env = cJSON_Parse(js);
    if (env) { item = cJSON_DetachItemFromObject(env, "data"); cJSON_Delete(env); }
    free(js);
  }
  if (!item) {
    item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "uid", uid);
  }
  return item;
}

static char *build_payload(const char *rule_id, const char *rule_name,
                           const char *event_id, const char *matched_at,
                           const cJSON *item) {
  cJSON *root = cJSON_CreateObject();
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "id", sv(rule_id, ""));
  cJSON_AddStringToObject(r, "name", sv(rule_name, ""));
  cJSON_AddItemToObject(root, "rule", r);
  cJSON *e = cJSON_CreateObject();
  cJSON_AddStringToObject(e, "id", sv(event_id, ""));
  cJSON_AddStringToObject(e, "matched_at", sv(matched_at, ""));
  cJSON_AddItemToObject(root, "event", e);
  cJSON_AddItemToObject(root, "item",
    item ? cJSON_Duplicate((cJSON *)item, 1) : cJSON_CreateNull());
  char *js = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return js;
}

static const char *item_str(const cJSON *item, const char *key) {
  if (!item) return NULL;
  cJSON *v = cJSON_GetObjectItem((cJSON *)item, key);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

/* ── webhook ───────────────────────────────────────────────────────────── */

static void hmac_hex(const char *secret, const char *body, char out[65]) {
  unsigned char mac[32]; unsigned int mlen = 0;
  HMAC(EVP_sha256(), secret, (int)strlen(secret),
       (const unsigned char *)body, strlen(body), mac, &mlen);
  static const char *H = "0123456789abcdef";
  unsigned int n = mlen > 32 ? 32 : mlen;
  for (unsigned int i = 0; i < n; i++) {
    out[i*2]     = H[(mac[i] >> 4) & 0xF];
    out[i*2 + 1] = H[mac[i] & 0xF];
  }
  out[n * 2] = '\0';
}

static void deliver_webhook(const char *url, const char *secret,
                            const char *event_id, const char *body,
                            outcome *o) {
  char sig_hdr[96] = {0}, del_hdr[128];
  snprintf(del_hdr, sizeof del_hdr, "X-JO-Delivery: %s", sv(event_id, ""));
  if (secret && *secret) {
    char hex[65];
    hmac_hex(secret, body, hex);
    snprintf(sig_hdr, sizeof sig_hdr, "X-JO-Signature: sha256=%s", hex);
  }
  const char *hdrs[4];
  int h = 0;
  hdrs[h++] = "Content-Type: application/json";
  hdrs[h++] = del_hdr;
  if (sig_hdr[0]) hdrs[h++] = sig_hdr;
  hdrs[h] = NULL;

  http_client *c = http_client_new();
  if (!c) { o->status = "failed"; o->error = xdup("http_client_alloc_failed"); return; }
  http_response res = {0};
  /* retries=0: the alert_deliveries ladder owns retry, so one wire attempt
   * per ledger attempt — otherwise "attempt 3" would mean 9 real POSTs. */
  int hard = http_request(c, "POST", url, hdrs, body, strlen(body),
                          WEBHOOK_TIMEOUT_MS, 0, &res);
  o->http_code = res.status;
  if (hard || res.status == 0) {
    o->status = "failed";
    o->error = xdup("transport_error");
  } else if (res.status >= 200 && res.status < 300) {
    o->status = "ok";
  } else {
    o->status = "failed";
    /* A 120-char slice of the body is usually the entire reason a webhook is
     * rejected ("invalid signature", "unknown tenant"); keeping it turns a
     * silent 400 loop into a one-glance fix. */
    char snip[121] = {0};
    if (res.body) {
      size_t n = res.body_len < 120 ? res.body_len : 120;
      for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)res.body[i];
        snip[i] = (ch == '\n' || ch == '\r' || ch == '\t') ? ' '
                : (ch < 0x20 ? '.' : (char)ch);
      }
    }
    o->error = fmtdup("http_%ld %s", res.status, snip);
  }
  http_response_free(&res);
  http_client_free(c);
}

/* ── email (SMTP over libcurl directly; httpclient.h is HTTP-only) ─────── */

typedef struct { const char *data; size_t len, off; } mail_src;

static size_t mail_read(char *buf, size_t sz, size_t nm, void *ud) {
  mail_src *m = (mail_src *)ud;
  size_t room = sz * nm, left = m->len - m->off;
  size_t n = left < room ? left : room;
  if (n == 0) return 0;
  memcpy(buf, m->data + m->off, n);
  m->off += n;
  return n;
}

static void b64(const unsigned char *in, size_t n, char *out, size_t out_sz) {
  static const char *A =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t o = 0;
  for (size_t i = 0; i < n; i += 3) {
    if (o + 5 >= out_sz) break;
    unsigned v = (unsigned)in[i] << 16;
    if (i + 1 < n) v |= (unsigned)in[i+1] << 8;
    if (i + 2 < n) v |= (unsigned)in[i+2];
    out[o++] = A[(v >> 18) & 0x3F];
    out[o++] = A[(v >> 12) & 0x3F];
    out[o++] = (i + 1 < n) ? A[(v >> 6) & 0x3F] : '=';
    out[o++] = (i + 2 < n) ? A[v & 0x3F] : '=';
  }
  out[o] = '\0';
}

/* Rule names here are routinely Japanese. A raw 8-bit Subject: is illegal and
 * gets mangled or bounced by strict MTAs, so non-ASCII subjects go out as an
 * RFC 2047 base64 encoded-word. ASCII subjects stay readable in the raw log. */
static void encode_subject(const char *name, char *out, size_t out_sz) {
  char raw[256];
  snprintf(raw, sizeof raw, "[JapanOSINT] %s", sv(name, "alert"));
  int ascii = 1;
  for (const char *p = raw; *p; p++)
    if ((unsigned char)*p > 0x7F) { ascii = 0; break; }
  if (ascii) { snprintf(out, out_sz, "%s", raw); return; }
  char enc[512];
  b64((const unsigned char *)raw, strlen(raw), enc, sizeof enc);
  snprintf(out, out_sz, "=?UTF-8?B?%s?=", enc);
}

static int smtp_config(const char **url, const char **user,
                       const char **pass, const char **from) {
  *url  = getenv("JO_SMTP_URL");
  *user = getenv("JO_SMTP_USER");
  *pass = getenv("JO_SMTP_PASS");
  *from = getenv("JO_SMTP_FROM");
  return (*url && **url && *from && **from);
}

static pthread_once_t g_curl_once = PTHREAD_ONCE_INIT;
static void curl_boot(void) { curl_global_init(CURL_GLOBAL_DEFAULT); }

static void deliver_email(const char *to, const char *rule_name,
                          const char *event_id, const char *matched_at,
                          const cJSON *item, outcome *o) {
  const char *url, *user, *pass, *from;
  if (!smtp_config(&url, &user, &pass, &from)) {
    /* Terminal, not retryable: no amount of waiting configures an MTA. */
    o->status = "skipped";
    o->error = xdup("smtp_not_configured (set JO_SMTP_URL and JO_SMTP_FROM)");
    return;
  }
  pthread_once(&g_curl_once, curl_boot);

  char subj[640];
  encode_subject(rule_name, subj, sizeof subj);
  char date[64];
  time_t now = time(NULL);
  struct tm tmv;
  gmtime_r(&now, &tmv);
  strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S +0000", &tmv);

  const char *title = item_str(item, "title");
  const char *link  = item_str(item, "link");
  const char *src   = item_str(item, "source_id");
  const char *uid   = item_str(item, "uid");

  char *msg = NULL;
  size_t msg_len = 0;
  {
    /* Plain text/plain, no MIME multipart: a single-part message needs no
     * boundary machinery and renders identically everywhere. */
    char head[2048];
    int hn = snprintf(head, sizeof head,
      "Date: %s\r\nFrom: %s\r\nTo: %s\r\nSubject: %s\r\n"
      "Message-ID: <%s@japanosint>\r\nMIME-Version: 1.0\r\n"
      "Content-Type: text/plain; charset=UTF-8\r\n\r\n",
      date, from, to, subj, sv(event_id, "test"));
    if (hn < 0) hn = 0;
    if ((size_t)hn >= sizeof head) hn = (int)sizeof head - 1;
    char body[3072];
    int bn = snprintf(body, sizeof body,
      "Alert rule: %s\r\n\r\n%s\r\n%s\r\n\r\nSource: %s\r\nItem: %s\r\n"
      "Matched: %s\r\nEvent: %s\r\n\r\n-- \r\nJapanOSINT alert delivery\r\n",
      sv(rule_name, "(unnamed rule)"), sv(title, "(no title)"),
      sv(link, "(no link)"), sv(src, "-"), sv(uid, "-"),
      sv(matched_at, "-"), sv(event_id, "-"));
    if (bn < 0) bn = 0;
    if ((size_t)bn >= sizeof body) bn = (int)sizeof body - 1;
    msg_len = (size_t)hn + (size_t)bn;
    msg = malloc(msg_len + 1);
    if (!msg) { o->status = "failed"; o->error = xdup("oom"); return; }
    memcpy(msg, head, (size_t)hn);
    memcpy(msg + hn, body, (size_t)bn);
    msg[msg_len] = '\0';
  }

  CURL *e = curl_easy_init();
  if (!e) { free(msg); o->status = "failed"; o->error = xdup("curl_init_failed"); return; }
  char from_a[320], to_a[320];
  snprintf(from_a, sizeof from_a, "<%s>", from);
  snprintf(to_a, sizeof to_a, "<%s>", to);
  struct curl_slist *rcpt = curl_slist_append(NULL, to_a);
  mail_src src_buf = { msg, msg_len, 0 };

  curl_easy_setopt(e, CURLOPT_URL, url);
  if (user && *user) curl_easy_setopt(e, CURLOPT_USERNAME, user);
  if (pass && *pass) curl_easy_setopt(e, CURLOPT_PASSWORD, pass);
  /* Opportunistic TLS: upgrade when the server advertises STARTTLS, but do not
   * make an internal relay that offers none undeliverable. smtps:// URLs are
   * implicit-TLS and unaffected. */
  curl_easy_setopt(e, CURLOPT_USE_SSL, (long)CURLUSESSL_TRY);
  curl_easy_setopt(e, CURLOPT_MAIL_FROM, from_a);
  curl_easy_setopt(e, CURLOPT_MAIL_RCPT, rcpt);
  curl_easy_setopt(e, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(e, CURLOPT_READFUNCTION, mail_read);
  curl_easy_setopt(e, CURLOPT_READDATA, &src_buf);
  curl_easy_setopt(e, CURLOPT_INFILESIZE_LARGE, (curl_off_t)msg_len);
  curl_easy_setopt(e, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(e, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(e, CURLOPT_NOSIGNAL, 1L);

  CURLcode rc = curl_easy_perform(e);
  long code = 0;
  curl_easy_getinfo(e, CURLINFO_RESPONSE_CODE, &code);
  o->http_code = code;
  if (rc == CURLE_OK) {
    o->status = "ok";
  } else {
    o->status = "failed";
    o->error = fmtdup("smtp_%d %s", (int)rc, curl_easy_strerror(rc));
  }
  curl_slist_free_all(rcpt);
  curl_easy_cleanup(e);
  free(msg);
}

/* ── channel dispatch ──────────────────────────────────────────────────── */

/* Pull channel #idx out of a rule's channels_json. Returns 1 and fills the
 * (malloc'd) out params on success; 0 if the JSON is malformed or the index no
 * longer exists — which is how a channel deleted between enqueue and send is
 * detected. */
static int channel_at(const char *channels_json, int idx,
                      char **type, char **target, char **secret) {
  *type = *target = *secret = NULL;
  if (!channels_json || !*channels_json || idx < 0) return 0;
  cJSON *arr = cJSON_Parse(channels_json);
  if (!arr || !cJSON_IsArray(arr)) { if (arr) cJSON_Delete(arr); return 0; }
  cJSON *c = cJSON_GetArrayItem(arr, idx);
  int ok = 0;
  if (c && cJSON_IsObject(c)) {
    cJSON *ty = cJSON_GetObjectItem(c, "type");
    cJSON *tg = cJSON_GetObjectItem(c, "target");
    cJSON *se = cJSON_GetObjectItem(c, "secret");
    if (ty && cJSON_IsString(ty) && tg && cJSON_IsString(tg)) {
      *type   = strdup(ty->valuestring);
      *target = strdup(tg->valuestring);
      if (se && cJSON_IsString(se) && se->valuestring[0])
        *secret = strdup(se->valuestring);
      ok = 1;
    }
  }
  cJSON_Delete(arr);
  return ok;
}

static void deliver_one(const char *type, const char *target, const char *secret,
                        const char *rule_id, const char *rule_name,
                        const char *event_id, const char *matched_at,
                        const cJSON *item, outcome *o) {
  o->status = "failed"; o->http_code = 0; o->error = NULL;
  if (!type || !target || !*target) {
    o->status = "skipped"; o->error = xdup("channel_incomplete"); return;
  }
  if (strcmp(type, "webhook") == 0) {
    char *payload = build_payload(rule_id, rule_name, event_id, matched_at, item);
    if (!payload) { o->error = xdup("payload_build_failed"); return; }
    deliver_webhook(target, secret, event_id, payload, o);
    free(payload);
  } else if (strcmp(type, "email") == 0) {
    deliver_email(target, rule_name, event_id, matched_at, item, o);
  } else {
    /* alertsapi validates the type on write; anything else got in around the
     * API and will never become deliverable. Terminal. */
    o->status = "skipped";
    o->error = fmtdup("unsupported_channel_type:%s", type);
  }
}

/* ── ledger writes ─────────────────────────────────────────────────────── */

/* Recompute the event's human-facing summary from the ledger. Kept as an array
 * of STRINGS: ios Models.swift decodes delivered_channels as [String], so an
 * array of objects would break every existing client. Truth is preserved by
 * qualifying anything that is not a clean send: "webhook" (ok) vs
 * "webhook:dead" / "email:skipped". Sentinel rows (channel_idx<0) are omitted,
 * leaving [] → the client's honest "NO CHANNELS" state. */
static void refresh_event_channels(db_handle *db, const char *event_id) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT channel_type,status FROM alert_deliveries "
        "WHERE event_id=?1 AND channel_idx>=0 AND status<>'pending' "
        "ORDER BY channel_idx", -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, event_id, -1, SQLITE_TRANSIENT);
  cJSON *arr = cJSON_CreateArray();
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *ty = sv(ctext(s, 0), "?"), *st = sv(ctext(s, 1), "?");
    char lbl[64];
    if (strcmp(st, "ok") == 0) snprintf(lbl, sizeof lbl, "%s", ty);
    else                       snprintf(lbl, sizeof lbl, "%s:%s", ty, st);
    cJSON_AddItemToArray(arr, cJSON_CreateString(lbl));
  }
  sqlite3_finalize(s);
  char *js = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  if (!js) return;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE alert_events SET delivered_channels_json=?1 WHERE id=?2",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, js, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, event_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
  free(js);
}

/* Persist an attempt's result. `backoff` >0 only when status is "pending". */
static void finish_delivery(db_handle *db, sqlite3_int64 row_id, int attempt,
                            const char *status, long http_code,
                            const char *error, int backoff) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE alert_deliveries SET attempt=?1,status=?2,http_code=?3,"
        "error=?4,attempted_at=datetime('now'),"
        "next_attempt_at=CASE WHEN ?2='pending' THEN datetime('now',?5) END "
        "WHERE id=?6", -1, &s, NULL) != SQLITE_OK) return;
  char modifier[32];
  snprintf(modifier, sizeof modifier, "+%d seconds", backoff > 0 ? backoff : 0);
  sqlite3_bind_int (s, 1, attempt);
  sqlite3_bind_text(s, 2, status, -1, SQLITE_TRANSIENT);
  if (http_code > 0) sqlite3_bind_int64(s, 3, http_code); else sqlite3_bind_null(s, 3);
  if (error && *error) sqlite3_bind_text(s, 4, error, -1, SQLITE_TRANSIENT);
  else sqlite3_bind_null(s, 4);
  sqlite3_bind_text(s, 5, modifier, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(s, 6, row_id);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

/* ── phase 1: enqueue ──────────────────────────────────────────────────── */

typedef struct {
  char *event_id, *channels_json, *skip_reason;
} pending_event;

static void enqueue_pending(db_handle *db) {
  long lookback = 3600;
  const char *lb = getenv("JO_ALERT_DELIVER_LOOKBACK_SEC");
  if (lb && *lb) { long v = strtol(lb, NULL, 10); if (v >= 60) lookback = v; }
  char horizon[48];
  snprintf(horizon, sizeof horizon, "-%ld seconds", lookback);

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT e.id, r.channels_json, r.enabled, "
        "  (r.muted_until IS NOT NULL AND r.muted_until>datetime('now')), r.id "
        "FROM alert_events e "
        "LEFT JOIN alert_rules r ON r.id=e.rule_id AND r.tenant_id=e.tenant_id "
        "WHERE e.suppressed=0 AND e.matched_at>=datetime('now',?1) "
        "  AND NOT EXISTS (SELECT 1 FROM alert_deliveries d WHERE d.event_id=e.id) "
        "ORDER BY e.matched_at LIMIT ?2", -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, horizon, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, 2, ENQUEUE_BATCH);

  pending_event *evs = calloc(ENQUEUE_BATCH, sizeof *evs);
  if (!evs) { sqlite3_finalize(s); return; }
  int n = 0;
  while (n < ENQUEUE_BATCH && sqlite3_step(s) == SQLITE_ROW) {
    const char *rule_id = ctext(s, 4);
    evs[n].event_id = xdup(ctext(s, 0));
    if (!evs[n].event_id) continue;
    if (!rule_id)                       evs[n].skip_reason = xdup("rule_deleted");
    else if (sqlite3_column_int(s, 2) == 0) evs[n].skip_reason = xdup("rule_disabled");
    else if (sqlite3_column_int(s, 3) != 0) evs[n].skip_reason = xdup("rule_muted");
    else evs[n].channels_json = xdup(ctext(s, 1));
    n++;
  }
  sqlite3_finalize(s);   /* close before writing: see file header */

  sqlite3_stmt *ins = NULL, *skip = NULL;
  sqlite3_prepare_v2(db->h,
    "INSERT OR IGNORE INTO alert_deliveries "
    "(event_id,channel_idx,channel_type,target,attempt,status,next_attempt_at) "
    "VALUES (?1,?2,?3,?4,0,'pending',datetime('now'))", -1, &ins, NULL);
  sqlite3_prepare_v2(db->h,
    "INSERT OR IGNORE INTO alert_deliveries "
    "(event_id,channel_idx,channel_type,target,attempt,status,error,attempted_at) "
    "VALUES (?1,-1,'none',NULL,0,'skipped',?2,datetime('now'))", -1, &skip, NULL);

  int queued = 0, skipped = 0;
  for (int i = 0; i < n; i++) {
    const char *reason = evs[i].skip_reason;
    int added = 0;
    if (!reason && ins) {
      cJSON *arr = evs[i].channels_json ? cJSON_Parse(evs[i].channels_json) : NULL;
      if (arr && cJSON_IsArray(arr)) {
        int idx = 0; cJSON *c;
        cJSON_ArrayForEach(c, arr) {
          cJSON *ty = cJSON_IsObject(c) ? cJSON_GetObjectItem(c, "type") : NULL;
          cJSON *tg = cJSON_IsObject(c) ? cJSON_GetObjectItem(c, "target") : NULL;
          if (ty && cJSON_IsString(ty) && tg && cJSON_IsString(tg)) {
            sqlite3_bind_text(ins, 1, evs[i].event_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int (ins, 2, idx);
            sqlite3_bind_text(ins, 3, ty->valuestring, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins, 4, tg->valuestring, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(ins) == SQLITE_DONE) added++;
            sqlite3_reset(ins);
            sqlite3_clear_bindings(ins);
          }
          idx++;
        }
      }
      if (arr) cJSON_Delete(arr);
      if (!added) reason = "no_deliverable_channels";
    }
    if (!added && reason && skip) {
      /* Sentinel row: resolves the event so the scan does not see it again. */
      sqlite3_bind_text(skip, 1, evs[i].event_id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(skip, 2, reason, -1, SQLITE_TRANSIENT);
      sqlite3_step(skip);
      sqlite3_reset(skip);
      sqlite3_clear_bindings(skip);
      skipped++;
    }
    queued += added;
    free(evs[i].event_id); free(evs[i].channels_json); free(evs[i].skip_reason);
  }
  if (ins)  sqlite3_finalize(ins);
  if (skip) sqlite3_finalize(skip);
  free(evs);
  if (queued || skipped)
    fprintf(stderr, "[alert-deliver] enqueued %d channel(s), %d event(s) skipped\n",
            queued, skipped);
}

/* ── phase 2: drain ────────────────────────────────────────────────────── */

typedef struct {
  sqlite3_int64 id;
  int channel_idx, attempt;
  char *event_id, *rule_id, *rule_name, *item_uid, *matched_at, *channels_json;
} dtask;

static void dtask_free(dtask *t) {
  free(t->event_id); free(t->rule_id); free(t->rule_name);
  free(t->item_uid); free(t->matched_at); free(t->channels_json);
}

static void drain_once(db_handle *db) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT d.id,d.channel_idx,d.attempt,d.event_id,e.rule_id,e.item_uid,"
        "       e.matched_at,r.name,r.channels_json "
        "FROM alert_deliveries d "
        "JOIN alert_events e ON e.id=d.event_id "
        "LEFT JOIN alert_rules r ON r.id=e.rule_id AND r.tenant_id=e.tenant_id "
        "WHERE d.status='pending' AND d.channel_idx>=0 AND e.suppressed=0 "
        "  AND (d.next_attempt_at IS NULL OR d.next_attempt_at<=datetime('now')) "
        "ORDER BY d.id LIMIT ?1", -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_int(s, 1, DRAIN_BATCH);

  dtask *batch = calloc(DRAIN_BATCH, sizeof *batch);
  if (!batch) { sqlite3_finalize(s); return; }
  int n = 0;
  while (n < DRAIN_BATCH && sqlite3_step(s) == SQLITE_ROW) {
    batch[n].id            = sqlite3_column_int64(s, 0);
    batch[n].channel_idx   = sqlite3_column_int(s, 1);
    batch[n].attempt       = sqlite3_column_int(s, 2);
    batch[n].event_id      = xdup(ctext(s, 3));
    batch[n].rule_id       = xdup(ctext(s, 4));
    batch[n].item_uid      = xdup(ctext(s, 5));
    batch[n].matched_at    = xdup(ctext(s, 6));
    batch[n].rule_name     = xdup(ctext(s, 7));
    batch[n].channels_json = xdup(ctext(s, 8));
    if (!batch[n].event_id) { dtask_free(&batch[n]); memset(&batch[n], 0, sizeof batch[n]); continue; }
    n++;
  }
  sqlite3_finalize(s);   /* no open cursor while we sit on the network */

  for (int i = 0; i < n; i++) {
    dtask *t = &batch[i];
    int attempt = t->attempt + 1;

    if (!t->channels_json) {   /* rule vanished after enqueue */
      finish_delivery(db, t->id, attempt, "dead", 0, "rule_deleted", 0);
      refresh_event_channels(db, t->event_id);
      dtask_free(t);
      continue;
    }
    char *type = NULL, *target = NULL, *secret = NULL;
    if (!channel_at(t->channels_json, t->channel_idx, &type, &target, &secret)) {
      finish_delivery(db, t->id, attempt, "dead", 0, "channel_removed", 0);
      refresh_event_channels(db, t->event_id);
      free(type); free(target); free(secret);
      dtask_free(t);
      continue;
    }

    cJSON *item = fetch_item(db, t->item_uid);
    outcome o = { "failed", 0, NULL };
    deliver_one(type, target, secret, t->rule_id, t->rule_name,
                t->event_id, t->matched_at, item, &o);
    if (item) cJSON_Delete(item);

    const char *final_status = o.status;
    int backoff = 0;
    if (strcmp(o.status, "failed") == 0) {
      if (attempt >= MAX_ATTEMPTS) final_status = "dead";
      else { final_status = "pending"; backoff = BACKOFF_SEC[attempt - 1]; }
    }
    finish_delivery(db, t->id, attempt, final_status, o.http_code, o.error, backoff);
    if (strcmp(final_status, "pending") != 0)
      refresh_event_channels(db, t->event_id);
    if (strcmp(final_status, "ok") != 0)
      fprintf(stderr, "[alert-deliver] %s ch#%d %s -> %s (attempt %d/%d) %s\n",
              t->event_id, t->channel_idx, type, final_status, attempt,
              MAX_ATTEMPTS, o.error ? o.error : "");
    free(o.error);
    free(type); free(target); free(secret);
    dtask_free(t);
  }
  free(batch);
}

/* ── worker thread ─────────────────────────────────────────────────────── */

static pthread_t   g_thread;
static volatile int g_stop = 0;
static int          g_running = 0;

static void *deliver_thread(void *arg) {
  db_handle *db = (db_handle *)arg;
  int interval = 5;
  const char *iv = getenv("JO_ALERT_DELIVER_INTERVAL_SEC");
  if (iv && *iv) {
    long v = strtol(iv, NULL, 10);
    if (v >= 1 && v <= 300) interval = (int)v;
  }
  while (!g_stop) {
    enqueue_pending(db);
    drain_once(db);
    /* Chunked sleep so shutdown is bounded by 1s, not by the poll interval. */
    for (int i = 0; i < interval && !g_stop; i++) sleep(1);
  }
  return NULL;
}

void alert_deliver_start(db_handle *db) {
  if (g_running) return;
  if (getenv("JO_NO_ALERT_DELIVER")) {
    fprintf(stderr, "[alert-deliver] disabled (JO_NO_ALERT_DELIVER)\n");
    return;
  }
  if (!db || !db->h) return;
  pthread_once(&g_curl_once, curl_boot);
  g_stop = 0;
  if (pthread_create(&g_thread, NULL, deliver_thread, db) == 0) {
    g_running = 1;
    fprintf(stderr, "[alert-deliver] worker started\n");
  } else {
    fprintf(stderr, "[alert-deliver] pthread_create failed; alerts will not be sent\n");
  }
}

void alert_deliver_stop(void) {
  if (!g_running) return;
  g_stop = 1;
  pthread_join(g_thread, NULL);
  g_running = 0;
  fprintf(stderr, "[alert-deliver] worker stopped\n");
}

/* ── operator test path ────────────────────────────────────────────────── */

char *alert_deliver_test(db_handle *db, const char *tenant_id,
                         const char *rule_id, int *status) {
  if (!db || !db->h || !tenant_id || !rule_id) return err_env(status, 400, "bad_request");

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT name,channels_json FROM alert_rules WHERE id=?1 AND tenant_id=?2",
        -1, &s, NULL) != SQLITE_OK)
    return err_env(status, 500, "server_error");
  sqlite3_bind_text(s, 1, rule_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant_id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) {
    sqlite3_finalize(s);
    return err_env(status, 404, "Not found");
  }
  char *rule_name = xdup(ctext(s, 0));
  char *channels  = xdup(ctext(s, 1));
  sqlite3_finalize(s);

  char uid[37]; uuid4(uid);
  char event_id[48];
  snprintf(event_id, sizeof event_id, "test-%s", uid);
  char matched_at[32];
  {
    time_t now = time(NULL); struct tm tmv; gmtime_r(&now, &tmv);
    strftime(matched_at, sizeof matched_at, "%Y-%m-%d %H:%M:%S", &tmv);
  }
  /* A synthetic item, not the rule's last real match: a test must produce the
   * same payload shape on a brand-new rule that has never fired. */
  cJSON *item = cJSON_CreateObject();
  cJSON_AddStringToObject(item, "uid", event_id);
  cJSON_AddStringToObject(item, "title", "JapanOSINT alert delivery test");
  cJSON_AddStringToObject(item, "source_id", "_test");
  cJSON_AddNullToObject(item, "link");
  cJSON_AddStringToObject(item, "summary",
    "Synthetic event generated by POST /api/alerts/:id/test. No real match occurred.");
  cJSON_AddBoolToObject(item, "test", 1);

  cJSON *results = cJSON_CreateArray();
  int all_ok = 1, tried = 0;
  cJSON *arr = channels ? cJSON_Parse(channels) : NULL;
  if (arr && cJSON_IsArray(arr)) {
    sqlite3_stmt *ins = NULL;
    sqlite3_prepare_v2(db->h,
      "INSERT OR IGNORE INTO alert_deliveries "
      "(event_id,channel_idx,channel_type,target,attempt,status,http_code,error,"
      " attempted_at) VALUES (?1,?2,?3,?4,1,?5,?6,?7,datetime('now'))",
      -1, &ins, NULL);
    int idx = 0; cJSON *c;
    cJSON_ArrayForEach(c, arr) {
      cJSON *ty = cJSON_IsObject(c) ? cJSON_GetObjectItem(c, "type") : NULL;
      cJSON *tg = cJSON_IsObject(c) ? cJSON_GetObjectItem(c, "target") : NULL;
      cJSON *se = cJSON_IsObject(c) ? cJSON_GetObjectItem(c, "secret") : NULL;
      if (ty && cJSON_IsString(ty) && tg && cJSON_IsString(tg)) {
        outcome o = { "failed", 0, NULL };
        deliver_one(ty->valuestring, tg->valuestring,
                    (se && cJSON_IsString(se)) ? se->valuestring : NULL,
                    rule_id, rule_name, event_id, matched_at, item, &o);
        tried++;
        if (strcmp(o.status, "ok") != 0) all_ok = 0;
        cJSON *r = cJSON_CreateObject();
        cJSON_AddNumberToObject(r, "channel_idx", idx);
        cJSON_AddStringToObject(r, "type", ty->valuestring);
        cJSON_AddStringToObject(r, "target", tg->valuestring);
        cJSON_AddStringToObject(r, "status", o.status);
        cJSON_AddItemToObject(r, "http_code",
          o.http_code > 0 ? cJSON_CreateNumber((double)o.http_code) : cJSON_CreateNull());
        cJSON_AddItemToObject(r, "error",
          o.error ? cJSON_CreateString(o.error) : cJSON_CreateNull());
        cJSON_AddItemToArray(results, r);
        if (ins) {
          sqlite3_bind_text(ins, 1, event_id, -1, SQLITE_TRANSIENT);
          sqlite3_bind_int (ins, 2, idx);
          sqlite3_bind_text(ins, 3, ty->valuestring, -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(ins, 4, tg->valuestring, -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(ins, 5, o.status, -1, SQLITE_TRANSIENT);
          if (o.http_code > 0) sqlite3_bind_int64(ins, 6, o.http_code);
          else sqlite3_bind_null(ins, 6);
          if (o.error) sqlite3_bind_text(ins, 7, o.error, -1, SQLITE_TRANSIENT);
          else sqlite3_bind_null(ins, 7);
          sqlite3_step(ins);
          sqlite3_reset(ins);
          sqlite3_clear_bindings(ins);
        }
        free(o.error);
      }
      idx++;
    }
    if (ins) sqlite3_finalize(ins);
  }
  if (arr) cJSON_Delete(arr);
  cJSON_Delete(item);
  if (!tried) all_ok = 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "rule_id", rule_id);
  cJSON_AddItemToObject(data, "rule_name",
    rule_name ? cJSON_CreateString(rule_name) : cJSON_CreateNull());
  cJSON_AddStringToObject(data, "event_id", event_id);
  cJSON_AddItemToObject(data, "results", results);
  cJSON *root = cJSON_CreateObject();
  /* ok/fired stay at the top level: the shipped clients read those two keys. */
  cJSON_AddBoolToObject(root, "ok", all_ok);
  cJSON_AddBoolToObject(root, "fired", 1);
  cJSON_AddItemToObject(root, "data", data);
  char *js = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  free(rule_name); free(channels);
  *status = 200;
  return js ? js : err_env(status, 500, "server_error");
}
