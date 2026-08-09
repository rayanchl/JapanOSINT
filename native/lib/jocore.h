/* lib/jocore.h — the micro-helpers every collector rewrote for itself.
 *
 * A census of the collectors/sources tree found the same dozen-odd four-line
 * functions copied 300+ times under a dozen different names (`sv`, `jsv`,
 * `asv`, `sstr`, `jstr`; `numv`, `anum`, `numf`; `add_str`, `add_s`,
 * `add_num`, `add_n`; `uri_encode`, `urlenc`, `uric`, `url_encode`,
 * `url_encode_dup`; `round4`, `uid_tail`, `utf8_trunc`, `next_line`, …).
 * Every copy is a place the fleet can drift: the NULL-`valuestring` guard, the
 * `isalnum` locale trap and the UTF-8-safe truncation each existed in some
 * copies and not others. One implementation each, here.
 *
 * Header-only and `static inline` on purpose: collectors are 1,000 separate
 * translation units, most use one or two of these, and `static inline` in a
 * header neither emits an unused-function warning nor needs a .c file.
 *
 * NAMING: the canonical name always carries the `jo_` prefix, so a converted
 * collector reads unambiguously and a stray local copy is visible at a glance.
 *
 * BEHAVIOUR: each helper is the *superset-safe* variant of the copies it
 * replaces — same return value for every input the copies handled, plus the
 * guard the strictest copy had. Nothing here changes what a collector emits.
 */
#ifndef JO_CORE_H
#define JO_CORE_H

#include "../source.h"
#include "../third_party/cJSON.h"
#include "csv.h"            /* csv_decode_sjis() — the Shift_JIS leg of jo_get */
#include "feedlib.h"        /* feed_url_host_is_jp() — the gate on that leg   */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* ---------------------------------------------------------------- cJSON get */

/* Object field as a NON-EMPTY string, else NULL. The workhorse: `""` reads as
 * absent, which is what every collector wanted when it wrote `v->valuestring[0]`.
 * cJSON_GetObjectItem tolerates a NULL object, so `o` may be NULL. */
static inline const char *jo_sv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

/* Object field as a string, EMPTY STRING INCLUDED, else NULL. Distinct from
 * jo_sv on purpose: a handful of collectors distinguish "present but blank"
 * from "absent" (blank overwrites a stale value; absent leaves it). */
static inline const char *jo_str(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

/* An already-fetched value as a string, else NULL. */
static inline const char *jo_vstr(const cJSON *v) {
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

/* Object field as a number — STRICT: a numeric string is not a number.
 * Returns 1 and writes *out on success, 0 otherwise (*out untouched). */
static inline int jo_num(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v || !cJSON_IsNumber(v)) return 0;
  *out = v->valuedouble;
  return 1;
}

/* Object field as a number, COERCING a fully-numeric string ("12.5" → 12.5,
 * "12abc" → fail). Upstreams that quote their numbers need this; upstreams
 * that use "N/A" as a value must NOT get a partial parse, hence `*end == 0`. */
static inline int jo_numf(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return 0;
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *end = NULL;
    double d = strtod(v->valuestring, &end);
    if (end && end != v->valuestring && *end == '\0') { *out = d; return 1; }
  }
  return 0;
}

/* ---------------------------------------------------------------- cJSON put */

/* Add k=v only when v is a non-empty string. Keeps `properties` free of the
 * wall of JSON nulls the JS ports used to emit. */
static inline void jo_add_str(cJSON *o, const char *k, const char *v) {
  if (v && *v) cJSON_AddStringToObject(o, k, v);
}

/* Copy src[k] → dst[k] when it is a non-empty string. */
static inline void jo_copy_str(cJSON *dst, const cJSON *src, const char *k) {
  const char *s = jo_sv(src, k);
  if (s) cJSON_AddStringToObject(dst, k, s);
}

/* Copy src[k] → dst[k] when it is a number. */
static inline void jo_copy_num(cJSON *dst, const cJSON *src, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(src, k);
  if (cJSON_IsNumber(v)) cJSON_AddNumberToObject(dst, k, v->valuedouble);
}

/* First of `keys` (NULL-terminated) present as a non-empty string, else NULL.
 * The "try these field names in order" idiom every upstream-shape-tolerant
 * collector needs. */
static inline const char *jo_first_sv(const cJSON *o, const char *const *keys) {
  for (int i = 0; keys[i]; i++) {
    const char *v = jo_sv(o, keys[i]);
    if (v) return v;
  }
  return NULL;
}

/* Duplicate r[a], or r[b] if a is absent/null, or a JSON null. Returns a NEW
 * node the caller owns (attach it or delete it). */
static inline cJSON *jo_pick_dup(cJSON *r, const char *a, const char *b) {
  cJSON *v = cJSON_GetObjectItem(r, a);
  if (v && !cJSON_IsNull(v)) return cJSON_Duplicate(v, 1);
  v = cJSON_GetObjectItem(r, b);
  if (v && !cJSON_IsNull(v)) return cJSON_Duplicate(v, 1);
  return cJSON_CreateNull();
}

/* Copy r[ik] → p[k], or an explicit JSON null when absent. Unlike
 * jo_copy_str, these two keep the key present-but-null on purpose: they back
 * fixed-shape property blocks where a client reads by key. */
static inline void jo_put_or_null(cJSON *p, const char *k,
                                  cJSON *r, const char *ik) {
  cJSON *v = cJSON_GetObjectItem(r, ik);
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static inline void jo_put_str_or_null(cJSON *p, const char *outk,
                                      cJSON *r, const char *ink) {
  cJSON *v = cJSON_GetObjectItem(r, ink);
  if (v && cJSON_IsString(v) && v->valuestring[0])
    cJSON_AddStringToObject(p, outk, v->valuestring);
  else
    cJSON_AddItemToObject(p, outk, cJSON_CreateNull());
}

/* ------------------------------------------------------------------ numbers */

/* Round to 4 decimals (~11 m of latitude) — the fleet's coordinate precision. */
static inline double jo_round4(double v) { return floor(v * 1e4 + 0.5) / 1e4; }

/* An already-fetched value as a number, coercing a LEADING numeric prefix
 * ("12 km" → 12). Deliberately laxer than jo_numf, which requires the whole
 * string to be numeric — the copies this replaces read free-text measurement
 * fields where the unit follows the number. */
static inline int jo_num_of(cJSON *v, double *out) {
  if (!v) return 0;
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *e;
    double d = strtod(v->valuestring, &e);
    if (e != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}

/* "AS64512" / "64512" / " AS 64512" → 64512; 0 when it is not a plain ASN.
 * Trailing junk is rejected rather than truncated: an ASN is an identity, and
 * a partially parsed one silently attributes a prefix to the wrong network. */
static inline unsigned long jo_parse_asn(const char *s) {
  if (!s) return 0;
  while (*s == ' ') s++;
  if ((s[0] == 'A' || s[0] == 'a') && (s[1] == 'S' || s[1] == 's')) s += 2;
  if (!*s) return 0;
  char *end = NULL;
  unsigned long v = strtoul(s, &end, 10);
  if (!end || end == s) return 0;
  while (*end == ' ') end++;
  if (*end) return 0;
  if (v == 0 || v > 4294967295UL) return 0;
  return v;
}

/* Strict dotted-quad test (no CIDR, no leading zeros longer than 3 digits). */
static inline int jo_is_ipv4(const char *s) {
  int parts = 0;
  while (*s) {
    int v = 0, d = 0;
    while (*s >= '0' && *s <= '9') {
      v = v * 10 + (*s - '0'); s++; d++;
      if (v > 255) return 0;
    }
    if (!d || d > 3) return 0;
    parts++;
    if (*s == '.') { s++; if (!*s) return 0; }
    else if (*s) return 0;
  }
  return parts == 4;
}

/* "lat,lon" / "lat lon" / "lat;lon" → two finite, in-range doubles. Anything
 * trailing the pair is a parse failure, not something to ignore: a coordinate
 * read out of a malformed string is a fabricated pin. */
static inline int jo_parse_coord(const char *s, double *lat, double *lon) {
  if (!s) return 0;
  char *end = NULL;
  double a = strtod(s, &end);
  if (end == s) return 0;
  while (*end == ' ' || *end == ',' || *end == ';') end++;
  const char *p2 = end;
  double b = strtod(p2, &end);
  if (end == p2) return 0;
  while (*end == ' ') end++;
  if (*end) return 0;
  if (a < -90.0 || a > 90.0 || b < -180.0 || b > 180.0) return 0;
  *lat = a; *lon = b;
  return 1;
}

/* ------------------------------------------------------------------ strings */

/* Truncate to at most max_bytes WITHOUT splitting a UTF-8 sequence: back off
 * over any 10xxxxxx continuation byte first. Splitting mid-sequence is what
 * put ~1,000 rows of invalid UTF-8 into `summary` and its FTS mirror. */
static inline void jo_utf8_trunc(char *s, size_t max_bytes) {
  if (!s) return;
  if (strlen(s) <= max_bytes) return;
  while (max_bytes > 0 && ((unsigned char)s[max_bytes] & 0xC0) == 0x80) max_bytes--;
  s[max_bytes] = 0;
}

/* ASCII-lowercase `in` into `out` (always NUL-terminated). Byte-wise and
 * ASCII-only on purpose: it must not touch UTF-8 lead/continuation bytes. */
static inline void jo_lower_buf(const char *in, char *out, size_t n) {
  size_t i = 0;
  if (n == 0) return;
  for (; in && in[i] && i + 1 < n; i++) {
    unsigned char c = (unsigned char)in[i];
    out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
  }
  out[i] = 0;
}

/* Stable per-record uid tail: lowercased first 60 bytes of the url, falling
 * back to the name. Deliberately NOT a hash — the tail stays greppable. */
static inline void jo_uid_tail(const char *url, const char *name,
                               char *out, size_t outsz) {
  const char *src = (url && *url) ? url : (name ? name : "");
  size_t i = 0;
  for (; src[i] && i < 60 && i + 1 < outsz; i++) {
    unsigned char c = (unsigned char)src[i];
    out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
  }
  out[i] = 0;
}

/* Consume one line from *p in place: NUL-terminates it, advances *p past the
 * newline, strips a trailing CR/space/tab and leading space/tab. Returns the
 * line, or NULL at end of buffer. Destructive — the buffer must be writable. */
static inline char *jo_next_line(char **p) {
  char *s = *p;
  if (!s || !*s) return NULL;
  char *e = strchr(s, '\n');
  if (e) { *e = '\0'; *p = e + 1; }
  else   { *p = s + strlen(s); }
  size_t n = strlen(s);
  while (n && (s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = '\0';
  while (*s == ' ' || *s == '\t') s++;
  return s;
}

/* Same, but strips ONLY the CR. CSV and fixed-width feeds carry significant
 * leading/trailing whitespace inside a field, so trimming would corrupt them. */
static inline char *jo_next_line_cr(char **p) {
  char *s = *p;
  if (!s || !*s) return NULL;
  char *e = strchr(s, '\n');
  if (e) { *e = '\0'; *p = e + 1; }
  else   { *p = s + strlen(s); }
  size_t n = strlen(s);
  while (n && s[n - 1] == '\r') s[--n] = '\0';
  return s;
}

/* ASCII-lowercase one char. */
static inline char jo_lc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

/* Case-insensitive strstr (ASCII folding only, so UTF-8 bytes pass through
 * untouched). Not in the C standard; every collector that wanted it wrote it. */
static inline const char *jo_stristr(const char *h, const char *n) {
  if (!h || !n || !*n) return NULL;
  size_t nl = strlen(n);
  for (; *h; h++) {
    if (tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
      size_t k = 1;
      while (k < nl && h[k] && tolower((unsigned char)h[k]) == tolower((unsigned char)n[k])) k++;
      if (k == nl) return h;
    }
  }
  return NULL;
}

/* Cheap "does this start like an ISO date" test — YYYY- prefix. Used to decide
 * whether a published_at can be stored as-is or needs normalising. */
static inline int jo_looks_iso(const char *s) {
  return s && strlen(s) >= 8 &&
         isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1]) &&
         isdigit((unsigned char)s[2]) && isdigit((unsigned char)s[3]) &&
         s[4] == '-';
}

/* A plausible search keyword: 2–128 bytes, no control characters. The guard
 * on entity-pivot services so a stray control byte cannot reach a query. */
static inline int jo_looks_like_keyword(const char *s) {
  if (!s) return 0;
  size_t n = strlen(s);
  if (n < 2 || n > 128) return 0;
  for (const char *p = s; *p; p++)
    if ((unsigned char)*p < 0x20) return 0;
  return 1;
}

/* Index of `name` in a parsed CSV header row, or -1. */
static inline int jo_col_of(char **hdr, int nh, const char *name) {
  for (int i = 0; i < nh; i++)
    if (hdr[i] && strcmp(hdr[i], name) == 0) return i;
  return -1;
}

/* Field i of a parsed CSV row, or NULL when out of range, empty, or the "-"
 * that these feeds use for "no value". */
static inline const char *jo_field(char **f, int nf, int i) {
  if (i < 0 || i >= nf || !f[i] || !f[i][0] || strcmp(f[i], "-") == 0) return NULL;
  return f[i];
}

/* Resolve `href` against `base`: absolute stays, root-relative gets base's
 * scheme+host, anything else is appended to the host. Deliberately not a full
 * RFC-3986 resolver — the pages these scrapers read only produce those forms. */
static inline void jo_abs_url(const char *href, const char *base,
                              char *out, size_t n) {
  if (href && (strncmp(href, "http://", 7) == 0 || strncmp(href, "https://", 8) == 0)) {
    snprintf(out, n, "%s", href);
    return;
  }
  const char *p = base;
  int slashes = 0;
  while (*p) {
    if (*p == '/') { slashes++; if (slashes == 3) break; }
    p++;
  }
  size_t hostlen = (size_t)(p - base);
  if (href && href[0] == '/') snprintf(out, n, "%.*s%s", (int)hostlen, base, href);
  else snprintf(out, n, "%.*s/%s", (int)hostlen, base, href ? href : "");
}

/* -------------------------------------------------------------------- time */

/* "now" as ISO-8601 UTC with milliseconds — the shape core/intel.c stores. */
static inline void jo_iso_now(char *o, size_t n) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm tm;
  gmtime_r(&tv.tv_sec, &tm);
  char base[32];
  strftime(base, sizeof base, "%Y-%m-%dT%H:%M:%S", &tm);
  snprintf(o, n, "%s.%03dZ", base, (int)(tv.tv_usec / 1000));
}

/* `days` before now as a bare ISO date (UTC) — the `from=` half of every
 * windowed API query. On the (impossible in practice) gmtime failure it writes
 * "0000-00-00" rather than a plausible date: a wrong window is worse than an
 * obviously broken one. */
static inline void jo_days_ago_iso(int days, char *out, size_t n) {
  time_t t = time(NULL) - (time_t)days * 86400;
  struct tm tmv;
  int ok;
#if defined(_WIN32)
  ok = (gmtime_s(&tmv, &t) == 0);
#else
  ok = (gmtime_r(&t, &tmv) != NULL);
#endif
  if (!ok) { snprintf(out, n, "0000-00-00"); return; }
  strftime(out, n, "%Y-%m-%d", &tmv);
}

/* ---------------------------------------------------------------------- URL */

/* RFC-3986 %-encode into a malloc'd buffer (caller frees). Unreserved set is
 * the strict `A-Za-z0-9-_.~`. NULL input encodes as "". */
static inline char *jo_urlencode(const char *s) {
  if (!s) s = "";
  size_t n = strlen(s);
  char *out = (char *)malloc(n * 3 + 1);
  if (!out) return NULL;
  static const char hx[] = "0123456789ABCDEF";
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
      out[j++] = (char)c;
    else { out[j++] = '%'; out[j++] = hx[c >> 4]; out[j++] = hx[c & 15]; }
  }
  out[j] = 0;
  return out;
}

/* Same encoding into a caller-supplied buffer; truncates rather than overflows. */
static inline void jo_urlencode_buf(const char *s, char *out, size_t cap) {
  size_t o = 0;
  if (cap == 0) return;
  for (const char *p = s ? s : ""; *p && o + 4 < cap; p++) {
    unsigned char c = (unsigned char)*p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
      out[o++] = (char)c;
    else o += (size_t)snprintf(out + o, cap - o, "%%%02X", c);
  }
  out[o] = 0;
}

/* JS encodeURIComponent's set: unreserved PLUS `!~*'()`. Several upstreams
 * (ArcGIS, ODPT, some ckan instances) were ported from JS against exactly
 * this set, and widening it changes the request they send — kept separate
 * from jo_urlencode_buf for that reason. */
static inline void jo_uri_encode_buf(const char *in, char *out, size_t cap) {
  static const char keep[] = "-_.!~*'()";
  size_t w = 0;
  if (cap == 0) return;
  for (const unsigned char *p = (const unsigned char *)(in ? in : ""); *p && w + 4 < cap; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(keep, c))
      out[w++] = (char)c;
    else { snprintf(out + w, cap - w, "%%%02X", c); w += 3; }
  }
  out[w] = 0;
}

/* --------------------------------------------------------------- env / HTTP */

/* Non-empty env var value, or NULL. Key-gated sources use this to return an
 * honest empty instead of firing a credential-less request. */
static inline const char *jo_env(const char *name) {
  const char *v = getenv(name);
  return (v && *v) ? v : NULL;
}

/* Strict UTF-8 validation. The implementation lives next to the transcoder it
 * guards (lib/csv.c) so feedlib and the collectors share one copy; the jo_
 * name is kept because 134 collectors already call it. */
static inline int jo_is_utf8(const char *s, size_t n) {
  return csv_is_utf8(s, n);
}

/* GET → malloc'd NUL-terminated body on HTTP 200 (caller frees), else NULL.
 * Logs the status under [tag]. headers may be NULL.
 *
 * Several Japanese government sites still serve Shift_JIS with no charset
 * header (customs.go.jp, soumu.go.jp). Passing those bytes through unchanged
 * persisted invalid UTF-8 into TEXT columns and their FTS mirror, which then
 * fails to decode on read. Validate; transcode only when the body is NOT
 * already valid UTF-8, so legitimate UTF-8 pages are untouched.
 *
 * AND ONLY ON A .jp HOST — the same gate feed_get_text() carries. Without it
 * this fires on any non-UTF-8 body, and Latin-1 is not UTF-8: verified with
 * GNU iconv, the ISO-8859-1 bytes of "Vær … CaféBar" decode as VALID
 * Shift_JIS and come back as "V誡 … Caf顳ar". csv_decode_sjis fails closed on
 * an undecodable body, so most European pages survived by luck; the ones
 * whose accented bytes happen to form legal SJIS pairs were silently
 * mojibake'd into intel_items and the FTS mirror. The majority of the 133
 * jo_get callers are non-Japanese (govdata.de, opendata.swiss,
 * api-adresse.data.gouv.fr, dataforsyningen.dk …), which is precisely the
 * population feed_get_text's gate was written to protect. */
static inline char *jo_get(const source_ctx *ctx, const char *url,
                           const char *const *headers, const char *tag) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, headers, NULL, 0, 20000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[%s] http status=%ld\n", tag, hr.status);
    http_response_free(&hr);
    return NULL;
  }
  char *body = hr.body;
  size_t blen = hr.body_len;
  hr.body = NULL;                 /* steal; prevent free below */
  http_response_free(&hr);
  if (body && blen && feed_url_host_is_jp(url) && !jo_is_utf8(body, blen)) {
    char *conv = csv_decode_sjis(body, blen);
    if (conv) { free(body); return conv; }
  }
  return body;
}

#endif /* JO_CORE_H */
