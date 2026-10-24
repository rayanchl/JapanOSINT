/* core/fewshot.{c,h} — the phase-1 analysis prompt's routing examples.
 *
 * WHERE THEY LIVE. grammars/fewshot_analysis.jsonl, one example per line:
 *   {"query": "...", "response": "<the exact JSON the model should answer>",
 *    "services": [every service the response names], "spine": true?}
 * They used to be ~27 KB of string literals inside prompt_analysis()
 * (core/prompts.c); the file holds them byte for byte (the static block
 * rebuilt from it is identical, and tests/unit/test_fewshot.c pins that), and
 * tests/unit/test_service_names.c reads it to keep every name registered.
 *
 * WHY RETRIEVE. All 76 examples went into every analysis prompt — ~27 KB of
 * a context the service catalogue then had to be squeezed into
 * (pipeline.c measures the preamble and gives the catalogue what is left). A
 * query about a VIN gains nothing from fifteen IP and domain examples. With an
 * embedding server the prompt gets the FEWSHOT_TOP_K examples whose query is
 * nearest to the user's, plus the "spine" examples (multi-entity, typos) that
 * teach behaviour every query needs, in file order. The bytes saved go back
 * to the service catalogue.
 *
 * INDEX. `fewshot_vec` (lib/vecindex, key = hash of query+response, text =
 * the example's query), synced with vecindex_sync() on first use in a
 * process — unchanged examples cost a hash compare. An edited file re-embeds
 * only what changed; an example removed from the file is never selected (a
 * KNN hit whose key is not in the loaded set is ignored).
 *
 * INERT. No JO_EMBED_URL, an unusable index, an embed failure or an empty KNN
 * → fewshot_select() returns NULL and the caller uses fewshot_static_block(),
 * which is the old prompt byte for byte. */
#ifndef JO_FEWSHOT_H
#define JO_FEWSHOT_H

#include "db.h"
#include "../lib/vecindex.h"

#define FEWSHOT_FILE   "grammars/fewshot_analysis.jsonl"
#define FEWSHOT_TOP_K  8

extern const vecindex_spec fewshot_spec;

typedef struct {
  int total;           /* examples in the file                              */
  int chosen;          /* examples in the block                             */
  int by_similarity;   /* of those, chosen by the KNN                       */
  int spine;           /* of those, always-included spine examples          */
} fewshot_note;

/* Every example, in file order, as "Query: q\nresponse\n\n" — what the
 * prompt held before retrieval existed. Cached for the process; never free.
 * "" when the file is missing (said once on stderr). */
const char *fewshot_static_block(void);

/* The retrieved block (same format, file order), malloc'd; NULL when it
 * cannot be built honestly (see INERT above). `note` may be NULL. */
char *fewshot_select(db_handle *db, const char *query, fewshot_note *note);

/* How many examples the file holds (0 when missing). */
int fewshot_count(void);

#endif
