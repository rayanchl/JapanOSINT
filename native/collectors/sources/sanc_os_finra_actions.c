/* FINRA disciplinary actions and barred brokers (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/us_finra_actions/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, 1,663 entities seen in the first 2 MB of the export.
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, country, topics (reg.action), sourceUrl. One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: FINRA disciplinary actions (US securities industry).
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : finra.org publishes only a JS search UI; /rss/disciplinary-actions returns the site shell, not a
 * feed. A companion dataset `us_finra_barred` covers the bar list specifically.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "us_finra_actions",
    .record_type = "os-finra-actions",
    .list_name = "FINRA disciplinary actions (US securities industry)",
    .tags_json = "[\"sanctions\",\"regulatory\",\"finra\",\"securities\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-finra-actions");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_finra_actions_def = {
  .id = "os-finra-actions", .collector = "sanctions",
  .name = "FINRA disciplinary actions and barred brokers (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/us_finra_actions/entities.ftm.json",
  .description = "US broker-dealer registrants disciplined or barred by FINRA — the securities-industry conduct list that has no public API of its own.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — FINRA disciplinary actions (US securities industry) via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_finra_actions_def)
