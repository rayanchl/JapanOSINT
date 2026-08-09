/* INTERPOL Red Notices (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/interpol_red_notices/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, INTERPOL Red Notice corpus (2,901 entities seen in the first 2 MB of the export).
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, firstName/lastName, birthDate, birthPlace, nationality, country, topics, sourceUrl. One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: INTERPOL Red Notices.
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : INTERPOL's own ws-public.interpol.int/notices/v1/red API now answers 403 behind Akamai for every
 * User-Agent tried, including a real browser UA. This export is the only working route to Red
 * Notice data.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "interpol_red_notices",
    .record_type = "os-interpol-red-notices",
    .list_name = "INTERPOL Red Notices",
    .tags_json = "[\"sanctions\",\"wanted\",\"interpol\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-interpol-red-notices");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_interpol_red_notices_def = {
  .id = "os-interpol-red-notices", .collector = "sanctions",
  .name = "INTERPOL Red Notices (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "safety", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/interpol_red_notices/entities.ftm.json",
  .description = "Internationally wanted persons subject to INTERPOL Red Notices, with date and place of birth, nationality and the originating notice URL.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — INTERPOL Red Notices via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_interpol_red_notices_def)
