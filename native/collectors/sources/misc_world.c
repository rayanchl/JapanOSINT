/* collectors/sources/misc_world.c
 * OSINT services — net-new keyless enrichment/discovery, pivot on ctx->entity.
 * Real fetch or honest-empty. No API keys. (Lower-value / niche additions.)
 *
 *   DBLP_SEARCH     dblp.org/search/publ/api?q=&format=json    (CS bibliography)
 *   CODEBERG_SEARCH codeberg.org/api/v1/repos/search?q=        (Gitea repos)
 *   NATIONALIZE     api.nationalize.io/?name=                  (name→nationality)
 *   GENDERIZE       api.genderize.io/?name=                    (name→gender)
 *   REST_COUNTRIES  restcountries.com/v3.1/name/<name>         (country facts)
 *   FRANKFURTER_FX  api.frankfurter.app/latest?from=<CUR>      (FX rates) */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *MI_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int mi_emit(intel_sink *sink, const char *service, const char *rtype,
                   const char *rk, const char *title, const char *summary,
                   const char *link, cJSON *body, cJSON *props) {
  char *bj = body ? cJSON_PrintUnformatted(body) : NULL;
  if (props) { cJSON_AddStringToObject(props, "service", service); cJSON_AddBoolToObject(props, "success", 1); }
  char *pj = props ? cJSON_PrintUnformatted(props) : NULL;
  char tags[96]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.summary = summary;
  it.body = bj; it.link = (link && link[0]) ? link : NULL; it.lang = "en";
  it.record_type = rtype; it.properties_json = pj; it.tags_json = tags;
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  if (body) cJSON_Delete(body); if (props) cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* DBLP authors: info.authors.author is either an object {text} or an array of them. */
static char *dblp_authors(const cJSON *info) {
  const cJSON *au = cJSON_GetObjectItem(info, "authors");
  const cJSON *a = au ? cJSON_GetObjectItem(au, "author") : NULL;
  if (!a) return NULL;
  size_t cap = 256, len = 0; char *out = malloc(cap); if (!out) return NULL; out[0] = 0; int n = 0;
  if (cJSON_IsArray(a)) {
    const cJSON *e;
    cJSON_ArrayForEach(e, a) {
      const char *t = jo_sv(e, "text"); if (!t) continue;
      size_t need = len + strlen(t) + 3; if (need > cap) { cap = need * 2; char *x = realloc(out, cap); if (!x) break; out = x; }
      if (n) { strcpy(out + len, ", "); len += 2; } strcpy(out + len, t); len += strlen(t);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  } else {
    const char *t = jo_sv(a, "text"); if (t) { strncpy(out, t, cap - 1); out[cap-1]=0; n = 1; }
  }
  if (!n) { free(out); return NULL; }
  return out;
}
static int run_dblp(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://dblp.org/search/publ/api?q=%s&format=json&h=20", enc);
  const char *hdrs[] = { "Accept: application/json", MI_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "DBLP_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *result = cJSON_GetObjectItem(root, "result");
  const cJSON *hits = result ? cJSON_GetObjectItem(result, "hits") : NULL;
  const cJSON *hit = hits ? cJSON_GetObjectItem(hits, "hit") : NULL;
  int emitted = 0;
  if (cJSON_IsArray(hit)) {
    const cJSON *h;
    cJSON_ArrayForEach(h, hit) {
      const cJSON *info = cJSON_GetObjectItem(h, "info");
      if (!info) continue;
      const char *title = jo_sv(info, "title");
      const char *venue = jo_sv(info, "venue");
      const char *year = jo_sv(info, "year");
      const char *url2 = jo_sv(info, "url");
      const char *doi = jo_sv(info, "doi");
      if (!title) continue;
      char *authors = dblp_authors(info);
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "title", title);
      if (authors) cJSON_AddStringToObject(d, "authors", authors);
      if (venue) cJSON_AddStringToObject(d, "venue", venue);
      if (year) cJSON_AddStringToObject(d, "year", year);
      if (doi) cJSON_AddStringToObject(d, "doi", doi);
      cJSON_AddStringToObject(d, "source", "DBLP");
      cJSON *p = cJSON_CreateObject();
      if (doi) cJSON_AddStringToObject(p, "doi", doi);
      emitted += mi_emit(sink, "DBLP_SEARCH", "dblp-publication", doi ? doi : title, title, authors ? authors : venue, url2, d, p);
      free(authors);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_codeberg(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://codeberg.org/api/v1/repos/search?q=%s&limit=20", enc);
  const char *hdrs[] = { "Accept: application/json", MI_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "CODEBERG_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *data = cJSON_GetObjectItem(root, "data");
  int emitted = 0;
  if (cJSON_IsArray(data)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, data) {
      const char *full = jo_sv(r, "full_name");
      const char *desc = jo_sv(r, "description");
      const char *html = jo_sv(r, "html_url");
      const cJSON *stars = cJSON_GetObjectItem(r, "stars_count");
      if (!full) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "full_name", full);
      if (desc) cJSON_AddStringToObject(d, "description", desc);
      if (stars && cJSON_IsNumber(stars)) cJSON_AddNumberToObject(d, "stars", stars->valuedouble);
      cJSON_AddStringToObject(d, "source", "Codeberg");
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "repo", full);
      emitted += mi_emit(sink, "CODEBERG_SEARCH", "codeberg-repo", full, full, desc, html, d, p);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_nationalize(const source_ctx *ctx, intel_sink *sink, const char *enc, const char *raw) {
  char url[256];
  snprintf(url, sizeof url, "https://api.nationalize.io/?name=%s", enc);
  const char *hdrs[] = { "Accept: application/json", MI_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "NATIONALIZE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *country = cJSON_GetObjectItem(root, "country");
  if (!cJSON_IsArray(country) || cJSON_GetArraySize(country) == 0) { cJSON_Delete(root); return 0; }
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "name", raw);
  cJSON *preds = cJSON_CreateArray();
  const cJSON *c; int k = 0;
  cJSON_ArrayForEach(c, country) {
    const char *cid = jo_sv(c, "country_id");
    const cJSON *prob = cJSON_GetObjectItem(c, "probability");
    cJSON *e = cJSON_CreateObject();
    if (cid) cJSON_AddStringToObject(e, "country", cid);
    if (prob && cJSON_IsNumber(prob)) cJSON_AddNumberToObject(e, "probability", prob->valuedouble);
    cJSON_AddItemToArray(preds, e);
    if (++k >= 5) break;
  }
  cJSON_AddItemToObject(d, "predictions", preds);
  cJSON_AddStringToObject(d, "source", "Nationalize.io");
  const cJSON *top = cJSON_GetArrayItem(country, 0);  /* exhaustive-ok: headline pick; predictions carries all */
  const char *topc = top ? jo_sv(top, "country_id") : NULL;
  cJSON *p = cJSON_CreateObject();
  char title[160]; snprintf(title, sizeof title, "%s → likely %s", raw, topc ? topc : "?");
  int rc = mi_emit(sink, "NATIONALIZE", "name-nationality", raw, title, topc, NULL, d, p);
  cJSON_Delete(root);
  return rc;
}

static int run_genderize(const source_ctx *ctx, intel_sink *sink, const char *enc, const char *raw) {
  char url[256];
  snprintf(url, sizeof url, "https://api.genderize.io/?name=%s", enc);
  const char *hdrs[] = { "Accept: application/json", MI_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "GENDERIZE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *gender = jo_sv(root, "gender");
  if (!gender) { cJSON_Delete(root); return 0; }
  const cJSON *prob = cJSON_GetObjectItem(root, "probability");
  const cJSON *count = cJSON_GetObjectItem(root, "count");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "name", raw);
  cJSON_AddStringToObject(d, "gender", gender);
  if (prob && cJSON_IsNumber(prob)) cJSON_AddNumberToObject(d, "probability", prob->valuedouble);
  if (count && cJSON_IsNumber(count)) cJSON_AddNumberToObject(d, "count", count->valuedouble);
  cJSON_AddStringToObject(d, "source", "Genderize.io");
  cJSON *p = cJSON_CreateObject();
  char title[160]; snprintf(title, sizeof title, "%s → %s", raw, gender);
  int rc = mi_emit(sink, "GENDERIZE", "name-gender", raw, title, gender, NULL, d, p);
  cJSON_Delete(root);
  return rc;
}

static int run_frankfurter(const source_ctx *ctx, intel_sink *sink, const char *raw) {
  char cur[8] = {0}; int i = 0;
  for (const char *pp = raw; *pp && i < 3; pp++) if (isalpha((unsigned char)*pp)) cur[i++] = (char)toupper((unsigned char)*pp);
  if (i < 3) return 0;
  char url[256];
  snprintf(url, sizeof url, "https://api.frankfurter.app/latest?from=%s", cur);
  const char *hdrs[] = { "Accept: application/json", MI_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "FRANKFURTER_FX");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *rates = cJSON_GetObjectItem(root, "rates");
  const char *date = jo_sv(root, "date");
  if (!rates) { cJSON_Delete(root); return 0; }
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "base", cur);
  if (date) cJSON_AddStringToObject(d, "date", date);
  cJSON_AddItemToObject(d, "rates", cJSON_Duplicate(rates, 1));
  cJSON_AddStringToObject(d, "source", "Frankfurter (ECB)");
  cJSON *p = cJSON_CreateObject();
  char title[96]; snprintf(title, sizeof title, "FX rates for %s (%s)", cur, date ? date : "");
  int rc = mi_emit(sink, "FRANKFURTER_FX", "fx-rates", cur, title, date, NULL, d, p);
  cJSON_Delete(root);
  return rc;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "DBLP_SEARCH"))     n = run_dblp(ctx, sink, enc);
  else if (!strcmp(sid, "CODEBERG_SEARCH")) n = run_codeberg(ctx, sink, enc);
  else if (!strcmp(sid, "NATIONALIZE"))     n = run_nationalize(ctx, sink, enc, q);
  else if (!strcmp(sid, "GENDERIZE"))       n = run_genderize(ctx, sink, enc, q);
  else if (!strcmp(sid, "FRANKFURTER_FX"))  n = run_frankfurter(ctx, sink, q);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define MI_DEF(sym, ID, NM, CAT, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = CAT, \
    .type = "api", .url = "internal://osint/misc", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

MI_DEF(mi_dblp_def, "DBLP_SEARCH", "DBLP CS Bibliography", "investigation",
  "DBLP keyless computer-science publication search (title, authors, venue, year).");
MI_DEF(mi_codeberg_def, "CODEBERG_SEARCH", "Codeberg Repo Search", "cyber",
  "Codeberg (Gitea) keyless repository search (description, stars).");
MI_DEF(mi_nationalize_def, "NATIONALIZE", "Name Nationality", "investigation",
  "Nationalize.io keyless: predicted nationality distribution for a first name.");
MI_DEF(mi_genderize_def, "GENDERIZE", "Name Gender", "investigation",
  "Genderize.io keyless: predicted gender for a first name (probability).");
MI_DEF(mi_frankfurter_def, "FRANKFURTER_FX", "Frankfurter FX Rates", "government",
  "Frankfurter (ECB) keyless: latest FX rates for a base currency code.");
