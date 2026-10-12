/* core/service_aliases.h — how people actually ASK for a well-known pivot.
 *
 * WHY THIS EXISTS. The service index (core/service_vec.c) matches a question
 * against each source's own prose, and that prose is written by the person
 * who wired the upstream, in the upstream's vocabulary: DOMAIN_WHOIS says
 * "WHOIS / RDAP", never "who owns this domain". Measured 2026-09-11 against
 * multilingual-e5-large at the production K of 60, "who owns example.com"
 * did not return DOMAIN_WHOIS at all. The fused keyword arm fixed the terse
 * case for words the card already carries; this table covers the words it
 * does not.
 *
 * THE RULES, because an alias table is an easy place to invent things:
 *   - existing ids only. tests/unit/test_service_aliases.c resolves every id
 *     through registry_get() and fails on one that does not exist, so a
 *     retired source cannot keep an alias pointing at nothing;
 *   - real phrases only: a phrase must be a plain restatement of what the
 *     source's own description says it answers. Nothing here claims a
 *     capability the collector does not have;
 *   - small and curated. This is for the hand-written entity services a
 *     person asks for in everyday words, not a second description field for
 *     the 18,000 generated rows.
 *
 * The phrases go on the card's last line ("Also asked as: …") and are
 * embedded with it; they never replace the description. */
#ifndef JO_SERVICE_ALIASES_H
#define JO_SERVICE_ALIASES_H

typedef struct {
  const char *id;        /* a registered source id                         */
  const char *phrases;   /* comma-separated everyday phrasings             */
} service_alias;

/* The phrases for `id`, or NULL when it has none. */
const char *service_alias_for(const char *id);

/* The whole table (for the test). Returns the row count. */
int service_aliases_all(const service_alias **out);

#endif
