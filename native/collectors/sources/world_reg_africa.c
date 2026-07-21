/* collectors/sources/world_reg_africa.c
 * World company/court/registry search portals — AFRICA region. One source_def
 * (AFRICA_REGISTRY), on-demand entity pivot (ctx->entity = company / person /
 * beneficial-owner name). For the given entity, actually FETCHES each registry's
 * server-rendered search-results page and emits ONE intel_item per real anchor
 * extracted from that page (via jo_emit_anchors). Nothing is synthesized:
 * JS-only / anti-bot / shape-mismatched registries simply yield 0 rows for that
 * row (honest empty) — that is expected and fine. Total emissions are capped so
 * a single query cannot fan out without bound.
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
#define AFR_TOTAL_MAX   40
#define AFR_PER_REG_MAX 3

typedef struct {
  const char *name;      /* registry / portal display name              */
  const char *url_tmpl;  /* search URL with a single %s for the encoded query */
  const char *cc;        /* ISO-3166 alpha-2 country code               */
  const char *category;  /* company | court | deeds | beneficial-owner  */
  const char *base;      /* origin for root-relative hrefs              */
  const char *href_must; /* substring an emitted href must contain, or NULL */
} afr_reg;

/* ~25 REAL African company / court / deeds / registry search portals. */
static const afr_reg AFR[] = {
  /* South Africa */
  { "SA CIPC — company/CC name search",
    "https://eservices.cipc.co.za/EnterpriseSearch.aspx?searchtext=%s",
    "ZA", "company", "https://eservices.cipc.co.za", NULL },
  { "SAFLII — SA court judgments",
    "https://www.saflii.org/cgi-bin/sinosrch.cgi?method=auto&query=%s",
    "ZA", "court", "https://www.saflii.org", NULL },
  { "SA DeedsWeb — property deeds portal",
    "https://deedsweb.dalrrd.gov.za/searchweb/?q=%s",
    "ZA", "deeds", "https://deedsweb.dalrrd.gov.za", NULL },
  { "SA CourtOnline — case search",
    "https://www.courtonline.judiciary.org.za/search?q=%s",
    "ZA", "court", "https://www.courtonline.judiciary.org.za", NULL },

  /* Nigeria */
  { "NG CAC — public company search",
    "https://search.cac.gov.ng/home?searchTerm=%s",
    "NG", "company", "https://search.cac.gov.ng", NULL },
  { "NigeriaLII — court judgments",
    "https://nigerialii.org/search/?q=%s",
    "NG", "court", "https://nigerialii.org", NULL },

  /* Kenya */
  { "KE BRS eCitizen — business registration",
    "https://brs.ecitizen.go.ke/search?name=%s",
    "KE", "company", "https://brs.ecitizen.go.ke", NULL },
  { "KenyaLaw — case law search",
    "http://kenyalaw.org/caselaw/cases/advanced_search/?q=%s",
    "KE", "court", "http://kenyalaw.org", NULL },

  /* Ghana */
  { "GH RGD — Registrar-General company search",
    "https://rgd.gov.gh/search?q=%s",
    "GH", "company", "https://rgd.gov.gh", NULL },
  { "GhaLII — court judgments",
    "https://ghalii.org/search/?q=%s",
    "GH", "court", "https://ghalii.org", NULL },

  /* Egypt */
  { "EG GAFI — investor/company services",
    "https://www.gafi.gov.eg/English/Pages/SearchResults.aspx?k=%s",
    "EG", "company", "https://www.gafi.gov.eg", NULL },

  /* Morocco */
  { "MA OMPIC — trademark/company search (Directinfo)",
    "https://www.directinfo.ma/recherche?q=%s",
    "MA", "company", "https://www.directinfo.ma", NULL },

  /* Rwanda */
  { "RW RDB — business registration search",
    "https://org.rdb.rw/busregonline/Search.aspx?q=%s",
    "RW", "company", "https://org.rdb.rw", NULL },
  { "RwandaLII — court judgments",
    "https://rwandalii.org/search/?q=%s",
    "RW", "court", "https://rwandalii.org", NULL },

  /* Tanzania / Uganda / Zambia / Zimbabwe / Malawi (LII network) */
  { "TanzLII — court judgments",
    "https://tanzlii.org/search/?q=%s",
    "TZ", "court", "https://tanzlii.org", NULL },
  { "UgandaLII — court judgments",
    "https://ulii.org/search/?q=%s",
    "UG", "court", "https://ulii.org", NULL },
  { "ZambiaLII — court judgments",
    "https://zambialii.org/search/?q=%s",
    "ZM", "court", "https://zambialii.org", NULL },
  { "ZimLII — court judgments",
    "https://zimlii.org/search/?q=%s",
    "ZW", "court", "https://zimlii.org", NULL },
  { "MalawiLII — court judgments",
    "https://malawilii.org/search/?q=%s",
    "MW", "court", "https://malawilii.org", NULL },
  { "SierraLII — court judgments",
    "https://sierralii.gov.sl/search/?q=%s",
    "SL", "court", "https://sierralii.gov.sl", NULL },
  { "SeyLII — Seychelles court judgments",
    "https://seylii.org/search/?q=%s",
    "SC", "court", "https://seylii.org", NULL },
  { "NamibLII — Namibia court judgments",
    "https://namiblii.org/search/?q=%s",
    "NA", "court", "https://namiblii.org", NULL },
  { "LesothoLII — court judgments",
    "https://lesotholii.org/search/?q=%s",
    "LS", "court", "https://lesotholii.org", NULL },

  /* Botswana */
  { "BotswanaLII — court judgments",
    "https://botswanalii.org/search/?q=%s",
    "BW", "court", "https://botswanalii.org", NULL },

  /* Cross-border beneficial ownership */
  { "OpenOwnership — beneficial ownership register",
    "https://register.openownership.org/search?q=%s",
    "XX", "beneficial-owner", "https://register.openownership.org", NULL },
};
static const int AFR_N = (int)(sizeof(AFR) / sizeof(AFR[0]));

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return -1;

  int total = 0;
  for (int i = 0; i < AFR_N && total < AFR_TOTAL_MAX; i++) {
    const afr_reg *r = &AFR[i];
    char url[1024];
    snprintf(url, sizeof url, r->url_tmpl, enc);

    char rectype[64];
    snprintf(rectype, sizeof rectype, "africa-registry-%s", r->category);
    char tag[64];
    snprintf(tag, sizeof tag, "africa_reg:%s", r->cc);

    int remaining = AFR_TOTAL_MAX - total;
    int cap = remaining < AFR_PER_REG_MAX ? remaining : AFR_PER_REG_MAX;

    /* REAL fetch + real anchor extraction. JS-only/anti-bot rows honest-empty. */
    total += jo_emit_anchors(ctx, sink, url, r->href_must, r->name,
                             rectype, r->base, q, cap, tag);
  }

  free(enc);
  fprintf(stderr, "[africa_reg] total emitted %d across %d registries\n",
          total, AFR_N);
  return 0;   /* honest empty is not an error */
}

static const source_def africa_registry_def = {
  .id = "AFRICA_REGISTRY", .collector = "osint",
  .name = "Africa Company/Court/Registry Search",
  .name_ja = "アフリカ 企業・裁判所・登記検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/world_reg_africa",
  .description = "Fetches ~25 African company/court/deeds/beneficial-owner "
                 "registry search portals (SA CIPC/SAFLII/Deeds, NG CAC, KE BRS, "
                 "GH RGD, EG GAFI, MA OMPIC, RW RDB, LII network, OpenOwnership) "
                 "and emits real anchors per registry; honest-empty on JS/anti-bot.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(africa_registry_def)
