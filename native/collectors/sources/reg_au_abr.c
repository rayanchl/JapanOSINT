/* collectors/sources/reg_au_abr.c
 * OSINT service — Australian Business Register (ABR) ABN Lookup. Entity pivot
 * (company / business name). Calls the public ABR name-matching web service:
 *   GET https://abr.business.gov.au/json/MatchingNames.aspx?name=<entity>&guid=<GUID>
 * The GUID is a free, self-service ABR Web Services key — gated on ABR_GUID
 * (no GUID → honest empty, exactly like gbizinfo). The endpoint replies in
 * JSONP (callback({...})); we strip the wrapper, then emit one intel_item per
 * matched entity with its ABN, name, state, postcode and status. No matches /
 * fetch failure → emits nothing. Nothing here is synthesized. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Strip a JSONP callback wrapper `ident({...})` down to the inner `{...}`.
 * Returns a pointer into `body` at the first '{' and NUL-terminates at the
 * matching final '}'. If no wrapper is present, returns body unchanged. */
static char *jo_strip_jsonp(char *body) {
  if (!body) return NULL;
  char *lb = strchr(body, '{');
  char *rb = strrchr(body, '}');
  if (!lb || !rb || rb < lb) return body;
  rb[1] = 0;
  return lb;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *guid = jo_env("ABR_GUID");
  if (!guid) { fprintf(stderr, "[au_abr] gated (no ABR_GUID)\n"); return 0; }

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[768];
  snprintf(url, sizeof url,
    "https://abr.business.gov.au/json/MatchingNames.aspx?name=%s&guid=%s",
    enc, guid);
  free(enc);

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "au_abr");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(jo_strip_jsonp(body));
  free(body);
  if (!root) return 0;

  int emitted = 0;
  cJSON *names = cJSON_GetObjectItem(root, "Names");
  cJSON *n = NULL;
  cJSON_ArrayForEach(n, names) {
    const char *abn   = jo_sv(n, "Abn");
    const char *name  = jo_sv(n, "Name");
    const char *state = jo_sv(n, "State");
    const char *post  = jo_sv(n, "Postcode");
    const char *stat  = jo_sv(n, "AbnStatus");
    if (!abn && !name) continue;

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "ABR ABN Lookup");
    cJSON_AddStringToObject(props, "source", "Australian Business Register");
    if (abn)   cJSON_AddStringToObject(props, "abn", abn);
    if (state) cJSON_AddStringToObject(props, "state", state);
    if (post)  cJSON_AddStringToObject(props, "postcode", post);
    if (stat)  cJSON_AddStringToObject(props, "status", stat);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    /* human-readable summary: "<state> <postcode> — <status>" */
    char summary[256] = {0};
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             state ? state : "", (state && post) ? " " : "", post ? post : "",
             (stat && (state || post)) ? " — " : "", stat ? stat : "");

    char link[128] = {0};
    if (abn) snprintf(link, sizeof link,
      "https://abr.business.gov.au/ABN/View?abn=%s", abn);

    intel_item it = {0};
    it.remote_key      = abn ? abn : name;
    it.title           = name ? name : abn;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "en";
    it.link            = link[0] ? link : NULL;
    it.record_type     = "abr-abn";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"registry\",\"AU_ABR\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[au_abr] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def au_abr_def = {
  .id = "AU_ABR", .collector = "osint",
  .name = "Australia ABR (ABN Lookup)", .name_ja = "豪州 ABR 事業者番号検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://abr.business.gov.au/",
  .description = "Australian Business Register name match — ABN, name, state, postcode, status (needs free ABR_GUID)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(au_abr_def)
