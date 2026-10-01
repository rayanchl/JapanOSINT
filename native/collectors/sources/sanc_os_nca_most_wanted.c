/* UK National Crime Agency most wanted (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/gb_nca_most_wanted/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, 23 entities — the complete list.
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, topics, notes (offence narrative), sourceUrl. One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: NCA most wanted (UK National Crime Agency).
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : Tiny file. properties.notes carries the alleged-offence narrative verbatim and is used as the row
 * subtitle.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "gb_nca_most_wanted",
    .record_type = "os-nca-most-wanted",
    .list_name = "NCA most wanted (UK National Crime Agency)",
    .tags_json = "[\"wanted\",\"organised-crime\",\"nca\",\"uk\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-nca-most-wanted");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_nca_most_wanted_def = {
  .id = "os-nca-most-wanted", .collector = "sanctions",
  .name = "UK National Crime Agency most wanted (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "safety", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/gb_nca_most_wanted/entities.ftm.json",
  .description = "The UK's most-wanted serious and organised crime fugitives, with the alleged offence in plain text.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — NCA most wanted (UK National Crime Agency) via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_nca_most_wanted_def)
