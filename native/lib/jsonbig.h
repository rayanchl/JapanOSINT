/* lib/jsonbig.h — exact integers beyond 2^53 for cJSON (LIB-08).
 *
 * cJSON keeps a number as a double, so {"id":1234567890123456781} and
 * {"id":1234567890123456782} both read back as 1234567890123456768: two
 * records collapse onto one uid and a detail hop asks for a different id.
 * jo_json_quote_bigints() rewrites such bare integer literals as JSON STRINGS
 * before the text reaches cJSON_Parse, so the exact digits survive as text.
 *
 * Deliberately narrow, so it cannot change anything that already worked:
 *  - only an INTEGER literal (digits, optional leading '-', no '.', 'e', 'E')
 *    OUTSIDE any JSON string is considered; strings are tracked with escape
 *    handling, so a number inside a string is never touched;
 *  - only when it would NOT survive the double round trip (its printed
 *    %lld form differs from the literal, or it is beyond long long). A 19-digit
 *    nanosecond timestamp that a double represents exactly stays a number.
 * Returns NULL when nothing needed quoting (use the original text), else a
 * malloc'd rewritten copy the caller frees. Header-only so every parser
 * (hpengine, feedlib, jsonstream) can use it without a link dependency. */
#ifndef JO_JSONBIG_H
#define JO_JSONBIG_H
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../third_party/cJSON.h"

static inline int jo_bigint_lossy(const char *lit, size_t n) {
  /* lit = [-]digits, n bytes, n >= 16 digits already established */
  char tmp[64];
  if (n >= sizeof tmp) return 1;                 /* far beyond a double */
  memcpy(tmp, lit, n); tmp[n] = 0;
  double d = strtod(tmp, NULL);
  if (!(d >= (double)LLONG_MIN && d < (double)LLONG_MAX)) return 1;
  char back[64];
  snprintf(back, sizeof back, "%lld", (long long)d);
  return strcmp(back, tmp) != 0;
}

static inline char *jo_json_quote_bigints(const char *s) {
  if (!s) return NULL;
  size_t n = strlen(s), extra = 0;
  int instr = 0, esc = 0;
  /* pass 1: count what needs quoting */
  for (size_t i = 0; i < n; ) {
    char c = s[i];
    if (instr) { if (esc) esc = 0; else if (c == '\\') esc = 1; else if (c == '"') instr = 0; i++; continue; }
    if (c == '"') { instr = 1; i++; continue; }
    if (c == '-' || (c >= '0' && c <= '9')) {
      size_t j = i + (c == '-');
      size_t ds = j;
      while (j < n && s[j] >= '0' && s[j] <= '9') j++;
      size_t digits = j - ds;
      int integer = digits > 0 && !(j < n && (s[j] == '.' || s[j] == 'e' || s[j] == 'E'));
      if (integer && digits >= 16 && jo_bigint_lossy(s + i, j - i)) extra += 2;
      i = j > i ? j : i + 1;
      /* skip the rest of a fractional/exponent literal so its digits are not re-read */
      while (i < n && (s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-' || (s[i] >= '0' && s[i] <= '9'))) i++;
      continue;
    }
    i++;
  }
  if (!extra) return NULL;
  char *out = malloc(n + extra + 1);
  if (!out) return NULL;
  size_t w = 0; instr = 0; esc = 0;
  for (size_t i = 0; i < n; ) {
    char c = s[i];
    if (instr) { out[w++] = c; if (esc) esc = 0; else if (c == '\\') esc = 1; else if (c == '"') instr = 0; i++; continue; }
    if (c == '"') { instr = 1; out[w++] = c; i++; continue; }
    if (c == '-' || (c >= '0' && c <= '9')) {
      size_t j = i + (c == '-');
      size_t ds = j;
      while (j < n && s[j] >= '0' && s[j] <= '9') j++;
      size_t digits = j - ds;
      int integer = digits > 0 && !(j < n && (s[j] == '.' || s[j] == 'e' || s[j] == 'E'));
      if (integer && digits >= 16 && jo_bigint_lossy(s + i, j - i)) {
        out[w++] = '"'; memcpy(out + w, s + i, j - i); w += j - i; out[w++] = '"';
        i = j;
      } else {
        size_t k = j > i ? j : i + 1;
        while (k < n && (s[k] == '.' || s[k] == 'e' || s[k] == 'E' || s[k] == '+' || s[k] == '-' || (s[k] >= '0' && s[k] <= '9'))) k++;
        memcpy(out + w, s + i, k - i); w += k - i; i = k;
      }
      continue;
    }
    out[w++] = c; i++;
  }
  out[w] = 0;
  return out;
}

/* Parse with exact big integers. Drop-in for cJSON_Parse. */
static inline cJSON *jo_json_parse(const char *text) {
  char *q = jo_json_quote_bigints(text);
  cJSON *d = cJSON_Parse(q ? q : text);
  free(q);
  return d;
}
#endif
