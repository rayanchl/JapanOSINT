/* US DEA fugitives (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/us_dea_fugitives/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, 538 entities seen in the first 2 MB of the export.
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, schema, topics, birthDate, country, referents (ofac-/usgsa- ids). One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: DEA fugitive listings (US Drug Enforcement Administration).
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : dea.gov returns 403 (Akamai) to any non-browser client. The `referents` array is the cheapest way
 * to link a fugitive to their OFAC designation, and is emitted as cross_referenced_ids.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "us_dea_fugitives",
    .record_type = "os-dea-fugitives",
    .list_name = "DEA fugitive listings (US Drug Enforcement Administration)",
    .tags_json = "[\"wanted\",\"narcotics\",\"dea\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-dea-fugitives");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_dea_fugitives_def = {
  .id = "os-dea-fugitives", .collector = "sanctions",
  .name = "US DEA fugitives (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "safety", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/us_dea_fugitives/entities.ftm.json",
  .description = "Narcotics fugitives sought by the US DEA, cross-linked to OFAC designations where the same person is sanctioned.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — DEA fugitive listings (US Drug Enforcement Administration) via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_dea_fugitives_def)
