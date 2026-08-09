/* core/fts_schema.h — the column set of intel_items_fts, and the boot
 * migration that widens it on an already-deployed database.
 *
 * ── WHY v2 EXISTS ─────────────────────────────────────────────────────────
 *
 * v1 was fts5(uid UNINDEXED, title, body, summary, keywords). Everything else
 * on an intel_items row — link, author, tags, and the whole `properties` JSON
 * blob — was unindexed, so `/api/intel/items?q=` could not find an item by its
 * URL, its author, a tag, or any structured field a collector emitted.
 *
 * The worst case was cameras. core/camera_store.c writes ONLY `title` (the
 * merged camera name); the discovery channel, operator, camera_uid, stream and
 * snapshot URLs all live in `properties`. So the entire camera corpus — the
 * largest record_type in the table — was searchable by name and by literally
 * nothing else. v2 indexes link/author/tags/props and fixes that.
 *
 * ── ANSWERING translate.h's OBJECTION ─────────────────────────────────────
 *
 * translate.h option (a) rejected exactly this change, on the grounds that a
 * full-corpus FTS rebuild is "NOT resumable — a rebuild interrupted at 40%
 * leaves an FTS index that silently answers every query, alert predicate,
 * export and corpus lookup with a fraction of the corpus." That objection is
 * correct and it is the reason this migration is written the way it is:
 *
 *   - The whole rebuild is ONE transaction. It is not resumable, but it is
 *     ATOMIC, which is the property that objection actually needs. An
 *     interruption at 40% rolls back to the intact v1 index and the migration
 *     simply runs again on the next boot. There is no window in which a
 *     partial index answers a query. Cost of a crash is time, never silence.
 *
 *   - It runs inside db_open(), BEFORE the HTTP server and the collector
 *     scheduler exist, so there is no concurrent writer to race and no
 *     catch-up pass to get subtly wrong.
 *
 *   - `keywords` is CARRIED OVER, not recomputed. That column is not written
 *     by the ingest path (intel.c hardcodes ''): it holds machine-translated
 *     English (translate.c fts_put_en) and burned-in OCR text (media.c). A
 *     rebuild that dropped it would silently un-translate the corpus and the
 *     only repair would be re-running the LLM over every row. The old values
 *     are copied into a temp table before the old index is dropped.
 *
 *   - `_fts_meta` (present in schema.sql, dead since the Node import) finally
 *     gets a writer: it records the column set, tokenizer and version that
 *     produced the live index.
 *
 * WHAT IT COSTS. One MeCab segmentation pass over every Japanese title/body/
 * summary in the corpus, single-writer, at boot. On a large corpus that is
 * minutes. Set JO_FTS_REBUILD=0 to skip it for one boot if you need the
 * process up immediately; the migration re-arms on the next boot without the
 * flag. Search keeps working against the v1 index, without the new columns.
 *
 * ── THAT LAST SENTENCE USED TO BE A LIE ───────────────────────────────────
 *
 * It said the same thing, and it was exactly backwards. intel.c's write path
 * DELETEd the item's FTS row and then prepared an INSERT naming all nine
 * columns. On a v1 index that prepare fails, so the row was deleted and never
 * put back — and scheduled collectors re-emit continuously, so the index bled
 * rows for as long as the flag was set, with no error surfaced to any API.
 * "JO_FTS_REBUILD=0 keeps search working" named the one state in which every
 * write was a net delete.
 *
 * Two things make the sentence true now, and both are needed:
 *   - fts_insert_sql() below returns the statement matching the LIVE index, so
 *     a v1 database indexes its five columns rather than nothing;
 *   - intel.c orders the DELETE after a successful prepare of that statement,
 *     so any future column drift degrades to a stale row instead of no row.
 */
#ifndef JO_FTS_SCHEMA_H
#define JO_FTS_SCHEMA_H
#include "db.h"

/* Bumped whenever FTS_SCHEMA_COLUMNS changes. Recorded in _fts_meta.version. */
#define FTS_SCHEMA_VERSION 2

/* The single source of truth for the column set. schema.sql repeats it
 * verbatim for the fresh-database case (CREATE ... IF NOT EXISTS cannot widen
 * an existing table, which is the entire reason this module exists); if you
 * change one, change the other. */
#define FTS_SCHEMA_COLUMNS \
  "uid UNINDEXED, title, body, summary, keywords, link, author, tags, props"
#define FTS_SCHEMA_TOKENIZER "unicode61 remove_diacritics 1"

/* The column added by v2 that is probed to decide whether a rebuild is due.
 * Must be a column name that exists in FTS_SCHEMA_COLUMNS and did NOT exist in
 * the previous version. */
#define FTS_SCHEMA_SENTINEL_COL "props"

/* Flatten a JSON document into indexable text: object → VALUES only (keys are
 * schema noise — nobody searches for the word "discovery_channel"), array →
 * elements, nested containers recursed, scalars stringified, all space-joined.
 * Non-JSON / empty input yields "". Individual strings longer than
 * FTS_FLATTEN_MAX_VALUE and output past FTS_FLATTEN_MAX_TOTAL are dropped, so
 * a base64 thumbnail or an inline GeoJSON ring cannot bloat the index.
 * Returns malloc'd; caller frees. Never returns NULL. */
#define FTS_FLATTEN_MAX_VALUE 512
#define FTS_FLATTEN_MAX_TOTAL 4096
char *fts_flatten_json(const char *json);

/* Idempotent. No-op when intel_items_fts already carries FTS_SCHEMA_COLUMNS.
 * Otherwise rebuilds the index in one transaction (see the header comment) and
 * rewrites intel_items_fts_uid_map to match. Call from db_open()'s boot
 * migration block, AFTER translate_migrate() (whose translate_state table this
 * rewinds) and before the server starts. Failures are logged and leave the
 * existing index untouched. */
void fts_schema_migrate(db_handle *db);

/* Does the LIVE intel_items_fts carry the v2 column set? Probed once per
 * process (the answer is a property of the database, not of a connection) and
 * re-probed after a successful rebuild. */
int fts_index_is_v2(sqlite3 *h);

/* The INSERT statement that matches the live index, so the write path can
 * never fail to prepare against a database it is otherwise happy to read.
 * Static storage; do not free.
 *
 *   v2: (uid,title,body,summary,keywords,link,author,tags,props)
 *       VALUES(?1,?2,?3,?4,'',?5,?6,?7,?8)
 *   v1: (uid,title,body,summary,keywords) VALUES(?1,?2,?3,?4,'')
 *
 * ?1..?4 mean the same thing in both; ?5..?8 exist only in the v2 form. Ask
 * fts_index_is_v2() before binding them. `keywords` is literal '' in both —
 * see the header comment: it is translate.c's and media.c's column, not the
 * ingest path's. */
const char *fts_insert_sql(sqlite3 *h);

#endif
