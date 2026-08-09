/* Autobahn GmbH Germany — motorway roadworks.
 * Endpoints: https://verkehr.autobahn.de/o/autobahn/              (road index)
 *            https://verkehr.autobahn.de/o/autobahn/<road>/services/roadworks
 * Emits: one intel row per roadworks site — identifier, title, subtitle, the
 * road it is on, startTimestamp, the isBlocked flag, the lane-symbol impact
 * object and the display lines, pinned on the site's own coordinate.
 * Keyless.
 * Licence: Autobahn GmbH des Bundes open API — Datenlizenz Deutschland,
 * Namensnennung 2.0.
 *
 * Parse notes: this is a PER-ROAD endpoint, so the ~120 road ids are read from
 * /o/autobahn/ and fanned out. coordinate.lat/.long are STRINGS on some
 * services and numbers on others, and the key is "long" not "lon" — both are
 * handled in trn_autobahn_service. `extent` is "lat1,lon1,lat2,lon2" in LAT,LON
 * order (unusual) and is kept as a verbatim string rather than misread.
 * `description` is an array of display lines, not prose.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = trn_autobahn_service(ctx, sink, "autobahn-de-roadworks", "roadworks",
      "road-roadworks", "[\"transport\",\"road\",\"germany\",\"roadworks\"]",
      NULL);
  if (n < 0) return -1;                     /* the road index itself failed */
  fprintf(stderr, "[autobahn-de-roadworks] emitted %d\n", n);
  return 0;                                 /* parsed fine → 0 (R3) */
}

static const source_def trn_autobahn_de_roadworks_def = {
  .id = "autobahn-de-roadworks", .collector = "transport",
  .name = "Autobahn GmbH Germany — motorway roadworks",
  .update_interval_sec = 3600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://verkehr.autobahn.de/o/autobahn/",
  .description = "Every active and planned roadworks site on a German Autobahn, geolocated, with the affected extent and a lane-symbol impact diagram.",
  .license = "Datenlizenz Deutschland – Namensnennung 2.0 (Autobahn GmbH des Bundes).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_autobahn_de_roadworks_def)
