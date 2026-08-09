/* AMF France Enforcement Committee sanctions (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/fr_amf_regulatory_sanctions/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, 838 entities seen in the first 2 MB of the export.
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, topics, sourceUrl (per-decision link), country. One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: AMF Commission des sanctions (France).
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : amf-france.org's news RSS path 404s. properties.sourceUrl carries the citable decision URL for
 * each row and is used as the row link.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "fr_amf_regulatory_sanctions",
    .record_type = "os-amf-france-sanctions",
    .list_name = "AMF Commission des sanctions (France)",
    .tags_json = "[\"sanctions\",\"regulatory\",\"amf\",\"france\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-amf-france-sanctions");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_amf_france_sanctions_def = {
  .id = "os-amf-france-sanctions", .collector = "sanctions",
  .name = "AMF France Enforcement Committee sanctions (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/fr_amf_regulatory_sanctions/entities.ftm.json",
  .description = "French financial market regulator sanction decisions naming firms and individuals, each linked to the published decision.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — AMF Commission des sanctions (France) via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_amf_france_sanctions_def)
