/* Autobahn GmbH Germany — motorway closures.
 * Endpoints: https://verkehr.autobahn.de/o/autobahn/            (road index)
 *            https://verkehr.autobahn.de/o/autobahn/<road>/services/closure
 * Emits: one intel row per full or partial closure — identifier, title,
 * subtitle, road, startTimestamp, the `future` flag, the impact symbol list and
 * the display lines, pinned on the closure's own coordinate. Keyless.
 * Licence: Autobahn GmbH des Bundes open API — Datenlizenz Deutschland,
 * Namensnennung 2.0.
 *
 * Parse notes: same envelope as roadworks but keyed "closure". The `future`
 * boolean separates announced-but-not-yet-active closures from live ones and is
 * emitted as a property so a consumer never presents a scheduled closure as
 * current. Per-road endpoint — the road ids come from /o/autobahn/.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = trn_autobahn_service(ctx, sink, "autobahn-de-closures", "closure",
      "road-closure", "[\"transport\",\"road\",\"germany\",\"closure\"]", NULL);
  if (n < 0) return -1;
  fprintf(stderr, "[autobahn-de-closures] emitted %d\n", n);
  return 0;
}

static const source_def trn_autobahn_de_closures_def = {
  .id = "autobahn-de-closures", .collector = "transport",
  .name = "Autobahn GmbH Germany — motorway closures",
  .update_interval_sec = 1800, .run = run,
  .category = "transport", .type = "api",
  .url = "https://verkehr.autobahn.de/o/autobahn/",
  .description = "Full and partial closures on the German motorway network, including scheduled future closures with their start timestamps.",
  .license = "Datenlizenz Deutschland – Namensnennung 2.0 (Autobahn GmbH des Bundes).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_autobahn_de_closures_def)
