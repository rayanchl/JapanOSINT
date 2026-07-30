/* collectors/sources/reg_gulf.c
 * OSINT service — Gulf / Turkey / Israel company & business registry search.
 * Single source id GULF_REGISTRY. Entity pivot (ctx->entity = company / trade
 * name / registration number). For the given entity we actually FETCH each
 * registry's public search page and emit ONE intel_item per real <a> anchor
 * extracted from the returned HTML (jo_emit_anchors). Nothing is synthesized:
 * a JS-only / SPA / anti-bot portal simply yields 0 rows (honest empty) — that
 * is expected for many Gulf portals and we never fake a hit or emit a bare
 * constructed URL as a record.
 *
 * Caps: PER_REG anchors per registry, TOTAL_MAX across the whole run, so one
 * query never fans out forever. The query is %-encoded (jo_urlencode, UTF-8
 * safe) for Arabic / Turkish / Hebrew scripts. Each row uses the registry's
 * REAL public search path; where the exact query parameter name is uncertain
 * the known search endpoint is used and the page is allowed to honest-empty
 * rather than inventing a param.
 *
 * This is Gulf-region scoped and distinct from any JP-scoped registry sources. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define PER_REG   3
#define TOTAL_MAX 500  /* exhaustive-ok: runaway guard, logged */

/* One registry row. `url_tmpl` has a single %s where the %-encoded query goes.
 * `base` is prepended to root-relative hrefs found on the results page. */
typedef struct {
  const char *name;      /* human/service label                        */
  const char *url_tmpl;  /* search URL, one %s for the encoded query    */
  const char *base;      /* origin for root-relative anchors            */
  const char *cc;        /* ISO-3166 alpha-2 country code               */
} gulf_reg;

/* ~10 real public Gulf/Turkey/Israel company-registry search portals. Several
 * are server-rendered (yield anchors); some are JS/SPA/anti-bot and will
 * honest-empty — kept because they are the real canonical portals. */
static const gulf_reg REGS[] = {
  /* --- United Arab Emirates --------------------------------------------- */
  { "UAE Dubai DED trade-name search",
    "https://ded.ae/ded_lp/searchbytradename.aspx?name=%s",
    "https://ded.ae", "AE" },
  { "UAE Abu Dhabi ADGM public register",
    "https://www.adgm.com/public-registers/search?q=%s",
    "https://www.adgm.com", "AE" },
  { "UAE Dubai DIFC public register",
    "https://www.difc.ae/public-register?search=%s",
    "https://www.difc.ae", "AE" },
  /* --- Saudi Arabia ----------------------------------------------------- */
  { "Saudi Ministry of Commerce register",
    "https://www.mc.gov.sa/en/eservices/Pages/search.aspx?q=%s",
    "https://www.mc.gov.sa", "SA" },
  /* --- Qatar ------------------------------------------------------------ */
  { "Qatar MOCI commercial register",
    "https://www.moci.gov.qa/en/?s=%s",
    "https://www.moci.gov.qa", "QA" },
  /* --- Bahrain ---------------------------------------------------------- */
  { "Bahrain Sijilat commercial register",
    "https://www.sijilat.bh/CRSearch.aspx?name=%s",
    "https://www.sijilat.bh", "BH" },
  /* --- Kuwait / Oman ---------------------------------------------------- */
  { "Kuwait MOCI commercial register",
    "https://www.moci.gov.kw/search?q=%s",
    "https://www.moci.gov.kw", "KW" },
  { "Oman MOCIIP commercial register",
    "https://www.business.gov.om/wps/portal/ecr/search?name=%s",
    "https://www.business.gov.om", "OM" },
  /* --- Turkey ----------------------------------------------------------- */
  { "Turkey MERSIS central registry",
    "https://mersis.ticaret.gov.tr/Portal/Home/Search?unvan=%s",
    "https://mersis.ticaret.gov.tr", "TR" },
  { "Turkey Ticaret Sicil Gazetesi (TTSG)",
    "https://www.ticaretsicil.gov.tr/view/hizli_arama.php?unvan=%s",
    "https://www.ticaretsicil.gov.tr", "TR" },
  /* --- Israel ----------------------------------------------------------- */
  { "Israel data.gov.il companies dataset",
    "https://data.gov.il/dataset/ica_companies?q=%s",
    "https://data.gov.il", "IL" },
};
#define NREGS ((int)(sizeof(REGS) / sizeof(REGS[0])))

static int run(const source_ctx *ctx, intel_sink *sink) {
  if (strcmp(ctx->source_id, "GULF_REGISTRY") != 0) return -1;
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
                             "gulf-registry-result", REGS[i].base,
                             NULL, cap, "gulf-registry");
  }
  free(q);
  fprintf(stderr, "[gulf-registry] total emitted %d across %d registries\n",
          total, NREGS);
  return 0;   /* honest empty is not an error */
}

static const source_def gulf_registry_def = {
  .id = "GULF_REGISTRY", .collector = "osint",
  .name = "Gulf / Turkey / Israel Registries",
  .name_ja = "湾岸・トルコ・イスラエル 企業登記検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/reg_gulf",
  .description = "Company registry search across UAE (DED/ADGM/DIFC), SA MC, QA MOCI, BH Sijilat, KW/OM MOCI, TR MERSIS/Ticaret Sicil, IL data.gov.il (entity pivot; per-registry anchor scrape, honest-empty on JS/anti-bot portals)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(gulf_registry_def)
