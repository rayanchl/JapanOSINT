/* San Diego County ZIP code boundaries (Socrata row tier).
 * Endpoint: https://data.sandiegocounty.gov/resource/dg7q-pn9q.json?$limit=100
 * Bare JSON array. Emits, per ZIP: zip, shape_star, shape_stle.
 * R2: the_geom is a full upstream GeoJSON MultiPolygon and is emitted as
 * geometry_geojson verbatim - no centroid is derived from it, so nothing that
 * the publisher did not state appears on the map. Static reference layer, so
 * the poll interval is monthly. Keyless.
 * Licence: County of San Diego open data, public record. */
#include "od_shared.c"

#define SID "socrata-sandiego-zips"
static const char *URL =
  "https://data.sandiegocounty.gov/resource/dg7q-pn9q.json?$limit=100";
static const char *const TITLE_KEYS[] = { "zip", NULL };
static const char *const GEOM_KEYS[] = { "the_geom", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "zip-boundary";
  sp.tags_json = "[\"opendata\",\"boundary\",\"us\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.id_key = "zip";
  sp.geom_keys = GEOM_KEYS;
  sp.title_prefix = "San Diego County ZIP";
  return od_rc(SID, od_fetch_rows(ctx, sink, URL, NULL, &sp));
}

static const source_def od_socrata_sandiego_zips_def = {
  .id = SID, .collector = "government",
  .name = "San Diego County ZIP code boundaries (Socrata)",
  .update_interval_sec = 2592000, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.sandiegocounty.gov/resource/dg7q-pn9q.json?$limit=100",
  .description = "San Diego County ZIP polygon reference layer with upstream MultiPolygon geometry for point-in-polygon enrichment",
  .license = "County of San Diego open data, public record",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_sandiego_zips_def)
