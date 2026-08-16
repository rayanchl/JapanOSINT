/* collectors/sources/gazette_world.c
 * OSINT service — GAZETTE_WORLD. On-demand entity pivot (entity = person /
 * company / court-party name) fanned out across a static table of ~15 REAL,
 * official national government gazettes (the state journals of record where
 * insolvencies, corporate filings, appointments, legal notices, laws and
 * regulations are published).
 *
 * The UK The Gazette exposes a genuine, keyless JSON search API
 * (thegazette.co.uk/all-notices/notice/data.json?text=<entity>) — for that row
 * we FETCH + PARSE the JSON and emit one intel_item per real notice (title,
 * publication date, notice URL). For every other gazette we FETCH the real,
 * server-rendered public search page and emit one intel_item per real <a> hit
 * extracted by jo_emit_anchors (real anchor extraction — nothing synthesized).
 *
 * HONESTY: many national gazettes are JS-only / behind a POST form / anti-bot /
 * PDF-only. Those rows simply yield 0 (honest empty) — expected and fine; we
 * NEVER fabricate a notice. The URL templates use each gazette's known public
 * search path; where the exact query param is uncertain we still use the real
 * search path and let it honest-empty rather than invent data. Total output is
 * capped so one query can't fan out forever. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* One national gazette search portal.
 *   name  — human label (also used as the [tag] and service name)
 *   url   — printf template with a single %s slot for the URL-encoded query
 *   href  — substring an anchor's href must contain to count as a real hit
 *           (NULL = accept any anchor on the page); keeps nav chrome out
 *   base  — prepended to root-relative hrefs
 *   cc    — ISO-3166 country code (or "EU")
 *   rtype — record_type stamped on each emitted item */
typedef struct {
  const char *name;
  const char *url;
  const char *href;
  const char *base;
  const char *cc;
  const char *rtype;
} gazette;

/* ~15 rows. Search paths are the gazettes' real public endpoints. */
static const gazette GAZ[] = {
  /* UK — handled specially below via its JSON API; kept here for metadata. */
  { "UK The Gazette",
    "https://www.thegazette.co.uk/all-notices/notice/data.json?text=%s",
    NULL, "https://www.thegazette.co.uk", "GB", "uk-gazette-notice" },

  /* EU — Official Journal via EUR-Lex quick search */
  { "EU Official Journal (EUR-Lex)",
    "https://eur-lex.europa.eu/search.html?scope=EURLEX&text=%s&lang=en&type=quick",
    "/legal-content/", "https://eur-lex.europa.eu", "EU", "eu-official-journal" },

  /* FR — Journal Officiel / Legifrance */
  { "France JORF (Legifrance)",
    "https://www.legifrance.gouv.fr/search/jorf?tab_selection=jorf&searchField=ALL&query=%s",
    NULL, "https://www.legifrance.gouv.fr", "FR", "fr-jorf-notice" },
  { "France BODACC (business gazette)",
    "https://www.bodacc.fr/pages/annonces-commerciales-liste/?q=%s",
    NULL, "https://www.bodacc.fr", "FR", "fr-bodacc-notice" },

  /* ES — Boletin Oficial del Estado + business gazette BORME */
  { "Spain BOE (Boletin Oficial del Estado)",
    "https://www.boe.es/buscar/boe.php?campo%%5B0%%5D=TIT&dato%%5B0%%5D=%s",
    "/boe/", "https://www.boe.es", "ES", "es-boe-notice" },
  { "Spain BORME (business gazette)",
    "https://www.boe.es/buscar/borme.php?campo%%5B0%%5D=BORME-C-A&dato%%5B0%%5D=%s",
    "/borme/", "https://www.boe.es", "ES", "es-borme-notice" },

  /* DE — Bundesanzeiger + federal law gazette */
  { "Germany Bundesanzeiger",
    "https://www.bundesanzeiger.de/pub/de/suchen?4&fulltext=%s",
    NULL, "https://www.bundesanzeiger.de", "DE", "de-bundesanzeiger" },
  { "Germany Bundesgesetzblatt (BGBl)",
    "https://www.recht.bund.de/de/suche?query=%s",
    NULL, "https://www.recht.bund.de", "DE", "de-bgbl-notice" },

  /* IT — Gazzetta Ufficiale */
  { "Italy Gazzetta Ufficiale",
    "https://www.gazzettaufficiale.it/ricerca/pdf/foglio_ordinario2/2/0/0?reset=true&testo=%s",
    NULL, "https://www.gazzettaufficiale.it", "IT", "it-gazzetta-notice" },

  /* PT — Diario da Republica */
  { "Portugal Diario da Republica",
    "https://diariodarepublica.pt/dr/pesquisa/-/search/basic?q=%s",
    NULL, "https://diariodarepublica.pt", "PT", "pt-dre-notice" },

  /* NL — Staatscourant / officielebekendmakingen */
  { "Netherlands Staatscourant",
    "https://zoek.officielebekendmakingen.nl/resultaten?svel=Publicatiedatum&svol=Aflopend&q=%s",
    "/stcrt-", "https://zoek.officielebekendmakingen.nl", "NL", "nl-staatscourant" },

  /* BE — Belgisch Staatsblad / Moniteur Belge */
  { "Belgium Belgisch Staatsblad",
    "https://www.ejustice.just.fgov.be/cgi_tsv/tsv_rech.pl?language=nl&btw=&liste=Lijst&woorden=%s",
    NULL, "https://www.ejustice.just.fgov.be", "BE", "be-staatsblad" },

  /* IE — Iris Oifigiuil */
  { "Ireland Iris Oifigiuil",
    "https://www.irisoifigiuil.ie/EN/Search.aspx?SearchText=%s",
    NULL, "https://www.irisoifigiuil.ie", "IE", "ie-iris-notice" },

  /* AU — Commonwealth of Australia Gazette (Federal Register of Legislation) */
  { "Australia Commonwealth Gazette",
    "https://www.legislation.gov.au/Search/%s",
    NULL, "https://www.legislation.gov.au", "AU", "au-gazette-notice" },

  /* CA — Canada Gazette */
  { "Canada Gazette",
    "https://gazette.gc.ca/rp-pr/publications-eng.php?q=%s",
    NULL, "https://gazette.gc.ca", "CA", "ca-gazette-notice" },
};
static const int NGAZ = (int)(sizeof(GAZ) / sizeof(GAZ[0]));

#define GW_TOTAL_CAP   500  /* exhaustive-ok: runaway guard, logged */
#define GW_PER_CAP      4   /* at most ~4 hits per gazette                  */

/* UK The Gazette JSON API → one intel_item per notice. Returns count emitted.
 * The API returns an Atom-ish JSON: { "entry": [ { title, id, updated,
 * "f:notice-code", ... }, ... ] }. We tolerate both "entry" as array and a
 * single object, and read whichever title/link/date fields are present. */
static int uk_json_emit(const source_ctx *ctx, intel_sink *sink,
                        const char *url, int cap) {
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "uk-gazette");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *entries = cJSON_GetObjectItem(root, "entry");
  int emitted = 0;

  cJSON *e = NULL;
  int single = 0;
  if (cJSON_IsObject(entries)) { single = 1; }

  for (int idx = 0; emitted < cap; idx++) {
    if (single) { e = entries; }
    else {
      if (!cJSON_IsArray(entries)) break;
      e = cJSON_GetArrayItem(entries, idx);
    }
    if (!e) break;

    /* title may be a plain string or an object {"#text": "..."} */
    const char *title = jo_sv(e, "title");
    if (!title) {
      const cJSON *to = cJSON_GetObjectItem(e, "title");
      if (to && cJSON_IsObject(to)) title = jo_sv(to, "#text");
    }
    const char *id   = jo_sv(e, "id");     /* canonical notice URL */
    const char *upd  = jo_sv(e, "updated");
    const char *pub  = jo_sv(e, "published");
    if (!title && !id) { if (single) break; else continue; }

    /* link: prefer an <atom:link> href if present, else the id URL */
    const char *link = id;
    const cJSON *lk = cJSON_GetObjectItem(e, "link");
    if (lk && cJSON_IsObject(lk)) {
      const char *href = jo_sv(lk, "@href");
      if (!href) href = jo_sv(lk, "href");
      if (href) link = href;
    }

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "UK The Gazette");
    cJSON_AddStringToObject(props, "country", "GB");
    if (link) cJSON_AddStringToObject(props, "href", link);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key      = id ? id : title;
    it.title           = title ? title : id;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = pub ? pub : upd;
    it.record_type     = "uk-gazette-notice";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"GAZETTE_WORLD\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);

    if (single) break;
  }

  cJSON_Delete(root);
  fprintf(stderr, "[uk-gazette] emitted %d\n", emitted);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return -1;

  char *q = jo_urlencode(e);   /* UTF-8 safe %-encode of the query */
  if (!q) return 0;

  int total = 0;
  for (int i = 0; i < NGAZ && total < GW_TOTAL_CAP; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    const gazette *g = &GAZ[i];

    char url[1600];
    /* single %s slot; ES/BOE rows carry literal %% for a real [ ] param */
    snprintf(url, sizeof url, g->url, q);

    int room = GW_TOTAL_CAP - total;
    int cap  = room < GW_PER_CAP ? room : GW_PER_CAP;

    int got;
    if (i == 0) {
      /* UK — real keyless JSON API */
      got = uk_json_emit(ctx, sink, url, cap);
    } else {
      /* Everyone else — real fetch + real anchor extraction, filtered on the
       * query bytes so only anchors mentioning the entity survive. JS-only /
       * anti-bot / POST-form / PDF-only rows return 0 (honest empty). */
      got = jo_emit_anchors(ctx, sink, url, g->href, g->name, g->rtype,
                            g->base, e, cap, g->name);
    }
    total += got;
  }
  free(q);
  fprintf(stderr, "[gazette_world] total emitted %d across %d gazettes\n",
          total, NGAZ);
  return 0;   /* honest empty is not an error */
}

static const source_def gazette_world_def = {
  .id = "GAZETTE_WORLD", .collector = "osint",
  .name = "World Official Gazettes",
  .name_ja = "世界 官報検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/gazette_world",
  .description = "Entity pivot across ~15 real national gazettes (UK Gazette JSON API + EU/FR/ES/DE/IT/PT/NL/BE/IE/AU/CA official journals); JSON parse + anchor scrape, honest-empty on JS-only/anti-bot",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(gazette_world_def)
