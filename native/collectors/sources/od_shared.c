/* od_shared.c — helpers shared by the od_* open-data collectors.
 *
 * Everything here is `static inline`, so this translation unit emits no
 * symbols: the Makefile wildcard compiles it standalone (cleanly, and with no
 * -Wunused-function noise) and every od_*.c #includes it to get its own
 * private copy. No source_def is registered here.
 *
 * The rules this file exists to enforce once instead of 54 times:
 *   R1 every emitted field came back from the network;
 *   R2 has_geo/lat/lon/geometry ONLY from upstream coordinates — no centroid,
 *      no fallback point, and (0,0) is rejected as a null-island artefact;
 *   R3 a fetch/parse failure returns -1, an empty upstream returns 0.
 */
#ifndef JO_OD_SHARED_C
#define JO_OD_SHARED_C

#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------- scalars */

/* obj[key] as a non-empty string, else NULL. */
static inline const char *od_s(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!cJSON_IsString(v) || !v->valuestring || !v->valuestring[0]) return NULL;
  return v->valuestring;
}

/* Some portals ship translatable text as {"en":…,"fr":…} objects (Ontario's
 * title/notes do). cJSON_GetStringValue returns NULL on those, so descend. */
static inline const char *od_sml(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return NULL;
  if (cJSON_IsString(v))
    return (v->valuestring && v->valuestring[0]) ? v->valuestring : NULL;
  if (cJSON_IsObject(v)) {
    static const char *pref[] = { "en", "en-CA", "en_GB", "fr", "de", "es",
                                  "pt", "it", NULL };
    for (int i = 0; pref[i]; i++) {
      const char *s = od_s(v, pref[i]);
      if (s) return s;
    }
    for (const cJSON *c = v->child; c; c = c->next)
      if (cJSON_IsString(c) && c->valuestring && c->valuestring[0])
        return c->valuestring;
  }
  return NULL;
}

/* Numeric field that upstream may ship as a JSON number OR as a numeric
 * string (Socrata ships every number as a string; ODS mixes both).
 * Returns 0 for absent / null / non-numeric — the caller must then skip the
 * field rather than substitute a zero. */
static inline int od_numv(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *end = NULL;
    double d = strtod(v->valuestring, &end);
    if (end && end != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}

/* Any scalar as text. Numbers/bools are rendered into `buf`. NULL if the
 * field is absent, null or a container. */
static inline const char *od_scalar(const cJSON *o, const char *k,
                                    char *buf, size_t n) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v || cJSON_IsNull(v)) return NULL;
  if (cJSON_IsString(v))
    return (v->valuestring && v->valuestring[0]) ? v->valuestring : NULL;
  if (cJSON_IsBool(v)) {
    snprintf(buf, n, "%s", cJSON_IsTrue(v) ? "true" : "false");
    return buf;
  }
  if (cJSON_IsNumber(v)) {
    double d = v->valuedouble;
    if (d == (double)(long long)d) snprintf(buf, n, "%lld", (long long)d);
    else                           snprintf(buf, n, "%g", d);
    return buf;
  }
  return NULL;
}

/* First key in `keys` (NULL-terminated) that yields a scalar. */
static inline const char *od_pick(const cJSON *o, const char *const *keys,
                                  char *buf, size_t n) {
  if (!keys) return NULL;
  for (int i = 0; keys[i]; i++) {
    const char *s = od_scalar(o, keys[i], buf, n);
    if (s) return s;
  }
  return NULL;
}

/* ------------------------------------------------------------- properties */

/* Copy o[k] into props verbatim (scalars and arrays alike) when present. */
static inline void od_copy(cJSON *props, const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v || cJSON_IsNull(v) || cJSON_IsInvalid(v)) return;
  if (cJSON_IsString(v) && (!v->valuestring || !v->valuestring[0])) return;
  cJSON *dup = cJSON_Duplicate(v, 1);
  if (dup) cJSON_AddItemToObject(props, k, dup);
}

static inline void od_copy_keys(cJSON *props, const cJSON *o,
                                const char *const *keys) {
  for (int i = 0; keys && keys[i]; i++) od_copy(props, o, keys[i]);
}

static inline void od_put_s(cJSON *props, const char *k, const char *v) {
  if (v && *v) cJSON_AddStringToObject(props, k, v);
}

/* Copy every scalar field of `o` into props. Skips nested containers, nulls,
 * empty strings and Socrata's ":@computed_region_*" internals. Everything it
 * copies is upstream data (R1). */
static inline int od_copy_scalars(cJSON *props, const cJSON *o) {
  int n = 0;
  for (const cJSON *c = o ? o->child : NULL; c; c = c->next) {
    if (!c->string || c->string[0] == ':') continue;
    if (cJSON_IsNull(c) || cJSON_IsObject(c) || cJSON_IsArray(c)) continue;
    if (cJSON_IsString(c) && (!c->valuestring || !c->valuestring[0])) continue;
    cJSON *dup = cJSON_Duplicate(c, 1);
    if (dup) { cJSON_AddItemToObject(props, c->string, dup); n++; }
  }
  return n;
}

/* --------------------------------------------------------------- geometry */

/* R2 sanity gate: in range, and not the (0,0) null-island artefact. */
static inline int od_ll_ok(double lat, double lon) {
  if (!(lat >= -90.0 && lat <= 90.0)) return 0;
  if (!(lon >= -180.0 && lon <= 180.0)) return 0;
  if (lat == 0.0 && lon == 0.0) return 0;
  return 1;
}

/* A coordinate container the ODS/Socrata families use: {lon,lat},
 * {latitude,longitude}, a GeoJSON Point, or a bare [lon,lat] pair. */
static inline int od_geo_item(const cJSON *v, double *lat, double *lon) {
  double la = 0, lo = 0;
  if (!v) return 0;
  if (cJSON_IsObject(v)) {
    const cJSON *t = cJSON_GetObjectItem(v, "type");
    const cJSON *c = cJSON_GetObjectItem(v, "coordinates");
    if (cJSON_IsString(t) && t->valuestring &&
        strcmp(t->valuestring, "Point") == 0 &&
        cJSON_IsArray(c) && cJSON_GetArraySize(c) >= 2) {
      const cJSON *x = cJSON_GetArrayItem(c, 0), *y = cJSON_GetArrayItem(c, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        lo = x->valuedouble; la = y->valuedouble;
        if (od_ll_ok(la, lo)) { *lat = la; *lon = lo; return 1; }
      }
      return 0;
    }
    if (od_numv(v, "lat", &la) && od_numv(v, "lon", &lo) && od_ll_ok(la, lo)) {
      *lat = la; *lon = lo; return 1;
    }
    if (od_numv(v, "latitude", &la) && od_numv(v, "longitude", &lo) &&
        od_ll_ok(la, lo)) { *lat = la; *lon = lo; return 1; }
    return 0;
  }
  if (cJSON_IsArray(v) && cJSON_GetArraySize(v) >= 2) {
    const cJSON *x = cJSON_GetArrayItem(v, 0), *y = cJSON_GetArrayItem(v, 1);
    if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
      lo = x->valuedouble; la = y->valuedouble;      /* GeoJSON order */
      if (od_ll_ok(la, lo)) { *lat = la; *lon = lo; return 1; }
    }
  }
  return 0;
}

/* Upstream point coordinates for a record, or 0. Never invents anything. */
static inline int od_geo(const cJSON *o, double *lat, double *lon) {
  static const char *CONT[] = { "coordinates", "coordonnees_geo",
    "position_geographique", "geo_point_2d", "geo_point", "point",
    "geolocation", "geocoded_column", "location", NULL };
  double la = 0, lo = 0;
  if (od_numv(o, "latitude", &la) && od_numv(o, "longitude", &lo) &&
      od_ll_ok(la, lo)) { *lat = la; *lon = lo; return 1; }
  if (od_numv(o, "lat", &la) && od_numv(o, "lon", &lo) && od_ll_ok(la, lo)) {
    *lat = la; *lon = lo; return 1;
  }
  for (int i = 0; CONT[i]; i++)
    if (od_geo_item(cJSON_GetObjectItem(o, CONT[i]), lat, lon)) return 1;
  return 0;
}

/* Full upstream GeoJSON geometry (polygon layers), emitted verbatim rather
 * than reduced to a derived point. malloc'd; caller frees. */
static inline char *od_geometry(const cJSON *o, const char *const *keys) {
  for (int i = 0; keys && keys[i]; i++) {
    const cJSON *v = cJSON_GetObjectItem(o, keys[i]);
    if (cJSON_IsObject(v) && cJSON_IsString(cJSON_GetObjectItem(v, "type")) &&
        cJSON_GetObjectItem(v, "coordinates"))
      return cJSON_PrintUnformatted(v);
  }
  return NULL;
}

/* ------------------------------------------------------------------- time */

/* epoch milliseconds → ISO-8601 UTC. Returns 0 if it cannot be represented. */
static inline int od_iso_from_ms(double ms, char *out, size_t n) {
  time_t t = (time_t)(ms / 1000.0);
  struct tm g;
  if (!gmtime_r(&t, &g)) return 0;
  return strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &g) > 0;
}

static inline void od_today_utc(char *out, size_t n) {
  time_t t = time(NULL);
  struct tm g;
  if (gmtime_r(&t, &g)) strftime(out, n, "%Y-%m-%d", &g);
  else snprintf(out, n, "1970-01-01");
}

/* ------------------------------------------------------------------- emit */

/* Serialise `props` (consumed) into properties_json (and body when the caller
 * has not set one), emit, free. Returns 1 when the sink accepted the row. */
static inline int od_emit(intel_sink *sink, intel_item *it, cJSON *props) {
  char *pj = props ? cJSON_PrintUnformatted(props) : NULL;
  if (props) cJSON_Delete(props);
  it->properties_json = pj;
  if (!it->body) it->body = pj;
  int rc = sink->emit(sink, it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

/* ------------------------------------------------------- generic row tier */

typedef struct {
  const char *record_type;
  const char *tags_json;
  const char *link;                 /* endpoint the rows came from */
  const char *lang;
  const char *const *title_keys;    /* preference list, NULL-terminated */
  const char *const *summary_keys;
  const char *id_key;               /* → remote_key */
  const char *date_key;             /* → published_at (string field) */
  const char *const *geom_keys;     /* full-geometry fields (the_geom, geom) */
  const char *title_prefix;         /* prepended to the chosen title */
} od_rowspec;

/* One intel row per object in `arr`, carrying every scalar field the upstream
 * returned. A record with nothing identifying is skipped, not invented. */
static inline int od_emit_rows(intel_sink *sink, const cJSON *arr,
                               const od_rowspec *sp) {
  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    if (!cJSON_IsObject(r)) continue;
    char tb[256], ib[160], sb[256], title[420];
    const char *t   = od_pick(r, sp->title_keys, tb, sizeof tb);
    const char *idv = sp->id_key ? od_scalar(r, sp->id_key, ib, sizeof ib)
                                 : NULL;
    const char *sum = od_pick(r, sp->summary_keys, sb, sizeof sb);
    const char *date = sp->date_key ? od_s(r, sp->date_key) : NULL;
    if (!t) t = idv;
    if (!t) t = date;
    if (!t) continue;                       /* nothing identifying → no row */
    if (sp->title_prefix) {
      snprintf(title, sizeof title, "%s %s", sp->title_prefix, t);
      t = title;
    }
    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_copy_scalars(props, r);

    intel_item it = {0};
    it.title = t;
    it.summary = (sum && sum != t) ? sum : NULL;
    it.remote_key = idv;
    it.published_at = date;
    it.link = sp->link;
    it.lang = sp->lang;
    it.record_type = sp->record_type;
    it.tags_json = sp->tags_json;
    double la = 0, lo = 0;
    if (od_geo(r, &la, &lo)) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    char *geom = sp->geom_keys ? od_geometry(r, sp->geom_keys) : NULL;
    it.geometry_geojson = geom;
    n += od_emit(sink, &it, props);
    free(geom);
  }
  return n;
}

/* Fetch `url`, locate an array of row objects at `path` (NULL = the document
 * itself is the array) and emit it. Returns #emitted, or -1 if the fetch or
 * the parse failed (R3). */
static inline int od_fetch_rows(const source_ctx *ctx, intel_sink *sink,
                                const char *url, const char *path,
                                const od_rowspec *sp) {
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) return -1;
  const cJSON *arr = path ? cJSON_GetObjectItem(doc, path) : doc;
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); return -1; }
  int n = od_emit_rows(sink, arr, sp);
  cJSON_Delete(doc);
  return n;
}

/* ---------------------------------------------------------- CKAN / udata  */

/* CKAN package_search → one row per dataset. The array is result.results[]
 * everywhere except the London Datastore, whose envelope is result.result[].
 * `dataset_base` builds the public dataset link from the upstream slug (NULL
 * to omit the link). `extra` copies portal-specific fields verbatim.
 * Returns #emitted, or -1 on a fetch/parse failure. */
static inline int od_ckan_collect(const source_ctx *ctx, intel_sink *sink,
                                  const char *url, const char *dataset_base,
                                  const char *const *extra,
                                  const char *tags) {
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) return -1;
  const cJSON *ok  = cJSON_GetObjectItem(doc, "success");
  const cJSON *res = cJSON_GetObjectItem(doc, "result");
  if ((ok && !cJSON_IsTrue(ok)) || !res) { cJSON_Delete(doc); return -1; }
  const cJSON *arr = cJSON_GetObjectItem(res, "results");
  if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(res, "result");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); return -1; }

  double total = 0;
  int have_total = od_numv(res, "count", &total);

  int n = 0;
  const cJSON *pkg;
  cJSON_ArrayForEach(pkg, arr) {
    if (!cJSON_IsObject(pkg)) continue;
    const char *title = od_sml(pkg, "title");
    const char *slug  = od_s(pkg, "name");
    if (!title) title = slug;
    if (!title) continue;                   /* no upstream title → no row */

    const char *notes = od_sml(pkg, "notes");
    const char *mod   = od_s(pkg, "metadata_modified");
    if (!mod) mod = od_s(pkg, "metadata_created");
    const cJSON *org  = cJSON_GetObjectItem(pkg, "organization");
    const char *orgn  = cJSON_IsObject(org) ? od_sml(org, "title") : NULL;
    if (!orgn && cJSON_IsObject(org)) orgn = od_s(org, "name");
    if (!orgn) orgn = od_s(pkg, "owner_org");   /* London: a readable name */

    char link[900];
    link[0] = 0;
    if (dataset_base && slug) snprintf(link, sizeof link, "%s/dataset/%s",
                                       dataset_base, slug);

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_put_s(props, "title", title);
    od_put_s(props, "name", slug);
    od_put_s(props, "organization", orgn);
    od_put_s(props, "notes", notes);
    static const char *STD[] = { "license_title", "license_id", "license_url",
      "author", "author_email", "maintainer", "maintainer_email",
      "metadata_created", "metadata_modified", "num_resources", "num_tags",
      "isopen", "state", "type", "url", "version", NULL };
    od_copy_keys(props, pkg, STD);
    od_copy_keys(props, pkg, extra);
    if (have_total) cJSON_AddNumberToObject(props, "catalogue_total", total);

    intel_item it = {0};
    it.title = title;
    it.summary = orgn;
    it.remote_key = slug;
    it.published_at = mod;
    it.link = link[0] ? link : NULL;
    it.record_type = "ckan-dataset";
    it.tags_json = tags;
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return n;
}

/* udata portal API (data.public.lu, dados.gov.pt, data.gov.rs). The envelope
 * is {"data":[…],"next_page":…} — there is NO CKAN result.results wrapper,
 * and organization/owner are objects. Returns #emitted or -1. */
static inline int od_udata_collect(const source_ctx *ctx, intel_sink *sink,
                                   const char *url, const char *tags) {
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) return -1;
  const cJSON *arr = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); return -1; }

  int n = 0;
  const cJSON *d;
  cJSON_ArrayForEach(d, arr) {
    if (!cJSON_IsObject(d)) continue;
    const char *title = od_sml(d, "title");
    if (!title) title = od_s(d, "acronym");
    if (!title) continue;
    const char *page = od_s(d, "page");            /* upstream public URL */
    if (!page) page = od_s(d, "uri");
    const char *mod = od_s(d, "last_modified");
    if (!mod) mod = od_s(d, "last_update");
    if (!mod) mod = od_s(d, "created_at");
    const cJSON *org = cJSON_GetObjectItem(d, "organization");
    const char *orgn = cJSON_IsObject(org) ? od_s(org, "name") : NULL;
    if (!orgn && cJSON_IsObject(org)) orgn = od_s(org, "slug");
    if (!orgn) {
      const cJSON *owner = cJSON_GetObjectItem(d, "owner");
      if (cJSON_IsObject(owner)) {
        const char *fn = od_s(owner, "first_name"), *ln = od_s(owner, "last_name");
        orgn = ln ? ln : fn;
      }
    }
    const cJSON *rsc = cJSON_GetObjectItem(d, "resources");

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_put_s(props, "title", title);
    od_put_s(props, "organization", orgn);
    static const char *STD[] = { "acronym", "slug", "id", "description",
      "created_at", "last_modified", "last_update", "license", "frequency",
      "archived", "private", "page", NULL };
    od_copy_keys(props, d, STD);
    if (cJSON_IsArray(rsc))
      cJSON_AddNumberToObject(props, "resource_count",
                              cJSON_GetArraySize(rsc));

    intel_item it = {0};
    it.title = title;
    it.summary = orgn;
    it.remote_key = od_s(d, "id");
    it.published_at = mod;
    it.link = page;
    it.record_type = "udata-dataset";
    it.tags_json = tags;
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return n;
}

/* --------------------------------------------------- OpenDataSoft catalog */

/* ODS Explore v2.1 catalogue: results[] with dataset_id + metas.default.*.
 * `explore_base` builds the public dataset page from the upstream id.
 * Returns #emitted or -1. */
static inline int od_ods_catalog_collect(const source_ctx *ctx,
                                         intel_sink *sink, const char *url,
                                         const char *explore_base,
                                         const char *tags) {
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) return -1;
  const cJSON *arr = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); return -1; }

  int n = 0;
  const cJSON *d;
  cJSON_ArrayForEach(d, arr) {
    if (!cJSON_IsObject(d)) continue;
    const char *dsid = od_s(d, "dataset_id");
    const cJSON *metas = cJSON_GetObjectItem(d, "metas");
    const cJSON *def = cJSON_IsObject(metas)
                         ? cJSON_GetObjectItem(metas, "default") : NULL;
    const char *title = cJSON_IsObject(def) ? od_sml(def, "title") : NULL;
    if (!title) title = dsid;
    if (!title) continue;

    const char *mod = cJSON_IsObject(def) ? od_s(def, "modified") : NULL;
    const char *pub = cJSON_IsObject(def) ? od_s(def, "publisher") : NULL;

    char link[700];
    link[0] = 0;
    if (explore_base && dsid)
      snprintf(link, sizeof link, "%s%s/", explore_base, dsid);

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_put_s(props, "dataset_id", dsid);
    od_copy(props, d, "has_records");
    if (cJSON_IsObject(def)) {
      static const char *M[] = { "title", "description", "modified",
        "publisher", "license", "license_url", "records_count", "theme",
        "keyword", "language", "data_processed", "metadata_processed", NULL };
      od_copy_keys(props, def, M);
    }
    const cJSON *dcat = cJSON_IsObject(metas)
                          ? cJSON_GetObjectItem(metas, "dcat") : NULL;
    if (cJSON_IsObject(dcat)) {
      static const char *D[] = { "creator", "issued", "publisher", "contactpoint",
        "accrualperiodicity", NULL };
      od_copy_keys(props, dcat, D);
    }

    intel_item it = {0};
    it.title = title;
    it.summary = pub;
    it.remote_key = dsid;
    it.published_at = mod;
    it.link = link[0] ? link : NULL;
    it.record_type = "ods-dataset";
    it.tags_json = tags;
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return n;
}

/* -------------------------------------------------------------- JSON-stat */

/* Ordered category code at position `pos` of a JSON-stat dimension. */
static inline const char *od_js_code(const cJSON *dim, int pos) {
  const cJSON *cat = cJSON_GetObjectItem(dim, "category");
  if (!cJSON_IsObject(cat)) return NULL;
  const cJSON *idx = cJSON_GetObjectItem(cat, "index");
  if (cJSON_IsArray(idx)) {
    const cJSON *c = cJSON_GetArrayItem(idx, pos);
    return cJSON_IsString(c) ? c->valuestring : NULL;
  }
  if (cJSON_IsObject(idx)) {
    for (const cJSON *c = idx->child; c; c = c->next)
      if (cJSON_IsNumber(c) && (int)c->valuedouble == pos) return c->string;
    return NULL;
  }
  const cJSON *lab = cJSON_GetObjectItem(cat, "label");
  if (cJSON_IsObject(lab)) {
    int i = 0;
    for (const cJSON *c = lab->child; c; c = c->next, i++)
      if (i == pos) return c->string;
  }
  return NULL;
}

static inline const char *od_js_label(const cJSON *dim, const char *code) {
  if (!code) return NULL;
  const cJSON *cat = cJSON_GetObjectItem(dim, "category");
  if (!cJSON_IsObject(cat)) return NULL;
  const cJSON *lab = cJSON_GetObjectItem(cat, "label");
  if (!cJSON_IsObject(lab)) return NULL;
  return od_s(lab, code);
}

#define OD_JS_MAXDIM 12

typedef struct {
  const cJSON *dimension;
  const char *ids[OD_JS_MAXDIM];
  int sizes[OD_JS_MAXDIM];
  int ndim;
  const char *dslabel;
  const char *record_type;
  const char *tags_json;
  const char *link;
} od_jsctx;

/* One observation → one row, with every dimension decoded to its label.
 * `v` must be numeric: JSON-stat nulls are gaps and are skipped, never
 * emitted as 0. */
static inline int od_js_row(intel_sink *sink, const od_jsctx *jc, int flat,
                            const cJSON *v) {
  double val;
  if (cJSON_IsNumber(v)) val = v->valuedouble;
  else if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *end = NULL;
    val = strtod(v->valuestring, &end);
    if (!end || end == v->valuestring) return 0;
  } else return 0;                        /* null / missing observation */

  char title[512];
  int off = 0;
  title[0] = 0;
  if (jc->dslabel) {
    off += snprintf(title + off, sizeof title - (size_t)off, "%s", jc->dslabel);
    if (off > (int)sizeof title - 1) off = (int)sizeof title - 1;
  }

  cJSON *props = cJSON_CreateObject();
  if (!props) return 0;
  od_put_s(props, "dataset", jc->dslabel);

  int rem = flat;
  int pos[OD_JS_MAXDIM];
  for (int d = jc->ndim - 1; d >= 0; d--) {
    int sz = jc->sizes[d] > 0 ? jc->sizes[d] : 1;
    pos[d] = rem % sz;
    rem /= sz;
  }
  for (int d = 0; d < jc->ndim; d++) {
    const cJSON *dim = cJSON_GetObjectItem(jc->dimension, jc->ids[d]);
    const char *code = od_js_code(dim, pos[d]);
    const char *lab = od_js_label(dim, code);
    const char *shown = lab ? lab : code;
    if (!shown) continue;
    if (jc->sizes[d] > 1 && off < (int)sizeof title - 8) {
      off += snprintf(title + off, sizeof title - (size_t)off, " - %s", shown);
      if (off > (int)sizeof title - 1) off = (int)sizeof title - 1;
    }
    cJSON_AddStringToObject(props, jc->ids[d], shown);
    if (code && lab) {
      char k[96];
      snprintf(k, sizeof k, "%s_code", jc->ids[d]);
      cJSON_AddStringToObject(props, k, code);
    }
  }
  cJSON_AddNumberToObject(props, "value", val);
  if (off <= 0) snprintf(title, sizeof title, "observation %d", flat);

  intel_item it = {0};
  it.title = title;
  it.link = jc->link;
  it.record_type = jc->record_type;
  it.tags_json = jc->tags_json;
  return od_emit(sink, &it, props);
}

/* JSON-stat 1.x ({"dataset":{…}}) and 2.0 (class=dataset at the root) reader.
 * Emits one row per NON-NULL observation, capped at `max_rows`.
 * Returns #emitted, or -1 on a fetch/parse failure. */
static inline int od_jsonstat_collect(const source_ctx *ctx, intel_sink *sink,
                                      const char *url, const char *rt,
                                      const char *tags, const char *link,
                                      int max_rows) {
  cJSON *doc = feed_get_json(ctx->http, url, 40000);
  if (!doc) return -1;
  const cJSON *ds = cJSON_GetObjectItem(doc, "dataset");
  if (!cJSON_IsObject(ds)) ds = doc;                         /* JSON-stat 2.0 */

  const cJSON *dimension = cJSON_GetObjectItem(ds, "dimension");
  const cJSON *value = cJSON_GetObjectItem(ds, "value");
  const cJSON *ids = cJSON_GetObjectItem(ds, "id");
  const cJSON *sizes = cJSON_GetObjectItem(ds, "size");
  if (!cJSON_IsArray(ids) && cJSON_IsObject(dimension))      /* JSON-stat 1.x */
    ids = cJSON_GetObjectItem(dimension, "id");
  if (!cJSON_IsArray(sizes) && cJSON_IsObject(dimension)) {
    sizes = cJSON_GetObjectItem(dimension, "size");
    if (!cJSON_IsArray(sizes))
      sizes = cJSON_GetObjectItem(dimension, "Size");
  }
  if (!cJSON_IsObject(dimension) || !cJSON_IsArray(ids) ||
      (!cJSON_IsArray(value) && !cJSON_IsObject(value))) {
    cJSON_Delete(doc);
    return -1;
  }

  od_jsctx jc = {0};
  jc.dimension = dimension;
  jc.dslabel = od_s(ds, "label");
  jc.record_type = rt;
  jc.tags_json = tags;
  jc.link = link;
  int d = 0;
  const cJSON *e;
  cJSON_ArrayForEach(e, ids) {
    if (d >= OD_JS_MAXDIM) break;
    if (!cJSON_IsString(e)) continue;
    jc.ids[d] = e->valuestring;
    const cJSON *sz = cJSON_IsArray(sizes) ? cJSON_GetArrayItem(sizes, d) : NULL;
    jc.sizes[d] = cJSON_IsNumber(sz) ? (int)sz->valuedouble : 1;
    d++;
  }
  jc.ndim = d;
  if (jc.ndim == 0) { cJSON_Delete(doc); return -1; }

  int n = 0, i = 0;
  const cJSON *v;
  if (cJSON_IsArray(value)) {
    cJSON_ArrayForEach(v, value) {
      if (n >= max_rows) break;
      n += od_js_row(sink, &jc, i, v);
      i++;
    }
  } else {
    cJSON_ArrayForEach(v, value) {                 /* sparse object form */
      if (n >= max_rows) break;
      if (!v->string) continue;
      n += od_js_row(sink, &jc, atoi(v->string), v);
    }
  }
  cJSON_Delete(doc);
  return n;
}

/* ------------------------------------------------------------- SDMX-JSON  */

/* SDMX-JSON (ECB Data Portal, Bundesbank). Observation keys are POSITIONAL
 * indices into structure.dimensions.observation[0].values[] — they must be
 * joined against that list to recover the real period, and the series key
 * ("0:0:0:0:0") is likewise positional into structure.dimensions.series[].
 * Values arrive as numbers (ECB) or strings (Bundesbank).
 * Returns #emitted, or -1 on a fetch/parse failure. */
static inline int od_sdmx_collect(const source_ctx *ctx, intel_sink *sink,
                                  const char *url, const char *rt,
                                  const char *tags, const char *link,
                                  int max_rows) {
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) return -1;
  const cJSON *root = doc;
  const cJSON *data = cJSON_GetObjectItem(doc, "data");
  if (cJSON_IsObject(data) && cJSON_GetObjectItem(data, "dataSets")) root = data;

  const cJSON *sets = cJSON_GetObjectItem(root, "dataSets");
  const cJSON *structure = cJSON_GetObjectItem(root, "structure");
  const cJSON *dims = cJSON_IsObject(structure)
                        ? cJSON_GetObjectItem(structure, "dimensions") : NULL;
  const cJSON *obsdims = cJSON_IsObject(dims)
                           ? cJSON_GetObjectItem(dims, "observation") : NULL;
  const cJSON *serdims = cJSON_IsObject(dims)
                           ? cJSON_GetObjectItem(dims, "series") : NULL;
  const cJSON *obsvals = cJSON_IsArray(obsdims) && cJSON_GetArraySize(obsdims) > 0
      ? cJSON_GetObjectItem(cJSON_GetArrayItem(obsdims, 0), "values") : NULL;
  if (!cJSON_IsArray(sets) || !cJSON_IsArray(obsvals)) {
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  const cJSON *set;
  cJSON_ArrayForEach(set, sets) {
    const cJSON *series = cJSON_GetObjectItem(set, "series");
    if (!cJSON_IsObject(series)) continue;
    const cJSON *s;
    cJSON_ArrayForEach(s, series) {
      if (n >= max_rows) break;
      const cJSON *obs = cJSON_GetObjectItem(s, "observations");
      if (!cJSON_IsObject(obs) || !s->string) continue;

      /* decode the positional series key against structure.dimensions.series */
      char slabel[320];
      int soff = 0;
      slabel[0] = 0;
      cJSON *skey = cJSON_CreateObject();
      if (skey) {
        const char *p = s->string;
        int di = 0;
        while (p && *p) {
          int pos = atoi(p);
          const cJSON *sd = cJSON_IsArray(serdims)
                              ? cJSON_GetArrayItem(serdims, di) : NULL;
          const cJSON *vals = sd ? cJSON_GetObjectItem(sd, "values") : NULL;
          const cJSON *vv = cJSON_IsArray(vals)
                              ? cJSON_GetArrayItem(vals, pos) : NULL;
          const char *nm = vv ? od_s(vv, "name") : NULL;
          const char *idv = vv ? od_s(vv, "id") : NULL;
          const char *dimid = sd ? od_s(sd, "id") : NULL;
          if (dimid && (nm || idv))
            cJSON_AddStringToObject(skey, dimid, nm ? nm : idv);
          if (idv && soff < (int)sizeof slabel - 2) {
            soff += snprintf(slabel + soff, sizeof slabel - (size_t)soff,
                             "%s%s", soff ? "." : "", idv);
            if (soff > (int)sizeof slabel - 1) soff = (int)sizeof slabel - 1;
          }
          const char *dot = strchr(p, ':');
          p = dot ? dot + 1 : NULL;
          di++;
        }
      }

      const cJSON *o;
      cJSON_ArrayForEach(o, obs) {
        if (n >= max_rows) break;
        if (!o->string || !cJSON_IsArray(o)) continue;
        const cJSON *first = cJSON_GetArrayItem(o, 0);
        double val;
        if (cJSON_IsNumber(first)) val = first->valuedouble;
        else if (cJSON_IsString(first) && first->valuestring[0]) {
          char *end = NULL;
          val = strtod(first->valuestring, &end);
          if (!end || end == first->valuestring) continue;
        } else continue;               /* null observation → skip, not zero */

        const cJSON *pv = cJSON_GetArrayItem(obsvals, atoi(o->string));
        const char *period = pv ? od_s(pv, "id") : NULL;
        if (!period && pv) period = od_s(pv, "name");
        if (!period) continue;         /* cannot name the period → no row */

        char title[420];
        snprintf(title, sizeof title, "%s %s = %g",
                 slabel[0] ? slabel : "series", period, val);

        cJSON *props = skey ? cJSON_Duplicate(skey, 1) : cJSON_CreateObject();
        if (!props) continue;
        cJSON_AddStringToObject(props, "period", period);
        cJSON_AddNumberToObject(props, "value", val);
        if (slabel[0]) cJSON_AddStringToObject(props, "series_key", slabel);

        char rk[420];
        snprintf(rk, sizeof rk, "%s|%s", slabel[0] ? slabel : "series", period);

        intel_item it = {0};
        it.title = title;
        it.remote_key = rk;
        it.published_at = strlen(period) == 10 ? period : NULL;
        it.link = link;
        it.record_type = rt;
        it.tags_json = tags;
        n += od_emit(sink, &it, props);
      }
      if (skey) cJSON_Delete(skey);
    }
  }
  cJSON_Delete(doc);
  return n;
}

/* Standard tail: turn a collector result into the contract's rc (R3). */
static inline int od_rc(const char *sid, int n) {
  if (n < 0) { fprintf(stderr, "[%s] fetch/parse failed\n", sid); return -1; }
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#endif /* JO_OD_SHARED_C */
