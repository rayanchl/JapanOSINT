/* US State Dept kleptocracy / human-rights visa bans (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/us_klepto_hr_visa/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, 1,055 entities seen in the first 2 MB of the export.
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, schema, notes (statutory basis + family link), country, topics. One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: US State Department public designations (kleptocracy / human rights visa bans).
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : properties.notes states the statutory basis AND the familial link ("is the adult son of …"),
 * which is the analytic value of this list; it is emitted VERBATIM rather than summarised.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "us_klepto_hr_visa",
    .record_type = "os-us-klepto-visa-bans",
    .list_name = "US State Department public designations (kleptocracy / human rights visa bans)",
    .tags_json = "[\"sanctions\",\"corruption\",\"human-rights\",\"us\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-us-klepto-visa-bans");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_us_klepto_visa_bans_def = {
  .id = "os-us-klepto-visa-bans", .collector = "sanctions",
  .name = "US State Dept kleptocracy / human-rights visa bans (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/us_klepto_hr_visa/entities.ftm.json",
  .description = "Public designations barring corrupt officials, their family members and human-rights abusers from entering the US — a PEP-adjacent list carrying explicit family relationships.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — US State Department public designations (kleptocracy / human rights visa bans) via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_us_klepto_visa_bans_def)
