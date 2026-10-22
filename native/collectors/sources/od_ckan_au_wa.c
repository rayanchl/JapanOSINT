/* Western Australia government data catalogue (Landgate-operated).
 * Endpoint: https://catalogue.data.wa.gov.au/api/3/action/package_search?rows=200&start=0
 * Emits, per dataset: title, organisation, notes, author/author_email,
 * access_level and custom_license_url - the last of which points at Landgate
 * terms for some records, so it is emitted verbatim rather than assuming
 * CC-BY. Keyless. Licence: per-dataset (license_title/custom_license_url). */
#include "od_shared.inc"

#define SID "ckan-au-wa"
static const char *URL =
  "https://catalogue.data.wa.gov.au/api/3/action/package_search?rows=200&start=0&sort=id%20asc";  /* exhaustive-ok: walked by pw_walk/pw_walk_max below, which advances the offset or page this URL carries */
/* 2,916 datasets (result.count, measured 2026-10-12) at 200 a page
 * = 15; 30 is that with headroom. */
#define CKAN_PAGE_MAX 30  /* exhaustive-ok: page ceiling handed to pw_walk_max(); a stop at it files a collector-truncation-notice */
static const char *const EXTRA[] = { "notes", "custom_license_url",
                                     "custom_license_title", "access_level",
                                     "contact_point", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_walk(ctx, sink, SID, URL,
                                    "https://catalogue.data.wa.gov.au", EXTRA,
                                    "[\"opendata\",\"ckan\",\"au\"]",
                                    CKAN_PAGE_MAX));
}

static const source_def od_ckan_au_wa_def = {
  .id = SID, .collector = "government",
  .name = "Western Australia catalogue.data.wa.gov.au CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://catalogue.data.wa.gov.au/api/3/action/package_search?rows=200&start=0&sort=id%20asc",  /* exhaustive-ok: metadata field, not a fetch; the row is walked by pw_walk below, which advances the offset this URL carries */
  .description = "Western Australian government data catalogue (2,882 datasets: cadastre, mining tenements, environment) with agency contacts",
  .license = "Per-dataset; custom_license_url (Landgate terms) emitted verbatim",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_au_wa_def)
