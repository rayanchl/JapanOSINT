/* core/entitystore.h — THE one entity-graph write surface (faithful port of
 * server/src/utils/entityStore.js write fns). Used by BOTH producers via the
 * SAME path: the LLM NER enricher (collector intel_items) and the OSINT
 * search ingest (pipeline.c). Tables already in core/schema.sql; entities_fts
 * mirror mirrors core/intel.c's fts_write pattern with fts_segment().
 *
 * Tier-1 key is exact (NFKC* + ws-collapse + trim); sameness is the LLM
 * resolver's job (tier-2 es_record_merge/es_union_entities). *NFKC is a
 * documented pragmatic approximation (ws-collapse only; no ICU compat
 * decomposition) — parity abandoned, determinism is what matters; the
 * resolver adjudicates real sameness anyway. Same stance as linecolor/
 * normName elsewhere in the C port. */
#ifndef JO_ENTITYSTORE_H
#define JO_ENTITYSTORE_H
#include "db.h"
#include <stddef.h>

/* ── the breach quarantine scope ───────────────────────────────────────────
 * A reserved value for entities.tenant_id / entity_mentions.tenant_id. It is
 * NOT a tenant: no tenant can ever be created with this id, and nothing joins
 * it to the tenants table.
 *
 * WHY it exists. breach_index.c materializes every breached identifier into
 * this graph as an entity + a mention whose `surface` is the CLEARTEXT
 * identifier and whose `source_id` names the breach. Those writes used to land
 * with tenant_id NULL, i.e. in the shared, everybody-can-read graph, and were
 * indexed into entities_fts. /api/breach/search, the two /api/intel breach
 * doors and /api/export are all wrapped in httpd.c's breach_gate()
 * (opgate_check == platform operator) — but the /api/entities subtree is only
 * tenant-resolved, and its predicate "tenant_id IS NULL OR tenant_id = :me"
 * matches every NULL-tenant row. So a `viewer` in any tenant could read
 * breached addresses out of /api/entities/search, read which breach an
 * identifier came from out of /:type/:id/mentions, and enumerate an
 * identifier's breaches out of /:type/:id/breaches. That is the whole gate,
 * bypassed, and it was bypassed because the gate lived on the ROUTES while the
 * data lived in a shared table.
 *
 * The fix is at the corpus. A row stamped with this scope is not NULL and is
 * not any tenant's id, so it drops out of "tenant_id IS NULL OR tenant_id = ?"
 * automatically — in entityapi.c and equally in casesapi.c:341,
 * aoiapi.c:639 and exportapi.c:529, which all carry that same predicate and
 * are therefore fixed for free. Only a caller that deliberately binds this
 * sentinel (entityapi's `is_operator` paths) can see them again. A new door
 * onto `entities` is safe by construction rather than by remembering to add a
 * gate — which is exactly what was forgotten twice. */
#define ES_BREACH_TENANT "__breach__"

/* Idempotent, self-healing schema step for the scope above: adds
 * entity_mentions.tenant_id (ensure_column — schema.sql is generated from the
 * live DB and its CREATE TABLE IF NOT EXISTS would never reach a deployed
 * database), creates the partial index the backfill probe rides on, and
 * quarantines rows written by breach ingests that ran BEFORE this change.
 *
 * Called behind a once-guard from every es_* write and from every entityapi
 * read entry point, the way breach_monitor_migrate() does it, so the module is
 * correct even though core/db.c's boot-migration block does not know about it
 * (called from db_open()'s boot-migration block; see entitystore.c). */
void  es_breach_scope_migrate(db_handle *db);

/* nfkcCollapse(value): ws-collapse + trim into out (caller-sized). */
void  es_norm_key(const char *value, char *out, size_t n);

/* upsertEntity({type,value}). Returns a malloc'd entity_id (caller frees) or
 * NULL on empty value. Idempotent on UNIQUE(type,norm_key); appends new
 * surface to aliases; keeps entities_fts in sync.
 *
 * This is the SHARED-graph writer: es_upsert_entity_scoped(..., NULL). */
char *es_upsert_entity(db_handle *db, const char *type, const char *value);

/* upsertEntity with an explicit corpus scope. `scope` is NULL for the ordinary
 * shared graph or ES_BREACH_TENANT for the breach corpus.
 *
 * Two asymmetries are deliberate:
 *
 *  - A breach write NEVER re-scopes an entity that already exists as shared.
 *    That value is already attested by the ordinary corpus (an NER hit on a
 *    published article); quarantining it would delete real intel from every
 *    tenant's graph to hide something that is not secret.
 *
 *  - A shared write PROMOTES a quarantined entity back to the shared graph
 *    (tenant_id NULL) and indexes it into entities_fts for the first time.
 *    Without this, an identifier first seen in a breach would swallow every
 *    later intel mention of the same value into an invisible node — the
 *    breach fix would start discarding non-breach data, which is house rule 2.
 *    It leaks nothing: the promotion is triggered by a non-breach source
 *    independently naming the value, and the breach MENTIONS stay quarantined,
 *    so "which breach" is still operator-only.
 *
 * A quarantined entity is never written to entities_fts: keeping it out of the
 * index means a future FTS reader that forgets to join back to entities.tenant_id
 * cannot leak it, and it keeps hundreds of millions of breach rows out of the
 * search index. Operators reach quarantined nodes by exact identifier instead
 * (entityapi_search_scoped's operator probe). */
char *es_upsert_entity_scoped(db_handle *db, const char *type,
                              const char *value, const char *scope);

/* addMention: INSERT ON CONFLICT(entity_id,item_uid,field); bumps
 * mention_count only on a genuinely new (entity,item,field) edge.
 * Shared-graph writer: es_add_mention_scoped(..., NULL). */
void  es_add_mention(db_handle *db, const char *entity_id,
                     const char *item_uid, const char *source_id,
                     const char *surface, const char *field,
                     double confidence, const char *extractor);

/* addMention with an explicit corpus scope (NULL | ES_BREACH_TENANT).
 *
 * The mention is the row that actually carries the disclosure: `surface` is the
 * cleartext identifier and `source_id` is the breach it came out of. It is
 * scoped SEPARATELY from its entity because the two are not the same claim —
 * a shared entity (public email, seen in an article) can still have breach
 * mentions, and those must stay operator-only.
 *
 * Quarantine is sticky on conflict: a later shared write onto the same
 * (entity,item,field) can raise the confidence and refresh the surface but can
 * never clear the scope. */
void  es_add_mention_scoped(db_handle *db, const char *entity_id,
                            const char *item_uid, const char *source_id,
                            const char *surface, const char *field,
                            double confidence, const char *extractor,
                            const char *scope);

/* addRelationship: skip self-edge; INSERT ON CONFLICT(src,dst,rel_type)
 * accumulating weight; evidence_uid COALESCEd. */
void  es_add_relationship(db_handle *db, const char *src_id,
                          const char *dst_id, const char *rel_type,
                          double weight, const char *evidence_uid);

/* tier-2 resolver helpers (entity_merges + union). */
int   es_merge_pair_exists(db_handle *db, const char *a, const char *b);
void  es_record_merge(db_handle *db, const char *a, const char *b,
                      int same, double confidence, const char *reason);
/* Fold loser into survivor (lexically-smaller id wins); repoint mentions +
 * relationships, append loser canonical to survivor aliases, delete loser,
 * recount, fix FTS. Returns 1 on success.
 *
 * Returns 0 (refuses) when exactly one side is ES_BREACH_TENANT-scoped: the
 * union appends the loser's canonical to the survivor's aliases and rewrites
 * the survivor's FTS row, so merging a quarantined identifier into a shared
 * node would publish that identifier into entity search — the union is a
 * back door into the very disjunct the scope exists to close. The resolver
 * treats a refusal like any other non-merge. */
int   es_union_entities(db_handle *db, const char *id_a, const char *id_b);

#endif
