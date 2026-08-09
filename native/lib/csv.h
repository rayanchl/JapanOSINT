/* lib/csv.h — port of _liveHelpers.js parseCsv + decodeShiftJis. Backs the
 * ~7 CSV collectors (NPA crime/fraud, grid CSVs, BGP feeds, SSL blocklist).
 * Each becomes a small source.c: fetch text (feed_get_text / decode SJIS) →
 * csv_parse → build features/intel → emit. */
#ifndef JO_CSV_H
#define JO_CSV_H
#include "../third_party/cJSON.h"
#include <stddef.h>

/* parseCsv: quoted fields w/ embedded commas and "" escapes. Byte-oriented
 * (safe for UTF-8: delimiters are ASCII, never collide with UTF-8 bytes).
 *  headers==0 → cJSON array of rows, each an array of string cells.
 *  headers!=0 → cJSON array of objects keyed by trimmed header names; a
 *               missing trailing cell yields "" (JS `r[i] ?? ''`).
 * Returns a new cJSON (caller cJSON_Delete) or an empty array; never NULL. */
cJSON *csv_parse(const char *text, int headers);

/* Shift_JIS → UTF-8, malloc'd NUL-terminated (caller frees). On iconv error
 * returns a plain UTF-8 copy of the input (mirrors JS catch → utf8). */
char *csv_decode_sjis(const char *buf, size_t len);

/* Strict UTF-8 validation. The cheap way to tell "already UTF-8" from "legacy
 * Japanese encoding" when the response carries no charset header — which is
 * the only signal csv_decode_sjis's callers have. Lives here rather than in
 * three collectors because it is the guard that makes the transcode safe. */
int csv_is_utf8(const char *s, size_t n);

#endif
