/* San Diego County ZIP code boundaries (Socrata row tier).
 * Endpoint: https://data.sandiegocounty.gov/resource/dg7q-pn9q.json?$limit=100
 * Bare JSON array. Emits, per ZIP: zip, shape_star, shape_stle.
 * R2: the_geom is a full upstream GeoJSON MultiPolygon and is emitted as
 * geometry_geojson verbatim - no centroid is derived from it, so nothing that
 * the publisher did not state appears on the map. Static reference layer, so
 * the poll interval is monthly. Keyless.
 * Licence: County of San Diego open data, public record. */
#include "od_shared.inc"

#define SID "socrata-sandiego-zips"
/* `$offset=0` seeds lib/pagewalk.c's offset walk — pw_walk only ever advances a
 * parameter the URL already carries, and never invents one. This is the one
 * dataset in the family that NEARLY fitted: measured 2026-09-19 it holds 122
 * ZIP polygons, so the single `$limit=100` request was dropping 22 of them —
 * small, permanent, and completely silent. */
static const char *URL =
  "https://data.sandiegocounty.gov/resource/dg7q-pn9q.json"
  "?$limit=100&$offset=0&$select=*,:id&$order=:id";
static const char *const TITLE_KEYS[] = { "zip", NULL };
static const char *const GEOM_KEYS[] = { "the_geom", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "zip-boundary";
  sp.tags_json = "[\"opendata\",\"boundary\",\"us\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.id_key = ":id";   /* rule 4b, measured: zip recurs across rows (100 emitted, 91 stored); :id is Socrata's per-row identity, via $select=*,:id */
  sp.geom_keys = GEOM_KEYS;
  sp.title_prefix = "San Diego County ZIP";
  return od_wrc(SID, od_walk_rows(ctx, sink, SID, URL, NULL, &sp));
}

static const source_def od_socrata_sandiego_zips_def = {
  .id = SID, .collector = "government",
  .name = "San Diego County ZIP code boundaries (Socrata)",
  .update_interval_sec = 2592000, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.sandiegocounty.gov/resource/dg7q-pn9q.json?$limit=100&$order=:id",
  .description = "San Diego County ZIP polygon reference layer with upstream MultiPolygon geometry for point-in-polygon enrichment",
  .license = "County of San Diego open data, public record",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_sandiego_zips_def)
