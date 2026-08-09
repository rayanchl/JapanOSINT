/* collectors/sources/reg_sg_data.c
 * OSINT service — SG_ACRA. On-demand entity pivot against Singapore's ACRA
 * (Accounting & Corporate Regulatory Authority) register of business entities,
 * exposed as an open dataset on data.gov.sg. Keyless and LIVE — the CKAN
 * datastore_search API needs no API key for public resources.
 *
 * We query
 *   GET https://data.gov.sg/api/action/datastore_search
 *         ?resource_id=d_3f960c10fed6145404ca7b821f263b87&q=<entity>&limit=20
 * (the consolidated ACRA "Entities registered with ACRA" resource, ~2.1M rows)
 * and emit one intel_item per matched record: UEN, entity name, status.
 *
 * Only real, parsed CKAN fields are emitted (field ids confirmed against the
 * live resource: uen / entity_name / uen_status_desc / entity_type_desc /
 * uen_issue_date / reg_street_name). Fetch failure or no matches → honest
 * empty (return 0). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* one CKAN record → one intel_item. Returns 1 if emitted. */
static int emit_record(intel_sink *sink, const cJSON *r) {
  if (!cJSON_IsObject(r)) return 0;
  static const char *uen_keys[]  = { "uen", "UEN", NULL };
  static const char *name_keys[] = { "entity_name", "company_name", "name", NULL };
  static const char *stat_keys[] = { "uen_status_desc",
                                     "entity_status_description",
                                     "entity_status", "status", NULL };
  const char *uen  = jo_first_sv(r, uen_keys);
  const char *name = jo_first_sv(r, name_keys);
  const char *stat = jo_first_sv(r, stat_keys);
  if (!uen && !name) return 0;

  static const char *type_keys[] = { "entity_type_desc",
                                     "entity_type_description", NULL };
  static const char *reg_keys[]  = { "uen_issue_date",
                                     "registration_incorporation_date", NULL };
  static const char *street_keys[] = { "reg_street_name", NULL };
  const char *type   = jo_first_sv(r, type_keys);
  const char *reg    = jo_first_sv(r, reg_keys);
  const char *street = jo_first_sv(r, street_keys);

  cJSON *data = cJSON_CreateObject();
  if (name)   cJSON_AddStringToObject(data, "entity_name", name);
  if (uen)    cJSON_AddStringToObject(data, "uen", uen);
  if (stat)   cJSON_AddStringToObject(data, "status", stat);
  if (type)   cJSON_AddStringToObject(data, "entity_type", type);
  if (reg)    cJSON_AddStringToObject(data, "registered", reg);
  if (street) cJSON_AddStringToObject(data, "reg_street_name", street);
  cJSON_AddStringToObject(data, "source", "ACRA (data.gov.sg)");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SG_ACRA");
  cJSON_AddStringToObject(props, "source", "ACRA");
  if (uen)  cJSON_AddStringToObject(props, "uen", uen);
  if (stat) cJSON_AddStringToObject(props, "status", stat);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = uen ? uen : name;
  it.title           = name ? name : uen;
  it.summary         = stat;
  it.body            = bj;
  it.lang            = "en";
  it.record_type     = "acra-entity";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SG_ACRA\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* data.gov.sg CKAN resource id for the consolidated ACRA entities register
 * ("Entities registered with ACRA"). Public, keyless. Verified live. */
#define SG_ACRA_RESOURCE "d_3f960c10fed6145404ca7b821f263b87"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  char url[1024];
  snprintf(url, sizeof url,
    "https://data.gov.sg/api/action/datastore_search?resource_id=%s&q=%s&limit=20",
    SG_ACRA_RESOURCE, enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "sg_acra");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = 0;
  const cJSON *result = cJSON_GetObjectItem(root, "result");
  const cJSON *records = result ? cJSON_GetObjectItem(result, "records") : NULL;
  if (cJSON_IsArray(records)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, records) emitted += emit_record(sink, r);
  }
  cJSON_Delete(root);

  fprintf(stderr, "[sg_acra] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def reg_sg_acra_def = {
  .id = "SG_ACRA", .collector = "osint",
  .name = "Singapore ACRA Entity Lookup", .name_ja = "シンガポール ACRA 企業検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://data.gov.sg/api/action/datastore_search",
  .description = "Singapore ACRA business register via data.gov.sg (keyless): UEN, entity name, status",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg_sg_acra_def)
