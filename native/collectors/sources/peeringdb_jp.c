/* collectors/telecom/sources/peeringdb_jp.c
 * Port of server/src/collectors/peeringdbJp.js.
 * Three independent PeeringDB JSON fetches (country=JP): /ix, /fac, /net.
 * Merged into one FeatureCollection in JS order: all IXP features, then all
 * facility features, then all network features. _meta dropped.
 *
 * AUDIT 2026-07-31: the JS original pinned every IXP and every NETWORK at
 * Tokyo station, and fell back to Tokyo station for a facility with no
 * coordinates. /api/ix carries no latitude/longitude at all (27 JP IXPs, 0
 * with coords) and an ASN is not a place, so those ~1.1k pins were invented.
 * Only facilities have real coordinates; everything else is now emitted
 * WITHOUT a geometry key (a cJSON null would serialise to the literal string
 * "null" in the geometry column, so the key must be absent). `city` /
 * `country` stay in properties for anyone who wants to geocode later. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define URL_NET "https://www.peeringdb.com/api/net?info_traffic__contains=&info_scope__contains=&info_type__contains=&policy_general__contains=&country=JP&depth=0&limit=1000"
#define URL_IX  "https://www.peeringdb.com/api/ix?country=JP&depth=0&limit=500"
#define URL_FAC "https://www.peeringdb.com/api/fac?country=JP&depth=0&limit=1000"

/* fetchPdb: GET → return j.data array (cJSON, owned by `doc`) or NULL. */
static cJSON *fetch_pdb(const source_ctx *ctx, const char *url, cJSON **doc) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *j = feed_get_json_h(ctx->http, url, hdrs, 20000);
  *doc = j;
  if (!j) return NULL;
  cJSON *d = cJSON_GetObjectItem(j, "data");
  return cJSON_IsArray(d) ? d : NULL;
}

/* JS pass-through: o.k is copied verbatim (may be string/number/null/absent
 * → cJSON null). PeeringDB omits → undefined → JSON drops it; we emit null to
 * keep the property present (geojson key-order parity is what matters here,
 * these features have no feature.id so uid = sha1{g,p}). */
static cJSON *pv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v || cJSON_IsNull(v)) return cJSON_CreateNull();
  return cJSON_Duplicate(v, 1);
}

static cJSON *mk_geom(double lon, double lat) {
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  return g;
}

/* Number(v) finite test for fac longitude/latitude. */
static int num_finite(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return 0;
  if (cJSON_IsNumber(v)) { *out = cJSON_GetNumberValue(v); return isfinite(*out); }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *e; double d = strtod(v->valuestring, &e);
    if (e != v->valuestring) { *out = d; return isfinite(d); }
  }
  return 0;
}

/* id → "https://www.peeringdb.com/<seg>/<id>" (id may be number/string). */
static void mk_id(char *buf, size_t n, const cJSON *idv) {
  buf[0] = 0;
  if (!idv) return;
  if (cJSON_IsNumber(idv)) snprintf(buf, n, "%g", cJSON_GetNumberValue(idv));
  else if (cJSON_IsString(idv) && idv->valuestring)
    snprintf(buf, n, "%s", idv->valuestring);
}
static void mk_url(char *buf, size_t n, const char *seg, const cJSON *idv) {
  char idbuf[40];
  mk_id(idbuf, sizeof idbuf, idv);
  snprintf(buf, n, "https://www.peeringdb.com/%s/%s", seg, idbuf);
}

/* A stable natural uid. Without one, lib/geojson.c falls back to
 * sha1(geometry+properties) and `idx` is in properties — so a re-ordered
 * PeeringDB page re-inserted every row as new. */
static void add_uid(cJSON *p, const char *seg, const cJSON *idv) {
  char idbuf[40], ub[64];
  mk_id(idbuf, sizeof idbuf, idv);
  if (!idbuf[0]) return;
  snprintf(ub, sizeof ub, "peeringdb:%s:%s", seg, idbuf);
  cJSON_AddStringToObject(p, "uid", ub);
}

/* PeeringDB's own last-modified stamp; without it every row landed undated. */
static void add_updated(cJSON *p, const cJSON *o) {
  const cJSON *v = cJSON_GetObjectItem(o, "updated");
  if (v && cJSON_IsString(v) && v->valuestring[0])
    cJSON_AddStringToObject(p, "published_at", v->valuestring);
}

/* JS `x ?? fallback` for a string field. */
static const char *s_or(const cJSON *o, const char *k, const char *dflt) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : dflt;
}
static long n_or(const cJSON *o, const char *k, long dflt) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsNumber(v)) ? (long)cJSON_GetNumberValue(v) : dflt;
}

/* PeeringDB rate-limits anonymous clients hard. Firing /net, /ix and /fac
 * back-to-back reliably got the 2nd and 3rd request throttled here, so two of
 * the three record kinds silently vanished from the run (27 rows instead of
 * ~1.1k) with no log line to say why. Pace the calls and report each. */
static void pdb_pause(void) {
  struct timespec ts = { 2, 0 };
  nanosleep(&ts, NULL);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *dNet = NULL, *dIx = NULL, *dFac = NULL;
  cJSON *nets = fetch_pdb(ctx, URL_NET, &dNet);
  pdb_pause();
  cJSON *ixs  = fetch_pdb(ctx, URL_IX,  &dIx);
  pdb_pause();
  cJSON *facs = fetch_pdb(ctx, URL_FAC, &dFac);
  fprintf(stderr, "[peeringdb-jp] fetched net=%d ix=%d fac=%d\n",
          nets ? cJSON_GetArraySize(nets) : -1,
          ixs  ? cJSON_GetArraySize(ixs)  : -1,
          facs ? cJSON_GetArraySize(facs) : -1);

  cJSON *features = cJSON_CreateArray();

  /* ── IXPs ── */
  int i = 0;
  cJSON *ix;
  if (ixs) cJSON_ArrayForEach(ix, ixs) {
    char url[80]; mk_url(url, sizeof url, "ix", cJSON_GetObjectItem(ix, "id"));
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    /* no geometry: /api/ix publishes no coordinates */
    cJSON *p = cJSON_CreateObject();
    add_uid(p, "ix", cJSON_GetObjectItem(ix, "id"));
    cJSON_AddNumberToObject(p, "idx", i++);
    cJSON_AddStringToObject(p, "kind", "ixp");
    cJSON_AddItemToObject(p, "pdb_id", pv(ix, "id"));
    cJSON_AddItemToObject(p, "name", pv(ix, "name"));
    cJSON_AddItemToObject(p, "name_long", pv(ix, "name_long"));
    cJSON_AddItemToObject(p, "media", pv(ix, "media"));
    cJSON_AddItemToObject(p, "country", pv(ix, "country"));
    cJSON_AddItemToObject(p, "city", pv(ix, "city"));
    cJSON_AddItemToObject(p, "net_count", pv(ix, "net_count"));
    cJSON_AddItemToObject(p, "proto_unicast", pv(ix, "proto_unicast"));
    cJSON_AddItemToObject(p, "proto_multicast", pv(ix, "proto_multicast"));
    cJSON_AddItemToObject(p, "proto_ipv6", pv(ix, "proto_ipv6"));
    cJSON_AddItemToObject(p, "fac_count", pv(ix, "fac_count"));
    cJSON_AddItemToObject(p, "website", pv(ix, "website"));
    cJSON_AddItemToObject(p, "tech_email", pv(ix, "tech_email"));
    cJSON_AddItemToObject(p, "policy_email", pv(ix, "policy_email"));
    cJSON_AddItemToObject(p, "url_stats", pv(ix, "url_stats"));
    cJSON_AddItemToObject(p, "service_level", pv(ix, "service_level"));
    cJSON_AddItemToObject(p, "region_continent", pv(ix, "region_continent"));
    cJSON_AddItemToObject(p, "created", pv(ix, "created"));
    add_updated(p, ix);
    {
      char sb[320];
      snprintf(sb, sizeof sb,
               "Internet exchange point in %s, %s — %ld connected network%s "
               "across %ld facilit%s, media %s%s.",
               s_or(ix, "city", "Japan"), s_or(ix, "country", "JP"),
               n_or(ix, "net_count", 0), n_or(ix, "net_count", 0) == 1 ? "" : "s",
               n_or(ix, "fac_count", 0), n_or(ix, "fac_count", 0) == 1 ? "y" : "ies",
               s_or(ix, "media", "unspecified"),
               cJSON_IsTrue(cJSON_GetObjectItem(ix, "proto_ipv6")) ? ", IPv6" : "");
      cJSON_AddStringToObject(p, "summary", sb);
    }
    cJSON_AddStringToObject(p, "url", url);
    cJSON_AddStringToObject(p, "source", "peeringdb_ix");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }

  /* ── Facilities ── */
  i = 0;
  cJSON *fc;
  if (facs) cJSON_ArrayForEach(fc, facs) {
    double lon, lat;
    int hasLon = num_finite(fc, "longitude", &lon);
    int hasLat = num_finite(fc, "latitude", &lat);
    char url[80]; mk_url(url, sizeof url, "fac", cJSON_GetObjectItem(fc, "id"));
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    /* real coordinates only — a facility PeeringDB has not geocoded stays
     * un-pinned rather than landing on Tokyo station */
    if (hasLon && hasLat)
      cJSON_AddItemToObject(f, "geometry", mk_geom(lon, lat));
    cJSON *p = cJSON_CreateObject();
    add_uid(p, "fac", cJSON_GetObjectItem(fc, "id"));
    cJSON_AddNumberToObject(p, "idx", i++);
    cJSON_AddStringToObject(p, "kind", "fac");
    cJSON_AddItemToObject(p, "pdb_id", pv(fc, "id"));
    cJSON_AddItemToObject(p, "name", pv(fc, "name"));
    cJSON_AddItemToObject(p, "org_id", pv(fc, "org_id"));
    cJSON_AddItemToObject(p, "country", pv(fc, "country"));
    cJSON_AddItemToObject(p, "city", pv(fc, "city"));
    cJSON_AddItemToObject(p, "clli", pv(fc, "clli"));
    cJSON_AddItemToObject(p, "rencode", pv(fc, "rencode"));
    cJSON_AddItemToObject(p, "npanxx", pv(fc, "npanxx"));
    cJSON_AddItemToObject(p, "net_count", pv(fc, "net_count"));
    cJSON_AddItemToObject(p, "address1", pv(fc, "address1"));
    cJSON_AddItemToObject(p, "zipcode", pv(fc, "zipcode"));
    cJSON_AddItemToObject(p, "website", pv(fc, "website"));
    cJSON_AddItemToObject(p, "latitude", pv(fc, "latitude"));
    cJSON_AddItemToObject(p, "longitude", pv(fc, "longitude"));
    cJSON_AddItemToObject(p, "created", pv(fc, "created"));
    add_updated(p, fc);
    {
      char sb[320];
      snprintf(sb, sizeof sb,
               "Colocation / interconnection facility in %s, %s — %ld network%s "
               "present%s.",
               s_or(fc, "city", "Japan"), s_or(fc, "country", "JP"),
               n_or(fc, "net_count", 0), n_or(fc, "net_count", 0) == 1 ? "" : "s",
               (hasLon && hasLat) ? "" : " (no coordinates published)");
      cJSON_AddStringToObject(p, "summary", sb);
    }
    cJSON_AddStringToObject(p, "url", url);
    cJSON_AddStringToObject(p, "source", "peeringdb_fac");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }

  /* ── Networks ── */
  i = 0;
  cJSON *nw;
  if (nets) cJSON_ArrayForEach(nw, nets) {
    char url[80]; mk_url(url, sizeof url, "net", cJSON_GetObjectItem(nw, "id"));
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    /* no geometry: an ASN is not a place */
    cJSON *p = cJSON_CreateObject();
    add_uid(p, "net", cJSON_GetObjectItem(nw, "id"));
    cJSON_AddNumberToObject(p, "idx", i++);
    cJSON_AddStringToObject(p, "kind", "net");
    cJSON_AddItemToObject(p, "pdb_id", pv(nw, "id"));
    cJSON_AddItemToObject(p, "asn", pv(nw, "asn"));
    cJSON_AddItemToObject(p, "name", pv(nw, "name"));
    cJSON_AddItemToObject(p, "aka", pv(nw, "aka"));
    cJSON_AddItemToObject(p, "info_type", pv(nw, "info_type"));
    cJSON_AddItemToObject(p, "info_traffic", pv(nw, "info_traffic"));
    cJSON_AddItemToObject(p, "info_scope", pv(nw, "info_scope"));
    cJSON_AddItemToObject(p, "info_ratio", pv(nw, "info_ratio"));
    cJSON_AddItemToObject(p, "irr_as_set", pv(nw, "irr_as_set"));
    cJSON_AddItemToObject(p, "info_prefixes4", pv(nw, "info_prefixes4"));
    cJSON_AddItemToObject(p, "info_prefixes6", pv(nw, "info_prefixes6"));
    cJSON_AddItemToObject(p, "policy_general", pv(nw, "policy_general"));
    cJSON_AddItemToObject(p, "website", pv(nw, "website"));
    cJSON_AddItemToObject(p, "looking_glass", pv(nw, "looking_glass"));
    cJSON_AddItemToObject(p, "route_server", pv(nw, "route_server"));
    cJSON_AddItemToObject(p, "created", pv(nw, "created"));
    add_updated(p, nw);
    {
      char sb[360];
      snprintf(sb, sizeof sb,
               "AS%ld %s — %s network, %s traffic, %s scope; %ld IPv4 / %ld IPv6 "
               "prefixes advertised. Peering policy: %s.",
               n_or(nw, "asn", 0), s_or(nw, "name", "(unnamed)"),
               s_or(nw, "info_type", "unspecified"),
               s_or(nw, "info_traffic", "unspecified"),
               s_or(nw, "info_scope", "unspecified"),
               n_or(nw, "info_prefixes4", 0), n_or(nw, "info_prefixes6", 0),
               s_or(nw, "policy_general", "unspecified"));
      cJSON_AddStringToObject(p, "summary", sb);
    }
    cJSON_AddStringToObject(p, "url", url);
    cJSON_AddStringToObject(p, "source", "peeringdb_net");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }

  if (dNet) cJSON_Delete(dNet);
  if (dIx)  cJSON_Delete(dIx);
  if (dFac) cJSON_Delete(dFac);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[peeringdb-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def peeringdb_jp_def = {
  .id = "peeringdb-jp", .collector = "telecom",
  .name = "PeeringDB (JP)", .name_ja = "PeeringDB 日本",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(peeringdb_jp_def)
