/* core/alert_eval.h — the alert MATCHER: what finally makes alert_rules fire.
 *
 * alertsapi.c is CRUD-only: it stores predicates and reads alert_events back,
 * but nothing in the process ever wrote an alert_events row. This module is
 * that missing half — it evaluates a rule's predicate_json against a freshly
 * ingested intel_item and records the match, with the suppression rules the
 * CRUD surface already promises (muted_until / dedup_window_sec /
 * storm_cap_per_hour) actually enforced.
 *
 * Design decisions worth knowing before you touch this file:
 *
 *  - ONE evaluator, two drivers. The live path and the preview/backtest path
 *    share pred_compile() + facts_match(); the only asymmetry is the `q` term.
 *    Live probes FTS per-uid (the item is known, the match set is not);
 *    preview lets FTS drive the scan (the match set is known, the items are
 *    not). Both use the *same* segmented MATCH string produced by the same
 *    compiler against the same table, so the answers agree. Two divergent
 *    matchers is the failure mode this shape exists to prevent. Everything
 *    added since — geofences (item 9) and entity terms (item 21) — went into
 *    that same shared pair, so preview covers them too.
 *
 *  - The ingest path is hot (hundreds of collectors, multi-threaded), so it
 *    must not SELECT rules per row. Enabled rules are cached in memory with
 *    their predicates PRE-COMPILED (including the MeCab segmentation of `q`,
 *    the vertex ring of a polygon, and the resolution of a named aoi_id, all
 *    of which are far too expensive to redo per row), refreshed at most every
 *    few seconds and only when a generation string over alert_rules — and now
 *    areas_of_interest — actually changed. The snapshot is immutable +
 *    refcounted, so evaluation never holds the mutex across SQL. When a tenant
 *    has no enabled rules, an ingested row costs zero queries.
 *
 *  - Suppression parameters are re-read from alert_rules at FIRE time, not
 *    taken from the cache. Firing is the rare path, so the extra lookup is
 *    free, and it means a mute takes effect immediately rather than at the
 *    next cache refresh.
 *
 *  - Malformed constraints fail CLOSED (the rule never matches) instead of
 *    being ignored. Ignoring a constraint you failed to parse silently WIDENS
 *    the rule, which is how an alert rule turns into a firehose.
 *
 *  - mode=="llm" + nl_query rules are SKIPPED, live and in preview alike.
 *    Synchronous LLM inference on every ingested row is not viable: it would
 *    put a multi-hundred-millisecond network call inside the collector loop,
 *    multiplied by rules × items, and make ingest throughput a function of an
 *    external API's latency. Those rules need an out-of-band evaluator
 *    (batch/queue) — deliberately out of scope here. Preview reports them as
 *    {"meta":{"skipped":"llm_mode_unsupported"}} rather than silently
 *    evaluating a *different* (nl_query-less) predicate, so the two paths
 *    keep telling the same story.
 *
 *  - No audit_events rows are written. audit_write() is for tenant-initiated
 *    mutations; alert_events are machine-generated at ingest and one audit
 *    row per matched item would bury the audit trail in noise. alert_events
 *    IS the record of what happened here. (aoiapi.c, the CRUD surface over
 *    areas_of_interest and watchlists, DOES audit — those are user edits.)
 *
 *  - delivered_channels_json is always written '[]'. Delivery (email/webhook)
 *    is the alertEngine dispatch subsystem; the event row is the durable
 *    record alert_deliver.c's worker fills in later.
 *
 * ─── PREDICATE GRAMMAR ──────────────────────────────────────────────────────
 *
 *   {
 *     "mode":         "fts" | "llm",
 *     "nl_query":     "…",                      // llm mode only; skipped here
 *     "q":            "…",                      // MeCab-segmented FTS MATCH
 *     "source_ids":   ["…"],                    // any-of, case-insensitive
 *     "tags_any":     ["…"],  "tags_all": ["…"],
 *
 *     // ── item 9: spatial. AT MOST ONE of these four may be present; two
 *     //    spatial terms in one predicate is an authoring mistake, not an
 *     //    intersection, and is rejected (fails closed).
 *     "bbox":     [w, s, e, n],
 *     "polygon":  [[lon,lat], …],               // ring; auto-closed
 *     "circle":   {"lat":…, "lon":…, "radius_m":…},
 *     "aoi_id":   "<areas_of_interest.id>",     // resolved TENANT-SCOPED
 *
 *     // ── item 21: entity watchlists. Both may be present (AND).
 *     "entity_ids":   ["…"],                    // any-of over entity_mentions
 *     "entity_types": ["person","org",…]        // any-of over entities.type
 *   }
 *
 * All present terms AND together. An absent term, or an empty array, is "no
 * constraint" — that is how the rule form serialises an untouched field.
 *
 * Spatial evaluation is deliberately two-stage so the ingest hot path stays
 * cheap: every shape's bounding box is computed ONCE at predicate-compile
 * time (cached with the rule), the per-item test rejects on that box first,
 * and only a point that survives it pays for ray-casting or haversine.
 *
 *   - `bbox` keeps its ORIGINAL parser and its original semantics verbatim,
 *     including w>e meaning an antimeridian-crossing box ([170,-50,-170,50]).
 *     It is the one term with deployed rules behind it, so it was not
 *     "improved" while being moved: same acceptance set, same test, same
 *     answers. The stricter range/finiteness validation below applies to the
 *     THREE NEW forms and to anything stored in areas_of_interest.
 *   - `polygon` is evaluated by even-odd ray casting in plain lon/lat space.
 *     A ring is auto-closed if its last point differs from its first. Rings
 *     are rejected if they have <3 points, >10,000 vertices, a non-finite or
 *     out-of-range coordinate, or zero shoelace area (a line, not an area).
 *     KNOWN LIMIT, deliberate: a ring that crosses the antimeridian is not
 *     special-cased — its bounding box would span the globe and the crossing
 *     edge would be interpolated the long way round. Author such an area as
 *     two polygons (or two AOIs, or a circle). Detecting the intent
 *     automatically guesses, and a geofence that guesses is worse than one
 *     that requires you to be explicit.
 *   - `circle` is a true geodesic cap: haversine distance on a sphere of
 *     radius 6371008.8 m (IUGG mean). Its prefilter box is widened using the
 *     cosine of the cap's pole-most latitude, not of its centre, so the box
 *     can never be narrower than the cap it guards; a cap containing a pole
 *     degrades to the full longitude range.
 *   - `aoi_id` is resolved with `WHERE id=? AND tenant_id=?`. A rule can only
 *     ever geofence on its OWN tenant's shape; an unknown or foreign id fails
 *     closed, which is why a probing rule learns nothing (it simply never
 *     matches) rather than quietly widening to "no spatial constraint".
 *     The shape is recompiled from geometry_json rather than trusting the
 *     stored bbox_* columns: a prefilter derived from different bytes than
 *     the exact test can silently drop true matches. Those columns exist for
 *     map-layer/index queries, not for this.
 *
 * ─── ITEM 21: THE ENTITY-MENTION ORDERING PROBLEM, AND HOW IT IS SOLVED ─────
 *
 * A watchlist is an ordinary alert_rules row (predicate {"entity_ids":[…]}),
 * so there is ONE engine, ONE delivery path and ONE inbox — see aoiapi.h for
 * the `watchlists` sugar table that manages them without exposing predicate
 * JSON. That choice creates one real problem:
 *
 *   alert_eval_on_item() runs from intel.c's emit() immediately after COMMIT.
 *   Entity extraction (entity_enrich.c → es_add_mention(), and pipeline.c's
 *   OSINT-search ingest) runs LATER and asynchronously. At alert time a fresh
 *   item therefore has NO entity_mentions rows at all. Evaluating an entity
 *   term there and stopping would mean a watchlist that NEVER fires.
 *
 * SOLVED WITH (b), A RECONCILING SWEEP over entity_mentions.created_at:
 * alert_eval_sweep_entities() walks mentions written since a persisted
 * watermark, and re-evaluates each newly-mentioned item against the rules
 * that actually have an entity term. The full predicate is re-run — a
 * watchlist ANDed with a geofence or a tag still has to satisfy all of it —
 * and fire() applies the same mute/dedup/storm-cap suppression as the live
 * path. Nothing about the matcher is duplicated: the sweep calls the same
 * eval_item() the ingest path calls, with one flag that restricts it to
 * entity rules.
 *
 * Why (b) and not (a), a second trigger fired by the extractor:
 *
 *   1. There is more than one producer. entity_enrich.c is the NER pod, but
 *      pipeline.c writes mentions through the same es_add_mention() for OSINT
 *      search results. (a) means a call site in each, and the bug is silent
 *      when the next producer forgets one: watchlists just stop firing for
 *      that source, with nothing in any log to say so. The sweep is anchored
 *      on the TABLE, so it cannot be bypassed by a new writer.
 *   2. es_add_mention() is called inside entity_enrich's per-item BEGIN/COMMIT
 *      and once per extracted entity. A trigger there either fires N times per
 *      item, or fires INSIDE a transaction — and this module's founding rule
 *      is that an alert write must never be able to roll back, or be rolled
 *      back by, the ingest that produced it.
 *   3. (a) covers only mentions written from now on. A mention that lands
 *      while the process is restarting, or during a window where the alert
 *      code failed, is lost forever. The sweep is watermark-driven, so it
 *      resumes from where it stopped — including across restarts.
 *   4. (a) would have required editing entity_enrich.c and pipeline.c, files
 *      this module does not own; a correctness property that depends on two
 *      foreign call sites staying correct is not a property, it is a hope.
 *
 * What the sweep deliberately does NOT do is backfill. On its first run in a
 * database (and whenever no enabled rule has an entity term) the watermark is
 * advanced to "now" without evaluating anything, so creating a watchlist
 * starts watching from that moment — exactly like every other alert rule. Ask
 * "what would this have matched?" with POST /api/alerts/preview, which runs
 * the same matcher over history and writes nothing.
 *
 * Entity terms are ALSO evaluated on the live path, not only in the sweep.
 * They cost one indexed lookup on idx_em_item, resolved lazily and at most
 * once per item however many rules ask, and only when some rule has an entity
 * term at all. Keeping them live is what makes an already-extracted item that
 * is re-ingested behave correctly, and it keeps the "one matcher" property
 * literal rather than aspirational.
 *
 * ─── DDL REQUIRED (orchestrator: append to core/schema.sql) ──────────────────
 *
 *   -- Item 9. Named shapes, so one geofence can back several rules and later
 *   -- be drawn as a map layer. bbox_* are precomputed for map/index queries;
 *   -- the evaluator recompiles from geometry_json (see above).
 *   CREATE TABLE IF NOT EXISTS areas_of_interest (
 *     id            TEXT PRIMARY KEY,
 *     tenant_id     TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
 *     name          TEXT NOT NULL,
 *     kind          TEXT NOT NULL CHECK(kind IN ('bbox','polygon','circle')),
 *     geometry_json TEXT NOT NULL,
 *     bbox_w REAL, bbox_s REAL, bbox_e REAL, bbox_n REAL,
 *     created_by    TEXT,
 *     created_at    TEXT NOT NULL DEFAULT (datetime('now')),
 *     updated_at    TEXT NOT NULL DEFAULT (datetime('now'))
 *   );
 *   CREATE INDEX IF NOT EXISTS idx_aoi_tenant
 *     ON areas_of_interest(tenant_id, created_at DESC);
 *
 *   -- Item 21 sugar table (see aoiapi.h).
 *   CREATE TABLE IF NOT EXISTS watchlists (
 *     id              TEXT PRIMARY KEY,
 *     tenant_id       TEXT NOT NULL,
 *     name            TEXT NOT NULL,
 *     entity_ids_json TEXT NOT NULL DEFAULT '[]',
 *     rule_id         TEXT,
 *     created_by      TEXT,
 *     created_at      TEXT NOT NULL DEFAULT (datetime('now')),
 *     updated_at      TEXT NOT NULL DEFAULT (datetime('now'))
 *   );
 *   CREATE INDEX IF NOT EXISTS idx_watchlists_tenant
 *     ON watchlists(tenant_id, created_at DESC);
 *
 *   -- Item 21: the sweep's resumable watermark. Process-wide, NOT per tenant
 *   -- — entity_mentions has no tenant column, and tenant isolation is
 *   -- enforced where it belongs: eval_item() selects rules by the ITEM row's
 *   -- own tenant_id, so a swept item can only ever reach its own tenant's
 *   -- rules. A per-tenant watermark would add a scan per tenant and buy
 *   -- nothing.
 *   CREATE TABLE IF NOT EXISTS alert_eval_cursor (
 *     k          TEXT PRIMARY KEY,
 *     v          TEXT NOT NULL,
 *     updated_at TEXT NOT NULL DEFAULT (datetime('now'))
 *   );
 *
 *   -- Item 21: the sweep scans entity_mentions by WRITE time. The existing
 *   -- idx_em_entity is (entity_id, created_at DESC) and cannot serve a bare
 *   -- created_at range. SQLite orders index entries by (key…, rowid), so this
 *   -- one also satisfies the sweep's ORDER BY created_at, rowid keyset with
 *   -- no temp B-tree.
 *   CREATE INDEX IF NOT EXISTS idx_em_created ON entity_mentions(created_at);
 *
 * NEW COLUMNS: none on any pre-existing table, so nothing here needs
 * ensure_column(). The two `updated_at` columns above are inside brand-new
 * CREATE TABLE statements. If a build of this branch ever shipped these
 * tables WITHOUT updated_at, add the idempotent pair to db.c's boot-migration
 * block (a CREATE TABLE IF NOT EXISTS silently no-ops on an existing table,
 * so the column would otherwise never arrive):
 *
 *     ensure_column(db, "areas_of_interest", "updated_at", "TEXT");
 *     ensure_column(db, "watchlists",        "updated_at", "TEXT");
 *
 * Objects relied on and already present: alert_rules, alert_events,
 * idx_alert_events_rule_item (dedup), idx_alert_events_rule_ts (storm cap),
 * idx_intel_items_tenant_fetched (preview window), intel_items_fts +
 * intel_items_fts_uid_map (q), entity_mentions + idx_em_item, entities.
 *
 * ─── WIRING THE ORCHESTRATOR MUST DO ────────────────────────────────────────
 *
 * 1) core/intel.c — UNCHANGED from the original port. Add the include, and ONE
 *    call in emit(), immediately after the transaction commits (never inside
 *    it: an alert write must not be able to roll back the ingest that produced
 *    it):
 *
 *      #include "alert_eval.h"                      // with the other includes
 *      ...
 *        int changes = sqlite3_changes(h);
 *        fts_write(h, uid, it->title, it->body, it->summary);
 *        sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
 *        if (changes)                                        // <-- ADD THIS
 *          alert_eval_on_item(st->db,                        // <-- LINE
 *                             st->tenant_id[0] ? st->tenant_id : "legacy",
 *                             uid);
 *        return changes ? 1 : 0;
 *
 *    CAVEAT the orchestrator should know: emit() cannot currently tell a NEW
 *    row from an UPDATE — after an "INSERT … ON CONFLICT(uid) DO UPDATE",
 *    sqlite3_changes() is 1 either way, which is why emit() effectively always
 *    returns 1. So the call above also fires on re-ingested rows. That is safe
 *    by construction: dedup_window_sec (default 3600s) collapses the repeats,
 *    and re-evaluating is cheap. If you want true new-only semantics, the
 *    minimal correct change is a pre-check before the upsert
 *    ("SELECT 1 FROM intel_items WHERE uid=?1" → is_new = !found) and gating
 *    the call on is_new; only then does a rule with dedup_window_sec=0 stop
 *    re-firing on every refresh of an unchanged item.
 *
 * 2) core/main.c — ONE new line beside the existing alert_eval_init(&db), in
 *    the serve path, next to alert_deliver_start(&db):
 *
 *      alert_eval_init(&db);
 *      alert_eval_sweep_start(&db);      // <-- ADD (item 21). Without it,
 *                                        //     entity watchlists never fire.
 *      scheduler_start_background(&db);
 *      ...
 *      alert_deliver_start(&db);
 *
 *    If main.c has a shutdown path that calls alert_deliver_stop(), pair it
 *    with alert_eval_sweep_stop(). Both are optional-but-recommended in the
 *    same sense as alert_deliver's: the thread is detachable and the process
 *    exits fine without a join.
 *
 * 3) core/alertsapi.c — extend validate_rule() so the new predicate terms are
 *    rejected at authoring time instead of silently failing closed at
 *    evaluation time. Inside the existing `if (pred) { cJSON *x; … }` block,
 *    REPLACE the current bbox check:
 *
 *        if ((x=cJSON_GetObjectItem(pred,"bbox")) &&
 *            (!cJSON_IsArray(x) || cJSON_GetArraySize(x) != 4))
 *          return "predicate.bbox must be [w,s,e,n]";
 *
 *    with this (add `#include "alert_eval.h"` at the top of alertsapi.c):
 *
 *        // ── item 9: at most one spatial term ──────────────────────────
 *        cJSON *sb = cJSON_GetObjectItem(pred,"bbox");
 *        cJSON *sp = cJSON_GetObjectItem(pred,"polygon");
 *        cJSON *sc = cJSON_GetObjectItem(pred,"circle");
 *        cJSON *sa = cJSON_GetObjectItem(pred,"aoi_id");
 *        if (sb && cJSON_IsNull(sb)) sb = NULL;
 *        if (sp && cJSON_IsNull(sp)) sp = NULL;
 *        if (sc && cJSON_IsNull(sc)) sc = NULL;
 *        if (sa && cJSON_IsNull(sa)) sa = NULL;
 *        if ((!!sb + !!sp + !!sc + !!sa) > 1)
 *          return "predicate may carry only one of bbox/polygon/circle/aoi_id";
 *        if (sb && (!cJSON_IsArray(sb) || cJSON_GetArraySize(sb) != 4))
 *          return "predicate.bbox must be [w,s,e,n]";
 *        if (sp) {
 *          char *gj = cJSON_PrintUnformatted(sp);
 *          const char *why = alert_eval_shape_check("polygon", gj, 0,0,0,0);
 *          free(gj);
 *          if (why) return why;          // static string, safe to return
 *        }
 *        if (sc) {
 *          char *gj = cJSON_PrintUnformatted(sc);
 *          const char *why = alert_eval_shape_check("circle", gj, 0,0,0,0);
 *          free(gj);
 *          if (why) return why;
 *        }
 *        if (sa && (!cJSON_IsString(sa) || !sa->valuestring[0]))
 *          return "predicate.aoi_id must be a non-empty string";
 *        // ── item 21: entity terms ─────────────────────────────────────
 *        if ((x=cJSON_GetObjectItem(pred,"entity_ids")) && !cJSON_IsNull(x) &&
 *            !cJSON_IsArray(x))
 *          return "predicate.entity_ids must be array";
 *        if ((x=cJSON_GetObjectItem(pred,"entity_types")) && !cJSON_IsNull(x) &&
 *            !cJSON_IsArray(x))
 *          return "predicate.entity_types must be array";
 *
 *    NOTE for whoever wires this: validate_rule() must NOT verify that
 *    predicate.aoi_id exists — it has no db_handle, and more importantly a
 *    404-on-save would make a rule un-editable the moment its AOI is deleted.
 *    Existence is a *matching* question, and the evaluator already answers it
 *    the safe way: an unresolvable aoi_id fails closed. aoiapi.c refuses to
 *    delete an AOI that a rule still references (409), which is where that
 *    integrity actually belongs. If you want a soft warning at save time,
 *    aoiapi.h exports aoi_exists() for exactly that.
 *
 * 4) core/httpd.c — two new route blocks. Copy the /api/annotations block
 *    verbatim (it already passes tenant_ctx, the raw query string and the
 *    body, and handles the 204 case); only the prefix length, the segment
 *    buffer and the dispatcher name change:
 *
 *      #include "aoiapi.h"                          // with the other includes
 *
 *      // ---- Roadmap 9: /api/aoi — named geofences. ----
 *      if (eq(u, "/api/aoi") || starts(u, "/api/aoi/")) {
 *        …resolve tenant exactly as the annotations block does…
 *        char aid[160] = {0};
 *        if (u.len > 9) {                            // after "/api/aoi/"
 *          char rest[256] = {0};
 *          size_t rl = u.len - 9; if (rl >= sizeof rest) rl = sizeof rest - 1;
 *          memcpy(rest, u.buf + 9, rl);
 *          char *sl = strchr(rest, '/'); if (sl) *sl = 0;
 *          mg_url_decode(rest, strlen(rest), aid, sizeof aid, 0);
 *        }
 *        …meth / bdy / qsb exactly as annotations…
 *        int status = 200;
 *        char *body = aoiapi(g_db, &tc, meth, aid, qsb, bdy, &status);
 *        free(bdy);
 *        if (!body && status == 204) { mg_http_reply(c, 204, "", ""); return; }
 *        if (!body) { reply_json(c, 500, "{\"error\":\"server_error\"}"); return; }
 *        reply_json(c, status, body); free(body); return;
 *      }
 *
 *      // ---- Roadmap 21: /api/watchlists — entity watchlists. ----
 *      if (eq(u, "/api/watchlists") || starts(u, "/api/watchlists/")) {
 *        …identical, with prefix length 16 ("/api/watchlists/") and…
 *        char *body = watchlistsapi(g_db, &tc, meth, aid, qsb, bdy, &status);
 *      }
 *
 *    Neither block needs to precede any existing route: no current prefix
 *    collides with /api/aoi or /api/watchlists.
 *
 * 5) Nothing to wire for preview — POST /api/alerts/preview already calls
 *    alert_eval_preview(), and the new terms went into the shared matcher, so
 *    it backtests geofences and watchlists with no route change.
 */
#ifndef JO_ALERT_EVAL_H
#define JO_ALERT_EVAL_H
#include "db.h"

/* Optional pre-warm of the rule cache. Safe to call more than once, safe to
 * never call. Never fails. */
void alert_eval_init(db_handle *db);

/* Evaluate every enabled rule of `tenant_id` against the just-upserted item
 * and write alert_events rows for the matches. Call AFTER the ingest
 * transaction commits (see wiring note above).
 *
 * Cheap by design: no query at all when the tenant has no enabled rules, one
 * indexed row read otherwise, and DB work per rule only once the in-memory
 * facts already matched. Never aborts, never longjmps, never leaves a
 * transaction open, and swallows every SQLite error — losing an alert is
 * survivable, losing the ingested row is not.
 *
 * `tenant_id` is a hint used to skip work early; the item row's own tenant_id
 * is authoritative for rule selection, so a mislabelled call can never leak
 * one tenant's items into another tenant's alerts. */
void alert_eval_on_item(db_handle *db, const char *tenant_id,
                        const char *item_uid);

/* ── item 21: the entity-mention reconciling sweep ───────────────────────── */

/* Run ONE sweep pass and return the number of items re-evaluated. Walks
 * entity_mentions written since the persisted watermark (alert_eval_cursor)
 * and re-evaluates each newly-mentioned item against the enabled rules that
 * carry an entity term, via the same eval path the live ingest uses.
 *
 * Idempotent, restart-safe, and bounded: at most ~10k mentions per call, and
 * the watermark only advances over ranges it actually drained. Swallows every
 * SQLite error. Safe to call from any thread; safe to call when no watchlist
 * exists (it just advances the watermark). Exposed separately from the worker
 * so a CLI/test can drive one deterministic pass. */
int  alert_eval_sweep_entities(db_handle *db);

/* Start/stop the background sweep worker (default every 30s, override with
 * JO_ALERT_SWEEP_SEC; disable entirely with JO_NO_ALERT_SWEEP). Mirrors
 * alert_deliver_start/stop. WITHOUT THIS THREAD RUNNING, ENTITY WATCHLISTS
 * WILL NOT FIRE — see the ordering discussion above. Starting twice is a
 * no-op; stopping joins. */
void alert_eval_sweep_start(db_handle *db);
void alert_eval_sweep_stop(void);

/* ── item 9: the ONE geometry validator ─────────────────────────────────── */

/* Parse + validate one shape. `kind` is "bbox" | "polygon" | "circle";
 * `geometry_json` is the matching JSON text ([w,s,e,n] / [[lon,lat],…] /
 * {"lat","lon","radius_m"}). Returns NULL when the shape is well-formed and
 * fills whichever of w/s/e/n are non-NULL with its bounding box (w>e encodes
 * an antimeridian-crossing box); otherwise returns a stable, human-readable
 * reason with static storage duration — safe to return straight out of
 * validate_rule() or to put in an {"error":…} envelope.
 *
 * This is the single place geometry is judged. aoiapi.c calls it to validate
 * a write and to precompute bbox_*; the predicate compiler runs the same
 * parser on the same bytes. A shape that stores is exactly a shape that
 * evaluates — two validators would eventually disagree, and the way you would
 * find out is a geofence that stopped firing. */
const char *alert_eval_shape_check(const char *kind, const char *geometry_json,
                                   double *w, double *s, double *e, double *n);

/* ── preview / backtest (roadmap item 10) ───────────────────────────────── */

/* Run `predicate_json` over historical intel_items WITHOUT writing
 * alert_events, using the same evaluator as the live path — including
 * geofences and entity terms.
 *
 * `since` is an ISO-8601 (or any SQLite-parseable) timestamp; NULL/"" means
 * 7 days back. The window is clamped to at most 30 days and is measured over
 * fetched_at (INGEST time, not publication time) because that is when the live
 * evaluator would have run — a backtest over published_at would answer a
 * different question. The candidate scan is capped so a huge corpus cannot
 * stall the request; meta.truncated says whether the cap was hit, in which
 * case match_count is a lower bound.
 *
 * COST NOTE: a predicate with an entity term costs one indexed lookup per
 * candidate row that survives the cheaper terms, up to the scan cap. Put the
 * narrowing terms (q, source_ids, tags) in the predicate alongside it and the
 * scan is bounded by FTS instead.
 *
 * Returns a malloc'd envelope (caller frees):
 *   {"data":[ up to 50 sample matches, newest first ],
 *    "page":{"limit":50,"has_more":bool},
 *    "meta":{"match_count":N,"scanned":S,"truncated":bool,"since":"…",
 *            "max_window_days":30,"writes_events":false,"skipped":null|"…"}}
 * On error returns {"error":"…"} with *status set: 400 tenant_required /
 * invalid_predicate / invalid_since, 500 server_error. Never returns NULL. */
char *alert_eval_preview(db_handle *db, const char *tenant_id,
                         const char *predicate_json, const char *since,
                         int *status);

#endif
