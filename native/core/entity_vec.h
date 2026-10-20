/* core/entity_vec.{c,h} — the entity name index (lib/vecindex).
 *
 * WHY THIS EXISTS. The tier-2 resolver (core/entity_enrich.c) pairs entities
 * whose norm_key shares its first four characters. That never pairs the same
 * company written in two scripts — トヨタ自動車 and "Toyota Motor" share no
 * prefix — and those are exactly the duplicates an analyst cannot see past.
 * entitystore already derives a Hepburn romaji reading for every CJK
 * canonical; embedding "canonical (romaji)" puts the two spellings near each
 * other, and the nearest neighbours above a cosine floor become extra
 * CANDIDATES for the same LLM judge. Nothing here merges anything: a
 * neighbour is a question put to the judge, and the judge's 0.7 confidence
 * rule (entity_enrich.c, union pass) is unchanged.
 *
 * The index is partitioned by entity TYPE (a KNN for a company never reads a
 * person's chunks) and carries tenant_id ('' = the shared graph) as a
 * metadata column, so a neighbour is only ever another entity of the same
 * type in the same scope. Breach-quarantined entities (ES_BREACH_TENANT) are
 * never embedded — the same rule that keeps them out of entities_fts.
 *
 * Filled by the `_entity_vec` maintenance pod, a keyset walk over entities
 * JO_ENTITY_VEC_MAX_PER_RUN (default 2000) rows per tick; an unchanged
 * canonical costs a hash compare, not an embed. Vectors of entities that a
 * union folded away are pruned by the same pod. Inert unless JO_EMBED_URL is
 * set; inert is announced, never silent.
 *
 *   JO_ENTITY_VEC_COS_FLOOR   neighbour floor for a candidate (default 0.80)
 *   JO_ENTITY_VEC_MAX_PER_RUN entities looked at per tick (default 2000)
 */
#ifndef JO_ENTITY_VEC_H
#define JO_ENTITY_VEC_H

#include "db.h"
#include "../lib/vecindex.h"

struct http_client;

/* 0.80: below it, multilingual-e5 puts unrelated short company names of the
 * same industry ("Kobe Steel" / "Kobe Gas") close enough to waste judge calls;
 * above ~0.9 the cross-script pair this index exists for (a kanji canonical
 * against its romaji-led Latin spelling) starts to drop out. It only decides
 * which pairs are ASKED about — a false candidate costs one LLM call and is
 * recorded as same=0. */
#define ENTITY_VEC_COS_FLOOR 0.80
/* Neighbours per entity asked of the KNN (self excluded). */
#define ENTITY_VEC_K 10

extern const vecindex_spec entity_vec_spec;

/* JO_ENTITY_VEC_COS_FLOOR when set to a number in (0,1], else the default. */
double entity_vec_cos_floor(void);

/* "canonical (romaji)" — the romaji only when present and different from the
 * canonical. malloc'd, bounded on a UTF-8 boundary. */
char *entity_vec_text(const char *canonical, const char *romaji);

/* One pod tick on `db` (the caller's own connection). Returns entities
 * embedded this tick, or -1 (reason in entity_vec_meta last_error). 0 and no
 * network when JO_EMBED_URL is unset. */
int entity_vec_tick(db_handle *db, struct http_client *http, int max_walk);

/* 1 when the index exists, is not refused and records a dimension — for
 * code that compares STORED vectors with each other (no query embed, so the
 * live server is not consulted). */
int entity_vec_ready(db_handle *db);

typedef struct { char *entity_id; double cos; } entity_nb;

/* Up to `k` stored neighbours of `entity_id` with cosine >= floor, of the
 * same type and tenant scope, nearest first. -1 when the entity has no
 * vector or the index is not ready. Free with entity_nb_free. */
int  entity_vec_neighbours(db_handle *db, const char *entity_id, const char *type,
                           const char *tenant, int k, double floor,
                           entity_nb **out);
void entity_nb_free(entity_nb *v, int n);

#endif
