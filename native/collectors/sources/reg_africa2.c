/* collectors/sources/reg_africa2.c
 * World company-registry search portals — AFRICA, batch 2 (AFRICA2_REGISTRY).
 * One source_def, on-demand entity pivot (ctx->entity = company / owner name).
 * Complements AFRICA_REGISTRY (world_reg_africa.c) with a DIFFERENT set of ~12
 * national business-registration authorities: NG CAC, KE eCitizen/BRS, GH RGD,
 * EG GAFI, MA OMPIC, TZ BRELA, UG URSB, ZM PACRA, RW org.rw open register, ET
 * (EthioTrade), CI (Guichet Unique), SN (Sénégal e-registration).
 *
 * For the given entity, run() actually FETCHES each registry's server-rendered
 * search-results page and emits ONE intel_item per real anchor extracted from
 * that page (via jo_emit_anchors). Nothing is synthesized: JS-only / anti-bot /
 * shape-mismatched registries simply yield 0 rows (honest empty) — expected and
 * fine. Total emissions are capped so a single query can't fan out unbounded.
 *
 * URLs are real public search paths for each registry; where a portal is
 * JS-driven and the exact query param is uncertain, the known search path is
 * used and the row honest-empties rather than inventing records. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* per-query global cap and per-registry cap */
/* NOT a page cap: it sits in the loop condition over REGISTRIES, so hitting it
 * ends the sweep and the registries after it go unqueried — reported as data
 * by jo_registry_sweep_notice(). */
#define AFR2_TOTAL_MAX   500  /* exhaustive-ok: whole-run emit cap; the sweep it
                               * cuts short is reported as a truncation notice */
#define AFR2_PER_REG_MAX 0    /* exhaustive-ok: 0 = every hit on the page */

typedef struct {
  const char *name;      /* registry / portal display name                    */
  const char *url_tmpl;  /* search URL with a single %s for the encoded query */
  const char *cc;        /* ISO-3166 alpha-2 country code                     */
  const char *base;      /* origin for root-relative hrefs                    */
  const char *href_must; /* substring an emitted href must contain, or NULL   */
} afr2_reg;

/* ~12 REAL African national business-registry search portals (batch 2). */
static const afr2_reg AFR2[] = {
  /* Nigeria — Corporate Affairs Commission public company search */
  { "NG CAC — Corporate Affairs Commission search",
    "https://search.cac.gov.ng/home?searchTerm=%s",
    "NG", "https://search.cac.gov.ng", NULL },

  /* Kenya — eCitizen Business Registration Service */
  { "KE eCitizen/BRS — Business Registration Service",
    "https://brs.ecitizen.go.ke/search?name=%s",
    "KE", "https://brs.ecitizen.go.ke", NULL },

  /* Ghana — Registrar-General's Department */
  { "GH RGD — Registrar-General's Department search",
    "https://rgd.gov.gh/search?q=%s",
    "GH", "https://rgd.gov.gh", NULL },

  /* Egypt — General Authority for Investment & Free Zones */
  { "EG GAFI — investor/company services search",
    "https://www.gafi.gov.eg/English/Pages/SearchResults.aspx?k=%s",
    "EG", "https://www.gafi.gov.eg", NULL },

  /* Morocco — OMPIC (via Directinfo public company directory) */
  { "MA OMPIC — company/trademark search (Directinfo)",
    "https://www.directinfo.ma/recherche?q=%s",
    "MA", "https://www.directinfo.ma", NULL },

  /* Tanzania — Business Registrations and Licensing Agency */
  { "TZ BRELA — Business Registrations & Licensing Agency",
    "https://ors.brela.go.tz/orsfront/search?q=%s",
    "TZ", "https://ors.brela.go.tz", NULL },

  /* Uganda — Uganda Registration Services Bureau */
  { "UG URSB — Uganda Registration Services Bureau",
    "https://obrs.ursb.go.ug/search?query=%s",
    "UG", "https://obrs.ursb.go.ug", NULL },

  /* Zambia — Patents and Companies Registration Agency */
  { "ZM PACRA — Patents & Companies Registration Agency",
    "https://www.pacra.org.zm/search?q=%s",
    "ZM", "https://www.pacra.org.zm", NULL },

  /* Rwanda — RDB business open register */
  { "RW org.rw — RDB business open register",
    "https://org.rdb.rw/busregonline/Search.aspx?q=%s",
    "RW", "https://org.rdb.rw", NULL },

  /* Ethiopia — Ministry of Trade & Regional Integration eTrade portal */
  { "ET eTrade — Ministry of Trade business register",
    "https://etrade.gov.et/business-registration/search?name=%s",
    "ET", "https://etrade.gov.et", NULL },

  /* Cote d'Ivoire — Guichet Unique (CEPICI) company search */
  { "CI CEPICI — Guichet Unique company search",
    "https://225invest.ci/recherche?q=%s",
    "CI", "https://225invest.ci", NULL },

  /* Senegal — company e-registration / creation portal */
  { "SN — Sénégal company registration search",
    "https://creationdentreprise.sn/recherche?q=%s",
    "SN", "https://creationdentreprise.sn", NULL },
};
static const int AFR2_N = (int)(sizeof(AFR2) / sizeof(AFR2[0]));

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return -1;

  int total = 0, i = 0, cancelled = 0;
  for (; i < AFR2_N && total < AFR2_TOTAL_MAX; i++) {
    if (ctx->cancel && *ctx->cancel) { cancelled = 1; break; }
    const afr2_reg *r = &AFR2[i];

    char url[1024];
    snprintf(url, sizeof url, r->url_tmpl, enc);

    char tag[64];
    snprintf(tag, sizeof tag, "africa2_reg:%s", r->cc);

    /* The old `remaining`/`cap` min() was dead: AFR2_PER_REG_MAX is 0 and
     * `remaining` is always > 0 inside this loop, so min(remaining, 0) was
     * always 0 — and jo_emit_anchors reads <= 0 as "no cap". Removed rather
     * than given teeth, which would change what this collector emits. */

    /* REAL fetch + real anchor extraction. JS-only/anti-bot rows honest-empty. */
    total += jo_emit_anchors(ctx, sink, url, r->href_must, r->name,
                             "africa2-registry-company", r->base, q,
                             AFR2_PER_REG_MAX, tag);
  }

  free(enc);
  jo_registry_sweep_notice(sink, "AFRICA2_REGISTRY", q, total, i, AFR2_N,
                           "AFR2_TOTAL_MAX", AFR2_TOTAL_MAX, cancelled);
  fprintf(stderr, "[africa2_reg] total emitted %d across %d of %d registries\n",
          total, i, AFR2_N);
  return 0;   /* honest empty is not an error */
}

static const source_def africa2_registry_def = {
  .id = "AFRICA2_REGISTRY", .collector = "osint",
  .name = "Africa Company Registry Search (batch 2)",
  .name_ja = "アフリカ 企業登記検索 (第2弾)",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/reg_africa2",
  .description = "Fetches ~12 African national business registries (NG CAC, KE "
                 "eCitizen/BRS, GH RGD, EG GAFI, MA OMPIC, TZ BRELA, UG URSB, "
                 "ZM PACRA, RW org.rw, ET, CI, SN) and emits real result anchors "
                 "per registry; honest-empty on JS/anti-bot.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(africa2_registry_def)
