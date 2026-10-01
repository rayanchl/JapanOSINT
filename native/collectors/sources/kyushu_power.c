/* collectors/sources/kyushu_power.c
 * 九州電力送配電 の denki-yoho (電力需給) 日次CSV → the current actual demand as ONE
 * map point. Parsing, URL dating and the intel envelope live in the shared
 * denki_yoho.h; this file is only the utility's configuration.
 *
 * The previous static "current" CSV URL 404s — every TSO moved to a
 * date-stamped daily file. See denki_yoho.h for the full list of what was
 * broken here (including load_mw being a clock minute). */
#include "denki_yoho.h"

static const denki_cfg CFG = {
  .source_id   = "kyushu-power",
  .meta_src    = "kyushu_power",
  .operator_ja = "九州電力送配電",
  .area_code   = "09",
  .url_fmt     = "https://www.kyuden.co.jp/td_power_usages/csv/juyo-hourly-%s.csv",
  .lat = 33.5902, .lon = 130.4017
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  return denki_run(ctx, sink, &CFG);
}

static const source_def kyushu_power_def = {
  .id = "kyushu-power", .collector = "infrastructure",
  .name = "Kyushu Electric Power", .name_ja = "九州電力 電力使用状況",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(kyushu_power_def)
