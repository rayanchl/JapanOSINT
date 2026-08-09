/* collectors/sources/hokkaido_power.c
 * 北海道電力ネットワーク の denki-yoho (電力需給) 日次CSV → the current actual demand as ONE
 * map point. Parsing, URL dating and the intel envelope live in the shared
 * denki_yoho.h; this file is only the utility's configuration.
 *
 * The previous static "current" CSV URL 404s — every TSO moved to a
 * date-stamped daily file. See denki_yoho.h for the full list of what was
 * broken here (including load_mw being a clock minute). */
#include "denki_yoho.h"

static const denki_cfg CFG = {
  .source_id   = "hokkaido-power",
  .meta_src    = "hokkaido_power",
  .operator_ja = "北海道電力ネットワーク",
  .area_code   = "01",
  .url_fmt     = "https://denkiyoho.hepco.co.jp/area/data/juyo_01_%s.csv",
  .lat = 43.0642, .lon = 141.3469
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  return denki_run(ctx, sink, &CFG);
}

static const source_def hokkaido_power_def = {
  .id = "hokkaido-power", .collector = "infrastructure",
  .name = "Hokkaido Electric Power", .name_ja = "北海道電力 電力使用状況",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(hokkaido_power_def)
