/* Find Case Law (The National Archives) — latest published judgments, Atom feed.
 *
 * Endpoint : https://caselaw.nationalarchives.gov.uk/atom.xml?per_page=50
 * Format   : Atom 1.0 (NOT RSS — <entry>, and the link is an href attribute) with a tna:
 *            namespace carrying the neutral citation.
 * Verified : HTTP 200, feed updated 2026-07-31T15:01Z; entries such as "BB, R (on the
 *            application of) v The Commissioner of Police of the Metropolis",
 *            tna:identifier type="ukncn" -> "[2026] EWHC 1986 (Admin)".
 * Keyless  : yes.
 * Emits    : case name, judgment URL, first-published/updated timestamps, the court (from
 *            <author><name>) and the neutral citation + document slug from tna:identifier.
 * Geometry : NONE (R2).
 * Licence  : Open Justice Licence (The National Archives). Polling this public Atom feed for
 *            new judgments is within the standard terms; BULK or computational reuse of the
 *            whole corpus needs a separate licence from TNA — do not turn this into a
 *            crawler over the full archive.
 *
 * Notes    : this host appears elsewhere in the repo only as a %s web-search URL in
 *            courts_world.c, which fetches nothing. per_page is capped at 50 by the API.
 */
#include "sanc_common.inc"

/* Pull the court name and the UK neutral citation out of the entry XML. Both
 * are values the feed returned; nothing is derived or guessed. */
static void tna_enrich(cJSON *props, const sanc_feed_item *fi) {
  const char *b = fi->raw, *e = fi->raw_end;
  if (!b || !e) return;

  /* <author><name>High Court (Administrative Court)</name></author> */
  const char *cur = b;
  sanc_el au;
  if (sanc_xml_next(&cur, e, "author", &au)) {
    char *court = sanc_xml_text(au.body, au.body_end, "name");
    if (court) {
      sanc_collapse_ws(court);
      sanc_add(props, "court", court);
      if (fi->summary && fi->summary_sz && !fi->summary[0])
        snprintf(fi->summary, fi->summary_sz, "%s", court);
      free(court);
    }
  }
  /* <tna:identifier slug="ewhc/admin/2026/1986" type="ukncn">[2026] EWHC 1986 (Admin)</> */
  cur = b;
  sanc_el id;
  while (sanc_xml_next(&cur, e, "identifier", &id)) {
    char *type = sanc_xml_attr(&id, "type");
    if (type && strcmp(type, "ukncn") == 0) {
      char *cite = sanc_decode(id.body, (size_t)(id.body_end - id.body));
      char *slug = sanc_xml_attr(&id, "slug");
      if (cite && cite[0]) sanc_add(props, "neutral_citation", cite);
      sanc_add(props, "document_slug", slug);
      free(cite);
      free(slug);
      free(type);
      return;
    }
    free(type);
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_feed_cfg cfg = {
    .record_type = "uk-judgment",
    .tags_json = "[\"court\",\"judgment\",\"uk\"]",
    .lang = "en",
    .enrich = tna_enrich,
  };
  int n = sanc_feed_run(ctx, sink,
                        "https://caselaw.nationalarchives.gov.uk/atom.xml?per_page=50",
                        &cfg, "uk-caselaw");
  return n < 0 ? -1 : 0;
}

static const source_def sanc_uk_caselaw_def = {
  .id = "uk-caselaw-atom", .collector = "sanctions",
  .name = "Find Case Law (National Archives) — latest judgments",
  .update_interval_sec = 10800, .run = run,
  .category = "government", .type = "api",
  .url = "https://caselaw.nationalarchives.gov.uk/atom.xml?per_page=50",
  .description = "Every judgment and tribunal decision published by the courts of England and Wales as it lands, with neutral citation and court.",
  .license = "Open Justice Licence (The National Archives). Bulk/computational reuse of the full corpus requires a separate licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_uk_caselaw_def)
