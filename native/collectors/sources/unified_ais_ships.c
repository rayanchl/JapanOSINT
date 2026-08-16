/* collectors/transport/sources/unified_ais_ships.c
 * Port of server/src/collectors/unifiedAisShips.js — the BESPOKE unified
 * collector (custom mmsi/imo/name+coord fusion, freshest-position-wins +
 * field-merge; does NOT use createUnifiedCollector). Upstreams captured via
 * lib/unified.h's capture sink in the JS Promise.allSettled order
 * (marine-traffic, vessel-finder, maritime-ais). JS keeps `_freshness` in
 * properties then strips it on output; we instead track freshness in the
 * slot table so it never enters properties (equivalent, cleaner). _meta
 * dropped per RULE 8; emitted via the geojson sink (uid keys off
 * properties.id). */
#include "source.h"
#include "lib/unified.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* JS parseTs: Date.parse(v) (NaN→0). Tolerant ISO-8601 → epoch seconds
 * (only relative ordering / Math.max matter; units only need consistency). */
static long parse_ts(const char *v) {
  if (!v || !*v) return 0;
  struct tm tm; memset(&tm, 0, sizeof tm);
  char *e = strptime(v, "%Y-%m-%dT%H:%M:%S", &tm);
  if (!e) { memset(&tm, 0, sizeof tm); e = strptime(v, "%Y-%m-%d %H:%M:%S", &tm); }
  if (!e) { memset(&tm, 0, sizeof tm); e = strptime(v, "%Y-%m-%d", &tm); }
  if (!e) return 0;
  return (long)timegm(&tm);
}

/* JS truthy of p.a || p.A, then String(v).trim(); NULL if falsy. */
static const char *idval(cJSON *p, const char *a, const char *b,
                         char *buf, size_t n) {
  cJSON *v = cJSON_GetObjectItem(p, a);
  if (!(cJSON_IsString(v) && v->valuestring[0]) &&
      !(cJSON_IsNumber(v) && v->valuedouble != 0))
    v = cJSON_GetObjectItem(p, b);
  if (cJSON_IsString(v) && v->valuestring[0]) {
    snprintf(buf, n, "%s", v->valuestring);
  } else if (cJSON_IsNumber(v) && v->valuedouble != 0) {
    if (v->valuedouble == (double)(long long)v->valuedouble)
      snprintf(buf, n, "%lld", (long long)v->valuedouble);
    else snprintf(buf, n, "%g", v->valuedouble);
  } else return NULL;
  char *s = buf; while (*s==' '||*s=='\t'||*s=='\n'||*s=='\r') s++;
  if (s != buf) memmove(buf, s, strlen(s) + 1);
  size_t L = strlen(buf);
  while (L && (buf[L-1]==' '||buf[L-1]=='\t'||buf[L-1]=='\n'||buf[L-1]=='\r'))
    buf[--L] = 0;
  return buf[0] ? buf : NULL;
}

static const char *pstr(cJSON *p, const char *k) {
  cJSON *v = cJSON_GetObjectItem(p, k);
  return (cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static void add_source(cJSON *arr, const char *s) {
  if (!s || !*s) return;
  cJSON *e;
  cJSON_ArrayForEach(e, arr)
    if (cJSON_IsString(e) && !strcmp(e->valuestring, s)) return;
  cJSON_AddItemToArray(arr, cJSON_CreateString(s));
}

typedef struct { char key[320]; cJSON *feat; long fresh; } slot_t;

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *raw = cJSON_CreateArray();
  unified_capture(ctx, "marine-traffic", raw);   /* JS allSettled order */
  unified_capture(ctx, "vessel-finder",  raw);
  unified_capture(ctx, "maritime-ais",   raw);

  int cap = 256, ns = 0;
  slot_t *S = malloc(sizeof(slot_t) * cap);
  cJSON *result = cJSON_CreateArray();            /* orderedKeys order */

  cJSON *f;
  cJSON_ArrayForEach(f, raw) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    if (!p) continue;
    char kb[320], mb[128], ib[128];
    const char *mmsi = idval(p, "mmsi", "MMSI", mb, sizeof mb);
    const char *imo  = idval(p, "imo",  "IMO",  ib, sizeof ib);
    if (mmsi) snprintf(kb, sizeof kb, "mmsi:%s", mmsi);
    else if (imo) snprintf(kb, sizeof kb, "imo:%s", imo);
    else {
      cJSON *co = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
      cJSON *x = co ? cJSON_GetArrayItem(co, 0) : NULL;  /* exhaustive-ok: [lon,lat] tuple */
      cJSON *y = co ? cJSON_GetArrayItem(co, 1) : NULL;
      if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y)) continue;
      const char *nm = pstr(p, "vessel_name");
      if (!nm) nm = pstr(p, "name");
      char nb[256]; unified_norm_name(nm, nb, sizeof nb);
      snprintf(kb, sizeof kb, "nc:%s:%.3f,%.3f", nb,
               x->valuedouble, y->valuedouble);
    }

    long ts = parse_ts(pstr(p, "last_position_update"));
    int idx = -1;
    for (int i = 0; i < ns; i++) if (!strcmp(S[i].key, kb)) { idx = i; break; }

    if (idx < 0) {                                /* first occurrence */
      cJSON *nf = cJSON_CreateObject();
      cJSON_AddStringToObject(nf, "type", "Feature");
      cJSON_AddItemToObject(nf, "geometry",
                            g ? cJSON_Duplicate(g, 1) : cJSON_CreateNull());
      cJSON *np = cJSON_Duplicate(p, 1);
      cJSON *srcs = cJSON_CreateArray();
      add_source(srcs, pstr(p, "source"));        /* [p.source].filter(Boolean) */
      cJSON_DeleteItemFromObject(np, "sources");
      cJSON_AddItemToObject(np, "sources", srcs);
      cJSON_AddItemToObject(nf, "properties", np);
      if (ns == cap) { cap *= 2; S = realloc(S, sizeof(slot_t) * cap); }
      snprintf(S[ns].key, sizeof S[ns].key, "%s", kb);
      S[ns].feat = nf; S[ns].fresh = ts; ns++;
      cJSON_AddItemToArray(result, nf);           /* result owns nf */
      continue;
    }

    /* merge: freshest last_position_update wins, then fill-null + sources.
     * Mutate the existing feature in place (preserves orderedKeys order). */
    cJSON *ex  = S[idx].feat;
    cJSON *exp = cJSON_GetObjectItem(ex, "properties");
    long exTs  = S[idx].fresh;
    int candWins = ts >= exTs;
    cJSON *win_props = candWins ? p : exp;        /* loser = the other */
    cJSON *los_props = candWins ? exp : p;
    cJSON *win_geom  = candWins ? g  : cJSON_GetObjectItem(ex, "geometry");

    cJSON *merged = cJSON_Duplicate(win_props, 1);
    cJSON *kv;
    cJSON_ArrayForEach(kv, los_props) {           /* fill nulls from loser */
      cJSON *cur = cJSON_GetObjectItem(merged, kv->string);
      if ((!cur || cJSON_IsNull(cur)) && !cJSON_IsNull(kv)) {
        if (cur) cJSON_DeleteItemFromObject(merged, kv->string);
        cJSON_AddItemToObject(merged, kv->string, cJSON_Duplicate(kv, 1));
      }
    }
    /* sources = Set(existing.sources) ∪ p.source ∪ winner.source */
    cJSON *srcs = cJSON_CreateArray();
    cJSON *exsrc = cJSON_GetObjectItem(exp, "sources"), *se;
    if (cJSON_IsArray(exsrc))
      cJSON_ArrayForEach(se, exsrc)
        if (cJSON_IsString(se)) add_source(srcs, se->valuestring);
    add_source(srcs, pstr(p, "source"));
    add_source(srcs, pstr(win_props, "source"));
    cJSON_DeleteItemFromObject(merged, "sources");
    cJSON_AddItemToObject(merged, "sources", srcs);

    cJSON *gdup = win_geom ? cJSON_Duplicate(win_geom, 1) : cJSON_CreateNull();
    cJSON_ReplaceItemInObject(ex, "properties", merged);  /* frees old exp */
    cJSON_ReplaceItemInObject(ex, "geometry", gdup);
    S[idx].fresh = (ts > exTs ? ts : exTs);               /* Math.max */
  }

  free(S);
  cJSON_Delete(raw);
  int n = geojson_emit_features(sink, "unified-ais-ships", result);
  cJSON_Delete(result);
  fprintf(stderr, "[unified-ais-ships] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def unified_ais_ships_def = {
  .id = "unified-ais-ships", .collector = "transport",
  .name = "Unified AIS Ships (fused)", .name_ja = "Unified AIS Ships",
   .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(unified_ais_ships_def)
