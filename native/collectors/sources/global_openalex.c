/* collectors/sources/global_openalex.c
 * OSINT service — OPENALEX. On-demand entity pivot against OpenAlex
 * (api.openalex.org), the open scholarly catalog of works & authors. Keyless
 * and LIVE (no API key required; the polite pool just wants a mailto).
 *
 * For any entity we search /works?search=<entity>&per_page=15 and emit one
 * intel_item per work (title, DOI, publication year, author list). When the
 * entity looks like a person (contains a space, no digits/@) we additionally
 * query /authors?search=<entity> and emit one item per matched author.
 *
 * Only real, parsed API fields are emitted; fetch failure / no matches →
 * honest empty (return 0). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Build "Author A, Author B, ..." from an OpenAlex work's authorships[].
 * malloc'd (caller frees) or NULL if none. */
static char *oa_author_list(const cJSON *work) {
  const cJSON *auths = cJSON_GetObjectItem(work, "authorships");
  if (!cJSON_IsArray(auths)) return NULL;
  size_t cap = 256, len = 0;
  char *out = (char *)malloc(cap);
  if (!out) return NULL;
  out[0] = 0;
  int n = 0;
  const cJSON *a;
  cJSON_ArrayForEach(a, auths) {
    const cJSON *au = cJSON_GetObjectItem(a, "author");
    const char *nm = au ? jo_sv(au, "display_name") : NULL;
    if (!nm) continue;
    size_t need = len + strlen(nm) + 3;
    if (need > cap) { cap = need * 2; char *t = realloc(out, cap); if (!t) break; out = t; }
    if (n) { strcpy(out + len, ", "); len += 2; }
    strcpy(out + len, nm); len += strlen(nm);
    if (++n >= 25) break;
  }
  if (!n) { free(out); return NULL; }
  return out;
}

/* one work → one intel_item. Returns 1 if emitted. */
static int emit_work(intel_sink *sink, const cJSON *w) {
  if (!w) return 0;
  const char *title = jo_sv(w, "title");
  const char *id    = jo_sv(w, "id");           /* openalex work URL */
  if (!title && !id) return 0;

  const char *doi = jo_sv(w, "doi");            /* full https://doi.org/... URL */
  const char *pub = jo_sv(w, "publication_date");
  const cJSON *yr = cJSON_GetObjectItem(w, "publication_year");
  const cJSON *cc = cJSON_GetObjectItem(w, "cited_by_count");
  const cJSON *typ = w ? cJSON_GetObjectItem(w, "type") : NULL;
  const char *type = (typ && cJSON_IsString(typ)) ? typ->valuestring : NULL;
  char *authors = oa_author_list(w);

  cJSON *data = cJSON_CreateObject();
  if (title)   cJSON_AddStringToObject(data, "title", title);
  if (doi)     cJSON_AddStringToObject(data, "doi", doi);
  if (yr && cJSON_IsNumber(yr)) cJSON_AddNumberToObject(data, "year", yr->valuedouble);
  if (authors) cJSON_AddStringToObject(data, "authors", authors);
  if (type)    cJSON_AddStringToObject(data, "type", type);
  if (cc && cJSON_IsNumber(cc)) cJSON_AddNumberToObject(data, "cited_by_count", cc->valuedouble);
  if (id)      cJSON_AddStringToObject(data, "openalex_id", id);
  cJSON_AddStringToObject(data, "source", "OpenAlex");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "OPENALEX");
  cJSON_AddStringToObject(props, "source", "OpenAlex");
  if (doi) cJSON_AddStringToObject(props, "doi", doi);
  if (yr && cJSON_IsNumber(yr)) cJSON_AddNumberToObject(props, "year", yr->valuedouble);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = id ? id : (doi ? doi : title);
  it.title           = title ? title : id;
  it.summary         = authors;
  it.body            = bj;
  it.author          = authors;
  it.lang            = "en";
  it.published_at    = pub;
  it.link            = doi ? doi : id;      /* real resolvable URL from the API */
  it.record_type     = "openalex-work";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"OPENALEX\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(authors);
  return rc >= 0 ? 1 : 0;
}

/* one author → one intel_item. Returns 1 if emitted. */
static int emit_author(intel_sink *sink, const cJSON *a) {
  if (!a) return 0;
  const char *name = jo_sv(a, "display_name");
  const char *id   = jo_sv(a, "id");
  if (!name && !id) return 0;

  const char *orcid = jo_sv(a, "orcid");
  const cJSON *wc = cJSON_GetObjectItem(a, "works_count");
  const cJSON *cc = cJSON_GetObjectItem(a, "cited_by_count");
  const cJSON *hint = cJSON_GetObjectItem(a, "last_known_institution");
  const char *inst = hint ? jo_sv(hint, "display_name") : NULL;

  cJSON *data = cJSON_CreateObject();
  if (name)  cJSON_AddStringToObject(data, "name", name);
  if (orcid) cJSON_AddStringToObject(data, "orcid", orcid);
  if (inst)  cJSON_AddStringToObject(data, "institution", inst);
  if (wc && cJSON_IsNumber(wc)) cJSON_AddNumberToObject(data, "works_count", wc->valuedouble);
  if (cc && cJSON_IsNumber(cc)) cJSON_AddNumberToObject(data, "cited_by_count", cc->valuedouble);
  if (id)    cJSON_AddStringToObject(data, "openalex_id", id);
  cJSON_AddStringToObject(data, "source", "OpenAlex");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "OPENALEX");
  cJSON_AddStringToObject(props, "source", "OpenAlex");
  if (orcid) cJSON_AddStringToObject(props, "orcid", orcid);
  if (inst)  cJSON_AddStringToObject(props, "institution", inst);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = id ? id : name;
  it.title           = name ? name : id;
  it.summary         = inst;
  it.body            = bj;
  it.lang            = "en";
  it.link            = orcid ? orcid : id;
  it.record_type     = "openalex-author";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"OPENALEX\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int fetch_and_emit(const source_ctx *ctx, intel_sink *sink,
                          const char *url, int is_author) {
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (mailto:osint@example.org)",
                         NULL };
  char *body = jo_get(ctx, url, hdrs, "openalex");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    cJSON *r;
    cJSON_ArrayForEach(r, results)
      emitted += is_author ? emit_author(sink, r) : emit_work(sink, r);
  }
  cJSON_Delete(root);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  /* works search — always */
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.openalex.org/works?search=%s&per_page=15", enc);
  int emitted = fetch_and_emit(ctx, sink, url, 0);

  /* person-shaped entity → also search authors */
  int person = strchr(q, ' ') && !strchr(q, '@');
  for (const char *p = q; person && *p; p++)
    if (isdigit((unsigned char)*p)) person = 0;
  if (person) {
    snprintf(url, sizeof url,
      "https://api.openalex.org/authors?search=%s&per_page=10", enc);
    emitted += fetch_and_emit(ctx, sink, url, 1);
  }

  free(enc);
  fprintf(stderr, "[openalex] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def openalex_def = {
  .id = "OPENALEX", .collector = "osint",
  .name = "OpenAlex Scholarly Search", .name_ja = "OpenAlex 学術検索",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "https://api.openalex.org",
  .description = "OpenAlex — open catalog of scholarly works & authors (keyless): title, DOI, year, authors",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(openalex_def)
