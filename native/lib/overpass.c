/* lib/overpass.c — port of _liveHelpers.js Overpass helpers. See overpass.h. */
#include "overpass.h"
#include "geojson.h"
#include "../core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

/* OVERPASS_ENDPOINTS, exact order. */
static const char *ENDPOINTS[] = {
  "https://overpass-api.de/api/interpreter",
  "https://overpass.kumi.systems/api/interpreter",
  "https://overpass.openstreetmap.ru/api/interpreter",
  "https://overpass.private.coffee/api/interpreter",
};
#define N_ENDPOINTS 4

/* JAPAN_TILES [south,west,north,east], exact values from _liveHelpers.js. */
static const double TILES[12][4] = {
  {41.35, 139.33, 45.55, 145.85}, {41.35, 139.33, 45.55, 148.00},
  {37.73, 139.33, 41.60, 142.05}, {34.55, 138.50, 37.80, 141.30},
  {33.80, 135.20, 37.80, 138.60}, {33.10, 131.90, 35.50, 135.30},
  {31.50, 129.80, 34.40, 132.40}, {31.20, 130.20, 33.80, 132.30},
  {32.50, 132.00, 34.60, 134.80}, {24.00, 122.80, 28.50, 131.60},
  {26.00, 140.80, 35.00, 142.30}, {33.00, 128.00, 36.00, 130.50},
};

/* encodeURIComponent: keep A-Za-z0-9 and - _ . ! ~ * ' ( ) ; %-encode rest. */
static char *uri_encode(const char *s) {
  static const char *keep = "-_.!~*'()";
  size_t L = strlen(s);
  char *o = malloc(L * 3 + 1);
  if (!o) return NULL;
  char *w = o;
  for (size_t i = 0; i < L; i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(keep, c))
      *w++ = (char)c;
    else { sprintf(w, "%%%02X", c); w += 3; }
  }
  *w = 0;
  return o;
}

/* POST data=<urlencoded query>; return parsed JSON doc or NULL. */
static cJSON *overpass_post(http_client *http, const char *endpoint,
                            const char *query, int timeout_ms) {
  char *enc = uri_encode(query);
  if (!enc) return NULL;
  size_t blen = strlen(enc) + 5;
  char *body = malloc(blen + 1);
  if (!body) { free(enc); return NULL; }
  snprintf(body, blen + 1, "data=%s", enc);
  free(enc);
  const char *hdrs[] = {
    "Content-Type: application/x-www-form-urlencoded",
    "User-Agent: JapanOSINT/1.0 (+https://github.com/rayanchl/JapanOSINT)",
    NULL };
  http_response r = {0};
  int rc = http_request(http, "POST", endpoint, hdrs, body, strlen(body),
                        timeout_ms > 0 ? timeout_ms : 60000, 0, &r);
  free(body);
  cJSON *doc = NULL;
  if (rc == 0 && r.status >= 200 && r.status < 300 && r.body)
    doc = cJSON_Parse(r.body);
  http_response_free(&r);
  return doc;
}

const char *ov_tag(cJSON *el, const char *k) {
  cJSON *tags = el ? cJSON_GetObjectItem(el, "tags") : NULL;
  cJSON *v = tags ? cJSON_GetObjectItem(tags, k) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int num_ok(cJSON *v) { return v && cJSON_IsNumber(v); }

/* el.lat!=null ? [el.lon,el.lat] : [el.center.lon,el.center.lat]; 0 if none. */
static int el_coords(cJSON *el, double *lon, double *lat) {
  cJSON *la = cJSON_GetObjectItem(el, "lat");
  cJSON *lo = cJSON_GetObjectItem(el, "lon");
  if (num_ok(la) && num_ok(lo)) { *lat = la->valuedouble; *lon = lo->valuedouble; return 1; }
  cJSON *c = cJSON_GetObjectItem(el, "center");
  if (c) {
    cJSON *cla = cJSON_GetObjectItem(c, "lat");
    cJSON *clo = cJSON_GetObjectItem(c, "lon");
    if (num_ok(cla) && num_ok(clo)) { *lat = cla->valuedouble; *lon = clo->valuedouble; return 1; }
  }
  return 0;
}

/* data.elements filtered to those with usable coords (overpassRequest). */
static cJSON *filter_point_elements(cJSON *doc) {
  cJSON *els = doc ? cJSON_GetObjectItem(doc, "elements") : NULL;
  if (!els || !cJSON_IsArray(els)) return NULL;
  cJSON *out = cJSON_CreateArray();
  cJSON *e;
  cJSON_ArrayForEach(e, els) {
    double lo, la;
    if (el_coords(e, &lo, &la)) cJSON_AddItemToArray(out, cJSON_Duplicate(e, 1));
  }
  return out;
}

/* data.elements filtered to ways with geometry[>=2] (overpassWayRequest). */
static cJSON *filter_way_elements(cJSON *doc) {
  cJSON *els = doc ? cJSON_GetObjectItem(doc, "elements") : NULL;
  if (!els || !cJSON_IsArray(els)) return NULL;
  cJSON *out = cJSON_CreateArray();
  cJSON *e;
  cJSON_ArrayForEach(e, els) {
    cJSON *t = cJSON_GetObjectItem(e, "type");
    cJSON *g = cJSON_GetObjectItem(e, "geometry");
    if (t && cJSON_IsString(t) && strcmp(t->valuestring, "way") == 0 &&
        g && cJSON_IsArray(g) && cJSON_GetArraySize(g) >= 2)
      cJSON_AddItemToArray(out, cJSON_Duplicate(e, 1));
  }
  return out;
}

/* "type/id" dedupe key into buf; 0 if not derivable (always kept). */
static int el_key(cJSON *el, char *buf, size_t n) {
  cJSON *t = cJSON_GetObjectItem(el, "type");
  cJSON *id = cJSON_GetObjectItem(el, "id");
  /* The range test guards the cast, which is undefined outside long long —
   * an OSM id is far below 2^63, but this parses whatever the endpoint (or
   * whatever is answering as the endpoint) returned. An id we cannot render
   * is simply "not derivable", the existing meaning of returning 0: the
   * element is kept rather than deduped against a bogus key. */
  if (t && cJSON_IsString(t) && id && cJSON_IsNumber(id) &&
      id->valuedouble >= (double)LLONG_MIN &&
      id->valuedouble <  (double)LLONG_MAX) {
    snprintf(buf, n, "%s/%lld", t->valuestring, (long long)id->valuedouble);
    return 1;
  }
  return 0;
}

/* Dedupe membership test. This was a linear scan over an array that tiled
 * queries grow to ~200k entries, making dedupe O(n²) — a timeout cause with no
 * network involved at all. An FNV-1a bloom-ish prefilter keeps the exact
 * strcmp path (so it stays correct) but skips it for the overwhelming majority
 * of non-matches. */
static unsigned long seen_hash(const char *s) {
  unsigned long h = 1469598103934665603UL;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    h ^= *p; h *= 1099511628211UL;
  }
  return h;
}

#define SEEN_FILTER_BITS 20                 /* 1 Mbit = 128 KB, ~1M slots */
#define SEEN_FILTER_MASK ((1UL << SEEN_FILTER_BITS) - 1)
#define SEEN_FILTER_BYTES (1UL << (SEEN_FILTER_BITS - 3))

/* PER-CALL, not file-scope. This filter used to be a `static unsigned char *`
 * shared by the whole process, reset with a memset at the top of each tiled
 * run. That was safe only while the scheduler ran one source at a time. Now
 * that core/scheduler.c is a worker pool, ~90 Overpass collectors can be in
 * flight together, and a global would break in two ways that both END IN
 * DUPLICATE ROWS — the exact thing this dedupe exists to prevent:
 *
 *   1. One collector's start-of-run memset clears bits another collector is
 *      relying on mid-run. A CLEARED bit is a false NEGATIVE: seen_has()
 *      short-circuits to "not seen" without reaching the authoritative
 *      strcmp, so the duplicate is appended.
 *   2. `byte |= bit` is a non-atomic read-modify-write. Two threads setting
 *      different bits in the same byte can lose one, with the same effect.
 *
 * (A stale or cross-contaminated bit in the OTHER direction is harmless — a
 * false positive just falls through to the exact strcmp. Only lost bits hurt.)
 *
 * Making it per-call removes the global rather than locking it, which is both
 * simpler and strictly better: no cross-source contamination, no lock on the
 * hot path, and the filter is naturally fresh each run instead of relying on
 * a reset that the non-tiled path never performed at all. */
typedef struct {
  char          **keys;      /* exact-match backing array (authoritative) */
  int             n;
  unsigned char  *filter;    /* NULL => prefilter disabled, still correct */
} seen_set;

static int seen_has(const seen_set *s, const char *k) {
  unsigned long h = seen_hash(k) & SEEN_FILTER_MASK;
  if (s->filter && !(s->filter[h >> 3] & (1u << (h & 7)))) return 0;
  for (int i = 0; i < s->n; i++) if (strcmp(s->keys[i], k) == 0) return 1;
  return 0;
}

/* Call after appending k to the set so the prefilter knows about it. */
static void seen_mark(seen_set *s, const char *k) {
  if (!s->filter) return;
  unsigned long h = seen_hash(k) & SEEN_FILTER_MASK;
  s->filter[h >> 3] |= (unsigned char)(1u << (h & 7));
}

/* Map point elements via `map` into a features array; emit through geojson. */
static int emit_points(const source_ctx *ctx, intel_sink *sink,
                       cJSON *elements, overpass_map map, void *ud) {
  cJSON *features = cJSON_CreateArray();
  int i = 0;
  cJSON *el;
  cJSON_ArrayForEach(el, elements) {
    double lo, la;
    if (!el_coords(el, &lo, &la)) continue;
    cJSON *f = map(el, i, lo, la, ud);
    if (f) cJSON_AddItemToArray(features, f);
    i++;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  return n;
}

/* The fetch half of overpass_collect: build the nationwide query, try each
 * endpoint until one returns point elements. Returns the elements array (caller
 * frees) or NULL if every endpoint failed. Split out so overpass_collect and
 * overpass_collect_via share one fetch path and can never drift. */
static cJSON *collect_fetch(const source_ctx *ctx, const char *body,
                            int query_timeout, int timeout_ms, int *fetched_ok) {
  if (fetched_ok) *fetched_ok = 0;
  int qt = query_timeout > 0 ? query_timeout : 180;
  size_t qlen = strlen(body) + 160;
  char *query = malloc(qlen);
  if (!query) return NULL;
  snprintf(query, qlen,
    "[out:json][timeout:%d];area[\"ISO3166-1\"=\"JP\"][admin_level=2]->.jp;(%s);out center;",
    qt, body);

  /* Overpass genuinely needs more than a minute for nationwide queries —
   * measured 100-162 s for real results — so the old flat 60 s per attempt was
   * read as "the mirror is down" when it was merely slow. But four endpoints ×
   * a long per-attempt timeout is an unbounded serial walk, which is what blew
   * whole scheduler slots. Keep a generous per-attempt timeout AND an overall
   * budget across all endpoints. Both overridable for slow/fast hosts. */
  const char *penv = getenv("JO_OVERPASS_ATTEMPT_MS");
  const char *tenv = getenv("JO_OVERPASS_TOTAL_MS");
  int attempt_ms = timeout_ms > 0 ? timeout_ms : (penv ? atoi(penv) : 180000);
  int total_ms   = tenv ? atoi(tenv) : 300000;
  if (attempt_ms <= 0) attempt_ms = 180000;
  if (total_ms   <= 0) total_ms   = 300000;

  struct timespec t0;
  clock_gettime(CLOCK_MONOTONIC, &t0);

  cJSON *elements = NULL;
  for (int e = 0; e < N_ENDPOINTS; e++) {
    struct timespec tn;
    clock_gettime(CLOCK_MONOTONIC, &tn);
    long spent = (tn.tv_sec - t0.tv_sec) * 1000 + (tn.tv_nsec - t0.tv_nsec) / 1000000;
    long left = total_ms - spent;
    if (left < 10000) {                 /* not enough left to be worth a try */
      fprintf(stderr, "[overpass] %s budget exhausted after %ld ms (%d/%d endpoints)\n",
              ctx->source_id, spent, e, N_ENDPOINTS);
      break;
    }
    int this_ms = attempt_ms < (int)left ? attempt_ms : (int)left;
    cJSON *doc = overpass_post(ctx->http, ENDPOINTS[e], query, this_ms);
    /* A parseable doc means the mirror answered. Zero elements after that is an
     * HONEST EMPTY, not a failure — conflating the two made every quiet query
     * return -1 and quarantine a working source. */
    if (doc && fetched_ok) *fetched_ok = 1;
    cJSON *els = filter_point_elements(doc);
    if (doc) cJSON_Delete(doc);
    if (els && cJSON_GetArraySize(els) > 0) { elements = els; break; }
    if (els) cJSON_Delete(els);
  }
  free(query);
  return elements;
}

int overpass_collect(const source_ctx *ctx, intel_sink *sink,
                     const char *body, int query_timeout, int timeout_ms,
                     overpass_map map, void *ud) {
  int fetched_ok = 0;
  cJSON *elements = collect_fetch(ctx, body, query_timeout, timeout_ms, &fetched_ok);
  if (!elements) {
    /* rc is a STATUS, not a count: core/scheduler.c treats non-zero as
     * status="error" and feeds anomaly_detect(). A query that ran fine and
     * matched nothing must return 0, or the source is quarantined for being
     * quiet. -1 is reserved for "no mirror answered at all". */
    fprintf(stderr, "[overpass] %s %s\n", ctx->source_id,
            fetched_ok ? "0 elements (honest empty)" : "no endpoint answered");
    return fetched_ok ? 0 : -1;
  }
  int n = emit_points(ctx, sink, elements, map, ud);
  cJSON_Delete(elements);
  fprintf(stderr, "[overpass] %s emitted %d\n", ctx->source_id, n);
  return n;
}

int overpass_collect_via(const source_ctx *ctx, const char *body,
                         int query_timeout, int timeout_ms,
                         overpass_map map, void *ud,
                         overpass_emit emit, void *eud) {
  if (!emit) return -1;
  int fetched_ok = 0;
  cJSON *elements = collect_fetch(ctx, body, query_timeout, timeout_ms, &fetched_ok);
  if (!elements) {
    fprintf(stderr, "[overpass] %s %s\n", ctx->source_id,
            fetched_ok ? "0 elements (honest empty)" : "no endpoint answered");
    return fetched_ok ? 0 : -1;
  }
  int n = 0, i = 0;
  cJSON *el;
  cJSON_ArrayForEach(el, elements) {
    double lo, la;
    if (!el_coords(el, &lo, &la)) continue;
    cJSON *f = map(el, i, lo, la, ud);
    i++;
    if (!f) continue;
    if (emit(f, eud) >= 0) n++;
    cJSON_Delete(f);            /* consumer borrows; toolkit owns the free */
  }
  cJSON_Delete(elements);
  fprintf(stderr, "[overpass] %s emitted %d\n", ctx->source_id, n);
  return n;
}

/* Shared tile fanout for point (out center) and way (out geom) queries. */
static cJSON *tiled_fetch(const source_ctx *ctx, overpass_bodyfn bodyfn,
                          int query_timeout, int timeout_ms, int ways,
                          void *ud, int *fetched_ok) {
  if (fetched_ok) *fetched_ok = 0;
  int qt = query_timeout > 0 ? query_timeout : 180;
  /* Same budget as collect_fetch — and this path needs it more: 12 tiles x 4
   * endpoints at 60-120 s each is 48-96 minutes inside one scheduler slot if
   * every mirror is slow. The single-query path was given a deadline first;
   * this one was missed, which left the worst case unbounded. */
  const char *tenv = getenv("JO_OVERPASS_TOTAL_MS");
  int total_ms = tenv ? atoi(tenv) : 300000;
  if (total_ms <= 0) total_ms = 300000;
  struct timespec t0;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  cJSON *all = cJSON_CreateArray();
  /* Owned by this call: no other collector can see or clear it. A failed
   * filter calloc is not fatal — seen_has falls back to the exact strcmp
   * scan, which is slower but still correct. */
  seen_set seen = { .keys = calloc(200000, sizeof(char *)), .n = 0,
                    .filter = calloc(SEEN_FILTER_BYTES, 1) };
  if (!seen.keys) { free(seen.filter); cJSON_Delete(all); return NULL; }
  for (int i = 0; i < 12; i++) {
    char bbox[96];
    snprintf(bbox, sizeof bbox, "%g,%g,%g,%g",
             TILES[i][0], TILES[i][1], TILES[i][2], TILES[i][3]);
    char tbody[4096];
    bodyfn(bbox, tbody, sizeof tbody, ud);
    size_t qlen = strlen(tbody) + 96;
    char *q = malloc(qlen);
    if (!q) continue;
    snprintf(q, qlen, "[out:json][timeout:%d];(%s);out %s;",
             qt, tbody, ways ? "geom" : "center");
    cJSON *got = NULL;
    for (int ei = 0; ei < N_ENDPOINTS; ei++) {
      struct timespec tn;
      clock_gettime(CLOCK_MONOTONIC, &tn);
      long spent = (tn.tv_sec - t0.tv_sec) * 1000 + (tn.tv_nsec - t0.tv_nsec) / 1000000;
      long left = total_ms - spent;
      if (left < 10000) {
        fprintf(stderr, "[overpass] %s tiled budget exhausted after %ld ms "
                        "(tile %d/12, endpoint %d/%d)\n",
                ctx->source_id, spent, i + 1, ei, N_ENDPOINTS);
        free(q);
        goto done;                 /* keep whatever tiles already succeeded */
      }
      int want = timeout_ms > 0 ? timeout_ms : (ways ? 120000 : 60000);
      int this_ms = want < (int)left ? want : (int)left;
      const char *ep = ENDPOINTS[(ei + i) % N_ENDPOINTS];
      cJSON *doc = overpass_post(ctx->http, ep, q, this_ms);
      /* A parseable doc means a mirror answered; zero elements after that is an
       * honest empty for this tile, not a failure. */
      if (doc && fetched_ok) *fetched_ok = 1;
      got = ways ? filter_way_elements(doc) : filter_point_elements(doc);
      if (doc) cJSON_Delete(doc);
      if (got && cJSON_GetArraySize(got) > 0) break;
      if (got) { cJSON_Delete(got); got = NULL; }
    }
    free(q);
    if (!got) continue;
    cJSON *el;
    cJSON_ArrayForEach(el, got) {
      char k[64];
      if (el_key(el, k, sizeof k)) {
        if (seen_has(&seen, k)) continue;
        if (seen.n < 200000) { seen.keys[seen.n++] = strdup(k); seen_mark(&seen, k); }
      }
      cJSON_AddItemToArray(all, cJSON_Duplicate(el, 1));
    }
    cJSON_Delete(got);
  }
done:
  for (int i = 0; i < seen.n; i++) free(seen.keys[i]);
  free(seen.keys);
  free(seen.filter);
  if (cJSON_GetArraySize(all) == 0) { cJSON_Delete(all); return NULL; }
  return all;
}

int overpass_tiled_collect(const source_ctx *ctx, intel_sink *sink,
                           overpass_bodyfn bodyfn, int query_timeout,
                           int timeout_ms, overpass_map map, void *ud) {
  int fetched_ok = 0;
  cJSON *all = tiled_fetch(ctx, bodyfn, query_timeout, timeout_ms, 0, ud, &fetched_ok);
  if (!all) {
    fprintf(stderr, "[overpass] %s %s\n", ctx->source_id,
            fetched_ok ? "0 elements (honest empty)" : "no endpoint answered");
    return fetched_ok ? 0 : -1;
  }
  int n = emit_points(ctx, sink, all, map, ud);
  cJSON_Delete(all);
  fprintf(stderr, "[overpass-tiled] %s emitted %d\n", ctx->source_id, n);
  return n;
}

int overpass_ways_collect(const source_ctx *ctx, intel_sink *sink,
                          overpass_bodyfn bodyfn, int query_timeout,
                          int timeout_ms, overpass_way_map map, void *ud) {
  int fetched_ok = 0;
  cJSON *all = tiled_fetch(ctx, bodyfn, query_timeout, timeout_ms, 1, ud, &fetched_ok);
  if (!all) {
    fprintf(stderr, "[overpass] %s %s\n", ctx->source_id,
            fetched_ok ? "0 ways (honest empty)" : "no endpoint answered");
    return fetched_ok ? 0 : -1;
  }
  cJSON *features = cJSON_CreateArray();
  int i = 0;
  cJSON *el;
  cJSON_ArrayForEach(el, all) {
    cJSON *geom = cJSON_GetObjectItem(el, "geometry");
    cJSON *coords = cJSON_CreateArray();
    cJSON *nd;
    cJSON_ArrayForEach(nd, geom) {
      cJSON *la = cJSON_GetObjectItem(nd, "lat");
      cJSON *lo = cJSON_GetObjectItem(nd, "lon");
      if (num_ok(la) && num_ok(lo)) {
        cJSON *pair = cJSON_CreateArray();
        cJSON_AddItemToArray(pair, cJSON_CreateNumber(lo->valuedouble));
        cJSON_AddItemToArray(pair, cJSON_CreateNumber(la->valuedouble));
        cJSON_AddItemToArray(coords, pair);
      }
    }
    if (cJSON_GetArraySize(coords) < 2) { cJSON_Delete(coords); i++; continue; }
    cJSON *f = map(el, i, coords, ud);
    cJSON_Delete(coords);
    if (f) cJSON_AddItemToArray(features, f);
    i++;
  }
  cJSON_Delete(all);
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[overpass-ways] %s emitted %d\n", ctx->source_id, n);
  return n;
}
