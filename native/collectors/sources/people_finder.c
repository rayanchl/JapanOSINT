/* collectors/osint/sources/people_finder.c — person & company OSINT, fused.
 *
 * Brings the OSINTsaas people-finder source set onto the JapanOSINT ABI:
 *
 *   PERSON_SEARCH  — one person-name pivot that fans across the real parsed
 *                    APIs OSINTsaas hits (GitHub, Wikipedia, DuckDuckGo,
 *                    Gravatar, HackerNews, Stack Overflow, Reddit,
 *                    CourtListener, OpenSanctions) PLUS the full directory of
 *                    ~119 country/people-search lookup sources (US, UK, EU,
 *                    IN, AU, CA, and the Japan set: mixi, 5ch, hatena, note,
 *                    CiNii, KAKEN, researchmap, NHK/Asahi/Yomiuri, suumo,…),
 *                    each surfaced as a lookup-link card.
 *
 *   COMPANY_SEARCH — all-business/company pivot (replaces the old UK-only
 *                    UK_COMPANIES_OFFICERS): UK Companies House officers +
 *                    OpenCorporates officers + the company/registry directory
 *                    cards (houjin-bangou, EDINET, J-PlatPat, NIKKEI,
 *                    Baseconnect, ZaubaCorp, MCA India, Canada registries…).
 *
 * Also registers the standalone LIVE person/company database sources (moved
 * here from people_research.c so this file is the person+company superset):
 *   KAKEN / CINII   — LIVE NII OpenSearch feeds (researcher→grants/papers).
 *   RESEARCHMAP     — researcher profiles (anchor scrape).
 *   MANSION_COMMUNITY — building resident forums (anchor scrape; who-lives-where).
 *   TDB_TSR         — paid credit-rated company DB (gated; TDB_TSR_API_KEY).
 *
 * No-fabrication rule: real parsed APIs emit real records or honest-empty. The
 * old ~119-source directory of constructed "lookup-link" cards (a search URL
 * per source, never actually fetched, marked success:true) was REMOVED — it
 * emitted records for endpoints it never queried. Key-gated APIs (Companies
 * House, OpenSanctions) emit nothing when their key is unset. */
#include "source.h"
#include "core/httpclient.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "_jp_osint.inc"

/* ── small helpers ──────────────────────────────────────────────────────── */
static char *ps_b64(const char *in) {
  static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t n = strlen(in), o = 0;
  char *out = malloc(((n + 2) / 3) * 4 + 1);
  if (!out) return NULL;
  for (size_t i = 0; i < n; i += 3) {
    unsigned v = (unsigned char)in[i] << 16;
    if (i + 1 < n) v |= (unsigned char)in[i + 1] << 8;
    if (i + 2 < n) v |= (unsigned char)in[i + 2];
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    out[o++] = (i + 1 < n) ? T[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < n) ? T[v & 63] : '=';
  }
  out[o] = 0;
  return out;
}

static cJSON *ps_get_json(http_client *http, const char *url, const char **hdrs) {
  http_response hr = {0};
  if (http_request(http, "GET", url, hdrs, NULL, 0, 15000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body);
  http_response_free(&hr);
  return j;
}

/* Emit one parsed-data row from a real API. sub_source_id = provider so the
 * run-detail "Sources" card lists it. Frees `data`. */
static int ps_emit_data(intel_sink *sink, const char *service, const char *source,
                        const char *entity, const char *remote_key,
                        const char *title, const char *summary, cJSON *data) {
  char *bj = cJSON_PrintUnformatted(data);
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", service);
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddStringToObject(props, "source", source);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  char tags[96]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
  intel_item it = {0};
  it.remote_key = remote_key; it.title = title; it.summary = summary; it.body = bj;
  it.record_type = "osint_service_result"; it.sub_source_id = source;
  it.properties_json = pj; it.tags_json = tags;
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); cJSON_Delete(props); cJSON_Delete(data);
  return rc >= 0 ? 1 : 0;
}

/* NOTE: the OSINTsaas directory of ~119 people/company "lookup-link" cards was
 * removed — it emitted constructed search URLs (per-source) marked success:true
 * without ever fetching them (no-fabrication rule). Only the genuinely-fetched
 * parsed-API providers below emit records. The 118-row table that fed those
 * cards, _person_links.inc, had been left in the tree unreferenced by anything
 * for the same reason; it is now deleted, so nobody re-wires it by accident. */

/* ── real parsed-API providers (ported from OSINTsaas) ────────────────────── */

/* GitHub and Gravatar are NOT reimplemented here — PERSON_SEARCH reuses the
 * single richer implementation in people_world.c (pw_run_github /
 * pw_run_gravatar, extern-declared below), which emits under the same
 * GITHUB_USER|<login> / GRAVATAR_GLOBAL|<hash> remote_keys as the standalone
 * GITHUB_USER / GRAVATAR_GLOBAL sources so the shared sink dedups by uid. */

static int ps_wikipedia(const source_ctx *ctx, intel_sink *sink, const char *name, const char *svc) {
  char enc[256]; jo_urlencode_buf(name, enc, sizeof enc);
  char url[400];
  snprintf(url, sizeof url, "https://en.wikipedia.org/api/rest_v1/page/summary/%s", enc);
  cJSON *j = ps_get_json(ctx->http, url, NULL);
  if (!j) return 0;
  cJSON *t = cJSON_GetObjectItem(j, "title");
  cJSON *ex = cJSON_GetObjectItem(j, "extract");
  int emitted = 0;
  if (t && cJSON_IsString(t) && ex && cJSON_IsString(ex) && *ex->valuestring) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "title", t->valuestring);
    cJSON_AddStringToObject(data, "summary", ex->valuestring);
    char rk[320], title[340];
    snprintf(rk, sizeof rk, "person:%s:wiki:%s", name, t->valuestring);
    snprintf(title, sizeof title, "%s — Wikipedia", t->valuestring);
    emitted += ps_emit_data(sink, svc, "Wikipedia", name, rk, title, ex->valuestring, data);
  }
  cJSON_Delete(j);
  return emitted;
}

static int ps_duckduckgo(const source_ctx *ctx, intel_sink *sink, const char *name, const char *svc) {
  char enc[256]; jo_urlencode_buf(name, enc, sizeof enc);
  char url[400];
  snprintf(url, sizeof url, "https://api.duckduckgo.com/?q=%s&format=json&no_html=1", enc);
  cJSON *j = ps_get_json(ctx->http, url, NULL);
  if (!j) return 0;
  cJSON *ab = cJSON_GetObjectItem(j, "Abstract");
  cJSON *hd = cJSON_GetObjectItem(j, "Heading");
  cJSON *au = cJSON_GetObjectItem(j, "AbstractURL");
  int emitted = 0;
  if (ab && cJSON_IsString(ab) && *ab->valuestring) {
    cJSON *data = cJSON_CreateObject();
    if (hd && cJSON_IsString(hd)) cJSON_AddStringToObject(data, "heading", hd->valuestring);
    cJSON_AddStringToObject(data, "abstract", ab->valuestring);
    if (au && cJSON_IsString(au)) cJSON_AddStringToObject(data, "source_url", au->valuestring);
    char rk[320], title[340];
    snprintf(rk, sizeof rk, "person:%s:ddg", name);
    snprintf(title, sizeof title, "%s — DuckDuckGo", (hd && cJSON_IsString(hd)) ? hd->valuestring : name);
    emitted += ps_emit_data(sink, svc, "DuckDuckGo", name, rk, title, ab->valuestring, data);
  }
  cJSON_Delete(j);
  return emitted;
}

static int ps_hackernews(const source_ctx *ctx, intel_sink *sink, const char *name, const char *svc) {
  if (strchr(name, ' ')) return 0;                  /* username-only */
  char enc[128]; jo_urlencode_buf(name, enc, sizeof enc);
  char url[256];
  snprintf(url, sizeof url, "https://hacker-news.firebaseio.com/v0/user/%s.json", enc);
  cJSON *j = ps_get_json(ctx->http, url, NULL);
  if (!j) return 0;
  cJSON *id = cJSON_GetObjectItem(j, "id");
  int emitted = 0;
  if (id && cJSON_IsString(id)) {
    cJSON *karma = cJSON_GetObjectItem(j, "karma");
    cJSON *about = cJSON_GetObjectItem(j, "about");
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "username", id->valuestring);
    if (karma && cJSON_IsNumber(karma)) cJSON_AddNumberToObject(data, "karma", karma->valuedouble);
    if (about && cJSON_IsString(about)) cJSON_AddStringToObject(data, "about", about->valuestring);
    char rk[256], title[280], purl[200];
    snprintf(purl, sizeof purl, "https://news.ycombinator.com/user?id=%s", id->valuestring);
    cJSON_AddStringToObject(data, "profile_url", purl);
    snprintf(rk, sizeof rk, "person:%s:hn:%s", name, id->valuestring);
    snprintf(title, sizeof title, "%s — HackerNews user", id->valuestring);
    emitted += ps_emit_data(sink, svc, "HackerNews", name, rk, title, id->valuestring, data);
  }
  cJSON_Delete(j);
  return emitted;
}

static int ps_stackoverflow(const source_ctx *ctx, intel_sink *sink, const char *name, const char *svc) {
  char enc[256]; jo_urlencode_buf(name, enc, sizeof enc);
  char url[400];
  snprintf(url, sizeof url,
    "https://api.stackexchange.com/2.3/users?order=desc&sort=reputation&inname=%s&site=stackoverflow&pagesize=5", enc);
  cJSON *j = ps_get_json(ctx->http, url, NULL);
  if (!j) return 0;
  cJSON *items = cJSON_GetObjectItem(j, "items");
  int emitted = 0, n = (items && cJSON_IsArray(items)) ? cJSON_GetArraySize(items) : 0;
  for (int i = 0; i < n && i < 5; i++) {
    cJSON *it = cJSON_GetArrayItem(items, i);
    cJSON *dn = cJSON_GetObjectItem(it, "display_name");
    if (!dn || !cJSON_IsString(dn)) continue;
    cJSON *link = cJSON_GetObjectItem(it, "link");
    cJSON *rep = cJSON_GetObjectItem(it, "reputation");
    cJSON *loc = cJSON_GetObjectItem(it, "location");
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "display_name", dn->valuestring);
    if (link && cJSON_IsString(link)) cJSON_AddStringToObject(data, "profile_url", link->valuestring);
    if (rep && cJSON_IsNumber(rep)) cJSON_AddNumberToObject(data, "reputation", rep->valuedouble);
    if (loc && cJSON_IsString(loc)) cJSON_AddStringToObject(data, "location", loc->valuestring);
    char rk[320], title[340];
    snprintf(rk, sizeof rk, "person:%s:so:%s", name, dn->valuestring);
    snprintf(title, sizeof title, "%s — Stack Overflow", dn->valuestring);
    emitted += ps_emit_data(sink, svc, "Stack Overflow", name, rk, title, dn->valuestring, data);
  }
  cJSON_Delete(j);
  return emitted;
}

static int ps_reddit(const source_ctx *ctx, intel_sink *sink, const char *name, const char *svc) {
  if (strchr(name, ' ')) return 0;                  /* username-only */
  char enc[128]; jo_urlencode_buf(name, enc, sizeof enc);
  char url[256];
  snprintf(url, sizeof url, "https://www.reddit.com/user/%s/about.json", enc);
  cJSON *j = ps_get_json(ctx->http, url, NULL);
  if (!j) return 0;
  cJSON *d = cJSON_GetObjectItem(j, "data");
  int emitted = 0;
  if (d && cJSON_IsObject(d)) {
    cJSON *nm = cJSON_GetObjectItem(d, "name");
    if (nm && cJSON_IsString(nm)) {
      cJSON *tk = cJSON_GetObjectItem(d, "total_karma");
      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "username", nm->valuestring);
      if (tk && cJSON_IsNumber(tk)) cJSON_AddNumberToObject(data, "total_karma", tk->valuedouble);
      char purl[200]; snprintf(purl, sizeof purl, "https://www.reddit.com/user/%s", nm->valuestring);
      cJSON_AddStringToObject(data, "profile_url", purl);
      char rk[256], title[280];
      snprintf(rk, sizeof rk, "person:%s:reddit:%s", name, nm->valuestring);
      snprintf(title, sizeof title, "%s — Reddit user", nm->valuestring);
      emitted += ps_emit_data(sink, svc, "Reddit", name, rk, title, nm->valuestring, data);
    }
  }
  cJSON_Delete(j);
  return emitted;
}

/* CourtListener is now served by the standalone COURT_RECORDS collector
 * (court_records.c, jo_court_records_run) and OpenSanctions by PEP_CHECK /
 * WATCHLIST_CHECK_NEW (pep_watchlist.c) — both folded into person_run below,
 * so the earlier inline approximations were removed. */

/* UK Companies House officer search (key-gated). */
static int ps_uk_companies(const source_ctx *ctx, intel_sink *sink, const char *name, const char *svc) {
  const char *key = getenv("COMPANIES_HOUSE_API_KEY");
  if (!key || !*key) return 0;
  char enc[256]; jo_urlencode_buf(name, enc, sizeof enc);
  char url[500];
  snprintf(url, sizeof url,
    "https://api.company-information.service.gov.uk/search/officers?q=%s&items_per_page=5", enc);
  char upw[256]; snprintf(upw, sizeof upw, "%s:", key);
  char *b = ps_b64(upw);
  char auth[400]; snprintf(auth, sizeof auth, "Authorization: Basic %s", b ? b : "");
  free(b);
  const char *hdr[] = { auth, NULL };
  cJSON *j = ps_get_json(ctx->http, url, hdr);
  if (!j) return 0;
  cJSON *items = cJSON_GetObjectItem(j, "items");
  int emitted = 0, n = (items && cJSON_IsArray(items)) ? cJSON_GetArraySize(items) : 0;
  for (int i = 0; i < n && i < 5; i++) {
    cJSON *item = cJSON_GetArrayItem(items, i);
    cJSON *t = cJSON_GetObjectItem(item, "title");
    if (!t || !cJSON_IsString(t)) continue;
    cJSON *a = cJSON_GetObjectItem(item, "address_snippet");
    cJSON *ap = cJSON_GetObjectItem(item, "appointed_on");
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "name", t->valuestring);
    if (a && cJSON_IsString(a)) cJSON_AddStringToObject(data, "address", a->valuestring);
    if (ap && cJSON_IsString(ap)) cJSON_AddStringToObject(data, "appointed_on", ap->valuestring);
    char rk[340], title[360];
    snprintf(rk, sizeof rk, "company:%s:uk:%s", name, t->valuestring);
    snprintf(title, sizeof title, "%s — UK company officer", t->valuestring);
    emitted += ps_emit_data(sink, svc, "UK Companies House", name, rk, title, t->valuestring, data);
  }
  cJSON_Delete(j);
  return emitted;
}

/* OpenCorporates officer search (keyless public tier; honest-empty on limit). */
static int ps_opencorporates(const source_ctx *ctx, intel_sink *sink, const char *name, const char *svc) {
  char enc[256]; jo_urlencode_buf(name, enc, sizeof enc);
  char url[400];
  snprintf(url, sizeof url,
    "https://api.opencorporates.com/v0.4/officers/search?q=%s&per_page=5", enc);
  const char *tok = getenv("OPENCORPORATES_API_KEY");
  char tokurl[480];
  if (tok && *tok) { snprintf(tokurl, sizeof tokurl, "%s&api_token=%s", url, tok); }
  cJSON *j = ps_get_json(ctx->http, (tok && *tok) ? tokurl : url, NULL);
  if (!j) return 0;
  cJSON *ro = cJSON_GetObjectItem(j, "results");
  cJSON *offs = ro ? cJSON_GetObjectItem(ro, "officers") : NULL;
  int emitted = 0, n = (offs && cJSON_IsArray(offs)) ? cJSON_GetArraySize(offs) : 0;
  for (int i = 0; i < n && i < 5; i++) {
    cJSON *wrap = cJSON_GetArrayItem(offs, i);
    cJSON *o = cJSON_GetObjectItem(wrap, "officer");
    if (!o) continue;
    cJSON *nm = cJSON_GetObjectItem(o, "name");
    if (!nm || !cJSON_IsString(nm)) continue;
    cJSON *pos = cJSON_GetObjectItem(o, "position");
    cJSON *ou = cJSON_GetObjectItem(o, "opencorporates_url");
    cJSON *co = cJSON_GetObjectItem(o, "company");
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "name", nm->valuestring);
    if (pos && cJSON_IsString(pos)) cJSON_AddStringToObject(data, "position", pos->valuestring);
    if (ou && cJSON_IsString(ou)) cJSON_AddStringToObject(data, "profile_url", ou->valuestring);
    if (co) { cJSON *cn = cJSON_GetObjectItem(co, "name");
      if (cn && cJSON_IsString(cn)) cJSON_AddStringToObject(data, "company", cn->valuestring); }
    char rk[360], title[380];
    snprintf(rk, sizeof rk, "company:%s:oc:%s", name, nm->valuestring);
    snprintf(title, sizeof title, "%s — company officer", nm->valuestring);
    emitted += ps_emit_data(sink, svc, "OpenCorporates", name, rk, title, nm->valuestring, data);
  }
  cJSON_Delete(j);
  return emitted;
}

/* Name-pivot services folded into PERSON_SEARCH. They stay registered
 * standalone — these are just their run entry points reused. KAKEN / CINII /
 * RESEARCHMAP / MANSION_COMMUNITY are local statics (defined below);
 * PEP_CHECK / WATCHLIST_CHECK_NEW live in pep_watchlist.c and COURT_RECORDS in
 * court_records.c (inline extern prototypes). */
static int run_kaken(const source_ctx *ctx, intel_sink *sink);
static int run_cinii(const source_ctx *ctx, intel_sink *sink);
static int run_researchmap(const source_ctx *ctx, intel_sink *sink);
static int run_mansion(const source_ctx *ctx, intel_sink *sink);
int jo_pep_run(const source_ctx *ctx, intel_sink *sink);
int jo_watchlist_run(const source_ctx *ctx, intel_sink *sink);
int jo_court_records_run(const source_ctx *ctx, intel_sink *sink);
/* Shared GitHub/Gravatar implementations (people_world.c). Reused here so the
 * pivot converges on the GITHUB_USER|<login> / GRAVATAR_GLOBAL|<hash> keys. */
int pw_run_github(const source_ctx *ctx, intel_sink *sink, const char *q);
int pw_run_gravatar(const source_ctx *ctx, intel_sink *sink, const char *q);

/* ── PERSON_SEARCH: fused people finder ───────────────────────────────────── */
static int person_run(const source_ctx *ctx, intel_sink *sink) {
  const char *name = ctx->entity;
  if (!name || !*name) return 0;
  int t = 0;
  /* real parsed APIs — GitHub/Gravatar reuse the richer people_world.c code and
   * emit under the GITHUB_USER|<login> / GRAVATAR_GLOBAL|<hash> keys so the sink
   * dedups them against the standalone GITHUB_USER / GRAVATAR_GLOBAL sources. */
  t += pw_run_github(ctx, sink, name);
  t += ps_wikipedia(ctx, sink, name, "PERSON_SEARCH");
  t += ps_duckduckgo(ctx, sink, name, "PERSON_SEARCH");
  t += pw_run_gravatar(ctx, sink, name);
  t += ps_hackernews(ctx, sink, name, "PERSON_SEARCH");
  t += ps_stackoverflow(ctx, sink, name, "PERSON_SEARCH");
  t += ps_reddit(ctx, sink, name, "PERSON_SEARCH");
  /* company / officer APIs (shared with COMPANY_SEARCH) */
  t += ps_uk_companies(ctx, sink, name, "PERSON_SEARCH");
  t += ps_opencorporates(ctx, sink, name, "PERSON_SEARCH");
  /* sanctions / PEP (PEP_CHECK, WATCHLIST_CHECK_NEW) + US court records */
  t += jo_pep_run(ctx, sink);
  t += jo_watchlist_run(ctx, sink);
  t += jo_court_records_run(ctx, sink);
  /* JP researcher + residential pivots (KAKEN/CINII/RESEARCHMAP/MANSION) */
  t += run_kaken(ctx, sink);
  t += run_cinii(ctx, sink);
  t += run_researchmap(ctx, sink);
  t += run_mansion(ctx, sink);
  /* No fabrication: directory lookup-link cards (constructed search URLs, never
   * fetched) were removed — only genuinely-fetched providers above emit. */
  /* run() must return 0 on success: core/scheduler.c does
   * `status = rc == 0 ? "ok" : "error"` and feeds it to anomaly_detect(), so
   * returning the row count marked every good run (50 rows) as a failure. */
  return t >= 0 ? 0 : -1;
}

/* ── COMPANY_SEARCH: all-business/company pivot ───────────────────────────── */
static int company_run(const source_ctx *ctx, intel_sink *sink) {
  const char *name = ctx->entity;
  if (!name || !*name) return 0;
  int t = 0;
  t += ps_uk_companies(ctx, sink, name, "COMPANY_SEARCH");
  t += ps_opencorporates(ctx, sink, name, "COMPANY_SEARCH");
  /* No fabrication: business-registry lookup-link cards removed (constructed
   * URLs were never fetched); only real officer-API results above emit. */
  return t >= 0 ? 0 : -1;          /* see person_run(): 0 == ok, <0 == error */
}

static const source_def person_search_def = {
  .id = "PERSON_SEARCH", .collector = "osint",
  .name = "Person Search", .run = person_run,
  .category = "investigation", .type = "api",
  .url = "internal://osint/person-search",
  .description = "Person name → fused people search: GitHub, Wikipedia, DuckDuckGo, Gravatar, HackerNews, Stack Overflow, Reddit, UK Companies House + OpenCorporates officers, OpenSanctions PEP/sanctions, US court records, JP researchers (KAKEN/CiNii/researchmap) + resident forums.",
  .free_tier = 1,
};
REGISTER_SOURCE(person_search_def)

static const source_def company_search_def = {
  .id = "COMPANY_SEARCH", .collector = "osint",
  .name = "Company / Business Search", .run = company_run,
  .category = "investigation", .type = "api",
  .url = "internal://osint/company-search",
  .description = "Company / officer name → UK Companies House + OpenCorporates officer records (real parsed APIs).",
  .free_tier = 1,
};
REGISTER_SOURCE(company_search_def)

/* ── standalone LIVE person/company DB sources (moved from people_research.c) ──
 * KAKEN/CINII/RESEARCHMAP are researcher (person) pivots; TDB_TSR is a company
 * pivot. Place/residential sources (MANSION_COMMUNITY, GOOGLE_PLACES) stay in
 * people_research.c. Real fetch or honest empty. */

/* Generic RSS/Atom feed → intel_items (title + link + date per entry). */
static int feed_emit(const source_ctx *ctx, intel_sink *sink, const char *url,
                     const char *service, const char *rectype, const char *tag) {
  char *body = jo_get(ctx, url, NULL, tag);
  if (!body) return 0;
  int emitted = 0;
  char *p = body, *e;
  while (emitted < 30) {
    char *it_s = strstr(p, "<item");
    char *en_s = strstr(p, "<entry");
    e = it_s; const char *closer = "</item>";
    if (!e || (en_s && en_s < e)) { e = en_s; closer = "</entry>"; }
    if (!e) break;
    char *end = strstr(e, closer);
    if (!end) break;
    char saved = *end; *end = 0;
    char *cur = e; char *title = jo_tag_inner((const char **)&cur, "title");
    cur = e; char *date = jo_tag_inner((const char **)&cur, "updated");
    if (!date) { cur = e; date = jo_tag_inner((const char **)&cur, "dc:date"); }
    /* link: inner text (RSS) or href attribute (Atom) */
    cur = e; char *link = jo_tag_inner((const char **)&cur, "link");
    char href[800] = {0};
    if ((!link || !*link)) {
      char *lh = strstr(e, "href=\"");
      if (lh) { lh += 6; char *le = strchr(lh, '"'); if (le && le - lh < 790) snprintf(href, sizeof href, "%.*s", (int)(le - lh), lh); }
    }
    *end = saved; p = end + strlen(closer);
    if (title) {
      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", service);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props); cJSON_Delete(props);
      intel_item it = {0};
      it.remote_key = (link && *link) ? link : (href[0] ? href : title);
      it.title = title; it.link = (link && *link) ? link : (href[0] ? href : NULL);
      it.published_at = date; it.lang = "ja"; it.record_type = rectype;
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"research\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(pj);
    }
    free(title); free(date); free(link);
  }
  free(body);
  fprintf(stderr, "[%s] emitted %d\n", tag, emitted);
  return 0;
}

static int run_kaken(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *q = jo_urlencode(ctx->entity); if (!q) return 0;
  char url[640];
  snprintf(url, sizeof url, "https://kaken.nii.ac.jp/opensearch/?qm=%s", q);
  free(q);
  return feed_emit(ctx, sink, url, "KAKEN", "research-grant", "kaken");
}
static int run_cinii(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *q = jo_urlencode(ctx->entity); if (!q) return 0;
  char url[640];
  snprintf(url, sizeof url, "https://cir.nii.ac.jp/opensearch/all?q=%s&format=rss", q);
  free(q);
  return feed_emit(ctx, sink, url, "CiNii Research", "research-paper", "cinii");
}

static int run_researchmap(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *q = jo_urlencode(ctx->entity); if (!q) return 0;
  char url[640];
  snprintf(url, sizeof url, "https://researchmap.jp/researchers?q=%s", q);
  free(q);
  jo_emit_anchors(ctx, sink, url, "/researchers/", "researchmap", "researcher-profile",
                  "https://researchmap.jp", NULL, 25, "researchmap");
  return 0;
}

static int run_mansion(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *q = jo_urlencode(ctx->entity); if (!q) return 0;
  char url[640];
  snprintf(url, sizeof url, "https://www.e-mansion.co.jp/bbs/search/?word=%s", q);
  free(q);
  jo_emit_anchors(ctx, sink, url, "/bbs/thread/", "Mansion Community", "resident-forum",
                  "https://www.e-mansion.co.jp", NULL, 25, "mansion");
  return 0;
}

static int run_tdb_tsr(const source_ctx *ctx, intel_sink *sink) {
  (void)sink; (void)ctx;
  if (!jo_env("TDB_TSR_API_KEY")) { fprintf(stderr, "[tdb-tsr] gated (no TDB_TSR_API_KEY, paid)\n"); return 0; }
  fprintf(stderr, "[tdb-tsr] key present but endpoint not provisioned\n");
  return 0;
}

#include "_source_macros.inc"

DEFR(kaken_def, "KAKEN", "KAKEN Grants", "KAKEN 科研費", run_kaken,
     "government", "api", "https://kaken.nii.ac.jp/", "Research grants: who got which 科研費 (LIVE OpenSearch)", 1);
DEFR(cinii_def, "CINII", "CiNii Research", "CiNii Research", run_cinii,
     "government", "api", "https://cir.nii.ac.jp/", "Papers→authors→institutions graph (LIVE OpenSearch)", 1);
DEFR(researchmap_def, "RESEARCHMAP", "researchmap", "researchmap 研究者", run_researchmap,
     "government", "scraped", "https://researchmap.jp/", "Researcher profiles, affiliations & collaborators (entity pivot)", 1);
DEFR(mansion_def, "MANSION_COMMUNITY", "Mansion Community", "マンションコミュニティ", run_mansion,
     "social", "scraped", "https://www.e-mansion.co.jp/", "Building-level resident forums (occupancy/incident chatter)", 1);
DEFR(tdbtsr_def, "TDB_TSR", "TDB / TSR Company DB", "帝国データバンク / 東京商工リサーチ", run_tdb_tsr,
     "commercial", "api", "https://www.tdb.co.jp/", "Full credit-rated company database (paid; needs TDB_TSR_API_KEY)", 0);
