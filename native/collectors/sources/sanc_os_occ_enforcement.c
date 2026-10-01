/* US OCC bank enforcement actions (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/us_occ_enfact/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, 4,211 entities seen in the first 2 MB of the export.
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, schema, registrationNumber (OCC charter number), country, topics. One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: OCC enforcement actions (Office of the Comptroller of the Currency).
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : occ.gov's static XML path 404s to an HTML homepage. Rows are a mix of Person and Company
 * schemas — the bank and its institution-affiliated parties — and the schema field distinguishes
 * them.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "us_occ_enfact",
    .record_type = "os-occ-enforcement",
    .list_name = "OCC enforcement actions (Office of the Comptroller of the Currency)",
    .tags_json = "[\"sanctions\",\"regulatory\",\"occ\",\"banking\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-occ-enforcement");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_occ_enforcement_def = {
  .id = "os-occ-enforcement", .collector = "sanctions",
  .name = "US OCC bank enforcement actions (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/us_occ_enfact/entities.ftm.json",
  .description = "Enforcement actions by the US Office of the Comptroller of the Currency against national banks and their institution-affiliated parties.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — OCC enforcement actions (Office of the Comptroller of the Currency) via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_occ_enforcement_def)
