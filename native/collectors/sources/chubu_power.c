/* collectors/sources/chubu_power.c
 * 中部電力パワーグリッド の denki-yoho (電力需給) 日次CSV → the current actual demand as ONE
 * map point. Parsing, URL dating and the intel envelope live in the shared
 * denki_yoho.h; this file is only the utility's configuration.
 *
 * The previous static "current" CSV URL 404s — every TSO moved to a
 * date-stamped daily file. See denki_yoho.h for the full list of what was
 * broken here (including load_mw being a clock minute). */
#include "denki_yoho.h"

static const denki_cfg CFG = {
  .source_id   = "chubu-power",
  .meta_src    = "chubu_power",
  .operator_ja = "中部電力パワーグリッド",
  .area_code   = "04",
  .url_fmt     = "https://powergrid.chuden.co.jp/denki_yoho_content_data/juyo_cepco003.csv",
  .lat = 35.1815, .lon = 136.9066
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  return denki_run(ctx, sink, &CFG);
}

static const source_def chubu_power_def = {
  .id = "chubu-power", .collector = "infrastructure",
  .name = "Chubu Electric Power", .name_ja = "中部電力 電力使用状況",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(chubu_power_def)
