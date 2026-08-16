/* collectors/infrastructure/sources/dam_water_level.c
 * MLIT 水文・水質データベース — dam storage/discharge.
 *
 * HISTORY, because the previous shape was the exact failure the house rules
 * exist to catch. This was a faithful port of damWaterLevel.js: it fetched
 * `DspDamData.exe` with NO parameters, `free()`d the body unread, and emitted
 * an empty feature array — then returned 0, which the scheduler logs as a
 * successful run. 144 requests a day, guaranteed zero rows, and in fetch_log,
 * /api/status and anomaly detection it was indistinguishable from a dam
 * network that genuinely had nothing to report. A silent empty that cost a
 * request is worse than an error, because it looks like a clean result.
 *
 * The endpoint is NOT dead. Probed 2026-08-16:
 *
 *   DspDamData.exe                       -> 200, 173b, "パラメータに誤りがあります。"
 *   DspDamData.exe?KIND=1&ID=1368080700010&BGNDATE=20260814&ENDDATE=20260816
 *                                        -> 200, 2.5KB, 任意期間ダム諸量検索結果:
 *                                           観測所記号 1368080700010,
 *                                           早明浦ダム（機構）, 水系 吉野川
 *
 * So the contract is known and works; what is missing is (a) the 観測所記号
 * station list, a 15-digit namespace distinct from 川の防災情報's 13-digit
 * obsrvtnPointFullCode, and (b) an EUC-JP HTML table parser. Both are real
 * work (see docs/jp-river-flood-sources.md, same blocker as the water-level
 * series) and neither is done here.
 *
 * Until they are, this source does NOT spend a request to throw the answer
 * away. It reports its own state as data — one record naming the endpoint,
 * the proven-working URL form and what is missing — the same way a truncation
 * shortfall is reported as a record rather than a log line nobody reads. That
 * makes it visible to the operator and mechanically sweepable, instead of
 * hiding inside a fake green run. */
#include "source.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdlib.h>

#define DAM_BASE "http://www1.river.go.jp/cgi-bin/DspDamData.exe"

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx;
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "status", "not_implemented");
  cJSON_AddStringToObject(props, "endpoint", DAM_BASE);
  cJSON_AddStringToObject(props, "endpoint_state",
      "live; returns 任意期間ダム諸量検索結果 when given KIND/ID/BGNDATE/ENDDATE");
  cJSON_AddStringToObject(props, "proven_url_form",
      DAM_BASE "?KIND=1&ID=<15-digit 観測所記号>&BGNDATE=YYYYMMDD&ENDDATE=YYYYMMDD");
  cJSON_AddStringToObject(props, "missing",
      "the 観測所記号 station list (15-digit, distinct from the 13-digit "
      "obsrvtnPointFullCode used by 川の防災情報) and an EUC-JP HTML table parser");
  cJSON_AddStringToObject(props, "tracked_in", "docs/jp-river-flood-sources.md");
  cJSON_AddStringToObject(props, "records_available", "unknown-until-implemented");
  cJSON_AddNumberToObject(props, "records_used", 0);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {
    .remote_key   = "collector-not-implemented",
    .title        = "Dam water levels — collector not implemented",
    .summary      = "MLIT 水文・水質DB dam data is reachable but this collector "
                    "does not yet parse it; no request is spent until it can.",
    .link         = DAM_BASE,
    .record_type  = "collector-status-notice",
    .sub_source_id = "dam-water-level",
    .properties_json = pj ? pj : "{}",
    .lang         = "ja",
  };
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc < 0 ? -1 : 0;
}

static const source_def dam_water_level_def = {
  .id = "dam-water-level", .collector = "infrastructure",
  .name = "Dam Water Levels", .name_ja = "\xe3\x83\x80\xe3\x83\xa0\xe8\xb2\xaf\xe6\xb0\xb4\xe4\xbd\x8d",
  /* Was 600s = 144 requests/day for a guaranteed-empty result. Daily until it
   * is implemented: the notice only needs to be present, not re-stated. */
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(dam_water_level_def)
