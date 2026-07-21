/* collectors/sources/world_reg_asia.c
 * OSINT service — ASIA_REGISTRY. On-demand entity pivot (ctx->entity = a
 * company / party name) fanned out across ~25 REAL public company / court /
 * business-registry search portals for East / South-East / South Asia:
 * Korea, Singapore, Malaysia, Indonesia, Thailand, Philippines, Vietnam,
 * India (+ a couple of pan-regional case-law / open-data portals).
 *
 * For each row we build the registry's own server-rendered search URL with the
 * %-encoded entity and fetch it via jo_emit_anchors — every emitted intel_item
 * is a REAL anchor (href + visible text) extracted from the live results page.
 * Nothing is synthesized: registries that are JS-only, anti-bot, or POST-only
 * simply yield 0 for that row (honest empty), which is expected and fine.
 *
 * Guardrails so one query can't fan out forever:
 *   - per-registry cap  ASIA_PER_REG  (default 3)
 *   - total cap         ASIA_TOTAL_MAX (default 40)
 * We NEVER emit a constructed search URL as a record — only extracted anchors.
 * Keyless, free_tier=1. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define ASIA_PER_REG   3
#define ASIA_TOTAL_MAX 40

/* One registry search portal. `url_tmpl` has exactly one %s for the encoded
 * query. `href_must` is a substring an emittable result href must contain (to
 * skip nav/footer chrome); NULL = accept any. `base` is prepended to
 * root-relative hrefs. */
typedef struct {
  const char *name;        /* human label (also the emitted "service")      */
  const char *cc;          /* ISO country code                              */
  const char *category;    /* company | court | registry | opendata         */
  const char *url_tmpl;    /* search URL template, one %s for encoded query */
  const char *href_must;   /* result-link substring filter, or NULL         */
  const char *base;        /* base for root-relative hrefs                   */
} asia_reg;

/* ~25 real public search portals. Search paths reflect each registry's known
 * public search endpoint; where the site is a JS SPA / anti-bot / POST-only,
 * we still point at the correct canonical search path and let it honest-empty
 * rather than invent a fake query param. */
static const asia_reg REGS[] = {
  /* --- Korea (KR) --- */
  { "KR DART Disclosure Search", "KR", "company",
    "https://dart.fss.or.kr/dsab007/main.do?textCrpNm=%s",
    "dsaf", "https://dart.fss.or.kr", },
  { "KR data.go.kr Open Data", "KR", "opendata",
    "https://www.data.go.kr/tcs/dss/selectDataSetList.do?keyword=%s",
    "/data/", "https://www.data.go.kr", },
  { "KR Supreme Court Case Search", "KR", "court",
    "https://glaw.scourt.go.kr/wsjo/panre/sjo060.do?q=%s",
    "panre", "https://glaw.scourt.go.kr", },

  /* --- Singapore (SG) --- */
  { "SG ACRA BizFile Directory", "SG", "company",
    "https://www.bizfile.gov.sg/ngbbizfileinternet/faces/oracle/webcenter/portalapp/pages/BizfileHomepage.jspx?q=%s",
    NULL, "https://www.bizfile.gov.sg", },
  { "SG data.gov.sg Datasets", "SG", "opendata",
    "https://data.gov.sg/datasets?query=%s",
    "/datasets", "https://data.gov.sg", },
  { "SG OpenGovSG Registers", "SG", "company",
    "https://data.gov.sg/search?q=%s",
    NULL, "https://data.gov.sg", },

  /* --- Malaysia (MY) --- */
  { "MY SSM e-Search", "MY", "company",
    "https://www.ssm-einfo.my/index.php?r=site/searchResult&keyword=%s",
    NULL, "https://www.ssm-einfo.my", },
  { "MY data.gov.my Datasets", "MY", "opendata",
    "https://data.gov.my/data-catalogue?search=%s",
    "/data-catalogue", "https://data.gov.my", },

  /* --- Indonesia (ID) --- */
  { "ID AHU Legal Entity (Ditjen AHU)", "ID", "company",
    "https://ahu.go.id/pencarian/profil-pt?nama=%s",
    NULL, "https://ahu.go.id", },
  { "ID OSS Business Licensing", "ID", "company",
    "https://oss.go.id/informasi/pencarian?keyword=%s",
    NULL, "https://oss.go.id", },
  { "ID data.go.id Datasets", "ID", "opendata",
    "https://data.go.id/dataset?q=%s",
    "/dataset", "https://data.go.id", },
  { "ID Supreme Court Directory (Putusan MA)", "ID", "court",
    "https://putusan3.mahkamahagung.go.id/search.html?q=%s",
    "putusan", "https://putusan3.mahkamahagung.go.id", },

  /* --- Thailand (TH) --- */
  { "TH DBD DataWarehouse", "TH", "company",
    "https://datawarehouse.dbd.go.th/searchJuristicInfo?keyword=%s",
    NULL, "https://datawarehouse.dbd.go.th", },
  { "TH data.go.th Datasets", "TH", "opendata",
    "https://data.go.th/dataset?q=%s",
    "/dataset", "https://data.go.th", },

  /* --- Philippines (PH) --- */
  { "PH SEC Express System", "PH", "company",
    "https://esearch.sec.gov.ph/api/company?searchKey=%s",
    NULL, "https://esearch.sec.gov.ph", },
  { "PH data.gov.ph Datasets", "PH", "opendata",
    "https://data.gov.ph/index/search?q=%s",
    NULL, "https://data.gov.ph", },

  /* --- Vietnam (VN) --- */
  { "VN National Business Registration (NBR)", "VN", "company",
    "https://dichvuthongtin.dkkd.gov.vn/inf/Forms/Searches/EnterpriseInfo.aspx?keyword=%s",
    NULL, "https://dichvuthongtin.dkkd.gov.vn", },
  { "VN data.gov.vn Datasets", "VN", "opendata",
    "https://data.gov.vn/dataset?q=%s",
    "/dataset", "https://data.gov.vn", },

  /* --- India (IN) --- */
  { "IN MCA Company/LLP Master Data", "IN", "company",
    "https://www.mca.gov.in/mcafoportal/showCheckCompanyName.do?companyName=%s",
    NULL, "https://www.mca.gov.in", },
  { "IN Zauba Corp Company Search", "IN", "company",
    "https://www.zaubacorp.com/companysearchresults/%s",
    "/company/", "https://www.zaubacorp.com", },
  { "IN Indian Kanoon Case Law", "IN", "court",
    "https://indiankanoon.org/search/?formInput=%s",
    "/doc/", "https://indiankanoon.org", },
  { "IN ECI Political Party / Candidate", "IN", "registry",
    "https://www.myneta.info/search_myneta.php?q=%s",
    NULL, "https://www.myneta.info", },
  { "IN data.gov.in Datasets", "IN", "opendata",
    "https://www.data.gov.in/catalogs?title=%s",
    "/catalog", "https://www.data.gov.in", },

  /* --- Pan-regional --- */
  { "AsianLII Case Law", "AS", "court",
    "http://www.asianlii.org/cgi-bin/sinosrch.cgi?query=%s",
    "/", "http://www.asianlii.org", },
  { "OpenCorporates Asia Companies", "AS", "company",
    "https://opencorporates.com/companies?q=%s",
    "/companies/", "https://opencorporates.com", },
};
static const int NREGS = (int)(sizeof(REGS) / sizeof(REGS[0]));

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);       /* UTF-8 safe %-encoding for CJK/Thai/Hangul */
  if (!enc) return -1;

  int total = 0;
  for (int i = 0; i < NREGS && total < ASIA_TOTAL_MAX; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    const asia_reg *r = &REGS[i];

    char url[1200];
    snprintf(url, sizeof url, r->url_tmpl, enc);

    char rt[64];
    snprintf(rt, sizeof rt, "asia-%s", r->category);   /* e.g. asia-company */

    char tag[64];
    snprintf(tag, sizeof tag, "asia_reg[%s]", r->cc);

    int remaining = ASIA_TOTAL_MAX - total;
    int cap = remaining < ASIA_PER_REG ? remaining : ASIA_PER_REG;

    int n = jo_emit_anchors(ctx, sink, url, r->href_must, r->name, rt,
                            r->base, q, cap, tag);
    if (n > 0) total += n;
  }

  free(enc);
  fprintf(stderr, "[asia_registry] emitted %d across %d portals\n", total, NREGS);
  return 0;   /* honest empty is not an error */
}

static const source_def world_reg_asia_def = {
  .id = "ASIA_REGISTRY", .collector = "osint",
  .name = "Asia Company & Court Registries", .name_ja = "アジア企業・裁判所登記",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/world_reg_asia",
  .description = "Fan-out entity search across ~25 East/SE/South Asian company, court & open-data registries (keyless anchor-scrape; JS-only portals honest-empty)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(world_reg_asia_def)
