/* _source_macros.inc — the registration macros that were byte-identical in
 * more than one collector.
 *
 * 55 distinct registration-macro NAMES exist across 80 collectors; 51 of them
 * are genuinely per-file (different field sets, different defaults) and stay
 * where they are. These four were not: `RSSX` was copied verbatim into 18
 * files, `RSS` into 4, `DEFR` into 4 and `DEF` into 2 — i.e. 28 independent
 * copies of the `source_def` field list, every one of which could drift from
 * source.h without anything noticing.
 *
 * Included, never compiled standalone (the Makefile globs *.c, not *.inc).
 * Every includer must already have pulled in source.h; RSSX/RSS additionally
 * need lib/rss_atom.h for rss_collect(). Both are included below so the .inc
 * is self-sufficient wherever it lands in a file's include block.
 *
 * NOTE for tools/lint_sources.py: its mini-preprocessor inlines local
 * `#include "*.inc"` before expanding macros, so the expanded source count is
 * unchanged by moving these here (verified: 2,563 before and after).
 */
#ifndef JO_SOURCE_MACROS_INC
#define JO_SOURCE_MACROS_INC

#include "../../source.h"
#include "../../lib/rss_atom.h"

/* RSS/Atom source, fully parameterised: collector and interval per call. */
#define RSSX(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, LANG, TAGS, IVAL, DESC)  \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                 \
    int n = rss_collect(c, s, URL, LANG, TAGS); return n < 0 ? -1 : 0; }     \
  static const source_def SYM = {                                           \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,            \
    .update_interval_sec = IVAL, .run = run_##SYM,                           \
    .category = CAT, .type = "web_request", .url = URL,                      \
    .description = DESC, .layer = NULL, .free_tier = 1 };                    \
  REGISTER_SOURCE(SYM)

/* Same, for files that fix one collector + one cadence for the whole file and
 * declare them as the object macros COLL and INTERVAL before the first use. */
#define RSS(SYM, ID, NAME, NAMEJA, CAT, URL, LANG, TAGS, DESC)               \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                 \
    int n = rss_collect(c, s, URL, LANG, TAGS); return n < 0 ? -1 : 0; }     \
  static const source_def SYM = {                                           \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,            \
    .update_interval_sec = INTERVAL, .run = run_##SYM,                       \
    .category = CAT, .type = "web_request", .url = URL,                      \
    .description = DESC, .layer = NULL, .free_tier = 1 };                    \
  REGISTER_SOURCE(SYM)

/* On-demand OSINT service with an explicit run function. */
#define DEFR(SYM, ID, NAME, NAMEJA, RUN, CAT, TYPE, URL, DESC, FREE) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = RUN, .category = CAT, \
    .type = TYPE, .url = URL, .description = DESC, .layer = NULL, .free_tier = FREE }; \
  REGISTER_SOURCE(SYM)

/* On-demand scraped service; the file provides run_scrape(). */
#define DEF(SYM, ID, NAME, NAMEJA, CAT, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run_scrape, \
    .category = CAT, .type = "scraped", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

#endif /* JO_SOURCE_MACROS_INC */
