/* collectors/sources/okinawa_power.c
 * 沖縄電力 の denki-yoho (電力需給) 日次CSV → the current actual demand as ONE
 * map point. Parsing, URL dating and the intel envelope live in the shared
 * denki_yoho.h; this file is only the utility's configuration.
 *
 * The previous static "current" CSV URL 404s — every TSO moved to a
 * date-stamped daily file. See denki_yoho.h for the full list of what was
 * broken here (including load_mw being a clock minute). */
#include "denki_yoho.h"

static const denki_cfg CFG = {
  .source_id   = "okinawa-power",
  .meta_src    = "okinawa_power",
  .operator_ja = "沖縄電力",
  .area_code   = "10",
  .url_fmt     = "https://www.okiden.co.jp/denki2/juyo_10_%s.csv",
  .lat = 26.2124, .lon = 127.6792
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  return denki_run(ctx, sink, &CFG);
}

static const source_def okinawa_power_def = {
  .id = "okinawa-power", .collector = "infrastructure",
  .name = "Okinawa Electric Power", .name_ja = "沖縄電力 電力使用状況",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(okinawa_power_def)
