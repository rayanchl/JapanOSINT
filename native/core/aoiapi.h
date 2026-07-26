/* core/aoiapi.h — the two CRUD surfaces that make alert_eval.c's new predicate
 * terms usable by a human: /api/aoi (roadmap 9, named geofences) and
 * /api/watchlists (roadmap 21, "who am I watching").
 *
 * Both are deliberately thin. Neither owns any matching logic — that lives in
 * core/alert_eval.c and is shared by the live ingest path and by
 * /api/alerts/preview. What lives here is naming, tenancy, authorization,
 * audit, and the one integrity rule each surface needs.
 *
 * ─── /api/aoi — areas of interest ──────────────────────────────────────────
 *
 * An AOI is a NAMED shape. The alternative was to inline every geofence in
 * predicate_json, and predicates already accept exactly that ("polygon": […]).
 * Naming exists for the two things inlining cannot do: back several rules with
 * ONE shape that is edited in one place, and hand the map layer something to
 * draw. A rule points at one with {"aoi_id":"…"}.
 *
 *   GET    /api/aoi              list, keyset-paginated (?limit=&cursor=)
 *   POST   /api/aoi              {name, kind, geometry}          analyst+
 *   GET    /api/aoi/:id          one
 *   PATCH  /api/aoi/:id          {name?, kind?, geometry?}       analyst+
 *   DELETE /api/aoi/:id                                          analyst+
 *
 * `kind` is "bbox" | "polygon" | "circle" and `geometry` is the matching JSON
 * value — [w,s,e,n], [[lon,lat],…], or {"lat","lon","radius_m"} — i.e. exactly
 * the value the equivalent inline predicate term takes. One vocabulary, so an
 * analyst who has written a predicate already knows this body, and so the
 * SAME validator can judge both: geometry goes through
 * alert_eval_shape_check(), the single function in the codebase that decides
 * whether a shape is well-formed. A shape that stores is exactly a shape that
 * evaluates. Two validators would eventually disagree and the way you would
 * find out is a geofence that quietly stopped firing.
 *
 * Rejected hard at write time (all from that one validator): >10,000 vertices,
 * non-finite coordinates, latitude outside [-90,90], longitude outside
 * [-180,180], a south edge above its north edge, a ring under 3 points, and a
 * ring of zero shoelace area (a line dressed as an area). Rings are auto-
 * closed on the way in, so the stored geometry and the evaluated geometry are
 * the same bytes.
 *
 * bbox_w/s/e/n are stored alongside geometry_json, recomputed on every write.
 * They exist for map/bounds queries and for a future spatial index. The
 * evaluator pointedly does NOT read them — it recompiles the box from
 * geometry_json — because a prefilter derived from different bytes than the
 * exact test can silently drop true matches.
 *
 * DELETE refuses with 409 {"error":"aoi_in_use","rules":[{id,name},…]} while
 * any alert_rules row in the tenant still references the id. Deleting anyway
 * would be SAFE (an unresolvable aoi_id fails closed, so those rules would
 * simply stop matching) and that is precisely the problem: a geofence that
 * silently stops firing is the failure this whole item exists to prevent.
 * Unlink the rules first and the refusal goes away.
 *
 * ─── /api/watchlists — entity watchlists ───────────────────────────────────
 *
 *   GET    /api/watchlists       list, keyset-paginated (?limit=&cursor=)
 *   POST   /api/watchlists       {name, entity_ids:[…]}          analyst+
 *   GET    /api/watchlists/:id   one
 *   PATCH  /api/watchlists/:id   {name?, entity_ids?, enabled?}  analyst+
 *   DELETE /api/watchlists/:id                                   analyst+
 *
 * A watchlist IS an alert_rules row. Creating one writes the rule (predicate
 * {"entity_ids":[…]}, channels '[]') and a `watchlists` row that points at it;
 * every mutation keeps the pair in step and DELETE removes both plus the
 * rule's alert_events. That is the whole design: ONE engine, ONE delivery
 * path, ONE inbox. A parallel "watchlist matcher" would have had to re-derive
 * mute, dedup, storm caps and delivery, and would have drifted.
 *
 * The sugar table therefore stores no semantics of its own — it stores the
 * NAME and the id list so the UI can render "who I'm watching" without ever
 * showing predicate JSON, and rule_id so the two can never disagree about
 * which rule is which. Read the enabled flag from the rule, not from here.
 *
 * Two deliberate constraints:
 *
 *  - entity_ids must be NON-EMPTY. An empty list is not "watch nobody", it is
 *    a predicate with no entity term at all — which matches every item in the
 *    tenant. A watchlist is one API call away from being a firehose and this
 *    is the call.
 *  - every id must resolve in `entities` under this tenant (tenant_id IS NULL,
 *    the shared/global rows the NER pod writes, or tenant_id = ours). This
 *    stops a watchlist that can never fire because of a typo, and it stops a
 *    tenant naming another tenant's private entity id to learn from the
 *    resulting alerts whether it appears in their corpus.
 *
 * Channels are NOT accepted here and are always stored '[]'. Delivery to the
 * notification inbox (alert_events → /api/alert-events) is unconditional and
 * needs no configuration; email/webhook needs the real channel validation that
 * lives in alertsapi.c's validate_rule(), so the flow is: create the watchlist
 * here, then PATCH its rule_id through /api/alerts/:id if you want it pushed.
 * Duplicating that validator to accept channels here would have created a
 * second, weaker gate in front of alert_deliver.c's outbound HTTP.
 *
 * WHEN DOES A WATCHLIST ACTUALLY FIRE? Not at ingest — entity extraction runs
 * asynchronously, long after the item commits, so a fresh item has no mentions
 * to match. alert_eval.c's reconciling sweep (alert_eval_sweep_start, wired in
 * main.c) is what closes that gap. The full rationale, and why a trigger inside
 * the extractor was rejected, is in core/alert_eval.h. If watchlists are not
 * firing, that thread is the first thing to check.
 *
 * ─── DDL REQUIRED ──────────────────────────────────────────────────────────
 *
 * All of it is listed in core/alert_eval.h's "DDL REQUIRED" block, together in
 * one place because the evaluator and these two surfaces share it:
 * areas_of_interest + idx_aoi_tenant, watchlists + idx_watchlists_tenant,
 * alert_eval_cursor, and idx_em_created on entity_mentions. All are new
 * CREATE ... IF NOT EXISTS objects; NO ALTER TABLE and no ensure_column() is
 * required by this module.
 *
 * ─── WIRING ────────────────────────────────────────────────────────────────
 *
 * Two route blocks in core/httpd.c, both copies of the existing
 * /api/annotations block (same tenant_ctx, same raw query string, same body,
 * same 204 handling) with only the prefix length and dispatcher changed. The
 * exact code is spelled out in core/alert_eval.h under "WIRING THE
 * ORCHESTRATOR MUST DO", step 4. Neither prefix collides with an existing
 * route, so ordering within the dispatcher does not matter.
 */
#ifndef JO_AOIAPI_H
#define JO_AOIAPI_H
#include "db.h"
#include "tenantapi.h"

/* /api/aoi[/:id] dispatcher. `seg` is the single path segment after
 * "/api/aoi/" ("" for the collection); `qs` is the raw query string; `body`
 * may be NULL. Returns a malloc'd envelope (caller frees) and sets *status;
 * returns NULL only with *status==204 (successful DELETE), which the caller
 * must reply as an empty 204. Every statement carries tenant_id=?. */
char *aoiapi(db_handle *db, const tenant_ctx *t, const char *method,
             const char *seg, const char *qs, const char *body, int *status);

/* /api/watchlists[/:id] dispatcher. Same contract as aoiapi(). */
char *watchlistsapi(db_handle *db, const tenant_ctx *t, const char *method,
                    const char *seg, const char *qs, const char *body,
                    int *status);

/* 1 if `aoi_id` names a shape owned by `tenant_id`. Offered for a soft
 * "this rule points at an AOI that no longer exists" warning at rule-save
 * time; alertsapi.c's validate_rule() must NOT turn this into a hard error
 * (see the note in core/alert_eval.h step 3 — a deleted AOI would otherwise
 * make its rules un-editable). Never allocates. */
int aoi_exists(db_handle *db, const char *tenant_id, const char *aoi_id);

#endif
