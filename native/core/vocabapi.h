/* core/vocabapi.h — GET /api/vocab: the server's own enumerations, served.
 *
 * WHY. The audit's §8 item 9 / recommendation 19: 22 contracts are defined in
 * two or more languages, nine of them disagree, and there are 189 hand-written
 * Swift `Codable` mirrors with no code generation. Every one of those drifts
 * silently, because nothing compares them. A client that reads its chip sets,
 * pickers and validators from the server cannot drift from the server.
 *
 * WHAT IS IN HERE — and, more importantly, where each list comes from. Nothing
 * below is typed out from memory; every entry is either read from an existing
 * C accessor, transcribed from the CHECK constraint that already rejects
 * anything else, or counted out of the database at request time. Each block
 * carries a `source` string naming that origin, so a reader can go and check.
 *
 *   CLOSED vocabularies — a fixed list the server enforces. Adding a value
 *   without changing the server is impossible, so the client may treat these
 *   as exhaustive:
 *     ref_types           annotationsapi.c REF_TYPES, via the existing
 *                         annotations_ref_types() accessor — NOT a second copy
 *     tenant_roles        schema.sql CHECK(role IN …) on memberships
 *     case_status         schema.sql CHECK(status IN …) on cases
 *     case_member_roles   schema.sql CHECK(role IN …) on case_members
 *     saved_search_kinds  savedsearchapi.c kind_valid()
 *     alert_channel_kinds alertsapi.c channel validation
 *     aoi_kinds           aoiapi.c kind_valid()
 *     breach_monitor_kinds schema.sql CHECK(kind IN …) on breach_monitors
 *     source_types        schema.sql CHECK(type IN …) on sources
 *     source_statuses     schema.sql CHECK(status IN …) on sources
 *     tenant_plans        schema.sql CHECK(plan IN …) on tenants
 *
 *   OPEN vocabularies — sets no CHECK constraint bounds, so the only truthful
 *   answer is what actually exists. These are COUNTED FROM THE DATABASE, not
 *   listed in C:
 *     record_types        SELECT record_type, COUNT(*) FROM intel_items
 *     entity_types        SELECT type, COUNT(*) FROM entities
 *     rel_types           SELECT rel_type, COUNT(*) FROM entity_relationships
 *
 * The record_type case is the one worth spelling out. The collector corpus
 * contains 525 distinct record_type string literals across 737 emission sites,
 * of which 462 appear exactly once — there is no central list anywhere and
 * inventing one would be fabricating a contract the code does not honour. The
 * honest, self-maintaining answer is the distribution actually present in the
 * corpus, which is what this endpoint returns, newest count first. A value
 * that no collector has ever emitted is not part of the vocabulary.
 *
 * TENANCY. record_types and entity_types are counted under the same predicate
 * entityapi.c and exportapi.c use — "(tenant_id IS NULL OR tenant_id = ?)" —
 * so the counts are the caller's own view and never leak another tenant's
 * volume. entity_relationships carries no tenant column, so rel_types is
 * corpus-wide; it is a closed schema vocabulary in practice (three values) and
 * carries no tenant signal, and the response says so.
 *
 * COST. The three GROUP BYs are index-assisted (idx_intel_items_type) but
 * still walk millions of rows on a large corpus, so the result is memoised in
 * process for VOCAB_TTL_SEC. The closed lists are free. A miss is the only
 * expensive path and it happens at most once every few minutes per server.
 */
#ifndef JO_VOCABAPI_H
#define JO_VOCABAPI_H
#include "db.h"

/* GET /api/vocab[?refresh=1] — malloc'd JSON, caller frees. `tenant` scopes
 * the counted vocabularies; NULL means "shared rows only". Never NULL except
 * on allocation failure. */
char *vocabapi_get(db_handle *db, const char *tenant, int refresh);

#endif
