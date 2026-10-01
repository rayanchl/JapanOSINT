/* collectors/sources/hokuriku_power.c
 * 北陸電力送配電 の denki-yoho (電力需給) 日次CSV → the current actual demand as ONE
 * map point. Parsing, URL dating and the intel envelope live in the shared
 * denki_yoho.h; this file is only the utility's configuration.
 *
 * The previous static "current" CSV URL 404s — every TSO moved to a
 * date-stamped daily file. See denki_yoho.h for the full list of what was
 * broken here (including load_mw being a clock minute). */
#include "denki_yoho.h"

static const denki_cfg CFG = {
  .source_id   = "hokuriku-power",
  .meta_src    = "hokuriku_power",
  .operator_ja = "北陸電力送配電",
  .area_code   = "05",
  .url_fmt     = "https://www.rikuden.co.jp/nw/denki-yoho/csv/juyo_05_%s.csv",
  .lat = 36.6953, .lon = 137.2113
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  return denki_run(ctx, sink, &CFG);
}

static const source_def hokuriku_power_def = {
  .id = "hokuriku-power", .collector = "infrastructure",
  .name = "Hokuriku Electric Power", .name_ja = "北陸電力 電力使用状況",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(hokuriku_power_def)
