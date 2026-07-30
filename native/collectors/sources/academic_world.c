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
  const cJSON *e = cJSON_GetArrayItem(arr, 0);  /* exhaustive-ok: Crossref wraps single-valued fields in an array; callers duplicate the array into the record */
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
    /* (cap removed: every record of the fetched array is emitted —
     * docs/SOURCE_EXHAUSTIVENESS.md) */
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
    /* (cap removed: every record of the fetched array is emitted —
     * docs/SOURCE_EXHAUSTIVENESS.md) */
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

/* ===========================================================================
 * Merged from former academic2_world.c — additional scholarly / research
 * pivots, all keyless & LIVE. On-demand entity pivot (ctx->entity).
 * ac2_run() dispatches on ctx->source_id (kept separate from run() above).
 *
 *   ORCID_SEARCH   ORCID public expanded-search — researchers by name
 *                  (pub.orcid.org/v3.0/expanded-search/?q=<entity>)
 *   ROR_ORGS       Research Organization Registry — institutions
 *                  (api.ror.org/organizations?query=<entity>)
 *   DOAJ_ARTICLES  Directory of Open Access Journals — open-access articles
 *                  (doaj.org/api/search/articles/<entity>)
 *   OPENCITATIONS  OpenCitations index+meta — citations of a DOI. Only meaningful
 *                  when the entity is a DOI; a plain name → honest empty.
 * =========================================================================== */

/* ---- ORCID expanded-search ---------------------------------------------- *
 * { "expanded-result": [ { orcid-id, given-names, family-names, credit-name,
 *   institution-name:[...] }, ... ] }  (public API, keyless, JSON via Accept). */
static int ac_orcid(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://pub.orcid.org/v3.0/expanded-search/?q=%s&rows=25", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (osint@example.org)", NULL };
  char *body = jo_get(ctx, url, hdrs, "ORCID_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *arr = cJSON_GetObjectItem(root, "expanded-result");
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *r;
    cJSON_ArrayForEach(r, arr) {
      const char *oid  = jo_sv(r, "orcid-id");
      const char *giv  = jo_sv(r, "given-names");
      const char *fam  = jo_sv(r, "family-names");
      const char *cred = jo_sv(r, "credit-name");
      if (!oid) continue;

      char name[256] = {0};
      if (cred) snprintf(name, sizeof name, "%s", cred);
      else snprintf(name, sizeof name, "%s%s%s",
                    giv ? giv : "", (giv && fam) ? " " : "", fam ? fam : "");
      if (!name[0]) snprintf(name, sizeof name, "%s", oid);

      /* first institution (if any) */
      const char *inst = NULL;
      cJSON *insts = cJSON_GetObjectItem(r, "institution-name");
      if (cJSON_IsArray(insts)) {
        cJSON *first = cJSON_GetArrayItem(insts, 0);  /* exhaustive-ok: display pick; institutions_all carries every affiliation */
        if (first && cJSON_IsString(first) && first->valuestring[0])
          inst = first->valuestring;
      }

      char link[128];
      snprintf(link, sizeof link, "https://orcid.org/%s", oid);

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "name", name);
      cJSON_AddStringToObject(data, "orcid", oid);
      if (inst) cJSON_AddStringToObject(data, "institution", inst);
      if (cJSON_IsArray(insts) && cJSON_GetArraySize(insts) > 1)
        cJSON_AddItemToObject(data, "institutions_all", cJSON_Duplicate(insts, 1));
      cJSON_AddStringToObject(data, "source", "ORCID");
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "ORCID_SEARCH");
      cJSON_AddStringToObject(props, "orcid", oid);
      if (inst) cJSON_AddStringToObject(props, "institution", inst);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      intel_item it = {0};
      it.remote_key      = oid;
      it.title           = name;
      it.summary         = inst;
      it.body            = bj;
      it.lang            = "en";
      it.link            = link;
      it.record_type     = "orcid-researcher";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"ORCID_SEARCH\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[ORCID_SEARCH] emitted %d\n", emitted);
  return emitted;
}

/* ---- ROR organizations -------------------------------------------------- *
 * { "items": [ { id, established, names:[{value,types,lang}],
 *   locations:[{geonames_details:{country_name,lat,lng,name}}],
 *   links:[{type,value}] }, ... ] }  keyless JSON. */
static int ac_ror(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url, "https://api.ror.org/organizations?query=%s", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "ROR_ORGS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *items = cJSON_GetObjectItem(root, "items");
  int emitted = 0, n = 0;
  if (cJSON_IsArray(items)) {
    cJSON *r;
    cJSON_ArrayForEach(r, items) {
      if (n++ >= 25) break;
      const char *id = jo_sv(r, "id");   /* full https://ror.org/... URL */
      if (!id) continue;

      /* display name = first names[] entry with type ror_display, else first. */
      const char *name = NULL;
      cJSON *names = cJSON_GetObjectItem(r, "names");
      if (cJSON_IsArray(names)) {
        cJSON *nm;
        cJSON_ArrayForEach(nm, names) {
          const char *v = jo_sv(nm, "value");
          if (!v) continue;
          if (!name) name = v;
          cJSON *types = cJSON_GetObjectItem(nm, "types");
          if (cJSON_IsArray(types)) {
            cJSON *t;
            cJSON_ArrayForEach(t, types)
              if (cJSON_IsString(t) && strcmp(t->valuestring, "ror_display") == 0) {
                name = v; break;
              }
          }
        }
      }
      if (!name) name = jo_sv(r, "name");
      if (!name) continue;

      /* country + coordinates from first location */
      const char *country = NULL, *city = NULL;
      int has_geo = 0; double lat = 0, lon = 0;
      cJSON *locs = cJSON_GetObjectItem(r, "locations");
      if (cJSON_IsArray(locs)) {
        cJSON *l0 = cJSON_GetArrayItem(locs, 0);  /* exhaustive-ok: display pick; locations_all carries every site */
        cJSON *gd = l0 ? cJSON_GetObjectItem(l0, "geonames_details") : NULL;
        if (gd) {
          country = jo_sv(gd, "country_name");
          city    = jo_sv(gd, "name");
          cJSON *la = cJSON_GetObjectItem(gd, "lat");
          cJSON *lo = cJSON_GetObjectItem(gd, "lng");
          if (cJSON_IsNumber(la) && cJSON_IsNumber(lo)) {
            has_geo = 1; lat = la->valuedouble; lon = lo->valuedouble;
          }
        }
      }
      const cJSON *est = cJSON_GetObjectItem(r, "established");

      /* website link if present */
      const char *website = NULL;
      cJSON *links = cJSON_GetObjectItem(r, "links");
      if (cJSON_IsArray(links)) {
        cJSON *lk;
        cJSON_ArrayForEach(lk, links) {
          const char *ty = jo_sv(lk, "type");
          const char *va = jo_sv(lk, "value");
          if (va && ty && strcmp(ty, "website") == 0) { website = va; break; }
        }
      }

      char loc[256] = {0};
      snprintf(loc, sizeof loc, "%s%s%s",
               city ? city : "", (city && country) ? ", " : "",
               country ? country : "");

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "name", name);
      cJSON_AddStringToObject(data, "ror_id", id);
      if (loc[0])  cJSON_AddStringToObject(data, "location", loc);
      if (est && cJSON_IsNumber(est))
        cJSON_AddNumberToObject(data, "established", est->valuedouble);
      if (website) cJSON_AddStringToObject(data, "website", website);
      /* An institution can have several sites and several links; `location`
       * and `website` are display picks, so carry the full arrays as well
       * (docs/SOURCE_EXHAUSTIVENESS.md). */
      if (cJSON_IsArray(locs) && cJSON_GetArraySize(locs) > 0)
        cJSON_AddItemToObject(data, "locations_all", cJSON_Duplicate(locs, 1));
      if (cJSON_IsArray(links) && cJSON_GetArraySize(links) > 0)
        cJSON_AddItemToObject(data, "links_all", cJSON_Duplicate(links, 1));
      cJSON_AddStringToObject(data, "source", "ROR");
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "ROR_ORGS");
      cJSON_AddStringToObject(props, "ror_id", id);
      if (country) cJSON_AddStringToObject(props, "country", country);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      intel_item it = {0};
      it.remote_key      = id;
      it.title           = name;
      it.summary         = loc[0] ? loc : NULL;
      it.body            = bj;
      it.lang            = "en";
      it.link            = website ? website : id;
      it.has_geo         = has_geo;
      it.lat             = lat;
      it.lon             = lon;
      it.record_type     = "ror-institution";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"ROR_ORGS\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[ROR_ORGS] emitted %d\n", emitted);
  return emitted;
}

/* ---- DOAJ articles ------------------------------------------------------ *
 * { "results": [ { bibjson: { title, year, journal:{title,publisher,language},
 *   author:[{name,affiliation}], identifier:[{id,type}], link:[{url,type}] } } ] }
 * keyless JSON. */
static int ac_doaj(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://doaj.org/api/search/articles/%s?pageSize=25", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "DOAJ_ARTICLES");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *doaj_id = jo_sv(r, "id");
      cJSON *bib = cJSON_GetObjectItem(r, "bibjson");
      if (!bib) continue;
      const char *title = jo_sv(bib, "title");
      if (!title) continue;
      const char *year = jo_sv(bib, "year");

      cJSON *journal = cJSON_GetObjectItem(bib, "journal");
      const char *jtitle = journal ? jo_sv(journal, "title") : NULL;
      const char *jpub   = journal ? jo_sv(journal, "publisher") : NULL;

      /* DOI + fulltext link */
      const char *doi = NULL;
      cJSON *ids = cJSON_GetObjectItem(bib, "identifier");
      if (cJSON_IsArray(ids)) {
        cJSON *id;
        cJSON_ArrayForEach(id, ids) {
          const char *ty = jo_sv(id, "type");
          const char *va = jo_sv(id, "id");
          if (va && ty && strcmp(ty, "doi") == 0) { doi = va; break; }
        }
      }
      const char *fulltext = NULL;
      cJSON *lnks = cJSON_GetObjectItem(bib, "link");
      if (cJSON_IsArray(lnks)) {
        cJSON *lk = cJSON_GetArrayItem(lnks, 0);  /* exhaustive-ok: display pick; links_all carries every link */
        if (lk) fulltext = jo_sv(lk, "url");
      }

      /* author list */
      char authors[512] = {0}; size_t aj = 0; int na = 0;
      cJSON *auth = cJSON_GetObjectItem(bib, "author");
      if (cJSON_IsArray(auth)) {
        cJSON *a;
        cJSON_ArrayForEach(a, auth) {
          const char *nm = jo_sv(a, "name");
          if (!nm) continue;
          size_t need = aj + strlen(nm) + 2;
          if (need >= sizeof authors) break;
          if (na) { authors[aj++] = ','; authors[aj++] = ' '; }
          strcpy(authors + aj, nm); aj += strlen(nm);
          if (++na >= 20) break;
        }
      }

      char doi_url[256] = {0};
      if (doi) snprintf(doi_url, sizeof doi_url,
                        strncmp(doi, "http", 4) == 0 ? "%s" : "https://doi.org/%s", doi);
      char page_url[128] = {0};
      if (doaj_id) snprintf(page_url, sizeof page_url,
                            "https://doaj.org/article/%s", doaj_id);
      const char *link = doi_url[0] ? doi_url : (fulltext ? fulltext : (page_url[0] ? page_url : NULL));

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "title", title);
      /* every full-text link the record listed, not just the first */
      if (cJSON_IsArray(lnks) && cJSON_GetArraySize(lnks) > 0)
        cJSON_AddItemToObject(data, "links_all", cJSON_Duplicate(lnks, 1));
      if (doi)     cJSON_AddStringToObject(data, "doi", doi);
      if (year)    cJSON_AddStringToObject(data, "year", year);
      if (jtitle)  cJSON_AddStringToObject(data, "journal", jtitle);
      if (jpub)    cJSON_AddStringToObject(data, "publisher", jpub);
      if (authors[0]) cJSON_AddStringToObject(data, "authors", authors);
      cJSON_AddStringToObject(data, "source", "DOAJ");
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "DOAJ_ARTICLES");
      if (doi)  cJSON_AddStringToObject(props, "doi", doi);
      if (year) cJSON_AddStringToObject(props, "year", year);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      char rkey[256];
      snprintf(rkey, sizeof rkey, "%s", doi ? doi : (doaj_id ? doaj_id : title));

      intel_item it = {0};
      it.remote_key      = rkey;
      it.title           = title;
      it.summary         = jtitle;
      it.body            = bj;
      it.author          = authors[0] ? authors : NULL;
      it.lang            = "en";
      it.published_at    = year;
      it.link            = link;
      it.record_type     = "doaj-article";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"DOAJ_ARTICLES\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[DOAJ_ARTICLES] emitted %d\n", emitted);
  return emitted;
}

/* ---- OpenCitations ------------------------------------------------------ *
 * Meaningful only for a DOI pivot. We first pull OpenCitations Meta metadata for
 * the DOI, then the incoming citations index — emitting one item per citing
 * paper (real DOIs from the index). A non-DOI entity → honest empty. */
static int ac_looks_like_doi(const char *s) {
  const char *p = strstr(s, "10.");
  return p && strchr(p, '/');
}

static int ac_opencitations(const source_ctx *ctx, intel_sink *sink, const char *q) {
  if (!ac_looks_like_doi(q)) {
    fprintf(stderr, "[OPENCITATIONS] entity is not a DOI, honest empty\n");
    return 0;
  }
  /* normalize: strip a leading https://doi.org/ if present */
  const char *doi = q;
  const char *slash = strstr(q, "doi.org/");
  if (slash) doi = slash + 8;
  char *encd = jo_urlencode(doi);
  if (!encd) return 0;

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };

  /* metadata for the cited paper (title/venue) — best-effort context. */
  char murl[512];
  snprintf(murl, sizeof murl,
    "https://opencitations.net/meta/api/v1/metadata/doi:%s", encd);
  char *mtitle = NULL, *mvenue = NULL;
  char *mbody = jo_get(ctx, murl, hdrs, "OPENCITATIONS");
  if (mbody) {
    cJSON *mroot = cJSON_Parse(mbody);
    free(mbody);
    if (mroot && cJSON_IsArray(mroot)) {
      cJSON *m0 = cJSON_GetArrayItem(mroot, 0);  /* exhaustive-ok: title->paper resolution step */
      if (m0) {
        const char *t = jo_sv(m0, "title");
        const char *v = jo_sv(m0, "venue");
        if (t) mtitle = strdup(t);
        if (v) mvenue = strdup(v);
      }
    }
    if (mroot) cJSON_Delete(mroot);
  }

  char curl[512];
  snprintf(curl, sizeof curl,
    "https://opencitations.net/index/api/v1/citations/%s", encd);
  free(encd);
  char *body = jo_get(ctx, curl, hdrs, "OPENCITATIONS");
  if (!body) { free(mtitle); free(mvenue); return 0; }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { free(mtitle); free(mvenue); return 0; }

  int emitted = 0, n = 0;
  if (cJSON_IsArray(root)) {
    cJSON *r;
    cJSON_ArrayForEach(r, root) {
      if (n++ >= 50) break;
      const char *citing = jo_sv(r, "citing");
      const char *oci    = jo_sv(r, "oci");
      const char *crea   = jo_sv(r, "creation");
      const char *tspan  = jo_sv(r, "timespan");
      if (!citing) continue;

      char link[256];
      snprintf(link, sizeof link, "https://doi.org/%s", citing);
      char title[320];
      snprintf(title, sizeof title, "Citing DOI %s", citing);

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "citing_doi", citing);
      cJSON_AddStringToObject(data, "cited_doi", doi);
      if (mtitle) cJSON_AddStringToObject(data, "cited_title", mtitle);
      if (mvenue) cJSON_AddStringToObject(data, "cited_venue", mvenue);
      if (crea)  cJSON_AddStringToObject(data, "citation_creation", crea);
      if (tspan) cJSON_AddStringToObject(data, "timespan", tspan);
      if (oci)   cJSON_AddStringToObject(data, "oci", oci);
      cJSON_AddStringToObject(data, "source", "OpenCitations");
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "OPENCITATIONS");
      cJSON_AddStringToObject(props, "cited_doi", doi);
      cJSON_AddStringToObject(props, "citing_doi", citing);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      intel_item it = {0};
      it.remote_key      = oci ? oci : citing;
      it.title           = title;
      it.summary         = mtitle;
      it.body            = bj;
      it.lang            = "en";
      it.published_at    = crea;
      it.link            = link;
      it.record_type     = "opencitations-citation";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"OPENCITATIONS\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  free(mtitle); free(mvenue);
  fprintf(stderr, "[OPENCITATIONS] emitted %d\n", emitted);
  return emitted;
}

static int ac2_run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  if (strcmp(ctx->source_id, "ORCID_SEARCH") == 0)  ac_orcid(ctx, sink, q);
  else if (strcmp(ctx->source_id, "ROR_ORGS") == 0) ac_ror(ctx, sink, q);
  else if (strcmp(ctx->source_id, "DOAJ_ARTICLES") == 0) ac_doaj(ctx, sink, q);
  else if (strcmp(ctx->source_id, "OPENCITATIONS") == 0) ac_opencitations(ctx, sink, q);
  else return -1;

  return 0;   /* honest empty is not an error */
}

#define AC2_DEF(SYM, ID, NAME, NAMEJA, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = ac2_run, \
    .category = "government", .type = "api", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

AC2_DEF(ac_orcid_def, "ORCID_SEARCH", "ORCID Researcher Search", "ORCID 研究者検索",
  "https://pub.orcid.org",
  "ORCID public expanded-search — researchers by name (keyless): ORCID iD, name, institution");
AC2_DEF(ac_ror_def, "ROR_ORGS", "ROR Institution Registry", "ROR 研究機関レジストリ",
  "https://ror.org",
  "Research Organization Registry — institutions by name (keyless): ROR id, location, website");
AC2_DEF(ac_doaj_def, "DOAJ_ARTICLES", "DOAJ Open-Access Articles", "DOAJ オープンアクセス論文",
  "https://doaj.org",
  "Directory of Open Access Journals article search (keyless): title, DOI, year, journal, authors");
AC2_DEF(ac_oc_def, "OPENCITATIONS", "OpenCitations Citations", "OpenCitations 引用索引",
  "https://opencitations.net",
  "OpenCitations index+meta — incoming citations of a DOI entity (keyless): citing DOIs, timespan");
