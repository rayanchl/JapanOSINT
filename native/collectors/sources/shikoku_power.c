/* collectors/sources/shikoku_power.c
 * 四国電力送配電 の denki-yoho (電力需給) 日次CSV → the current actual demand as ONE
 * map point. Parsing, URL dating and the intel envelope live in the shared
 * denki_yoho.h; this file is only the utility's configuration.
 *
 * The previous static "current" CSV URL 404s — every TSO moved to a
 * date-stamped daily file. See denki_yoho.h for the full list of what was
 * broken here (including load_mw being a clock minute). */
#include "denki_yoho.h"

static const denki_cfg CFG = {
  .source_id   = "shikoku-power",
  .meta_src    = "shikoku_power",
  .operator_ja = "四国電力送配電",
  .area_code   = "08",
  .url_fmt     = "https://www.yonden.co.jp/nw/denkiyoho/juyo_08_%s.csv",
  .lat = 34.3401, .lon = 134.0434
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  return denki_run(ctx, sink, &CFG);
}

static const source_def shikoku_power_def = {
  .id = "shikoku-power", .collector = "infrastructure",
  .name = "Shikoku Electric Power", .name_ja = "四国電力 電力使用状況",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(shikoku_power_def)
