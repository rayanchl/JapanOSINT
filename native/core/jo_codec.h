/* core/jo_codec.h — the small codecs core/ had copied per module.
 *
 * Measured 2026-11-07: base64url encode/decode pairs in nine route modules
 * (cursor tokens) plus decode-only copies in exportapi.c and auth.c, the hex
 * digit helper nine times, the query-string URL decoder six times, three
 * query-string lookups in three shapes, and a SHA-256 hex digest five times.
 * The cursor decoders also built their reverse table lazily behind a plain
 * `static int init` flag — commented "single-threaded httpd loop" — while
 * those routes now run on worker threads: a data race on first use. The
 * decoder here uses no table at all, so there is nothing to initialise.
 *
 * Every function reproduces the copies it replaced byte for byte; where two
 * copies differed in BEHAVIOUR (not spelling), the difference is a flag and
 * the caller keeps its own (tests/unit/test_jo_codec.c pins each). */
#ifndef JO_CODEC_H
#define JO_CODEC_H

#include <stddef.h>

/* One hex digit's value, or -1. */
int jo_hexv(int c);

/* base64url (RFC 4648 §5 alphabet "-_"), NO padding — the cursor and JWT
 * spelling. malloc'd, NUL-terminated; NULL on allocation failure. */
char *jo_b64url_encode(const void *in, size_t len);

/* Convenience: encode a NUL-terminated string (its strlen bytes). */
char *jo_b64url_encode_str(const char *in);

/* Decode base64url. Output is malloc'd and always NUL-terminated (the byte
 * count, without the NUL, goes to *out_len when out_len is non-NULL).
 *
 *   flags 0                    STRICT: any byte outside the alphabet —
 *                              '=' padding included — fails (NULL). The
 *                              cursor decoders' behaviour: a tampered cursor
 *                              is refused, never half-read.
 *   JO_B64_SKIP_INVALID        bytes outside the alphabet are skipped
 *                              (padding, whitespace) — auth.c's JWT decoder.
 *
 * Trailing bits that do not make a whole byte are dropped in both modes. */
#define JO_B64_SKIP_INVALID 1
unsigned char *jo_b64url_decode(const char *in, size_t len, size_t *out_len,
                                int flags);

/* The cursor modules' string form: strict decode of a NUL-terminated string. */
char *jo_b64url_decode_str(const char *in);

/* STANDARD base64 (RFC 4648 §4 alphabet "+/", '=' padding) — the spelling
 * Node's Buffer.toString('base64') writes, RFC 2047 encoded-words use, and
 * operators paste into SECRETS_MASTER_KEY. Three private copies of it existed
 * (keysapi.c's master-key decoder, camera_store.c's discovery cursor,
 * alert_deliver.c's Subject encoder — measured 2026-11-12), each with its own
 * table; this is the one.
 *
 * Encode: malloc'd, NUL-terminated, always padded to a multiple of 4;
 * NULL on allocation failure. */
char *jo_b64_encode(const void *in, size_t len);

/* Decode, LENIENT — the behaviour all three copies shared: decoding stops at
 * the first '=' (padding ends the data) and any other byte outside the
 * alphabet is skipped (whitespace, a line-wrapped PEM-style value). Output is
 * malloc'd and always NUL-terminated; *out_len (nullable) gets the byte count
 * without the NUL. NULL only for a NULL input or allocation failure — a
 * malformed input decodes to whatever whole bytes it carried, possibly zero,
 * so a caller that needs a length (a 32-byte key) checks *out_len. */
unsigned char *jo_b64_decode(const char *in, size_t len, size_t *out_len);

/* application/x-www-form-urlencoded decode of s[0..n) into out (cap bytes,
 * always NUL-terminated when cap > 0): '+' -> ' ', %XX -> byte, a '%' that
 * is not followed by two hex digits is kept literally. Returns 1 when the
 * output filled before the input ended (truncated), else 0. */
int jo_url_decode(const char *s, size_t n, char *out, size_t cap);

/* key=value lookup in a raw query string ('&'-separated, no leading '?').
 * The FIRST segment whose name equals `key` exactly wins. Returns 1 when
 * present (value possibly empty), 0 when absent; out is always written
 * ("" when absent). A value longer than cap-1 is truncated, and
 * *truncated (nullable; set, never cleared) says so.
 *
 *   JO_QS_DECODE    form-decode the value (jo_url_decode rules); without it
 *                   the value is copied raw (alertsapi.c's qs_param).
 *   JO_QS_BARE_KEY  a segment with no '=' ("?flag") counts as present with
 *                   an empty value — the qget() copies' behaviour; without
 *                   it such a segment never matches (the qs_get() copies'). */
#define JO_QS_DECODE   1
#define JO_QS_BARE_KEY 2
int jo_qs_get(const char *qs, const char *key, char *out, size_t cap,
              int flags);
int jo_qs_get_tl(const char *qs, const char *key, char *out, size_t cap,
                 int flags, int *truncated);

/* The same lookup through mongoose's mg_http_get_var (its decoding, which
 * REFUSES a malformed %XX rather than keeping it). out is "" unless found.
 * Returns 1 when present and non-empty, 0 when absent or empty, -1 when the
 * value does not fit `cap` or does not decode; *too_long (nullable) is set
 * to 1 in the -1 case and never cleared. */
int jo_qs_get_mg(const char *qs, const char *key, char *out, size_t cap,
                 int *too_long);

/* Lower-case hex SHA-256 of b[0..n), NUL-terminated. */
void jo_sha256_hex(const void *b, size_t n, char out[65]);

#endif
