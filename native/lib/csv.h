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

/* Same, with an explicit field delimiter. Not every "CSV" feed uses a comma:
 * DataPlane.org publishes `ASN | AS name | ip | lastseen | category`, and
 * parsing it on commas made each whole line a single cell — so the IP, the
 * timestamp and the category were present in the store as one unqueryable blob.
 *
 * When `delim` is not ',' each UNQUOTED cell is also whitespace-trimmed. Those
 * feeds pad their columns to align them for a human reader, so the padding is
 * layout rather than content; comma CSV is left byte-exact as RFC 4180 expects,
 * which is why this is not simply applied to every parse. */
cJSON *csv_parse_d(const char *text, int headers, char delim);

/* Shift_JIS → UTF-8, malloc'd NUL-terminated (caller frees). On iconv error
 * returns a plain UTF-8 copy of the input (mirrors JS catch → utf8). */
char *csv_decode_sjis(const char *buf, size_t len);

/* Strict UTF-8 validation. The cheap way to tell "already UTF-8" from "legacy
 * Japanese encoding" when the response carries no charset header — which is
 * the only signal csv_decode_sjis's callers have. Lives here rather than in
 * three collectors because it is the guard that makes the transcode safe. */
int csv_is_utf8(const char *s, size_t n);

#endif
