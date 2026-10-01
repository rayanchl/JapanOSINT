/* collectors/sources/tohoku_power.c
 * 東北電力ネットワーク の denki-yoho (電力需給) 日次CSV → the current actual demand as ONE
 * map point. Parsing, URL dating and the intel envelope live in the shared
 * denki_yoho.h; this file is only the utility's configuration.
 *
 * The previous static "current" CSV URL 404s — every TSO moved to a
 * date-stamped daily file. See denki_yoho.h for the full list of what was
 * broken here (including load_mw being a clock minute). */
#include "denki_yoho.h"

static const denki_cfg CFG = {
  .source_id   = "tohoku-power",
  .meta_src    = "tohoku_power",
  .operator_ja = "東北電力ネットワーク",
  .area_code   = "02",
  .url_fmt     = "https://setsuden.nw.tohoku-epco.co.jp/common/demand/juyo_02_%s.csv",
  .lat = 38.2682, .lon = 140.8694
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  return denki_run(ctx, sink, &CFG);
}

static const source_def tohoku_power_def = {
  .id = "tohoku-power", .collector = "infrastructure",
  .name = "Tohoku Electric Power", .name_ja = "東北電力 電力使用状況",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(tohoku_power_def)
