/* US FinCEN enforcement actions (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/us_fincen_enforcement/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, 72 entities — the complete list.
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, country, sector, topics, sourceUrl. One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: FinCEN enforcement actions (US Treasury).
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : fincen.gov has no working feed (/news/news-releases/feed returns a 404 page). Small file, fully
 * covered by the default byte budget.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "us_fincen_enforcement",
    .record_type = "os-fincen-enforcement",
    .list_name = "FinCEN enforcement actions (US Treasury)",
    .tags_json = "[\"sanctions\",\"regulatory\",\"fincen\",\"aml\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-fincen-enforcement");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_fincen_enforcement_def = {
  .id = "os-fincen-enforcement", .collector = "sanctions",
  .name = "US FinCEN enforcement actions (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/us_fincen_enforcement/entities.ftm.json",
  .description = "AML/BSA penalties issued by FinCEN against banks, money services businesses, casinos and crypto exchanges.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — FinCEN enforcement actions (US Treasury) via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_fincen_enforcement_def)
