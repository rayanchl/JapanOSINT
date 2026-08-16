/* Autobahn GmbH Germany — motorway lorry parking and rest areas.
 * Endpoints: https://verkehr.autobahn.de/o/autobahn/         (road index)
 *            https://verkehr.autobahn.de/o/autobahn/<road>/services/parking_lorry
 * Emits: one intel row per rest area / lorry parking site — the stable
 * DE-<state>-<n> identifier, title, subtitle, road, the on-site facility list
 * and the car (PKW) and lorry (LKW) space counts, pinned on the site's own
 * coordinate. Keyless.
 * Licence: Autobahn GmbH des Bundes open API — Datenlizenz Deutschland,
 * Namensnennung 2.0.
 *
 * Parse note: the space counts are NOT structured fields — they are embedded in
 * German free-text description lines ("PKW Stellplätze: 20", "LKW Stellplätze:
 * 8"), so they are extracted by prefix match and only added when the line is
 * actually present. Per-road endpoint; road ids come from /o/autobahn/.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static void parking_extra(cJSON *pr, const cJSON *item) {
  double v;
  if (trn_desc_number(item, "PKW Stellpl", &v))
    cJSON_AddNumberToObject(pr, "car_spaces", v);
  if (trn_desc_number(item, "LKW Stellpl", &v))
    cJSON_AddNumberToObject(pr, "lorry_spaces", v);
  const cJSON *icons = cJSON_GetObjectItem(item, "lorryParkingFeatureIcons");
  if (cJSON_IsArray(icons))
    cJSON_AddItemToObject(pr, "facility_icons", cJSON_Duplicate(icons, 1));
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = trn_autobahn_service(ctx, sink, "autobahn-de-lorry-parking",
      "parking_lorry", "motorway-rest-area",
      "[\"transport\",\"road\",\"germany\",\"freight\"]", parking_extra);
  if (n < 0) return -1;
  fprintf(stderr, "[autobahn-de-lorry-parking] emitted %d\n", n);
  return 0;
}

static const source_def trn_autobahn_de_lorry_parking_def = {
  .id = "autobahn-de-lorry-parking", .collector = "transport",
  .name = "Autobahn GmbH Germany — motorway lorry parking areas",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "api",
  .url = "https://verkehr.autobahn.de/o/autobahn/",
  .description = "Every rest area and lorry parking site on the German Autobahn network with coordinates, car/truck space counts and on-site facilities.",
  .license = "Datenlizenz Deutschland – Namensnennung 2.0 (Autobahn GmbH des Bundes).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_autobahn_de_lorry_parking_def)
