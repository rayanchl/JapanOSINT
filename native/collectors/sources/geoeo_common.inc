/* geoeo_common.inc — shared helpers for the geoeo_* geospatial/earth-observation
 * collectors. Not a translation unit: it is #included by the geoeo_*.c sources
 * (the build globs *.c, so this file is never compiled on its own).
 *
 * Everything here is `static inline` on purpose: a file that uses only two of
 * these helpers still compiles warning-free under -Wall -Wextra (an unused
 * `static inline` is not diagnosed, an unused plain `static` is).
 *
 * The geometry helpers exist because of the audit finding this whole wave is
 * shaped around: a JSON `null` geometry serialised as the literal 4-byte string
 * "null" passes every `geometry IS NOT NULL AND geometry<>''` test downstream,
 * so the row reads as pinned when it is not. geoeo_geom() returns 0 and leaves
 * *gj == NULL for a null/absent/unusable geometry — never a "null" string.
 */
#ifndef GEOEO_COMMON_INC
#define GEOEO_COMMON_INC

#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------- scalar accessors ---------- */

/* Non-empty JSON string, or NULL. A JSON null / number / object gives NULL. */
static inline const char *geoeo_str(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0])
    return v->valuestring;
  return NULL;
}

/* Full-string strtod: "12abc" and "" are rejected rather than coerced to a
 * number. Used everywhere an upstream string-encodes an ordinate. */
static inline int geoeo_strtod(const char *s, double *out) {
  if (!s) return 0;
  while (*s == ' ' || *s == '\t') s++;
  if (!*s) return 0;
  char *e = NULL;
  double d = strtod(s, &e);
  if (e == s) return 0;
  while (*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n') e++;
  if (*e) return 0;
  if (!isfinite(d)) return 0;
  *out = d;
  return 1;
}

/* Real JSON number only — a JSON null or a string is rejected. */
static inline int geoeo_num(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble)) return 0;
  *out = v->valuedouble;
  return 1;
}

/* Number that may arrive as a JSON number OR as a JSON string ("34.92383").
 * Several upstreams here string-encode every ordinate, so cJSON_IsNumber()
 * alone would reject all of them. */
static inline int geoeo_numlax(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (cJSON_IsNumber(v)) {
    if (!isfinite(v->valuedouble)) return 0;
    *out = v->valuedouble;
    return 1;
  }
  if (cJSON_IsString(v)) return geoeo_strtod(v->valuestring, out);
  return 0;
}

/* WGS84 sanity gate. A coordinate outside this box is not a coordinate, and
 * emitting it would put a pin off the planet (the projected-CRS trap). */
static inline int geoeo_ll_ok(double lat, double lon) {
  return isfinite(lat) && isfinite(lon) && lat >= -90.0 && lat <= 90.0 &&
         lon >= -180.0 && lon <= 180.0;
}

/* ---------- property assembly ---------- */

static inline void geoeo_add_str(cJSON *o, const char *k, const char *v) {
  if (v && v[0]) cJSON_AddStringToObject(o, k, v);
}

/* Copy one member across only when it is a real scalar. A JSON null is
 * DROPPED, never stringified — "null" in a property is indistinguishable from
 * a value downstream. */
static inline void geoeo_copy(cJSON *dst, const cJSON *src, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(src, k);
  if (!v || cJSON_IsNull(v)) return;
  if (cJSON_IsString(v)) {
    if (v->valuestring && v->valuestring[0])
      cJSON_AddStringToObject(dst, k, v->valuestring);
  } else if (cJSON_IsNumber(v)) {
    cJSON_AddNumberToObject(dst, k, v->valuedouble);
  } else if (cJSON_IsBool(v)) {
    cJSON_AddBoolToObject(dst, k, cJSON_IsTrue(v) ? 1 : 0);
  }
}

/* Copy several members by name (NULL-terminated list). */
static inline void geoeo_copy_all(cJSON *dst, const cJSON *src,
                                  const char *const *keys) {
  for (int i = 0; keys && keys[i]; i++) geoeo_copy(dst, src, keys[i]);
}

/* Copy EVERY real scalar member of `src`, dropping nulls, objects and arrays.
 * For upstreams whose attribute set is not fixed (ArcGIS layers), this keeps
 * the row faithful to what actually came back without inventing a schema. */
static inline void geoeo_copy_scalars(cJSON *dst, const cJSON *src) {
  if (!cJSON_IsObject(src)) return;
  const cJSON *m;
  cJSON_ArrayForEach(m, src) {
    if (!m->string || cJSON_IsNull(m)) continue;
    if (cJSON_IsString(m)) {
      if (m->valuestring && m->valuestring[0])
        cJSON_AddStringToObject(dst, m->string, m->valuestring);
    } else if (cJSON_IsNumber(m)) {
      cJSON_AddNumberToObject(dst, m->string, m->valuedouble);
    } else if (cJSON_IsBool(m)) {
      cJSON_AddBoolToObject(dst, m->string, cJSON_IsTrue(m) ? 1 : 0);
    }
  }
}

/* First present, non-empty string among `keys`. */
static inline const char *geoeo_first_str(const cJSON *o,
                                          const char *const *keys) {
  for (int i = 0; keys && keys[i]; i++) {
    const char *v = geoeo_str(o, keys[i]);
    if (v) return v;
  }
  return NULL;
}

/* Epoch milliseconds (ArcGIS date fields) or an ISO string → ISO-8601.
 * Returns 1 when `out` was filled. */
static inline int geoeo_epoch_ms_iso(const cJSON *o, const char *k, char *out,
                                     size_t n) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    snprintf(out, n, "%s", v->valuestring);
    return 1;
  }
  if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble)) return 0;
  long long ms = (long long)v->valuedouble;
  if (ms < 100000000000LL || ms > 4000000000000LL) return 0; /* not a ms epoch */
  long long secs = ms / 1000, days = secs / 86400, rem = secs % 86400;
  long long zz = days + 719468;
  long long era = (zz >= 0 ? zz : zz - 146096) / 146097;
  unsigned doe = (unsigned)(zz - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long long y = (long long)yoe + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  unsigned d = doy - (153 * mp + 2) / 5 + 1;
  unsigned m = mp + (mp < 10 ? 3 : -9);
  y += (m <= 2);
  snprintf(out, n, "%04lld-%02u-%02uT%02u:%02u:%02uZ", y, m, d,
           (unsigned)(rem / 3600), (unsigned)((rem % 3600) / 60),
           (unsigned)(rem % 60));
  return 1;
}

/* ---------- geometry ---------- */

/* First [x,y] numeric pair anywhere in a coordinates tree. */
static inline int geoeo_first_pos(const cJSON *c, double *lon, double *lat) {
  if (!cJSON_IsArray(c)) return 0;
  const cJSON *a = cJSON_GetArrayItem(c, 0), *b = cJSON_GetArrayItem(c, 1);
  if (cJSON_IsNumber(a) && cJSON_IsNumber(b)) {
    *lon = a->valuedouble;
    *lat = b->valuedouble;
    return 1;
  }
  const cJSON *ch;
  cJSON_ArrayForEach(ch, c)
    if (geoeo_first_pos(ch, lon, lat)) return 1;
  return 0;
}

/* Vertex mean of one ring — a representative point that is inside the hull. */
static inline int geoeo_ring_mean(const cJSON *ring, double *lon, double *lat) {
  if (!cJSON_IsArray(ring)) return 0;
  double sx = 0, sy = 0;
  int used = 0;
  const cJSON *pt;
  cJSON_ArrayForEach(pt, ring) {
    const cJSON *a = cJSON_GetArrayItem(pt, 0), *b = cJSON_GetArrayItem(pt, 1);
    if (!cJSON_IsNumber(a) || !cJSON_IsNumber(b)) continue;
    sx += a->valuedouble;
    sy += b->valuedouble;
    used++;
  }
  if (!used) return 0;
  *lon = sx / used;
  *lat = sy / used;
  return 1;
}

/* Representative pin + serialised geometry from a GeoJSON geometry object.
 * Returns 1 only when the upstream really gave coordinates. On 0, *gj stays
 * NULL: a `"geometry": null` must NOT become the string "null". Caller frees
 * *gj. Coordinate arrays with a third ordinate (elevation / dummy z) are
 * accepted — a strict "length == 2" test would drop SWOB, GLIMS and POWER
 * entirely. */
static inline int geoeo_geom(const cJSON *g, double *lat, double *lon,
                             char **gj) {
  if (gj) *gj = NULL;
  if (!g || cJSON_IsNull(g) || !cJSON_IsObject(g)) return 0;
  const char *ty = geoeo_str(g, "type");
  const cJSON *c = cJSON_GetObjectItem(g, "coordinates");
  if (!ty || !cJSON_IsArray(c)) return 0;
  double x = 0, y = 0;
  int ok = 0;
  if (strcmp(ty, "Polygon") == 0)
    ok = geoeo_ring_mean(cJSON_GetArrayItem(c, 0), &x, &y);
  else if (strcmp(ty, "MultiPolygon") == 0)
    ok = geoeo_ring_mean(cJSON_GetArrayItem(cJSON_GetArrayItem(c, 0), 0), &x, &y);
  if (!ok) ok = geoeo_first_pos(c, &x, &y);
  if (!ok || !geoeo_ll_ok(y, x)) return 0;
  *lat = y;
  *lon = x;
  if (gj) *gj = cJSON_PrintUnformatted(g);
  return 1;
}

/* GeoJSON Point object from real fetched ordinates. */
static inline cJSON *geoeo_mk_point(double lon, double lat) {
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_AddArrayToObject(g, "coordinates");
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  return g;
}

/* GeoJSON Polygon from an axis-aligned box of real fetched ordinates. */
static inline cJSON *geoeo_mk_bbox(double w, double s, double e, double n) {
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Polygon");
  cJSON *rings = cJSON_AddArrayToObject(g, "coordinates");
  cJSON *ring = cJSON_CreateArray();
  const double xs[5] = { w, e, e, w, w }, ys[5] = { s, s, n, n, s };
  for (int i = 0; i < 5; i++) {
    cJSON *p = cJSON_CreateArray();
    cJSON_AddItemToArray(p, cJSON_CreateNumber(xs[i]));
    cJSON_AddItemToArray(p, cJSON_CreateNumber(ys[i]));
    cJSON_AddItemToArray(ring, p);
  }
  cJSON_AddItemToArray(rings, ring);
  return g;
}

/* ---------- text ---------- */

static inline char *geoeo_dupn(const char *p, size_t len) {
  char *s = (char *)malloc(len + 1);
  if (!s) return NULL;
  memcpy(s, p, len);
  s[len] = 0;
  return s;
}

static inline char *geoeo_trim(char *s) {
  if (!s) return s;
  char *p = s;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  if (p != s) memmove(s, p, strlen(p) + 1);
  size_t n = strlen(s);
  while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' ||
               s[n - 1] == '\r'))
    s[--n] = 0;
  return s;
}

/* The XML entities these feeds actually use, decoded in place. */
static inline void geoeo_unescape(char *s) {
  if (!s) return;
  char *w = s;
  for (char *r = s; *r;) {
    if (*r == '&') {
      if (!strncmp(r, "&amp;", 5))       { *w++ = '&';  r += 5; continue; }
      if (!strncmp(r, "&lt;", 4))        { *w++ = '<';  r += 4; continue; }
      if (!strncmp(r, "&gt;", 4))        { *w++ = '>';  r += 4; continue; }
      if (!strncmp(r, "&quot;", 6))      { *w++ = '"';  r += 6; continue; }
      if (!strncmp(r, "&apos;", 6))      { *w++ = '\''; r += 6; continue; }
      if (!strncmp(r, "&#39;", 5))       { *w++ = '\''; r += 5; continue; }
      if (!strncmp(r, "&#x27;", 6))      { *w++ = '\''; r += 6; continue; }
    }
    *w++ = *r++;
  }
  *w = 0;
}

/* Unwrap <![CDATA[ ... ]]> in place (no-op when absent). */
static inline void geoeo_uncdata(char *s) {
  if (!s) return;
  char *a = strstr(s, "<![CDATA[");
  if (!a) return;
  char *b = strstr(a + 9, "]]>");
  if (!b) return;
  size_t len = (size_t)(b - (a + 9));
  memmove(s, a + 9, len);
  s[len] = 0;
}

/* Text of the first <tag>…</tag> inside a NUL-terminated XML fragment,
 * CDATA-unwrapped, entity-decoded and trimmed. Namespace-tolerant via
 * html_tag (so "lat" matches <geo:lat>). Returns 1 when non-empty. */
static inline int geoeo_xml_text(const char *xml, const char *tag, char *out,
                                 size_t n) {
  extern int html_tag(const char *s, const char *tag, char *out, size_t n);
  out[0] = 0;
  if (!html_tag(xml, tag, out, n)) return 0;
  geoeo_uncdata(out);
  geoeo_unescape(out);
  geoeo_trim(out);
  return out[0] != 0;
}

/* Numeric text of the first <tag> — full-string parse, so an empty element
 * (<tag></tag>, very common in these XML feeds) yields 0, not 0.0. */
static inline int geoeo_xml_num(const char *xml, const char *tag, double *out) {
  char buf[128];
  if (!geoeo_xml_text(xml, tag, buf, sizeof buf)) return 0;
  return geoeo_strtod(buf, out);
}

/* ---------- misc ---------- */

/* Days since 1970-01-01 from a proleptic Gregorian date (Howard Hinnant). */
static inline long geoeo_days_from_civil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

static inline long long geoeo_utc_epoch(int y, int mo, int d, int h, int mi,
                                        int s) {
  return (long long)geoeo_days_from_civil(y, mo, d) * 86400LL + h * 3600LL +
         mi * 60LL + s;
}

#endif /* GEOEO_COMMON_INC */
