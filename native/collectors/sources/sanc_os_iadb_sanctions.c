/* Inter-American Development Bank sanctioned firms (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/iadb_sanctions/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, 2,506 entities seen in the first 2 MB of the export.
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, schema, topics, country, referents. One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: IDB Group Sanctions List (Inter-American Development Bank).
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : iadb.org serves a JS-hydrated page with no data endpoint behind it.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "iadb_sanctions",
    .record_type = "os-iadb-sanctions",
    .list_name = "IDB Group Sanctions List (Inter-American Development Bank)",
    .tags_json = "[\"sanctions\",\"debarment\",\"iadb\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-iadb-sanctions");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_iadb_sanctions_def = {
  .id = "os-iadb-sanctions", .collector = "sanctions",
  .name = "Inter-American Development Bank sanctioned firms (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/iadb_sanctions/entities.ftm.json",
  .description = "IDB Group debarment list covering Latin American and Caribbean procurement.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — IDB Group Sanctions List (Inter-American Development Bank) via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_iadb_sanctions_def)
