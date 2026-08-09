/* core/fts.h — Japanese segmentation for FTS5 (MeCab replacement for the
 * JS Kuromoji path in jpTokenizer.js). Exact contract parity:
 *   - no Japanese char  -> return input unchanged (Latin passthrough)
 *   - else              -> morpheme SURFACE forms, space-joined, no trailing
 *                          space (== kuromoji tokenize().map(surface).join(' '))
 * MeCab-IPADIC and kuromoji (also IPADIC) produce identical JP boundaries;
 * verified in P2. Lazy global tokenizer, mutex-guarded. */
#ifndef JO_FTS_H
#define JO_FTS_H

/* 1 if s contains a hiragana/katakana/CJK/CJK- extA/halfwidth-katakana char
 * (replicates jpTokenizer.js JP_RE). */
int fts_has_japanese(const char *s);

/* Returns a malloc'd segmented string (caller frees), or a strdup of the
 * input on passthrough / any MeCab failure (fail-open, like the JS path). */
char *fts_segment(const char *text);

/* Turn raw end-user input into a safe FTS5 MATCH expression.
 *
 * WHY. fts_segment() is a TOKENIZER, not a query builder, and Latin input goes
 * through it untouched — so binding its output straight into `MATCH ?` fed the
 * user's raw keystrokes to the FTS5 expression parser. Two consequences, both
 * user-visible:
 *   - operators fired by accident: `cam-tabi` parses as `cam NOT tabi`, so a
 *     hyphenated query returned the opposite of what was asked for;
 *   - a lone `"` or `(` — ordinary things to type mid-word — made
 *     sqlite3_prepare_v2() fail, which the API surfaced as a 500/empty body
 *     rather than "no results".
 *
 * WHAT IT DOES. Segments (MeCab for Japanese, passthrough for Latin), splits
 * on whitespace, drops tokens with no indexable character, wraps each in a
 * double-quoted FTS5 string (embedded `"` doubled) so every remaining byte is
 * a literal, and joins with an explicit AND. The LAST token additionally gets
 * a `*` when it is 2+ characters, which is what makes search-as-you-type work:
 * without it `shibu` could never match `Shibuya`, no matter which columns are
 * indexed. At most FTS_QUERY_MAX_TOKENS tokens are used.
 *
 * COST: column filters (`title:tokyo`) and hand-written boolean/NEAR syntax
 * become literal text on the surfaces that call this. That is the intended
 * trade for a search box. Callers whose `q` is an operator-authored predicate
 * rather than typed input — alert_eval's rule `q` — must NOT use this.
 *
 * Returns malloc'd (caller frees), or NULL when the input yields no usable
 * token; NULL means "no text filter", not "match nothing". */
#define FTS_QUERY_MAX_TOKENS 24
char *fts_query_expr(const char *raw);

#endif
