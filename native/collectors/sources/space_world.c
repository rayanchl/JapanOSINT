/* collectors/sources/space_world.c
 * OSINT services — satellites / space objects. Global-scoped, on-demand entity
 * pivot (ctx->entity = a satellite name / NORAD id / designator; NULL on a
 * scheduled feed run is fine — CelesTrak/SatNOGS then just emit the head of the
 * live catalog). Every run() REAL-fetches a public endpoint and emits ONLY
 * parsed live records, honest-empty (return 0), or gated (missing key → log +
 * return 0). Nothing is fabricated; constructed URLs are never emitted as
 * records.
 *
 *   CELESTRAK_TLE  CelesTrak GP element sets (keyless JSON) — active satellites
 *   SATNOGS        SatNOGS DB satellites catalog (keyless JSON)
 *   SPACETRACK     Space-Track.org (gated SPACETRACK_USER / SPACETRACK_PASS)
 *   N2YO           N2YO tracking API (gated N2YO_KEY)
 *
 * One run() dispatches on ctx->source_id. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* number field → malloc'd string, or NULL. caller frees. */
static char *sp_num(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v || !cJSON_IsNumber(v)) return NULL;
  char buf[64];
  double d = v->valuedouble;
  if (d == (double)(long long)d) snprintf(buf, sizeof buf, "%lld", (long long)d);
  else snprintf(buf, sizeof buf, "%.6g", d);
  return strdup(buf);
}

/* does record r (any of a set of name-ish fields) match the query? q may be
 * NULL (scheduled run) → match everything. */
static int sp_match(const cJSON *r, const char *q, const char *const *keys) {
  if (!q || !*q) return 1;
  for (int i = 0; keys[i]; i++) {
    const char *v = jo_sv(r, keys[i]);
    if (v && jo_stristr(v, q)) return 1;
  }
  return 0;
}

/* ---- CelesTrak GP (keyless JSON array of GP element sets) ---------------- *
 * Each element: OBJECT_NAME, OBJECT_ID (intl designator), NORAD_CAT_ID,
 * EPOCH, MEAN_MOTION, ECCENTRICITY, INCLINATION, RA_OF_ASC_NODE, ARG_OF_PERICENTER,
 * MEAN_ANOMALY, ... (CelesTrak OMM/GP JSON schema). */
static int sp_celestrak(const source_ctx *ctx, intel_sink *sink, const char *q, int max) {
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (satellite)", NULL };
  char *body = jo_get(ctx,
    "https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=json",
    hdrs, "CELESTRAK_TLE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!cJSON_IsArray(root)) { if (root) cJSON_Delete(root); return 0; }

  static const char *mkeys[] = { "OBJECT_NAME", "OBJECT_ID", NULL };
  int emitted = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, root) {
    if (emitted >= max) break;
    if (!sp_match(r, q, mkeys)) continue;
    const char *name = jo_sv(r, "OBJECT_NAME");
    const char *desig = jo_sv(r, "OBJECT_ID");
    const char *epoch = jo_sv(r, "EPOCH");
    char *norad = sp_num(r, "NORAD_CAT_ID");
    char *incl  = sp_num(r, "INCLINATION");
    char *ecc   = sp_num(r, "ECCENTRICITY");
    char *mm    = sp_num(r, "MEAN_MOTION");
    if (!name && !norad) { free(norad); free(incl); free(ecc); free(mm); continue; }

    cJSON *data = cJSON_CreateObject();
    if (name)  cJSON_AddStringToObject(data, "object_name", name);
    if (desig) cJSON_AddStringToObject(data, "intl_designator", desig);
    if (norad) cJSON_AddStringToObject(data, "norad_cat_id", norad);
    if (epoch) cJSON_AddStringToObject(data, "epoch", epoch);
    if (incl)  cJSON_AddStringToObject(data, "inclination", incl);
    if (ecc)   cJSON_AddStringToObject(data, "eccentricity", ecc);
    if (mm)    cJSON_AddStringToObject(data, "mean_motion", mm);
    cJSON_AddStringToObject(data, "source", "CelesTrak");
    char *bj = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "CELESTRAK_TLE");
    cJSON_AddStringToObject(props, "source", "CelesTrak");
    if (norad) cJSON_AddStringToObject(props, "norad_cat_id", norad);
    if (desig) cJSON_AddStringToObject(props, "intl_designator", desig);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char link[256] = {0};
    if (norad) snprintf(link, sizeof link,
      "https://celestrak.org/satcat/table-satcat.php?CATNR=%s", norad);

    intel_item it = {0};
    it.remote_key      = norad ? norad : (desig ? desig : name);
    it.title           = name ? name : (norad ? norad : desig);
    it.summary         = desig;
    it.body            = bj;
    it.lang            = "en";
    it.published_at    = epoch;
    it.link            = link[0] ? link : NULL;
    it.record_type     = "satellite-tle";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"satellite\",\"CELESTRAK\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj); free(norad); free(incl); free(ecc); free(mm);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[CELESTRAK_TLE] emitted %d\n", emitted);
  return emitted;
}

/* ---- SatNOGS DB satellites (keyless JSON array) ------------------------- *
 * Each: sat_id, norad_cat_id, name, names, image, status, launched, deployed,
 * decayed, countries, ... (SatNOGS DB API schema). */
static int sp_satnogs(const source_ctx *ctx, intel_sink *sink, const char *q, int max) {
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (satellite)", NULL };
  char *body = jo_get(ctx, "https://db.satnogs.org/api/satellites/",
                      hdrs, "SATNOGS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!cJSON_IsArray(root)) { if (root) cJSON_Delete(root); return 0; }

  static const char *mkeys[] = { "name", "names", "sat_id", NULL };
  int emitted = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, root) {
    if (emitted >= max) break;
    /* norad may be numeric; build a string version for matching + fields */
    char *norad = sp_num(r, "norad_cat_id");
    const char *name  = jo_sv(r, "name");
    if (!sp_match(r, q, mkeys) &&
        !(q && *q && norad && jo_stristr(norad, q))) { free(norad); continue; }
    const char *satid  = jo_sv(r, "sat_id");
    const char *status = jo_sv(r, "status");
    const char *launched = jo_sv(r, "launched");
    const char *countries = jo_sv(r, "countries");
    const char *image  = jo_sv(r, "image");
    if (!name && !norad && !satid) { free(norad); continue; }

    cJSON *data = cJSON_CreateObject();
    if (name)      cJSON_AddStringToObject(data, "name", name);
    if (satid)     cJSON_AddStringToObject(data, "sat_id", satid);
    if (norad)     cJSON_AddStringToObject(data, "norad_cat_id", norad);
    if (status)    cJSON_AddStringToObject(data, "status", status);
    if (launched)  cJSON_AddStringToObject(data, "launched", launched);
    if (countries) cJSON_AddStringToObject(data, "countries", countries);
    cJSON_AddStringToObject(data, "source", "SatNOGS");
    char *bj = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "SATNOGS");
    cJSON_AddStringToObject(props, "source", "SatNOGS");
    if (norad)  cJSON_AddStringToObject(props, "norad_cat_id", norad);
    if (satid)  cJSON_AddStringToObject(props, "sat_id", satid);
    if (status) cJSON_AddStringToObject(props, "status", status);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char link[256] = {0};
    if (satid) snprintf(link, sizeof link,
      "https://db.satnogs.org/satellite/%s", satid);

    intel_item it = {0};
    it.remote_key      = satid ? satid : (norad ? norad : name);
    it.title           = name ? name : (satid ? satid : norad);
    it.summary         = status;
    it.body            = bj;
    it.lang            = "en";
    it.link            = link[0] ? link : (image ? image : NULL);
    it.record_type     = "satellite-catalog";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"satellite\",\"SATNOGS\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj); free(norad);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[SATNOGS] emitted %d\n", emitted);
  return emitted;
}

/* ---- Space-Track.org (gated) ------------------------------------------- *
 * Requires a free account. Flow: POST identity/password to /ajaxauth/login
 * (cookie session), then GET a satcat query. Gated on SPACETRACK_USER /
 * SPACETRACK_PASS — no creds → honest empty. */
static int sp_spacetrack(const source_ctx *ctx, intel_sink *sink, const char *q, int max) {
  const char *user = jo_env("SPACETRACK_USER");
  const char *pass = jo_env("SPACETRACK_PASS");
  if (!user || !pass) {
    fprintf(stderr, "[SPACETRACK] gated (no SPACETRACK_USER/SPACETRACK_PASS)\n");
    return 0;
  }

  /* login (sets a session cookie handled by the http client) */
  char *eu = jo_urlencode(user), *ep = jo_urlencode(pass);
  if (!eu || !ep) { free(eu); free(ep); return 0; }
  char form[512];
  int fn = snprintf(form, sizeof form, "identity=%s&password=%s", eu, ep);
  free(eu); free(ep);
  const char *lh[] = { "Content-Type: application/x-www-form-urlencoded", NULL };
  http_response lr = {0};
  int rc = http_request(ctx->http, "POST",
    "https://www.space-track.org/ajaxauth/login", lh, form, (size_t)fn,
    20000, 1, &lr);
  if (rc != 0 || (lr.status != 200 && lr.status != 302)) {
    fprintf(stderr, "[SPACETRACK] login status=%ld\n", lr.status);
    http_response_free(&lr);
    return 0;
  }
  http_response_free(&lr);

  /* satcat query: by name when we have an entity, else latest launches */
  char url[1024];
  if (q && *q) {
    char *enc = jo_urlencode(q);
    if (!enc) return 0;
    snprintf(url, sizeof url,
      "https://www.space-track.org/basicspacedata/query/class/satcat/"
      "SATNAME/~~%s/orderby/LAUNCH desc/limit/%d/format/json", enc, max);
    free(enc);
  } else {
    snprintf(url, sizeof url,
      "https://www.space-track.org/basicspacedata/query/class/satcat/"
      "CURRENT/Y/orderby/LAUNCH desc/limit/%d/format/json", max);
  }
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "SPACETRACK");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!cJSON_IsArray(root)) { if (root) cJSON_Delete(root); return 0; }

  int emitted = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, root) {
    if (emitted >= max) break;
    const char *name  = jo_sv(r, "SATNAME");
    const char *norad = jo_sv(r, "NORAD_CAT_ID");
    const char *desig = jo_sv(r, "INTLDES");
    const char *launch = jo_sv(r, "LAUNCH");
    const char *country = jo_sv(r, "COUNTRY");
    const char *objtype = jo_sv(r, "OBJECT_TYPE");
    if (!name && !norad) continue;

    cJSON *data = cJSON_CreateObject();
    if (name)    cJSON_AddStringToObject(data, "object_name", name);
    if (norad)   cJSON_AddStringToObject(data, "norad_cat_id", norad);
    if (desig)   cJSON_AddStringToObject(data, "intl_designator", desig);
    if (launch)  cJSON_AddStringToObject(data, "launch", launch);
    if (country) cJSON_AddStringToObject(data, "country", country);
    if (objtype) cJSON_AddStringToObject(data, "object_type", objtype);
    cJSON_AddStringToObject(data, "source", "Space-Track");
    char *bj = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "SPACETRACK");
    cJSON_AddStringToObject(props, "source", "Space-Track");
    if (norad)   cJSON_AddStringToObject(props, "norad_cat_id", norad);
    if (country) cJSON_AddStringToObject(props, "country", country);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char link[256] = {0};
    if (norad) snprintf(link, sizeof link,
      "https://www.space-track.org/#/catalog/%s", norad);

    intel_item it = {0};
    it.remote_key      = norad ? norad : name;
    it.title           = name ? name : norad;
    it.summary         = desig;
    it.body            = bj;
    it.lang            = "en";
    it.published_at    = launch;
    it.link            = link[0] ? link : NULL;
    it.record_type     = "satellite-catalog";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"satellite\",\"SPACETRACK\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[SPACETRACK] emitted %d\n", emitted);
  return emitted;
}

/* ---- N2YO (gated by N2YO_KEY) ------------------------------------------ *
 * The N2YO REST API is keyed and pivots on a NORAD id (numeric). We only run
 * when the entity is a NORAD id; otherwise there's nothing honest to fetch
 * (N2YO has no name-search endpoint). /rest/v1/satellite/tle/{id}&apiKey=..
 * returns { info: { satname, satid }, tle }. */
static int sp_n2yo(const source_ctx *ctx, intel_sink *sink, const char *q, int max) {
  (void)max;
  const char *key = jo_env("N2YO_KEY");
  if (!key) {
    fprintf(stderr, "[N2YO] gated (no N2YO_KEY)\n");
    return 0;
  }
  /* need a numeric NORAD id */
  if (!q || !*q) { fprintf(stderr, "[N2YO] no NORAD id entity\n"); return 0; }
  for (const char *p = q; *p; p++)
    if (!isdigit((unsigned char)*p)) { fprintf(stderr, "[N2YO] entity not a NORAD id\n"); return 0; }

  char url[512];
  snprintf(url, sizeof url,
    "https://api.n2yo.com/rest/v1/satellite/tle/%s&apiKey=%s", q, key);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "N2YO");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *info = cJSON_GetObjectItem(root, "info");
  const char *name = info ? jo_sv(info, "satname") : NULL;
  char *satid = info ? sp_num(info, "satid") : NULL;
  const char *tle = jo_sv(root, "tle");
  int emitted = 0;
  if (name || satid) {
    cJSON *data = cJSON_CreateObject();
    if (name)  cJSON_AddStringToObject(data, "name", name);
    if (satid) cJSON_AddStringToObject(data, "norad_cat_id", satid);
    if (tle)   cJSON_AddStringToObject(data, "tle", tle);
    cJSON_AddStringToObject(data, "source", "N2YO");
    char *bj = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "N2YO");
    cJSON_AddStringToObject(props, "source", "N2YO");
    if (satid) cJSON_AddStringToObject(props, "norad_cat_id", satid);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char link[256];
    snprintf(link, sizeof link, "https://www.n2yo.com/satellite/?s=%s", q);

    intel_item it = {0};
    it.remote_key      = satid ? satid : q;
    it.title           = name ? name : q;
    it.body            = bj;
    it.lang            = "en";
    it.link            = link;
    it.record_type     = "satellite-tle";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"satellite\",\"N2YO\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj);
  }
  free(satid);
  cJSON_Delete(root);
  fprintf(stderr, "[N2YO] emitted %d\n", emitted);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  const char *id = ctx->source_id;
  if (strcmp(id, "CELESTRAK_TLE") == 0) { sp_celestrak(ctx, sink, q, 60); return 0; }
  if (strcmp(id, "SATNOGS") == 0)       { sp_satnogs(ctx, sink, q, 60);   return 0; }
  if (strcmp(id, "SPACETRACK") == 0)    { sp_spacetrack(ctx, sink, q, 40); return 0; }
  if (strcmp(id, "N2YO") == 0)          { sp_n2yo(ctx, sink, q, 1);        return 0; }
  return -1;
}

#define SPACE_DEF(SYM, ID, NAME, NAMEJA, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "satellite", .type = "api", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

SPACE_DEF(sp_celestrak_def, "CELESTRAK_TLE", "CelesTrak TLE / GP Elements",
  "CelesTrak 軌道要素", "https://celestrak.org",
  "CelesTrak active-satellite GP/TLE element sets (keyless JSON): name, NORAD id, designator, orbit");
SPACE_DEF(sp_satnogs_def, "SATNOGS", "SatNOGS Satellite DB",
  "SatNOGS 衛星データベース", "https://db.satnogs.org",
  "SatNOGS DB satellites catalog (keyless JSON): name, NORAD id, sat_id, status, launch");
SPACE_DEF(sp_spacetrack_def, "SPACETRACK", "Space-Track Satellite Catalog",
  "Space-Track 衛星カタログ", "https://www.space-track.org",
  "Space-Track.org satcat query (needs SPACETRACK_USER/SPACETRACK_PASS): name, NORAD id, launch, country");
SPACE_DEF(sp_n2yo_def, "N2YO", "N2YO Satellite Tracking",
  "N2YO 衛星追跡", "https://www.n2yo.com",
  "N2YO TLE lookup by NORAD id (needs N2YO_KEY): satellite name, NORAD id, two-line elements");
