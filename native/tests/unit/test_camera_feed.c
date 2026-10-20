/* tests/unit/test_camera_feed.c — camera record properties → capture targets
 * (core/camera_feed.c). No network, no database.
 *
 * WHAT IS BEING PROTECTED. camera_stills used to read six top-level keys, so
 * every camera record outside the camera-discovery keyspace resolved to
 * "no_feed_url" forever. Each fixture below is a property blob in the exact
 * shape a real collector writes — the source file is named beside it — and
 * the assertion is the target(s) the stills pod would now capture:
 *
 *   1. camera_store.c (camera-discovery): page `url` + `thumbnail_url` → the
 *      thumbnail, never the page.
 *   2. trn_digitraffic_weathercam_stations.c: no top-level image, one
 *      image_url per preset in views[] → one target per preset, suffixed by
 *      the preset id, labelled by its presentation name.
 *   3. trn_common.inc trn_511_cameras (Alberta/Ontario): views[].url with no
 *      id → index suffixes; the description is the label.
 *   4. hpengine-flattened ArcGIS (hp3b29a_webcams.c Iowa DOT): upstream case
 *      (`attributes.ImageURL`) beats `attributes.VideoURL` and never takes the
 *      hp `endpoint` (the LIST url) for a feed.
 *   5. hpengine-flattened arrays (`views.0.url`) are the same as nested ones.
 *   6. The per-source override: Alaska RWIS slots live INSIDE the key name.
 *   7. Case/underscore variants: IMAGE_URL, ImageUrl, cameraimageurl, cam_img.
 *   8. No feed: ftp, a bare filename, invalid JSON → 0 targets.
 *   9. Duplicate URL (station repeats its default view) → one target.
 *  10. A record with >CAM_FEED_MAX_TARGETS views reports the real total.
 *  11. Target ids round-trip, including a uid that itself contains '#'.
 */
#include "../../core/camera_feed.c"

#include <assert.h>

static int resolve(const char *props, const char *sid, camera_feed_target *t) {
  return camera_feed_resolve(props, sid, t, CAM_FEED_MAX_TARGETS);
}

static void t_discovery(void) {
  camera_feed_target t[CAM_FEED_MAX_TARGETS];
  const char *p =
    "{\"camera_uid\":\"windy_1234\",\"name\":\"Shibuya crossing\","
    "\"camera_type\":\"webcam\",\"discovery_channel\":\"windy_api\","
    "\"discovery_channels\":[\"windy_api\"],\"country\":\"JP\","
    "\"url\":\"https://www.windy.com/webcams/1234\","
    "\"thumbnail_url\":\"https://imgproxy.windy.com/_/preview/1234.jpg\","
    "\"first_seen_at\":\"2026-10-01T00:00:00Z\",\"seen_count\":3}";
  int n = resolve(p, "camera-discovery", t);
  assert(n == 1);
  assert(strcmp(t[0].url, "https://imgproxy.windy.com/_/preview/1234.jpg") == 0);
  assert(t[0].suffix[0] == '\0');
  assert(strcmp(t[0].key, "thumbnail_url") == 0);
  camera_feed_free(t, n);

  /* Only the page: still one target (the pod finds out it is HTML and says
   * not_an_image — the record is not hidden). */
  n = resolve("{\"url\":\"https://example.jp/live/cam1\"}", NULL, t);
  assert(n == 1 && strcmp(t[0].key, "url") == 0);
  camera_feed_free(t, n);
}

static void t_digitraffic(void) {
  camera_feed_target t[CAM_FEED_MAX_TARGETS];
  const char *p =
    "{\"operator\":\"Fintraffic\",\"station_id\":\"C01502\","
    "\"name\":\"Tie 51 Espoo, Kivenlahti\",\"collection_status\":\"GATHERING\","
    "\"preset_count\":3,\"presets_in_collection\":2,\"views\":["
    "{\"preset_id\":\"C0150201\",\"presentation_name\":\"Helsinkiin\","
    "\"direction\":\"1\",\"image_url\":\"https://weathercam.digitraffic.fi/C0150201.jpg\"},"
    "{\"preset_id\":\"C0150202\",\"presentation_name\":\"Kirkkonummelle\","
    "\"direction\":\"2\",\"image_url\":\"https://weathercam.digitraffic.fi/C0150202.jpg\"}]}";
  int n = resolve(p, "digitraffic-weathercam-stations", t);
  assert(n == 2);
  assert(strcmp(t[0].suffix, "views/C0150201") == 0);
  assert(strcmp(t[0].label, "Helsinkiin") == 0);
  assert(strcmp(t[0].url, "https://weathercam.digitraffic.fi/C0150201.jpg") == 0);
  assert(strcmp(t[0].key, "views.0.image_url") == 0);
  assert(strcmp(t[1].suffix, "views/C0150202") == 0);
  camera_feed_free(t, n);
}

static void t_511(void) {
  camera_feed_target t[CAM_FEED_MAX_TARGETS];
  const char *p =
    "{\"operator\":\"Government of Alberta (511)\",\"camera_id\":\"AB--123\","
    "\"source\":\"AB\",\"roadway\":\"Hwy 2\",\"direction\":\"Northbound\","
    "\"location\":\"Hwy 2 at Hwy 2A\",\"view_count\":2,\"views_enabled\":1,"
    "\"views\":[{\"url\":\"https://511.alberta.ca/map/Cctv/123--1\","
    "\"status\":\"Enabled\",\"description\":\"Looking North\"},"
    "{\"url\":\"https://511.alberta.ca/map/Cctv/123--2\","
    "\"status\":\"Disabled\",\"description\":\"Looking South\"}]}";
  int n = resolve(p, "511-alberta-cameras", t);
  assert(n == 2);
  assert(strcmp(t[0].suffix, "views/0") == 0);
  assert(strcmp(t[1].suffix, "views/1") == 0);
  assert(strcmp(t[0].label, "Looking North") == 0);
  camera_feed_free(t, n);
}

static void t_hp_flat(void) {
  camera_feed_target t[CAM_FEED_MAX_TARGETS];
  const char *p =
    "{\"attributes.OBJECTID\":17,\"attributes.Route\":\"I-80\","
    "\"attributes.ImageName\":\"DMTV07\","
    "\"attributes.ImageURL\":\"https://atmsiowa.com/snapshots/DMTV07.jpg\","
    "\"attributes.VideoURL\":\"https://iowadotsfs1.us-east-1.skyvdn.com/rtplive/dmtv07/playlist.m3u8\","
    "\"geometry.x\":-93.6,\"geometry.y\":41.6,"
    "\"service\":\"Iowa DOT cameras\",\"source_id\":\"JO29A_CAM_IOWA_DOT_TRAFFIC_CAMERAS\","
    "\"endpoint\":\"https://services.arcgis.com/x/FeatureServer/0/query?f=json\","
    "\"real_fetch\":true}";
  int n = resolve(p, NULL, t);
  assert(n == 1);
  assert(strcmp(t[0].url, "https://atmsiowa.com/snapshots/DMTV07.jpg") == 0);
  assert(strcmp(t[0].key, "attributes.ImageURL") == 0);
  camera_feed_free(t, n);

  /* Only a stream: the stream is the target. */
  n = resolve("{\"attributes.VideoUrl\":\"https://x.example/live.m3u8\","
              "\"endpoint\":\"https://e.example/q\"}", NULL, t);
  assert(n == 1 && strcmp(t[0].url, "https://x.example/live.m3u8") == 0);
  camera_feed_free(t, n);

  /* hp-flattened arrays group exactly like nested ones. */
  n = resolve("{\"views.0.url\":\"https://a.example/0.jpg\",\"views.0.status\":\"Enabled\","
              "\"views.1.url\":\"https://a.example/1.jpg\",\"views.1.id\":42}", NULL, t);
  assert(n == 2);
  assert(strcmp(t[0].suffix, "views/0") == 0);
  assert(strcmp(t[1].suffix, "views/42") == 0);
  camera_feed_free(t, n);
}

static void t_override(void) {
  camera_feed_target t[CAM_FEED_MAX_TARGETS];
  /* hp3b29a_webcams.c JO29A_CAM_US_AK_DOT_RWIS: the slot index is part of
   * the key NAME, invisible to any generic rule. */
  const char *p =
    "{\"properties.OBJECTID\":5,\"properties.properties_name\":\"Atigun Pass\","
    "\"properties.cameras_0_latestImage\":\"https://511.alaska.gov/cams/a0.jpg\","
    "\"properties.cameras_0_akst\":\"2026-10-10 12:00\","
    "\"properties.cameras_1_latestImage\":\"https://511.alaska.gov/cams/a1.jpg\","
    "\"source_id\":\"JO29A_CAM_US_AK_DOT_RWIS\"}";
  int n = resolve(p, NULL, t);          /* source id read from the record */
  assert(n == 2);
  assert(strcmp(t[0].suffix, "cameras/0") == 0);
  assert(strcmp(t[1].suffix, "cameras/1") == 0);
  assert(strcmp(t[1].url, "https://511.alaska.gov/cams/a1.jpg") == 0);
  camera_feed_free(t, n);
  /* Without the override the same blob has no recognisable key. */
  assert(resolve(p, "SOME_OTHER_SOURCE", NULL) == 0);

  /* Fullwidth ＵＲＬ (hp3b30_jpcam.c Tokushima CSV). */
  n = resolve("{\"名称\":\"吉野川\",\"ＵＲＬ\":\"https://tokushima.example/cam1.jpg\"}",
              "JO30_CAM_TOKUSHIMA_LIVECAM_INVENTORY", t);
  assert(n == 1 && strcmp(t[0].url, "https://tokushima.example/cam1.jpg") == 0);
  camera_feed_free(t, n);
}

static void t_variants(void) {
  camera_feed_target t[CAM_FEED_MAX_TARGETS];
  const char *cases[][2] = {
    { "{\"IMAGE_URL\":\"https://v.example/a.jpg\"}", "IMAGE_URL" },
    { "{\"ImageUrl\":\"https://v.example/a.jpg\"}", "ImageUrl" },
    { "{\"cameraimageurl\":\"https://v.example/a.jpg\",\"description\":\"x\"}", "cameraimageurl" },
    { "{\"attributes.cam_img\":\"https://v.example/a.jpg\"}", "attributes.cam_img" },
    { "{\"links.imageDisplay\":\"https://v.example/a.jpg\",\"links.self\":\"https://v.example/api\"}", "links.imageDisplay" },
    { "{\"camera\":{\"snapshot\":\"https://v.example/a.jpg\",\"url\":\"https://v.example/page\"}}", "camera.snapshot" },
    { "{\"Thumbnail_Url\":\"https://v.example/a.jpg\",\"Webcam_URL\":\"https://v.example/p\"}", "Thumbnail_Url" },
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    int n = resolve(cases[i][0], NULL, t);
    if (n != 1 || strcmp(t[0].key, cases[i][1]) != 0) {
      fprintf(stderr, "variant %zu: n=%d key=%s\n", i, n, n ? t[0].key : "-");
      assert(0);
    }
    assert(strcmp(t[0].url, "https://v.example/a.jpg") == 0);
    camera_feed_free(t, n);
  }
  /* Ranking: a still beats a stream beats a thumbnail beats a page. */
  int n = resolve("{\"url\":\"https://r.example/page\",\"thumbnail_url\":\"https://r.example/t.jpg\","
                  "\"stream_url\":\"rtsp://r.example/live\",\"snapshot_url\":\"https://r.example/s.jpg\"}",
                  NULL, t);
  assert(n == 1 && strcmp(t[0].key, "snapshot_url") == 0);
  camera_feed_free(t, n);
}

static void t_none(void) {
  assert(resolve("{\"url\":\"ftp://x.example/a.jpg\"}", NULL, NULL) == 0);
  assert(resolve("{\"image\":\"a.jpg\",\"name\":\"x\"}", NULL, NULL) == 0);
  assert(resolve("{\"image_url\":\"javascript:alert(1)\"}", NULL, NULL) == 0);
  assert(resolve("not json", NULL, NULL) == 0);
  assert(resolve("[\"https://x.example/a.jpg\"]", NULL, NULL) == 0);
  assert(resolve("", NULL, NULL) == 0);
  assert(resolve(NULL, NULL, NULL) == 0);
}

static void t_dedupe_and_page(void) {
  camera_feed_target t[CAM_FEED_MAX_TARGETS];
  /* The station repeats its first view at the top level; and its top-level
   * `url` is its page, which is not a target once it has views. */
  const char *p =
    "{\"url\":\"https://d.example/station/9\","
    "\"image_url\":\"https://d.example/v1.jpg\","
    "\"views\":[{\"id\":\"v1\",\"image_url\":\"https://d.example/v1.jpg\"},"
    "{\"id\":\"v2\",\"image_url\":\"https://d.example/v2.jpg\"}]}";
  int n = resolve(p, NULL, t);
  assert(n == 2);
  assert(t[0].suffix[0] == '\0' && strcmp(t[0].url, "https://d.example/v1.jpg") == 0);
  assert(strcmp(t[1].suffix, "views/v2") == 0);
  camera_feed_free(t, n);

  /* Page url + views (no top-level image): only the views. */
  n = resolve("{\"url\":\"https://d.example/station/9\",\"views\":[{\"url\":\"https://d.example/a.jpg\"}]}",
              NULL, t);
  assert(n == 1 && strcmp(t[0].suffix, "views/0") == 0);
  camera_feed_free(t, n);

  /* Two views claiming one id stay two targets. */
  n = resolve("{\"views\":[{\"id\":\"x\",\"image_url\":\"https://d.example/1.jpg\"},"
              "{\"id\":\"x\",\"image_url\":\"https://d.example/2.jpg\"}]}", NULL, t);
  assert(n == 2 && strcmp(t[0].suffix, t[1].suffix) != 0);
  camera_feed_free(t, n);
}

static void t_overflow(void) {
  size_t cap = 64 + 200 * 80;
  char *p = malloc(cap);
  size_t o = (size_t)snprintf(p, cap, "{\"views\":[");
  for (int i = 0; i < 70; i++)
    o += (size_t)snprintf(p + o, cap - o, "%s{\"image_url\":\"https://o.example/%d.jpg\"}",
                          i ? "," : "", i);
  snprintf(p + o, cap - o, "]}");
  camera_feed_target t[CAM_FEED_MAX_TARGETS];
  int n = resolve(p, NULL, t);
  assert(n == 70);                        /* the TRUE total, not the cap */
  assert(strcmp(t[CAM_FEED_MAX_TARGETS - 1].suffix, "views/63") == 0);
  camera_feed_free(t, CAM_FEED_MAX_TARGETS);
  free(p);
}

static void t_ids(void) {
  char id[256];
  assert(camera_feed_target_id("src|cam#frag", "views/C01", id, sizeof id));
  assert(strcmp(id, "src|cam#frag#view=views/C01") == 0);
  const char *suf = NULL;
  char *uid = camera_feed_split(id, &suf);
  assert(uid && strcmp(uid, "src|cam#frag") == 0 && suf && strcmp(suf, "views/C01") == 0);
  free(uid);
  uid = camera_feed_split("src|plain", &suf);
  assert(uid && strcmp(uid, "src|plain") == 0 && suf == NULL);
  free(uid);
  assert(camera_feed_target_id("u", "", id, sizeof id) && strcmp(id, "u") == 0);
  assert(!camera_feed_target_id("uuuuuuuu", "vvvvvvvv", id, 8) && id[0] == '\0');
}

int main(void) {
  t_discovery();
  t_digitraffic();
  t_511();
  t_hp_flat();
  t_override();
  t_variants();
  t_none();
  t_dedupe_and_page();
  t_overflow();
  t_ids();
  printf("test_camera_feed: OK\n");
  return 0;
}
