/* collectors/osint/sources/gov_enforcement.c
 * OSINT services — regulators naming companies/products: approvals, sanctions,
 * penalties, recalls. All server-rendered .go.jp listing pages → real anchor
 * scrape (links/titles are extracted from the live page; honest empty if the
 * page is shaped differently). One run() dispatches on ctx->source_id. */
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
  { "PMDA_APPROVALS",
    "https://www.pmda.go.jp/review-services/drug-reviews/review-information/p-drugs/0010.html",
    "https://www.pmda.go.jp", "PMDA Approvals", "drug-device-approval" },
  { "MHLW_LABOR_VIOLATIONS",
    "https://www.mhlw.go.jp/stf/seisakunitsuite/bunya/koyou_roudou/roudoukijun/anzen/anzeneisei14/index.html",
    "https://www.mhlw.go.jp", "MHLW Labor Violations", "labor-violation" },
  { "MLIT_NEGATIVE_INFO",
    "https://www.mlit.go.jp/nega-inf/",
    "https://www.mlit.go.jp", "MLIT Negative Info", "sanctioned-operator" },
  { "CAA_ENFORCEMENT",
    "https://www.caa.go.jp/policies/policy/consumer_transaction/public_notice/",
    "https://www.caa.go.jp", "Consumer Affairs Enforcement", "consumer-penalty" },
  { "FSA_ENFORCEMENT",
    "https://www.fsa.go.jp/status/index.html",
    "https://www.fsa.go.jp", "FSA Enforcement", "fsa-penalty" },
  { "JFTC_ENFORCEMENT",
    "https://www.jftc.go.jp/houdou/index.html",
    "https://www.jftc.go.jp", "JFTC Enforcement", "antitrust-decision" },
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

#define DEF(SYM, ID, NAME, NAMEJA, CAT, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run_scrape, \
    .category = CAT, .type = "scraped", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

DEF(pmda_def, "PMDA_APPROVALS", "PMDA Approvals", "PMDA 承認情報", "health",
    "https://www.pmda.go.jp/", "Approved drugs/devices, applicants & recalls/safety alerts");
DEF(mhlw_def, "MHLW_LABOR_VIOLATIONS", "MHLW Labor Violations", "労働基準法令違反公表",
    "government", "https://www.mhlw.go.jp/", "Companies cited for labor-law breaches (\"black company\" list)");
DEF(mlit_neg_def, "MLIT_NEGATIVE_INFO", "MLIT Negative Info", "国交省 ネガティブ情報",
    "government", "https://www.mlit.go.jp/", "Sanctioned operators: transport/construction/realty/travel");
DEF(caa_def, "CAA_ENFORCEMENT", "Consumer Affairs Enforcement", "消費者庁 行政処分",
    "government", "https://www.caa.go.jp/", "Enforcement actions against businesses (特商法 etc.)");
DEF(fsa_enf_def, "FSA_ENFORCEMENT", "FSA Enforcement", "金融庁 行政処分事例",
    "government", "https://www.fsa.go.jp/", "FSA penalties & registration revocations naming firms");
DEF(jftc_enf_def, "JFTC_ENFORCEMENT", "JFTC Enforcement", "公取委 排除措置・課徴金",
    "government", "https://www.jftc.go.jp/", "Cartel/antitrust decisions & surcharge orders naming companies");
