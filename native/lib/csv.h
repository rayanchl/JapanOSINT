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

/* Same again, with a comment prefix — and it strips the banner BEFORE the
 * parse, not after it, because with headers!=0 the parser takes line 0 as the
 * column names and a banner puts a comment there.
 *
 * NOAA GML is the fleet's case: sf6_mm_gl.csv is 393 lines of which the first
 * 45 are a `#` licence notice, so `# ----------------------------` became the
 * header, all 347 monthly observations were keyed on nonsense, and the source
 * emitted zero on every run while fetching perfectly. Six registered sources
 * come off that one directory.
 *
 * Only the LEADING contiguous run of comment lines is removed, so a `#` that
 * appears in a data cell later in the file is never mistaken for a comment —
 * data does not precede the header.
 *
 * If the LAST line of that banner splits into exactly as many cells as the
 * first data row, it IS the header and is kept as one: URLhaus publishes its
 * column names as `# id,dateadded,url,url_status,…`, and dropping it would
 * have promoted the first real record into the header's place, losing that
 * record and naming every column after its values.
 *
 * `comment` NULL or empty behaves exactly like csv_parse_d. */
cJSON *csv_parse_dc(const char *text, int headers, char delim,
                    const char *comment);

/* The general form the hpengine CSV mode uses. `delim` is a named or literal
 * separator string rather than one character:
 *   NULL / ""   comma (RFC 4180, byte-exact cells)
 *   "x"         any single character, exactly as csv_parse_d
 *   "ws"        a RUN of blanks is one separator — fixed-width text tables
 *               (JPNIC's as-numbers.txt is `2497      IIJ          JP00006327`),
 *               with leading alignment dropped and ruler lines (`-----`) skipped
 *   "<>"        any longer literal, for the 2ch-family subject.txt whose cells
 *               are separated by the two characters `<>`
 * `skip_lines` drops that many PHYSICAL lines before anything is parsed — the
 * "title line above the header" case (MEXT, Kawasaki, Saitama), which used to
 * force csv_no_header=1 and emit the title and the header as two junk records.
 * The skip is physical on purpose: the header and the records after it are
 * still parsed with full quoting, so a quoted line break inside a header cell
 * stays one cell. `comment` is as csv_parse_dc and is applied after the skip. */
cJSON *csv_parse_x(const char *text, int headers, const char *delim,
                   int skip_lines, const char *comment);

/* Shift_JIS → UTF-8, malloc'd NUL-terminated (caller frees). On iconv error
 * returns a plain UTF-8 copy of the input (mirrors JS catch → utf8). */
char *csv_decode_sjis(const char *buf, size_t len);
/* Same, for any iconv source encoding ("SHIFT_JIS", "EUC-JP", ...).
 * Fails closed: an undecodable body comes back as a verbatim copy. */
char *csv_decode_charset(const char *buf, size_t len, const char *from);

/* Strict UTF-8 validation. The cheap way to tell "already UTF-8" from "legacy
 * Japanese encoding" when the response carries no charset header — which is
 * the only signal csv_decode_sjis's callers have. Lives here rather than in
 * three collectors because it is the guard that makes the transcode safe. */
int csv_is_utf8(const char *s, size_t n);

#endif
