/* collectors/sources/osint_extras2.c
 * OSINT services — additional keyless pivots on ctx->entity (lower-value / niche).
 * Real fetch or honest-empty. No API keys.
 *
 *   WIKIDATA_ENTITY_SEARCH www.wikidata.org/w/api.php?action=wbsearchentities
 *   SEC_FULLTEXT           efts.sec.gov/LATEST/search-index?q=
 *   BIGDATACLOUD_REVERSE   api.bigdatacloud.net/data/reverse-geocode-client (lat,lon)
 *   LOBSTERS_SEARCH        lobste.rs/search.json?q=
 *   HN_USER                hn.algolia.com/api/v1/users/<user>
 *   OPENVERSE              api.openverse.org/v1/images/?q=
 *   STACKEXCHANGE_SEARCH   api.stackexchange.com/2.3/search/advanced?q= */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *OE2_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";
/* SEC EDGAR requires a declared UA with contact. */
static const char *SEC_UA = "User-Agent: JapanOSINT osint@example.org";

static int oe2_emit(intel_sink *sink, const char *service, const char *rtype,
                    const char *rk, const char *title, const char *summary,
                    const char *link, cJSON *body, cJSON *props,
                    int has_geo, double lat, double lon) {
  char *bj = body ? cJSON_PrintUnformatted(body) : NULL;
  if (props) { cJSON_AddStringToObject(props, "service", service); cJSON_AddBoolToObject(props, "success", 1); }
  char *pj = props ? cJSON_PrintUnformatted(props) : NULL;
  char tags[96]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.summary = summary;
  it.body = bj; it.link = (link && link[0]) ? link : NULL; it.lang = "en";
  it.has_geo = has_geo; it.lat = lat; it.lon = lon;
  it.record_type = rtype; it.properties_json = pj; it.tags_json = tags;
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  if (body) cJSON_Delete(body); if (props) cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run_wikidata_ent(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url,
    "https://www.wikidata.org/w/api.php?action=wbsearchentities&search=%s&language=en&format=json&limit=20", enc);
  const char *hdrs[] = { "Accept: application/json", OE2_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "WIKIDATA_ENTITY_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *search = cJSON_GetObjectItem(root, "search");
  int n = 0;
  if (cJSON_IsArray(search)) {
    const cJSON *s;
    cJSON_ArrayForEach(s, search) {
      const char *id = jo_sv(s, "id");
      const char *label = jo_sv(s, "label");
      const char *desc = jo_sv(s, "description");
      if (!id) continue;
      cJSON *d = cJSON_CreateObject();
      if (label) cJSON_AddStringToObject(d, "label", label);
      if (desc) cJSON_AddStringToObject(d, "description", desc);
      cJSON_AddStringToObject(d, "qid", id);
      cJSON_AddStringToObject(d, "source", "Wikidata");
      cJSON *p = cJSON_CreateObject(); cJSON_AddStringToObject(p, "qid", id);
      char link[128]; snprintf(link, sizeof link, "https://www.wikidata.org/wiki/%s", id);
      n += oe2_emit(sink, "WIKIDATA_ENTITY_SEARCH", "wikidata-entity", id, label ? label : id, desc, link, d, p, 0, 0, 0);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run_sec_fulltext(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://efts.sec.gov/LATEST/search-index?q=%s", enc);
  const char *hdrs[] = { "Accept: application/json", SEC_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "SEC_FULLTEXT");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *hits = cJSON_GetObjectItem(root, "hits");
  const cJSON *hitarr = hits ? cJSON_GetObjectItem(hits, "hits") : NULL;
  int n = 0;
  if (cJSON_IsArray(hitarr)) {
    const cJSON *h;
    cJSON_ArrayForEach(h, hitarr) {
      const char *hid = jo_sv(h, "_id");
      const cJSON *src = cJSON_GetObjectItem(h, "_source");
      const char *form = src ? jo_sv(src, "form") : NULL;
      const char *fdate = src ? jo_sv(src, "file_date") : NULL;
      const char *disp = NULL;
      const cJSON *names = src ? cJSON_GetObjectItem(src, "display_names") : NULL;
      if (cJSON_IsArray(names) && cJSON_GetArraySize(names)) { const cJSON *n0 = cJSON_GetArrayItem(names, 0);  /* exhaustive-ok: display pick; filers_all carries every filer */ if (cJSON_IsString(n0)) disp = n0->valuestring; }
      if (!hid) continue;
      cJSON *d = cJSON_CreateObject();
      if (cJSON_IsArray(names) && cJSON_GetArraySize(names) > 1)
        cJSON_AddItemToObject(d, "filers_all", cJSON_Duplicate(names, 1));
      if (disp) cJSON_AddStringToObject(d, "filer", disp);
      if (form) cJSON_AddStringToObject(d, "form", form);
      if (fdate) cJSON_AddStringToObject(d, "file_date", fdate);
      cJSON_AddStringToObject(d, "doc_id", hid);
      cJSON_AddStringToObject(d, "source", "SEC EDGAR FTS");
      cJSON *p = cJSON_CreateObject();
      char title[256]; snprintf(title, sizeof title, "%s%s%s", disp ? disp : "SEC filing", form ? " · " : "", form ? form : "");
      n += oe2_emit(sink, "SEC_FULLTEXT", "sec-filing", hid, title, fdate, "https://www.sec.gov/cgi-bin/srqsb", d, p, 0, 0, 0);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run_bigdatacloud(const source_ctx *ctx, intel_sink *sink, const char *raw) {
  double lat, lon;
  if (sscanf(raw, "%lf , %lf", &lat, &lon) != 2 && sscanf(raw, "%lf/%lf", &lat, &lon) != 2) {
    fprintf(stderr, "[BIGDATACLOUD_REVERSE] entity must be 'lat,lon'\n"); return 0;
  }
  char url[320];
  snprintf(url, sizeof url,
    "https://api.bigdatacloud.net/data/reverse-geocode-client?latitude=%.6f&longitude=%.6f&localityLanguage=en", lat, lon);
  const char *hdrs[] = { "Accept: application/json", OE2_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "BIGDATACLOUD_REVERSE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *city = jo_sv(root, "city");
  const char *locality = jo_sv(root, "locality");
  const char *sub = jo_sv(root, "principalSubdivision");
  const char *country = jo_sv(root, "countryName");
  if (!country && !city && !locality) { cJSON_Delete(root); return 0; }
  cJSON *d = cJSON_CreateObject();
  if (locality) cJSON_AddStringToObject(d, "locality", locality);
  if (city) cJSON_AddStringToObject(d, "city", city);
  if (sub) cJSON_AddStringToObject(d, "subdivision", sub);
  if (country) cJSON_AddStringToObject(d, "country", country);
  cJSON_AddNumberToObject(d, "lat", lat); cJSON_AddNumberToObject(d, "lon", lon);
  cJSON_AddStringToObject(d, "source", "BigDataCloud");
  cJSON *p = cJSON_CreateObject();
  char summ[200]; snprintf(summ, sizeof summ, "%s%s%s", locality ? locality : (city ? city : ""), country ? ", " : "", country ? country : "");
  char rk[96]; snprintf(rk, sizeof rk, "revgeo:%.5f,%.5f", lat, lon);
  int rc = oe2_emit(sink, "BIGDATACLOUD_REVERSE", "reverse-geocode", rk, summ[0] ? summ : "location", sub, NULL, d, p, 1, lat, lon);
  cJSON_Delete(root);
  return rc;
}

static int run_hn_user(const source_ctx *ctx, intel_sink *sink, const char *enc, const char *raw) {
  char url[256];
  snprintf(url, sizeof url, "https://hn.algolia.com/api/v1/users/%s", enc);
  const char *hdrs[] = { "Accept: application/json", OE2_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "HN_USER");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *username = jo_sv(root, "username");
  if (!username) { cJSON_Delete(root); return 0; }
  const cJSON *karma = cJSON_GetObjectItem(root, "karma");
  const char *about = jo_sv(root, "about");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "username", username);
  if (karma && cJSON_IsNumber(karma)) cJSON_AddNumberToObject(d, "karma", karma->valuedouble);
  if (about) cJSON_AddStringToObject(d, "about", about);
  cJSON_AddStringToObject(d, "source", "Hacker News");
  cJSON *p = cJSON_CreateObject();
  char link[200]; snprintf(link, sizeof link, "https://news.ycombinator.com/user?id=%s", username);
  char title[160]; snprintf(title, sizeof title, "HN user %s (karma %.0f)", username, (karma && cJSON_IsNumber(karma)) ? karma->valuedouble : 0);
  int rc = oe2_emit(sink, "HN_USER", "hackernews-user", username, title, about, link, d, p, 0, 0, 0);
  cJSON_Delete(root); (void)raw;
  return rc;
}

static int run_openverse(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://api.openverse.org/v1/images/?q=%s&page_size=20", enc);
  const char *hdrs[] = { "Accept: application/json", OE2_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "OPENVERSE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  int n = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *id = jo_sv(r, "id");
      const char *title = jo_sv(r, "title");
      const char *creator = jo_sv(r, "creator");
      const char *landing = jo_sv(r, "foreign_landing_url");
      const char *license = jo_sv(r, "license");
      if (!id) continue;
      cJSON *d = cJSON_CreateObject();
      if (title) cJSON_AddStringToObject(d, "title", title);
      if (creator) cJSON_AddStringToObject(d, "creator", creator);
      if (license) cJSON_AddStringToObject(d, "license", license);
      cJSON_AddStringToObject(d, "source", "Openverse");
      cJSON *p = cJSON_CreateObject();
      n += oe2_emit(sink, "OPENVERSE", "openverse-image", id, title ? title : id, creator, landing, d, p, 0, 0, 0);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run_stackexchange(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[640];
  snprintf(url, sizeof url,
    "https://api.stackexchange.com/2.3/search/advanced?order=desc&sort=relevance&q=%s&site=stackoverflow", enc);
  const char *hdrs[] = { "Accept: application/json", OE2_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "STACKEXCHANGE_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);   /* fails if gzip not decompressed → honest empty */
  if (!root) return 0;
  const cJSON *items = cJSON_GetObjectItem(root, "items");
  int n = 0;
  if (cJSON_IsArray(items)) {
    const cJSON *q;
    cJSON_ArrayForEach(q, items) {
      const char *title = jo_sv(q, "title");
      const char *link = jo_sv(q, "link");
      const cJSON *score = cJSON_GetObjectItem(q, "score");
      const cJSON *answered = cJSON_GetObjectItem(q, "is_answered");
      if (!title) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "title", title);
      if (score && cJSON_IsNumber(score)) cJSON_AddNumberToObject(d, "score", score->valuedouble);
      if (cJSON_IsBool(answered)) cJSON_AddBoolToObject(d, "is_answered", cJSON_IsTrue(answered));
      cJSON_AddStringToObject(d, "source", "Stack Overflow");
      cJSON *p = cJSON_CreateObject();
      n += oe2_emit(sink, "STACKEXCHANGE_SEARCH", "stackexchange-question", link ? link : title, title, NULL, link, d, p, 0, 0, 0);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "WIKIDATA_ENTITY_SEARCH")) n = run_wikidata_ent(ctx, sink, enc);
  else if (!strcmp(sid, "SEC_FULLTEXT"))           n = run_sec_fulltext(ctx, sink, enc);
  else if (!strcmp(sid, "BIGDATACLOUD_REVERSE"))   n = run_bigdatacloud(ctx, sink, q);
  else if (!strcmp(sid, "HN_USER"))                n = run_hn_user(ctx, sink, enc, q);
  else if (!strcmp(sid, "OPENVERSE"))              n = run_openverse(ctx, sink, enc);
  else if (!strcmp(sid, "STACKEXCHANGE_SEARCH"))   n = run_stackexchange(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define OE2_DEF(sym, ID, NM, CAT, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = CAT, \
    .type = "api", .url = "internal://osint/extras2", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

OE2_DEF(oe2_wd_def, "WIKIDATA_ENTITY_SEARCH", "Wikidata Entity Search", "investigation",
  "Wikidata keyless entity resolution (QID, label, description).");
OE2_DEF(oe2_sec_def, "SEC_FULLTEXT", "SEC EDGAR Full-Text", "government",
  "SEC EDGAR keyless full-text filing search (filer, form, date).");
OE2_DEF(oe2_bdc_def, "BIGDATACLOUD_REVERSE", "BigDataCloud Reverse Geocode", "geospatial",
  "BigDataCloud keyless reverse geocode of lat,lon (city, subdivision, country).");
OE2_DEF(oe2_hn_def, "HN_USER", "Hacker News User", "investigation",
  "Hacker News keyless user profile (karma, about).");
OE2_DEF(oe2_ov_def, "OPENVERSE", "Openverse Media", "investigation",
  "Openverse keyless CC-media image search (title, creator, license).");
OE2_DEF(oe2_se_def, "STACKEXCHANGE_SEARCH", "Stack Exchange Search", "cyber",
  "Stack Overflow keyless question search (title, score, answered).");
