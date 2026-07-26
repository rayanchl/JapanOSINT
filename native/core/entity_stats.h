/* core/entity_stats.h — correlation scoring for the entity co-mention graph
 * (roadmap 22). Turns entity_relationships.weight, which is a raw accumulated
 * co-mention count, into a statement about *significance*.
 *
 * WHY. weight is monotone in corpus volume, so it ranks hubs. A prefecture
 * co-occurs with everything, so "Tokyo — <anything>" always outranks the one
 * edge an analyst actually wants. Raw count answers "how often", which for a
 * hub is a property of the hub, not of the pair. Pointwise mutual information
 * answers "how much more often than chance", which is a property of the pair:
 *
 *   N      = distinct item_uids carrying at least one mention
 *   P(a)   = distinct item_uids mentioning a            / N
 *   P(a,b) = distinct item_uids mentioning both a and b / N
 *   lift   = P(a,b) / (P(a)P(b))            (1.0 == exactly chance)
 *   PMI    = log(lift)                      (0.0 == exactly chance)
 *
 * Both are computed as the algebraically identical (co*N)/(count_a*count_b),
 * which keeps one division instead of three and avoids underflowing P(a)P(b)
 * on a large corpus. weight is left untouched — it is the ingest surface's
 * output (entitystore.h) and other readers rely on it; these columns are an
 * annotation alongside it, never a replacement.
 *
 * DELIBERATE LIMITS
 *  - Support floor. PMI is pathologically unstable in the tail: a pair seen
 *    once in one document out of a 200k-document corpus scores PMI ~ log(200k)
 *    and would sit at the top of every ranking, above every real association.
 *    That is not a weak signal, it is an artefact of the estimator. Pairs below
 *    ENTITY_STATS_MIN_CO_COUNT get NULL pmi and NULL lift — an explicit "not
 *    enough evidence", which a reader can order last, rather than a number that
 *    looks strong and is noise. co_count is still written for those rows: the
 *    count is an observed fact, only the derived statistic is withheld.
 *  - rel_type='asserted' is skipped entirely and never stamped. A human saying
 *    "these two are linked" is not a claim about co-occurrence frequency;
 *    scoring it would let a low lift demote an assertion a person made, which
 *    inverts the authority order. Those edges keep pmi/lift/co_count/stats_at
 *    NULL forever, and that NULL is the signal "not a statistical claim".
 *  - Self-edges (src==dst) are stamped but left unscored. entitystore.h's
 *    es_add_relationship refuses to create them; the guard is for rows a
 *    tier-2 union (es_union_entities repointing both ends onto the survivor)
 *    can leave behind, whose lift would be N/count_a — a large meaningless
 *    number.
 *
 * REQUIRED DDL — four columns on entity_relationships:
 *
 *   ALTER TABLE entity_relationships ADD COLUMN pmi      REAL;
 *   ALTER TABLE entity_relationships ADD COLUMN lift     REAL;
 *   ALTER TABLE entity_relationships ADD COLUMN co_count INTEGER;
 *   ALTER TABLE entity_relationships ADD COLUMN stats_at TEXT;
 *
 * DO NOT paste those four lines into schema.sql. core/db.c db_open() slurps
 * schema.sql and hands the whole file to ONE sqlite3_exec() on EVERY boot, and
 * ADD COLUMN is not idempotent: on the second boot the first ALTER fails with
 * "duplicate column name: pmi", sqlite3_exec abandons the rest of the script,
 * db_open returns 1 and the process refuses to start. It does not degrade, it
 * bricks boot. Apply it the way this codebase already applies added columns —
 * two halves, both wanted:
 *
 *  (a) EXISTING databases: a guarded boot migration, exactly the shape db.c
 *      already uses for sources.schedule_mode — PRAGMA table_info(<table>),
 *      ALTER only when the column is absent. db.c's own comment states the
 *      rule: "schema.sql is generated from the live DB so new runtime-only
 *      columns are added here, not in the generated schema."
 *      entity_stats_ensure_columns() below IS that migration, written and
 *      idempotent. Call it; nothing else is needed.
 *
 *  (b) FRESH databases: schema.sql is regenerated from the live japanmap.db,
 *      so once (a) has run the columns come back *inline*, appended to the
 *      CREATE TABLE body — the `, triage_class TEXT, ...` shape already on
 *      collector_anomalies and collector_fixes. That inline form is the OUTPUT
 *      of regeneration, never a hand-written bare ALTER. To get fresh DBs
 *      correct before the next regeneration, hand-edit the
 *      CREATE TABLE IF NOT EXISTS entity_relationships body to end
 *        , pmi REAL, lift REAL, co_count INTEGER, stats_at TEXT);
 *      That is safe to combine with (a): CREATE TABLE IF NOT EXISTS is a no-op
 *      against an existing DB, so it can never collide with the migration.
 *
 * No new index is required. The staleness predicate is written IFNULL(...) on
 * purpose so it stays a residual filter and the planner keeps the rowid scan;
 * an index on stats_at would only tempt it into an index scan plus a sort.
 *
 * COMPLEXITY. M = rows in entity_mentions, E = distinct entities, R = rows in
 * entity_relationships, d(x) = distinct item_uids mentioning x.
 *   N          one distinct scan over idx_em_item                O(M)
 *   marginals  one ordered GROUP BY riding the (entity_id, item_uid, field)
 *              PK autoindex — no temp b-tree                     O(M) time,
 *                                                                O(E) memory
 *   per edge   COUNT(DISTINCT a.item_uid) driven from the SMALLER side and
 *              probing the larger through that same autoindex
 *                                                        O(min(d_a,d_b) log M)
 *   total      O(M + Sum_edges min(d_a, d_b) log M), two SQL round trips per
 *              edge, R edges.
 * The pairwise cross-product is never materialised — neither the O(E^2) form
 * nor the O(Sum_uid k^2) co-occurrence form. The pair set is exactly the R
 * edges already present in entity_relationships, which is what we annotate;
 * pairs that do not already have an edge are not our business. Driving the
 * joint from the smaller side is what makes hubs affordable: a prefecture with
 * d=200000 paired against a person with d=4 costs four index probes, not
 * 200000. The only unbounded resource is the marginal map: E entries at
 * ~(key + 16) bytes, roughly 60 MB at one million entities.
 *
 * ROWID ASSUMPTION. Paging is keyset over entity_relationships.rowid — the
 * table is an ordinary rowid table (PRIMARY KEY(src,dst,rel_type), no
 * WITHOUT ROWID), so rowid seeks are B-tree seeks and paging costs O(R) in
 * total rather than a re-scan per page. If that table is ever redeclared
 * WITHOUT ROWID this module must switch to a keyset over the PK triple.
 */
#ifndef JO_ENTITY_STATS_H
#define JO_ENTITY_STATS_H

#include "db.h"

/* Minimum joint count for pmi/lift to be written at all. Below this the
 * estimator is dominated by sampling noise; see the header note above. */
#define ENTITY_STATS_MIN_CO_COUNT 3

/* Edges per write transaction. Sized so a batch commits in single-digit
 * milliseconds: this competes with httpd writer threads for SQLite's single
 * write lock, and one giant transaction over a real corpus would stall them
 * for minutes and blow the WAL up to the size of the whole table. */
#define ENTITY_STATS_BATCH 500

/* Freshness horizon. An edge whose stats_at is newer than this is skipped as
 * already scored. Two jobs at once: (1) steady-state cost — the pod ticks far
 * more often than corpus statistics meaningfully move, so most ticks do
 * nothing beyond one bounded probe; (2) resumability — a run cancelled at
 * shutdown leaves its completed prefix *fresh*, so the next run skips it and
 * continues where the last one stopped instead of restarting from the top.
 * Force a full rescore with: UPDATE entity_relationships SET stats_at=NULL. */
#define ENTITY_STATS_TTL_SEC 21600   /* 6h */

/* Idempotent guarded migration for the four columns above (PRAGMA table_info
 * then ALTER only what is missing). Safe to call on every boot and on every
 * run; entity_stats_recompute() calls it itself, so wiring it into db.c is
 * optional and only buys read paths the columns before the first pod tick.
 * Returns 0 on success, -1 if a column is missing AND could not be added. */
int entity_stats_ensure_columns(db_handle *db);

/* Recompute significance for existing relationship edges. Returns number of
 * edges updated, or negative on failure.
 *
 * Batched (ENTITY_STATS_BATCH edges per BEGIN IMMEDIATE / COMMIT), so partial
 * work is durable and no single transaction is ever held open for the length
 * of the sweep. `cancel` may be NULL; when non-NULL it is polled between edges
 * and between batches, and a set flag stops after the in-flight batch commits
 * — the return value is then the count actually written, not an error.
 * Edges still within ENTITY_STATS_TTL_SEC of their last scoring are skipped,
 * so a return of 0 normally means "everything already fresh", not "nothing to
 * do at all". */
long entity_stats_recompute(db_handle *db, volatile int *cancel);

/* ORCHESTRATOR WIRING — three items, none of which touch this module.
 *
 * 1. (optional) core/db.c, inside db_open's boot-migration block, after the
 *    schedule_mode guard and before db_seed_sources(db):
 *        #include "entity_stats.h"
 *        entity_stats_ensure_columns(db);
 *    Skip it and the first pod tick performs the same migration.
 *
 * 2. REQUIRED — the internal pod. New file
 *    native/collectors/sources/entity_stats_pod.c (the Makefile globs that
 *    directory, so there is no build change):
 *
 *        #include "../../source.h"
 *        #include "../../core/entity_stats.h"
 *
 *        static int run(const source_ctx *ctx, intel_sink *sink) {
 *          (void)sink;              // scoring writes through entity_stats
 *          return entity_stats_recompute(ctx->db, ctx->cancel) < 0 ? -1 : 0;
 *        }
 *
 *        static const source_def entity_stats_def = {
 *          .id = "entity-stats", .collector = "_maint",
 *          .name = "Entity Correlation Scoring",
 *          .name_ja = "エンティティ相関スコア",
 *          .update_interval_sec = 900, .run = run };
 *        REGISTER_SOURCE(entity_stats_def)
 *
 *    collector MUST be "_maint" (or any leading-underscore name). scheduler.c
 *    (~line 56) skips fetch_log_write + anomaly_detect for underscore-prefixed
 *    collectors, and this pod needs that skip on both counts: it emits zero
 *    intel_items, so it would log records=0 on every run forever and trip
 *    records_drop against its own baseline; and a first full sweep over a real
 *    corpus runs for minutes, which trips duration_outlier. Neither is a fault
 *    — they are what this job correctly looks like. It also inherits the
 *    scheduler's skip-if-running serialisation, so two sweeps never overlap.
 *
 *    Interval 900s pairs with the 6h TTL: new edges (stats_at NULL) are picked
 *    up within 15 minutes, the corpus is fully rescored every 6 hours, and a
 *    sweep too large for one tick simply continues on the next.
 *
 * 3. No httpd.c wiring. This module has no HTTP surface — exposing pmi/lift on
 *    the graph read path is entityapi.c's business, not ours.
 */

#endif
