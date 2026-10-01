/* lib/jpnorm.h — Japanese text normalisation for the write path AND the query
 * path, so the same surface reaches MeCab and FTS5 from both sides.
 *
 * WHY. The corpus spells one thing many ways: 「ＮＴＴドコモ㈱」, 「NTTドコモ
 * 株式会社」, 「ｴﾇﾃｨｰﾃｨｰ ﾄﾞｺﾓ」. FTS5's unicode61 tokenizer folds none of
 * that (it is Latin-aware, not kana-aware), and MeCab segments the full-width
 * and half-width spellings differently, so a document written one way was not
 * found by a query typed another way. This module is the one place that
 * decides what "the same text" means; fts_segment() and es_norm_key() both go
 * through it, which is what makes write and query symmetric.
 *
 * Pure C tables — no ICU, no locale, no allocation on the fixed-buffer path.
 * Idempotent: fold(fold(x)) == fold(x), and the unit test checks it.
 *
 * ── WHAT IS FOLDED (in this order) ──────────────────────────────────────────
 *
 *  1. Compatibility folding (the NFKC subset that matters for this corpus):
 *     - full-width ASCII U+FF01–FF5E → half-width (Ａ→A, １→1, （→(, ～→~)
 *     - ideographic space U+3000, NBSP, U+2000–200A, U+202F, U+205F → ' '
 *     - zero-width U+200B–200D and BOM U+FEFF → dropped
 *     - half-width katakana U+FF61–FF9F → full-width katakana, with a
 *       trailing ﾞ/ﾟ COMPOSED onto the preceding kana (ｶﾞ → ガ, ﾊﾟ → パ,
 *       ｳﾞ → ヴ); combining U+3099/309A and spacing U+309B/309C likewise
 *     - ｰ (U+FF70) → ー (U+30FC); the long-vowel mark itself is KEPT (it is
 *       part of the reading — see jpnorm_hepburn, which drops it)
 *     - wave dashes U+301C/U+2053 → '~';  dashes U+2010–2015, U+2212 → '-'
 *     - ㈱ → 株式会社, ㈲ → 有限会社, ㈳ → 社団法人, ㈴ → 合名会社,
 *       ㈶ → 財団法人, ㍿ → 株式会社, ㊤㊥㊦㊧㊨ → 上中下左右
 *     - № → "no", ℡ → "tel"
 *     - circled digits ①–⑳ → 1–20, ⓪ → 0, ⑴–⒇ → (1)–(20), ⒈–⒛ → 1.–20.
 *     - roman numerals Ⅰ–Ⅻ / ⅰ–ⅻ → i–xii, Ⅼ Ⅽ Ⅾ Ⅿ → l c d m
 *  2. Katakana → hiragana: U+30A1–30F6 → U+3041–3096 (ヴ→ゔ, ヵヶ→ゕゖ).
 *     ヷヸヹヺ (U+30F7–30FA) have no hiragana form and are left alone.
 *  3. Latin case fold: ASCII A–Z, Latin-1 À–Þ (except ×), and the paired
 *     upper/lower letters of Latin Extended-A (Ā–ž, Ÿ) → lower case.
 *  4. Whitespace: every run of the characters folded to ' ' above plus ASCII
 *     \t\n\r\f\v collapses to ONE space; leading/trailing space is trimmed.
 *
 * ── WHAT IS NOT FOLDED ──────────────────────────────────────────────────────
 *
 *  - Kanji variants (旧字体/新字体: 澤↔沢, 國↔国, 髙↔高, CJK compatibility
 *    ideographs U+F900–FAFF). A variant is a different spelling of the same
 *    name and collapsing it is a real gain — but the tables are large, the
 *    pairs are not always one-to-one, and getting one wrong merges two
 *    different people. That is the LLM resolver's job (entity_merges), not a
 *    byte-level fold's.
 *  - Small kana vs large kana (ッ vs ツ, ャ vs ヤ): they are different sounds.
 *  - Katakana middle dot ・ (U+30FB) and CJK punctuation 、。「」: kept.
 *  - Greek, Cyrillic, Hangul, Arabic: passed through unchanged.
 *  - Diacritics on Latin (é→e): left to FTS5's remove_diacritics.
 *
 * Buffer sizing: an output can be LONGER than its input (㈱ is 3 bytes and
 * becomes 12). JPNORM_OUT_CAP(inlen) is a safe capacity; jpnorm_fold_dup()
 * allocates it for you.
 */
#ifndef JO_JPNORM_H
#define JO_JPNORM_H
#include <stddef.h>

/* Worst case is 4x (㈱ → 株式会社), plus the terminator. */
#define JPNORM_OUT_CAP(inlen) ((inlen) * 4 + 1)

/* Fold everything listed above (stages 1–4). Writes at most n-1 bytes plus a
 * NUL, never splits a UTF-8 sequence, and returns the number of bytes
 * written (excluding the NUL). n==0 writes nothing and returns 0. */
size_t jpnorm_fold(const char *in, char *out, size_t n);

/* Stages 1, 3 and 4 only — the compatibility fold WITHOUT katakana→hiragana.
 * This is what the reading pipeline runs MeCab on: IPADIC has katakana
 * entries (ドコモ), not hiragana ones, so folding kana before asking for a
 * reading would lose the dictionary hit. Idempotent, and jpnorm_fold() of its
 * output equals jpnorm_fold() of the input. */
size_t jpnorm_compat(const char *in, char *out, size_t n);

/* malloc'd jpnorm_fold()/jpnorm_compat() of `in` (NULL in → ""). Never
 * returns NULL except on allocation failure. Caller frees. */
char *jpnorm_fold_dup(const char *in);
char *jpnorm_compat_dup(const char *in);

/* Hiragana (or katakana — it is folded first) → Hepburn romaji, ONE canonical
 * form so that a typed query and a stored reading meet:
 *   - macron-less: とうきょう → "tokyo", おおさか → "osaka", ゆうき → "yuki".
 *     The long vowels ou/oo/uu/aa/ee collapse to o/o/u/a/e; ei and ii are
 *     kept (Hepburn writes them out: "sensei", "niigata"). ー is dropped.
 *   - し shi, ち chi, つ tsu, ふ fu, じ/ぢ ji, づ zu, を o, ゐ i, ゑ e, ゔ vu;
 *     しゃ sha, ちゃ cha, じゃ ja, きゃ kya; ふぁ fa, てぃ ti, うぃ wi.
 *   - ん is always plain "n" (no apostrophe, no m before b/p/m): "shinbun",
 *     "kenichi". Users type it that way.
 *   - っ doubles the next consonant; before ch it is written t ("matcha").
 *   - lower case throughout; spaces are preserved as token separators;
 *     anything that is not kana (kanji MeCab could not read, Latin, digits)
 *     passes through unchanged so nothing is silently lost.
 * Returns bytes written (excluding NUL). n bytes of `out` is enough when
 * n >= JPNORM_OUT_CAP(strlen(in)) (worst case: っちゃ → "tcha"). */
size_t jpnorm_hepburn(const char *kana, char *out, size_t n);
char  *jpnorm_hepburn_dup(const char *kana);

/* 1 if s contains hiragana, katakana, half-width katakana, or a CJK
 * ideograph (URI/ExtA/compat) — the set for which readings are meaningful. */
int jpnorm_has_cjk(const char *s);

#endif
