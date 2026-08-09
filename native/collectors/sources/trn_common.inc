/* trn_common.inc — shared static helpers for the trn_* transport collectors.
 *
 * Included, never compiled standalone (the Makefile globs collectors/sources/
 * *.c, not *.inc — same trick as _jp_osint.inc / username_osint_platforms.inc).
 * Everything is `static inline`, so each translation unit gets its own copy and
 * an unused helper does not trip -Wunused-function.
 *
 * House rules honoured by every helper (SOURCE_AUTHORING_CONTRACT.md):
 *  - R1: nothing here invents a field. Helpers only reshape fetched values.
 *  - R2: trn_geo_ok() is the ONLY gate on has_geo; there is no centroid/city
 *        fallback anywhere in this file. A row with no upstream coordinate is
 *        emitted with has_geo = 0.
 *  - R3: the GBFS helpers return -1 only when the fetch/parse itself failed;
 *        a feed that parsed but listed nothing returns 0.
 */
#ifndef TRN_COMMON_INC
#define TRN_COMMON_INC

#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* ---- scalar accessors ------------------------------------------------- */

/* Finite number from a cJSON value, coercing a numeric STRING — a lot of these
 * feeds ship lat/lon/counters as strings (SEPTA, RDW, Socrata, HK Citybus). */
static inline int trn_numv(const cJSON *v, double *out) {
  if (!v) return 0;
  if (cJSON_IsNumber(v)) {
    if (!isfinite(v->valuedouble)) return 0;
    *out = v->valuedouble; return 1;
  }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *end = NULL;
    double d = strtod(v->valuestring, &end);
    if (end && end != v->valuestring && isfinite(d)) { *out = d; return 1; }
  }
  return 0;
}

static inline int trn_num(const cJSON *o, const char *k, double *out) {
  return trn_numv(cJSON_GetObjectItem(o, k), out);
}

/* Boolean field: 1/0 when present as a JSON bool, -1 when absent. */
static inline int trn_bool(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v || !cJSON_IsBool(v)) return -1;
  return cJSON_IsTrue(v) ? 1 : 0;
}

/* A coordinate pair we are willing to pin on the map: finite, inside the WGS84
 * domain, and not the (0,0) null-island placeholder that several registries
 * use for "unknown". Nothing else is ever promoted to has_geo (R2). */
static inline int trn_geo_ok(double lat, double lon) {
  if (!isfinite(lat) || !isfinite(lon)) return 0;
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) return 0;
  if (lat == 0.0 && lon == 0.0) return 0;
  return 1;
}

/* Inside an explicit bounding box (used where the publisher is known to ship a
 * handful of corrupt coordinates — see trn_digitraffic_tms_stations.c). */
static inline int trn_in_bbox(double lat, double lon,
                              double lat0, double lat1,
                              double lon0, double lon1) {
  return lat >= lat0 && lat <= lat1 && lon >= lon0 && lon <= lon1;
}

/* Identifier as text whether upstream typed it as a string or a number
 * (Vélib' station_id is a number, everyone else's is a string). */
static inline void trn_idstr(const cJSON *v, char *out, size_t n) {
  out[0] = 0;
  if (!v) return;
  if (cJSON_IsString(v) && v->valuestring) snprintf(out, n, "%s", v->valuestring);
  else if (cJSON_IsNumber(v)) {
    if (v->valuedouble == (double)(long long)v->valuedouble)
      snprintf(out, n, "%lld", (long long)v->valuedouble);
    else snprintf(out, n, "%.10g", v->valuedouble);
  }
}

static inline void trn_idfield(const cJSON *o, const char *k,
                               char *out, size_t n) {
  trn_idstr(cJSON_GetObjectItem(o, k), out, n);
}

/* Unix seconds → ISO-8601 UTC. Only ever called with a timestamp the upstream
 * actually returned. */
static inline void trn_iso_from_unix(double secs, char *out, size_t n) {
  out[0] = 0;
  if (!isfinite(secs) || secs <= 0 || secs > 4102444800.0) return; /* > 2100 */
  time_t t = (time_t)secs;
  struct tm g;
  if (!gmtime_r(&t, &g)) return;
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &g);
}

/* cJSON_AddStringToObject only when the value is really there. */
static inline void trn_put_str(cJSON *o, const char *k, const char *v) {
  if (v && v[0]) cJSON_AddStringToObject(o, k, v);
}
static inline void trn_put_num(cJSON *o, const char *k, const cJSON *src,
                               const char *srckey) {
  double d;
  if (trn_num(src, srckey, &d)) cJSON_AddNumberToObject(o, k, d);
}
static inline void trn_put_bool(cJSON *o, const char *k, const cJSON *src,
                                const char *srckey) {
  int b = trn_bool(src, srckey);
  if (b >= 0) cJSON_AddBoolToObject(o, k, b);
}

/* RFC-3986 %-encode (UTF-8 safe). malloc'd; caller frees. */
static inline char *trn_urlencode(const char *s) {
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

/* ---- GeoJSON ----------------------------------------------------------- */

/* A GeoJSON geometry's first position, as lat/lon. GeoJSON positions are
 * [lon, lat] and MAY carry a third elevation element (Digitraffic ships
 * [lon, lat, elev]) — only the first two are read. Works for Point directly and
 * descends into the first position of any nested coordinate array, which is a
 * representative vertex the upstream really published (never a computed centre).
 * Returns 1 and writes through the lat and lon out-params on success. */
static inline int trn_geom_first_pos(const cJSON *geom, double *lat,
                                     double *lon) {
  if (!geom) return 0;
  const cJSON *c = cJSON_GetObjectItem(geom, "coordinates");
  int guard = 0;
  while (cJSON_IsArray(c) && guard++ < 8) {
    const cJSON *a = cJSON_GetArrayItem(c, 0);
    const cJSON *b = cJSON_GetArrayItem(c, 1);
    if (cJSON_IsNumber(a) && cJSON_IsNumber(b)) {
      if (!trn_geo_ok(b->valuedouble, a->valuedouble)) return 0;
      *lon = a->valuedouble; *lat = b->valuedouble;
      return 1;
    }
    c = a;
  }
  return 0;
}

/* ---- XML scanning ------------------------------------------------------ */

/* Decode the five XML predefined entities plus numeric references, in place
 * semantics but into a fresh malloc'd string (caller frees). */
static inline char *trn_xml_unescape(const char *s, size_t len) {
  char *o = (char *)malloc(len + 1);
  if (!o) return NULL;
  size_t w = 0;
  for (size_t i = 0; i < len; ) {
    if (s[i] != '&') { o[w++] = s[i++]; continue; }
    size_t j = i + 1, e = j;
    while (e < len && e < j + 12 && s[e] != ';' && s[e] != '&') e++;
    if (e >= len || s[e] != ';') { o[w++] = s[i++]; continue; }
    size_t nlen = e - j;
    if (nlen == 2 && !strncmp(s + j, "lt", 2))        o[w++] = '<';
    else if (nlen == 2 && !strncmp(s + j, "gt", 2))   o[w++] = '>';
    else if (nlen == 3 && !strncmp(s + j, "amp", 3))  o[w++] = '&';
    else if (nlen == 4 && !strncmp(s + j, "quot", 4)) o[w++] = '"';
    else if (nlen == 4 && !strncmp(s + j, "apos", 4)) o[w++] = '\'';
    else if (nlen >= 2 && s[j] == '#') {
      long cp = (s[j + 1] == 'x' || s[j + 1] == 'X')
                  ? strtol(s + j + 2, NULL, 16) : strtol(s + j + 1, NULL, 10);
      if (cp <= 0 || cp > 0x10FFFF) { o[w++] = s[i++]; continue; }
      if (cp < 0x80) o[w++] = (char)cp;
      else if (cp < 0x800) { o[w++] = (char)(0xC0 | (cp >> 6));
                             o[w++] = (char)(0x80 | (cp & 0x3F)); }
      else if (cp < 0x10000) { o[w++] = (char)(0xE0 | (cp >> 12));
                               o[w++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                               o[w++] = (char)(0x80 | (cp & 0x3F)); }
      else { o[w++] = (char)(0xF0 | (cp >> 18));
             o[w++] = (char)(0x80 | ((cp >> 12) & 0x3F));
             o[w++] = (char)(0x80 | ((cp >> 6) & 0x3F));
             o[w++] = (char)(0x80 | (cp & 0x3F)); }
    } else { o[w++] = s[i++]; continue; }
    i = e + 1;
  }
  o[w] = 0;
  return o;
}

/* Inner text of the first <tag>…</tag> found at or after `from` inside the
 * NUL-terminated `block`. Namespace tolerant, exact tag-name match (so a scan
 * for "latitude" never matches "startLatitude" — see the NZTA trap).
 * Returns a malloc'd, entity-decoded, whitespace-trimmed string, or NULL. */
static inline char *trn_xml_text(const char *block, const char *tag) {
  if (!block) return NULL;
  size_t tl = strlen(tag);
  for (const char *p = block; (p = strchr(p, '<')) != NULL; p++) {
    const char *q = p + 1;
    if (*q == '/' || *q == '!' || *q == '?') continue;
    const char *ne = q;
    while (*ne && *ne != ' ' && *ne != '>' && *ne != '/' && *ne != '\t' &&
           *ne != '\n' && *ne != '\r') ne++;
    const char *base = q;
    for (const char *c = q; c < ne; c++) if (*c == ':') base = c + 1;
    if ((size_t)(ne - base) != tl || strncasecmp(base, tag, tl) != 0) continue;
    const char *gt = strchr(q, '>');
    if (!gt) return NULL;
    if (gt > q && gt[-1] == '/') return NULL;      /* <tag/> → no text */
    const char *inner = gt + 1;
    /* matching close tag */
    for (const char *r = inner; (r = strchr(r, '<')) != NULL; r++) {
      if (r[1] != '/') continue;
      const char *cn = r + 2, *ce = cn;
      while (*ce && *ce != '>') ce++;
      const char *cb = cn;
      for (const char *c = cn; c < ce; c++) if (*c == ':') cb = c + 1;
      const char *cend = ce;
      while (cend > cb && cend[-1] == ' ') cend--;
      if ((size_t)(cend - cb) != tl || strncasecmp(cb, tag, tl) != 0) continue;
      const char *s = inner, *e = r;
      while (s < e && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')) s++;
      while (e > s && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r' ||
                       e[-1] == '\t')) e--;
      return trn_xml_unescape(s, (size_t)(e - s));
    }
    return NULL;
  }
  return NULL;
}

/* Same, but the text must parse as a finite number. */
static inline int trn_xml_num(const char *block, const char *tag, double *out) {
  char *t = trn_xml_text(block, tag);
  if (!t) return 0;
  char *end = NULL;
  double d = strtod(t, &end);
  int ok = (end && end != t && isfinite(d));
  free(t);
  if (ok) *out = d;
  return ok;
}

/* Iterate <tag>…</tag> records. *cur is advanced past the record; returns the
 * record's inner XML as a malloc'd NUL-terminated buffer (caller frees), or
 * NULL when there are no more. */
static inline char *trn_xml_record(const char **cur, const char *tag) {
  if (!cur || !*cur) return NULL;
  const char *p = *cur;
  size_t tl = strlen(tag);
  for (; (p = strchr(p, '<')) != NULL; p++) {
    const char *q = p + 1;
    if (*q == '/' || *q == '!' || *q == '?') continue;
    const char *ne = q;
    while (*ne && *ne != ' ' && *ne != '>' && *ne != '/' && *ne != '\t' &&
           *ne != '\n' && *ne != '\r') ne++;
    const char *base = q;
    for (const char *c = q; c < ne; c++) if (*c == ':') base = c + 1;
    if ((size_t)(ne - base) != tl || strncasecmp(base, tag, tl) != 0) continue;
    const char *gt = strchr(q, '>');
    if (!gt) break;
    if (gt > q && gt[-1] == '/') { p = gt; continue; }   /* empty element */
    const char *inner = gt + 1;
    for (const char *r = inner; (r = strchr(r, '<')) != NULL; r++) {
      if (r[1] != '/') continue;
      const char *cn = r + 2, *ce = cn;
      while (*ce && *ce != '>') ce++;
      const char *cb = cn;
      for (const char *c = cn; c < ce; c++) if (*c == ':') cb = c + 1;
      const char *cend = ce;
      while (cend > cb && cend[-1] == ' ') cend--;
      if ((size_t)(cend - cb) != tl || strncasecmp(cb, tag, tl) != 0) continue;
      size_t len = (size_t)(r - inner);
      char *buf = (char *)malloc(len + 1);
      if (!buf) return NULL;
      memcpy(buf, inner, len);
      buf[len] = 0;
      *cur = (*ce == '>') ? ce + 1 : r;
      return buf;
    }
    break;
  }
  *cur = NULL;
  return NULL;
}

/* ---- GBFS ------------------------------------------------------------- */

/* GBFS station/vehicle name. v1/v2 ship a plain string; GBFS v3.0 ships an
 * ARRAY of {text,language} objects, so taking .name as a string silently
 * drops every name on the publicbikesystem.net / Entur / Cyclocity v3 feeds. */
static inline const char *trn_gbfs_name(const cJSON *st) {
  const cJSON *nm = cJSON_GetObjectItem(st, "name");
  if (!nm) return NULL;
  if (cJSON_IsString(nm)) return nm->valuestring[0] ? nm->valuestring : NULL;
  if (cJSON_IsArray(nm)) {
    const cJSON *e; const char *first = NULL;
    cJSON_ArrayForEach(e, nm) {
      const char *t = jo_sv(e, "text");
      if (!t) continue;
      const char *l = jo_sv(e, "language");
      if (l && strncmp(l, "en", 2) == 0) return t;
      if (!first) first = t;
    }
    return first;
  }
  return NULL;
}

/* GBFS auto-discovery: read gbfs.json and return the URL published for
 * `feed_name`. Several operators (nextbike above all) put a LANGUAGE segment in
 * the feed path (/pl/, /sv/, /sr/ …) that differs per system, so the discovery
 * document is the only safe way to build the URL. Any language block is
 * accepted — the feed content is identical, only the labels differ.
 * Returns 1 and fills `out` on success, 0 otherwise. */
static inline int trn_gbfs_discover(const source_ctx *ctx, const char *gbfs_url,
                                    const char *feed_name, char *out, size_t n,
                                    const char *const *hdrs) {
  cJSON *doc = feed_get_json_h(ctx->http, gbfs_url, hdrs, 20000);
  if (!doc) return 0;
  int found = 0;
  cJSON *data = cJSON_GetObjectItem(doc, "data");
  /* GBFS v3 drops the language layer: data.feeds[] directly. v1/v2 nest it
   * under a language key. Try the flat shape first, then every language. */
  cJSON *flat = cJSON_GetObjectItem(data, "feeds");
  cJSON *lang;
  cJSON_ArrayForEach(lang, data) {
    cJSON *feeds = cJSON_IsArray(flat) ? flat
                                       : cJSON_GetObjectItem(lang, "feeds");
    if (!cJSON_IsArray(feeds)) continue;
    cJSON *f;
    cJSON_ArrayForEach(f, feeds) {
      const char *fn = jo_sv(f, "name");
      const char *fu = jo_sv(f, "url");
      if (fn && fu && strcmp(fn, feed_name) == 0) {
        snprintf(out, n, "%s", fu);
        found = 1; break;
      }
    }
    if (found) break;
  }
  cJSON_Delete(doc);
  return found;
}

typedef struct {
  const char *sid;              /* source id — also the uid/remote_key prefix */
  const char *op;               /* operator label, recorded in properties     */
  const char *info_url;         /* station_information                        */
  const char *status_url;       /* station_status, or NULL                    */
  const char *const *hdrs;      /* extra request headers, or NULL             */
} trn_gbfs_cfg;

/* One intel row per GBFS dock. Joins station_status on station_id when a
 * status URL is given (the live counts are only in that second document).
 * Returns the number emitted, or -1 when station_information could not be
 * fetched/parsed. A feed that parsed but listed no stations returns 0 (R3). */
static inline int trn_gbfs_stations(const source_ctx *ctx, intel_sink *sink,
                                    const trn_gbfs_cfg *c) {
  cJSON *info = feed_get_json_h(ctx->http, c->info_url, c->hdrs, 30000);
  if (!info) {
    fprintf(stderr, "[%s] station_information fetch/parse failed\n", c->sid);
    return -1;
  }
  cJSON *sdoc = NULL;
  if (c->status_url)
    sdoc = feed_get_json_h(ctx->http, c->status_url, c->hdrs, 30000);

  cJSON *stations = cJSON_GetObjectItem(
                      cJSON_GetObjectItem(info, "data"), "stations");
  cJSON *sstat = sdoc ? cJSON_GetObjectItem(
                          cJSON_GetObjectItem(sdoc, "data"), "stations") : NULL;

  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, stations) {
    if (!cJSON_IsObject(s)) continue;
    char stid[96];
    trn_idfield(s, "station_id", stid, sizeof stid);
    const char *nm = trn_gbfs_name(s);
    if (!stid[0] && !nm) continue;              /* nothing real to key on */

    /* live counts live in the sibling document, keyed on station_id */
    const cJSON *live = NULL;
    if (cJSON_IsArray(sstat) && stid[0]) {
      cJSON *q;
      cJSON_ArrayForEach(q, sstat) {
        char qid[96];
        trn_idfield(q, "station_id", qid, sizeof qid);
        if (qid[0] && strcmp(qid, stid) == 0) { live = q; break; }
      }
    }

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", c->op);
    trn_put_str(pr, "station_id", stid);
    trn_put_str(pr, "name", nm);
    trn_put_str(pr, "short_name", jo_sv(s, "short_name"));
    trn_put_str(pr, "address", jo_sv(s, "address"));
    trn_put_str(pr, "cross_street", jo_sv(s, "cross_street"));
    trn_put_str(pr, "post_code", jo_sv(s, "post_code"));
    trn_put_num(pr, "capacity", s, "capacity");
    trn_put_bool(pr, "is_charging_station", s, "is_charging_station");
    trn_put_bool(pr, "is_charging", s, "is_charging");
    int virt = trn_bool(s, "is_virtual_station");
    if (virt >= 0) cJSON_AddBoolToObject(pr, "is_virtual_station", virt);
    /* A virtual station is a mandated parking ZONE, not a dock: say so rather
     * than let the point read as survey-grade infrastructure. */
    if (virt == 1) cJSON_AddStringToObject(pr, "geo_precision", "parking-zone");

    char summary[192]; summary[0] = 0;
    char isots[40]; isots[0] = 0;
    if (live) {
      double bikes = 0, docks = 0;
      int haveb = trn_num(live, "num_bikes_available", &bikes) ||
                  trn_num(live, "num_vehicles_available", &bikes);
      int haved = trn_num(live, "num_docks_available", &docks);
      if (haveb) cJSON_AddNumberToObject(pr, "num_vehicles_available", bikes);
      if (haved) cJSON_AddNumberToObject(pr, "num_docks_available", docks);
      trn_put_num(pr, "num_ebikes_available", live, "num_ebikes_available");
      trn_put_bool(pr, "is_renting", live, "is_renting");
      trn_put_bool(pr, "is_returning", live, "is_returning");
      trn_put_bool(pr, "is_installed", live, "is_installed");
      double lr;
      if (trn_num(live, "last_reported", &lr)) {
        trn_iso_from_unix(lr, isots, sizeof isots);
        trn_put_str(pr, "last_reported", isots);
      } else {
        const char *lrs = jo_sv(live, "last_reported");   /* v3: ISO string */
        if (lrs) { snprintf(isots, sizeof isots, "%s", lrs);
                   cJSON_AddStringToObject(pr, "last_reported", lrs); }
      }
      if (haveb && haved)
        snprintf(summary, sizeof summary, "%.0f vehicles / %.0f docks free",
                 bikes, docks);
      else if (haveb)
        snprintf(summary, sizeof summary, "%.0f vehicles available", bikes);
    }
    if (!summary[0]) {
      double cap;
      if (trn_num(s, "capacity", &cap))
        snprintf(summary, sizeof summary, "capacity %.0f", cap);
    }

    char *pj = cJSON_PrintUnformatted(pr);
    char rk[224], title[320];
    snprintf(rk, sizeof rk, "%s|%s", c->op, stid[0] ? stid : (nm ? nm : "?"));
    if (nm) snprintf(title, sizeof title, "%s", nm);
    else    snprintf(title, sizeof title, "%s station %s", c->op, stid);

    intel_item it = {0};
    it.remote_key      = rk;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.published_at    = isots[0] ? isots : NULL;
    it.record_type     = "bike-share-station";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"micromobility\",\"gbfs\"]";
    double la, lo;
    if (trn_num(s, "lat", &la) && trn_num(s, "lon", &lo) &&
        trn_geo_ok(la, lo)) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }

  if (sdoc) cJSON_Delete(sdoc);
  cJSON_Delete(info);
  return n;
}

/* One intel row per free-floating GBFS vehicle (free_bike_status / v3
 * vehicle_status). `cap` bounds how many rows a single run writes — the
 * dockless feeds run to 8,000+ vehicles every few minutes. Returns the number
 * emitted, or -1 on a fetch/parse failure. */
static inline int trn_gbfs_vehicles(const source_ctx *ctx, intel_sink *sink,
                                    const char *sid, const char *op,
                                    const char *url,
                                    const char *const *hdrs, int cap) {
  cJSON *doc = feed_get_json_h(ctx->http, url, hdrs, 30000);
  if (!doc) { fprintf(stderr, "[%s] vehicle feed fetch/parse failed\n", sid);
              return -1; }
  cJSON *data = cJSON_GetObjectItem(doc, "data");
  cJSON *arr = cJSON_GetObjectItem(data, "bikes");
  if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(data, "vehicles");

  int n = 0;
  cJSON *v;
  cJSON_ArrayForEach(v, arr) {
    if (!cJSON_IsObject(v)) continue;
    if (cap > 0 && n >= cap) break;
    double la, lo;
    if (!trn_num(v, "lat", &la) || !trn_num(v, "lon", &lo)) continue;
    if (!trn_geo_ok(la, lo)) continue;          /* no coord → no row (R2) */
    char vid[128];
    trn_idfield(v, "bike_id", vid, sizeof vid);
    if (!vid[0]) trn_idfield(v, "vehicle_id", vid, sizeof vid);
    if (!vid[0]) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", op);
    cJSON_AddStringToObject(pr, "vehicle_id", vid);
    trn_put_str(pr, "vehicle_type_id", jo_sv(v, "vehicle_type_id"));
    trn_put_str(pr, "vehicle_type", jo_sv(v, "vehicle_type"));
    trn_put_bool(pr, "is_reserved", v, "is_reserved");
    trn_put_bool(pr, "is_disabled", v, "is_disabled");
    trn_put_num(pr, "current_range_meters", v, "current_range_meters");
    trn_put_num(pr, "current_fuel_percent", v, "current_fuel_percent");
    cJSON_AddStringToObject(pr, "geo_precision", "vehicle-gps");
    char isots[40]; isots[0] = 0;
    double lr;
    if (trn_num(v, "last_reported", &lr)) {
      trn_iso_from_unix(lr, isots, sizeof isots);
      trn_put_str(pr, "last_reported", isots);
    }
    char *pj = cJSON_PrintUnformatted(pr);

    char rk[192], title[256], summary[128];
    snprintf(rk, sizeof rk, "%s|%s", op, vid);
    snprintf(title, sizeof title, "%s vehicle %s", op, vid);
    double rng;
    if (trn_num(v, "current_range_meters", &rng))
      snprintf(summary, sizeof summary, "range %.0f m", rng);
    else if (trn_num(v, "current_fuel_percent", &rng))
      snprintf(summary, sizeof summary, "battery %.0f%%", rng * 100.0);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = rk;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.published_at    = isots[0] ? isots : NULL;
    it.record_type     = "micromobility-vehicle";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"micromobility\",\"gbfs\"]";
    it.has_geo = 1; it.lat = la; it.lon = lo;
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  return n;
}

/* ---- 511 (iCone/Castle Rock) camera API -------------------------------- */

/* Ontario's 511on.ca and Alberta's 511.alberta.ca expose the identical camera
 * schema (top-level array; Id/Source/SourceId/Roadway/Direction/Latitude/
 * Longitude/Location plus a Views[] array of {Url,Status,Description}), so one
 * emitter serves both. Only views whose Status is "Enabled" carry live imagery,
 * and that is recorded rather than assumed. Returns the number emitted, or -1
 * when the fetch/parse failed. */
static inline int trn_511_cameras(const source_ctx *ctx, intel_sink *sink,
                                  const char *sid, const char *url,
                                  const char *jurisdiction,
                                  const char *tags_json) {
  cJSON *doc = feed_get_json_h(ctx->http, url, NULL, 30000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[%s] fetch/parse failed\n", sid);
    cJSON_Delete(doc);
    return -1;
  }
  int n = 0;
  cJSON *c;
  cJSON_ArrayForEach(c, doc) {
    char cid[64];
    trn_idfield(c, "Id", cid, sizeof cid);
    const char *loc = jo_sv(c, "Location");
    if (!cid[0] && !loc) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", jurisdiction);
    trn_put_str(pr, "camera_id", cid);
    trn_put_str(pr, "source", jo_sv(c, "Source"));
    trn_put_str(pr, "source_id", jo_sv(c, "SourceId"));
    trn_put_str(pr, "roadway", jo_sv(c, "Roadway"));
    trn_put_str(pr, "direction", jo_sv(c, "Direction"));
    trn_put_str(pr, "location", loc);

    cJSON *views = cJSON_CreateArray();
    int live = 0, total = 0;
    cJSON *v;
    cJSON_ArrayForEach(v, cJSON_GetObjectItem(c, "Views")) {
      total++;
      const char *st = jo_sv(v, "Status");
      const char *vu = jo_sv(v, "Url");
      if (!vu) continue;
      if (st && strcasecmp(st, "Enabled") == 0) live++;
      cJSON *o = cJSON_CreateObject();
      cJSON_AddStringToObject(o, "url", vu);
      trn_put_str(o, "status", st);
      trn_put_str(o, "description", jo_sv(v, "Description"));
      cJSON_AddItemToArray(views, o);
    }
    cJSON_AddNumberToObject(pr, "view_count", total);
    cJSON_AddNumberToObject(pr, "views_enabled", live);
    cJSON_AddItemToObject(pr, "views", views);      /* ownership transferred */
    char *pj = cJSON_PrintUnformatted(pr);

    char title[320], summary[96];
    if (loc) snprintf(title, sizeof title, "%s", loc);
    else     snprintf(title, sizeof title, "%s camera %s", jurisdiction, cid);
    snprintf(summary, sizeof summary, "%d of %d views enabled", live, total);

    intel_item it = {0};
    it.remote_key      = cid[0] ? cid : loc;
    it.title           = title;
    it.summary         = summary;
    it.lang            = "en";
    it.record_type     = "traffic-camera";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = tags_json ? tags_json : "[]";
    double la, lo;
    if (trn_num(c, "Latitude", &la) && trn_num(c, "Longitude", &lo) &&
        trn_geo_ok(la, lo)) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  return n;
}

/* ---- Autobahn GmbH (verkehr.autobahn.de) -------------------------------- */

/* Every verkehr.autobahn.de service is per-ROAD, so a collector has to read the
 * road index at /o/autobahn/ first and then fan out. The response envelope is
 * { "<service>": [ ... ] } — the array key equals the service name.
 *
 * `extra` (may be NULL) is called with the properties object and the raw item
 * so a caller can pull service-specific values out of the free-text description
 * lines. Returns the number emitted, or -1 when the road index itself failed. */
static inline int trn_autobahn_service(
    const source_ctx *ctx, intel_sink *sink, const char *sid,
    const char *service, const char *record_type, const char *tags_json,
    void (*extra)(cJSON *pr, const cJSON *item)) {

  cJSON *idx = feed_get_json(ctx->http, "https://verkehr.autobahn.de/o/autobahn/",
                             20000);
  if (!idx) { fprintf(stderr, "[%s] road index fetch failed\n", sid); return -1; }
  cJSON *roads = cJSON_GetObjectItem(idx, "roads");
  if (!cJSON_IsArray(roads)) {
    fprintf(stderr, "[%s] road index had no roads[]\n", sid);
    cJSON_Delete(idx);
    return -1;
  }

  int n = 0;
  cJSON *road;
  cJSON_ArrayForEach(road, roads) {
    if (!cJSON_IsString(road) || !road->valuestring[0]) continue;
    const char *rd = road->valuestring;
    char url[256];
    snprintf(url, sizeof url,
             "https://verkehr.autobahn.de/o/autobahn/%s/services/%s", rd,
             service);
    cJSON *doc = feed_get_json(ctx->http, url, 20000);
    if (!doc) continue;                      /* one bad road is not a failure */
    cJSON *arr = cJSON_GetObjectItem(doc, service);

    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
      const char *ident = jo_sv(item, "identifier");
      const char *title = jo_sv(item, "title");
      if (!ident && !title) continue;

      cJSON *pr = cJSON_CreateObject();
      cJSON_AddStringToObject(pr, "operator", "Autobahn GmbH des Bundes");
      cJSON_AddStringToObject(pr, "road", rd);
      trn_put_str(pr, "identifier", ident);
      trn_put_str(pr, "title", title);
      trn_put_str(pr, "subtitle", jo_sv(item, "subtitle"));
      trn_put_str(pr, "start_timestamp", jo_sv(item, "startTimestamp"));
      trn_put_str(pr, "display_type", jo_sv(item, "display_type"));
      trn_put_bool(pr, "is_blocked", item, "isBlocked");
      trn_put_bool(pr, "future", item, "future");
      {
        const cJSON *imp = cJSON_GetObjectItem(item, "impact");
        if (cJSON_IsObject(imp))
          cJSON_AddItemToObject(pr, "impact", cJSON_Duplicate(imp, 1));
        const cJSON *d = cJSON_GetObjectItem(item, "description");
        if (cJSON_IsArray(d))
          cJSON_AddItemToObject(pr, "description", cJSON_Duplicate(d, 1));
      }
      /* `extent` is the comma string "lat1,lon1,lat2,lon2" — LAT first, which
       * is the opposite of the GeoJSON convention. Kept verbatim so nobody
       * silently reads it as lon,lat. */
      trn_put_str(pr, "extent_lat_lon", jo_sv(item, "extent"));
      if (extra) extra(pr, item);
      char *pj = cJSON_PrintUnformatted(pr);

      char rk[192], tbuf[320];
      snprintf(rk, sizeof rk, "%s|%s", rd, ident ? ident : (title ? title : "?"));
      snprintf(tbuf, sizeof tbuf, "%s", title ? title : ident);

      intel_item it = {0};
      it.remote_key      = rk;
      it.title           = tbuf;
      it.summary         = jo_sv(item, "subtitle");
      it.lang            = "de";
      it.published_at    = jo_sv(item, "startTimestamp");
      it.record_type     = record_type;
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = tags_json;
      /* coordinate.lat / coordinate.long are STRINGS on some services and
       * numbers on others; trn_num coerces both. Note the key is "long". */
      const cJSON *co = cJSON_GetObjectItem(item, "coordinate");
      double la, lo;
      if (co && trn_num(co, "lat", &la) && trn_num(co, "long", &lo) &&
          trn_geo_ok(la, lo)) { it.has_geo = 1; it.lat = la; it.lon = lo; }
      if (sink->emit(sink, &it) >= 0) n++;
      free(pj);
      cJSON_Delete(pr);
    }
    cJSON_Delete(doc);
  }
  cJSON_Delete(idx);
  return n;
}

/* Pull "<label>: <number>" out of the German free-text description lines that
 * the parking/charging services use instead of structured fields. */
static inline int trn_desc_number(const cJSON *item, const char *label,
                                  double *out) {
  const cJSON *d = cJSON_GetObjectItem(item, "description");
  const cJSON *line;
  size_t ll = strlen(label);
  cJSON_ArrayForEach(line, d) {
    if (!cJSON_IsString(line) || !line->valuestring) continue;
    const char *s = line->valuestring;
    if (strncasecmp(s, label, ll) != 0) continue;
    const char *p = s + ll;
    while (*p == ' ' || *p == ':') p++;
    char *end = NULL;
    double v = strtod(p, &end);
    if (end && end != p && isfinite(v)) { *out = v; return 1; }
  }
  return 0;
}

/* First description line that starts with `label`, verbatim (malloc'd). */
static inline char *trn_desc_line(const cJSON *item, const char *label) {
  const cJSON *d = cJSON_GetObjectItem(item, "description");
  const cJSON *line;
  size_t ll = strlen(label);
  cJSON_ArrayForEach(line, d) {
    if (!cJSON_IsString(line) || !line->valuestring) continue;
    if (strncasecmp(line->valuestring, label, ll) != 0) continue;
    const char *p = line->valuestring + ll;
    while (*p == ' ' || *p == ':') p++;
    return p[0] ? strdup(p) : NULL;
  }
  return NULL;
}

#endif /* TRN_COMMON_INC */
