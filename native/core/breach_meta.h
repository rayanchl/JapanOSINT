/* core/breach_meta.h — the breach CATALOG. One row per breach (e.g.
 * "goose-creek"), loaded from the docs/breach-corpus.json manifest into the
 * breach_meta table, and surfaced as intel SOURCES via /api/intel/sources
 * (category "breach"). breach_id == the slug used as source_id on every
 * breach_items row and on entity_mentions.source_id, so catalog, records, and
 * entities all line up. See core/breach_adapter.c for the record→item adapter. */
#ifndef JO_BREACH_META_H
#define JO_BREACH_META_H
#include "db.h"

/* Load the breach manifest into breach_meta (idempotent upsert).
 * `path` NULL → repo-relative docs/breach-corpus.json (env JO_BREACH_CORPUS
 * overrides). Returns the number of rows loaded, or <0 on error. */
int breach_meta_load_manifest(db_handle *db, const char *path);

/* ── Seed TSV (docs/breach-corpus.seed.tsv) ───────────────────────────────
 * The TSV is the hand-maintained identity manifest; scripts/fetch-breach-corpus.mjs
 * turns it into breach-corpus.json. These two entry points read the TSV directly so
 * the backend can seed itself with no Node step.
 *
 * The slug and record-count rules below are a DELIBERATE port of that script's
 * slug()/parseCount(), quirks included (see breach_meta.c). breach_id is the
 * source_id on every breach_items row and on entity_mentions.source_id, so a slug
 * that drifts from the script orphans existing records. Do not "improve" it. */

/* One parsed catalog row, pre-write. */
typedef struct {
  char breach_id[128];   /* slug(name); "" when the name has no word chars */
  char name[256];
  long long pwn_count;   /* -1 when the records field didn't parse */
  char added_date[32];   /* YYYY-MM-DD */
  char breach_date[32];  /* YYYY-MM (month precision, as in the file) */
} breach_seed_row;

typedef struct {
  int rows_read;      /* non-blank, non-comment lines */
  int rows_parsed;    /* rows with a usable (non-empty) slug */
  int rows_skipped;   /* header + nameless rows + rows whose slug came out empty */
  int rows_new;       /* parsed rows not yet in breach_meta */
  int rows_existing;  /* parsed rows already in breach_meta */
  char format[8];     /* "tsv" | "json" */
  char path[512];     /* the file actually read */
} breach_seed_stats;

/* Parse only — writes nothing. Fills *stats, and (when `sample` is non-NULL)
 * returns up to `limit` rows in a malloc'd array the caller frees. Returns
 * rows_parsed, or <0 if the file can't be read. */
int breach_meta_parse_seed(db_handle *db, const char *path, int limit,
                           breach_seed_row **sample, int *n_sample,
                           breach_seed_stats *stats);

/* Parse + idempotent upsert into breach_meta. `stats` may be NULL. Returns the
 * number of rows written, or <0 on error. */
int breach_meta_load_seed_tsv(db_handle *db, const char *path,
                              breach_seed_stats *stats);

/* Default seed path: env JO_BREACH_SEED, else <repo>/docs/breach-corpus.seed.tsv. */
const char *breach_meta_seed_path(void);

/* 1 if `id` is a known breach (a breach_meta.breach_id), else 0. Used by the
 * intel item/list endpoints to route a source-filtered request to the adapter. */
int breach_meta_is_source(db_handle *db, const char *id);

/* One catalog row joined with its materialized breach_items aggregate. */
typedef struct {
  char breach_id[128];   /* == intel source id */
  char name[256];
  char domain[256];
  char breach_date[32];
  char added_date[32];
  long long pwn_count;   /* catalog "accounts" count (display fallback) */
  long long item_count;  /* COUNT(*) of materialized breach_items (0 if none) */
  char last_seen[64];    /* MAX(breach_items.first_seen) or "" */
} breach_src_row;

/* Return a malloc'd array of catalog rows (caller frees) via *n. NULL/0 if the
 * catalog is empty. Joins breach_meta with a breach_items GROUP BY source_id
 * aggregate so item_count/last_seen are the real materialized figures. */
breach_src_row *breach_meta_sources(db_handle *db, int *n);

#endif
