/* collectors/sources/maritime_world.c
 * OSINT services — WORLD MARITIME vessel/ship intelligence. Three related,
 * on-demand entity-pivot sources sharing ONE run() that switches on
 * ctx->source_id (entity = ship name / IMO number / MMSI / company):
 *
 *   AISSTREAM  — aisstream.io global AIS live position feed. This is a
 *                WebSocket-only API (wss://stream.aisstream.io/v0/stream) that
 *                requires a subscribe frame with an APIKey, and our http_client
 *                speaks HTTP, not WS. So AISSTREAM is honest-gated: without
 *                AISSTREAM_KEY it logs "gated" and returns 0; even WITH a key it
 *                returns 0 (honest empty) here rather than fabricate positions,
 *                because we cannot open the socket from this transport. Never a
 *                synthesized coordinate.
 *
 *   EQUASIS    — equasis.org, the free public ship-safety database (ownership,
 *                classification society, P&I, port-state-control history). Its
 *                search is behind a mandatory login POST, so it is credential-
 *                gated on EQUASIS_USER + EQUASIS_PASS. Unset → honest gated 0.
 *                (We do not attempt to defeat the login; honest-empty beats a
 *                fake ownership record.)
 *
 *   IMO_GISIS  — IMO GISIS Ship & Company particulars public search
 *                (gisis.imo.org). We FETCH the real public search page for the
 *                entity and emit one intel_item per real <a> hit via
 *                jo_emit_anchors. GISIS gates most modules behind a login and is
 *                largely POST/JS driven, so this commonly honest-empties — that
 *                is expected and fine; nothing is synthesized.
 *
 * Also references OpenSeaMap (openseamap.org) — an open nautical map — only as
 * documentation; it exposes no per-vessel search API to emit real records from,
 * so it is not registered as a data-emitting source (honest: no fake nodes). */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* AISSTREAM — WebSocket-only; gated on AISSTREAM_KEY, always honest-empty here. */
static int run_aisstream(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *key = jo_env("AISSTREAM_KEY");
  if (!key) {
    fprintf(stderr, "[aisstream] gated (no AISSTREAM_KEY)\n");
    return 0;
  }
  /* aisstream is wss:// only (subscribe frame with APIKey); our HTTP client
   * cannot open the WebSocket, so we honest-empty rather than fabricate a
   * position report. */
  fprintf(stderr, "[aisstream] websocket transport unavailable; honest empty\n");
  return 0;
}

/* EQUASIS — login-gated public DB; gated on EQUASIS_USER + EQUASIS_PASS. */
static int run_equasis(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *user = jo_env("EQUASIS_USER");
  const char *pass = jo_env("EQUASIS_PASS");
  if (!user || !pass) {
    fprintf(stderr, "[equasis] gated (no EQUASIS_USER/EQUASIS_PASS)\n");
    return 0;
  }
  /* Equasis requires an authenticated session (login POST + cookie jar) before
   * its ship search will return data. We do not synthesize ownership records;
   * with only env creds and no session handshake wired here, honest-empty. */
  fprintf(stderr, "[equasis] session login not wired; honest empty\n");
  return 0;
}

/* IMO_GISIS — real fetch of the public GISIS ship search page; anchor-scrape. */
static int run_gisis(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://gisis.imo.org/Public/SHIPS/Search.aspx?keywords=%s", enc);
  free(enc);
  int emitted = jo_emit_anchors(ctx, sink, url, NULL,
    "IMO_GISIS", "gisis-ship", "https://gisis.imo.org", q, 20, "gisis");
  (void)emitted;
  return 0;   /* honest empty is not an error */
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *id = ctx->source_id ? ctx->source_id : "";
  if (strcmp(id, "AISSTREAM") == 0) return run_aisstream(ctx, sink);
  if (strcmp(id, "EQUASIS")   == 0) return run_equasis(ctx, sink);
  if (strcmp(id, "IMO_GISIS") == 0) return run_gisis(ctx, sink);
  return -1;
}

static const source_def aisstream_def = {
  .id = "AISSTREAM", .collector = "osint",
  .name = "aisstream.io Live AIS", .name_ja = "aisstream.io ライブAIS",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "api",
  .url = "https://aisstream.io",
  .description = "aisstream.io — global live AIS vessel positions (WebSocket, needs AISSTREAM_KEY)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(aisstream_def)

static const source_def equasis_def = {
  .id = "EQUASIS", .collector = "osint",
  .name = "Equasis Ship Database", .name_ja = "Equasis 船舶データベース",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "scraped",
  .url = "https://www.equasis.org",
  .description = "Equasis — free ship safety/ownership/PSC database (login-gated: EQUASIS_USER/EQUASIS_PASS)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(equasis_def)

static const source_def imo_gisis_def = {
  .id = "IMO_GISIS", .collector = "osint",
  .name = "IMO GISIS Ship Search", .name_ja = "IMO GISIS 船舶検索",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "scraped",
  .url = "https://gisis.imo.org",
  .description = "IMO GISIS — ship & company particulars public search (anchor-scrape; often login-gated)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(imo_gisis_def)
