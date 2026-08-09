/* collectors/sources/av_uk_aaib.c
 * UK Air Accidents Investigation Branch reports, via the official GOV.UK
 * Search API.
 * Endpoint: https://www.gov.uk/api/search.json
 *             ?filter_content_store_document_type=aaib_report
 *             &count=50&order=-public_timestamp
 *             &fields=title,link,public_timestamp,description
 *
 * Emits per report, all parsed from `results[]`: title (which carries the
 * aircraft type and registration, so every row is already tail-number
 * pivotable), description (the one-line circumstance summary), the absolute
 * report URL, public_timestamp and the document's `_id`.
 *
 * TRAP (quoted from the work list): "`link` is a site-relative path: prefix
 * https://www.gov.uk."
 *
 * NO COORDINATES (R2): the search response has none, and the report bodies
 * give locations only as free text. has_geo is always 0 — "near <village>" in
 * a title is deliberately not geocoded.
 *
 * R3: fetch/parse failure → -1; a results array with nothing new → 0.
 *
 * Licence: Crown copyright, Open Government Licence v3.0 — free reuse with
 * attribution. The GOV.UK Search API is public, documented and keyless.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_common.inc"

#define AAIB_URL \
  "https://www.gov.uk/api/search.json?filter_content_store_document_type=" \
  "aaib_report&count=50&order=-public_timestamp&fields=title,link," \
  "public_timestamp,description"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Accept: application/json", AV_UA_HDR, NULL };
  char *body = jo_get(ctx, AAIB_URL, hdrs, "uk-aaib-reports");
  if (!body) { fprintf(stderr, "[uk-aaib-reports] fetch failed\n"); return -1; }
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) { fprintf(stderr, "[uk-aaib-reports] unparseable JSON\n"); return -1; }
  cJSON *res = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(res)) {
    cJSON_Delete(doc);
    fprintf(stderr, "[uk-aaib-reports] no results array\n");
    return -1;
  }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, res) {
    const char *title = jo_sv(r, "title");
    const char *link  = jo_sv(r, "link");
    if (!title || !link) continue;
    const char *desc  = jo_sv(r, "description");
    const char *when  = jo_sv(r, "public_timestamp");
    const char *docid = jo_sv(r, "_id");

    char url[600];
    if (strncmp(link, "http", 4) == 0) snprintf(url, sizeof url, "%s", link);
    else snprintf(url, sizeof url, "https://www.gov.uk%s", link);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "title", title);
    if (desc) cJSON_AddStringToObject(data, "description", desc);
    cJSON_AddStringToObject(data, "url", url);
    if (when) cJSON_AddStringToObject(data, "public_timestamp", when);
    if (docid) cJSON_AddStringToObject(data, "document_id", docid);
    cJSON_AddStringToObject(data, "source", "UK AAIB via GOV.UK Search API");
    char *bj = cJSON_PrintUnformatted(data);

    cJSON *props = cJSON_Duplicate(data, 1);
    cJSON_AddStringToObject(props, "record_type", "air-accident-report");
    cJSON_AddStringToObject(props, "geometry_note",
      "no coordinates in the API or the report body; location text is "
      "deliberately not geocoded");
    cJSON_AddStringToObject(props, "attribution",
      "Crown copyright, Open Government Licence v3.0");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);
    cJSON_Delete(data);

    intel_item it = {0};
    it.remote_key      = docid ? docid : link;
    it.title           = title;
    it.summary         = desc;
    it.body            = bj;
    it.link            = url;
    it.lang            = "en";
    it.published_at    = when;
    it.record_type     = "air-accident-report";
    it.properties_json = pj;
    it.tags_json       = "[\"aviation\",\"accident\",\"AAIB\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj); free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[uk-aaib-reports] emitted %d\n", n);
  return 0;
}

static const source_def av_uk_aaib_def = {
  .id = "uk-aaib-reports", .collector = "transport",
  .name = "UK AAIB Air Accident Investigation Reports",
  .update_interval_sec = 3600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://www.gov.uk/api/search.json?filter_content_store_document_type=aaib_report",
  .description = "Every Air Accidents Investigation Branch report as it publishes, newest first, via the official GOV.UK Search API. Aircraft type and registration are in the title, so each row is already a tail-number-pivotable accident record.",
  .license = "Crown copyright, Open Government Licence v3.0 — free reuse with attribution. GOV.UK Search API is public and keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_uk_aaib_def)
