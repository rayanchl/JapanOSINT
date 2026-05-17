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

void fts_shutdown(void); /* free the cached tokenizer (optional) */

#endif
