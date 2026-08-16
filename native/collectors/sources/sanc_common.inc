/* sanc_common.inc — shared static helpers for the sanctions / legal / court
 * collector batch (every file in that batch is named collectors/sources/sanc_*.c).
 *
 * Included, never compiled standalone (the Makefile globs *.c, not *.inc —
 * same trick as _jp_osint.inc / username_osint_platforms.inc). EVERY helper
 * here is `static inline` so a translation unit that uses only two of them
 * compiles clean under -Wall -Wextra (a plain `static` helper left unused
 * would trip -Wunused-function).
 *
 * House rules honoured throughout (SOURCE_AUTHORING_CONTRACT.md):
 *   R1 nothing here invents a record: every emitted value is bytes that came
 *      back from the network.
 *   R2 no helper ever sets has_geo/lat/lon. This whole domain is geometry-free:
 *      a designated person is not "at" their country's centroid.
 *   R3 fetch failure -> the caller returns -1; a successful fetch that yields
 *      zero records is a CLEAN result and the caller returns 0.
 *   R8 every malloc/cJSON here is freed on every path.
 */
#ifndef JO_SANC_COMMON_INC
#define JO_SANC_COMMON_INC

#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "_jp_osint.inc"   /* jo_sv / jo_env / jo_urlencode / jo_is_utf8 */

/* A descriptive UA with a contact route: several of these publishers (GOV.UK
 * Search, PACER, HUDOC) block bare/absent user agents outright. */
#define SANC_UA "User-Agent: JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; " \
                "sanctions/legal collector; contact via repo issues)"

/* The attribution string the OpenSanctions bulk exports REQUIRE. See the long
 * warning on sanc_opensanctions_run() below — CC-BY-NC means NON-COMMERCIAL. */
#define SANC_OS_LICENSE \
  "Data: OpenSanctions (https://www.opensanctions.org) — CC-BY-NC 4.0. " \
  "ATTRIBUTION REQUIRED and NON-COMMERCIAL USE ONLY: a commercial deployment " \
  "of this platform needs a paid OpenSanctions licence before this source may " \
  "be shipped."

/* ------------------------------------------------------------------ env --- */

static inline int sanc_env_int(const char *name, int dflt) {
  const char *v = getenv(name);
  if (!v || !*v) return dflt;
  int n = atoi(v);
  return n > 0 ? n : dflt;
}

/* ----------------------------------------------------------------- http --- */

/* GET url. `accept`/`range`/`auth` are complete header lines ("Accept: …") or
 * NULL. Returns a malloc'd NUL-terminated body on 200/206 (caller frees), or
 * NULL — in which case the caller must treat it as a FETCH FAILURE (-1).
 * *out_len gets the byte length, *out_status the HTTP status (206 == the
 * server honoured our Range and the body is a prefix/suffix of the file). */
static inline char *sanc_http_get(const source_ctx *ctx, const char *url,
                                  const char *accept, const char *range,
                                  const char *auth, int timeout_ms,
                                  const char *tag, size_t *out_len,
                                  long *out_status) {
  const char *hdrs[6];
  int h = 0;
  hdrs[h++] = SANC_UA;
  if (accept) hdrs[h++] = accept;
  if (range) {
    /* A byte range over a gzip stream is a range over the COMPRESSED bytes and
     * decodes to garbage, so ask for identity whenever we range-request. */
    hdrs[h++] = "Accept-Encoding: identity";
    hdrs[h++] = range;
  }
  if (auth) hdrs[h++] = auth;
  hdrs[h] = NULL;

  http_response r = {0};
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0,
                        timeout_ms > 0 ? timeout_ms : 30000, 1, &r);
  if (out_status) *out_status = r.status;
  if (rc != 0 || !r.body || (r.status != 200 && r.status != 206)) {
    fprintf(stderr, "[%s] fetch failed rc=%d status=%ld\n", tag, rc, r.status);
    http_response_free(&r);
    return NULL;
  }
  char *body = r.body;
  if (out_len) *out_len = r.body_len;
  r.body = NULL;                       /* steal */
  http_response_free(&r);
  return body;
}

/* GET + cJSON_Parse. NULL on fetch OR parse failure (caller -> -1). */
static inline cJSON *sanc_http_json(const source_ctx *ctx, const char *url,
                                    const char *auth, int timeout_ms,
                                    const char *tag) {
  size_t len = 0;
  char *b = sanc_http_get(ctx, url, "Accept: application/json, */*", NULL, auth,
                          timeout_ms, tag, &len, NULL);
  if (!b) return NULL;
  cJSON *d = cJSON_Parse(b);
  free(b);
  if (!d) fprintf(stderr, "[%s] response was not JSON\n", tag);
  return d;
}

/* Latin-1 -> UTF-8 when the body is not already valid UTF-8 (PACER declares
 * ISO-8859-1). Frees `body` and returns a new buffer, or returns `body`. */
static inline char *sanc_to_utf8(char *body, size_t len, size_t *out_len) {
  if (!body || !len || jo_is_utf8(body, len)) return body;
  char *o = (char *)malloc(len * 2 + 1);
  if (!o) return body;
  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)body[i];
    if (c < 0x80) o[j++] = (char)c;
    else { o[j++] = (char)(0xC0 | (c >> 6)); o[j++] = (char)(0x80 | (c & 0x3F)); }
  }
  o[j] = 0;
  free(body);
  if (out_len) *out_len = j;
  return o;
}

/* Skip a UTF-8 BOM (federalreserve.gov ships one before the XML declaration). */
static inline const char *sanc_skip_bom(const char *s) {
  if (s && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB &&
      (unsigned char)s[2] == 0xBF) return s + 3;
  return s;
}

/* ----------------------------------------------------------- text utils --- */

static inline int sanc_ncaseeq(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
  return 1;
}

/* Append codepoint cp as UTF-8 at o; returns bytes written. */
static inline size_t sanc_utf8_put(char *o, unsigned cp) {
  if (cp < 0x80) { o[0] = (char)cp; return 1; }
  if (cp < 0x800) { o[0] = (char)(0xC0 | (cp >> 6));
                    o[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
  if (cp < 0x10000) { o[0] = (char)(0xE0 | (cp >> 12));
                      o[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                      o[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
  o[0] = (char)(0xF0 | (cp >> 18));
  o[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  o[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
  o[3] = (char)(0x80 | (cp & 0x3F));
  return 4;
}

/* Trim ASCII whitespace in place (leading via memmove). */
static inline void sanc_trim(char *s) {
  if (!s) return;
  size_t n = strlen(s);
  while (n && (unsigned char)s[n - 1] <= ' ') s[--n] = 0;
  size_t i = 0;
  while (s[i] && (unsigned char)s[i] <= ' ') i++;
  if (i) memmove(s, s + i, n - i + 1);
}

/* Collapse every run of ASCII whitespace to a single space, then trim. The SCC
 * feed pretty-prints its <title> across three lines. */
static inline void sanc_collapse_ws(char *s) {
  if (!s) return;
  size_t j = 0;
  int sp = 0;
  for (size_t i = 0; s[i]; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c <= ' ') { sp = 1; continue; }
    if (sp && j) s[j++] = ' ';
    sp = 0;
    s[j++] = (char)c;
  }
  s[j] = 0;
  sanc_trim(s);
}

/* CDATA-strip + XML entity decode of [s,s+n). malloc'd; caller frees. */
static inline char *sanc_decode(const char *s, size_t n) {
  if (!s) return NULL;
  while (n && (unsigned char)*s <= ' ') { s++; n--; }
  while (n && (unsigned char)s[n - 1] <= ' ') n--;
  if (n >= 12 && strncmp(s, "<![CDATA[", 9) == 0) {
    const char *e = NULL;
    for (size_t i = 9; i + 3 <= n; i++)
      if (s[i] == ']' && s[i + 1] == ']' && s[i + 2] == '>') { e = s + i; break; }
    if (e) { s += 9; n = (size_t)(e - s); }
  }
  char *o = (char *)malloc(n + 1);
  if (!o) return NULL;
  size_t j = 0;
  for (size_t i = 0; i < n; ) {
    if (s[i] != '&') { o[j++] = s[i++]; continue; }
    /* find the ';' within a sane window */
    size_t k = i + 1, lim = (i + 12 < n) ? i + 12 : n;
    while (k < lim && s[k] != ';') k++;
    if (k >= lim || s[k] != ';') { o[j++] = s[i++]; continue; }
    size_t elen = k - i - 1;
    const char *e = s + i + 1;
    if (elen == 3 && sanc_ncaseeq(e, "amp", 3)) o[j++] = '&';
    else if (elen == 2 && sanc_ncaseeq(e, "lt", 2)) o[j++] = '<';
    else if (elen == 2 && sanc_ncaseeq(e, "gt", 2)) o[j++] = '>';
    else if (elen == 4 && sanc_ncaseeq(e, "quot", 4)) o[j++] = '"';
    else if (elen == 4 && sanc_ncaseeq(e, "apos", 4)) o[j++] = '\'';
    else if (elen == 4 && sanc_ncaseeq(e, "nbsp", 4)) o[j++] = ' ';
    else if (elen >= 2 && e[0] == '#') {
      unsigned cp = 0;
      if (e[1] == 'x' || e[1] == 'X') {
        for (size_t m = 2; m < elen; m++) {
          char c = e[m];
          cp = cp * 16 + (unsigned)(isdigit((unsigned char)c) ? c - '0'
                                    : (tolower((unsigned char)c) - 'a' + 10));
        }
      } else {
        for (size_t m = 1; m < elen; m++) cp = cp * 10 + (unsigned)(e[m] - '0');
      }
      if (cp && cp <= 0x10FFFF) j += sanc_utf8_put(o + j, cp);
    } else { o[j++] = s[i++]; continue; }
    i = k + 1;
  }
  o[j] = 0;
  return o;
}

/* Copy src into dst (size dstsz) truncating on a UTF-8 CHARACTER boundary, so a
 * multi-byte sequence is never cut in half and persisted as invalid text. */
static inline void sanc_trunc(char *dst, size_t dstsz, const char *src) {
  if (!dst || !dstsz) return;
  dst[0] = 0;
  if (!src) return;
  size_t L = strlen(src);
  if (L > dstsz - 1) {
    L = dstsz - 1;
    while (L > 0 && ((unsigned char)src[L] & 0xC0) == 0x80) L--;
  }
  memcpy(dst, src, L);
  dst[L] = 0;
}

/* ------------------------------------------------------------------ xml --- */

typedef struct {
  const char *tag_start, *tag_end;   /* '<' … '>' of the OPEN tag            */
  const char *body, *body_end;       /* inner content (empty when self-closed)*/
} sanc_el;

/* Locate the next opening tag named `name` (namespace-agnostic, case-
 * insensitive, requires a real delimiter after the name so <Names> never
 * matches "Name"). Returns the '<' or NULL. */
static inline const char *sanc_xml_open(const char *p, const char *end,
                                        const char *name) {
  size_t nl = strlen(name);
  while (p && p < end) {
    const char *lt = (const char *)memchr(p, '<', (size_t)(end - p));
    if (!lt) return NULL;
    const char *q = lt + 1;
    if (q >= end || *q == '/' || *q == '!' || *q == '?') { p = lt + 1; continue; }
    const char *ne = q;
    while (ne < end && *ne != ' ' && *ne != '>' && *ne != '/' && *ne != '\t' &&
           *ne != '\n' && *ne != '\r') ne++;
    const char *base = q;
    for (const char *c = q; c < ne; c++) if (*c == ':') base = c + 1;
    if ((size_t)(ne - base) == nl && sanc_ncaseeq(base, name, nl)) return lt;
    p = lt + 1;
  }
  return NULL;
}

/* Iterate elements named `name` in [*cur,end). Fills `el` and advances *cur
 * past the element. Returns 1 while elements remain, 0 at the end (which also
 * happens when the last element is cut in half by a ranged download — we stop
 * rather than emit a half-parsed record). */
static inline int sanc_xml_next(const char **cur, const char *end,
                                const char *name, sanc_el *el) {
  const char *lt = sanc_xml_open(*cur, end, name);
  if (!lt) { *cur = end; return 0; }
  const char *gt = (const char *)memchr(lt, '>', (size_t)(end - lt));
  if (!gt) { *cur = end; return 0; }
  el->tag_start = lt;
  el->tag_end = gt;
  if (gt > lt && gt[-1] == '/') {           /* <Foo … /> — empty element */
    el->body = el->body_end = gt + 1;
    *cur = gt + 1;
    return 1;
  }
  size_t nl = strlen(name);
  const char *p = gt + 1;
  while (p < end) {
    const char *c = (const char *)memchr(p, '<', (size_t)(end - p));
    if (!c) break;
    if (c + 1 < end && c[1] == '/') {
      const char *q = c + 2, *ne = q;
      while (ne < end && *ne != '>' && *ne != ' ') ne++;
      const char *base = q;
      for (const char *k = q; k < ne; k++) if (*k == ':') base = k + 1;
      if ((size_t)(ne - base) == nl && sanc_ncaseeq(base, name, nl)) {
        const char *ce = (const char *)memchr(ne, '>', (size_t)(end - ne));
        if (!ce) break;
        el->body = gt + 1;
        el->body_end = c;
        *cur = ce + 1;
        return 1;
      }
    }
    p = c + 1;
  }
  *cur = end;
  return 0;
}

/* Inner text of the FIRST descendant element `name` in [b,e), decoded and
 * trimmed. malloc'd or NULL when the element is absent or empty. */
static inline char *sanc_xml_text(const char *b, const char *e,
                                  const char *name) {
  const char *cur = b;
  sanc_el el;
  if (!sanc_xml_next(&cur, e, name, &el)) return NULL;
  char *s = sanc_decode(el.body, (size_t)(el.body_end - el.body));
  if (s && !s[0]) { free(s); return NULL; }
  return s;
}

/* Attribute value from an element's open tag. malloc'd (decoded) or NULL. */
static inline char *sanc_xml_attr(const sanc_el *el, const char *attr) {
  size_t al = strlen(attr);
  for (const char *p = el->tag_start; p + al + 2 < el->tag_end; p++) {
    if (!sanc_ncaseeq(p, attr, al)) continue;
    if (p > el->tag_start && (isalnum((unsigned char)p[-1]) || p[-1] == '-' ||
                              p[-1] == ':' || p[-1] == '_')) continue;
    const char *q = p + al;
    while (q < el->tag_end && (*q == ' ' || *q == '\t')) q++;
    if (q >= el->tag_end || *q != '=') continue;
    q++;
    while (q < el->tag_end && (*q == ' ' || *q == '\t')) q++;
    if (q >= el->tag_end || (*q != '"' && *q != '\'')) continue;
    char quote = *q++;
    const char *v = q;
    while (q < el->tag_end && *q != quote) q++;
    return sanc_decode(v, (size_t)(q - v));
  }
  return NULL;
}

/* ----------------------------------------------------------------- dates --- */

/* "31/07/2026" -> "2026-07-31" (UK/FCDO). Returns 1 on success. */
static inline int sanc_ddmmyyyy(const char *in, char *out, size_t n) {
  if (!in || n < 11) return 0;
  int d, m, y;
  if (sscanf(in, "%d/%d/%d", &d, &m, &y) != 3) return 0;
  if (y < 1000 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
  char tmp[32];
  int L = snprintf(tmp, sizeof tmp, "%04d-%02d-%02d", y, m, d);
  if (L < 0 || (size_t)L >= n) return 0;
  memcpy(out, tmp, (size_t)L + 1);
  return 1;
}

/* "07/27/2026" -> "2026-07-27" (OFAC publish date). */
static inline int sanc_mmddyyyy(const char *in, char *out, size_t n) {
  if (!in || n < 11) return 0;
  int d, m, y;
  if (sscanf(in, "%d/%d/%d", &m, &d, &y) != 3) return 0;
  if (y < 1000 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
  char tmp[32];
  int L = snprintf(tmp, sizeof tmp, "%04d-%02d-%02d", y, m, d);
  if (L < 0 || (size_t)L >= n) return 0;
  memcpy(out, tmp, (size_t)L + 1);
  return 1;
}

/* "20230518" -> "2023-05-18". OIG/openFDA use 00000000 as the null marker. */
static inline int sanc_yyyymmdd(const char *in, char *out, size_t n) {
  if (!in || n < 11 || strlen(in) != 8) return 0;
  for (int i = 0; i < 8; i++) if (!isdigit((unsigned char)in[i])) return 0;
  if (strncmp(in, "0000", 4) == 0) return 0;
  snprintf(out, n, "%.4s-%.2s-%.2s", in, in + 4, in + 6);
  return 1;
}

/* Days since 1970-01-01 for a proleptic-Gregorian y/m/d (Howard Hinnant's
 * days_from_civil). Used only to apply an RFC-822 zone offset. */
static inline long sanc_days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153u * (m + (m > 2 ? -3u : 9u)) + 2u) / 5u + d - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return (long)era * 146097L + (long)doe - 719468L;
}

static inline void sanc_civil_from_days(long z, int *y, int *m, int *d) {
  z += 719468L;
  const long era = (z >= 0 ? z : z - 146096L) / 146097L;
  const unsigned long doe = (unsigned long)(z - era * 146097L);
  const unsigned long yoe =
      (doe - doe / 1460UL + doe / 36524UL - doe / 146096UL) / 365UL;
  const long yy = (long)yoe + era * 400L;
  const unsigned long doy = doe - (365UL * yoe + yoe / 4UL - yoe / 100UL);
  const unsigned long mp = (5UL * doy + 2UL) / 153UL;
  const unsigned long dd = doy - (153UL * mp + 2UL) / 5UL + 1UL;
  const int mm = (int)mp + (mp < 10UL ? 3 : -9);
  *y = (int)yy + (mm <= 2 ? 1 : 0);
  *m = mm;
  *d = (int)dd;
}

/* RFC-822/1123 ("Fri, 31 Jul 2026 12:00:00 +0000" / "… GMT") -> ISO-8601 UTC.
 * Returns 1 on success. Anything unrecognised -> 0 and the caller passes the
 * original string through rather than inventing a timestamp. */
static inline int sanc_rfc822_iso(const char *in, char *out, size_t n) {
  if (!in || n < 21) return 0;
  static const char *MON = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *p = in;
  while (*p == ' ') p++;
  if (strlen(p) > 5 && p[3] == ',') p += 4;      /* skip "Fri," */
  while (*p == ' ') p++;
  int d = 0;
  if (!isdigit((unsigned char)*p)) return 0;
  while (isdigit((unsigned char)*p)) d = d * 10 + (*p++ - '0');
  while (*p == ' ') p++;
  if (strlen(p) < 3) return 0;
  int mon = 0;
  for (int i = 0; i < 12; i++)
    if (sanc_ncaseeq(p, MON + i * 3, 3)) { mon = i + 1; break; }
  if (!mon) return 0;
  p += 3;
  while (*p == ' ') p++;
  int y = 0;
  while (isdigit((unsigned char)*p)) y = y * 10 + (*p++ - '0');
  if (y < 100) y += 2000;
  if (y < 1000) return 0;
  int hh = 0, mm = 0, ss = 0;
  while (*p == ' ') p++;
  if (isdigit((unsigned char)*p)) sscanf(p, "%d:%d:%d", &hh, &mm, &ss);
  /* zone */
  int offmin = 0;
  const char *z = strrchr(p, ' ');
  if (z) {
    z++;
    if ((*z == '+' || *z == '-') && strlen(z) >= 5) {
      int sign = (*z == '-') ? -1 : 1;
      int oh = (z[1] - '0') * 10 + (z[2] - '0');
      int om = (z[3] - '0') * 10 + (z[4] - '0');
      offmin = sign * (oh * 60 + om);
    }
  }
  long secs = sanc_days_from_civil(y, (unsigned)mon, (unsigned)d) * 86400L +
              hh * 3600L + mm * 60L + ss - (long)offmin * 60L;
  long days = secs / 86400L;
  long rem = secs % 86400L;
  if (rem < 0) { rem += 86400L; days -= 1; }
  int oy, om2, od;
  sanc_civil_from_days(days, &oy, &om2, &od);
  snprintf(out, n, "%04d-%02d-%02dT%02ld:%02ld:%02ldZ", oy, om2, od,
           rem / 3600L, (rem / 60L) % 60L, rem % 60L);
  return 1;
}

/* UTC "YYYY-MM-DDT00:00:00Z" for (now - days_back). Used by the one endpoint
 * whose path carries a start date (GovInfo) so the window does not go stale. */
static inline void sanc_utc_days_ago(int days_back, char *out, size_t n) {
  time_t t = time(NULL);
  long days = (long)(t / 86400) - days_back;
  int y, m, d;
  sanc_civil_from_days(days, &y, &m, &d);
  snprintf(out, n, "%04d-%02d-%02dT00:00:00Z", y, m, d);
}

/* ----------------------------------------------------------------- json --- */

/* Add a string member only when v is non-NULL and non-empty. */
static inline void sanc_add(cJSON *o, const char *k, const char *v) {
  if (o && k && v && *v) cJSON_AddStringToObject(o, k, v);
}

/* First string element of a cJSON array (FollowTheMoney stores EVERY property
 * as an array, even single-valued ones). */
static inline const char *sanc_arr0(const cJSON *arr) {
  if (!cJSON_IsArray(arr)) return NULL;
  const cJSON *it;
  cJSON_ArrayForEach(it, arr)
    if (cJSON_IsString(it) && it->valuestring && it->valuestring[0])
      return it->valuestring;
  return NULL;
}

/* properties[key][0] as a string, or NULL. */
static inline const char *sanc_prop0(const cJSON *props, const char *key) {
  if (!props) return NULL;
  return sanc_arr0(cJSON_GetObjectItem(props, key));
}

/* Join up to `max` string elements of `arr` with `sep` into out. */
static inline void sanc_join(const cJSON *arr, const char *sep, int max,
                             char *out, size_t n) {
  if (!out || !n) return;
  out[0] = 0;
  if (!cJSON_IsArray(arr)) return;
  size_t j = 0;
  int c = 0;
  const cJSON *it;
  cJSON_ArrayForEach(it, arr) {
    if (c >= max) break;
    if (!cJSON_IsString(it) || !it->valuestring || !it->valuestring[0]) continue;
    size_t need = strlen(it->valuestring) + (j ? strlen(sep) : 0);
    if (j + need >= n - 1) break;
    if (j) { strcpy(out + j, sep); j += strlen(sep); }
    strcpy(out + j, it->valuestring);
    j += strlen(it->valuestring);
    c++;
  }
}

/* Copy the string members of `arr` (up to max) into a fresh cJSON array. */
static inline cJSON *sanc_str_array(const cJSON *arr, int max) {
  cJSON *o = cJSON_CreateArray();
  if (!cJSON_IsArray(arr)) return o;
  int c = 0;
  const cJSON *it;
  cJSON_ArrayForEach(it, arr) {
    if (c >= max) break;
    if (cJSON_IsString(it) && it->valuestring && it->valuestring[0]) {
      cJSON_AddItemToArray(o, cJSON_CreateString(it->valuestring));
      c++;
    }
  }
  return o;
}

/* Depth-first search for the first STRING value stored under member `key`.
 * Austria's RIS nests the same field at different depths depending on the
 * document type, so a fixed path misses records that are really there. */
static inline const char *sanc_deep_str(const cJSON *node, const char *key,
                                        int depth) {
  if (!node || depth <= 0) return NULL;
  const cJSON *c;
  cJSON_ArrayForEach(c, node) {
    if (c->string && strcmp(c->string, key) == 0) {
      if (cJSON_IsString(c) && c->valuestring && c->valuestring[0])
        return c->valuestring;
      if (cJSON_IsObject(c)) {                 /* {"item": "…"} / {"#text": …} */
        const char *t = sanc_deep_str(c, "item", 2);
        if (!t) t = sanc_deep_str(c, "#text", 2);
        if (t) return t;
      }
      if (cJSON_IsArray(c)) {
        const char *t = sanc_arr0(c);
        if (t) return t;
      }
    }
    if (cJSON_IsObject(c) || cJSON_IsArray(c)) {
      const char *t = sanc_deep_str(c, key, depth - 1);
      if (t) return t;
    }
  }
  return NULL;
}

/* ------------------------------------------------------- RSS / Atom feeds --- */

/* One parsed feed entry handed to an enrich hook. `raw`/`raw_end` bracket the
 * item/entry XML so a hook can reach namespaced elements the generic parser
 * does not know about (tna:identifier, decision:date …). */
typedef struct sanc_feed_item {
  const char *title, *link, *desc;
  const char *raw, *raw_end;
  char *summary;
  size_t summary_sz;
} sanc_feed_item;

typedef struct sanc_feed_cfg {
  const char *record_type;
  const char *tags_json;
  const char *lang;
  /* When set, only items whose (whitespace-collapsed) title begins with this
   * prefix are emitted — the EFTA feed interleaves judgments and press
   * releases in one channel. */
  const char *title_prefix;
  /* Optional hook to add REAL fields parsed out of the item (case number,
   * court code, neutral citation …) to the row's properties and summary. Never
   * used to invent values: it only re-reads bytes the feed actually returned. */
  void (*enrich)(cJSON *props, const sanc_feed_item *it);
} sanc_feed_cfg;

/* Fetch an RSS 2.0 / RDF / Atom feed and emit one row per item/entry.
 * Returns the number emitted, or -1 when the fetch itself failed. Zero items
 * from a feed that fetched fine is a CLEAN result (the caller returns 0). */
static inline int sanc_feed_run(const source_ctx *ctx, intel_sink *sink,
                                const char *url, const sanc_feed_cfg *cfg,
                                const char *tag) {
  size_t len = 0;
  char *raw = sanc_http_get(ctx, url, "Accept: application/rss+xml, application/atom+xml, */*",
                            NULL, NULL, 25000, tag, &len, NULL);
  if (!raw) return -1;
  raw = sanc_to_utf8(raw, len, &len);
  const char *xml = sanc_skip_bom(raw);
  const char *end = raw + len;
  if (xml > end) end = xml;

  int max_items = sanc_env_int("JO_SANC_MAX_ITEMS", 300);
  int n = 0;
  for (int pass = 0; pass < 2 && n == 0; pass++) {
    const char *item_tag = pass == 0 ? "item" : "entry";
    const char *cur = xml;
    sanc_el el;
    while (n < max_items && sanc_xml_next(&cur, end, item_tag, &el)) {
      const char *b = el.body, *e = el.body_end;
      char *title = sanc_xml_text(b, e, "title");
      char *desc = sanc_xml_text(b, e, "description");
      if (!desc) desc = sanc_xml_text(b, e, "summary");
      if (!desc) desc = sanc_xml_text(b, e, "content");
      char *guid = sanc_xml_text(b, e, "guid");
      if (!guid) guid = sanc_xml_text(b, e, "id");
      char *pub = sanc_xml_text(b, e, "pubDate");
      if (!pub) pub = sanc_xml_text(b, e, "published");
      if (!pub) pub = sanc_xml_text(b, e, "date");
      if (!pub) pub = sanc_xml_text(b, e, "updated");

      /* link: RSS carries it as text, Atom as href= on <link rel="alternate">. */
      char *link = sanc_xml_text(b, e, "link");
      if (!link) {
        const char *lc = b;
        sanc_el le;
        while (sanc_xml_next(&lc, e, "link", &le)) {
          char *rel = sanc_xml_attr(&le, "rel");
          char *href = sanc_xml_attr(&le, "href");
          int ok = href && (!rel || strcmp(rel, "alternate") == 0);
          free(rel);
          if (ok) { link = href; break; }
          free(href);
        }
      }
      if (title) sanc_collapse_ws(title);

      int keep = title && title[0];
      if (keep && cfg->title_prefix &&
          strncmp(title, cfg->title_prefix, strlen(cfg->title_prefix)) != 0)
        keep = 0;

      if (keep) {
        char iso[40];
        const char *pub_out = pub;
        if (pub && sanc_rfc822_iso(pub, iso, sizeof iso)) pub_out = iso;

        char summary[512];
        sanc_trunc(summary, sizeof summary, desc ? desc : "");
        sanc_collapse_ws(summary);

        cJSON *props = cJSON_CreateObject();
        sanc_add(props, "feed", url);
        sanc_add(props, "guid", guid);
        if (cfg->enrich) {
          sanc_feed_item fi;
          fi.title = title; fi.link = link; fi.desc = desc;
          fi.raw = b; fi.raw_end = e;
          fi.summary = summary; fi.summary_sz = sizeof summary;
          cfg->enrich(props, &fi);
        }
        char *pj = cJSON_PrintUnformatted(props);
        cJSON_Delete(props);

        intel_item it = {0};
        it.remote_key = guid && guid[0] ? guid : (link && link[0] ? link : title);
        it.title = title;
        it.summary = summary[0] ? summary : NULL;
        it.body = desc;
        it.link = link;
        it.lang = cfg->lang;
        it.published_at = pub_out;
        it.record_type = cfg->record_type;
        it.properties_json = pj ? pj : "{}";
        it.tags_json = cfg->tags_json;
        /* R2: no geometry in this domain, ever. */
        if (sink->emit(sink, &it) >= 0) n++;
        free(pj);
      }
      free(title); free(desc); free(guid); free(pub); free(link);
    }
  }
  free(raw);
  fprintf(stderr, "[%s] emitted %d\n", tag, n);
  return n;
}

/* Shared enrich hook for the three PACER district docket feeds (CAND / NYSD /
 * DCD), which all serve the identical cgi-bin/rss_outside.pl shape:
 *   <title>   "3:26-cv-01132 Yakovlev v. The Superior Court …"
 *   <description> "[Leave to File Document] (<a href=…>34</a>)"
 * Everything added here is re-read out of those two fetched strings — the case
 * number is the leading token, the docket-entry type is the bracketed prefix.
 * "cr" in the case number marks a criminal prosecution. */
static inline void sanc_pacer_enrich(cJSON *props, const sanc_feed_item *fi) {
  const char *title = fi->title, *desc = fi->desc;
  char *summary = fi->summary;
  size_t summary_sz = fi->summary_sz;
  if (title) {
    char caseno[64];
    size_t i = 0;
    while (title[i] && title[i] != ' ' && i + 1 < sizeof caseno) {
      caseno[i] = title[i];
      i++;
    }
    caseno[i] = 0;
    if (caseno[0] && strchr(caseno, ':')) {
      sanc_add(props, "case_number", caseno);
      cJSON_AddBoolToObject(props, "criminal", strstr(caseno, "-cr-") != NULL);
    }
  }
  if (desc) {
    const char *lb = strchr(desc, '[');
    const char *rb = lb ? strchr(lb, ']') : NULL;
    if (lb && rb && rb > lb + 1 && (size_t)(rb - lb) < 120) {
      char etype[128];
      size_t l = (size_t)(rb - lb - 1);
      if (l >= sizeof etype) l = sizeof etype - 1;
      memcpy(etype, lb + 1, l);
      etype[l] = 0;
      sanc_add(props, "entry_type", etype);
      if (summary && summary_sz) snprintf(summary, summary_sz, "%s", etype);
    }
  }
}

/* ------------------------------------------------- OpenSanctions NDJSON --- *
 *
 *  !!!  LICENCE WARNING — READ BEFORE SHIPPING  !!!
 *
 *  Everything routed through this helper comes from the OpenSanctions bulk
 *  exports at data.opensanctions.org, which are published under
 *  CREATIVE COMMONS BY-NC 4.0:
 *      * ATTRIBUTION IS MANDATORY (carried on every row we emit, in
 *        properties.attribution, and in each source_def .license), and
 *      * USE IS RESTRICTED TO NON-COMMERCIAL PURPOSES.
 *  A commercial deployment of this platform may NOT ship these seventeen
 *  sources without first buying an OpenSanctions commercial licence. The
 *  product owner accepted that constraint knowingly; do not quietly drop this
 *  note when refactoring.
 *
 * Wire format is NDJSON: one FollowTheMoney entity per LINE, not a JSON array
 * — the whole payload is NOT a parseable document. Every property value is an
 * ARRAY even when single-valued.
 *
 * The exports run from tens of KB to hundreds of MB, so the download is bounded
 * with a Range request (JO_SANC_OS_MB, default 12 MB). When the server honours
 * it (HTTP 206) the final line is a fragment and is DISCARDED rather than
 * half-parsed. Rows per run are capped by JO_SANC_MAX_ROWS (default 5000).
 */
typedef struct sanc_os_cfg {
  const char *dataset;      /* OpenSanctions dataset slug                    */
  const char *record_type;  /* intel record_type for the rows                */
  const char *list_name;    /* human name of the ORIGINAL publisher's list   */
  const char *tags_json;
} sanc_os_cfg;

/* FollowTheMoney schemas that describe a relationship, an interval or an
 * address rather than a named target. They carry no name of their own and must
 * not be emitted as if they were list entries (jp_mof_sanctions ships Address
 * entities interleaved with the designations). */
static inline int sanc_os_skip_schema(const char *s) {
  if (!s) return 1;
  static const char *const SKIP[] = {
    "Address", "Sanction", "Ownership", "Directorship", "Membership",
    "Associate", "Family", "Employment", "Representation", "Succession",
    "UnknownLink", "Documentation", "Identification", "Payment", "Note",
    "Occupancy", "Position", "CourtCaseParty", "Debt", "Assessment", NULL };
  for (int i = 0; SKIP[i]; i++) if (strcmp(s, SKIP[i]) == 0) return 1;
  return 0;
}

static inline int sanc_opensanctions_run(const source_ctx *ctx, intel_sink *sink,
                                         const sanc_os_cfg *cfg,
                                         const char *tag) {
  char url[256];
  snprintf(url, sizeof url,
           "https://data.opensanctions.org/datasets/latest/%s/entities.ftm.json",
           cfg->dataset);

  int mb = sanc_env_int("JO_SANC_OS_MB", 12);
  char range[64];
  snprintf(range, sizeof range, "Range: bytes=0-%lld",
           (long long)mb * 1024LL * 1024LL - 1LL);

  size_t len = 0;
  long status = 0;
  char *body = sanc_http_get(ctx, url, "Accept: application/x-ndjson, */*",
                             range, NULL, 120000, tag, &len, &status);
  if (!body) return -1;                                /* real fetch failure */

  int max_rows = sanc_env_int("JO_SANC_MAX_ROWS", 5000);
  int n = 0, seen = 0;
  char *p = body;
  char *stop = body + len;
  while (p < stop && n < max_rows) {
    char *nl = (char *)memchr(p, '\n', (size_t)(stop - p));
    size_t llen = nl ? (size_t)(nl - p) : (size_t)(stop - p);
    if (!nl && status == 206) break;      /* truncated tail line — discard it */
    char save = p[llen];
    p[llen] = 0;
    if (llen > 2) {
      cJSON *ent = cJSON_Parse(p);
      if (ent) {
        seen++;
        const char *id = jo_sv(ent, "id");
        const char *schema = jo_sv(ent, "schema");
        const cJSON *props = cJSON_GetObjectItem(ent, "properties");
        const char *caption = jo_sv(ent, "caption");
        if (!caption) caption = sanc_prop0(props, "name");
        if (caption && id && !sanc_os_skip_schema(schema)) {
          const char *notes = sanc_prop0(props, "notes");
          const char *src_url = sanc_prop0(props, "sourceUrl");
          const char *country = sanc_prop0(props, "country");
          const char *nationality = sanc_prop0(props, "nationality");
          const char *birth = sanc_prop0(props, "birthDate");
          const char *birthplace = sanc_prop0(props, "birthPlace");
          const char *program = sanc_prop0(props, "programId");
          const char *regno = sanc_prop0(props, "registrationNumber");
          const char *sector = sanc_prop0(props, "sector");
          const char *first_seen = jo_sv(ent, "first_seen");
          const char *listed = sanc_prop0(props, "listingDate");
          if (!listed) listed = sanc_prop0(props, "createdAt");
          if (!listed) listed = sanc_prop0(props, "startDate");
          if (!listed) listed = first_seen;

          char topics[192];
          sanc_join(cJSON_GetObjectItem(props, "topics"), ", ", 8,
                    topics, sizeof topics);
          char aliases[512];
          sanc_join(cJSON_GetObjectItem(props, "alias"), " / ", 8,
                    aliases, sizeof aliases);

          char link[512];
          if (src_url) snprintf(link, sizeof link, "%s", src_url);
          else snprintf(link, sizeof link,
                        "https://www.opensanctions.org/entities/%s/", id);

          char summary[600];
          int off = snprintf(summary, sizeof summary, "%s%s%s%s%s%s",
                             schema ? schema : "Entity",
                             topics[0] ? " · " : "", topics,
                             country ? " · " : "", country ? country : "",
                             notes ? " · " : "");
          if (notes && off > 0 && (size_t)off < sizeof summary)
            sanc_trunc(summary + off, sizeof summary - (size_t)off, notes);
          sanc_collapse_ws(summary);

          cJSON *b2 = cJSON_CreateObject();
          sanc_add(b2, "name", caption);
          sanc_add(b2, "schema", schema);
          sanc_add(b2, "list", cfg->list_name);
          if (topics[0]) sanc_add(b2, "topics", topics);
          if (aliases[0]) sanc_add(b2, "aliases", aliases);
          sanc_add(b2, "country", country);
          sanc_add(b2, "nationality", nationality);
          sanc_add(b2, "birth_date", birth);
          sanc_add(b2, "birth_place", birthplace);
          sanc_add(b2, "program", program);
          sanc_add(b2, "registration_number", regno);
          sanc_add(b2, "sector", sector);
          sanc_add(b2, "notes", notes);
          sanc_add(b2, "source_url", src_url);
          sanc_add(b2, "attribution", SANC_OS_LICENSE);
          char *bj = cJSON_PrintUnformatted(b2);
          cJSON_Delete(b2);

          cJSON *pr = cJSON_CreateObject();
          sanc_add(pr, "dataset", cfg->dataset);
          sanc_add(pr, "list", cfg->list_name);
          sanc_add(pr, "entity_id", id);
          sanc_add(pr, "schema", schema);
          if (topics[0]) sanc_add(pr, "topics", topics);
          sanc_add(pr, "country", country);
          sanc_add(pr, "nationality", nationality);
          sanc_add(pr, "birth_date", birth);
          sanc_add(pr, "birth_place", birthplace);
          sanc_add(pr, "program", program);
          sanc_add(pr, "registration_number", regno);
          sanc_add(pr, "notes", notes);
          sanc_add(pr, "source_url", src_url);
          /* Provenance for published_at: when the underlying list gives no
           * listing/creation date we fall back to OpenSanctions' first_seen, so
           * both are recorded rather than presenting one as the other. */
          sanc_add(pr, "first_seen", first_seen);
          sanc_add(pr, "last_seen", jo_sv(ent, "last_seen"));
          cJSON_AddItemToObject(pr, "aliases",
                                sanc_str_array(cJSON_GetObjectItem(props, "alias"), 12));
          cJSON_AddItemToObject(pr, "cross_referenced_ids",
                                sanc_str_array(cJSON_GetObjectItem(ent, "referents"), 12));
          sanc_add(pr, "license", SANC_OS_LICENSE);
          sanc_add(pr, "attribution", "OpenSanctions.org (CC-BY-NC 4.0)");
          char *pj = cJSON_PrintUnformatted(pr);
          cJSON_Delete(pr);

          intel_item it = {0};
          it.remote_key = id;
          it.title = caption;
          it.summary = summary[0] ? summary : NULL;
          it.body = bj;
          it.link = link;
          it.lang = "en";
          it.published_at = listed;
          it.record_type = cfg->record_type;
          it.properties_json = pj ? pj : "{}";
          it.tags_json = cfg->tags_json;
          /* R2: OpenSanctions ships no coordinates for these lists and we do
           * NOT pin a designated party to its country. has_geo stays 0. */
          if (sink->emit(sink, &it) >= 0) n++;
          free(bj);
          free(pj);
        }
        cJSON_Delete(ent);
      }
    }
    p[llen] = save;
    if (!nl) break;
    p = nl + 1;
  }
  free(body);
  fprintf(stderr, "[%s] %s: %d lines parsed, emitted %d (http %ld)\n",
          tag, cfg->dataset, seen, n, status);
  return n;
}

#endif /* JO_SANC_COMMON_INC */
