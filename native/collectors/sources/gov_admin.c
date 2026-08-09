/* collectors/osint/sources/gov_admin.c
 * OSINT services — administrative transparency & licensing registries.
 * Server-rendered listing pages → real anchor scrape; one run() dispatches on
 * ctx->source_id. Entity (when given) filters anchors by keyword. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

typedef struct { const char *id, *url, *base, *service, *rectype; } scrape_row;

static const scrape_row ROWS[] = {
  { "EGOV_PUBLIC_COMMENT",
    "https://public-comment.e-gov.go.jp/contents/public-comments",
    "https://public-comment.e-gov.go.jp", "e-Gov Public Comment", "public-comment" },
  { "ADVISORY_COUNCILS",
    "https://www.cao.go.jp/council/index.html",
    "https://www.cao.go.jp", "Advisory Councils", "advisory-council" },
  { "BUSINESS_LICENSES",
    "https://ifeats.mhlw.go.jp/eats/FD01000000.action",
    "https://ifeats.mhlw.go.jp", "Business Licenses", "business-license" },
  { "WASTE_HAULERS",
    "https://www2.sanpainet.or.jp/zyoho/zyoho_kensaku.php",
    "https://www2.sanpainet.or.jp", "Industrial Waste Haulers", "waste-hauler" },
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

#include "_source_macros.inc"

DEF(egov_pc_def, "EGOV_PUBLIC_COMMENT", "e-Gov Public Comment", "e-Gov パブリックコメント",
    "government", "https://public-comment.e-gov.go.jp/", "Policy consultations & organizational submitters");
DEF(council_def, "ADVISORY_COUNCILS", "Advisory Council Rosters", "審議会・委員会 委員名簿",
    "government", "https://www.cao.go.jp/", "Who advises each ministry (influence mapping)");
DEF(license_def, "BUSINESS_LICENSES", "Business Licenses", "営業許可 (食品衛生ほか)",
    "government", "https://ifeats.mhlw.go.jp/", "Restaurant/lodging/etc. licensed premises + addresses");
DEF(waste_def, "WASTE_HAULERS", "Industrial Waste Haulers", "産業廃棄物処理業者 名簿",
    "government", "https://www.sanpainet.or.jp/", "Licensed industrial-waste haulers (entity pivot)");
