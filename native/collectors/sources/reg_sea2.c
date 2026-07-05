/* collectors/sources/reg_sea2.c
 * OSINT service — SEA2_REGISTRY. On-demand entity pivot (ctx->entity = a
 * company / business name) fanned out across ~10 REAL public company
 * registries of South-East & South Asia:
 *   MY SSM e-Search, TH DBD DataWarehouse, PH SEC, VN dangkykinhdoanh,
 *   ID AHU + OSS, PK SECP, BD RJSC, LK Dept. of Registrar of Companies,
 *   KH MoC Business Registration, MM DICA MyCO.
 *
 * For each row we build the registry's own server-rendered search URL with the
 * %-encoded entity and fetch it via jo_emit_anchors — every emitted intel_item
 * is a REAL anchor (href + visible text) extracted from the live results page.
 * Nothing is synthesized: registries that are JS-only, anti-bot, or POST-only
 * simply yield 0 for that row (honest empty), which is expected and fine. We
 * NEVER emit a constructed search URL as a record — only extracted anchors.
 *
 * Guardrails so one query can't fan out forever:
 *   - per-registry cap  SEA2_PER_REG   (3)
 *   - total cap         SEA2_TOTAL_MAX (30)
 * Keyless, free_tier=1. One id, one run(). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define SEA2_PER_REG   3
#define SEA2_TOTAL_MAX 30

/* One registry search portal. `url_tmpl` has exactly one %s for the encoded
 * query. `href_must` is a substring an emittable result href must contain (to
 * skip nav/footer chrome); NULL = accept any. `base` is prepended to
 * root-relative hrefs. */
typedef struct {
  const char *name;        /* human label (also the emitted "service")      */
  const char *cc;          /* ISO country code                              */
  const char *url_tmpl;    /* search URL template, one %s for encoded query */
  const char *href_must;   /* result-link substring filter, or NULL         */
  const char *base;        /* base for root-relative hrefs                   */
} sea2_reg;

/* ~10 real public company-registry search portals. Search paths reflect each
 * registry's known public search endpoint; where the site is a JS SPA /
 * anti-bot / POST-only, we still point at the correct canonical search path
 * and let it honest-empty rather than invent a fake query param. */
static const sea2_reg REGS[] = {
  /* Malaysia — SSM e-Info company search */
  { "MY SSM e-Search", "MY",
    "https://www.ssm-einfo.my/index.php?r=site/searchResult&keyword=%s",
    NULL, "https://www.ssm-einfo.my", },
  /* Thailand — DBD DataWarehouse juristic-person search */
  { "TH DBD DataWarehouse", "TH",
    "https://datawarehouse.dbd.go.th/searchJuristicInfo?keyword=%s",
    NULL, "https://datawarehouse.dbd.go.th", },
  /* Philippines — SEC Express company search */
  { "PH SEC Express System", "PH",
    "https://esearch.sec.gov.ph/api/company?searchKey=%s",
    NULL, "https://esearch.sec.gov.ph", },
  /* Vietnam — National Business Registration Portal (dangkykinhdoanh) */
  { "VN National Business Registration", "VN",
    "https://dangkykinhdoanh.gov.vn/vn/Pages/Trangchu.aspx?keyword=%s",
    NULL, "https://dangkykinhdoanh.gov.vn", },
  /* Indonesia — Ditjen AHU legal-entity (PT) profile search */
  { "ID AHU Legal Entity", "ID",
    "https://ahu.go.id/pencarian/profil-pt?nama=%s",
    NULL, "https://ahu.go.id", },
  /* Indonesia — OSS business-licensing search */
  { "ID OSS Business Licensing", "ID",
    "https://oss.go.id/informasi/pencarian?keyword=%s",
    NULL, "https://oss.go.id", },
  /* Pakistan — SECP eServices company-name search */
  { "PK SECP eServices", "PK",
    "https://eservices.secp.gov.pk/eServices/NameSearch.jsp?companyName=%s",
    NULL, "https://eservices.secp.gov.pk", },
  /* Bangladesh — RJSC (Registrar of Joint Stock Companies) entity search */
  { "BD RJSC Entity Search", "BD",
    "https://app.roc.gov.bd:7781/psp/searchAction?searchText=%s",
    NULL, "https://app.roc.gov.bd:7781", },
  /* Sri Lanka — Dept. of the Registrar of Companies e-ROC name search */
  { "LK Registrar of Companies", "LK",
    "https://eroc.drc.gov.lk/nameSearch?name=%s",
    NULL, "https://eroc.drc.gov.lk", },
  /* Cambodia — MoC Business Registration search */
  { "KH MoC Business Registration", "KH",
    "https://www.businessregistration.moc.gov.kh/cambodia-master/service/search.html?q=%s",
    NULL, "https://www.businessregistration.moc.gov.kh", },
  /* Myanmar — DICA MyCO online company registry search */
  { "MM DICA MyCO", "MM",
    "https://www.myco.dica.gov.mm/Corp/EntitySearchResult.aspx?searchText=%s",
    NULL, "https://www.myco.dica.gov.mm", },
};
static const int NREGS = (int)(sizeof(REGS) / sizeof(REGS[0]));

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);   /* UTF-8 safe %-encoding */
  if (!enc) return -1;

  int total = 0;
  for (int i = 0; i < NREGS && total < SEA2_TOTAL_MAX; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    const sea2_reg *r = &REGS[i];

    char url[1200];
    snprintf(url, sizeof url, r->url_tmpl, enc);

    char tag[64];
    snprintf(tag, sizeof tag, "sea2_reg[%s]", r->cc);

    int remaining = SEA2_TOTAL_MAX - total;
    int cap = remaining < SEA2_PER_REG ? remaining : SEA2_PER_REG;

    int n = jo_emit_anchors(ctx, sink, url, r->href_must, r->name,
                            "sea2-company", r->base, q, cap, tag);
    if (n > 0) total += n;
  }

  free(enc);
  fprintf(stderr, "[sea2_registry] emitted %d across %d portals\n", total, NREGS);
  return 0;   /* honest empty is not an error */
}

static const source_def reg_sea2_def = {
  .id = "SEA2_REGISTRY", .collector = "osint",
  .name = "SE/South Asia Company Registries", .name_ja = "東南・南アジア企業登記",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/reg_sea2",
  .description = "Fan-out entity search across ~10 SE/South Asian company registries (MY/TH/PH/VN/ID/PK/BD/LK/KH/MM; keyless anchor-scrape; JS-only portals honest-empty)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg_sea2_def)
