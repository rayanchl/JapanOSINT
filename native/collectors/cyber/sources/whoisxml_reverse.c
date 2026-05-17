/* collectors/cyber/sources/whoisxml_reverse.c
 * Port of server/src/collectors/whoisXmlReverse.js — fetchHead reachability
 * probe → ONE intel item (uid = whoisxml-reverse|portal). Key-gated by
 * WHOISXML_KEY. OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://whois.whoisxmlapi.com/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int has_key = getenv("WHOISXML_KEY") && *getenv("WHOISXML_KEY");
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['whois','reverse','whoisxml', live?'reachable':'unreachable', hasKey?'key-present':'key-missing'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("whois"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("reverse"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("whoisxml"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(has_key ? "key-present" : "key-missing"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { reachable, requires_key, has_key } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddBoolToObject(p, "reachable", live);
  cJSON_AddBoolToObject(p, "requires_key", 1);
  cJSON_AddBoolToObject(p, "has_key", has_key);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";
  it.title        = "WhoisXMLAPI — bulk / reverse WHOIS";
  it.summary      = has_key ? "Configured" : "Set WHOISXML_KEY to enable reverse-WHOIS queries";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[whoisxml-reverse] probe reachable=%d has_key=%d\n", live, has_key);
  return rc >= 0 ? 0 : -1;
}

static const source_def whoisxml_reverse_def = {
  .id = "whoisxml-reverse", .collector = "cyber",
  .name = "WhoisXMLAPI reverse / bulk", .name_ja = "WhoisXMLAPI 逆引き",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(whoisxml_reverse_def)
