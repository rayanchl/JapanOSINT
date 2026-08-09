/* Autobahn GmbH Germany — motorway EV charging stations.
 * Endpoints: https://verkehr.autobahn.de/o/autobahn/    (road index)
 *            https://verkehr.autobahn.de/o/autobahn/<road>/services/
 *            electric_charging_station
 * Emits: one intel row per rapid-charging site — identifier, title, subtitle,
 * road, display_type (STRONG_ELECTRIC_CHARGING_STATION vs the normal-power
 * variant), the charge-point operator and the connector/power description
 * lines, pinned on the site's own coordinate. Keyless.
 * Licence: Autobahn GmbH des Bundes open API — Datenlizenz Deutschland,
 * Namensnennung 2.0.
 *
 * Parse note: connector type, power class and the operator are free-text lines
 * inside the description array ("DC Kupplung Combo (CCS)", "200+kW",
 * "Ladesäulenbetreiber: …"), not structured fields — the operator line is
 * extracted by prefix match and the full array is carried through as published.
 * This is a keyless alternative to Open Charge Map, which now refuses unkeyed
 * requests. Per-road endpoint; road ids come from /o/autobahn/.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static void charging_extra(cJSON *pr, const cJSON *item) {
  char *op = trn_desc_line(item, "Ladesäulenbetreiber");
  if (!op) op = trn_desc_line(item, "Ladesaeulenbetreiber");
  if (op) { cJSON_AddStringToObject(pr, "charge_point_operator", op); free(op); }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = trn_autobahn_service(ctx, sink, "autobahn-de-charging-stations",
      "electric_charging_station", "ev-charging-station",
      "[\"transport\",\"road\",\"germany\",\"ev-charging\"]", charging_extra);
  if (n < 0) return -1;
  fprintf(stderr, "[autobahn-de-charging-stations] emitted %d\n", n);
  return 0;
}

static const source_def trn_autobahn_de_charging_stations_def = {
  .id = "autobahn-de-charging-stations", .collector = "transport",
  .name = "Autobahn GmbH Germany — motorway EV charging stations",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "api",
  .url = "https://verkehr.autobahn.de/o/autobahn/",
  .description = "EV rapid-charging sites along German motorways with coordinates, connector type, power class and the charge-point operator.",
  .license = "Datenlizenz Deutschland – Namensnennung 2.0 (Autobahn GmbH des Bundes).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_autobahn_de_charging_stations_def)
