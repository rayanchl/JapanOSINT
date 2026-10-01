/* EFTA Court judgments.
 *
 * Endpoint : https://eftacourt.int/feed/
 * Format   : RSS 2.0 (WordPress).
 * Verified : HTTP 200, several judgments in the first ten items
 *            ("Judgment in Case E-23/25 – EFTA Surveillance Authority v Iceland").
 * Keyless  : yes.
 * Emits    : judgment title, case number (E-nn/nn, parsed out of the fetched title), link,
 *            pubDate as ISO-8601 UTC, description.
 * Geometry : NONE (R2).
 * Licence  : Court publication, free to reproduce with attribution to the EFTA Court.
 *
 * Notes    : the channel interleaves "Judgment in Case E-nn/nn" with "Press Release nn/nn"
 *            items about the same case. Only the judgments are emitted (title-prefix filter),
 *            so the row count is deliberately smaller than the feed's item count. This court
 *            covers Iceland, Liechtenstein and Norway under the EEA Agreement — none of its
 *            judgments appear in CURIA.
 */
#include "sanc_common.inc"

/* Case number is the token after "Case " in the court's own title. */
static void efta_enrich(cJSON *props, const sanc_feed_item *fi) {
  const char *title = fi->title;
  char *summary = fi->summary;
  size_t summary_sz = fi->summary_sz;
  if (!title) return;
  const char *c = strstr(title, "Case ");
  if (!c) return;
  c += 5;
  char no[32];
  size_t i = 0;
  while (c[i] && c[i] != ' ' && i + 1 < sizeof no) { no[i] = c[i]; i++; }
  no[i] = 0;
  if (!no[0]) return;
  sanc_add(props, "case_number", no);
  if (summary && summary_sz && !summary[0])
    snprintf(summary, summary_sz, "EFTA Court judgment, Case %s", no);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_feed_cfg cfg = {
    .record_type = "efta-court-judgment",
    .tags_json = "[\"court\",\"judgment\",\"efta\",\"eea\"]",
    .lang = "en",
    .title_prefix = "Judgment in Case",
    .enrich = efta_enrich,
  };
  int n = sanc_feed_run(ctx, sink, "https://eftacourt.int/feed/", &cfg, "efta-court");
  return n < 0 ? -1 : 0;
}

static const source_def sanc_efta_court_def = {
  .id = "efta-court-judgments", .collector = "sanctions",
  .name = "EFTA Court judgments",
  .update_interval_sec = 43200, .run = run,
  .category = "government", .type = "api",
  .url = "https://eftacourt.int/feed/",
  .description = "The court for Iceland, Liechtenstein and Norway under the EEA Agreement — infringement judgments against EEA states and advisory opinions, none of which appear in CURIA.",
  .license = "EFTA Court publication — free to reproduce with attribution.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_efta_court_def)
