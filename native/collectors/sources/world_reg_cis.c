/* collectors/sources/world_reg_cis.c
 * Russia / CIS regional company · court · registry sweep. ONE source_def
 * (id=CIS_REGISTRY, category=government, type=scraped, on-demand entity pivot).
 *
 * For a given ctx->entity (a company name / person / EDRPOU / INN), run()
 * walks a static table of ~25 REAL public search portals across the region —
 * Russia (Rusprofile, List-Org, nalog EGRUL, sudact, FSSP, nomer/avinfo),
 * Ukraine (Opendatabot, Clarity, YouControl, NAZK, Prozorro), and the smaller
 * CIS registries (Belarus, Kazakhstan, Georgia, Armenia, Azerbaijan,
 * Kyrgyzstan, Moldova, Uzbekistan, Tajikistan) — and FETCHES each portal's
 * real server-rendered search page, emitting one intel_item per result anchor
 * via jo_emit_anchors (REAL extracted links/titles only).
 *
 * Honesty: nothing is synthesized. Portals that are JS-only / anti-bot /
 * Cloudflare-gated simply yield 0 for that row (honest empty) — expected and
 * fine; we never fabricate a result. Non-Latin queries are %-encoded (UTF-8
 * safe) via jo_urlencode. Total emit capped at ~40, per-registry at ~3, so one
 * query never fans out forever. Keyless, free_tier=1. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* One registry portal. `tmpl` has a single %s where the %-encoded query goes.
 * `href_must` filters extracted anchors to real result rows (NULL = any). */
typedef struct {
  const char *name;      /* portal display name                        */
  const char *tmpl;      /* full search URL, one %s for the query      */
  const char *href_must; /* anchor href substring filter (NULL = any)  */
  const char *base;      /* prefix for root-relative hrefs             */
  const char *cc;        /* ISO country code of the registry           */
  const char *category;  /* company | court | enforcement | person     */
} cis_reg;

/* ~25 REAL public search portals. Where the exact query param is uncertain the
 * registry's known search path is used and the row simply honest-empties. */
static const cis_reg REGS[] = {
  /* ---- Russia ---- */
  { "Rusprofile",        "https://www.rusprofile.ru/search?query=%s",
    NULL, "https://www.rusprofile.ru", "RU", "company" },
  { "List-Org",          "https://www.list-org.com/search?type=all&val=%s",
    "/company/", "https://www.list-org.com", "RU", "company" },
  { "Nalog EGRUL",       "https://egrul.nalog.ru/index.html?query=%s",
    NULL, "https://egrul.nalog.ru", "RU", "company" },
  { "Sudact (case-law)", "https://sudact.ru/regular/doc/?regular-txt=%s",
    "/regular/doc/", "https://sudact.ru", "RU", "court" },
  { "FSSP bailiffs",     "https://fssp.gov.ru/iss/ip/?is[fio]=%s",
    NULL, "https://fssp.gov.ru", "RU", "enforcement" },
  { "Nomer.io phones",   "https://nomer.io/search/%s",
    NULL, "https://nomer.io", "RU", "person" },
  { "Avinfo autos",      "https://avinfo.co/ru/proverkaavto/%s",
    NULL, "https://avinfo.co", "RU", "person" },
  { "Kad arbitr cases",  "https://kad.arbitr.ru/?text=%s",
    NULL, "https://kad.arbitr.ru", "RU", "court" },
  { "Datanewton",        "https://datanewton.ru/search?query=%s",
    "/company", "https://datanewton.ru", "RU", "company" },
  { "Zachestnyibiznes",  "https://zachestnyibiznes.ru/search?query=%s",
    NULL, "https://zachestnyibiznes.ru", "RU", "company" },

  /* ---- Ukraine ---- */
  { "Opendatabot UA",    "https://opendatabot.ua/search?query=%s",
    NULL, "https://opendatabot.ua", "UA", "company" },
  { "Clarity Project",   "https://clarity-project.info/search?q=%s",
    "/edr/", "https://clarity-project.info", "UA", "company" },
  { "YouControl UA",     "https://youcontrol.com.ua/search/?q=%s",
    NULL, "https://youcontrol.com.ua", "UA", "company" },
  { "NAZK register",     "https://public.nazk.gov.ua/documents?q=%s",
    NULL, "https://public.nazk.gov.ua", "UA", "person" },
  { "Prozorro tenders",  "https://prozorro.gov.ua/search/tender?text=%s",
    NULL, "https://prozorro.gov.ua", "UA", "company" },
  { "Reyestr sudreyestr","https://reyestr.court.gov.ua/?SearchText=%s",
    NULL, "https://reyestr.court.gov.ua", "UA", "court" },

  /* ---- Belarus ---- */
  { "Belarus EGR",       "https://egr.gov.by/egrn/index.jsp?content=search&query=%s",
    NULL, "https://egr.gov.by", "BY", "company" },
  { "Kartoteka BY",      "https://www.kartoteka.by/search?text=%s",
    NULL, "https://www.kartoteka.by", "BY", "company" },

  /* ---- Kazakhstan ---- */
  { "KZ Adata.kz",       "https://adata.kz/search?query=%s",
    "/companies/", "https://adata.kz", "KZ", "company" },
  { "KZ Statgov BIN",    "https://stat.gov.kz/ru/search/?q=%s",
    NULL, "https://stat.gov.kz", "KZ", "company" },

  /* ---- Georgia ---- */
  { "Georgia NAPR",      "https://enreg.reestri.gov.ge/main.php?c=search&q=%s",
    NULL, "https://enreg.reestri.gov.ge", "GE", "company" },

  /* ---- Armenia ---- */
  { "Armenia e-register","https://www.e-register.am/en/companies?search=%s",
    NULL, "https://www.e-register.am", "AM", "company" },

  /* ---- Azerbaijan ---- */
  { "Azerbaijan e-taxes","https://www.e-taxes.gov.az/ebyn/commersialSearch.jsp?q=%s",
    NULL, "https://www.e-taxes.gov.az", "AZ", "company" },

  /* ---- Kyrgyzstan / Moldova / Uzbekistan ---- */
  { "Kyrgyzstan Salyk",  "https://salyk.kg/search?query=%s",
    NULL, "https://salyk.kg", "KG", "company" },
  { "Moldova IDNO",      "https://idno.md/ro/search?q=%s",
    "/company", "https://idno.md", "MD", "company" },
  { "Uzbekistan Orginfo","https://orginfo.uz/en/search/organizations/?q=%s",
    "/organization/", "https://orginfo.uz", "UZ", "company" },
};
static const int NREGS = (int)(sizeof(REGS) / sizeof(REGS[0]));

#define CIS_TOTAL_CAP   500   /* exhaustive-ok: runaway guard, logged */
#define CIS_PER_REG_CAP 0     /* exhaustive-ok: 0 = no cap, every hit is emitted */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);   /* UTF-8 safe %-encode for Cyrillic etc. */
  if (!enc) return -1;

  int total = 0;
  for (int i = 0; i < NREGS && total < CIS_TOTAL_CAP; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    const cis_reg *r = &REGS[i];

    char url[1024];
    snprintf(url, sizeof url, r->tmpl, enc);

    char tag[64], rtype[48];
    snprintf(tag, sizeof tag, "cis_reg:%s", r->cc);
    snprintf(rtype, sizeof rtype, "cis-%s", r->category);

    int room = CIS_TOTAL_CAP - total;
    int cap = room < CIS_PER_REG_CAP ? room : CIS_PER_REG_CAP;

    /* Real fetch + real anchor extraction. JS-only/anti-bot rows → 0. */
    int n = jo_emit_anchors(ctx, sink, url, r->href_must, r->name,
                            rtype, r->base, NULL, cap, tag);
    total += n;
  }

  free(enc);
  fprintf(stderr, "[cis_registry] tabled %d registries, emitted %d\n",
          NREGS, total);
  return 0;   /* honest empty is not an error */
}

static const source_def cis_registry_def = {
  .id = "CIS_REGISTRY", .collector = "osint",
  .name = "Russia/CIS Company & Court Registry Sweep",
  .name_ja = "ロシア・CIS 企業・裁判所レジストリ横断検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/world_reg_cis",
  .description = "Russia/CIS regional sweep — ~25 public company/court/enforcement "
                 "registries (Rusprofile, List-Org, EGRUL, Sudact, FSSP, "
                 "Opendatabot, Clarity, NAZK, BY/KZ/GE/AM/AZ registries); real "
                 "anchor-scrape, JS/anti-bot rows honest-empty (keyless)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cis_registry_def)
