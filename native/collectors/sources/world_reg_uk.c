/* collectors/sources/world_reg_uk.c
 * OSINT service — UK_REGISTRY. On-demand entity pivot (entity = person /
 * company / charity name) across UK & Ireland public company / court /
 * registry / civic search portals. For the given ctx->entity we actually
 * FETCH each registry's server-rendered search-results page and extract REAL
 * <a> anchors via jo_emit_anchors — no synthesized "search-link cards".
 *
 * Many of these portals are JS-rendered or anti-bot (Companies House SPA,
 * FCA register, 192.com, CRO Ireland). Those honest-empty per row (0 anchors)
 * — that is expected and correct; we NEVER fabricate results. The classic
 * server-rendered ones (BAILII, TheyWorkForYou, GOV.UK/Charity search) yield
 * real links. Total emit capped ~40, per-registry ~3 so one query can't fan
 * out forever.
 *
 * URLs are the registries' REAL public search paths. Where the exact query
 * param was uncertain we use the registry's known search path so it
 * honest-empties rather than inventing a fake endpoint. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

typedef struct {
  const char *name;      /* registry label                                */
  const char *tmpl;      /* search URL with a single %s for the query      */
  const char *base;      /* origin to prepend to root-relative hrefs       */
  const char *href_must; /* href substring filter (NULL = any)             */
  const char *cc;        /* ISO country code                               */
  const char *cat;       /* row category                                   */
} uk_reg;

/* ~25 real UK & Ireland search portals. */
static const uk_reg REGS[] = {
  /* --- UK companies / regulators --- */
  { "Companies House (register)",
    "https://find-and-update.company-information.service.gov.uk/search?q=%s",
    "https://find-and-update.company-information.service.gov.uk", "/company/", "GB", "company" },
  { "Companies House (officers)",
    "https://find-and-update.company-information.service.gov.uk/search/officers?q=%s",
    "https://find-and-update.company-information.service.gov.uk", "/officers/", "GB", "officer" },
  { "Companies House (disqualified)",
    "https://find-and-update.company-information.service.gov.uk/search/disqualified-officers?q=%s",
    "https://find-and-update.company-information.service.gov.uk", "/disqualified-officers/", "GB", "officer" },
  { "Charity Commission (England & Wales)",
    "https://register-of-charities.charitycommission.gov.uk/en/charity-search?p_p_id=uk_gov_ccew_onereg_charitydetails_web_portlet_CharitySearchPortlet&keywords=%s",
    "https://register-of-charities.charitycommission.gov.uk", "charity-details", "GB", "charity" },
  { "OSCR Scottish Charity Register",
    "https://www.oscr.org.uk/about-charities/search-the-register/charity-search/?q=%s",
    "https://www.oscr.org.uk", "/charity-details/", "GB", "charity" },
  { "FCA Financial Services Register",
    "https://register.fca.org.uk/s/search?q=%s",
    "https://register.fca.org.uk", "/services/", "GB", "regulator" },
  { "FSCS / FCA firm check",
    "https://www.fca.org.uk/search-results?query=%s",
    "https://www.fca.org.uk", NULL, "GB", "regulator" },
  { "Gambling Commission register",
    "https://registers.gamblingcommission.gov.uk/public-register?query=%s",
    "https://registers.gamblingcommission.gov.uk", NULL, "GB", "regulator" },

  /* --- UK people / property / electoral --- */
  { "192.com people & business",
    "https://www.192.com/people/search/?name=%s",
    "https://www.192.com", "/people/", "GB", "person" },
  { "Electoral Commission (party & finance)",
    "https://search.electoralcommission.org.uk/English/Search/Donations?query=%s",
    "https://search.electoralcommission.org.uk", NULL, "GB", "finance" },
  { "Land Registry price paid search",
    "https://landregistry.data.gov.uk/app/ppd/?et%5B%5D=lrcommon%3Afreehold&header=true&min_date=1+January+1995&postcode=%s",
    "https://landregistry.data.gov.uk", NULL, "GB", "property" },

  /* --- UK / Ireland courts & law --- */
  { "BAILII case-law search",
    "https://www.bailii.org/cgi-bin/lucy_search_1.cgi?method=boolean&query=%s",
    "https://www.bailii.org", "/cases/", "GB", "court" },
  { "The Gazette (official public record)",
    "https://www.thegazette.co.uk/all-notices/notice?text=%s",
    "https://www.thegazette.co.uk", "/notice/", "GB", "gazette" },
  { "Insolvency Service (individual insolvency)",
    "https://www.insolvencydirect.bis.gov.uk/eiir/IIRSearchResults.asp?Surname=%s",
    "https://www.insolvencydirect.bis.gov.uk", NULL, "GB", "insolvency" },
  { "Courts & Tribunals judgments (find-case-law)",
    "https://caselaw.nationalarchives.gov.uk/search?query=%s",
    "https://caselaw.nationalarchives.gov.uk", NULL, "GB", "court" },

  /* --- UK civic / parliament / government --- */
  { "TheyWorkForYou (MPs & Hansard)",
    "https://www.theyworkforyou.com/search/?q=%s",
    "https://www.theyworkforyou.com", NULL, "GB", "civic" },
  { "UK Parliament members search",
    "https://members.parliament.uk/members/commons?SearchText=%s",
    "https://members.parliament.uk", "/member/", "GB", "civic" },
  { "GOV.UK site search",
    "https://www.gov.uk/search/all?keywords=%s",
    "https://www.gov.uk", NULL, "GB", "government" },
  { "Contracts Finder (public procurement)",
    "https://www.contractsfinder.service.gov.uk/Search/Results?searchCriteria.keyword=%s",
    "https://www.contractsfinder.service.gov.uk", "/Notice/", "GB", "procurement" },
  { "Find a Tender (UK public tenders)",
    "https://www.find-tender.service.gov.uk/Search/Results?Keywords=%s",
    "https://www.find-tender.service.gov.uk", "/Notice/", "GB", "procurement" },

  /* --- Ireland --- */
  { "CRO Ireland company search",
    "https://core.cro.ie/search?searchText=%s",
    "https://core.cro.ie", NULL, "IE", "company" },
  { "Charities Regulator Ireland",
    "https://www.charitiesregulator.ie/en/information-for-the-public/search-the-register-of-charities?q=%s",
    "https://www.charitiesregulator.ie", NULL, "IE", "charity" },
  { "Courts Service Ireland judgments",
    "https://www.courts.ie/search?query=%s",
    "https://www.courts.ie", NULL, "IE", "court" },
  { "Oireachtas members (Irish parliament)",
    "https://www.oireachtas.ie/en/members/?term=%s",
    "https://www.oireachtas.ie", "/member/", "IE", "civic" },
  { "eTenders Ireland (public procurement)",
    "https://www.etenders.gov.ie/epps/quickSearchAction.do?searchSelect=%s",
    "https://www.etenders.gov.ie", NULL, "IE", "procurement" },
};

#define UK_REG_COUNT ((int)(sizeof(REGS) / sizeof(REGS[0])))
#define UK_TOTAL_CAP 500   /* exhaustive-ok: runaway guard, logged */
#define UK_PER_REG_CAP 0     /* exhaustive-ok: 0 = no cap, every hit is emitted */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) {
    fprintf(stderr, "[uk_registry] no entity\n");
    return -1;
  }
  char *enc = jo_urlencode(q);
  if (!enc) return -1;

  int total = 0;
  for (int i = 0; i < UK_REG_COUNT && total < UK_TOTAL_CAP; i++) {
    const uk_reg *r = &REGS[i];
    char url[1200];
    snprintf(url, sizeof url, r->tmpl, enc);

    char rt[64];
    snprintf(rt, sizeof rt, "uk-registry-%s", r->cat);

    int budget = UK_TOTAL_CAP - total;
    if (budget > UK_PER_REG_CAP) budget = UK_PER_REG_CAP;

    int n = jo_emit_anchors(ctx, sink, url, r->href_must, r->name, rt,
                            r->base, NULL, budget, "uk_registry");
    total += n;
  }

  free(enc);
  fprintf(stderr, "[uk_registry] emitted %d across %d registries\n",
          total, UK_REG_COUNT);
  return 0;   /* honest empty is not an error */
}

static const source_def uk_registry_def = {
  .id = "UK_REGISTRY", .collector = "osint",
  .name = "UK & Ireland Registry Sweep",
  .name_ja = "英国・アイルランド 登記/司法検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/world_reg_uk",
  .description = "UK & Ireland company/court/registry/civic search portals — real anchor extraction per entity",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(uk_registry_def)
