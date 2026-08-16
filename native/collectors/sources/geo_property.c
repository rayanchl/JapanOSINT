/* collectors/osint/sources/geo_property.c
 * OSINT services — land, parcels & property.
 *   • Cadastral parcels — 法務省 登記所備付地図 (free G-spatial dataset index).
 *   • chikamap          — 全国地価マップ (固定資産税評価額) index.
 *   • rosenka           — 国税庁 相続税路線価 prefecture index.
 *   • GSI historical    — 旧版地図・空中写真 index.
 *   • Real-property registry — 登記情報提供サービス (paid; TOUKI_API_KEY).
 *   • Zenrin jūtaku-chizu    — building-level maps (paid; ZENRIN_API_KEY).
 * Real fetch or honest empty; paid sources gate on their key. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

typedef struct { const char *id, *url, *base, *service, *rectype; } scrape_row;
static const scrape_row ROWS[] = {
  { "CADASTRAL_PARCELS", "https://front.geospatial.jp/moj-chizu-shp-download/",
    "https://front.geospatial.jp", "Cadastral Parcels", "cadastral-dataset" },
  { "CHIKAMAP", "https://www.chikamap.jp/chikamap/Portal",
    "https://www.chikamap.jp", "Chikamap Land Values", "assessed-land-value" },
  { "ROSENKA", "https://www.rosenka.nta.go.jp/",
    "https://www.rosenka.nta.go.jp", "Inheritance-tax Roadside Values", "rosenka" },
  { "GSI_HISTORICAL", "https://mapps.gsi.go.jp/maplibSearch.do",
    "https://mapps.gsi.go.jp", "GSI Historical Maps/Aerial", "historical-imagery" },
};
static int run_scrape(const source_ctx *ctx, intel_sink *sink) {
  for (size_t i = 0; i < sizeof ROWS / sizeof ROWS[0]; i++) {
    if (strcmp(ctx->source_id, ROWS[i].id) != 0) continue;
    const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
    jo_emit_anchors(ctx, sink, ROWS[i].url, NULL, ROWS[i].service,
                    ROWS[i].rectype, ROWS[i].base, q, 30, ROWS[i].id);
    return 0;
  }
  return -1;
}

static int run_touki(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  if (!jo_env("TOUKI_API_KEY")) { fprintf(stderr, "[touki] gated (no TOUKI_API_KEY, paid)\n"); return 0; }
  fprintf(stderr, "[touki] key present but endpoint not provisioned\n");
  return 0;
}
static int run_zenrin(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  if (!jo_env("ZENRIN_API_KEY")) { fprintf(stderr, "[zenrin] gated (no ZENRIN_API_KEY, paid)\n"); return 0; }
  fprintf(stderr, "[zenrin] key present but endpoint not provisioned\n");
  return 0;
}

#include "_source_macros.inc"

DEFR(cadastral_def, "CADASTRAL_PARCELS", "Cadastral Parcels", "登記所備付地図データ", run_scrape,
     "geospatial", "dataset", "https://front.geospatial.jp/", "Nationwide cadastral parcel polygons (free MOJ release)", 1);
DEFR(chikamap_def, "CHIKAMAP", "Chikamap Land Values", "全国地価マップ", run_scrape,
     "economy", "scraped", "https://www.chikamap.jp/", "Parcel-level assessed land values (固定資産税評価額)", 1);
DEFR(rosenka_def, "ROSENKA", "Inheritance-tax Roadside Values", "相続税路線価", run_scrape,
     "economy", "scraped", "https://www.rosenka.nta.go.jp/", "Inheritance-tax roadside land values by prefecture", 1);
DEFR(gsihist_def, "GSI_HISTORICAL", "GSI Historical Maps", "国土地理院 旧版地図・空中写真", run_scrape,
     "geospatial", "scraped", "https://mapps.gsi.go.jp/", "Historical maps & aerial photography for change detection", 1);
DEFR(touki_def, "REALPROPERTY_REGISTRY", "Real-property Registry", "不動産登記情報提供サービス", run_touki,
     "economy", "api", "https://www.touki.or.jp/", "Real-property ownership, mortgages, liens (paid; needs TOUKI_API_KEY)", 0);
DEFR(zenrin_def, "ZENRIN_JUTAKU", "Zenrin Residential Maps", "ゼンリン住宅地図", run_zenrin,
     "geospatial", "api", "https://www.zenrin.co.jp/", "Building-level maps with occupant names (paid; needs ZENRIN_API_KEY)", 0);
