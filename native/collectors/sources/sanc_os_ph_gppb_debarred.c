/* Philippines GPPB blacklisted contractors (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/ph_gppb_debarred/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, 218 entities — the complete list.
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, schema, country, topics (debarment), registrationNumber. One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: GPPB blacklisting orders (Philippines Government Procurement Policy Board).
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : Small national debarment list; complements the multilateral development bank coverage with a
 * domestic procurement regulator.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "ph_gppb_debarred",
    .record_type = "os-ph-gppb-debarred",
    .list_name = "GPPB blacklisting orders (Philippines Government Procurement Policy Board)",
    .tags_json = "[\"sanctions\",\"debarment\",\"philippines\",\"procurement\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-ph-gppb-debarred");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_ph_gppb_debarred_def = {
  .id = "os-ph-gppb-debarred", .collector = "sanctions",
  .name = "Philippines GPPB blacklisted contractors (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/ph_gppb_debarred/entities.ftm.json",
  .description = "Contractors blacklisted from Philippine government procurement by the Government Procurement Policy Board.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — GPPB blacklisting orders (Philippines Government Procurement Policy Board) via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_ph_gppb_debarred_def)
