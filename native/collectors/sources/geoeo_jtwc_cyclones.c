/* US Joint Typhoon Warning Center tropical cyclone feed — the Western Pacific,
 * Indian Ocean and Southern Hemisphere basins NHC does not cover.
 *
 * Endpoint (keyless): https://www.metoc.navy.mil/jtwc/rss/jtwc.rss
 * Emits: the RSS item title, description, link and pubDate for each current
 *        JTWC warning/product, via the shared rss_collect() toolkit (uid =
 *        guid → link → sha1(title|pubDate)).
 * Licence: US Navy public domain. Note the feed's own <link> points at an
 *          s3.amazonaws.com mirror of the JTWC page.
 *
 * GEOMETRY (from parse_notes): plain RSS 2.0, no geo namespace and no
 * coordinates anywhere in the feed — the storm position lives inside the
 * linked warning TEXT file, a second fetch. Every row is therefore emitted with
 * no position at all rather than a basin-centre guess (R2). rss_collect never
 * sets has_geo, so that is what happens here.
 *
 * Distinct host and product from the NHC sources; do not confuse the two.
 */
#include "../../source.h"
#include "../../lib/rss_atom.h"
#include <stdio.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = rss_collect(ctx, sink, "https://www.metoc.navy.mil/jtwc/rss/jtwc.rss",
                      "en", "[\"cyclone\",\"typhoon\",\"jtwc\",\"military\"]");
  if (n < 0) {
    fprintf(stderr, "[jtwc-cyclones] fetch/parse failed\n");
    return -1;
  }
  fprintf(stderr, "[jtwc-cyclones] emitted %d\n", n);
  return 0;                                   /* zero items is R3 return 0 */
}

static const source_def geoeo_jtwc_def = {
  .id = "jtwc-cyclones", .collector = "environment",
  .name = "US Joint Typhoon Warning Center Feed",
  .update_interval_sec = 1800, .run = run,
  .category = "environment", .type = "api",
  .url = "https://www.metoc.navy.mil/jtwc/rss/jtwc.rss",
  .description = "JTWC (US Navy/Air Force) tropical cyclone warnings for the Western Pacific, Indian Ocean and Southern Hemisphere — the operational counterpart to JMA's typhoon products west of the dateline.",
  .license = "US Navy public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_jtwc_def)
