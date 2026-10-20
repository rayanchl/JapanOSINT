/* core/relevance.{c,h} — run-time relevance check on an entity pivot's answer
 * (house rule 4d, applied to every dispatch instead of once per batch).
 *
 * WHY. `probe_hp_batch.py --check-filter` asks a pivot about an impossible
 * entity once, at authoring time, and fails it FILTER_IGNORED when the answer
 * does not change. 65 of 360 batch-31 rows failed it. But a filter can stop
 * working AFTER that probe — an upstream renames a parameter and starts
 * ignoring it — and from then on every investigation gets the whole
 * collection back with HTTP 200, attributed to the entity it asked about:
 * the confident wrong answer, with every count green.
 *
 * WHAT IT DOES, after def->run() in osint_dispatch(), on the first
 * RELEV_SAMPLE_MAX records the service emitted:
 *   1. DETERMINISTIC: is the entity (jpnorm-folded) in the record's title,
 *      body, summary or properties — the whole string, or every one of its
 *      words? If at least RELEV_LEXICAL_PASS of the sample have it, done.
 *   2. Only when most lack it, and only with an embedding server: cosine of
 *      the entity against each record's title + summary. A record at or above
 *      RELEV_SIM_MIN is "related" (a romanised name, a parent company).
 *   3. If no more than RELEV_SUSPECT_MAX of the sample is either, the service
 *      result carries a `relevance` object with verdict "relevance-suspect" —
 *      in-band, beside the records, so the model and the reader see it.
 * It never drops, reorders or relabels a record: it is a disclosure.
 *
 * INERT. No JO_EMBED_URL → step 2 cannot run, and no note is ever attached:
 * a lexical miss alone is not evidence (a Japanese registry answering a
 * romanised query in kanji is the normal case), so the deterministic step on
 * its own only ever clears a service, never accuses one.
 *
 *   JO_RELEVANCE_SIM_MIN   overrides RELEV_SIM_MIN (model-dependent)
 *   JO_RELEVANCE_CHECK=0   disables the check */
#ifndef JO_RELEVANCE_H
#define JO_RELEVANCE_H

#include "../source.h"
#include "../third_party/cJSON.h"

#define RELEV_SAMPLE_MAX     32     /* records examined per dispatch          */
#define RELEV_MIN_RECORDS     5     /* fewer: too few to judge, no verdict    */
#define RELEV_MIN_ENTITY      3     /* folded bytes; shorter entities match
                                     * everything and prove nothing           */
#define RELEV_TEXT_MAX     4096     /* bytes of a record searched lexically   */
#define RELEV_LEXICAL_PASS  0.5     /* this share carrying the entity clears  */
#define RELEV_SIM_MIN       0.40    /* cosine at which a record is "related"  */
#define RELEV_SUSPECT_MAX   0.10    /* at most this share related → suspect   */

typedef struct {
  char *text[RELEV_SAMPLE_MAX];     /* folded title+summary+body+properties   */
  char *label[RELEV_SAMPLE_MAX];    /* title + summary, for the embedding     */
  int n;
} relev_sample;

/* Keep `it` if the sample is not full (notices must not be passed). */
void relev_sample_add(relev_sample *s, const intel_item *it);
void relev_sample_free(relev_sample *s);

/* The verdict for `entity` over the sample, `records` being how many the
 * service emitted in all. NULL when there is nothing to say (relevant, too
 * few records, check disabled, or no embedding server); otherwise a
 * {"verdict":"relevance-suspect", …} object the caller owns. */
cJSON *relev_check(const relev_sample *s, const char *entity, long records);

#endif
