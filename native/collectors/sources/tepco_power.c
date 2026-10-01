/* collectors/sources/tepco_power.c
 * 東京電力パワーグリッド の denki-yoho (電力需給) 日次CSV → the current actual demand as ONE
 * map point. Parsing, URL dating and the intel envelope live in the shared
 * denki_yoho.h; this file is only the utility's configuration.
 *
 * The previous static "current" CSV URL 404s — every TSO moved to a
 * date-stamped daily file. See denki_yoho.h for the full list of what was
 * broken here (including load_mw being a clock minute). */
#include "denki_yoho.h"

static const denki_cfg CFG = {
  .source_id   = "tepco-power",
  .meta_src    = "tepco_power",
  .operator_ja = "東京電力パワーグリッド",
  .area_code   = "03",
  .url_fmt     = "https://www.tepco.co.jp/forecast/html/images/juyo_03_%s.csv",
  .lat = 35.6762, .lon = 139.6503
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  return denki_run(ctx, sink, &CFG);
}

static const source_def tepco_power_def = {
  .id = "tepco-power", .collector = "infrastructure",
  .name = "TEPCO Power Usage", .name_ja = "東京電力 電力使用状況",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(tepco_power_def)
