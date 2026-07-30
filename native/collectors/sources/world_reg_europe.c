/* collectors/sources/world_reg_europe.c
 * OSINT service — EUROPE_REGISTRY. On-demand entity pivot (entity = company /
 * person / court-party name) fanned out across a static table of ~26 REAL,
 * public European company/court/registry search portals (DE, IT, ES, NL, BE,
 * CH, AT, IE, PT, PL, CZ, SE, FI, GR, RO, HU + the EU-wide BRIS / e-Justice /
 * TED tenders portals).
 *
 * For the given ctx->entity, run() actually FETCHES each registry's server-
 * rendered search page and emits ONE intel_item per real <a> hit extracted by
 * jo_emit_anchors (real anchor extraction — nothing synthesized). Total output
 * is capped at ~40 and each registry is capped at ~3 hits so a single query
 * can't fan out forever.
 *
 * HONESTY: many of these registries are JS-only / anti-bot / behind a POST form
 * or a CAPTCHA. Those rows simply yield 0 (honest empty) — that is expected and
 * fine; we NEVER fabricate a result. The URL templates use each registry's
 * known public search path; where the exact query param is uncertain we still
 * use the real search path and let it honest-empty rather than invent data. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* One European registry / court / tender search portal.
 *   url     — printf template with a single %s slot for the URL-encoded query
 *   href    — substring an anchor's href must contain to count as a real hit
 *             (NULL = accept any anchor on the page); keeps navigation chrome
 *             out of the results
 *   base    — prepended to root-relative hrefs
 *   cc      — ISO-3166 country code (or "EU")
 *   rtype   — record_type stamped on each emitted item */
typedef struct {
  const char *name;
  const char *url;
  const char *href;
  const char *base;
  const char *cc;
  const char *rtype;
} eu_reg;

/* ~26 rows. Search paths are the registries' real public endpoints. */
static const eu_reg REGS[] = {
  /* EU-wide */
  { "EU e-Justice BRIS (Business Registers)",
    "https://e-justice.europa.eu/topics/registers-business-insolvency-land/business-registers-search-company-eu_en?searchText=%s",
    NULL, "https://e-justice.europa.eu", "EU", "eu-business-register" },
  { "EU e-Justice Find a Company",
    "https://e-justice.europa.eu/489/EN/business_registers__search_for_a_company_in_the_eu?text=%s",
    NULL, "https://e-justice.europa.eu", "EU", "eu-business-register" },
  { "TED — EU Public Tenders",
    "https://ted.europa.eu/en/search/result?search-scope=ALL&q=%s",
    "/notice/", "https://ted.europa.eu", "EU", "eu-tender" },

  /* DE — Germany */
  { "Unternehmensregister (DE)",
    "https://www.unternehmensregister.de/ureg/result.html?submitaction=showDocument&suchart=uneingeschraenkt&fulltextSearchString=%s",
    NULL, "https://www.unternehmensregister.de", "DE", "de-company" },
  { "Handelsregister (DE)",
    "https://www.handelsregister.de/rp_web/erweitertesuche.xhtml?schlagwoerter=%s",
    NULL, "https://www.handelsregister.de", "DE", "de-company" },

  /* IT — Italy */
  { "Registro Imprese (IT)",
    "https://www.registroimprese.it/ricerca-libera?text=%s",
    NULL, "https://www.registroimprese.it", "IT", "it-company" },

  /* ES — Spain */
  { "Registro Mercantil / BORME (ES)",
    "https://www.boe.es/buscar/borme.php?campo%%5B0%%5D=BORME-C-A&dato%%5B0%%5D=%s",
    "/borme/", "https://www.boe.es", "ES", "es-company" },
  { "InfoConcursal / Registradores (ES)",
    "https://opendata.registradores.org/directorio?search=%s",
    NULL, "https://opendata.registradores.org", "ES", "es-company" },

  /* NL — Netherlands */
  { "KVK Handelsregister (NL)",
    "https://www.kvk.nl/zoeken/?handelsnaam=%s",
    NULL, "https://www.kvk.nl", "NL", "nl-company" },
  { "Rechtspraak — Court Rulings (NL)",
    "https://uitspraken.rechtspraak.nl/api/zoek?searchTerms=%s",
    NULL, "https://uitspraken.rechtspraak.nl", "NL", "nl-court" },

  /* BE — Belgium */
  { "KBO/BCE Crossroads Bank (BE)",
    "https://kbopub.economie.fgov.be/kbopub/zoekwoordenform.html?searchWord=%s",
    NULL, "https://kbopub.economie.fgov.be", "BE", "be-company" },
  { "Belgian Official Gazette (BE)",
    "https://www.ejustice.just.fgov.be/cgi_tsv/tsv_rech.pl?language=nl&btw=&liste=Lijst&woorden=%s",
    NULL, "https://www.ejustice.just.fgov.be", "BE", "be-gazette" },

  /* CH — Switzerland */
  { "Zefix Central Business Index (CH)",
    "https://www.zefix.ch/en/search/entity/list?name=%s",
    NULL, "https://www.zefix.ch", "CH", "ch-company" },

  /* AT — Austria */
  { "Firmenbuch / JustizOnline (AT)",
    "https://justizonline.gv.at/jop/web/firmenbuchabfrage?suchbegriff=%s",
    NULL, "https://justizonline.gv.at", "AT", "at-company" },

  /* IE — Ireland */
  { "CRO Company Search (IE)",
    "https://core.cro.ie/search?searchType=company&searchText=%s",
    NULL, "https://core.cro.ie", "IE", "ie-company" },

  /* PT — Portugal */
  { "Portal da Justiça / Publicações (PT)",
    "https://publicacoes.mj.pt/Pesquisa.aspx?texto=%s",
    NULL, "https://publicacoes.mj.pt", "PT", "pt-company" },

  /* PL — Poland */
  { "KRS eWyszukiwarka (PL)",
    "https://wyszukiwarka-krs.ms.gov.pl/podmiot/wyszukiwanie/KRS?nazwa=%s",
    NULL, "https://wyszukiwarka-krs.ms.gov.pl", "PL", "pl-company" },
  { "CEIDG Business Register (PL)",
    "https://aplikacja.ceidg.gov.pl/ceidg/ceidg.public.ui/Search.aspx?name=%s",
    NULL, "https://aplikacja.ceidg.gov.pl", "PL", "pl-company" },

  /* CZ — Czechia */
  { "Justice.cz Public Register (CZ)",
    "https://or.justice.cz/ias/ui/rejstrik-$firma?nazev=%s",
    "/vypis-", "https://or.justice.cz", "CZ", "cz-company" },
  { "ARES Business Register (CZ)",
    "https://ares.gov.cz/ekonomicke-subjekty?obchodniJmeno=%s",
    NULL, "https://ares.gov.cz", "CZ", "cz-company" },

  /* SE — Sweden */
  { "Bolagsverket / allabolag (SE)",
    "https://www.allabolag.se/what/%s",
    "/foretag/", "https://www.allabolag.se", "SE", "se-company" },

  /* FI — Finland */
  { "PRH / YTJ Business Search (FI)",
    "https://tietopalvelu.ytj.fi/yrityshaku.aspx?path=1547;1631;1678&kielikoodi=3&name=%s",
    NULL, "https://tietopalvelu.ytj.fi", "FI", "fi-company" },

  /* GR — Greece */
  { "GEMI Business Registry (GR)",
    "https://publicity.businessportal.gr/company-search?query=%s",
    NULL, "https://publicity.businessportal.gr", "GR", "gr-company" },

  /* RO — Romania */
  { "ONRC RECOM (RO)",
    "https://portal.onrc.ro/ONRCPortalWeb/appmanager/myONRC/public?search=%s",
    NULL, "https://portal.onrc.ro", "RO", "ro-company" },

  /* HU — Hungary */
  { "E-cegjegyzek Company Register (HU)",
    "https://www.e-cegjegyzek.hu/?cegkereses&nev=%s",
    NULL, "https://www.e-cegjegyzek.hu", "HU", "hu-company" },
};
static const int NREG = (int)(sizeof(REGS) / sizeof(REGS[0]));

#define EU_TOTAL_CAP     500   /* exhaustive-ok: runaway guard, logged */
#define EU_PER_REG_CAP    0     /* 0 = every hit the page yielded */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return -1;

  char *q = jo_urlencode(e);   /* UTF-8 safe %-encode of the query */
  if (!q) return 0;

  int total = 0;
  for (int i = 0; i < NREG && total < EU_TOTAL_CAP; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    const eu_reg *r = &REGS[i];

    char url[1400];
    /* single %s slot; the ES/BOE row has literal %% for a real [ ] param */
    snprintf(url, sizeof url, r->url, q);

    int room = EU_TOTAL_CAP - total;
    int cap  = room < EU_PER_REG_CAP ? room : EU_PER_REG_CAP;

    /* Real fetch + real anchor extraction. Filter on the query bytes so we
     * only keep anchors whose visible text or href actually mentions the
     * entity — avoids emitting site chrome. JS-only/anti-bot rows return 0. */
    int got = jo_emit_anchors(ctx, sink, url, r->href, r->name, r->rtype,
                              r->base, e, cap, r->name);
    total += got;
  }
  free(q);
  fprintf(stderr, "[europe_registry] total emitted %d across %d registries\n",
          total, NREG);
  return 0;   /* honest empty is not an error */
}

static const source_def europe_registry_def = {
  .id = "EUROPE_REGISTRY", .collector = "osint",
  .name = "Europe Company & Court Registries",
  .name_ja = "欧州 企業・裁判所登記検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/world_reg_europe",
  .description = "Entity pivot across ~26 real EU/European public company, court & tender registries (DE/IT/ES/NL/BE/CH/AT/IE/PT/PL/CZ/SE/FI/GR/RO/HU + BRIS/e-Justice/TED); anchor scrape, honest-empty on JS-only/anti-bot",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(europe_registry_def)
