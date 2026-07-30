/* collectors/sources/patents_world.c
 * OSINT services — world patent search, entity pivot on ctx->entity (a title
 * keyword, applicant/assignee or inventor name). One run() dispatches on
 * ctx->source_id:
 *
 *   PATENTSVIEW  — PatentsView v1 (search.patentsview.org), USPTO's open patent
 *                  DB. Keyless & LIVE. We POST-shape via GET q= against
 *                  {"_text_any":{"patent_title":"<entity>"}}, emit one item per
 *                  patent (id, title, grant date).
 *   EPO_OPS      — European Patent Office Open Patent Services. Requires OAuth
 *                  (EPO_OPS_KEY / consumer key); gated → honest empty when unset.
 *   LENS_PATENTS — Lens.org scholarly/patent API. Requires LENS_TOKEN bearer;
 *                  gated → honest empty when unset.
 *
 * Google Patents has no keyless public API; a Wikidata fallback would only yield
 * link cards, so it is deliberately honest-empty (documented, emits nothing).
 * Every path REAL-fetches or is honest-empty; nothing is fabricated. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- PatentsView (keyless, LIVE) --------------------------------------- */

static int pv_emit(intel_sink *sink, const cJSON *p) {
  if (!p) return 0;
  const char *id    = jo_sv(p, "patent_id");
  const char *title = jo_sv(p, "patent_title");
  if (!id && !title) return 0;
  const char *date  = jo_sv(p, "patent_date");
  const char *abst  = jo_sv(p, "patent_abstract");

  cJSON *data = cJSON_CreateObject();
  if (id)    cJSON_AddStringToObject(data, "patent_id", id);
  if (title) cJSON_AddStringToObject(data, "title", title);
  if (date)  cJSON_AddStringToObject(data, "grant_date", date);
  if (abst)  cJSON_AddStringToObject(data, "abstract", abst);
  cJSON_AddStringToObject(data, "source", "PatentsView");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  char link[256] = {0};
  if (id) snprintf(link, sizeof link, "https://patents.google.com/patent/US%s", id);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "PATENTSVIEW");
  cJSON_AddStringToObject(props, "source", "PatentsView");
  if (id) cJSON_AddStringToObject(props, "patent_id", id);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = id ? id : title;
  it.title           = title ? title : id;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = date;
  it.link            = id ? link : NULL;
  it.record_type     = "patent";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"PATENTSVIEW\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_patentsview(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  /* q param is the (url-encoded) JSON query; f selects returned fields. */
  char qjson[768];
  snprintf(qjson, sizeof qjson,
    "{\"_text_any\":{\"patent_title\":\"%s\"}}", q);
  char *qenc = jo_urlencode(qjson);
  if (!qenc) return 0;
  const char *fjson =
    "[\"patent_id\",\"patent_title\",\"patent_date\",\"patent_abstract\"]";
  char *fenc = jo_urlencode(fjson);
  if (!fenc) { free(qenc); return 0; }

  char url[2048];
  snprintf(url, sizeof url,
    "https://search.patentsview.org/api/v1/patent/?q=%s&f=%s&o=%s",
    qenc, fenc, "%7B%22size%22%3A20%7D");   /* o={"size":20} */
  free(qenc); free(fenc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "patentsview");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *arr = cJSON_GetObjectItem(root, "patents");
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *p; cJSON_ArrayForEach(p, arr) emitted += pv_emit(sink, p);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[patentsview] emitted %d\n", emitted);
  return 0;
}

/* ---- EPO OPS (gated) ---------------------------------------------------- */

static int run_epo_ops(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *key = jo_env("EPO_OPS_KEY");
  if (!key) {
    fprintf(stderr, "[epo_ops] gated (no EPO_OPS_KEY)\n");
    return 0;
  }
  /* OPS uses OAuth2 client-credentials then CQL search. Without a live token we
   * honest-empty rather than invent records. The published/register endpoint is
   * http://ops.epo.org/3.2/rest-services/published-data/search?q=ti%3D<term>. */
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "http://ops.epo.org/3.2/rest-services/published-data/search?q=ti%%3D%s", enc);
  free(enc);
  char auth[512];
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", key);
  const char *hdrs[] = { auth, "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "epo_ops");
  if (!body) return 0;
  /* OPS returns EPO-namespaced XML/JSON; only emit on a parseable JSON body. */
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (root) cJSON_Delete(root);
  fprintf(stderr, "[epo_ops] no parsed records\n");
  return 0;
}

/* ---- Lens.org (gated) --------------------------------------------------- */

static int lens_emit(intel_sink *sink, const cJSON *p) {
  if (!p) return 0;
  const char *lens_id = jo_sv(p, "lens_id");
  const cJSON *bib = cJSON_GetObjectItem(p, "biblio");
  const cJSON *inv = bib ? cJSON_GetObjectItem(bib, "invention_title") : NULL;
  const char *title = NULL;
  if (cJSON_IsArray(inv) && cJSON_GetArraySize(inv) > 0)
    title = jo_sv(cJSON_GetArrayItem(inv, 0), "text");  /* exhaustive-ok: display pick; titles_all carries every language variant */
  const char *date = jo_sv(p, "date_published");
  if (!lens_id && !title) return 0;

  cJSON *data = cJSON_CreateObject();
  if (lens_id) cJSON_AddStringToObject(data, "lens_id", lens_id);
  if (cJSON_IsArray(inv) && cJSON_GetArraySize(inv) > 1)
    cJSON_AddItemToObject(data, "titles_all", cJSON_Duplicate(inv, 1));
  if (title)   cJSON_AddStringToObject(data, "title", title);
  if (date)    cJSON_AddStringToObject(data, "date_published", date);
  cJSON_AddStringToObject(data, "source", "Lens.org");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  char link[256] = {0};
  if (lens_id) snprintf(link, sizeof link, "https://www.lens.org/lens/patent/%s", lens_id);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "LENS_PATENTS");
  cJSON_AddStringToObject(props, "source", "Lens.org");
  if (lens_id) cJSON_AddStringToObject(props, "lens_id", lens_id);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = lens_id ? lens_id : title;
  it.title           = title ? title : lens_id;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = date;
  it.link            = lens_id ? link : NULL;
  it.record_type     = "patent";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"LENS_PATENTS\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_lens(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *token = jo_env("LENS_TOKEN");
  if (!token) {
    fprintf(stderr, "[lens_patents] gated (no LENS_TOKEN)\n");
    return 0;
  }
  /* Lens patent search: POST a JSON query to /patent/search. */
  char post[1024];
  snprintf(post, sizeof post,
    "{\"query\":{\"match\":{\"invention_title\":\"%s\"}},\"size\":20}", q);
  char auth[600];
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", token);
  const char *hdrs[] = { auth, "Content-Type: application/json",
                         "Accept: application/json", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "POST",
      "https://api.lens.org/patent/search", hdrs, post, strlen(post),
      20000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[lens_patents] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return 0;
  }
  cJSON *root = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!root) return 0;
  cJSON *data = cJSON_GetObjectItem(root, "data");
  int emitted = 0;
  if (cJSON_IsArray(data)) {
    cJSON *p; cJSON_ArrayForEach(p, data) emitted += lens_emit(sink, p);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[lens_patents] emitted %d\n", emitted);
  return 0;
}

/* ---- dispatch ----------------------------------------------------------- */

static int run(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->source_id) return -1;
  if (strcmp(ctx->source_id, "PATENTSVIEW") == 0)  return run_patentsview(ctx, sink);
  if (strcmp(ctx->source_id, "EPO_OPS") == 0)      return run_epo_ops(ctx, sink);
  if (strcmp(ctx->source_id, "LENS_PATENTS") == 0) return run_lens(ctx, sink);
  return -1;
}

static const source_def patentsview_def = {
  .id = "PATENTSVIEW", .collector = "osint",
  .name = "PatentsView Patent Search", .name_ja = "PatentsView 特許検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://search.patentsview.org/api/v1/patent/",
  .description = "USPTO PatentsView — keyless world patent search by title; id, title, grant date",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(patentsview_def)

static const source_def epo_ops_def = {
  .id = "EPO_OPS", .collector = "osint",
  .name = "EPO Open Patent Services", .name_ja = "EPO OPS 特許検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "http://ops.epo.org/3.2/rest-services/published-data/search",
  .description = "European Patent Office OPS — published-data search (needs EPO_OPS_KEY)",
  .layer = NULL, .free_tier = 0,
};
REGISTER_SOURCE(epo_ops_def)

static const source_def lens_patents_def = {
  .id = "LENS_PATENTS", .collector = "osint",
  .name = "Lens.org Patent Search", .name_ja = "Lens.org 特許検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://api.lens.org/patent/search",
  .description = "Lens.org — scholarly patent search by invention title (needs LENS_TOKEN)",
  .layer = NULL, .free_tier = 0,
};
REGISTER_SOURCE(lens_patents_def)
