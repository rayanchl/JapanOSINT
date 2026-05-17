/* lib/unified.c — see header. Faithful port of unifiedCollectorTemplate.js
 * + _dedupe.js. */
#include "unified.h"
#include "geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

/* ── capture sink: emitted intel_item → Feature into a cJSON array ──────── */
typedef struct { cJSON *arr; } cap_ctx;

static int cap_emit(struct intel_sink *s, const intel_item *it) {
  cap_ctx *c = (cap_ctx *)s->ctx;
  cJSON *g = (it->geometry_geojson && *it->geometry_geojson)
               ? cJSON_Parse(it->geometry_geojson) : NULL;
  cJSON *p = (it->properties_json && *it->properties_json)
               ? cJSON_Parse(it->properties_json) : NULL;
  if (!g || !p) { cJSON_Delete(g); cJSON_Delete(p); return 0; }
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON_AddItemToObject(f, "geometry", g);
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(c->arr, f);
  return 1;
}

/* ── normName (pragmatic; no ICU NFKC) ─────────────────────────────────── */
static int strip_prefix(const char *s, const char *lit) {
  size_t n = strlen(lit);
  return strncasecmp(s, lit, n) == 0 ? (int)n : 0;
}
void unified_norm_name(const char *raw, char *out, size_t n) {
  size_t o = 0;
  if (!raw) { if (n) out[0] = 0; return; }
  /* JS: .replace(/駅|station|停留所|バス停|stop/gi,'')
   *     .replace(/[\s・\-()（）、,.]/g,'') ; toLowerCase first. */
  static const char *DROP[] = { "駅","停留所","バス停","station","stop",
    "・","、","（","）"," ","\t","\n","\r","-","(",")",",",".", NULL };
  for (const char *s = raw; *s && o + 4 < n; ) {
    int matched = 0;
    for (int i = 0; DROP[i]; i++) {
      int k = strip_prefix(s, DROP[i]);
      if (k) { s += k; matched = 1; break; }
    }
    if (matched) continue;
    unsigned char ch = (unsigned char)*s;
    out[o++] = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : (char)ch;
    s++;
  }
  out[o] = 0;
}

/* JS Number.toFixed(p) — %.*f (round-half-to-even libc; post-parity ok) */
static void to_fixed(double v, int p, char *out, size_t n) {
  snprintf(out, n, "%.*f", p, v);
}

/* ── tiny string→index hashmap for dedupe slots ────────────────────────── */
typedef struct { char *key; int idx; } hslot;
typedef struct { hslot *s; size_t cap, cnt; } hmap;

static unsigned long fnv1a(const char *s) {
  unsigned long h = 1469598103934665603UL;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211UL; }
  return h;
}
static void hm_init(hmap *m) { m->cap = 1024; m->cnt = 0;
  m->s = calloc(m->cap, sizeof(hslot)); }
static int *hm_slot(hmap *m, const char *key) {       /* &idx or NULL-new */
  if (m->cnt * 2 >= m->cap) {                          /* grow */
    size_t nc = m->cap * 2; hslot *ns = calloc(nc, sizeof(hslot));
    for (size_t i = 0; i < m->cap; i++) if (m->s[i].key) {
      size_t j = fnv1a(m->s[i].key) & (nc - 1);
      while (ns[j].key) j = (j + 1) & (nc - 1);
      ns[j] = m->s[i];
    }
    free(m->s); m->s = ns; m->cap = nc;
  }
  size_t j = fnv1a(key) & (m->cap - 1);
  while (m->s[j].key) {
    if (!strcmp(m->s[j].key, key)) return &m->s[j].idx;
    j = (j + 1) & (m->cap - 1);
  }
  m->s[j].key = strdup(key); m->s[j].idx = -1; m->cnt++;
  return &m->s[j].idx;
}
static void hm_free(hmap *m) {
  for (size_t i = 0; i < m->cap; i++) free(m->s[i].key);
  free(m->s);
}

/* dedupeByKeys port: first keyfn nonempty → key; else coord-grid; merge
 * fill-null + sources[]. */
static cJSON *dedupe(cJSON *raw, const unified_keyfn *kf, int nk, int prec) {
  cJSON *kept = cJSON_CreateArray();
  hmap m; hm_init(&m);
  cJSON *f;
  cJSON_ArrayForEach(f, raw) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    char kb[512]; const char *key = NULL;
    for (int i = 0; i < nk && !key; i++) key = kf[i](f, kb, sizeof kb);
    char cbuf[640];
    if (!key) {
      cJSON *co = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
      cJSON *x = co ? cJSON_GetArrayItem(co, 0) : NULL;
      cJSON *y = co ? cJSON_GetArrayItem(co, 1) : NULL;
      if (!x || !y || !cJSON_IsNumber(x) || !cJSON_IsNumber(y) ||
          !isfinite(x->valuedouble) || !isfinite(y->valuedouble)) continue;
      char xs[32], ys[32], nm[256];
      to_fixed(x->valuedouble, prec, xs, sizeof xs);
      to_fixed(y->valuedouble, prec, ys, sizeof ys);
      cJSON *nv = p ? cJSON_GetObjectItem(p, "name") : NULL;
      unified_norm_name(cJSON_IsString(nv) ? nv->valuestring : NULL,
                        nm, sizeof nm);
      snprintf(cbuf, sizeof cbuf, "_c:%s,%s:%s", xs, ys, nm);
      key = cbuf;
    }
    int *slot = hm_slot(&m, key);
    if (*slot >= 0) {                                   /* merge into existing */
      cJSON *ex = cJSON_GetArrayItem(kept, *slot);
      cJSON *exp = cJSON_GetObjectItem(ex, "properties");
      cJSON *kv;
      cJSON_ArrayForEach(kv, p) {
        cJSON *cur = cJSON_GetObjectItem(exp, kv->string);
        int cur_null = !cur || cJSON_IsNull(cur);
        int v_nn = !cJSON_IsNull(kv);
        if (cur_null && v_nn) {
          if (cur) cJSON_DeleteItemFromObject(exp, kv->string);
          cJSON_AddItemToObject(exp, kv->string, cJSON_Duplicate(kv, 1));
        }
      }
      /* sources = unique(existing.source, f.source) */
      cJSON *exsrc = cJSON_GetObjectItem(exp, "source");
      cJSON *fsrc  = p ? cJSON_GetObjectItem(p, "source") : NULL;
      cJSON *srcs = cJSON_CreateArray();
      const char *a = cJSON_IsString(exsrc) ? exsrc->valuestring : NULL;
      const char *b = cJSON_IsString(fsrc)  ? fsrc->valuestring  : NULL;
      if (a) cJSON_AddItemToArray(srcs, cJSON_CreateString(a));
      if (b && (!a || strcmp(a, b)))
        cJSON_AddItemToArray(srcs, cJSON_CreateString(b));
      cJSON_DeleteItemFromObject(exp, "sources");
      cJSON_AddItemToObject(exp, "sources", srcs);
    } else {                                            /* new slot */
      cJSON *copy = cJSON_CreateObject();
      cJSON_AddStringToObject(copy, "type", "Feature");
      cJSON_AddItemToObject(copy, "geometry",
                            g ? cJSON_Duplicate(g, 1) : cJSON_CreateNull());
      cJSON *np = p ? cJSON_Duplicate(p, 1) : cJSON_CreateObject();
      cJSON *fsrc = p ? cJSON_GetObjectItem(p, "source") : NULL;
      cJSON *srcs = cJSON_CreateArray();
      if (cJSON_IsString(fsrc) && fsrc->valuestring[0])
        cJSON_AddItemToArray(srcs, cJSON_CreateString(fsrc->valuestring));
      cJSON_DeleteItemFromObject(np, "sources");
      cJSON_AddItemToObject(np, "sources", srcs);
      cJSON_AddItemToObject(copy, "properties", np);
      *slot = cJSON_GetArraySize(kept);
      cJSON_AddItemToArray(kept, copy);
    }
  }
  hm_free(&m);
  return kept;
}

int unified_capture(const source_ctx *ctx, const char *upstream_id,
                    cJSON *out) {
  const source_def *def = registry_get(upstream_id);
  if (!def || !def->run || !cJSON_IsArray(out)) return 0;
  cJSON *cap_arr = cJSON_CreateArray();
  cap_ctx cc = { cap_arr };
  intel_sink cap = { &cc, cap_emit };
  source_ctx sub = *ctx;
  sub.source_id = upstream_id;
  int rc = def->run(&sub, &cap);
  int n = 0;
  if (rc >= 0) {                                  /* allSettled: reject ⇒ none */
    cJSON *f;
    cJSON_ArrayForEach(f, cap_arr) {
      cJSON *g = cJSON_GetObjectItem(f, "geometry");
      cJSON *p = cJSON_GetObjectItem(f, "properties");
      if (!g || !p) continue;                      /* mergeFC guard */
      cJSON_AddItemToArray(out, cJSON_Duplicate(f, 1));
      n++;
    }
  }
  cJSON_Delete(cap_arr);
  return n;
}

int unified_collect(const source_ctx *ctx, intel_sink *sink,
                     const char *source_id,
                     const unified_upstream *ups, int n_ups,
                     const unified_keyfn *keyfns, int n_keys,
                     int coord_precision,
                     unified_filterfn filter, unified_postfn post) {
  cJSON *merged = cJSON_CreateArray();

  for (int i = 0; i < n_ups; i++) {
    const source_def *def = registry_get(ups[i].name);
    if (!def || !def->run) continue;                    /* missing → 0 */
    cJSON *cap_arr = cJSON_CreateArray();
    cap_ctx cc = { cap_arr };
    intel_sink cap = { &cc, cap_emit };
    source_ctx sub = *ctx;
    sub.source_id = ups[i].name;
    int rc = def->run(&sub, &cap);
    if (rc < 0) { cJSON_Delete(cap_arr); continue; }    /* allSettled reject */

    cJSON *f;
    cJSON_ArrayForEach(f, cap_arr) {
      cJSON *g = cJSON_GetObjectItem(f, "geometry");
      cJSON *p = cJSON_GetObjectItem(f, "properties");
      if (!g || !p) continue;                            /* mergeFC guard */
      if (ups[i].kind) {                                 /* tag kind if unset */
        cJSON *k = cJSON_GetObjectItem(p, "kind");
        if (!k || cJSON_IsNull(k) ||
            (cJSON_IsString(k) && !k->valuestring[0]))
          cJSON_AddStringToObject(p, "kind", ups[i].kind);
      }
      cJSON_AddItemToArray(merged, cJSON_Duplicate(f, 1));
    }
    cJSON_Delete(cap_arr);
  }

  cJSON *filtered = merged;
  if (filter) {
    filtered = cJSON_CreateArray();
    cJSON *f;
    cJSON_ArrayForEach(f, merged)
      if (filter(f)) cJSON_AddItemToArray(filtered, cJSON_Duplicate(f, 1));
    cJSON_Delete(merged);
  }

  cJSON *features = (keyfns && n_keys > 0)
    ? dedupe(filtered, keyfns, n_keys, coord_precision > 0 ? coord_precision : 4)
    : cJSON_Duplicate(filtered, 1);
  if (filtered != features) cJSON_Delete(filtered);

  if (post) { cJSON *f; cJSON_ArrayForEach(f, features) post(f); }

  int n = geojson_emit_features(sink, source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[%s] emitted %d\n", source_id, n);
  return n >= 0 ? n : -1;
}
