/* Supreme Court of Canada judgments.
 *
 * Endpoint : https://decisions.scc-csc.ca/scc-csc/scc-csc/en/rss.do
 * Format   : RSS 2.0 with a decision: namespace (<decision:date> carries the judgment date).
 * Verified : HTTP 200, newest judgment 2026-07-31 ("SS&C Technologies Canada Corp. v. Bank of
 *            New York Mellon Corp. - 2026 SCC 29 - 2026-07-31").
 * Keyless  : yes.
 * Emits    : style of cause, neutral citation and judgment date (all three parsed out of the
 *            fetched <title>), link to the judgment, subject-matter description.
 * Geometry : NONE (R2).
 * Licence  : SCC decisions may be reproduced without permission with accurate attribution;
 *            reproduced here from the Court's own publication.
 *
 * Notes    : titles are pretty-printed across three lines, so the title is whitespace-collapsed
 *            BEFORE it is split on " - " — a naive split on the raw bytes finds nothing.
 *            CanLII (www.canlii.org) is behind DataDome and cannot be fetched at all; this is
 *            the working route to SCC coverage.
 */
#include "sanc_common.inc"

/* Split "<style of cause> - <neutral citation> - <YYYY-MM-DD>" that the court
 * itself puts in the title. Only re-reads fetched bytes; invents nothing. */
static void scc_enrich(cJSON *props, const sanc_feed_item *fi) {
  const char *title = fi->title;
  char *summary = fi->summary;
  size_t summary_sz = fi->summary_sz;
  if (!title) return;
  const char *d1 = NULL, *d2 = NULL;
  for (const char *p = title; (p = strstr(p, " - ")) != NULL; p += 3) {
    d1 = d2;
    d2 = p;
  }
  if (!d2) return;
  const char *datep = d2 + 3;
  if (strlen(datep) >= 10 && datep[4] == '-' && datep[7] == '-') {
    char iso[11];
    memcpy(iso, datep, 10);
    iso[10] = 0;
    sanc_add(props, "judgment_date", iso);
  }
  if (d1) {
    size_t l = (size_t)(d2 - (d1 + 3));
    if (l && l < 64) {
      char cite[64];
      memcpy(cite, d1 + 3, l);
      cite[l] = 0;
      sanc_add(props, "neutral_citation", cite);
      if (summary && summary_sz && !summary[0])
        snprintf(summary, summary_sz, "%s", cite);
    }
    size_t sl = (size_t)(d1 - title);
    if (sl && sl < 400) {
      char cause[400];
      memcpy(cause, title, sl);
      cause[sl] = 0;
      sanc_add(props, "style_of_cause", cause);
    }
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_feed_cfg cfg = {
    .record_type = "scc-judgment",
    .tags_json = "[\"court\",\"judgment\",\"canada\"]",
    .lang = "en",
    .enrich = scc_enrich,
  };
  int n = sanc_feed_run(ctx, sink, "https://decisions.scc-csc.ca/scc-csc/scc-csc/en/rss.do",
                        &cfg, "scc-canada");
  return n < 0 ? -1 : 0;
}

static const source_def sanc_scc_canada_def = {
  .id = "scc-canada-judgments", .collector = "sanctions",
  .name = "Supreme Court of Canada judgments",
  .update_interval_sec = 21600, .run = run,
  .category = "government", .type = "api",
  .url = "https://decisions.scc-csc.ca/scc-csc/scc-csc/en/rss.do",
  .description = "Canada's apex court decisions with neutral citation, straight from the court rather than via CanLII.",
  .license = "Supreme Court of Canada publication — reproducible without permission with accurate attribution.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_scc_canada_def)
