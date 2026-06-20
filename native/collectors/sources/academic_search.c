/* collectors/osint/sources/academic_search.c
 * OSINT service — ACADEMIC_SEARCH (osint_dispatcher.c:
 * {SERVICE_ACADEMIC_SEARCH, handle_academic_search, "ACADEMIC_SEARCH", true};
 * aliases PUBMED_SEARCH / ARXIV_SEARCH / SCHOLAR_SEARCH share the handler).
 * Entity = keyword/name. No API key. Sources: PubMed (esearch+esummary),
 * arXiv (Atom XML), CrossRef, Semantic Scholar, ORCID, CiNii (live).
 *
 * PER-RECORD EMIT: emits ONE osint_service_result row per PAPER (keyed by DOI
 * when present, else "paper:<source>:<id-or-title>"), not a single summary
 * blob. Each row carries the paper title, a per-paper body, an authors+year
 * summary, the publication date, and the paper/DOI link. If no papers are
 * found across all sources, emits nothing (honest empty — no seeded rows). The
 * former J-STAGE / KAKEN / researchmap lookup-URL stubs (which emitted only a
 * label + URL with no fetched data) have been removed. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void uri_encode(const char *in, char *out, size_t cap) {
  static const char *keep = "-_.!~*'()";
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && w + 4 < cap; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(keep, c)) out[w++] = (char)c;
    else { snprintf(out + w, cap - w, "%%%02X", c); w += 3; }
  }
  out[w] = 0;
}

static cJSON *http_json(http_client *h, const char *url) {
  http_response hr = {0};
  int hc = http_request(h, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  return j;
}

static cJSON *search_pubmed(http_client *h, const char *q, int limit) {
  cJSON *results = cJSON_CreateArray();
  char enc[1024]; uri_encode(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url,
    "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/esearch.fcgi?db=pubmed&retmode=json&retmax=%d&term=%s",
    limit, enc);
  cJSON *sj = http_json(h, url);
  if (!sj) return results;
  cJSON *esr = cJSON_GetObjectItem(sj, "esearchresult");
  cJSON *idl = esr ? cJSON_GetObjectItem(esr, "idlist") : NULL;
  if (!idl || !cJSON_IsArray(idl) || cJSON_GetArraySize(idl) == 0) {
    cJSON_Delete(sj); return results;
  }
  char ids[2048] = "";
  int n = cJSON_GetArraySize(idl);
  for (int i = 0; i < n && i < limit; i++) {
    cJSON *id = cJSON_GetArrayItem(idl, i);
    if (id && id->valuestring) {
      if (*ids) strncat(ids, ",", sizeof ids - strlen(ids) - 1);
      strncat(ids, id->valuestring, sizeof ids - strlen(ids) - 1);
    }
  }
  cJSON_Delete(sj);
  snprintf(url, sizeof url,
    "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/esummary.fcgi?db=pubmed&retmode=json&id=%s",
    ids);
  cJSON *fj = http_json(h, url);
  if (fj) {
    cJSON *ro = cJSON_GetObjectItem(fj, "result");
    cJSON *uids = ro ? cJSON_GetObjectItem(ro, "uids") : NULL;
    if (uids && cJSON_IsArray(uids)) {
      int m = cJSON_GetArraySize(uids);
      for (int i = 0; i < m; i++) {
        cJSON *u = cJSON_GetArrayItem(uids, i);
        if (!(u && u->valuestring)) continue;
        cJSON *p = cJSON_GetObjectItem(ro, u->valuestring);
        if (!p) continue;
        cJSON *it = cJSON_CreateObject();
        cJSON_AddStringToObject(it, "source", "pubmed");
        cJSON_AddStringToObject(it, "pmid", u->valuestring);
        cJSON *t = cJSON_GetObjectItem(p, "title");
        cJSON *pd = cJSON_GetObjectItem(p, "pubdate");
        cJSON *sr = cJSON_GetObjectItem(p, "source");
        if (t && t->valuestring) cJSON_AddStringToObject(it, "title", t->valuestring);
        if (pd && pd->valuestring) cJSON_AddStringToObject(it, "date", pd->valuestring);
        if (sr && sr->valuestring) cJSON_AddStringToObject(it, "journal", sr->valuestring);
        char pu[256];
        snprintf(pu, sizeof pu, "https://pubmed.ncbi.nlm.nih.gov/%s/", u->valuestring);
        cJSON_AddStringToObject(it, "url", pu);
        cJSON *au = cJSON_GetObjectItem(p, "authors");
        if (au && cJSON_IsArray(au)) {
          cJSON *an = cJSON_CreateArray();
          int ac = cJSON_GetArraySize(au);
          for (int k = 0; k < ac && k < 5; k++) {
            cJSON *a = cJSON_GetArrayItem(au, k);
            cJSON *nm = cJSON_GetObjectItem(a, "name");
            if (nm && nm->valuestring)
              cJSON_AddItemToArray(an, cJSON_CreateString(nm->valuestring));
          }
          cJSON_AddItemToObject(it, "authors", an);
        }
        cJSON_AddItemToArray(results, it);
      }
    }
    cJSON_Delete(fj);
  }
  return results;
}

/* arXiv Atom XML — same substring extraction as upstream. */
static cJSON *search_arxiv(http_client *h, const char *q, int limit) {
  cJSON *results = cJSON_CreateArray();
  char enc[1024]; uri_encode(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url,
    "http://export.arxiv.org/api/query?search_query=%s&start=0&max_results=%d",
    enc, limit);
  http_response hr = {0};
  int hc = http_request(h, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return results; }
  char *body = hr.body, *entry = body;
  while ((entry = strstr(entry, "<entry>")) != NULL) {
    char *ee = strstr(entry, "</entry>");
    if (!ee) break;
    cJSON *it = cJSON_CreateObject();
    cJSON_AddStringToObject(it, "source", "arxiv");
    char *ts = strstr(entry, "<title>"), *te = strstr(entry, "</title>");
    if (ts && te && ts < ee) {
      ts += 7; size_t L = te - ts;
      char *t = malloc(L + 1); memcpy(t, ts, L); t[L] = 0;
      cJSON_AddStringToObject(it, "title", t); free(t);
    }
    char *is = strstr(entry, "<id>"), *ie = strstr(entry, "</id>");
    if (is && ie && is < ee) {
      is += 4; size_t L = ie - is;
      char *id = malloc(L + 1); memcpy(id, is, L); id[L] = 0;
      cJSON_AddStringToObject(it, "url", id);
      char *ax = strstr(id, "abs/");
      if (ax) cJSON_AddStringToObject(it, "arxiv_id", ax + 4);
      free(id);
    }
    char *ps = strstr(entry, "<published>"), *pe = strstr(entry, "</published>");
    if (ps && pe && ps < ee) {
      ps += 11; size_t L = pe - ps;
      char d[32]; size_t c = L > 10 ? 10 : L;
      memcpy(d, ps, c); d[10] = 0;
      cJSON_AddStringToObject(it, "date", d);
    }
    char *ss = strstr(entry, "<summary>"), *se = strstr(entry, "</summary>");
    if (ss && se && ss < ee) {
      ss += 9; size_t L = se - ss; if (L > 500) L = 500;
      char *s = malloc(L + 4); memcpy(s, ss, L); s[L] = 0;
      if (L == 500) strcat(s, "...");
      cJSON_AddStringToObject(it, "abstract", s); free(s);
    }
    cJSON_AddItemToArray(results, it);
    entry = ee + 1;
    if (cJSON_GetArraySize(results) >= limit) break;
  }
  http_response_free(&hr);
  return results;
}

static cJSON *search_crossref(http_client *h, const char *q, int limit) {
  cJSON *results = cJSON_CreateArray();
  char enc[1024]; uri_encode(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url, "https://api.crossref.org/works?query=%s&rows=%d", enc, limit);
  cJSON *j = http_json(h, url);
  if (!j) return results;
  cJSON *msg = cJSON_GetObjectItem(j, "message");
  cJSON *items = msg ? cJSON_GetObjectItem(msg, "items") : NULL;
  if (items && cJSON_IsArray(items)) {
    int n = cJSON_GetArraySize(items);
    for (int i = 0; i < n && i < limit; i++) {
      cJSON *w = cJSON_GetArrayItem(items, i);
      if (!w) continue;
      cJSON *it = cJSON_CreateObject();
      cJSON_AddStringToObject(it, "source", "crossref");
      cJSON *doi = cJSON_GetObjectItem(w, "DOI");
      if (doi && doi->valuestring) {
        cJSON_AddStringToObject(it, "doi", doi->valuestring);
        char du[256];
        snprintf(du, sizeof du, "https://doi.org/%s", doi->valuestring);
        cJSON_AddStringToObject(it, "url", du);
      }
      cJSON *ta = cJSON_GetObjectItem(w, "title");
      if (ta && cJSON_IsArray(ta) && cJSON_GetArraySize(ta) > 0) {
        cJSON *t = cJSON_GetArrayItem(ta, 0);
        if (t && t->valuestring) cJSON_AddStringToObject(it, "title", t->valuestring);
      }
      cJSON *pub = cJSON_GetObjectItem(w, "publisher");
      if (pub && pub->valuestring) cJSON_AddStringToObject(it, "publisher", pub->valuestring);
      cJSON *ty = cJSON_GetObjectItem(w, "type");
      if (ty && ty->valuestring) cJSON_AddStringToObject(it, "type", ty->valuestring);
      cJSON *ci = cJSON_GetObjectItem(w, "is-referenced-by-count");
      if (ci) cJSON_AddNumberToObject(it, "citations", ci->valueint);
      cJSON_AddItemToArray(results, it);
    }
  }
  cJSON_Delete(j);
  return results;
}

static cJSON *search_semantic(http_client *h, const char *q, int limit) {
  cJSON *results = cJSON_CreateArray();
  char enc[1024]; uri_encode(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url,
    "https://api.semanticscholar.org/graph/v1/paper/search?query=%s&limit=%d", enc, limit);
  cJSON *j = http_json(h, url);
  if (!j) return results;
  cJSON *d = cJSON_GetObjectItem(j, "data");
  if (d && cJSON_IsArray(d)) {
    int n = cJSON_GetArraySize(d);
    for (int i = 0; i < n && i < limit; i++) {
      cJSON *p = cJSON_GetArrayItem(d, i);
      if (!p) continue;
      cJSON *it = cJSON_CreateObject();
      cJSON_AddStringToObject(it, "source", "semantic_scholar");
      cJSON *pid = cJSON_GetObjectItem(p, "paperId");
      if (pid && pid->valuestring) {
        cJSON_AddStringToObject(it, "paper_id", pid->valuestring);
        char su[256];
        snprintf(su, sizeof su, "https://www.semanticscholar.org/paper/%s", pid->valuestring);
        cJSON_AddStringToObject(it, "url", su);
      }
      cJSON *t = cJSON_GetObjectItem(p, "title");
      if (t && t->valuestring) cJSON_AddStringToObject(it, "title", t->valuestring);
      cJSON_AddItemToArray(results, it);
    }
  }
  cJSON_Delete(j);
  return results;
}

static cJSON *search_orcid(http_client *h, const char *q, int limit) {
  cJSON *results = cJSON_CreateArray();
  char enc[1024]; uri_encode(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url, "https://pub.orcid.org/v3.0/search/?q=%s&rows=%d", enc, limit);
  cJSON *j = http_json(h, url);
  if (!j) return results;
  cJSON *sr = cJSON_GetObjectItem(j, "result");
  if (sr && cJSON_IsArray(sr)) {
    int n = cJSON_GetArraySize(sr);
    for (int i = 0; i < n && i < limit; i++) {
      cJSON *ri = cJSON_GetArrayItem(sr, i);
      cJSON *oid = cJSON_GetObjectItem(ri, "orcid-identifier");
      if (oid) {
        cJSON *it = cJSON_CreateObject();
        cJSON_AddStringToObject(it, "source", "orcid");
        cJSON *pa = cJSON_GetObjectItem(oid, "path");
        cJSON *uri = cJSON_GetObjectItem(oid, "uri");
        if (pa && pa->valuestring) cJSON_AddStringToObject(it, "orcid_id", pa->valuestring);
        if (uri && uri->valuestring) cJSON_AddStringToObject(it, "url", uri->valuestring);
        cJSON_AddItemToArray(results, it);
      }
    }
  }
  cJSON_Delete(j);
  return results;
}

static cJSON *search_cinii(http_client *h, const char *q, int limit) {
  cJSON *results = cJSON_CreateArray();
  char enc[1024]; uri_encode(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url,
    "https://cir.nii.ac.jp/all?q=%s&count=%d&format=json", enc, limit);
  cJSON *j = http_json(h, url);
  if (!j) return results;   /* honest empty — no lookup-URL stub */
  cJSON *items = cJSON_GetObjectItem(j, "items");
  if (items && cJSON_IsArray(items)) {
    int n = cJSON_GetArraySize(items);
    for (int i = 0; i < n && i < limit; i++) {
      cJSON *p = cJSON_GetArrayItem(items, i);
      if (!p) continue;
      cJSON *it = cJSON_CreateObject();
      cJSON_AddStringToObject(it, "source", "cinii");
      cJSON *t = cJSON_GetObjectItem(p, "title");
      if (t && t->valuestring) cJSON_AddStringToObject(it, "title", t->valuestring);
      cJSON *l = cJSON_GetObjectItem(p, "@id");
      if (l && l->valuestring) cJSON_AddStringToObject(it, "url", l->valuestring);
      cJSON_AddItemToArray(results, it);
    }
  }
  cJSON_Delete(j);
  return results;
}

/* FNV-1a 32-bit hash → hex, for a stable key when a paper lacks DOI/id. */
static void hash_hex(const char *s, char *out, size_t cap) {
  unsigned int h = 2166136261u;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    h ^= *p; h *= 16777619u;
  }
  snprintf(out, cap, "%08x", h);
}

/* Emit one intel row for a single fetched paper object. Returns 1 if emitted.
 * `paper` carries source-specific fields built by the search_* helpers:
 * source, title, url, doi, date, journal/publisher, authors[], abstract, etc. */
static int emit_paper(intel_sink *sink, const char *q, cJSON *paper) {
  if (!paper) return 0;
  cJSON *src   = cJSON_GetObjectItem(paper, "source");
  cJSON *t     = cJSON_GetObjectItem(paper, "title");
  cJSON *url   = cJSON_GetObjectItem(paper, "url");
  cJSON *doi   = cJSON_GetObjectItem(paper, "doi");
  cJSON *date  = cJSON_GetObjectItem(paper, "date");
  cJSON *abst  = cJSON_GetObjectItem(paper, "abstract");
  cJSON *auth  = cJSON_GetObjectItem(paper, "authors");

  const char *source_db = (src && src->valuestring) ? src->valuestring : "academic";
  const char *title_s   = (t && t->valuestring) ? t->valuestring : NULL;
  const char *url_s     = (url && url->valuestring) ? url->valuestring : NULL;
  const char *doi_s     = (doi && doi->valuestring) ? doi->valuestring : NULL;
  const char *date_s    = (date && date->valuestring) ? date->valuestring : NULL;

  /* Per-paper body. */
  cJSON *data = cJSON_CreateObject();
  if (title_s) cJSON_AddStringToObject(data, "title", title_s);
  if (auth) cJSON_AddItemToObject(data, "authors", cJSON_Duplicate(auth, 1));
  if (doi_s) cJSON_AddStringToObject(data, "doi", doi_s);
  if (date_s) cJSON_AddStringToObject(data, "year", date_s);
  cJSON_AddStringToObject(data, "source", source_db);
  if (url_s) cJSON_AddStringToObject(data, "url", url_s);
  if (abst && abst->valuestring) cJSON_AddStringToObject(data, "abstract", abst->valuestring);
  char *bj = cJSON_PrintUnformatted(data);

  /* Summary = authors + year. */
  char summary[320] = {0};
  size_t sw = 0;
  if (auth && cJSON_IsArray(auth) && cJSON_GetArraySize(auth) > 0) {
    int ac = cJSON_GetArraySize(auth);
    for (int k = 0; k < ac && k < 3 && sw + 2 < sizeof summary; k++) {
      cJSON *a = cJSON_GetArrayItem(auth, k);
      const char *an = (a && a->valuestring) ? a->valuestring : NULL;
      if (!an) continue;
      if (sw) sw += (size_t)snprintf(summary + sw, sizeof summary - sw, ", ");
      sw += (size_t)snprintf(summary + sw, sizeof summary - sw, "%s", an);
    }
    if (ac > 3 && sw + 8 < sizeof summary)
      sw += (size_t)snprintf(summary + sw, sizeof summary - sw, " et al.");
  }
  if (date_s && sw + 4 < sizeof summary)
    snprintf(summary + sw, sizeof summary - sw, "%s(%s)", sw ? " " : "", date_s);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "ACADEMIC_SEARCH");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddStringToObject(props, "source_db", source_db);
  char *pj = cJSON_PrintUnformatted(props);

  /* Stable per-paper key: DOI if present, else source + id/title hash. */
  char rk[400];
  if (doi_s) {
    snprintf(rk, sizeof rk, "paper:doi:%s", doi_s);
  } else {
    cJSON *pmid = cJSON_GetObjectItem(paper, "pmid");
    cJSON *axid = cJSON_GetObjectItem(paper, "arxiv_id");
    cJSON *spid = cJSON_GetObjectItem(paper, "paper_id");
    cJSON *oid  = cJSON_GetObjectItem(paper, "orcid_id");
    const char *id = NULL;
    if (pmid && pmid->valuestring) id = pmid->valuestring;
    else if (axid && axid->valuestring) id = axid->valuestring;
    else if (spid && spid->valuestring) id = spid->valuestring;
    else if (oid && oid->valuestring) id = oid->valuestring;
    if (id) snprintf(rk, sizeof rk, "paper:%s:%s", source_db, id);
    else if (url_s) snprintf(rk, sizeof rk, "paper:%s:%s", source_db, url_s);
    else {
      char hx[16]; hash_hex(title_s ? title_s : source_db, hx, sizeof hx);
      snprintf(rk, sizeof rk, "paper:%s:%s", source_db, hx);
    }
  }

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title_s ? title_s : "academic paper";
  it.body            = bj;
  it.summary         = summary[0] ? summary : source_db;
  it.published_at    = date_s;
  it.link            = url_s;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"ACADEMIC_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int emit_all(intel_sink *sink, const char *q, cJSON *arr) {
  int n = 0;
  if (arr && cJSON_IsArray(arr)) {
    int c = cJSON_GetArraySize(arr);
    for (int i = 0; i < c; i++) n += emit_paper(sink, q, cJSON_GetArrayItem(arr, i));
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  cJSON *pubmed   = search_pubmed(ctx->http, q, 10);
  cJSON *arxiv    = search_arxiv(ctx->http, q, 10);
  cJSON *crossref = search_crossref(ctx->http, q, 10);
  cJSON *semantic = search_semantic(ctx->http, q, 10);
  cJSON *orcid    = search_orcid(ctx->http, q, 5);
  cJSON *cinii    = search_cinii(ctx->http, q, 10);

  int emitted = 0;
  emitted += emit_all(sink, q, pubmed);
  emitted += emit_all(sink, q, arxiv);
  emitted += emit_all(sink, q, crossref);
  emitted += emit_all(sink, q, semantic);
  emitted += emit_all(sink, q, orcid);
  emitted += emit_all(sink, q, cinii);

  cJSON_Delete(pubmed); cJSON_Delete(arxiv); cJSON_Delete(crossref);
  cJSON_Delete(semantic); cJSON_Delete(orcid); cJSON_Delete(cinii);

  (void)emitted;
  return 0;   /* honest empty is not an error */
}

static const source_def academic_search_def = {
  .id = "ACADEMIC_SEARCH", .collector = "osint",
  .name = "Academic Search", .name_ja = "学術検索",
  .update_interval_sec = 0, .run = run,
  .category = "news", .type = "api",
  .url = "internal://osint/academic-search",
  .description = "Searches PubMed, arXiv, CrossRef, Semantic Scholar, ORCID, CiNii",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(academic_search_def)
