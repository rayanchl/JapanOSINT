/* core/breach_monitor.h — Roadmap 24: breach EXPOSURE MONITORING.
 *
 * breach_index.c answers "was this identifier in a breach?" once, on demand.
 * This module is the standing-order half: an analyst registers an identifier
 * they care about and the product tells them when a corpus ingest lands on it,
 * without them ever asking again. That is the actual HIBP product — the lookup
 * is the demo, the monitor is what people pay for.
 *
 * ── WHY THE MONITOR TABLE STORES NO PLAINTEXT ──────────────────────────────
 *
 * A table of "email addresses our customers care about" is a higher-value
 * target than the breach corpus itself: the corpus is already public, the
 * monitor list is a curated, de-duplicated, currently-valid roster of the
 * people an organisation is worried about. So breach_monitors stores ONLY the
 * SHA-1 hex of the normalized identifier (`value_hash`), which is exactly the
 * key breach_items.hash is written with, and nothing echoes it back — not the
 * list endpoint, not the detail endpoint, not the audit payload. `label` is
 * the operator's own free-text handle for the row ("CFO", "support alias") and
 * is the only human-readable thing on it. A dump of this table tells an
 * attacker how MANY identifiers a tenant watches, not which.
 *
 * Consequence, stated plainly so nobody "fixes" it later: a monitor cannot be
 * un-hashed. There is no endpoint that returns the monitored address, and
 * there cannot be one. An operator who forgets which address a monitor covers
 * deletes it and creates a new one.
 *
 * NORMALIZATION IS DELIBERATELY DUMB. lowercase + trim, and for phones the
 * same "+ then digits only" reduction core/breach_index.c's norm() applies —
 * that symmetry is load-bearing, a monitor normalized differently from the
 * corpus can never match. What we explicitly do NOT do is Gmail-style dot/plus
 * canonicalization (f.o.o+spam@gmail.com → foo@gmail.com). It is tempting and
 * it is wrong here: it would silently monitor a DIFFERENT address than the one
 * the user typed, and the whole feature is a promise about one specific
 * identifier. Users who want the canonical form can register it themselves.
 *
 * ── DOMAIN MONITORS: WHY breach_items NEEDS A COLUMN ───────────────────────
 *
 * kind='domain' ("alert me when anything @acme.co.jp shows up") cannot be
 * hash-matched: the corpus is keyed on the hash of the FULL address, and the
 * set of addresses at a domain is unbounded and unknown. The only honest match
 * is a suffix test over breach_items.value.
 *
 * `WHERE value LIKE '%@acme.co.jp'` is NOT viable at this corpus size, and the
 * reason is structural rather than a matter of tuning: a leading wildcard makes
 * the predicate unindexable, so every domain monitor becomes a full scan of
 * breach_items — a table that materializes 1,018 breaches and is designed in
 * core/breach_store.h to hold "millions–billions of rows". One such scan per
 * monitor per ingest, on the ingest thread, would make corpus loading a
 * function of how many domain monitors exist. It also cannot use breach_fts:
 * that index tokenizes `value`, and a tokenized index cannot express "ends
 * with this exact host" without false positives across subdomains.
 *
 * So domain matching gets a real indexable column: breach_items.value_domain,
 * the lowercased text after the first '@' of `value`, populated for
 * type='email' rows only (usernames and phones have no domain, passwords have
 * no `value` at all). A domain monitor is then one index seek on an equality
 * predicate — the same cost shape as a hash monitor.
 *
 * BACKFILL. The column arrives on a table that already has the corpus in it,
 * so it is NULL for every existing row and the feature would silently see
 * nothing. Two population paths, both in this module, both idempotent:
 *
 *   · breach_monitor_scan_new() backfills only the rows of the source it was
 *     just handed (indexed by breach_items_source_idx), and only when at least
 *     one domain monitor exists anywhere — an ingest on a deploy with no
 *     domain monitors pays nothing.
 *   · breach_monitor_rescan_all() runs a full batched pass before matching,
 *     walking id windows (see BACKFILL_WINDOW) rather than repeatedly issuing
 *     `... WHERE value_domain IS NULL LIMIT n`, which re-scans the already-done
 *     prefix on every batch and is quadratic. Each window is its own
 *     transaction and *cancel is honoured between windows, so a shutdown
 *     mid-backfill loses at most one window and the next run resumes where the
 *     NULLs still are.
 *
 * Extraction uses instr(value,'@') — the FIRST '@'. RFC 5321 permits a quoted
 * local part containing '@'; the ingest corpus does not contain such addresses
 * and handling them would need a UDF. Documented, not hidden.
 *
 * ── HOW A HIT BECOMES A NOTIFICATION ───────────────────────────────────────
 *
 * A hit writes ONE alert_events row and stops. It does not send anything: the
 * existing worker in core/alert_deliver.c already drains that table, and a
 * second transport would mean two retry ladders, two ledgers and two ways for
 * an alert to go missing. The row is written to match what enqueue_pending()
 * in alert_deliver.c actually selects, exactly:
 *
 *     INSERT INTO alert_events
 *       (id, tenant_id, rule_id, item_uid, matched_at,
 *        delivered_channels_json, suppressed, reason)
 *     VALUES (<uuid4>, <monitor.tenant_id>, <rule_id>,
 *             'breach:'||<breach_items.keyid>, datetime('now'),
 *             '[]', 0, 'breach_monitor:'||<monitor.id>);
 *
 *   · suppressed=0 and matched_at=now are both required for the worker to see
 *     the row at all (it enqueues `suppressed=0 AND matched_at >= now-lookback`,
 *     default 1h). A hit that is not delivered within the lookback window is
 *     never delivered; that is the worker's anti-poison-queue rule and this
 *     module deliberately does not work around it.
 *   · rule_id is breach_monitors.rule_id when the operator attached one — that
 *     alert_rules row is where the CHANNELS live, so attaching a rule is what
 *     turns a monitor into an email/webhook. When no rule is attached we write
 *     the MONITOR's id into rule_id instead of leaving it NULL: the column is
 *     NOT NULL, and a monitor id is at least a stable, correct grouping key.
 *     The delivery worker's LEFT JOIN then finds no rule and writes exactly one
 *     terminal `skipped`/'rule_deleted' ledger row (channel_idx=-1) — no retry,
 *     no backlog. Such a hit is in-app only: it shows up in
 *     GET /api/alert-events (the inbox) and in GET /api/breach-monitors/:id/hits.
 *   · reason carries 'breach_monitor:<id>' on EVERY hit regardless of rule_id,
 *     so a hit is always attributable to the monitor that produced it.
 *   · Idempotence is by existence check against
 *     idx_alert_events_rule_item(rule_id, item_uid), not by a UNIQUE
 *     constraint (alert_events has none and adding one would rewrite a table
 *     this module does not own). Re-running any scan over the same rows
 *     therefore produces zero new events, which is what makes rescan safe to
 *     invoke repeatedly.
 *
 * KNOWN COSMETIC GAP the orchestrator should not be surprised by: alertsapi.c's
 * inbox query LEFT JOINs intel_items on item_uid, and a 'breach:*' uid has no
 * intel_items row, so item_title/item_source_id/item_link come back null for
 * these events. The uid is still correct and resolvable — intelapi_item_by_uid()
 * routes 'breach:' to breach_adapter_item_by_uid(), which is also how
 * alert_deliver.c's fetch_item() builds the webhook payload, so the DELIVERED
 * alert does carry the full (redacted) breach item. Only the inbox list's
 * denormalized preview columns are blank.
 *
 * ── SECRETS ────────────────────────────────────────────────────────────────
 *
 * Nothing here ever touches a leaked secret. /hits returns breach_meta catalog
 * metadata plus the synthetic `breach:<keyid>` uid and the has_secret FLAG, so
 * the client opens the record through the normal intel route and inherits
 * breach_adapter.c's redaction. The operator reveal path
 * (breach_adapter_reveal_by_uid) is not called, referenced or duplicated. Nor
 * does /hits return breach_items.value: for a hash monitor the caller already
 * knows the address, and for a domain monitor the matched addresses are
 * reachable through the intel route with the same gating as every other breach
 * record. One redaction chokepoint, not two.
 *
 * ── REQUIRED DDL (orchestrator: append verbatim to core/schema.sql) ────────
 *
 * CREATE TABLE IF NOT EXISTS breach_monitors (
 *   id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL,
 *   kind TEXT NOT NULL CHECK(kind IN ('email','domain','username','phone')),
 *   value_hash TEXT NOT NULL,        -- SHA-1 hex, matching breach_items.hash
 *   value_domain TEXT,               -- for kind='domain' only, lowercased
 *   label TEXT, rule_id TEXT, created_by TEXT,
 *   created_at TEXT NOT NULL DEFAULT (datetime('now')), last_checked_at TEXT);
 * CREATE INDEX IF NOT EXISTS idx_breach_monitors_hash ON breach_monitors(value_hash);
 * CREATE INDEX IF NOT EXISTS idx_breach_monitors_domain ON breach_monitors(value_domain);
 * CREATE INDEX IF NOT EXISTS idx_breach_monitors_tenant ON breach_monitors(tenant_id);
 *
 * -- added by this module: the hash join is the whole feature and breach_items
 * -- had NO index on `hash` (only type, (source_id,id) and UNIQUE(keyid)).
 * -- Without this every monitor check is a full corpus scan.
 * CREATE INDEX IF NOT EXISTS idx_breach_items_hash ON breach_items(hash);
 *
 * -- keyset page over the tenant's monitor list, sort key (created_at DESC, id)
 * CREATE INDEX IF NOT EXISTS idx_breach_monitors_tenant_created
 *   ON breach_monitors(tenant_id, created_at DESC, id);
 *
 * There is deliberately NO UNIQUE index on (tenant_id, kind, value_hash): the
 * duplicate check is done in the API so it can return 409 with the existing
 * monitor's id instead of a constraint error. If a later migration adds one,
 * this module keeps working.
 *
 * ── REQUIRED ensure_column (orchestrator: core/db.c boot-migration block) ──
 *
 * NEVER in schema.sql — see core/db.h. Add to db.c beside the existing
 * ensure_column() calls (~line 170, after the entity_relationships group):
 *
 *     ensure_column(db, "breach_items", "value_domain", "TEXT");
 *
 * The index over that column CANNOT go in schema.sql either: db_open() execs
 * schema.sql BEFORE the ensure_column block, so a `CREATE INDEX ... ON
 * breach_items(value_domain)` there would hit "no such column" on a database
 * created before this change, sqlite3_exec would abandon the REST of the
 * script, and boot would fail. It is created by breach_monitor_migrate()
 * instead, which does the ensure_column and the index together in the right
 * order. breach_monitor_migrate() is also self-healing: every entry point in
 * this file calls it behind a once-guard, so the module works even if the db.c
 * line is missed.
 *
 * ── WIRING (three call sites, all one line) ────────────────────────────────
 *
 * 1) core/breach_index.c — THE INGEST HOOK. In breach_index_ingest(), at the
 *    end, immediately after the store is finished (~line 323) so every row is
 *    committed and visible before we diff. Never inside the row loop and never
 *    before breach_store_finish(): a monitor scan must not be able to fail an
 *    ingest, and must not see a half-written batch.
 *
 *      #include "breach_monitor.h"                   // with the other includes
 *      ...
 *        if (store) breach_store_finish(store);
 *        if (store) breach_monitor_scan_new(db, source_id, NULL);   // <-- ADD
 *        wc_closeall(&wc);
 *
 *    `if (store)` is the right guard, not `if (db)`: `store` is already exactly
 *    "db != NULL && materialize && !dry_run", i.e. the only case in which rows
 *    reached breach_items at all. A dry run must stay side-effect free.
 *
 * 2) core/db.c — the ensure_column line above (or `breach_monitor_migrate(db);`
 *    after it, which is equivalent and also creates the index).
 *
 * 3) core/httpd.c — the route block. Add `#include "breach_monitor.h"` at the
 *    top and this block immediately after the `/api/alerts` block. Note the
 *    prefix lengths: "/api/breach-monitors" is 20 bytes, with the trailing
 *    slash 21 (where "/api/alerts/" is 12).
 *
 *   // ---- Roadmap 24: /api/breach-monitors (tenant-scoped exposure monitors) ----
 *   if (eq(u, "/api/breach-monitors") || starts(u, "/api/breach-monitors/")) {
 *     struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
 *     char xtid[128] = {0};
 *     if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
 *     tenant_ctx tc;
 *     int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
 *     if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
 *     if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
 *
 *     char mid[128] = {0}, act[32] = {0};
 *     if (u.len > 21) {                        // after "/api/breach-monitors/"
 *       char rest[256] = {0};
 *       size_t rl = u.len - 21; if (rl >= sizeof rest) rl = sizeof rest - 1;
 *       memcpy(rest, u.buf + 21, rl);
 *       char *sl = strchr(rest, '/');
 *       if (sl) { *sl = 0; snprintf(act, sizeof act, "%s", sl + 1); }
 *       mg_url_decode(rest, strlen(rest), mid, sizeof mid, 0);
 *     }
 *     char meth[12] = {0};
 *     size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
 *     memcpy(meth, hm->method.buf, ml);
 *     char *bdy = NULL;
 *     if (hm->body.len) { bdy = malloc(hm->body.len + 1);
 *       memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
 *     char qcu[512] = {0}, qli[16] = {0};
 *     mg_http_get_var(&hm->query, "cursor", qcu, sizeof qcu);
 *     int hl = mg_http_get_var(&hm->query, "limit", qli, sizeof qli);
 *     int status = 200;
 *     char *body = breach_monitors_api(g_db, &tc, meth, mid, act, bdy,
 *                                      qcu[0] ? qcu : NULL,
 *                                      hl > 0 ? atoi(qli) : 0, &status);
 *     free(bdy);
 *     if (!body && status == 204) { mg_http_reply(c, 204, "", ""); return; }
 *     if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
 *     reply_json(c, status, body); free(body); return;
 *   }
 *
 * No Makefile change (the core sources are globbed) and no main.c worker:
 * rescans are request-driven, not scheduled.
 *
 * ── ROUTES ─────────────────────────────────────────────────────────────────
 *   GET    /api/breach-monitors            list, keyset paged, no identifiers
 *   POST   /api/breach-monitors            create {kind,value,label?,rule_id?}
 *   GET    /api/breach-monitors/:id        detail + hit_count
 *   DELETE /api/breach-monitors/:id        remove (alert_events are KEPT)
 *   GET    /api/breach-monitors/:id/hits   matched breaches + catalog metadata
 *   POST   /api/breach-monitors/:id/rescan re-run this monitor over the corpus
 *
 * AUTH: read = any tenant member (tenant_resolve already proved membership);
 * write (POST/DELETE/rescan) = role analyst|admin|owner, else 403 forbidden.
 * AUDIT: breach_monitor.create / .delete / .rescan via audit_write(). The
 * payload carries kind, label, rule_id, the domain for domain monitors and a
 * 10-char hash PREFIX (the same non-reversible fragment breach_index_keyid()
 * already publishes) — never the identifier.
 *
 * ── ENVIRONMENT ────────────────────────────────────────────────────────────
 *   JO_BREACH_MONITOR_MAX_HITS   events emitted per monitor per scan pass,
 *                                default 500, clamped 1..100000. A domain
 *                                monitor on a widely-breached domain can match
 *                                six figures of rows; without a cap the first
 *                                scan would write an inbox nobody can read and
 *                                hand the delivery worker a firehose.
 */
#ifndef JO_BREACH_MONITOR_H
#define JO_BREACH_MONITOR_H
#include "db.h"
#include "tenantapi.h"

/* Apply this module's schema dependencies: ensure_column() for
 * breach_items.value_domain and the two indexes that make the hash and domain
 * joins index seeks. Idempotent, cheap after the first call (once-guarded), and
 * called automatically by every other function here — the explicit db.c call is
 * a nicety so the cost lands at boot instead of on the first request. */
void breach_monitor_migrate(db_handle *db);

/* INGEST HOOK. Diff the rows just written for `source_id` against every
 * breach_monitors row (all tenants) and write one alert_events row per new
 * (monitor, breach_item) pair. Backfills value_domain for that source first,
 * but only if a domain monitor exists.
 *
 * `source_id` NULL/"" widens the diff to the whole corpus (equivalent to a
 * rescan). `cancel` may be NULL; when non-NULL a non-zero value stops the pass
 * at the next monitor boundary.
 *
 * Cheap when idle: one COUNT over breach_monitors and nothing else if the
 * deploy has no monitors. Never throws, never leaves a transaction open, and
 * swallows every SQLite error — losing an alert is survivable, failing the
 * corpus ingest that produced it is not. Returns the number of alert_events
 * rows written (0 is normal), or <0 only on a bad handle. */
long long breach_monitor_scan_new(db_handle *db, const char *source_id,
                                  volatile int *cancel);

/* FULL RESCAN — the reason this module is not just the ingest hook. A monitor
 * created today would otherwise only ever see FUTURE breaches and would miss
 * the entire already-ingested 1,018-breach corpus, which is the point of the
 * feature. Runs the batched value_domain backfill, then matches every selected
 * monitor against the whole of breach_items.
 *
 * `tenant_id` NULL/"" = every tenant. `monitor_id` NULL/"" = every monitor of
 * that scope; set it to rescan exactly one. `cancel` may be NULL; checked
 * between backfill windows, between monitor batches and between monitors, so a
 * shutdown returns within one batch. Progress is durable: work already
 * committed is not undone, and re-running skips it via the existence check.
 * `*out_monitors` / `*out_hits` (either may be NULL) receive the number of
 * monitors examined and alert_events rows written. Returns 0, or <0 on a bad
 * handle. */
int breach_monitor_rescan_all(db_handle *db, const char *tenant_id,
                              const char *monitor_id, volatile int *cancel,
                              long long *out_monitors, long long *out_hits);

/* One entrypoint for the whole /api/breach-monitors subtree.
 *   `seg`    = monitor id, "" for the collection            (NULL treated as "")
 *   `act`    = "hits" | "rescan", "" for the monitor itself (NULL treated as "")
 *   `body`   = raw request body, may be NULL
 *   `cursor` = ?cursor= keyset token, may be NULL
 *   `limit`  = ?limit=, <=0 → 50, clamped 1..200
 * Sets *status and returns a malloc'd JSON body, or NULL. NULL + 204 is an
 * empty success (DELETE); NULL + anything else means the caller emits a generic
 * error of that status. Caller frees. */
char *breach_monitors_api(db_handle *db, const tenant_ctx *t,
                          const char *method, const char *seg, const char *act,
                          const char *body, const char *cursor, int limit,
                          int *status);

#endif
