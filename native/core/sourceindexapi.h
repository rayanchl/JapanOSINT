/* core/sourceindexapi.h — HTTP routes over the widened service index
 * (core/service_vec.h): searching the registry by meaning.
 *
 * GET /api/sources/search?q=&kind=&k=
 *   The k registered sources closest to q — reciprocal-rank fusion of the
 *   vector arm and the keyword arm over every source's card
 *   (core/source_card.h). `kind` narrows to pivot|scheduled|pod|manual.
 *   Each result: id, name, category, kind, score, vector_rank, keyword_rank.
 *   `coverage` says which kinds were searched, which were asked for and are
 *   NOT ready yet (not searched, never silently), how many cards were in
 *   reach, and the index's own coverage object. 503 with the reason when the
 *   index cannot answer (no JO_EMBED_URL, not built, model changed); 400 on a
 *   missing q or an unknown kind; k is capped at SIDX_K_MAX and the cap is
 *   stated in-band.
 *
 * It embeds the query, so httpd.c runs it through sem_spawn, off the event
 * loop, with the semsearch_route_fn signature. */
#ifndef JO_SOURCEINDEXAPI_H
#define JO_SOURCEINDEXAPI_H

#include "db.h"

#define SIDX_K_DEF 20
#define SIDX_K_MAX 200

/* GET /api/sources/categories — how the registry maps onto source.h's 31
 * categories (core/source_category.h): {declared:{cat:n}, derived:{cat:n},
 * unlabelled, derived_at, vote_k, vocabulary}. Reads the sources table only;
 * the per-source labels are the category_canonical / category_provenance /
 * category_confidence columns of GET /api/sources. */
char *sourceindexapi_categories(db_handle *db);

char *sourceindexapi_search(db_handle *db, const char *tenant, const char *id,
                            const char *qs, int is_operator, int *status);

#endif
