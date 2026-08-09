/* Asian Development Bank debarred parties (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/adb_sanctions/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, 2,807 entities seen in the first 2 MB of the export.
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, schema, topics, country, registrationNumber, referents (cross-MDB ids). One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: ADB Sanctions List (Asian Development Bank).
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : adb.org sits behind a Cloudflare interstitial and cannot be fetched server-side. The `referents`
 * array carried on each row shows which OTHER multilateral development bank lists (iadb-, ebrd-,
 * wbdeb-, afdb-) name the same party, which is how a cross-debarment is detected without
 * re-fetching those lists.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "adb_sanctions",
    .record_type = "os-adb-debarred",
    .list_name = "ADB Sanctions List (Asian Development Bank)",
    .tags_json = "[\"sanctions\",\"debarment\",\"adb\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-adb-debarred");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_adb_debarred_def = {
  .id = "os-adb-debarred", .collector = "sanctions",
  .name = "Asian Development Bank debarred parties (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/adb_sanctions/entities.ftm.json",
  .description = "Firms and individuals debarred by the Asian Development Bank, including cross-debarments recognised from the other multilateral development banks.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — ADB Sanctions List (Asian Development Bank) via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_adb_debarred_def)
