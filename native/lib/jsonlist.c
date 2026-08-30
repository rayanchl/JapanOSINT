/* lib/jsonlist.c — see jsonlist.h. */
#include "jsonlist.h"
#include "feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* Field-name precedence. Order matters: the first hit wins, so the most
 * specific name has to come first. These lists are deliberately short — a
 * long list starts matching things that merely sound right, and a wrong
 * title is worse than no row. */
static const char *K_TITLE[] = {
  "title", "name", "headline", "subject", "label", "event_name",
  "common_name", "display_name", "full_name", "short_name",
  "station", "site", "location", "place", "area", "region",
  "country_name", "country", "indicator", "event",
  /* The fleet is worldwide and most of it is not published in English: a
   * government API in Brazil calls the label `nome`, in France `nom`, in Spain
   * `nombre`, in the Netherlands `naam`. These are the same field, and without
   * them a correctly-fetched national dataset emits nothing at all. Placed
   * after the English keys so precedence is unchanged for anything that has
   * both, and kept to unambiguous nouns — short or overloaded words (Turkish
   * `ad`, Slovene `ime`) are omitted rather than risk a wrong title. */
  "nome", "nom", "nombre", "naam", "navn", "namn", "nimi",
  "nazwa", "nazev", "n\xc3\xa1zev", "denumire", "naziv", "titulo",
  "t\xc3\xadtulo", "titre", "titel", "tytul",
  "summary", "description", "text", NULL };
static const char *K_LINK[] = {
  "url", "link", "href", "permalink", "web_url", "uri", "detail", NULL };
static const char *K_DATE[] = {
  "published_at", "published", "pubDate", "pub_date", "date_published",
  "updated_at", "updated", "created_at", "datetime", "date", "timestamp",
  /* Observation datasets name their timestamp after the act of observing
   * rather than after publication: NASA FIRMS uses acq_date, buoy and sensor
   * networks use obs_date, time_tag or captured_at. Without these a
   * geolocated observation looks undated and is dropped. */
  /* Both spellings are real and appear side by side within one publisher:
   * NOAA SWPC serves `time_tag` on its space-weather products and `time-tag`
   * on its solar-cycle series. Matching only the underscore form silently
   * dropped the entire hyphenated set. */
  "acq_date", "obs_date", "observed_at", "observed", "time_tag", "time-tag",
  "captured_at", "measured_at", "start_time", "event_time",
  /* ADS-B feeds report age-of-contact as `seen`; without it ten aircraft
   * watchboxes carried real positions but looked undated and were dropped. */
  "seen", "time", "last_modified", NULL };
static const char *K_ID[] = {
  "id", "uid", "guid", "uuid", "_id", "identifier", "code", "key", NULL };
static const char *K_LAT[] = { "lat", "latitude", "Latitude", "LAT", "y", NULL };
static const char *K_LON[] = { "lon", "lng", "long", "longitude", "Longitude",
                               "LON", "x", NULL };
static const char *K_BODY[] = { "body", "content", "description", "summary",
                                "abstract", "detail", NULL };

/* cJSON's object lookup is case-insensitive already, but only for an exact
 * spelling; walk the candidate list and take the first that is present AND
 * carries a usable scalar. */
static cJSON *pick(cJSON *o, const char *const *keys) {
  for (int i = 0; keys[i]; i++) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, keys[i]);
    if (!v) v = cJSON_GetObjectItem(o, keys[i]);
    /* "carries a usable scalar" has to be enforced HERE, not left to
     * scalar_dup: returning the first present-but-nested value stops the walk
     * dead, and scalar_dup then yields NULL, so the record loses a field it
     * actually had further down the list. `location` sits in K_TITLE ahead of
     * `country`, so {"location":{"lat":..,"lon":..},"country":"Japan"} was
     * dropped as untitled — with 790 VJSON/VCSV sources on this path, that is
     * a whole-source silent zero for any upstream whose first-listed name is
     * an object. Nested values are skipped; the walk continues. */
    if (v && (cJSON_IsString(v) || cJSON_IsNumber(v) || cJSON_IsBool(v)))
      return v;
  }
  return NULL;
}

/* Scalar → heap string. Numbers and bools are stringified so that an upstream
 * which returns a numeric id or an epoch timestamp still yields a usable
 * field rather than being dropped. Returns NULL when there is nothing real. */
static char *scalar_dup(const cJSON *v) {
  if (!v) return NULL;
  if (cJSON_IsString(v)) {
    const char *s = v->valuestring;
    if (!s) return NULL;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (!*s) return NULL;
    return strdup(s);
  }
  if (cJSON_IsNumber(v)) {
    char buf[64];
    double d = v->valuedouble;
    /* Range-check BEFORE the cast. `(long long)d` is undefined when d is
     * outside long long's range, and an upstream only has to return 1e308 in
     * any numeric field to get there — UBSan: "1e+308 is outside the range of
     * representable values of type 'long long int'". (double)LLONG_MAX is
     * exactly 2^63, so `<` keeps the unrepresentable boundary out; NaN fails
     * every comparison and correctly falls through to %.10g. */
    if (d >= (double)LLONG_MIN && d < (double)LLONG_MAX &&
        d == (double)(long long)d)
      snprintf(buf, sizeof buf, "%lld", (long long)d);
    else
      snprintf(buf, sizeof buf, "%.10g", d);
    return strdup(buf);
  }
  if (cJSON_IsBool(v)) return strdup(cJSON_IsTrue(v) ? "true" : "false");
  return NULL;
}

/* A coordinate is only a coordinate if it is finite and in range. Upstreams
 * routinely use 0/0 or 999 as "unknown"; 0,0 is Null Island and is far more
 * often a missing value than a real position, so it is rejected. */
static int coord_ok(double lat, double lon) {
  if (!isfinite(lat) || !isfinite(lon)) return 0;
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) return 0;
  if (lat == 0.0 && lon == 0.0) return 0;
  return 1;
}

/* Read a coordinate pair off a record. Two shapes are accepted: flat
 * lat/lon-ish fields, and an embedded GeoJSON Point. Nothing else — an
 * inferred position is exactly what R2 forbids. */
static int record_coords(cJSON *rec, double *lat, double *lon) {
  cJSON *a = pick(rec, K_LAT), *o = pick(rec, K_LON);
  if (cJSON_IsNumber(a) && cJSON_IsNumber(o)) {
    *lat = a->valuedouble; *lon = o->valuedouble;
    if (coord_ok(*lat, *lon)) return 1;
  }
  /* Some string APIs quote their numbers. */
  if (cJSON_IsString(a) && cJSON_IsString(o) && a->valuestring && o->valuestring) {
    char *ea = NULL, *eo = NULL;
    double la = strtod(a->valuestring, &ea), lo = strtod(o->valuestring, &eo);
    if (ea && ea != a->valuestring && eo && eo != o->valuestring &&
        coord_ok(la, lo)) { *lat = la; *lon = lo; return 1; }
  }
  cJSON *g = cJSON_GetObjectItem(rec, "geometry");
  if (g) {
    const char *t = cJSON_GetStringValue(cJSON_GetObjectItem(g, "type"));
    cJSON *c = cJSON_GetObjectItem(g, "coordinates");
    if (t && !strcmp(t, "Point") && cJSON_IsArray(c) &&
        cJSON_GetArraySize(c) >= 2) {
      cJSON *x = cJSON_GetArrayItem(c, 0), *y = cJSON_GetArrayItem(c, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        *lon = x->valuedouble; *lat = y->valuedouble;   /* GeoJSON is lon,lat */
        if (coord_ok(*lat, *lon)) return 1;
      }
    }
  }
  return 0;
}

/* Fallback label hunt: the first key whose NAME ends in "_name"/"Name" and
 * whose value is a non-empty string. Statistical APIs name their label columns
 * by domain rather than generically — UNHCR returns `coo_name`/`coa_name`
 * (country of origin / of asylum) and nothing called "title" — so a fixed list
 * can never cover them. This reads a real value the record supplied; it does
 * not synthesise one. */
static char *pick_name_suffixed(cJSON *o) {
  for (cJSON *k = o->child; k; k = k->next) {
    if (!k->string || !cJSON_IsString(k) || !k->valuestring || !*k->valuestring)
      continue;
    size_t n = strlen(k->string);
    if (n < 5) continue;
    /* Case-INsensitive: CelesTrak's satellite catalogues label every object
     * `OBJECT_NAME`, and a case-sensitive "_name" test misses it, which zeroed
     * 36 orbital sources that were fetching perfectly. Upper-snake is normal in
     * scientific and government CSV-derived JSON, so this is the common case,
     * not an edge one. Only the underscored form is case-relaxed: matching a
     * bare "…name" case-insensitively would also swallow `surname`,
     * `filename` and `hostname`, and a wrong title is worse than no row. */
    const char *tail = k->string + n - 5;
    if (!strcasecmp(tail, "_name") || !strcmp(k->string + n - 4, "Name")) {
      char *v = scalar_dup(k);
      if (v) {
        /* "-" is UNHCR's null. A dash is not a label. */
        if (strcmp(v, "-") && strcmp(v, "N/A")) return v;
        free(v);
      }
    }
  }
  return NULL;
}

/* The record's first non-empty scalar, whatever it is called. Last resort for
 * a label, and the direct counterpart of lib/hpengine.c:474 hp_first_scalar —
 * the two engines are supposed to answer the same question the same way.
 *
 * Nested values are skipped, because a label has to be something a human can
 * read, and keys beginning with "_" are skipped because those are markers this
 * tree stamps on a record rather than fields the upstream sent. */
static char *first_scalar_dup(cJSON *o) {
  for (cJSON *k = o->child; k; k = k->next) {
    if (!k->string || k->string[0] == '_') continue;
    if (!cJSON_IsString(k) && !cJSON_IsNumber(k) && !cJSON_IsBool(k)) continue;
    char *v = scalar_dup(k);
    if (v) return v;
  }
  return NULL;
}

static int key_listed(const char *k, const char *const *keys) {
  for (int i = 0; keys[i]; i++)
    if (!strcmp(k, keys[i])) return 1;
  return 0;
}

/* The record's first genuine measurement: a numeric field that is not the
 * position and not the timestamp. Used only to label an otherwise-nameless
 * observation, so it must not pick the very fields already in the label. */
static const cJSON *first_measure(cJSON *o) {
  for (cJSON *k = o->child; k; k = k->next) {
    if (!k->string || !cJSON_IsNumber(k)) continue;
    if (key_listed(k->string, K_LAT) || key_listed(k->string, K_LON) ||
        key_listed(k->string, K_DATE) || key_listed(k->string, K_ID))
      continue;
    return k;
  }
  return NULL;
}

/* Compact rendering of a numeric cJSON into a small static-lifetime buffer.
 * Only ever used inside one snprintf of the caller, so a per-call static is
 * safe here and avoids an allocation on a path that runs per record. */
static const char *num_brief(const cJSON *v) {
  static _Thread_local char b[40];
  double d = v->valuedouble;
  /* Range-check BEFORE the cast, exactly as scalar_dup above does. `(long
   * long)d` is undefined outside long long's range, and this runs on the FIRST
   * NUMERIC FIELD of any unlabelled record — an upstream only has to return
   * 1e308 (or inf/NaN) in one measurement to reach it. On x86 the cast yields
   * INT64_MIN, so the composed title read "…flux=-9223372036854775808" for a
   * value that was nothing of the sort. NaN fails every comparison and
   * correctly falls through to %.6g. */
  if (d >= (double) LLONG_MIN && d < (double) LLONG_MAX &&
      d == (double) (long long) d) snprintf(b, sizeof b, "%lld", (long long) d);
  else                             snprintf(b, sizeof b, "%.6g", d);
  return b;
}

static int array_of_objects(cJSON *a) {
  if (!cJSON_IsArray(a) || cJSON_GetArraySize(a) == 0) return 0;
  cJSON *first = cJSON_GetArrayItem(a, 0);  /* exhaustive-ok: type probe — is this an array OF OBJECTS; the caller then walks every element */
  return first && cJSON_IsObject(first);
}

cJSON *jsonlist_find_array(cJSON *doc, const char *path) {
  if (!doc) return NULL;
  if (!path || !*path) return cJSON_IsArray(doc) ? doc : NULL;

  if (!strcmp(path, "*")) {
    if (cJSON_IsArray(doc)) return doc;
    if (!cJSON_IsObject(doc)) return NULL;
    cJSON *best = NULL; int bestn = 0;
    for (cJSON *k = doc->child; k; k = k->next) {
      if (array_of_objects(k) && cJSON_GetArraySize(k) > bestn) {
        bestn = cJSON_GetArraySize(k); best = k;
      } else if (cJSON_IsObject(k)) {
        for (cJSON *k2 = k->child; k2; k2 = k2->next)
          if (array_of_objects(k2) && cJSON_GetArraySize(k2) > bestn) {
            bestn = cJSON_GetArraySize(k2); best = k2;
          }
      }
    }
    return best;
  }

  /* Dot path. Bounded copy — the path is a compile-time literal in generated
   * collectors, but this is a library and callers change. */
  char buf[256];
  snprintf(buf, sizeof buf, "%s", path);
  cJSON *cur = doc;
  /* strtok_r, not strtok: this runs on 8 scheduler workers and up to 16
   * dispatch workers concurrently, and strtok's resume pointer is ONE
   * process-global. Interleaved calls made a worker resume inside another
   * thread's buffer, so cJSON_GetObjectItem() looked up a garbage key, the
   * array was not found, and the collector reported a clean empty result for a
   * fetch that had actually succeeded — silently, non-deterministically,
   * across the ~142 sources that reach this path. */
  char *save = NULL;
  for (char *tok = strtok_r(buf, ".", &save); tok && cur;
       tok = strtok_r(NULL, ".", &save))
    cur = cJSON_GetObjectItem(cur, tok);
  return (cur && cJSON_IsArray(cur)) ? cur : NULL;
}

/* ── columnar tables ──────────────────────────────────────────────────────
 *
 * Some upstreams publish a record list SIDEWAYS: one array of column names and
 * one array of rows, each row a bare array of cells. Nothing above understands
 * that shape — emit_record wants an object, so every row scores as a non-object
 * and the whole source emits zero while reporting a clean success.
 *
 * ERDDAP's tabledap is the fleet's case. `allDatasets.json` answers
 * {"table":{"columnNames":["datasetID","title","institution",…],
 *           "columnTypes":[…],"columnUnits":[…],"rows":[[…],[…]]}} and
 * geo-erddap-ioos-sensors declares path "table.rows" — 27,245 catalogued
 * sensor datasets, each with a real title and institution, discarded on every
 * single run. 16 registered sources use this shape.
 *
 * The projection invents nothing: the names are the upstream's own column
 * names, the values are the upstream's own cells, and it only fires when a
 * sibling array of non-empty strings has EXACTLY the row's arity — anything
 * less certain is left alone rather than guessed at. */

/* The object that CONTAINS the record array, so its sibling keys are visible.
 * "table.rows" → doc["table"]; "rows" → doc. NULL when the path cannot name
 * one ("" is a bare array with no envelope, "*" was auto-detected and is only
 * ever an array of objects anyway). */
static cJSON *columnar_parent(cJSON *doc, const char *path) {
  if (!path || !*path || !strcmp(path, "*") || !strcmp(path, ".")) return NULL;
  char buf[256];
  snprintf(buf, sizeof buf, "%s", path);
  char *dot = strrchr(buf, '.');
  if (!dot) return cJSON_IsObject(doc) ? doc : NULL;
  *dot = 0;
  cJSON *cur = doc;
  char *save = NULL;
  for (char *tok = strtok_r(buf, ".", &save); tok && cur;
       tok = strtok_r(NULL, ".", &save))
    cur = cJSON_GetObjectItem(cur, tok);
  return (cur && cJSON_IsObject(cur)) ? cur : NULL;
}

/* rows-of-arrays + a sibling name list → a real array of objects, or NULL when
 * this is not that shape. Caller cJSON_Delete()s the result. */
static cJSON *columnar_objects(cJSON *parent, cJSON *rows) {
  if (!parent || !cJSON_IsArray(rows) || cJSON_GetArraySize(rows) == 0)
    return NULL;
  cJSON *first = cJSON_GetArrayItem(rows, 0);  /* exhaustive-ok: shape probe — is this rows-of-arrays; every row is projected below */
  if (!first || !cJSON_IsArray(first)) return NULL;
  int width = cJSON_GetArraySize(first);
  if (width < 1) return NULL;

  static const char *const NAMEKEYS[] = {
    "columnNames", "column_names", "columnnames", "columns", "cols",
    "header", "headers", "fields", NULL };
  cJSON *names = NULL;
  for (int i = 0; NAMEKEYS[i] && !names; i++) {
    cJSON *c = cJSON_GetObjectItem(parent, NAMEKEYS[i]);
    if (!cJSON_IsArray(c) || cJSON_GetArraySize(c) != width) continue;
    int ok = 1;
    cJSON *e;
    cJSON_ArrayForEach(e, c)
      if (!cJSON_IsString(e) || !e->valuestring || !e->valuestring[0]) {
        ok = 0; break;
      }
    if (ok) names = c;
  }
  if (!names) return NULL;

  cJSON *out = cJSON_CreateArray();
  if (!out) return NULL;
  cJSON *row;
  cJSON_ArrayForEach(row, rows) {
    if (!cJSON_IsArray(row)) continue;
    cJSON *o = cJSON_CreateObject();
    if (!o) break;
    int i = 0;
    cJSON *cell;
    cJSON_ArrayForEach(cell, row) {
      /* A row longer than the header keeps the positional name csv.c already
       * uses for the same situation, rather than losing the surplus cells. */
      char pos[24];
      const char *key;
      if (i < width) key = cJSON_GetArrayItem(names, i)->valuestring;
      else { snprintf(pos, sizeof pos, "col%d", i); key = pos; }
      i++;
      if (cJSON_IsNull(cell)) continue;          /* an empty cell is nothing */
      cJSON *dupv = cJSON_Duplicate(cell, 1);
      if (dupv) cJSON_AddItemToObject(o, key, dupv);
    }
    cJSON_AddItemToArray(out, o);
  }
  return out;
}

/* The node a path names, whatever its type — jsonlist_find_array's sibling for
 * the cases below, which have to look at something that is deliberately NOT an
 * array. "", "*" and "." all mean the document itself. */
static cJSON *jsonlist_node(cJSON *doc, const char *path) {
  if (!path || !*path || !strcmp(path, "*") || !strcmp(path, ".")) return doc;
  char buf[256];
  snprintf(buf, sizeof buf, "%s", path);
  cJSON *cur = doc;
  char *save = NULL;
  for (char *tok = strtok_r(buf, ".", &save); tok && cur;
       tok = strtok_r(NULL, ".", &save))
    cur = cJSON_GetObjectItem(cur, tok);
  return cur;
}

/* The same table, written a third way: one array PER COLUMN, side by side, with
 * no array of records anywhere in the document.
 *
 * api.energy-charts.info answers `{"license_info":"CC BY 4.0 …",
 * "unix_seconds":[96 timestamps], "price":[96 numbers], "unit":"EUR / MWh",
 * "deprecated":false}`. There is no array of objects, so jsonlist_find_array
 * returns nothing under every path spelling and all 96 half-hourly wholesale
 * prices were discarded on every run — 31 registered sources on that one API.
 *
 * The pairing of index i across the arrays is the publisher's own row
 * structure, not an inference this file is making up — but it IS the one thing
 * here that was not stated field-by-field, so every projected record carries
 * `_projection` saying so in-band. The guard is arity: EVERY array in the
 * object must be all-scalar and exactly the same length, and there must be at
 * least two of them. One array of a different length means the document is
 * something else and it is left alone rather than guessed at. */
static cJSON *parallel_arrays_objects(cJSON *node) {
  if (!node || !cJSON_IsObject(node)) return NULL;
  int len = -1, cols = 0;
  for (cJSON *k = node->child; k; k = k->next) {
    if (!cJSON_IsArray(k)) continue;
    int n = cJSON_GetArraySize(k);
    if (n < 2) return NULL;                 /* not a column */
    cJSON *e;
    cJSON_ArrayForEach(e, k)
      if (cJSON_IsObject(e) || cJSON_IsArray(e)) return NULL;  /* not scalar */
    if (len < 0) len = n;
    else if (n != len) return NULL;         /* ragged — ambiguous, refuse */
    cols++;
  }
  if (cols < 2 || len < 2) return NULL;

  cJSON *out = cJSON_CreateArray();
  if (!out) return NULL;
  for (int i = 0; i < len; i++) {
    cJSON *o = cJSON_CreateObject();
    if (!o) break;
    for (cJSON *k = node->child; k; k = k->next) {
      if (!cJSON_IsArray(k) || !k->string) continue;
      cJSON *cell = cJSON_GetArrayItem(k, i);
      if (!cell || cJSON_IsNull(cell)) continue;
      cJSON *dupv = cJSON_Duplicate(cell, 1);
      if (dupv) cJSON_AddItemToObject(o, k->string, dupv);
    }
    if (cJSON_GetArraySize(o) == 0) { cJSON_Delete(o); continue; }
    cJSON_AddStringToObject(o, "_projection",
                            "parallel-arrays: row i is index i of every column"
                            " array the upstream published together");
    cJSON_AddItemToArray(out, o);
  }
  if (cJSON_GetArraySize(out) == 0) { cJSON_Delete(out); return NULL; }
  return out;
}

/* ── nested scalar maps ───────────────────────────────────────────────────
 *
 * The fourth way to publish a table: as a map of maps of maps, with the record
 * keys spent on the nesting and no array anywhere at all.
 *
 * IMF's DataMapper is the fleet's case. /api/v1/PCPIPCH answers
 * {"values":{"PCPIPCH":{"SDN":{"1980":26.5,"1981":24,…},"USA":{…},…}},
 *  "api":{"version":"1","output-method":"json"}} — 228 economies × ~47 years =
 * 10,789 real World Economic Outlook observations, measured, in one response,
 * and jsonlist_find_array returns NULL under every path spelling because there
 * is no array to find. Nine registered sources sit on that API and all nine
 * emitted zero on every run.
 *
 * The projection reads what the document says and nothing else: one record per
 * LEAF, carrying the leaf's value and the keys that addressed it. It is
 * deliberately narrow — the subtree must be uniformly deep, every leaf must be
 * a scalar, no arrays may appear anywhere inside it, and it must hold at least
 * eight leaves, so an error envelope or a two-field config block is not mistaken
 * for a dataset. Like the other two projections it stamps `_projection` on every
 * record, because the row structure is the one thing here the upstream did not
 * write out itself. */
#define MAP_MAX_DEPTH 6

/* Uniform depth and leaf count of an all-scalar-leaf map. depth < 0 means the
 * subtree is not that shape (mixed depths, an array, an empty object). */
static void map_profile(cJSON *node, int limit, int *depth, long *leaves) {
  *depth = -1; *leaves = 0;
  if (!cJSON_IsObject(node) || !node->child || limit <= 0) return;
  int d = -1; long total = 0;
  for (cJSON *k = node->child; k; k = k->next) {
    if (!k->string || !k->string[0]) return;
    if (cJSON_IsString(k) || cJSON_IsNumber(k) || cJSON_IsBool(k)) {
      if (d < 0) d = 1; else if (d != 1) return;
      total++;
    } else if (cJSON_IsObject(k)) {
      int cd; long cl;
      map_profile(k, limit - 1, &cd, &cl);
      if (cd < 0) return;
      if (d < 0) d = cd + 1; else if (d != cd + 1) return;
      total += cl;
    } else return;                    /* array, null — not this shape */
  }
  *depth = d; *leaves = total;
}

/* The richest eligible subtree, preferring the OUTERMOST on a tie so the
 * outer key survives as a field rather than being spent walking past it. */
static void map_best(cJSON *node, int limit, cJSON **best, long *bestl) {
  if (!cJSON_IsObject(node) || limit <= 0) return;
  int d; long l;
  map_profile(node, MAP_MAX_DEPTH, &d, &l);
  if (d >= 2 && l >= 8 && l > *bestl) { *best = node; *bestl = l; }
  for (cJSON *k = node->child; k; k = k->next)
    if (cJSON_IsObject(k)) map_best(k, limit - 1, best, bestl);
}

static void map_flatten(cJSON *node, int level, char keys[][160],
                        const char *prefix, cJSON *out) {
  for (cJSON *k = node->child; k; k = k->next) {
    if (!k->string) continue;
    char path[512];
    snprintf(path, sizeof path, "%s%s%s", prefix, prefix[0] ? "." : "", k->string);
    if (cJSON_IsObject(k)) {
      if (level < MAP_MAX_DEPTH) {
        snprintf(keys[level], 160, "%s", k->string);
        map_flatten(k, level + 1, keys, path, out);
      }
      continue;
    }
    cJSON *o = cJSON_CreateObject();
    if (!o) return;
    /* The dotted address IS this record's identifier inside the document — it
     * is what makes the row stable across runs and distinct from its siblings —
     * so it is named `id` and jsonlist keys the row on it. Nothing is invented:
     * every segment is a key the upstream wrote. */
    cJSON_AddStringToObject(o, "id", path);
    for (int i = 0; i < level; i++) {
      char kn[16];
      snprintf(kn, sizeof kn, "key%d", i + 1);
      cJSON_AddStringToObject(o, kn, keys[i]);
    }
    cJSON_AddStringToObject(o, "key", k->string);
    /* Built fresh rather than duplicated: a duplicate carries the leaf's own
     * key in ->string, which would then have to be freed before the value could
     * be re-named "value" — and freeing a cJSON-allocated string with libc's
     * free() is only correct while the default allocator hooks are installed.
     * map_profile has already guaranteed the leaf is one of these three. */
    cJSON *v = cJSON_IsString(k) ? cJSON_CreateString(k->valuestring)
             : cJSON_IsNumber(k) ? cJSON_CreateNumber(k->valuedouble)
             : cJSON_CreateBool(cJSON_IsTrue(k));
    if (v) cJSON_AddItemToObject(o, "value", v);
    cJSON_AddStringToObject(o, "_projection",
                            "nested-map: one record per leaf, keyed by the"
                            " path the upstream nested it under");
    cJSON_AddItemToArray(out, o);
  }
}

static cJSON *scalar_map_objects(cJSON *node) {
  if (!node || !cJSON_IsObject(node)) return NULL;
  cJSON *best = NULL; long bestl = 0;
  map_best(node, MAP_MAX_DEPTH, &best, &bestl);
  if (!best) return NULL;
  cJSON *out = cJSON_CreateArray();
  if (!out) return NULL;
  char keys[MAP_MAX_DEPTH][160];
  map_flatten(best, 0, keys, "", out);
  if (cJSON_GetArraySize(out) == 0) { cJSON_Delete(out); return NULL; }
  return out;
}

/* One record → one row. Split out of the loop so that a single-object
 * document ("." path) goes through byte-identical logic to a list element;
 * two code paths that "should" agree are how the tree grew its duplicate-fix
 * problem in the first place. */
/* Everything a record resolves to, derived once. This used to live inline in
 * emit_record; it was lifted out because the uid collision guard in
 * jsonlist_emit has to derive the SAME title/link/date a second time, BEFORE
 * anything is emitted, in order to see which records are about to key onto each
 * other. Two copies of this logic is exactly how the two collector engines in
 * this tree drifted apart in the first place, so there is one. */
typedef struct {
  char  *title, *link, *when, *rid;
  cJSON *fields;                 /* rec, or the envelope the label came from */
  int    geo;
  double lat, lon;
  /* 1 = the label came from a LAST-RESORT composition (measurement, id,
   * first scalar) rather than from the title precedence list or an envelope
   * descent. Read by the shape tally below: a page on which no record had a
   * real label is the jsonlist form of "title_keys matched 0 of N". */
  int    label_fallback;
} row_view;

/* ── schema drift, disclosed as data ─────────────────────────────────────
 *
 * The same disclosure lib/hpengine.c makes (hp_shape_notices), for the two
 * conditions this engine can measure: a declared record path that resolved to
 * nothing, and a page of records none of which carried a label the precedence
 * list knows. Both are runs that "succeeded" while reading the wrong thing.
 *
 * _Thread_local for the same reason the scratch buffer at :207 is: this runs
 * on 8 scheduler workers and 16 dispatch workers at once, and a shared tally
 * would attribute one source's drift to another. jsonlist_emit() is called
 * once per PAGE — directly by ~6,500 generated collectors (one page) and by
 * jsonlist_emit_paged() in a loop — so the paged walk marks itself `paged`
 * and flushes once at the end, while a direct caller flushes per call. */
#include "osintemit.h"
typedef struct {
  int  paged;          /* inside jsonlist_emit_paged: accumulate, flush at end */
  int  n;              /* records that resolved to a row                     */
  int  labelled;       /* …of which the label came from the precedence list  */
  int  dropped;        /* records that resolved to nothing at all             */
  int  path_missing;   /* pages where the declared path resolved to nothing   */
  char path_kind[48];  /* what it resolved to                                  */
  int  pages;
} jl_shape;
static _Thread_local jl_shape g_jl_shape;

static void jl_shape_reset(void) {
  int paged = g_jl_shape.paged;
  memset(&g_jl_shape, 0, sizeof g_jl_shape);
  g_jl_shape.paged = paged;
}

static void jl_shape_flush(intel_sink *sink, const char *source_id,
                           const char *path, int emitted) {
  jl_shape *t = &g_jl_shape;
  char title[512];
  if (t->path_missing) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "declared_array_path", path ? path : "");
    cJSON_AddStringToObject(p, "resolved_to", t->path_kind);
    cJSON_AddNumberToObject(p, "pages_affected", t->path_missing);
    cJSON_AddNumberToObject(p, "pages_read", t->pages);
    cJSON_AddNumberToObject(p, "records_emitted", emitted);
    cJSON_AddStringToObject(p, "remedy",
      "the upstream's envelope changed: re-point the collector's record path "
      "(tools/diagnose_emit_keys.py), or the row is not a record source");
    snprintf(title, sizeof title,
             "%s: declared record path \"%s\" resolved to %s on %d of %d page(s); "
             "%d record(s) emitted",
             source_id, path ? path : "", t->path_kind, t->path_missing,
             t->pages, emitted);
    jo_shape_notice(sink, source_id, "array-path-missing", title, p, NULL);
  }
  if ((t->n + t->dropped) > 0 && t->labelled == 0) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "records_seen", t->n + t->dropped);
    cJSON_AddNumberToObject(p, "records_labelled", 0);
    cJSON_AddNumberToObject(p, "records_emitted_under_fallback_label", t->n);
    cJSON_AddNumberToObject(p, "records_dropped_unlabelled", t->dropped);
    cJSON_AddNumberToObject(p, "pages_read", t->pages);
    cJSON_AddStringToObject(p, "remedy",
      "no record carried a field the title precedence list knows: add the "
      "upstream's label key to K_TITLE in lib/jsonlist.c, or give the row a "
      "hpengine table with title_keys");
    snprintf(title, sizeof title,
             "%s: label keys matched 0 of %d record(s) on %d page(s); %d emitted "
             "under a fallback label, %d dropped as unlabelled",
             source_id, t->n + t->dropped, t->pages, t->n, t->dropped);
    jo_shape_notice(sink, source_id, "label-keys-unmatched", title, p, NULL);
  }
  jl_shape_reset();
}

static void row_view_free(row_view *v) {
  free(v->title); free(v->link); free(v->when); free(v->rid);
  v->title = v->link = v->when = v->rid = NULL;
}

/* 1 = this record resolves to an intel row; 0 = shape noise (R1). */
static int row_derive(cJSON *rec, const char *record_type, row_view *v) {
    memset(v, 0, sizeof *v);
    if (!cJSON_IsObject(rec)) return 0;

    char *when = scalar_dup(pick(rec, K_DATE));
    double lat = 0, lon = 0;
    int geo = record_coords(rec, &lat, &lon);

    char *title = scalar_dup(pick(rec, K_TITLE));
    if (!title) title = pick_name_suffixed(rec);

    /* Envelope descent. JSON:API (`{id, type, attributes:{…}}`) and Esri
     * (`{attributes:{…}, geometry:{…}}`) put every real field one level down,
     * so a record that plainly HAS a name scores as untitled and is dropped.
     * MBTA's route and stop APIs are the canonical case here.
     *
     * Only consulted when the top level yielded no label, so a record that
     * already emits is untouched. When the inner object supplies the title,
     * the other fields are re-read from it too — taking the label from one
     * object and the date from another would attribute a value to a record
     * that never carried it. */
    cJSON *fields = rec;
    if (!title) {
      /* Dotted entries descend more than one level. OpenDataSoft's Explore
       * v2.1 catalogue is the case that forced it: a dataset record is
       * {visibility, fields[], dataset_id, dataset_uid, has_records, features,
       * attachments, metas:{dcat, inspire, default:{title, description, theme,
       * keyword, license, modified, …}, custom}} — the title is two levels
       * down under metas.default, so a one-level descent found nothing and
       * every record was dropped as unlabelled. Measured against
       * data.loire-atlantique.fr: total_count 2, records emitted 0, while the
       * response carried real titles ("Sentiers et points d'intérêt
       * d'Abbaretz"), licences and modification dates. Same shape on every ODS
       * deployment, which is a large share of the EU portal fleet. */
      /* `resource` is Socrata's cross-portal catalogue envelope. A hit on
       * api.us.socrata.com/api/catalog/v1 is
       * {resource:{name, id, description, columns_name[], updatedAt, …},
       *  classification, metadata, permalink, link, owner, creator} — the
       * dataset's name, id and every column are one level down under
       * `resource`, and the outer object carries nothing title-ish at all, so
       * all 100 results of every catalogue query were dropped as unlabelled.
       * Measured against ?q=restaurant%20inspections: 100 results returned,
       * 0 emitted, while the first one alone was "DOHMH New York City
       * Restaurant Inspection Results". 75 registered sources are built on
       * this one endpoint shape. */
      static const char *const ENV[] = {
        "attributes", "properties", "resource", "metas.default", "metas", NULL
      };
      for (int i = 0; ENV[i] && !title; i++) {
        cJSON *inner = rec;
        char path[64];
        snprintf(path, sizeof path, "%s", ENV[i]);
        char *save = NULL;
        for (char *seg = strtok_r(path, ".", &save); seg && inner;
             seg = strtok_r(NULL, ".", &save))
          inner = cJSON_GetObjectItem(inner, seg);
        if (!inner || !cJSON_IsObject(inner)) continue;
        title = scalar_dup(pick(inner, K_TITLE));
        if (!title) title = pick_name_suffixed(inner);
        if (title) {
          fields = inner;
          free(when);
          when = scalar_dup(pick(inner, K_DATE));
          if (!geo) geo = record_coords(inner, &lat, &lon);
        }
      }
    }

    /* Last resort. A record with a real timestamp and a real measurement is a
     * genuine observation — a fire detection, a buoy reading, a geomagnetic
     * index — that simply has no name column. Composing a label out of the
     * record's OWN values is presentation, not fabrication: every part below
     * was supplied by the upstream, nothing is guessed, and the alternative is
     * discarding real data.
     *
     * This originally required coordinates AND a timestamp, because NASA FIRMS
     * (latitude, longitude, acq_date, frp) was the first case that needed it.
     * A full-fleet probe then measured **279 sources dropping 232,792 records
     * per pass** for want of a label, and the dominant shape was a pure time
     * series — NOAA SWPC's {time_tag, kp}, {time_tag, flux} — which has a real
     * timestamp and a real measurement but no position. Requiring geometry was
     * an accident of the first example, not a principle.
     *
     * It is still not "anything with a date": the record must carry a
     * timestamp AND at least one numeric field, and the label names that field,
     * so the row says what it actually measured rather than being a bare
     * timestamp. A record with neither a name nor a measurement is still not
     * an intel row (R1). */
    char composed[192];
    if (!title && when) {
      const cJSON *meas = first_measure(fields);
      if (geo && meas)
        snprintf(composed, sizeof composed, "%s %s %s=%s (%.4f, %.4f)",
                 record_type ? record_type : "observation", when,
                 meas->string, num_brief(meas), lat, lon);
      else if (geo)
        snprintf(composed, sizeof composed, "%s %s (%.4f, %.4f)",
                 record_type ? record_type : "observation", when, lat, lon);
      else if (meas)
        snprintf(composed, sizeof composed, "%s %s %s=%s",
                 record_type ? record_type : "observation", when,
                 meas->string, num_brief(meas));
      if (geo || meas) { title = strdup(composed); v->label_fallback = 1; }
    }

    /* id stays anchored to the OUTER record when it has one: JSON:API puts the
     * stable identifier at the top level and the descriptive fields inside
     * `attributes`, and the uid is what makes a row stable across runs. */
    char *rid  = scalar_dup(pick(rec, K_ID));
    if (!rid && fields != rec) rid = scalar_dup(pick(fields, K_ID));

    /* Still unlabelled, but the upstream gave it a stable id: label it
     * "<record_type> <id>", exactly as lib/hpengine.c:588 already does.
     *
     * The two engines disagreed on the same question and hpengine had the
     * better answer. Government tables are routinely keyed on a code with no
     * human-readable column anywhere in the row — EPA Envirofacts
     * tri_transfer_qty is {doc_ctrl_num, transfer_loc_num, type_of_waste_
     * management, …}: real regulatory data, no name, no timestamp, so the
     * measurement composer above cannot fire either. jsonlist dropped every
     * such record. Measured over a 59-source sample of batch 16, 33 fetched
     * successfully and emitted NOTHING, and this shape was the dominant cause.
     *
     * This is composition, not invention, on the same grounds the comment above
     * argues for the measurement case: record_type is the collector's own
     * declared type and the id came from the upstream. Nothing is guessed. A
     * record with neither a title NOR an id is still shape noise and is still
     * dropped. */
    char idtitle[224];
    if (!title && rid) {
      snprintf(idtitle, sizeof idtitle, "%s %s",
               record_type ? record_type : "record", rid);
      title = strdup(idtitle);
      v->label_fallback = 1;
    }

    /* Still nothing conventional — and "no CONVENTIONALLY NAMED field" is not
     * the same thing as "no content". lib/hpengine.c:717 settled this question
     * for the other engine and this one never got the answer: a record that
     * carried something real is keyed on its FIRST NON-EMPTY SCALAR rather than
     * thrown away.
     *
     * The measured cost of not doing it, on a 1,234-source sweep of the
     * scheduled fleet: Regione Lombardia's access register answers 390 rows of
     * {tipologia_accesso, oggetto_della_richiesta, data_della_richiesta,
     * stato_della_pratica, esito, direzione_competente, …} and Migración
     * Colombia answers 500 of {a_o, mes, nacionalidad, codigo_m49, femenino,
     * masculino, total} — real, complete, public-register rows, every field
     * present, and not one field named title/name/id/date, so every one of
     * them was discarded. Whole national datasets, fetched daily, stored never.
     *
     * It is a worse label than a real title — which is why an upstream that
     * HAS one still wins on every branch above — but a worse label is not a
     * reason to destroy the record. Nothing is invented: record_type is the
     * collector's own declared type and the value came from the upstream. A
     * record that yields no scalar at all is still shape noise and is still
     * dropped (R1). */
    char fstitle[224];
    if (!title) {
      char *fs = first_scalar_dup(fields);
      if (fs) {
        snprintf(fstitle, sizeof fstitle, "%s %s",
                 record_type ? record_type : "record", fs);
        free(fs);
        title = strdup(fstitle);
        v->label_fallback = 1;
      }
    }
    if (!title) { free(when); free(rid); return 0; }  /* nothing real -> not a row (R1) */

    v->title  = title;
    v->when   = when;
    v->rid    = rid;
    v->fields = fields;
    v->geo    = geo;
    v->lat    = lat;
    v->lon    = lon;
    v->link   = scalar_dup(pick(fields, K_LINK));
    return 1;
}

/* The uid an id-less record falls back to.
 *
 * `disambiguate` folds the serialized record into the hash. It is set ONLY for
 * records that jsonlist_emit has already proved would otherwise key onto a
 * sibling — see the collision guard below — so a record that was uniquely
 * identified by (title, link, date) keeps byte-identical uids across this
 * change and nothing already stored re-emits. */
static void row_hash(const row_view *v, cJSON *rec, int disambiguate,
                     char out[21]) {
  const char *parts[4] = { v->title, v->link ? v->link : "",
                           v->when ? v->when : "", NULL };
  int np = 3;
  char *js = NULL;
  if (disambiguate) {
    js = cJSON_PrintUnformatted(rec);
    parts[3] = js ? js : "";
    np = 4;
  }
  feed_hash_key(out, parts, np);
  free(js);
}

static int emit_record(intel_sink *sink, const char *source_id, cJSON *rec,
                       const char *record_type, const char *lang,
                       const char *tags_json, int disambiguate) {
    row_view v;
    if (!row_derive(rec, record_type, &v)) return 0;
    g_jl_shape.n++;
    if (!v.label_fallback) g_jl_shape.labelled++;

    char *body  = scalar_dup(pick(v.fields, K_BODY));
    char *props = cJSON_PrintUnformatted(rec);

    /* uid: prefer the upstream's own id, else a hash of the record's stable
     * parts. Deriving it from the serialized record unconditionally would make
     * every cosmetic upstream change look like a new row, which is why the
     * record's content only joins the hash when it has to. */
    char hash[21] = {0};
    if (!v.rid) row_hash(&v, rec, disambiguate, hash);
    char uid[192];
    snprintf(uid, sizeof uid, "%s|%s", source_id, v.rid ? v.rid : hash);

    intel_item it = {0};
    it.uid             = uid;
    it.remote_key      = v.rid;
    it.title           = v.title;
    it.body            = body;
    it.link            = v.link;
    it.lang            = lang;
    it.published_at    = v.when;
    it.record_type     = record_type;
    it.has_geo         = v.geo;
    it.lat             = v.lat;
    it.lon             = v.lon;
    it.properties_json = props ? props : "{}";
    it.tags_json       = tags_json ? tags_json : "[]";

    int ok = sink->emit(sink, &it) >= 0;

    row_view_free(&v);
    free(body); free(props);
    return ok ? 1 : 0;
}

/* ── the uid collision guard ──────────────────────────────────────────────
 *
 * `records=N` in a run line counts sink->emit() CALLS. It says nothing about
 * how many rows landed, because the sink upserts on uid — so a source whose
 * records key onto each other reports a healthy N and stores one row. That is
 * the same invisible data loss as an emit-zero source and it is invisible to
 * the metric normally used to find them.
 *
 * `us-openfda-device-pma-detail` is the case that forced this: 109 PMA
 * supplements, each a distinct regulatory filing, all sharing one device trade
 * name. pick_name_suffixed reads `trade_name` as the title, none of them
 * carries a field this file recognises as an id, and (title, link, date) is
 * therefore identical for all 109. Measured: emitted 109, stored 1.
 *
 * The guard derives every record's fallback key first and only extends the ones
 * that are about to collide. That precision is the point: a record whose key
 * was already unique keeps the uid it has, so this change re-emits nothing that
 * was correctly stored. The colliding groups DO change uid — but those rows
 * were never correctly stored to begin with (n-1 of every group had been
 * overwritten), so the only residue is one stale row per group under the old
 * shared key.
 *
 * Scope is one array — one page of one response. A record on page 2 that keys
 * onto one from page 1 is not caught, because page 1 is already emitted by
 * then; catching that would mean buffering the whole walk in memory. Stated
 * plainly rather than papered over. */
typedef struct { char key[21]; int idx; } keyed_row;

static int keyed_cmp(const void *a, const void *b) {
  return strcmp(((const keyed_row *)a)->key, ((const keyed_row *)b)->key);
}

static unsigned char *collision_map(cJSON *arr, const char *record_type, int n) {
  if (n < 2) return NULL;
  keyed_row *k = malloc((size_t)n * sizeof *k);
  unsigned char *dup = calloc((size_t)n, 1);
  /* Out of memory here must degrade to the old behaviour, not abort the emit:
   * a colliding row is a worse outcome than a dropped guard, but a dropped
   * SOURCE is worse than both. */
  if (!k || !dup) { free(k); free(dup); return NULL; }

  int m = 0, i = 0;
  cJSON *rec;
  cJSON_ArrayForEach(rec, arr) {
    row_view v;
    if (row_derive(rec, record_type, &v)) {
      /* A record with the upstream's own id is keyed on that id; two records
       * sharing one is the upstream telling us they are the same record, and
       * second-guessing it would fabricate a distinction. Only the hashed
       * fallback is at risk here. */
      if (!v.rid) { row_hash(&v, rec, 0, k[m].key); k[m].idx = i; m++; }
      row_view_free(&v);
    }
    i++;
  }
  qsort(k, (size_t)m, sizeof *k, keyed_cmp);
  int flagged = 0;
  for (int a = 0; a < m; ) {
    int b = a + 1;
    while (b < m && !strcmp(k[a].key, k[b].key)) b++;
    if (b - a > 1)
      for (int j = a; j < b; j++) { dup[k[j].idx] = 1; flagged++; }
    a = b;
  }
  free(k);
  if (!flagged) { free(dup); return NULL; }
  return dup;
}

/* ── list envelopes ───────────────────────────────────────────────────────
 *
 * A "record" that holds nothing but ANOTHER list of records is not a record;
 * it is a grouping level the declared path stopped one short of.
 *
 * JMA's warning feed is the fleet's case, and it is 56 registered sources.
 * warning/440000.json is {reportDatetime, publishingOffice, headlineText,
 * areaTypes:[{areas:[{code:"440010", warnings:[{status:"…"}]}, …]}, …]} and
 * the rows declare path "areatypes" — so every element handed to emit_record
 * is {areas:[…]}, which has no label, no id and not one scalar of its own. All
 * 56 prefectural warning feeds stored nothing, on a 15-minute schedule, for
 * live disaster data.
 *
 * Only reached when the record itself produced no row, and only when it has NO
 * scalar of its own and EXACTLY ONE array of objects inside it — one candidate
 * and nothing else to lose, so there is no choice being made here. Depth-capped
 * because a malicious or merely odd document could nest these forever. */
#define ENVELOPE_MAX_DEPTH 3

static int emit_array(intel_sink *sink, const char *source_id, cJSON *arr,
                      const char *record_type, const char *lang,
                      const char *tags_json, int depth);

static int emit_wrapped_list(intel_sink *sink, const char *source_id,
                             cJSON *rec, const char *record_type,
                             const char *lang, const char *tags_json,
                             int depth) {
  if (depth >= ENVELOPE_MAX_DEPTH || !cJSON_IsObject(rec)) return 0;
  cJSON *only = NULL;
  for (cJSON *k = rec->child; k; k = k->next) {
    /* Any scalar means the record had content of its own; it was dropped for
     * some other reason and descending past it would lose that content. */
    if (cJSON_IsString(k) || cJSON_IsNumber(k) || cJSON_IsBool(k)) return 0;
    if (array_of_objects(k)) {
      if (only) return 0;                 /* two candidates — not obvious */
      only = k;
    }
  }
  if (!only) return 0;
  return emit_array(sink, source_id, only, record_type, lang, tags_json,
                    depth + 1);
}

static int emit_array(intel_sink *sink, const char *source_id, cJSON *arr,
                      const char *record_type, const char *lang,
                      const char *tags_json, int depth) {
  int total = cJSON_GetArraySize(arr);
  unsigned char *dup = collision_map(arr, record_type, total);

  int n = 0, i = 0;
  cJSON *rec;
  cJSON_ArrayForEach(rec, arr) {
    int got = emit_record(sink, source_id, rec, record_type, lang, tags_json,
                          dup && dup[i]);
    if (!got)
      got = emit_wrapped_list(sink, source_id, rec, record_type, lang,
                              tags_json, depth);
    if (!got) g_jl_shape.dropped++;      /* no label, no id, no scalar: shape tally */
    n += got;
    i++;
  }
  free(dup);
  return n;
}

int jsonlist_emit(intel_sink *sink, const char *source_id, cJSON *doc,
                  const char *path, const char *record_type,
                  const char *lang, const char *tags_json) {
  /* A single-object document is one record, not a degenerate list. Several
   * public APIs return exactly this — a status or summary document — and
   * treating it as an empty list would silently drop the source. */
  if (!g_jl_shape.paged) jl_shape_reset();   /* a direct caller: one page, one tally */
  g_jl_shape.pages++;
  if (path && !strcmp(path, ".")) {
    int n = emit_record(sink, source_id, doc, record_type, lang, tags_json, 0);
    if (!n)
      n = emit_wrapped_list(sink, source_id, doc, record_type, lang,
                            tags_json, 0);
    if (!g_jl_shape.paged) jl_shape_flush(sink, source_id, path, n);
    return n;
  }

  /* A columnar table is a list of records written sideways; project it before
   * anything else looks at it. `owned` is non-NULL only when we built one. */
  cJSON *arr = jsonlist_find_array(doc, path), *owned = NULL;
  if (arr) {
    owned = columnar_objects(columnar_parent(doc, path), arr);
    if (owned) arr = owned;
  } else {
    /* No array under this path at all. Before reporting an empty source, ask
     * whether the records are here in a shape that simply is not an array. */
    cJSON *node = jsonlist_node(doc, path);
    owned = parallel_arrays_objects(node);
    if (!owned) owned = scalar_map_objects(node);
    if (!owned) {
      /* Shape notice: a DECLARED path ("" is a bare array and "*" is
       * discovery — neither declares anything) that names nothing in this
       * document, in no shape this engine knows. Said as data, not only as a
       * clean zero. */
      if (path && *path && strcmp(path, "*")) {
        const char *kind = !node ? "absent"
                         : cJSON_IsObject(node) ? "an object with no record array"
                         : cJSON_IsString(node) ? "a string" : cJSON_IsNumber(node) ? "a number"
                         : cJSON_IsNull(node)   ? "null" : "a node of another type";
        g_jl_shape.path_missing++;
        snprintf(g_jl_shape.path_kind, sizeof g_jl_shape.path_kind, "%s", kind);
        if (!g_jl_shape.paged) jl_shape_flush(sink, source_id, path, 0);
      }
      return 0;
    }
    arr = owned;
  }

  int n = emit_array(sink, source_id, arr, record_type, lang, tags_json, 0);
  cJSON_Delete(owned);
  if (!g_jl_shape.paged) jl_shape_flush(sink, source_id, path, n);
  return n;
}

/* ── the page walk ────────────────────────────────────────────────────────
 * See jsonlist.h for why this exists. Everything below is about ONE question:
 * has the upstream told us there is more? We answer it from what the upstream
 * actually said, and never from an assumption. */

/* Dotted lookup that tolerates a missing level, for probing candidate keys. */
static cJSON *dotted(cJSON *doc, const char *path) {
  char buf[128];
  snprintf(buf, sizeof buf, "%s", path);
  cJSON *cur = doc;
  char *save = NULL;
  for (char *tok = strtok_r(buf, ".", &save); tok && cur;
       tok = strtok_r(NULL, ".", &save))
    cur = cJSON_GetObjectItem(cur, tok);
  return cur;
}

/* The server's own "next page" link, if it published one. Ordered most- to
 * least-specific: `links.next` is the JSON:API/CKAN spelling, `@odata.nextLink`
 * is OData, the rest are common house styles. A next link that is present but
 * null/false/empty means "this is the last page" in every one of these
 * dialects, so it is treated as absent rather than followed. */
/* Resolve a next-link against the page it came from. An absolute URL passes
 * through; "?page=2" and "/api/x?page=2" are the two relative spellings that
 * actually occur (bio.tools answers `"next": "?page=2&format=json"`, and that
 * one string was the whole reason 30,000 bio.tools rows stopped at page 1 —
 * the link WAS published, we just refused to read it). Anything else — a
 * scheme we do not speak, a bare token — is not a URL and is refused rather
 * than glued onto the base and hoped for. */
static char *resolve_link(const char *base, const char *href) {
  if (!href || !href[0]) return NULL;
  if (!strncmp(href, "http://", 7) || !strncmp(href, "https://", 8))
    return strdup(href);
  size_t cap = strlen(base) + strlen(href) + 2;
  char *out = malloc(cap);
  if (!out) return NULL;
  if (href[0] == '?') {                       /* same path, new query */
    const char *q = strchr(base, '?');
    size_t head = q ? (size_t)(q - base) : strlen(base);
    memcpy(out, base, head);
    snprintf(out + head, cap - head, "%s", href);
    return out;
  }
  if (href[0] == '/') {                       /* same origin, new path */
    const char *p = strstr(base, "://");
    const char *slash = p ? strchr(p + 3, '/') : NULL;
    size_t head = slash ? (size_t)(slash - base) : strlen(base);
    memcpy(out, base, head);
    snprintf(out + head, cap - head, "%s", href);
    return out;
  }
  free(out);
  return NULL;
}

/* The server's own "next page" link, if it published one. Ordered most- to
 * least-specific: `links.next` is the JSON:API/CKAN spelling, `@odata.nextLink`
 * is OData, the rest are common house styles. A next link that is present but
 * null/false/empty means "this is the last page" in every one of these
 * dialects, so it is treated as absent rather than followed. */
static char *next_link(cJSON *doc, const char *base) {
  static const char *const KEYS[] = {
    "links.next", "next", "next_url", "nextUrl", "nextPageUrl",
    "meta.next", "paging.next", "@odata.nextLink", "next_page",
    "meta.pagination.next", "pagination.next", "pagination.next_page",
    "meta.pagination.next_page", "links.next_url", NULL };
  for (int i = 0; KEYS[i]; i++) {
    cJSON *v = dotted(doc, KEYS[i]);
    if (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
      char *u = resolve_link(base, v->valuestring);
      if (u) return u;
    }
    /* CKAN and some JSON:API servers nest it as {next: {href: "..."}}. */
    if (v && cJSON_IsObject(v)) {
      cJSON *h = cJSON_GetObjectItem(v, "href");
      if (h && cJSON_IsString(h) && h->valuestring) {
        char *u = resolve_link(base, h->valuestring);
        if (u) return u;
      }
    }
  }
  return NULL;
}

/* Fingerprint of a page's records, used only to notice that the upstream is
 * handing back the SAME page again. That happens whenever a cursor guess is
 * wrong — the server ignores the parameter it does not know and re-serves
 * page 1 — and without this the walk would spend the whole ceiling refetching
 * one page and then file a truncation notice claiming pages were pending.
 * Stopping on no-progress makes a wrong guess cost one wasted request and
 * tell no lies. */
static unsigned long long page_fp(cJSON *arr) {
  unsigned long long h = 1469598103934665603ULL;   /* FNV-1a 64 */
  if (!arr) return h;
  int i = 0;
  cJSON *rec;
  cJSON_ArrayForEach(rec, arr) {
    char *s = cJSON_PrintUnformatted(rec);
    if (s) {
      for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        h = (h ^ *p) * 1099511628211ULL;
      free(s);
    }
    if (++i >= 8) break;   /* exhaustive-ok: identity probe, not a record read */
  }
  return h;
}

/* The upstream's own count of what exists, for the in-band disclosure. -1 when
 * it did not say, which is itself worth recording: "unknown" is honest, a
 * guessed total is not. */
static long declared_total(cJSON *doc) {
  static const char *const KEYS[] = {
    "total_count", "totalCount", "total", "count", "meta.count",
    "numberMatched", "totalResults", "result.count", "meta.total",
    "totalElements", "recordsTotal",
    /* The families whose totals the walk was blind to. ROR answers
     * `number_of_results: 108000` next to 20 items, Solr `response.numFound`,
     * OpenDataSoft `nhits`, CORDIS `payload.totalHits`; each of those is the
     * upstream telling us, in its own words, how much it is holding back. */
    "number_of_results", "numFound", "response.numFound", "nhits",
    "totalHits", "payload.totalHits", "totalResultCount", "total_results",
    "resultCount", "hits.total", "meta.pagination.total", "pagination.total",
    NULL };
  for (int i = 0; KEYS[i]; i++) {
    cJSON *v = dotted(doc, KEYS[i]);
    if (v && cJSON_IsNumber(v) && v->valuedouble >= 0)
      return (long)v->valuedouble;
  }
  return -1;
}

/* Read `name=<int>` out of a query string. Returns -1 if absent/unparseable. */
long jsonlist_query_int(const char *url, const char *name) {
  const char *q = strchr(url, '?');
  if (!q) return -1;
  size_t nlen = strlen(name);
  for (const char *p = q + 1; p && *p; ) {
    if (!strncmp(p, name, nlen) && p[nlen] == '=') {
      char *end = NULL;
      long v = strtol(p + nlen + 1, &end, 10);
      return (end && end != p + nlen + 1) ? v : -1;
    }
    p = strchr(p, '&');
    if (p) p++;
  }
  return -1;
}

/* Replace `name=<old>` with `name=<new>`, or append it. Caller frees. */
char *jsonlist_query_set(const char *url, const char *name, long value) {
  size_t cap = strlen(url) + strlen(name) + 48;
  char *out = malloc(cap);
  if (!out) return NULL;
  const char *q = strchr(url, '?');
  size_t nlen = strlen(name);
  const char *hit = NULL;
  if (q) {
    for (const char *p = q + 1; p && *p; ) {
      if (!strncmp(p, name, nlen) && p[nlen] == '=') { hit = p; break; }
      p = strchr(p, '&');
      if (p) p++;
    }
  }
  if (!hit) {
    snprintf(out, cap, "%s%c%s=%ld", url, q ? '&' : '?', name, value);
    return out;
  }
  const char *tail = strchr(hit, '&');
  size_t head = (size_t)(hit - url);
  memcpy(out, url, head);
  int w = snprintf(out + head, cap - head, "%s=%ld", name, value);
  if (tail) snprintf(out + head + w, cap - head - w, "%s", tail);
  return out;
}

/* A page-size parameter the URL already declares, paired with the cursor
 * parameter that upstream family uses to advance. The pairing is what makes
 * the arithmetic safe: we only ever move a cursor whose page-size sibling is
 * present, so a URL with no declared page size is never paginated by guess.
 *
 * `alt_cursor` is the same family's other spelling. It is used only when the
 * URL ALREADY carries it, which is how openFDA (`limit` + `skip`) is told
 * apart from Socrata (`limit` + `offset`): both declare `limit`, and moving
 * the wrong one would re-serve page 1 forever. */
struct pager { const char *size_param, *cursor_param, *alt_cursor; int page_numbered; };
static const struct pager PAGERS[] = {
  { "per_page",  "page",   NULL,     1 },   /* CKAN/dane.gov.pl, GitHub, uData */
  { "page_size", "page",   NULL,     1 },   /* DRF                             */
  { "pageSize",  "page",   NULL,     1 },   /* ArcGIS Hub, many .NET APIs      */
  { "perPage",   "page",   NULL,     1 },   /* house style                     */
  { "rp",        "page",   NULL,     1 },   /* flexigrid — taginfo (OSM)       */
  { "itemsPerPage", "page",NULL,     1 },   /* ERDDAP index.json               */
  { "size",      "page",   NULL,     1 },   /* Spring Data / Apollo (RESF)     */
  { "num",       "p",      NULL,     1 },   /* CORDIS search API               */
  { "rows",      "start",  NULL,     0 },   /* Solr / CKAN package_search      */
  { "length",    "start",  NULL,     0 },   /* DataTables — HUDOC (ECHR)       */
  { "limit",     "offset", "skip",   0 },   /* Socrata, ODS, most REST; openFDA*/
  { "$top",      "$skip",  NULL,     0 },   /* OData                           */
  { "maxRecords","offset", NULL,     0 },   /* Airtable-style                  */
  { NULL, NULL, NULL, 0 }
};

int jsonlist_emit_paged(intel_sink *sink, const char *source_id,
                        http_client *http, const char *url, int timeout_ms,
                        const char *path, const char *record_type,
                        const char *lang, const char *tags_json) {
  int page_max = 20;   /* exhaustive-ok: page-walk ceiling; an early stop is disclosed as a collector-truncation-notice and $JO_JSONLIST_PAGE_MAX raises it */
  const char *env = getenv("JO_JSONLIST_PAGE_MAX");
  if (env && *env) {
    int v = atoi(env);
    if (v > 0) page_max = v;
  }

  char *page_url = strdup(url);
  if (!page_url) return -1;
  /* One shape tally for the WHOLE walk: jsonlist_emit() accumulates per page
   * and the flush below files one notice per condition per run. */
  jl_shape_reset();
  g_jl_shape.paged = 1;

  int total = 0, pages = 0, truncated = 0;
  long available = -1;
  unsigned long long prev_fp = 0;
  int repeated = 0;

  for (; pages < page_max && page_url; pages++) {
    cJSON *doc = feed_get_json(http, page_url, timeout_ms);
    if (!doc) {
      /* A failed FIRST fetch is a dead endpoint and belongs to the caller as
       * an error. A failure mid-walk is different: we already have real
       * records, so we keep them and stop — but the walk ended early, which is
       * a shortfall and gets disclosed below. */
      if (pages == 0) { free(page_url); g_jl_shape.paged = 0; return -1; }
      truncated = 1;
      break;
    }
    if (available < 0) available = declared_total(doc);

    int n = jsonlist_emit(sink, source_id, doc, path, record_type, lang, tags_json);
    total += n;

    cJSON *arr = jsonlist_find_array(doc, path);
    int got = arr ? cJSON_GetArraySize(arr) : 0;

    /* Did the upstream actually move? An ignored cursor parameter re-serves
     * the page we already have; that is the end of the data as far as this
     * URL is concerned, not a page we are owed. */
    unsigned long long fp = page_fp(arr);
    if (pages > 0 && got > 0 && fp == prev_fp) repeated = 1;
    prev_fp = fp;

    char *next = repeated ? NULL : next_link(doc, page_url);
    if (!next && got > 0 && !repeated) {
      /* No server link. Advance a cursor only when the URL declares a page
       * size AND this page came back exactly full — a short page is the
       * upstream saying it is finished, and following it would be us
       * inventing a page that was never offered. */
      for (int i = 0; PAGERS[i].size_param && !next; i++) {
        long size = jsonlist_query_int(page_url, PAGERS[i].size_param);
        if (size <= 0 || got < size) continue;
        /* Move the cursor this URL already names, when the family has two
         * spellings; otherwise the canonical one. */
        const char *cursor = PAGERS[i].cursor_param;
        if (PAGERS[i].alt_cursor && jsonlist_query_int(page_url, PAGERS[i].alt_cursor) >= 0)
          cursor = PAGERS[i].alt_cursor;
        long cur = jsonlist_query_int(page_url, cursor);
        long nextval = PAGERS[i].page_numbered
                         ? (cur > 0 ? cur + 1 : 2)
                         : (cur >= 0 ? cur + size : size);
        next = jsonlist_query_set(page_url, cursor, nextval);
      }
    }
    /* Last resort, and the only one that needs no page-size sibling: the
     * upstream published a total BIGGER than what it has handed us, and the
     * URL already carries a page number. ROR says `number_of_results: 108000`
     * and hands over 20; retsinformation.dk says `totalResultCount` and hands
     * over a screenful. There is no guessing here — the remainder is the
     * upstream's own arithmetic — and the cursor is one this URL already
     * declares, so we are turning a dial the caller wrote, not inventing one.
     * A server that ignores it re-serves page 1 and the no-progress guard
     * above ends the walk on the next turn. */
    if (!next && got > 0 && !repeated && available > (long)(total)) {
      static const char *const PAGE_CURSORS[] = { "page", "p", "pageNumber",
                                                  "pagina", "pageNum", NULL };
      for (int i = 0; PAGE_CURSORS[i] && !next; i++) {
        long cur = jsonlist_query_int(page_url, PAGE_CURSORS[i]);
        if (cur < 1) continue;               /* must already be declared */
        next = jsonlist_query_set(page_url, PAGE_CURSORS[i], cur + 1);
      }
    }
    cJSON_Delete(doc);

    if (got <= 0 || repeated) { free(next); break; }   /* upstream is exhausted */
    free(page_url);
    page_url = next;
    if (pages + 1 >= page_max && page_url) truncated = 1;   /* ceiling bit */
  }
  free(page_url);

  if (truncated) {
    /* Same disclosure hpengine makes, same record_type, so a partial result is
     * mechanically detectable from either engine. A ceiling stop leaves an
     * UNKNOWN remainder — we never fetched those pages — so records_available
     * may be -1 here, and saying "unknown" is the honest answer. */
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "source_id", source_id);
    cJSON_AddStringToObject(p, "endpoint", url);
    cJSON_AddNumberToObject(p, "records_used", total);
    if (available >= 0) cJSON_AddNumberToObject(p, "records_available", available);
    else cJSON_AddStringToObject(p, "records_available", "unknown — upstream declared no total");
    cJSON_AddNumberToObject(p, "pages_read", pages);
    cJSON_AddNumberToObject(p, "page_ceiling", page_max);
    cJSON_AddBoolToObject(p, "more_pages_pending", 1);
    cJSON_AddStringToObject(p, "reason",
      "the page ceiling stopped the walk while the upstream still had pages");
    cJSON_AddStringToObject(p, "remedy",
      "raise $JO_JSONLIST_PAGE_MAX — see docs/SOURCE_EXHAUSTIVENESS.md");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);
    char title[256];
    /* remote_key is just "truncation": the sink derives the uid as
     * "<source_id>|<remote_key>", so prefixing the id here would stutter it
     * into "<id>|<id>|truncation". One notice per source, upsert-keyed, so a
     * re-run updates the disclosure rather than piling up duplicates. */
    const char *key = "truncation";
    if (available >= 0)
      snprintf(title, sizeof title, "%s used %d of %ld available records",
               source_id, total, available);
    else
      snprintf(title, sizeof title, "%s used %d records and stopped at the page ceiling",
               source_id, total);
    intel_item note = {0};
    note.remote_key      = key;
    note.title           = title;
    note.lang            = "en";
    note.record_type     = "collector-truncation-notice";
    note.properties_json = pj ? pj : "{}";
    note.tags_json       = "[\"truncation-notice\"]";
    sink->emit(sink, &note);
    free(pj);
  }

  /* Schema drift measured during the walk, one record per condition, in
   * addition to the records above. Clears `paged` for the next caller. */
  jl_shape_flush(sink, source_id, path, total);
  g_jl_shape.paged = 0;

  if (pages > 1 || truncated)
    fprintf(stderr, "[%s] emitted %d across %d page(s)%s\n",
            source_id, total, pages, truncated ? " (TRUNCATED)" : "");
  return total;
}
