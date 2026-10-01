/* Ukraine war sanctions register (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/ua_war_sanctions/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, 2,186 entities seen in the first 2 MB of the export.
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, schema, topics, country, referents (cross-list ids). One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: Ukraine war sanctions registers (NSDC / GUR).
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : Ukraine's own endpoints all failed: sanctions.nazk.gov.ua and sanctions-t.rnbo.gov.ua do not
 * resolve in DNS, and every war-sanctions.gur.gov.ua/api path returns a Ukrainian 404 page. This export is
 * the working route. referents cross-match each target to ch-seco-, eu-fsf-, ua-nsdc-, be-fod- and
 * mc-freezes- ids.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "ua_war_sanctions",
    .record_type = "os-ua-war-sanctions",
    .list_name = "Ukraine war sanctions registers (NSDC / GUR)",
    .tags_json = "[\"sanctions\",\"ukraine\",\"russia\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-ua-war-sanctions");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_ua_war_sanctions_def = {
  .id = "os-ua-war-sanctions", .collector = "sanctions",
  .name = "Ukraine war sanctions register (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/ua_war_sanctions/entities.ftm.json",
  .description = "Ukraine's war-related designations — entities and persons enabling the invasion, heavily cross-matched to Western lists.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — Ukraine war sanctions registers (NSDC / GUR) via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_ua_war_sanctions_def)
