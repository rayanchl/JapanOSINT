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
  if (d == (double) (long long) d) snprintf(b, sizeof b, "%lld", (long long) d);
  else                             snprintf(b, sizeof b, "%.6g", d);
  return b;
}

static int array_of_objects(cJSON *a) {
  if (!cJSON_IsArray(a) || cJSON_GetArraySize(a) == 0) return 0;
  cJSON *first = cJSON_GetArrayItem(a, 0);
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

/* One record → one row. Split out of the loop so that a single-object
 * document ("." path) goes through byte-identical logic to a list element;
 * two code paths that "should" agree are how the tree grew its duplicate-fix
 * problem in the first place. */
static int emit_record(intel_sink *sink, const char *source_id, cJSON *rec,
                       const char *record_type, const char *lang,
                       const char *tags_json) {
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
      static const char *const ENV[] = {
        "attributes", "properties", "metas.default", "metas", NULL
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
      if (geo || meas) title = strdup(composed);
    }
    if (!title) { free(when); return 0; }  /* no label -> not a row (R1) */

    char *link = scalar_dup(pick(fields, K_LINK));
    /* id stays anchored to the OUTER record when it has one: JSON:API puts the
     * stable identifier at the top level and the descriptive fields inside
     * `attributes`, and the uid is what makes a row stable across runs. */
    char *rid  = scalar_dup(pick(rec, K_ID));
    if (!rid && fields != rec) rid = scalar_dup(pick(fields, K_ID));
    char *body = scalar_dup(pick(fields, K_BODY));
    char *props = cJSON_PrintUnformatted(rec);

    /* uid: prefer the upstream's own id, else a hash of the record's stable
     * parts. Deriving it from the serialized record would make every
     * cosmetic upstream change look like a new row. */
    char hash[21] = {0};
    if (!rid) {
      const char *parts[3] = { title, link ? link : "", when ? when : "" };
      feed_hash_key(hash, parts, 3);
    }
    char uid[192];
    snprintf(uid, sizeof uid, "%s|%s", source_id, rid ? rid : hash);

    intel_item it = {0};
    it.uid             = uid;
    it.remote_key      = rid;
    it.title           = title;
    it.body            = body;
    it.link            = link;
    it.lang            = lang;
    it.published_at    = when;
    it.record_type     = record_type;
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = props ? props : "{}";
    it.tags_json       = tags_json ? tags_json : "[]";

    int ok = sink->emit(sink, &it) >= 0;

    free(title); free(link); free(when); free(rid); free(body); free(props);
    return ok ? 1 : 0;
}

int jsonlist_emit(intel_sink *sink, const char *source_id, cJSON *doc,
                  const char *path, const char *record_type,
                  const char *lang, const char *tags_json) {
  /* A single-object document is one record, not a degenerate list. Several
   * public APIs return exactly this — a status or summary document — and
   * treating it as an empty list would silently drop the source. */
  if (path && !strcmp(path, "."))
    return emit_record(sink, source_id, doc, record_type, lang, tags_json);

  cJSON *arr = jsonlist_find_array(doc, path);
  if (!arr) return 0;

  int n = 0;
  cJSON *rec;
  cJSON_ArrayForEach(rec, arr)
    n += emit_record(sink, source_id, rec, record_type, lang, tags_json);
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
static char *next_link(cJSON *doc) {
  static const char *const KEYS[] = {
    "links.next", "next", "next_url", "nextUrl", "nextPageUrl",
    "meta.next", "paging.next", "@odata.nextLink", "next_page", NULL };
  for (int i = 0; KEYS[i]; i++) {
    cJSON *v = dotted(doc, KEYS[i]);
    if (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0] &&
        !strncmp(v->valuestring, "http", 4))
      return strdup(v->valuestring);
    /* CKAN and some JSON:API servers nest it as {next: {href: "..."}}. */
    if (v && cJSON_IsObject(v)) {
      cJSON *h = cJSON_GetObjectItem(v, "href");
      if (h && cJSON_IsString(h) && h->valuestring && !strncmp(h->valuestring, "http", 4))
        return strdup(h->valuestring);
    }
  }
  return NULL;
}

/* The upstream's own count of what exists, for the in-band disclosure. -1 when
 * it did not say, which is itself worth recording: "unknown" is honest, a
 * guessed total is not. */
static long declared_total(cJSON *doc) {
  static const char *const KEYS[] = {
    "total_count", "totalCount", "total", "count", "meta.count",
    "numberMatched", "totalResults", "result.count", "meta.total",
    "totalElements", "recordsTotal", NULL };
  for (int i = 0; KEYS[i]; i++) {
    cJSON *v = dotted(doc, KEYS[i]);
    if (v && cJSON_IsNumber(v) && v->valuedouble >= 0)
      return (long)v->valuedouble;
  }
  return -1;
}

/* Read `name=<int>` out of a query string. Returns -1 if absent/unparseable. */
static long query_int(const char *url, const char *name) {
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
static char *query_set(const char *url, const char *name, long value) {
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
 * present, so a URL with no declared page size is never paginated by guess. */
struct pager { const char *size_param, *cursor_param; int page_numbered; };
static const struct pager PAGERS[] = {
  { "per_page",  "page",        1 },   /* CKAN/dane.gov.pl, GitHub, uData     */
  { "page_size", "page",        1 },   /* DRF                                 */
  { "pageSize",  "page",        1 },   /* ArcGIS Hub, many .NET APIs          */
  { "rows",      "start",       0 },   /* Solr / CKAN package_search          */
  { "limit",     "offset",      0 },   /* Socrata, ODS, most REST             */
  { "$top",      "$skip",       0 },   /* OData                               */
  { "maxRecords","offset",      0 },   /* Airtable-style                      */
  { NULL, NULL, 0 }
};

int jsonlist_emit_paged(intel_sink *sink, const char *source_id,
                        http_client *http, const char *url, int timeout_ms,
                        const char *path, const char *record_type,
                        const char *lang, const char *tags_json) {
  int page_max = 20;
  const char *env = getenv("JO_JSONLIST_PAGE_MAX");
  if (env && *env) {
    int v = atoi(env);
    if (v > 0) page_max = v;
  }

  char *page_url = strdup(url);
  if (!page_url) return -1;

  int total = 0, pages = 0, truncated = 0;
  long available = -1;

  for (; pages < page_max && page_url; pages++) {
    cJSON *doc = feed_get_json(http, page_url, timeout_ms);
    if (!doc) {
      /* A failed FIRST fetch is a dead endpoint and belongs to the caller as
       * an error. A failure mid-walk is different: we already have real
       * records, so we keep them and stop — but the walk ended early, which is
       * a shortfall and gets disclosed below. */
      if (pages == 0) { free(page_url); return -1; }
      truncated = 1;
      break;
    }
    if (available < 0) available = declared_total(doc);

    int n = jsonlist_emit(sink, source_id, doc, path, record_type, lang, tags_json);
    total += n;

    cJSON *arr = jsonlist_find_array(doc, path);
    int got = arr ? cJSON_GetArraySize(arr) : 0;

    char *next = next_link(doc);
    if (!next && got > 0) {
      /* No server link. Advance a cursor only when the URL declares a page
       * size AND this page came back exactly full — a short page is the
       * upstream saying it is finished, and following it would be us
       * inventing a page that was never offered. */
      for (int i = 0; PAGERS[i].size_param && !next; i++) {
        long size = query_int(page_url, PAGERS[i].size_param);
        if (size <= 0 || got < size) continue;
        long cur = query_int(page_url, PAGERS[i].cursor_param);
        long nextval = PAGERS[i].page_numbered
                         ? (cur > 0 ? cur + 1 : 2)
                         : (cur >= 0 ? cur + size : size);
        next = query_set(page_url, PAGERS[i].cursor_param, nextval);
      }
    }
    cJSON_Delete(doc);

    if (got <= 0) { free(next); break; }   /* upstream is exhausted */
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

  if (pages > 1 || truncated)
    fprintf(stderr, "[%s] emitted %d across %d page(s)%s\n",
            source_id, total, pages, truncated ? " (TRUNCATED)" : "");
  return total;
}
