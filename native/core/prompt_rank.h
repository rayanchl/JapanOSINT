/* core/prompt_rank.{c,h} — which records a bounded prompt view shows.
 *
 * WHY. results_view_for_prompt() (core/pipeline.c) can show the model only
 * JO_PROMPT_RECORDS_PER_SERVICE records per service (default 8) — the prompt
 * is the one consumer that physically cannot take everything. It kept the
 * FIRST 8, which is whatever order the upstream answered in: a registry
 * search that lists 400 filings alphabetically shows the model eight filings
 * starting with "A", and the one that names the entity asked about is never
 * in front of it. Choosing the 8 by cosine similarity to the query is the
 * same bound, spent on the records most likely to matter.
 *
 * WHAT IT GUARANTEES.
 *   - The bound and how it was chosen are stated in-band: a reranked service's
 *     data carries records_selected_by:"similarity_to_query" and
 *     records_ranked (how many were scored), and the stored result keeps every
 *     record in its original order — only the prompt's view is reordered.
 *   - One query embed and one batch of record embeds per RUN: a record already
 *     embedded for round 1 is not sent again for round 2 or the synthesis
 *     (cached by text hash).
 *   - INERT. No JO_EMBED_URL → prompt_rank_new() returns NULL and the caller
 *     takes the positional path byte for byte. An embed failure mid-run turns
 *     the ranker off for the rest of the run (positional from then on) rather
 *     than retrying a failing server on every round.
 *
 *   JO_PROMPT_RERANK_MAX   records per service that are scored (default 256);
 *                          a service with more is ranked among its first N and
 *                          records_ranked says so. */
#ifndef JO_PROMPT_RANK_H
#define JO_PROMPT_RANK_H

#include "../third_party/cJSON.h"

#define PROMPT_RERANK_MAX_DEFAULT 256

typedef struct prompt_rank prompt_rank;

/* NULL when no embedding server is configured (nothing is sent). The query is
 * embedded lazily, on the first service that needs ranking. */
prompt_rank *prompt_rank_new(const char *query);
void         prompt_rank_free(prompt_rank *pr);

/* 1 while the ranker can still rank (configured, nothing has failed). */
int prompt_rank_active(const prompt_rank *pr);

/* Reorder `records` (a JSON array, modified in place) so that its first
 * min(keep, n) items are the ones most similar to the query, best first, and
 * delete everything after `keep`. Only the first JO_PROMPT_RERANK_MAX are
 * scored; *ranked_out (may be NULL) gets that number. Returns 1 when ranked,
 * 0 when it did nothing (inactive, n <= keep, or an embed failed — then the
 * array is untouched and the caller trims positionally). */
int prompt_rank_select(prompt_rank *pr, cJSON *records, int keep, int *ranked_out);

/* Texts sent to the embedding server by this ranker so far (tests, logs). */
long prompt_rank_sent(const prompt_rank *pr);

/* The text a record is judged by: its title/name/summary/description-like
 * fields when it has them, else its JSON. Malloc'd; never NULL on success. */
char *prompt_rank_record_text(const cJSON *rec);

#endif
