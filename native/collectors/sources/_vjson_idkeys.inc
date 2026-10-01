/* _vjson_idkeys.inc — per-row record identity and page shaping for the
 * generated VJSON / VGEO fleet. Include AFTER _verified_macros.inc.
 *
 * WHY. VJSON keys a record on a fixed id precedence list and VJSON_KEYED on ONE
 * top-level field; neither can say "this record is identified by the TUPLE
 * (Code, ApplicationDate)", which is what most registers actually are, and
 * VJSON_KEYED rewrites the record's own `id` field to do its relabel. VGEO has
 * no override at all. Measured 2026-09-14 on the rows that use this file:
 * TWSE foreign listings reuse a stock Code across re-applications (124 records,
 * 118 codes), data.sa.gov.au's CKAN index serves one package id twice with
 * different harvest metadata, d-portal's act⋈trans rows have no single key,
 * and the Caltrans statewide stop layer keys on a per-agency GTFS stop_id.
 *
 * HOW. A shim sink between the emitter and the real sink. The emitter walks,
 * pages, labels and discloses exactly as before; the shim only replaces the uid
 * with "<source id>|<v1>|<v2>…", the values read off the item's own
 * properties_json (dotted paths allowed). `+` composes, as in hpengine id_keys.
 * Nothing is added to or removed from the record. A record carrying none of the
 * named fields keeps the emitter's uid; collector-* notices pass untouched.
 *
 * VJSON_PREP additionally runs a per-page shaping hook before the emit, for
 * upstreams whose records are wrapped ({depute:{…}}) or carry no scalar the
 * emitter can label. A hook may only reshape or compose from values already in
 * the document. Keep long commentary OUT of macro bodies (tools/lint_sources.py
 * expands them textually). */
#ifndef JO_VJSON_IDKEYS_INC
#define JO_VJSON_IDKEYS_INC
#include "_verified_macros.inc"
#include "lib/pagewalk.h"

typedef struct { intel_sink *inner; const char *sid; const char *keys; } vidk_ctx;

static inline const cJSON *vidk_lookup(const cJSON *o, const char *path) {
  char seg[128];
  const char *p = path;
  while (o && *p) {
    const char *dot = strchr(p, '.');
    size_t n = dot ? (size_t)(dot - p) : strlen(p);
    if (n >= sizeof seg) return NULL;
    memcpy(seg, p, n);
    seg[n] = '\0';
    o = cJSON_GetObjectItemCaseSensitive(o, seg);
    p = dot ? dot + 1 : p + n;
  }
  return o;
}

/* Append `s` to the heap buffer, growing it. 0 on allocation failure. */
static inline int vidk_put(char **buf, size_t *len, size_t *cap, const char *s,
                           size_t n) {
  if (*len + n + 1 > *cap) {
    size_t nc = (*cap ? *cap * 2 : 128);
    while (nc < *len + n + 1) nc *= 2;
    char *nb = realloc(*buf, nc);
    if (!nb) return 0;
    *buf = nb;
    *cap = nc;
  }
  memcpy(*buf + *len, s, n);
  *len += n;
  (*buf)[*len] = '\0';
  return 1;
}

static inline int vidk_emit(struct intel_sink *s, const intel_item *it) {
  vidk_ctx *c = (vidk_ctx *)s->ctx;
  if (!c->keys || !it->properties_json ||
      (it->record_type && strncmp(it->record_type, "collector-", 10) == 0))
    return c->inner->emit(c->inner, it);
  cJSON *p = cJSON_Parse(it->properties_json);
  if (!p) return c->inner->emit(c->inner, it);

  char *key = NULL;
  size_t len = 0, cap = 0;
  int any = 0, ok = 1;
  const char *k = c->keys;
  while (ok) {
    const char *plus = strchr(k, '+');
    size_t kl = plus ? (size_t)(plus - k) : strlen(k);
    char path[128];
    if (kl >= sizeof path) kl = sizeof path - 1;
    memcpy(path, k, kl);
    path[kl] = '\0';
    const cJSON *v = vidk_lookup(p, path);
    char num[64];
    const char *sv = NULL;
    if (cJSON_IsString(v) && v->valuestring && *v->valuestring) {
      sv = v->valuestring;
    } else if (cJSON_IsNumber(v)) {
      double d = v->valuedouble;
      if (d > -9e15 && d < 9e15 && d == (double)(long long)d)
        snprintf(num, sizeof num, "%lld", (long long)d);
      else
        snprintf(num, sizeof num, "%.17g", d);
      sv = num;
    } else if (cJSON_IsBool(v)) {
      sv = cJSON_IsTrue(v) ? "true" : "false";
    }
    if (len) ok = vidk_put(&key, &len, &cap, "|", 1);
    if (ok && sv) { any = 1; ok = vidk_put(&key, &len, &cap, sv, strlen(sv)); }
    if (!plus) break;
    k = plus + 1;
  }
  cJSON_Delete(p);
  if (!ok || !any || !key) {
    free(key);
    return c->inner->emit(c->inner, it);
  }

  char hashed[21];
  const char *rk = key;
  if (len > 160) {
    const char *parts[1] = { key };
    feed_hash_key(hashed, parts, 1);
    rk = hashed;
  }
  char uid[256];
  snprintf(uid, sizeof uid, "%s|%s", c->sid, rk);
  intel_item copy = *it;
  copy.uid = uid;
  copy.remote_key = rk;
  int rc = c->inner->emit(c->inner, &copy);
  free(key);
  return rc;
}

typedef void (*vidk_prep_fn)(cJSON *doc);
typedef struct {
  const char *path, *cat, *lang, *tags;
  vidk_prep_fn prep;
} vidk_prep_opts;

static inline int vidk_prep_page(const source_ctx *c, intel_sink *s,
                                 const char *id, cJSON *doc, void *ud,
                                 int *seen) {
  (void)c;
  vidk_prep_opts *o = (vidk_prep_opts *)ud;
  if (o->prep) o->prep(doc);
  return jsonlist_emit_ex(s, id, doc, o->path, o->cat, o->lang, o->tags, seen);
}

/* As VJSON, keyed on IDKEYS ("field", "a+b", "outer.inner"). */
#define VJSON_IDKEYS(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, PATH, LANG, TAGS, IVAL, DESC, IDKEYS) \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                  \
    vidk_ctx kc = { s, ID, IDKEYS };                                          \
    intel_sink ks = { &kc, vidk_emit };                                       \
    char *du = vsrc_url_dates(URL);                                           \
    int n = jsonlist_emit_paged(&ks, ID, c->http, du ? du : URL, 25000,       \
                                PATH, CAT, LANG, TAGS);                       \
    free(du);                                                                 \
    if (n < 0) { fprintf(stderr, "[%s] fetch failed\n", ID); return -1; }     \
    return 0; }                                                               \
  static const source_def SYM = {                                            \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,             \
    .update_interval_sec = IVAL, .run = run_##SYM,                            \
    .category = CAT, .type = "api", .url = URL,                               \
    .description = DESC, .layer = NULL, .free_tier = 1 };                     \
  REGISTER_SOURCE(SYM)

/* As VGEO, keyed on IDKEYS read off each feature's properties. */
#define VGEO_IDKEYS(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, LANG, TAGS, IVAL, DESC, IDKEYS) \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                  \
    vidk_ctx kc = { s, ID, IDKEYS };                                          \
    intel_sink ks = { &kc, vidk_emit };                                       \
    vgeo_cat_ctx vc = { &ks, CAT, ID };                                       \
    intel_sink vs = { &vc, vgeo_cat_emit };                                   \
    char *du = vsrc_url_dates(URL);                                           \
    int n = geojson_emit_paged(&vs, ID, c->http, du ? du : URL, 25000);       \
    free(du);                                                                 \
    if (n < 0) { fprintf(stderr, "[%s] fetch failed\n", ID); return -1; }     \
    return 0; }                                                               \
  static const source_def SYM = {                                            \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,             \
    .update_interval_sec = IVAL, .run = run_##SYM,                            \
    .category = CAT, .type = "api", .url = URL,                               \
    .description = DESC, .layer = NULL, .free_tier = 1 };                     \
  REGISTER_SOURCE(SYM)

/* pw_walk + a per-page shaping hook PREP (may be NULL) + IDKEYS (may be NULL). */
#define VJSON_PREP(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, PATH, LANG, TAGS, IVAL, DESC, IDKEYS, PREP) \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                  \
    vidk_ctx kc = { s, ID, IDKEYS };                                          \
    intel_sink ks = { &kc, vidk_emit };                                       \
    vidk_prep_opts po = { PATH, CAT, LANG, TAGS, PREP };                      \
    char *du = vsrc_url_dates(URL);                                           \
    int n = pw_walk(c, &ks, ID, du ? du : URL, pw_fetch_json,                 \
                    vidk_prep_page, &po);                                     \
    free(du);                                                                 \
    if (n < 0) { fprintf(stderr, "[%s] fetch failed\n", ID); return -1; }     \
    return 0; }                                                               \
  static const source_def SYM = {                                            \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,             \
    .update_interval_sec = IVAL, .run = run_##SYM,                            \
    .category = CAT, .type = "api", .url = URL,                               \
    .description = DESC, .layer = NULL, .free_tier = 1 };                     \
  REGISTER_SOURCE(SYM)

#endif /* JO_VJSON_IDKEYS_INC */
