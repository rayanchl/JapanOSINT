/* collectors/sources/reg_courts.c
 * Legal case-law search across four public court databases, dispatched on
 * ctx->source_id from a single run():
 *   COURT_BAILII  — British & Irish Legal Information Institute (bailii.org)
 *   COURT_CANLII  — Canadian Legal Information Institute (canlii.org, JS UI)
 *   COURT_AUSTLII — Australasian Legal Information Institute (austlii.edu.au)
 *   COURT_CJEU    — EU Court of Justice case law (curia.europa.eu)
 * On-demand entity pivot: ctx->entity = a party / case name. Each source
 * fetches the real server-rendered results page and emits one intel_item per
 * result anchor via jo_emit_anchors (REAL extracted links/titles only). Pages
 * that are JS-rendered (notably CanLII) or shaped differently yield 0 — honest
 * empty, never fabricated. Keyless, all free_tier=1. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *id = ctx->source_id ? ctx->source_id : "";

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[1024];
  int n = 0;

  if (strcmp(id, "COURT_BAILII") == 0) {
    snprintf(url, sizeof url,
      "https://www.bailii.org/cgi-bin/lucy_search_1.cgi?query=%s", enc);
    n = jo_emit_anchors(ctx, sink, url, "/", "COURT_BAILII", "court-case",
                        "https://www.bailii.org", q, 25, "court_bailii");
  } else if (strcmp(id, "COURT_CANLII") == 0) {
    /* CanLII search UI is a client-side SPA (#search fragment); the results
     * are hydrated by JS, so a plain fetch of the fragment URL yields no
     * server-rendered anchors → honest empty is the expected outcome. */
    snprintf(url, sizeof url,
      "https://www.canlii.org/en/#search/text=%s", enc);
    n = jo_emit_anchors(ctx, sink, url, "canlii.org", "COURT_CANLII",
                        "court-case", "https://www.canlii.org", q, 25,
                        "court_canlii");
  } else if (strcmp(id, "COURT_AUSTLII") == 0) {
    snprintf(url, sizeof url,
      "http://www.austlii.edu.au/cgi-bin/sinosrch.cgi?query=%s", enc);
    n = jo_emit_anchors(ctx, sink, url, "/", "COURT_AUSTLII", "court-case",
                        "http://www.austlii.edu.au", q, 25, "court_austlii");
  } else if (strcmp(id, "COURT_CJEU") == 0) {
    snprintf(url, sizeof url,
      "https://curia.europa.eu/juris/liste.jsf?text=%s", enc);
    n = jo_emit_anchors(ctx, sink, url, "juris", "COURT_CJEU", "court-case",
                        "https://curia.europa.eu", q, 25, "court_cjeu");
  } else {
    free(enc);
    return -1;
  }
  free(enc);
  (void)n;
  return 0;   /* honest empty is not an error */
}

static const source_def court_bailii_def = {
  .id = "COURT_BAILII", .collector = "osint",
  .name = "BAILII Case Law Search", .name_ja = "BAILII 判例検索",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "scraped",
  .url = "internal://osint/court_bailii",
  .description = "British & Irish Legal Information Institute — full-text case law search (keyless, anchor-scrape)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(court_bailii_def)

static const source_def court_canlii_def = {
  .id = "COURT_CANLII", .collector = "osint",
  .name = "CanLII Case Law Search", .name_ja = "CanLII 判例検索",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "scraped",
  .url = "internal://osint/court_canlii",
  .description = "Canadian Legal Information Institute — case law search (JS-rendered UI; often honest-empty)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(court_canlii_def)

static const source_def court_austlii_def = {
  .id = "COURT_AUSTLII", .collector = "osint",
  .name = "AustLII Case Law Search", .name_ja = "AustLII 判例検索",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "scraped",
  .url = "internal://osint/court_austlii",
  .description = "Australasian Legal Information Institute — SINO full-text case law search (keyless, anchor-scrape)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(court_austlii_def)

static const source_def court_cjeu_def = {
  .id = "COURT_CJEU", .collector = "osint",
  .name = "CJEU Curia Case Law Search", .name_ja = "EU司法裁判所 判例検索",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "scraped",
  .url = "internal://osint/court_cjeu",
  .description = "Court of Justice of the EU (Curia) — case law list search (keyless, anchor-scrape)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(court_cjeu_def)
