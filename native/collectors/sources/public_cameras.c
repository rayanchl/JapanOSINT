/* collectors/sources/public_cameras.c — port of
 * server/src/collectors/publicCameras.js. Primary live path = tryLive()
 * Overpass man_made=surveillance; the curated CAMERAS seed list is
 * intentionally not ported (rule 7).
 *
 * Emits through camera_upsert(), NOT the default geojson path, so its rows land
 * in the camera keyspace (record_type='camera', uid "camera-discovery|<uid>")
 * alongside the fourteen cam_* channels. It used to emit plain intel rows under
 * uid "public-cameras|h:<hash>" with record_type='public-cameras', which
 * camera_fc_json cannot see — so its geocoded OSM surveillance cameras were
 * collected and then never rendered on the cameras layer. Same makeFeature
 * shape and property order as every other channel (camera_uid, name,
 * camera_type, discovery_channel, country, then extras), so the merge
 * semantics — discovery_channels[] union, existing-non-null-wins, seen_count++,
 * first_seen_at preservation — apply here identically.
 * discovery_channel = 'osm_surveillance'. */
#include "../../source.h"
#include "../../core/camera_store.h"
#include "../../lib/camfeature.h"
#include "../../lib/overpass.h"
#include <stdio.h>

/* ── Overpass element → camera Feature ─────────────────────────────────────*/
static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;

  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;

  char namebuf[64];
  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (!nm) {
    snprintf(namebuf, sizeof namebuf, "Camera %lld", oid);
    nm = namebuf;
  }

  /* surveillance=public|traffic|webcam is the closest thing OSM carries to the
   * aggregators' camera_type, so it maps straight through behind an
   * osm_surveillance_ prefix that keeps the provenance legible. */
  const char *sv = ov_tag(el, "surveillance");
  char typebuf[64];
  snprintf(typebuf, sizeof typebuf, "osm_surveillance_%s", sv ? sv : "public");

  char oidbuf[32];
  snprintf(oidbuf, sizeof oidbuf, "%lld", oid);

  cam_kv ex[4] = {
    { .k = "url",               .sv = ov_tag(el, "url")               },
    { .k = "surveillance_type", .sv = ov_tag(el, "surveillance:type") },
    { .k = "operator",          .sv = ov_tag(el, "operator")          },
    { .k = "osm_id",            .sv = oidbuf                          },
  };
  return cam_make_feature(lat, lon, nm, typebuf, "osm_surveillance", ex, 4);
}

typedef struct { db_handle *db; intel_sink *sink; } emit_ctx;

static int emit_camera(cJSON *f, void *ud) {
  emit_ctx *e = ud;
  return camera_upsert(e->db, e->sink, f, "osm_surveillance");
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  emit_ctx e = { .db = ctx->db, .sink = sink };
  int n = overpass_collect_via(ctx,
    "node[\"man_made\"=\"surveillance\"]"
    "[\"surveillance\"~\"public|traffic|webcam\"](area.jp);",
    180, 60000, map, NULL, emit_camera, &e);
  if (n >= 0) fprintf(stderr, "[public-cameras] upserted %d cameras\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def public_cameras_def = {
  .id = "public-cameras", .collector = "camera-discovery",
  .name = "Public Cameras", .name_ja = "公開カメラ",
   .layer = "cameras",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(public_cameras_def)
