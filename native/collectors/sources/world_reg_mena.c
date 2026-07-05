/* collectors/sources/world_reg_mena.c
 * OSINT service — MENA / Gulf / Turkey / Israel business & registry search
 * portals. Entity pivot (company / person / trade-name). For the given
 * ctx->entity we actually FETCH each registry's public search page and emit
 * ONE intel_item per real anchor extracted from the returned HTML
 * (jo_emit_anchors). Nothing is synthesized: a JS-only / anti-bot / shaped-
 * differently portal simply yields 0 rows (honest empty) — that is expected
 * for many of these and we never fake a hit.
 *
 * Caps: PER_REG anchors per registry, TOTAL_MAX across the whole run, so one
 * query never fans out forever.
 *
 * URL honesty: each row uses the registry's REAL public search path. Where the
 * exact query parameter name is uncertain the known search endpoint is used and
 * the page is allowed to honest-empty rather than inventing a param. The query
 * is %-encoded (jo_urlencode, UTF-8 safe) for Arabic/Turkish/Hebrew scripts. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define PER_REG   3
#define TOTAL_MAX 40

/* One registry row. `url_tmpl` has a single %s where the %-encoded query goes.
 * `base` is prepended to root-relative hrefs found on the results page. */
typedef struct {
  const char *name;      /* human/service label                             */
  const char *url_tmpl;  /* search URL, one %s for the encoded query         */
  const char *base;      /* origin for root-relative anchors                 */
  const char *cc;        /* ISO-3166 alpha-2 country code                    */
} mena_reg;

/* ~25 real public company/court/registry search portals across MENA + Gulf +
 * Turkey + Israel. Many are server-rendered (yield anchors); several are
 * JS/SPA/anti-bot and will honest-empty — kept because they are the real
 * canonical portals an investigator would pivot to. */
static const mena_reg REGS[] = {
  /* --- United Arab Emirates ------------------------------------------------ */
  { "UAE Basher (unified econ. register)",
    "https://basher.gov.ae/en/search?query=%s",
    "https://basher.gov.ae", "AE" },
  { "Dubai DED / Invest in Dubai licence",
    "https://ded.ae/ded_lp/searchbytradename.aspx?name=%s",
    "https://ded.ae", "AE" },
  { "Abu Dhabi ADGM public register",
    "https://www.adgm.com/public-registers/search?q=%s",
    "https://www.adgm.com", "AE" },
  { "Dubai DIFC public register",
    "https://www.difc.ae/public-register?search=%s",
    "https://www.difc.ae", "AE" },
  { "UAE Ministry of Economy trademarks",
    "https://www.moec.gov.ae/en/search?searchText=%s",
    "https://www.moec.gov.ae", "AE" },

  /* --- Saudi Arabia -------------------------------------------------------- */
  { "Saudi Ministry of Commerce (المركز السعودي للأعمال)",
    "https://www.mc.gov.sa/en/eservices/Pages/search.aspx?q=%s",
    "https://www.mc.gov.sa", "SA" },
  { "Saudi Business Center unified register",
    "https://business.sa/en/search?query=%s",
    "https://business.sa", "SA" },
  { "Saudi Qiwa establishment lookup",
    "https://www.qiwa.sa/en/search?query=%s",
    "https://www.qiwa.sa", "SA" },

  /* --- Qatar --------------------------------------------------------------- */
  { "Qatar MOCI commercial register",
    "https://www.moci.gov.qa/en/?s=%s",
    "https://www.moci.gov.qa", "QA" },
  { "Qatar Financial Centre public register",
    "https://www.qfc.qa/en/public-register?search=%s",
    "https://www.qfc.qa", "QA" },

  /* --- Bahrain ------------------------------------------------------------- */
  { "Bahrain Sijilat commercial register",
    "https://www.sijilat.bh/CRSearch.aspx?name=%s",
    "https://www.sijilat.bh", "BH" },

  /* --- Kuwait / Oman ------------------------------------------------------- */
  { "Kuwait MOCI commercial register",
    "https://www.moci.gov.kw/search?q=%s",
    "https://www.moci.gov.kw", "KW" },
  { "Oman MOCIIP commercial register",
    "https://www.business.gov.om/wps/portal/ecr/search?name=%s",
    "https://www.business.gov.om", "OM" },

  /* --- Jordan / Lebanon / Egypt / Iraq ------------------------------------- */
  { "Jordan Companies Control Dept. (dawaer)",
    "https://www.ccd.gov.jo/echoportal/search?name=%s",
    "https://www.ccd.gov.jo", "JO" },
  { "Lebanon Ministry of Justice commercial register",
    "https://cr.justice.gov.lb/search/res.aspx?name=%s",
    "https://cr.justice.gov.lb", "LB" },
  { "Egypt GAFI investor service",
    "https://www.gafi.gov.eg/English/Pages/search.aspx?q=%s",
    "https://www.gafi.gov.eg", "EG" },
  { "Iraq Companies Registrar (registrar-companies)",
    "https://companies.mot.gov.iq/search?name=%s",
    "https://companies.mot.gov.iq", "IQ" },

  /* --- Turkey -------------------------------------------------------------- */
  { "Turkey MERSIS central registry",
    "https://mersis.ticaret.gov.tr/Portal/Home/Search?unvan=%s",
    "https://mersis.ticaret.gov.tr", "TR" },
  { "Turkey Ticaret Sicil Gazetesi (TTSG)",
    "https://www.ticaretsicil.gov.tr/view/hizli_arama.php?unvan=%s",
    "https://www.ticaretsicil.gov.tr", "TR" },
  { "Turkey e-Devlet firma sorgulama",
    "https://www.turkiye.gov.tr/arama?aranan=%s",
    "https://www.turkiye.gov.tr", "TR" },
  { "Turkey TOBB firma bilgi sistemi",
    "https://firmabilgileri.tobb.org.tr/firma-arama?unvan=%s",
    "https://firmabilgileri.tobb.org.tr", "TR" },

  /* --- Israel -------------------------------------------------------------- */
  { "Israel Rasham HaChavarot (Companies Registrar)",
    "https://ica.justice.gov.il/GenericCorporarion/SearchCorporation?unit=8&mispar_ta=%s",
    "https://ica.justice.gov.il", "IL" },
  { "Israel data.gov.il companies dataset",
    "https://data.gov.il/dataset/ica_companies?q=%s",
    "https://data.gov.il", "IL" },

  /* --- Cross-region aggregators ------------------------------------------- */
  { "Dalili UAE business directory",
    "https://www.dalili.ae/search?keyword=%s",
    "https://www.dalili.ae", "AE" },
  { "Yellow Pages Qatar business directory",
    "https://www.yellowpagesqatar.com/en/search?q=%s",
    "https://www.yellowpagesqatar.com", "QA" },
};
#define NREGS ((int)(sizeof(REGS) / sizeof(REGS[0])))

static int run(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *q = jo_urlencode(ctx->entity);
  if (!q) return 0;

  int total = 0;
  for (int i = 0; i < NREGS && total < TOTAL_MAX; i++) {
    char url[1024];
    snprintf(url, sizeof url, REGS[i].url_tmpl, q);
    int room = TOTAL_MAX - total;
    int cap  = room < PER_REG ? room : PER_REG;
    /* real fetch + real anchor extraction; JS-only/anti-bot rows → 0 (honest) */
    total += jo_emit_anchors(ctx, sink, url, NULL, REGS[i].name,
                             "mena-registry-result", REGS[i].base,
                             NULL, cap, "mena-registry");
  }
  free(q);
  fprintf(stderr, "[mena-registry] total emitted %d across %d registries\n",
          total, NREGS);
  return 0;   /* honest empty is not an error */
}

static const source_def mena_registry_def = {
  .id = "MENA_REGISTRY", .collector = "osint",
  .name = "MENA / Gulf / Turkey / Israel Registries",
  .name_ja = "中東・湾岸・トルコ・イスラエル 企業登記検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/world_reg_mena",
  .description = "Company/court/registry search across UAE, SA, QA, BH, KW, OM, JO, LB, EG, IQ, TR, IL (entity pivot; per-registry anchor scrape, honest-empty on JS/anti-bot portals)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mena_registry_def)
