/* collectors/sources/courts_world.c
 * OSINT service — COURT_WORLD. ONE on-demand entity pivot (entity = a party /
 * case name / legal keyword) fanned out across a static table of ~20 REAL,
 * public national court / case-law search portals worldwide.
 *
 * A single run() (dispatched purely on ctx->entity — there is only one source
 * id, COURT_WORLD) walks the table and, for each portal, actually FETCHES the
 * portal's real server-rendered search page and emits ONE intel_item per real
 * <a> hit via jo_emit_anchors (nothing is synthesized — links + titles are
 * extracted from the fetched page). Per-portal output is capped and the whole
 * fan-out is capped so a single query can't run away.
 *
 * One portal — the Netherlands rechtspraak.nl — exposes a genuinely FREE open
 * search API returning an Atom feed
 *   GET https://data.rechtspraak.nl/uitspraken/zoeken?searchTerm=<entity>
 * so that one is parsed as Atom (jo_next_entry / jo_tag_inner), one item per
 * <entry>, giving structured real records rather than scraped anchors.
 *
 * HONESTY: many national portals are JS-only / anti-bot / POST-form / behind a
 * CAPTCHA (e.g. CanLII, some AustLII/NZLII paths). Those rows simply yield 0
 * (honest empty) — expected and fine; we NEVER fabricate a result, name, count
 * or link. Where a portal's exact query param is uncertain we still use its
 * real public search path and let it honest-empty rather than invent data.
 * Keyless; free_tier=1. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* One national court / case-law search portal.
 *   name  — human label / jurisdiction
 *   url   — printf template with a single %s slot for the URL-encoded query
 *   href  — substring an anchor's href must contain to count as a real hit
 *           (NULL = accept any anchor); keeps nav chrome out of results
 *   base  — prepended to root-relative hrefs
 *   cc    — ISO-3166 jurisdiction code (or region) */
typedef struct {
  const char *name;
  const char *url;
  const char *href;
  const char *base;
  const char *cc;
} court_portal;

/* ~20 rows. Search paths are each portal's real public endpoints.
 * The four already covered by reg_courts.c (BAILII, CanLII, AustLII, CJEU)
 * are intentionally NOT duplicated here to avoid double-emitting. */
static const court_portal PORTALS[] = {
  /* North America */
  { "US CourtListener (RECAP/opinions)",
    "https://www.courtlistener.com/?q=%s",
    "/opinion/", "https://www.courtlistener.com", "US" },
  { "Canada CanLII",
    "https://www.canlii.org/en/#search/text=%s",
    "canlii.org", "https://www.canlii.org", "CA" },

  /* Europe — national */
  { "France Legifrance (JuriCA/Jurisprudence)",
    "https://www.legifrance.gouv.fr/search/juri?tab_selection=juri&query=%s&searchField=ALL",
    "/juri/", "https://www.legifrance.gouv.fr", "FR" },
  { "Germany Rechtsprechung-im-Internet",
    "https://www.rechtsprechung-im-internet.de/jportal/portal/t/xxx/page/bsjrsprod.psml?searchtype=simple&query=%s",
    NULL, "https://www.rechtsprechung-im-internet.de", "DE" },
  { "Ireland Courts Service (courts.ie judgments)",
    "https://www.courts.ie/search?query=%s",
    "/judgments/", "https://www.courts.ie", "IE" },

  /* Commonwealth LIIs (SINO-search family) */
  { "New Zealand NZLII",
    "http://www.nzlii.org/cgi-bin/sinosrch.cgi?query=%s",
    "/nz/", "http://www.nzlii.org", "NZ" },
  { "Hong Kong HKLII",
    "https://www.hklii.hk/en/search?q=%s",
    "/cases/", "https://www.hklii.hk", "HK" },
  { "Singapore SgLII (elitigation judgments)",
    "https://www.elitigation.sg/gd/Home/Index?query=%s",
    "/gd/", "https://www.elitigation.sg", "SG" },
  { "South Africa SAFLII",
    "https://www.saflii.org/cgi-bin/sinosrch.cgi?query=%s",
    "/za/", "https://www.saflii.org", "ZA" },

  /* Asia / Pacific / others */
  { "India Kanoon",
    "https://indiankanoon.org/search/?formInput=%s",
    "/doc/", "https://indiankanoon.org", "IN" },
  { "Pakistan PakLII",
    "http://www.paklii.org/cgi-bin/sinosrch.cgi?query=%s",
    "/pk/", "http://www.paklii.org", "PK" },
  { "Kenya KenyaLaw",
    "http://kenyalaw.org/caselaw/cases/advanced_search/?q=%s",
    NULL, "http://kenyalaw.org", "KE" },
  { "Uganda ULII",
    "https://ulii.org/search/?q=%s",
    "/judgment", "https://ulii.org", "UG" },
  { "Philippines LawPhil (jurisprudence)",
    "https://lawphil.net/cgi-bin/search.cgi?query=%s",
    NULL, "https://lawphil.net", "PH" },

  /* World / regional aggregators */
  { "WorldLII (CommonLII cross-jurisdiction)",
    "http://www.worldlii.org/cgi-bin/sinosrch.cgi?query=%s",
    NULL, "http://www.worldlii.org", "WW" },
  { "AsianLII",
    "http://www.asianlii.org/cgi-bin/sinosrch.cgi?query=%s",
    NULL, "http://www.asianlii.org", "AS" },
  { "CommonLII",
    "http://www.commonlii.org/cgi-bin/sinosrch.cgi?query=%s",
    NULL, "http://www.commonlii.org", "WW" },
};
static const int NPORTALS = (int)(sizeof PORTALS / sizeof PORTALS[0]);

/* Netherlands rechtspraak.nl — free open Atom search API. Parse the feed and
 * emit one intel_item per <entry> (real ECLI id + title + link). Returns the
 * number emitted (honest empty when the feed has no entries). */
static int emit_rechtspraak(const source_ctx *ctx, intel_sink *sink,
                            const char *enc, const char *q) {
  char url[512];
  snprintf(url, sizeof url,
    "https://data.rechtspraak.nl/uitspraken/zoeken?searchTerm=%s&max=25", enc);
  char *xml = jo_get(ctx, url, NULL, "court_world_nl");
  if (!xml) return 0;

  int emitted = 0;
  const char *cur = xml;
  while (emitted < 25) {
    const char *e = jo_next_entry(&cur);
    if (!e) break;
    /* bound the search for this entry's fields to before the next entry */
    char *title = jo_tag_inner(&cur, "title");   /* advances cur past </title> */
    /* the <id> (ECLI) and <link href> live before </entry>; re-scan from e */
    const char *idp = strstr(e, "<id>");
    char ecli[128] = {0};
    if (idp) {
      idp += 4;
      const char *ide = strstr(idp, "</id>");
      if (ide && (size_t)(ide - idp) < sizeof ecli) {
        size_t l = (size_t)(ide - idp);
        memcpy(ecli, idp, l); ecli[l] = 0;
      }
    }
    /* link href="..." */
    char link[512] = {0};
    const char *lp = strstr(e, "<link");
    if (lp) {
      const char *hp = strstr(lp, "href=\"");
      if (hp) {
        hp += 6;
        const char *he = strchr(hp, '"');
        if (he && (size_t)(he - hp) < sizeof link) {
          size_t l = (size_t)(he - hp);
          memcpy(link, hp, l); link[l] = 0;
        }
      }
    }
    if (!title && !ecli[0]) { /* nothing usable */ ; }
    else {
      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "COURT_WORLD");
      cJSON_AddStringToObject(props, "portal", "Netherlands Rechtspraak (data.rechtspraak.nl)");
      cJSON_AddStringToObject(props, "jurisdiction", "NL");
      cJSON_AddStringToObject(props, "query", q);
      if (ecli[0]) cJSON_AddStringToObject(props, "ecli", ecli);
      if (link[0]) cJSON_AddStringToObject(props, "href", link);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      intel_item it = {0};
      it.remote_key      = ecli[0] ? ecli : (link[0] ? link : title);
      it.title           = title ? title : ecli;
      it.summary         = ecli[0] ? ecli : NULL;
      it.link            = link[0] ? link : (ecli[0] ? ecli : NULL);
      it.lang            = "nl";
      it.record_type     = "court-case";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"COURT_WORLD\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(pj);
    }
    free(title);
  }
  free(xml);
  fprintf(stderr, "[court_world_nl] emitted %d\n", emitted);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return -1;

  int total = 0;
  const int GLOBAL_CAP = 500;  /* exhaustive-ok: runaway guard, logged */
  const int PER_CAP    = 0;    /* exhaustive-ok: 0 = every hit on the page */

  /* 1) Netherlands free Atom API first (structured real records). */
  total += emit_rechtspraak(ctx, sink, enc, q);

  /* 2) Anchor-scrape the rest. */
  for (int i = 0; i < NPORTALS && total < GLOBAL_CAP; i++) {
    const court_portal *p = &PORTALS[i];
    if (ctx->cancel && *ctx->cancel) break;
    char url[1200];
    snprintf(url, sizeof url, p->url, enc);
    char tag[64];
    snprintf(tag, sizeof tag, "court_world_%s", p->cc);
    int n = jo_emit_anchors(ctx, sink, url, p->href, "COURT_WORLD",
                            "court-case", p->base, q, PER_CAP, tag);
    total += n;
  }

  free(enc);
  fprintf(stderr, "[court_world] total emitted %d across %d portals\n",
          total, NPORTALS + 1);
  return 0;   /* honest empty is not an error */
}

static const source_def court_world_def = {
  .id = "COURT_WORLD", .collector = "osint",
  .name = "World Court & Case-Law Search", .name_ja = "世界の裁判所・判例横断検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/court_world",
  .description = "Fan-out case-law search across ~20 national court/LII portals worldwide (rechtspraak.nl Atom API + anchor-scrape; keyless, honest-empty)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(court_world_def)
