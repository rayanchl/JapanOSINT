/* core/alert_deliver.h — the thing that actually sends the alert.
 *
 * alertsapi.c persists rules and the matcher persists alert_events, but until
 * now nothing ever left the box: `delivered_channels_json` was written once at
 * match time and never reflected a real send. This module is the missing
 * transport half — a single background thread that drains undelivered events
 * and pushes them over each rule's channels (webhook POST, SMTP email).
 *
 * Why a separate table instead of more columns on alert_events: an event fans
 * out to N channels, each with its own attempt count, HTTP code and error. One
 * JSON blob on the event cannot express "webhook retried 4x with 502 while the
 * email went out first try", which is exactly what an operator debugging a
 * silent alert needs. alert_deliveries is that per-channel ledger;
 * delivered_channels_json stays the human-facing summary the iOS client reads.
 *
 * Two deliberate anti-poison-queue decisions, both learned from the fact that
 * this deploy already has a backlog of never-delivered events:
 *   1. Only events matched within JO_ALERT_DELIVER_LOOKBACK_SEC (default 1h)
 *      are ever enqueued. Booting this worker must not blast months of history
 *      at a customer's webhook.
 *   2. A structurally undeliverable event (SMTP unconfigured, rule deleted,
 *      channel removed, no channels) is marked terminal — `skipped`/`dead` —
 *      never retried. An unconfigured deploy accumulates nothing.
 *
 * ── REQUIRED DDL (orchestrator: append verbatim to core/schema.sql) ─────────
 *
 * CREATE TABLE IF NOT EXISTS alert_deliveries (
 *   id INTEGER PRIMARY KEY AUTOINCREMENT,
 *   event_id TEXT NOT NULL, channel_idx INTEGER NOT NULL,
 *   channel_type TEXT NOT NULL, target TEXT,
 *   attempt INTEGER NOT NULL DEFAULT 0,
 *   status TEXT NOT NULL,            -- pending|ok|failed|dead|skipped
 *   http_code INTEGER, error TEXT,
 *   next_attempt_at TEXT, attempted_at TEXT);
 *
 * CREATE INDEX IF NOT EXISTS idx_alert_deliveries_due
 *   ON alert_deliveries(status, next_attempt_at);
 * CREATE INDEX IF NOT EXISTS idx_alert_deliveries_event
 *   ON alert_deliveries(event_id);
 * CREATE UNIQUE INDEX IF NOT EXISTS idx_alert_deliveries_uniq
 *   ON alert_deliveries(event_id, channel_idx);
 *
 * The UNIQUE index is load-bearing, not an optimisation: enqueue is
 * INSERT OR IGNORE, so a crash mid-enqueue or a second tick racing the first
 * can never double-send a channel. `channel_idx = -1` is the reserved sentinel
 * row meaning "this event was resolved without any channel being tried"
 * (rule deleted/disabled/muted, or the rule has no channels); it is what stops
 * such an event from being rescanned forever, and it is excluded from the
 * delivered_channels summary so the UI shows "NO CHANNELS", not a fake send.
 *
 * ── ENVIRONMENT ─────────────────────────────────────────────────────────────
 *   JO_SMTP_URL   e.g. "smtp://mail.example.com:587" (or smtps://). Required
 *                 for email; absent → every email channel is `skipped`.
 *   JO_SMTP_FROM  envelope + header From. Required for email.
 *   JO_SMTP_USER  optional SMTP AUTH user (presence enables STARTTLS-if-avail)
 *   JO_SMTP_PASS  optional SMTP AUTH password
 *   JO_NO_ALERT_DELIVER            set → worker does not start (tests/CLI)
 *   JO_ALERT_DELIVER_INTERVAL_SEC  poll interval, default 5, clamped 1..300
 *   JO_ALERT_DELIVER_LOOKBACK_SEC  enqueue horizon, default 3600, min 60
 *
 * ── WEBHOOK CONTRACT (what the receiver sees) ───────────────────────────────
 *   POST <channel.target>
 *   Content-Type: application/json
 *   X-JO-Delivery: <event_id>
 *   X-JO-Signature: sha256=<lowercase hex HMAC-SHA256(secret, raw body)>
 *   {"rule":{"id","name"},"event":{"id","matched_at"},"item":{…intel envelope}}
 * The signature header is omitted entirely when the channel has no secret
 * (alertsapi validation requires one on create, but an older or hand-edited
 * row may lack it — that is not a reason to drop the alert).
 *
 * ── RETRY LADDER ────────────────────────────────────────────────────────────
 * 5 attempts, backoff 30s / 2m / 10m / 30m, then `dead`. 2xx is success; any
 * other HTTP status or a transport failure is retryable. Retries live here,
 * not in http_request() (retries=0 is passed deliberately) so that the ledger
 * and the backoff are the single source of truth about attempt count.
 */
#ifndef JO_ALERT_DELIVER_H
#define JO_ALERT_DELIVER_H
#include "db.h"

/* Start the background delivery worker (one detachable-but-joined pthread).
 * Idempotent: a second call while running is a no-op. No-op if the env var
 * JO_NO_ALERT_DELIVER is set. `db` must outlive the worker.
 *
 * ORCHESTRATOR WIRING — native/main.c, in the `if (!selftest)` serve block,
 * immediately after `scheduler_start_background(&db);` (~line 151):
 *
 *     #include "core/alert_deliver.h"        // with the other core includes
 *     ...
 *     scheduler_start_background(&db);
 *     alert_deliver_start(&db);              // ADD
 *     int rc = httpd_serve(&db, port);
 *     alert_deliver_stop();                  // ADD — join before db_close
 *     db_close(&db);
 */
void alert_deliver_start(db_handle *db);

/* Signal the worker to stop and join it (returns within ~1s: the sleep is
 * chunked). Safe to call when not running. */
void alert_deliver_stop(void);

/* Synthesize a test event for `rule_id` and push it through that rule's
 * channels RIGHT NOW — one attempt each, no retry, no alert_events row (a test
 * must not pollute the operator's event feed). The per-channel result is still
 * recorded in alert_deliveries under a `test-<uuid>` event_id so a failed
 * webhook is diagnosable after the HTTP response is gone.
 *
 * Returns a malloc'd JSON body, caller frees; sets *status. 404 + a
 * {"error":...} body if the rule does not exist for this tenant, otherwise
 * 200 with:
 *   {"ok":<all channels ok>,"fired":true,
 *    "data":{"rule_id","rule_name","event_id","results":[
 *       {"channel_idx","type","target","status","http_code","error"}]}}
 * A channel failing is NOT an API error — that is the whole point of the
 * endpoint — so the status stays 200 and "ok" carries the verdict.
 * Secrets are never echoed. Never returns NULL for a valid tenant/rule.
 *
 * ORCHESTRATOR WIRING (P0.3) — the endpoint already exists as a non-dispatching
 * stub. Replace the body of the `test` action in core/alertsapi.c (~line 404,
 * the block returning "{\"ok\":true,\"fired\":true}") with:
 *
 *     if (is_post && strcmp(action,"test")==0) {
 *       if (jb) cJSON_Delete(jb);
 *       return alert_deliver_test(db, tid, id, st);   // 404 handled inside
 *     }
 *
 * (plus `#include "alert_deliver.h"`). No httpd.c change is needed — the route
 * POST /api/alerts/:id/test already reaches alertsapi(). Requires role analyst
 * or better; apply the same role gate the surrounding mutations use.
 *
 * CLIENT NOTE: ios/JapanOsintApp/API.swift:458 decodes this response as
 * `[String: Bool]`, which cannot hold the nested "data" object. That line must
 * become a small Decodable struct (or `_ = try await postRaw(...)`) in the same
 * change, or alertTest() will throw on an otherwise successful test. */
char *alert_deliver_test(db_handle *db, const char *tenant_id,
                         const char *rule_id, int *status);

#endif
