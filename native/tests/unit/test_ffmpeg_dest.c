/* tests/unit/test_ffmpeg_dest.c — core/ffmpeg.c refuses network inputs aimed
 * at loopback / LAN / metadata BEFORE it spawns anything, and its RTSP
 * protocol whitelist is the minimum. No network, no ffmpeg needed.
 *
 * JO_NO_FFMPEG=1 makes every spawn path answer FFMPEG_ERR_DISABLED, so an input
 * that comes back BLOCKED was judged before the gate — i.e. no process could
 * have been created — and one that comes back DISABLED passed the check.
 *
 * Includes ffmpeg.c to reach whitelist_for(). */

#include "../../core/ffmpeg.c"

#include <assert.h>

static int frame(const char *in) {
  unsigned char *out = NULL; size_t n = 0; char err[256];
  int rc = ffmpeg_extract_frame(in, -1.0, 0, 1000, &out, &n, err, sizeof err);
  free(out);
  return rc;
}

int main(void) {
  setenv("JO_NO_FFMPEG", "1", 1);
  unsetenv("JO_HTTP_BLOCK_PRIVATE");
  unsetenv("JO_CAMERA_ALLOW_LAN");

  assert(frame("rtsp://127.0.0.1:554/live") == FFMPEG_ERR_BLOCKED);
  assert(frame("rtsp://192.168.0.20/ch1") == FFMPEG_ERR_BLOCKED);
  assert(frame("http://169.254.169.254/latest/x.m3u8") == FFMPEG_ERR_BLOCKED);
  assert(frame("tcp://10.0.0.1:9000") == FFMPEG_ERR_BLOCKED);
  assert(frame("http://localhost/x.m3u8") == FFMPEG_ERR_BLOCKED);
  assert(frame("rtsp://cam.invalid/s") == FFMPEG_ERR_BLOCKED);   /* unresolvable */
  assert(strcmp(ffmpeg_strerror(FFMPEG_ERR_BLOCKED), "ffmpeg_blocked_destination") == 0);
  /* ffprobe and gray32 share classify(), so they refuse too */
  { ffmpeg_media_info mi; char err[128];
    assert(ffmpeg_info("rtsp://127.0.0.1/x", 1000, &mi, err, sizeof err) == FFMPEG_ERR_BLOCKED); }

  /* Public literal addresses and local files reach the (disabled) gate. */
  assert(frame("rtsp://93.184.216.34/live") == FFMPEG_ERR_DISABLED);
  assert(frame("/tmp/some-upload.mp4") == FFMPEG_ERR_DISABLED);

  /* The operator opt-in admits LAN cameras — and never the metadata range. */
  setenv("JO_CAMERA_ALLOW_LAN", "1", 1);
  assert(frame("rtsp://192.168.0.20/ch1") == FFMPEG_ERR_DISABLED);
  assert(frame("http://169.254.169.254/latest/x.m3u8") == FFMPEG_ERR_BLOCKED);
  unsetenv("JO_CAMERA_ALLOW_LAN");

  /* Minimum RTSP whitelist: transport is forced to TCP, so no udp/rtp, and
   * network lists still never contain `file`. */
  const char *wr = whitelist_for("rtsp");
  assert(strcmp(wr, "rtsp,rtsps,tcp,tls") == 0);
  assert(!strstr(wr, "udp") && !strstr(wr, "rtp,") && !strstr(wr, "file"));
  assert(!strstr(whitelist_for("https"), "file"));

  printf("test_ffmpeg_dest: OK\n");
  return 0;
}
