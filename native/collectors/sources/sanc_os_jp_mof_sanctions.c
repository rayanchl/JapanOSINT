/* Japan MOF economic sanctions list (OpenSanctions export)
 *
 * Endpoint : https://data.opensanctions.org/datasets/latest/jp_mof_sanctions/entities.ftm.json
 * Format   : NDJSON — one FollowTheMoney entity per LINE (the payload as a whole is NOT a
 *            JSON document). Every property value is an ARRAY even when single-valued.
 * Verified : HTTP 200, 1,476 entities seen in the first 2 MB of the export.
 * Keyless  : yes, no credential of any kind.
 * Emits    : name, schema, topics, country, program, notes. One intel row per named entity;
 *            Address/relationship schemas are skipped so no row is a bare street address.
 * Geometry : NONE (R2). Nothing in this list is a place; a designated party is never pinned
 *            to a country centroid.
 *
 * LICENCE — CC-BY-NC 4.0, AND THAT MATTERS:
 *   Underlying data: Japan Ministry of Finance economic sanctions designations.
 *   Redistributed by OpenSanctions (https://www.opensanctions.org) under
 *   CREATIVE COMMONS ATTRIBUTION-NONCOMMERCIAL 4.0. Two obligations ride on every row:
 *     (1) ATTRIBUTION IS REQUIRED — carried in properties.attribution / properties.license
 *         on each emitted row and in source_def.license below;
 *     (2) NON-COMMERCIAL USE ONLY — a commercial deployment of this platform MUST obtain a
 *         paid OpenSanctions licence before shipping this source. The product owner chose to
 *         build it anyway with that restriction understood. Do not delete this notice.
 *
 * Notes    : The MOF page itself (mof.go.jp/.../economic_sanctions/list.html) is a Japanese HTML index linking
 * to PDF and Excel attachments and is not parseable as a feed. In this export Address entities are
 * SEPARATE rows linked to their target by addressEntity ids; sanc_os_skip_schema() drops them so a
 * street address is never emitted as if it were a designated party. Companion dataset jp_meti_eul
 * covers the METI End User List (export control).
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_os_cfg cfg = {
    .dataset = "jp_mof_sanctions",
    .record_type = "os-jp-mof-sanctions",
    .list_name = "Japan Ministry of Finance economic sanctions designations",
    .tags_json = "[\"sanctions\",\"japan\",\"mof\",\"opensanctions\"]",
  };
  int n = sanc_opensanctions_run(ctx, sink, &cfg, "os-jp-mof-sanctions");
  /* R3: n<0 is a real fetch/parse failure; an export that fetched fine and held
   * nothing we could name is a CLEAN empty, not an error. Never `return n`. */
  return n < 0 ? -1 : 0;
}

static const source_def sanc_os_jp_mof_sanctions_def = {
  .id = "os-jp-mof-sanctions", .collector = "sanctions",
  .name = "Japan MOF economic sanctions list (OpenSanctions export)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.opensanctions.org/datasets/latest/jp_mof_sanctions/entities.ftm.json",
  .description = "Japan's own asset-freeze designations under the Foreign Exchange and Foreign Trade Act — the national list for a Japan-focused platform, machine-readable.",
  .license = "CC-BY-NC 4.0 (NON-COMMERCIAL ONLY, attribution required) — Japan Ministry of Finance economic sanctions designations via OpenSanctions.org. Commercial use requires a paid OpenSanctions licence.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_os_jp_mof_sanctions_def)
