/* collectors/government/sources/fsa_crypto_exchanges.c
 * Port of server/src/collectors/fsaCryptoExchanges.js — fetchHead
 * reachability probe → ONE intel item (uid fsa-crypto-exchanges|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.fsa.go.jp/policy/virtual_currency02/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['crypto','fsa','exchange', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("crypto"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("fsa"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("exchange"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "金融庁");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid fsa-crypto-exchanges|portal */
  it.title        = "FSA registered crypto exchange list";
  it.summary      = "Financial Services Agency — registered crypto-asset exchange service providers";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[fsa-crypto-exchanges] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def fsa_crypto_exchanges_def = {
  .id = "fsa-crypto-exchanges", .collector = "government",
  .name = "FSA registered crypto exchanges", .name_ja = "金融庁 暗号資産業者",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(fsa_crypto_exchanges_def)
