/* tests/unit/test_camera_privacy.c — the camera indexing allowlist
 * (core/camera_privacy.c). No network, no database.
 *
 * WHAT IS BEING PROTECTED. Indexing makes every frame of a camera searchable
 * at once, so only public-infrastructure channels may be indexed, and the
 * channels that find PRIVATE devices (insecam, shodan, manual IP seeds) or run
 * policing networks never are — not even when an operator widens the allowlist.
 * Channel and source names below are the real ones the collectors write
 * (cam_curated_jp.c, cam_insecam_scrape.c, shodan_cameras_jp.c,
 * public_cameras.c, trn_*_cameras.c, hp3b29a_webcams.c).
 */
#include "../../core/camera_privacy.c"

#include <assert.h>
#include <stdio.h>

int main(void) {
  unsetenv("JO_CAMERA_INDEX_CHANNELS");

  /* default allow: public infrastructure */
  assert(camera_channel_indexable("mlit_river", NULL));
  assert(camera_channel_indexable("jma_volcano", NULL));
  assert(camera_channel_indexable("expressway_cctv", NULL));
  assert(camera_channel_indexable(NULL, "digitraffic-weathercam-stations"));
  assert(camera_channel_indexable(NULL, "511-alberta-cameras"));
  assert(camera_channel_indexable(NULL, "nzta-traffic-cameras"));
  assert(camera_channel_indexable(NULL, "JO29A_CAM_IOWA_DOT_TRAFFIC_CAMERAS"));
  assert(camera_channel_indexable(NULL, "JO29A_CAM_US_AK_DOT_RWIS"));

  /* denied always */
  assert(!camera_channel_indexable("insecam_scrape", NULL));
  assert(!camera_channel_indexable("shodan_api", NULL));
  assert(!camera_channel_indexable("manual_ip_seed", NULL));
  assert(!camera_channel_indexable("osm_surveillance", NULL));
  assert(!camera_channel_indexable(NULL, "JO29A_CAM_BALTIMORE_CITIWATCH_CCTV"));
  /* a deny word beats an allow word, in either argument */
  assert(!camera_channel_indexable("insecam_scrape", "traffic-cams"));
  assert(!camera_channel_indexable("mlit_river", "shodan-cameras-jp"));

  /* unknown is no */
  assert(!camera_channel_indexable("tourism_webcam", NULL));
  assert(!camera_channel_indexable("broadcast_livecam", NULL));
  assert(!camera_channel_indexable("windy_api", "camera-discovery"));
  assert(!camera_channel_indexable(NULL, NULL));
  assert(!camera_channel_indexable("", ""));

  /* whole-word matching: "ip" is not "ship", "port" is not "airport" */
  assert(camera_channel_indexable("ship_port_cam", NULL));      /* port */
  assert(!camera_channel_indexable("airport_webcam", NULL));
  assert(!camera_channel_indexable("roadside_pub", NULL));       /* not "road" */

  /* override replaces the allow words, never the deny words */
  setenv("JO_CAMERA_INDEX_CHANNELS", "tourism, windy", 1);
  assert(camera_channel_indexable("tourism_webcam", NULL));
  assert(camera_channel_indexable("windy_api", NULL));
  assert(!camera_channel_indexable("mlit_river", NULL));         /* replaced */
  assert(!camera_channel_indexable("insecam_scrape", "tourism"));
  setenv("JO_CAMERA_INDEX_CHANNELS", "none", 1);
  assert(!camera_channel_indexable("mlit_river", NULL));
  unsetenv("JO_CAMERA_INDEX_CHANNELS");

  /* record form: camera_store.c's discovery_channel(s) */
  assert(camera_record_indexable(
    "{\"discovery_channel\":\"mlit_river\",\"discovery_channels\":[\"mlit_river\"]}",
    "camera-discovery"));
  assert(!camera_record_indexable(   /* also found by insecam → private device */
    "{\"discovery_channel\":\"mlit_river\",\"discovery_channels\":[\"mlit_river\",\"insecam_scrape\"]}",
    "camera-discovery"));
  assert(camera_record_indexable(    /* allowed via a secondary channel */
    "{\"discovery_channel\":\"windy_api\",\"discovery_channels\":[\"windy_api\",\"jma_volcano\"]}",
    "camera-discovery"));
  assert(camera_record_indexable("{\"views\":[]}", "digitraffic-weathercam-stations"));
  assert(!camera_record_indexable("not json", "camera-discovery"));

  printf("test_camera_privacy: OK\n");
  return 0;
}
