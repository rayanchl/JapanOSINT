/* core/source_category.h — which of the 31 declared categories a source
 * belongs to, for the sources that declared something else.
 *
 * THE PROBLEM. source.h writes down ONE category vocabulary (31 values) "so
 * the next author picks from it instead of inventing a 32nd". Measured
 * 2026-10-12 over the 18,884 registered sources: about 9,300 use one of the
 * 31; the rest spread over ~190 others — "opendata" 1,916, "research" 707,
 * "legal" 385, "finance" 366, "local-gov" 180, "geo" 109 … — so a client
 * grouping by category gets 220 groups, most of them synonyms of one of the
 * 31.
 *
 * WHAT THIS DOES. It never renames anything: `sources.category` stays what
 * the source declared, because it is user-visible and a source's own claim.
 * Beside it, three DERIVED columns:
 *   category_canonical   one of the 31
 *   category_provenance  'declared' — the source declared one of the 31
 *                        'derived'  — proposed by a vote of the k nearest
 *                                     DECLARED sources in the service index
 *   category_confidence  1.0 when declared; for a derived label, the
 *                        winning category's share of the neighbours' vote
 *                        weight (cosine similarity), so a 0.34 is visibly a
 *                        guess and a 0.95 is not
 * Pods (_maint/_enrich/_test) are pipeline machinery, not data, and get no
 * label. A source with no vector (the index not ready, the server refused
 * its card) gets none either — NULL, never a default: an unlabelled source
 * is reported as unlabelled.
 *
 * WHEN. Declared labels need no embedding and are written every pod tick.
 * Derived labels are recomputed only when the service index is complete and
 * differs (cards, model, dim) from the one the last derivation used — a
 * brute-force cosine pass over ~9,600 × ~9,300 vectors, about a minute of
 * CPU, so never per tick. */
#ifndef JO_SOURCE_CATEGORY_H
#define JO_SOURCE_CATEGORY_H

#include "db.h"
#include "../third_party/cJSON.h"

#define SOURCE_CATEGORY_COUNT 31
extern const char *const SOURCE_CATEGORIES[SOURCE_CATEGORY_COUNT];

/* 1 when `c` is one of the 31. */
int source_category_is_canonical(const char *c);

/* Write the declared labels (and clear a label whose source no longer
 * declares a canonical category). Returns rows changed, -1 on SQL failure. */
int source_category_declare(db_handle *db);

/* Recompute the derived labels if the service index is complete and has
 * changed since the last derivation. Returns the number of sources labelled
 * 'derived' (0 when there was nothing to do or the index is not ready), -1
 * on failure. Reads vectors from the service index; never embeds. */
int source_category_derive(db_handle *db);

/* Write sources.entity_types / entity_types_provenance from the cards
 * (core/source_card.h): the mask for pivots (0 = unlabelled), NULL for every
 * other kind. No embedding; writes only what differs. Rows changed, or -1. */
int source_entity_types_write(db_handle *db);

/* {pivots, labelled, unlabelled, by_type:{person:n,…}} from the cards. */
cJSON *source_entity_types_summary(void);

/* {declared:{cat:n…}, derived:{cat:n…}, unlabelled:n, derived_at, basis}
 * from the sources table. Never NULL. */
cJSON *source_category_distribution(db_handle *db);

#endif
