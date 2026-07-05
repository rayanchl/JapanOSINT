/* collectors/sources/academic_world.c
 * OSINT services — five keyless, LIVE global scholarly / bibliographic sources,
 * all pivoting on ctx->entity. One shared run() switches on ctx->source_id:
 *
 *   CROSSREF         api.crossref.org/works?query=<e>&rows=15   (JSON)
 *   SEMANTIC_SCHOLAR api.semanticscholar.org/graph/v1/paper/search  (JSON)
 *   ARXIV            export.arxiv.org/api/query?search_query=all:<e> (Atom XML)
 *   PUBMED           eutils esearch.fcgi → esummary.fcgi         (JSON)
 *   OPENLIBRARY      openlibrary.org/search.json?q=<e>           (JSON)
 *
 * Every source REAL-fetches and emits ONE intel_item per parsed record, or
 * honest-empty (return 0) on fetch failure / no matches. Nothing is fabricated;
 * links are resolvable URLs taken from (or trivially derived from) the API's own
 * identifiers (DOI, arXiv id, PMID, OpenLibrary key). No API keys required. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *AW_UA =
  "User-Agent: JapanOSINT/1.0 (mailto:osint@example.org)";

/* ---- Crossref ----------------------------------------------------------- */
/* Crossref work title/author arrays are arrays-of-strings / arrays-of-objects. */
static char *aw_crossref_first_str(const cJSON *arr) {
  if (!cJSON_IsArray(arr)) return NULL;
  const cJSON *e = cJSON_GetArrayItem(arr, 0);
  if (e && cJSON_IsString(e) && e->valuestring && e->valuestring[0])
    return e->valuestring;
  return NULL;
}
static char *aw_crossref_authors(const cJSON *w) {
  const cJSON *au = cJSON_GetObjectItem(w, "author");
  if (!cJSON_IsArray(au)) return NULL;
  size_t cap = 256, len = 0; char *out = (char *)malloc(cap);
  if (!out) return NULL; out[0] = 0; int n = 0;
  const cJSON *a;
  cJSON_ArrayForEach(a, au) {
    const char *given = jo_sv(a, "given");
    const char *family = jo_sv(a, "family");
    const char *nm = family ? family : given;
    if (!nm) continue;
    char buf[256];
    if (given && family) snprintf(buf, sizeof buf, "%s %s", given, family);
    else snprintf(buf, sizeof buf, "%s", nm);
    size_t need = len + strlen(buf) + 3;
    if (need > cap) { cap = need * 2; char *t = realloc(out, cap); if (!t) break; out = t; }
    if (n) { strcpy(out + len, ", "); len += 2; }
    strcpy(out + len, buf); len += strlen(buf);
    if (++n >= 20) break;
  }
  if (!n) { free(out); return NULL; }
  return out;
}
static int aw_emit_crossref(intel_sink *sink, const cJSON *w) {
  if (!w) return 0;
  char *title = aw_crossref_first_str(cJSON_GetObjectItem(w, "title"));
  const char *doi = jo_sv(w, "DOI");
  if (!title && !doi) return 0;
  const char *type = jo_sv(w, "type");
  const char *pub  = aw_crossref_first_str(cJSON_GetObjectItem(w, "container-title"));
  const cJSON *cc  = cJSON_GetObjectItem(w, "is-referenced-by-count");
  const char *url  = jo_sv(w, "URL");
  char *authors = aw_crossref_authors(w);

  cJSON *data = cJSON_CreateObject();
  if (title) cJSON_AddStringToObject(data, "title", title);
  if (doi)   cJSON_AddStringToObject(data, "doi", doi);
  if (type)  cJSON_AddStringToObject(data, "type", type);
  if (pub)   cJSON_AddStringToObject(data, "container", pub);
  if (authors) cJSON_AddStringToObject(data, "authors", authors);
  if (cc && cJSON_IsNumber(cc)) cJSON_AddNumberToObject(data, "cited_by_count", cc->valuedouble);
  cJSON_AddStringToObject(data, "source", "Crossref");
  char *bj = cJSON_PrintUnformatted(data); cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CROSSREF");
  cJSON_AddStringToObject(props, "source", "Crossref");
  if (doi) cJSON_AddStringToObject(props, "doi", doi);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props); cJSON_Delete(props);

  char link[512] = {0};
  if (url) snprintf(link, sizeof link, "%s", url);
  else if (doi) snprintf(link, sizeof link, "https://doi.org/%s", doi);

  intel_item it = {0};
  it.remote_key      = doi ? doi : title;
  it.title           = title ? title : doi;
  it.summary         = authors ? authors : pub;
  it.body            = bj;
  it.author          = authors;
  it.lang            = "en";
  it.link            = link[0] ? link : NULL;
  it.record_type     = "crossref-work";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"CROSSREF\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(authors);
  return rc >= 0 ? 1 : 0;
}
static int run_crossref(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.crossref.org/works?query=%s&rows=15", enc);
  const char *hdrs[] = { "Accept: application/json", AW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "crossref");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *msg = cJSON_GetObjectItem(root, "message");
  const cJSON *items = msg ? cJSON_GetObjectItem(msg, "items") : NULL;
  int emitted = 0;
  if (cJSON_IsArray(items)) {
    const cJSON *w; cJSON_ArrayForEach(w, items) emitted += aw_emit_crossref(sink, w);
  }
  cJSON_Delete(root);
  return emitted;
}

/* ---- Semantic Scholar ---------------------------------------------------- */
static char *aw_s2_authors(const cJSON *p) {
  const cJSON *au = cJSON_GetObjectItem(p, "authors");
  if (!cJSON_IsArray(au)) return NULL;
  size_t cap = 256, len = 0; char *out = (char *)malloc(cap);
  if (!out) return NULL; out[0] = 0; int n = 0;
  const cJSON *a;
  cJSON_ArrayForEach(a, au) {
    const char *nm = jo_sv(a, "name");
    if (!nm) continue;
    size_t need = len + strlen(nm) + 3;
    if (need > cap) { cap = need * 2; char *t = realloc(out, cap); if (!t) break; out = t; }
    if (n) { strcpy(out + len, ", "); len += 2; }
    strcpy(out + len, nm); len += strlen(nm);
    if (++n >= 20) break;
  }
  if (!n) { free(out); return NULL; }
  return out;
}
static int aw_emit_s2(intel_sink *sink, const cJSON *p) {
  if (!p) return 0;
  const char *title = jo_sv(p, "title");
  const char *pid   = jo_sv(p, "paperId");
  if (!title && !pid) return 0;
  const char *abstract = jo_sv(p, "abstract");
  const char *venue = jo_sv(p, "venue");
  const char *urlf  = jo_sv(p, "url");
  const cJSON *yr = cJSON_GetObjectItem(p, "year");
  const cJSON *cc = cJSON_GetObjectItem(p, "citationCount");
  char *authors = aw_s2_authors(p);

  cJSON *data = cJSON_CreateObject();
  if (title) cJSON_AddStringToObject(data, "title", title);
  if (authors) cJSON_AddStringToObject(data, "authors", authors);
  if (venue) cJSON_AddStringToObject(data, "venue", venue);
  if (yr && cJSON_IsNumber(yr)) cJSON_AddNumberToObject(data, "year", yr->valuedouble);
  if (cc && cJSON_IsNumber(cc)) cJSON_AddNumberToObject(data, "citation_count", cc->valuedouble);
  if (abstract) cJSON_AddStringToObject(data, "abstract", abstract);
  if (pid) cJSON_AddStringToObject(data, "paper_id", pid);
  cJSON_AddStringToObject(data, "source", "Semantic Scholar");
  char *bj = cJSON_PrintUnformatted(data); cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SEMANTIC_SCHOLAR");
  cJSON_AddStringToObject(props, "source", "Semantic Scholar");
  if (pid) cJSON_AddStringToObject(props, "paper_id", pid);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props); cJSON_Delete(props);

  char link[512] = {0};
  if (urlf) snprintf(link, sizeof link, "%s", urlf);
  else if (pid) snprintf(link, sizeof link, "https://www.semanticscholar.org/paper/%s", pid);

  intel_item it = {0};
  it.remote_key      = pid ? pid : title;
  it.title           = title ? title : pid;
  it.summary         = authors ? authors : venue;
  it.body            = bj;
  it.author          = authors;
  it.lang            = "en";
  it.link            = link[0] ? link : NULL;
  it.record_type     = "s2-paper";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SEMANTIC_SCHOLAR\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(authors);
  return rc >= 0 ? 1 : 0;
}
static int run_s2(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.semanticscholar.org/graph/v1/paper/search?query=%s&limit=15"
    "&fields=title,abstract,venue,year,citationCount,url,authors", enc);
  const char *hdrs[] = { "Accept: application/json", AW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "semantic_scholar");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *data = cJSON_GetObjectItem(root, "data");
  int emitted = 0;
  if (cJSON_IsArray(data)) {
    const cJSON *p; cJSON_ArrayForEach(p, data) emitted += aw_emit_s2(sink, p);
  }
  cJSON_Delete(root);
  return emitted;
}

/* ---- arXiv (Atom XML) ---------------------------------------------------- */
static int run_arxiv(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[1024];
  snprintf(url, sizeof url,
    "https://export.arxiv.org/api/query?search_query=all:%s&max_results=15", enc);
  const char *hdrs[] = { AW_UA, NULL };
  char *xml = jo_get(ctx, url, hdrs, "arxiv");
  if (!xml) return 0;
  int emitted = 0;
  const char *cur = xml;
  /* skip the feed-level <title>/<id> by starting scan at first <entry> */
  const char *entry = strstr(cur, "<entry");
  cur = entry;
  while (cur && (cur = strstr(cur, "<entry")) != NULL) {
    const char * eend = strstr(cur, "</entry>");
    if (!eend) break;
    /* bounded copy of this entry block so tag scans don't cross entries */
    size_t elen = (size_t)(eend - cur);
    char *blk = (char *)malloc(elen + 1);
    if (!blk) break;
    memcpy(blk, cur, elen); blk[elen] = 0;

    const char *bc = blk;
    char *id    = jo_tag_inner(&bc, "id");        /* arXiv abs URL */
    bc = blk; char *title = jo_tag_inner(&bc, "title");
    bc = blk; char *summ  = jo_tag_inner(&bc, "summary");
    bc = blk; char *pub   = jo_tag_inner(&bc, "published");

    /* authors: repeated <author><name>..</name></author> */
    char authors[1024]; authors[0] = 0; size_t alen = 0; int an = 0;
    const char *ap = blk;
    while (an < 20) {
      const char *nm_open = strstr(ap, "<name>");
      if (!nm_open) break;
      const char *nc = nm_open;
      char *nm = jo_tag_inner(&nc, "name");
      ap = nc;
      if (nm && nm[0]) {
        size_t need = alen + strlen(nm) + 3;
        if (need < sizeof authors) {
          if (an) { strcpy(authors + alen, ", "); alen += 2; }
          strcpy(authors + alen, nm); alen += strlen(nm); an++;
        }
      }
      free(nm);
    }

    if (title || id) {
      /* collapse whitespace/newlines in title */
      if (title) for (char *t = title; *t; t++) if (*t == '\n' || *t == '\t') *t = ' ';

      cJSON *data = cJSON_CreateObject();
      if (title) cJSON_AddStringToObject(data, "title", title);
      if (an)    cJSON_AddStringToObject(data, "authors", authors);
      if (summ)  cJSON_AddStringToObject(data, "abstract", summ);
      if (id)    cJSON_AddStringToObject(data, "arxiv_url", id);
      cJSON_AddStringToObject(data, "source", "arXiv");
      char *bj = cJSON_PrintUnformatted(data); cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "ARXIV");
      cJSON_AddStringToObject(props, "source", "arXiv");
      if (id) cJSON_AddStringToObject(props, "arxiv_url", id);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props); cJSON_Delete(props);

      intel_item it = {0};
      it.remote_key      = id ? id : title;
      it.title           = title ? title : id;
      it.summary         = an ? authors : NULL;
      it.body            = bj;
      it.author          = an ? authors : NULL;
      it.lang            = "en";
      it.published_at    = pub;
      it.link            = id;
      it.record_type     = "arxiv-paper";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"ARXIV\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
    free(id); free(title); free(summ); free(pub);
    free(blk);
    cur = eend + 8;
  }
  free(xml);
  return emitted;
}

/* ---- PubMed (E-utilities: esearch → esummary) ---------------------------- */
static int aw_emit_pubmed(intel_sink *sink, const cJSON *doc, const char *uid) {
  if (!doc) return 0;
  const char *title = jo_sv(doc, "title");
  if (!title && !uid) return 0;
  const char *source = jo_sv(doc, "source");    /* journal */
  const char *pubdate = jo_sv(doc, "pubdate");

  /* authors[] → array of objects with "name" */
  char authors[1024]; authors[0] = 0; size_t alen = 0; int an = 0;
  const cJSON *au = cJSON_GetObjectItem(doc, "authors");
  if (cJSON_IsArray(au)) {
    const cJSON *a;
    cJSON_ArrayForEach(a, au) {
      const char *nm = jo_sv(a, "name");
      if (!nm) continue;
      size_t need = alen + strlen(nm) + 3;
      if (need >= sizeof authors) break;
      if (an) { strcpy(authors + alen, ", "); alen += 2; }
      strcpy(authors + alen, nm); alen += strlen(nm);
      if (++an >= 20) break;
    }
  }

  cJSON *data = cJSON_CreateObject();
  if (title)   cJSON_AddStringToObject(data, "title", title);
  if (an)      cJSON_AddStringToObject(data, "authors", authors);
  if (source)  cJSON_AddStringToObject(data, "journal", source);
  if (pubdate) cJSON_AddStringToObject(data, "pubdate", pubdate);
  if (uid)     cJSON_AddStringToObject(data, "pmid", uid);
  cJSON_AddStringToObject(data, "source", "PubMed");
  char *bj = cJSON_PrintUnformatted(data); cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "PUBMED");
  cJSON_AddStringToObject(props, "source", "PubMed");
  if (uid) cJSON_AddStringToObject(props, "pmid", uid);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props); cJSON_Delete(props);

  char link[256] = {0};
  if (uid) snprintf(link, sizeof link, "https://pubmed.ncbi.nlm.nih.gov/%s/", uid);

  intel_item it = {0};
  it.remote_key      = uid ? uid : title;
  it.title           = title ? title : uid;
  it.summary         = an ? authors : source;
  it.body            = bj;
  it.author          = an ? authors : NULL;
  it.lang            = "en";
  it.published_at    = pubdate;
  it.link            = link[0] ? link : NULL;
  it.record_type     = "pubmed-article";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"PUBMED\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}
static int run_pubmed(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *hdrs[] = { "Accept: application/json", AW_UA, NULL };
  /* 1. esearch → list of PMIDs */
  char url[1024];
  snprintf(url, sizeof url,
    "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/esearch.fcgi"
    "?db=pubmed&retmode=json&retmax=15&term=%s", enc);
  char *body = jo_get(ctx, url, hdrs, "pubmed");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *er = cJSON_GetObjectItem(root, "esearchresult");
  const cJSON *idlist = er ? cJSON_GetObjectItem(er, "idlist") : NULL;
  if (!cJSON_IsArray(idlist) || cJSON_GetArraySize(idlist) == 0) {
    cJSON_Delete(root); return 0;
  }
  /* build comma-joined id csv */
  char ids[512]; ids[0] = 0; size_t il = 0;
  const cJSON *idn;
  cJSON_ArrayForEach(idn, idlist) {
    if (!cJSON_IsString(idn) || !idn->valuestring) continue;
    size_t need = il + strlen(idn->valuestring) + 2;
    if (need >= sizeof ids) break;
    if (il) { ids[il++] = ','; ids[il] = 0; }
    strcpy(ids + il, idn->valuestring); il += strlen(idn->valuestring);
  }
  cJSON_Delete(root);
  if (!ids[0]) return 0;

  /* 2. esummary → per-PMID metadata */
  snprintf(url, sizeof url,
    "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/esummary.fcgi"
    "?db=pubmed&retmode=json&id=%s", ids);
  body = jo_get(ctx, url, hdrs, "pubmed");
  if (!body) return 0;
  root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *result = cJSON_GetObjectItem(root, "result");
  int emitted = 0;
  if (result) {
    const cJSON *uids = cJSON_GetObjectItem(result, "uids");
    if (cJSON_IsArray(uids)) {
      const cJSON *u;
      cJSON_ArrayForEach(u, uids) {
        if (!cJSON_IsString(u) || !u->valuestring) continue;
        const cJSON *doc = cJSON_GetObjectItem(result, u->valuestring);
        emitted += aw_emit_pubmed(sink, doc, u->valuestring);
      }
    }
  }
  cJSON_Delete(root);
  return emitted;
}

/* ---- OpenLibrary --------------------------------------------------------- */
static int aw_emit_openlibrary(intel_sink *sink, const cJSON *d) {
  if (!d) return 0;
  const char *title = jo_sv(d, "title");
  const char *key   = jo_sv(d, "key");      /* e.g. /works/OL123W */
  if (!title && !key) return 0;
  const cJSON *fpy = cJSON_GetObjectItem(d, "first_publish_year");

  /* author_name[] array of strings */
  char authors[1024]; authors[0] = 0; size_t alen = 0; int an = 0;
  const cJSON *au = cJSON_GetObjectItem(d, "author_name");
  if (cJSON_IsArray(au)) {
    const cJSON *a;
    cJSON_ArrayForEach(a, au) {
      if (!cJSON_IsString(a) || !a->valuestring) continue;
      size_t need = alen + strlen(a->valuestring) + 3;
      if (need >= sizeof authors) break;
      if (an) { strcpy(authors + alen, ", "); alen += 2; }
      strcpy(authors + alen, a->valuestring); alen += strlen(a->valuestring);
      if (++an >= 20) break;
    }
  }

  cJSON *data = cJSON_CreateObject();
  if (title) cJSON_AddStringToObject(data, "title", title);
  if (an)    cJSON_AddStringToObject(data, "authors", authors);
  if (fpy && cJSON_IsNumber(fpy)) cJSON_AddNumberToObject(data, "first_publish_year", fpy->valuedouble);
  if (key)   cJSON_AddStringToObject(data, "openlibrary_key", key);
  cJSON_AddStringToObject(data, "source", "OpenLibrary");
  char *bj = cJSON_PrintUnformatted(data); cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "OPENLIBRARY");
  cJSON_AddStringToObject(props, "source", "OpenLibrary");
  if (key) cJSON_AddStringToObject(props, "openlibrary_key", key);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props); cJSON_Delete(props);

  char link[256] = {0};
  if (key) snprintf(link, sizeof link, "https://openlibrary.org%s", key);

  intel_item it = {0};
  it.remote_key      = key ? key : title;
  it.title           = title ? title : key;
  it.summary         = an ? authors : NULL;
  it.body            = bj;
  it.author          = an ? authors : NULL;
  it.lang            = "en";
  it.link            = link[0] ? link : NULL;
  it.record_type     = "openlibrary-work";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"OPENLIBRARY\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}
static int run_openlibrary(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[1024];
  snprintf(url, sizeof url,
    "https://openlibrary.org/search.json?q=%s&limit=15", enc);
  const char *hdrs[] = { "Accept: application/json", AW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "openlibrary");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *docs = cJSON_GetObjectItem(root, "docs");
  int emitted = 0;
  if (cJSON_IsArray(docs)) {
    const cJSON *d; cJSON_ArrayForEach(d, docs) emitted += aw_emit_openlibrary(sink, d);
  }
  cJSON_Delete(root);
  return emitted;
}

/* ---- dispatch ------------------------------------------------------------ */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *sid = ctx->source_id ? ctx->source_id : "";

  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  int emitted = 0;
  if      (!strcmp(sid, "CROSSREF"))         emitted = run_crossref(ctx, sink, enc);
  else if (!strcmp(sid, "SEMANTIC_SCHOLAR")) emitted = run_s2(ctx, sink, enc);
  else if (!strcmp(sid, "ARXIV"))            emitted = run_arxiv(ctx, sink, enc);
  else if (!strcmp(sid, "PUBMED"))           emitted = run_pubmed(ctx, sink, enc);
  else if (!strcmp(sid, "OPENLIBRARY"))      emitted = run_openlibrary(ctx, sink, enc);

  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def crossref_def = {
  .id = "CROSSREF", .collector = "osint",
  .name = "Crossref Works Search", .name_ja = "Crossref 文献検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://api.crossref.org",
  .description = "Crossref — global DOI/scholarly metadata (keyless): title, authors, DOI, citations",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(crossref_def)

static const source_def semantic_scholar_def = {
  .id = "SEMANTIC_SCHOLAR", .collector = "osint",
  .name = "Semantic Scholar Search", .name_ja = "Semantic Scholar 検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://api.semanticscholar.org",
  .description = "Semantic Scholar — paper search (keyless): title, abstract, authors, venue, citations",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(semantic_scholar_def)

static const source_def arxiv_def = {
  .id = "ARXIV", .collector = "osint",
  .name = "arXiv Preprint Search", .name_ja = "arXiv プレプリント検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://export.arxiv.org/api/query",
  .description = "arXiv — open preprint search (keyless, Atom API): title, authors, abstract, abs URL",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(arxiv_def)

static const source_def pubmed_def = {
  .id = "PUBMED", .collector = "osint",
  .name = "PubMed E-utilities Search", .name_ja = "PubMed 文献検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://eutils.ncbi.nlm.nih.gov/entrez/eutils",
  .description = "PubMed/NCBI E-utilities — biomedical literature (keyless): title, authors, journal, PMID",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(pubmed_def)

static const source_def openlibrary_def = {
  .id = "OPENLIBRARY", .collector = "osint",
  .name = "OpenLibrary Book Search", .name_ja = "OpenLibrary 書籍検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://openlibrary.org",
  .description = "OpenLibrary — open bibliographic catalog (keyless): title, authors, first publish year",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(openlibrary_def)
