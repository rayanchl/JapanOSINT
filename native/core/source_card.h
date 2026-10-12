/* core/source_card.h — the text a registered source is FOUND by.
 *
 * WHY THIS EXISTS. The service index used to embed `id: name. description`
 * for the ~2,300 entity pivots only. Widening it to the whole registry
 * (~18,900 sources: pivots, scheduled feeds, pods) means the card text is
 * read by three consumers that must agree byte for byte — the vector index
 * (core/service_vec.c), its keyword twin (service_fts), and the Python
 * manifest checker (tools/batch_exclusions.py --semantic), which embeds a
 * candidate row and compares it against `japanosint --dump-source-cards`.
 * One function builds the card so the three cannot drift.
 *
 * WHAT A CARD IS. `ID: Name. description` with the description's
 * AUTHORING-TIME NOISE removed, plus an optional `Also asked as: …` line from
 * core/service_aliases.c. The noise is real and it is not small: thousands of
 * generated rows say "33 datasets at authoring time", "At probe time
 * (2026-09-14) …" or "MEASURED 2026-10-05 over …" — facts about the day the
 * row was verified, true then, describing nothing a question could be about,
 * and (being numbers and dates) exactly the tokens a hashed or learned
 * embedding gives the most weight to between near-identical template rows.
 * Removed at CLAUSE level, so "Tochigi press releases, 50 items at probe
 * time." keeps "Tochigi press releases.".
 *
 * Sentences repeated verbatim across many cards (a generator's boilerplate:
 * the same "Records carry title, description, licence." on 300 rows) are
 * collapsed: kept on no card when the card has anything else to say. A
 * sentence every row of a template shares cannot tell those rows apart and
 * only pulls them towards each other.
 *
 * NOTHING IS INVENTED. A card is the source's own prose, minus clauses; a
 * description that is entirely noise leaves the id and name. The catalogue
 * the LLM reads still shows the description verbatim (service_vec.c) — the
 * card is what the index matches on, not what anyone is told.
 *
 * KIND. Every source is exactly one of:
 *   pivot      collector "osint", interval 0 — dispatched on an entity
 *              (house rule 3's first way to be reachable; the same rule
 *              osint_dispatch.c's is_entity_pivot applies)
 *   scheduled  interval > 0 — the scheduler runs it
 *   pod        collector starting with '_' (_maint, _enrich, _test):
 *              pipeline machinery, not a data source
 *   manual     anything else: interval 0 under a non-osint collector, run
 *              only on an explicit --run / POST …/run. Named rather than
 *              folded into a neighbour so a count of it stays visible. */
#ifndef JO_SOURCE_CARD_H
#define JO_SOURCE_CARD_H

#include "../source.h"

typedef enum {
  SC_KIND_PIVOT = 0,
  SC_KIND_SCHEDULED,
  SC_KIND_POD,
  SC_KIND_MANUAL,
  SC_NKINDS
} source_kind;

source_kind source_kind_of(const source_def *d);
const char *source_kind_name(source_kind k);
/* "pivot"/"scheduled"/"pod"/"manual" → kind; -1 for anything else. */
int source_kind_parse(const char *s);

typedef struct {
  const source_def *def;
  source_kind kind;
  const char *category;            /* the declared (resolved) category     */
  char *text;                      /* the card, no embedding prefix         */
  unsigned long long hash;         /* vecindex_hash(text)                  */
} source_card;

/* Every registered source's card, in registry order. Built once per process
 * (the registry is immutable after the constructors) and never freed.
 * Returns the count; *out may be NULL on allocation failure (count 0). */
int source_cards_all(const source_card **out);

/* The card for `id`, or NULL. O(1) through registry_index(). */
const source_card *source_card_get(const char *id);

/* The noise-stripping half of a card, exposed for the test: a malloc'd
 * cleaned copy of `desc` (never NULL for a non-NULL input; "" when every
 * clause was noise). Corpus-frequent sentences are NOT dropped here — that
 * needs the whole registry and happens in source_cards_all(). */
char *source_card_clean(const char *desc);

#endif
