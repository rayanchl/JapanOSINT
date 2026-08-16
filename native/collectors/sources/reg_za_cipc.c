/* collectors/sources/reg_za_cipc.c
 * OSINT service — ZA_CIPC. On-demand entity pivot (entity = South African
 * company / enterprise / court-party name) across two REAL, public South
 * African portals:
 *   - CIPC (Companies and Intellectual Property Commission) public enterprise
 *     search — eservices.cipc.co.za. The full CIPC transactional portal is
 *     login-walled (BizPortal / e-Services accounts), so full company detail
 *     is not scrapeable; we hit the public EnterpriseSearch page and extract
 *     whatever real result anchors it server-renders.
 *   - SAFLII (Southern African Legal Information Institute) — saflii.org free
 *     case-law search, which surfaces litigation involving the entity.
 *
 * run() actually FETCHES each portal's server-rendered search page and emits
 * ONE intel_item per real <a> hit via jo_emit_anchors (real anchor extraction;
 * nothing synthesized). Output is capped so one query can't fan out forever.
 *
 * HONESTY: CIPC's search is largely JS / ASP.NET-postback and login-gated, and
 * SAFLII may rate-limit — those simply yield 0 (honest empty), which is
 * expected and fine. We NEVER fabricate a company, registration number, or
 * case. Honest-empty beats fake. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* One ZA public search portal.
 *   url   — printf template with a single %s slot for the URL-encoded query
 *   href  — substring an anchor's href must contain to count as a real hit
 *           (NULL = accept any anchor); keeps site chrome out of the results
 *   base  — prepended to root-relative hrefs
 *   rtype — record_type stamped on each emitted item */
typedef struct {
  const char *name;
  const char *url;
  const char *href;
  const char *base;
  const char *rtype;
} za_portal;

static const za_portal PORTALS[] = {
  /* CIPC public enterprise search (transactional detail is login-walled). */
  { "CIPC Enterprise Search (ZA)",
    "https://eservices.cipc.co.za/EnterpriseSearch.aspx?searchtext=%s",
    NULL, "https://eservices.cipc.co.za", "za-company" },
  /* SAFLII free case-law search — litigation involving the entity. */
  { "SAFLII Case Law (ZA)",
    "https://www.saflii.org/cgi-bin/sinosrch.cgi?method=auto&query=%s",
    "/za/cases/", "https://www.saflii.org", "za-court" },
};
static const int NPORTAL = (int)(sizeof(PORTALS) / sizeof(PORTALS[0]));

#define ZA_TOTAL_CAP     500  /* exhaustive-ok: runaway guard, logged */
#define ZA_PER_PORT_CAP  15   /* at most ~15 hits per portal                  */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return -1;

  char *q = jo_urlencode(e);   /* UTF-8 safe %-encode of the query */
  if (!q) return 0;

  int total = 0;
  for (int i = 0; i < NPORTAL && total < ZA_TOTAL_CAP; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    const za_portal *p = &PORTALS[i];

    char url[1200];
    snprintf(url, sizeof url, p->url, q);

    int room = ZA_TOTAL_CAP - total;
    int cap  = room < ZA_PER_PORT_CAP ? room : ZA_PER_PORT_CAP;

    /* Real fetch + real anchor extraction. Filter on the query bytes so we
     * only keep anchors whose visible text or href actually mentions the
     * entity — avoids site chrome. JS-only / login-gated rows return 0. */
    int got = jo_emit_anchors(ctx, sink, url, p->href, p->name, p->rtype,
                              p->base, e, cap, p->name);
    total += got;
  }
  free(q);
  fprintf(stderr, "[za_cipc] total emitted %d across %d portals\n",
          total, NPORTAL);
  return 0;   /* honest empty is not an error */
}

static const source_def za_cipc_def = {
  .id = "ZA_CIPC", .collector = "osint",
  .name = "South Africa CIPC & SAFLII",
  .name_ja = "南アフリカ CIPC・SAFLII 検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/reg_za_cipc",
  .description = "Entity pivot across South Africa's CIPC public enterprise search (login-walled detail → honest-empty) and SAFLII free case law; anchor scrape.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(za_cipc_def)
